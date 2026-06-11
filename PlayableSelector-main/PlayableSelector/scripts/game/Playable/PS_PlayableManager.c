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
	// Non-replicated: confirmed dead code (no callers of GetGroupEntityName in scripts/).
	// Removed [RplProp()] to shrink JIP/reconnect snapshots. Kept as a regular map for
	// potential future use; writes in RPC_InsertLobbySlot still populate it server-side only.
	protected ref ReplicatedBasicMap<int, string> m_GroupEntityNames = new ReplicatedBasicMap<int, string>();
	ref array<RplId> m_SlotsSortedCached = {};

	array<RplId> GetSortedSlotIds()
	{
		if (m_SlotsSortedCached.Count() != m_SlotsMap.Count())
			BuildSortedSlotsArray();
		return m_SlotsSortedCached;
	}
	[RplProp()]
	protected ref ReplicatedClassMap<RplId, ref PS_VehicleData> m_VehicleMap = new ReplicatedClassMap<RplId, ref PS_VehicleData>();
	[RplProp()]
	protected ref ReplicatedBasicMap<FactionKey, int> m_FactionReadyMap = new ReplicatedBasicMap<FactionKey, int>();
	protected ref ReplicatedBasicMap<int, FactionKey> m_PlayerFactionMap = new ReplicatedBasicMap<int, FactionKey>();
	protected ref ReplicatedBasicMap<int, PS_EPlayableControllerState> m_PlayerStatesMap = new ReplicatedBasicMap<int, PS_EPlayableControllerState>();
	protected ref ReplicatedBasicMap<int, bool> m_PlayerPinMap = new ReplicatedBasicMap<int, bool>();
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
		if (FindSlotData(slotId, slot) && slot.m_PlayerId != -1)
		{
			playerId = slot.m_PlayerId;
			return true;
		}
		// Fallback for JIP clients: m_SlotsMap may have stale m_PlayerId because
		// ReplicatedClassMap does not replicate per-field mutations inside class values.
		// Use the replicated m_PlayerSlotMap (playerId -> slotId) as the authoritative
		// source for slot-to-player lookup.
		foreach (int pid, RplId sid : m_PlayerSlotMap.GetRawMap())
		{
			if (sid == slotId)
			{
				playerId = pid;
				return true;
			}
		}
		playerId = -1;
		return false;
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
		foreach (RplId otherSlotId : GetSortedSlotIds())
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
		foreach (RplId otherSlotId : GetSortedSlotIds())
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

		IEntity ent;
		if (FindValidatedSlotEntity(slotId, ent))
		{
			PS_PlayableComponent comp = PS_PlayableComponent.Cast(ent.FindComponent(PS_PlayableComponent));
			if (comp)
				return comp.GetPlayableContainer();
		}

		PS_PlayableContainer container = new PS_PlayableContainer();
		RplId effectiveRplId = slotId;
		if (slotId == RplId.Invalid())
			effectiveRplId = slot.m_RplId;
		if (effectiveRplId == RplId.Invalid())
		{
			return null;
		}
		container.InitFromSlotData(slot, effectiveRplId);
		return container;
	}

	map<FactionKey, ref array<int>> GetTopPlayerInEachGroup(out map<int, int> groupLeaders = null)
	{
		if (!groupLeaders)
			groupLeaders = new map<int, int>();
		else
			groupLeaders.Clear();
		ref map<FactionKey, ref array<int>> result = new map<FactionKey, ref array<int>>();
		foreach (RplId slotId : GetSortedSlotIds())
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
    PS_DebugLogger.LogImportant("PS_PlayableManager OnPostInit START rplMode=" + RplSession.Mode().ToString() + " isServer=" + Replication.IsServer().ToString());
    m_GameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
    m_CallQueue = GetGame().GetCallqueue();
    m_PlayerManager = GetGame().GetPlayerManager();
    m_bRplLoaded = true;
    BuildSortedSlotsArray();
    PS_DebugLogger.LogImportant("PS_PlayableManager OnPostInit slots=" + m_SlotsMap.Count().ToString() + " sorted=" + m_SlotsSortedCached.Count().ToString() + " playerSlots=" + m_PlayerSlotMap.Count().ToString() + " vehicles=" + m_VehicleMap.Count().ToString());

    if (RplSession.Mode() == RplMode.Dedicated)
      ForceGetSessionMaxPlayersCount();

    m_GameModeCoop.GetOnPlayerConnected().Insert(OnPlayerConnected);
    m_GameModeCoop.GetOnPlayerDisconnected().Insert(OnPlayerDisconnected);
    m_GameModeCoop.GetOnPlayerRoleChange().Insert(OnPlayerRoleChange);
    m_CallQueue.Call(LateInit, owner);
    string gameModeStateStr;
    if (m_GameModeCoop)
      gameModeStateStr = m_GameModeCoop.GetState().ToString();
    else
      gameModeStateStr = "NULL_GAMEMODE";
    PS_DebugLogger.LogImportant("PS_PlayableManager OnPostInit END gameModeState=" + gameModeStateStr);
  }

  protected void LateInit(IEntity owner)
  {
    if (RplSession.Mode() == RplMode.Dedicated)
      return;
    m_CurrentPlayerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
    if (!m_CurrentPlayerController)
    {
      PS_DebugLogger.Log("PS_PlayableManager LateInit: awaiting PlayerController, retrying");
      m_CallQueue.Call(LateInit, owner);
      return;
    }
    s_CurrentPlayableController = m_CurrentPlayerController.PS_GetPlayableComponent();
    PS_DebugLogger.LogImportant("PS_PlayableManager LateInit COMPLETE playerId=" + m_CurrentPlayerController.GetPlayerId().ToString() + " hasPlayableController=" + (s_CurrentPlayableController != null).ToString());
  }

	protected void ForceGetSessionMaxPlayersCount()
	{
		DSSession dSSession = GetGame().GetBackendApi().GetDSSession();
		if (dSSession)
		{
			m_iMaxPlayersCount = dSSession.PlayerLimit();
		}
		else
			m_CallQueue.Call(ForceGetSessionMaxPlayersCount);
	}

	// --------------------------------------------------------------------------------------------
	void StartTime()
	{
		m_iStartTimerCounter -= 1;
		BroadcastStartTimerCounter(m_iStartTimerCounter);
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

  void BroadcastStartTimerCounter(int value)
  {
    Rpc(RPC_OnStartTimerCounterChanged, value);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  void RPC_OnStartTimerCounterChanged(int value)
  {
    m_iStartTimerCounter = value;
    m_eOnStartTimerCounterChanged.Invoke(value);
  }

	// --------------------------------------------------------------------------------------------
	// Tries to find a slot entity safely, with validation against stale cache.
	// Returns: true if a valid entity was found (output in outSlotEntity).
	// --------------------------------------------------------------------------------------------
	bool FindValidatedSlotEntity(RplId slotId, out IEntity outSlotEntity)
	{
		outSlotEntity = null;

		// Tier 1: Standard RPL lookup
		IEntity ent = IEntity.Cast(Replication.FindItem(slotId));
		if (ent)
		{
			// Refresh cache on successful RPL lookup
			m_EntityCache[slotId] = ent;
			outSlotEntity = ent;
			return true;
		}

		// Tier 2: Cache fallback — validated via RplComponent comparison
		IEntity cached;
		if (m_EntityCache.Find(slotId, cached) && cached)
		{
			RplComponent rpl = RplComponent.Cast(cached.FindComponent(RplComponent));
			if (rpl && rpl.Id() == slotId)
			{
				//PS_DebugLogger.LogImportant("FindValidatedSlotEntity Tier2 HIT slot=" + slotId.ToString());
				outSlotEntity = cached;
				return true;
			}
			else
			{
				//PS_DebugLogger.LogImportant("FindValidatedSlotEntity Tier2 STALE removing slot=" + slotId.ToString());
				// Stale cache — remove so we don't try again
				m_EntityCache.Remove(slotId);
			}
		}

		// Tier 3: Slot data cached entity fallback (last resort, for Workbench peer mode)
		// Does NOT call Replication.FindItem, so it works even when RPL hasn't indexed the entity yet.
		PS_SlotCharacterData slotData;
		if (m_SlotsMap.Find(slotId, slotData) && slotData.m_CachedEntity)
		{
			RplComponent verifyRpl = RplComponent.Cast(slotData.m_CachedEntity.FindComponent(RplComponent));
			if (verifyRpl && verifyRpl.Id() == slotId)
			{
				//PS_DebugLogger.LogImportant("FindValidatedSlotEntity Tier3 HIT slot=" + slotId.ToString());
				m_EntityCache[slotId] = slotData.m_CachedEntity;
				outSlotEntity = slotData.m_CachedEntity;
				return true;
			}
		}

		//PS_DebugLogger.LogImportant("FindValidatedSlotEntity ALL MISS slot=" + slotId.ToString());
		return false;
	}

	// --------------------------------------------------------------------------------------------
  void ApplyPlayable(int playerId)
  {
    if (!Replication.IsServer())
      return;
    SCR_PlayerController playerController = SCR_PlayerController.Cast(m_PlayerManager.GetPlayerController(playerId));
    if (!playerController)
    {
      PS_DebugLogger.LogError("ApplyPlayable: playerController NULL for playerId=" + playerId.ToString(), playerId);
      return;
    }
    PS_PlayableControllerComponent playableController = playerController.PS_GetPlayableComponent();

    PS_EPlayableControllerState prevState = GetPlayerState(playerId);
    RplId prevSlotId = GetPlayableByPlayer(playerId);
    PS_DebugLogger.Log("ApplyPlayable SRV START playerId=" + playerId.ToString() + " prevState=" + typename.EnumToString(PS_EPlayableControllerState, prevState) + " prevSlot=" + prevSlotId.ToString(), playerId);

    SetPlayerState(playerId, PS_EPlayableControllerState.Playing);

    RplId slotId = GetPlayableByPlayer(playerId);
    PS_DebugLogger.Log("ApplyPlayable SRV slotId=" + slotId.ToString() + " playerId=" + playerId.ToString(), playerId);

		// Echo Lobby pattern: destroyed slot — player stays on slot, spectator handled by HandlePlayerKilled.
		if (slotId != RplId.Invalid() && IsSlotCharacterDestroyed(slotId))
		{
			PS_DebugLogger.Log("ApplyPlayable BRANCH: slot destroyed (Echo Lobby pattern) playerId=" + playerId.ToString(), playerId);
			PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();
			if (vonManager)
				vonManager.SetPlayerToChannel(playerId, "");
			return;
		}

		IEntity entity;
		if (slotId == RplId.Invalid())
		{
			PS_DebugLogger.Log("ApplyPlayable BRANCH: slot INVALID, switching to spectator playerId=" + playerId.ToString(), playerId);
			SCR_GroupsManagerComponent groupsManagerComponent = SCR_GroupsManagerComponent.GetInstance();
			SCR_AIGroup currentGroup = groupsManagerComponent.GetPlayerGroup(playerId);
			if (currentGroup)
				currentGroup.RemovePlayer(playerId);
			SetPlayerFactionKey(playerId, "");

			PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();
			if (vonManager)
				vonManager.SetPlayerToChannel(playerId, "");

			// Capture death position from current controlled entity before we change it
			IEntity currentControlledEntity = playerController.GetControlledEntity();
			vector deathPos = "0 0 0";
			if (currentControlledEntity)
				deathPos = currentControlledEntity.GetOrigin();

			entity = playableController.GetInitialEntity();
			if (!entity)
			{
				Resource resource = Resource.Load("{ADDE38E4119816AB}Prefabs/InitialPlayer_Version2.et");
				EntitySpawnParams params = new EntitySpawnParams();
				entity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
				playableController.SetInitialEntity(entity);
			}
			playerController.SetInitialMainEntity(entity);
			playableController.SwitchToObserverServer(deathPos);
			return;
		}

		if (!m_SlotsMap.Contains(slotId))
		{
			PS_DebugLogger.Log("ApplyPlayable FAIL: slot not in map slotId=" + slotId.ToString(), playerId);
			return;
		}

		PS_SlotCharacterData slotData = m_SlotsMap[slotId];
		IEntity slotEntity;
		bool foundEntity = FindValidatedSlotEntity(slotId, slotEntity);

		if (!foundEntity)
		{
			PS_DebugLogger.Log("ApplyPlayable: entity not found for slotId=" + slotId.ToString() + " retrying in 1s", playerId);
			m_CallQueue.CallLater(RetryApplyPlayable, 1000, false, playerId, slotId, 0);
			return;
		}			PS_DebugLogger.Log("ApplyPlayable: entity FOUND for slotId=" + slotId.ToString() + " playerId=" + playerId.ToString(), playerId);

		IEntity defaultEntity = playableController.GetInitialEntity();
		if (defaultEntity)
		{
			PS_DebugLogger.Log("ApplyPlayable defaultEntity EXISTS, deleting playerId=" + playerId.ToString(), playerId);
			SCR_EntityHelper.DeleteEntityAndChildren(defaultEntity);
		}
		else
		{
			PS_DebugLogger.Log("ApplyPlayable defaultEntity NULL playerId=" + playerId.ToString(), playerId);
		}

		playerController.SetInitialMainEntity(slotEntity);
			PS_DebugLogger.Log("ApplyPlayable SetInitialMainEntity done, calling ChangeGroup", playerId);

		SCR_ChimeraCharacter playableCharacter = SCR_ChimeraCharacter.Cast(slotEntity);
		if (!playableCharacter)
			return;
		SCR_Faction faction = SCR_Faction.Cast(playableCharacter.GetFaction());
		SetPlayerFactionKey(playerId, faction.GetFactionKey());

		m_CallQueue.CallLater(ChangeGroup, 0, false, playerId, slotId);
		
		PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();
		if (vonManager)
			vonManager.SetPlayerToChannel(playerId, "");
		
			PS_DebugLogger.Log("ApplyPlayable SUCCESS player=" + playerId.ToString() + " slot=" + slotId.ToString(), playerId);
	}

	protected void RetryApplyPlayable(int playerId, RplId slotId, int attempt)
	{
		if (!m_SlotsMap.Contains(slotId))
		{
			PS_DebugLogger.Log("RetryApplyPlayable FAIL: slot removed from map slotId=" + slotId.ToString(), playerId);
			return;
		}
		
		IEntity slotEntity;
		bool found = FindValidatedSlotEntity(slotId, slotEntity);
		if (found)
		{
			PS_DebugLogger.Log("RetryApplyPlayable SUCCESS on attempt=" + attempt.ToString() + " slotId=" + slotId.ToString(), playerId);
			ApplyPlayable(playerId);
			return;
		}    if (attempt >= 5)
    {
      PS_DebugLogger.LogError("RetryApplyPlayable GAVE UP after 5 attempts, marking slot destroyed slotId=" + slotId.ToString(), playerId);
      SetSlotDestroyed(slotId, true);
      m_CallQueue.Remove(RetryApplyPlayable);
      ApplyPlayable(playerId);
      return;
    }
    
    m_CallQueue.CallLater(RetryApplyPlayable, 1000, false, playerId, slotId, attempt + 1);
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
		{
			vonManager.InitChannelIfNeeded(vonManager.GetFactionChannelKey(slot.m_FactionKey));
			// Also pre-create the group room so it's available in lobby/briefing
			// without waiting for EnsureAllFactionAndGroupRoomsExist on state transition.
			if (encodedCallsign > 0)
				vonManager.InitChannelIfNeeded(vonManager.BuildChannelKey(slot.m_FactionKey, encodedCallsign.ToString()));
		}
	}

  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  protected void RPC_InsertLobbySlot(PS_SlotCharacterData slot, int aiGroupId, int joinableGroupId, int encodedCallsign, string groupEntityName)
  {
    RplId rplId = slot.m_RplId;
    PS_DebugLogger.LogImportant("RPC_InsertLobbySlot slot=" + rplId.ToString() + " aiGroupId=" + aiGroupId.ToString() + " joinableGroupId=" + joinableGroupId.ToString() + " name=" + slot.m_sName + " faction=" + slot.m_FactionKey + " sortedBefore=" + m_SlotsSortedCached.Count().ToString());

    m_SlotsMap[rplId] = slot;
    m_AIToPlayerGroupMap[aiGroupId] = joinableGroupId;

    IEntity cachedEnt = IEntity.Cast(Replication.FindItem(rplId));
    if (cachedEnt)
      m_EntityCache[rplId] = cachedEnt;

    if (!m_GroupCallsignsMap.Contains(joinableGroupId))
    {
      m_GroupCallsignsMap[joinableGroupId] = encodedCallsign;
      m_GroupEntityNames[joinableGroupId] = groupEntityName;
    }

    if (!m_FactionReadyMap.Contains(slot.m_FactionKey))
      m_FactionReadyMap.Insert(slot.m_FactionKey, 0);

    BuildSortedSlotsArray();
    PS_DebugLogger.LogImportant("RPC_InsertLobbySlot DONE sortedAfter=" + m_SlotsSortedCached.Count().ToString() + " totalSlots=" + m_SlotsMap.Count().ToString());
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
		if (slotId == RplId.Invalid())
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
    if (slotId == RplId.Invalid())
    {
      PS_DebugLogger.LogError("RPC_RemoveLobbySlot: invalid slotId");
      return;
    }

    // JIP-safe: slot may not be replicated yet; retry later
    if (!m_SlotsMap.Contains(slotId))
    {
      PS_DebugLogger.Log("RPC_RemoveLobbySlot slot=" + slotId.ToString() + " desync, retrying");
      GetGame().GetCallqueue().CallLater(RetryRPC_RemoveLobbySlot, 500, false, slotId, 0);
      return;
    }

    int playerId;
    if (FindPlayerIdBySlot(slotId, playerId))
      m_PlayerSlotMap.Remove(playerId);
    else
      playerId = -1;
    FactionKey factionKey = GetSlotFactionKey(slotId);

    PS_DebugLogger.LogImportant("RPC_RemoveLobbySlot slot=" + slotId.ToString() + " faction=" + factionKey + " prevPlayer=" + playerId.ToString() + " slotsBefore=" + m_SlotsMap.Count().ToString());

    m_SlotsMap.Remove(slotId);
    m_EntityCache.Remove(slotId);
    m_CallbackHandler.GetOnSlotRemoved().Invoke(slotId, factionKey, playerId);
    m_eOnPlayableUnregistered.Invoke(slotId, GetPlayableById(slotId));

    PS_DebugLogger.LogImportant("RPC_RemoveLobbySlot DONE slotsAfter=" + m_SlotsMap.Count().ToString());

    BuildSortedSlotsArray();
  }

  // JIP-safe retry for RPC_RemoveLobbySlot
  protected void RetryRPC_RemoveLobbySlot(RplId slotId, int attempt)
  {
    if (slotId == RplId.Invalid())
      return;
    if (m_SlotsMap.Contains(slotId))
    {
      RPC_RemoveLobbySlot(slotId);
      return;
    }
    if (attempt >= 5)
    {
      PS_DebugLogger.LogError("RetryRPC_RemoveLobbySlot GAVE UP after 5 attempts slot=" + slotId.ToString());
      return;
    }
    GetGame().GetCallqueue().CallLater(RetryRPC_RemoveLobbySlot, 500, false, slotId, attempt + 1);
  }

	protected int GetOrCreatePlayerGroup(RplId rplId, SCR_AIGroup group, out int encodedCallsign)
	{
		int aiGroupId = RplComponent.Cast(group.FindComponent(RplComponent)).Id();

		// Reuse existing player group if one was already created for this AI group.
		// All slots from the same AI group share a single player group, so the lobby
		// squad header shows one group per squad (e.g. "Dinamo-11") with all roles under it.
		int playerGroupId;
		if (m_AIToPlayerGroupMap.Find(aiGroupId, playerGroupId))
		{
			encodedCallsign = m_GroupCallsignsMap[playerGroupId];
			PS_DebugLogger.Log("GetOrCreatePlayerGroup REUSING slot=" + rplId.ToString() + " aiGroupId=" + aiGroupId.ToString() + " playerGroupId=" + playerGroupId.ToString());
			return playerGroupId;
		}

		// Prefer the PS_GroupCallsignAssigner component on the AI group when present
		// (the World Editor "Assign callsigns" tool bakes C/P/S into it). Fall back to
		// the vanilla SCR_CallsignGroupComponent only if no manual assignment exists.
		// This mirrors ReforgerLobby17's UpdateGroupCallsign precedence so the callsign
		// sequence in the lobby matches for missions authored with the old editor tool.
		int company, platoon, squad;
		PS_GroupCallsignAssigner groupCallsignAssigner = PS_GroupCallsignAssigner.Cast(group.FindComponent(PS_GroupCallsignAssigner));
		if (groupCallsignAssigner)
		{
			groupCallsignAssigner.GetCallsign(company, platoon, squad);
		}
		else
		{
			SCR_CallsignGroupComponent groupCallsign = SCR_CallsignGroupComponent.Cast(group.FindComponent(SCR_CallsignGroupComponent));
			groupCallsign.GetCallsignIndexes(company, platoon, squad);
		}
		encodedCallsign = 1000000 * company + 1000 * platoon + squad;

		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		Faction groupFaction = group.GetFaction();

		PS_DebugLogger.LogImportant("GetOrCreatePlayerGroup CREATING slot=" + rplId.ToString() + " aiGroupId=" + aiGroupId.ToString() + " callsign=" + encodedCallsign.ToString());

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

		m_AIToPlayerGroupMap[aiGroupId] = joinableGroup.GetGroupID();

		PS_DebugLogger.LogImportant("GetOrCreatePlayerGroup DONE slot=" + rplId.ToString() + " playerGroupId=" + joinableGroup.GetGroupID().ToString());
		return joinableGroup.GetGroupID();
	}

	array<RplId> BuildSortedSlotsArray()
	{
		// Step 1: group slots by faction
		map<FactionKey, ref array<RplId>> factionSlots = new map<FactionKey, ref array<RplId>>();
		foreach (RplId slotId, PS_SlotCharacterData slot : m_SlotsMap.GetRawMap())
		{
			FactionKey fk = slot.m_FactionKey;
			if (!factionSlots.Contains(fk))
				factionSlots[fk] = new array<RplId>();
			factionSlots[fk].Insert(slotId);
		}

		// Step 2: sort each faction's slots by callsign -> rank -> rplId
		foreach (FactionKey fk, array<RplId> slots : factionSlots)
		{
			for (int i = 0; i < slots.Count(); i++)
			{
				RplId slotId = slots[i];
				PS_SlotCharacterData slot;
				m_SlotsMap.Find(slotId, slot);
				int groupId = slot.m_PlayerGroupId;
				int callsign = m_GroupCallsignsMap[groupId];

				int insertIndex = i;
				for (int j = 0; j < i; j++)
				{
					RplId otherSlotId = slots[j];
					PS_SlotCharacterData otherSlot;
					m_SlotsMap.Find(otherSlotId, otherSlot);
					int otherGroupId = otherSlot.m_PlayerGroupId;
					int otherCallsign = m_GroupCallsignsMap[otherGroupId];

					bool callsignEquival = otherCallsign == callsign;
					bool callsignGreater = otherCallsign > callsign;
					bool rankGreater = slot.m_eCharacterRank > otherSlot.m_eCharacterRank;
					bool rankEquival = slot.m_eCharacterRank == otherSlot.m_eCharacterRank;
					bool rplIdGreater = otherSlotId > slotId;

					if ((((rplIdGreater && rankEquival) || rankGreater) && callsignEquival) || callsignGreater)
					{
						insertIndex = j;
						break;
					}
				}
				if (insertIndex != i)
				{
					slots.Remove(i);
					slots.InsertAt(slotId, insertIndex);
				}
			}
		}

		// Step 3: build sorted faction list (more slots first, then alphabetical)
		array<FactionKey> sortedFactions = {};
		foreach (FactionKey fk, array<RplId> slots : factionSlots)
		{
			int insertIndex = sortedFactions.Count();
			for (int i = 0; i < sortedFactions.Count(); i++)
			{
				FactionKey otherFk = sortedFactions[i];
				int slotCount = factionSlots[fk].Count();
				int otherSlotCount = factionSlots[otherFk].Count();
				if (slotCount > otherSlotCount)
				{
					insertIndex = i;
					break;
				}
				if (slotCount == otherSlotCount && fk < otherFk)
				{
					insertIndex = i;
					break;
				}
			}
			sortedFactions.InsertAt(fk, insertIndex);
		}

		// Step 4: assemble final sorted array
		array<RplId> slotsSorted = {};
		foreach (FactionKey fk : sortedFactions)
		{
			foreach (RplId slotId : factionSlots[fk])
			{
				slotsSorted.Insert(slotId);
			}
		}

		m_SlotsSortedCached = slotsSorted;
		if (slotsSorted.Count() > 0)
			PS_DebugLogger.LogImportant("BuildSortedSlotsArray sorted=" + slotsSorted.Count().ToString() + " firstRplId=" + slotsSorted[0].ToString());
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
    {
      PS_DebugLogger.LogError("SetPlayerToSlot SRV REJECTED slot destroyed slot=" + slotId.ToString(), playerId);
      return;
    }

		// Check faction balance (vacating bypass)
		if (slotId != RplId.Invalid() && playerId >= 0)
		{
			PS_GameModeCoop gameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
			FactionKey playerFaction = GetPlayerFactionKey(playerId);
			PS_SlotCharacterData slot;
			if (FindSlotData(slotId, slot) && gameModeCoop)
			{
				if (!gameModeCoop.CanJoinFaction(slot.m_FactionKey, playerFaction))
				{
					PS_DebugLogger.Log("SetPlayerToSlot SRV REJECTED faction balance slot=" + slotId.ToString(), playerId);
					return;
				}
			}
		}

    RplId prevSlotId;
    FindPlayerSlotById(playerId, prevSlotId);
    FactionKey oldFactionKey = GetPlayerFactionKey(playerId);
    PS_EPlayableControllerState prevState = GetPlayerState(playerId);
    SCR_EGameModeState gameModeState = PS_GameModeCoop.Cast(GetGame().GetGameMode()).GetState();
    PS_DebugLogger.Log("SetPlayerToSlot SRV player=" + playerId.ToString() + " slot=" + slotId.ToString() + " prevSlot=" + prevSlotId.ToString() + " oldFaction=" + oldFactionKey + " playerState=" + typename.EnumToString(PS_EPlayableControllerState, prevState) + " gameModeState=" + typename.EnumToString(SCR_EGameModeState, gameModeState), playerId);

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

      if (gameModeState == SCR_EGameModeState.GAME)
      {
        PS_DebugLogger.LogImportant("SetPlayerToSlot SRV state==GAME, scheduling ApplyPlayable slotId=" + slotId.ToString(), playerId);
        m_CallQueue.CallLater(ApplyPlayable, 200, false, playerId);
      }
    }
    else
    {
      PS_DebugLogger.LogError("SetPlayerToSlot SRV slot data NOT FOUND for slotId=" + slotId.ToString(), playerId);
    }
  }

  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  protected void RPC_SetPlayerToSlot(RplId slotId, int playerId)
  {
    PS_DebugLogger.LogImportant("RPC_SetPlayerToSlot START player=" + playerId.ToString() + " slot=" + slotId.ToString() + " slotMapContains=" + m_SlotsMap.Contains(slotId).ToString() + " slotMapCount=" + m_SlotsMap.Count().ToString() + " playerSlotMapCount=" + m_PlayerSlotMap.Count().ToString());

    RplId prevSlotId;
    if (!m_PlayerSlotMap.Find(playerId, prevSlotId))
      prevSlotId = RplId.Invalid();

    if (prevSlotId != RplId.Invalid() && prevSlotId != slotId && !IsSlotCharacterDestroyed(prevSlotId))
    {
      PS_SlotCharacterData prevSlot;
      if (m_SlotsMap.Find(prevSlotId, prevSlot))
      {
        prevSlot.m_PlayerId = -1;
        PS_DebugLogger.Log("RPC_SetPlayerToSlot cleared prevSlot=" + prevSlotId.ToString() + " playerId=-1");
      }
    }

    if (slotId != RplId.Invalid())
    {
      PS_SlotCharacterData newSlot;
      if (m_SlotsMap.Find(slotId, newSlot))
      {
        newSlot.m_PlayerId = playerId;
        PS_DebugLogger.LogImportant("RPC_SetPlayerToSlot WRITE slot=" + slotId.ToString() + " storedPlayerId=" + newSlot.m_PlayerId.ToString(), playerId);
      }
      else
      {
        // Fix #4: Slot not yet in m_SlotsMap — JIP client received RPC before RplProp replication
        // Store in playerSlotMap anyway; when slot replicates, it will carry the correct playerId
        PS_DebugLogger.LogError("RPC_SetPlayerToSlot slot=" + slotId.ToString() + " NOT FOUND in m_SlotsMap! JIP desync — scheduling retry", playerId);
        m_PlayerSlotMap[playerId] = slotId;
        m_CallbackHandler.GetOnPlayerSetOnSlot().Invoke(playerId, prevSlotId, slotId);
        m_eOnPlayerPlayableChange.Invoke(playerId, slotId);
        GetGame().GetCallqueue().CallLater(RetrySetPlayerToSlot, 500, false, slotId, playerId, 0);
        return;
      }
    }

    if (slotId == RplId.Invalid())
      m_PlayerSlotMap.Remove(playerId);
    else
      m_PlayerSlotMap[playerId] = slotId;

    PS_DebugLogger.LogImportant("RPC_SetPlayerToSlot DONE player=" + playerId.ToString() + " prevSlot=" + prevSlotId.ToString() + " newSlot=" + slotId.ToString() + " playerSlotMapCount=" + m_PlayerSlotMap.Count().ToString(), playerId);

    m_CallbackHandler.GetOnPlayerSetOnSlot().Invoke(playerId, prevSlotId, slotId);
    m_eOnPlayerPlayableChange.Invoke(playerId, slotId);
  }

  // Fix #4: Retry applying playerId to slot data when m_SlotsMap wasn't replicated yet
  protected void RetrySetPlayerToSlot(RplId slotId, int playerId, int attempt)
  {
    if (slotId == RplId.Invalid())
      return;

    if (m_SlotsMap.Contains(slotId))
    {
      PS_SlotCharacterData slot;
      if (m_SlotsMap.Find(slotId, slot))
      {
        slot.m_PlayerId = playerId;
        PS_DebugLogger.LogImportant("RetrySetPlayerToSlot SUCCESS on attempt=" + attempt.ToString() + " slot=" + slotId.ToString() + " playerId=" + playerId.ToString(), playerId);
      }
      return;
    }

    if (attempt >= 5)
    {
      PS_DebugLogger.LogError("RetrySetPlayerToSlot GAVE UP after 5 attempts slot=" + slotId.ToString() + " playerId=" + playerId.ToString(), playerId);
      return;
    }

    PS_DebugLogger.Log("RetrySetPlayerToSlot attempt=" + attempt.ToString() + " slot=" + slotId.ToString() + " still not in m_SlotsMap, retrying", playerId);
    GetGame().GetCallqueue().CallLater(RetrySetPlayerToSlot, 500, false, slotId, playerId, attempt + 1);
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

		PS_VoNChannelsManager vonManager = PS_VoNChannelsManager.GetInstance();
		if (vonManager)
			vonManager.MoveToRoom(playerIdToKick, "", "#PS-VoNRoom_Global");
	}

  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  protected void RPC_KickPlayerFromSlot(RplId slotId)
  {
    PS_DebugLogger.LogImportant("RPC_KickPlayerFromSlot slot=" + slotId.ToString());
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
      PS_DebugLogger.LogImportant("RPC_KickPlayerFromSlot DONE player=" + playerId.ToString(), playerId);
    }
    else
    {
      PS_DebugLogger.LogError("RPC_KickPlayerFromSlot slot=" + slotId.ToString() + " NOT FOUND in m_SlotsMap");
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
    PS_DebugLogger.LogImportant("SetPlayerFactionKey SRV player=" + playerId.ToString() + " old=" + oldKey + " new=" + factionKey, playerId);

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
  }

  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  protected void RPC_SetPlayerFactionKey(int playerId, FactionKey factionKey, FactionKey oldKey)
  {
    PS_DebugLogger.LogImportant("RPC_SetPlayerFactionKey CLI player=" + playerId.ToString() + " old=" + oldKey + " new=" + factionKey + " isServer=" + Replication.IsServer().ToString(), playerId);

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
    PS_EPlayableControllerState oldState = GetPlayerState(playerId);
    PS_DebugLogger.LogImportant("SetPlayerState SRV player=" + playerId.ToString() + " oldState=" + typename.EnumToString(PS_EPlayableControllerState, oldState) + " newState=" + typename.EnumToString(PS_EPlayableControllerState, state), playerId);

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
						BroadcastStartTimerCounter(-1);
					}
					return;
				}
			}
			if (adminExist)
			{
				int countdown = gameModeCoop.GetReadyCountdown();
				m_iStartTimerCounter = countdown;
				BroadcastStartTimerCounter(countdown);
				m_CallQueue.CallLater(StartTime, 1000, true);
			}
		}
	}
  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  void RPC_SetPlayerState(int playerId, PS_EPlayableControllerState state)
  {
    PS_EPlayableControllerState oldState = PS_EPlayableControllerState.NotReady;
    m_PlayerStatesMap.Find(playerId, oldState);
    PS_DebugLogger.LogImportant("RPC_SetPlayerState CLI player=" + playerId.ToString() + " oldState=" + typename.EnumToString(PS_EPlayableControllerState, oldState) + " newState=" + typename.EnumToString(PS_EPlayableControllerState, state), playerId);
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

	// Getters for JIP sync (maps are no longer [RplProp])
	ReplicatedBasicMap<int, FactionKey> GetPlayerFactionMap() { return m_PlayerFactionMap; }
	ReplicatedBasicMap<int, PS_EPlayableControllerState> GetPlayerStatesMap() { return m_PlayerStatesMap; }
	ReplicatedBasicMap<int, bool> GetPlayerPinMap() { return m_PlayerPinMap; }
	array<int> GetDisconnectedPlayersClient() { return m_DisconnectedPlayersClient; }
	ReplicatedBasicMap<int, string> GetPlayerNamesCached() { return m_PlayerNamesCached; }

	// --------------------------------------------------------------------------------------------
	void SetPlayerName(int playerId, string playerName)
	{
		RPC_SetPlayerName(playerId, playerName);
		Rpc(RPC_SetPlayerName, playerId, playerName);
	}
  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  void RPC_SetPlayerName(int playerId, string playerName)
  {
    PS_DebugLogger.Log("RPC_SetPlayerName player=" + playerId.ToString() + " name=" + playerName, playerId);
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
    PS_DebugLogger.LogImportant("RPC_SetFactionReady faction=" + factionKey + " readyValue=" + readyValue.ToString());
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
    PS_DebugLogger.LogImportant("SetPlayerPin SRV player=" + playerId.ToString() + " pinned=" + pined.ToString(), playerId);
    RPC_SetPlayerPin(playerId, pined);
    Rpc(RPC_SetPlayerPin, playerId, pined);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  void RPC_SetPlayerPin(int playerId, bool pined)
  {
    PS_DebugLogger.LogImportant("RPC_SetPlayerPin player=" + playerId.ToString() + " pinned=" + pined.ToString(), playerId);
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
    PS_DebugLogger.LogImportant("RPC_InsertVehicleData rplId=" + vehicleData.m_RplId.ToString() + " name=" + vehicleData.m_Name + " groupId=" + vehicleData.m_GroupId.ToString());
    m_VehicleMap[vehicleData.m_RplId] = vehicleData;
    m_CallbackHandler.GetOnVehicleInserted().Invoke(vehicleData.m_RplId);
  }

  void UnRegisterGroupVehicle(RplId rplId)
  {
    if (!Replication.IsServer())
      return;
    PS_DebugLogger.LogImportant("UnRegisterGroupVehicle rplId=" + rplId.ToString());
    RPC_UnRegisterGroupVehicle(rplId);
    Rpc(RPC_UnRegisterGroupVehicle, rplId);
  }

  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  protected void RPC_UnRegisterGroupVehicle(RplId rplId)
  {
    PS_DebugLogger.LogImportant("RPC_UnRegisterGroupVehicle rplId=" + rplId.ToString());
    m_VehicleMap.Remove(rplId);
  }

  void SetPlayableVehicleLocked(RplId vehicleId, bool lock)
  {
    if (!Replication.IsServer())
      return;
    PS_DebugLogger.LogImportant("SetPlayableVehicleLocked vehicle=" + vehicleId.ToString() + " lock=" + lock.ToString());
    RPC_BroadcastVehicleLocked(vehicleId, lock);
    Rpc(RPC_BroadcastVehicleLocked, vehicleId, lock);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  void RPC_BroadcastVehicleLocked(RplId vehicleId, bool lock)
  {
    PS_DebugLogger.Log("RPC_BroadcastVehicleLocked vehicle=" + vehicleId.ToString() + " lock=" + lock.ToString());
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
		if (damageState == EDamageState.DESTROYED)
			slot.m_IsDestroyed = true;
		RPC_OnSlotDamageStateChanged(slotId, damageState);
		Rpc(RPC_OnSlotDamageStateChanged, slotId, damageState);
	}
  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  void RPC_OnSlotDamageStateChanged(RplId slotId, EDamageState damageState)
  {
    PS_DebugLogger.LogImportant("RPC_OnSlotDamageStateChanged slot=" + slotId.ToString() + " damageState=" + typename.EnumToString(EDamageState, damageState));
    if (m_SlotsMap.Contains(slotId))
    {
      PS_SlotCharacterData sd;
      if (m_SlotsMap.Find(slotId, sd))
      {
        sd.m_eDamageState = damageState;
        if (damageState == EDamageState.DESTROYED)
        {
          sd.m_IsDestroyed = true;
          m_CallbackHandler.GetOnSlotDestroyed().Invoke(slotId, true);
        }

        // BUGFIX: Notify listeners via callback so the Alive Players spectator
        // selectors update dead/alive display. Uses a callback (not GetPlayableById)
        // because GetPlayableById creates a brand-new container via InitFromSlotData
        // when the entity isn't replicated — that new container has zero subscribers
        // and the real selectors (created during InitList) never get notified.
        // The callback lets PS_AlivePlayerList look up the selector by slotId directly.
        m_CallbackHandler.GetOnSlotDamageStateChanged().Invoke(slotId, damageState);
      }
    }
    else
    {
      PS_DebugLogger.LogError("RPC_OnSlotDamageStateChanged slot=" + slotId.ToString() + " NOT FOUND in m_SlotsMap");
    }
  }

  void SetSlotDestroyed(RplId slotId, bool isDestroyed = true)
  {
    PS_DebugLogger.LogImportant("SetSlotDestroyed slot=" + slotId.ToString() + " isDestroyed=" + isDestroyed.ToString());
    RPC_SetSlotDestroyed(slotId, isDestroyed);
    Rpc(RPC_SetSlotDestroyed, slotId, isDestroyed);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  void RPC_SetSlotDestroyed(RplId slotId, bool isDestroyed)
  {
    PS_DebugLogger.LogImportant("RPC_SetSlotDestroyed slot=" + slotId.ToString() + " isDestroyed=" + isDestroyed.ToString());
    if (m_SlotsMap.Contains(slotId))
    {
      PS_SlotCharacterData sd;
      if (m_SlotsMap.Find(slotId, sd))
        sd.m_IsDestroyed = isDestroyed;
      m_CallbackHandler.GetOnSlotDestroyed().Invoke(slotId, isDestroyed);
    }
    else
    {
      PS_DebugLogger.LogError("RPC_SetSlotDestroyed slot=" + slotId.ToString() + " NOT FOUND in m_SlotsMap");
    }
  }

  void SetSlotLockState(RplId slotId, bool isLocked = true)
  {
    if (!IsSlotAvailable(slotId))
      return;
    PS_DebugLogger.Log("SetSlotLockState slot=" + slotId.ToString() + " isLocked=" + isLocked.ToString());
    RPC_SetSlotLockState(slotId, isLocked);
    Rpc(RPC_SetSlotLockState, slotId, isLocked);
  }
  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  void RPC_SetSlotLockState(RplId slotId, bool isLocked)
  {
    PS_DebugLogger.Log("RPC_SetSlotLockState slot=" + slotId.ToString() + " isLocked=" + isLocked.ToString());
    if (m_SlotsMap.Contains(slotId))
    {
      PS_SlotCharacterData sd;
      if (m_SlotsMap.Find(slotId, sd))
        sd.m_IsLocked = isLocked;
      m_CallbackHandler.GetOnSlotLocked().Invoke(slotId, isLocked);
    }
    else
    {
      PS_DebugLogger.LogError("RPC_SetSlotLockState slot=" + slotId.ToString() + " NOT FOUND in m_SlotsMap");
    }
  }

	// --------------------------------------------------------------------------------------------
	void AddDisconnectedPlayerInfo(string guid, RplId slotId, int playerId)
	{
		m_DisconnectedPlayers.Insert(guid, slotId);
		RPC_AddDisconnectedPlayerInfo(playerId);
		Rpc(RPC_AddDisconnectedPlayerInfo, playerId);
	}

  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  void RPC_AddDisconnectedPlayerInfo(int playerId)
  {
    PS_DebugLogger.LogImportant("RPC_AddDisconnectedPlayerInfo player=" + playerId.ToString(), playerId);
    m_DisconnectedPlayersClient.Insert(playerId);
  }

  void RemoveDisconnectedPlayerInfo(string guid)
  {
    int playerId;
    m_PlayerGUIDtoIdCached.Find(guid, playerId);
    m_DisconnectedPlayers.Remove(guid);
    PS_DebugLogger.LogImportant("RemoveDisconnectedPlayerInfo guid=" + guid + " player=" + playerId.ToString(), playerId);
    RPC_RemoveDisconnectedPlayerInfo(playerId);
    Rpc(RPC_RemoveDisconnectedPlayerInfo, playerId);
  }

  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  void RPC_RemoveDisconnectedPlayerInfo(int playerId)
  {
    PS_DebugLogger.LogImportant("RPC_RemoveDisconnectedPlayerInfo player=" + playerId.ToString(), playerId);
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
    PS_DebugLogger.LogImportant("RPC_RemovePlayer player=" + playerId.ToString() + " guid=" + playerGuid, playerId);

    // Guard against double-remove: player already gone
    if (!m_PlayerSlotMap.Contains(playerId))
    {
      PS_DebugLogger.Log("RPC_RemovePlayer player=" + playerId.ToString() + " already removed, skipping");
      return;
    }

    RplId slotId;
    FindPlayerSlotById(playerId, slotId);

    {
      PS_SlotCharacterData sd;
      if (slotId != RplId.Invalid() && m_SlotsMap.Find(slotId, sd))
        sd.m_PlayerId = -1;
    }
    m_PlayerSlotMap.Remove(playerId);
    m_PlayerNamesCached.Remove(playerId);
    m_DisconnectedPlayersClient.RemoveItem(playerId);

    PS_DebugLogger.Log("RPC_RemovePlayer DONE player=" + playerId.ToString() + " slot=" + slotId.ToString() + " playerSlotMapCount=" + m_PlayerSlotMap.Count().ToString());

    m_CallbackHandler.GetOnPlayerRemoved().Invoke(playerId, slotId);
  }

	void UpdatePlayerReconnected(int playerId, string playerGuid)
	{
		int oldPlayerId;
		m_PlayerGUIDtoIdCached.Find(playerGuid, oldPlayerId);
		m_PlayerGUIDtoIdCached.Set(playerGuid, playerId);
		if (oldPlayerId == playerId)
			return;
		if (m_PlayerIdToGuidCached.Contains(oldPlayerId))
		{
			m_PlayerIdToGuidCached.Set(playerId, m_PlayerIdToGuidCached.Get(oldPlayerId));
			m_PlayerIdToGuidCached.Remove(oldPlayerId);
		}

		PS_DebugLogger.LogImportant("UpdatePlayerReconnected newPlayer=" + playerId.ToString() + " oldPlayer=" + oldPlayerId.ToString());

		RPC_UpdatePlayerReconnected(playerId, oldPlayerId);
		Rpc(RPC_UpdatePlayerReconnected, playerId, oldPlayerId);
	}

  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  void RPC_UpdatePlayerReconnected(int playerId, int oldPlayerId)
  {
    PS_DebugLogger.LogImportant("RPC_UpdatePlayerReconnected newPlayer=" + playerId.ToString() + " oldPlayer=" + oldPlayerId.ToString(), playerId);
    if (playerId != oldPlayerId)
    {
      RplId slotId;
      if (m_PlayerSlotMap.Find(oldPlayerId, slotId) && m_SlotsMap.Contains(slotId))
      {
        PS_SlotCharacterData sd;
        if (m_SlotsMap.Find(slotId, sd))
          sd.m_PlayerId = playerId;
        PS_DebugLogger.Log("RPC_UpdatePlayerReconnected updated slot=" + slotId.ToString() + " to newPlayerId=" + playerId.ToString(), playerId);
      }
      else
      {
        PS_DebugLogger.Log("RPC_UpdatePlayerReconnected oldPlayer=" + oldPlayerId.ToString() + " had no slot in m_PlayerSlotMap or slot not in m_SlotsMap", playerId);
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

	// ---- JIP Sync: send full state of non-RplProp maps to a connecting client ----
	void SyncStateToClient(int playerId)
	{
		SyncStateToClientInternal(playerId, 0);
	}

	protected void SyncStateToClientInternal(int playerId, int attempt)
	{
		PlayerController pc = m_PlayerManager.GetPlayerController(playerId);
		if (!pc)
		{
			if (attempt < 10)
			{
				PS_DebugLogger.Log("SyncStateToClient player=" + playerId.ToString() + " controller NULL, retrying attempt=" + attempt.ToString());
				m_CallQueue.CallLater(SyncStateToClientInternal, 200, false, playerId, attempt + 1);
			}
			else
			{
				PS_DebugLogger.LogError("SyncStateToClient GAVE UP after " + attempt.ToString() + " attempts player=" + playerId.ToString());
			}
			return;
		}
		PS_PlayableControllerComponent pcc = PS_PlayableControllerComponent.Cast(pc.FindComponent(PS_PlayableControllerComponent));
		if (!pcc)
		{
			if (attempt < 10)
			{
				PS_DebugLogger.Log("SyncStateToClient player=" + playerId.ToString() + " pcc NULL, retrying attempt=" + attempt.ToString());
				m_CallQueue.CallLater(SyncStateToClientInternal, 200, false, playerId, attempt + 1);
			}
			else
			{
				PS_DebugLogger.LogError("SyncStateToClient GAVE UP: pcc NULL after " + attempt.ToString() + " attempts player=" + playerId.ToString());
			}
			return;
		}

		// Build faction array
		array<int> fKeys = {};
		array<string> fVals = {};
		foreach (int pid, FactionKey fk : m_PlayerFactionMap.GetRawMap())
		{
			fKeys.Insert(pid);
			fVals.Insert(fk);
		}

		// Build states array
		array<int> sKeys = {};
		array<int> sVals = {};
		foreach (int pid, PS_EPlayableControllerState st : m_PlayerStatesMap.GetRawMap())
		{
			sKeys.Insert(pid);
			sVals.Insert(st);
		}

		// Build pin array
		array<int> pKeys = {};
		array<bool> pVals = {};
		foreach (int pid, bool pinned : m_PlayerPinMap.GetRawMap())
		{
			pKeys.Insert(pid);
			pVals.Insert(pinned);
		}

		// Build names array
		array<int> nKeys = {};
		array<string> nVals = {};
		foreach (int pid, string name : m_PlayerNamesCached.GetRawMap())
		{
			nKeys.Insert(pid);
			nVals.Insert(name);
		}

		pcc.SyncFullState(fKeys, fVals, sKeys, sVals, pKeys, pVals, m_DisconnectedPlayersClient, nKeys, nVals);
		PS_DebugLogger.LogImportant("SyncStateToClient SUCCESS player=" + playerId.ToString() + " attempt=" + attempt.ToString() + " factions=" + fKeys.Count().ToString() + " states=" + sKeys.Count().ToString() + " pins=" + pKeys.Count().ToString() + " disconnected=" + m_DisconnectedPlayersClient.Count().ToString() + " names=" + nKeys.Count().ToString());
	}

	void InvokePlayerConnectedOnClients(int playerId)
	{
		Rpc(RPC_InvokePlayerConnectedOnClients, playerId);
	}

  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  protected void RPC_InvokePlayerConnectedOnClients(int playerId)
  {
    PS_DebugLogger.LogImportant("RPC_InvokePlayerConnectedOnClients player=" + playerId.ToString(), playerId);
    m_CallbackHandler.GetOnPlayerConnected().Invoke(playerId);
  }

  void InvokePlayerDisconnectedOnClients(int playerId, KickCauseCode cause = KickCauseCode.NONE, int timeout = -1)
  {
    PS_DebugLogger.LogImportant("InvokePlayerDisconnectedOnClients player=" + playerId.ToString() + " cause=(" + typename.EnumToString(KickCauseGroup2, KickCauseCodeAPI.GetGroup(cause)) + "," + KickCauseCodeAPI.GetReason(cause).ToString() + ")", playerId);
    Rpc(RPC_InvokePlayerDisconnectedOnClients, playerId, cause, timeout);
  }

  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  protected void RPC_InvokePlayerDisconnectedOnClients(int playerId, KickCauseCode cause, int timeout)
  {
    PS_DebugLogger.LogImportant("RPC_InvokePlayerDisconnectedOnClients player=" + playerId.ToString(), playerId);
    m_CallbackHandler.GetOnPlayerDisconnected().Invoke(playerId, cause, timeout);
    m_eOnPlayerDisconnected.Invoke(playerId, cause, timeout);
  }

	// --------------------------------------------------------------------------------------------
  void NotifyKick(int playerId)
  {
    PS_DebugLogger.LogImportant("NotifyKick player=" + playerId.ToString(), playerId);
    Rpc(RPC_NotifyKick, playerId);
  }

  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  protected void RPC_NotifyKick(int playerId)
  {
    PS_DebugLogger.LogImportant("RPC_NotifyKick player=" + playerId.ToString(), playerId);
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
    PS_DebugLogger.LogImportant("ForceSwitch SRV player=" + playerId.ToString(), playerId);
    Rpc(RPC_ForceSwitch, playerId);
  }

  [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
  protected void RPC_ForceSwitch(int playerId)
  {
    PS_DebugLogger.LogImportant("RPC_ForceSwitch BROADCAST player=" + playerId.ToString(), playerId);
    PlayerController playerController = GetGame().GetPlayerController();
    if (!playerController || playerController.GetPlayerId() != playerId)
      return;
    if (!s_CurrentPlayableController)
    {
      PS_DebugLogger.LogError("RPC_ForceSwitch s_CurrentPlayableController NULL! Cannot switch to GAME menu", playerId);
      return;
    }
    PS_EPlayableControllerState myState = GetPlayerState(playerId);
    SCR_EGameModeState gameModeState = PS_GameModeCoop.Cast(GetGame().GetGameMode()).GetState();
    PS_DebugLogger.LogImportant("RPC_ForceSwitch switching to GAME menu myState=" + typename.EnumToString(PS_EPlayableControllerState, myState) + " gameModeState=" + typename.EnumToString(SCR_EGameModeState, gameModeState), playerId);
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

		foreach (RplId slotId : GetSortedSlotIds())
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
	protected int m_iPendingDeletions = 0;

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
		m_iPendingDeletions = 0;

		// Phase 1: Detach entities from AI groups, freeze animation to reduce proxy RPC flood on deletion
		foreach (RplId slotId : toRemove)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(slotId));
			if (!rpl)
				continue;
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(rpl.GetEntity());
			if (!character)
				continue;
			CharacterControllerComponent charController = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
			if (charController)
				charController.SetMovement(0, vector.Forward);
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

		// Phase 2: Queue deferred deletion with pending-deletion tracking
		int delay = 0;
		foreach (RplId slotId : toRemove)
		{
			PS_DebugLogger.LogImportant("RemoveRedundantUnits DELETING slot=" + slotId.ToString());
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(slotId));
			IEntity entity = null;
			if (rpl) entity = rpl.GetEntity();
			m_EntityCache.Remove(slotId);
			if (entity)
			{
				m_iPendingDeletions++;
				m_CallQueue.CallLater(DeleteEntityDeferred, delay, false, entity);
			}
			delay += 50;
		}
		if (m_iPendingDeletions == 0)
			m_bBulkRemoving = false;

		// Phase 3: Stagger slot removals to avoid replication burst
		int slotDelay = 0;
		foreach (RplId slotId : toRemove)
		{
			m_CallQueue.CallLater(RemoveLobbySlot, slotDelay, false, slotId);
			slotDelay += 50;
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
		m_iPendingDeletions--;
		if (m_iPendingDeletions <= 0)
			m_bBulkRemoving = false;
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
    PS_DebugLogger.LogImportant("RPC_ReplaceLobbySlot oldSlot=" + oldSlotId.ToString() + " newSlot=" + newSlotId.ToString());
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

    RplComponent newRpl = RplComponent.Cast(Replication.FindItem(newSlotId));
    if (newRpl && newRpl.GetEntity())
    {
      m_EntityCache[newSlotId] = newRpl.GetEntity();
      PS_DebugLogger.LogImportant("RPC_ReplaceLobbySlot cache REFRESH newSlot=" + newSlotId.ToString());
    }
    else
    {
      PS_DebugLogger.LogImportant("RPC_ReplaceLobbySlot cache MISS newSlot=" + newSlotId.ToString() + " (RPL not indexed yet)");
    }
    m_EntityCache.Remove(oldSlotId);
    PS_DebugLogger.LogImportant("RPC_ReplaceLobbySlot cache REMOVED oldSlot=" + oldSlotId.ToString());

    PS_DebugLogger.LogImportant("RPC_ReplaceLobbySlot DONE player=" + playerId.ToString() + " faction=" + factionKey);
  }

	// --------------------------------------------------------------------------------------------
  protected void OnPlayerConnected(int playerId)
  {
    PS_DebugLogger.LogImportant("PS_PlayableManager OnPlayerConnected player=" + playerId.ToString() + " slotMapCount=" + m_PlayerSlotMap.Count().ToString() + " slotsCount=" + m_SlotsMap.Count().ToString(), playerId);
    m_CallbackHandler.GetOnPlayerConnected().Invoke(playerId);
    m_eOnPlayerConnected.Invoke(playerId);

    RplId slotId = GetPlayableByPlayer(playerId);
    PS_DebugLogger.LogImportant("PS_PlayableManager OnPlayerConnected slotId=" + slotId.ToString(), playerId);
    PS_PlayableContainer container = GetPlayableById(slotId);
    if (container)
      container.GetOnPlayerConnected().Invoke(playerId);
  }

  protected void OnPlayerDisconnected(int playerId, KickCauseCode cause = KickCauseCode.NONE, int timeout = -1)
  {
    PS_DebugLogger.LogImportant("PS_PlayableManager OnPlayerDisconnected player=" + playerId.ToString() + " cause=(" + typename.EnumToString(KickCauseGroup2, KickCauseCodeAPI.GetGroup(cause)) + "," + KickCauseCodeAPI.GetReason(cause).ToString() + ")", playerId);
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
		foreach (RplId slotId : GetSortedSlotIds())
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
		int slotCount = m_SlotsMap.Count();
		PS_DebugLogger.LogImportant("GetPlayables slotsMapCount=" + slotCount.ToString() + " sortedCount=" + m_SlotsSortedCached.Count().ToString());
		foreach (RplId slotId, PS_SlotCharacterData slot : m_SlotsMap.GetRawMap())
		{
			PS_PlayableContainer container = GetPlayableById(slotId);
			if (container)
			{
				PS_DebugLogger.Log("GetPlayables INSERT slotId=" + slotId.ToString() + " containerRplId=" + container.GetRplId().ToString() + " name=" + container.GetName() + " faction=" + container.GetFactionKey());
				result.Insert(slotId, container);
			}
			else
			{
				PS_DebugLogger.LogError("GetPlayables SKIP_NULL_CONTAINER slotId=" + slotId.ToString());
			}
		}
		PS_DebugLogger.LogImportant("GetPlayables DONE resultCount=" + result.Count().ToString() + " slotsMapCount=" + slotCount.ToString());
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
