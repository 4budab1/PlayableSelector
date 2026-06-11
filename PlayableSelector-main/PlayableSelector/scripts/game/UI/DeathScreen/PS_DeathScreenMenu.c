// Call of Duty-style death screen
// Flow (matches user's spec):
//   1) Screen fades to black (2s) — camera stays on dead body, heartbeat sounds start playing
//   2) Quote displays on black screen (8s)
//   3) Sounds stop, overlay with quote stops
//   4) SwitchToObserver(null) chains to normal spectator flow
// Total: 10s from death to spectator
modded enum ChimeraMenuPreset : ScriptMenuPresetEnum
{
	DeathScreen
}

class PS_DeathScreenMenu : MenuBase
{
	// Static guard: prevents multiple death-screen menus from opening simultaneously
	// for the same player. If the server sends a second death-screen RPC while one
	// is already open (e.g. double HandlePlayerKilled), the second one is ignored.
	static bool s_bDeathScreenOpen = false;

	// Timing (in seconds)
	static const float EYES_DELAY_S		= 0.0;	// No delay — start the fade immediately on death
	static const float FADE_DURATION_S	= 2.0;	// Fade to black duration (sounds start here)
	static const float QUOTE_DURATION_S	= 8.0;	// Black screen with quote duration (sounds play through this)

	protected float m_fElapsedTime = 0;
	protected bool m_bSoundsStopped = false;
	protected bool m_bDiagAt1s = false;
	protected bool m_bDiagAt3s = false;

	// Widgets
	protected ImageWidget m_wBlackOverlay;
	protected TextWidget m_wQuoteText;
	protected TextWidget m_wQuoteAuthor;
	protected bool m_bQuoteRevealed = false;

	// Word-by-word reveal state
	protected const float WORD_REVEAL_INTERVAL_S = 0.20;  // 200ms per word
	protected const int WORD_REVEAL_MAX_WORDS = 40;       // Fall back to instant if quote is too long
	protected ref array<string> m_aQuoteWords = new array<string>();
	protected ref array<string> m_aAuthorWords = new array<string>();
	protected int m_iQuoteWordIdx = 0;
	protected int m_iAuthorWordIdx = 0;
	protected string m_sRevealAccum = "";
	protected bool m_bWordRevealActive = false;

	// Heartbeat sound playback
	protected bool m_bHeartbeatsScheduled = false;
	protected static ref array<string> s_HeartbeatSounds = {
		"{93F9F242DC3E0E1C}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Fast_01.wav",
		"{0AE74BDBC1D93F8C}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Fast_02.wav",
		"{7DED2353357BD0FC}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Fast_03.wav",
		"{7A2AD90253FD6A3F}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Fast_04.wav",
		"{0D20B18AA75F854F}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Fast_05.wav",
		"{7795DA13D207A2B1}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Mid_01.wav",
		"{EE8B638ACFE09321}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Mid_02.wav",
		"{99810B023B427C51}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Mid_03.wav",
		"{9E46F1535DC4C692}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Mid_04.wav",
		"{E94C99DBA96629E2}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Mid_05.wav",
		"{9A36B88CB586AA8C}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Slow_01.wav",
		"{03280115A8619B1C}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Slow_02.wav",
		"{7422699D5CC3746C}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Slow_03.wav",
		"{73E593CC3A45CEAF}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Slow_04.wav",
		"{04EFFB44CEE721DF}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Slow_05.wav"
	};

	// Quotes live in PS_DeathQuotes (scripts/game/Deathquote/DeathQuotes.c)

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		// Re-entrancy guard: if a death screen is already open, close this duplicate
		// immediately so the player isn't stuck with two overlapping black overlays.
		if (s_bDeathScreenOpen)
		{
			Print("[DS][CLI] PS_DeathScreenMenu OnMenuOpen DUPLICATE — closing immediately", LogLevel.WARNING);
			Close();
			return;
		}
		s_bDeathScreenOpen = true;

