class PS_AlivePlayerSelector : SCR_ButtonBaseComponent
{
	// Const
	protected const ResourceName m_sImageSet = "{D17288006833490F}UI/Textures/Icons/icons_wrapperUI-32.imageset";
	protected ref Color m_DeathColor = Color.FromInt(0xFF2c2c2c);
	
	// Cache global
	protected PS_PlayableManager m_PlayableManager;
	protected PlayerController m_PlayerController;
	protected SCR_FactionManager m_FactionManager;
	
	// Parameters
	protected PS_AlivePlayerGroup m_AliveGroup;
	protected PS_SpectatorMenu m_mSpectatorMenu;
	protected RplId m_iPlayableId;
	
	// Cache parameters
	protected PS_AlivePlayerList m_AlivePlayerList;
	protected PS_PlayableContainer m_PlayableContainer;
	protected SCR_CharacterDamageManagerComponent m_CharacterDamageManagerComponent;
	protected bool m_bDead;

	// Player name retry state (handles JIP/timing where name hasn't replicated yet)
	protected int m_iNameRetryCount = 0;
	protected static const int MAX_NAME_RETRIES = 10;
	protected static const float NAME_RETRY_INTERVAL_MS = 500;

	// Cached base player name (without alive/dead suffix), so UpdateDammage can
	// re-apply the status label without losing the nickname.
	protected string m_sBasePlayerName;
	
	// Widgets
	protected ImageWidget m_wPlayerFactionColor;
	protected ImageWidget m_wUnitIcon;
	protected ImageWidget m_wDeadIcon;
	protected RichTextWidget m_wPlayerName;
	
	// Init
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		
		// Widgets
		m_wUnitIcon = ImageWidget.Cast(w.FindAnyWidget("UnitIcon"));
		m_wDeadIcon = ImageWidget.Cast(w.FindAnyWidget("DeadIcon"));
		m_wPlayerName = RichTextWidget.Cast(w.FindAnyWidget("PlayerName"));
		m_wPlayerFactionColor = ImageWidget.Cast(w.FindAnyWidget("PlayerFactionColor"));
		
