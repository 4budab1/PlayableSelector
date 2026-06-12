void PS_PlayerChangedVoiceChannel(int playerId, string oldChannelKey, string newChannelKey);
typedef func PS_PlayerChangedVoiceChannel;

[ComponentEditorProps(category: "GameScripted/GameMode/Components", description: "Encryption-key VoN channel manager", color: "0 0 255 255", icon: HYBRID_COMPONENT_ICON)]
class PS_VoNChannelsManagerClass : ScriptComponentClass
{
}

class PS_VoNChannelsManager : ScriptComponent
{
	static vector roomInitialPosition = "0 100000 0";

	[RplProp()]
	protected ref ReplicatedBasicMap<int, string> m_PlayerChannelKeyMap = new ReplicatedBasicMap<int, string>();
	[RplProp()]
	protected ref ReplicatedBasicMap<string, int> m_ChannelKeyToRoomId = new ReplicatedBasicMap<string, int>();
	[RplProp()]
	protected ref ReplicatedBasicMap<int, string> m_RoomIdToChannelKey = new ReplicatedBasicMap<int, string>();
	[RplProp()]
	protected int m_iLastRoomId = 1;

	protected ref ScriptInvokerBase<PS_PlayerChangedVoiceChannel> m_OnPlayerChangedVoiceChannel = new ScriptInvokerBase<PS_PlayerChangedVoiceChannel>();
	ref ScriptInvoker m_eOnRoomChanged = new ScriptInvoker();
	protected static PS_VoNChannelsManager m_Instance;
	// Per-player monotonic version counter for deferred room-change events. Each
	// RPC_SetPlayerToChannel bumps the version; DeferredRoomChangeEvent bails if
	// its captured version no longer matches (i.e. a newer RPC superseded it).
	// Prevents fast A→B room bounces from leaving the UI in a stale state when
	// the first deferred retry fires after the second RPC has already moved the
	// player to B correctly.
	protected ref map<int, int> m_mDeferredVersion = new map<int, int>();

	private const string CHANNEL_LOBBY = "Lobby";
	private const string CHANNEL_FACTION_PREFIX = "Faction";
	private const string CHANNEL_GROUP_PREFIX = "Group";
	private const string CHANNEL_BRIEFING_PREFIX = "FactionBriefing";
	private const string CHANNEL_SILENT_PREFIX = "NO_SOUND";

	static PS_VoNChannelsManager GetInstance()
	{
		return m_Instance;
	}

	bool IsReplicated()
	{
		return true;
	}

  ScriptInvoker GetOnRoomChanged()
  {
    return m_eOnRoomChanged;
  }

  ScriptInvokerBase<PS_PlayerChangedVoiceChannel> GetOnPlayerChangedVoiceChannel()
  {
    return m_OnPlayerChangedVoiceChannel;
  }

  // Diagnostic helpers (used by PS_VoiceChatList Rebuild logging)
  int GetPlayerChannelKeyMapSize()
  {
    if (m_PlayerChannelKeyMap)
      return m_PlayerChannelKeyMap.Count();
    return 0;
  }
  int GetRoomMapSize()
  {
    if (m_ChannelKeyToRoomId)
      return m_ChannelKeyToRoomId.Count();
    return 0;
  }

  override protected void OnPostInit(IEntity owner)
  {
    m_Instance = this;
    if (!m_PlayerChannelKeyMap)
      m_PlayerChannelKeyMap = new ReplicatedBasicMap<int, string>();
    if (!m_ChannelKeyToRoomId)
      m_ChannelKeyToRoomId = new ReplicatedBasicMap<string, int>();
    if (!m_RoomIdToChannelKey)
      m_RoomIdToChannelKey = new ReplicatedBasicMap<int, string>();

    bool isServer = Replication.IsServer();
    // CRITICAL FIX: OnPostInit is called on EVERY client when they connect (JIP).
    // Clearing the replicated channel maps here wipes the server's room state that
    // was just replicated to the new client, breaking VoN room display forever
    // because InitChannel RPCs are only sent once when a room is first created.
    if (isServer)
    {
      m_PlayerChannelKeyMap.Clear();
      m_ChannelKeyToRoomId.Clear();
      m_RoomIdToChannelKey.Clear();
      m_iLastRoomId = 1;
    }

    int existingChannels = m_ChannelKeyToRoomId.Count();
    PS_DebugLogger.LogImportant("[VoN] OnPostInit isServer=" + isServer.ToString() + " existingChannels=" + existingChannels.ToString() + " cleared=" + isServer.ToString());

    SCR_BaseGameMode baseGameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
    if (baseGameMode)
    {
      baseGameMode.GetOnPlayerConnected().Insert(OnPlayerConnected);
      baseGameMode.GetOnPlayerDisconnected().Insert(OnPlayerDisconnected);
    }
  }

