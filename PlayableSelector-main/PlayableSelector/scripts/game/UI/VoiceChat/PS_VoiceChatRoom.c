class PS_VoiceChatRoom : SCR_ScriptedWidgetComponent
{
	[Attribute("{086F282C8CE692F1}UI/VoiceChat/VoicePlayerSelector.layout")]
	protected ResourceName m_sPlayerVoiceSelectorPrefab;
	
	// Global cached
	protected PlayerManager m_gPlayerManager;
	protected PS_PlayableManager m_gPlayableManager;
	protected PS_VoNChannelsManager m_gVoNChannelsManager;
	
	// Local
	int m_iRoomId;
	PS_VoiceRoomHeader m_hRoomHandler;
	VerticalLayoutWidget m_wPlayersVerticalLayout;
	ref map<int, PS_PlayerVoiceSelector> m_mPlayers = new map<int, PS_PlayerVoiceSelector>;
	
	int m_iSelectedPlayer = -1;
	// Tracks the last known channel key so UpdateInfo can detect when a placeholder
	// room name (empty) resolves to the real name via RPC_InitChannel.
	protected string m_sLastKnownChannelKey = "";
	
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		
		if (!GetGame().InPlayMode())
			return;
		
		// global
		m_gPlayerManager   = GetGame().GetPlayerManager();
		m_gPlayableManager = PS_PlayableManager.GetInstance();
		m_gVoNChannelsManager = PS_VoNChannelsManager.GetInstance();
		
		// local
		m_hRoomHandler = PS_VoiceRoomHeader.Cast(w.FindAnyWidget("VoiceRoomHeader").FindHandler(PS_VoiceRoomHeader));
		m_wPlayersVerticalLayout = VerticalLayoutWidget.Cast(w.FindAnyWidget("PlayersVerticalLayout"));
	}
	
	void UpdateInfo()
	{
		// Refresh room name in case the placeholder empty name resolved to the real
		// channel key via RPC_InitChannel (common JIP/race condition). Without this,
		// rooms created with an empty name show "Room not registered on server" forever.
		string currentKey = m_gVoNChannelsManager.GetRoomName(m_iRoomId);
		if (currentKey != "" && currentKey != m_sLastKnownChannelKey)
		{
			SetRoomId(m_iRoomId);
		}
		m_hRoomHandler.UpdateInfo();
		foreach (int playerId, PS_PlayerVoiceSelector playerVoiceSelector : m_mPlayers)
		{
			playerVoiceSelector.UpdateInfo();
		}
	}
	
	void SetRoomId(int roomId, string forcedRoomKey = "")
	{
		m_iRoomId = roomId;
		
		string roomName = forcedRoomKey;
		if (roomName == "")
			roomName = m_gVoNChannelsManager.GetRoomName(roomId);
		m_sLastKnownChannelKey = roomName;
		FactionKey factionKey = "";
		if (roomName == "") roomName = "Room not registered on server";
		else {
			array<string> outTokens = new array<string>();
			roomName.Split("|", outTokens, false);
			if (outTokens.Count() >= 2)
			{
				factionKey = outTokens[0];
				roomName = outTokens[1];
			}
		}
		SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		SCR_Faction faction = SCR_Faction.Cast(factionManager.GetFactionByKey(factionKey));
		m_hRoomHandler.SetRoomName(faction, roomName, roomId);
	}
	
	void AddPlayer(int playerId)
	{
		if (m_mPlayers.Contains(playerId))
			return;

		Widget playerSelector = GetGame().GetWorkspace().CreateWidgets(m_sPlayerVoiceSelectorPrefab);
		PS_PlayerVoiceSelector handler = PS_PlayerVoiceSelector.Cast(playerSelector.FindHandler(PS_PlayerVoiceSelector));

		m_mPlayers[playerId] = handler;

		handler.SetPlayer(playerId);
		m_wPlayersVerticalLayout.AddChild(playerSelector);
	}

	// Public query for PS_VoiceChatList.SyncRoomPlayers to avoid duplicate adds.
	bool HasPlayer(int playerId)
	{
		return m_mPlayers.Contains(playerId);
	}
	
	void RemovePlayer(int playerId)
	{
		if (!m_mPlayers.Contains(playerId)) return;
		
		PS_PlayerVoiceSelector handler = m_mPlayers[playerId];
		handler.GetRootWidget().RemoveFromHierarchy();
		m_mPlayers.Remove(playerId);
	}
	
	void SetSelectedPlayer(int playerId)
	{
		PS_PlayerVoiceSelector playerSelectorOld = m_mPlayers.Get(m_iSelectedPlayer);
		PS_PlayerVoiceSelector playerSelectorNew = m_mPlayers.Get(playerId);
		if (playerSelectorOld)
			playerSelectorOld.Deselect();
		if (playerSelectorNew)
			playerSelectorNew.Select();
		m_iSelectedPlayer = playerId;
	}
}







