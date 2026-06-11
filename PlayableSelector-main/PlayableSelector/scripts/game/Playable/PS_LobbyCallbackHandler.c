void PS_PlayerReconnectedCallback(int oldPlayerId, int newPlayerId);
typedef func PS_PlayerReconnectedCallback;

void PS_PlayerRemovedCallback(int playerId, RplId slotId);
typedef func PS_PlayerRemovedCallback;

void PS_PlayerSetOnSlotCallback(int playerId, RplId prevSlotId, RplId slotId);
typedef func PS_PlayerSetOnSlotCallback;

void PS_SlotInsertedCallback(RplId slotId, int joinableGroupId, FactionKey factionKey);
typedef func PS_SlotInsertedCallback;

void PS_SlotRemovedCallback(RplId slotId, FactionKey factionKey, int playerId);
typedef func PS_SlotRemovedCallback;

void PS_SlotDestroyedCallback(RplId slotId, bool isDestroyed);
typedef func PS_SlotDestroyedCallback;

void PS_SlotLockedCallback(RplId slotId, bool isLocked);
typedef func PS_SlotLockedCallback;

void PS_SlotUpdatedCallback(RplId oldSlotId, RplId newSlotId);
typedef func PS_SlotUpdatedCallback;

void PS_FactionReadyStateChangedCallback(FactionKey factionKey, int readyCount);
typedef func PS_FactionReadyStateChangedCallback;

void PS_PlayerNameUpdatedCallback(int playerId, string newName);
typedef func PS_PlayerNameUpdatedCallback;

void PS_PlayerStateChangedCallback(int playerId, PS_EPlayableControllerState state);
typedef func PS_PlayerStateChangedCallback;

void PS_PlayerFactionChangedCallback(int playerId, FactionKey factionKey, FactionKey oldFactionKey);
typedef func PS_PlayerFactionChangedCallback;

void PS_VehicleInsertedCallback(RplId vehicleId);
typedef func PS_VehicleInsertedCallback;

void PS_SlotDamageStateChangedCallback(RplId slotId, EDamageState damageState);
typedef func PS_SlotDamageStateChangedCallback;

class PS_LobbyCallbackHandler : Managed
{
	ref ScriptInvokerBase<SCR_BaseGameMode_PlayerId> m_OnPlayerConnected = new ScriptInvokerBase<SCR_BaseGameMode_PlayerId>();
	ref ScriptInvokerBase<SCR_BaseGameMode_OnPlayerDisconnected> m_OnPlayerDisconnected = new ScriptInvokerBase<SCR_BaseGameMode_OnPlayerDisconnected>();
	ref ScriptInvokerBase<PS_PlayerRemovedCallback> m_OnPlayerRemoved = new ScriptInvokerBase<PS_PlayerRemovedCallback>();
	ref ScriptInvokerBase<PS_PlayerReconnectedCallback> m_OnPlayerReconnected = new ScriptInvokerBase<PS_PlayerReconnectedCallback>();
	ref ScriptInvokerBase<PS_PlayerSetOnSlotCallback> m_OnPlayerSetOnSlot = new ScriptInvokerBase<PS_PlayerSetOnSlotCallback>();
	ref ScriptInvokerBase<PS_SlotInsertedCallback> m_OnSlotInserted = new ScriptInvokerBase<PS_SlotInsertedCallback>();
	ref ScriptInvokerBase<PS_SlotRemovedCallback> m_OnSlotRemoved = new ScriptInvokerBase<PS_SlotRemovedCallback>();
	ref ScriptInvokerBase<PS_SlotDestroyedCallback> m_OnSlotDestroyed = new ScriptInvokerBase<PS_SlotDestroyedCallback>();
	ref ScriptInvokerBase<PS_SlotLockedCallback> m_OnSlotLocked = new ScriptInvokerBase<PS_SlotLockedCallback>();
	ref ScriptInvokerBase<PS_SlotUpdatedCallback> m_OnSlotUpdated = new ScriptInvokerBase<PS_SlotUpdatedCallback>();
	ref ScriptInvokerBase<PS_FactionReadyStateChangedCallback> m_OnFactionReadyStateChanged = new ScriptInvokerBase<PS_FactionReadyStateChangedCallback>();
	ref ScriptInvokerBase<PS_PlayerNameUpdatedCallback> m_OnPlayerNameUpdated = new ScriptInvokerBase<PS_PlayerNameUpdatedCallback>();
	ref ScriptInvokerBase<PS_PlayerStateChangedCallback> m_OnPlayerStateChanged = new ScriptInvokerBase<PS_PlayerStateChangedCallback>();
	ref ScriptInvokerBase<PS_PlayerFactionChangedCallback> m_OnPlayerFactionChanged = new ScriptInvokerBase<PS_PlayerFactionChangedCallback>();
	ref ScriptInvokerBase<PS_VehicleInsertedCallback> m_OnVehicleInserted = new ScriptInvokerBase<PS_VehicleInsertedCallback>();
	ref ScriptInvokerBase<PS_SlotDamageStateChangedCallback> m_OnSlotDamageStateChanged = new ScriptInvokerBase<PS_SlotDamageStateChangedCallback>();