  void OnPlayerConnected(int playerId)
  {
    PS_DebugLogger.LogImportant("[VoN-SRV] OnPlayerConnected player=" + playerId.ToString() + " currentChannelMapSize=" + m_PlayerChannelKeyMap.Count().ToString() + " currentRoomMapSize=" + m_ChannelKeyToRoomId.Count().ToString() + " nextRoomId=" + m_iLastRoomId.ToString(), playerId);
    if (!m_ChannelKeyToRoomId || !m_RoomIdToChannelKey)
    {
      PS_DebugLogger.LogError("[VoN-SRV] OnPlayerConnected: maps not initialized!");
      return;
    }
    InitChannelIfNeeded(GetPlayerSilentChannelKey(playerId));
    InitChannelIfNeeded(BuildChannelKey("", "#PS-VoNRoom_Local" + playerId.ToString()));
    InitChannelIfNeeded(BuildChannelKey("", "#PS-VoNRoom_Public" + playerId.ToString()));
    MoveToRoom(playerId, "", "#PS-VoNRoom_Global");
    PS_DebugLogger.LogImportant("[VoN-SRV] OnPlayerConnected DONE player=" + playerId.ToString() + " channelMapSize=" + m_PlayerChannelKeyMap.Count().ToString() + " roomMapSize=" + m_ChannelKeyToRoomId.Count().ToString(), playerId);
  }

  void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
  {
    PS_DebugLogger.LogImportant("VoN OnPlayerDisconnected player=" + playerId.ToString(), playerId);
    // Remove stale channel key entry so GetPlayersInRoom/GetPlayersPublicRooms
    // don't iterate over disconnected players (causes O(N) ghost lookups).
    if (m_PlayerChannelKeyMap)
      m_PlayerChannelKeyMap.Remove(playerId);
    // Also clear the per-player deferred-event version counter. Without this, a
    // player who disconnects with a pending deferred event leaves a stale entry
    // in m_mDeferredVersion that grows unbounded over a long session with churn.
    if (m_mDeferredVersion)
      m_mDeferredVersion.Remove(playerId);
    RemovePerPlayerChannels(playerId);
  }

  // Three channels are created for every connecting player (NO_SOUND_*, Local,
  // Public). They were never removed, so over an evening with reconnect churn the
  // channel maps grew unbounded (150+ channels observed) — inflating every JIP
  // snapshot and every room sweep. OnPlayerDisconnected fires on the server and
  // on every client, so removing locally keeps all machines consistent, and JIP
  // clients receive the already-cleaned [RplProp] map snapshot.
  protected void RemovePerPlayerChannels(int playerId)
  {
    if (!m_ChannelKeyToRoomId || !m_RoomIdToChannelKey)
      return;

    array<string> keysToRemove = {};
    string localKey = BuildChannelKey("", "#PS-VoNRoom_Local" + playerId.ToString());
    string publicKey = BuildChannelKey("", "#PS-VoNRoom_Public" + playerId.ToString());
    string silentSuffix = "_" + playerId.ToString();
    for (int i = 0; i < m_ChannelKeyToRoomId.Count(); i++)
    {
      string key = m_ChannelKeyToRoomId.GetKey(i);
      if (key == localKey || key == publicKey)
      {
        keysToRemove.Insert(key);
        continue;
      }
      // Silent channel embeds the player NAME (may be unavailable at disconnect
      // time), so match by prefix + "_<playerId>" suffix instead of rebuilding it.
      if (key.StartsWith(CHANNEL_SILENT_PREFIX) && key.EndsWith(silentSuffix))
        keysToRemove.Insert(key);
    }

    foreach (string key : keysToRemove)
    {
      // Keep the channel if any connected player is still assigned to it (e.g.
      // someone sitting in the disconnected player's public room) — it will be
      // dropped once it empties and that player disconnects or moves away.
      bool occupied = false;
      if (m_PlayerChannelKeyMap)
      {
        for (int i = 0; i < m_PlayerChannelKeyMap.Count(); i++)
        {
          if (m_PlayerChannelKeyMap.GetElement(i) == key)
          {
            occupied = true;
            break;
          }
        }
      }
      if (occupied)
        continue;

      int roomId;
      if (m_ChannelKeyToRoomId.Find(key, roomId))
        m_RoomIdToChannelKey.Remove(roomId);
      m_ChannelKeyToRoomId.Remove(key);
    }
  }

	// --------------------------------------------------------------------------------------------
	// Channel key generators
	// --------------------------------------------------------------------------------------------
	string GetLobbyChannelKey()
	{
		return CHANNEL_LOBBY;
	}

	string GetFactionChannelKey(FactionKey factionKey)
	{
		return CHANNEL_FACTION_PREFIX + "_" + factionKey;
	}

	string GetGroupChannelKey(FactionKey factionKey, int groupId)
	{
		return CHANNEL_GROUP_PREFIX + "_" + groupId.ToString() + "_" + factionKey;
	}

	string GetFactionBriefingChannelKey(FactionKey factionKey)
	{
		return CHANNEL_BRIEFING_PREFIX + "_" + factionKey;
	}

	string GetPlayerSilentChannelKey(int playerId)
	{
		string name = GetGame().GetPlayerManager().GetPlayerName(playerId);
		return CHANNEL_SILENT_PREFIX + "_" + name + "_" + playerId.ToString();
	}