		Print("[DS][CLI] PS_DeathScreenMenu OnMenuOpen FIRED rootWidget=" + (GetRootWidget() != null).ToString(), LogLevel.NORMAL);

		m_wBlackOverlay = ImageWidget.Cast(GetRootWidget().FindAnyWidget("BlackOverlay"));
		m_wQuoteText = TextWidget.Cast(GetRootWidget().FindAnyWidget("QuoteText"));
		m_wQuoteAuthor = TextWidget.Cast(GetRootWidget().FindAnyWidget("QuoteAuthor"));

		Print("[DS][CLI] PS_DeathScreenMenu widgets: overlay=" + (m_wBlackOverlay != null).ToString() + " quoteText=" + (m_wQuoteText != null).ToString() + " quoteAuthor=" + (m_wQuoteAuthor != null).ToString(), LogLevel.NORMAL);

		// DIAGNOSTIC: log the widget hierarchy so we can see if the text widgets
		// are siblings of BlackOverlay or nested under it
		Widget diagRoot = GetRootWidget();
		Print("[DS][CLI] DIAG rootWidget name=" + diagRoot.GetName() + " class=" + diagRoot.ClassName(), LogLevel.NORMAL);
		if (m_wQuoteText)
			Print("[DS][CLI] DIAG QuoteText parent=" + m_wQuoteText.GetParent().GetName() + " isVisible=" + m_wQuoteText.IsVisible() + " isEnabled=" + m_wQuoteText.IsEnabled() + " zOrder=" + m_wQuoteText.GetZOrder().ToString(), LogLevel.NORMAL);
		if (m_wQuoteAuthor)
			Print("[DS][CLI] DIAG QuoteAuthor parent=" + m_wQuoteAuthor.GetParent().GetName() + " isVisible=" + m_wQuoteAuthor.IsVisible() + " isEnabled=" + m_wQuoteAuthor.IsEnabled() + " zOrder=" + m_wQuoteAuthor.GetZOrder().ToString(), LogLevel.NORMAL);
		if (m_wBlackOverlay)
			Print("[DS][CLI] DIAG BlackOverlay parent=" + m_wBlackOverlay.GetParent().GetName() + " zOrder=" + m_wBlackOverlay.GetZOrder().ToString() + " opacity=" + m_wBlackOverlay.GetOpacity().ToString(), LogLevel.NORMAL);

		m_fElapsedTime = 0;
		m_bHeartbeatsScheduled = false;
		m_bSoundsStopped = false;
		m_bQuoteRevealed = false;

		// Hide the quote/author text widgets initially — they will be revealed only
		// AFTER the black overlay has fully faded in (per user spec: "wait until screen
		// fully faded, and only then show quote").
		if (m_wQuoteText)
		{
			m_wQuoteText.SetVisible(false);
			m_wQuoteText.SetZOrder(100);
		}
		if (m_wQuoteAuthor)
		{
			m_wQuoteAuthor.SetVisible(false);
			m_wQuoteAuthor.SetZOrder(100);
		}

