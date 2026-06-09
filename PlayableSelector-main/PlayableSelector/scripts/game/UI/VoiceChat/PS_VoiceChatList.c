// Widget displays info about voice rooms in voice chat.
// Path: {35DB604900C55B98}UI/VoiceChat/VoiceChatFrame.layout

class PS_VoiceChatList : SCR_ScriptedWidgetComponent
{
	// consts
	/*
	protected const ResourceName m_sPlayerVoiceSelectorPrefab = "{086F282C8CE692F1}UI/VoiceChat/VoicePlayerSelector.layout";
	protected const ResourceName m_sVoiceRoomHeaderPrefab = "{C976E42779159507}UI/VoiceChat/VoiceRoomHeader.layout";
	*/
	
	[Attribute("{E705A59E59B577F2}UI/VoiceChat/VoiceChatRoom.layout")]
	protected ResourceName m_sVoiceChatRoomPrefab;
	
	// Global cached
	protected PlayerManager m_gPlayerManager;
	protected PS_PlayableManager m_gPlayableManager;
	protected PS_VoNChannelsManager m_gVoNChannelsManager;
	protected PlayerController m_pPlayerController;
	protected int m_iPlayerId;
	protected int m_iPublicRoomId;
	
	// Local
	// List of voice rooms
	protected VerticalLayoutWidget m_wRoomsList;
	protected ref map<int, PS_VoiceChatRoom> m_wRooms = new map<int, PS_VoiceChatRoom>;
	protected FactionKey m_sCurrentFactionKey;
	
	// -------------------- Handler events --------------------
	override void HandlerAttached(Widget w)
	{
	if (!GetGame().InPlayMode())
		return;

	m_gPlayerManager = GetGame().GetPlayerManager();
	m_gPlayableManager = PS_PlayableManager.GetInstance();
	m_gVoNChannelsManager = PS_VoNChannelsManager.GetInstance();
	m_pPlayerController = GetGame().GetPlayerController();

	if (!m_gVoNChannelsManager || !m_pPlayerController || !m_gPlayableManager)
	{
		GetGame().GetCallqueue().CallLater(HandlerAttached, 100, false, w);
		return;
	}

	super.HandlerAttached(w);

		m_wRoomsList = VerticalLayoutWidget.Cast(w.FindAnyWidget("RoomsList"));

		m_gVoNChannelsManager.m_eOnRoomChanged.Insert(MovePlayer);

		m_iPlayerId = m_pPlayerController.GetPlayerId();
		m_sCurrentFactionKey = m_gPlayableManager.GetPlayerFactionKey(m_iPlayerId);
		m_iPublicRoomId = m_gVoNChannelsManager.GetRoomWithFaction("", "#PS-VoNRoom_Public" + m_iPlayerId.ToString());

		Rebuild();

		GetGame().GetCallqueue().CallLater(UpdateInfo, 100, true);
	}
	
	void ~PS_VoiceChatList()
	{
		if (!GetGame().InPlayMode())
			return;
		
		if (!m_gVoNChannelsManager)
			return;
		if (!m_gVoNChannelsManager.m_eOnRoomChanged)
			return;
		
		m_gVoNChannelsManager.m_eOnRoomChanged.Remove(MovePlayer);
	}
	
	private int m_iOldPlayersCount = 0;
	private int m_iOldRoomsCount = 0;
		
	// -------------------- Update content functions --------------------
	void Clear()
	{
		SCR_WidgetHelper.RemoveAllChildren(m_wRoomsList);
		m_wRooms.Clear();
	}
	
	void Rebuild()
	{
		Clear();

		PS_DebugLogger.LogImportant("VoN Rebuild BEGIN playerId=" + m_iPlayerId.ToString() + " faction=" + m_sCurrentFactionKey);
		
		// Create initial list of visible rooms
		array<int> visibleRooms = new array<int>();
		GetVisibleRooms(visibleRooms);
		PS_DebugLogger.LogImportant("VoN Rebuild visibleRooms count=" + visibleRooms.Count().ToString());
		foreach (int roomId : visibleRooms)
		{
			string name = m_gVoNChannelsManager.GetRoomName(roomId);
			PS_DebugLogger.LogImportant("VoN Rebuild creating room id=" + roomId.ToString() + " name=" + name);
			CreateRoom(roomId);
		}

		UpdateInfo();
	}
	
