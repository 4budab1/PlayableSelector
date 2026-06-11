// Widget displays info about voice rooms in voice chat.
// Path: {35DB604900C55B98}UI/VoiceChat/VoiceChatFrame.layout

class PS_VoiceChatList : SCR_ScriptedWidgetComponent
{
	// consts
	/*
	protected const ResourceName m_sPlayerVoiceSelectorPrefab = "{086F282C8CE692F1}UI/VoiceChat/VoicePlayerSelector.layout";
	protected const ResourceName m_sVoiceRoomHeaderPrefab = "{C976E42779159507}UI/VoiceChat/VoiceRoomHeader.layout";
	*/
	
	[Attribute("{E705A59E59B577F2}UI/VoiceChat/VoiceChatRoom.layout")]
	protected ResourceName m_sVoiceChatRoomPrefab;
	
	// Global cached
	protected PlayerManager m_gPlayerManager;
	protected PS_PlayableManager m_gPlayableManager;
	protected PS_VoNChannelsManager m_gVoNChannelsManager;
	protected PlayerController m_pPlayerController;
	protected int m_iPlayerId;
	protected int m_iPublicRoomId;
	
	// Local
	// List of voice rooms
	protected VerticalLayoutWidget m_wRoomsList;
	protected ref map<int, PS_VoiceChatRoom> m_wRooms = new map<int, PS_VoiceChatRoom>;
	protected FactionKey m_sCurrentFactionKey;
	
	// -------------------- Handler events --------------------
	override void HandlerAttached(Widget w)
	{
	if (!GetGame() || !GetGame().InPlayMode())
		return;

	// Guard against destroyed widget when retrying after a delayed CallLater
	if (!w || !w.GetParent())
		return;

	m_gPlayerManager = GetGame().GetPlayerManager();
	m_gPlayableManager = PS_PlayableManager.GetInstance();
	m_gVoNChannelsManager = PS_VoNChannelsManager.GetInstance();
	m_pPlayerController = GetGame().GetPlayerController();

	if (!m_gVoNChannelsManager || !m_pPlayerController || !m_gPlayableManager)
	{
		GetGame().GetCallqueue().CallLater(HandlerAttached, 100, false, w);
		return;
	}

	super.HandlerAttached(w);

		m_wRoomsList = VerticalLayoutWidget.Cast(w.FindAnyWidget("RoomsList"));

		m_gVoNChannelsManager.m_eOnRoomChanged.Insert(MovePlayer);

		m_iPlayerId = m_pPlayerController.GetPlayerId();
		m_sCurrentFactionKey = m_gPlayableManager.GetPlayerFactionKey(m_iPlayerId);
		// Seed the cached spectator flag so the CreateRoomIfNeed guard has the
		// correct value on the very first call (before the first SyncRoomPlayers
		// tick). Uses ComputeIsSpectator() — same logic as every other refresh site.
		m_bIsSpectator = ComputeIsSpectator();
		// BUGFIX: use GetOrCreateRoomWithFaction (not GetRoomWithFaction) so JIP
		// clients whose server-side RPC_InitChannel for their own Public room hasn't
		// arrived yet get a placeholder roomId (resolved when the real RPC lands)
		// instead of -1. Without this, RemoveRoomIfNeed's `m_iPublicRoomId != roomId`
		// check would always be true (m_iPublicRoomId=-1 vs valid positive roomId),
		// making the player's own empty Public room eligible for removal before
		// it's ever visible. Placeholder rooms are filtered by CreateRoom/CreateRoomIfNeed.
		m_iPublicRoomId = m_gVoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Public" + m_iPlayerId.ToString());
		// Seed the state tracker with the current state so the FIRST SyncRoomPlayers
		// tick doesn't fire a spurious rebuild. Subsequent state transitions will
		// trigger a rebuild in SyncRoomPlayers.
		PS_GameModeCoop gameModeSeed = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (gameModeSeed)
			m_eLastKnownGameState = gameModeSeed.GetState();

		Rebuild();

		GetGame().GetCallqueue().CallLater(UpdateInfo, 100, true);
		// Periodic full room-player sync. MovePlayer only fires for current clients;
		// JIP clients need an extra sweep to populate rooms with players whose
		// channel assignments were already replicated via m_PlayerChannelKeyMap
		// before this menu opened.
		// NOTE: Reduced from 500ms to 200ms for snappier JIP recovery and race-condition repair.
		GetGame().GetCallqueue().CallLater(SyncRoomPlayers, 200, true);
	}
	
	void ~PS_VoiceChatList()
	{
		if (!GetGame())
			return;
		if (!GetGame().InPlayMode())
			return;

		// Guard against null callqueue during game shutdown
		ScriptCallQueue callqueue = GetGame().GetCallqueue();
		if (callqueue)
		{
			callqueue.Remove(UpdateInfo);
			callqueue.Remove(SyncRoomPlayers);
		}

		if (!m_gVoNChannelsManager)
			return;
		if (!m_gVoNChannelsManager.m_eOnRoomChanged)
			return;

		m_gVoNChannelsManager.m_eOnRoomChanged.Remove(MovePlayer);
	}
	
