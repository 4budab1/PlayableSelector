class PS_MissionDescriptionClass : GenericEntityClass
{
}

class PS_MissionDescription : GenericEntity
{
	[RplProp(), Attribute("")]
	string m_sTitle;
	[RplProp(), Attribute("")]
	ResourceName m_sDescriptionLayout;
	[RplProp(), Attribute(defvalue: "", uiwidget: UIWidgets.EditBoxMultiline)]
	string m_sTextData;
	
	[RplProp()]
	protected ref ReplicatedBasicMap<FactionKey, bool> m_aVisibleForFactions = new ReplicatedBasicMap<FactionKey, bool>();

	ref map<FactionKey, bool> GetVisibleForFactionsRaw()
	{
		return m_aVisibleForFactions.GetRawMap();
	}

	[RplProp(), Attribute("")]
	bool m_bEmptyFactionVisibility;
	
	[RplProp(), Attribute("")]
	bool m_bShowForAnyFaction;
	
	[RplProp(), Attribute("")]
	int m_iOrder;
	
	int GetOrder()
	{
		return m_iOrder;
	}
	void SetOrder(int order)
	{
		m_iOrder = order;
		Replication.BumpMe();
		Rpc(RPC_SetOrder, order);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetOrder(int order)
	{
		m_iOrder = order;
	}
			
	ResourceName GetDescriptionLayout()
	{
		return m_sDescriptionLayout;
	}
	
	bool GetVisibleForFaction(FactionKey factionKey)
	{
		if (m_bShowForAnyFaction) return true;
		bool visible;
		m_aVisibleForFactions.Find(factionKey, visible);
		return visible;
	}
	void SetVisibleForFaction(Faction faction, bool visible)
	{
		Rpc(RPC_SetVisibleForFaction_ByKey, faction.GetFactionKey(), visible);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_SetVisibleForFaction_ByKey(FactionKey factionKey, bool visible)
	{
		if (visible)
		{
			if (!GetVisibleForFaction(factionKey))
				m_aVisibleForFactions.Insert(factionKey, true);
		}
		else
		{
			if (GetVisibleForFaction(factionKey))
				m_aVisibleForFactions.Remove(factionKey);
		}
		
		if (Replication.IsServer())
			Replication.BumpMe();
	}
	
	bool GetVisibleForEmptyFaction()
	{
		return m_bEmptyFactionVisibility;
	}
	void SetVisibleForEmptyFaction(bool visible)
	{
		m_bEmptyFactionVisibility = visible;
		Replication.BumpMe();
		Rpc(RPC_SetVisibleForEmptyFaction, visible);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_SetVisibleForEmptyFaction(bool visible)
	{
		m_bEmptyFactionVisibility = visible;
	}
	
	void SetLayout(ResourceName layout)
	{
		m_sDescriptionLayout = layout;
		Replication.BumpMe();
	}
	
	string GetTitle()
	{
		return m_sTitle;
	}
	void SetTitle(string title)
	{
		m_sTitle = title;
		Replication.BumpMe();
		Rpc(RPC_SetTitle, title);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetTitle(string title)
	{
		m_sTitle = title;
	}
	

	bool GetShowForAnyFaction()
	{
		return m_bShowForAnyFaction;
	}
	void SetShowForAnyFaction(bool enable)
	{
		m_bShowForAnyFaction = enable;
		Replication.BumpMe();
		Rpc(RPC_SetShowForAnyFaction, enable);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetShowForAnyFaction(bool enable)
	{
		m_bShowForAnyFaction = enable;
	}

	
	string GetTextData()
	{
		return m_sTextData;
	}
	void SetTextData(string textData)
	{
		m_sTextData = textData;
		Replication.BumpMe();
		Rpc(RPC_SetTextData, textData);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_SetTextData(string textData)
	{
		m_sTextData = textData;
	}
	
	// Main functions
	override protected void EOnInit(IEntity owner)
	{
		GetGame().GetCallqueue().CallLater(RegisterToDescriptionManager);
	}
	void RegisterToDescriptionManager()
	{
		PS_MissionDescriptionManager missionDescriptionManager = PS_MissionDescriptionManager.GetInstance();
		if (!missionDescriptionManager)	
			return;
		missionDescriptionManager.RegisterDescription(this);
	}
	
	void PS_MissionDescription(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT);
	}
	
	void ~PS_MissionDescription()
	{
		PS_MissionDescriptionManager missionDescriptionManager = PS_MissionDescriptionManager.GetInstance();
		if (!missionDescriptionManager)
			return;
		missionDescriptionManager.UnregisterDescription(this);
	}
}
