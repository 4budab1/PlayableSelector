// Widget displays info about alive players in game.
// Path: {18D3CF175C9AA974}UI/Spectator/AlivePlayersList.layout

class PS_AlivePlayerList : ScriptedWidgetComponent
{
	// Const
	protected ResourceName m_sAliveGroupPrefab = "{71DA7743CE51DABF}UI/Spectator/AlivePlayerGroup.layout";
	protected ResourceName m_sAliveFactionButtonPrefab = "{4959360D4111DACC}UI/Spectator/AliveFactionButton.layout";
	
	// Cache global
	protected PS_PlayableManager m_PlayableManager;
	protected PlayerController m_PlayerController;
	protected SCR_FactionManager m_FactionManager;
	protected WorkspaceWidget m_WorkspaceWidget;
	
	// Widgets
	protected VerticalLayoutWidget m_wPlayersList;
	protected HorizontalLayoutWidget m_wHorizontalLayoutFactions;
	protected ScrollLayoutWidget m_wAlivePlayersListScroll;
	protected ButtonWidget m_wShowDeathButton;
	
	// Handlers
	protected SCR_ButtonBaseComponent m_hShowDeathButton;
	
	// Parameters
	protected PS_SpectatorMenu m_mSpectatorMenu;
	
	// Vars
	protected ref map<SCR_AIGroup, PS_AlivePlayerGroup> m_aAlivePlayerGroups = new map<SCR_AIGroup, PS_AlivePlayerGroup>();
	protected ref map<Faction, PS_AliveFactionButton> m_aFactionButtons = new map<Faction, PS_AliveFactionButton>();
	protected ref array<Faction> m_aSelectedFactions = {};
	protected ref map<int, PS_AlivePlayerSelector> m_aSelectorsByPlayer = new map<int, PS_AlivePlayerSelector>();
	protected ref map<RplId, PS_AlivePlayerSelector> m_aSelectorsBySlot = new map<RplId, PS_AlivePlayerSelector>();
	
	ref ScriptInvokerBool m_OnShowDead = new ScriptInvokerBool();
	ScriptInvokerBool GetOnShowDead()
	{
		return m_OnShowDead;
	}
	
	override void HandlerAttached(Widget w)
	{
		PS_DebugLogger.LogImportant("Spectator AlivePlayerList HandlerAttached");
		
		// Widgets
		m_wPlayersList = VerticalLayoutWidget.Cast(w.FindAnyWidget("AlivePlayersList"));
		m_wHorizontalLayoutFactions = HorizontalLayoutWidget.Cast(w.FindAnyWidget("HorizontalLayoutFactions"));
		m_wAlivePlayersListScroll = ScrollLayoutWidget.Cast(w.FindAnyWidget("AlivePlayersListScroll"));
		m_wShowDeathButton = ButtonWidget.Cast(w.FindAnyWidget("ShowDeathButton"));
		
		// Handlers
		m_hShowDeathButton = SCR_ButtonBaseComponent.Cast(m_wShowDeathButton.FindHandler(SCR_ButtonBaseComponent));
		
		// Cache global
		m_PlayableManager = PS_PlayableManager.GetInstance();
		m_PlayerController = GetGame().GetPlayerController();
		m_FactionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		m_WorkspaceWidget = GetGame().GetWorkspace();
		
		// Buttons
		m_hShowDeathButton.m_OnClicked.Insert(ShowDeadButtonClicked);
	}
	
	override void HandlerDeattached(Widget w)
	{
		PS_DebugLogger.LogImportant("Spectator AlivePlayerList HandlerDeattached CLEANUP playerMap=" + m_aSelectorsByPlayer.Count().ToString() + " slotMap=" + m_aSelectorsBySlot.Count().ToString());
		
		// Remove callqueue
		GetGame().GetCallqueue().Remove(TryInitList);
		
		// Remove callbacks
		if (m_PlayableManager)
		{
			m_PlayableManager.GetOnPlayableRegistered().Remove(OnPlayableRegistered);
			
			PS_LobbyCallbackHandler callbackHandler = m_PlayableManager.GetCallbackHandler();
			callbackHandler.GetOnPlayerSetOnSlot().Remove(OnPlayerSetOnSlot);
			callbackHandler.GetOnPlayerRemoved().Remove(OnPlayerRemoved);
			callbackHandler.GetOnPlayerNameUpdated().Remove(OnPlayerNameUpdated);
			callbackHandler.GetOnPlayerDisconnected().Remove(OnPlayerDisconnected);
			callbackHandler.GetOnPlayerConnected().Remove(OnPlayerConnected);
		}
	}
	
