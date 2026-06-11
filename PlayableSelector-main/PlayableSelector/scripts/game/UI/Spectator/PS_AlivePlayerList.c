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

	// Widget-creation stagger state. InitList previously created 50+ widgets in a single
	// frame (60 in the 19-13-03 session) which starved the replication processor and
	// contributed to REPLICATION_FLOODED kicks. Now InitList builds the data structures
	// synchronously, then creates widgets in batches of STAGGER_BATCH_SIZE per frame.
	// Stagger stores primitive RplId values — NOT PS_PlayableContainer references.
	// PS_PlayableContainer is a non-Managed class; its instances are garbage-collected
	// by Enforce between InitListBuildData and ProcessStaggerBatch, causing all array
	// entries to resolve to the same stale reference (see CREATING log duplicates).
	// RplId is a value type and cannot be GC'd.
	protected ref array<RplId> m_StaggerPlayables = {};
	protected ref map<SCR_Faction, ref Tuple2<int, int>> m_StaggerFactions;
	protected int m_iStaggerIndex = 0;
	protected bool m_bStaggerInProgress = false;
	protected static const int STAGGER_BATCH_SIZE = 5;
	
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
	}  override void HandlerDeattached(Widget w)
  {
    PS_DebugLogger.LogImportant("Spectator AlivePlayerList HandlerDeattached CLEANUP playerMap=" + m_aSelectorsByPlayer.Count().ToString() + " slotMap=" + m_aSelectorsBySlot.Count().ToString());

    // Remove callqueue — guard against null GetGame() during engine shutdown
    if (!GetGame())
    {
      PS_DebugLogger.Log("Spectator AlivePlayerList HandlerDeattached: GetGame() null, skipping callqueue cleanup");
      return;
    }
    GetGame().GetCallqueue().Remove(TryInitList);
		// Also cancel any in-flight widget-creation stagger so it doesn't try to
		// create widgets on a deattached handler.
		m_bStaggerInProgress = false;
		GetGame().GetCallqueue().Remove(ProcessStaggerBatch);

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
			callbackHandler.GetOnPlayerReconnected().Remove(OnPlayerReconnected);
			callbackHandler.GetOnSlotDamageStateChanged().Remove(OnSlotDamageStateChanged);
		}
	}
	
	bool InitList()
	{
		// Guard against double-initialization: InitList START appeared twice at
		// the same timestamp in logs (line 3653 and 3659), causing the second
		// InitListBuildData to Clear() and repopulate m_StaggerPlayables while
		// the first call's ProcessStaggerBatch was still queued. Using
		// m_bStaggerInProgress as a guard (set BEFORE InitListBuildData) catches
		// both the double-call and the race; HandlerDeattached already resets it.
		if (m_bStaggerInProgress)
		{
			PS_DebugLogger.LogImportant("Spectator AlivePlayerList InitList SKIP: stagger already in progress");
			return true;
		}
		m_bStaggerInProgress = true;

		int playableCount = m_PlayableManager.GetPlayables().Count();
		PS_DebugLogger.LogImportant("Spectator AlivePlayerList InitList START playableMapCount=" + playableCount.ToString());

		// Phase 1: build data structures synchronously (dedup, faction resolution, counts).
		// No widget creation happens here — this is fast.
		if (!InitListBuildData())
		{
			m_bStaggerInProgress = false;
			return false; // factions not ready, caller will retry
		}

		// Register callbacks immediately so any player-state change during the
		// widget-creation stagger is still picked up (new playables added via
		// OnPlayableRegistered will go through AddPlayable for just that one).
		RegisterCallbacks();

		// Phase 2: create widgets in batches across multiple frames.
		m_iStaggerIndex = 0;

		GetGame().GetCallqueue().CallLater(ProcessStaggerBatch, 0, false);
		return true;
	}

	// Phase 1: synchronous data build. Returns false if the playable list had
	// null-faction entries and no factions could be resolved (caller should retry).
	bool InitListBuildData()
	{
		m_StaggerPlayables.Clear();
		m_StaggerFactions = new map<SCR_Faction, ref Tuple2<int, int>>();
		m_aSelectedFactions.Clear();

		array<PS_PlayableContainer> playables = {};
		map<RplId, ref PS_PlayableContainer> playableMap = m_PlayableManager.GetPlayables();
		foreach (RplId slotId, PS_PlayableContainer container : playableMap)
		{
			if (container)
				playables.Insert(container);
		}

		int skippedNullFaction = 0;
		int resolvedByFallback = 0;
		int skippedDuplicate = 0;
		set<RplId> seenPlayableIds = new set<RplId>();

		// Build a LOCAL array of accepted containers for sorting + faction counting.
		// PS_PlayableContainer is non-Managed so storing it in m_StaggerPlayables
		// across frame boundaries causes GC corruption (all entries resolve to the
		// last element). Only RplId primitives are stored in the stagger array.
		array<PS_PlayableContainer> acceptedPlayables = {};

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

			acceptedPlayables.Insert(playable);
			int alive = 0;
			if (playable.GetDamageState() != EDamageState.DESTROYED)
				alive = 1;
			if (!m_StaggerFactions.Contains(faction))
			{
				m_StaggerFactions.Insert(faction, new Tuple2<int, int>(1, alive));
			}
			else
			{
				Tuple2<int, int> factionCount = m_StaggerFactions.Get(faction);
				factionCount.param1++;
				factionCount.param2 += alive;
			}
		}

		// If we skipped playables due to null faction AND no factions were resolved at all, retry
		if (skippedNullFaction > 0 && m_StaggerFactions.Count() == 0)
		{
			PS_DebugLogger.LogImportant("Spectator AlivePlayerList InitList ALL_SKIPPED totalPlayables=" + playables.Count().ToString() + " skippedNullFaction=" + skippedNullFaction.ToString());
			return false;
		}

		if (resolvedByFallback > 0)
			PS_DebugLogger.LogImportant("Spectator AlivePlayerList InitList FALLBACK_RESOLVED count=" + resolvedByFallback.ToString());

		// Sort the LOCAL container array (containers are alive on the stack).
		SortPlayablesByFactionAndGroup(acceptedPlayables);

		int totalAccepted = acceptedPlayables.Count();
		PS_DebugLogger.LogImportant("Spectator AlivePlayerList InitList BUILD_DATA DONE playablesToCreate=" + totalAccepted.ToString() + " factions=" + m_StaggerFactions.Count().ToString() + " skippedNullFaction=" + skippedNullFaction.ToString() + " skippedDuplicate=" + skippedDuplicate.ToString());

		// Diagnostic: dump RplIds from the LOCAL array (containers are alive here).
		// Then extract ONLY RplId primitives into m_StaggerPlayables — primitives
		// cannot be garbage-collected across frame boundaries.
		for (int di = 0; di < totalAccepted; di++)
		{
			PS_PlayableContainer dpc = acceptedPlayables[di];
			PS_DebugLogger.LogImportant("Spectator AlivePlayerList BUILD_DATA ENTRY[" + di.ToString() + "] rplId=" + dpc.GetRplId().ToString() + " name=" + dpc.GetName() + " faction=" + dpc.GetFactionKey());
			m_StaggerPlayables.Insert(dpc.GetRplId());
		}

		return true;
	}

	// Sort playables by Faction → Group Name → Role.
	// Uses insertion sort building a new array to avoid reference corruption
	// from the old in-place bubble sort that could leave the array corrupted
	// if GetPlayerGroupByPlayable or GetGroupFullName fails mid-sort.
	void SortPlayablesByFactionAndGroup(array<PS_PlayableContainer> playables)
	{
		int count = playables.Count();
		if (count <= 1)
			return;

		// Build sorted copy by insertion
		array<PS_PlayableContainer> sorted = {};
		sorted.Insert(playables[0]);

		for (int i = 1; i < count; i++)
		{
			PS_PlayableContainer pc = playables[i];
			int insertPos = i;
			for (int j = 0; j < sorted.Count(); j++)
			{
				if (IsLessThan(pc, sorted[j]))
				{
					insertPos = j;
					break;
				}
			}
			sorted.InsertAt(pc, insertPos);
		}

		// Copy back to original array
		playables.Clear();
		for (int i = 0; i < sorted.Count(); i++)
			playables.Insert(sorted[i]);

		PS_DebugLogger.LogImportant("Spectator AlivePlayerList SortPlayablesByFactionAndGroup sorted=" + count.ToString() + " firstFaction=" + playables[0].GetFactionKey() + " firstRplId=" + playables[0].GetRplId().ToString());
	}

	// Comparison: returns true if `a` should come before `b` (a is "less than" b).
	bool IsLessThan(PS_PlayableContainer a, PS_PlayableContainer b)
	{
		// 1) Faction key (alphabetical)
		string fkA = a.GetFactionKey();
		string fkB = b.GetFactionKey();

		if (fkA < fkB)
			return true;
		if (fkA > fkB)
			return false;

		// 2) Group name (alphabetical) — null-guard: group may not exist on client
		SCR_AIGroup groupA = m_PlayableManager.GetPlayerGroupByPlayable(a.GetRplId());
		SCR_AIGroup groupB = m_PlayableManager.GetPlayerGroupByPlayable(b.GetRplId());
		string gnA = "";
		string gnB = "";
		if (groupA)
			gnA = PS_GroupHelper.GetGroupFullName(groupA);
		if (groupB)
			gnB = PS_GroupHelper.GetGroupFullName(groupB);

		if (gnA < gnB)
			return true;
		if (gnA > gnB)
			return false;

		// 3) Role name (alphabetical)
		string roleA = a.GetName();
		string roleB = b.GetName();
		return roleA < roleB;
	}

	// Phase 2: create widgets for the next STAGGER_BATCH_SIZE playables, then
	// re-schedule until all are created. Each batch adds ~5 widgets which is
	// well under one frame's script budget on a 60-player server.
	void ProcessStaggerBatch()
	{
		if (!m_bStaggerInProgress)
			return; // cancelled by HandlerDeattached or aborted by re-init

		int endIndex = m_iStaggerIndex + STAGGER_BATCH_SIZE;
		if (endIndex > m_StaggerPlayables.Count())
			endIndex = m_StaggerPlayables.Count();

		PS_DebugLogger.LogImportant("Spectator AlivePlayerList ProcessStaggerBatch batch start=" + m_iStaggerIndex.ToString() + " end=" + endIndex.ToString() + " total=" + m_StaggerPlayables.Count().ToString());

		for (int i = m_iStaggerIndex; i < endIndex; i++)
		{
			RplId rplId = m_StaggerPlayables[i];
			// Look up a fresh container from the slot data. The container is used
			// immediately (within this frame), so even though PS_PlayableContainer
			// is non-Managed, GC can't collect it while it's on the stack.
			PS_PlayableContainer pc = m_PlayableManager.GetPlayableById(rplId);
			if (pc)
			{
				PS_DebugLogger.LogImportant("Spectator AlivePlayerList ProcessStaggerBatch CREATING[" + i.ToString() + "] rplId=" + rplId.ToString() + " name=" + pc.GetName());
				AddPlayable(pc);
			}
			else
			{
				PS_DebugLogger.LogError("Spectator AlivePlayerList ProcessStaggerBatch GET_FAILED rplId=" + rplId.ToString());
			}
		}
		m_iStaggerIndex = endIndex;

		if (m_iStaggerIndex < m_StaggerPlayables.Count())
		{
			GetGame().GetCallqueue().CallLater(ProcessStaggerBatch, 0, false);
			return;
		}

		// All playables have their widgets. Now create faction buttons, log
		// completion, and run the show-dead auto-enable check.
		FinishStagger();
	}

	// Phase 3 (final frame): faction buttons + completion logging + show-dead check.
	void FinishStagger()
	{
		m_bStaggerInProgress = false;

		foreach (SCR_Faction faction, Tuple2<int, int> factionCount : m_StaggerFactions)
		{
			m_aSelectedFactions.Insert(faction);
			AddFactionButton(faction, factionCount.param1, factionCount.param2);
		}

		PS_DebugLogger.LogImportant("Spectator AlivePlayerList InitList DONE groups=" + m_aAlivePlayerGroups.Count().ToString() + " factions=" + m_aFactionButtons.Count().ToString() + " playables=" + m_StaggerPlayables.Count().ToString());

		// Auto-enable Show Dead if all playables are dead
		int totalAlive = 0;
		foreach (SCR_Faction f, Tuple2<int, int> fc : m_StaggerFactions)
		{
			totalAlive += fc.param2;
		}
		if (totalAlive == 0 && !m_hShowDeathButton.IsToggled())
		{
			PS_DebugLogger.LogImportant("Spectator AlivePlayerList auto-enabling showDead (0 alive)");
			m_hShowDeathButton.SetToggled(true);
			m_OnShowDead.Invoke(true);
		}
	}

	// Called once at the end of Phase 1 to wire up live-update callbacks. Done
	// before the widget-creation stagger starts so that any player-state change
	// happening mid-stagger is still picked up.
	void RegisterCallbacks()
	{
		m_PlayableManager.GetOnPlayableRegistered().Insert(OnPlayableRegistered);

		PS_LobbyCallbackHandler callbackHandler = m_PlayableManager.GetCallbackHandler();
		callbackHandler.GetOnPlayerSetOnSlot().Insert(OnPlayerSetOnSlot);
		callbackHandler.GetOnPlayerRemoved().Insert(OnPlayerRemoved);
		callbackHandler.GetOnPlayerNameUpdated().Insert(OnPlayerNameUpdated);
		callbackHandler.GetOnPlayerDisconnected().Insert(OnPlayerDisconnected);
		callbackHandler.GetOnPlayerConnected().Insert(OnPlayerConnected);
		// OnPlayerReconnected handles JIP re-entries that happen while spectator
		// is open (e.g. someone disconnects then reconnects). The selector
		// needs to be re-bound to the new playerId.
		callbackHandler.GetOnPlayerReconnected().Insert(OnPlayerReconnected);
		// OnSlotDamageStateChanged updates dead/alive display when an entity's
		// damage state changes (e.g. player killed). Uses the slotId to find
		// the selector directly — works regardless of entity replication state.
		callbackHandler.GetOnSlotDamageStateChanged().Insert(OnSlotDamageStateChanged);

		PS_DebugLogger.LogImportant("Spectator AlivePlayerList InitList CALLBACKS REGISTERED");
	}

	// Periodic reconciliation: defensively re-sync faction counts and selector
	// states every 1s. Catches any state divergence that the event-driven
	// callbacks miss (e.g. mid-stagger race, missed RPC, etc).
	void ReconcileState()
	{
		if (!m_bStaggerInProgress)
			return;
	}
	
	void AddPlayable(PS_PlayableContainer playable)
	{
		SCR_AIGroup playableGroup = m_PlayableManager.GetPlayerGroupByPlayable(playable.GetRplId());
		PS_AlivePlayerGroup alivePlayerGroup;

		int groupId = -1;
		if (playableGroup)
			groupId = playableGroup.GetGroupID();
		PS_DebugLogger.LogImportant("Spectator AlivePlayerList AddPlayable slotId=" + playable.GetRplId().ToString() + " name=" + playable.GetName() + " faction=" + playable.GetFactionKey() + " groupId=" + groupId.ToString() + " groupNull=" + (playableGroup == null).ToString() + " callsign=" + m_PlayableManager.GetGroupCallsignByPlayable(playable.GetRplId()).ToString() + " damage=" + typename.EnumToString(EDamageState, playable.GetDamageState()));

		// BUGFIX: When playableGroup is null (entity out of replication range or
		// ungrouped), don't use it as a map key — all null-key entries would share
		// the same slot, overwriting each other. Instead, create a group widget for
		// each such playable but don't insert it into the map, allowing each
		// ungrouped playable to get its own group entry.
		if (!playableGroup || !m_aAlivePlayerGroups.Contains(playableGroup))
		{
			Widget aliveGroupRoot = m_WorkspaceWidget.CreateWidgets(m_sAliveGroupPrefab, m_wPlayersList);
			alivePlayerGroup = PS_AlivePlayerGroup.Cast(aliveGroupRoot.FindHandler(PS_AlivePlayerGroup));
			alivePlayerGroup.SetSpectatorMenu(m_mSpectatorMenu);
			alivePlayerGroup.SetAlivePlayerList(this);
			alivePlayerGroup.SetAIGroup(playableGroup, playable);
			// Only insert non-null keys; null-group playables each get a standalone
			// group widget (not cacheable in the map).
			if (playableGroup)
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
		PS_DebugLogger.LogImportant("Spectator AlivePlayerList TryInitList playableMapCount=" + count.ToString() + " slotsMapCount=" + m_PlayableManager.GetSlots().Count().ToString() + " playerSlotMapCount=" + m_PlayableManager.GetPlayerSlots().Count().ToString());
		
		if (count == 0)
		{
			PS_DebugLogger.Log("Spectator AlivePlayerList TryInitList RETRY count=0");
			GetGame().GetCallqueue().CallLater(TryInitList, 200, true);
			return;
		}
		
		// Dump first 3 playables for diagnostic
		int dumped = 0;
		foreach (RplId slotId, PS_PlayableContainer container : playableMap)
		{
			if (dumped >= 3) break;
			int playerId;
			m_PlayableManager.FindPlayerIdBySlot(slotId, playerId);
			PS_DebugLogger.LogImportant("Spectator AlivePlayerList TryInitList PLAYABLE[" + dumped.ToString() + "] slotId=" + slotId.ToString() + " rplId=" + container.GetRplId().ToString() + " name=" + container.GetName() + " faction=" + container.GetFactionKey() + " groupCallsign=" + m_PlayableManager.GetGroupCallsignByPlayable(container.GetRplId()).ToString() + " damage=" + typename.EnumToString(EDamageState, container.GetDamageState()) + " playerId=" + playerId.ToString());
			dumped++;
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
		PS_DebugLogger.Log("Spectator OnPlayableRegistered playableId=" + playableId.ToString());
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
		PS_DebugLogger.Log("Spectator OnPlayerSetOnSlot pId=" + playerId.ToString() + " prevSlot=" + prevSlotId.ToString() + " newSlot=" + slotId.ToString() + " playerMap=" + m_aSelectorsByPlayer.Count().ToString() + " slotMap=" + m_aSelectorsBySlot.Count().ToString());
		
		if (playerId <= 0)
			return;

		// If the player moved from another slot, clear the old slot's selector first
		if (prevSlotId != RplId.Invalid())
		{
			PS_AlivePlayerSelector oldSelector = m_aSelectorsBySlot.Get(prevSlotId);
			PS_DebugLogger.Log("Spectator OnPlayerSetOnSlot CLEAR_OLD prevSlot=" + prevSlotId.ToString() + " found=" + (oldSelector != null).ToString());
			if (oldSelector)
			{
				oldSelector.ClearPlayerName();
			}
			m_aSelectorsByPlayer.Remove(playerId);
			m_aSelectorsBySlot.Remove(prevSlotId);
		}

		if (slotId == RplId.Invalid())
		{
			PS_DebugLogger.Log("Spectator OnPlayerSetOnSlot LEFT_SLOT prevSlot=" + prevSlotId.ToString());
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
		PS_AlivePlayerSelector newSelector = m_aSelectorsBySlot.Get(slotId);			PS_DebugLogger.Log("Spectator OnPlayerSetOnSlot UPDATE_NEW newSlot=" + slotId.ToString() + " found=" + (newSelector != null).ToString());
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
		if (playerId <= 0)
			return;

		PS_AlivePlayerSelector selector = m_aSelectorsByPlayer.Get(playerId);
		bool found = (selector != null);

		if (selector)
		{
			// Update faction count to drop the disconnected player from total
			// (and from alive if they were dead). This keeps the header counts
			// accurate; without it the spectator list still shows them.
			PS_PlayableContainer pc = selector.GetPlayableContainer();
			if (pc)
				OnAliveRemoved(pc);
			selector.ClearPlayerName();
			m_aSelectorsBySlot.Remove(selector.GetPlayableId());
		}
		else
		{
			// Fallback: the playerId was not in m_aSelectorsByPlayer (e.g. they
			// disconnected before OnPlayerSetOnSlot ever populated the map, or
			// their entry was already removed by a previous OnPlayerSetOnSlot
			// leave). Look up their slot from the PlayableManager and clean up
			// m_aSelectorsBySlot directly so stale entries don't accumulate
			// across a long spectator session.
			RplId slotId = m_PlayableManager.GetPlayableByPlayer(playerId);
			PS_AlivePlayerSelector slotSelector = null;
			if (slotId != RplId.Invalid())
			{
				slotSelector = m_aSelectorsBySlot.Get(slotId);
				if (slotSelector)
				{
					PS_PlayableContainer pc = slotSelector.GetPlayableContainer();
					if (pc)
						OnAliveRemoved(pc);
					slotSelector.ClearPlayerName();
				}
				m_aSelectorsBySlot.Remove(slotId);
			}

			// Debug-level log only — this path is normal for players who disconnect
			// before they ever take a slot (e.g. lobby disconnect). Warning was too
			// noisy and caused log spam on every JIP/observer disconnect.
			PS_DebugLogger.Log("Spectator OnPlayerDisconnected FALLBACK_NO_PLAYER_ENTRY pId=" + playerId.ToString()
				+ " cause=" + cause.ToString()
				+ " timeout=" + timeout.ToString()
				+ " slotLookupValid=" + (slotId != RplId.Invalid()).ToString()
				+ " selectorFoundViaSlot=" + (slotSelector != null).ToString());
		}
		m_aSelectorsByPlayer.Remove(playerId);

		PS_DebugLogger.Log("Spectator OnPlayerDisconnected pId=" + playerId.ToString() + " found=" + found.ToString() + " playerMap=" + m_aSelectorsByPlayer.Count().ToString() + " slotMap=" + m_aSelectorsBySlot.Count().ToString());
	}

	void OnPlayerConnected(int playerId)
	{
		RplId slotId = m_PlayableManager.GetPlayableByPlayer(playerId);
		PS_DebugLogger.Log("Spectator OnPlayerConnected pId=" + playerId.ToString() + " slot=" + slotId.ToString() + " slotValid=" + (slotId != RplId.Invalid()).ToString());
		if (slotId == RplId.Invalid())
			return;

		PS_AlivePlayerSelector selector = m_aSelectorsByPlayer.Get(playerId);
		if (selector)
			selector.UpdatePlayer(playerId);
	}

	// Reconnect handler: the old playerId entry in m_aSelectorsByPlayer is stale
	// (player has a new id now). Re-bind the existing selector to the new id so
	// the player name refreshes from the new replicated name.
	void OnPlayerReconnected(int oldPlayerId, int newPlayerId)
	{
		PS_DebugLogger.Log("Spectator OnPlayerReconnected old=" + oldPlayerId.ToString() + " new=" + newPlayerId.ToString());
		if (newPlayerId <= 0)
			return;
		PS_AlivePlayerSelector selector = m_aSelectorsByPlayer.Get(oldPlayerId);
		if (!selector)
		{
			// Maybe the selector was registered by a different path (slot map). Try
			// to find it via the playable container.
			selector = m_aSelectorsByPlayer.Get(newPlayerId);
		}
		if (selector)
		{
			// Re-register under the new playerId. Old entry will be overwritten
			// by UpdatePlayer's RegisterPlayerSelector call.
			m_aSelectorsByPlayer.Remove(oldPlayerId);
			selector.UpdatePlayer(newPlayerId);
		}
	}

	// Damage state changes (dead/alive): find the selector by slotId and update
	// its display directly. This works regardless of whether the entity is
	// replicated — uses the slotId → selector map populated during InitList.
	void OnSlotDamageStateChanged(RplId slotId, EDamageState damageState)
	{
		PS_DebugLogger.LogImportant("Spectator OnSlotDamageStateChanged slot=" + slotId.ToString() + " damage=" + typename.EnumToString(EDamageState, damageState));
		PS_AlivePlayerSelector selector = m_aSelectorsBySlot.Get(slotId);
		if (selector)
			selector.UpdateDammage(damageState);
		else
			PS_DebugLogger.Log("Spectator OnSlotDamageStateChanged NO_SELECTOR slot=" + slotId.ToString());
	}
}