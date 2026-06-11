[ComponentEditorProps(category: "GameScripted/Character", description: "Set character playable", color: "0 0 255 255", icon: HYBRID_COMPONENT_ICON)]
class PS_PlayableComponentClass : ScriptComponentClass
{
}

class PS_PlayableComponent : ScriptComponent
{
	[Attribute()]
	protected string m_sName;
	[Attribute()]
	protected bool m_bIsPlayable;
	protected RplId m_RplId;
	protected vector m_SpawnTransform[4];

	protected ref PS_SlotCharacterData m_SlotData;
	protected PS_GameModeCoop m_GameModeCoop;
	protected PS_PlayableManager m_PlayableManager;
	protected ref PS_PlayableContainer m_PlayableContainer;

	PS_PlayableContainer GetPlayableContainer()
	{
		return m_PlayableContainer;
	}

	SCR_ChimeraCharacter m_Owner;
	SCR_ChimeraCharacter GetOwnerCharacter()
	{
		return m_Owner;
	}

	protected FactionAffiliationComponent m_FactionAffiliationComponent;
	FactionAffiliationComponent GetFactionAffiliationComponent()
	{
		return m_FactionAffiliationComponent;
	}

	protected SCR_EditableCharacterComponent m_EditableCharacterComponent;
	protected SCR_UIInfo m_EditableUIInfo;
	protected SCR_CharacterDamageManagerComponent m_CharacterDamageManagerComponent;
	SCR_CharacterDamageManagerComponent GetCharacterDamageManagerComponent()
	{
		return m_CharacterDamageManagerComponent;
	}

	protected AIControlComponent m_AIControlComponent;
	protected AIAgent m_AIAgent;

	override void OnPostInit(IEntity owner)
	{
		m_Owner = SCR_ChimeraCharacter.Cast(owner);
		m_Owner.PS_SetPlayable(this);

		if (Replication.IsServer())
			owner.GetTransform(m_SpawnTransform);

		SetEventMask(owner, EntityEvent.INIT);
	}

	void GetSpawnTransform(inout vector outMat[4])
	{
		Math3D.MatrixCopy(m_SpawnTransform, outMat);
	}

override void EOnInit(IEntity owner)
	{
		m_FactionAffiliationComponent = FactionAffiliationComponent.Cast(owner.FindComponent(FactionAffiliationComponent));
		m_EditableCharacterComponent = SCR_EditableCharacterComponent.Cast(owner.FindComponent(SCR_EditableCharacterComponent));
		m_EditableUIInfo = m_EditableCharacterComponent.GetInfo();
		m_CharacterDamageManagerComponent = SCR_CharacterDamageManagerComponent.Cast(owner.FindComponent(SCR_CharacterDamageManagerComponent));
		m_AIControlComponent = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
		if (m_AIControlComponent)
			m_AIAgent = m_AIControlComponent.GetAIAgent();
		GetGame().GetCallqueue().Call(LateInit);
	}

	void LateInit()
	{
		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		if (!rpl)
			return;
		m_RplId = rpl.Id();
		m_GameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		m_PlayableManager = PS_PlayableManager.GetInstance();

		m_PlayableContainer = new PS_PlayableContainer();
		m_PlayableContainer.Init(this);

		m_SlotData = new PS_SlotCharacterData();
		m_SlotData.InitFromPlayable(this);

		if (Replication.IsServer())
		{
			GetGame().GetCallqueue().CallLater(AddToList, 0, false, GetOwner());
			m_CharacterDamageManagerComponent.GetOnDamageStateChanged().Insert(OnDamageStateChange);
		}
	}

	bool GetPlayable()
	{
		return m_bIsPlayable;
	}