	ScriptInvokerBase<SCR_BaseGameMode_PlayerId> GetOnPlayerConnected()
	{
		return m_OnPlayerConnected;
	}

	ScriptInvokerBase<SCR_BaseGameMode_OnPlayerDisconnected> GetOnPlayerDisconnected()
	{
		return m_OnPlayerDisconnected;
	}

	ScriptInvokerBase<PS_PlayerRemovedCallback> GetOnPlayerRemoved()
	{
		return m_OnPlayerRemoved;
	}

	ScriptInvokerBase<PS_PlayerReconnectedCallback> GetOnPlayerReconnected()
	{
		return m_OnPlayerReconnected;
	}

	ScriptInvokerBase<PS_PlayerSetOnSlotCallback> GetOnPlayerSetOnSlot()
	{
		return m_OnPlayerSetOnSlot;
	}

	ScriptInvokerBase<PS_SlotInsertedCallback> GetOnSlotInserted()
	{
		return m_OnSlotInserted;
	}

	ScriptInvokerBase<PS_SlotRemovedCallback> GetOnSlotRemoved()
	{
		return m_OnSlotRemoved;
	}

	ScriptInvokerBase<PS_SlotDestroyedCallback> GetOnSlotDestroyed()
	{
		return m_OnSlotDestroyed;
	}

	ScriptInvokerBase<PS_SlotLockedCallback> GetOnSlotLocked()
	{
		return m_OnSlotLocked;
	}

	ScriptInvokerBase<PS_SlotUpdatedCallback> GetOnSlotUpdated()
	{
		return m_OnSlotUpdated;
	}

	ScriptInvokerBase<PS_FactionReadyStateChangedCallback> GetOnFactionReadyStateChanged()
	{
		return m_OnFactionReadyStateChanged;
	}

	ScriptInvokerBase<PS_PlayerNameUpdatedCallback> GetOnPlayerNameUpdated()
	{
		return m_OnPlayerNameUpdated;
	}

	ScriptInvokerBase<PS_PlayerStateChangedCallback> GetOnPlayerStateChanged()
	{
		return m_OnPlayerStateChanged;
	}

	ScriptInvokerBase<PS_PlayerFactionChangedCallback> GetOnPlayerFactionChanged()
	{
		return m_OnPlayerFactionChanged;
	}

	ScriptInvokerBase<PS_VehicleInsertedCallback> GetOnVehicleInserted()
	{
		return m_OnVehicleInserted;
	}

	ScriptInvokerBase<PS_SlotDamageStateChangedCallback> GetOnSlotDamageStateChanged()
	{
		return m_OnSlotDamageStateChanged;
	}
};
