class PS_VehicleData
{
	RplId m_RplId = RplId.Invalid();
	int m_GroupId;
	string m_Name;
	FactionKey m_FactionKey;
	ResourceName m_PrefabPath;
	ResourceName m_IconPath;
	string m_IconSetName;
	bool m_IsLocked;

	SCR_Faction GetFaction()
	{
		return SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(m_FactionKey));
	}

	static void Encode(SSnapSerializerBase snapshot, ScriptCtx ctx, ScriptBitSerializer packet)
	{
		snapshot.EncodeInt(packet);
		snapshot.EncodeInt(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeString(packet);
		snapshot.EncodeBool(packet);
	}

	static bool Decode(ScriptBitSerializer packet, ScriptCtx ctx, SSnapSerializerBase snapshot)
	{
		snapshot.DecodeInt(packet);
		snapshot.DecodeInt(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeString(packet);
		snapshot.DecodeBool(packet);
		return true;
	}

	static bool SnapCompare(SSnapSerializerBase lhs, SSnapSerializerBase rhs, ScriptCtx ctx)
	{
		return lhs.CompareSnapshots(rhs, 4)
			&& lhs.CompareSnapshots(rhs, 4)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareStringSnapshots(rhs)
			&& lhs.CompareSnapshots(rhs, 1);
	}

	static bool PropCompare(PS_VehicleData data, SSnapSerializerBase snapshot, ScriptCtx ctx)
	{
		return snapshot.CompareInt(data.m_RplId)
			&& snapshot.CompareInt(data.m_GroupId)
			&& snapshot.CompareString(data.m_Name)
			&& snapshot.CompareString(data.m_FactionKey)
			&& snapshot.CompareString(data.m_PrefabPath)
			&& snapshot.CompareString(data.m_IconPath)
			&& snapshot.CompareString(data.m_IconSetName)
			&& snapshot.CompareBool(data.m_IsLocked);
	}

	static bool Extract(PS_VehicleData data, ScriptCtx ctx, SSnapSerializerBase snapshot)
	{
		snapshot.SerializeInt(data.m_RplId);
		snapshot.SerializeInt(data.m_GroupId);
		snapshot.SerializeString(data.m_Name);
		snapshot.SerializeString(data.m_FactionKey);
		snapshot.SerializeString(data.m_PrefabPath);
		snapshot.SerializeString(data.m_IconPath);
		snapshot.SerializeString(data.m_IconSetName);
		snapshot.SerializeBool(data.m_IsLocked);
		return true;
	}

	static bool Inject(SSnapSerializerBase snapshot, ScriptCtx ctx, PS_VehicleData data)
	{
		snapshot.SerializeInt(data.m_RplId);
		snapshot.SerializeInt(data.m_GroupId);
		snapshot.SerializeString(data.m_Name);
		snapshot.SerializeString(data.m_FactionKey);
		snapshot.SerializeString(data.m_PrefabPath);
		snapshot.SerializeString(data.m_IconPath);
		snapshot.SerializeString(data.m_IconSetName);
		snapshot.SerializeBool(data.m_IsLocked);
		return true;
	}
};