		// Start fully transparent — fade-in handled in OnMenuUpdate
		if (m_wBlackOverlay)
		{
			m_wBlackOverlay.SetOpacity(0);
			m_wBlackOverlay.SetZOrder(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);

		m_fElapsedTime = m_fElapsedTime + tDelta;

		// One-shot trace on first update to confirm the menu is alive
		if (m_fElapsedTime < 0.05)
			Print("[DS][CLI] PS_DeathScreenMenu OnMenuUpdate FIRST FRAME tDelta=" + tDelta.ToString() + " elapsed=" + m_fElapsedTime.ToString(), LogLevel.NORMAL);

		// Reveal the quote/author widgets once the black overlay has fully faded in.
		// Per user spec: "wait until screen fully faded, and only then show quote".
		// We display the quote and author as a single block separated by a newline so
		// they always appear together (avoids any layout/clipping issues with two
		// independent widgets in the bottom/corner positions).
		if (!m_bQuoteRevealed && m_fElapsedTime >= FADE_DURATION_S)
		{
			m_bQuoteRevealed = true;
			Print("[DS][CLI] PS_DeathScreenMenu fade complete — revealing quote", LogLevel.NORMAL);
			ScheduleHeartbeats();
			if (m_wQuoteText)
			{
				string quoteText;
				string authorText;
				PS_DeathQuotes.SplitQuoteAuthor(PS_DeathQuotes.GetRandomQuote(), quoteText, authorText);
				// Split quote and author into words for the word-by-word reveal effect.
				// If the total word count exceeds WORD_REVEAL_MAX_WORDS, fall back to
				// instant display so we never run out of time before the menu closes.
				m_aQuoteWords.Clear();
				m_aAuthorWords.Clear();
				quoteText.Split(" ", m_aQuoteWords, false);
				if (authorText != "")
					authorText.Split(" ", m_aAuthorWords, false);
				int totalWords = m_aQuoteWords.Count() + m_aAuthorWords.Count();
				Print("[DS][CLI] PS_DeathScreenMenu quote: " + m_aQuoteWords.Count().ToString() + " words, author: " + m_aAuthorWords.Count().ToString() + " words, total: " + totalWords.ToString(), LogLevel.NORMAL);
				m_iQuoteWordIdx = 0;
				m_iAuthorWordIdx = 0;
				m_sRevealAccum = "";
				m_wQuoteText.SetText("");
				m_wQuoteText.SetVisible(true);
				// Hide the separate author widget — its text is now embedded in QuoteText
				if (m_wQuoteAuthor)
				{
					m_wQuoteAuthor.SetText("");
					m_wQuoteAuthor.SetVisible(false);
				}
				if (totalWords > WORD_REVEAL_MAX_WORDS)
				{
					// Long quote — fall back to instant display to avoid running out of time
					Print("[DS][CLI] PS_DeathScreenMenu quote too long (" + totalWords.ToString() + " words > " + WORD_REVEAL_MAX_WORDS.ToString() + "), instant display", LogLevel.NORMAL);
					while (m_iQuoteWordIdx < m_aQuoteWords.Count())
					{
						if (m_sRevealAccum != "")
							m_sRevealAccum = m_sRevealAccum + " ";
						m_sRevealAccum = m_sRevealAccum + m_aQuoteWords[m_iQuoteWordIdx];
						m_iQuoteWordIdx++;
					}
					if (m_aAuthorWords.Count() > 0)
					{
						m_sRevealAccum = m_sRevealAccum + "\n— ";
						while (m_iAuthorWordIdx < m_aAuthorWords.Count())
						{
							if (m_iAuthorWordIdx > 0)
								m_sRevealAccum = m_sRevealAccum + " ";
							m_sRevealAccum = m_sRevealAccum + m_aAuthorWords[m_iAuthorWordIdx];
							m_iAuthorWordIdx++;
						}
					}
					m_wQuoteText.SetText(m_sRevealAccum);
					m_bWordRevealActive = false;
				}
				else
				{
					// Start the word-by-word reveal
					m_bWordRevealActive = true;
					RevealNextWord();
					GetGame().GetCallqueue().CallLater(RevealNextWord, WORD_REVEAL_INTERVAL_S * 1000, true);
				}
				Print("[DS][CLI] PS_DeathScreenMenu quote reveal started", LogLevel.NORMAL);
			}
		}

		// Stop all sounds when total duration is reached (per spec step 3:
		// "3) Sounds stop, Overlay with quote also stop" happens AFTER the
		// 8s quote phase, so at totalDuration = FADE_DURATION_S + QUOTE_DURATION_S = 10s)
		float totalDuration = FADE_DURATION_S + QUOTE_DURATION_S;
		if (!m_bSoundsStopped && m_fElapsedTime >= totalDuration)
		{
			m_bSoundsStopped = true;
			Print("[DS][CLI] PS_DeathScreenMenu total duration reached — stopping sounds", LogLevel.NORMAL);
			StopAllHeartbeatSounds();
			// Clear remaining scheduled heartbeats so they don't fire after we've stopped
			GetGame().GetCallqueue().Remove(PlayHeartbeatSound);
		}

		// Fade in over FADE_DURATION_S
		if (m_fElapsedTime <= FADE_DURATION_S)
		{
			if (m_wBlackOverlay)
			{
				float fadeT = m_fElapsedTime / FADE_DURATION_S;
				m_wBlackOverlay.SetOpacity(fadeT);
			}
		}
		else
		{
			// Full black during quote display
			if (m_wBlackOverlay)
				m_wBlackOverlay.SetOpacity(1);
		}

		// One-shot DIAG at 1s and 3s to confirm the text widget state during the fade
		if (m_fElapsedTime > 0.9 && m_fElapsedTime < 1.1 && !m_bDiagAt1s)
		{
			m_bDiagAt1s = true;
			if (m_wQuoteText)
				Print("[DS][CLI] DIAG @1s QuoteText visible=" + m_wQuoteText.IsVisible().ToString() + " opacity=" + m_wQuoteText.GetOpacity().ToString() + " textLen=" + m_wQuoteText.GetText().Length().ToString() + " zOrder=" + m_wQuoteText.GetZOrder().ToString() + " parentVisible=" + m_wQuoteText.GetParent().IsVisible().ToString() + " parentZOrder=" + m_wQuoteText.GetParent().GetZOrder().ToString(), LogLevel.NORMAL);
		}
		if (m_fElapsedTime > 2.9 && m_fElapsedTime < 3.1 && !m_bDiagAt3s)
		{
			m_bDiagAt3s = true;
			if (m_wQuoteText)
				Print("[DS][CLI] DIAG @3s QuoteText visible=" + m_wQuoteText.IsVisible().ToString() + " opacity=" + m_wQuoteText.GetOpacity().ToString() + " textLen=" + m_wQuoteText.GetText().Length().ToString() + " zOrder=" + m_wQuoteText.GetZOrder().ToString() + " parentVisible=" + m_wQuoteText.GetParent().IsVisible().ToString() + " parentZOrder=" + m_wQuoteText.GetParent().GetZOrder().ToString(), LogLevel.NORMAL);
			if (m_wBlackOverlay)
				Print("[DS][CLI] DIAG @3s BlackOverlay opacity=" + m_wBlackOverlay.GetOpacity().ToString() + " zOrder=" + m_wBlackOverlay.GetZOrder().ToString(), LogLevel.NORMAL);
		}

		// Close when total duration (fade + quote) is reached
		if (m_fElapsedTime >= totalDuration)
		{
			Print("[DS][CLI] PS_DeathScreenMenu elapsed=" + m_fElapsedTime.ToString() + " >= totalDuration=" + totalDuration.ToString() + " — CLOSING", LogLevel.NORMAL);
			Close();
		}
	}

