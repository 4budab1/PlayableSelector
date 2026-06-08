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
    PS_DebugLogger.LogImportant("PS_WaitScreen OnMenuOpen — starting AwaitPlayerController loop");
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

    // Fix #1: Declare playerId early so it's available throughout the method
    int playerId = playerController.GetPlayerId();

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

    if (playerId == 0)
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

    int roomId = VoNChannelsManager.GetPlayerRoom(playerId);
    string roomKey = VoNChannelsManager.GetRoomName(roomId);

    SCR_EGameModeState currentState = gameMode.GetState();
    RplId mySlotId = playableManager.GetPlayableByPlayer(playerId);
    PS_EPlayableControllerState myState = playableManager.GetPlayerState(playerId);

    // Log full JIP state on client
    PS_DebugLogger.LogImportant("PS_WaitScreen ALL CHECKS PASSED playerId=" + playerId.ToString()
      + " gameModeState=" + typename.EnumToString(SCR_EGameModeState, currentState)
      + " slotId=" + mySlotId.ToString()
      + " playerState=" + typename.EnumToString(PS_EPlayableControllerState, myState)
      + " freezeTimeLeft=" + gameMode.GetCurrentFreezeTime().ToString()
      + " roomKey=" + roomKey, playerId);

    m_bWaitEnded = true;
    Close();

    PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
    if (!playableController)
      return;

    // In GAME state the server handles player state (ApplyPlayable already ran)
    // Only set state in non-GAME states where client is authoritative
    if (currentState != SCR_EGameModeState.GAME)
    {
      if (mySlotId != RplId.Invalid() && !playableManager.IsSlotCharacterDestroyed(mySlotId))
      {
        PS_DebugLogger.LogImportant("PS_WaitScreen setting state Playing", playerId);
        playableController.SetPlayerState(playerId, PS_EPlayableControllerState.Playing);
      }
      else
      {
        PS_DebugLogger.LogImportant("PS_WaitScreen setting state NotReady, switching to menu state=" + typename.EnumToString(SCR_EGameModeState, currentState), playerId);
        playableController.SetPlayerState(playerId, PS_EPlayableControllerState.NotReady);
      }
    }
    else
    {
      PS_DebugLogger.LogImportant("PS_WaitScreen GAME state — trusting server playerState=" + typename.EnumToString(PS_EPlayableControllerState, myState), playerId);
    }

    playableController.MoveToVoNRoomByKey(playerId, roomKey);
    playableController.SwitchToMenu(currentState);

    // Fix #3: If JIP client arrives during GAME with a slot, request deploy from client side
    if (currentState == SCR_EGameModeState.GAME && mySlotId != RplId.Invalid())
    {
      PS_DebugLogger.LogImportant("PS_WaitScreen JIP in GAME — requesting client-side deploy slot=" + mySlotId.ToString(), playerId);
      playableController.RequestDeployFromClient();
    }

    // Fix #2: If JIP client arrives after freeze time ended, clean up stale freeze UI
    if (currentState == SCR_EGameModeState.GAME && gameMode.IsFreezeTimeEnd())
    {
      PS_DebugLogger.LogImportant("PS_WaitScreen JIP in GAME freeze already ended — ensuring freeze UI cleaned up", playerId);
      gameMode.RPC_restrictedZonesTimer(0);
    }

    GetGame().GetCallqueue().Remove(AwaitPlayerController);
  }

  override void OnMenuClose()
  {

  }
}