	bool InitList()
	{
		array<PS_PlayableContainer> playables = {};
		map<RplId, ref PS_PlayableContainer> playableMap = m_PlayableManager.GetPlayables();
		foreach (RplId slotId, PS_PlayableContainer container : playableMap)
		{
			if (container)
				playables.Insert(container);
		}
		
		map<SCR_Faction, ref Tuple2<int, int>> factions = new map<SCR_Faction, ref Tuple2<int, int>>();
		int skippedNullFaction = 0;
		int resolvedByFallback = 0;
		int skippedDuplicate = 0;
		set<RplId> seenPlayableIds = new set<RplId>();
		
		foreach (PS_PlayableContainer playable : playables)
		{
			RplId rplId = playable.GetRplId();
			if (seenPlayableIds.Contains(rplId))
			{
				PS_DebugLogger.LogImportant("Spectator AlivePlayerList InitList SKIP_DUPLICATE rplId=" + rplId.ToString());
				skippedDuplicate++;
				continue;
			}
			seenPlayableIds.Insert(rplId);
			
			SCR_Faction faction = playable.GetFaction();
			if (!faction)
			{
				string factionKey = playable.GetFactionKey();
				if (m_FactionManager)
					faction = SCR_Faction.Cast(m_FactionManager.GetFactionByKey(factionKey));
				
				if (!faction)
				{
					PS_DebugLogger.LogError("Spectator AlivePlayerList InitList SKIP: null faction rplId=" + rplId.ToString() + " factionKey=" + factionKey + " fmGetByKey=" + (m_FactionManager != null).ToString());
					skippedNullFaction++;
					continue;
				}
				resolvedByFallback++;
			}

			AddPlayable(playable);
			int alive = 0;
			if (playable.GetDamageState() != EDamageState.DESTROYED)
				alive = 1;
			if (!factions.Contains(faction))
			{
				factions.Insert(faction, new Tuple2<int, int>(1, alive));
			}
			else
			{
				Tuple2<int, int> factionCount = factions.Get(faction);
				factionCount.param1++;
				factionCount.param2 += alive;
			}
		}
		
		// If we skipped playables due to null faction AND no factions were resolved at all, retry
		if (skippedNullFaction > 0 && factions.Count() == 0)
		{
			PS_DebugLogger.LogImportant("Spectator AlivePlayerList InitList ALL_SKIPPED totalPlayables=" + playables.Count().ToString() + " skippedNullFaction=" + skippedNullFaction.ToString());
			return false;
		}
		
		if (resolvedByFallback > 0)
			PS_DebugLogger.LogImportant("Spectator AlivePlayerList InitList FALLBACK_RESOLVED count=" + resolvedByFallback.ToString());
		
		foreach (SCR_Faction faction, Tuple2<int, int> factionCount : factions)
		{
			m_aSelectedFactions.Insert(faction);
			AddFactionButton(faction, factionCount.param1, factionCount.param2);
		}
		
		PS_DebugLogger.LogImportant("Spectator AlivePlayerList InitList DONE groups=" + m_aAlivePlayerGroups.Count().ToString() + " factions=" + m_aFactionButtons.Count().ToString() + " playables=" + playables.Count().ToString() + " skippedNullFaction=" + skippedNullFaction.ToString() + " skippedDuplicate=" + skippedDuplicate.ToString());
		
		// Added in runtime
		m_PlayableManager.GetOnPlayableRegistered().Insert(OnPlayableRegistered);

		PS_LobbyCallbackHandler callbackHandler = m_PlayableManager.GetCallbackHandler();
		callbackHandler.GetOnPlayerSetOnSlot().Insert(OnPlayerSetOnSlot);
		callbackHandler.GetOnPlayerRemoved().Insert(OnPlayerRemoved);
		callbackHandler.GetOnPlayerNameUpdated().Insert(OnPlayerNameUpdated);
		callbackHandler.GetOnPlayerDisconnected().Insert(OnPlayerDisconnected);
		callbackHandler.GetOnPlayerConnected().Insert(OnPlayerConnected);
		
		PS_DebugLogger.LogImportant("Spectator AlivePlayerList InitList CALLBACKS REGISTERED");
		
		// Auto-enable Show Dead if all playables are dead
		int totalAlive = 0;
		foreach (SCR_Faction f, Tuple2<int, int> fc : factions)
		{
			totalAlive += fc.param2;
		}
		if (totalAlive == 0 && !m_hShowDeathButton.IsToggled())
		{
			PS_DebugLogger.LogImportant("Spectator AlivePlayerList auto-enabling showDead (0 alive)");
			m_hShowDeathButton.SetToggled(true);
			m_OnShowDead.Invoke(true);
		}
		
		return true;
	}
	
