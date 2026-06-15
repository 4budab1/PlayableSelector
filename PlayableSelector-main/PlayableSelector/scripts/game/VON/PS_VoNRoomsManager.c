//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "GameScripted/GameMode/Components", description: "", color: "0 0 255 255", icon: HYBRID_COMPONENT_ICON)]
class PS_VoNRoomsManagerClass: ScriptComponentClass
{

};

// just string but funnier
// struct: [FactionKey + "|"] + roomName
typedef string VoNRoomKey;

// Manage VoN "channels" (formerly position-based "rooms").
//
// Echo-style channel model: a channel is just a string key (which is also the radio encryption key
// that separates who can hear whom). Channels are created LAZILY - only when a player actually joins
// one - and player<->channel is a single replicated map. The old system pre-created a Local + Public
// room for EVERY player on connect (110+ rooms at 55 players, each via a Reliable Broadcast RPC) and
// also moved bodies to per-room sky positions; on a full server that reliable-channel traffic is a
// direct contributor to "Replication Flooded/Stalled" kicks. This version drops both.
//
// The voice routing itself (radio encryption keys via PS_PlayableControllerComponent.SetVoNKey) is
// unchanged - only channel identity (string key instead of int roomId + position) and replication.

class PS_VoNRoomsManager : ScriptComponent
{
	// Replicated channel state
	ref array<string> m_aChannels = {};                                    // channel keys that exist (lazy)
	protected ref map<string, bool> m_mChannelsSet = new map<string, bool>(); // fast existence lookup
	ref map<int, string> m_mPlayersChannel = new map<int, string>();       // playerId -> channelKey

	// Move speech bois to space (kept only as the parked-body anchor for SetVoNPosition compatibility)
	static vector roomInitialPosition = "-1 1000000 1";

	// Invokers
	// (playerId, channelKey, oldChannelKey)
	ref ScriptInvoker m_eOnRoomChanged = new ScriptInvoker();

	// ---- Body-less VoN proxy (per-player tiny replicated entity carrying the radio + VoN) ----
	[Attribute("", UIWidgets.ResourceNamePicker, "Per-player VoN proxy prefab (PS_VoNProxyComponent + SCR_VoNComponent + BaseRadioComponent, RplComponent streaming disabled)", "et")]
	protected ResourceName m_sVoNProxyPrefab;
	protected ref map<int, IEntity> m_mProxies_S = new map<int, IEntity>(); // server-side, for cleanup
	protected ref map<int, int> m_mProxySlots_S = new map<int, int>();      // server-side, grid slot per player
	protected ref array<int> m_aPendingRadioApplies = {};                   // deferred (1 frame) re-tune queue

	bool m_bRplLoaded = false;
	bool IsReplicated()
	{
		return m_bRplLoaded;
	}

	override protected void OnPostInit(IEntity owner)
	{
		// The "" (global lobby) channel always exists
		RegisterChannelLocal("");
		if (Replication.IsServer())
			m_bRplLoaded = true;
	}

