// Coop game mode
// Open lobby on game start
// Disable respawn logic

class PS_GameModeCoopClass : SCR_BaseGameModeClass
{
}

class PS_GameModeCoop : SCR_BaseGameMode
{
	[RplProp(), Attribute("120000", UIWidgets.EditBox, "Time during which disconnected players reserve role for reconnection in ms, -1 for infinity time", "", category: "Reforger Lobby")]
	int m_iReconnectTime;

	[Attribute("-1", UIWidgets.EditBox, "Time during which disconnected players reserve role for reconnection in ms, -1 for infinity time", "", category: "Reforger Lobby")]
	int m_iReconnectTimeAfterBriefing;

	[Attribute("1", uiwidget: UIWidgets.CheckBox, "Game may be started only if admin on server.", category: "Reforger Lobby")]
	protected bool m_bAdminMode;

	[RplProp(), Attribute("0", uiwidget: UIWidgets.CheckBox, "Anyone can open lobby in game stage.", category: "Reforger Lobby")]
	protected bool m_bTeamSwitch;

	[RplProp(), Attribute("0", uiwidget: UIWidgets.CheckBox, "Faction locked after selection.", category: "Reforger Lobby")]
	protected bool m_bFactionLock;

	[RplProp(), Attribute("0", uiwidget: UIWidgets.CheckBox, "Markers can be placed only by squad leaders and only on briefing.", category: "Reforger Lobby")]
	protected bool m_bMarkersOnlyOnBriefing;

	[Attribute("0", UIWidgets.CheckBox, "Instead of just the leaders, every member is moved to their factions HQ room for a common briefing.\nMoving back to group channel is still possible.", category: "Reforger Lobby")]
	bool m_bPublicCommandBriefing;

	[RplProp(), Attribute("0", uiwidget: UIWidgets.CheckBox, "Remove units not occupied by players.", category: "Reforger Lobby")]
	protected bool m_bRemoveRedundantUnits;

	[RplProp(), Attribute("0", uiwidget: UIWidgets.CheckBox, "Remove default markers on squad leaders.", category: "Reforger Lobby")]
	protected bool m_bRemoveSquadMarkers;

	[RplProp(), Attribute("60000", UIWidgets.EditBox, "Time in milliseconds before restriction zones are removed.", category: "Reforger Lobby")]
	int m_iFreezeTime;
	
	[Attribute("0", UIWidgets.EditBox, "Time in milliseconds before characters are activated.", category: "Reforger Lobby (WIP)")]
	int m_iDisableTime;

	[RplProp(), Attribute("0", UIWidgets.CheckBox, "Disables text chat for alive players on game stage. Admins can always see text chat.", category: "Reforger Lobby")]
	protected bool m_bDisableChat;

	[RplProp()]
	float m_fCurrentFreezeTime = 1;
	[RplProp()]
	protected float m_fGameStartTime = 0;
	[RplProp()]
	protected float m_fGameStartElapsedTime = 0;

	[Attribute("0", uiwidget: UIWidgets.CheckBox, "Creates a whitelist on the server for players who have taken roles and also for players specified in $profile:PS_SlotsReserver_Config.json and kicks everyone else.", category: "Reforger Lobby")]
	protected bool m_bReserveSlots;

	[Attribute("3", UIWidgets.EditBox, "Ready countdown in seconds before game auto-starts when all players are ready.", "", category: "Reforger Lobby")]
	int m_iReadyCountdown;

	[Attribute("0", UIWidgets.CheckBox, "", category: "Reforger Lobby")]
	protected bool m_bDisableVanillaGroupMenu;

	[Attribute("0", UIWidgets.CheckBox, "", category: "Reforger Lobby")]
	protected bool m_bDisablePlayablesStreaming;
	
	[Attribute("0", UIWidgets.CheckBox, "", category: "Reforger Lobby")]
	protected bool m_bDisableGarbageSystem;

	[RplProp(), Attribute("0", UIWidgets.CheckBox, "", category: "Reforger Lobby")]
	protected bool m_bFriendliesSpectatorOnly;

	[Attribute("0", UIWidgets.CheckBox, "", category: "Reforger Lobby")]
	protected bool m_bFreezeTimeShootingForbiden;
	
	[RplProp(), Attribute("1", UIWidgets.CheckBox, "", category: "Reforger Lobby")]
	protected bool m_bDisableArmaVision;
	
	[Attribute("0", UIWidgets.CheckBox, "", category: "Reforger Lobby")]
	protected bool m_bDisableBuildingModeAfterFreezeTime;
	
	[RplProp(), Attribute("-1", UIWidgets.Auto, "Max difference between most and least populated factions allowed. -1 to disable.", "", category: "Reforger Lobby")]
	protected int m_iFactionsBalance;
	
	[Attribute("0", UIWidgets.CheckBox, "", category: "Reforger Lobby (WIP)")]
	protected bool m_bShowCutscene;

	[Attribute("1", UIWidgets.CheckBox, "", category: "Reforger Lobby (WIP)")]
	protected bool m_bHolsterWeapon;

	[Attribute("0", UIWidgets.Auto, "", category: "Reforger Lobby (WIP)")]
	protected int m_iForceMenuFramerate;
	protected static int m_iOldMenuFramerate;

	protected ref ScriptInvokerInt m_OnGameStateChange = new ScriptInvokerInt();
	ScriptInvokerInt GetOnGameStateChange()
	{
		return m_OnGameStateChange;
	}

	protected ref ScriptInvokerString m_OnOnlyOneFactionAlive = new ScriptInvokerString();
	ScriptInvokerString GetOnOnlyOneFactionAlive()
	{
		return m_OnOnlyOneFactionAlive;
	}

	protected ref ScriptInvoker m_OnHandlePlayerKilled = new ScriptInvoker();
	ScriptInvoker GetOnHandlePlayerKilled()
	{
		return m_OnHandlePlayerKilled;
	}

	// Cache global
	protected PS_PlayableManager m_playableManager;
	protected PS_CutsceneManager m_CutsceneManager;

	// Re-entrancy guard: tracks players already sent through the death-screen flow
	// so HandlePlayerKilled (which can fire multiple times for the same death) doesn't
	// double-trigger the death screen or spectator transition.
	protected ref set<int> m_DeathScreenSent = new set<int>();

