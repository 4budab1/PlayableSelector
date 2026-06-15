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
	protected vector m_vVoNPosition = PS_VoNRoomsManager.roomInitialPosition;
	protected SCR_EGameModeState m_eMenuState = SCR_EGameModeState.PREGAME;
	protected bool m_bAfterInitialSwitch = false;
	protected vector m_vObserverPosition = "0 0 0";
	protected vector lastCameraTransform[4];

	[RplProp()]
	bool m_bOutFreezeTime;
	
	void SetOutFreezeTime(bool outFreezeTime)
	{
		Rpc(RPC_SetOutFreezeTime, outFreezeTime);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RPC_SetOutFreezeTime(bool outFreezeTime)
	{
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
		Rpc(RPC_SetFactionReady, factionKey, readyValue);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_SetFactionReady(FactionKey factionKey, int readyValue)
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
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
		Rpc(RPC_SwitchToMenuServer, state);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RPC_SwitchToMenuServer(SCR_EGameModeState state)
	{
		SwitchToMenu(state);
	}

	void SwitchToMenu(SCR_EGameModeState state)
	{
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

		// Body-less: tear down the spectator camera/menu on any state change (the GAME case re-opens it
		// below via ApplyPlayable when the player has no slot). No-op when not spectating.
		SwitchFromObserver();

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
				GetGame().GetCallqueue().Call(ApplyPlayable);
				GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.FadeToGame);
				break;
			case SCR_EGameModeState.DEBRIEFING:
				GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.DebriefingMenu);
				break;
			case SCR_EGameModeState.POSTGAME:
				GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.DebriefingMenu);
				break;
		}

		// Body-less voice: re-evaluate the menu talking device on every menu/state change.
		PS_MenuVoN.Refresh();
	}

	void AdvanceGameState(SCR_EGameModeState state)
	{
		Rpc(RPC_AdvanceGameState, state);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_AdvanceGameState(SCR_EGameModeState state)
	{
		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		gameMode.AdvanceGameState(state);
	}

	void LoadMission(string missionName)
	{
		Rpc(RPC_LoadMission, missionName);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_LoadMission(string missionName)
	{
		// SCR_SaveManagerCore saveManager = GetGame().GetSaveManager();
		// It's litteraly broken on dedicated.
		// saveManager.RestartAndLoad(missionName);
	}

	// ------ FactionLock ------
	void FactionLockSwitch()
	{
		Rpc(RPC_FactionLockSwitch);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_FactionLockSwitch()
	{
		// only admins can change faction lock
		PlayerManager playerManager = GetGame().GetPlayerManager();
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		EPlayerRole playerRole = playerManager.GetPlayerRoles(thisPlayerController.GetPlayerId());
		if (playerRole == EPlayerRole.NONE)
			return;

		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		gameMode.FactionLockSwitch();
	}

	// ------ FreezeTimer ------
	void FreezeTimerAdvance(int time)
	{
		Rpc(RPC_FreezeTimerAdvance, time);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_FreezeTimerAdvance(int time)
	{
		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.FreezeTimerAdvance(time);
	}
	void FreezeTimerEnd()
	{
		Rpc(RPC_FreezeTimerEnd);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_FreezeTimerEnd()
	{
		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.FreezeTimerEnd();
	}
	
	// ------ SpawnPrefab ------
	void SpawnPrefab(string GUID, vector position)
	{
		IEntity camera = GetGame().GetCameraManager().CurrentCamera();
		if (position == "0 0 0")
			position = camera.GetOrigin();
		Rpc(RPC_SpawnPrefab, position, GUID);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_SpawnPrefab(vector position, string GUID)
	{
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
			return;

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
		Rpc(RPC_SpawnAdministrator, position);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_SpawnAdministrator(vector position)
	{
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
			return;

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
		if (Replication.IsServer())
			RPC_RespawnPlayable(playableId, useInitPosition);
		else
			Rpc(RPC_RespawnPlayable, playableId, useInitPosition);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_RespawnPlayable(RplId playableId, bool useInitPosition)
	{
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
			return;
		
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
		SCR_AIGroup playabelGroup = aiGroup.m_BotsGroup;
		playabelGroup.AddAIEntityToGroup(newCharacter);
		playableManager.SetPlayablePlayerGroupId(playableContainer.GetRplId(), aiGroup.GetGroupID());

		playableContainer.SetPlayable(true);
		oldPlayableComponent.SetPlayable(false);

		character.GetDamageManager().Kill(Instigator.CreateInstigator(newCharacter));
		character.GetDamageManager().SetHealthScaled(0);
		GetGame().GetCallqueue().CallLater(RPC_ForceRespawnPlayerLate, 300, false, character, oldPlayableComponent, newCharacter, playableContainer);
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

		Rpc(RPC_ForceRespawnPlayer, Replication.FindId(character), initPosition);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_ForceRespawnPlayer(RplId respawnEntityRplId, bool initPosition)
	{
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
			return;

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
		SCR_AIGroup playabelGroup = aiGroup.m_BotsGroup;
		playabelGroup.AddAIEntityToGroup(newCharacter);
		playableManager.SetPlayablePlayerGroupId(playableContainer.GetRplId(), aiGroup.GetGroupID());

		playableContainer.SetPlayable(true);
		oldPlayableComponent.SetPlayable(false);

		character.GetDamageManager().Kill(Instigator.CreateInstigator(newCharacter));
		character.GetDamageManager().SetHealthScaled(0);
		GetGame().GetCallqueue().CallLater(RPC_ForceRespawnPlayerLate, 300, false, character, oldPlayableComponent, newCharacter, playableContainer);
	}

	void RPC_ForceRespawnPlayerLate(SCR_ChimeraCharacter character, PS_PlayableComponent oldPlayableComponent, SCR_ChimeraCharacter newCharacter, PS_PlayableComponent playableContainer)
	{
		character.GetDamageManager().Kill(Instigator.CreateInstigator(newCharacter));
		character.GetDamageManager().SetHealthScaled(0);
		if (!character.GetDamageManager().IsDestroyed())
		{
			GetGame().GetCallqueue().CallLater(RPC_ForceRespawnPlayerLate, 300, false, character, oldPlayableComponent, newCharacter, playableContainer);
			return;
		}

		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		PS_VoNRoomsManager VoNRoomsManager = PS_VoNRoomsManager.GetInstance();
		SCR_AIGroup aiGroup = playableManager.GetPlayerGroupByPlayable(oldPlayableComponent.GetRplId());
		SCR_AIGroup playabelGroup = aiGroup.GetSlave();
		playabelGroup.AddAIEntityToGroup(character);
		playableManager.SetPlayablePlayerGroupId(playableContainer.GetRplId(), aiGroup.GetGroupID());
		int playerId = playableManager.GetPlayerByPlayableRemembered(oldPlayableComponent.GetRplId());
		VoNRoomsManager.MoveToRoom(playerId, "", "");
		if (playerId > -1)
		{
			GetGame().GetCallqueue().CallLater(RPC_ForceRespawnPlayerLate2, 500, false, playerId, playableContainer);
		}
	}
	void RPC_ForceRespawnPlayerLate2(int playerId, PS_PlayableComponent playable)
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		playableManager.SetPlayerPlayable(playerId, playable.GetRplId());
		ForceSwitch(playerId);
	}

	// Just don't look at it.
	override protected void OnPostInit(IEntity owner)
	{  
		/*
		EntitySpawnParams params = new EntitySpawnParams(); 
		Resource resource = Resource.Load("{6EAA30EF620F4A2E}Prefabs/Editor/Camera/ManualCameraSpectator.et");
		m_Camera = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		*/
		
		// (Removed the per-frame UpdatePosition CallLater - it parked the old controlled BODY every frame;
		// body-less there is no body, so it just burned a FindComponent per frame on the owner client doing
		// nothing. SetEventMask FRAME stays for EOnFrame, the freeze-time fire blocker.)
		SetEventMask(GetOwner(), EntityEvent.FRAME);
		SCR_PlayerController playerController = SCR_PlayerController.Cast(PlayerController.Cast(GetOwner()));
		playerController.m_OnControlledEntityChanged.Insert(OnControlledEntityChanged);

		PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (!gameModeCoop)
			return;

		ScriptInvokerBase<SCR_BaseGameMode_OnPlayerRoleChanged> onPlayerRoleChanged = gameModeCoop.GetOnPlayerRoleChange();
		if (!onPlayerRoleChanged)
			return;

		onPlayerRoleChanged.Insert(OnPlayerRoleChange);
	}

	void OnPlayerRoleChange(int playerId, EPlayerRole roleFlags)
	{
		m_eOnPlayerRoleChange.Invoke(playerId, roleFlags);
	}

	// We change to VoN boi lets enable camera
	private void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		
		// Write entity change to replay
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

		// Body-less voice: control changes flip menu-speaker state (took a playable / died) -
		// re-evaluate the local menu talking device.
		PS_MenuVoN.Refresh();

		// Remember where control was lost - the spectator camera starts there.
		if (!to && from)
			m_vObserverPosition = from.GetOrigin();

		// Body-less: the old design keyed the observer transitions off a PS_LobbyVoNComponent on the
		// controlled body. There is no body now (vonTo would ALWAYS be null -> SwitchFromObserver fired on
		// every change, tearing the spectator down). Decide off the new entity's life state instead:
		// returning to a LIVING character means the player is back in the game (leave spectator + tell the
		// editor core we are alive so it releases the camera). Control going to NULL or to a DEAD corpse
		// must NOT tear the spectator down - that path is owned by SendPlayerToSpectator_S /
		// RPC_EnterSpectator, which also DELETES the corpse so control drops to null and frees VONDirect.
		bool toIsLivingCharacter = false;
		ChimeraCharacter toCharacter = ChimeraCharacter.Cast(to);
		if (toCharacter)
		{
			SCR_DamageManagerComponent toDmg = SCR_DamageManagerComponent.Cast(toCharacter.FindComponent(SCR_DamageManagerComponent));
			toIsLivingCharacter = !toDmg || toDmg.GetState() != EDamageState.DESTROYED;
		}

		if (toIsLivingCharacter)
		{
			SwitchFromObserver();
			PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
			if (gameModeCoop.GetState() == SCR_EGameModeState.GAME)
				GetGame().GetCallqueue().Call(TellFuckingEditorCoreThanWeAlive, thisPlayerController.GetPlayerId(), to);
		}
	}
	
	// There is sure no ебанорго game modes without spawns, yeah sure блять
	void TellFuckingEditorCoreThanWeAlive(int playerId, IEntity entity)
	{
		PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (gameModeCoop.IsFreezeTimeEnd() && gameModeCoop.GetDisableBuildingModeAfterFreezeTime())
			 return;
		SCR_BaseGameMode.Cast(GetGame().GetGameMode()).GetOnPlayerSpawned().Invoke(playerId, entity);
		Rpc(AndFuckingServerTo, playerId, Replication.FindId(entity))
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void AndFuckingServerTo(int playerId, RplId entityId)
	{
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

	// EOnFixedFrame removed - FIXEDFRAME was never masked (the POSTFIXEDFRAME SetEventMask is commented out),
	// so it never fired, and it only called the now-unused body-parking UpdatePosition.
	void UpdatePosition(bool force)
	{
		// Repeating call may still fire while the player controller is being torn down on disconnect
		IEntity owner = GetOwner();
		if (!owner)
			return;
		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (!rpl || !rpl.IsOwner())
			return;
		PlayerController ownerPlayerController = PlayerController.Cast(owner);
		if (!ownerPlayerController)
			return;

		// Lets fight with phisyc engine
		if (m_InitialEntity)
		{
			// While spectating, the server parks the body above the player's own corpse so the
			// battlefield stays streamed around it (NDS streams around the controlled entity).
			// The no-physics component keeps it there - don't drag it to the lobby grid.
			if (m_Camera)
			{
				// Who broke camera on map?
				CameraBase specCameraBase = GetGame().GetCameraManager().CurrentCamera();
				if (specCameraBase)
					specCameraBase.ApplyTransform(GetGame().GetWorld().GetTimeSlice());
			}
			else
			{
				// In menus (preview/lobby/briefing) pin the body to a deterministic per-player spot.
				// VoN "rooms" rely on each player's body being spatially separated so proximity voice
				// never bleeds between players; forcing the position guarantees uniqueness regardless
				// of replication timing.
				int playerId = ownerPlayerController.GetPlayerId();
				m_vVoNPosition = PS_GameModeCoop.GetInitialEntityPosition(playerId);
				vector currentOrigin = m_InitialEntity.GetOrigin();

				vector mat[4];
				Math3D.MatrixIdentity4(mat);
				mat[3] = m_vVoNPosition;

				// Touch the transform only when it actually drifted: this entity is owned by the local
				// client, every SetTransform dirties its replication state, so per-frame would flood.
				if (force || vector.DistanceSq(currentOrigin, m_vVoNPosition) > 0.01)
				{
					GameEntity gameEntity = GameEntity.Cast(m_InitialEntity);
					if (force)
						gameEntity.Teleport(mat);
					gameEntity.SetTransform(mat);

					Physics physics = m_InitialEntity.GetPhysics();
					if (physics)
						physics.SetActive(ActiveState.INACTIVE);
				}

				MenuBase menu = GetGame().GetMenuManager().GetTopMenu();
				if (menu && (menu.IsInherited(PS_PreviewMapMenu) || menu.IsInherited(PS_CoopLobby) || menu.IsInherited(PS_BriefingMapMenu)))
				{
					GetGame().GetCameraManager().CurrentCamera().SetWorldTransform(mat);
				}

				// Who broke camera on map?
				CameraBase cameraBase = GetGame().GetCameraManager().CurrentCamera();
				if (cameraBase)
					cameraBase.ApplyTransform(GetGame().GetWorld().GetTimeSlice());
			}
		} else {
			IEntity entity = ownerPlayerController.GetControlledEntity();
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
		Rpc(RPC_ChangeFactionKey, playerId, factionKey)
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_ChangeFactionKey(int playerId, FactionKey factionKey)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();

		// If not admin you can change only herself
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		EPlayerRole playerRole = playerManager.GetPlayerRoles(thisPlayerController.GetPlayerId());

		// Check faction balance
		PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()) && !gameModeCoop.CanJoinFaction(factionKey, playableManager.GetPlayerFactionKey(playerId)))
			return;

		if (thisPlayerController.GetPlayerId() != playerId && playerRole == EPlayerRole.NONE)
			return;
		if (playableManager.GetPlayerPin(playerId) && playerRole == EPlayerRole.NONE)
			return;

		playableManager.SetPlayerFactionKey(playerId, factionKey);
	}

	// ------------------ VoN controlls ------------------
	void MoveToVoNRoomByKey(int playerId, string roomKey)
	{
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
		Rpc(RPC_MoveVoNToRoom, playerId, factionKey, roomName);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_MoveVoNToRoom(int playerId, FactionKey factionKey, string roomName)
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();

		PS_VoNRoomsManager VoNRoomsManager = PS_VoNRoomsManager.GetInstance();
		VoNRoomsManager.MoveToRoom(playerId, factionKey, roomName);
	}

	// Body-less: dead path (lobby voice is on the VoN proxy now, see PS_MenuVoN). Kept for the
	// in-game character VoN callers; guarded so a body-less (null) controlled entity never derefs.
	PS_LobbyVoNComponent GetVoN()
	{
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		if (!thisPlayerController)
			return null;
		IEntity entity = thisPlayerController.GetControlledEntity();
		if (!entity)
			return null;
		return PS_LobbyVoNComponent.Cast(entity.FindComponent(PS_LobbyVoNComponent));
	}
	// Collect the lobby VoN radios from the controlled entity, in order.
	// Works whether the radios are carried as inventory gadgets (full character carrier)
	// or attached as direct child entities (stripped carrier without the inventory system).
	// The carrier prefab must keep the two radios in a stable order (radio 0 first, radio 1 second).
	protected void GetVoNRadios(out array<BaseRadioComponent> radios)
	{
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		if (!thisPlayerController)
			return;
		IEntity entity = thisPlayerController.GetControlledEntity();
		if (!entity)
			return;

		// Inventory gadget path (radios carried as items)
		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.Cast(entity.FindComponent(SCR_GadgetManagerComponent));
		if (gadgetManager)
		{
			array<SCR_GadgetComponent> gadgets = gadgetManager.GetGadgetsByType(EGadgetType.RADIO);
			foreach (SCR_GadgetComponent gadget : gadgets)
			{
				BaseRadioComponent radio = BaseRadioComponent.Cast(gadget.GetOwner().FindComponent(BaseRadioComponent));
				if (radio)
					radios.Insert(radio);
			}
			if (radios.Count() >= 2)
				return;
			radios.Clear();
		}

		// Direct child entity path (radios attached without an inventory system)
		IEntity child = entity.GetChildren();
		while (child)
		{
			BaseRadioComponent radio = BaseRadioComponent.Cast(child.FindComponent(BaseRadioComponent));
			if (radio)
				radios.Insert(radio);
			child = child.GetSibling();
		}
	}

	RadioTransceiver GetVoNTransiver(int radioId)
	{
		array<BaseRadioComponent> radios = {};
		GetVoNRadios(radios);
		if (radioId < 0 || radioId >= radios.Count())
			return null;
		BaseRadioComponent radio = radios[radioId];
		radio.SetPower(true);
		RadioTransceiver transiver = RadioTransceiver.Cast(radio.GetTransceiver(0));
		transiver.SetFrequency(radioId + 1);
		return transiver;
	}
	// Body-less: lobby push-to-talk is now owned by PS_MenuVoN (it binds VONDirect while the local
	// player is a menu speaker). These three remain only because some menus still bind them as input
	// actions; they are deliberate no-ops now (there is no controlled body VoN to drive).
	void LobbyVoNEnable()
	{
	}
	void LobbyVoNRadioEnable()
	{
	}
	void LobbyVoNDisable()
	{
	}
	void LobbyVoNDisableDelayed()
	{
		PS_LobbyVoNComponent von = GetVoN();
		if (!von)
			return;
		von.SetCommMethod(ECommMethod.DIRECT);
		von.SetCapture(false);
	}
	// Separate radio VoNs, CALL IT FROM SERVER
	void SetVoNKey(string VoNKey, string VoNKeyLocal)
	{
		if (!GetVoN())
			return;
		array<BaseRadioComponent> radios = {};
		GetVoNRadios(radios);
		if (radios.Count() >= 2)
		{
			radios[0].SetEncryptionKey(VoNKey);
			radios[1].SetEncryptionKey(VoNKeyLocal);
		}
	}
	bool isVonInit()
	{
		array<BaseRadioComponent> radios = {};
		GetVoNRadios(radios);
		return radios.Count() >= 2;
	}
	
	void GetArmaIdFromServer(int playerId)
	{
		Rpc(RPC_GetArmaIdFromServer_Server, playerId);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_GetArmaIdFromServer_Server(int playerId)
	{
		string playerUUID = GetGame().GetBackendApi().GetPlayerUID(playerId);
		Rpc(RPC_GetArmaIdFromServer_Owner, playerUUID);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RPC_GetArmaIdFromServer_Owner(string playerUUID)
	{
		System.ExportToClipboard(playerUUID);
	}

	// ------------------ Observer camera controlls ------------------
	void SaveCameraTransform()
	{
		SCR_CameraEditorComponent cameraManager = SCR_CameraEditorComponent.Cast(SCR_BaseEditorComponent.GetInstance(SCR_CameraEditorComponent, false));
		cameraManager.GetLastCameraTransform(lastCameraTransform);
	}

	// ------------------ Spectator streaming observer: REMOVED ------------------
	// An MPObserver (RplComponent.InsertMPObserver) used to follow the spectator camera and stream the
	// battlefield around the view. It was the ONLY spectator-streaming mechanism among the reference
	// lobbies (Echo and LiteLobby use none) and the main remaining "Replication Flooded/Stalled" lever, so
	// it is removed entirely. Spectators now see only what default NDS streams around their parked corpse,
	// exactly like Echo/LiteLobby. The GetSpectatorStreamingObserver() gamemode flag is now inert.

	// ------------------ Spectate a player outside this client's replication pool ------------------
	// With default NDS culling (force-streaming disabled) a distant playable is not replicated here, so
	// PS_SpectatorMenu.SetCameraCharacter cannot resolve a local entity to follow. Instead ask the server
	// for that playable's world position, fly the free spectator camera there, and (when the streaming
	// observer is enabled) push the observer to that spot so the area - and the player - streams in. Once
	// the player is streamed, clicking them again takes the normal first-person follow path.
	protected static const float SPECTATE_JUMP_EYE_OFFSET_M = 2.0;

	void RequestSpectatePosition(RplId playableId)
	{
		if (playableId == RplId.Invalid() || !m_Camera)
			return;
		Rpc(RPC_RequestSpectatePosition, playableId);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestSpectatePosition(RplId playableId)
	{
		// Server has every entity - resolve the playable and read its current position.
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(playableId));
		if (!rpl)
			return;
		IEntity entity = rpl.GetEntity();
		if (!entity)
			return;
		vector pos = entity.GetOrigin();
		if (GetGame().GetPlayerController() == GetOwner())
			RPC_ReceiveSpectatePosition(playableId, pos);
		else
			Rpc(RPC_ReceiveSpectatePosition, playableId, pos);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_ReceiveSpectatePosition(RplId playableId, vector pos)
	{
		PS_ManualCameraSpectator camera = PS_ManualCameraSpectator.Cast(m_Camera);
		if (!camera)
			return; // no longer spectating

		// A previous AttachTo would fight the teleport - drop it first.
		if (PS_AttachManualCameraObserverComponent.s_Instance && PS_AttachManualCameraObserverComponent.s_Instance.GetTarget())
			PS_AttachManualCameraObserverComponent.s_Instance.Detach();

		camera.MoveToPosition(pos + vector.Up * SPECTATE_JUMP_EYE_OFFSET_M);
		// NOTE: with the spectator streaming observer removed, this moves the camera to the player's reported
		// position, but the player model only renders if they fall within what default NDS already streams
		// around the spectator's parked corpse - there is no battlefield streaming following the camera now.
	}

	void SwitchToObserver(IEntity from)
	{
		SCR_EditorManagerEntity editorManagerEntity = SCR_EditorManagerEntity.GetInstance();
		if (editorManagerEntity.IsOpened())
			return;
		
		if (m_Camera)
			return;
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.SpectatorMenu);
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		IEntity entity = thisPlayerController.GetControlledEntity();
		EntitySpawnParams params = new EntitySpawnParams();
		if (from)
			from.GetTransform(params.Transform);
		// Spectators talk on the global VoN room (matches SendPlayerToSpectator_S and RoomSwitchToGlobal).
		// This was "", "" (the empty-room channel), which briefly routed the spectator to a different
		// channel than everyone else's global room.
		MoveToVoNRoom(thisPlayerController.GetPlayerId(), "", "#PS-VoNRoom_Global");
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

		// Body-less: keep the spectator camera from being stolen by the corpse death-cam / editor / map.
		StartSpectatorCameraWatchdog();
	}

	void SwitchFromObserver()
	{
		if (!m_Camera)
			return;
		GetGame().GetCallqueue().Remove(EnforceSpectatorCamera);
		GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.SpectatorMenu);
		SCR_EntityHelper.DeleteEntityAndChildren(m_Camera);
		m_Camera = null;
	}

	// Body-less spectator camera watchdog: the player keeps their dead CORPSE as the controlled entity,
	// whose death-cam (and the editor/world/preview cameras) try to grab the view. While spectating,
	// re-assert the free spectator camera as the active one. Ported from LiteLobby (EnforceSpectatorCamera).
	protected void StartSpectatorCameraWatchdog()
	{
		GetGame().GetCallqueue().Remove(EnforceSpectatorCamera);
		GetGame().GetCallqueue().CallLater(EnforceSpectatorCamera, 500, true);
		GetGame().GetCallqueue().CallLater(EnforceSpectatorCamera, 0, false);
	}
	protected void EnforceSpectatorCamera()
	{
		if (!m_Camera)
		{
			GetGame().GetCallqueue().Remove(EnforceSpectatorCamera);
			return;
		}
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		MenuBase topMenu = menuManager.GetTopMenu();
		// No menu at all (a stage preview closed over us): the spectator menu IS this player's GAME
		// view - restore it, then re-take the camera below.
		if (!topMenu && !menuManager.IsAnyDialogOpen())
		{
			if (!menuManager.FindMenuByPreset(ChimeraMenuPreset.SpectatorMenu))
				menuManager.OpenMenu(ChimeraMenuPreset.SpectatorMenu);
		}
		// A fullscreen menu other than the spectator screen is on top (lobby/briefing/map opened over
		// us): ownership is irrelevant while it covers the screen - decide again next tick.
		else if (topMenu && !topMenu.IsInherited(PS_SpectatorMenu))
			return;

		// An opened editor grabs the camera every frame - close it (reopening GM stays one key away).
		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();
		if (editorManager && editorManager.IsOpened())
		{
			if (editorManager.IsInTransition())
				return;
			editorManager.Close(false);
			return;
		}

		CameraManager cameraManager = GetGame().GetCameraManager();
		if (!cameraManager)
			return;
		if (cameraManager.CurrentCamera() != m_Camera)
			cameraManager.SetCamera(CameraBase.Cast(m_Camera));
	}

	// Force change game state
	void ForceGameStart()
	{
		Rpc(RPC_ForceGameStart)
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_ForceGameStart()
	{
		// only admins can force start
		PlayerManager playerManager = GetGame().GetPlayerManager();
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		EPlayerRole playerRole = playerManager.GetPlayerRoles(thisPlayerController.GetPlayerId());
		if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
			return;

		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (gameMode.GetState() == SCR_EGameModeState.PREGAME)
			gameMode.StartGameMode();
	}

	void ForceSwitch(int playerId)
	{
		Rpc(RPC_ForceSwitch, playerId);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_ForceSwitch(int playerId)
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		playableManager.ForceSwitch(playerId)
	}

	// Server: ask the owning client to enter the spectator camera/menu. Used on death - the player
	// keeps their corpse as the controlled entity, so no control change fires the client trigger.
	void EnterSpectatorOwner()
	{
		// Listen host: Rpc() never executes on the sending machine, so call directly there.
		if (GetGame().GetPlayerController() == GetOwner())
			RPC_EnterSpectator();
		else
			Rpc(RPC_EnterSpectator);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RPC_EnterSpectator()
	{
		// Dead -> menu speaker: refresh the menu talking device (death does not change the controlled
		// entity, so OnControlledEntityChanged would not fire this).
		PS_MenuVoN.Refresh();

		// Open the spectator camera/menu, starting at the corpse if we still control it.
		PlayerController pc = PlayerController.Cast(GetOwner());
		IEntity corpse;
		if (pc)
			corpse = pc.GetControlledEntity();
		SwitchToObserver(corpse);

		// The corpse's dead life-state and the released playable slot replicate on a path that is NOT
		// ordered with this RPC, so PS_IsMenuSpeaker can still read FALSE for a frame or two right here.
		// Refresh() would then deactivate the menu device and never re-trigger (control does not change
		// while spectating) - the spectator could neither speak nor hear. Re-run it after the state has
		// settled so it activates reliably.
		GetGame().GetCallqueue().Remove(RefreshMenuVoNRetry);
		GetGame().GetCallqueue().CallLater(RefreshMenuVoNRetry, 300, false);
		GetGame().GetCallqueue().CallLater(RefreshMenuVoNRetry, 1200, false);
	}
	protected void RefreshMenuVoNRetry()
	{
		PS_MenuVoN.Refresh();
	}

	// Get controll on selected playable entity
	void ApplyPlayable()
	{
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		if (!playableManager)
			return;
		if (playableManager.GetPlayableByPlayer(thisPlayerController.GetPlayerId()) == RplId.Invalid())
			SwitchToObserver(null);
		Rpc(RPC_ApplyPlayable);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_ApplyPlayable()
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		PlayerController playerController = PlayerController.Cast(GetOwner());
		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		playableManager.ApplyPlayable(playerController.GetPlayerId());
	}

	void UnpinPlayer(int playerId)
	{
		Rpc(RPC_UnpinPlayer, playerId)
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_UnpinPlayer(int playerId)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
			return;

		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		playableManager.SetPlayerPin(playerId, false);
	}

	void PinPlayer(int playerId)
	{
		Rpc(RPC_PinPlayer, playerId)
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_PinPlayer(int playerId)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
			return;

		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		playableManager.SetPlayerPin(playerId, true);
	}

	void KickPlayer(int playerId)
	{
		Rpc(RPC_KickPlayer, playerId)
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_KickPlayer(int playerId)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();

		// If not admin you can change only herself
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		EPlayerRole playerRole = playerManager.GetPlayerRoles(thisPlayerController.GetPlayerId());
		if (playerRole == EPlayerRole.NONE)
			return;

		playerManager.KickPlayer(playerId, PlayerManagerKickReason.KICK, 0);
	}

	// -------------------- Set ---------------------
	void SetPlayerState(int playerId, PS_EPlayableControllerState state)
	{
		Rpc(RPC_SetPlayerState, playerId, state)
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_SetPlayerState(int playerId, PS_EPlayableControllerState state)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();

		// If not admin you can change only herself
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		EPlayerRole playerRole = playerManager.GetPlayerRoles(thisPlayerController.GetPlayerId());
		if (thisPlayerController.GetPlayerId() != playerId && playerRole == EPlayerRole.NONE)
			return;

		playableManager.SetPlayerState(playerId, state);
	}

	void SetPlayablePlayer(RplId playableId, int playerId)
	{
		Rpc(RPC_SetPlayablePlayer, playableId, playerId);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_SetPlayablePlayer(RplId playableId, int playerId)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();

		// You can't change playable if pinned and not admin
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		EPlayerRole playerRole = playerManager.GetPlayerRoles(thisPlayerController.GetPlayerId());
		if (playableManager.GetPlayerPin(playerId) && playerRole == EPlayerRole.NONE)
			return;

		// Check faction balance
		PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		PS_PlayableContainer playableContainer = playableManager.GetPlayableById(playableId);
		if (playableContainer)
		{
			FactionKey factionKey = playableContainer.GetFactionKey();
			if (playerId >= 0 && !SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()) && !gameModeCoop.CanJoinFaction(factionKey, playableManager.GetPlayerFactionKey(playerId)))
				return;
		}

		playableManager.SetPlayablePlayer(playableId, playerId);
	}

	void SetPlayableVehicleLocked(RplId vehicleId, bool lock)
	{
		Rpc(RPC_SetPlayableVehicleLocked, vehicleId, lock);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_SetPlayableVehicleLocked(RplId vehicleId, bool lock)
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		if (!SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()))
			return;
		
		playableManager.SetPlayableVehicleLocked(vehicleId, lock);
	}
	
	void SetPlayerPlayable(int playerId, RplId playableId)
	{
		Rpc(RPC_SetPlayerPlayable, playerId, playableId);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_SetPlayerPlayable(int playerId, RplId playableId)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();

		// You can't change playable if pinned and not admin
		PlayerController thisPlayerController = PlayerController.Cast(GetOwner());
		EPlayerRole playerRole = playerManager.GetPlayerRoles(thisPlayerController.GetPlayerId());
		if (playableManager.GetPlayerPin(playerId) && playerRole == EPlayerRole.NONE)
			return;

		// don't check other staff if empty playable
		if (playableId == RplId.Invalid()) {
			if (playerId != thisPlayerController.GetPlayerId())
				playableManager.NotifyKick(playerId);
			playableManager.SetPlayerPlayable(playerId, playableId);
			return;
		}

		// Check faction balance
		PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		PS_PlayableContainer playableContainer = playableManager.GetPlayableById(playableId);
		if (playableContainer)
		{
			FactionKey factionKey = playableContainer.GetFactionKey();
			if (playerId >= 0 && !SCR_Global.IsAdmin(thisPlayerController.GetPlayerId()) && !gameModeCoop.CanJoinFaction(factionKey, playableManager.GetPlayerFactionKey(playerId)))
				return;
		}

		SCR_ChimeraCharacter playableCharacter = SCR_ChimeraCharacter.Cast(playableContainer.GetPlayableComponent().GetOwner());

		// Check is playable already selected or dead
		int curretPlayerId = playableManager.GetPlayerByPlayable(playableId);
		if (playableCharacter.GetDamageManager().IsDestroyed() || (curretPlayerId != -1 && curretPlayerId != playerId)) {
			return;
		}

		playableManager.SetPlayerPlayable(playerId, playableId);

		// Pin player if setted by admin
		if (playerId != thisPlayerController.GetPlayerId())
			playableManager.SetPlayerPin(playerId, true);
	}

	void SetObjectiveCompleteState(PS_Objective objective, bool complete)
	{
		RplId objectiveId = objective.GetRplId();
		Rpc(RPC_SetObjectiveCompleteState, objectiveId, complete);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RPC_SetObjectiveCompleteState(RplId objectiveId, bool complete)
	{
		PS_Objective objective = PS_Objective.Cast(Replication.FindItem(objectiveId));
		if (objective)
			objective.SetCompleted(complete);
	}
}
