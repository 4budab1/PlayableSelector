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

  override protected void OnPostInit(IEntity owner)
  {
    m_Instance = this;
    PS_DebugLogger.LogImportant("PS_VoNChannelsManager OnPostInit isServer=" + Replication.IsServer().ToString() + " existingChannels=" + m_ChannelKeyToRoomId.Count().ToString());
    m_PlayerChannelKeyMap.Clear();
    m_ChannelKeyToRoomId.Clear();
    m_RoomIdToChannelKey.Clear();
		m_iLastRoomId = 1;
		SCR_BaseGameMode baseGameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (baseGameMode)
			baseGameMode.GetOnPlayerConnected().Insert(OnPlayerConnected);
	}

  void OnPlayerConnected(int playerId)
  {
    PS_DebugLogger.LogImportant("VoN OnPlayerConnected player=" + playerId.ToString(), playerId);
    InitChannelIfNeeded(GetPlayerSilentChannelKey(playerId));
    InitChannelIfNeeded(BuildChannelKey("", "#PS-VoNRoom_Local" + playerId.ToString()));
    InitChannelIfNeeded(BuildChannelKey("", "#PS-VoNRoom_Public" + playerId.ToString()));
    MoveToRoom(playerId, "", "#PS-VoNRoom_Global");
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

    string channelKey = BuildChannelKey(factionKey, roomName);
    InitChannelIfNeeded(channelKey);

    PS_DebugLogger.LogImportant("VoN MoveToRoom SRV player=" + playerId.ToString() + " faction=" + factionKey + " room=" + roomName + " channelKey=" + channelKey, playerId);

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
		PS_DebugLogger.Log("MoveToRoom player=" + playerId.ToString() + " channel=" + channelKey, playerId);
	}

  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  protected void RPC_SetPlayerToChannel(int playerId, string channelKey)
  {
    string oldChannelKey;
    m_PlayerChannelKeyMap.Find(playerId, oldChannelKey);

    PS_DebugLogger.LogImportant("VoN RPC_SetPlayerToChannel player=" + playerId.ToString() + " oldChannel=" + oldChannelKey + " newChannel=" + channelKey, playerId);

    int oldRoomId = GetPlayerRoom(playerId);

    if (channelKey != "")
      m_PlayerChannelKeyMap[playerId] = channelKey;
    else
      m_PlayerChannelKeyMap.Remove(playerId);

    InitChannelIfNeeded(channelKey);

    int newRoomId;
    if (!m_ChannelKeyToRoomId.Find(channelKey, newRoomId))
    {
      // Fix #6: Room not yet replicated — set to -1, will be resolved by RPC_InitChannel
      PS_DebugLogger.LogError("VoN RPC_SetPlayerToChannel channelKey=" + channelKey + " not yet in roomIdMap — JIP desync, will resolve on RPC_InitChannel", playerId);
      newRoomId = -1;
    }

		m_OnPlayerChangedVoiceChannel.Invoke(playerId, oldChannelKey, channelKey);
		m_eOnRoomChanged.Invoke(playerId, newRoomId, oldRoomId);
	}

	// --------------------------------------------------------------------------------------------
	// Init channel if it doesn't exist (Echo Lobby pattern)
	// --------------------------------------------------------------------------------------------
	void InitChannelIfNeeded(string channelKey)
	{
		if (m_ChannelKeyToRoomId.Contains(channelKey))
			return;

		if (Replication.IsServer())
		{
			m_ChannelKeyToRoomId[channelKey] = m_iLastRoomId;
			m_RoomIdToChannelKey[m_iLastRoomId] = channelKey;
			PS_DebugLogger.LogImportant("VoN InitChannel SRV id=" + m_iLastRoomId.ToString() + " key=" + channelKey);
			Rpc(RPC_InitChannel, m_iLastRoomId, channelKey);
			m_eOnRoomChanged.Invoke(-1, m_iLastRoomId, -1);
			m_iLastRoomId++;
		}
		else
		{
			// Client-side: create placeholder, RPC_InitChannel will fix the real ID
			m_ChannelKeyToRoomId[channelKey] = -1;
			PS_DebugLogger.LogImportant("VoN InitChannel CLI placeholder key=" + channelKey);
		}
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_InitChannel(int roomId, string channelKey)
	{
		m_ChannelKeyToRoomId[channelKey] = roomId;
		m_RoomIdToChannelKey[roomId] = channelKey;
		PS_DebugLogger.LogImportant("VoN RPC_InitChannel RCV id=" + roomId.ToString() + " key=" + channelKey);
		m_eOnRoomChanged.Invoke(-1, roomId, -1);
	}

	// --------------------------------------------------------------------------------------------
	// Set channel key and replicate
	// --------------------------------------------------------------------------------------------
	void SetPlayerToChannel(int playerId, string channelKey)
	{
		if (!Replication.IsServer())
			return;

		if (channelKey != "")
			InitChannelIfNeeded(channelKey);

		Rpc(RPC_SetPlayerToChannel, playerId, channelKey);
	}

	string GetPlayerChannelKey(int playerId)
	{
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
    string key;
    if (!m_PlayerChannelKeyMap.Find(playerId, key))
    {
      PS_DebugLogger.Log("VoN GetPlayerRoom player=" + playerId.ToString() + " NOT in channelKeyMap (JIP or no channel assigned)", playerId);
      return -1;
    }
    if (!m_ChannelKeyToRoomId.Contains(key))
    {
      // Fix #6: Channel key exists but room ID not replicated yet (JIP client received channel assignment before room creation RPC)
      PS_DebugLogger.LogError("VoN GetPlayerRoom player=" + playerId.ToString() + " channelKey=" + key + " exists but NO roomId — JIP desync, requesting init", playerId);
      // Trigger re-init on client so the placeholder gets resolved when RPC arrives
      InitChannelIfNeeded(key);
      int roomId = m_ChannelKeyToRoomId[key];
      if (roomId == -1)
      {
        PS_DebugLogger.Log("VoN GetPlayerRoom player=" + playerId.ToString() + " roomId still -1 after InitChannelIfNeeded (awaiting RPC)", playerId);
      }
      return roomId;
    }
    return m_ChannelKeyToRoomId[key];
  }

	int GetRoomWithFaction(FactionKey factionKey, string roomName)
	{
		string key = BuildChannelKey(factionKey, roomName);
		if (!m_ChannelKeyToRoomId.Contains(key))
			return -1;
		return m_ChannelKeyToRoomId[key];
	}

	string GetRoomName(int roomId)
	{
		if (!m_RoomIdToChannelKey.Contains(roomId))
			return "";
		return m_RoomIdToChannelKey[roomId];
	}

	void GetPlayersPublicRooms(out notnull array<int> rooms)
	{
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			int playerRoomId = GetPlayerRoom(playerId);
			if (rooms.Contains(playerRoomId))
				continue;
			if (IsPublicRoom(playerRoomId))
				rooms.Insert(playerRoomId);
		}
	}

	void GetPlayersInRoom(out notnull array<int> players, int roomId)
	{
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (GetPlayerRoom(playerId) == roomId)
				players.Insert(playerId);
		}
	}

	bool IsPublicRoom(int roomId)
	{
		if (roomId < 0)
			return false;
		string name = GetRoomName(roomId);
		if (name.IsEmpty() || name.Length() <= 13)
			return false;
		return name.ContainsAt("Public", 13);
	}

	bool IsFactionRoom(int roomId, FactionKey factionKey)
	{
		if (roomId < 0)
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
		if (roomId < 0)
			return false;
		string name = GetRoomName(roomId);
		if (name.IsEmpty())
			return false;
		return name.Contains("#PS-VoNRoom_Global");
	}

	bool IsLocalRoom(int roomId)
	{
		if (roomId < 0)
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

  void UpdatePlayerId(int oldPlayerId, int newPlayerId)
  {
    PS_DebugLogger.LogImportant("VoN UpdatePlayerId oldPlayer=" + oldPlayerId.ToString() + " newPlayer=" + newPlayerId.ToString());
    m_PlayerChannelKeyMap.ReplaceKey(oldPlayerId, newPlayerId);
  }
};