	private int m_iOldPlayersCount = 0;
	private int m_iOldRoomsCount = 0;
	// Last game state observed by SyncRoomPlayers; used to detect state transitions
	// (BRIEFING -> GAME -> spectator) so the visible-room set can be rebuilt.
	// Sentinel -1 = uninitialized; first sync reads the real state and seeds it.
	private SCR_EGameModeState m_eLastKnownGameState = SCR_EGameModeState.PREGAME;
	// Cached spectator flag, refreshed in SyncRoomPlayers and MovePlayer. Used by
	// CreateRoomIfNeed's guard so the lookup is O(1) and — critically — so the
	// guard doesn't run on every MovePlayer tick (the old version did 4 lookups
	// per call). Also prevents the JIP false-positive bug where GetPlayableByPlayer
	// returns Invalid before the playable snapshot replicates, which would
	// mis-identify a real player as a spectator and strip their faction rooms.
	// Computed once per state-change tick using the same logic as GetVisibleRooms.
	private bool m_bIsSpectator = false;

	// Single source of truth for the spectator check. All four refresh sites
	// (HandlerAttached seed, MovePlayer, SyncRoomPlayers, Rebuild) call this so
	// the call sites can never diverge. Returns false if the game mode can't
	// be cast (defensive — shouldn't happen in normal play).
	protected bool ComputeIsSpectator()
	{
		PS_GameModeCoop gm = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (!gm)
			return false;
		if (gm.GetState() != SCR_EGameModeState.GAME)
			return false;
		RplId slotId = m_gPlayableManager.GetPlayableByPlayer(m_iPlayerId);
		if (slotId == RplId.Invalid())
			return true;
		// If the slot's character is destroyed, the player is dead/observer
		// and should be treated as a spectator even though the slot hasn't been
		// invalidated yet (CoD-style death-screen transition).
		if (m_gPlayableManager.IsSlotCharacterDestroyed(slotId))
			return true;
		return false;
	}
		
	// -------------------- Update content functions --------------------
	void Clear()
	{
		SCR_WidgetHelper.RemoveAllChildren(m_wRoomsList);
		m_wRooms.Clear();
	}
	
	void Rebuild()
	{
		Clear();

		// BUGFIX: refresh the cached spectator flag at the top of Rebuild().
		// Rebuild is called from HandlerAttached, MovePlayer (on state change),
		// and SyncRoomPlayers (on state change). The first CreateRoom call inside
		// this Rebuild relies on CreateRoomIfNeed's spectator guard, which reads
		// m_bIsSpectator. Without this refresh, a Rebuild triggered by a
		// BRIEFING->GAME transition would use the stale BRIEFING value and the
		// guard would let BRIEFING rooms through. (m_sCurrentFactionKey is
		// overwritten by GetVisibleRooms below, so we only need to refresh the
		// spectator flag here.)
		m_bIsSpectator = ComputeIsSpectator();

		PS_DebugLogger.Log("[VoN-CLI] Rebuild BEGIN playerId=" + m_iPlayerId.ToString() + " faction=" + m_sCurrentFactionKey + " isSpectator=" + m_bIsSpectator + " channelMapSize=" + m_gVoNChannelsManager.GetPlayerChannelKeyMapSize() + " roomMapSize=" + m_gVoNChannelsManager.GetRoomMapSize());

		// Create initial list of visible rooms
		array<int> visibleRooms = new array<int>();
		GetVisibleRooms(visibleRooms);
		string roomList = "";
		foreach (int rid : visibleRooms)
			roomList += rid.ToString() + "(" + m_gVoNChannelsManager.GetRoomName(rid) + ") ";
		PS_DebugLogger.Log("[VoN-CLI] Rebuild visibleRooms count=" + visibleRooms.Count().ToString() + " rooms=[" + roomList + "]");
		foreach (int roomId : visibleRooms)
		{
			string name = m_gVoNChannelsManager.GetRoomName(roomId);
			CreateRoomIfNeed(roomId);
			if (m_wRooms.Contains(roomId))
				PS_DebugLogger.Log("[VoN-CLI] Rebuild created room id=" + roomId.ToString() + " name=" + name);
		}

		PS_DebugLogger.Log("[VoN-CLI] Rebuild DONE m_wRooms count=" + m_wRooms.Count().ToString());
		UpdateInfo();
	}
	
	void CreateRoomIfNeed(int roomId)
	{
		if (roomId < 0) return;
		string roomKey = m_gVoNChannelsManager.GetRoomName(roomId);
		// FIX: If the roomId is valid but the name is empty (JIP/race condition where
		// RPC_InitChannel hasn't arrived yet), still create the room with a placeholder.
		// The name will update automatically when the channel key resolves. Without this,
		// players in rooms that haven't been named yet simply disappear from the UI.
		if (roomKey == "")
		{
			PS_DebugLogger.Log("[VoN-CLI] CreateRoomIfNeed roomId=" + roomId.ToString() + " has empty name — creating placeholder (JIP/race)");
			roomKey = "UnknownRoom";
		}
		if (m_gVoNChannelsManager.IsNoSoundChannel(roomKey)) return;
		// BUGFIX: use exact match instead of EndsWith, which incorrectly accepted
		// other players' local rooms when their IDs shared a suffix (e.g. player 1
		// matched room "#PS-VoNRoom_Local21" because it ends with "1").
		// NOTE: the room key includes the "|" prefix from GetOrCreateRoomWithFaction
		// (faction "" + "|" + channel), so the exact match must include it.
		if (roomKey.Contains("#PS-VoNRoom_Local") && roomKey != "|#PS-VoNRoom_Local" + m_iPlayerId.ToString()) return;

		// BUGFIX: Spectator must never see Faction/Command/Group rooms.
		// Reject ALL faction-prefixed rooms (not just the player's old faction).
		// Previously only `m_sCurrentFactionKey + "|"` was rejected, so a spectator
		// who was US in briefing would still see USSR briefing rooms.
		if (m_bIsSpectator)
		{
			if (roomKey.Contains("#PS-VoNRoom_Faction")) return;
			if (roomKey.Contains("#PS-VoNRoom_Command")) return;
			if (roomKey.Contains("|") && !roomKey.StartsWith("|"))
				return;
		}

		// BUGFIX: In lobby (PREGAME / SLOTSELECTION) show ALL groups regardless of faction.
		// Skip the faction filter so opposite-faction group rooms are visible.
		// Also skip the faction filter for placeholder rooms (JIP race condition where
		// RPC_InitChannel hasn't arrived yet) so they get created and can be renamed
		// when the real channel key replicates in.
		bool isPlaceholder = (roomKey == "UnknownRoom");
		if (!isPlaceholder && m_eLastKnownGameState != SCR_EGameModeState.PREGAME && m_eLastKnownGameState != SCR_EGameModeState.SLOTSELECTION)
		{
			if (m_sCurrentFactionKey != "")
			{
				if (!roomKey.StartsWith("|") && !roomKey.StartsWith(m_sCurrentFactionKey + "|"))
					return;
			}
			else
			{
				if (!roomKey.StartsWith("|"))
					return;
			}
		}
		CreateRoom(roomId, roomKey);
	}
	