	void CreateRoomIfNeed(int roomId)
	{
		if (roomId < 0) return;
		string roomKey = m_gVoNChannelsManager.GetRoomName(roomId);
		if (roomKey == "") return;
		if (m_gVoNChannelsManager.IsNoSoundChannel(roomKey)) return;
		if (roomKey.Contains("#PS-VoNRoom_Local") && !roomKey.EndsWith(m_iPlayerId.ToString())) return;
		if (m_sCurrentFactionKey != "")
		{
			if (!roomKey.StartsWith("|") && !roomKey.StartsWith(m_sCurrentFactionKey + "|"))
				return;
		}
		else
		{
			if (!roomKey.StartsWith("|"))
				return;
		}
		CreateRoom(roomId);
	}
	
	void CreateRoom(int roomId)
	{
		string roomKey = m_gVoNChannelsManager.GetRoomName(roomId);
		if (roomKey == "") return;
		Widget roomWidget = GetGame().GetWorkspace().CreateWidgets(m_sVoiceChatRoomPrefab);
		PS_VoiceChatRoom voiceChatRoom = PS_VoiceChatRoom.Cast(roomWidget.FindHandler(PS_VoiceChatRoom));
		voiceChatRoom.SetRoomId(roomId);
		
		array<int> playersInRoom = new array<int>();
		m_gVoNChannelsManager.GetPlayersInRoom(playersInRoom, roomId);
		foreach (int playerId : playersInRoom)
		{
			voiceChatRoom.AddPlayer(playerId);
		}
		
		m_wRoomsList.AddChild(roomWidget);
		m_wRooms[roomId] = voiceChatRoom;
	}
	
	void RemoveRoomIfNeed(int roomId)
	{
		if (m_gVoNChannelsManager.IsGlobalRoom(roomId)) return;
		if (m_gVoNChannelsManager.IsLocalRoom(roomId)) return;
		
		// current player
		if (m_gVoNChannelsManager.IsPublicRoom(roomId))
		{
			if (m_iPublicRoomId != roomId)
			{
				array<int> playersInRoom = new array<int>();
				m_gVoNChannelsManager.GetPlayersInRoom(playersInRoom, roomId);
				if (playersInRoom.IsEmpty())
					RemoveRoom(roomId);
			}
		} else {
			if (!m_gVoNChannelsManager.IsFactionRoom(roomId, m_sCurrentFactionKey) && m_gVoNChannelsManager.GetPlayerRoom(m_iPlayerId) != roomId)
			{
				RemoveRoom(roomId);
			}
		}
	}
	
	void RemoveRoom(int roomId)
	{
		if (!m_wRooms.Contains(roomId)) return;
		PS_VoiceChatRoom voiceChatRoom = m_wRooms[roomId];
		voiceChatRoom.GetRootWidget().RemoveFromHierarchy();
		m_wRooms.Remove(roomId);
	}
	
	void SwitchFaction(FactionKey factionKey)
	{
		m_sCurrentFactionKey = factionKey;
		Rebuild();
	}
	
	FactionKey GetFactionKey()
	{
		return m_sCurrentFactionKey;
	}
	
	void MovePlayer(int playerId, int roomId, int oldRoomId)
	{
		// remove from old room
		if (m_wRooms.Contains(oldRoomId))
		{
			m_wRooms[oldRoomId].RemovePlayer(playerId);
			RemoveRoomIfNeed(oldRoomId);
		}
		if (!m_wRooms.Contains(roomId))
			CreateRoomIfNeed(roomId);
		else if (playerId >= 0)
		{
			m_wRooms[roomId].AddPlayer(playerId);
		}
		
		// ensure all visible rooms exist (catch rooms created server-side but no player moved to yet)
		array<int> visibleRooms = {};
		GetVisibleRooms(visibleRooms);
		foreach (int vr : visibleRooms)
		{
			if (vr >= 0 && !m_wRooms.Contains(vr))
				CreateRoom(vr);
		}
		
		UpdateInfo();
	}
	
	void UpdateInfo()
	{
		foreach (int roomId, PS_VoiceChatRoom voiceChatRoom : m_wRooms)
		{
			voiceChatRoom.UpdateInfo();
		}
	}
	