	// --------------------------------------------------------------------------------------------
	// Move player between channels (replaces VoNRoomsManager.MoveToRoom)
	// --------------------------------------------------------------------------------------------
  void MoveToRoom(int playerId, FactionKey factionKey, string roomName)
  {
    if (!Replication.IsServer())
      return;
    if (!m_ChannelKeyToRoomId || !m_RoomIdToChannelKey || !m_PlayerChannelKeyMap)
    {
      PS_DebugLogger.LogError("VoN MoveToRoom: maps not initialized!");
      return;
    }

    string channelKey = BuildChannelKey(factionKey, roomName);
    InitChannelIfNeeded(channelKey);

    string prevChannel;
    m_PlayerChannelKeyMap.Find(playerId, prevChannel);
    PS_DebugLogger.Log("[VoN-SRV] MoveToRoom player=" + playerId.ToString() + " faction=" + factionKey + " room=" + roomName + " channelKey=" + channelKey + " prevChannel=" + prevChannel + " channelMapSize=" + m_PlayerChannelKeyMap.Count().ToString(), playerId);

    PlayerManager playerManager = GetGame().GetPlayerManager();
		PlayerController playerController = playerManager.GetPlayerController(playerId);

		if (playerController)
		{
			PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
		if (playableController)
		{
			PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
			if (!gameMode)
				return;
			SCR_EGameModeState state = gameMode.GetState();
				PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();

				if (roomName.StartsWith("#PS-VoNRoom_Local"))
				{
					playableController.SetVoNKey(roomName, channelKey);
				}
				else if (state == SCR_EGameModeState.GAME)
				{
					playableController.SetVoNKey("Menu" + factionKey + roomName, channelKey);
				}
				else if (state == SCR_EGameModeState.BRIEFING)
				{
					RplId playableId = playableManager.GetPlayableByPlayer(playerId);
					int groupCallsign = playableManager.GetGroupCallsignByPlayable(playableId);
					playableController.SetVoNKey("Menu" + factionKey + groupCallsign.ToString(), channelKey);
				}
				else
				{
					playableController.SetVoNKey("Menu" + factionKey, channelKey);
				}
			}
		}

		SetPlayerToChannel(playerId, channelKey);
		PS_DebugLogger.Log("[VoN-SRV] MoveToRoom DONE player=" + playerId.ToString() + " channel=" + channelKey + " mapSize=" + m_PlayerChannelKeyMap.Count().ToString(), playerId);
	}

  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  protected void RPC_SetPlayerToChannel(int playerId, string channelKey)
  {
    if (!m_PlayerChannelKeyMap)
    {
      PS_DebugLogger.LogError("[VoN-CLI] RPC_SetPlayerToChannel: m_PlayerChannelKeyMap not initialized!");
      return;
    }
    string oldChannelKey;
    m_PlayerChannelKeyMap.Find(playerId, oldChannelKey);

    PS_DebugLogger.Log("[VoN-CLI] RPC_SetPlayerToChannel RCV player=" + playerId.ToString() + " oldChannel=" + oldChannelKey + " newChannel=" + channelKey + " mapSize=" + m_PlayerChannelKeyMap.Count().ToString(), playerId);

    int oldRoomId = GetPlayerRoom(playerId);

    if (channelKey != "")
      m_PlayerChannelKeyMap[playerId] = channelKey;
    else
      m_PlayerChannelKeyMap.Remove(playerId);

    InitChannelIfNeeded(channelKey);

    int newRoomId;
    if (!m_ChannelKeyToRoomId.Find(channelKey, newRoomId))
    {
      // BUGFIX: Room not yet replicated (RPC_InitChannel hasn't arrived yet because
      // it was sent one tick earlier on the server but RPC_SetPlayerToChannel can
      // overtake it on the client). Invoking m_eOnRoomChanged with newRoomId=-1
      // here causes PS_VoiceChatList.MovePlayer to fail silently (CreateRoomIfNeed
      // rejects roomId < 0) and the player is never added to the new room on this
      // client. Result: "voice channel didn't update for everyone".
      // Defer the event by 50ms — by then RPC_InitChannel should have arrived and
      // resolved the roomId. If it still hasn't, the SyncRoomPlayers 200ms tick
      // (or the next RPC_InitChannel) will heal the state.
      // Bump a per-player version so any older deferred event for the same player
      // bails on fire (handles fast A→B bounces where a second RPC resolves
      // before the first deferred retry).
      int myVersion = 0;
      m_mDeferredVersion.Find(playerId, myVersion);
      myVersion++;
      m_mDeferredVersion[playerId] = myVersion;
      PS_DebugLogger.Log("[VoN-CLI] RPC_SetPlayerToChannel channelKey=" + channelKey + " not yet in roomIdMap — deferring event 50ms version=" + myVersion.ToString(), playerId);
      GetGame().GetCallqueue().CallLater(DeferredRoomChangeEvent, 50, false, playerId, channelKey, oldChannelKey, oldRoomId, myVersion);
      return;
    }

    // Successful (non-deferred) RPC also bumps the version so any older deferred
    // event for the same player bails on fire.
    int currentVersion = 0;
    m_mDeferredVersion.Find(playerId, currentVersion);
    currentVersion++;
    m_mDeferredVersion[playerId] = currentVersion;

    PS_DebugLogger.Log("[VoN-CLI] RPC_SetPlayerToChannel player=" + playerId.ToString() + " resolved roomId=" + newRoomId.ToString() + " oldRoomId=" + oldRoomId.ToString(), playerId);

		m_OnPlayerChangedVoiceChannel.Invoke(playerId, oldChannelKey, channelKey);
		m_eOnRoomChanged.Invoke(playerId, newRoomId, oldRoomId);
		PS_DebugLogger.Log("[VoN-CLI] RPC_SetPlayerToChannel DONE player=" + playerId.ToString() + " newRoomId=" + newRoomId.ToString() + " eventFired", playerId);
	}

