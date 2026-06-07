void PS_ScriptInvokerFactionChangeMethod(int playerId, FactionKey factionKey, FactionKey factionKeyOld);
typedef func PS_ScriptInvokerFactionChangeMethod;
typedef ScriptInvokerBase<PS_ScriptInvokerFactionChangeMethod> PS_ScriptInvokerFactionChange;

void PS_ScriptInvokerPlayableMethod(RplId id, PS_PlayableContainer playableComponent);
typedef func PS_ScriptInvokerPlayableMethod;
typedef ScriptInvokerBase<PS_ScriptInvokerPlayableMethod> PS_ScriptInvokerPlayable;

void PS_ScriptInvokerPinChangeMethod(int playerId, bool pin);
typedef func PS_ScriptInvokerPinChangeMethod;
typedef ScriptInvokerBase<PS_ScriptInvokerPinChangeMethod> PS_ScriptInvokerPinChange;

void PS_ScriptInvokerPlayerStateChangeMethod(int playerId, PS_EPlayableControllerState state);
typedef func PS_ScriptInvokerPlayerStateChangeMethod;
typedef ScriptInvokerBase<PS_ScriptInvokerPlayerStateChangeMethod> PS_ScriptInvokerPlayerStateChange;

void PS_ScriptInvokerPlayerPlayableChangeMethod(int playerId, RplId playbleId);
typedef func PS_ScriptInvokerPlayerPlayableChangeMethod;
typedef ScriptInvokerBase<PS_ScriptInvokerPlayerPlayableChangeMethod> PS_ScriptInvokerPlayerPlayableChange;

void PS_ScriptInvokerPlayableChangeGroupMethod(RplId id, PS_PlayableContainer playableComponent, SCR_AIGroup aiGroup);
typedef func PS_ScriptInvokerPlayableChangeGroupMethod;
typedef ScriptInvokerBase<PS_ScriptInvokerPlayableChangeGroupMethod> PS_ScriptInvokerPlayableChangeGroup;

void PS_ScriptInvokerFactionReadyChangeMethod(FactionKey factionKey, int readyValue);
typedef func PS_ScriptInvokerFactionReadyChangeMethod;
typedef ScriptInvokerBase<PS_ScriptInvokerFactionReadyChangeMethod> PS_ScriptInvokerFactionReadyChangeGroup;

[ComponentEditorProps(category: "GameScripted/GameMode/Components", description: "", color: "0 0 255 255", icon: HYBRID_COMPONENT_ICON)]
class PS_PlayableManagerClass : ScriptComponentClass
{
}

class PS_PlayableManager : ScriptComponent
{
	protected ref PS_LobbyCallbackHandler m_CallbackHandler = new PS_LobbyCallbackHandler();

	// Backward-compatible invokers (wire to CallbackHandler events)
	ref ScriptInvokerInt m_eOnPlayerConnected = new ScriptInvokerInt();
	ref ScriptInvokerBase<SCR_BaseGameMode_OnPlayerDisconnected> m_eOnPlayerDisconnected = new ScriptInvokerBase<SCR_BaseGameMode_OnPlayerDisconnected>();
	ref PS_ScriptInvokerFactionChange m_eOnFactionChange = new PS_ScriptInvokerFactionChange();
	ref PS_ScriptInvokerPlayable m_eOnPlayableRegistered = new PS_ScriptInvokerPlayable();
	ref PS_ScriptInvokerPlayable m_eOnPlayableUnregistered = new PS_ScriptInvokerPlayable();
	ref PS_ScriptInvokerPinChange m_eOnPlayerPinChange = new PS_ScriptInvokerPinChange();
	ref PS_ScriptInvokerPlayerStateChange m_eOnPlayerStateChange = new PS_ScriptInvokerPlayerStateChange();
	ref PS_ScriptInvokerPlayerPlayableChange m_eOnPlayerPlayableChange = new PS_ScriptInvokerPlayerPlayableChange();
	ref PS_ScriptInvokerPlayableChangeGroup m_eOnPlayableChangeGroup = new PS_ScriptInvokerPlayableChangeGroup();
	ref PS_ScriptInvokerFactionReadyChangeGroup m_eFactionReadyChanged = new PS_ScriptInvokerFactionReadyChangeGroup();
	ref ScriptInvokerInt m_eOnStartTimerCounterChanged = new ScriptInvokerInt();

	bool m_bRplLoaded = false;
	bool IsReplicated()
	{
		return m_bRplLoaded;
	}

	ScriptInvokerInt GetOnPlayerConnected() { return m_eOnPlayerConnected; }
	ScriptInvokerBase<SCR_BaseGameMode_OnPlayerDisconnected> GetOnPlayerDisconnected() { return m_eOnPlayerDisconnected; }
	PS_ScriptInvokerFactionChange GetOnFactionChange() { return m_eOnFactionChange; }
	PS_ScriptInvokerPlayable GetOnPlayableRegistered() { return m_eOnPlayableRegistered; }
	PS_ScriptInvokerPlayable GetOnPlayableUnregistered() { return m_eOnPlayableUnregistered; }
	PS_ScriptInvokerPinChange GetOnPlayerPinChange() { return m_eOnPlayerPinChange; }
	PS_ScriptInvokerPlayerStateChange GetOnPlayerStateChange() { return m_eOnPlayerStateChange; }
	PS_ScriptInvokerPlayerPlayableChange GetOnPlayerPlayableChange() { return m_eOnPlayerPlayableChange; }
	PS_ScriptInvokerPlayableChangeGroup GetOnPlayableChangeGroup() { return m_eOnPlayableChangeGroup; }
	PS_ScriptInvokerFactionReadyChangeGroup GetOnFactionReadyChanged() { return m_eFactionReadyChanged; }
	ScriptInvokerInt GetOnStartTimerCounterChanged() { return m_eOnStartTimerCounterChanged; }

	[RplProp()]
	protected ref ReplicatedClassMap<RplId, ref PS_SlotCharacterData> m_SlotsMap = new ReplicatedClassMap<RplId, ref PS_SlotCharacterData>();
	[RplProp()]
	protected ref ReplicatedBasicMap<int, RplId> m_PlayerSlotMap = new ReplicatedBasicMap<int, RplId>();
	[RplProp()]
	protected ref ReplicatedBasicMap<int, int> m_GroupCallsignsMap = new ReplicatedBasicMap<int, int>();
	[RplProp()]
	protected ref ReplicatedBasicMap<int, string> m_PlayerNamesCached = new ReplicatedBasicMap<int, string>();
	[RplProp()]
	protected ref ReplicatedBasicMap<int, string> m_GroupEntityNames = new ReplicatedBasicMap<int, string>();
	[RplProp()]
	ref array<RplId> m_SlotsSortedCached = {};

	array<RplId> GetSortedSlotIds() { return m_SlotsSortedCached; }
	[RplProp()]
	protected ref ReplicatedClassMap<RplId, ref PS_VehicleData> m_VehicleMap = new ReplicatedClassMap<RplId, ref PS_VehicleData>();
	[RplProp()]
	protected ref ReplicatedBasicMap<FactionKey, int> m_FactionReadyMap = new ReplicatedBasicMap<FactionKey, int>();
	[RplProp()]
	protected ref ReplicatedBasicMap<int, FactionKey> m_PlayerFactionMap = new ReplicatedBasicMap<int, FactionKey>();
	[RplProp()]
	protected ref ReplicatedBasicMap<int, PS_EPlayableControllerState> m_PlayerStatesMap = new ReplicatedBasicMap<int, PS_EPlayableControllerState>();
	[RplProp()]
	protected ref ReplicatedBasicMap<int, bool> m_PlayerPinMap = new ReplicatedBasicMap<int, bool>();
	[RplProp()]
	protected ref array<int> m_DisconnectedPlayersClient = {};
	[RplProp()]
	int m_iMaxPlayersCount = 1;
	[RplProp(onRplName: "OnStartTimerCounterChanged")]
	int m_iStartTimerCounter = -1;

	protected ref map<int, int> m_AIToPlayerGroupMap = new map<int, int>();
	protected ref map<string, int> m_PlayerGUIDtoIdCached = new map<string, int>();
	protected ref map<int, string> m_PlayerIdToGuidCached = new map<int, string>();
	protected ref map<string, RplId> m_DisconnectedPlayers = new map<string, RplId>();
	protected ref map<int, FactionKey> m_FactionRemembered = new map<int, FactionKey>();
	protected ref map<RplId, IEntity> m_EntityCache = new map<RplId, IEntity>();

	protected PS_GameModeCoop m_GameModeCoop;
	protected ScriptCallQueue m_CallQueue;
	protected PlayerManager m_PlayerManager;
	protected SCR_PlayerController m_CurrentPlayerController;
	static protected PS_PlayableControllerComponent s_CurrentPlayableController;
	protected static PS_PlayableManager s_Instance;
	bool m_bFactionsReadySended;

