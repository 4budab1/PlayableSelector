[ComponentEditorProps(category: "GameScripted/Character", description: "Set character playable", color: "0 0 255 255", icon: HYBRID_COMPONENT_ICON)]
class PS_PlayableControllerComponentClass : ScriptComponentClass
{
}

// We can send rpc only from authority
// And here we are, modifying player controller since it's only what we have on client.
class PS_PlayableControllerComponent : ScriptComponent
{
	protected IEntity m_Camera;
	protected IEntity m_InitialEntity;
	// Tracks whether physics.SetActive(INACTIVE) has already been applied to m_InitialEntity,
	// so we can skip the redundant per-frame call (which contributes to replication traffic
	// and cumulative connection flooding — see TodayFixes.MD REPLICATION_FLOODED investigation).
	protected bool m_bPhysicsInactive = false;
	protected vector m_vVoNPosition = PS_VoNChannelsManager.roomInitialPosition;
	protected SCR_EGameModeState m_eMenuState = SCR_EGameModeState.PREGAME;
	protected bool m_bAfterInitialSwitch = false;
	protected vector m_vObserverPosition = "0 0 0";
	protected vector lastCameraTransform[4];

	// Rate limiting for client-triggered RPCs
	protected ref map<int, float> m_mLastRpcTime = new map<int, float>();
	static const float RPC_COOLDOWN_MS = 250;

	protected bool IsRpcOnCooldown(int playerId, float cooldownMs = RPC_COOLDOWN_MS)
	{
		float now = GetGame().GetWorld().GetWorldTime() * 1000;
		float lastTime;
		if (m_mLastRpcTime.Find(playerId, lastTime) && (now - lastTime) < cooldownMs)
			return true;
		m_mLastRpcTime[playerId] = now;
		return false;
	}

	void SetVoNPosition(vector VoNPosition)
	{
		m_vVoNPosition = VoNPosition;
	}
	
	vector GetObserverPosition()
	{
		return m_vObserverPosition;
	}
	
	[RplProp()]
	bool m_bOutFreezeTime;
	