	// Deferred room-change event: fires when RPC_SetPlayerToChannel arrived before
	// RPC_InitChannel resolved the roomId. Re-runs the event once the roomId is
	// available, or logs a warning if it still isn't after 50ms (in which case the
	// 200ms SyncRoomPlayers sweep or the next RPC_InitChannel will heal the UI).
	// Stale-suppression: if a newer RPC (or deferred event) for the same player
	// arrived while we were waiting, capturedVersion != current stored version and
	// we bail. This prevents a stale A→Global move from overwriting a correct
	// A→B move that resolved in the meantime.
	protected void DeferredRoomChangeEvent(int playerId, string channelKey, string oldChannelKey, int oldRoomId, int capturedVersion)
	{
		int currentVersion = 0;
		m_mDeferredVersion.Find(playerId, currentVersion);
		if (capturedVersion != currentVersion)
		{
			PS_DebugLogger.Log("[VoN-CLI] DeferredRoomChangeEvent SKIPPED stale player=" + playerId.ToString() + " channelKey=" + channelKey + " capturedVersion=" + capturedVersion.ToString() + " currentVersion=" + currentVersion.ToString());
			return;
		}
		int newRoomId;
		if (!m_ChannelKeyToRoomId.Find(channelKey, newRoomId))
		{
			PS_DebugLogger.LogWarning("[VoN-CLI] DeferredRoomChangeEvent: channelKey=" + channelKey + " still not in roomIdMap after 50ms — falling back to SyncRoomPlayers / next RPC_InitChannel");
			return;
		}
		PS_DebugLogger.Log("[VoN-CLI] DeferredRoomChangeEvent FIRED player=" + playerId.ToString() + " channelKey=" + channelKey + " newRoomId=" + newRoomId.ToString());
		m_OnPlayerChangedVoiceChannel.Invoke(playerId, oldChannelKey, channelKey);
		m_eOnRoomChanged.Invoke(playerId, newRoomId, oldRoomId);
	}

