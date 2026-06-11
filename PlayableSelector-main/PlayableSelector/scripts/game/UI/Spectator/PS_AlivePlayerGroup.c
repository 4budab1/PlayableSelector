class PS_AlivePlayerGroup : SCR_ScriptedWidgetComponent
{
	// Const
	protected ResourceName m_sAlivePlayerSelectorPrefab = "{20DCB7288210C151}UI/Spectator/AlivePlayerSelector.layout";
	
	// Widgets
	protected VerticalLayoutWidget m_PlayersVerticalLayout;
	protected TextWidget m_wGroupName;
	protected ImageWidget m_wGroupFactionColor;
	protected ButtonWidget m_wAlivePlayerGroupButton;
	
	// Cache global
	protected WorkspaceWidget m_WorkspaceWidget;
	
	// Parameters
	protected PS_AlivePlayerList m_AlivePlayerList;
	protected PS_SpectatorMenu m_SpectatorMenu;
	protected SCR_AIGroup m_AIGroup;
	
	// Handler
	SCR_ButtonBaseComponent m_hAlivePlayerGroupButton;
	
	// Init
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		
		// Cache global
		m_WorkspaceWidget = GetGame().GetWorkspace();
		
		// Widgets
		m_PlayersVerticalLayout = VerticalLayoutWidget.Cast(w.FindWidget("PlayersVerticalLayout"));
		m_wGroupName = TextWidget.Cast(w.FindAnyWidget("GroupName"));
		m_wGroupFactionColor = ImageWidget.Cast(w.FindAnyWidget("GroupFactionColor"));
		m_wAlivePlayerGroupButton = ButtonWidget.Cast(w.FindAnyWidget("AlivePlayerGroupButton"));
		
		// Handler
		m_hAlivePlayerGroupButton = SCR_ButtonBaseComponent.Cast(m_wAlivePlayerGroupButton.FindHandler(SCR_ButtonBaseComponent));
		
		m_hAlivePlayerGroupButton.m_OnClicked.Insert(OnGroupClick);
	}
	
	void SetSpectatorMenu(PS_SpectatorMenu spectatorMenu)
	{
		m_SpectatorMenu = spectatorMenu;
	}
	
	void SetAlivePlayerList(PS_AlivePlayerList alivePlayerList)
	{
		m_AlivePlayerList = alivePlayerList;
	}
	
	void SetAIGroup(SCR_AIGroup aiGroup, PS_PlayableContainer playable = null)
	{
		m_AIGroup = aiGroup;
		
		// Init: group name from AI group, or fallback for ungrouped playables
		string groupName = PS_GroupHelper.GetGroupFullName(m_AIGroup);
		if (groupName == "" && playable)
			groupName = playable.GetName();
		if (groupName == "")
			groupName = "Ungrouped";
		m_wGroupName.SetText(groupName);

		// Resolve faction: try AI group first, then playable (replicated slot data),
		// then FactionManager by key. The AI group's GetFaction() depends on the
		// entity being within replication range (~1500m); the slot's faction key
		// is always available from the replicated slot data.
		Faction faction = null;
		if (m_AIGroup)
			faction = m_AIGroup.GetFaction();
		if (!faction && playable)
		{
			faction = playable.GetFaction();
			if (!faction)
			{
				SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
				if (fm)
					faction = SCR_Faction.Cast(fm.GetFactionByKey(playable.GetFactionKey()));
			}
		}

		if (faction)
		{
			Color factionColor = faction.GetFactionColor();
			m_wGroupFactionColor.SetColor(factionColor);
		}
	}
	
	void InsertPlayable(PS_PlayableContainer playable)
	{
		Widget alivePlayerRoot = m_WorkspaceWidget.CreateWidgets(m_sAlivePlayerSelectorPrefab, m_PlayersVerticalLayout);
		PS_AlivePlayerSelector alivePlayerSelector = PS_AlivePlayerSelector.Cast(alivePlayerRoot.FindHandler(PS_AlivePlayerSelector));
		alivePlayerSelector.SetAliveGroup(this);
		alivePlayerSelector.SetSpectatorMenu(m_SpectatorMenu);
		alivePlayerSelector.SetAlivePlayerList(m_AlivePlayerList);
		alivePlayerSelector.SetPlayableContainer(playable);
	}
	
	void OnAliveRemoved(PS_PlayableContainer playableComponent)
	{
		if (!m_PlayersVerticalLayout.GetChildren())
		{
			m_wRoot.RemoveFromHierarchy();
			m_AlivePlayerList.OnAliveGroupRemoved(m_AIGroup);
		}
	}
	
	void OnGroupClick(SCR_ButtonBaseComponent button)
	{
		m_PlayersVerticalLayout.SetVisible(!m_PlayersVerticalLayout.IsVisible());
	}
}