	static PS_PlayableManager GetInstance()
	{
		return s_Instance;
	}

	bool IsBulkRemoving()
	{
		return m_bBulkRemoving;
	}

	PS_LobbyCallbackHandler GetCallbackHandler()
	{
		return m_CallbackHandler;
	}

	ReplicatedClassMap<RplId, ref PS_SlotCharacterData> GetSlots()
	{
		return m_SlotsMap;
	}

	ReplicatedBasicMap<int, RplId> GetPlayerSlots()
	{
		return m_PlayerSlotMap;
	}

	ReplicatedClassMap<RplId, ref PS_VehicleData> GetVehicles()
	{
		return m_VehicleMap;
	}

	ReplicatedBasicMap<FactionKey, int> GetFactionReadyMap()
	{
		return m_FactionReadyMap;
	}

	bool FindSlotData(RplId slotId, out PS_SlotCharacterData slotData)
	{
		return m_SlotsMap.Find(slotId, slotData);
	}

	bool FindPlayerIdBySlot(RplId slotId, out int playerId)
	{
		PS_SlotCharacterData slot;
		if (FindSlotData(slotId, slot))
			playerId = slot.m_PlayerId;
		else
			playerId = -1;
		return playerId != -1;
	}

	bool FindPlayerSlotById(int playerId, out RplId slotId)
	{
		bool found = m_PlayerSlotMap.Find(playerId, slotId);
		if (!found) slotId = RplId.Invalid();
		return found;
	}

	bool IsSlotAvailable(RplId rplId)
	{
		int playerId;
		return !FindPlayerIdBySlot(rplId, playerId);
	}

	bool IsSlotLocked(RplId slotId)
	{
		PS_SlotCharacterData slot;
		if (FindSlotData(slotId, slot))
			return slot.m_IsLocked;
		return false;
	}

	bool IsSlotCharacterDestroyed(RplId slotId)
	{
		PS_SlotCharacterData slot;
		if (FindSlotData(slotId, slot))
			return slot.m_IsDestroyed;
		return false;
	}

	string GetPlayerNameById(int playerId)
	{
		return m_PlayerNamesCached.Get(playerId);
	}

	FactionKey GetPlayerFactionKey(int playerId)
	{
		FactionKey key;
		m_PlayerFactionMap.Find(playerId, key);
		return key;
	}

	RplId GetPlayableByPlayer(int playerId)
	{
		RplId slotId;
		FindPlayerSlotById(playerId, slotId);
		return slotId;
	}

	int GetPlayerByPlayableRemembered(RplId slotId)
	{
		PS_SlotCharacterData slot;
		if (FindSlotData(slotId, slot))
			return slot.m_PlayerId;
		return -1;
	}

	string GetSlotName(RplId slotId)
	{
		PS_SlotCharacterData slot;
		if (FindSlotData(slotId, slot))
			return slot.m_sName;
		return "";
	}

	string GetSlotFactionKey(RplId slotId)
	{
		PS_SlotCharacterData slot;
		if (FindSlotData(slotId, slot))
			return slot.m_FactionKey;
		return "";
	}

	int GetSlotGroupId(RplId slotId)
	{
		PS_SlotCharacterData slot;
		if (FindSlotData(slotId, slot))
			return slot.m_PlayerGroupId;
		return -1;
	}

	ResourceName GetSlotCharacterPrefabPath(RplId slotId)
	{
		PS_SlotCharacterData slot;
		if (FindSlotData(slotId, slot))
			return slot.m_CharacterPrefabPath;
		return "";
	}

	void GetSlotIcon(RplId slotId, out ResourceName icon, out string iconSetName)
	{
		PS_SlotCharacterData slot;
		if (!FindSlotData(slotId, slot))
			return;
		icon = slot.m_sRoleIconPath;
		iconSetName = slot.m_sRoleIconQuad;
	}

	int GetGroupCallsignByPlayable(RplId slotId)
	{
		int groupId = GetSlotGroupId(slotId);
		return m_GroupCallsignsMap.Get(groupId);
	}

	string GetGroupEntityName(int groupId)
	{
		return m_GroupEntityNames.Get(groupId);
	}

	bool IsPlayerTopSlotInGroup(int playerId)
	{
		RplId slotId;
		if (!FindPlayerSlotById(playerId, slotId) || slotId == RplId.Invalid())
			return false;

		int groupId = GetSlotGroupId(slotId);
		FactionKey factionKey = GetSlotFactionKey(slotId);
		foreach (RplId otherSlotId : m_SlotsSortedCached)
		{
			if (factionKey != GetSlotFactionKey(otherSlotId))
				continue;
			int otherGroupId = GetSlotGroupId(otherSlotId);
			if (groupId != otherGroupId)
				continue;
			int otherPlayerId;
			if (!FindPlayerIdBySlot(otherSlotId, otherPlayerId))
				continue;
			if (otherPlayerId == playerId)
				return true;
			else
				return false;
		}
		return false;
	}

	bool IsPlayerFactionCommander(int playerId, out FactionKey factionKey)
	{
		RplId slotId;
		if (!FindPlayerSlotById(playerId, slotId) || slotId == RplId.Invalid())
			return false;
		factionKey = GetSlotFactionKey(slotId);
		foreach (RplId otherSlotId : m_SlotsSortedCached)
		{
			if (factionKey != GetSlotFactionKey(otherSlotId))
				continue;
			int otherPlayerId;
			if (!FindPlayerIdBySlot(otherSlotId, otherPlayerId))
				continue;
			if (otherPlayerId == playerId)
				return true;
			else
				return false;
		}
		return false;
	}