	void AddPlayable(PS_PlayableContainer playable)
	{
		SCR_AIGroup playableGroup = m_PlayableManager.GetPlayerGroupByPlayable(playable.GetRplId());
		PS_AlivePlayerGroup alivePlayerGroup;
		if (!m_aAlivePlayerGroups.Contains(playableGroup))
		{
			Widget aliveGroupRoot = m_WorkspaceWidget.CreateWidgets(m_sAliveGroupPrefab, m_wPlayersList);
			alivePlayerGroup = PS_AlivePlayerGroup.Cast(aliveGroupRoot.FindHandler(PS_AlivePlayerGroup));
			alivePlayerGroup.SetAIGroup(playableGroup);	
			alivePlayerGroup.SetSpectatorMenu(m_mSpectatorMenu);
			alivePlayerGroup.SetAlivePlayerList(this);
			m_aAlivePlayerGroups.Insert(playableGroup, alivePlayerGroup);
		}
		else alivePlayerGroup = m_aAlivePlayerGroups.Get(playableGroup);
		alivePlayerGroup.InsertPlayable(playable);
	}
	
	void AddFactionButton(SCR_Faction faction, int count, int countAlive)
	{
		Widget aliveFactionRoot = m_WorkspaceWidget.CreateWidgets(m_sAliveFactionButtonPrefab, m_wHorizontalLayoutFactions);
		bool factionSelected = m_aSelectedFactions.Contains(faction);
		aliveFactionRoot.SetVisible(factionSelected);
		PS_AliveFactionButton aliveFactionButton = PS_AliveFactionButton.Cast(aliveFactionRoot.FindHandler(PS_AliveFactionButton));
		aliveFactionButton.SetFaction(faction);
		aliveFactionButton.SetCount(count);
		aliveFactionButton.SetCountAlive(countAlive);
		aliveFactionButton.m_OnClicked.Insert(FactionButtonClicked);
		m_aFactionButtons.Insert(faction, aliveFactionButton);
	}
	
	void SetSpectatorMenu(PS_SpectatorMenu spectatorMenu)
	{
		m_mSpectatorMenu = spectatorMenu;
		TryInitList();
	}
	
	void TryInitList()
	{
		map<RplId, ref PS_PlayableContainer> playableMap = m_PlayableManager.GetPlayables();
		int count = playableMap.Count();
		if (count == 0)
		{
			PS_DebugLogger.Log("Spectator AlivePlayerList TryInitList RETRY count=0");
			GetGame().GetCallqueue().CallLater(TryInitList, 200, true);
			return;
		}
		GetGame().GetCallqueue().Remove(TryInitList);
		PS_DebugLogger.LogImportant("Spectator AlivePlayerList InitList START playableMapCount=" + count.ToString());
		
		if (!InitList())
		{
			// Factions not ready yet, retry in 500ms
			PS_DebugLogger.LogImportant("Spectator AlivePlayerList InitList FAILED (factions not ready), retrying in 500ms");
			GetGame().GetCallqueue().CallLater(TryInitList, 500, false);
		}
	}
	
	void OnPlayableRegistered(RplId playableId, PS_PlayableContainer playable)
	{
		PS_DebugLogger.LogImportant("Spectator OnPlayableRegistered playableId=" + playableId.ToString());
		SCR_Faction faction = playable.GetFaction();
		if (!faction)
		{
			if (m_FactionManager)
				faction = SCR_Faction.Cast(m_FactionManager.GetFactionByKey(playable.GetFactionKey()));
			if (!faction)
			{
				PS_DebugLogger.LogWarning("Spectator OnPlayableRegistered SKIP: null faction rplId=" + playableId.ToString() + " factionKey=" + playable.GetFactionKey());
				return;
			}
		}
		
		AddPlayable(playable);
		
		int addAlive = 0;
		if (playable.GetDamageState() != EDamageState.DESTROYED)
			addAlive = 1;
		AddFactionCount(faction, 1, addAlive);	
	}
	