	// --------------------------------------------------------------------------------------------
	// Init channel if it doesn't exist (Echo Lobby pattern)
	// --------------------------------------------------------------------------------------------
	void InitChannelIfNeeded(string channelKey)
	{
		if (!m_ChannelKeyToRoomId || !m_RoomIdToChannelKey)
		{
			PS_DebugLogger.LogError("[VoN] InitChannelIfNeeded: maps not initialized!");
			return;
		}
		if (m_ChannelKeyToRoomId.Contains(channelKey))
		{
			// Guarded — hit every SyncRoomPlayers/GetVisibleRooms tick for existing rooms.
			if (PS_DebugLogger.DebugEnabled)
			{
				int existingId = m_ChannelKeyToRoomId[channelKey];
				PS_DebugLogger.Log("[VoN] InitChannelIfNeeded key=" + channelKey + " ALREADY EXISTS roomId=" + existingId.ToString() + " isServer=" + Replication.IsServer().ToString());
			}
			return;
		}

		if (Replication.IsServer())
		{
			m_ChannelKeyToRoomId[channelKey] = m_iLastRoomId;
			m_RoomIdToChannelKey[m_iLastRoomId] = channelKey;
			PS_DebugLogger.LogImportant("[VoN-SRV] InitChannel id=" + m_iLastRoomId.ToString() + " key=" + channelKey + " totalChannels=" + m_ChannelKeyToRoomId.Count().ToString());
			Rpc(RPC_InitChannel, m_iLastRoomId, channelKey);
			m_eOnRoomChanged.Invoke(-1, m_iLastRoomId, -1);
			m_iLastRoomId++;
		}
		else
		{
			// Client-side: create placeholder, RPC_InitChannel will fix the real ID
			m_ChannelKeyToRoomId[channelKey] = -1;
			PS_DebugLogger.LogImportant("[VoN-CLI] InitChannel PLACEHOLDER key=" + channelKey + " (awaiting RPC_InitChannel)");
		}
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_InitChannel(int roomId, string channelKey)
	{
		if (!m_ChannelKeyToRoomId || !m_RoomIdToChannelKey)
		{
			PS_DebugLogger.LogError("[VoN-CLI] RPC_InitChannel: maps not initialized! roomId=" + roomId.ToString() + " key=" + channelKey);
			return;
		}
		int prevRoomId;
		bool hadPrev = m_ChannelKeyToRoomId.Find(channelKey, prevRoomId);
		m_ChannelKeyToRoomId[channelKey] = roomId;
		m_RoomIdToChannelKey[roomId] = channelKey;
		if (hadPrev && prevRoomId != roomId)
			m_RoomIdToChannelKey.Remove(prevRoomId);
		PS_DebugLogger.LogImportant("[VoN-CLI] RPC_InitChannel RCV id=" + roomId.ToString() + " key=" + channelKey + " hadPrev=" + hadPrev.ToString() + " prevRoomId=" + prevRoomId.ToString() + " totalChannels=" + m_ChannelKeyToRoomId.Count().ToString());
		if (m_eOnRoomChanged)
			m_eOnRoomChanged.Invoke(-1, roomId, -1);
	}

	// --------------------------------------------------------------------------------------------
	// Set channel key and replicate
	// --------------------------------------------------------------------------------------------
	void SetPlayerToChannel(int playerId, string channelKey)
	{
		if (!Replication.IsServer())
			return;
		if (!m_PlayerChannelKeyMap)
		{
			PS_DebugLogger.LogError("VoN SetPlayerToChannel: m_PlayerChannelKeyMap not initialized!");
			return;
		}

		if (channelKey != "")
			InitChannelIfNeeded(channelKey);

		Rpc(RPC_SetPlayerToChannel, playerId, channelKey);
	}

	string GetPlayerChannelKey(int playerId)
	{
		if (!m_PlayerChannelKeyMap)
			return "";
		string key;
		m_PlayerChannelKeyMap.Find(playerId, key);
		return key;
	}

	// --------------------------------------------------------------------------------------------
	// Room management (backward-compatible with old room-based API)
	// --------------------------------------------------------------------------------------------
	int GetOrCreateRoomWithFaction(FactionKey factionKey, string roomName)
	{
		return GetOrCreateRoom(factionKey, roomName);
	}

	int GetOrCreateRoom(FactionKey factionKey, string roomName)
	{
		if (!m_ChannelKeyToRoomId)
			return -1;
		string key = BuildChannelKey(factionKey, roomName);
		InitChannelIfNeeded(key);
		return m_ChannelKeyToRoomId[key];
	}

	void RestoreRoom(int playerId)
	{
		string channelKey = GetPlayerChannelKey(playerId);
		if (channelKey == "")
			return;
		SetPlayerToChannel(playerId, channelKey);
	}

  int GetPlayerRoom(int playerId)
  {
    if (!m_PlayerChannelKeyMap)
    {
      PS_DebugLogger.LogError("VoN GetPlayerRoom: m_PlayerChannelKeyMap not initialized!");
      return -1;
    }
    string key;
    if (!m_PlayerChannelKeyMap.Find(playerId, key))
    {
      // Rate-limit: use generic message without player ID so all missing-player lookups
      // share one rate-limited key (5 per 10s). Without this, each player ID creates
      // a unique key, resulting in 53K+ log entries per session.
      PS_DebugLogger.Log("VoN GetPlayerRoom: player NOT in channelKeyMap (JIP or no channel assigned)");
      return -1;
    }
    if (!m_ChannelKeyToRoomId)
    {
      PS_DebugLogger.LogError("VoN GetPlayerRoom: m_ChannelKeyToRoomId not initialized!");
      return -1;
    }
    if (!m_ChannelKeyToRoomId.Contains(key))
    {
      // Fix #6: Channel key exists but room ID not replicated yet (JIP client received channel assignment before room creation RPC)
      PS_DebugLogger.Log("VoN GetPlayerRoom: channelKey=" + key + " exists but NO roomId — JIP desync, requesting init");
      // Trigger re-init on client so the placeholder gets resolved when RPC arrives
      InitChannelIfNeeded(key);
      int roomId = m_ChannelKeyToRoomId[key];
      if (roomId == -1)
      {
        PS_DebugLogger.Log("VoN GetPlayerRoom: roomId still -1 after InitChannelIfNeeded (awaiting RPC)");
      }
      return roomId;
    }
    return m_ChannelKeyToRoomId[key];
  }

	int GetRoomWithFaction(FactionKey factionKey, string roomName)
	{
		if (!m_ChannelKeyToRoomId)
			return -1;
		string key = BuildChannelKey(factionKey, roomName);
		if (!m_ChannelKeyToRoomId.Contains(key))
			return -1;
		return m_ChannelKeyToRoomId[key];
	}

	string GetRoomName(int roomId)
	{
		if (!m_RoomIdToChannelKey)
			return "";
		if (!m_RoomIdToChannelKey.Contains(roomId))
			return "";
		return m_RoomIdToChannelKey[roomId];
	}

	// Return all group-room IDs for a specific faction. Used by the briefing UI so
	// empty squads are still visible. Accepts any room name after the | separator
	// that is NOT a special room (#PS-VoNRoom_*), not just numeric callsigns.
	void GetFactionGroupRooms(FactionKey factionKey, out notnull array<int> roomIds)
	{
		if (!m_ChannelKeyToRoomId || !m_RoomIdToChannelKey)
			return;
		for (int i = 0; i < m_ChannelKeyToRoomId.Count(); i++)
		{
			string key = m_ChannelKeyToRoomId.GetKey(i);
			int roomId = m_ChannelKeyToRoomId.GetElement(i);
			if (roomId < 0)
				continue;
			if (factionKey != "")
			{
				if (!key.StartsWith(factionKey + "|"))
					continue;
			}
			else
			{
				if (!key.StartsWith("|"))
					continue;
			}
			int separatorIndex = key.IndexOf("|");
			string roomName = key.Substring(separatorIndex + 1, key.Length() - separatorIndex - 1);
			// Exclude special rooms (Faction, Command, etc.) — group rooms have
			// plain callsign names that don't start with "#PS-VoNRoom_".
			if (roomName.IsEmpty())
				continue;
			if (roomName.StartsWith("#PS-VoNRoom_"))
				continue;
			roomIds.Insert(roomId);
		}
	}

	// Return all group-room IDs across every faction. Used by the lobby UI so every
	// squad — even empty or opposite-faction — is visible.
	// Accepts any room name that is NOT a special room (#PS-VoNRoom_*).
	void GetAllGroupRooms(out notnull array<int> roomIds)
	{
		if (!m_ChannelKeyToRoomId || !m_RoomIdToChannelKey)
			return;
		for (int i = 0; i < m_ChannelKeyToRoomId.Count(); i++)
		{
			string key = m_ChannelKeyToRoomId.GetKey(i);
			int roomId = m_ChannelKeyToRoomId.GetElement(i);
			if (roomId < 0)
				continue;
			if (!key.Contains("|"))
				continue;
			int separatorIndex = key.IndexOf("|");
			string roomName = key.Substring(separatorIndex + 1, key.Length() - separatorIndex - 1);
			// Exclude special rooms (Faction, Command, Local, Public, Global) —
			// group rooms have plain callsign names that don't start with "#PS-VoNRoom_".
			if (roomName.IsEmpty())
				continue;
			if (roomName.StartsWith("#PS-VoNRoom_"))
				continue;
			roomIds.Insert(roomId);
		}
	}

	void GetPlayersPublicRooms(out notnull array<int> rooms)
	{
		if (!m_PlayerChannelKeyMap || !m_ChannelKeyToRoomId)
		{
			PS_DebugLogger.LogError("VoN GetPlayersPublicRooms: maps not initialized!");
			return;
		}
		// Iterate m_PlayerChannelKeyMap directly instead of all players via GetPlayerManager.
		// This avoids O(N) GetPlayerRoom() calls per room rebuild. With 50+ players and
		// 10+ visible rooms, the old pattern caused 500+ GetPlayerRoom calls in a single
		// frame (60+ per millisecond burst), starving the script thread.
		for (int i = 0; i < m_PlayerChannelKeyMap.Count(); i++)
		{
		// NOTE: Do NOT filter by GetPlayerController here. On clients, remote players'
		// controllers are not replicated, so this filter would hide ALL remote players.
		// OnPlayerDisconnected already cleans up m_PlayerChannelKeyMap server-side,
		// and that removal is replicated via RplProp. JIP clients receive the current
		// map snapshot, so stale entries are not a problem.
			string channelKey = m_PlayerChannelKeyMap.GetElement(i);
			int playerRoomId;
			if (!m_ChannelKeyToRoomId.Find(channelKey, playerRoomId))
				continue;
			if (rooms.Contains(playerRoomId))
				continue;
			if (IsPublicRoom(playerRoomId))
				rooms.Insert(playerRoomId);
		}
	}

	void GetPlayersInRoom(out notnull array<int> players, int roomId)
	{
		if (!m_PlayerChannelKeyMap || !m_ChannelKeyToRoomId)
		{
			PS_DebugLogger.LogError("[VoN] GetPlayersInRoom: maps not initialized!");
			return;
		}
		// Iterate m_PlayerChannelKeyMap directly instead of all players via GetPlayerManager.
		// This avoids O(N) GetPlayerRoom() calls. The old pattern called GetPlayerRoom()
		// for every player in the game, even players with no room assignment.
		for (int i = 0; i < m_PlayerChannelKeyMap.Count(); i++)
		{
			int playerId = m_PlayerChannelKeyMap.GetKey(i);
		// NOTE: Do NOT filter by GetPlayerController here. On clients, remote players'
		// controllers are not replicated, so this filter would hide ALL remote players.
		// OnPlayerDisconnected already cleans up m_PlayerChannelKeyMap server-side,
		// and that removal is replicated via RplProp. JIP clients receive the current
		// map snapshot, so stale entries are not a problem.
			string channelKey = m_PlayerChannelKeyMap.GetElement(i);
			int playerRoomId;
			if (m_ChannelKeyToRoomId.Find(channelKey, playerRoomId) && playerRoomId == roomId)
				players.Insert(playerId);
		}
		// DEBUG: trace every call so we can see what's in the room from the client's perspective.
		// Guarded — this runs every SyncRoomPlayers tick per room, so the string building
		// alone is measurable when diagnostics are off.
		if (PS_DebugLogger.DebugEnabled)
		{
			string playersStr = "";
			for (int p = 0; p < players.Count(); p++)
			{
				if (p > 0) playersStr += ",";
				playersStr += players[p].ToString();
			}
			PS_DebugLogger.Log("[VoN] GetPlayersInRoom roomId=" + roomId.ToString() + " found=" + players.Count().ToString() + " mapSize=" + m_PlayerChannelKeyMap.Count().ToString() + " players=[" + playersStr + "]");
		}
	}

	bool IsPublicRoom(int roomId)
	{
		if (roomId < 0 || !m_RoomIdToChannelKey)
			return false;
		string name = GetRoomName(roomId);
		if (name.IsEmpty() || name.Length() <= 13)
			return false;
		return name.ContainsAt("Public", 13);
	}

	bool IsFactionRoom(int roomId, FactionKey factionKey)
	{
		if (roomId < 0 || !m_RoomIdToChannelKey)
			return false;
		string name = GetRoomName(roomId);
		if (name.IsEmpty())
			return false;
		if (factionKey == "")
			return name.StartsWith("|");
		return name.StartsWith(factionKey + "|");
	}

	bool IsGlobalRoom(int roomId)
	{
		if (roomId < 0 || !m_RoomIdToChannelKey)
			return false;
		string name = GetRoomName(roomId);
		if (name.IsEmpty())
			return false;
		return name.Contains("#PS-VoNRoom_Global");
	}

	bool IsLocalRoom(int roomId)
	{
		if (roomId < 0 || !m_RoomIdToChannelKey)
			return false;
		string name = GetRoomName(roomId);
		if (name.IsEmpty())
			return false;
		return name.Contains("#PS-VoNRoom_Local") || IsNoSoundChannel(name);
	}

	bool IsNoSoundChannel(string channelKey)
	{
		return channelKey.StartsWith(CHANNEL_SILENT_PREFIX);
	}

	bool IsDeafenChannel(string channelKey)
	{
		return IsNoSoundChannel(channelKey) || channelKey.Contains("#PS-VoNRoom_Local");
	}

	// --------------------------------------------------------------------------------------------
	// Internal helpers
	// --------------------------------------------------------------------------------------------
	string BuildChannelKey(FactionKey factionKey, string roomName)
	{
		string key = factionKey + "|" + roomName;
		if (key == "|")
			key = "";
		return key;
	}

	void ParseChannelKey(string channelKey, out FactionKey factionKey, out string roomName)
	{
		factionKey = "";
		roomName = CHANNEL_LOBBY;
		if (channelKey == "")
			return;
		if (channelKey.Contains("|"))
		{
			array<string> tokens = {};
			channelKey.Split("|", tokens, false);
			factionKey = tokens[0];
			roomName = tokens[1];
		}
		else
		{
			roomName = channelKey;
		}
	}

  // --------------------------------------------------------------------------------------------
  // JIP sync: send all existing channel state to a connecting client.
  // Called from server after a player connects. Because ReplicatedBasicMap
  // snapshots may not carry channel data reliably, we explicitly blast the
  // current state via RPC.
  // --------------------------------------------------------------------------------------------
  void SyncChannelsToClient(int playerId)
  {
    if (!Replication.IsServer())
      return;

    int channelCount = m_ChannelKeyToRoomId.Count();
    int playerCount = m_PlayerChannelKeyMap.Count();
    PS_DebugLogger.LogImportant("[VoN-SRV] SyncChannelsToClient player=" + playerId.ToString() + " channels=" + channelCount.ToString() + " players=" + playerCount.ToString(), playerId);

    // Serialize channel key -> roomId pairs into parallel arrays
    array<string> keys = {};
    array<int> roomIds = {};
    for (int i = 0; i < m_ChannelKeyToRoomId.Count(); i++)
    {
      keys.Insert(m_ChannelKeyToRoomId.GetKey(i));
      roomIds.Insert(m_ChannelKeyToRoomId.GetElement(i));
    }

    // Serialize playerId -> channelKey pairs into parallel arrays
    array<int> pids = {};
    array<string> pKeys = {};
    for (int i = 0; i < m_PlayerChannelKeyMap.Count(); i++)
    {
      pids.Insert(m_PlayerChannelKeyMap.GetKey(i));
      pKeys.Insert(m_PlayerChannelKeyMap.GetElement(i));
    }

    Rpc(RPC_SyncVoNStateToClient, playerId, keys, roomIds, pids, pKeys);
    PS_DebugLogger.LogImportant("[VoN-SRV] SyncChannelsToClient SENT player=" + playerId.ToString() + " channelPackets=" + keys.Count().ToString() + " playerPackets=" + pids.Count().ToString(), playerId);
  }

  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  protected void RPC_SyncVoNStateToClient(int playerId, array<string> keys, array<int> roomIds, array<int> pids, array<string> pKeys)
  {
    // FIX: Removed the player-id guard. On JIP clients, GetPlayerController() may not
    // return a valid controller (or GetPlayerId() may be 0) when this RPC arrives, causing
    // the sync to be silently discarded and leaving the client with no voice channels.
    // Since m_PlayerChannelKeyMap / m_ChannelKeyToRoomId are [RplProp()] and already
    // replicated, applying the blast on all clients is harmless (overwrites with same
    // data) and ensures the JIP client fires the room-changed events that populate the UI.
    if (Replication.IsServer())
      return;

    // Guard: only apply the blast if the local state is empty or diverged.
    // This prevents unnecessary UI churn on existing clients who already have
    // the correct state. The blast is only needed for JIP clients who missed
    // the incremental RPCs while loading.
    // BUGFIX: The old needsSync check only compared counts, not contents. If the
    // client had a different set of channels with the same count, the sync was
    // skipped and the client never received the missing channels. This caused JIP
    // clients to miss group rooms that were created after the initial sync.
    // Now we explicitly check if the client is missing any server channels or has
    // extra channels not on the server.
    bool needsSync = false;
    // Check if client is missing any server channels or has mismatched roomIds
    // (e.g. placeholder -1 from client-side GetOrCreateRoomWithFaction vs real
    // server roomId). Without the roomId comparison, a JIP client with a -1
    // placeholder would see the key as "present" and skip the sync blast,
    // leaving the placeholder in place forever.
    for (int i = 0; i < keys.Count(); i++)
    {
      if (!m_ChannelKeyToRoomId.Contains(keys[i]))
      {
        needsSync = true;
        break;
      }
      int clientRoomId;
      if (!m_ChannelKeyToRoomId.Find(keys[i], clientRoomId) || clientRoomId != roomIds[i])
      {
        needsSync = true;
        break;
      }
    }
    // Check if client has extra channels not on server
    if (!needsSync)
    {
      for (int i = 0; i < m_ChannelKeyToRoomId.Count(); i++)
      {
        string clientKey = m_ChannelKeyToRoomId.GetKey(i);
        bool found = false;
        for (int j = 0; j < keys.Count(); j++)
        {
          if (keys[j] == clientKey)
          {
            found = true;
            break;
          }
        }
        if (!found)
        {
          needsSync = true;
          break;
        }
      }
    }
    if (!needsSync)
    {
      PS_DebugLogger.Log("[VoN-CLI] RPC_SyncVoNStateToClient SKIPPED (local state already matches) playerId=" + playerId.ToString() + " channels=" + keys.Count().ToString() + " players=" + pids.Count().ToString());
      return;
    }

    PS_DebugLogger.LogImportant("[VoN-CLI] RPC_SyncVoNStateToClient RCV playerId=" + playerId.ToString() + " channels=" + keys.Count().ToString() + " players=" + pids.Count().ToString());

    // 1. Clear existing maps so stale entries are removed
    m_ChannelKeyToRoomId.Clear();
    m_RoomIdToChannelKey.Clear();
    m_PlayerChannelKeyMap.Clear();

    // 2. Restore channel key -> roomId mappings
    for (int i = 0; i < keys.Count(); i++)
    {
      string key = keys[i];
      int roomId = roomIds[i];
      m_ChannelKeyToRoomId[key] = roomId;
      m_RoomIdToChannelKey[roomId] = key;
      PS_DebugLogger.Log("[VoN-CLI] RPC_SyncVoNStateToClient restored channel key=" + key + " roomId=" + roomId.ToString());
    }

    // 3. Restore playerId -> channelKey mappings and fire room-changed events
    for (int i = 0; i < pids.Count(); i++)
    {
      int pid = pids[i];
      string key = pKeys[i];
      m_PlayerChannelKeyMap[pid] = key;
      // Fire event so VoiceChatList catches up
      int resolvedRoomId = -1;
      m_ChannelKeyToRoomId.Find(key, resolvedRoomId);
      PS_DebugLogger.Log("[VoN-CLI] RPC_SyncVoNStateToClient restored player=" + pid.ToString() + " key=" + key + " roomId=" + resolvedRoomId.ToString());
      m_eOnRoomChanged.Invoke(pid, resolvedRoomId, -1);
    }

    PS_DebugLogger.LogImportant("[VoN-CLI] RPC_SyncVoNStateToClient DONE channelsRestored=" + m_ChannelKeyToRoomId.Count().ToString() + " roomKeyMap=" + m_RoomIdToChannelKey.Count().ToString());
  }

  // --------------------------------------------------------------------------------------------
  // Diagnostic: dump current VoN state to the log (both server and client)
  // --------------------------------------------------------------------------------------------
  void DumpVoNState(string context)
  {
    bool isServer = Replication.IsServer();
    string prefix;
    if (isServer)
      prefix = "[VoN-SRV]";
    else
      prefix = "[VoN-CLI]";
    PS_DebugLogger.LogImportant(prefix + " DumpVoNState context=" + context + " channels=" + m_ChannelKeyToRoomId.Count().ToString() + " players=" + m_PlayerChannelKeyMap.Count().ToString());
    for (int i = 0; i < m_ChannelKeyToRoomId.Count(); i++)
    {
      string key = m_ChannelKeyToRoomId.GetKey(i);
      int roomId = m_ChannelKeyToRoomId.GetElement(i);
      string roomName;
      if (m_RoomIdToChannelKey.Contains(roomId))
        roomName = m_RoomIdToChannelKey[roomId];
      else
        roomName = "???";
      PS_DebugLogger.Log(prefix + "  Channel[" + i.ToString() + "] key=" + key + " roomId=" + roomId.ToString() + " roomName=" + roomName);
    }
    for (int i = 0; i < m_PlayerChannelKeyMap.Count(); i++)
    {
      int pid = m_PlayerChannelKeyMap.GetKey(i);
      string key = m_PlayerChannelKeyMap.GetElement(i);
      int roomId = -1;
      m_ChannelKeyToRoomId.Find(key, roomId);
      PS_DebugLogger.Log(prefix + "  Player[" + i.ToString() + "] id=" + pid.ToString() + " key=" + key + " roomId=" + roomId.ToString());
    }
  }

  void UpdatePlayerId(int oldPlayerId, int newPlayerId)
  {
    PS_DebugLogger.LogImportant("VoN UpdatePlayerId oldPlayer=" + oldPlayerId.ToString() + " newPlayer=" + newPlayerId.ToString());
    if (m_PlayerChannelKeyMap)
      m_PlayerChannelKeyMap.ReplaceKey(oldPlayerId, newPlayerId);
  }
};
