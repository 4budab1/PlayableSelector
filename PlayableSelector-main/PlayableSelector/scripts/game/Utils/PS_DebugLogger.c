class PS_DebugLogger
{
	static bool DebugEnabled = true;
	private static ref map<string, ref array<float>> m_RateTracker = new map<string, ref array<float>>();
	private const int MAX_DUPLICATES = 5;
	private const float DEDUP_WINDOW = 10.0;

	static void Log(string message, int playerId = -1)
	{
		if (!DebugEnabled)
			return;

		string prefix = "[PS_DBG]";
		if (playerId > 0)
			prefix = prefix + "[P" + playerId.ToString() + "]";
		string role;
		if (Replication.IsServer())
			role = "[SRV]";
		else
			role = "[CLI]";
		string key = prefix + role + message;

		if (!IsAllowed(key))
			return;

		Print(prefix + role + " " + message, LogLevel.NORMAL);
	}

	static void LogImportant(string message, int playerId = -1)
	{
		if (!DebugEnabled)
			return;

		string prefix = "[PS_DBG]";
		if (playerId > 0)
			prefix = prefix + "[P" + playerId.ToString() + "]";
		string role;
		if (Replication.IsServer())
			role = "[SRV]";
		else
			role = "[CLI]";

		Print(prefix + role + " " + message, LogLevel.NORMAL);
	}

	static void LogWarning(string message, int playerId = -1)
	{
		string prefix = "[PS_WRN]";
		if (playerId > 0)
			prefix = prefix + "[P" + playerId.ToString() + "]";
		string role;
		if (Replication.IsServer())
			role = "[SRV]";
		else
			role = "[CLI]";

		Print(prefix + role + " " + message, LogLevel.WARNING);
	}

	static void LogError(string message, int playerId = -1)
	{
		string prefix = "[PS_ERR]";
		if (playerId > 0)
			prefix = prefix + "[P" + playerId.ToString() + "]";
		string role;
		if (Replication.IsServer())
			role = "[SRV]";
		else
			role = "[CLI]";

		Print(prefix + role + " " + message, LogLevel.ERROR);
	}

	private static bool IsAllowed(string key)
	{
		float now = GetGame().GetWorld().GetWorldTime();

		if (!m_RateTracker.Contains(key))
			m_RateTracker[key] = {};

		array<float> timestamps = m_RateTracker[key];

		// Purge old entries — timestamps are chronologically ordered (oldest at index 0)
		while (timestamps.Count() > 0 && now - timestamps[0] > DEDUP_WINDOW)
			timestamps.Remove(0);

		if (timestamps.Count() >= MAX_DUPLICATES)
			return false;

		timestamps.Insert(now);
		return true;
	}
};