	void CreateRoom(int roomId, string forcedRoomKey = "")
	{
		string roomKey = forcedRoomKey;
		if (roomKey == "")
			roomKey = m_gVoNChannelsManager.GetRoomName(roomId);
		if (roomKey == "")
		{
			PS_DebugLogger.Log("[VoN-CLI] CreateRoom roomId=" + roomId.ToString() + " has empty name — creating placeholder (JIP/race)");
			roomKey = "UnknownRoom";
		}
		Widget roomWidget = GetGame().GetWorkspace().CreateWidgets(m_sVoiceChatRoomPrefab);
		PS_VoiceChatRoom voiceChatRoom = PS_VoiceChatRoom.Cast(roomWidget.FindHandler(PS_VoiceChatRoom));
		voiceChatRoom.SetRoomId(roomId, roomKey);
		
		array<int> playersInRoom = new array<int>();
		m_gVoNChannelsManager.GetPlayersInRoom(playersInRoom, roomId);
		foreach (int playerId : playersInRoom)
		{
			voiceChatRoom.AddPlayer(playerId);
		}
		
		m_wRoomsList.AddChild(roomWidget);
		m_wRooms[roomId] = voiceChatRoom;
	}
	
	void RemoveRoomIfNeed(int roomId)
	{
		if (m_gVoNChannelsManager.IsGlobalRoom(roomId)) return;
		if (m_gVoNChannelsManager.IsLocalRoom(roomId)) return;
		
		// current player
		if (m_gVoNChannelsManager.IsPublicRoom(roomId))
		{
			if (m_iPublicRoomId != roomId)
			{
				array<int> playersInRoom = new array<int>();
				m_gVoNChannelsManager.GetPlayersInRoom(playersInRoom, roomId);
				if (playersInRoom.IsEmpty())
					RemoveRoom(roomId);
			}
		} else {
			string roomKey = m_gVoNChannelsManager.GetRoomName(roomId);
			
			// BUGFIX: Don't remove rooms whose name is empty (JIP placeholder waiting
			// for RPC_InitChannel). Removing them would defeat the placeholder purpose
			// and cause the room to disappear before the real name arrives.
			if (roomKey == "")
				return;
			
			// BUGFIX: Spectator — strict whitelist: only Global, Local/Deafen, and Public
			// rooms are allowed. Everything else (Faction, Command, Group, and any
			// other non-special room) is removed immediately.
			if (m_bIsSpectator)
			{
				if (m_gVoNChannelsManager.IsGlobalRoom(roomId) ||
				    m_gVoNChannelsManager.IsLocalRoom(roomId) ||
				    m_gVoNChannelsManager.IsPublicRoom(roomId))
				{
					return; // allowed for spectator
				}
				PS_DebugLogger.Log("[VoN-CLI] RemoveRoomIfNeed SPECTATOR removing roomId=" + roomId.ToString() + " name=" + roomKey);
				RemoveRoom(roomId);
				return;
			}
			
			// BUGFIX: Lobby — keep ALL group rooms visible even when empty.
			// Do NOT remove empty rooms in the lobby; SyncRoomPlayers Phase 1.5
			// already handles removing rooms that are no longer in the visible set.
			if (m_eLastKnownGameState == SCR_EGameModeState.PREGAME || m_eLastKnownGameState == SCR_EGameModeState.SLOTSELECTION)
			{
				return;
			}
			
			if (!m_gVoNChannelsManager.IsFactionRoom(roomId, m_sCurrentFactionKey) && m_gVoNChannelsManager.GetPlayerRoom(m_iPlayerId) != roomId)
			{
				RemoveRoom(roomId);
			}
		}
	}
	
	void RemoveRoom(int roomId)
	{
		if (!m_wRooms.Contains(roomId)) return;
		PS_VoiceChatRoom voiceChatRoom = m_wRooms[roomId];
		voiceChatRoom.GetRootWidget().RemoveFromHierarchy();
		m_wRooms.Remove(roomId);
	}
	
	void SwitchFaction(FactionKey factionKey)
	{
		m_sCurrentFactionKey = factionKey;
		Rebuild();
	}
	
	FactionKey GetFactionKey()
	{
		return m_sCurrentFactionKey;
	}
	
