modded enum ChimeraMenuPreset : ScriptMenuPresetEnum
{
	WaitScreen
}

class PS_WaitScreen: MenuBase
{
	static bool m_bWaitEnded;
	
	TextWidget m_wInfoText;
	
	override void OnMenuOpen()
	{
		m_wInfoText = TextWidget.Cast(GetRootWidget().FindAnyWidget("InfoText"));
		
		if (RplSession.Mode() == RplMode.Dedicated) {
			Close();
			return;
		}
		GetGame().GetCallqueue().CallLater(AwaitPlayerController, 100, true);
	}
	
	void AwaitPlayerController()
	{
		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (!gameMode)
		{
			m_wInfoText.SetText("Awaiting gamemode entity.");
			return;
		}
		
		#ifdef WORKBENCH
		if (gameMode.GetState() != SCR_EGameModeState.GAME)
		{
			IEntity WBCharacter = SCR_PlayerController.GetLocalControlledEntity();
			if (WBCharacter)
			{
				SCR_VoNComponent WBVoN = SCR_VoNComponent.Cast(WBCharacter.FindComponent(SCR_VoNComponent));
				if (WBVoN)
				{
					PS_PlayableComponent WBPlayableComponent = PS_PlayableComponent.Cast(WBCharacter.FindComponent(PS_PlayableComponent));
				if (WBPlayableComponent)
					WBPlayableComponent.SetPlayable(true);
				RplComponent rplComponent = RplComponent.Cast(WBCharacter.FindComponent(RplComponent));
				if (rplComponent)
					PS_PlayableManager.GetInstance().SetPlayerToSlot(rplComponent.Id(), SCR_PlayerController.GetLocalPlayerId());
					gameMode.StartGameMode();
				}
			}
		}
		#endif
		
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
		{
			m_wInfoText.SetText("Awaiting player controller.");
			return;
		}
		
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		if (!playableManager.IsReplicated())
		{
			m_wInfoText.SetText("Awaiting playableManager replication.");
			return;
		}
		
		PS_VoNChannelsManager VoNChannelsManager = PS_VoNChannelsManager.GetInstance();
		if (!VoNChannelsManager.IsReplicated())
		{
			m_wInfoText.SetText("Awaiting VoNChannelsManager replication.");
			return;
		}
		
		if (playerController.GetPlayerId() == 0)
		{
			m_wInfoText.SetText("Awaiting player id.");
			return;
		}
		
		if (!playerController.GetControlledEntity())
		{
			m_wInfoText.SetText("Awaiting initial character.");
			return;
		}
		
		PS_PlayableControllerComponent playableControllerComponent = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
		if (!playableControllerComponent || !playableControllerComponent.isVonInit())
		{
			m_wInfoText.SetText("Awaiting VoN Initialization.");
			return;
		}
		
		int globalRoomId = VoNChannelsManager.GetRoomWithFaction("", "#PS-VoNRoom_Global");
		if (globalRoomId == -1)
		{
			m_wInfoText.SetText("Awaiting VoN room creation.");
			return;
		}
		
		int roomId = VoNChannelsManager.GetPlayerRoom(playerController.GetPlayerId());
		string roomKey = VoNChannelsManager.GetRoomName(roomId);
		
		m_bWaitEnded = true;
		Close();
		
		PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
		if (!playableController)
			return;
		playableController.SetPlayerState(playerController.GetPlayerId(), PS_EPlayableControllerState.NotReady);
		playableController.MoveToVoNRoomByKey(playerController.GetPlayerId(), roomKey);
		playableController.SwitchToMenu(gameMode.GetState());
		
		GetGame().GetCallqueue().Remove(AwaitPlayerController);
	}
	
	override void OnMenuClose()
	{
		
	}
}