	PS_PlayableContainer GetPlayableById(RplId slotId)
	{
		PS_SlotCharacterData slot;
		if (!FindSlotData(slotId, slot))
			return null;
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(slotId));
		if (!rpl)
			return null;
		PS_PlayableComponent comp = PS_PlayableComponent.Cast(rpl.GetEntity().FindComponent(PS_PlayableComponent));
		if (comp)
			return comp.GetPlayableContainer();
		return null;
	}

	map<FactionKey, ref array<int>> GetTopPlayerInEachGroup(out map<int, int> groupLeaders = null)
	{
		if (!groupLeaders)
			groupLeaders = new map<int, int>();
		else
			groupLeaders.Clear();
		ref map<FactionKey, ref array<int>> result = new map<FactionKey, ref array<int>>();
		foreach (RplId slotId : m_SlotsSortedCached)
		{
			FactionKey factionKey = GetSlotFactionKey(slotId);
			if (!result.Contains(factionKey))
				result[factionKey] = {};
			int groupId = GetSlotGroupId(slotId);
			int playerId = -1;
			if (groupId != -1 && !groupLeaders.Contains(groupId) && FindPlayerIdBySlot(slotId, playerId))
			{
				groupLeaders[groupId] = playerId;
				result[factionKey].Insert(playerId);
			}
		}
		return result;
	}

	SCR_AIGroup GetPlayerGroupByPlayable(RplId slotId)
	{
		int groupId = GetSlotGroupId(slotId);
		if (groupId == -1) return null;
		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		return groupsManager.FindGroup(groupId);
	}

	SCR_AIGroup GetPlayerGroupByVehicle(PS_VehicleData vehicleData)
	{
		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		return groupsManager.FindGroup(vehicleData.m_GroupId);
	}

	map<RplId, ref PS_VehicleData> GetPlayableVehicles()
	{
		return m_VehicleMap.GetRawMap();
	}

	int GetMaxPlayers()
	{
		return m_iMaxPlayersCount;
	}

	static PS_PlayableControllerComponent GetPlayableController()
	{
		return s_CurrentPlayableController;
	}

	// --------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		s_Instance = this;
		PS_DebugLogger.LogImportant("PS_PlayableManager initialized");
		m_GameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		m_CallQueue = GetGame().GetCallqueue();
		m_PlayerManager = GetGame().GetPlayerManager();
		m_bRplLoaded = true;
		BuildSortedSlotsArray();
		PS_DebugLogger.LogImportant("OnPostInit slots=" + m_SlotsMap.Count().ToString() + " sorted=" + m_SlotsSortedCached.Count().ToString());

		if (RplSession.Mode() == RplMode.Dedicated)
			ForceGetSessionMaxPlayersCount();

		m_GameModeCoop.GetOnPlayerConnected().Insert(OnPlayerConnected);
		m_GameModeCoop.GetOnPlayerDisconnected().Insert(OnPlayerDisconnected);
		m_GameModeCoop.GetOnPlayerRoleChange().Insert(OnPlayerRoleChange);
		m_CallQueue.Call(LateInit, owner);
	}

	protected void LateInit(IEntity owner)
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return;
		m_CurrentPlayerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!m_CurrentPlayerController)
		{
			m_CallQueue.Call(LateInit, owner);
			return;
		}
		s_CurrentPlayableController = m_CurrentPlayerController.PS_GetPlayableComponent();
	}

	protected void ForceGetSessionMaxPlayersCount()
	{
		DSSession dSSession = GetGame().GetBackendApi().GetDSSession();
		if (dSSession)
		{
			m_iMaxPlayersCount = dSSession.PlayerLimit();
			Replication.BumpMe();
		}
		else
			m_CallQueue.Call(ForceGetSessionMaxPlayersCount);
	}

	// --------------------------------------------------------------------------------------------
	void StartTime()
	{
		m_iStartTimerCounter -= 1;
		Replication.BumpMe();
		OnStartTimerCounterChanged();
		if (m_iStartTimerCounter == 0)
		{
			PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
			gameModeCoop.AdvanceGameState(SCR_EGameModeState.SLOTSELECTION);
			m_CallQueue.Remove(StartTime);
		}
	}

	void OnStartTimerCounterChanged()
	{
		m_eOnStartTimerCounterChanged.Invoke(m_iStartTimerCounter);
	}

	// --------------------------------------------------------------------------------------------
	void ApplyPlayable(int playerId)
	{
		if (!Replication.IsServer())
			return;
		SCR_PlayerController playerController = SCR_PlayerController.Cast(m_PlayerManager.GetPlayerController(playerId));
		if (!playerController)
			return;
		PS_PlayableControllerComponent playableController = playerController.PS_GetPlayableComponent();

		PS_DebugLogger.LogImportant("ApplyPlayable player=" + playerId.ToString(), playerId);

		SetPlayerState(playerId, PS_EPlayableControllerState.Playing);

		RplId slotId = GetPlayableByPlayer(playerId);
		PS_DebugLogger.LogImportant("ApplyPlayable slotId=" + slotId.ToString() + " playerId=" + playerId.ToString(), playerId);
		
		if (slotId != RplId.Invalid() && IsSlotCharacterDestroyed(slotId))
		{
			PS_DebugLogger.LogImportant("ApplyPlayable BRANCH: slot destroyed, delayed switch playerId=" + playerId.ToString(), playerId);
			PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();
			if (vonManager)
				vonManager.SetPlayerToChannel(playerId, "");
			m_CallQueue.CallLater(DelayedSwitchToInitialEntity, 1000, false, playerId);
			return;
		}

		IEntity entity;
		if (slotId == RplId.Invalid())
		{
			PS_DebugLogger.LogImportant("ApplyPlayable BRANCH: slot INVALID, switching to spectator playerId=" + playerId.ToString(), playerId);
			SCR_GroupsManagerComponent groupsManagerComponent = SCR_GroupsManagerComponent.GetInstance();
			SCR_AIGroup currentGroup = groupsManagerComponent.GetPlayerGroup(playerId);
			if (currentGroup)
				currentGroup.RemovePlayer(playerId);
			SetPlayerFactionKey(playerId, "");

			PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();
			if (vonManager)
				vonManager.SetPlayerToChannel(playerId, "");

			entity = playableController.GetInitialEntity();
			if (!entity)
			{
				Resource resource = Resource.Load("{ADDE38E4119816AB}Prefabs/InitialPlayer_Version2.et");
				EntitySpawnParams params = new EntitySpawnParams();
				entity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
				playableController.SetInitialEntity(entity);
			}
			playerController.SetInitialMainEntity(entity);
			playableController.SwitchToObserverServer();
			return;
		}

		if (!m_SlotsMap.Contains(slotId))
		{
			PS_DebugLogger.LogImportant("ApplyPlayable FAIL: slot not in map slotId=" + slotId.ToString(), playerId);
			return;
		}

		PS_SlotCharacterData slotData = m_SlotsMap[slotId];
		IEntity slotEntity = IEntity.Cast(Replication.FindItem(slotId));
		if (!slotEntity)
		{
			if (m_EntityCache.Find(slotId, slotEntity) && slotEntity)
			{
				PS_DebugLogger.LogImportant("ApplyPlayable using CACHED entity as fallback slotId=" + slotId.ToString(), playerId);
				RplComponent verifyRpl = RplComponent.Cast(slotEntity.FindComponent(RplComponent));
				if (verifyRpl)
					PS_DebugLogger.LogImportant("ApplyPlayable cached entity RplId=" + verifyRpl.Id().ToString() + " expected=" + slotId.ToString(), playerId);
			}
			else
			{
				PS_DebugLogger.LogImportant("ApplyPlayable FAIL: entity not found via Replication or cache slotId=" + slotId.ToString() + " retrying in 1s", playerId);
				m_CallQueue.CallLater(RetryApplyPlayable, 1000, false, playerId, slotId, 0);
				return;
			}
		}

		IEntity defaultEntity = playableController.GetInitialEntity();
		if (defaultEntity)
		{
			PS_DebugLogger.LogImportant("ApplyPlayable defaultEntity EXISTS, deleting playerId=" + playerId.ToString(), playerId);
			SCR_EntityHelper.DeleteEntityAndChildren(defaultEntity);
		}
		else
		{
			PS_DebugLogger.LogImportant("ApplyPlayable defaultEntity NULL playerId=" + playerId.ToString(), playerId);
		}

		playerController.SetInitialMainEntity(slotEntity);
		PS_DebugLogger.LogImportant("ApplyPlayable SetInitialMainEntity done, calling ChangeGroup", playerId);

		SCR_ChimeraCharacter playableCharacter = SCR_ChimeraCharacter.Cast(slotEntity);
		if (!playableCharacter)
			return;
		SCR_Faction faction = SCR_Faction.Cast(playableCharacter.GetFaction());
		SetPlayerFactionKey(playerId, faction.GetFactionKey());

		m_CallQueue.CallLater(ChangeGroup, 0, false, playerId, slotId);
		
		PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();
		if (vonManager)
			vonManager.SetPlayerToChannel(playerId, "");
		
		PS_DebugLogger.LogImportant("ApplyPlayable SUCCESS player=" + playerId.ToString() + " slot=" + slotId.ToString(), playerId);
	}

	protected void RetryApplyPlayable(int playerId, RplId slotId, int attempt)
	{
		if (!m_SlotsMap.Contains(slotId))
		{
			PS_DebugLogger.LogImportant("RetryApplyPlayable FAIL: slot removed from map slotId=" + slotId.ToString(), playerId);
			return;
		}
		
		IEntity slotEntity = IEntity.Cast(Replication.FindItem(slotId));
		if (slotEntity)
		{
			PS_DebugLogger.LogImportant("RetryApplyPlayable SUCCESS on attempt=" + attempt.ToString() + " slotId=" + slotId.ToString(), playerId);
			ApplyPlayable(playerId);
			return;
		}
		
		if (attempt >= 30)
		{
			PS_DebugLogger.LogImportant("RetryApplyPlayable GAVE UP after 30 attempts, marking slot destroyed slotId=" + slotId.ToString(), playerId);
			SetSlotDestroyed(slotId, true);
			m_CallQueue.Remove(RetryApplyPlayable);
			ApplyPlayable(playerId);
			return;
		}
		
		m_CallQueue.CallLater(RetryApplyPlayable, 1000, false, playerId, slotId, attempt + 1);
	}
	
	protected void DelayedSwitchToInitialEntity(int playerId)
	{
		PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		gameModeCoop.SwitchToInitialEntity(playerId);
	}

	void ChangeGroup(int playerId, RplId slotId)
	{
		if (!m_SlotsMap.Contains(slotId))
			return;

		SCR_PlayerController playerController = SCR_PlayerController.Cast(m_PlayerManager.GetPlayerController(playerId));
		PS_PlayableControllerComponent playableController = playerController.PS_GetPlayableComponent();

		SCR_AIGroup playerGroup = GetPlayerGroupByPlayable(slotId);
		SCR_ChimeraCharacter leaderCharacter = null;
		if (playerGroup)
			leaderCharacter = SCR_ChimeraCharacter.Cast(playerGroup.GetLeaderEntity());

		SCR_PlayerControllerGroupComponent playerControllerGroupComponent = SCR_PlayerControllerGroupComponent.Cast(playerController.FindComponent(SCR_PlayerControllerGroupComponent));
		if (playerGroup)
			playerControllerGroupComponent.PS_AskJoinGroup(playerGroup.GetGroupID());

		if (playerGroup && playerGroup.GetNameAuthorID() == -1)
			playerGroup.SetCustomName(playerGroup.GetCustomName(), playerId);
	}

	// --------------------------------------------------------------------------------------------
	void InsertLobbySlot(PS_SlotCharacterData slot)
	{
		if (!Replication.IsServer())
			return;
		RplId rplId = slot.m_RplId;
		if (slot.m_CachedEntity)
			m_EntityCache[rplId] = slot.m_CachedEntity;

		SCR_AIGroup group = slot.GetGroup();
		if (m_SlotsMap.Contains(rplId) || !group)
			return;

		RplComponent rplComp = RplComponent.Cast(group.FindComponent(RplComponent));
		if (!rplComp)
			return;
		int aiGroupId = rplComp.Id();
		int encodedCallsign;
		int joinableGroupId = GetOrCreatePlayerGroup(rplId, group, encodedCallsign);
		slot.m_PlayerGroupId = joinableGroupId;

		PS_DebugLogger.Log("InsertLobbySlot slot=" + rplId.ToString() + " name=" + slot.m_sName + " faction=" + slot.m_FactionKey);

		RPC_InsertLobbySlot(slot, aiGroupId, joinableGroupId, encodedCallsign, group.GetName());
		Rpc(RPC_InsertLobbySlot, slot, aiGroupId, joinableGroupId, encodedCallsign, group.GetName());

		PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();
		if (vonManager)
			vonManager.InitChannelIfNeeded(vonManager.GetFactionChannelKey(slot.m_FactionKey));
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_InsertLobbySlot(PS_SlotCharacterData slot, int aiGroupId, int joinableGroupId, int encodedCallsign, string groupEntityName)
	{
		RplId rplId = slot.m_RplId;
		PS_DebugLogger.Log("RPC_InsertLobbySlot slot=" + rplId.ToString() + " groupId=" + joinableGroupId.ToString());

		m_SlotsMap[rplId] = slot;
		m_AIToPlayerGroupMap[aiGroupId] = joinableGroupId;

		if (!m_GroupCallsignsMap.Contains(joinableGroupId))
		{
			m_GroupCallsignsMap[joinableGroupId] = encodedCallsign;
			m_GroupEntityNames[joinableGroupId] = groupEntityName;
		}

		if (!m_FactionReadyMap.Contains(slot.m_FactionKey))
			m_FactionReadyMap.Insert(slot.m_FactionKey, 0);

		BuildSortedSlotsArray();
		GetGame().GetCallqueue().Call(InvokeSlotInserted, rplId, joinableGroupId, slot.m_FactionKey);
	}

	protected void InvokeSlotInserted(RplId slotId, int joinableGroupId, FactionKey factionKey)
	{
		m_CallbackHandler.GetOnSlotInserted().Invoke(slotId, joinableGroupId, factionKey);
		m_eOnPlayableRegistered.Invoke(slotId, GetPlayableById(slotId));
	}

	void RemoveLobbySlot(RplId slotId)
	{
		if (!Replication.IsServer())
			return;
		if (!m_SlotsMap.Contains(slotId))
			return;

		int playerId = -1;
		bool isPlayerOnSlot = FindPlayerIdBySlot(slotId, playerId);
		
		// Fallback: use m_PlayerSlotMap if slot data lookup failed
		if (!isPlayerOnSlot)
		{
			foreach (int pid, RplId sid : m_PlayerSlotMap.GetRawMap())
			{
				if (sid == slotId)
				{
					playerId = pid;
					isPlayerOnSlot = true;
					break;
				}
			}
		}
		FactionKey factionKey = GetSlotFactionKey(slotId);

		PS_DebugLogger.Log("RemoveLobbySlot slot=" + slotId.ToString() + " hadPlayer=" + playerId.ToString());

		RPC_RemoveLobbySlot(slotId);
		Rpc(RPC_RemoveLobbySlot, slotId);

		if (isPlayerOnSlot)
		{
			m_PlayerSlotMap.Remove(playerId);
			PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();
			if (vonManager)
				vonManager.MoveToRoom(playerId, "", "#PS-VoNRoom_Global");
		}
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_RemoveLobbySlot(RplId slotId)
	{
		int playerId;
		if (FindPlayerIdBySlot(slotId, playerId))
			m_PlayerSlotMap.Remove(playerId);
		else
			playerId = -1;
		FactionKey factionKey = GetSlotFactionKey(slotId);

		m_SlotsMap.Remove(slotId);
		m_EntityCache.Remove(slotId);
		m_CallbackHandler.GetOnSlotRemoved().Invoke(slotId, factionKey, playerId);
		m_eOnPlayableUnregistered.Invoke(slotId, GetPlayableById(slotId));

		PS_DebugLogger.Log("RPC_RemoveLobbySlot slot=" + slotId.ToString());

		BuildSortedSlotsArray();
		Replication.BumpMe();
	}

	protected int GetOrCreatePlayerGroup(RplId rplId, SCR_AIGroup group, out int encodedCallsign)
	{
		int aiGroupId = RplComponent.Cast(group.FindComponent(RplComponent)).Id();
		int playerGroupId;
		if (m_AIToPlayerGroupMap.Find(aiGroupId, playerGroupId))
		{
			encodedCallsign = m_GroupCallsignsMap[playerGroupId];
			return playerGroupId;
		}

		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		Faction groupFaction = group.GetFaction();

		SCR_CallsignGroupComponent groupCallsign = SCR_CallsignGroupComponent.Cast(group.FindComponent(SCR_CallsignGroupComponent));
		int company, platoon, squad;
		groupCallsign.GetCallsignIndexes(company, platoon, squad);
		encodedCallsign = 1000000 * company + 1000 * platoon + squad;

		SCR_AIGroup joinableGroup = groupsManager.CreateNewPlayableGroup(groupFaction);
		SCR_CallsignGroupComponent joinableGroupCallsign = SCR_CallsignGroupComponent.Cast(joinableGroup.FindComponent(SCR_CallsignGroupComponent));

		group.SetDeleteWhenEmpty(false);
		group.SetCanDeleteIfNoPlayer(false);
		joinableGroup.SetDeleteWhenEmpty(false);
		joinableGroup.SetCanDeleteIfNoPlayer(false);
		joinableGroup.SetMaxMembers(group.m_aUnitPrefabSlots.Count());
		joinableGroup.SetCustomName(group.GetCustomName(), -1);
		GetGame().GetCallqueue().Call(joinableGroupCallsign.DoAssignCallsign, company, platoon, squad);
		GetGame().GetCallqueue().Call(joinableGroup.SetSlave, group);

		return joinableGroup.GetGroupID();
	}

	array<RplId> BuildSortedSlotsArray()
	{
		array<RplId> slotsSorted = {};
		foreach (RplId slotId, PS_SlotCharacterData slot : m_SlotsMap.GetRawMap())
		{
			int groupId = slot.m_PlayerGroupId;
			int callsign = m_GroupCallsignsMap[groupId];
			int insertIndex = slotsSorted.Count();

			for (int i = 0; i < slotsSorted.Count(); i++)
			{
				RplId otherSlotId = slotsSorted[i];
				int otherGroupId = GetSlotGroupId(otherSlotId);
				int otherCallsign = m_GroupCallsignsMap[otherGroupId];
				PS_SlotCharacterData otherSlot = m_SlotsMap[otherSlotId];

				bool rplIdGreater = otherSlotId > slotId;
				bool rankEquival = slot.m_eCharacterRank == otherSlot.m_eCharacterRank;
				bool rankGreater = slot.m_eCharacterRank > otherSlot.m_eCharacterRank;
				bool callsignEquival = otherCallsign == callsign;
				bool callsignGreater = otherCallsign > callsign;

				if ((((rplIdGreater && rankEquival) || rankGreater) && callsignEquival) || callsignGreater)
				{
					insertIndex = i;
					break;
				}
			}
			slotsSorted.InsertAt(slotId, insertIndex);
		}
		m_SlotsSortedCached = slotsSorted;
		return slotsSorted;
	}

	void RegisterPlayable(PS_PlayableComponent playableComponent)
	{
		if (!Replication.IsServer())
			return;

		RplId playableId = playableComponent.GetRplId();
		if (m_SlotsMap.Contains(playableId))
			return;

		SCR_ChimeraCharacter playableCharacter = playableComponent.GetCharacter();
		if (!playableCharacter.PS_GetChimeraAIControlComponent())
			return;

		AIControlComponent aiControl = playableCharacter.PS_GetChimeraAIControlComponent();
		SCR_AIGroup playableGroup = SCR_AIGroup.Cast(aiControl.GetControlAIAgent().GetParentGroup());
		if (!playableGroup)
			return;

		PS_SlotCharacterData slot = new PS_SlotCharacterData();
		slot.InitFromPlayable(playableComponent);
		slot.SetGroup(playableGroup);

		m_EntityCache[playableId] = playableComponent.GetOwner();

		InsertLobbySlot(slot);
	}

	void UnRegisterPlayable(RplId playableId)
	{
		RemoveLobbySlot(playableId);
	}

	// --------------------------------------------------------------------------------------------
	void SetPlayerToSlot(RplId slotId, int playerId)
	{
		if (!Replication.IsServer())
			return;

		if (slotId != RplId.Invalid() && IsSlotCharacterDestroyed(slotId))
			return;

		RplId prevSlotId;
		FindPlayerSlotById(playerId, prevSlotId);
		FactionKey oldFactionKey = GetPlayerFactionKey(playerId);

		RPC_SetPlayerToSlot(slotId, playerId);
		Rpc(RPC_SetPlayerToSlot, slotId, playerId);

		PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();

		if (slotId == RplId.Invalid())
		{
			if (vonManager)
				vonManager.MoveToRoom(playerId, "", "#PS-VoNRoom_Global");
			SetPlayerFactionKey(playerId, "");
			return;
		}

		PS_SlotCharacterData slot;
		if (FindSlotData(slotId, slot))
		{
			FactionKey factionKey = slot.m_FactionKey;
			int groupCallsign = GetGroupCallsignByPlayable(slotId);

			if (vonManager)
			{
				if (oldFactionKey != "" && oldFactionKey != factionKey)
					vonManager.MoveToRoom(playerId, "", "#PS-VoNRoom_Global");

				vonManager.MoveToRoom(playerId, factionKey, groupCallsign.ToString());
			}

			SetPlayerFactionKey(playerId, factionKey);

			if (PS_GameModeCoop.Cast(GetGame().GetGameMode()).GetState() == SCR_EGameModeState.GAME)
			{
				PS_DebugLogger.LogImportant("SetPlayerToSlot state==GAME, scheduling ApplyPlayable slotId=" + slotId.ToString(), playerId);
				m_CallQueue.CallLater(ApplyPlayable, 200, false, playerId);
			}
		}
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_SetPlayerToSlot(RplId slotId, int playerId)
	{
		RplId prevSlotId;
		if (!m_PlayerSlotMap.Find(playerId, prevSlotId))
			prevSlotId = RplId.Invalid();

		if (prevSlotId != RplId.Invalid() && prevSlotId != slotId && !IsSlotCharacterDestroyed(prevSlotId))
		{
			PS_SlotCharacterData prevSlot;
			if (m_SlotsMap.Find(prevSlotId, prevSlot))
				prevSlot.m_PlayerId = -1;
		}

		if (slotId != RplId.Invalid())
		{
			PS_SlotCharacterData newSlot;
			if (m_SlotsMap.Find(slotId, newSlot))
			{
				newSlot.m_PlayerId = playerId;
				PS_DebugLogger.LogImportant("RPC_SetPlayerToSlot WRITE slot=" + slotId.ToString() + " storedPlayerId=" + newSlot.m_PlayerId.ToString(), playerId);
			}
		}

		if (slotId == RplId.Invalid())
			m_PlayerSlotMap.Remove(playerId);
		else
			m_PlayerSlotMap[playerId] = slotId;

		m_CallbackHandler.GetOnPlayerSetOnSlot().Invoke(playerId, prevSlotId, slotId);
		m_eOnPlayerPlayableChange.Invoke(playerId, slotId);

		PS_DebugLogger.Log("RPC_SetPlayerToSlot player=" + playerId.ToString() + " prevSlot=" + prevSlotId.ToString() + " newSlot=" + slotId.ToString(), playerId);
	}

	void KickPlayerFromSlot(RplId slotId, int kickingPlayerId)
	{
		if (!Replication.IsServer() || IsSlotAvailable(slotId))
			return;

		int playerIdToKick;
		if (!FindPlayerIdBySlot(slotId, playerIdToKick))
			return;

		string guid;
		if (m_PlayerIdToGuidCached.Find(playerIdToKick, guid))
			m_DisconnectedPlayers.Remove(guid);

		RPC_KickPlayerFromSlot(slotId);
		Rpc(RPC_KickPlayerFromSlot, slotId);
		Replication.BumpMe();

		PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();
		if (vonManager)
			vonManager.MoveToRoom(playerIdToKick, "", "#PS-VoNRoom_Global");
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_KickPlayerFromSlot(RplId slotId)
	{
		if (m_SlotsMap.Contains(slotId))
		{
			PS_SlotCharacterData sd;
			m_SlotsMap.Find(slotId, sd);
			int playerId = sd.m_PlayerId;
			sd.m_PlayerId = -1;
			m_PlayerSlotMap.Remove(playerId);
			m_CallbackHandler.GetOnPlayerSetOnSlot().Invoke(playerId, slotId, RplId.Invalid());
			m_eOnPlayerPlayableChange.Invoke(playerId, RplId.Invalid());
			if (IsDisconnected(playerId))
				m_DisconnectedPlayersClient.RemoveItem(playerId);
		}
	}

	bool IsDisconnected(int playerId)
	{
		return m_DisconnectedPlayersClient.Contains(playerId);
	}

	// --------------------------------------------------------------------------------------------
	void SetPlayerFactionKey(int playerId, FactionKey factionKey)
	{
		FactionKey oldKey = GetPlayerFactionKey(playerId);

		RPC_SetPlayerFactionKey(playerId, factionKey, oldKey);
		Rpc(RPC_SetPlayerFactionKey, playerId, factionKey, oldKey);

		if (factionKey != "")
			m_FactionRemembered[playerId] = factionKey;

		PlayerController playerController = m_PlayerManager.GetPlayerController(playerId);
		if (playerController && Replication.IsServer())
		{
			SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
			SCR_PlayerFactionAffiliationComponent playerFactionAffiliation = SCR_PlayerFactionAffiliationComponent.Cast(playerController.FindComponent(SCR_PlayerFactionAffiliationComponent));
			if (playerFactionAffiliation)
			{
				playerFactionAffiliation.SetAffiliatedFactionByKey(factionKey);
				factionManager.UpdatePlayerFaction_S(playerFactionAffiliation);
			}
		}

		PS_DebugLogger.Log("SetPlayerFactionKey player=" + playerId.ToString() + " old=" + oldKey + " new=" + factionKey, playerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_SetPlayerFactionKey(int playerId, FactionKey factionKey, FactionKey oldKey)
	{
		if (factionKey != "")
			m_PlayerFactionMap[playerId] = factionKey;
		else
			m_PlayerFactionMap.Remove(playerId);

		m_CallbackHandler.GetOnPlayerFactionChanged().Invoke(playerId, factionKey, oldKey);
		m_eOnFactionChange.Invoke(playerId, factionKey, oldKey);
	}

	FactionKey GetPlayerFactionKeyRemembered(int playerId)
	{
		FactionKey key = GetPlayerFactionKey(playerId);
		if (key != "")
			return key;
		FactionKey remembered;
		m_FactionRemembered.Find(playerId, remembered);
		return remembered;
	}

	// --------------------------------------------------------------------------------------------
	void SetPlayerState(int playerId, PS_EPlayableControllerState state)
	{
		RPC_SetPlayerState(playerId, state);
		Rpc(RPC_SetPlayerState, playerId, state);

		PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		SCR_EGameModeState gameModeState = gameModeCoop.GetState();
		if (gameModeState == SCR_EGameModeState.SLOTSELECTION)
		{
			m_CallQueue.Remove(StartTime);
			bool adminExist = !gameModeCoop.IsAdminMode();
			array<int> players = {};
			GetGame().GetPlayerManager().GetPlayers(players);
			foreach (int otherPlayerId : players)
			{
				if (!adminExist)
					adminExist = SCR_Global.IsAdmin(otherPlayerId);
				PS_EPlayableControllerState playerState;
				if (!m_PlayerStatesMap.Find(otherPlayerId, playerState) || playerState != PS_EPlayableControllerState.Ready)
				{
					if (m_iStartTimerCounter != -1)
					{
						m_iStartTimerCounter = -1;
						Replication.BumpMe();
						OnStartTimerCounterChanged();
					}
					return;
				}
			}
			if (adminExist)
			{
				int countdown = gameModeCoop.GetReadyCountdown();
				m_iStartTimerCounter = countdown;
				Replication.BumpMe();
				OnStartTimerCounterChanged();
				m_CallQueue.CallLater(StartTime, 1000, true);
			}
		}
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetPlayerState(int playerId, PS_EPlayableControllerState state)
	{
		m_PlayerStatesMap[playerId] = state;
		m_CallbackHandler.GetOnPlayerStateChanged().Invoke(playerId, state);
		m_eOnPlayerStateChange.Invoke(playerId, state);

		RplId slotId = GetPlayableByPlayer(playerId);
		PS_PlayableContainer container = GetPlayableById(slotId);
		if (container)
			container.GetOnPlayerStateChange().Invoke(state);
	}

	PS_EPlayableControllerState GetPlayerState(int playerId)
	{
		PS_EPlayableControllerState state = PS_EPlayableControllerState.NotReady;
		m_PlayerStatesMap.Find(playerId, state);
		return state;
	}

	// --------------------------------------------------------------------------------------------
	void SetPlayerName(int playerId, string playerName)
	{
		RPC_SetPlayerName(playerId, playerName);
		Rpc(RPC_SetPlayerName, playerId, playerName);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetPlayerName(int playerId, string playerName)
	{
		m_PlayerNamesCached[playerId] = playerName;
		m_CallbackHandler.GetOnPlayerNameUpdated().Invoke(playerId, playerName);
	}

	string GetPlayerName(int playerId)
	{
		return m_PlayerNamesCached.Get(playerId);
	}

	// --------------------------------------------------------------------------------------------
	void SetFactionReady(FactionKey factionKey, int readyValue)
	{
		RPC_SetFactionReady(factionKey, readyValue);
		Rpc(RPC_SetFactionReady, factionKey, readyValue);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetFactionReady(FactionKey factionKey, int readyValue)
	{
		m_FactionReadyMap[factionKey] = readyValue;
		m_CallbackHandler.GetOnFactionReadyStateChanged().Invoke(factionKey, readyValue);
		m_eFactionReadyChanged.Invoke(factionKey, readyValue);

		if (Replication.IsServer())
		{
			if (!readyValue)
				m_bFactionsReadySended = false;

			if (m_bFactionsReadySended)
				return;

			array<int> players = {};
			GetGame().GetPlayerManager().GetPlayers(players);
			m_bFactionsReadySended = true;
			foreach (int playerId : players)
			{
				FactionKey fk = GetPlayerFactionKey(playerId);
				if (fk == "")
					continue;
				int val;
				if (m_FactionReadyMap.Find(fk, val) && val)
					continue;
				m_bFactionsReadySended = false;
				break;
			}
			if (m_bFactionsReadySended)
			{
				SCR_ChatPanelManager chatPanelManager = SCR_ChatPanelManager.GetInstance();
				ChatCommandInvoker invoker = chatPanelManager.GetCommandInvoker("tmsg");
				invoker.Invoke(null, "Factions ready");
			}
		}
	}

	int GetFactionReady(FactionKey factionKey)
	{
		int val;
		m_FactionReadyMap.Find(factionKey, val);
		return val;
	}

	// --------------------------------------------------------------------------------------------
	void SetPlayerPin(int playerId, bool pined)
	{
		RPC_SetPlayerPin(playerId, pined);
		Rpc(RPC_SetPlayerPin, playerId, pined);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetPlayerPin(int playerId, bool pined)
	{
		m_PlayerPinMap[playerId] = pined;
		m_eOnPlayerPinChange.Invoke(playerId, pined);

		RplId slotId = GetPlayableByPlayer(playerId);
		PS_PlayableContainer container = GetPlayableById(slotId);
		if (container)
			container.GetOnPlayerPinChange().Invoke(pined);
	}

	bool GetPlayerPin(int playerId)
	{
		bool pinned;
		m_PlayerPinMap.Find(playerId, pinned);
		return pinned;
	}

	// --------------------------------------------------------------------------------------------
	void RegisterGroupVehicle(RplId rplId, SCR_AIGroup group, IEntity vehicle)
	{
		if (!Replication.IsServer())
			return;
		if (!group.m_PlayersGroup)
		{
			m_CallQueue.Call(RegisterGroupVehicle, rplId, group, vehicle);
			return;
		}

		PS_VehicleData vehicleData = new PS_VehicleData();
		vehicleData.m_RplId = rplId;

		SCR_EditableVehicleComponent editableComp = SCR_EditableVehicleComponent.Cast(vehicle.FindComponent(SCR_EditableVehicleComponent));
		SCR_VehicleFactionAffiliationComponent factionComp = SCR_VehicleFactionAffiliationComponent.Cast(vehicle.FindComponent(SCR_VehicleFactionAffiliationComponent));

		if (editableComp)
		{
			SCR_UIInfo info = editableComp.GetInfo();
			vehicleData.m_Name = info.GetName();
			vehicleData.m_IconPath = info.GetIconPath();
			if (vehicleData.m_IconPath == "")
			{
				vehicleData.m_IconPath = info.GetImageSetPath();
				vehicleData.m_IconSetName = info.GetIconSetName();
			}
		}

		ResourceName prefab = vehicle.GetPrefabData().GetPrefabName();
		if (prefab == "")
		{
			BaseContainer ancestor = vehicle.GetPrefabData().GetPrefab().GetAncestor();
			if (ancestor)
			{
				prefab = ancestor.GetResourceName();
				if (prefab == "")
				{
					BaseContainer ancestor2 = ancestor.GetAncestor();
					if (ancestor2)
						prefab = ancestor2.GetResourceName();
				}
			}
		}
		vehicleData.m_PrefabPath = prefab;

		if (factionComp)
			vehicleData.m_FactionKey = factionComp.GetDefaultFactionKey();

		vehicleData.m_GroupId = group.m_PlayersGroup.GetGroupID();

		RPC_InsertVehicleData(vehicleData);
		Rpc(RPC_InsertVehicleData, vehicleData);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_InsertVehicleData(PS_VehicleData vehicleData)
	{
		m_VehicleMap[vehicleData.m_RplId] = vehicleData;
		m_CallbackHandler.GetOnVehicleInserted().Invoke(vehicleData.m_RplId);
	}

	void UnRegisterGroupVehicle(RplId rplId)
	{
		if (!Replication.IsServer())
			return;
		RPC_UnRegisterGroupVehicle(rplId);
		Rpc(RPC_UnRegisterGroupVehicle, rplId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_UnRegisterGroupVehicle(RplId rplId)
	{
		m_VehicleMap.Remove(rplId);
	}

	void SetPlayableVehicleLocked(RplId vehicleId, bool lock)
	{
		if (!Replication.IsServer())
			return;
		RPC_BroadcastVehicleLocked(vehicleId, lock);
		Rpc(RPC_BroadcastVehicleLocked, vehicleId, lock);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_BroadcastVehicleLocked(RplId vehicleId, bool lock)
	{
		if (m_VehicleMap.Contains(vehicleId))
			m_VehicleMap[vehicleId].m_IsLocked = lock;
	}

	// --------------------------------------------------------------------------------------------
	void OnPlayableDamageStateChanged(RplId slotId, EDamageState damageState)
	{
		if (!m_SlotsMap.Contains(slotId))
			return;
		PS_SlotCharacterData slot = m_SlotsMap[slotId];
		slot.m_eDamageState = damageState;
		RPC_OnSlotDamageStateChanged(slotId, damageState);
		Rpc(RPC_OnSlotDamageStateChanged, slotId, damageState);
		if (damageState == EDamageState.DESTROYED)
			SetSlotDestroyed(slotId, true);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_OnSlotDamageStateChanged(RplId slotId, EDamageState damageState)
	{
		if (m_SlotsMap.Contains(slotId))
		{
			PS_SlotCharacterData sd;
			if (m_SlotsMap.Find(slotId, sd))
				sd.m_eDamageState = damageState;
		}
	}

	void SetSlotDestroyed(RplId slotId, bool isDestroyed = true)
	{
		RPC_SetSlotDestroyed(slotId, isDestroyed);
		Rpc(RPC_SetSlotDestroyed, slotId, isDestroyed);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetSlotDestroyed(RplId slotId, bool isDestroyed)
	{
		if (m_SlotsMap.Contains(slotId))
		{
			PS_SlotCharacterData sd;
			if (m_SlotsMap.Find(slotId, sd))
				sd.m_IsDestroyed = isDestroyed;
			m_CallbackHandler.GetOnSlotDestroyed().Invoke(slotId, isDestroyed);
		}
	}

	void SetSlotLockState(RplId slotId, bool isLocked = true)
	{
		if (!IsSlotAvailable(slotId))
			return;
		RPC_SetSlotLockState(slotId, isLocked);
		Rpc(RPC_SetSlotLockState, slotId, isLocked);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetSlotLockState(RplId slotId, bool isLocked)
	{
		if (m_SlotsMap.Contains(slotId))
		{
			PS_SlotCharacterData sd;
			if (m_SlotsMap.Find(slotId, sd))
				sd.m_IsLocked = isLocked;
			m_CallbackHandler.GetOnSlotLocked().Invoke(slotId, isLocked);
		}
	}

	// --------------------------------------------------------------------------------------------
	void AddDisconnectedPlayerInfo(string guid, RplId slotId, int playerId)
	{
		m_DisconnectedPlayers.Insert(guid, slotId);
		RPC_AddDisconnectedPlayerInfo(playerId);
		Rpc(RPC_AddDisconnectedPlayerInfo, playerId);
		Replication.BumpMe();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_AddDisconnectedPlayerInfo(int playerId)
	{
		m_DisconnectedPlayersClient.Insert(playerId);
	}

	void RemoveDisconnectedPlayerInfo(string guid)
	{
		int playerId;
		m_PlayerGUIDtoIdCached.Find(guid, playerId);
		m_DisconnectedPlayers.Remove(guid);
		RPC_RemoveDisconnectedPlayerInfo(playerId);
		Rpc(RPC_RemoveDisconnectedPlayerInfo, playerId);
		Replication.BumpMe();
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_RemoveDisconnectedPlayerInfo(int playerId)
	{
		m_DisconnectedPlayersClient.RemoveItem(playerId);
	}

	bool FindDisconnectedPlayerByGUID(string guid, out RplId slotId)
	{
		return m_DisconnectedPlayers.Find(guid, slotId);
	}

	string GetPlayerGUIDById(int playerId)
	{
		string guid;
		m_PlayerIdToGuidCached.Find(playerId, guid);
		return guid;
	}

	int GetPlayerIdByGUID(string playerGuid)
	{
		int playerId;
		m_PlayerGUIDtoIdCached.Find(playerGuid, playerId);
		return playerId;
	}

	void SetPlayerInfo(int playerId, string playerName, string playerGuid)
	{
		m_PlayerGUIDtoIdCached[playerGuid] = playerId;
		m_PlayerIdToGuidCached[playerId] = playerGuid;
		SetPlayerName(playerId, playerName);
	}

	bool RemovePlayer(int playerId, string playerGuid, bool forced = false)
	{
		if (!m_DisconnectedPlayers.Contains(playerGuid) && !forced)
			return false;

		m_PlayerGUIDtoIdCached.Remove(playerGuid);
		m_PlayerIdToGuidCached.Remove(playerId);
		m_DisconnectedPlayers.Remove(playerGuid);

		RPC_RemovePlayer(playerId, playerGuid);
		Rpc(RPC_RemovePlayer, playerId, playerGuid);
		return true;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_RemovePlayer(int playerId, string playerGuid)
	{
		RplId slotId;
		FindPlayerSlotById(playerId, slotId);

	{
			PS_SlotCharacterData sd;
			if (m_SlotsMap.Find(slotId, sd))
				sd.m_PlayerId = -1;
		}
		m_PlayerSlotMap.Remove(playerId);
		m_PlayerNamesCached.Remove(playerId);
		m_DisconnectedPlayersClient.RemoveItem(playerId);

		PS_DebugLogger.Log("RPC_RemovePlayer player=" + playerId.ToString() + " slot=" + slotId.ToString());

		m_CallbackHandler.GetOnPlayerRemoved().Invoke(playerId, slotId);
	}

	void UpdatePlayerReconnected(int playerId, string playerGuid)
	{
		int oldPlayerId;
		m_PlayerGUIDtoIdCached.Find(playerGuid, oldPlayerId);
		m_PlayerGUIDtoIdCached.Set(playerGuid, playerId);
		m_PlayerIdToGuidCached.ReplaceKey(oldPlayerId, playerId);

		PS_DebugLogger.LogImportant("UpdatePlayerReconnected newPlayer=" + playerId.ToString() + " oldPlayer=" + oldPlayerId.ToString());

		RPC_UpdatePlayerReconnected(playerId, oldPlayerId);
		Rpc(RPC_UpdatePlayerReconnected, playerId, oldPlayerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_UpdatePlayerReconnected(int playerId, int oldPlayerId)
	{
		if (playerId != oldPlayerId)
		{
			RplId slotId;
			if (m_PlayerSlotMap.Find(oldPlayerId, slotId) && m_SlotsMap.Contains(slotId))
			{
				PS_SlotCharacterData sd;
				if (m_SlotsMap.Find(slotId, sd))
					sd.m_PlayerId = playerId;
			}
			m_PlayerSlotMap.ReplaceKey(oldPlayerId, playerId);
			m_PlayerNamesCached.ReplaceKey(oldPlayerId, playerId);
			
			PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();
			if (vonManager)
				vonManager.UpdatePlayerId(oldPlayerId, playerId);
		}
		GetGame().GetCallqueue().CallLater(InvokePlayerReconnected, 100, false, playerId, oldPlayerId);
	}

	void InvokePlayerReconnected(int playerId, int oldPlayerId)
	{
		m_CallbackHandler.GetOnPlayerReconnected().Invoke(oldPlayerId, playerId);
	}

	void InvokePlayerConnectedOnClients(int playerId)
	{
		Rpc(RPC_InvokePlayerConnectedOnClients, playerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_InvokePlayerConnectedOnClients(int playerId)
	{
		m_CallbackHandler.GetOnPlayerConnected().Invoke(playerId);
	}

	void InvokePlayerDisconnectedOnClients(int playerId, KickCauseCode cause = KickCauseCode.NONE, int timeout = -1)
	{
		Rpc(RPC_InvokePlayerDisconnectedOnClients, playerId, cause, timeout);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_InvokePlayerDisconnectedOnClients(int playerId, KickCauseCode cause, int timeout)
	{
		m_CallbackHandler.GetOnPlayerDisconnected().Invoke(playerId, cause, timeout);
		m_eOnPlayerDisconnected.Invoke(playerId, cause, timeout);
	}

	// --------------------------------------------------------------------------------------------
	void NotifyKick(int playerId)
	{
		Rpc(RPC_NotifyKick, playerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_NotifyKick(int playerId)
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (playerController && playerId == playerController.GetPlayerId())
		{
			SCR_ChatPanelManager chatPanelManager = SCR_ChatPanelManager.GetInstance();
			ChatCommandInvoker invoker = chatPanelManager.GetCommandInvoker("lmsg");
			invoker.Invoke(null, "#PS-Lobby_RoleKick");
		}
	}

	void ForceSwitch(int playerId)
	{
		Rpc(RPC_ForceSwitch, playerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_ForceSwitch(int playerId)
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController || playerController.GetPlayerId() != playerId)
			return;
		s_CurrentPlayableController.SwitchToMenu(SCR_EGameModeState.GAME);
	}

	// --------------------------------------------------------------------------------------------
	bool IsPlayerGroupLeader(int thisPlayerId)
	{
		if (thisPlayerId == -1)
			return false;

		RplId thisSlotId = GetPlayableByPlayer(thisPlayerId);
		if (thisSlotId == RplId.Invalid())
			return false;

		int thisGroupId = GetSlotGroupId(thisSlotId);

		foreach (RplId slotId : m_SlotsSortedCached)
		{
			int playerId;
			if (!FindPlayerIdBySlot(slotId, playerId) || playerId <= 0)
				continue;
			if (playerId == thisPlayerId)
				return true;
			if (GetPlayerFactionKey(playerId) != GetPlayerFactionKey(thisPlayerId))
				continue;
			if (GetSlotGroupId(slotId) != thisGroupId)
				continue;
			return false;
		}
		return true;
	}

	protected bool m_bBulkRemoving;

	void RemoveRedundantUnits()
	{
		if (!Replication.IsServer())
			return;
		
		PS_DebugLogger.LogImportant("RemoveRedundantUnits START slotsCount=" + m_SlotsMap.Count().ToString() + " playerSlotsCount=" + m_PlayerSlotMap.Count().ToString());
		
		// Build set of protected slots (players assigned to them)
		array<RplId> protectedSlots = {};
		foreach (int pid, RplId sid : m_PlayerSlotMap.GetRawMap())
		{
			protectedSlots.Insert(sid);
		}
		
		array<RplId> toRemove = {};
		foreach (RplId slotId, PS_SlotCharacterData slot : m_SlotsMap.GetRawMap())
		{
			bool isProtected = false;
			foreach (RplId ps : protectedSlots)
			{
				if (ps == slotId) { isProtected = true; break; }
			}
			if (!isProtected && m_GameModeCoop.GetRemoveRedundantUnits())
				toRemove.Insert(slotId);
		}

		m_bBulkRemoving = true;

		// Phase 1: Detach entities from AI groups
		foreach (RplId slotId : toRemove)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(slotId));
			if (!rpl)
				continue;
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(rpl.GetEntity());
			if (!character)
				continue;
			AIControlComponent ai = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
			if (ai)
			{
				AIAgent agent = ai.GetControlAIAgent();
				if (agent)
				{
					SCR_AIGroup aiGroup = SCR_AIGroup.Cast(agent.GetParentGroup());
					if (aiGroup)
						aiGroup.RemoveAIEntityFromGroup(character);
				}
			}
		}

		// Phase 2: Queue deferred deletion — one entity per 50ms to avoid engine cascade
		int delay = 0;
		foreach (RplId slotId : toRemove)
		{
			PS_DebugLogger.LogImportant("RemoveRedundantUnits DELETING slot=" + slotId.ToString());
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(slotId));
			IEntity entity = null;
			if (rpl) entity = rpl.GetEntity();
			m_EntityCache.Remove(slotId);
			if (entity)
				m_CallQueue.CallLater(DeleteEntityDeferred, delay, false, entity);
			delay += 50;
		}
		m_bBulkRemoving = false;

		foreach (RplId slotId : toRemove)
		{
			RemoveLobbySlot(slotId);
		}

		foreach (RplId ps : protectedSlots)
		{
			RplComponent protectedRpl = RplComponent.Cast(Replication.FindItem(ps));
			if (!protectedRpl)
			{
				PS_DebugLogger.LogImportant("RemoveRedundantUnits WARN: protected slot entity gone slot=" + ps.ToString());
				IEntity cachedEntity;
				if (m_EntityCache.Find(ps, cachedEntity) && cachedEntity)
				{
					RplComponent cachedRpl = RplComponent.Cast(cachedEntity.FindComponent(RplComponent));
					if (cachedRpl)
						PS_DebugLogger.LogImportant("RemoveRedundantUnits DIAG: cached entity alive, live RplId=" + cachedRpl.Id().ToString() + " stored=" + ps.ToString());
					else
						PS_DebugLogger.LogImportant("RemoveRedundantUnits DIAG: cached entity alive but no RplComponent slot=" + ps.ToString());
				}
			}
		}

		array<RplId> vehiclesToRemove = {};
		foreach (RplId vehicleId, PS_VehicleData vehicleData : m_VehicleMap.GetRawMap())
		{
			if (vehicleData.m_IsLocked)
				vehiclesToRemove.Insert(vehicleId);
		}

		foreach (RplId vehicleId : vehiclesToRemove)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(vehicleId));
			if (rpl)
				SCR_EntityHelper.DeleteEntityAndChildren(rpl.GetEntity());
		}
	}

	void DeleteEntityDeferred(IEntity entity)
	{
		if (entity)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}

	void HolsterWeapons()
	{
		if (!Replication.IsServer())
			return;
		foreach (RplId slotId, PS_SlotCharacterData slot : m_SlotsMap.GetRawMap())
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(slotId));
			if (rpl && rpl.GetEntity())
			{
				PS_PlayableComponent comp = PS_PlayableComponent.Cast(rpl.GetEntity().FindComponent(PS_PlayableComponent));
				if (comp)
					comp.HolsterWeapon();
			}
		}
	}

	// --------------------------------------------------------------------------------------------
	void RespawnCharacter(RplId oldSlotId)
	{
		if (!Replication.IsServer())
			return;

		PS_SlotCharacterData oldSlot = m_SlotsMap[oldSlotId];
		if (!oldSlot)
			return;

		RplComponent oldRpl = RplComponent.Cast(Replication.FindItem(oldSlotId));
		if (!oldRpl)
			return;
		SCR_ChimeraCharacter oldCharacter = SCR_ChimeraCharacter.Cast(oldRpl.GetEntity());
		if (!oldCharacter)
			return;

		Resource resource = Resource.Load(oldCharacter.GetPrefabData().GetPrefabName());
		EntitySpawnParams params = new EntitySpawnParams();
		oldCharacter.GetWorldTransform(params.Transform);
		SCR_ChimeraCharacter newCharacter = SCR_ChimeraCharacter.Cast(GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params));

		PS_PlayableComponent oldComp = PS_PlayableComponent.Cast(oldCharacter.FindComponent(PS_PlayableComponent));
		PS_PlayableComponent newComp = PS_PlayableComponent.Cast(newCharacter.FindComponent(PS_PlayableComponent));
		newComp.SetPlayable(oldComp.GetPlayable());

		GetGame().GetCallqueue().Call(RespawnPlayerAssignGroup, oldComp, newComp);
	}

	void RespawnPlayerAssignGroup(PS_PlayableComponent oldComp, PS_PlayableComponent newComp)
	{
		RplId oldRplId = oldComp.GetRplId(), newRplId = newComp.GetRplId();
		SCR_ChimeraCharacter oldCharacter = SCR_ChimeraCharacter.Cast(oldComp.GetOwner());
		SCR_ChimeraCharacter newCharacter = SCR_ChimeraCharacter.Cast(newComp.GetOwner());

		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		SCR_AIGroup group = groupsManager.FindGroup(GetSlotGroupId(oldRplId));
		if (!group || !group.GetSlave())
			return;
		group = group.GetSlave();

		AIControlComponent control = AIControlComponent.Cast(newCharacter.FindComponent(AIControlComponent));
		if (!control) return;
		AIAgent agent = control.GetControlAIAgent();
		if (!agent) return;

		bool isLeader = group.GetLeaderEntity() == oldCharacter;
		group.AddAIEntityToGroup(newCharacter);
		if (isLeader)
			group.SetNewLeader(agent);
		control.DeactivateAI();

		oldCharacter.GetDamageManager().SetHealthScaled(0);

		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		int playerId = -1;
		bool applyToSlot = FindPlayerIdBySlot(oldRplId, playerId) && gameMode && gameMode.GetState() == SCR_EGameModeState.GAME;

		RPC_ReplaceLobbySlot(oldRplId, newRplId);
		Rpc(RPC_ReplaceLobbySlot, oldRplId, newRplId);

		if (applyToSlot)
			GetGame().GetCallqueue().Call(ApplyPlayable, playerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_ReplaceLobbySlot(RplId oldSlotId, RplId newSlotId)
	{
		int playerId;
		if (FindPlayerIdBySlot(oldSlotId, playerId))
			m_PlayerSlotMap[playerId] = newSlotId;
		else
			playerId = -1;

		FactionKey factionKey = GetSlotFactionKey(oldSlotId);
		m_SlotsMap.ReplaceKey(oldSlotId, newSlotId);
		PS_SlotCharacterData nsd;
		if (m_SlotsMap.Find(newSlotId, nsd))
			nsd.m_IsDestroyed = false;

		m_CallbackHandler.GetOnSlotUpdated().Invoke(oldSlotId, newSlotId);
		SCR_GroupsManagerComponent groupsManagerComponent = SCR_GroupsManagerComponent.GetInstance();
		m_eOnPlayableChangeGroup.Invoke(newSlotId, GetPlayableById(newSlotId), groupsManagerComponent.FindGroup(GetSlotGroupId(newSlotId)));
		BuildSortedSlotsArray();
	}

	// --------------------------------------------------------------------------------------------
	protected void OnPlayerConnected(int playerId)
	{
		m_CallbackHandler.GetOnPlayerConnected().Invoke(playerId);
		m_eOnPlayerConnected.Invoke(playerId);

		RplId slotId = GetPlayableByPlayer(playerId);
		PS_PlayableContainer container = GetPlayableById(slotId);
		if (container)
			container.GetOnPlayerConnected().Invoke(playerId);
	}

	protected void OnPlayerDisconnected(int playerId, KickCauseCode cause = KickCauseCode.NONE, int timeout = -1)
	{
		InvokePlayerDisconnectedOnClients(playerId, cause, timeout);
	}

	protected void OnPlayerRoleChange(int playerId, EPlayerRole roleFlags)
	{
	}

	// --------------------------------------------------------------------------------------------
	void AssignVehicleToGroup(Vehicle vehicle, string groupEntityName, int tryN = 0)
	{
		SCR_AIGroup aiGroup = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByName(groupEntityName));
		if (!aiGroup) return;
		SCR_AIGroup group = aiGroup.GetMaster();
		if (!group)
		{
			if (tryN < 10)
				GetGame().GetCallqueue().CallLater(AssignVehicleToGroup, 100, false, vehicle, groupEntityName, ++tryN);
			return;
		}

		RplId rplId = Replication.FindItemId(vehicle);
		if (!rplId || rplId == RplId.Invalid())
			return;

		RegisterGroupVehicle(rplId, group, vehicle);
	}

	// --------------------------------------------------------------------------------------------
	// BACKWARD-COMPATIBLE WRAPPERS (for existing UI code that hasn't been updated yet)
	// --------------------------------------------------------------------------------------------
	array<PS_PlayableContainer> GetPlayablesSorted()
	{
		array<PS_PlayableContainer> result = {};
		foreach (RplId slotId : m_SlotsSortedCached)
		{
			PS_PlayableContainer container = GetPlayableById(slotId);
			if (container)
				result.Insert(container);
		}
		return result;
	}

	map<RplId, ref PS_PlayableContainer> GetPlayables()
	{
		map<RplId, ref PS_PlayableContainer> result = new map<RplId, ref PS_PlayableContainer>();
		foreach (RplId slotId, PS_SlotCharacterData slot : m_SlotsMap.GetRawMap())
		{
			PS_PlayableContainer container = GetPlayableById(slotId);
			if (container)
				result.Insert(slotId, container);
		}
		return result;
	}

	int GetPlayerByPlayable(RplId slotId)
	{
		int playerId = -1;
		FindPlayerIdBySlot(slotId, playerId);
		return playerId;
	}

	RplId GetPlayableByPlayerRemembered(int playerId)
	{
		return GetPlayableByPlayer(playerId);
	}

	string GetPlayablePrefab(RplId slotId)
	{
		return GetSlotCharacterPrefabPath(slotId);
	}

	string GetPlayableName(RplId slotId)
	{
		return GetSlotName(slotId);
	}

	void SetPlayablePlayerGroupId(RplId playableId, int groupId)
	{
		PS_SlotCharacterData sd;
		if (m_SlotsMap.Find(playableId, sd))
			sd.m_PlayerGroupId = groupId;
	}
};