	void MovePlayer(int playerId, int roomId, int oldRoomId)
	{
		PS_DebugLogger.Log("[VoN-CLI] MovePlayer player=" + playerId.ToString() + " roomId=" + roomId.ToString() + " oldRoomId=" + oldRoomId.ToString() + " m_wRooms hasNewRoom=" + m_wRooms.Contains(roomId).ToString() + " hasOldRoom=" + m_wRooms.Contains(oldRoomId).ToString());

		// State-change detection: if the game state has transitioned (e.g. BRIEFING -> GAME
		// when the player dies and goes to spectator), the visible-room set changes
		// dramatically. We bump m_eLastKnownGameState and rebuild BEFORE doing any
		// room creation, so the "ensure all visible rooms exist" sweep below uses
		// the NEW state's visible set (spectator rooms, not BRIEFING rooms). We do
		// NOT return early — we still want the triggering MovePlayer event to add
		// the new roomId and the player to it; the Rebuild inside MovePlayer is
		// what makes the room list match the new state, but the per-room player
		// update below is still needed for this specific player's transition.
		// Idempotency: if SyncRoomPlayers detects the same state change in the same
		// tick, it'll see m_eLastKnownGameState already up-to-date and skip its
		// rebuild — no double-Rebuild.
		PS_GameModeCoop gameModeCheck = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		bool stateChanged = false;
		if (gameModeCheck)
		{
			SCR_EGameModeState currentState = gameModeCheck.GetState();
			if (m_eLastKnownGameState != currentState)
			{
				PS_DebugLogger.Log("[VoN-CLI] MovePlayer state change " + typename.EnumToString(SCR_EGameModeState, m_eLastKnownGameState) + " -> " + typename.EnumToString(SCR_EGameModeState, currentState) + " — Rebuild + continue with new-state rooms");
				m_eLastKnownGameState = currentState;
				Rebuild();
				stateChanged = true;
			}
		}

		// Refresh faction key so visibility checks use the CURRENT state's faction,
		// not a stale one set in HandlerAttached or in a prior state. Without this,
		// a player who transitions BRIEFING -> spectator mid-stream would still have
		// the BRIEFING faction in m_sCurrentFactionKey, causing the old faction rooms
		// to pass the visibility filter in CreateRoomIfNeed.
		// Note: minor race with user-initiated SwitchFaction() — if the user clicks
		// Switch Faction and MovePlayer fires before PS_PlayableManager is updated
		// server-side, this overwrites the optimistic UI choice with the stale
		// server value. SyncRoomPlayers heals it within 200ms. Acceptable.
		m_sCurrentFactionKey = m_gPlayableManager.GetPlayerFactionKey(m_iPlayerId);

		// Refresh the cached spectator flag. Uses ComputeIsSpectator() so all four
		// refresh sites (HandlerAttached, MovePlayer, SyncRoomPlayers, Rebuild) can
		// never diverge. Without this refresh, a player who transitions BRIEFING ->
		// spectator mid-stream would still have m_bIsSpectator=false from
		// HandlerAttached and the guard would allow BRIEFING rooms to be re-created
		// on this MovePlayer event.
		m_bIsSpectator = ComputeIsSpectator();

		// remove from old room
		if (m_wRooms.Contains(oldRoomId))
		{
			m_wRooms[oldRoomId].RemovePlayer(playerId);
			RemoveRoomIfNeed(oldRoomId);
		}

		bool roomExisted = m_wRooms.Contains(roomId);
		if (!roomExisted)
			CreateRoomIfNeed(roomId);

		// FIX: When a room is newly created (either by MovePlayer or InitChannel event),
		// it must be populated with ALL current players in that room. Otherwise the room
		// widget stays empty until the next SyncRoomPlayers sweep (which is 500ms away
		// and may miss fast-moving state). We also add the specific playerId that
		// triggered this MovePlayer (playerId may be -1 for InitChannel events).
		if (m_wRooms.Contains(roomId))
		{
			array<int> expectedPlayers = {};
			m_gVoNChannelsManager.GetPlayersInRoom(expectedPlayers, roomId);
			foreach (int pid : expectedPlayers)
			{
				if (!m_wRooms[roomId].HasPlayer(pid))
				{
					PS_DebugLogger.Log("[VoN-CLI] MovePlayer ADD existing player=" + pid.ToString() + " to room=" + roomId.ToString());
					m_wRooms[roomId].AddPlayer(pid);
				}
			}
			// Ensure the triggering player is also added (if not already covered)
			if (playerId >= 0 && !m_wRooms[roomId].HasPlayer(playerId))
			{
				PS_DebugLogger.Log("[VoN-CLI] MovePlayer ADD trigger player=" + playerId.ToString() + " to room=" + roomId.ToString());
				m_wRooms[roomId].AddPlayer(playerId);
			}
		}

		// ensure all visible rooms exist (catch rooms created server-side but no player moved to yet)
		// SKIP this sweep if a state change was just detected and Rebuild already
		// ran — Rebuild populated all current visible rooms, so this would be
		// redundant work. The currentPlayerRoom guard in RemoveRoomIfNeed also
		// handles transient state where a new roomId is mid-replication.
		if (!stateChanged)
		{
			array<int> visibleRooms = {};
			GetVisibleRooms(visibleRooms);
			foreach (int vr : visibleRooms)
			{
				if (vr >= 0 && !m_wRooms.Contains(vr))
				{
					CreateRoomIfNeed(vr);
					if (m_wRooms.Contains(vr))
						PS_DebugLogger.Log("[VoN-CLI] MovePlayer created visible room id=" + vr.ToString());
				}
			}
		}
		else
		{
			PS_DebugLogger.Log("[VoN-CLI] MovePlayer skipped visible-rooms sweep (state-change Rebuild already populated new visible set)");
		}

		UpdateInfo();
	}
	