	//------------------------------------------------------------------------------------------------
	// Reveal the next word in the word-by-word animation. Called every WORD_REVEAL_INTERVAL_S
	// while m_bWordRevealActive. Reveals quote words first, then inserts "\n— " and reveals
	// author words. Stops itself when all words are shown.
	protected void RevealNextWord()
	{
		if (!m_bWordRevealActive)
			return;

		// Phase 1: reveal quote words
		if (m_iQuoteWordIdx < m_aQuoteWords.Count())
		{
			if (m_sRevealAccum != "")
				m_sRevealAccum = m_sRevealAccum + " ";
			m_sRevealAccum = m_sRevealAccum + m_aQuoteWords[m_iQuoteWordIdx];
			if (m_wQuoteText)
				m_wQuoteText.SetText(m_sRevealAccum);
			m_iQuoteWordIdx++;
			return;
		}

		// Phase 2: transition to author line (insert "\n— " prefix once)
		if (m_iAuthorWordIdx == 0 && m_aAuthorWords.Count() > 0)
		{
			m_sRevealAccum = m_sRevealAccum + "\n— ";
			if (m_wQuoteText)
				m_wQuoteText.SetText(m_sRevealAccum);
		}

		// Phase 3: reveal author words
		if (m_iAuthorWordIdx < m_aAuthorWords.Count())
		{
			if (m_iAuthorWordIdx > 0)
				m_sRevealAccum = m_sRevealAccum + " ";
			m_sRevealAccum = m_sRevealAccum + m_aAuthorWords[m_iAuthorWordIdx];
			if (m_wQuoteText)
				m_wQuoteText.SetText(m_sRevealAccum);
			m_iAuthorWordIdx++;
			return;
		}

		// All done — stop the recurring CallLater
		GetGame().GetCallqueue().Remove(RevealNextWord);
		m_bWordRevealActive = false;
		Print("[DS][CLI] PS_DeathScreenMenu word reveal COMPLETE", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// Schedule all 15 heartbeat sounds to play sequentially, evenly spaced across the full
	// death-screen duration (2s fade + 8s quote = 10s total). Scheduling is triggered at
	// FADE_DURATION_S (2s), so sound 0 fires immediately (next frame) and subsequent sounds
	// fire at `interval` (~0.667s) apart. The last sound (i=14) fires at ~9.333s.
	// Sounds are stopped at totalDuration (10s) via StopAllHeartbeatSounds().
	protected void ScheduleHeartbeats()
	{
		if (!s_HeartbeatSounds || s_HeartbeatSounds.Count() == 0)
			return;

		int count = s_HeartbeatSounds.Count();
		// Distribute sounds across the full (fade + quote) duration so the last sound
		// fires at or just before totalDuration. This ensures all sounds play through
		// the end of the death screen without cutting off.
		float soundWindow = FADE_DURATION_S + QUOTE_DURATION_S;
		float interval = soundWindow / count;

		for (int i = 0; i < count; i++)
		{
			float delayMs = (i * interval) * 1000;
			GetGame().GetCallqueue().CallLater(PlayHeartbeatSound, delayMs, false, i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void PlayHeartbeatSound(int index)
	{
		if (!s_HeartbeatSounds || index < 0 || index >= s_HeartbeatSounds.Count())
			return;

		AudioSystem.PlaySound(s_HeartbeatSounds[index]);
	}

	//------------------------------------------------------------------------------------------------
	// Stop all heartbeat sounds (called at totalDuration, per spec step 3).
	// The heartbeat sounds are short one-shots from the ACP file. They will finish
	// naturally. Any remaining scheduled heartbeats are removed via
	// GetGame().GetCallqueue().Remove(PlayHeartbeatSound) in OnMenuUpdate
	// before this call to prevent further sounds from playing.
	protected void StopAllHeartbeatSounds()
	{
		Print("[DS][CLI] PS_DeathScreenMenu StopAllHeartbeatSounds — sounds will finish naturally, pending CallLater entries removed", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		// Stop the recurring word-reveal CallLater so it doesn't fire on a closed menu
		GetGame().GetCallqueue().Remove(RevealNextWord);
		m_bWordRevealActive = false;

		// Release the static guard so future deaths can open a new death screen
		s_bDeathScreenOpen = false;

		super.OnMenuClose();
		PS_DebugLogger.Log("PS_DeathScreenMenu OnMenuClose elapsed=" + m_fElapsedTime.ToString());

		// Chain to the normal spectator transition. The observer position was stashed in
		// m_vObserverPosition by RPC_SwitchToDeathScreenServer before this menu opened, so
		// SwitchToObserver(null) will consume it and spawn the spectator camera at the
		// dead body's location.
		PlayerController pc = GetGame().GetPlayerController();
		if (pc)
		{
			PS_PlayableControllerComponent pcc = PS_PlayableControllerComponent.Cast(pc.FindComponent(PS_PlayableControllerComponent));
			if (pcc)
				pcc.SwitchToObserver(null);
		}
	}
}