	void AddFactionCount(SCR_Faction faction, int added, int addedAlive)
	{
		if (!m_aFactionButtons.Contains(faction))
			AddFactionButton(faction, 0, 0);
		PS_AliveFactionButton aliveFactionButton = m_aFactionButtons.Get(faction);
		int count = aliveFactionButton.GetCount();
		int countAlive = aliveFactionButton.GetCountAlive();
		aliveFactionButton.SetCount(count + added);
		aliveFactionButton.SetCountAlive(countAlive + addedAlive);
		if ((count + added) == 0)
		{
			aliveFactionButton.GetRootWidget().RemoveFromHierarchy();
			m_aFactionButtons.Remove(faction);
		}
	}
	
	void OnAliveDie(PS_PlayableContainer playableContainer)
	{
		SCR_Faction faction = playableContainer.GetFaction();
		AddFactionCount(faction, 0, -1);
	}
	
	void OnAliveRemoved(PS_PlayableContainer playableContainer)
	{
		SCR_Faction faction = playableContainer.GetFaction();
		int removeAlive = 0;
		if (playableContainer.GetDamageState() == EDamageState.DESTROYED)
			removeAlive = -1;
		AddFactionCount(faction, -1, removeAlive);
	}
	
	void OnAliveGroupRemoved(SCR_AIGroup group)
	{
		m_aAlivePlayerGroups.Remove(group);
	}
	
	// ETC
	bool IsShowDead()
	{
		return m_hShowDeathButton.IsToggled();
	}
	
	void ShowDeadButtonClicked(SCR_ButtonBaseComponent deadButton)
	{
		PS_DebugLogger.LogImportant("Spectator ShowDeadButton toggled=" + m_hShowDeathButton.IsToggled().ToString());
		m_OnShowDead.Invoke(m_hShowDeathButton.IsToggled());
	}
	
	// -------------------- Buttons events --------------------
	void FactionButtonClicked(SCR_ButtonBaseComponent playerButton)
	{
		m_wAlivePlayersListScroll.SetSliderPos(0, 0);
		
		PS_AliveFactionButton aliveFactionButton = PS_AliveFactionButton.Cast(playerButton);
		if (!aliveFactionButton)
			return;
		SCR_Faction faction = aliveFactionButton.GetFaction();
		if (!faction)
			return;
		
		if (m_aSelectedFactions.Contains(faction))
			m_aSelectedFactions.RemoveItem(faction);
		else
			m_aSelectedFactions.Insert(faction);
			
		foreach (SCR_AIGroup aiGroup, PS_AlivePlayerGroup alivePlayerGroup : m_aAlivePlayerGroups)
		{
			if (!aiGroup)
				continue;
			SCR_Faction goupFaction = SCR_Faction.Cast(aiGroup.GetFaction());
			bool factionSelected = (goupFaction && m_aSelectedFactions.Contains(goupFaction));
			alivePlayerGroup.GetRootWidget().SetVisible(factionSelected);
		}
	}

	void RegisterPlayerSelector(int playerId, RplId playableId, PS_AlivePlayerSelector selector)
	{
		if (playerId > 0)
			m_aSelectorsByPlayer[playerId] = selector;
		m_aSelectorsBySlot[playableId] = selector;
		PS_DebugLogger.Log("Spectator RegisterPlayerSelector pId=" + playerId.ToString() + " slot=" + playableId.ToString() + " playerMap=" + m_aSelectorsByPlayer.Count().ToString() + " slotMap=" + m_aSelectorsBySlot.Count().ToString());
	}

	void UnregisterPlayerSelector(int playerId, RplId playableId)
	{
		m_aSelectorsByPlayer.Remove(playerId);
		m_aSelectorsBySlot.Remove(playableId);
		PS_DebugLogger.Log("Spectator UnregisterPlayerSelector pId=" + playerId.ToString() + " slot=" + playableId.ToString() + " playerMap=" + m_aSelectorsByPlayer.Count().ToString() + " slotMap=" + m_aSelectorsBySlot.Count().ToString());
	}