  void SetOutFreezeTime(bool outFreezeTime)
  {
    if (m_bOutFreezeTime == outFreezeTime) return;
    PS_DebugLogger.Log("SetOutFreezeTime outFreezeTime=" + outFreezeTime.ToString());
    Rpc(RPC_SetOutFreezeTime, outFreezeTime);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
  void RPC_SetOutFreezeTime(bool outFreezeTime)
  {
    if (m_bOutFreezeTime == outFreezeTime) return;
    PS_DebugLogger.LogImportant("RPC_SetOutFreezeTime outFreezeTime=" + outFreezeTime.ToString());
    m_bOutFreezeTime = outFreezeTime;
  }
	
	// Event
	protected ref ScriptInvokerBase<SCR_BaseGameMode_OnPlayerRoleChanged> m_eOnPlayerRoleChange = new ScriptInvokerBase<SCR_BaseGameMode_OnPlayerRoleChanged>();
	ScriptInvokerBase<SCR_BaseGameMode_OnPlayerRoleChanged> GetOnPlayerRoleChange()
	{
		return m_eOnPlayerRoleChange;
	}

	// ------ FactionReady ------
  void SetFactionReady(FactionKey factionKey, int readyValue)
  {
    PS_DebugLogger.Log("SetFactionReady faction=" + factionKey + " readyValue=" + readyValue.ToString());
    Rpc(RPC_SetFactionReady, factionKey, readyValue);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_SetFactionReady(FactionKey factionKey, int readyValue)
  {
    PS_DebugLogger.LogImportant("RPC_SetFactionReady SERVER faction=" + factionKey + " readyValue=" + readyValue.ToString());
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    int playerId = thisPlayerController.GetPlayerId();
    if (IsRpcOnCooldown(playerId)) { PS_DebugLogger.Log("RPC_SetFactionReady REJECTED cooldown player=" + playerId.ToString(), playerId); return; }
    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();

    FactionKey commanderFactionKey;
    bool isCommander = playableManager.IsPlayerFactionCommander(playerId, commanderFactionKey);
    if (!SCR_Global.IsAdmin(playerId) && (!isCommander || commanderFactionKey != factionKey))
    {
      PS_DebugLogger.Log("RPC_SetFactionReady REJECTED player=" + playerId.ToString() + " not admin/commander for faction", playerId);
      return;
    }

    playableManager.SetFactionReady(factionKey, readyValue);
  }

	// ------ MenuState ------
	void SetMenuState(SCR_EGameModeState state)
	{
		m_eMenuState = state;
	}

	SCR_EGameModeState GetMenuState()
	{
		return m_eMenuState;
	}

  void SwitchToMenuServer(SCR_EGameModeState state)
  {
    PS_DebugLogger.LogImportant("SwitchToMenuServer state=" + typename.EnumToString(SCR_EGameModeState, state));
    Rpc(RPC_SwitchToMenuServer, state);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
  void RPC_SwitchToMenuServer(SCR_EGameModeState state)
  {
    PS_DebugLogger.LogImportant("RPC_SwitchToMenuServer OWNER state=" + typename.EnumToString(SCR_EGameModeState, state));
    SwitchToMenu(state);
  }

  void SwitchToMenu(SCR_EGameModeState state)
  {
    PS_DebugLogger.LogImportant("SwitchToMenu state=" + typename.EnumToString(SCR_EGameModeState, state));
    SetMenuState(state);
		MenuBase topMenu = GetGame().GetMenuManager().GetTopMenu();
		if (topMenu)
			topMenu.Close();
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.PreviewMapMenu);
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.CoopLobby);
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.CutsceneMenu);
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.BriefingMapMenu);
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.FadeToGame);
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.DebriefingMenu);
		switch (state)
		{
			case SCR_EGameModeState.PREGAME:
				GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.PreviewMapMenu);
				break;
			case SCR_EGameModeState.SLOTSELECTION:
				GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.CoopLobby);
				break;
			case SCR_EGameModeState.CUTSCENE:
				GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.CutsceneMenu);
				break;
			case SCR_EGameModeState.BRIEFING:
				GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.BriefingMapMenu);
				break;
			case SCR_EGameModeState.GAME:
				GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.FadeToGame);
				PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
				PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
				int playerId = thisPlayerController.GetPlayerId();
				RplId playerSlot = playableManager.GetPlayableByPlayer(playerId);
				if (playerSlot == RplId.Invalid())
					GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.SpectatorMenu);
				break;
			case SCR_EGameModeState.DEBRIEFING:
				GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.DebriefingMenu);
				break;
			case SCR_EGameModeState.POSTGAME:
				GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.DebriefingMenu);
				break;
		}
	}

  void AdvanceGameState(SCR_EGameModeState state)
  {
    PS_DebugLogger.LogImportant("AdvanceGameState state=" + typename.EnumToString(SCR_EGameModeState, state));
    Rpc(RPC_AdvanceGameState, state);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_AdvanceGameState(SCR_EGameModeState state)
  {
    PS_DebugLogger.LogImportant("RPC_AdvanceGameState SERVER state=" + typename.EnumToString(SCR_EGameModeState, state));
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    int thisPlayerId = thisPlayerController.GetPlayerId();
    if (!SCR_Global.IsAdmin(thisPlayerId))
    {
      PS_DebugLogger.Log("RPC_AdvanceGameState REJECTED not admin");
      return;
    }
    if (IsRpcOnCooldown(thisPlayerId)) { PS_DebugLogger.Log("RPC_AdvanceGameState REJECTED cooldown player=" + thisPlayerId.ToString(), thisPlayerId); return; }
    PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
    if (!gameMode)
    {
      PS_DebugLogger.LogError("RPC_AdvanceGameState: gameMode NULL");
      return;
    }
    gameMode.AdvanceGameState(state);
  }

  void LoadMission(string missionName)
  {
    PS_DebugLogger.LogImportant("LoadMission CLIENT mission=" + missionName);
    Rpc(RPC_LoadMission, missionName);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_LoadMission(string missionName)
  {
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
    {
      PS_DebugLogger.Log("RPC_LoadMission REJECTED not admin");
      return;
    }
    PS_DebugLogger.LogImportant("RPC_LoadMission SERVER mission=" + missionName + " (disabled)");
  }

	// ------ FactionLock ------
  void FactionLockSwitch()
  {
    PS_DebugLogger.LogImportant("FactionLockSwitch");
    Rpc(RPC_FactionLockSwitch);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_FactionLockSwitch()
  {
    PS_DebugLogger.LogImportant("RPC_FactionLockSwitch SERVER");
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
    {
      PS_DebugLogger.Log("RPC_FactionLockSwitch REJECTED not admin");
      return;
    }

    PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
    if (!gameMode)
    {
      PS_DebugLogger.LogError("RPC_FactionLockSwitch: gameMode NULL");
      return;
    }
    gameMode.FactionLockSwitch();
  }

	// ------ FreezeTimer ------
  void FreezeTimerAdvance(int time)
  {
    PS_DebugLogger.LogImportant("FreezeTimerAdvance time=" + time.ToString());
    Rpc(RPC_FreezeTimerAdvance, time);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_FreezeTimerAdvance(int time)
  {
    PS_DebugLogger.LogImportant("RPC_FreezeTimerAdvance SERVER time=" + time.ToString());
    PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
    if (gameMode)
      gameMode.FreezeTimerAdvance(time);
  }
  void FreezeTimerEnd()
  {
    PS_DebugLogger.LogImportant("FreezeTimerEnd");
    Rpc(RPC_FreezeTimerEnd);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_FreezeTimerEnd()
  {
    PS_DebugLogger.LogImportant("RPC_FreezeTimerEnd SERVER");
    PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
    if (gameMode)
      gameMode.FreezeTimerEnd();
  }
	
	// ------ SpawnPrefab ------
  void SpawnPrefab(string GUID, vector position)
  {
    PS_DebugLogger.LogImportant("SpawnPrefab CLIENT guid=" + GUID);
    IEntity camera = GetGame().GetCameraManager().CurrentCamera();
    if (position == "0 0 0")
      position = camera.GetOrigin();
    Rpc(RPC_SpawnPrefab, position, GUID);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_SpawnPrefab(vector position, string GUID)
  {
    PS_DebugLogger.LogImportant("RPC_SpawnPrefab SERVER guid=" + GUID);
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
    {
      PS_DebugLogger.Log("RPC_SpawnPrefab REJECTED not admin");
      return;
    }

		Resource resource = Resource.Load(GUID);
		EntitySpawnParams entitySpawnParams = new EntitySpawnParams();
		Math3D.MatrixIdentity4(entitySpawnParams.Transform);
		entitySpawnParams.Transform[3] = position;

		IEntity entity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), entitySpawnParams);
		if (!entity)
			return;
		Physics physics = entity.GetPhysics();
		if (physics)
			physics.SetActive(ActiveState.ACTIVE);
	}
	
	// ------ SpawnAdministrator ------
  void SpawnAdministrator(vector position)
  {
    PS_DebugLogger.LogImportant("SpawnAdministrator CLIENT position=" + position.ToString());
    Rpc(RPC_SpawnAdministrator, position);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_SpawnAdministrator(vector position)
  {
    PS_DebugLogger.LogImportant("RPC_SpawnAdministrator SERVER position=" + position.ToString());
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
    {
      PS_DebugLogger.Log("RPC_SpawnAdministrator REJECTED not admin");
      return;
    }

		Resource resource = Resource.Load("{3C87CA398115BBD4}Prefabs/Characters/Core/Character_Administrator.et");
		EntitySpawnParams entitySpawnParams = new EntitySpawnParams();
		Math3D.MatrixIdentity4(entitySpawnParams.Transform);
		entitySpawnParams.Transform[3] = position;

		IEntity entity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), entitySpawnParams);
		if (!entity)
			return;
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (rpl)
			rpl.GiveExt(thisPlayerController.GetRplIdentity(), false);
		thisPlayerController.SetControlledEntity(entity);
	}
	
	// ------ RespawnPlayable ------
  void RespawnPlayable(RplId playableId, bool useInitPosition)
  {
    PS_DebugLogger.LogImportant("RespawnPlayable CLIENT playableId=" + playableId.ToString() + " useInitPosition=" + useInitPosition.ToString());
    if (Replication.IsServer())
      RPC_RespawnPlayable(playableId, useInitPosition);
    else
      Rpc(RPC_RespawnPlayable, playableId, useInitPosition);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_RespawnPlayable(RplId playableId, bool useInitPosition)
  {
    PS_DebugLogger.LogImportant("RPC_RespawnPlayable SERVER playableId=" + playableId.ToString());
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
    {
      PS_DebugLogger.Log("RPC_RespawnPlayable REJECTED not admin");
      return;
    }
		
		RplComponent rplComponent = RplComponent.Cast(Replication.FindItem(playableId));
		if (!rplComponent)
			return;
		
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(rplComponent.GetEntity());
		if (!character)
			return;

		EntityPrefabData prefab = character.GetPrefabData();
		if (!prefab)
			return;
		
		PS_PlayableComponent oldPlayableComponent = character.PS_GetPlayable();
		EntitySpawnParams params = new EntitySpawnParams();
		if (useInitPosition)
			oldPlayableComponent.GetSpawnTransform(params.Transform);
		else
			character.GetWorldTransform(params.Transform);
		
		SCR_ChimeraCharacter newCharacter = SCR_ChimeraCharacter.Cast(GetGame().SpawnEntityPrefab(Resource.Load(prefab.GetPrefabName()), GetGame().GetWorld(), params));
		PS_PlayableComponent playableContainer = newCharacter.PS_GetPlayable();

		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		SCR_AIGroup aiGroup = playableManager.GetPlayerGroupByPlayable(oldPlayableComponent.GetRplId());
		if (!aiGroup)
			return;
		SCR_AIGroup playableGroup = aiGroup.GetSlave();
		if (!playableGroup)
			return;
		playableGroup.AddAIEntityToGroup(newCharacter);
		playableManager.SetPlayablePlayerGroupId(playableContainer.GetRplId(), aiGroup.GetGroupID());

		playableContainer.SetPlayable(true);
		oldPlayableComponent.SetPlayable(false);

		character.GetDamageManager().Kill(Instigator.CreateInstigator(newCharacter));
		character.GetDamageManager().SetHealthScaled(0);
		GetGame().GetCallqueue().CallLater(ForceRespawnPlayerLate, 300, false, character, oldPlayableComponent, newCharacter, playableContainer);
	}

	// ------ ForceRespawnPlayer ------
	void ForceRespawnPlayer(bool initPosition = false)
	{
		IEntity camera = GetGame().GetCameraManager().CurrentCamera();
		if (!camera)
			return;
		PS_ManualCameraSpectator cameraSpectator = PS_ManualCameraSpectator.Cast(camera);

		SCR_ChimeraCharacter character;
		if (cameraSpectator)
			character = SCR_ChimeraCharacter.Cast(cameraSpectator.GetCharacterEntity());
		if (!character)
		{
			SCR_AttachEntity attachEntity = SCR_AttachEntity.Cast(camera.GetParent());
			if (!attachEntity)
				return;

			character = SCR_ChimeraCharacter.Cast(attachEntity.GetTarget());
		}
		if (!character)
			return;

		Rpc(RPC_ForceRespawnPlayer, Replication.FindItemId(character), initPosition);
	}
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_ForceRespawnPlayer(RplId respawnEntityRplId, bool initPosition)
  {
    PS_DebugLogger.LogImportant("RPC_ForceRespawnPlayer SERVER entityRplId=" + respawnEntityRplId.ToString() + " initPosition=" + initPosition.ToString());
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
    {
      PS_DebugLogger.Log("RPC_ForceRespawnPlayer REJECTED not admin");
      return;
    }

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(Replication.FindItem(respawnEntityRplId));
		if (!character)
			return;

		EntityPrefabData prefab = character.GetPrefabData();
		if (!prefab)
			return;

		PS_PlayableComponent oldPlayableComponent = character.PS_GetPlayable();
		EntitySpawnParams params = new EntitySpawnParams();
		if (initPosition)
			oldPlayableComponent.GetSpawnTransform(params.Transform);
		else
			character.GetWorldTransform(params.Transform);

		SCR_ChimeraCharacter newCharacter = SCR_ChimeraCharacter.Cast(GetGame().SpawnEntityPrefab(Resource.Load(prefab.GetPrefabName()), GetGame().GetWorld(), params));
		PS_PlayableComponent playableContainer = newCharacter.PS_GetPlayable();

		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		SCR_AIGroup aiGroup = playableManager.GetPlayerGroupByPlayable(oldPlayableComponent.GetRplId());
		if (!aiGroup)
			return;
		SCR_AIGroup playableGroup = aiGroup.GetSlave();
		if (!playableGroup)
			return;
		playableGroup.AddAIEntityToGroup(newCharacter);
		playableManager.SetPlayablePlayerGroupId(playableContainer.GetRplId(), aiGroup.GetGroupID());

		playableContainer.SetPlayable(true);
		oldPlayableComponent.SetPlayable(false);

		character.GetDamageManager().Kill(Instigator.CreateInstigator(newCharacter));
		character.GetDamageManager().SetHealthScaled(0);
		GetGame().GetCallqueue().CallLater(ForceRespawnPlayerLate, 300, false, character, oldPlayableComponent, newCharacter, playableContainer);
	}

	void ForceRespawnPlayerLate(SCR_ChimeraCharacter character, PS_PlayableComponent oldPlayableComponent, SCR_ChimeraCharacter newCharacter, PS_PlayableComponent playableContainer)
	{
		character.GetDamageManager().Kill(Instigator.CreateInstigator(newCharacter));
		character.GetDamageManager().SetHealthScaled(0);
		if (!character.GetDamageManager().IsDestroyed())
		{
			GetGame().GetCallqueue().CallLater(ForceRespawnPlayerLate, 300, false, character, oldPlayableComponent, newCharacter, playableContainer);
			return;
		}

		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		PS_VoNChannelsManager VoNChannelsManager = PS_VoNChannelsManager.GetInstance();
		SCR_AIGroup aiGroup = playableManager.GetPlayerGroupByPlayable(oldPlayableComponent.GetRplId());
		if (!aiGroup)
			return;
		playableManager.SetPlayablePlayerGroupId(playableContainer.GetRplId(), aiGroup.GetGroupID());
		int playerId = playableManager.GetPlayerByPlayableRemembered(oldPlayableComponent.GetRplId());
		// BUGFIX: same issue as SwitchToObserver — empty factionKey/roomName produces
		// channelKey="" which causes the RPC handler to REMOVE the player from
		// m_PlayerChannelKeyMap. Force the respawned player into the Global room so
		// they remain visible/audible to the rest of the lobby until the
		// post-respawn slot assignment routes them to the proper faction/group room
		// (handled by the next MoveToRoom call a few lines later in this flow).
		VoNChannelsManager.MoveToRoom(playerId, "", "#PS-VoNRoom_Global");
		if (playerId > -1)
		{
			GetGame().GetCallqueue().CallLater(ForceRespawnPlayerLate2, 500, false, playerId, playableContainer);
		}
	}
	void ForceRespawnPlayerLate2(int playerId, PS_PlayableComponent playable)
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		playableManager.SetPlayerToSlot(playable.GetRplId(), playerId);
		ForceSwitch(playerId);
	}

  override protected void OnPostInit(IEntity owner)
  {
    PS_DebugLogger.LogImportant("PS_PlayableControllerComponent OnPostInit isServer=" + Replication.IsServer().ToString() + " isOwner=" + RplComponent.Cast(owner.FindComponent(RplComponent)).IsOwner().ToString());

    // Throttle from 0ms (every frame) to 200ms (5x/sec) to avoid per-frame
    // replication traffic when the lobby entity is parked. The function already
    // has distance-based early-outs, but the 0ms CallLater itself executes
    // continuously and causes micro-jitter transform replication.
    // See Echo Lobby pattern: they do not teleport parked entities every frame.
    GetGame().GetCallqueue().CallLater(UpdatePosition, 200, true, false);
    SetEventMask(GetOwner(), EntityEvent.FRAME);
    SCR_PlayerController playerController = SCR_PlayerController.Cast(PlayerController.Cast(GetOwner()));
    playerController.m_OnControlledEntityChanged.Insert(OnControlledEntityChanged);

    PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
    if (!gameModeCoop)
    {
      PS_DebugLogger.LogError("PS_PlayableControllerComponent OnPostInit: gameModeCoop NULL");
      return;
    }

    ScriptInvokerBase<SCR_BaseGameMode_OnPlayerRoleChanged> onPlayerRoleChanged = gameModeCoop.GetOnPlayerRoleChange();
    if (!onPlayerRoleChanged)
      return;

    onPlayerRoleChanged.Insert(OnPlayerRoleChange);

    gameModeCoop.GetOnPlayerDisconnected().Insert(OnPlayerDisconnected);

    PS_DebugLogger.LogImportant("PS_PlayableControllerComponent OnPostInit COMPLETE playerId=" + playerController.GetPlayerId().ToString());
  }
	
	void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		if (!thisPlayerController || thisPlayerController.GetPlayerId() != playerId)
			return;
		GetGame().GetCallqueue().Remove(UpdatePosition);
		ClearEventMask(GetOwner(), EntityEvent.FRAME);
	}

	void OnPlayerRoleChange(int playerId, EPlayerRole roleFlags)
	{
		m_eOnPlayerRoleChange.Invoke(playerId, roleFlags);
	}

	// We change to VoN boi lets enable camera
	void OnControlledEntityChanged(IEntity from, IEntity to)
    {
        PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
        if (!thisPlayerController)
        {
            PS_DebugLogger.LogError("OnControlledEntityChanged: thisPlayerController NULL — aborting");
            return;
        }
        int playerId = thisPlayerController.GetPlayerId();
        string fromName = "NULL";
        if (from) fromName = from.GetPrefabData().GetPrefabName();
        string toName = "NULL";
        if (to) toName = to.GetPrefabData().GetPrefabName();
        PS_DebugLogger.Log("OnControlledEntityChanged playerId=" + playerId.ToString() + " from=" + fromName + " to=" + toName, playerId);

        RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
        if (!rpl.IsOwner())
            return;
        if (!from && !m_bAfterInitialSwitch)
        {
            PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
            if (!gameModeCoop)
            {
                PS_DebugLogger.LogError("OnControlledEntityChanged: gameModeCoop NULL during JIP check — skipping observer switch");
                return;
            }
            if (gameModeCoop.GetState() == SCR_EGameModeState.GAME)
            {
                PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
                RplId playerSlot = playableManager.GetPlayableByPlayer(playerId);
                if (playerSlot == RplId.Invalid())
                {
                    PS_DebugLogger.Log("OnControlledEntityChanged JIP no slot — switching to observer", playerId);
                    SwitchToObserver(null);
                }
            }
            return;
        }
		if (!to && !m_bAfterInitialSwitch)
			return;
		if (!to) {
			m_vObserverPosition = from.GetOrigin();
		}
		m_bAfterInitialSwitch = true;
		
    PS_LobbyVoNComponent vonFrom;
    if (from)
      vonFrom = PS_LobbyVoNComponent.Cast(from.FindComponent(PS_LobbyVoNComponent));
    PS_LobbyVoNComponent vonTo;
    if (to)
      vonTo = PS_LobbyVoNComponent.Cast(to.FindComponent(PS_LobbyVoNComponent));

    string vonFromStr = "NO";
    if (vonFrom) vonFromStr = "YES";
    string vonToStr = "NO";
    if (vonTo) vonToStr = "YES";
    PS_DebugLogger.Log("OnControlledEntityChanged vonFrom=" + vonFromStr + " vonTo=" + vonToStr, playerId);

    if (!vonTo)
    {
      PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
      if (!gameModeCoop)
      {
        PS_DebugLogger.LogError("OnControlledEntityChanged: gameModeCoop NULL during VoN transition — skipping GAME logic");
        return;
      }
      PS_DebugLogger.Log("OnControlledEntityChanged entity has NO VoN — game state=" + typename.EnumToString(SCR_EGameModeState, gameModeCoop.GetState()), playerId);
      if (gameModeCoop.GetState() == SCR_EGameModeState.GAME)
      {
        // Guard: skip all transition logic when entity is being detached (death transition,
        // spectator mode, or entity despawn). Prevents SwitchFromObserver from destroying
        // the spectator camera, and prevents ForceNotifyEditorPlayerSpawned from firing
        // OnPlayerSpawned with a null entity.
        if (!to)
        {            PS_DebugLogger.Log("OnControlledEntityChanged entity detached, skipping GAME transition logic", playerId);
          return;
        }

        // Apply VoN encryption keys to the new character's radios (playable character has no PS_LobbyVoNComponent)
        ApplyCurrentVoNKeys();

        GetGame().GetCallqueue().Call(ForceNotifyEditorPlayerSpawned, thisPlayerController.GetPlayerId(), to);
        PS_DebugLogger.Log("OnControlledEntityChanged switching from observer (!vonTo)", playerId);
        SwitchFromObserver();
      }
    }
  }
	
	// Notify editor core that player is alive (bypasses missing spawns in some game modes)
	void ForceNotifyEditorPlayerSpawned(int playerId, IEntity entity)
    {
        PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
        if (!gameModeCoop)
        {
            PS_DebugLogger.LogError("ForceNotifyEditorPlayerSpawned: gameModeCoop NULL");
            return;
        }
        if (gameModeCoop.IsFreezeTimeEnd() && gameModeCoop.GetDisableBuildingModeAfterFreezeTime())
             return;
        gameModeCoop.GetOnPlayerSpawned().Invoke(playerId, entity);
        Rpc(ForceNotifyServerPlayerSpawned, playerId, Replication.FindItemId(entity));
    }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void ForceNotifyServerPlayerSpawned(int playerId, RplId entityId)
  {
    PS_DebugLogger.LogImportant("ForceNotifyServerPlayerSpawned SERVER player=" + playerId.ToString() + " entityId=" + entityId.ToString(), playerId);
    IEntity entity = IEntity.Cast(Replication.FindItem(entityId));
    SCR_BaseGameMode.Cast(GetGame().GetGameMode()).GetOnPlayerSpawned().Invoke(playerId, entity);	}

	override protected void EOnFrame(IEntity owner, float timeSlice)
    {
        PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
        if (!gameMode)
        {
            PS_DebugLogger.LogError("PS_PlayableControllerComponent EOnFrame: gameMode NULL — clearing frame event mask");
            ClearEventMask(GetOwner(), EntityEvent.FRAME);
            return;
        }
        if ((gameMode.GetState() == SCR_EGameModeState.GAME && gameMode.IsFreezeTimeEnd()) || !gameMode.IsFreezeTimeShootingForbiden())
        {
            ClearEventMask(GetOwner(), EntityEvent.FRAME);
            return;
        }
		
		if (!GetGame().GetPlayerController())
			return;
		if (PS_PlayersHelper.IsAdminOrServer())
			return;
		
		PlayerController playerController = PlayerController.Cast(owner);
		ActionManager actionManager = playerController.GetActionManager();
		if (!actionManager)
			return;
		
		actionManager.SetActionValue("CharacterFire", 0);
		actionManager.SetActionValue("CharacterThrowGrenade", 0);
		actionManager.SetActionValue("CharacterMelee", 0);
		actionManager.SetActionValue("CharacterFireStatic", 0);
		actionManager.SetActionValue("TurretFire", 0);
		actionManager.SetActionValue("VehicleFire", 0);
		actionManager.SetActionValue("VehicleHorn", 0);
		
		IEntity character = playerController.GetControlledEntity();
		if (character)
		{
			Vehicle vehicle = Vehicle.Cast(character.GetRootParent());
			if (vehicle)
			{
				if (!vehicle.IsEnableMoveOnFreeze())
				{
					DisableVehicleMove(actionManager);
					VehicleWheeledSimulation vehicleWheeledSimulation = VehicleWheeledSimulation.Cast(vehicle.FindComponent(VehicleWheeledSimulation));
					if (vehicleWheeledSimulation)
					{
						if (vehicleWheeledSimulation.EngineIsOn())
							vehicleWheeledSimulation.EngineStop();
					}
				}
			}
		}
		
		if (m_bOutFreezeTime)
		{
			actionManager.SetActionValue("CharacterForward", 0);
			actionManager.SetActionValue("CharacterRight", 0);
			actionManager.SetActionValue("CharacterTurnUp", 0);
			actionManager.SetActionValue("CharacterTurnRight", 0);
			actionManager.SetActionValue("GetOut", 0);
			actionManager.SetActionValue("JumpOut", 0);
			actionManager.SetActionValue("CharacterStand", 0);
			actionManager.SetActionValue("CharacterCrouch", 0);
			actionManager.SetActionValue("CharacterProne", 0);
			actionManager.SetActionValue("CharacterStandCrouchToggle", 0);
			actionManager.SetActionValue("CharacterStandProneToggle", 0);
			actionManager.SetActionValue("CharacterRoll", 0);
			actionManager.SetActionValue("CharacterJump", 0);
			
			DisableVehicleMove(actionManager);
			
			if (character)
			{
				Vehicle vehicle = Vehicle.Cast(character.GetRootParent());
				if (vehicle)
				{
					BaseVehicleNodeComponent vehicleNodeComponent = BaseVehicleNodeComponent.Cast(vehicle.FindComponent(BaseVehicleNodeComponent));
					if (vehicleNodeComponent)
					{
						SCR_HelicopterControllerComponent helicopterControllerComponent = SCR_HelicopterControllerComponent.Cast(vehicleNodeComponent.FindComponent(SCR_HelicopterControllerComponent));
						if (!helicopterControllerComponent.GetAutohoverEnabled())
						{
							actionManager.SetActionValue("AutohoverToggle", 1);
						}
					}
				}
			}
		}
		
		if (character)
		{
			SCR_ChimeraCharacter chimeraChar = SCR_ChimeraCharacter.Cast(character);
			if (chimeraChar)
				chimeraChar.GetDamageManager().SetHealthScaled(1);
		}
	}
	
	void DisableVehicleMove(ActionManager actionManager)
	{
		actionManager.SetActionValue("VehicleEngineStop", 1);
		actionManager.SetActionValue("VehicleEngineStart", 0);
		actionManager.SetActionValue("AutohoverToggle", 0);
		actionManager.SetActionValue("WheelBrake", 0);
		actionManager.SetActionValue("WheelBrakePersistent", 1);
		actionManager.SetActionValue("CyclicForward", 0);
		actionManager.SetActionValue("CyclicBack", 0);
		actionManager.SetActionValue("CyclicLeft", 0);
		actionManager.SetActionValue("CyclicRight", 0);
		actionManager.SetActionValue("AntiTorqueLeft", 0);
		actionManager.SetActionValue("AntiTorqueRight", 0);
		actionManager.SetActionValue("CollectiveIncrease", 0);
		actionManager.SetActionValue("CollectiveDecrease", 0);
		actionManager.SetActionValue("HelicopterEngineStop", 0);
		actionManager.SetActionValue("HelicopterEngineStart", 0);
		
		actionManager.SetActionValue("CarThrust", 0);
		actionManager.SetActionValue("CarBrake", 0);
		actionManager.SetActionValue("CarSteering", 0);
		actionManager.SetActionValue("CarTurbo", 0);
		actionManager.SetActionValue("CarTurboToggle", 0);
		actionManager.SetActionValue("CarShift", 0);
		actionManager.SetActionValue("CarShiftReverse", 0);
		actionManager.SetActionValue("CarHandBrake", 1);
		actionManager.SetActionValue("CarHandBrakePersistent", 0);
		actionManager.SetActionValue("CarLightsHiBeamToggle", 0);
		actionManager.SetActionValue("CarHazardLights", 0);
	}

	protected CameraManager m_CachedCameraManager;

	void CacheCameraManager()
	{
		if (!m_CachedCameraManager)
			m_CachedCameraManager = GetGame().GetCameraManager();
	}

	void UpdatePosition(bool force)
	{
		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		if (!rpl.IsOwner())
			return;

		// Echo Lobby pattern: skip the whole per-frame work while the spectator menu is
		// the top menu OR the spectator camera is active. In spectator the lobby entity
		// is parked at a fixed altitude and doesn't need to be re-teleported/transformed
		// every frame. Running this loop during spectator produces per-frame replication
		// traffic that fills the connection buffer and causes REPLICATION_FLOODED kicks
		// (see TodayFixes.MD).
		MenuBase topMenuEarly = GetGame().GetMenuManager().GetTopMenu();
		if (topMenuEarly && topMenuEarly.IsInherited(PS_SpectatorMenu))
			return;
		// Also skip if spectator camera is active (catches transition before menu is top)
		CameraBase cam = GetGame().GetCameraManager().CurrentCamera();
		if (cam && cam.IsInherited(PS_ManualCameraSpectator))
			return;

		// Additional early-out: in GAME state with no menu open, the player is either
		// controlling their character (alive) or spectating (dead). In both cases the
		// lobby entity is not needed and should not be teleported.
		PS_GameModeCoop gameModeCheck = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (gameModeCheck && gameModeCheck.GetState() == SCR_EGameModeState.GAME && !force)
		{
			MenuBase topMenu = GetGame().GetMenuManager().GetTopMenu();
			if (!topMenu)
				return;
		}

		// Lets fight with phisyc engine
		if (m_InitialEntity)
		{
			PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
			int playerId = thisPlayerController.GetPlayerId();
			m_vVoNPosition = Vector(0, 100000, 0) + Vector(1000 * Math.Mod(playerId, 10), 5000 * Math.Floor(Math.Mod(playerId, 100) / 10), 5000 * Math.Floor(playerId / 100));
		vector currentOrigin = m_InitialEntity.GetOrigin();
		// Early-out: skip the transform/physics/camera work when the entity is already
		// near its parked position. Using a distance threshold instead of strict equality
		// prevents per-frame SetTransform calls caused by tiny floating-point drift or
		// network interpolation jitter, which replicate continuously and fill the connection
		// buffer (see TodayFixes.MD REPLICATION_FLOODED investigation).
		if (vector.Distance(currentOrigin, m_vVoNPosition) < 0.1 && !force)
			return;
		//Print("Move to: " + m_vVoNPosition.ToString());
			
			GameEntity gameEntity = GameEntity.Cast(m_InitialEntity);
			vector mat[4];
			Math3D.MatrixIdentity4(mat);
			mat[3] = m_vVoNPosition;
			if (force)
				gameEntity.Teleport(mat);
			gameEntity.SetTransform(mat);
			
			// Cache camera manager to avoid repeated expensive lookups
			CacheCameraManager();
			bool isMenu = false;
			MenuBase menu = GetGame().GetMenuManager().GetTopMenu();
			if (menu && (menu.IsInherited(PS_PreviewMapMenu) || menu.IsInherited(PS_CoopLobby) || menu.IsInherited(PS_BriefingMapMenu)))
			{
				isMenu = true;
			if (m_CachedCameraManager)
			{
				CameraBase menuCam = m_CachedCameraManager.CurrentCamera();
				if (menuCam)
					menuCam.SetWorldTransform(mat);
				}
				if (m_Camera)
					m_Camera.SetTransform(mat);	
			}

			// Who broke camera on map?
			if (m_CachedCameraManager && !isMenu)
			{
				CameraBase cameraBase = m_CachedCameraManager.CurrentCamera();
				if (cameraBase)
					cameraBase.ApplyTransform(GetGame().GetWorld().GetTimeSlice());
			}
 

		Physics physics = m_InitialEntity.GetPhysics();
		if (physics && !m_bPhysicsInactive)
		{
			//physics.SetVelocity("0 0 0");
			//physics.SetAngularVelocity("0 0 0");
			//physics.SetMass(0);
			//physics.SetDamping(1, 1);
			//physics.ChangeSimulationState(SimulationState.NONE);
			physics.SetActive(ActiveState.INACTIVE);
			m_bPhysicsInactive = true;
		}
		} else {
			PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
			IEntity entity = thisPlayerController.GetControlledEntity();
			if (entity)
			{
				PS_LobbyVoNComponent von = PS_LobbyVoNComponent.Cast(entity.FindComponent(PS_LobbyVoNComponent));
				if (von)
				{
					m_InitialEntity = entity;
					m_bPhysicsInactive = false; // new entity - physics state unknown, re-apply INACTIVE next frame
				}
			}
		}
	}

	// Save VoN boi for reuse
	IEntity GetInitialEntity()
	{
		return m_InitialEntity;
	}
	void SetInitialEntity(IEntity initialEntity)
	{
		m_InitialEntity = initialEntity;
		m_bPhysicsInactive = false; // new entity - physics state unknown, re-apply INACTIVE next frame
	}

  void ChangeFactionKey(int playerId, FactionKey factionKey)
  {
    PS_DebugLogger.LogImportant("ChangeFactionKey CLIENT player=" + playerId.ToString() + " faction=" + factionKey, playerId);
    Rpc(RPC_ChangeFactionKey, playerId, factionKey);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_ChangeFactionKey(int playerId, FactionKey factionKey)
  {
    PS_DebugLogger.LogImportant("RPC_ChangeFactionKey SERVER player=" + playerId.ToString() + " faction=" + factionKey, playerId);
    PlayerManager playerManager = GetGame().GetPlayerManager();
    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();

    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    EPlayerRole playerRole = playerManager.GetPlayerRoles(thisPlayerController.GetPlayerId());

    if (thisPlayerController.GetPlayerId() != playerId && playerRole == EPlayerRole.NONE)
    {
      PS_DebugLogger.Log("RPC_ChangeFactionKey REJECTED not self and not admin", playerId);
      return;
    }
    if (playableManager.GetPlayerPin(playerId) && playerRole == EPlayerRole.NONE)
    {
      PS_DebugLogger.Log("RPC_ChangeFactionKey REJECTED player is pinned", playerId);
      return;
    }

		// Check faction balance (admins bypass)
		if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
		{
			PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
			if (gameModeCoop && !gameModeCoop.CanJoinFaction(factionKey, playableManager.GetPlayerFactionKey(playerId)))
			{
				PS_DebugLogger.Log("RPC_ChangeFactionKey REJECTED faction balance", playerId);
				return;
			}
		}

    playableManager.SetPlayerFactionKey(playerId, factionKey);
  }

	// ------------------ VoN controlls ------------------
  void MoveToVoNRoomByKey(int playerId, string roomKey)
  {
    PS_DebugLogger.Log("MoveToVoNRoomByKey CLIENT player=" + playerId.ToString() + " roomKey=" + roomKey, playerId);
    string factionKey = "";
    string roomName = "#PS-VoNRoom_Global";

    if (roomKey.Contains("|")) {
      array<string> outTokens = {};
      roomKey.Split("|", outTokens, false);
      factionKey = outTokens[0];
      roomName = outTokens[1];
    }

    Rpc(RPC_MoveVoNToRoom, playerId, factionKey, roomName);
  }
  void MoveToVoNRoom(int playerId, FactionKey factionKey, string roomName)
  {
    PS_DebugLogger.Log("MoveToVoNRoom CLIENT player=" + playerId.ToString() + " faction=" + factionKey + " room=" + roomName, playerId);
    Rpc(RPC_MoveVoNToRoom, playerId, factionKey, roomName);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_MoveVoNToRoom(int playerId, FactionKey factionKey, string roomName)
  {
    PS_DebugLogger.LogImportant("RPC_MoveVoNToRoom SERVER player=" + playerId.ToString() + " faction=" + factionKey + " room=" + roomName, playerId);
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    if (playerId != thisPlayerController.GetPlayerId() && !SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
    {
      PS_DebugLogger.Log("RPC_MoveVoNToRoom REJECTED not self and not admin", playerId);
      return;
    }

    PS_VoNChannelsManager VoNChannelsManager = PS_VoNChannelsManager.GetInstance();
    VoNChannelsManager.MoveToRoom(playerId, factionKey, roomName);
  }

	PS_LobbyVoNComponent GetVoN()
	{
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		IEntity entity = thisPlayerController.GetControlledEntity();
		if (!entity)
			return null;
		PS_LobbyVoNComponent von = PS_LobbyVoNComponent.Cast(entity.FindComponent(PS_LobbyVoNComponent));
		return von;
	}
	RadioTransceiver GetVoNTransiver(int radioId)
	{
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		IEntity entity = thisPlayerController.GetControlledEntity();
		if (!entity)
			return null;
		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.Cast(entity.FindComponent(SCR_GadgetManagerComponent));
		if (!gadgetManager)
			return null;
		array<SCR_GadgetComponent> radios = gadgetManager.GetGadgetsByType(EGadgetType.RADIO);
		if (!radios || radios.Count() <= radioId)
			return null;
		IEntity radioEntity = radios[radioId].GetOwner();
		if (!radioEntity)
			return null;
		BaseRadioComponent radio = BaseRadioComponent.Cast(radioEntity.FindComponent(BaseRadioComponent));
		if (!radio)
			return null;
		radio.SetPower(true);
		RadioTransceiver transiver = RadioTransceiver.Cast(radio.GetTransceiver(0));
		if (!transiver)
			return null;
		transiver.SetFrequency(radioId + 1);
		return transiver;
	}
	void LobbyVoNEnable()
	{
		UpdatePosition(true);
		GetGame().GetCallqueue().Remove(LobbyVoNDisableDelayed);
		PS_LobbyVoNComponent von = GetVoN();
		if (!von)
			return;
		von.SetTransmitRadio(GetVoNTransiver(1));
		von.SetCommMethod(ECommMethod.SQUAD_RADIO);
		von.SetCapture(true);
	}
	void LobbyVoNRadioEnable()
	{
		UpdatePosition(true);
		GetGame().GetCallqueue().Remove(LobbyVoNDisableDelayed);
		PS_LobbyVoNComponent von = GetVoN();
		if (!von)
			return;
		von.SetTransmitRadio(GetVoNTransiver(0));
		von.SetCommMethod(ECommMethod.SQUAD_RADIO);
		von.SetCapture(true);
	}
	void LobbyVoNDisable()
	{
		// Delay VoN disable
		GetGame().GetCallqueue().CallLater(LobbyVoNDisableDelayed, PS_LobbyVoNComponent.PS_TRANSMISSION_TIMEOUT_MS);
	}
	void LobbyVoNDisableDelayed()
	{
		PS_LobbyVoNComponent von = GetVoN();
		if (!von)
			return;
		von.SetCommMethod(ECommMethod.DIRECT);
		von.SetCapture(false);
	}

	void LobbyVoNDisableImmediate()
	{
		GetGame().GetCallqueue().Remove(LobbyVoNDisableDelayed);
		LobbyVoNDisableDelayed();
	}
	// Separate radio VoNs, CALL IT FROM SERVER
	void SetVoNKey(string VoNKey, string VoNKeyLocal)
	{
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		IEntity entity = thisPlayerController.GetControlledEntity();
		if (!entity)
			return;
		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.Cast(entity.FindComponent(SCR_GadgetManagerComponent));
		if (!gadgetManager)
			return;
		array<SCR_GadgetComponent> radios = gadgetManager.GetGadgetsByType(EGadgetType.RADIO);
		if (radios.Count() > 0)
		{
			BaseRadioComponent radio = BaseRadioComponent.Cast(radios[0].GetOwner().FindComponent(BaseRadioComponent));
			if (radio)
				radio.SetEncryptionKey(VoNKey);
		}
		if (radios.Count() > 1)
		{
			BaseRadioComponent radioLocal = BaseRadioComponent.Cast(radios[1].GetOwner().FindComponent(BaseRadioComponent));
			if (radioLocal)
				radioLocal.SetEncryptionKey(VoNKeyLocal);
		}
	}

	// Apply VoN encryption keys from the current PS_VoNChannelsManager room
	void ApplyCurrentVoNKeys()
	{
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		if (!thisPlayerController)
			return;
		IEntity entity = thisPlayerController.GetControlledEntity();
		if (!entity)
			return;

		PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();
		if (!vonManager)
			return;

		int playerId = thisPlayerController.GetPlayerId();
		string channelKey = vonManager.GetPlayerChannelKey(playerId);
		if (channelKey == "")
			return;

		string factionKey = "";
		string roomName = channelKey;
		if (channelKey.Contains("|"))
		{
			array<string> tokens = {};
			channelKey.Split("|", tokens, false);
			factionKey = tokens[0];
			roomName = tokens[1];
		}

		string encryptionKey = "Menu" + factionKey + roomName;

		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.Cast(entity.FindComponent(SCR_GadgetManagerComponent));
		if (!gadgetManager)
			return;
		array<SCR_GadgetComponent> radios = gadgetManager.GetGadgetsByType(EGadgetType.RADIO);
		if (radios.Count() > 0)
		{
			BaseRadioComponent radio = BaseRadioComponent.Cast(radios[0].GetOwner().FindComponent(BaseRadioComponent));
			if (radio)
				radio.SetEncryptionKey(encryptionKey);
		}
		if (radios.Count() > 1)
		{
			BaseRadioComponent radioLocal = BaseRadioComponent.Cast(radios[1].GetOwner().FindComponent(BaseRadioComponent));
			if (radioLocal)
				radioLocal.SetEncryptionKey(channelKey);
		}
	}
	bool isVonInit()
	{
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		if (!thisPlayerController)
			return false;
		IEntity entity = thisPlayerController.GetControlledEntity();
		if (!entity)
			return false;
		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.Cast(entity.FindComponent(SCR_GadgetManagerComponent));
		if (!gadgetManager)
			return false;
		IEntity radioEntity = gadgetManager.GetGadgetByType(EGadgetType.RADIO);
		return radioEntity != null;
	}
	
  void GetArmaIdFromServer(int playerId)
  {
    PS_DebugLogger.Log("GetArmaIdFromServer CLIENT player=" + playerId.ToString(), playerId);
    Rpc(RPC_GetArmaIdFromServer_Server, playerId);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_GetArmaIdFromServer_Server(int playerId)
  {
    PS_DebugLogger.LogImportant("RPC_GetArmaIdFromServer_Server player=" + playerId.ToString(), playerId);
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    if (playerId != thisPlayerController.GetPlayerId() && !SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
      return;

    string playerUUID = GetGame().GetBackendApi().GetPlayerPlatformId(playerId);
    Rpc(RPC_GetArmaIdFromServer_Owner, playerUUID);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
  void RPC_GetArmaIdFromServer_Owner(string playerUUID)
  {
    PS_DebugLogger.LogImportant("RPC_GetArmaIdFromServer_Owner uuid=" + playerUUID);
    System.ExportToClipboard(playerUUID);
  }

	// ------------------ Observer camera controlls ------------------
  // Call of Duty-style death screen: when a player dies in GAME state, the server's
  // HandlePlayerKilled path calls SwitchToDeathScreenServer() (instead of
  // SwitchToObserverServer). The death-screen RPC opens a black overlay + quote menu
  // on the client, and on close the local client chains to SwitchToObserver(null) to
  // continue into the normal spectator flow.
  //
  // Crucially, the death screen only fires from the death path (HandlePlayerKilled).
  // JIP-without-slot (OnControlledEntityChanged) and game-start-without-role
  // (RPC_RequestDeployForAllPlayers) still go through SwitchToObserverServer directly,
  // so they don't get the death-screen preamble.
  void SwitchToDeathScreenServer(vector observerPosition = Vector(0, 0, 0))
  {
    Print("[DS][SRV] SwitchToDeathScreenServer pos=" + observerPosition.ToString(), LogLevel.NORMAL);
    Rpc(RPC_SwitchToDeathScreenServer, observerPosition);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
  void RPC_SwitchToDeathScreenServer(vector observerPosition)
  {
    Print("[DS][CLI] RPC_SwitchToDeathScreenServer OWNER RECEIVED pos=" + observerPosition.ToString(), LogLevel.NORMAL);
    if (observerPosition != "0 0 0")
      m_vObserverPosition = observerPosition;
    SwitchToDeathScreen();
  }

  void SwitchToDeathScreen()
  {
    Print("[DS][CLI] SwitchToDeathScreen START spawning observer camera at body pos, then opening death screen menu in " + PS_DeathScreenMenu.EYES_DELAY_S.ToString() + "s", LogLevel.NORMAL);

    // CRITICAL: Spawn the manual camera at the dead body's position IMMEDIATELY so
    // the vanilla death camera (which flies the view "very up high") never appears
    // to the player. The death screen menu will overlay on top of this camera view.
    // When the death screen menu closes, SwitchToObserver will reuse the existing
    // m_Camera entity (it just opens the spectator menu).
    if (!m_Camera)
      SpawnObserverCameraAtObserverPos();

    // Phase 1: keep camera at dead body's eyes for EYES_DELAY_S
    // Then open the death screen menu (handles 2s fade + 8s quote internally).
    // The death screen menu's OnMenuClose chains to local SwitchToObserver(null) so
    // the player continues into normal spectator after the quote.
    GetGame().GetCallqueue().CallLater(OpenDeathScreenMenu, PS_DeathScreenMenu.EYES_DELAY_S * 1000, false);
  }

  // Spawn the manual spectator camera at the dead body's position without opening
  // the spectator menu. Used by the death flow so the vanilla death camera does not
  // fly the player's view "very up high" during the death screen.
  protected void SpawnObserverCameraAtObserverPos()
  {
    Print("[DS][CLI] SpawnObserverCameraAtObserverPos observerPos=" + m_vObserverPosition.ToString(), LogLevel.NORMAL);
    EntitySpawnParams spawnParams = new EntitySpawnParams();
    if (m_vObserverPosition != "0 0 0")
      spawnParams.Transform[3] = m_vObserverPosition;
    Resource resource = Resource.Load("{6EAA30EF620F4A2E}Prefabs/Editor/Camera/ManualCameraSpectator.et");
    m_Camera = GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), spawnParams);
    if (m_Camera)
    {
      if (m_vObserverPosition != "0 0 0")
        m_Camera.SetOrigin(m_vObserverPosition);
      GetGame().GetCameraManager().SetCamera(CameraBase.Cast(m_Camera));
      Print("[DS][CLI] SpawnObserverCameraAtObserverPos camera spawned and set", LogLevel.NORMAL);
    }
    else
    {
      Print("[DS][CLI] SpawnObserverCameraAtObserverPos FAILED to spawn camera", LogLevel.WARNING);
    }
  }
  void OpenDeathScreenMenu()
  {
    Print("[DS][CLI] OpenDeathScreenMenu FIRING OpenMenu(DeathScreen)", LogLevel.NORMAL);
    MenuBase openedMenu = GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.DeathScreen);
    if (openedMenu)
      Print("[DS][CLI] OpenDeathScreenMenu result=OPENED", LogLevel.NORMAL);
    else
      Print("[DS][CLI] OpenDeathScreenMenu result=NULL (preset missing? layout missing?)", LogLevel.WARNING);
  }

  void SwitchToObserverServer(vector observerPosition = Vector(0, 0, 0))
  {
    Print("[DS][SRV] SwitchToObserverServer pos=" + observerPosition.ToString() + " [INTERCEPTOR — should NOT fire on death path]", LogLevel.WARNING);
    Rpc(RPC_SwitchToObserverServer, observerPosition);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
  void RPC_SwitchToObserverServer(vector observerPosition)
  {
    Print("[DS][CLI] RPC_SwitchToObserverServer OWNER [INTERCEPTOR — should NOT fire on death path]", LogLevel.WARNING);
    if (observerPosition != "0 0 0")
      m_vObserverPosition = observerPosition;
    SwitchToObserver(null);
  }

	void SaveCameraTransform()
	{
		SCR_CameraEditorComponent cameraManager = SCR_CameraEditorComponent.Cast(SCR_BaseEditorComponent.GetInstance(SCR_CameraEditorComponent, false));
		cameraManager.GetLastCameraTransform(lastCameraTransform);
	}

	void SwitchToObserver(IEntity from)
	{
		string fromName = "NULL";
		if (from)
			fromName = from.GetPrefabData().GetPrefabName();
		Print("[DS][CLI] SwitchToObserver ENTRY from=" + fromName + " [INTERCEPTOR — any call here means something is bypassing the death screen]", LogLevel.WARNING);
		SCR_EditorManagerEntity editorManagerEntity = SCR_EditorManagerEntity.GetInstance();
		if (editorManagerEntity && editorManagerEntity.IsOpened())
		{
			Print("[DS][CLI] SwitchToObserver ABORT editor open", LogLevel.WARNING);
			return;
		}

		// If the camera was already spawned by the death flow (SpawnObserverCameraAtObserverPos),
		// reuse it — just open the spectator menu and let the player take control of the existing
		// manual camera (positioned at the dead body's location).
		if (m_Camera)
		{
			Print("[DS][CLI] SwitchToObserver reusing existing camera (spawned by death flow), opening spectator menu", LogLevel.NORMAL);
			GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.SpectatorMenu);
			return;
		}
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.SpectatorMenu);
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		IEntity entity = thisPlayerController.GetControlledEntity();
		EntitySpawnParams params = new EntitySpawnParams();
		if (from)
			from.GetTransform(params.Transform);
		// BUGFIX: previously called MoveToVoNRoom(playerId, "", "") which translates to
		// BuildChannelKey("", "") = "" and then SetPlayerToChannel(playerId, "") — the RPC
		// handler removes the player from m_PlayerChannelKeyMap when the key is empty, so
		// the spectator silently vanished from the Global room and any other room they
		// were in. The user-visible symptom was "3 clients connected, only 2 in Global".
		// Use MoveToVoNRoomByKey("") which defaults roomName to #PS-VoNRoom_Global so the
		// spectator stays present in the Global room count.
		MoveToVoNRoomByKey(thisPlayerController.GetPlayerId(), "");
		Resource resource = Resource.Load("{6EAA30EF620F4A2E}Prefabs/Editor/Camera/ManualCameraSpectator.et");
		m_Camera = GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), params);

		if (m_vObserverPosition != "0 0 0") {
			m_Camera.SetOrigin(m_vObserverPosition);
			m_vObserverPosition = "0 0 0";
		} else if (lastCameraTransform[3][1] < 10000 && lastCameraTransform[3][1] > 0) {
			m_Camera.SetTransform(lastCameraTransform);
			lastCameraTransform[3][1] = 10000;
		} else {
			SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
			m_Camera.SetOrigin(mapEntity.Size() / 2.0 + vector.Up * 100);
		}
		GetGame().GetCameraManager().SetCamera(CameraBase.Cast(m_Camera));

		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (gameMode && gameMode.GetFriendliesSpectatorOnly())
		{
			PS_ManualCameraSpectator cameraSpectator = PS_ManualCameraSpectator.Cast(m_Camera);
			if (cameraSpectator)
				cameraSpectator.SetCharacterEntityMove(from);
		}
	}

	void SwitchFromObserver()
	{
		if (!m_Camera)
			return;
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.SpectatorMenu);
		SCR_EntityHelper.DeleteEntityAndChildren(m_Camera);
		m_Camera = null;
	}

	// Force change game state
  void ForceGameStart()
  {
    PS_DebugLogger.LogImportant("ForceGameStart CLIENT");
    Rpc(RPC_ForceGameStart);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  protected void RPC_ForceGameStart()
  {
    PS_DebugLogger.LogImportant("RPC_ForceGameStart SERVER");
    PlayerManager playerManager = GetGame().GetPlayerManager();
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    EPlayerRole playerRole = playerManager.GetPlayerRoles(thisPlayerController.GetPlayerId());
    if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
    {
      PS_DebugLogger.Log("RPC_ForceGameStart REJECTED not admin");
      return;
    }

    PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
    if (!gameMode)
    {
      PS_DebugLogger.LogError("RPC_ForceGameStart: gameMode NULL");
      return;
    }
    if (gameMode.GetState() == SCR_EGameModeState.PREGAME)
      gameMode.StartGameMode();
  }

  void ForceSwitch(int playerId)
  {
    PS_DebugLogger.LogImportant("ForceSwitch playerId=" + playerId.ToString(), playerId);
    Rpc(RPC_ForceSwitch, playerId);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_ForceSwitch(int playerId)
  {
    PS_DebugLogger.LogImportant("RPC_ForceSwitch SERVER playerId=" + playerId.ToString(), playerId);
    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
    playableManager.ForceSwitch(playerId);
  }	// ---- JIP Sync: receive full state of non-RplProp maps from server ----
	// NOTE: Split into two RPCs because Enfusion's Rpc() rejects calls with too many
	// parameters. 6 was empirically the largest payload still accepted (9 caused a
	// "Too many parameters for 'Rpc' method" compile error at the original call site).
	// Both halves travel on the same Reliable channel from the same sender, so they
	// arrive in order and the client's state is assembled atomically from the caller's
	// point of view.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RPC_SyncFullState1(array<int> fKeys, array<string> fVals, array<int> sKeys, array<int> sVals, array<int> pKeys, array<bool> pVals)
	{
		PS_PlayableManager pm = PS_PlayableManager.GetInstance();
		if (!pm) return;
		PS_DebugLogger.LogImportant("RPC_SyncFullState1 factions=" + fKeys.Count().ToString() + " states=" + sKeys.Count().ToString() + " pins=" + pKeys.Count().ToString());

		// Faction sync with callback invocations
		for (int i = 0; i < fKeys.Count(); i++)
		{
			pm.GetPlayerFactionMap()[fKeys[i]] = fVals[i];
			pm.GetCallbackHandler().GetOnPlayerFactionChanged().Invoke(fKeys[i], fVals[i], "");
			pm.GetOnFactionChange().Invoke(fKeys[i], fVals[i], "");
		}

		// State sync with callback invocations
		for (int i = 0; i < sKeys.Count(); i++)
		{
			pm.GetPlayerStatesMap()[sKeys[i]] = sVals[i];
			pm.GetCallbackHandler().GetOnPlayerStateChanged().Invoke(sKeys[i], sVals[i]);
			pm.GetOnPlayerStateChange().Invoke(sKeys[i], sVals[i]);
		}

		// Pin sync with callback invocations
		for (int i = 0; i < pKeys.Count(); i++)
		{
			pm.GetPlayerPinMap()[pKeys[i]] = pVals[i];
			pm.GetOnPlayerPinChange().Invoke(pKeys[i], pVals[i]);
		}
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RPC_SyncFullState2(array<int> disconnected, array<int> nKeys, array<string> nVals)
	{
		PS_PlayableManager pm = PS_PlayableManager.GetInstance();
		if (!pm) return;
		int nameCount = 0;
		if (nKeys) nameCount = nKeys.Count();
		PS_DebugLogger.LogImportant("RPC_SyncFullState2 disconnected=" + disconnected.Count().ToString() + " names=" + nameCount.ToString());

		// Name sync with callback invocations (JIP-safe: server blasts all cached names)
		if (nKeys && nVals && nKeys.Count() == nVals.Count())
		{
			for (int i = 0; i < nKeys.Count(); i++)
			{
				pm.GetPlayerNamesCached()[nKeys[i]] = nVals[i];
				pm.GetCallbackHandler().GetOnPlayerNameUpdated().Invoke(nKeys[i], nVals[i]);
			}
		}

		pm.GetDisconnectedPlayersClient().Copy(disconnected);
		PS_DebugLogger.LogImportant("RPC_SyncFullState2 COMPLETE");
	}

	// Get controll on selected playable entity
	void ApplyPlayable()
  {
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
    if (!playableManager)
      return;
    int playerId = thisPlayerController.GetPlayerId();
    RplId slotId = playableManager.GetPlayableByPlayer(playerId);
    PS_EPlayableControllerState myState = playableManager.GetPlayerState(playerId);
    PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
    if (!gameMode)
    {
      PS_DebugLogger.LogError("ApplyPlayable: gameMode NULL — aborting");
      return;
    }
    SCR_EGameModeState gameModeState = gameMode.GetState();
    PS_DebugLogger.LogImportant("ApplyPlayable CLIENT playerId=" + playerId.ToString() + " slotId=" + slotId.ToString() + " myState=" + typename.EnumToString(PS_EPlayableControllerState, myState) + " gameModeState=" + typename.EnumToString(SCR_EGameModeState, gameModeState), playerId);
    if (slotId == RplId.Invalid())
    {
      PS_DebugLogger.LogImportant("ApplyPlayable CLIENT: slot INVALID, switching to observer", playerId);
      SwitchToObserver(null);
    }
    Rpc(RPC_ApplyPlayable);
  }

  // Called from PS_GameModeCoop broadcast handler — sends deploy RPC without stale map check
  void RequestDeployFromClient()
  {
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    int playerId = thisPlayerController.GetPlayerId();
    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
    RplId slotId = playableManager.GetPlayableByPlayer(playerId);
    PS_EPlayableControllerState myState = playableManager.GetPlayerState(playerId);
    PS_DebugLogger.LogImportant("RequestDeployFromClient CLIENT playerId=" + playerId.ToString() + " slotId=" + slotId.ToString() + " myState=" + typename.EnumToString(PS_EPlayableControllerState, myState), playerId);
    Rpc(RPC_ApplyPlayable);
  }	// Public wrapper for JIP sync — called from PS_PlayableManager
	// Fire-and-forget: the two RPCs are issued back-to-back on the same Reliable
	// channel, so the receiver (RPC_SyncFullState1 / RPC_SyncFullState2) processes
	// them in order. The public signature is unchanged so PS_PlayableManager does
	// not need to be touched.
	void SyncFullState(array<int> fKeys, array<string> fVals, array<int> sKeys, array<int> sVals, array<int> pKeys, array<bool> pVals, array<int> disconnected, array<int> nKeys = null, array<string> nVals = null)
	{
		Rpc(RPC_SyncFullState1, fKeys, fVals, sKeys, sVals, pKeys, pVals);
		Rpc(RPC_SyncFullState2, disconnected, nKeys, nVals);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_ApplyPlayable()
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		PlayerController playerController = PlayerController.Cast(GetOwner());
		int playerId = playerController.GetPlayerId();
		RplId slotId = playableManager.GetPlayableByPlayer(playerId);
		IEntity entity = IEntity.Cast(Replication.FindItem(slotId));
		string entState = "NULL";
		if (entity) entState = "ALIVE";
		PS_DebugLogger.LogImportant("RPC_ApplyPlayable RECEIVED slotId=" + slotId.ToString() + " entity=" + entState, playerId);
		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		playableManager.ApplyPlayable(playerId);
	}

  void UnpinPlayer(int playerId)
  {
    PS_DebugLogger.LogImportant("UnpinPlayer CLIENT player=" + playerId.ToString(), playerId);
    Rpc(RPC_UnpinPlayer, playerId);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  protected void RPC_UnpinPlayer(int playerId)
  {
    PS_DebugLogger.LogImportant("RPC_UnpinPlayer SERVER player=" + playerId.ToString(), playerId);
    PlayerManager playerManager = GetGame().GetPlayerManager();
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
    {
      PS_DebugLogger.Log("RPC_UnpinPlayer REJECTED not admin", playerId);
      return;
    }

    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
    playableManager.SetPlayerPin(playerId, false);
  }

  void PinPlayer(int playerId)
  {
    PS_DebugLogger.LogImportant("PinPlayer CLIENT player=" + playerId.ToString(), playerId);
    Rpc(RPC_PinPlayer, playerId);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  protected void RPC_PinPlayer(int playerId)
  {
    PS_DebugLogger.LogImportant("RPC_PinPlayer SERVER player=" + playerId.ToString(), playerId);
    PlayerManager playerManager = GetGame().GetPlayerManager();
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
    {
      PS_DebugLogger.Log("RPC_PinPlayer REJECTED not admin", playerId);
      return;
    }

    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
    playableManager.SetPlayerPin(playerId, true);
  }

  void KickPlayer(int playerId)
  {
    PS_DebugLogger.LogImportant("KickPlayer CLIENT player=" + playerId.ToString(), playerId);
    Rpc(RPC_KickPlayer, playerId);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  protected void RPC_KickPlayer(int playerId)
  {
    PS_DebugLogger.LogImportant("RPC_KickPlayer SERVER player=" + playerId.ToString(), playerId);
    PlayerManager playerManager = GetGame().GetPlayerManager();
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
    {
      PS_DebugLogger.Log("RPC_KickPlayer REJECTED not admin", playerId);
      return;
    }

    playerManager.KickPlayer(playerId, PlayerManagerKickReason.KICK, 0);
  }

	// -------------------- Set ---------------------
  void SetPlayerState(int playerId, PS_EPlayableControllerState state)
  {
    PS_DebugLogger.LogImportant("SetPlayerState CLIENT player=" + playerId.ToString() + " state=" + typename.EnumToString(PS_EPlayableControllerState, state), playerId);
    Rpc(RPC_SetPlayerState, playerId, state);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  protected void RPC_SetPlayerState(int playerId, PS_EPlayableControllerState state)
  {
    PS_DebugLogger.LogImportant("RPC_SetPlayerState SERVER player=" + playerId.ToString() + " state=" + typename.EnumToString(PS_EPlayableControllerState, state), playerId);
    PlayerManager playerManager = GetGame().GetPlayerManager();
    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();

    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    EPlayerRole playerRole = playerManager.GetPlayerRoles(thisPlayerController.GetPlayerId());
    if (thisPlayerController.GetPlayerId() != playerId && playerRole == EPlayerRole.NONE)
    {
      PS_DebugLogger.Log("RPC_SetPlayerState REJECTED not self and not admin", playerId);
      return;
    }

    playableManager.SetPlayerState(playerId, state);
  }

	bool CanPlayerSetToSlot(RplId slotId, int playerId)
	{
		bool isAdmin = PS_PlayersHelper.IsAdminOrServer();
		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (!gameMode)
		{
			PS_DebugLogger.LogError("CanPlayerSetToSlot: gameMode null — allowing slot assignment");
			return true;
		}
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		if (!playableManager)
		{
			PS_DebugLogger.LogError("CanPlayerSetToSlot: playableManager null — allowing slot assignment");
			return true;
		}
		SCR_EGameModeState state = gameMode.GetState();
		RplId prevSlotId;
		bool foundPrevSlot = playableManager.FindPlayerSlotById(playerId, prevSlotId) && prevSlotId != RplId.Invalid();

		if (state == SCR_EGameModeState.BRIEFING && foundPrevSlot && !isAdmin)
			return false;

		if (state == SCR_EGameModeState.GAME && foundPrevSlot && !playableManager.IsSlotCharacterDestroyed(prevSlotId) && !isAdmin)
			return false;

		return true;
	}

  void SetPlayerToSlot(RplId slotId, int playerId)
  {
    PS_DebugLogger.LogImportant("SetPlayerToSlot CLIENT player=" + playerId.ToString() + " slot=" + slotId.ToString(), playerId);
    if (!CanPlayerSetToSlot(slotId, playerId))
    {
      PS_DebugLogger.LogImportant("SetPlayerToSlot REJECTED by CanPlayerSetToSlot player=" + playerId.ToString(), playerId);
      return;
    }
    Rpc(RPC_SetPlayerToSlot, slotId, playerId);
  }

  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_SetPlayerToSlot(RplId slotId, int playerId)
  {
    PS_DebugLogger.LogImportant("RPC_SetPlayerToSlot SERVER player=" + playerId.ToString() + " slot=" + slotId.ToString(), playerId);
    PlayerManager playerManager = GetGame().GetPlayerManager();
    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    int thisPlayerId = thisPlayerController.GetPlayerId();
    if (IsRpcOnCooldown(thisPlayerId)) { PS_DebugLogger.Log("RPC_SetPlayerToSlot REJECTED cooldown player=" + thisPlayerId.ToString(), thisPlayerId); return; }
    EPlayerRole playerRole = playerManager.GetPlayerRoles(thisPlayerId);

    if (thisPlayerId != playerId && playerRole == EPlayerRole.NONE)
    {
      PS_DebugLogger.Log("RPC_SetPlayerToSlot REJECTED not self and not admin", playerId);
      return;
    }

    if (playableManager.GetPlayerPin(playerId) && playerRole == EPlayerRole.NONE)
    {
      PS_DebugLogger.Log("RPC_SetPlayerToSlot REJECTED player pinned", playerId);
      return;
    }

    if (slotId == RplId.Invalid())
    {
      if (playerId != thisPlayerId)
        playableManager.NotifyKick(playerId);
      playableManager.SetPlayerToSlot(slotId, playerId);
      return;
    }


    if (!playableManager.IsSlotAvailable(slotId) && !SCR_Global.IsAdmin(thisPlayerId))
    {
      PS_DebugLogger.Log("RPC_SetPlayerToSlot REJECTED slot not available slot=" + slotId.ToString(), playerId);
      return;
    }

    if (playableManager.IsSlotCharacterDestroyed(slotId))
    {
      PS_DebugLogger.LogImportant("RPC_SetPlayerToSlot REJECTED slot destroyed slot=" + slotId.ToString(), playerId);
      return;
    }

    if (playableManager.IsSlotLocked(slotId) && !SCR_Global.IsAdmin(thisPlayerId))
    {
      PS_DebugLogger.Log("RPC_SetPlayerToSlot REJECTED slot locked slot=" + slotId.ToString(), playerId);
      return;
    }

    int currentPlayerId = playableManager.GetPlayerByPlayableRemembered(slotId);
    if (currentPlayerId != -1 && currentPlayerId != playerId && !SCR_Global.IsAdmin(thisPlayerId))
    {
      PS_DebugLogger.Log("RPC_SetPlayerToSlot REJECTED slot occupied by player=" + currentPlayerId.ToString(), playerId);
      return;
    }

    // Check faction balance (admins bypass)
    if (!SCR_Global.IsAdmin(thisPlayerId))
    {
      PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
      PS_SlotCharacterData slotData;
      if (gameModeCoop && playableManager.FindSlotData(slotId, slotData))
      {
        if (!gameModeCoop.CanJoinFaction(slotData.m_FactionKey, playableManager.GetPlayerFactionKey(playerId)))
        {
          PS_DebugLogger.Log("RPC_SetPlayerToSlot REJECTED faction balance", playerId);
          return;
        }
      }
    }

    playableManager.SetPlayerToSlot(slotId, playerId);

    if (playerId != thisPlayerId)
      playableManager.SetPlayerPin(playerId, true);
  }

  void KickPlayerFromSlot(RplId slotId)
  {
    int localPlayerId = SCR_PlayerController.Cast(GetOwner()).GetPlayerId();
    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
    PS_DebugLogger.LogImportant("KickPlayerFromSlot CLIENT slot=" + slotId.ToString() + " kicker=" + localPlayerId.ToString(), localPlayerId);

    if (playableManager.IsSlotAvailable(slotId))
      return;

    if (!PS_PlayersHelper.IsAdminOrServer() && !playableManager.IsPlayerGroupLeader(localPlayerId))
      return;

    Rpc(RPC_KickPlayerFromSlot, slotId, localPlayerId);
  }

  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_KickPlayerFromSlot(RplId slotId, int kickingPlayerId)
  {
    PS_DebugLogger.LogImportant("RPC_KickPlayerFromSlot SERVER slot=" + slotId.ToString() + " kicker=" + kickingPlayerId.ToString(), kickingPlayerId);
    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    int senderId = thisPlayerController.GetPlayerId();
    if (!SCR_Global.IsAdmin(senderId) && !playableManager.IsPlayerGroupLeader(senderId))
    {
      PS_DebugLogger.Log("RPC_KickPlayerFromSlot REJECTED sender not admin/leader", senderId);
      return;
    }
    playableManager.KickPlayerFromSlot(slotId, senderId);
  }

  void SetSlotLockState(RplId slotId, bool isLocked)
  {
    PS_DebugLogger.Log("SetSlotLockState CLIENT slot=" + slotId.ToString() + " isLocked=" + isLocked.ToString());
    if (!PS_PlayersHelper.IsAdminOrServer())
      return;
    Rpc(RPC_SetSlotLockState, slotId, isLocked);
  }

  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void RPC_SetSlotLockState(RplId slotId, bool isLocked)
  {
    PS_DebugLogger.LogImportant("RPC_SetSlotLockState SERVER slot=" + slotId.ToString() + " isLocked=" + isLocked.ToString());
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
    {
      PS_DebugLogger.Log("RPC_SetSlotLockState REJECTED not admin");
      return;
    }

    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
    if (!playableManager.IsSlotAvailable(slotId))
    {
      PS_DebugLogger.Log("RPC_SetSlotLockState REJECTED slot not available");
      return;
    }
    playableManager.SetSlotLockState(slotId, isLocked);
  }

  void SetPlayableVehicleLocked(RplId vehicleId, bool lock)
  {
    PS_DebugLogger.Log("SetPlayableVehicleLocked CLIENT vehicle=" + vehicleId.ToString() + " lock=" + lock.ToString());
    Rpc(RPC_SetPlayableVehicleLocked, vehicleId, lock);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  protected void RPC_SetPlayableVehicleLocked(RplId vehicleId, bool lock)
  {
    PS_DebugLogger.LogImportant("RPC_SetPlayableVehicleLocked SERVER vehicle=" + vehicleId.ToString() + " lock=" + lock.ToString());
    PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();

    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
    {
      PS_DebugLogger.Log("RPC_SetPlayableVehicleLocked REJECTED not admin");
      return;
    }

    playableManager.SetPlayableVehicleLocked(vehicleId, lock);
  }	void SetObjectiveCompleteState(PS_Objective objective, bool complete)
	{
		RplId objectiveId = objective.GetRplId();
		PS_DebugLogger.Log("SetObjectiveCompleteState CLIENT objective=" + objectiveId.ToString() + " complete=" + complete.ToString());
		Rpc(RPC_SetObjectiveCompleteState, objectiveId, complete);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_SetObjectiveCompleteState(RplId objectiveId, bool complete)
	{
		PS_DebugLogger.LogImportant("RPC_SetObjectiveCompleteState SERVER objective=" + objectiveId.ToString());
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
		{
			PS_DebugLogger.Log("RPC_SetObjectiveCompleteState REJECTED not admin");
			return;
		}

		PS_Objective objective = PS_Objective.Cast(Replication.FindItem(objectiveId));
		if (objective)
			objective.SetCompleted(complete);
	}

	// ---- Alive Players: entity position request for spectator camera teleport ----
	// Called from PS_SpectatorMenu when the user clicks an alive player whose entity
	// isn't replicated yet (camera too far away). The server finds the entity position
	// and replies via RplRcver.Owner so only the requesting client gets the reply.
	void RequestEntityPosition(RplId rplId)
	{
		PS_DebugLogger.LogImportant("RequestEntityPosition CLIENT rplId=" + rplId.ToString());
		Rpc(RPC_RequestEntityPosition, rplId);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_RequestEntityPosition(RplId rplId)
	{
		PS_DebugLogger.LogImportant("RPC_RequestEntityPosition SERVER rplId=" + rplId.ToString());

		// Rate-limit: prevent spam-click floods (matches other Server RPCs in this class)
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (pc && IsRpcOnCooldown(pc.GetPlayerId(), 500))
			return;

		PS_PlayableManager pm = PS_PlayableManager.GetInstance();
		if (!pm)
		{
			PS_DebugLogger.LogError("RPC_RequestEntityPosition SERVER: PS_PlayableManager NULL");
			return;
		}

		IEntity ent;
		vector pos = "0 0 0";
		bool found = pm.FindValidatedSlotEntity(rplId, ent);
		if (found)
			pos = ent.GetOrigin();

		PS_DebugLogger.LogImportant("RPC_RequestEntityPosition SERVER reply rplId=" + rplId.ToString() + " found=" + found.ToString() + " pos=" + pos.ToString());
		Rpc(RPC_ReceiveEntityPosition, rplId, pos, found);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RPC_ReceiveEntityPosition(RplId rplId, vector pos, bool found)
	{
		PS_DebugLogger.LogImportant("RPC_ReceiveEntityPosition OWNER rplId=" + rplId.ToString() + " found=" + found.ToString() + " pos=" + pos.ToString());

		// Forward to the spectator menu via its static instance
		PS_SpectatorMenu spectatorMenu = PS_SpectatorMenu.s_SpectatorMenu;
		if (spectatorMenu)
			spectatorMenu.OnEntityPositionReceived(rplId, pos, found);
	}
}
