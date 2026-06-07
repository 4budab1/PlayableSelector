// Widget displays info about playable character.
// Path: {3F761F63F1DF29D1}UI/Lobby/CharacterSelector.layout
// Part of Lobby menu PS_CoopLobby ({9DECCA625D345B35}UI/Lobby/CoopLobby.layout)
// Lobby insert it into PS_RolesGroup widget

// State states...
enum PS_ECharacterState
{
	Empty,
	Player,
	Disconnected,
	Pin,
	Kick,
	Lock,
	Dead,
}

class PS_CharacterSelector : SCR_ButtonComponent
{
	// Const
	protected ResourceName m_sUIWrapper = "{2EFEA2AF1F38E7F0}UI/Textures/Icons/icons_wrapperUI-64.imageset";
	const static ResourceName IMAGESET_PS = "{F3A9B47F55BE8D2B}UI/Textures/Icons/PS_Atlas_x64.imageset";
	
	protected ref Color m_DefaultColor = Color.White;
	protected ref Color m_AdminColor = Color.FromInt(0xfff2a34b);
	protected ref Color m_DeathColor = Color.FromInt(0xFF2c2c2c);
	protected ref Color m_ReadyColor = Color.Green;
	
	// Cache global
	protected PS_GameModeCoop m_GameModeCoop;
	protected PS_PlayableManager m_PlayableManager;
	protected PlayerController m_PlayerController;
	protected SCR_FactionManager m_FactionManager;
	protected PlayerManager m_PlayerManager;
	protected PS_PlayableControllerComponent m_PlayableControllerComponent;
	protected int m_iCurrentPlayerId;
	
	// Widgets
	protected ImageWidget m_wCharacterFactionColor;
	protected ImageWidget m_wUnitIcon;
	protected TextWidget m_wCharacterClassName;
	protected ImageWidget m_wStateIcon;
	protected ButtonWidget m_wStateButton;
	protected RichTextWidget m_wCharacterStatus;
	protected OverlayWidget m_wVoiceHideableButton;
	
	// Handlers
	protected SCR_ButtonBaseComponent m_StateButtonBaseComponent;
	protected PS_VoiceButton m_VoiceHideableButton;
	
	// Parameters
	protected PS_ECharacterState m_state;
	protected PS_CoopLobby m_CoopLobby;
	protected PS_RolesGroup m_RolesGroup;
	protected PS_PlayableContainer m_PlayableContainer;
	protected RplId m_iPlayableId;
	protected int m_iPlayableCallsign;
	protected string m_sPlayableCallsign;
	
	protected bool m_bDead;
	protected bool m_bPined;
	protected bool m_bDisconnected;
	protected EDamageState m_iDamageState;
	protected int m_iPlayerId;
	protected bool m_bAdmin;
	protected bool m_bReady;
	protected bool m_bCanKick;
	
	protected bool m_bStateClickSkip;
	
	// Cache parameters
	protected SCR_CharacterDamageManagerComponent m_CharacterDamageManagerComponent;
	protected SCR_Faction m_Faction;
	protected FactionKey m_sFactionKey;
	
	// --------------------------------------------------------------------------------------------------------------------------------
	// Init
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		
		// Cache global (stuff not depending on PlayerController)
		m_GameModeCoop = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		m_PlayableManager = PS_PlayableManager.GetInstance();
		
		// Widgets
		m_wCharacterFactionColor = ImageWidget.Cast(w.FindAnyWidget("CharacterFactionColor"));
		m_wUnitIcon = ImageWidget.Cast(w.FindAnyWidget("UnitIcon"));
		m_wCharacterClassName = TextWidget.Cast(w.FindAnyWidget("CharacterClassName"));
		m_wStateIcon = ImageWidget.Cast(w.FindAnyWidget("StateIcon"));
		m_wStateButton = ButtonWidget.Cast(w.FindAnyWidget("StateButton"));
		m_wCharacterStatus = RichTextWidget.Cast(w.FindAnyWidget("CharacterStatus"));
		m_wVoiceHideableButton = OverlayWidget.Cast(w.FindAnyWidget("VoiceHideableButton"));
		