	void OnPlayerSetOnSlot(int playerId, RplId prevSlotId, RplId slotId)
	{
		PS_DebugLogger.LogImportant("Spectator OnPlayerSetOnSlot pId=" + playerId.ToString() + " prevSlot=" + prevSlotId.ToString() + " newSlot=" + slotId.ToString() + " playerMap=" + m_aSelectorsByPlayer.Count().ToString() + " slotMap=" + m_aSelectorsBySlot.Count().ToString());
		
		if (playerId <= 0)
			return;

		// If the player moved from another slot, clear the old slot's selector first
		if (prevSlotId != RplId.Invalid())
		{
			PS_AlivePlayerSelector oldSelector = m_aSelectorsBySlot.Get(prevSlotId);
			PS_DebugLogger.LogImportant("Spectator OnPlayerSetOnSlot CLEAR_OLD prevSlot=" + prevSlotId.ToString() + " found=" + (oldSelector != null).ToString());
			if (oldSelector)
			{
				oldSelector.ClearPlayerName();
			}
			m_aSelectorsByPlayer.Remove(playerId);
			m_aSelectorsBySlot.Remove(prevSlotId);
		}

		if (slotId == RplId.Invalid())
		{
			PS_DebugLogger.LogImportant("Spectator OnPlayerSetOnSlot LEFT_SLOT prevSlot=" + prevSlotId.ToString());
			// Player left a slot without a previous slot (rare, but handle it)
			if (prevSlotId == RplId.Invalid())
			{
				PS_AlivePlayerSelector selector = m_aSelectorsByPlayer.Get(playerId);
				if (selector)
				{
					selector.ClearPlayerName();
					m_aSelectorsByPlayer.Remove(playerId);
				}
			}
			return;
		}

		// Look up the new slot's selector via the slot map (not player map, which still
		// holds the old selector). UpdatePlayer will re-register in both maps.
		PS_AlivePlayerSelector newSelector = m_aSelectorsBySlot.Get(slotId);
		PS_DebugLogger.LogImportant("Spectator OnPlayerSetOnSlot UPDATE_NEW newSlot=" + slotId.ToString() + " found=" + (newSelector != null).ToString());
		if (newSelector)
			newSelector.UpdatePlayer(playerId);
	}

	void OnPlayerRemoved(int playerId, RplId slotId)
	{
		if (playerId <= 0)
			return;

		PS_AlivePlayerSelector selector = m_aSelectorsByPlayer.Get(playerId);
		if (selector)
			selector.ClearPlayerName();
		m_aSelectorsByPlayer.Remove(playerId);
		m_aSelectorsBySlot.Remove(slotId);
	}

	void OnPlayerNameUpdated(int playerId, string playerName)
	{
		PS_AlivePlayerSelector selector = m_aSelectorsByPlayer.Get(playerId);
		PS_DebugLogger.Log("Spectator OnPlayerNameUpdated pId=" + playerId.ToString() + " found=" + (selector != null).ToString());
		if (selector)
			selector.UpdatePlayer(playerId);
	}

	void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		PS_AlivePlayerSelector selector = m_aSelectorsByPlayer.Get(playerId);
		PS_DebugLogger.LogImportant("Spectator OnPlayerDisconnected pId=" + playerId.ToString() + " found=" + (selector != null).ToString());
		if (selector)
		{
			selector.ClearPlayerName();
			m_aSelectorsBySlot.Remove(selector.GetPlayableId());
		}
		m_aSelectorsByPlayer.Remove(playerId);
	}

	void OnPlayerConnected(int playerId)
	{
		RplId slotId = m_PlayableManager.GetPlayableByPlayer(playerId);
		PS_DebugLogger.LogImportant("Spectator OnPlayerConnected pId=" + playerId.ToString() + " slot=" + slotId.ToString() + " slotValid=" + (slotId != RplId.Invalid()).ToString());
		if (slotId == RplId.Invalid())
			return;

		PS_AlivePlayerSelector selector = m_aSelectorsByPlayer.Get(playerId);
		if (selector)
			selector.UpdatePlayer(playerId);
	}
}