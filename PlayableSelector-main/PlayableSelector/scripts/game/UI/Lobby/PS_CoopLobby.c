// Insert new menu to global pressets enum
// Don't forge modify config {C747AFB6B750CE9A}Configs/System/chimeraMenus.conf, if you do the same.
modded enum ChimeraMenuPreset : ScriptMenuPresetEnum
{
	CoopLobby
}

/*
	Script generate widgets tree:
	Lobby 							- Our lobby widget
	├── FactionList 				- Contains all factions that have playable
	│ └── FactionWidget			- (m_sFactionSelectorPrefab) Set m_fCurrentFaction And update CharactersList
	├── CharactersList 				- Contains all playables from current selected faction (m_fCurrentFaction)
	│ └── GroupList 				- (m_sRolesGroupPrefab) Has no functional just group playable by group name
	│ └── CharacterWidget		- (m_sCharacterSelectorPrefab) Select playable for player
	└── PlayersList 				- Contains all CONNECTED players
		└── PlayerWidget			- (m_sPlayerSelectorPrefab) Kick or mute players.
*/

// Main lobby widget
// Menu preset: ChimeraMenuPreset.CoopLobby
// Path: {9DECCA625D345B35}UI/Lobby/CoopLobby.layout
class PS_CoopLobby : MenuBase
{
	// Const
	const static ResourceName IMAGESET_PS = "{F3A9B47F55BE8D2B}UI/Textures/Icons/PS_Atlas_x64.imageset";
	
	protected ResourceName m_sRolesGroupPrefab = "{B45A0FA6883A7A0E}UI/Lobby/RolesGroup.layout"; // Handler: PS_RolesGroup
	protected ResourceName m_sCharacterSelectorPrefab = "{3F761F63F1DF29D1}UI/Lobby/CharacterSelector.layout"; // Handler: PS_CharacterSelector
	protected ResourceName m_sFactionSelectorPrefab = "{DA22ED7112FA8028}UI/Lobby/FactionSelector.layout"; // Handler: PS_FactionSelector
	
	// Cache global
	protected InputManager m_InputManager;
	protected PS_GameModeCoop m_GameModeCoop;
	protected PS_PlayableManager m_PlayableManager;
	protected PlayerManager m_PlayerManager;
	protected PlayerController m_PlayerController;
	protected SCR_FactionManager m_FactionManager;
	protected WorkspaceWidget m_wWorkspaceWidget;
	protected PS_PlayableControllerComponent m_PlayableControllerComponent;
	protected int m_iPlayerId;

	// Widgets
	protected Widget m_wRoot;
	protected FrameWidget m_wGameModeHeader;
	protected OverlayWidget m_wChatPanel;
	protected VerticalLayoutWidget m_wFactionList;
	protected VerticalLayoutWidget m_wRolesList;
	protected VerticalLayoutWidget m_wPlayersSearch;
	protected ScrollLayoutWidget m_wPlayersScroll;
	protected OverlayWidget m_wPlayersSearchBox;
	protected VerticalLayoutWidget m_wPlayersList;
	protected ButtonWidget m_wPreviewHideButton;
	protected TextWidget m_wPlayersCounter;
	protected FrameWidget m_wVoiceChatFrame;
	protected OverlayWidget m_wLobbyLittleInventoryItemInfo;
	protected ButtonWidget m_wPlayersSwitch;
	protected ButtonWidget m_wVoiceSwitch;
	protected VerticalLayoutWidget m_wMainLoadoutPreview;
	protected OverlayWidget m_wLoadoutPreviewBody;
	protected OverlayWidget m_wPlayersBody;
	protected ItemPreviewWidget m_wPreview;
	protected ButtonWidget m_wNavigationStart;
	protected ButtonWidget m_wNavigationChat;
	protected ButtonWidget m_wNavigationClose;
	protected ScrollLayoutWidget m_wRolesScroll;
	protected OverlayWidget m_wOverlayCounter;
	protected TextWidget m_wTextCounter;
	protected ButtonWidget m_wScreenButton;
	protected ButtonWidget m_wRolesFoldButton;
	protected ImageWidget m_wRolesFoldButtonImage;
	