	// ------------------------------------------ Events ------------------------------------------
	
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		if (!GetGame().InPlayMode() || !Replication.IsServer()){
			return;
		}
		World world = GetGame().GetWorld();
		SCR_GarbageSystem garbageSystem = SCR_GarbageSystem.Cast(world.FindSystem(SCR_GarbageSystem));
		if (garbageSystem)
			garbageSystem.Enable(!m_bDisableGarbageSystem);
	}
	
  override void OnGameStart()
  {
    super.OnGameStart();

    PS_DebugLogger.LogImportant("PS_GameModeCoop OnGameStart isServer=" + Replication.IsServer().ToString() + " rplMode=" + RplSession.Mode().ToString());

    InputManager inputManager = GetGame().GetInputManager();
		if (inputManager && m_bDisableVanillaGroupMenu)
		{
			inputManager.RemoveActionListener("ShowScoreboard", EActionTrigger.DOWN, ArmaReforgerScripted.OnShowPlayerList);
			inputManager.RemoveActionListener("ShowGroupMenu", EActionTrigger.DOWN, ArmaReforgerScripted.OnShowGroupMenu);
		}

		Widget FreezeTimeCounterOverlay = GetGame().GetWorkspace().FindAnyWidget("FreezeTimeCounterOverlay");
		if (FreezeTimeCounterOverlay)
			FreezeTimeCounterOverlay.RemoveFromHierarchy();

		m_playableManager = PS_PlayableManager.GetInstance();
		m_CutsceneManager = PS_CutsceneManager.GetInstance();

		if (Replication.IsServer())
		{
			PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();
			if (vonManager)
				vonManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Global");

			m_fCurrentFreezeTime = m_iReconnectTime;
		}

		if (RplSession.Mode() != RplMode.Dedicated) {
			GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.WaitScreen);
			GetGame().GetInputManager().AddActionListener("OpenLobby", EActionTrigger.DOWN, Action_OpenLobby);
		}

		GetGame().GetCallqueue().CallLater(AddAdvanceAction, 0, false);

		GetGame().GetCallqueue().CallLater(RegisterEditorClosed, 100, false);

		if (!Replication.IsServer() && m_iForceMenuFramerate != 0)
		{
			BaseContainer video = GetGame().GetEngineUserSettings().GetModule("VideoUserSettings");
			video.Get("MaxFps", m_iOldMenuFramerate);
			video.Set("MaxFps", m_iForceMenuFramerate);
			GetGame().GetCallqueue().CallLater(ForceFramerate, 1000, true);
		}
	}
	protected BaseContainer m_CachedVideoSettings;

	void ForceFramerate()
	{
		if (!m_CachedVideoSettings)
		{
			UserSettings videoSettings = GetGame().GetEngineUserSettings();
			if (!videoSettings)
				return;
			m_CachedVideoSettings = videoSettings.GetModule("VideoUserSettings");
		}
		if (!m_CachedVideoSettings)
			return;

		PS_GameModeCoop gm = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (gm && gm.GetState() == SCR_EGameModeState.GAME)
		{
			m_CachedVideoSettings.Set("MaxFps", m_iOldMenuFramerate);
			GetGame().UserSettingsChanged();

			GetGame().GetCallqueue().Remove(ForceFramerate);
		}
		else
		{
			int currentFramerate;
			m_CachedVideoSettings.Get("MaxFps", currentFramerate);
			if (currentFramerate != m_iForceMenuFramerate)
			{
				m_CachedVideoSettings.Set("MaxFps", m_iForceMenuFramerate);
				GetGame().UserSettingsChanged();
			}
		}
	}

	void RegisterEditorClosed()
	{
		SCR_EditorModeEntity editorModeEntity = SCR_EditorModeEntity.GetInstance();
		if (editorModeEntity)
		{
			editorModeEntity.GetOnClosed().Insert(EditorClosed);
		}
		else
			GetGame().GetCallqueue().CallLater(RegisterEditorClosed, 100, false);
	}
	
	void FreezeTimerAdvance(int time)
	{
		time = time * 1000;
		m_iFreezeTime += time;
		m_fCurrentFreezeTime += time;
		GetGame().GetCallqueue().Remove(restrictedZonesTimer);
		restrictedZonesTimer(m_fCurrentFreezeTime);
		
		FreezeTimerAdvance_Notify();
	}
	
	void FreezeTimerEnd()
	{
		GetGame().GetCallqueue().Remove(restrictedZonesTimer);
		restrictedZonesTimer(5000);
		
		FreezeTimerEnd_Notify();
	}

	void EditorClosed()
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return;
		PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
		if (!playableController)
			return;

		playableController.SaveCameraTransform();
		playableController.SwitchFromObserver();
		
		IEntity entity = playerController.GetControlledEntity();
		if (!entity)
		{
			playableController.SwitchToMenu(SCR_EGameModeState.GAME);
			return;
		}
		
		PS_LobbyVoNComponent von = PS_LobbyVoNComponent.Cast(entity.FindComponent(PS_LobbyVoNComponent));
		if (von)
		{
			playableController.SwitchToMenu(SCR_EGameModeState.GAME);
			return;
		}
	}
	
	void AddAdvanceAction()
	{
		SCR_ChatPanelManager chatPanelManager = SCR_ChatPanelManager.GetInstance();
		if (!chatPanelManager)
			return;
		ChatCommandInvoker invoker = chatPanelManager.GetCommandInvoker("adv");
		invoker.Insert(AdvanceStage_Callback);
		invoker = chatPanelManager.GetCommandInvoker("lom");
		invoker.Insert(LoadMap_Callback);
		invoker = chatPanelManager.GetCommandInvoker("sav");
		invoker.Insert(ExportMissionData_Callback);
		invoker = chatPanelManager.GetCommandInvoker("tst");
		invoker.Insert(Test_Callback);
		invoker = chatPanelManager.GetCommandInvoker("pgc");
		invoker.Insert(PlayGameConfig_Callback);
		invoker = chatPanelManager.GetCommandInvoker("res");
		invoker.Insert(Respawn_Callback);
		invoker = chatPanelManager.GetCommandInvoker("rei");
		invoker.Insert(RespawnInit_Callback);
		invoker = chatPanelManager.GetCommandInvoker("unc");
		invoker.Insert(ForceUnconscious_Callback);
		invoker = chatPanelManager.GetCommandInvoker("spw");
		invoker.Insert(SpawnInit_Callback);
		invoker = chatPanelManager.GetCommandInvoker("spp");
		invoker.Insert(SpawnPosition_Callback);
		invoker = chatPanelManager.GetCommandInvoker("fta");
		invoker.Insert(FreezeTimerAdvance_Callback);
		invoker = chatPanelManager.GetCommandInvoker("fte");
		invoker.Insert(FreezeTimerEnd_Callback);
		invoker = chatPanelManager.GetCommandInvoker("cmc");
		invoker.Insert(CopyAllMarkersToClipboard_Callback);
		invoker = chatPanelManager.GetCommandInvoker("lmc");
		invoker.Insert(LoadAllMarkersToClipboard_Callback);
	}
	
	
	void CopyAllMarkersToClipboard_Callback(SCR_ChatPanel panel, string data)
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playableManager.IsPlayerGroupLeader(playerController.GetPlayerId())) return;
		
		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		array<SCR_MapMarkerBase> markers = markerMgr.GetStaticMarkers();
		
		PS_MapMarkersBaseJson mapMarkers = new PS_MapMarkersBaseJson();
		foreach (SCR_MapMarkerBase marker : markers)
		{
			PS_MapMarkerBaseJson markerJson = marker.PS_GetMapMarkerBaseJson();
			mapMarkers.m_aMapMarkers.Insert(markerJson);
		}
		JsonSaveContext saveContext = new JsonSaveContext();
		saveContext.WriteValue("", mapMarkers);
		System.ExportToClipboard(saveContext.SaveToString());
	}
	
	
	void LoadAllMarkersToClipboard_Callback(SCR_ChatPanel panel, string data)
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playableManager.IsPlayerGroupLeader(playerController.GetPlayerId())) return;
		
		if (GetState() != SCR_EGameModeState.BRIEFING)
			return;
		
		string json = System.ImportFromClipboard();
		
		JsonLoadContext loadContext = new JsonLoadContext();
		loadContext.LoadFromString(json);
		
		PS_MapMarkersBaseJson mapMarkers = new PS_MapMarkersBaseJson();
		loadContext.ReadValue("", mapMarkers);
		
		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		foreach (PS_MapMarkerBaseJson markerJson : mapMarkers.m_aMapMarkers)
		{
			SCR_MapMarkerBase marker = markerJson.GetMapMarkerBase();
			marker.SetMarkerFactionFlags(0);
			FactionManager factionManager = GetGame().GetFactionManager();
			if (factionManager)
			{
				Faction markerOwnerFaction = SCR_FactionManager.SGetPlayerFaction(GetGame().GetPlayerController().GetPlayerId());
				if (markerOwnerFaction)
					marker.AddMarkerFactionFlags(factionManager.GetFactionIndex(markerOwnerFaction));
			}
			
			markerMgr.InsertStaticMarker(marker, false, false);
		}
	}
	
	void FreezeTimerAdvance_Callback(SCR_ChatPanel panel, string data)
	{
		if (!PS_PlayersHelper.IsAdminOrServer())
			return;
		
		PlayerController playerController = GetGame().GetPlayerController();
		PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
		if (!playableController)
			return;
		
		if (GetState() != SCR_EGameModeState.GAME || IsFreezeTimeEnd())
			return;
		
		playableController.FreezeTimerAdvance(data.ToInt());
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void FreezeTimerAdvance_Notify()
	{
		SCR_ChatPanelManager chatPanelManager = SCR_ChatPanelManager.GetInstance();
		ChatCommandInvoker invoker = chatPanelManager.GetCommandInvoker("smsg");
		invoker.Invoke(null, "#PS-Freeze_time_advanced");
	}
	
	void FreezeTimerEnd_Callback(SCR_ChatPanel panel, string data)
	{
		if (!PS_PlayersHelper.IsAdminOrServer())
			return;
		
		PlayerController playerController = GetGame().GetPlayerController();
		PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
		if (!playableController)
			return;
		
		if (GetState() != SCR_EGameModeState.GAME || IsFreezeTimeEnd())
			return;
		
		playableController.FreezeTimerEnd();
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void FreezeTimerEnd_Notify()
	{
		SCR_ChatPanelManager chatPanelManager = SCR_ChatPanelManager.GetInstance();
		ChatCommandInvoker invoker = chatPanelManager.GetCommandInvoker("smsg");
		invoker.Invoke(null, "#PS-Freeze_time_force_end");
	}

	void SpawnPosition_Callback(SCR_ChatPanel panel, string data)
	{
		PlayerController playerController = GetGame().GetPlayerController();
		PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
		if (!playableController)
			return;
		
		array<string> outTokens = {};
		data.Split(" ", outTokens, true);
		string positionStr = outTokens[0];
		positionStr.Replace("|", " ");
		positionStr.Replace("<", " ");
		positionStr.Replace(">", " ");
		positionStr.Replace(",", " ");
		vector position = positionStr.ToVector();
		
		playableController.SpawnPrefab(data, position);
	}
	
	void SpawnInit_Callback(SCR_ChatPanel panel, string data)
	{
		PlayerController playerController = GetGame().GetPlayerController();
		PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
		if (!playableController)
			return;

		playableController.SpawnPrefab(data, "0 0 0");
	}

	void ForceUnconscious_Callback(SCR_ChatPanel panel, string data)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
			return;
		CharacterControllerComponent characterControllerComponent = character.GetCharacterController();
		if (characterControllerComponent.IsUnconscious())
			return;
		characterControllerComponent.SetUnconscious(true);
		GetGame().GetCallqueue().CallLater(ResetUnconscious, 400, false, characterControllerComponent);
	}
	void ResetUnconscious(CharacterControllerComponent characterControllerComponent)
	{
		characterControllerComponent.SetUnconscious(false);
	}

	void Respawn_Callback(SCR_ChatPanel panel, string data)
	{
		PlayerController playerController = GetGame().GetPlayerController();
		PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
		if (!playableController)
			return;

		playableController.ForceRespawnPlayer();
	}

	void RespawnInit_Callback(SCR_ChatPanel panel, string data)
	{
		PlayerController playerController = GetGame().GetPlayerController();
		PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
		if (!playableController)
			return;

		playableController.ForceRespawnPlayer(true);
	}

	void Test_Callback(SCR_ChatPanel panel, string data)
	{
		MemoryStatsSnapshot snapshot = new MemoryStatsSnapshot();
		int statsCount = MemoryStatsSnapshot.GetStatsCount();
		for (int i = 0; i < statsCount; i++)
		{
			Print(MemoryStatsSnapshot.GetStatName(i));
			Print(snapshot.GetStatValue(i));
		}
	}

	void ExportMissionData_Callback(SCR_ChatPanel panel, string data)
	{
		PS_MissionDataManager.GetInstance().WriteToFile();
	}

	void LoadMap_Callback(SCR_ChatPanel panel, string data)
	{
		PlayerController playerController = GetGame().GetPlayerController();
		PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));

		PlayerManager playerManager = GetGame().GetPlayerManager();
		EPlayerRole playerRole = playerManager.GetPlayerRoles(playerController.GetPlayerId());
		if (!PS_PlayersHelper.IsAdminOrServer()) return;

		playableController.LoadMission(data);
	}

	void AdvanceStage_Callback(SCR_ChatPanel panel, string data)
	{
		PlayerController playerController = GetGame().GetPlayerController();
		PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));

		PlayerManager playerManager = GetGame().GetPlayerManager();
		EPlayerRole playerRole = playerManager.GetPlayerRoles(playerController.GetPlayerId());
		if (!PS_PlayersHelper.IsAdminOrServer()) return;

		playableController.AdvanceGameState(SCR_EGameModeState.NULL);
	}

	void PlayGameConfig_Callback(SCR_ChatPanel panel, string data)
	{
		if (data == "")
			return;
		if (!PS_PlayersHelper.IsAdminOrServer())
			return;
		Resource resource = BaseContainerTools.LoadContainer(data);
		if (!resource)
			return;
		GameStateTransitions.RequestScenarioChangeTransition(data, "", "");
	}

	void removeRestrictedZones()
	{
		BaseGameMode gamemode = GetGame().GetGameMode();
		SCR_PlayersRestrictionZoneManagerComponent restrictionZoneManager = SCR_PlayersRestrictionZoneManagerComponent.Cast(gamemode.FindComponent(SCR_PlayersRestrictionZoneManagerComponent));
		if (!restrictionZoneManager)
			return;
		set<SCR_EditorRestrictionZoneEntity> zones = restrictionZoneManager.GetZones();

		SCR_ChatPanelManager chatPanelManager = SCR_ChatPanelManager.GetInstance();
		ChatCommandInvoker invoker = chatPanelManager.GetCommandInvoker("smsg");
		invoker.Invoke(null, "#PS-Freeze_End");

		array<int> playerIds = new array<int>();
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			restrictionZoneManager.ResetPlayerZoneData(playerId);
		}

		for (int i = 0; i < zones.Count(); i++)
		{
			SCR_EditorRestrictionZoneEntity zone = zones.Get(i);
			SCR_EntityHelper.DeleteEntityAndChildren(zone);
		}
	}

	protected override void OnPlayerConnected(int playerId)
  {
    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
    PlayerManager playerManager = GetGame().GetPlayerManager();
    string name = playerManager.GetPlayerName(playerId);
    string guid = GetGame().GetBackendApi().GetPlayerPlatformId(playerId);
    playableManager.SetPlayerInfo(playerId, name, guid);

    SCR_EGameModeState currentState = GetState();
    PS_DebugLogger.LogImportant("OnPlayerConnected player=" + playerId.ToString() + " name=" + name + " gameModeState=" + typename.EnumToString(SCR_EGameModeState, currentState), playerId);

    RplId parkedSlot;
    if (playableManager.FindDisconnectedPlayerByGUID(guid, parkedSlot))
    {
      PS_DebugLogger.LogImportant("OnPlayerConnected RECONNECT player=" + playerId.ToString() + " recovered slot=" + parkedSlot.ToString() + " slotDestroyed=" + playableManager.IsSlotCharacterDestroyed(parkedSlot).ToString() + " gameModeState=" + typename.EnumToString(SCR_EGameModeState, currentState), playerId);

      playableManager.RemoveDisconnectedPlayerInfo(guid);
      playableManager.UpdatePlayerReconnected(playerId, guid);
      if (parkedSlot != RplId.Invalid() && !PS_PlayableManager.GetInstance().IsSlotCharacterDestroyed(parkedSlot))
      {
        playableManager.SetPlayerFactionKey(playerId, playableManager.GetSlotFactionKey(parkedSlot));

        // Fix #5: Set Playing state immediately on reconnect in GAME so clients see correct state
        if (currentState == SCR_EGameModeState.GAME)
        {
          PS_DebugLogger.LogImportant("OnPlayerConnected RECONNECT GAME — setting Playing state immediately then scheduling ApplyPlayable", playerId);
          playableManager.SetPlayerState(playerId, PS_EPlayableControllerState.Playing);
          GetGame().GetCallqueue().CallLater(playableManager.ApplyPlayable, 1000, false, playerId);
        }
      }
      else
      {
        PS_DebugLogger.LogImportant("OnPlayerConnected RECONNECT slot INVALID or DESTROYED — player has no slot", playerId);
      }
    }
    else
    {
      PS_DebugLogger.LogImportant("OnPlayerConnected NEW_PLAYER player=" + playerId.ToString() + " gameModeState=" + typename.EnumToString(SCR_EGameModeState, currentState), playerId);
    }

    // Clear death-screen guard so a reconnected player can die again in a new round.
    m_DeathScreenSent.RemoveItem(playerId);

    // JIP sync: send full state of non-RplProp maps to connecting client
    playableManager.SyncStateToClient(playerId);

    // JIP sync: send all VoN channel state to connecting client.
    // This is needed because InitChannel RPCs are only sent once when rooms
    // are created; a JIP client would otherwise never learn existing room names.
    PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();
    if (vonManager)
      vonManager.SyncChannelsToClient(playerId);

    // Fix JIP voice room assignment: after channel sync, move the player to the
    // correct room based on current game state. Without this, JIP players land in
    // the default Global room even when the game is in BRIEFING or GAME.
    if (vonManager && currentState == SCR_EGameModeState.BRIEFING)
    {
      RplId slotId = playableManager.GetPlayableByPlayer(playerId);
      if (slotId != RplId.Invalid())
      {
        FactionKey fk = playableManager.GetPlayerFactionKey(playerId);
        if (playableManager.IsPlayerTopSlotInGroup(playerId))
          vonManager.MoveToRoom(playerId, fk, "#PS-VoNRoom_Command");
        else
        {
          int groupCallsign = playableManager.GetGroupCallsignByPlayable(slotId);
          vonManager.MoveToRoom(playerId, fk, groupCallsign.ToString());
        }
      }
      else
      {
        vonManager.MoveToRoom(playerId, "", "#PS-VoNRoom_Global");
      }
    }
    else if (vonManager && currentState == SCR_EGameModeState.GAME)
    {
      RplId slotId = playableManager.GetPlayableByPlayer(playerId);
      if (slotId != RplId.Invalid())
      {
        FactionKey fk = playableManager.GetPlayerFactionKey(playerId);
        int groupCallsign = playableManager.GetGroupCallsignByPlayable(slotId);
        vonManager.MoveToRoom(playerId, fk, groupCallsign.ToString());
      }
    }

    if (currentState == SCR_EGameModeState.GAME)
    {
      PS_DebugLogger.LogImportant("OnPlayerConnected GAME state — scheduling SpawnInitialEntity", playerId);
      GetGame().GetCallqueue().CallLater(SpawnInitialEntity, 200, false, playerId);
    }
    else
    {
      #ifdef WORKBENCH
      GetGame().GetCallqueue().CallLater(SpawnInitialEntity, 500, false, playerId);
      #else
      PS_DebugLogger.LogImportant("OnPlayerConnected NON-GAME state — scheduling SpawnInitialEntity 100ms", playerId);
      GetGame().GetCallqueue().CallLater(SpawnInitialEntity, 100, false, playerId);
      #endif
    }
    m_OnPlayerConnected.Invoke(playerId);
  }

	protected override bool HandlePlayerKilled(int playerId, IEntity playerEntity, IEntity killerEntity, notnull Instigator killer)
	{
		m_OnHandlePlayerKilled.Invoke(playerId, playerEntity, killerEntity, killer);

		Print("[DS][SRV] HandlePlayerKilled FIRED player=" + playerId.ToString() + " state=" + typename.EnumToString(SCR_EGameModeState, GetState()) + " hasEntity=" + (playerEntity != null).ToString(), LogLevel.NORMAL);

		// Re-entrancy guard: HandlePlayerKilled can fire multiple times per death (e.g.
		// from multiple damage sources, or from the engine's repeated kill notifications).
		// Once we have sent the death-screen flow, never send it again for the same player.
		if (m_DeathScreenSent.Contains(playerId))
		{
			Print("[DS][SRV] HandlePlayerKilled REJECTED player=" + playerId.ToString() + " already in death-screen flow", LogLevel.WARNING);
			return super.HandlePlayerKilled(playerId, playerEntity, killerEntity, killer);
		}

		// CoD-style death screen flow: when a player dies in GAME state, the server sends
		// SwitchToDeathScreenServer() to the owning client. The client runs the 10s-eyes +
		// 2s-fade + 8s-quote sequence locally, then chains to the normal spectator flow
		// (PS_DeathScreenMenu.OnMenuClose → SwitchToObserver(null)).
		//
		// Other spectator transitions are NOT wrapped in the death screen:
		//   - JIP without a slot → OnControlledEntityChanged → SwitchToObserverServer(null)
		//   - Game start without a role → RPC_RequestDeployForAllPlayers → SwitchToObserverServer(null)
		if (GetState() == SCR_EGameModeState.GAME && playerId > 0)
		{
			PS_PlayableManager pm = PS_PlayableManager.GetInstance();
			RplId slotId = pm.GetPlayableByPlayer(playerId);
			// Don't check IsSlotCharacterDestroyed here — HandlePlayerKilled fires before
			// OnDamageStateChange(DESTROYED), so damage state may still be INCAPACITATED.
			if (slotId != RplId.Invalid())
			{
				Print("[DS][SRV] HandlePlayerKilled slot=" + slotId.ToString() + " — sending death-screen RPC", LogLevel.NORMAL);
				SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
				if (playerController)
				{
					PS_PlayableControllerComponent pcc = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
					if (pcc)
					{
						// Mark this player as processed so we never double-trigger
						m_DeathScreenSent.Insert(playerId);

						// Send death-screen RPC: client will run the CoD sequence and chain
						// to SwitchToObserver(null) locally when the menu closes.
						vector observerPos = "0 0 0";
						if (playerEntity)
							observerPos = playerEntity.GetOrigin();
						Print("[DS][SRV] HandlePlayerKilled CALLING pcc.SwitchToDeathScreenServer observerPos=" + observerPos.ToString(), LogLevel.NORMAL);
						pcc.SwitchToDeathScreenServer(observerPos);
						// Deferred: spawn lobby entity 200ms later (matches Echo Lobby's SpawnDefaultEntity delay)
						GetGame().GetCallqueue().CallLater(SpawnDefaultEntityForDeadPlayer, 200, false, playerId);
					}
					else
					{
						Print("[DS][SRV] HandlePlayerKilled ABORT pcc NULL", LogLevel.WARNING);
					}
				}
				else
				{
					Print("[DS][SRV] HandlePlayerKilled ABORT playerController NULL", LogLevel.WARNING);
				}
			}
			else
			{
				Print("[DS][SRV] HandlePlayerKilled ABORT slot INVALID — bypassing death screen", LogLevel.WARNING);
			}
		}
		else
		{
			Print("[DS][SRV] HandlePlayerKilled SKIP state=" + typename.EnumToString(SCR_EGameModeState, GetState()) + " playerId=" + playerId.ToString(), LogLevel.WARNING);
		}

		return super.HandlePlayerKilled(playerId, playerEntity, killerEntity, killer);
	}

	// Deferred entity spawn for dead players (Echo Lobby pattern — 200ms delay)
	void SpawnDefaultEntityForDeadPlayer(int playerId)
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!playerController)
			return;
		PS_PlayableControllerComponent pcc = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
		if (!pcc)
			return;
		IEntity initialEntity = pcc.GetInitialEntity();
		if (!initialEntity)
		{
			Resource resource = Resource.Load("{ADDE38E4119816AB}Prefabs/InitialPlayer_Version2.et");
			EntitySpawnParams params = new EntitySpawnParams();
			initialEntity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
			pcc.SetInitialEntity(initialEntity);
		}
		playerController.SetInitialMainEntity(initialEntity);
	}

  protected override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
  {
    PlayerManager playerManager = GetGame().GetPlayerManager();
    SCR_PlayerController playerController = SCR_PlayerController.Cast(playerManager.GetPlayerController(playerId));

    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
    playableManager.SetPlayerState(playerId, PS_EPlayableControllerState.Disconnected);

    // Clean up VoN channel key map so disconnected players don't leave ghost entries
    PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();
    if (vonManager)
      vonManager.OnPlayerDisconnected(playerId, cause, timeout);

    PS_DebugLogger.LogImportant("OnPlayerDisconnected player=" + playerId.ToString() + " gameModeState=" + typename.EnumToString(SCR_EGameModeState, GetState()), playerId);

    string guid = playableManager.GetPlayerGUIDById(playerId);
    RplId controlledSlot;
    if (playableManager.FindPlayerSlotById(playerId, controlledSlot))
    {
      PS_DebugLogger.LogImportant("OnPlayerDisconnected player=" + playerId.ToString() + " had slot=" + controlledSlot.ToString() + " scheduling RemoveDisconnectedPlayer in " + m_iReconnectTime.ToString() + "ms", playerId);
      RplComponent rpl = RplComponent.Cast(Replication.FindItem(controlledSlot));
      if (rpl)
        rpl.GiveExt(RplIdentity.Local(), false);
      playableManager.AddDisconnectedPlayerInfo(guid, controlledSlot, playerId);
      if (GetState() != SCR_EGameModeState.GAME)
        GetGame().GetCallqueue().CallLater(RemoveDisconnectedPlayer, m_iReconnectTime, false, playerId);
      else
      {
        // In GAME state, use m_iReconnectTimeAfterBriefing. -1 means reserve forever.
        if (m_iReconnectTimeAfterBriefing >= 0)
        {
          PS_DebugLogger.LogImportant("OnPlayerDisconnected GAME state — scheduling RemoveDisconnectedPlayer in " + m_iReconnectTimeAfterBriefing.ToString() + "ms", playerId);
          GetGame().GetCallqueue().CallLater(RemoveDisconnectedPlayer, m_iReconnectTimeAfterBriefing, false, playerId);
        }
        else
        {
          PS_DebugLogger.LogImportant("OnPlayerDisconnected GAME state — NOT scheduling RemoveDisconnectedPlayer (m_iReconnectTimeAfterBriefing=-1, infinite)", playerId);
        }
      }
    }
    else
    {
      PS_DebugLogger.LogImportant("OnPlayerDisconnected player=" + playerId.ToString() + " had NO slot, removing directly", playerId);
      playableManager.RemovePlayer(playerId, guid, true);
    }

    IEntity controlledEntity;
    if (playerController)
      controlledEntity = playerController.GetControlledEntity();
    else
      controlledEntity = null;

    if (controlledEntity)
    {
      if (GetGame().GetPlayerManager().IsPlayerConnected(playerId))
      {
        PS_DebugLogger.LogImportant("OnPlayerDisconnected player=" + playerId.ToString() + " reconnected before authority reset, skipping", playerId);
      }
      else
      {
        CharacterControllerComponent charController = CharacterControllerComponent.Cast(controlledEntity.FindComponent(CharacterControllerComponent));
        if (charController)
          charController.SetMovement(0, vector.Forward);
        RplComponent rpl = RplComponent.Cast(controlledEntity.FindComponent(RplComponent));
        if (rpl)
          rpl.GiveExt(RplIdentity.Local(), false);
      }
    }

    SCR_GroupsManagerComponent groupsManagerComponent = SCR_GroupsManagerComponent.GetInstance();
    if (groupsManagerComponent)
    {
      SCR_AIGroup playerGroup = groupsManagerComponent.GetPlayerGroup(playerId);
      if (playerGroup)
        playerGroup.RemovePlayer(playerId);
    }

    // Echo Lobby pattern: do NOT call super.OnPlayerDisconnected() — the vanilla handler
    // destroys the player's controlled entity, which prevents reconnection (the body is
    // gone by the time the player reconnects, so IsSlotCharacterDestroyed() is true and
    // the player falls through to spectator mode).
    // Instead, manually propagate the disconnect event to all game-mode components
    // without triggering the vanilla entity-deletion logic, leaving the physical
    // character intact for the player to re-possess on reconnect.
    m_OnPlayerDisconnected.Invoke(playerId, cause, timeout);

    if (IsMaster() && m_pRespawnSystemComponent)
      m_pRespawnSystemComponent.OnPlayerDisconnected_S(playerId, cause, timeout);

    foreach (SCR_BaseGameModeComponent comp : m_aAdditionalGamemodeComponents)
    {
      comp.OnPlayerDisconnected(playerId, cause, timeout);
    }

    m_OnPostCompPlayerDisconnected.Invoke(playerId, cause, timeout);
  }

	// ------------------------------------------ Faction Balance ------------------------------------------
	bool CanJoinFaction(FactionKey factionKeyPlayer, FactionKey currentFaction)
	{
		PS_DebugLogger.LogImportant("CanJoinFaction START targetFaction=" + factionKeyPlayer + " currentFaction=" + currentFaction + " balance=" + m_iFactionsBalance.ToString());
		
		if (m_iFactionsBalance == -1)
		{
			PS_DebugLogger.Log("CanJoinFaction ALLOW balance disabled");
			return true;
		}
		if (factionKeyPlayer == currentFaction)
		{
			PS_DebugLogger.Log("CanJoinFaction ALLOW same faction");
			return true;
		}

		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		map<FactionKey, int> players = new map<FactionKey, int>();
		map<FactionKey, int> playables = new map<FactionKey, int>();
		foreach (RplId slotId, PS_SlotCharacterData slot : playableManager.GetSlots().GetRawMap())
		{
			FactionKey factionKey = slot.m_FactionKey;

			if (!players.Contains(factionKey))
				players[factionKey] = 0;
			if (!playables.Contains(factionKey))
				playables[factionKey] = 0;

			playables[factionKey] = playables[factionKey] + 1;
			if (slot.m_PlayerId > 0)
				players[factionKey] = players[factionKey] + 1;
		}
		if (currentFaction != "")
			players[currentFaction] = players[currentFaction] - 1;

		float maxFaction = 0;
		foreach (FactionKey factionKey, int count : playables)
			if (maxFaction < count)
				maxFaction = count;

		int minFaction = 999;
		foreach (FactionKey factionKey, int count : players)
		{
			int scaledCount = players[factionKey] * (maxFaction / playables[factionKey]);
			if (minFaction > scaledCount)
				minFaction = scaledCount;
		}

		int currentCount = players[factionKeyPlayer];
		int diff = currentCount - minFaction;

		bool result = diff <= m_iFactionsBalance;
		PS_DebugLogger.LogImportant("CanJoinFaction targetCount=" + currentCount.ToString() + " minScaled=" + minFaction.ToString() + " diff=" + diff.ToString() + " balance=" + m_iFactionsBalance.ToString() + " => " + result.ToString());
		return result;
	}

	// ------------------------------------------ Actions ------------------------------------------
	// Open lobby in game
	void Action_OpenLobby()
	{
		PlayerController playerController = GetGame().GetPlayerController();
		PlayerManager playerManager = GetGame().GetPlayerManager();
		PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
		EPlayerRole playerRole = playerManager.GetPlayerRoles(playerController.GetPlayerId());
		
		if (!m_bTeamSwitch && !PS_PlayersHelper.IsAdminOrServer()) return;

		MenuBase lobbyMenu = GetGame().GetMenuManager().FindMenuByPreset(ChimeraMenuPreset.CoopLobby);
		if (!lobbyMenu)
			GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.CoopLobby);
	}


	// Force open current game state menu
  void OpenCurrentMenuOnClients()
  {
    PS_DebugLogger.LogImportant("OpenCurrentMenuOnClients state=" + typename.EnumToString(SCR_EGameModeState, GetState()));
    if (RplSession.Mode() != RplMode.Dedicated) RPC_OpenCurrentMenu(GetState());
    Rpc(RPC_OpenCurrentMenu, GetState());
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  void RPC_OpenCurrentMenu(SCR_EGameModeState state)
  {
    PS_DebugLogger.LogImportant("RPC_OpenCurrentMenu state=" + typename.EnumToString(SCR_EGameModeState, state));
    PlayerController playerController = GetGame().GetPlayerController();
    if (!playerController) return;
    if (playerController.GetPlayerId() == 0)
    {
      PS_DebugLogger.LogImportant("RPC_OpenCurrentMenu playerId=0, retrying");
      return;
    }
    PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
    if (!playableController)
    {
      PS_DebugLogger.LogImportant("RPC_OpenCurrentMenu playableController NULL, retrying");
      GetGame().GetCallqueue().CallLater(RPC_OpenCurrentMenu, 100, false, state);
      return;
    }
    playableController.SwitchToMenu(state);
  }

  void SpawnInitialEntity(int playerId)
  {
    PS_PlayableManager pm = PS_PlayableManager.GetInstance();
    RplId currentSlot = pm.GetPlayableByPlayer(playerId);
    PS_EPlayableControllerState pState = pm.GetPlayerState(playerId);
    PS_DebugLogger.LogImportant("SpawnInitialEntity START playerId=" + playerId.ToString() + " slot=" + currentSlot.ToString() + " state=" + typename.EnumToString(PS_EPlayableControllerState, pState), playerId);
		#ifdef WORKBENCH
		IEntity WBCharacter = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (WBCharacter)
			return;
		#endif

			PS_VoNChannelsManager VoNChannelsManager = PS_VoNChannelsManager.GetInstance();
		Resource resource = Resource.Load("{ADDE38E4119816AB}Prefabs/InitialPlayer_Version2.et");
		if (!resource.IsValid())
			return;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		SCR_PlayerController playerController = SCR_PlayerController.Cast(playerManager.GetPlayerController(playerId));
		if (!playerController)
		{
			if (!playerManager.IsPlayerConnected(playerId))
				return;
			GetGame().GetCallqueue().CallLater(SpawnInitialEntity, 200, false, playerId);
			return;
		}
		PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
		if (!playableController)
		{
			GetGame().GetCallqueue().CallLater(SpawnInitialEntity, 200, false, playerId);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		GetTransform(params.Transform);
		vector position = Vector(0, 100000, 0) + Vector(1000 * Math.Mod(playerId, 10), 5000 * Math.Floor(Math.Mod(playerId, 100) / 10), 5000 * Math.Floor(playerId / 100));
		params.Transform[3] = position;
		IEntity initialEntity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
    playableController.SetInitialEntity(initialEntity);
    playerController.SetInitialMainEntity(initialEntity);
    if (VoNChannelsManager)
      VoNChannelsManager.RestoreRoom(playerId);
  }

  void SwitchToInitialEntity(int playerId)
  {
    if (playerId <= 0)
      return;
    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
    RplId currentSlot = playableManager.GetPlayableByPlayer(playerId);
    PS_EPlayableControllerState pState = playableManager.GetPlayerState(playerId);
    PS_DebugLogger.LogImportant("SwitchToInitialEntity playerId=" + playerId.ToString() + " slot=" + currentSlot.ToString() + " state=" + typename.EnumToString(PS_EPlayableControllerState, pState), playerId);
    if (currentSlot != RplId.Invalid() && !playableManager.IsSlotCharacterDestroyed(currentSlot))
    {
      PS_DebugLogger.LogImportant("SwitchToInitialEntity SKIP: player already has valid slot=" + currentSlot.ToString(), playerId);
      playableManager.ApplyPlayable(playerId);
      playableManager.ForceSwitch(playerId);
      return;
    }
    PS_DebugLogger.LogImportant("SwitchToInitialEntity INVALIDATING slot and applying spectator", playerId);
    playableManager.SetPlayerToSlot(RplId.Invalid(), playerId);
    playableManager.ApplyPlayable(playerId);
  }

	// If after m_iReconnectTime player still disconnected release playable
	void RemoveDisconnectedPlayer(int playerId)
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		string guid = playableManager.GetPlayerGUIDById(playerId);
		playableManager.RemovePlayer(playerId, guid, true);
	}

  override void OnGameStateChanged()
  {
    super.OnGameStateChanged();

    SCR_EGameModeState state = GetState();
    PS_DebugLogger.LogImportant("OnGameStateChanged state=" + typename.EnumToString(SCR_EGameModeState, state) + " isServer=" + Replication.IsServer().ToString() + " freezeTimeLeft=" + m_fCurrentFreezeTime.ToString());

    // Clear death-screen tracking when game state changes (new round) so players
    // who died in the previous round can receive the death screen again.
    if (Replication.IsServer())
      m_DeathScreenSent.Clear();

		PS_VoNChannelsManager VoNChannelsManager = PS_VoNChannelsManager.GetInstance();
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		array<int> playerIds = new array<int>();
		PlayerManager playerManagerForState = GetGame().GetPlayerManager();
		if (playerManagerForState)
			playerManagerForState.GetPlayers(playerIds);

    // Log every player's state at game state transition (both server and client)
    foreach (int pid : playerIds)
    {
      RplId slotId = playableManager.GetPlayableByPlayer(pid);
      PS_EPlayableControllerState pState = playableManager.GetPlayerState(pid);
      FactionKey fKey = playableManager.GetPlayerFactionKey(pid);
      PS_DebugLogger.LogImportant("OnGameStateChanged player=" + pid.ToString() + " slot=" + slotId.ToString() + " state=" + typename.EnumToString(PS_EPlayableControllerState, pState) + " faction=" + fKey, pid);
    }
    m_OnGameStateChange.Invoke(state);
		switch (state)
		{
	case SCR_EGameModeState.PREGAME:
		// BUGFIX: pre-create all faction and group VoN rooms in PREGAME so that
		// players in the initial lobby phase (before SLOTSELECTION) can see every
		// squad channel immediately. Without this, GetAllGroupRooms returns empty
		// in PREGAME because EnsureAllFactionAndGroupRoomsExist was only called
		// for SLOTSELECTION and BRIEFING transitions.
		EnsureAllFactionAndGroupRoomsExist();
		break;
	case SCR_EGameModeState.SLOTSELECTION:
		// BUGFIX: pre-create all faction and group VoN rooms on transition to
		// SLOTSELECTION (lobby) so clients can see every group channel in the
		// lobby UI even before any player is assigned to a slot. Previously the
		// group rooms were only created when a player was moved to them via
		// MoveToRoom, so the client got roomId=-1 placeholders that CreateRoom
		// filtered out — the "i don't see other group channels in lobby" bug.
		EnsureAllFactionAndGroupRoomsExist();
		if (VoNChannelsManager)
		{
			foreach (int playerId : playerIds)
			{
				int roomId = VoNChannelsManager.GetPlayerRoom(playerId);
				string roomName = VoNChannelsManager.GetRoomName(roomId);
				if (roomName.Contains("#PS-VoNRoom_Public"))
					VoNChannelsManager.MoveToRoom(playerId, "", "#PS-VoNRoom_Global");
			}
		}
		break;
	case SCR_EGameModeState.BRIEFING:
		// BUGFIX: same as SLOTSELECTION — pre-create ALL group rooms (not just
		// the ones with players) so the briefing UI shows every same-faction
		// group channel, even empty ones. Fixes the "i don't see faction group
		// channels in briefing" bug.
		EnsureAllFactionAndGroupRoomsExist();
		if (VoNChannelsManager)
			VoNChannelsManager.DumpVoNState("BRIEFING_transition");
		foreach (int playerId : playerIds)
		{
			RplId slotId;
			if (!playableManager.FindPlayerSlotById(playerId, slotId) || slotId == RplId.Invalid())
			{
				if (VoNChannelsManager)
					VoNChannelsManager.MoveToRoom(playerId, "", "#PS-VoNRoom_Global");
			}
			else
			{
				if (playableManager.IsPlayerTopSlotInGroup(playerId))
				{
					if (VoNChannelsManager)
						VoNChannelsManager.MoveToRoom(playerId, playableManager.GetPlayerFactionKey(playerId), "#PS-VoNRoom_Command");
				}
				else if (m_bPublicCommandBriefing)
				{
					if (VoNChannelsManager)
						VoNChannelsManager.MoveToRoom(playerId, playableManager.GetPlayerFactionKey(playerId), "#PS-VoNRoom_Command");
				}
				else
				{
					int groupCallsign = playableManager.GetGroupCallsignByPlayable(slotId);
					if (VoNChannelsManager)
						VoNChannelsManager.MoveToRoom(playerId, playableManager.GetPlayerFactionKey(playerId), groupCallsign.ToString());
				}
			}
		}
		if (m_bHolsterWeapon)
			playableManager.HolsterWeapons();
		if (VoNChannelsManager)
			VoNChannelsManager.DumpVoNState("BRIEFING_post_assignment");
		break;
			case SCR_EGameModeState.GAME:
				// VoN rooms are already set up from BRIEFING — players stay in their squad/command channels
				// No cleanup needed here (matching ReforgerLobby17 approach)
				if (Replication.IsServer())
					OpenCurrentMenuOnClients();
				GetGame().GetCallqueue().Remove(DumpPlayableEntityState);
				GetGame().GetCallqueue().CallLater(DumpPlayableEntityState, 10000, false);
				break;
			case SCR_EGameModeState.DEBRIEFING:
			case SCR_EGameModeState.POSTGAME:
				GetGame().GetCallqueue().Remove(DumpPlayableEntityState);
				break;
		}
	}

	// BUGFIX: Pre-create every faction and group VoN room on the server so
	// clients can see the full group structure in lobby and briefing UIs even
	// before any player is assigned to a slot. Without this, the client calls
	// GetOrCreateRoomWithFaction and gets a roomId=-1 placeholder (since the
	// room doesn't exist server-side yet), and CreateRoom filters out the
	// placeholder — resulting in the "group channels not showing" bug.
	// Iterates ALL playables (not just ones with players) so empty groups are
	// also visible. Idempotent: GetOrCreateRoomWithFaction returns the existing
	// roomId if the room already exists, so calling this on every state
	// transition is safe.
	void EnsureAllFactionAndGroupRoomsExist()
	{
		if (!Replication.IsServer())
			return;

		PS_VoNChannelsManager VoNChannelsManager = PS_VoNChannelsManager.GetInstance();
		if (!VoNChannelsManager)
			return;

		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		if (!playableManager)
			return;

		// Two passes:
		//   Pass 1: collect every unique factionKey and create its Faction/Command
		//   rooms. This is NOT gated on groupCallSign validity — a faction with
		//   only unassigned playables (groupCallSign < 0) still needs its
		//   Faction/Command rooms created so the BRIEFING UI can show them to
		//   players in that faction.
		//   Pass 2: for playables with a valid groupCallSign, create the per-group
		//   room. Deduped by (factionKey, groupCallSign) so playables in the same
		//   group don't trigger redundant room-creation calls.
		// NOTE: Using `map<string, bool>` instead of `set<string>` because
		// Enfusion Script's `set<T>` template appears to be limited to primitive
		// int keys (no `set<string>` examples exist in this file or its
		// dependencies). `map<string, bool>` is the idiomatic dedup pattern used
		// elsewhere in this file (see `CanJoinFaction` at line 850).
		map<string, bool> seenFactions = new map<string, bool>();
		map<string, bool> seenGroups = new map<string, bool>();
		array<PS_PlayableContainer> playables = playableManager.GetPlayablesSorted();
		// BUGFIX: use `for` for C-style indexed loops. Enfusion Script's `foreach`
		// is range-based only (requires `foreach (var : collection)` syntax with
		// a `:`). Using `foreach` with `(int i = 0; ...)` produced the parser
		// error "Expected ':', not a '='" because the parser was looking for the
		// range-based foreach delimiter.
		for (int i = 0; i < playables.Count(); i++)
		{
			PS_PlayableContainer playable = playables[i];
			if (!playable)
				continue;
			// RplId may be Invalid during teardown/replication handoff — skip
			// those to avoid NRE in GetGroupCallsignByPlayable.
			if (playable.GetRplId() == RplId.Invalid())
				continue;
			FactionKey fk = playable.GetFactionKey();
			if (fk == "")
				continue;

			// Pass 1: Faction/Command rooms for every unique faction
			if (!seenFactions.Contains(fk))
			{
				seenFactions[fk] = true;
				VoNChannelsManager.GetOrCreateRoomWithFaction(fk, "#PS-VoNRoom_Faction");
				VoNChannelsManager.GetOrCreateRoomWithFaction(fk, "#PS-VoNRoom_Command");
			}

			// Pass 2: per-group room (only for playables with a valid group)
			int groupCallSign = playableManager.GetGroupCallsignByPlayable(playable.GetRplId());
			if (groupCallSign <= 0)
				continue;
			string dedupKey = fk + "|" + groupCallSign.ToString();
			if (seenGroups.Contains(dedupKey))
				continue;
			seenGroups[dedupKey] = true;
			VoNChannelsManager.GetOrCreateRoomWithFaction(fk, groupCallSign.ToString());
		}
		PS_DebugLogger.LogImportant("EnsureAllFactionAndGroupRoomsExist created/verified " + seenFactions.Count().ToString() + " faction rooms + " + seenGroups.Count().ToString() + " group rooms");
	}

	void DumpPlayableEntityState()
	{
		if (!Replication.IsServer())
			return;
		// Guard against firing in wrong state (e.g. after premature state change)
		if (GetState() != SCR_EGameModeState.GAME)
		{
			PS_DebugLogger.LogImportant("DumpPlayableEntityState aborted: state != GAME");
			return;
		}
		PS_PlayableManager pm = PS_PlayableManager.GetInstance();
		if (!pm)
			return;
		Print("=== PLAYABLE ENTITY STATE DUMP (10s after GAME) ===", LogLevel.NORMAL);
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int pid : playerIds)
		{
			RplId slotId = pm.GetPlayableByPlayer(pid);
			if (slotId == RplId.Invalid())
				continue;
			IEntity entity = IEntity.Cast(Replication.FindItem(slotId));
			string entityState = "NULL";
			if (entity) entityState = "ALIVE";
			PS_SlotCharacterData slotData;
			bool inMap = pm.FindSlotData(slotId, slotData);
			Print(string.Format("Player=%1 Slot=%2 Entity=%3 InMap=%4 Name=%5", pid, slotId, entityState, inMap, pm.GetSlotName(slotId)), LogLevel.NORMAL);
		}
		Print("=== END DUMP ===", LogLevel.NORMAL);
	}

	// Switch to next game state
	void AdvanceGameState(SCR_EGameModeState oldState)
	{
		if (!Replication.IsServer())
			return;

		SCR_EGameModeState state = GetState();
		if (oldState != SCR_EGameModeState.NULL && oldState != state) return;
		switch (state)
		{
			case SCR_EGameModeState.PREGAME:
				SetGameModeState(SCR_EGameModeState.SLOTSELECTION);
				break;
			case SCR_EGameModeState.SLOTSELECTION:
				if (m_bShowCutscene)
				{
					SetGameModeState(SCR_EGameModeState.CUTSCENE);
					GetGame().GetCallqueue().CallLater(AdvanceGameState, m_CutsceneManager.GetCutsceneTime() + 400, false, SCR_EGameModeState.CUTSCENE);
					if (RplSession.Mode() == RplMode.Dedicated)
						PS_CutsceneManager.GetInstance().RunCutscene(0);
				}
				else
					SetGameModeState(SCR_EGameModeState.BRIEFING);
				break;
			case SCR_EGameModeState.CUTSCENE:
				SetGameModeState(SCR_EGameModeState.BRIEFING);
				break;
			case SCR_EGameModeState.BRIEFING:
				StartGame();
				break;
			case SCR_EGameModeState.GAME:
				SetGameModeState(SCR_EGameModeState.DEBRIEFING);
				break;
			case SCR_EGameModeState.DEBRIEFING:
				SetGameModeState(SCR_EGameModeState.POSTGAME);
				break;
			case SCR_EGameModeState.POSTGAME:
				break;
		}
		OpenCurrentMenuOnClients();
	}

  protected ref array<int> m_StartGamePendingPlayers = new array<int>();
  protected int m_StartGamePhase = 0;

  void StartGame()
  {
    PS_DebugLogger.LogImportant("StartGame BEGIN isServer=" + Replication.IsServer().ToString() + " reconnectTimeAfterBriefing=" + m_iReconnectTimeAfterBriefing.ToString());
    m_iReconnectTime = m_iReconnectTimeAfterBriefing;
    if (m_bReserveSlots)
      ReserveSlots();

    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();

    // Phase 0: collect all players and stagger ApplyPlayable
    array<int> allPlayers = {};
    GetGame().GetPlayerManager().GetPlayers(allPlayers);
    m_StartGamePendingPlayers.Clear();
    foreach (int pid : allPlayers)
    {
      RplId slotId = playableManager.GetPlayableByPlayer(pid);
      PS_EPlayableControllerState pState = playableManager.GetPlayerState(pid);
      PS_DebugLogger.LogImportant("StartGame queueing player=" + pid.ToString() + " slot=" + slotId.ToString() + " state=" + typename.EnumToString(PS_EPlayableControllerState, pState), pid);
      m_StartGamePendingPlayers.Insert(pid);
    }

    m_StartGamePhase = 0;
    GetGame().GetCallqueue().CallLater(StartGamePhase_ApplyPlayables, 0, false);
  }

  protected void StartGamePhase_ApplyPlayables()
  {
    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
    // Apply up to 4 players per frame to avoid replication burst
    int batchSize = 4;
    for (int i = 0; i < batchSize && m_StartGamePendingPlayers.Count() > 0; i++)
    {
      int pid = m_StartGamePendingPlayers[0];
      m_StartGamePendingPlayers.Remove(0);
      playableManager.ApplyPlayable(pid);
    }

    if (m_StartGamePendingPlayers.Count() > 0)
    {
      GetGame().GetCallqueue().CallLater(StartGamePhase_ApplyPlayables, 50, false);
      return;
    }

    // Move to next phase with a small delay to let replication catch up
    GetGame().GetCallqueue().CallLater(StartGamePhase_Cleanup, 200, false);
  }

  protected void StartGamePhase_Cleanup()
  {
    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
    playableManager.RemoveRedundantUnits();
    // Wait for bulk deletion to settle before starting game mode
    GetGame().GetCallqueue().CallLater(StartGamePhase_StartMode, 300, false);
  }

  protected void StartGamePhase_StartMode()
  {
    PS_DebugLogger.LogImportant("StartGame freezeTime=" + m_iFreezeTime.ToString() + " starting restrictedZonesTimer");
    restrictedZonesTimer(m_iFreezeTime);
    StartGameMode();
    PS_DebugLogger.LogImportant("StartGame AFTER StartGameMode, scheduling deploy RPC");
    // Small delay before sending the deploy broadcast so clients can process state changes
    GetGame().GetCallqueue().CallLater(StartGamePhase_Deploy, 200, false);
  }

  protected void StartGamePhase_Deploy()
  {
    Rpc(RPC_RequestDeployForAllPlayers);
    PS_DebugLogger.LogImportant("StartGame END");
  }
	
  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  void RPC_RequestDeployForAllPlayers()
  {
    PlayerController pc = GetGame().GetPlayerController();
    if (!pc) return;
    int playerId = pc.GetPlayerId();
    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
    RplId slotId = playableManager.GetPlayableByPlayer(playerId);
    PS_EPlayableControllerState myState = playableManager.GetPlayerState(playerId);
    PS_DebugLogger.LogImportant("RPC_RequestDeploy playerId=" + playerId.ToString() + " slotId=" + slotId.ToString() + " myState=" + typename.EnumToString(PS_EPlayableControllerState, myState), playerId);
    if (slotId == RplId.Invalid())
    {
      PS_DebugLogger.LogImportant("RPC_RequestDeploy SKIP: no slot, switching to observer", playerId);
      PS_PlayableControllerComponent pccNoSlot = PS_PlayableControllerComponent.Cast(pc.FindComponent(PS_PlayableControllerComponent));
      if (pccNoSlot)
        pccNoSlot.SwitchToObserver(null);
      return;
    }
    PS_PlayableControllerComponent pcc = PS_PlayableControllerComponent.Cast(pc.FindComponent(PS_PlayableControllerComponent));
    if (!pcc)
      return;
    pcc.RequestDeployFromClient();
  }

	void ReserveSlots()
	{
		if (!Replication.IsServer())
			return;

		PS_SlotsReserver slotsReserver = PS_SlotsReserver.Cast(FindComponent(PS_SlotsReserver));
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();

		array<string> GUIDs = {};
		foreach (RplId slotId, PS_SlotCharacterData slot : playableManager.GetSlots().GetRawMap())
		{
			int playerId = slot.m_PlayerId;
			if (playerId <= 0)
				continue;

			string GUID = playableManager.GetPlayerGUIDById(playerId);
			GUIDs.Insert(GUID);
		}

		slotsReserver.AddGUIDs(GUIDs);
		slotsReserver.SetEnabled(true);
	}

	// TODO: move it to component
  void restrictedZonesTimer(int freezeTime)
  {
    int time = 1000;
    if (freezeTime < time) time = freezeTime;
    freezeTime -= time;

    m_fCurrentFreezeTime = freezeTime;

    if (RplSession.Mode() != RplMode.Dedicated) RPC_restrictedZonesTimer(freezeTime);
    Rpc(RPC_restrictedZonesTimer, freezeTime);

    if (freezeTime <= 0)
    {
      PS_DebugLogger.LogImportant("restrictedZonesTimer FREEZE_TIME_ENDED m_fGameStartTime=" + GetGame().GetWorld().GetWorldTime().ToString());
      m_fGameStartTime = GetGame().GetWorld().GetWorldTime();
      m_fGameStartElapsedTime = GetElapsedTime();
      removeRestrictedZones();
      if (m_bDisableBuildingModeAfterFreezeTime)
        DisableBuildingMode();
    }
    else
      GetGame().GetCallqueue().CallLater(restrictedZonesTimer, time, false, freezeTime);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  void RPC_restrictedZonesTimer(int freezeTime)
  {
    PS_DebugLogger.Log("RPC_restrictedZonesTimer freezeTime=" + freezeTime.ToString());
    if (freezeTime <= 0)
		{
			if (m_hFreezeTimeCounter)
			{
				m_hFreezeTimeCounter.GetRootWidget().RemoveFromHierarchy();
			}
			return;
		}

		if (m_hFreezeTimeCounter == null)
		{
			Widget widget = GetGame().GetWorkspace().CreateWidgets("{EC8A548C3F53BE4F}UI/layouts/FreezeTime/FreezeTimeCounter.layout");
			m_hFreezeTimeCounter = PS_FreezeTimeCounter.Cast(widget.FindHandler(PS_FreezeTimeCounter));
		}

		m_hFreezeTimeCounter.SetTime(freezeTime);
	}
	void DisableBuildingMode()
	{
		SCR_EditorManagerCore editorManagerCore = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
		if (!editorManagerCore)
			return;
		array<int> outPlayers = {};
		GetGame().GetPlayerManager().GetAllPlayers(outPlayers);
		foreach (int player : outPlayers)
		{
			SCR_EditorManagerEntity editorManager = editorManagerCore.GetEditorManager(player);
			if (editorManager)
				editorManager.SetCanOpen(false, EEditorCanOpen.ALIVE);
		}
	}
	PS_FreezeTimeCounter m_hFreezeTimeCounter;

	// ------------------------------------------ Global flags ------------------------------------------
	float GetCurrentFreezeTime()
	{
		return m_fCurrentFreezeTime;
	}

	bool IsFreezeTimeEnd()
	{
		return m_fCurrentFreezeTime <= 0;
	}
	
	bool IsDisableTimeEnd()
	{
		return m_fCurrentFreezeTime <= (m_iFreezeTime - m_iDisableTime) && m_fCurrentFreezeTime != 1;
	}
	
	bool IsFreezeTimeShootingForbiden()
	{
		return m_bFreezeTimeShootingForbiden;
	}

	bool IsAdminMode()
	{
		return m_bAdminMode;
	}

	bool GetFriendliesSpectatorOnly()
	{
		if (!GetGame().GetPlayerController()) return true;
		if (SCR_Global.IsAdmin(GetGame().GetPlayerController().GetPlayerId())) return false;
		return m_bFriendliesSpectatorOnly;
	}

	bool GetDisablePlayablesStreaming()
	{
		return m_bDisablePlayablesStreaming;
	}

	bool IsChatDisabled()
	{
		return m_bDisableChat;
	}

	bool IsFactionLockMode()
	{
		return m_bFactionLock;
	}
	
	bool IsArmaVisionDisabled()
	{
		return m_bDisableArmaVision;
	}
	
	bool GetDisableBuildingModeAfterFreezeTime()
	{
		return m_bDisableBuildingModeAfterFreezeTime;
	}

	bool GetMarkersOnlyOnBriefing()
	{
		return m_bMarkersOnlyOnBriefing;
	}
	void SetMarkersOnlyOnBriefing(bool markersOnlyOnBriefing)
	{
		m_bMarkersOnlyOnBriefing = markersOnlyOnBriefing;
		Rpc(RPC_SetMarkersOnlyOnBriefing, markersOnlyOnBriefing);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetMarkersOnlyOnBriefing(bool markersOnlyOnBriefing)
	{
		m_bMarkersOnlyOnBriefing = markersOnlyOnBriefing;
	}

	bool GetDisableLeaderSquadMarkers()
	{
		return m_bRemoveSquadMarkers;
	}
	void SetDisableLeaderSquadMarkers(bool disableLeaderSquadMarkers)
	{
		RPC_SetDisableLeaderSquadMarkers(disableLeaderSquadMarkers);
		Rpc(RPC_SetDisableLeaderSquadMarkers, disableLeaderSquadMarkers);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetDisableLeaderSquadMarkers(bool disableLeaderSquadMarkers)
	{
		m_bRemoveSquadMarkers = disableLeaderSquadMarkers;
	}

	// Global flags set
	void FactionLockSwitch()
	{
		m_bFactionLock = !m_bFactionLock;
		Rpc(RPC_SetFactionLock, m_bFactionLock);
	}
  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  void RPC_SetFactionLock(bool factionLock)
  {
    PS_DebugLogger.LogImportant("RPC_SetFactionLock factionLock=" + factionLock.ToString());
    m_bFactionLock = factionLock;
  }

	// ------------------------------------------ Global variables ------------------------------------------
	int GetFreezeTime()
	{
		return m_iFreezeTime;
	}
	void SetFreezeTime(int freezeTime)
	{
		RPC_SetFreezeTime(freezeTime);
		Rpc(RPC_SetFreezeTime, freezeTime);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetFreezeTime(int freezeTime)
	{
		m_iFreezeTime = freezeTime;
	}
	
	int GetDisableTime()
	{
		return m_iDisableTime;
	}
	
	float GetGameStartTime()
	{
		return m_fGameStartTime;
	}

	float GetGameStartElapsedTime()
	{
		return m_fGameStartElapsedTime;
	}

	int GetReconnectTime()
	{
		return m_iReconnectTime;
	}
	void SetReconnectTime(int availableReconnectTime)
	{
		RPC_SetReconnectTime(availableReconnectTime);
		Rpc(RPC_SetReconnectTime, availableReconnectTime);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetReconnectTime(int availableReconnectTime)
	{
		m_iReconnectTime = availableReconnectTime;
	}

	bool GetRemoveRedundantUnits()
	{
		return m_bRemoveRedundantUnits;
	}
	int GetReadyCountdown()
	{
		return m_iReadyCountdown;
	}
	void SetRemoveRedundantUnits(bool killRedundantUnits)
	{
		RPC_SetRemoveRedundantUnits(killRedundantUnits);
		Rpc(RPC_SetRemoveRedundantUnits, killRedundantUnits);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetRemoveRedundantUnits(bool killRedundantUnits)
	{
		m_bRemoveRedundantUnits = killRedundantUnits;
	}

	bool GetCanOpenLobbyInGame()
	{
		return m_bTeamSwitch;
	}
	void SetCanOpenLobbyInGame(bool canOpenLobbyInGame)
	{
		RPC_SetCanOpenLobbyInGame(canOpenLobbyInGame);
		Rpc(RPC_SetCanOpenLobbyInGame, canOpenLobbyInGame);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetCanOpenLobbyInGame(bool canOpenLobbyInGame)
	{
		m_bTeamSwitch = canOpenLobbyInGame;
	}
}