	// more singletons for singletons god, make our spagetie kingdom great
	static PS_VoNRoomsManager GetInstance()
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (gameMode)
			return PS_VoNRoomsManager.Cast(gameMode.FindComponent(PS_VoNRoomsManager));
		else
			return null;
	}

	// ------------------------- Channel keys -------------------------
	// Channel key format preserved from the old room keys: factionKey + "|" + roomName ("" == global)
	static string MakeChannelKey(FactionKey factionKey, string roomName)
	{
		string channelKey = factionKey + "|" + roomName;
		if (channelKey == "|")
			channelKey = "";
		return channelKey;
	}

	// ------------------------- Channel changing -------------------------
	// Move to channel by faction/room name, creating the channel if new. RUN ONLY ON SERVER
	void MoveToRoom(int playerId, FactionKey factionKey, string roomName)
	{
		if (!Replication.IsServer())
			return;

		string channelKey = MakeChannelKey(factionKey, roomName);
		InitChannelIfNeeded(channelKey);

		// Skip if already in this channel
		if (channelKey == GetPlayerChannel(playerId))
			return;

		// Body-less: the encryption key + frequency live on the player's VoN proxy and are
		// derived from the channel itself (factionKey|roomName already encodes faction/group),
		// applied on every machine in RPC_SetPlayerChannel -> ApplyRadioKey. No body radios.

		// Finally move player to channel
		RPC_SetPlayerChannel(playerId, channelKey);
		Rpc(RPC_SetPlayerChannel, playerId, channelKey);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetPlayerChannel(int playerId, string channelKey)
	{
		PS_NetStat.Hit("RPC_SetPlayerChannel");
		string oldChannelKey = GetPlayerChannel(playerId);

		RegisterChannelLocal(channelKey); // ensure the channel exists on every client
		m_mPlayersChannel[playerId] = channelKey;

		ApplyRadioKey(playerId); // re-tune this player's VoN proxy radio on this machine

		m_eOnRoomChanged.Invoke(playerId, channelKey, oldChannelKey);
	}

	// Re-apply a player's current channel (e.g. after they respawn into a new body)
	void RestoreRoom(int playerId)
	{
		string channelKey = GetPlayerChannel(playerId);
		RPC_SetPlayerChannel(playerId, channelKey);
		Rpc(RPC_SetPlayerChannel, playerId, channelKey);
	}

	// ============================ Body-less VoN proxy ============================
	// Per-player tiny replicated entity carrying the radio + SCR_VoNComponent, so menu
	// speakers (no living character) transmit/receive without a controlled body. Ported
	// from LiteLobby. The radio key+frequency are applied on EVERY machine from replicated
	// channel state (proxies replicate to all), so routing works regardless of ownership.

	// Server: spawn this player's proxy, or RE-BIND an existing one on reconnect.
	// Reconnect keeps the SAME playerId but gives a NEW connection (RplIdentity). The existing proxy is
	// still rpl.Give'n to the dead OLD connection, so the reconnected client can no longer drive capture
	// on it = nobody hears that player (the reported "can't hear Banan after his first reconnect" bug).
	// We also can't rely on RemoveProxy_S running on disconnect: the live server's QuickTvT gamemode
	// overrides OnPlayerDisconnected and does not run our cleanup, so the proxy persists. So instead of
	// skipping when a proxy exists, re-bind it (re-Give + re-register + re-tune) to the new connection.
	void SpawnProxy_S(int playerId)
	{
		if (!Replication.IsServer())
			return;

		PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!pc)
		{
			// Controller can lag behind the connect callback - try again.
			GetGame().GetCallqueue().CallLater(SpawnProxy_S, 500, false, playerId);
			return;
		}

		IEntity proxy;
		bool reusing = m_mProxies_S.Find(playerId, proxy) && proxy;
		if (!reusing)
		{
			// Use the operator-assigned attribute if set, else the registered proxy prefab GUID. The
			// hardcoded fallback means this works regardless of which game-mode prefab the server loads
			// (the QuickTvT gamemode that extends this lobby makes attribute assignment ambiguous).
			ResourceName proxyPrefab = m_sVoNProxyPrefab;
			if (proxyPrefab == "")
				proxyPrefab = "{8DE2B66B89E14F88}Prefabs/VoN/PS_VoNProxy.et";

			// Spread proxies 200m apart at 10km altitude: a radio also carries proximity speech
			// around the sender, so co-located proxies would let every menu speaker hear every
			// other regardless of channel. Encryption keys still seal cross-channel audio.
			int slot = AllocateProxySlot_S(playerId);
			EntitySpawnParams params = new EntitySpawnParams();
			Math3D.MatrixIdentity4(params.Transform);
			params.Transform[3] = Vector(200 * Math.Mod(slot, 16), 10000, 200 * Math.Floor(slot / 16));

			proxy = GetGame().SpawnEntityPrefab(Resource.Load(proxyPrefab), GetGame().GetWorld(), params);
			if (!proxy)
			{
				Print("[PS_VoN] Failed to spawn VoN proxy prefab", LogLevel.ERROR);
				return;
			}

			PS_VoNProxyComponent proxyComp = PS_VoNProxyComponent.Cast(proxy.FindComponent(PS_VoNProxyComponent));
			if (proxyComp)
				proxyComp.SetPlayerId_S(playerId);
			else
				Print("[PS_VoN] VoN proxy prefab is missing PS_VoNProxyComponent", LogLevel.ERROR);

			m_mProxies_S.Set(playerId, proxy);
		}

		// Ownership: the owning client must be allowed to drive capture. ALWAYS (re-)Give to the current
		// connection - on reconnect this transfers the proxy off the dead old connection onto the new one.
		RplIdentity playerRplID = pc.GetRplIdentity();
		if (playerRplID != RplIdentity.Local())
		{
			RplComponent rpl = RplComponent.Cast(proxy.FindComponent(RplComponent));
			if (rpl)
				rpl.Give(playerRplID);
		}

		// The radio gadget wrapper does not init itself on a non-character entity - force it.
		SCR_RadioComponent radioGadget = SCR_RadioComponent.Cast(proxy.FindComponent(SCR_RadioComponent));
		if (radioGadget)
			radioGadget.OnPostInit(proxy);

		// Register the endpoint with the SERVER's VoN system, or the server computes an empty delivery set
		// (transmissions arrive but reach nobody). Re-done on reconnect: the per-player editor manager is
		// recreated, so the old registration is stale.
		SCR_VoNComponent vonComp = SCR_VoNComponent.Cast(proxy.FindComponent(SCR_VoNComponent));
		if (vonComp)
			vonComp.ConnectEditorToVoNSystem(playerId);

		if (reusing)
			Print(string.Format("[PS_VoN] VoN proxy re-bound to reconnected player %1", playerId), LogLevel.NORMAL);
		else
			Print(string.Format("[PS_VoN] VoN proxy spawned for player %1 at %2", playerId, proxy.GetOrigin().ToString()), LogLevel.NORMAL);

		ApplyRadioKey(playerId); // key may have been assigned before the proxy existed
	}

	// Server: delete this player's proxy (on disconnect).
	void RemoveProxy_S(int playerId)
	{
		if (!Replication.IsServer())
			return;
		IEntity proxy;
		if (m_mProxies_S.Find(playerId, proxy) && proxy)
			SCR_EntityHelper.DeleteEntityAndChildren(proxy);
		m_mProxies_S.Remove(playerId);
		m_mProxySlots_S.Remove(playerId);
	}

	// Smallest free grid slot (slots are reused as players leave - ids grow forever).
	protected int AllocateProxySlot_S(int playerId)
	{
		int existing;
		if (m_mProxySlots_S.Find(playerId, existing))
			return existing;

		int slot = 0;
		while (true)
		{
			bool taken = false;
			foreach (int pid, int s : m_mProxySlots_S)
			{
				if (s == slot)
				{
					taken = true;
					break;
				}
			}
			if (!taken)
				break;
			slot++;
		}
		m_mProxySlots_S.Set(playerId, slot);
		return slot;
	}

	// Re-tune a player's proxy radio. Runs on every machine (proxies replicate everywhere).
	// Deferred one frame + coalesced: a single event can deliver two state updates for one
	// player in the same frame, and the transceiver does not reliably apply two SetFrequency
	// calls in one frame.
	void ApplyRadioKey(int playerId)
	{
		if (m_aPendingRadioApplies.Contains(playerId))
			return;
		m_aPendingRadioApplies.Insert(playerId);
		GetGame().GetCallqueue().Remove(ApplyPendingRadioApplies);
		GetGame().GetCallqueue().CallLater(ApplyPendingRadioApplies, 0, false);
	}
	protected void ApplyPendingRadioApplies()
	{
		foreach (int playerId : m_aPendingRadioApplies)
			ApplyRadioKeyNow(playerId);
		m_aPendingRadioApplies.Clear();
	}
	protected void ApplyRadioKeyNow(int playerId)
	{
		IEntity proxy = PS_VoNProxyComponent.GetProxyEntity(playerId);
		if (!proxy)
			return;
		BaseRadioComponent radio = BaseRadioComponent.Cast(proxy.FindComponent(BaseRadioComponent));
		if (!radio || radio.TransceiversCount() < 1)
			return;
		BaseTransceiver tsv = radio.GetTransceiver(0);
		if (!tsv)
			return;

		string channelKey;
		if (!m_mPlayersChannel.Find(playerId, channelKey))
			channelKey = "";

		// Park the proxy (silent: unique key + min freq) when the player is NOT a menu speaker
		// (alive in a playable) or their own GM editor is open - the menu net must be silent
		// for them. Otherwise tune to their channel. The "PSVoN#" prefix namespaces the key so
		// it cannot collide with mission-configured in-game radios on an overlapping frequency.
		if (!SCR_VoNComponent.PS_IsMenuSpeaker(playerId) || IsLocalEditorOpenFor(playerId))
		{
			radio.SetEncryptionKey(string.Format("PSVoN#Parked_%1", playerId));
			tsv.SetFrequency(tsv.GetMinFrequency());
			return;
		}

		radio.SetEncryptionKey("PSVoN#" + channelKey);
		tsv.SetFrequency(ChannelFrequencyFor(tsv, channelKey));
	}

	// Frequency from the channel's index in the replicated m_aChannels (consistent order on
	// every machine), stepped by the transceiver resolution within its band.
	protected int ChannelFrequencyFor(BaseTransceiver tsv, string channelKey)
	{
		int minFreq = tsv.GetMinFrequency();
		int maxFreq = tsv.GetMaxFrequency();
		int step = tsv.GetFrequencyResolution();
		if (step <= 0)
			step = 10;

		int index = m_aChannels.Find(channelKey);
		if (index < 0)
			return minFreq;

		int freq = minFreq + (index + 1) * step;
		if (freq > maxFreq)
		{
			Print(string.Format("[PS_VoN] Out of frequency slots for channel '%1' (%2 channels) - widen the proxy radio band", channelKey, m_aChannels.Count()), LogLevel.WARNING);
			freq = maxFreq;
		}
		return freq;
	}

	// True only on the machine of a player whose OWN Game Master editor is open.
	protected bool IsLocalEditorOpenFor(int playerId)
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc || pc.GetPlayerId() != playerId)
			return false;
		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();
		return editorManager && editorManager.IsOpened();
	}

	// ------------------------- Channel creation -------------------------
	// Get (and lazily create) the channel key for a faction/room name. RUN ONLY ON SERVER
	string GetOrCreateRoomWithFaction(FactionKey factionKey, string roomName)
	{
		string channelKey = MakeChannelKey(factionKey, roomName);
		InitChannelIfNeeded(channelKey);
		return channelKey;
	}
	// Create the channel on all clients if it does not exist yet. RUN ONLY ON SERVER
	void InitChannelIfNeeded(string channelKey)
	{
		if (!Replication.IsServer())
			return;
		if (m_mChannelsSet.Contains(channelKey))
			return;
		RPC_CreateChannel(channelKey);
		Rpc(RPC_CreateChannel, channelKey);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_CreateChannel(string channelKey)
	{
		PS_NetStat.Hit("RPC_CreateChannel");
		RegisterChannelLocal(channelKey);
	}
	protected void RegisterChannelLocal(string channelKey)
	{
		if (m_mChannelsSet.Contains(channelKey))
			return;
		m_mChannelsSet[channelKey] = true;
		m_aChannels.Insert(channelKey);
	}

	// ------------------------- Get -------------------------
	string GetPlayerChannel(int playerId)
	{
		if (!m_mPlayersChannel.Contains(playerId))
			return "";
		return m_mPlayersChannel[playerId];
	}
	// Channel key for a faction/room name (deterministic - does not create)
	string GetRoomWithFaction(FactionKey factionKey, string roomName)
	{
		return MakeChannelKey(factionKey, roomName);
	}
	// The display name of a channel IS its key (kept for UI compatibility)
	string GetRoomName(string channelKey)
	{
		return channelKey;
	}

	void GetPlayersInRoom(out notnull array<int> players, string channelKey)
	{
		foreach (int playerId, string playerChannel : m_mPlayersChannel)
		{
			if (playerChannel == channelKey)
				players.Insert(playerId);
		}
	}

	void GetPlayersPublicChannels(out notnull array<string> channelKeys)
	{
		foreach (int playerId, string playerChannel : m_mPlayersChannel)
		{
			if (channelKeys.Contains(playerChannel))
				continue;
			if (IsPublicRoom(playerChannel))
				channelKeys.Insert(playerChannel);
		}
	}

	// ------------------------- Channel classification (by key) -------------------------
	bool IsPublicRoom(string channelKey)
	{
		if (channelKey.Length() <= 13)
			return false;
		return channelKey.ContainsAt("Public", 13);
	}

	bool IsFactionRoom(string channelKey, FactionKey factionKey)
	{
		if (factionKey == "")
			return channelKey.StartsWith("|");
		return channelKey.StartsWith(factionKey + "|");
	}

	bool IsGlobalRoom(string channelKey)
	{
		return channelKey == "|#PS-VoNRoom_Global";
	}

	bool IsLocalRoom(string channelKey)
	{
		return channelKey.StartsWith("|#PS-VoNRoom_Local");
	}

	// ------------------------- JIP Replication -------------------------
	// Only the channel list and player->channel map (no positions, no per-player auto rooms)
	override bool RplSave(ScriptBitWriter writer)
	{
		int channelsCount = m_aChannels.Count();
		writer.WriteInt(channelsCount);
		for (int i = 0; i < channelsCount; i++)
			writer.WriteString(m_aChannels[i]);

		int playersChannelCount = m_mPlayersChannel.Count();
		writer.WriteInt(playersChannelCount);
		for (int i = 0; i < playersChannelCount; i++)
		{
			writer.WriteInt(m_mPlayersChannel.GetKey(i));
			writer.WriteString(m_mPlayersChannel.GetElement(i));
		}

		return true;
	}

	override bool RplLoad(ScriptBitReader reader)
	{
		int channelsCount;
		reader.ReadInt(channelsCount);
		for (int i = 0; i < channelsCount; i++)
		{
			string channelKey;
			reader.ReadString(channelKey);
			RegisterChannelLocal(channelKey);
		}

		int playersChannelCount;
		reader.ReadInt(playersChannelCount);
		for (int i = 0; i < playersChannelCount; i++)
		{
			int key;
			string value;
			reader.ReadInt(key);
			reader.ReadString(value);
			m_mPlayersChannel.Insert(key, value);
		}

		m_bRplLoaded = true;

		return true;
	}
};