	// Handlers
	protected SCR_ButtonBaseComponent m_PlayersSwitchButtonComponent;
	protected SCR_ButtonBaseComponent m_VoiceSwitchButtonComponent;
	protected SCR_ButtonBaseComponent m_PreviewHideButtonComponent;
	protected PS_GameModeHeader m_GameModeHeader;
	protected SCR_ChatPanel m_ChatPanel;
	protected PS_LobbyLoadoutPreview m_LobbyLoadoutPreview;
	protected PS_PlayersList m_PlayersList;
	protected SCR_InputButtonComponent m_NavigationStart;
	protected SCR_InputButtonComponent m_NavigationChat;
	protected SCR_InputButtonComponent m_NavigationClose;
	protected PS_VoiceChatList m_VoiceChatList;
	protected SCR_EditBoxComponent m_PlayersSearchBox;
	protected SCR_ButtonBaseComponent m_RolesFoldButtonComponent;
	
	// Vars
	protected ref map<SCR_Faction, PS_FactionSelector> m_mFactions = new map<SCR_Faction, PS_FactionSelector>();
	protected ref map<SCR_AIGroup, PS_RolesGroup> m_mGroups = new map<SCR_AIGroup, PS_RolesGroup>();
	protected ref map<int, PS_RolesGroup> m_mGroupsById = new map<int, PS_RolesGroup>();
	protected ref map<int, FactionKey> m_mGroupFactionKeys = new map<int, FactionKey>();
	protected SCR_Faction m_CurrentFaction = null;
	protected int m_iSelectedPlayer;
	