	void UpdateInfo()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return;
		foreach (int roomId, PS_VoiceChatRoom voiceChatRoom : m_wRooms)
		{
			voiceChatRoom.UpdateInfo();
		}
	}

	// Reconcile each known room's player list with the current m_PlayerChannelKeyMap.
	// Also create any visible rooms that were skipped during Rebuild (e.g. because
	// the roomId came back as -1 when the JIP snapshot hadn't replicated yet).
	// Runs every 200ms; cheap O(rooms * playersInRoom) per tick.
	void SyncRoomPlayers()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return;
		if (!m_gVoNChannelsManager || !m_gPlayerManager)
			return;

		// BUGFIX: refresh the cached spectator flag and faction key BEFORE the
		// state-change early-return. Previously this refresh lived in Phase 1
		// (after the early-return), so on the tick that detected a BRIEFING->GAME
		// transition the flag would still be the old BRIEFING value when Rebuild()
		// ran. That 200ms window allowed CreateRoomIfNeed (called from the Rebuild
		// path) to re-create BRIEFING rooms even with the guard in place.
		// Also: refresh m_sCurrentFactionKey for the same JIP-late-faction reason
		// (player faction assigned after the menu opens is a common JIP scenario).
		PS_GameModeCoop gameModeCheck = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		m_sCurrentFactionKey = m_gPlayableManager.GetPlayerFactionKey(m_iPlayerId);
		m_bIsSpectator = ComputeIsSpectator();
		// Update the cached public room ID so RemoveRoomIfNeed can protect the
		// player's own public room once the real roomId arrives (JIP case where
		// HandlerAttached seeded it with -1). Without this, the own public room
		// is treated as "other player's public room" and becomes eligible for
		// removal when the player leaves it.
		int updatedPublicRoomId = m_gVoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Public" + m_iPlayerId.ToString());
		if (updatedPublicRoomId >= 0)
			m_iPublicRoomId = updatedPublicRoomId;

		// State-change detection: when the game mode transitions (e.g. BRIEFING -> GAME
		// when the user dies and goes to spectator), the visible-rooms set changes
		// dramatically. BRIEFING exposes Faction/Command/Group channels; spectator should
		// only show Deafen/Global/own Public + other players' public rooms. Without a full
		// rebuild the old BRIEFING-only rooms linger in the UI ("briefing leftovers in
		// spectator"). Detect the state delta and Rebuild so the room list snaps to the
		// new state's room set.
		if (gameModeCheck)
		{
			SCR_EGameModeState currentState = gameModeCheck.GetState();
			if (m_eLastKnownGameState != currentState)
			{
				PS_DebugLogger.Log("[VoN-CLI] SyncRoomPlayers state change " + typename.EnumToString(SCR_EGameModeState, m_eLastKnownGameState) + " -> " + typename.EnumToString(SCR_EGameModeState, currentState) + " — triggering Rebuild");
				m_eLastKnownGameState = currentState;
				Rebuild();
				return;
			}
		}

		// Phase 1: create any missing visible rooms. This is the key fix for JIP
		// clients: Rebuild() runs once in HandlerAttached; if a room's roomId was
		// still -1 at that point (channel not yet replicated), the room was never
		// created. By now the RplProp snapshot should have caught up.
		// (m_sCurrentFactionKey was already refreshed at the top of this function
		// for the JIP-late-faction case; no need to repeat it here.)
		array<int> visibleRooms = {};
		GetVisibleRooms(visibleRooms);
		int createdCount = 0;
		foreach (int vr : visibleRooms)
		{
			if (vr >= 0 && !m_wRooms.Contains(vr))
			{
				PS_DebugLogger.Log("[VoN-CLI] SyncRoomPlayers creating MISSING room id=" + vr.ToString() + " name=" + m_gVoNChannelsManager.GetRoomName(vr));
				CreateRoomIfNeed(vr);
				if (m_wRooms.Contains(vr))
					createdCount++;
			}
		}
		if (createdCount > 0)
			PS_DebugLogger.Log("[VoN-CLI] SyncRoomPlayers created " + createdCount.ToString() + " missing rooms");

		// Phase 1.5: surgical removal of rooms that are no longer in the visible
		// set. Without this, BRIEFING-only rooms (Faction/Command/Group) stick
		// around forever in the UI even after the player transitions to spectator
		// or another non-BRIEFING state. We never remove the global room, the
		// local/deafen room, or the player's own current room (the latter in
		// case of mid-tick transient state).
		int currentPlayerRoom = m_gVoNChannelsManager.GetPlayerRoom(m_iPlayerId);
		array<int> roomsToRemove = {};
		foreach (int roomId, PS_VoiceChatRoom voiceChatRoom : m_wRooms)
		{
			if (roomId < 0)
			{
				// Placeholder room (RPC_InitChannel not yet arrived). Remove it only if
				// it's no longer in the visible set (e.g. the group room was skipped by
				// GetRoomWithFaction). Keep it if visibleRooms still contains -1 (e.g. for
				// the own public/local room that uses GetOrCreateRoomWithFaction).
				if (!visibleRooms.Contains(roomId))
					roomsToRemove.Insert(roomId);
				continue;
			}
			if (visibleRooms.Contains(roomId))
				continue;
			if (m_gVoNChannelsManager.IsGlobalRoom(roomId)) continue;
			if (m_gVoNChannelsManager.IsLocalRoom(roomId)) continue;
			if (roomId == currentPlayerRoom && !m_bIsSpectator) continue;
			roomsToRemove.Insert(roomId);
		}
		int removedCount = 0;
		foreach (int roomId : roomsToRemove)
		{
			PS_DebugLogger.Log("[VoN-CLI] SyncRoomPlayers REMOVE no-longer-visible room=" + roomId.ToString() + " name=" + m_gVoNChannelsManager.GetRoomName(roomId));
			RemoveRoom(roomId);
			removedCount++;
		}
		if (removedCount > 0)
			PS_DebugLogger.Log("[VoN-CLI] SyncRoomPlayers removed " + removedCount.ToString() + " no-longer-visible rooms");

		// Phase 2: reconcile each existing room's player list.
		foreach (int roomId, PS_VoiceChatRoom voiceChatRoom : m_wRooms)
		{
			if (roomId < 0)
				continue;
			array<int> expectedPlayers = {};
			m_gVoNChannelsManager.GetPlayersInRoom(expectedPlayers, roomId);
			// Add missing
			foreach (int pid : expectedPlayers)
			{
				// NOTE: Do NOT filter by GetPlayerController here. On clients, remote players'
				// controllers are not replicated, so this filter would hide ALL remote players.
				// OnPlayerDisconnected already cleans up m_PlayerChannelKeyMap server-side,
				// and that removal is replicated via RplProp. JIP clients receive the current
				// map snapshot, so stale entries are not a problem.
				if (!voiceChatRoom.HasPlayer(pid))
				{
					PS_DebugLogger.Log("[VoN-CLI] SyncRoomPlayers ADD player=" + pid.ToString() + " to room=" + roomId.ToString());
					voiceChatRoom.AddPlayer(pid);
				}
			}			// Remove stale
			array<int> toRemove = {};
			foreach (int existingPid, PS_PlayerVoiceSelector sel : voiceChatRoom.m_mPlayers)
			{
				bool stillExpected = false;
				foreach (int pid : expectedPlayers)
				{
					if (pid == existingPid) { stillExpected = true; break; }
				}
				if (!stillExpected)
				{
					PS_DebugLogger.Log("[VoN-CLI] SyncRoomPlayers REMOVE stale player=" + existingPid.ToString() + " from room=" + roomId.ToString());
					toRemove.Insert(existingPid);
				}
			}
			foreach (int pid : toRemove)
				voiceChatRoom.RemovePlayer(pid);
		}
	}
	
	// ----- Actions -----
	protected void Action_PlayerClick(SCR_ButtonBaseComponent playerSelector)
	{	
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.CLICK);
	}
	
	// -------------------- Extra lobby functions --------------------
	void GetVisibleRooms(out array<int> outRoomsArray)
	{
		PS_GameModeCoop gameMode = PS_GameModeCoop.Cast(GetGame().GetGameMode());
		if (!gameMode)
			return;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		if (!playableManager)
			return;
		PS_VoNChannelsManager VoNChannelsManager = PS_VoNChannelsManager.GetInstance();
		if (!VoNChannelsManager)
			return;
		SCR_EGameModeState gameState = gameMode.GetState();

		PlayerController currentPlayerController = GetGame().GetPlayerController();
		if (!currentPlayerController)
			return;
		int currentPlayerId = currentPlayerController.GetPlayerId();

		RplId playerSlot = playableManager.GetPlayableByPlayer(currentPlayerId);
		// BUGFIX: Use ComputeIsSpectator() instead of the simple playerSlot check.
		// The simple check misses death-screen transitions where the slot is still
		// valid but the character is destroyed (IsSlotCharacterDestroyed). In that
		// state GetVisibleRooms would return the non-spectator GAME layout, causing
		// briefing rooms (Smoking room, HQ, Group) to persist in the spectator UI.
		bool isSpectator = ComputeIsSpectator();
		bool isLobby = (gameState == SCR_EGameModeState.PREGAME || gameState == SCR_EGameModeState.SLOTSELECTION);
		// A "lobby spectator" is a player in PREGAME/SLOTSELECTION who hasn't
		// picked a slot yet. They should see the simplified layout (Deafen,
		// Global, own Public) instead of the full squad list.
		bool isLobbySpectator = (isLobby && playerSlot == RplId.Invalid());

		FactionKey actualPlayerFaction = playableManager.GetPlayerFactionKey(currentPlayerId);
		FactionKey currentPlayerFactionKey;
		if (isLobby && !isLobbySpectator)
		{
			if (actualPlayerFaction != "")
				currentPlayerFactionKey = actualPlayerFaction;
			else
				currentPlayerFactionKey = m_sCurrentFactionKey;
		}
		else if (isSpectator || isLobbySpectator)
		{
			currentPlayerFactionKey = actualPlayerFaction;
		}
		else
		{
			currentPlayerFactionKey = actualPlayerFaction;
		}
		if (currentPlayerFactionKey == "")
			currentPlayerFactionKey = m_sCurrentFactionKey;
		m_sCurrentFactionKey = currentPlayerFactionKey;

		if (isLobby && !isLobbySpectator)
		{
			// Lobby layout (PREGAME + SLOTSELECTION):
			//   1) Deafen (Local) — always show
			//   2) Global — everyone is here
			//   3) Groups sorted: faction A→Z, then callsign A→Z within each faction
			// All groups from ALL factions are shown regardless of whether the
			// player has selected a faction yet.

			int localRoom = VoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Local" + currentPlayerId.ToString());
			if (localRoom >= 0) outRoomsArray.Insert(localRoom);

			int globalRoom = VoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Global");
			if (globalRoom >= 0) outRoomsArray.Insert(globalRoom);

			array<int> allGroupRooms = {};
			VoNChannelsManager.GetAllGroupRooms(allGroupRooms);

			// Sort by faction key (alphabetical), then by callsign within faction.
			// Uses bubble sort with length-then-lexicographic comparison on the
			// callsign portion for correct numeric ordering (e.g. US|9 before US|10).
			for (int i = 0; i < allGroupRooms.Count(); i++)
			{
				for (int j = i + 1; j < allGroupRooms.Count(); j++)
				{
					string nameI = VoNChannelsManager.GetRoomName(allGroupRooms[i]);
					string nameJ = VoNChannelsManager.GetRoomName(allGroupRooms[j]);
					int sepI = nameI.IndexOf("|");
					int sepJ = nameJ.IndexOf("|");
					string factionI, factionJ, callsignI, callsignJ;
					if (sepI >= 0) { factionI = nameI.Substring(0, sepI); callsignI = nameI.Substring(sepI + 1, nameI.Length() - sepI - 1); }
					else { factionI = nameI; callsignI = ""; }
					if (sepJ >= 0) { factionJ = nameJ.Substring(0, sepJ); callsignJ = nameJ.Substring(sepJ + 1, nameJ.Length() - sepJ - 1); }
					else { factionJ = nameJ; callsignJ = ""; }

					// Compare faction first (alphabetical)
					bool shouldSwap = false;
					if (factionI > factionJ)
						shouldSwap = true;
					else if (factionI == factionJ)
					{
						// Same faction: compare callsign (length-then-lexicographic)
						if (callsignI.Length() > callsignJ.Length() ||
						    (callsignI.Length() == callsignJ.Length() && callsignI > callsignJ))
							shouldSwap = true;
					}

					if (shouldSwap)
					{
						int temp = allGroupRooms[i];
						allGroupRooms[i] = allGroupRooms[j];
						allGroupRooms[j] = temp;
					}
				}
			}

			foreach (int groupRoom : allGroupRooms)
			{
				if (!outRoomsArray.Contains(groupRoom))
					outRoomsArray.Insert(groupRoom);
			}
		}
		else if (isLobbySpectator)
		{
			// Lobby spectator (player hasn't picked a slot yet): simplified layout
			// per user spec — only Deafen, Global, and option to create own Public.
			int localRoom = VoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Local" + currentPlayerId.ToString());
			if (localRoom >= 0) outRoomsArray.Insert(localRoom);

			int globalRoom = VoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Global");
			if (globalRoom >= 0) outRoomsArray.Insert(globalRoom);

			int publicRoom = VoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Public" + currentPlayerId.ToString());
			if (publicRoom >= 0) outRoomsArray.Insert(publicRoom);
		}
		else if (gameState == SCR_EGameModeState.BRIEFING)
		{
			if (currentPlayerFactionKey != "")
			{
				int factionRoom = VoNChannelsManager.GetOrCreateRoomWithFaction(currentPlayerFactionKey, "#PS-VoNRoom_Faction");
				if (factionRoom >= 0) outRoomsArray.Insert(factionRoom);

				int commandRoom = VoNChannelsManager.GetOrCreateRoomWithFaction(currentPlayerFactionKey, "#PS-VoNRoom_Command");
				if (commandRoom >= 0) outRoomsArray.Insert(commandRoom);

				// Show ALL same-faction groups in the briefing, even empty ones.
				// BUGFIX: Use GetFactionGroupRooms instead of iterating client-side
				// playables. GetPlayablesSorted() depends on m_SlotsMap replication
				// which may not be complete on JIP clients, causing empty groups to
				// be missed. GetFactionGroupRooms reads from the replicated room
				// map (m_ChannelKeyToRoomId) which is populated server-side by
				// EnsureAllFactionAndGroupRoomsExist and reliably replicated.
				array<int> factionGroupRooms = {};
				VoNChannelsManager.GetFactionGroupRooms(currentPlayerFactionKey, factionGroupRooms);
				// BUGFIX: Sort briefing group rooms alphabetically so the order is
				// stable and doesn't "jump" when the hash map iteration changes.
				for (int i = 0; i < factionGroupRooms.Count(); i++)
				{
				for (int j = i + 1; j < factionGroupRooms.Count(); j++)
				{
					string nameI = VoNChannelsManager.GetRoomName(factionGroupRooms[i]);
					string nameJ = VoNChannelsManager.GetRoomName(factionGroupRooms[j]);
					int sepI = nameI.IndexOf("|");
					int sepJ = nameJ.IndexOf("|");
					string numI;
					if (sepI >= 0) numI = nameI.Substring(sepI + 1, nameI.Length() - sepI - 1);
					else numI = "";
					string numJ;
					if (sepJ >= 0) numJ = nameJ.Substring(sepJ + 1, nameJ.Length() - sepJ - 1);
					else numJ = "";
					if (numI.Length() > numJ.Length() || (numI.Length() == numJ.Length() && numI > numJ))
					{
						int temp = factionGroupRooms[i];
						factionGroupRooms[i] = factionGroupRooms[j];
						factionGroupRooms[j] = temp;
					}
				}
				}
				foreach (int groupRoom : factionGroupRooms)
				{
					if (!outRoomsArray.Contains(groupRoom))
						outRoomsArray.Insert(groupRoom);
				}
			}
		}
		else if (gameState == SCR_EGameModeState.GAME && isSpectator)
		{
			// Spectator layout (user-requested): Deafen, Global, option to create
			// public room only — strictly per user spec, no other players' public
			// rooms are shown here. Players can talk in Global; the Public channel
			// is a personal space for the spectator to broadcast on.
			//   1) Deafen (Local) — own deafen channel
			//   2) Global — everyone is here
			//   3) Own Public — the player's own public room (so they can click "create")
			// BUGFIX: use GetOrCreateRoomWithFaction (not GetRoomWithFaction) for the
			// spectator's own Local and Public rooms. A JIP spectator whose server-side
			// RPC_InitChannel for these rooms hasn't arrived yet would otherwise see
			// an empty room list ("only my group channel" ghost). Placeholder rooms
			// (roomId=-1) are filtered by CreateRoom/CreateRoomIfNeed and become visible
			// when the real roomId replicates in.
			int localRoom = VoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Local" + currentPlayerId.ToString());
			if (localRoom >= 0) outRoomsArray.Insert(localRoom);

			int globalRoom = VoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Global");
			if (globalRoom >= 0) outRoomsArray.Insert(globalRoom);

			int publicRoom = VoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Public" + currentPlayerId.ToString());
			if (publicRoom >= 0) outRoomsArray.Insert(publicRoom);
		}
		else
		{
			// BUGFIX: same as spectator branch — use GetOrCreateRoomWithFaction for
			// every room so JIP clients see the full room set even before the server's
			// RPC_InitChannel for faction/command/group/public rooms has replicated.
			// Placeholder rooms are filtered by CreateRoom/CreateRoomIfNeed.
			int localRoom = VoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Local" + currentPlayerId.ToString());
			if (localRoom >= 0) outRoomsArray.Insert(localRoom);

			int globalRoom = VoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Global");
			if (globalRoom >= 0) outRoomsArray.Insert(globalRoom);

			if (currentPlayerFactionKey != "")
			{
				int factionRoom = VoNChannelsManager.GetOrCreateRoomWithFaction(currentPlayerFactionKey, "#PS-VoNRoom_Faction");
				if (factionRoom >= 0) outRoomsArray.Insert(factionRoom);

				int commandRoom = VoNChannelsManager.GetOrCreateRoomWithFaction(currentPlayerFactionKey, "#PS-VoNRoom_Command");
				if (commandRoom >= 0) outRoomsArray.Insert(commandRoom);

				RplId myPlayableId = playableManager.GetPlayableByPlayer(currentPlayerId);
				if (myPlayableId != RplId.Invalid())
				{
					int myGroupCallSign = playableManager.GetGroupCallsignByPlayable(myPlayableId);
					// BUGFIX: guard against 0 callsign creating ghost "US|0" room
					if (myGroupCallSign > 0)
					{
						int myGroupRoom = VoNChannelsManager.GetOrCreateRoomWithFaction(currentPlayerFactionKey, myGroupCallSign.ToString());
						if (myGroupRoom >= 0 && !outRoomsArray.Contains(myGroupRoom))
							outRoomsArray.Insert(myGroupRoom);
					}
				}
			}

			int publicRoom = VoNChannelsManager.GetOrCreateRoomWithFaction("", "#PS-VoNRoom_Public" + currentPlayerId.ToString());
			if (publicRoom >= 0) outRoomsArray.Insert(publicRoom);

			array<int> playersPublicRooms = {};
			VoNChannelsManager.GetPlayersPublicRooms(playersPublicRooms);
			foreach (int roomId : playersPublicRooms)
			{
				if (!outRoomsArray.Contains(roomId))
					outRoomsArray.Insert(roomId);
			}
		}

		// Safety net: re-add the player's actual current room to the visible set
		// so they're never stranded in a room the phase-specific branch didn't list
		// (e.g. a faction room lingering across a transient state). Skipped for
		// GAME spectator and lobby spectator so the strict simplified layouts hold
		// even if the player was admin-moved to another room before transitioning.
		int currentRoom = VoNChannelsManager.GetPlayerRoom(currentPlayerId);
		if (!isSpectator && !isLobbySpectator && currentRoom >= 0 && !outRoomsArray.Contains(currentRoom))
		{
			string currentRoomName = VoNChannelsManager.GetRoomName(currentRoom);
			if (!VoNChannelsManager.IsNoSoundChannel(currentRoomName))
				outRoomsArray.Insert(currentRoom);
		}


		// DIAGNOSTIC: trace the final visible room list for this phase
		string visibleList = "";
		foreach (int vrid : outRoomsArray)
		{
			if (visibleList != "") visibleList += ", ";
			visibleList += vrid.ToString() + "=" + VoNChannelsManager.GetRoomName(vrid);
		}
		PS_DebugLogger.Log("[VoN-CLI] GetVisibleRooms phase=" + typename.EnumToString(SCR_EGameModeState, gameState) + " isSpectator=" + isSpectator.ToString() + " faction=" + currentPlayerFactionKey + " visibleCount=" + outRoomsArray.Count().ToString() + " rooms=[" + visibleList + "]");
	}
	
	void SetSelectedPlayer(int playerId)
	{
		foreach (PS_VoiceChatRoom room : m_wRooms)
		{
			room.SetSelectedPlayer(playerId);
		}
	}
};


