	void SetPlayable(bool isPlayable)
	{
		RPC_SetPlayable(isPlayable);
		Rpc(RPC_SetPlayable, isPlayable);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetPlayable(bool isPlayable)
	{
		m_bIsPlayable = isPlayable;
		if (m_bIsPlayable)
			GetGame().GetCallqueue().CallLater(AddToList, 0, false, m_Owner);
		else RemoveFromList();
	}

void RemoveFromList()
	{
		GetGame().GetCallqueue().Remove(AddToList);
		GetGame().GetCallqueue().Remove(AddToListWrap);
		GetGame().GetCallqueue().Remove(ForceDeactivateAI);

		BaseGameMode gamemode = GetGame().GetGameMode();
		if (!gamemode)
			return;

		if (m_Owner)
		{
			AIControlComponent aiComponent = AIControlComponent.Cast(m_Owner.FindComponent(AIControlComponent));
			if (aiComponent)
			{
				AIAgent agent = aiComponent.GetAIAgent();
				if (agent)
					agent.ActivateAI();
			}
			RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
			rpl.EnableStreaming(true);
		}

		if (m_PlayableManager && !m_PlayableManager.IsBulkRemoving())
			m_PlayableManager.UnRegisterPlayable(m_RplId);
	}

	void OnDamageStateChange(EDamageState state)
	{
		if (!m_PlayableManager)
			return;
		// Echo Lobby pattern: only mark slot destroyed, do NOT trigger respawn/slot clearing.
		// Spectator transition is handled by HandlePlayerKilled in PS_GameModeCoop.
		m_PlayableManager.OnPlayableDamageStateChanged(m_RplId, state);
	}

	void AddToList(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(ForceActivateAI);
		if (GetOwner().GetWorld() != GetGame().GetWorld())
			return;

		if (!m_bIsPlayable)
			return;

		GetGame().GetCallqueue().CallLater(AddToListWrap, 0, false, owner);
	}

	void AddToListWrap(IEntity owner)
	{
		if (!m_bIsPlayable)
			return;

		if (GetGame().GetAIWorld().CanAIBeActivated())
			GetGame().GetCallqueue().CallLater(ForceDeactivateAI, 500, true);

		if (m_GameModeCoop.GetDisablePlayablesStreaming())
		{
			RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
			rpl.EnableStreaming(false);
		}

		if (!m_PlayableManager)
			return;

		AIControlComponent aiControl = AIControlComponent.Cast(m_Owner.FindComponent(AIControlComponent));
		if (aiControl)
		{
			SCR_AIGroup playableGroup = SCR_AIGroup.Cast(aiControl.GetControlAIAgent().GetParentGroup());
			if (playableGroup)
				m_SlotData.SetGroup(playableGroup);
		}

		m_PlayableManager.InsertLobbySlot(m_SlotData);
	}

	void ForceActivateAI()
	{
		if (!m_AIAgent)
		{
			GetGame().GetCallqueue().Remove(ForceActivateAI);
			return;
		}
		if (!m_AIAgent.IsAIActivated())
			m_AIAgent.ActivateAI();
		else
			GetGame().GetCallqueue().Remove(ForceActivateAI);
	}

	void ForceDeactivateAI()
	{
		if (!m_AIAgent)
		{
			GetGame().GetCallqueue().Remove(ForceDeactivateAI);
			return;
		}
		if (m_AIAgent.IsAIActivated())
			m_AIAgent.DeactivateAI();
	}

	void HolsterWeapon()
	{
		m_Owner.GetCharacterController().TryEquipRightHandItem(null, EEquipItemType.EEquipTypeUnarmedContextual);
	}

	string GetName()
	{
		if (m_sName != "")
			return m_sName;
		return m_EditableUIInfo.GetName();
	}

	RplId GetId()
	{
		return GetRplId();
	}

	RplId GetRplId()
	{
		return m_RplId;
	}

	FactionKey GetFactionKey()
	{
		return GetFactionAffiliationComponent().GetDefaultFactionKey();
	}

	string GetRoleIconPath()
	{
		SCR_EditableCharacterComponent editableCharacterComponent = SCR_EditableCharacterComponent.Cast(GetOwner().FindComponent(SCR_EditableCharacterComponent));
		SCR_UIInfo uiInfo = editableCharacterComponent.GetInfo();
		if (uiInfo.GetIconSetName() == "")
			return uiInfo.GetIconPath();
		else
			return uiInfo.GetImageSetPath();
	}

	string GetRoleIconQuad()
	{
		SCR_EditableCharacterComponent editableCharacterComponent = SCR_EditableCharacterComponent.Cast(GetOwner().FindComponent(SCR_EditableCharacterComponent));
		SCR_UIInfo uiInfo = editableCharacterComponent.GetInfo();
		return uiInfo.GetIconSetName();
	}

	string GetRoleName()
	{
		SCR_EditableCharacterComponent editableCharacterComponent = SCR_EditableCharacterComponent.Cast(GetOwner().FindComponent(SCR_EditableCharacterComponent));
		SCR_UIInfo uiInfo = editableCharacterComponent.GetInfo();
		return uiInfo.GetName();
	}

	SCR_ECharacterRank GetCharacterRank()
	{
		return SCR_CharacterRankComponent.GetCharacterRank(GetOwner());
	}

	EDamageState GetDamageState()
	{
		return m_CharacterDamageManagerComponent.GetState();
	}

	void PS_PlayableComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
	}

	void ~PS_PlayableComponent()
	{
		if (Replication.IsServer())
			RemoveFromList();
	}

	SCR_ChimeraCharacter GetCharacter()
	{
		return SCR_ChimeraCharacter.Cast(GetOwner());
	}
};

;