	override void OnMenuOpen()
	{
		if (!PS_WaitScreen.m_bWaitEnded) {
			Close();
			return;
		}
			
		if (RplSession.Mode() == RplMode.Dedicated) {
			Close();
			return;
		}
		
		// Cache global
		m_InputManager = GetGame().GetInputManager();
		m_GameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		m_PlayableManager = PS_PlayableManager.GetInstance();
		m_PlayerManager = GetGame().GetPlayerManager();
		m_PlayerController = GetGame().GetPlayerController();
		m_FactionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		m_wWorkspaceWidget = GetGame().GetWorkspace();
		m_iPlayerId = m_PlayerController.GetPlayerId();
		m_PlayableControllerComponent = PS_PlayableControllerComponent.Cast(m_PlayerController.FindComponent(PS_PlayableControllerComponent));
		m_iSelectedPlayer = m_iPlayerId;
		
		// Widgets
		m_wRoot = GetRootWidget();
		m_wGameModeHeader = FrameWidget.Cast(m_wRoot.FindAnyWidget("GameModeHeader"));
		m_wChatPanel = OverlayWidget.Cast(m_wRoot.FindAnyWidget("ChatPanel"));
		m_wFactionList = VerticalLayoutWidget.Cast(m_wRoot.FindAnyWidget("FactionList"));
		m_wRolesList = VerticalLayoutWidget.Cast(m_wRoot.FindAnyWidget("RolesList"));
		m_wPlayersSearch = VerticalLayoutWidget.Cast(m_wRoot.FindAnyWidget("PlayersSearch"));
		m_wPlayersScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("PlayersScroll"));
		m_wPlayersSearchBox = OverlayWidget.Cast(m_wRoot.FindAnyWidget("PlayersSearchBox"));
		m_wPreviewHideButton = ButtonWidget.Cast(m_wRoot.FindAnyWidget("PreviewHideButton"));
		m_wPlayersCounter = TextWidget.Cast(m_wRoot.FindAnyWidget("PlayersCounter"));
		m_wVoiceChatFrame = FrameWidget.Cast(m_wRoot.FindAnyWidget("VoiceChatFrame"));
		m_wLobbyLittleInventoryItemInfo = OverlayWidget.Cast(m_wRoot.FindAnyWidget("LobbyLittleInventoryItemInfo"));
		m_wPlayersSwitch = ButtonWidget.Cast(m_wRoot.FindAnyWidget("PlayersSwitch"));
		m_wVoiceSwitch = ButtonWidget.Cast(m_wRoot.FindAnyWidget("VoiceSwitch"));
		m_wMainLoadoutPreview = VerticalLayoutWidget.Cast(m_wRoot.FindAnyWidget("MainLoadoutPreview"));
		m_wLoadoutPreviewBody = OverlayWidget.Cast(m_wRoot.FindAnyWidget("LoadoutPreviewBody"));
		m_wPlayersBody = OverlayWidget.Cast(m_wRoot.FindAnyWidget("PlayersBody"));
		m_wPreview = ItemPreviewWidget.Cast(m_wRoot.FindAnyWidget("Preview"));
		m_wNavigationStart = ButtonWidget.Cast(m_wRoot.FindAnyWidget("NavigationStart"));
		m_wRolesScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("RolesScroll"));
		m_wNavigationChat = ButtonWidget.Cast(m_wRoot.FindAnyWidget("NavigationChat"));
		m_wNavigationClose = ButtonWidget.Cast(m_wRoot.FindAnyWidget("NavigationClose"));
		m_wOverlayCounter = OverlayWidget.Cast(m_wRoot.FindAnyWidget("OverlayCounter"));
		m_wTextCounter = TextWidget.Cast(m_wRoot.FindAnyWidget("TextCounter"));
		m_wScreenButton = ButtonWidget.Cast(m_wRoot.FindAnyWidget("ScreenButton"));
		m_wRolesFoldButton = ButtonWidget.Cast(m_wRoot.FindAnyWidget("RolesFoldButton"));
		m_wRolesFoldButtonImage = ImageWidget.Cast(m_wRoot.FindAnyWidget("RolesFoldButtonImage"));
		
		// Handlers
		m_GameModeHeader = PS_GameModeHeader.Cast(m_wGameModeHeader.FindHandler(PS_GameModeHeader));
		m_ChatPanel = SCR_ChatPanel.Cast(m_wChatPanel.FindHandler(SCR_ChatPanel));
		m_PreviewHideButtonComponent = SCR_ButtonBaseComponent.Cast(m_wPreviewHideButton.FindHandler(SCR_ButtonBaseComponent));
		m_PlayersSwitchButtonComponent = SCR_ButtonBaseComponent.Cast(m_wPlayersSwitch.FindHandler(SCR_ButtonBaseComponent));
		m_VoiceSwitchButtonComponent = SCR_ButtonBaseComponent.Cast(m_wVoiceSwitch.FindHandler(SCR_ButtonBaseComponent));
		m_NavigationChat = SCR_InputButtonComponent.Cast(m_wNavigationChat.FindHandler(SCR_InputButtonComponent));
		m_NavigationClose = SCR_InputButtonComponent.Cast(m_wNavigationClose.FindHandler(SCR_InputButtonComponent));
		m_LobbyLoadoutPreview = PS_LobbyLoadoutPreview.Cast(m_wMainLoadoutPreview.FindHandler(PS_LobbyLoadoutPreview));
		m_PlayersList = PS_PlayersList.Cast(m_wPlayersBody.FindHandler(PS_PlayersList));
		m_NavigationStart = SCR_InputButtonComponent.Cast(m_wNavigationStart.FindHandler(SCR_InputButtonComponent));
		m_VoiceChatList = PS_VoiceChatList.Cast(m_wVoiceChatFrame.FindHandler(PS_VoiceChatList));
		m_PlayersSearchBox = SCR_EditBoxComponent.Cast(m_wPlayersSearchBox.FindHandler(SCR_EditBoxComponent));
		m_RolesFoldButtonComponent = SCR_ButtonBaseComponent.Cast(m_wRolesFoldButton.FindHandler(SCR_ButtonBaseComponent));
		
		FactionKey factionKey = m_PlayableManager.GetPlayerFactionKey(m_iPlayerId);
		m_CurrentFaction = SCR_Faction.Cast(m_FactionManager.GetFactionByKey(factionKey));
		m_VoiceChatList.SwitchFaction(factionKey);
		
		// Buttons
		m_PreviewHideButtonComponent.m_OnClicked.Insert(OnClickedPreviewHide);
		m_PlayersSwitchButtonComponent.m_OnClicked.Insert(OnClickedPlayersSwitch);
		m_VoiceSwitchButtonComponent.m_OnClicked.Insert(OnClickedVoiceSwitch);
		m_NavigationStart.m_OnActivated.Insert(Action_Ready);
		m_NavigationChat.m_OnActivated.Insert(Action_ChatOpen);
		m_NavigationClose.m_OnActivated.Insert(Action_Exit);
		m_RolesFoldButtonComponent.m_OnClicked.Insert(OnClickedRolesFold);
		
		// Events
		m_PlayableManager.GetOnFactionChange().Insert(UpdatePlayerFaction);
		m_PlayableManager.GetOnPlayerPlayableChange().Insert(OnPlayerPlayableChangeVoiceChat);
		m_PlayableManager.GetOnStartTimerCounterChanged().Insert(OnStartTimerCounterChanged);
		m_PlayableManager.GetOnPlayerConnected().Insert(OnPlayerConnected);
		m_PlayableManager.GetOnPlayerDisconnected().Insert(OnPlayerDisconnected);
		m_PlayersSearchBox.m_OnChanged.Insert(OnPlayersSearchChanged);
		m_PlayersSearchBox.m_OnWriteModeEnter.Insert(OnPlayersSearchWriteModeEnter);
		m_PlayersSearchBox.m_OnWriteModeLeave.Insert(OnPlayersSearchWriteModeLeave);
		
		// Actions
		if (m_GameModeCoop.GetState() == SCR_EGameModeState.SLOTSELECTION)
		{
			m_InputManager.AddActionListener("VONDirect", EActionTrigger.DOWN, Action_LobbyVoNOn);
			m_InputManager.AddActionListener("VONDirect", EActionTrigger.UP, Action_LobbyVoNOff);
			//m_InputManager.AddActionListener("VONChannel", EActionTrigger.DOWN, Action_LobbyVoNChannelOn);
			m_InputManager.AddActionListener("VONChannel", EActionTrigger.UP, Action_LobbyVoNChannelOff);
		}
		
		m_LobbyLoadoutPreview.SetItemInfoWidget(m_wLobbyLittleInventoryItemInfo);
		
		// Init
		Init();
		
		if (!m_CurrentFaction && m_mFactions.Count() > 0)
		{
			foreach (SCR_Faction f, PS_FactionSelector _ : m_mFactions)
			{
				m_CurrentFaction = f;
				break;
			}
			if (m_CurrentFaction)
			{
				SwitchCurrentFaction(m_CurrentFaction);
				m_VoiceChatList.SwitchFaction(m_CurrentFaction.GetFactionKey());
			}
		}
	}
	
	override void OnMenuUpdate(float tDelta)
	{
		m_ChatPanel.OnUpdateChat(tDelta);
		
		// Force refreash
		//if (m_wLoadoutPreviewBody.IsVisible())
		//	m_wPreview.SetRefresh(1, 1);
		
		m_GameModeHeader.TryUpdate();
	};
	
	override void OnMenuClose()
	{
		m_PlayableControllerComponent.LobbyVoNDisableImmediate();
		if (m_InputManager)
		{
			m_InputManager.RemoveActionListener("VONDirect", EActionTrigger.DOWN, Action_LobbyVoNOn);
			m_InputManager.RemoveActionListener("VONDirect", EActionTrigger.UP, Action_LobbyVoNOff);
			//m_InputManager.RemoveActionListener("VONChannel", EActionTrigger.DOWN, Action_LobbyVoNChannelOn);
			m_InputManager.RemoveActionListener("VONChannel", EActionTrigger.UP, Action_LobbyVoNChannelOff);
		}
		if (m_PlayableManager)
		{
			m_PlayableManager.GetOnFactionChange().Remove(UpdatePlayerFaction);
			m_PlayableManager.GetOnPlayerPlayableChange().Remove(OnPlayerPlayableChangeVoiceChat);
			m_PlayableManager.GetOnStartTimerCounterChanged().Remove(OnStartTimerCounterChanged);
			m_PlayableManager.GetOnPlayerConnected().Remove(OnPlayerConnected);
			m_PlayableManager.GetOnPlayerDisconnected().Remove(OnPlayerDisconnected);
		}
	}

	// --------------------------------------------------------------------------------------------------------------------------------
	// Init
	void Init()
	{
		InitPlayables();
		InitPlayers();
		
		m_wPlayersCounter.SetTextFormat("%1/%2", m_PlayerManager.GetPlayerCount(), m_PlayableManager.GetMaxPlayers());
	}
	
	void InitPlayables()
	{
		PS_DebugLogger.LogImportant("InitPlayables slotMapSize=" + m_PlayableManager.GetSlots().Count().ToString() + " sortedCacheSize=" + m_PlayableManager.GetSortedSlotIds().Count().ToString());
		map<RplId, ref PS_VehicleData> playableVehicles = m_PlayableManager.GetVehicles().GetRawMap();
		map<SCR_Faction, ref Tuple2<int, int>> factionSlots = new map<SCR_Faction, ref Tuple2<int, int>>();
		map<SCR_Faction, int> factionLocked = new map<SCR_Faction, int>();

		foreach (RplId slotId : m_PlayableManager.GetSortedSlotIds())
		{
			PS_SlotCharacterData slot;
			if (!m_PlayableManager.FindSlotData(slotId, slot))
				continue;
			AddPlayable(slot);

			int playerAdded = 0;
			int playerAddedLocked = 0;
			if (slot.m_PlayerId >= 0)
				playerAdded = 1;
			if (slot.m_IsLocked)
				playerAddedLocked = 1;

			SCR_Faction faction = slot.GetFaction();
			if (!factionSlots.Contains(faction))
			{
				factionSlots.Insert(faction, new Tuple2<int, int>(playerAdded, 1));
				factionLocked.Insert(faction, playerAddedLocked);
			}
			else
			{
				Tuple2<int, int> tuple = factionSlots.Get(faction);
				tuple.param1 += playerAdded;
				tuple.param2 += 1;
				factionLocked.Set(faction, factionLocked.Get(faction) + playerAddedLocked);
			}
		}

		foreach (RplId rplId, PS_VehicleData vehicleData : playableVehicles)
		{
			PS_PlayableVehicleContainer container = new PS_PlayableVehicleContainer();
			container.Init(vehicleData.m_RplId, vehicleData.m_PrefabPath, vehicleData.m_IconPath, 0, vehicleData.m_GroupId, vehicleData.m_FactionKey);
			AddPlayableVehicle(container);
		}
		
		foreach (SCR_Faction faction, Tuple2<int, int> slotCount : factionSlots)
		{
			int locked = 0;
			factionLocked.Find(faction, locked);
			AddFaction(faction, slotCount.param1, slotCount.param2, locked);
		}
		
		// Added in runtime
		m_PlayableManager.GetCallbackHandler().GetOnSlotInserted().Remove(OnSlotInserted);
		m_PlayableManager.GetCallbackHandler().GetOnSlotInserted().Insert(OnSlotInserted);
	}
	
	void InitPlayers()
	{
		if (m_PlayersList)
			m_PlayersList.InitPlayers();
	}

	void AddPlayableVehicle(PS_PlayableVehicleContainer vehicleContainer)
	{
		int groupId = vehicleContainer.m_iGroupId;
		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		SCR_AIGroup playableGroup = groupsManager.FindGroup(groupId);
		PS_RolesGroup rolesGroup;
		if (!m_mGroups.Contains(playableGroup))
		{
			return;
		}
		else rolesGroup = m_mGroups.Get(playableGroup);
		rolesGroup.InsertVehicle(vehicleContainer);
	}

	void AddPlayable(PS_SlotCharacterData slot)
	{
		PS_PlayableContainer playable = m_PlayableManager.GetPlayableById(slot.m_RplId);
		if (!playable)
		{
			playable = new PS_PlayableContainer();
			playable.InitFromSlotData(slot);
		}
		SCR_AIGroup playableGroup = m_PlayableManager.GetPlayerGroupByPlayable(slot.m_RplId);
		PS_RolesGroup rolesGroup;
		if (playableGroup && m_mGroups.Contains(playableGroup))
			rolesGroup = m_mGroups.Get(playableGroup);
		else if (m_mGroupsById.Contains(slot.m_PlayerGroupId))
			rolesGroup = m_mGroupsById.Get(slot.m_PlayerGroupId);

		if (!rolesGroup)
		{
			Widget rolesGroupRoot = m_wWorkspaceWidget.CreateWidgets(m_sRolesGroupPrefab, m_wRolesList);
			rolesGroup = PS_RolesGroup.Cast(rolesGroupRoot.FindHandler(PS_RolesGroup));
			rolesGroup.SetLobbyMenu(this);
			rolesGroup.SetAIGroup(playableGroup);
			if (playableGroup)
				m_mGroups.Insert(playableGroup, rolesGroup);
			m_mGroupsById.Insert(slot.m_PlayerGroupId, rolesGroup);
			m_mGroupFactionKeys.Insert(slot.m_PlayerGroupId, slot.m_FactionKey);

			SCR_Faction faction = slot.GetFaction();
			rolesGroupRoot.SetVisible(m_CurrentFaction == faction);
		}
		rolesGroup.InsertPlayable(playable);
	}

	void AddFaction(SCR_Faction faction, int count, int maxCount, int lockedCount)
	{
		Widget factionSelectorRoot = m_wWorkspaceWidget.CreateWidgets(m_sFactionSelectorPrefab, m_wFactionList);
		PS_FactionSelector factionSelector = PS_FactionSelector.Cast(factionSelectorRoot.FindHandler(PS_FactionSelector));
		factionSelector.SetFaction(faction);
		factionSelector.SetCount(count);
		factionSelector.SetMaxCount(maxCount);
		factionSelector.SetLockedCount(lockedCount);
		factionSelector.SetCoopLobby(this);
		factionSelector.SetToggled(m_CurrentFaction == faction);
		m_mFactions.Insert(faction, factionSelector);
	}

	void AddPlayer(int playerId)
	{
		
	}
	
	void AddFactionCount(SCR_Faction faction, int added, int addedMax, int addedLocked = 0)
	{
		if (!m_mFactions.Contains(faction))
			AddFaction(faction, 0, 0, 0);
		PS_FactionSelector factionSelector = m_mFactions.Get(faction);
		int count = factionSelector.GetCount();
		int maxCount = factionSelector.GetMaxCount();
		int lockedCount = factionSelector.GetLockedCount();
		factionSelector.SetCount(count + added);
		factionSelector.SetMaxCount(maxCount + addedMax);
		factionSelector.SetLockedCount(lockedCount + addedLocked);
		if ((maxCount + addedMax) < 1)
		{
			factionSelector.GetRootWidget().RemoveFromHierarchy();
			m_mFactions.Remove(faction);
		}
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	// Buttons
	void OnClickedPreviewHide(SCR_ButtonBaseComponent factionButton)
	{
		m_wLoadoutPreviewBody.SetVisible(!m_wLoadoutPreviewBody.IsVisible());
	}
	
	void OnClickedPlayersSwitch(SCR_ButtonBaseComponent factionButton)
	{
		m_wVoiceChatFrame.SetVisible(false);
		m_wPlayersSearch.SetVisible(true);
		m_VoiceSwitchButtonComponent.SetToggled(false);
	}
	
	void OnClickedVoiceSwitch(SCR_ButtonBaseComponent factionButton)
	{
		m_wVoiceChatFrame.SetVisible(true);
		m_wPlayersSearch.SetVisible(false);
		m_PlayersSwitchButtonComponent.SetToggled(false);
	}
	
	void OnClickedRolesFold(SCR_ButtonBaseComponent rolesFold)
	{
		bool fold = false;
		foreach (SCR_AIGroup aIGroup, PS_RolesGroup rolesGroup : m_mGroups)
		{
			if (!rolesGroup.GetRootWidget().IsVisible())
				continue;
			if (!rolesGroup.IsFolded())
			{
				fold = true;
				break;
			}
		}
		if (!fold)
		{
			foreach (int groupId, PS_RolesGroup rolesGroup : m_mGroupsById)
			{
				if (!rolesGroup.GetRootWidget().IsVisible())
					continue;
				if (!rolesGroup.IsFolded())
				{
					fold = true;
					break;
				}
			}
		}
		foreach (SCR_AIGroup aIGroup, PS_RolesGroup rolesGroup : m_mGroups)
		{
			if (!rolesGroup.GetRootWidget().IsVisible())
				continue;
			rolesGroup.SetFolded(fold);
		}
		foreach (int groupId, PS_RolesGroup rolesGroup : m_mGroupsById)
		{
			if (!rolesGroup.GetRootWidget().IsVisible())
				continue;
			rolesGroup.SetFolded(fold);
		}
	}
	void OnRolesFold(PS_RolesGroup _rolesGroup)
	{
		bool fold = false;
		foreach (SCR_AIGroup aIGroup, PS_RolesGroup rolesGroup : m_mGroups)
		{
			if (!rolesGroup.GetRootWidget().IsVisible())
				continue;
			if (!rolesGroup.IsFolded())
			{
				fold = true;
				break;
			}
		}
		if (!fold)
		{
			foreach (int groupId, PS_RolesGroup rolesGroup : m_mGroupsById)
			{
				if (!rolesGroup.GetRootWidget().IsVisible())
					continue;
				if (!rolesGroup.IsFolded())
				{
					fold = true;
					break;
				}
			}
		}
		if (fold)
			m_wRolesFoldButtonImage.LoadImageFromSet(0, IMAGESET_PS, "Fold");
		else
			m_wRolesFoldButtonImage.LoadImageFromSet(0, IMAGESET_PS, "Unfold");
	}
	
	void OnSlotInserted(RplId slotId, int joinableGroupId, FactionKey factionKey)
	{
		PS_SlotCharacterData slot;
		if (!m_PlayableManager.FindSlotData(slotId, slot))
			return;
		SCR_Faction faction = slot.GetFaction();
		if (!m_mFactions.Contains(faction))
		{
			AddFaction(faction, 0, 1, 0);
		}
		else
			AddFactionCount(faction, 0, 1, 0);
		AddPlayable(slot);
	}
	
	void OnRolesGroupRemoved(PS_RolesGroup rolesGroup)
	{
		m_mGroups.Remove(SCR_MapHelper<SCR_AIGroup, PS_RolesGroup>.GetKeyByValue(m_mGroups, rolesGroup));
		int key = SCR_MapHelper<int, PS_RolesGroup>.GetKeyByValue(m_mGroupsById, rolesGroup);
		m_mGroupsById.Remove(key);
	}
	
	void OnPlayableRemoved(RplId slotId, FactionKey factionKey, int playerId)
	{
		PS_SlotCharacterData slot;
		if (!m_PlayableManager.FindSlotData(slotId, slot))
			return;
		SCR_Faction faction = slot.GetFaction();

		int playerAdded = 0;
		int lockedAdded = 0;
		if (slot.m_PlayerId >= 0)
			playerAdded = -1;
		if (slot.m_IsLocked)
			lockedAdded = -1;

		AddFactionCount(faction, playerAdded, -1, lockedAdded);
	}
	
	void OnPlayerRemoved(int playerId)
	{
		if (playerId != m_iSelectedPlayer)
			return;
		
		m_iSelectedPlayer = m_iPlayerId;
		m_PlayersList.SetSelectedPlayer(m_iPlayerId);
	}
	
	void OnPlayersSearchChanged(SCR_EditBoxComponent editBoxComponent, string text)
	{
		m_PlayersList.SetSearchText(text);
	}
	
	void OnPlayersSearchWriteModeEnter()
	{
		m_wScreenButton.SetVisible(true);
	}
	
	void OnPlayersSearchWriteModeLeave(string searchText)
	{
		m_wScreenButton.SetVisible(false);
	}
	
	
	void SwitchCurrentFaction(SCR_Faction faction)
	{
		if (m_CurrentFaction != faction)
		{
			if (m_CurrentFaction && m_mFactions.Get(m_CurrentFaction))
				m_mFactions.Get(m_CurrentFaction).SetToggled(false);
			m_CurrentFaction = faction;
		}
		else
			m_CurrentFaction = null;
		
		foreach (SCR_AIGroup aiGroup, PS_RolesGroup rolesGroup : m_mGroups)
		{
			if (!aiGroup)
				continue;
			SCR_Faction groupFaction = SCR_Faction.Cast(aiGroup.GetFaction());
			bool factionSelected = m_CurrentFaction == groupFaction;
			rolesGroup.GetRootWidget().SetVisible(factionSelected);
		}

		foreach (int groupId, PS_RolesGroup rolesGroup : m_mGroupsById)
		{
			FactionKey factionKey = m_mGroupFactionKeys.Get(groupId);
			SCR_Faction groupFaction = SCR_Faction.Cast(m_FactionManager.GetFactionByKey(factionKey));
			bool factionSelected = m_CurrentFaction == groupFaction;
			rolesGroup.GetRootWidget().SetVisible(factionSelected);
		}
		
		m_wRolesScroll.SetSliderPos(0, 0);
		OnRolesFold(null);
	}
	
	void SetPreviewPlayableVehicle(PS_PlayableVehicleContainer vehicleData, bool openInventory)
	{
		m_LobbyLoadoutPreview.SetPreviewPlayableVehicle(vehicleData, openInventory);
	}
	void SetPreviewPlayable(RplId playableId, bool openInventory)
	{
		if (!playableId.IsValid())
		{
			playableId = m_PlayableManager.GetPlayableByPlayer(m_iPlayerId);
			if (playableId == RplId.Invalid())
				return;
		}
		ResourceName prefabName = m_PlayableManager.GetSlotCharacterPrefabPath(playableId);
		m_LobbyLoadoutPreview.SetPreviewPlayable(playableId, prefabName, openInventory);
	}
	
	void UpdatePlayerFaction(int playerId, FactionKey factionKey, FactionKey factionKeyOld)
	{
		if (playerId != m_iPlayerId)
			return;
		
		SwitchVoiceChatFaction(factionKey);
	}
	
	void OnPlayerPlayableChangeVoiceChat(int playerId, RplId slotId)
	{
		m_VoiceChatList.Rebuild();
	}
	
	void SwitchVoiceChatFaction(FactionKey factionKey)
	{
		m_VoiceChatList.SwitchFaction(factionKey);
	}
	
	
	void OnPlayerConnected(int playerId)
	{
		m_wPlayersCounter.SetTextFormat("%1/%2", m_PlayerManager.GetPlayerCount(), m_PlayableManager.GetMaxPlayers());
	}
	
	void OnPlayerDisconnected(int playerId)
	{
		m_wPlayersCounter.SetTextFormat("%1/%2", m_PlayerManager.GetPlayerCount(), m_PlayableManager.GetMaxPlayers());
	}
	
	void OnStartTimerCounterChanged(int timer)
	{
		if (timer == -1)
		{
			m_wOverlayCounter.SetVisible(false);
		} else {
			m_wOverlayCounter.SetVisible(true);
			m_wTextCounter.SetText(timer.ToString());
			SCR_UISoundEntity.SoundEvent("SOUND_RADIO_FREQUENCY_CYCLE");
		}
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	// Set
	void SetSelectedPlayer(int playerId)
	{
		m_iSelectedPlayer = playerId;
		m_PlayersList.SetSelectedPlayer(playerId);
		m_VoiceChatList.SetSelectedPlayer(playerId);
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	// Get
	int GetSelectedPlayer()
	{
		return m_iSelectedPlayer;
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	// Actions
	void Action_Ready()
	{
		SCR_EGameModeState gameModeState = m_GameModeCoop.GetState();
		if (gameModeState == SCR_EGameModeState.GAME)
		{
			m_PlayableControllerComponent.SwitchToMenu(SCR_EGameModeState.GAME);
			return;
		}
		
		PS_EPlayableControllerState currentState = m_PlayableManager.GetPlayerState(m_iPlayerId);
		RplId playableId = m_PlayableManager.GetPlayableByPlayer(m_iPlayerId);
		if (currentState != PS_EPlayableControllerState.Ready && playableId != RplId.Invalid()) m_PlayableControllerComponent.SetPlayerState(m_iPlayerId, PS_EPlayableControllerState.Ready);
		if (currentState == PS_EPlayableControllerState.Ready) m_PlayableControllerComponent.SetPlayerState(m_iPlayerId, PS_EPlayableControllerState.NotReady);
	}
	
	// Direct
	void Action_LobbyVoNOn()
	{
		m_PlayableControllerComponent.LobbyVoNEnable();
	}
	void Action_LobbyVoNOff()
	{
		m_PlayableControllerComponent.LobbyVoNDisable();
	}
	// Channel
	void Action_LobbyVoNChannelOn()
	{
		m_PlayableControllerComponent.LobbyVoNRadioEnable();
	}
	void Action_LobbyVoNChannelOff()
	{
		m_PlayableControllerComponent.LobbyVoNDisable();
	}
	
	void Action_ChatOpen()
	{
		// Delay is esential, if any character already controlled
		GetGame().GetCallqueue().CallLater(ChatWrap, 0);
	}
	void ChatWrap()
	{
		SCR_ChatPanelManager.GetInstance().OpenChatPanel(m_ChatPanel);
	}
	
	void Action_Exit()
	{
		// For some strange reason players all the time accidentally exit game, maybe jus open pause menu
		//GameStateTransitions.RequestGameplayEndTransition();  
		//Close();
		
		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		SCR_EGameModeState gameModeState = gameMode.GetState();
		if (gameModeState == SCR_EGameModeState.GAME)
		{
			Close();
			return;
		}
		
		GetGame().GetCallqueue().CallLater(OpenPauseMenuWrap, 0); //  Else menu auto close itself
	}
	void OpenPauseMenuWrap()
	{
		ArmaReforgerScripted.OpenPauseMenu();
	}
}





























