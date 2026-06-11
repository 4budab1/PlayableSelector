class PS_RateLimitEntry
{
	ref array<float> m_Timestamps = {};
}

class PS_DebugLogger
{
	static bool DebugEnabled = true;
	private static ref map<string, ref PS_RateLimitEntry> m_RateTracker = new map<string, ref PS_RateLimitEntry>();
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
		string key = prefix + role + message;

		if (!IsAllowed(key))
			return;

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

		ref PS_RateLimitEntry entry;
		if (!m_RateTracker.Find(key, entry))
		{
			entry = new PS_RateLimitEntry();
			m_RateTracker.Insert(key, entry);
		}

		// Purge old entries — timestamps are chronologically ordered (oldest at index 0)
		while (entry.m_Timestamps.Count() > 0 && now - entry.m_Timestamps[0] > DEDUP_WINDOW)
			entry.m_Timestamps.Remove(0);

		if (entry.m_Timestamps.Count() >= MAX_DUPLICATES)
			return false;

		entry.m_Timestamps.Insert(now);
		return true;
	}
};
