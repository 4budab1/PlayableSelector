class PS_PlayersHelper
{
	static bool IsAdminOrServer()
	{
		return SCR_Global.IsAdmin();
	}

	static bool IsAdmin(int playerId)
	{
		return SCR_Global.IsAdmin(playerId);
	}
}
