class PS_SlotCharacterData
{
	RplId m_RplId = RplId.Invalid();
	string m_sName;
	FactionKey m_FactionKey;
	SCR_ECharacterRank m_eCharacterRank;
	string m_sRoleIconPath;
	string m_sRoleIconQuad;
	string m_sRoleName;
	ResourceName m_CharacterPrefabPath;
	EDamageState m_eDamageState;

	int m_PlayerId = -1;
	int m_PlayerGroupId = -1;
	bool m_IsLocked = false;
	bool m_IsDestroyed = false;

	IEntity m_CachedEntity;

	private SCR_AIGroup m_AIGroup;

	SCR_AIGroup GetGroup()
	{
		return m_AIGroup;
	}

	void SetGroup(SCR_AIGroup group)
	{
		m_AIGroup = group;
	}

	void InitFromPlayable(PS_PlayableComponent playableComponent)
	{
		m_RplId = playableComponent.GetRplId();
		m_CachedEntity = playableComponent.GetOwner();
		m_sName = playableComponent.GetName();
		m_FactionKey = playableComponent.GetFactionKey();
		m_eCharacterRank = playableComponent.GetCharacterRank();
		m_sRoleIconPath = playableComponent.GetRoleIconPath();
		m_sRoleIconQuad = playableComponent.GetRoleIconQuad();
		m_sRoleName = playableComponent.GetRoleName();
		m_CharacterPrefabPath = playableComponent.GetOwnerCharacter().GetPrefabData().GetPrefabName();
		m_eDamageState = playableComponent.GetDamageState();
	}

	void UpdateFromPlayable(PS_PlayableComponent playableComponent)
	{
		m_sName = playableComponent.GetName();
		m_eDamageState = playableComponent.GetDamageState();
	}

	SCR_Faction GetFaction()
	{
		return SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(m_FactionKey));
	}

	bool SetIconTo(ImageWidget imageWidget)
	{
		if (!imageWidget || m_sRoleIconPath.IsEmpty())
			return false;

		if (m_sRoleIconQuad != "")
			imageWidget.LoadImageFromSet(0, m_sRoleIconPath, m_sRoleIconQuad);
		else
			imageWidget.LoadImageTexture(0, m_sRoleIconPath);

		return true;
	}

	static void Encode(SSnapSerializerBase snapshot, ScriptCtx ctx, ScriptBitSerializer packet)
	{
		snapshot.EncodeInt(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeInt(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeInt(packet);

		snapshot.EncodeInt(packet);
		snapshot.EncodeInt(packet);
		snapshot.EncodeBool(packet);
		snapshot.EncodeBool(packet);
	}

	static bool Decode(ScriptBitSerializer packet, ScriptCtx ctx, SSnapSerializerBase snapshot)
	{
		snapshot.DecodeInt(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeInt(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeInt(packet);

		snapshot.DecodeInt(packet);
		snapshot.DecodeInt(packet);
		snapshot.DecodeBool(packet);
		snapshot.DecodeBool(packet);
		return true;
	}

	static bool SnapCompare(SSnapSerializerBase lhs, SSnapSerializerBase rhs, ScriptCtx ctx)
	{
		return lhs.CompareSnapshots(rhs, 4)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareSnapshots(rhs, 4)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareSnapshots(rhs, 4)

			&& lhs.CompareSnapshots(rhs, 4)
			&& lhs.CompareSnapshots(rhs, 4)
			&& lhs.CompareSnapshots(rhs, 1)
			&& lhs.CompareSnapshots(rhs, 1);
	}

	static bool PropCompare(PS_SlotCharacterData data, SSnapSerializerBase snapshot, ScriptCtx ctx)
	{
		return snapshot.CompareInt(data.m_RplId)
			&& snapshot.CompareString(data.m_sName)
			&& snapshot.CompareString(data.m_FactionKey)
			&& snapshot.CompareInt(data.m_eCharacterRank)
			&& snapshot.CompareString(data.m_sRoleIconPath)
			&& snapshot.CompareString(data.m_sRoleIconQuad)
			&& snapshot.CompareString(data.m_sRoleName)
			&& snapshot.CompareString(data.m_CharacterPrefabPath)
			&& snapshot.CompareInt(data.m_eDamageState)

			&& snapshot.CompareInt(data.m_PlayerId)
			&& snapshot.CompareInt(data.m_PlayerGroupId)
			&& snapshot.CompareBool(data.m_IsLocked)
			&& snapshot.CompareBool(data.m_IsDestroyed);
	}

	static bool Extract(PS_SlotCharacterData data, ScriptCtx ctx, SSnapSerializerBase snapshot)
	{
		snapshot.SerializeInt(data.m_RplId);
		snapshot.SerializeString(data.m_sName);
		snapshot.SerializeString(data.m_FactionKey);
		snapshot.SerializeInt(data.m_eCharacterRank);
		snapshot.SerializeString(data.m_sRoleIconPath);
		snapshot.SerializeString(data.m_sRoleIconQuad);
		snapshot.SerializeString(data.m_sRoleName);
		snapshot.SerializeString(data.m_CharacterPrefabPath);
		snapshot.SerializeInt(data.m_eDamageState);

		snapshot.SerializeInt(data.m_PlayerId);
		snapshot.SerializeInt(data.m_PlayerGroupId);
		snapshot.SerializeBool(data.m_IsLocked);
		snapshot.SerializeBool(data.m_IsDestroyed);
		return true;
	}

	static bool Inject(SSnapSerializerBase snapshot, ScriptCtx ctx, PS_SlotCharacterData data)
	{
		snapshot.SerializeInt(data.m_RplId);
		snapshot.SerializeString(data.m_sName);
		snapshot.SerializeString(data.m_FactionKey);
		snapshot.SerializeInt(data.m_eCharacterRank);
		snapshot.SerializeString(data.m_sRoleIconPath);
		snapshot.SerializeString(data.m_sRoleIconQuad);
		snapshot.SerializeString(data.m_sRoleName);
		snapshot.SerializeString(data.m_CharacterPrefabPath);
		snapshot.SerializeInt(data.m_eDamageState);

		snapshot.SerializeInt(data.m_PlayerId);
		snapshot.SerializeInt(data.m_PlayerGroupId);
		snapshot.SerializeBool(data.m_IsLocked);
		snapshot.SerializeBool(data.m_IsDestroyed);
		return true;
	}
};