		// Handlers
		m_StateButtonBaseComponent = SCR_ButtonBaseComponent.Cast(m_wStateButton.FindHandler(SCR_ButtonBaseComponent));
		m_VoiceHideableButton = PS_VoiceButton.Cast(m_wVoiceHideableButton.FindHandler(PS_VoiceButton));
		
		// Buttons
		m_OnHover.Insert(OnHover);
		m_OnHoverLeave.Insert(OnHoverLeave);
		m_StateButtonBaseComponent.m_OnClicked.Insert(OnStateClicked);
		
		// Events (from PlayableManager — always available)
		m_PlayableManager.GetOnPlayerPlayableChange().Insert(OnPlayerPlayableChange);
		m_PlayableManager.GetOnPlayerStateChange().Insert(OnStateChange);
		m_PlayableManager.GetOnPlayerPinChange().Insert(UpdatePined);
		m_PlayableManager.GetOnPlayerConnected().Insert(OnConnected);
	}
	
	override void HandlerDeattached(Widget w)
	{
		PS_PlayableControllerComponent pcc = GetControllerComp();
		if (pcc)
			pcc.GetOnPlayerRoleChange().Remove(OnRoleChangeCurrent);
		if (m_PlayableManager)
		{
			m_PlayableManager.GetOnPlayerPlayableChange().Remove(OnPlayerPlayableChange);
			m_PlayableManager.GetOnPlayerStateChange().Remove(OnStateChange);
			m_PlayableManager.GetOnPlayerPinChange().Remove(UpdatePined);
			m_PlayableManager.GetOnPlayerConnected().Remove(OnConnected);
		}
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	// Helpers
	PS_PlayableControllerComponent GetControllerComp()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return null;
		return PS_PlayableControllerComponent.Cast(pc.FindComponent(PS_PlayableControllerComponent));
	}
	
	int GetCurrentPlayerId()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return -1;
		return pc.GetPlayerId();
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	// Set
	void SetLobbyMenu(PS_CoopLobby coopLobby)
	{
		m_CoopLobby = coopLobby;
	}
	
	void SetRolesGroup(PS_RolesGroup rolesGroup)
	{
		m_RolesGroup = rolesGroup;
	}
	
	void SetPlayable(PS_PlayableContainer playableContainer)
	{
		m_PlayableContainer = playableContainer;
		m_iPlayableId = playableContainer.GetRplId();
		m_iPlayerId = m_PlayableManager.GetPlayerByPlayable(m_iPlayableId);
		
		// Visual setup — safe fallbacks if container data not available on client
		m_sFactionKey = "";
		if (m_PlayableContainer)
		{
			m_Faction = m_PlayableContainer.GetFaction();
			if (m_Faction)
				m_sFactionKey = m_Faction.GetFactionKey();
			
			if (m_wUnitIcon)
				m_PlayableContainer.SetIconTo(m_wUnitIcon);
			if (m_wCharacterFactionColor && m_Faction)
				m_wCharacterFactionColor.SetColor(m_Faction.GetFactionColor());
			if (m_wCharacterClassName)
				m_wCharacterClassName.SetText(m_PlayableContainer.GetName());
		}
		
		// Fallback: get faction key from slot data if container failed
		if (m_sFactionKey == "" && m_PlayableManager)
		{
			PS_SlotCharacterData slotData;
			if (m_PlayableManager.FindSlotData(m_iPlayableId, slotData))
				m_sFactionKey = slotData.m_FactionKey;
		}
		
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (m_iPlayerId > 0 && playerManager)
			m_bDisconnected = !playerManager.IsPlayerConnected(m_iPlayerId);
		m_iPlayableCallsign = m_PlayableManager.GetGroupCallsignByPlayable(m_iPlayableId);
		m_sPlayableCallsign = m_iPlayableCallsign.ToString();
		
		UpdatePlayer(0, m_iPlayerId);
		UpdateStateIcon();
		
		// Events (container-level — only work if m_PlayableContainer is valid)
		if (m_PlayableContainer)
		{
			m_PlayableContainer.GetOnPlayerChange().Insert(UpdatePlayer);
			m_PlayableContainer.GetOnDamageStateChanged().Insert(UpdateDammage);
			m_PlayableContainer.GetOnUnregister().Insert(RemoveSelf);
			m_PlayableContainer.GetOnPlayerRoleChange().Insert(OnRoleChange);
		}
	}
	
	PS_PlayableContainer GetPlayable()
	{
		return m_PlayableContainer;
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	// Update character
	void UpdateDammage(EDamageState state)
	{
		m_iDamageState = state;
		m_bDead = m_iDamageState == EDamageState.DESTROYED;
		UpdateState();
	}
	
	void UpdatePined(int playerId, bool pined)
	{
		if (playerId != m_iPlayerId) return;
		m_bPined = pined;
		UpdateState();
	}
	
	void UpdatePlayer(int oldPlayerId, int playerId)
	{
		m_VoiceHideableButton.SetPlayer(playerId);
		
		if (m_iPlayerId != -2 && playerId == -2)
		{
			m_RolesGroup.UpdateLockedState(true);
			m_CoopLobby.AddFactionCount(m_Faction, 0, 0, 1);
		}
		if (m_iPlayerId == -2 && playerId != -2)
		{
			m_RolesGroup.UpdateLockedState(false);
			m_CoopLobby.AddFactionCount(m_Faction, 0, 0, -1);
		}
		
		PS_EPlayableControllerState state = m_PlayableManager.GetPlayerState(m_iPlayerId);
		m_bReady = state == PS_EPlayableControllerState.Ready;
		
		if (m_iPlayerId > 0 && playerId <= 0)
		{
			m_CoopLobby.AddFactionCount(m_Faction, -1, 0);
		}
		if (m_iPlayerId <= 0 && playerId > 0)
			m_CoopLobby.AddFactionCount(m_Faction, 1, 0);
		
		m_iPlayerId = playerId;
		
		m_bPined = m_PlayableManager.GetPlayerPin(m_iPlayerId);
		if (m_iPlayerId > 0)
		{
			PlayerManager pm = GetGame().GetPlayerManager();
			if (pm)
				m_bDisconnected = !pm.IsPlayerConnected(m_iPlayerId);
			else
				m_bDisconnected = false;
		}
		else
		{
			m_bDisconnected = false;
		}
		
		string playerName = m_PlayableManager.GetPlayerName(playerId);
		m_wCharacterStatus.SetText(playerName);
		
		m_bAdmin = SCR_Global.IsAdmin(playerId);
		
		if (oldPlayerId == GetCurrentPlayerId() || playerId == GetCurrentPlayerId())
			m_CoopLobby.SetPreviewPlayable(m_iPlayableId, false);

		OnPlayerPlayableChange(playerId, m_iPlayableId);
		UpdateColor();
	}
	
	void RemoveSelf()
	{
		m_wRoot.RemoveFromHierarchy();
		m_RolesGroup.OnPlayableRemoved(m_PlayableContainer);
		m_CoopLobby.OnPlayableRemoved(m_PlayableContainer.GetRplId(), m_sFactionKey, m_iPlayerId);
		if (m_iPlayerId == -2)
			m_RolesGroup.UpdateLockedState(false);
	}
	
	void OnPlayerPlayableChange(int playerId, RplId playbleId)
	{
		// Clear occupant if this slot no longer has a player assigned to it
		if (playbleId != m_iPlayableId && m_iPlayerId > 0 && m_iPlayerId == playerId)
		{
			m_CoopLobby.AddFactionCount(m_Faction, -1, 0);
			m_iPlayerId = -1;
			if (m_wCharacterStatus)
				m_wCharacterStatus.SetText("");
			UpdateStateIcon();
			UpdateColor();
		}
		
		// Visual update for this slot when assigned
		PS_SlotCharacterData slotData;
		if (playbleId == m_iPlayableId && m_PlayableManager && m_PlayableManager.FindSlotData(m_iPlayableId, slotData))
		{
			int newOccupant = slotData.m_PlayerId;
			if (newOccupant != m_iPlayerId)
			{
				if (m_iPlayerId <= 0 && newOccupant > 0)
					m_CoopLobby.AddFactionCount(m_Faction, 1, 0);
				if (m_iPlayerId > 0 && newOccupant <= 0)
					m_CoopLobby.AddFactionCount(m_Faction, -1, 0);
				
				m_iPlayerId = newOccupant;
				if (m_wCharacterStatus)
				{
					string playerName = m_PlayableManager.GetPlayerName(newOccupant);
					if (playerName != "")
						m_wCharacterStatus.SetText(playerName);
				}
				UpdateStateIcon();
				UpdateColor();
			}
		}
		
		// Self kick
		if (m_iPlayerId == GetCurrentPlayerId())
		{
			m_bCanKick = false;
			UpdateState();
			return;
		}
		
		// Admin can kick any
		m_bCanKick = PS_PlayersHelper.IsAdminOrServer();
		if (m_bCanKick)
		{
			UpdateState();
			return;
		}
		
		// get CURRENT PLAYER playable
		RplId currentPlayableId = m_PlayableManager.GetPlayableByPlayer(GetCurrentPlayerId());
		if (currentPlayableId == RplId.Invalid())
		{
			m_bCanKick = false;
			UpdateState();
			return;
		}
		
		// Only group leader can kick (Longest event?)
		FactionKey factionKeyCurrent = m_PlayableManager.GetPlayerFactionKey(GetCurrentPlayerId());
		SCR_AIGroup groupCurrent = m_PlayableManager.GetPlayerGroupByPlayable(currentPlayableId);
		FactionKey factionKey = m_PlayableManager.GetPlayerFactionKey(m_iPlayerId);
		SCR_AIGroup group = m_PlayableManager.GetPlayerGroupByPlayable(m_iPlayableId);
		m_bCanKick = m_PlayableManager.IsPlayerGroupLeader(GetCurrentPlayerId())
			&& factionKeyCurrent == factionKey
			&& groupCurrent == group;
		
		UpdateState();
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	// Get
	int GetPlayerId()
	{
		return m_iPlayerId;
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	// Update player
	void OnDisconnected(int playerId, KickCauseCode cause = KickCauseCode.NONE, int timeout = -1)
	{
		m_bDisconnected = true;
		
		m_wCharacterStatus.SetColor(m_DeathColor);
		
		UpdateColor();
		UpdateState();
	}
	
	void OnConnected(int playerId)
	{
		m_bDisconnected = false;
		
		UpdateColor();
		UpdateState();
	}
	
	void OnStateChange(int playerId, PS_EPlayableControllerState state)
	{
		if (playerId != m_iPlayerId) return;
		m_bReady = state == PS_EPlayableControllerState.Ready;
		m_bDisconnected = state == PS_EPlayableControllerState.Disconnected;
		
		UpdateColor();
	}
	
	void OnRoleChangeCurrent(int playerId, EPlayerRole roleFlags)
	{
		UpdateState(true);
	}
	
	void OnRoleChange(int playerId, EPlayerRole roleFlags)
	{
		m_bAdmin = roleFlags & EPlayerRole.ADMINISTRATOR ||
			roleFlags & EPlayerRole.SESSION_ADMINISTRATOR;
		
		if (m_bAdmin)
			m_wCharacterStatus.SetColor(m_AdminColor);
		else
			m_wCharacterStatus.SetColor(m_DefaultColor);
		
		UpdateState(true);
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	// State
	void UpdateColor()
	{
		if (m_bDisconnected)
			m_wCharacterStatus.SetColor(m_DeathColor);
		else if (m_bReady)
			m_wCharacterStatus.SetColor(m_ReadyColor);
		else if (m_bAdmin)
				m_wCharacterStatus.SetColor(m_AdminColor);
			else
				m_wCharacterStatus.SetColor(m_DefaultColor);
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	void UpdateState(bool forceIcons = false)
	{
		PS_ECharacterState state = PS_ECharacterState.Empty;
		if (m_bDead)
			state = PS_ECharacterState.Dead;
		else if (m_bDisconnected)
			state = PS_ECharacterState.Disconnected;
		else if (m_bPined)
			state = PS_ECharacterState.Pin;
		else if (m_iPlayerId == -2)
			state = PS_ECharacterState.Lock;
		//else if (m_bCanKick && m_iPlayerId >= 0 && m_iPlayerId != GetCurrentPlayerId())
		//	state = PS_ECharacterState.Kick;
		else if (m_iPlayerId >= 0)
			state = PS_ECharacterState.Player;
		
		if (forceIcons || m_state != state)
		{
			m_state = state;
			UpdateStateIcon();
		}
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	void UpdateStateIcon()
	{
		m_wStateIcon.SetVisible(true);
		m_wStateButton.SetVisible(false);
		switch (m_state)
		{
			case PS_ECharacterState.Pin:
				m_wStateIcon.LoadImageFromSet(0, m_sUIWrapper, "pinPlay");
				break;
			case PS_ECharacterState.Dead:
				m_wStateIcon.LoadImageFromSet(0, m_sUIWrapper, "death");
				break;
			case PS_ECharacterState.Lock:
				m_wStateIcon.LoadImageFromSet(0, IMAGESET_PS, "Locked");
				break;
			case PS_ECharacterState.Kick:
				m_wStateIcon.LoadImageFromSet(0, m_sUIWrapper, "kickCommandAlt");
				break;
			case PS_ECharacterState.Empty:
				m_wStateIcon.LoadImageFromSet(0, IMAGESET_PS, "Unlocked");
				m_wStateIcon.SetVisible(false);
				break;
			case PS_ECharacterState.Player:
				m_wStateIcon.LoadImageFromSet(0, m_sUIWrapper, "player");
				break;
			case PS_ECharacterState.Disconnected:
				m_wStateIcon.LoadImageFromSet(0, m_sUIWrapper, "disconnection");
				break;
		}
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	// Buttons
	override bool OnClick(Widget w, int x, int y, int button)
	{
		super.OnClick(w, x, y, button);
		if (button == 1)
		{
			OpenContext();
			return false;
		}
		if (button != 0)
			return false;
		
		OnClicked(this);
		return false;
	}
	void OnClicked(SCR_ButtonBaseComponent button)
	{
		if (m_bStateClickSkip)
		{
			m_bStateClickSkip = false;
			return;
		}
		
		PS_SlotCharacterData slotData;
		if (!m_PlayableManager || !m_PlayableManager.FindSlotData(m_iPlayableId, slotData))
			return;
		
		int slotOccupant = slotData.m_PlayerId;
		int selectedPlayerId = -1;
		if (m_CoopLobby)
			selectedPlayerId = m_CoopLobby.GetSelectedPlayer();
		int currentPlayerId = GetCurrentPlayerId();
		
		PS_PlayableControllerComponent pcc = GetControllerComp();
		SCR_EGameModeState gameState = SCR_EGameModeState.PREGAME;
		if (m_GameModeCoop)
			gameState = m_GameModeCoop.GetState();
		
		// Locked slot
		if (slotOccupant == -2)
		{
			if (m_CoopLobby) m_CoopLobby.SetPreviewPlayable(m_iPlayableId, true);
			SCR_UISoundEntity.SoundEvent("SOUND_FE_BUTTON_FAIL");
			return;
		}
		
		// Can't take someone else's slot
		if (slotOccupant > 0 && selectedPlayerId != slotOccupant && !PS_PlayersHelper.IsAdminOrServer())
		{
			if (m_CoopLobby) m_CoopLobby.SetPreviewPlayable(m_iPlayableId, true);
			return;
		}
		
		// Briefing guard
		if (!PS_PlayersHelper.IsAdminOrServer())
		{
			RplId currentPlayableId = m_PlayableManager.GetPlayableByPlayer(currentPlayerId);
			if (gameState == SCR_EGameModeState.BRIEFING && currentPlayableId != RplId.Invalid())
			{
				if (m_CoopLobby) m_CoopLobby.SetPreviewPlayable(m_iPlayableId, true);
				return;
			}
		}
		
		if (!pcc)
			return;
		
		if (slotOccupant > 0 && selectedPlayerId == slotOccupant)
		{
			// Vacate own slot
			SCR_UISoundEntity.SoundEvent("SOUND_HUD_GADGET_SELECT");
			GetGame().GetCallqueue().Call(pcc.SetPlayerState, selectedPlayerId, PS_EPlayableControllerState.NotReady);
			GetGame().GetCallqueue().Call(pcc.SetPlayerToSlot, RplId.Invalid(), selectedPlayerId);
			if (PS_PlayersHelper.IsAdminOrServer())
				GetGame().GetCallqueue().Call(pcc.UnpinPlayer, selectedPlayerId);
		}
		else
		{
			// Assign to slot + open inventory preview (matches vanilla behavior)
			SCR_UISoundEntity.SoundEvent("SOUND_HUD_GADGET_SELECT");
			GetGame().GetCallqueue().Call(pcc.SetPlayerState, selectedPlayerId, PS_EPlayableControllerState.NotReady);
			GetGame().GetCallqueue().Call(pcc.SetPlayerToSlot, m_iPlayableId, selectedPlayerId);
			if (m_CoopLobby)
				GetGame().GetCallqueue().Call(m_CoopLobby.SetPreviewPlayable, m_iPlayableId, false);
		}
		
		if (PS_PlayersHelper.IsAdminOrServer() && selectedPlayerId != currentPlayerId && gameState == SCR_EGameModeState.GAME)
			GetGame().GetCallqueue().Call(pcc.ForceSwitch, selectedPlayerId);
		if (!PS_PlayersHelper.IsAdminOrServer() && selectedPlayerId == currentPlayerId && gameState == SCR_EGameModeState.BRIEFING)
			GetGame().GetCallqueue().Call(pcc.SwitchToMenuServer, SCR_EGameModeState.BRIEFING);
	}
	
	void OnHover()
	{
		if (m_PlayableContainer)
			m_CoopLobby.SetPreviewPlayable(m_PlayableContainer.GetRplId(), false);
	}
	
	void OnHoverLeave()
	{
		if (m_PlayableContainer)
			m_CoopLobby.SetPreviewPlayable(RplId.Invalid(), false);
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	// Context menu
	void OpenContext()
	{
		string playerName = PS_PlayableManager.GetInstance().GetPlayerName(m_iPlayerId);
		PS_ContextMenu contextMenu = PS_ContextMenu.CreateContextMenuOnMousePosition(m_CoopLobby.GetRootWidget(), playerName);
		contextMenu.ActionOpenInventory(m_iPlayableId).Insert(OnActionOpenInventory);
		
		if (m_iPlayerId > 0)
		{
			if (PS_PlayersHelper.IsAdminOrServer())
			{
				contextMenu.ActionGetArmaId(m_iPlayerId);
			}
			if (m_iPlayerId != GetCurrentPlayerId())
			{
				PermissionState mute = PermissionState.DISALLOWED;
				SocialComponent socialComp = SocialComponent.Cast(GetGame().GetPlayerController().FindComponent(SocialComponent));
				if (socialComp.IsMuted(m_iPlayerId))
					contextMenu.ActionUnmute(m_iPlayerId);
				else
					contextMenu.ActionMute(m_iPlayerId);
				
				if (m_bCanKick && m_iPlayerId >= 0 && m_iPlayerId != GetCurrentPlayerId())
					contextMenu.ActionFreeSlot(m_iPlayableId).Insert(OnActionFreeSlot);
				
				if (PS_PlayersHelper.IsAdminOrServer())
				{
					contextMenu.ActionDirectMessage(m_iPlayerId);
					contextMenu.ActionKick(m_iPlayerId);
					
					if (m_PlayableManager.GetPlayerPin(m_iPlayerId))
						contextMenu.ActionUnpin(m_iPlayerId);
					else
						contextMenu.ActionPin(m_iPlayerId);
				}
			}
			if (m_CoopLobby.GetSelectedPlayer() != m_iPlayerId && PS_PlayersHelper.IsAdminOrServer())
			{
				contextMenu.ActionPlayerSelect(m_iPlayerId);
			}
		}
		
		if (PS_PlayersHelper.IsAdminOrServer())
			if (m_iPlayerId != -2)
				contextMenu.ActionLock(m_iPlayableId).Insert(OnActionLock);
			else
				contextMenu.ActionUnlock(m_iPlayableId).Insert(OnActionUnlock);
	}
	void OnActionOpenInventory(PS_ContextAction contextAction, PS_ContextActionDataPlayable contextActionDataPlayable)
	{
		m_CoopLobby.SetPreviewPlayable(contextActionDataPlayable.GetPlayableId(), true);
	}
	void OnActionLock(PS_ContextAction contextAction, PS_ContextActionDataPlayable contextActionDataPlayable)
	{
		SCR_UISoundEntity.SoundEvent("SOUND_FE_BUTTON_FILTER_ON");
		if (m_iPlayerId > 0)
			OnActionFreeSlot(contextAction, contextActionDataPlayable);
		PS_PlayableControllerComponent pcc = GetControllerComp();
		if (pcc)
			pcc.SetSlotLockState(contextActionDataPlayable.GetPlayableId(), true);
	}
	void OnActionUnlock(PS_ContextAction contextAction, PS_ContextActionDataPlayable contextActionDataPlayable)
	{
		SCR_UISoundEntity.SoundEvent("SOUND_FE_BUTTON_FILTER_OFF");
		PS_PlayableControllerComponent pcc = GetControllerComp();
		if (pcc)
			pcc.SetSlotLockState(contextActionDataPlayable.GetPlayableId(), false);
	}
	void OnActionFreeSlot(PS_ContextAction contextAction, PS_ContextActionDataPlayable contextActionDataPlayable)
	{
		if (m_iPlayerId <= 0)
			return;
		
		SCR_UISoundEntity.SoundEvent("SOUND_LOBBY_KICK");
		PS_PlayableControllerComponent pcc = GetControllerComp();
		if (pcc)
		{
			pcc.SetPlayerState(m_iPlayerId, PS_EPlayableControllerState.NotReady);
			pcc.KickPlayerFromSlot(m_iPlayableId);
			if (PS_PlayersHelper.IsAdminOrServer())
				pcc.UnpinPlayer(m_iPlayerId);
		}
	}
	
	// --------------------------------------------------------------------------------------------------------------------------------
	void OnStateClicked(SCR_ButtonBaseComponent button)
	{
		m_bStateClickSkip = true;
		PS_PlayableControllerComponent pcc = GetControllerComp();
		if (!pcc)
			return;
		switch (m_state)
		{
			case PS_ECharacterState.Pin:
				SCR_UISoundEntity.SoundEvent("SOUND_E_LAYER_BACK");
				pcc.UnpinPlayer(m_iPlayerId);
				break;
			case PS_ECharacterState.Dead:
				break;
			case PS_ECharacterState.Lock:
				SCR_UISoundEntity.SoundEvent("SOUND_FE_BUTTON_FILTER_OFF");
				pcc.SetSlotLockState(m_iPlayableId, false);
				break;
			case PS_ECharacterState.Kick:
				SCR_UISoundEntity.SoundEvent("SOUND_LOBBY_KICK");
				pcc.SetPlayerState(m_iPlayerId, PS_EPlayableControllerState.NotReady);
				pcc.KickPlayerFromSlot(m_iPlayableId);
				if (PS_PlayersHelper.IsAdminOrServer())
					pcc.UnpinPlayer(m_iPlayerId);
				break;
			case PS_ECharacterState.Empty:
				SCR_UISoundEntity.SoundEvent("SOUND_FE_BUTTON_FILTER_ON");
				if (m_iPlayerId > 0)
					pcc.SetPlayerState(m_iPlayerId, PS_EPlayableControllerState.NotReady);
				pcc.SetSlotLockState(m_iPlayableId, true);
			   break;
			case PS_ECharacterState.Player:
				break;
			case PS_ECharacterState.Disconnected:
				SCR_UISoundEntity.SoundEvent("SOUND_LOBBY_KICK");
				pcc.SetPlayerToSlot(RplId.Invalid(), m_iPlayerId);
				break;
		}
	}
}




