		// Cache global
		m_PlayableManager = PS_PlayableManager.GetInstance();
		m_PlayerController = GetGame().GetPlayerController();
		m_FactionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		
		GetGame().GetCallqueue().CallLater(AddOnClick, 0);
	}
	
	void AddOnClick()
	{
		m_OnClicked.Insert(AlivePlayerButtonClicked);
	}
	
	// Set parameters
	void SetPlayableContainer(PS_PlayableContainer container)
	{
		m_PlayableContainer = container;
		m_iPlayableId = container.GetRplId();
		if (m_iPlayableId == RplId.Invalid())
		{
			PS_DebugLogger.LogError("Spectator AlivePlayerSelector SetPlayableContainer FAIL: invalid RplId");
			return;
		}

		Faction faction = m_PlayableContainer.GetFaction();
		if (!faction)
		{
			// Fallback: try direct FactionManager lookup
			SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
			if (fm)
				faction = SCR_Faction.Cast(fm.GetFactionByKey(m_PlayableContainer.GetFactionKey()));
			if (!faction)
			{
				PS_DebugLogger.LogError("Spectator AlivePlayerSelector SetPlayableContainer FAIL: null faction for rplId=" + m_iPlayableId.ToString() + " factionKey=" + m_PlayableContainer.GetFactionKey());
				return;
			}
		}
		
		// Initial setup
		m_wPlayerFactionColor.SetColor(faction.GetFactionColor());
		EDamageState damageState = m_PlayableContainer.GetDamageState();
		int initialPlayerId;
		m_PlayableManager.FindPlayerIdBySlot(m_iPlayableId, initialPlayerId);
		PS_DebugLogger.LogImportant("Spectator AlivePlayerSelector SetPlayableContainer playableId=" + m_iPlayableId.ToString() + " factionKey=" + m_PlayableContainer.GetFactionKey() + " initialPlayer=" + initialPlayerId.ToString() + " damage=" + damageState.ToString());
		UpdateDammage(damageState);
		UpdatePlayer(initialPlayerId);
		UpdateShowDead(m_AlivePlayerList.IsShowDead());
		
		// Events
		m_PlayableContainer.GetOnPlayerChange().Insert(UpdatePlayerWrap);
		m_PlayableContainer.GetOnDamageStateChanged().Insert(UpdateDammage);
		m_PlayableContainer.GetOnUnregister().Insert(RemoveSelf);
	}
	
	void SetSpectatorMenu(PS_SpectatorMenu spectatorMenu)
	{
		m_mSpectatorMenu = spectatorMenu;
	}
	
	void SetAlivePlayerList(PS_AlivePlayerList alivePlayerList)
	{
		m_AlivePlayerList = alivePlayerList;
		m_AlivePlayerList.GetOnShowDead().Insert(UpdateShowDead);
	}
	
	void SetAliveGroup(PS_AlivePlayerGroup alivePlayerGroup)
	{
		m_AliveGroup = alivePlayerGroup;
	}
	
	// Updates
	void UpdateDammage(EDamageState state)
	{
		PS_DebugLogger.LogImportant("Spectator UpdateDammage playableId=" + m_iPlayableId.ToString() + " state=" + state.ToString() + " wasDead=" + m_bDead.ToString());
		bool wasDead = m_bDead;
		m_bDead = state == EDamageState.DESTROYED;
		if (m_bDead)
		{
			if (m_wRoot)
				m_wRoot.SetVisible(m_AlivePlayerList && m_AlivePlayerList.IsShowDead());
			// Guard: only notify the faction count on the FIRST transition to dead.
			// Without this, double-fires from the component's own damage handler +
			// the new PS_SlotDamageStateChangedCallback would double-decrement the
			// faction alive count in the header.
			if (m_AlivePlayerList && m_PlayableContainer && !wasDead)
				m_AlivePlayerList.OnAliveDie(m_PlayableContainer);
			if (m_wUnitIcon)
				m_wUnitIcon.SetVisible(false);
			if (m_wDeadIcon)
			{
				m_wDeadIcon.SetVisible(true);
				m_wDeadIcon.SetColor(m_DeathColor);
			}
			if (m_wPlayerName)
				m_wPlayerName.SetColor(m_DeathColor);
		}
		else
		{
			bool showDead = false;
			if (m_AlivePlayerList)
				showDead = m_AlivePlayerList.IsShowDead();
			UpdateShowDead(showDead);
			if (m_PlayableContainer)
				m_PlayableContainer.SetIconTo(m_wUnitIcon);
			if (m_wPlayerName)
				m_wPlayerName.SetColor(Color.White);
		}

		// Refresh display name with alive/dead status suffix
		RefreshDisplayName();

		if (m_PlayableContainer)
		{
			int playerId;
			m_PlayableManager.FindPlayerIdBySlot(m_PlayableContainer.GetRplId(), playerId);
			if (m_PlayerController && playerId == m_PlayerController.GetPlayerId())
			{
				if (m_wPlayerName)
					m_wPlayerName.SetColor(Color.FromInt(0xFF666666));
			}
		}
	}
	
	void UpdatePlayerWrap(int oldPlayerId, int playerId)
	{
		PS_DebugLogger.Log("Spectator UpdatePlayerWrap playableId=" + m_iPlayableId.ToString() + " oldPId=" + oldPlayerId.ToString() + " newPId=" + playerId.ToString());
		UpdatePlayer(playerId);
	}
	void UpdatePlayer(int playerId)
	{
		if (!m_wPlayerName)
			return;
		// BUGFIX: query native PlayerManager first — it holds the authoritative name.
		// The PlayableManager cache (m_PlayerNamesCached) may lag on JIP clients.
		string playerName = "";
		if (playerId > 0 && GetGame().GetPlayerManager())
			playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		if (playerName == "")
			playerName = m_PlayableManager.GetPlayerName(playerId);

		if (playerName == "") // No player name yet (JIP/timing)
		{
			// Only retry for real players (playerId > 0). AI slots (playerId <= 0)
			// will never get a name — show the container name as a fallback once
			// and don't retry.
			if (m_PlayableContainer)
				playerName = m_PlayableContainer.GetName();
			else
				playerName = "";

			if (playerId > 0)
			{
				m_iNameRetryCount = 1;
				GetGame().GetCallqueue().CallLater(RetryUpdatePlayerName, NAME_RETRY_INTERVAL_MS, false, playerId);
			}
		}
		else
		{
			// Real name found — cancel any pending retries and reset counter
			m_iNameRetryCount = 0;
			GetGame().GetCallqueue().Remove(RetryUpdatePlayerName);
		}

		// Store base name and refresh display with alive/dead suffix
		m_sBasePlayerName = playerName;
		RefreshDisplayName();

		PS_DebugLogger.Log("Spectator UpdatePlayer playableId=" + m_iPlayableId.ToString() + " pId=" + playerId.ToString() + " name=" + playerName);
		if (m_AlivePlayerList)
			m_AlivePlayerList.RegisterPlayerSelector(playerId, m_iPlayableId, this);
	}

	// Retry player name resolution when the initial UpdatePlayer call found an empty name.
	// This handles the JIP case where the player name hasn't been replicated yet, or the
	// OnPlayerNameUpdated callback fired before the selector was registered.
	// Counter-based: re-schedules itself up to MAX_NAME_RETRIES times at NAME_RETRY_INTERVAL_MS.
	void RetryUpdatePlayerName(int playerId)
	{
		if (!m_wPlayerName || !m_PlayableContainer)
			return;
		if (m_iNameRetryCount >= MAX_NAME_RETRIES)
		{
			PS_DebugLogger.Log("Spectator RetryUpdatePlayerName GAVE UP after " + MAX_NAME_RETRIES.ToString() + " attempts playableId=" + m_iPlayableId.ToString() + " pId=" + playerId.ToString());
			return;
		}
		m_iNameRetryCount++;
		// BUGFIX: try native PlayerManager first, fall back to PlayableManager cache
		string playerName = "";
		if (GetGame().GetPlayerManager())
			playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		if (playerName == "")
			playerName = m_PlayableManager.GetPlayerName(playerId);
		if (playerName != "")
		{
			m_sBasePlayerName = playerName;
			RefreshDisplayName();
			m_iNameRetryCount = 0;
			PS_DebugLogger.Log("Spectator RetryUpdatePlayerName SUCCESS attempt=" + m_iNameRetryCount.ToString() + " playableId=" + m_iPlayableId.ToString() + " pId=" + playerId.ToString() + " name=" + playerName);
			return;
		}
		// Still no name — schedule next retry
		GetGame().GetCallqueue().CallLater(RetryUpdatePlayerName, NAME_RETRY_INTERVAL_MS, false, playerId);
	}

	void ClearPlayerName()
	{
		PS_DebugLogger.Log("Spectator ClearPlayerName playableId=" + m_iPlayableId.ToString());
		m_sBasePlayerName = "";
		if (!m_wPlayerName)
			return;
		if (m_PlayableContainer)
			m_sBasePlayerName = m_PlayableContainer.GetName();
		RefreshDisplayName();
	}

	// Compose the display text from base name + alive/dead status suffix.
	// Called by both UpdatePlayer and UpdateDammage so the label stays consistent.
	// Only appends " (DEAD)" for dead players — alive is the default state.
	void RefreshDisplayName()
	{
		if (!m_wPlayerName)
			return;
		string displayName = m_sBasePlayerName;
		if (displayName != "" && m_bDead)
			displayName = displayName + " (DEAD)";
		m_wPlayerName.SetText(displayName);
	}
	
	RplId GetPlayableId()
	{
		return m_iPlayableId;
	}

	// Public accessor for the playable container, needed by PS_AlivePlayerList to
	// update faction counts on disconnect/reconnect (m_PlayableContainer is
	// protected and cannot be accessed directly from a different class).
	PS_PlayableContainer GetPlayableContainer()
	{
		return m_PlayableContainer;
	}
	
	void RemoveSelf()
	{
		// Cancel any pending name-retry CallLater so it doesn't fire on a disposed object
		GetGame().GetCallqueue().Remove(RetryUpdatePlayerName);
		m_iNameRetryCount = 0;
		m_sBasePlayerName = "";

		if (!m_PlayableContainer)
		{
			PS_DebugLogger.LogImportant("Spectator RemoveSelf playableId=" + m_iPlayableId.ToString() + " NO CONTAINER");
			if (m_wRoot)
				m_wRoot.RemoveFromHierarchy();
			return;
		}
		int playerId;
		m_PlayableManager.FindPlayerIdBySlot(m_PlayableContainer.GetRplId(), playerId);
		PS_DebugLogger.LogImportant("Spectator RemoveSelf playableId=" + m_iPlayableId.ToString() + " playerId=" + playerId.ToString());
		if (m_AlivePlayerList)
		{
			m_AlivePlayerList.UnregisterPlayerSelector(playerId, m_iPlayableId);
			m_AlivePlayerList.OnAliveRemoved(m_PlayableContainer);
		}
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
		if (m_AliveGroup)
			m_AliveGroup.OnAliveRemoved(m_PlayableContainer);
	}
	
	void UpdateShowDead(bool showDead)
	{
		PS_DebugLogger.Log("Spectator UpdateShowDead playableId=" + m_iPlayableId.ToString() + " showDead=" + showDead.ToString() + " isDead=" + m_bDead.ToString() + " visible=" + (showDead || !m_bDead).ToString());
		if (m_wRoot)
			m_wRoot.SetVisible(showDead || !m_bDead);
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	// Buttons
	override bool OnClick(Widget w, int x, int y, int button)
	{
		super.OnClick(w, x, y, button);
		if (button == 1)
		{
			RplComponent rplComponent = RplComponent.Cast(Replication.FindItem(m_iPlayableId));
			if (rplComponent)
			{
				SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(rplComponent.GetEntity());
				OpenContext(character);
			}
			return false;
		}
		if (button != 0)
			return false;
		
		// AlivePlayerButtonClicked is already invoked via m_OnClicked by super.OnClick.
		// Calling it directly here would cause a double-call (one from the event, one from below).
		return false;
	}
	// --------------------------------------------------------------------------------------------------------------------------------
	void OpenContext(SCR_ChimeraCharacter character)
	{
		MenuBase menu = GetGame().GetMenuManager().GetTopMenu();
		if (!menu)
			return;
		
		PS_PlayableComponent playableComponent = character.PS_GetPlayable();
		if (!playableComponent)
			return;
		
		int playerId = PS_PlayableManager.GetInstance().GetPlayerByPlayable(playableComponent.GetRplId());
		string playerName = PS_PlayableManager.GetInstance().GetPlayerName(playerId);
		if (playerName == "")
		{
			playerName = playableComponent.GetName();
		}
		PS_ContextMenu contextMenu = PS_ContextMenu.CreateContextMenuOnMousePosition(menu.GetRootWidget(), playerName);
		
		PS_AttachManualCameraObserverComponent attachComponent = PS_AttachManualCameraObserverComponent.s_Instance;
		if (!attachComponent.GetTarget())
			contextMenu.ActionAttachTo(character).Insert(OnActionAttachTo);
		else
			contextMenu.ActionDetachFrom(character).Insert(OnActionDetachFrom);
		contextMenu.ActionLookAt(character).Insert(OnActionLookAt);
		contextMenu.ActionFirstPersonView(character).Insert(OnActionFirstPersonView);
		contextMenu.ActionRespawnInPlace(playableComponent.GetRplId(), playerId);
		if (playerId > 0)
		{
			contextMenu.ActionDirectMessage(playerId);
			contextMenu.ActionKick(playerId);
			
			if (PS_PlayersHelper.IsAdminOrServer())
			{
				contextMenu.ActionGetArmaId(playerId);
			}
		}
	}
	void OnActionAttachTo(PS_ContextAction contextAction, PS_ContextActionDataCharacter contextActionDataCharacter)
	{
		PS_AttachManualCameraObserverComponent attachComponent = PS_AttachManualCameraObserverComponent.s_Instance;
		if (!attachComponent)
			return;
		
		SCR_ChimeraCharacter character = contextActionDataCharacter.GetCharacter();
		attachComponent.AttachTo(character);
	}
	void OnActionDetachFrom(PS_ContextAction contextAction, PS_ContextActionDataCharacter contextActionDataCharacter)
	{
		PS_AttachManualCameraObserverComponent attachComponent = PS_AttachManualCameraObserverComponent.s_Instance;
		if (!attachComponent)
			return;
		
		attachComponent.Detach();
	}
	void OnActionLookAt(PS_ContextAction contextAction, PS_ContextActionDataCharacter contextActionDataCharacter)
	{
		SCR_ChimeraCharacter character = contextActionDataCharacter.GetCharacter();
		PS_SpectatorLabel spectatorLabel = PS_SpectatorLabel.Cast(character.FindComponent(PS_SpectatorLabel));
		if (!spectatorLabel)
			return;
		PS_SpectatorMenu spectatorMenu = PS_SpectatorMenu.Cast(GetGame().GetMenuManager().GetTopMenu());
		if (!spectatorMenu)
			return;
		spectatorMenu.SetLookTarget(spectatorLabel.GetLabelIcon());
	}
	void OnActionFirstPersonView(PS_ContextAction contextAction, PS_ContextActionDataCharacter contextActionDataCharacter)
	{
		PS_AttachManualCameraObserverComponent attachComponent = PS_AttachManualCameraObserverComponent.s_Instance;
		if (!attachComponent)
			return;
		
		PS_SpectatorMenu spectatorMenu = PS_SpectatorMenu.Cast(GetGame().GetMenuManager().GetTopMenu());
		if (!spectatorMenu)
			return;
		
		spectatorMenu.SetLookTarget(null);
		attachComponent.Detach();
		
		SCR_ChimeraCharacter character = contextActionDataCharacter.GetCharacter();
		PS_ManualCameraSpectator camera = PS_ManualCameraSpectator.Cast(GetGame().GetCameraManager().CurrentCamera());
		if (camera)
			camera.SetCharacterEntity(character);
	}
	
	// -------------------- Buttons events --------------------
	void AlivePlayerButtonClicked(SCR_ButtonBaseComponent playerButton)
	{
		if (!m_mSpectatorMenu)
		{
			PS_DebugLogger.LogError("Spectator AlivePlayerButtonClicked: m_mSpectatorMenu is null");
			return;
		}
		bool ok = m_mSpectatorMenu.SetCameraCharacter(m_iPlayableId);
		PS_DebugLogger.LogImportant("Spectator AlivePlayerClicked playableId=" + m_iPlayableId.ToString() + " ok=" + ok.ToString());
	}
	
}