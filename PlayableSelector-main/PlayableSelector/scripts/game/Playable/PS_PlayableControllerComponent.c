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
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.PlayableRespawnMenu);
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
		VoNChannelsManager.MoveToRoom(playerId, "", "");
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

    GetGame().GetCallqueue().CallLater(UpdatePosition, 0, true, false);
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
  private void OnControlledEntityChanged(IEntity from, IEntity to)
  {
    PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
    int playerId = thisPlayerController.GetPlayerId();
    string fromName = "NULL";
    if (from) fromName = from.GetPrefabData().GetPrefabName();
    string toName = "NULL";
    if (to) toName = to.GetPrefabData().GetPrefabName();
    PS_DebugLogger.LogImportant("OnControlledEntityChanged playerId=" + playerId.ToString() + " from=" + fromName + " to=" + toName, playerId);

    if (Replication.IsServer()) {
			RplId toRplId = RplId.Invalid();
			if (to) {
				RplComponent rplTo = RplComponent.Cast(to.FindComponent(RplComponent));
				toRplId = rplTo.Id();
			}
		}

		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		if (!rpl.IsOwner())
			return;
		if (!from && !m_bAfterInitialSwitch)
		{
			PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
			if (gameModeCoop.GetState() == SCR_EGameModeState.GAME)
			{
				PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
				RplId playerSlot = playableManager.GetPlayableByPlayer(playerId);
				if (playerSlot == RplId.Invalid())
				{
					PS_DebugLogger.LogImportant("OnControlledEntityChanged JIP no slot — switching to observer", playerId);
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
    PS_DebugLogger.LogImportant("OnControlledEntityChanged vonFrom=" + vonFromStr + " vonTo=" + vonToStr, playerId);

    if (!vonTo)
    {
      PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
      PS_DebugLogger.LogImportant("OnControlledEntityChanged entity has NO VoN — game state=" + typename.EnumToString(SCR_EGameModeState, gameModeCoop.GetState()), playerId);
      if (gameModeCoop.GetState() == SCR_EGameModeState.GAME)
      {
        GetGame().GetCallqueue().Call(ForceNotifyEditorPlayerSpawned, thisPlayerController.GetPlayerId(), to);
        if (!from)
        {
          PS_DebugLogger.LogImportant("OnControlledEntityChanged JIP with no VoN entity — switching to observer", playerId);
          SwitchToObserver(null);
        }
        else
        {
          PS_DebugLogger.LogImportant("OnControlledEntityChanged switching from observer (!vonTo)", playerId);
          SwitchFromObserver();
        }
      }
    }
  }
	
	// Notify editor core that player is alive (bypasses missing spawns in some game modes)
	void ForceNotifyEditorPlayerSpawned(int playerId, IEntity entity)
	{
		PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (gameModeCoop.IsFreezeTimeEnd() && gameModeCoop.GetDisableBuildingModeAfterFreezeTime())
			 return;
		SCR_BaseGameMode.Cast(GetGame().GetGameMode()).GetOnPlayerSpawned().Invoke(playerId, entity);
		Rpc(ForceNotifyServerPlayerSpawned, playerId, Replication.FindItemId(entity));
	}
  [RplRpc(RplChannel.Reliable, RplRcver.Server)]
  void ForceNotifyServerPlayerSpawned(int playerId, RplId entityId)
  {
    PS_DebugLogger.LogImportant("ForceNotifyServerPlayerSpawned SERVER player=" + playerId.ToString() + " entityId=" + entityId.ToString(), playerId);
    IEntity entity = IEntity.Cast(Replication.FindItem(entityId));
    SCR_BaseGameMode.Cast(GetGame().GetGameMode()).GetOnPlayerSpawned().Invoke(playerId, entity);
  }
	
	override protected void EOnFrame(IEntity owner, float timeSlice)
	{
		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
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

	override void EOnFixedFrame(IEntity owner, float timeSlice)
	{
		UpdatePosition(false);
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
		
		// Lets fight with phisyc engine
		if (m_InitialEntity)
		{
			PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
			int playerId = thisPlayerController.GetPlayerId();
			m_vVoNPosition = Vector(0, 100000, 0) + Vector(1000 * Math.Mod(playerId, 10), 5000 * Math.Floor(Math.Mod(playerId, 100) / 10), 5000 * Math.Floor(playerId / 100));
			vector currentOrigin = m_InitialEntity.GetOrigin();
			//if (currentOrigin == m_vVoNPosition) return;
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
					CameraBase cam = m_CachedCameraManager.CurrentCamera();
					if (cam)
						cam.SetWorldTransform(mat);
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
			if (physics)
			{
				//physics.SetVelocity("0 0 0");
				//physics.SetAngularVelocity("0 0 0");
				//physics.SetMass(0);
				//physics.SetDamping(1, 1);
				//physics.ChangeSimulationState(SimulationState.NONE);
				physics.SetActive(ActiveState.INACTIVE);
			}
		} else {
			PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
			IEntity entity = thisPlayerController.GetControlledEntity();
			if (entity)
			{
				PS_LobbyVoNComponent von = PS_LobbyVoNComponent.Cast(entity.FindComponent(PS_LobbyVoNComponent));
				if (von)
					m_InitialEntity = entity;
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
		if (!GetVoN())
			return;
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		IEntity entity = thisPlayerController.GetControlledEntity();
		if (!entity)
			return;
		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.Cast(entity.FindComponent(SCR_GadgetManagerComponent));
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
	bool isVonInit()
	{
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		IEntity entity = thisPlayerController.GetControlledEntity();
		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.Cast(entity.FindComponent(SCR_GadgetManagerComponent));
		IEntity radioEntity = gadgetManager.GetGadgetByType(EGadgetType.RADIO);
		return radioEntity;
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

    string playerUUID = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
    Rpc(RPC_GetArmaIdFromServer_Owner, playerUUID);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
  void RPC_GetArmaIdFromServer_Owner(string playerUUID)
  {
    PS_DebugLogger.LogImportant("RPC_GetArmaIdFromServer_Owner uuid=" + playerUUID);
    System.ExportToClipboard(playerUUID);
  }

	// ------------------ Observer camera controlls ------------------
  void SwitchToObserverServer()
  {
    PS_DebugLogger.LogImportant("SwitchToObserverServer");
    Rpc(RPC_SwitchToObserverServer);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
  void RPC_SwitchToObserverServer()
  {
    PS_DebugLogger.LogImportant("RPC_SwitchToObserverServer OWNER");
    SwitchToObserver(null);
  }

	void SaveCameraTransform()
	{
		SCR_CameraEditorComponent cameraManager = SCR_CameraEditorComponent.Cast(SCR_BaseEditorComponent.GetInstance(SCR_CameraEditorComponent, false));
		cameraManager.GetLastCameraTransform(lastCameraTransform);
	}

	void SwitchToObserver(IEntity from)
	{
		SCR_EditorManagerEntity editorManagerEntity = SCR_EditorManagerEntity.GetInstance();
		if (editorManagerEntity && editorManagerEntity.IsOpened())
			return;
		
		if (m_Camera)
			return;
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.SpectatorMenu);
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		IEntity entity = thisPlayerController.GetControlledEntity();
		EntitySpawnParams params = new EntitySpawnParams();
		if (from)
			from.GetTransform(params.Transform);
		MoveToVoNRoom(thisPlayerController.GetPlayerId(), "", "");
		Resource resource = Resource.Load("{6EAA30EF620F4A2E}Prefabs/Editor/Camera/ManualCameraSpectator.et");
		m_Camera = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);

		if (lastCameraTransform[3][1] < 10000 && lastCameraTransform[3][1] > 0)
		{
			m_Camera.SetTransform(lastCameraTransform);
			lastCameraTransform[3][1] = 10000;
		} else if (m_vObserverPosition != "0 0 0") {
			m_Camera.SetOrigin(m_vObserverPosition);
			m_vObserverPosition = "0 0 0";
		} else {
			SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
			m_Camera.SetOrigin(mapEntity.Size() / 2.0 + vector.Up * 100);
		}
		GetGame().GetCameraManager().SetCamera(CameraBase.Cast(m_Camera));

		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (gameMode.GetFriendliesSpectatorOnly())
			PS_ManualCameraSpectator.Cast(m_Camera).SetCharacterEntityMove(from);
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
    SCR_EGameModeState gameModeState = PS_GameModeCoop.Cast(GetGame().GetGameMode()).GetState();
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
  }

  void SetObjectiveCompleteState(PS_Objective objective, bool complete)
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
}
