// PS_M_SCR_VONController - keep the VON push-to-talk input alive for body-less menu speakers.
//
// THE spectator-voice fix (keeps corpses in the world). Two vanilla behaviours together kill the menu PTT
// for a player who died with a slot (they keep CONTROLLING their dead corpse):
//   1. SCR_VONController.Update() activates the VON input context every frame - this is what makes the
//      VONDirect ("talk") action available to listeners - but only while NOT unconscious.
//   2. SCR_VONController.UpdateSystemState() DISCONNECTS the controller from the update system entirely
//      once the controlled character is dead/unconscious, so Update() stops being called at all.
// Net result: a dead-controlling spectator gets no VON context -> VONDirect never reaches PS_MenuVoN (on
// the player's VoN proxy) -> they can HEAR but not SPEAK. A no-slot spectator controls nothing, is never
// flagged unconscious, stays registered, and so can speak - that asymmetry was the whole bug.
//
// Fix: while PS_MenuVoN is active (local player is a menu speaker - lobby / briefing / spectator), keep the
// controller registered AND re-assert the VON context every frame, regardless of the controlled corpse's
// life state. This does NOT let dead players talk on the in-game net: vanilla proximity/radio transmit
// stays gated by life state inside SCR_VONController, and PS_MenuVoN detached the controller's own VoN
// component (SetVONComponent(null)). Only the menu proxy radio transmits. PS_MenuVoN calls
// PS_RefreshSystemState() on (de)activate so registration is re-evaluated the moment speaker-ness flips
// (UpdateSystemState is otherwise only called by vanilla on life-state/disabled changes).
//
// VON_CONTEXT / VON_MENU_OPENING_CONTEXT and m_InputManager are inherited protected members.
modded class SCR_VONController
{
	override protected void UpdateSystemState()
	{
		if (PS_MenuVoN.IsActive())
		{
			ConnectToHandleUpdateVONControllersSystem();
			return;
		}
		super.UpdateSystemState();
	}

	override void Update(float timeSlice)
	{
		super.Update(timeSlice);

		if (!m_InputManager || !PS_MenuVoN.IsActive())
			return;

		// Re-assert the contexts vanilla stops activating once the controlled character is dead/unconscious.
		m_InputManager.ActivateContext(VON_CONTEXT);
		m_InputManager.ActivateContext(VON_MENU_OPENING_CONTEXT);
	}

	// Let PS_MenuVoN re-evaluate update-system registration when the menu device (de)activates.
	void PS_RefreshSystemState()
	{
		UpdateSystemState();
	}
}