	// ----- Actions -----
	protected void Action_PlayerClick(SCR_ButtonBaseComponent playerSelector)
	{	
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.CLICK);
	}
	
	// -------------------- Extra lobby functions --------------------
	void GetVisibleRooms(out array<int> outRoomsArray)
	{
		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (!gameMode)
			return;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		if (!playableManager)
			return;
		PS_VoNChannelsManager VoNChannelsManager = PS_VoNChannelsManager.GetInstance();
		if (!VoNChannelsManager)
			return;
		SCR_EGameModeState gameState = gameMode.GetState();

		PlayerController currentPlayerController = GetGame().GetPlayerController();
		if (!currentPlayerController)
			return;
		int currentPlayerId = currentPlayerController.GetPlayerId();

		RplId playerSlot = playableManager.GetPlayableByPlayer(currentPlayerId);
		bool isSpectator = (gameState == SCR_EGameModeState.GAME && playerSlot == RplId.Invalid());
		bool isLobby = (gameState == SCR_EGameModeState.PREGAME || gameState == SCR_EGameModeState.SLOTSELECTION);

		FactionKey actualPlayerFaction = playableManager.GetPlayerFactionKey(currentPlayerId);
		FactionKey currentPlayerFactionKey;
		bool isPreview = (gameState == SCR_EGameModeState.PREGAME);
		if (isLobby && !isPreview)
		{
			if (actualPlayerFaction != "")
				currentPlayerFactionKey = actualPlayerFaction;
			else
				currentPlayerFactionKey = m_sCurrentFactionKey;
		}
		else if (isSpectator)
		{
			currentPlayerFactionKey = actualPlayerFaction;
		}
		else
		{
			currentPlayerFactionKey = actualPlayerFaction;
		}
		if (currentPlayerFactionKey == "")
			currentPlayerFactionKey = m_sCurrentFactionKey;
		m_sCurrentFactionKey = currentPlayerFactionKey;

		if (isLobby && !isPreview)
		{
			if (currentPlayerFactionKey != "")
			{
				int localRoom = VoNChannelsManager.GetRoomWithFaction("", "#PS-VoNRoom_Local" + currentPlayerId.ToString());
				if (localRoom >= 0) outRoomsArray.Insert(localRoom);

				int commandRoom = VoNChannelsManager.GetOrCreateRoomWithFaction(currentPlayerFactionKey, "#PS-VoNRoom_Command");
				outRoomsArray.Insert(commandRoom);

				array<PS_PlayableContainer> playables = playableManager.GetPlayablesSorted();
				for (int i = 0; i < playables.Count(); i++)
				{
					PS_PlayableContainer playable = playables[i];
					FactionKey fk = playable.GetFactionKey();
					if (currentPlayerFactionKey != fk) continue;
					int groupCallSign = playableManager.GetGroupCallsignByPlayable(playable.GetRplId());
					int groupRoom = VoNChannelsManager.GetOrCreateRoomWithFaction(fk, groupCallSign.ToString());
					if (!outRoomsArray.Contains(groupRoom))
						outRoomsArray.Insert(groupRoom);
				}
			}
		}
		else if (isPreview)
		{
			int localRoom = VoNChannelsManager.GetRoomWithFaction("", "#PS-VoNRoom_Local" + currentPlayerId.ToString());
			if (localRoom >= 0) outRoomsArray.Insert(localRoom);

			int globalRoom = VoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Global");
			outRoomsArray.Insert(globalRoom);

			// Show all players public rooms in preview so everyone can see and join them
			array<int> playerIds = {};
			GetGame().GetPlayerManager().GetPlayers(playerIds);
			foreach (int pid : playerIds)
			{
				int publicRoom = VoNChannelsManager.GetRoomWithFaction("", "#PS-VoNRoom_Public" + pid.ToString());
				if (publicRoom >= 0 && !outRoomsArray.Contains(publicRoom))
					outRoomsArray.Insert(publicRoom);
			}
		}
		else if (gameState == SCR_EGameModeState.BRIEFING)
		{
			if (currentPlayerFactionKey != "")
			{
				int factionRoom = VoNChannelsManager.GetOrCreateRoomWithFaction(currentPlayerFactionKey, "#PS-VoNRoom_Faction");
				outRoomsArray.Insert(factionRoom);

				int commandRoom = VoNChannelsManager.GetOrCreateRoomWithFaction(currentPlayerFactionKey, "#PS-VoNRoom_Command");
				outRoomsArray.Insert(commandRoom);

				RplId playableId = playableManager.GetPlayableByPlayer(currentPlayerId);
				if (playableId != RplId.Invalid())
				{
					int groupCallSign = playableManager.GetGroupCallsignByPlayable(playableId);
					int groupRoom = VoNChannelsManager.GetOrCreateRoomWithFaction(currentPlayerFactionKey, groupCallSign.ToString());
					outRoomsArray.Insert(groupRoom);
				}
			}
		}
		else if (gameState == SCR_EGameModeState.GAME && isSpectator)
		{
			int localRoom = VoNChannelsManager.GetRoomWithFaction("", "#PS-VoNRoom_Local" + currentPlayerId.ToString());
			if (localRoom >= 0) outRoomsArray.Insert(localRoom);

			int globalRoom = VoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Global");
			outRoomsArray.Insert(globalRoom);

			int publicRoom = VoNChannelsManager.GetRoomWithFaction("", "#PS-VoNRoom_Public" + currentPlayerId.ToString());
			if (publicRoom >= 0) outRoomsArray.Insert(publicRoom);

			array<int> playersPublicRooms = {};
			VoNChannelsManager.GetPlayersPublicRooms(playersPublicRooms);
			foreach (int roomId : playersPublicRooms)
			{
				if (!outRoomsArray.Contains(roomId))
					outRoomsArray.Insert(roomId);
			}
		}
		else
		{
			int localRoom = VoNChannelsManager.GetRoomWithFaction("", "#PS-VoNRoom_Local" + currentPlayerId.ToString());
			if (localRoom >= 0) outRoomsArray.Insert(localRoom);

			int globalRoom = VoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Global");
			outRoomsArray.Insert(globalRoom);

			if (currentPlayerFactionKey != "")
			{
				int factionRoom = VoNChannelsManager.GetRoomWithFaction(currentPlayerFactionKey, "#PS-VoNRoom_Faction");
				if (factionRoom >= 0) outRoomsArray.Insert(factionRoom);

				int commandRoom = VoNChannelsManager.GetRoomWithFaction(currentPlayerFactionKey, "#PS-VoNRoom_Command");
				if (commandRoom >= 0) outRoomsArray.Insert(commandRoom);

				RplId myPlayableId = playableManager.GetPlayableByPlayer(currentPlayerId);
				if (myPlayableId != RplId.Invalid())
				{
					int myGroupCallSign = playableManager.GetGroupCallsignByPlayable(myPlayableId);
					int myGroupRoom = VoNChannelsManager.GetRoomWithFaction(currentPlayerFactionKey, myGroupCallSign.ToString());
					if (myGroupRoom >= 0 && !outRoomsArray.Contains(myGroupRoom))
						outRoomsArray.Insert(myGroupRoom);
				}
			}

			int publicRoom = VoNChannelsManager.GetRoomWithFaction("", "#PS-VoNRoom_Public" + currentPlayerId.ToString());
			if (publicRoom >= 0) outRoomsArray.Insert(publicRoom);

			array<int> playersPublicRooms = {};
			VoNChannelsManager.GetPlayersPublicRooms(playersPublicRooms);
			foreach (int roomId : playersPublicRooms)
			{
				if (!outRoomsArray.Contains(roomId))
					outRoomsArray.Insert(roomId);
			}
		}

		int currentRoom = VoNChannelsManager.GetPlayerRoom(currentPlayerId);
		if (currentRoom >= 0 && !outRoomsArray.Contains(currentRoom))
		{
			string currentRoomName = VoNChannelsManager.GetRoomName(currentRoom);
			if (!VoNChannelsManager.IsNoSoundChannel(currentRoomName))
				outRoomsArray.Insert(currentRoom);
		}
	}
	
	void SetSelectedPlayer(int playerId)
	{
		foreach (PS_VoiceChatRoom room : m_wRooms)
		{
			room.SetSelectedPlayer(playerId);
		}
	}
};


























