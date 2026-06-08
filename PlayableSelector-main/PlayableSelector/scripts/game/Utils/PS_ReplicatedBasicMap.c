class ReplicatedBasicMap<Class TKey, Class TValue>
{
	protected ref map<TKey, TValue> data = new map<TKey, TValue>();

	ref map<TKey, TValue> GetRawMap()
	{
		return data;
	}

	int Count()
	{
		return data.Count();
	}

	bool IsEmpty()
	{
		return data.IsEmpty();
	}

	void Clear()
	{
		data.Clear();
	}

	TValue Get(TKey key)
	{
		return data.Get(key);
	}

	bool Find(TKey key, out TValue val)
	{
		return data.Find(key, val);
	}

	TValue GetElement(int index)
	{
		return data.GetElement(index);
	}

	TKey GetKey(int i)
	{
		return data.GetKey(i);
	}

	void Set(TKey key, TValue value)
	{
		data.Set(key, value);
	}

	bool Remove(TKey key)
	{
		return data.Remove(key);
	}

	bool RemoveElement(int i)
	{
		return data.RemoveElement(i);
	}

	bool Contains(TKey key)
	{
		return data.Contains(key);
	}

	bool Insert(TKey key, TValue value)
	{
		return data.Insert(key, value);
	}

	int Copy(map<TKey, TValue> from)
	{
		return data.Copy(from);
	}

	bool ReplaceKey(TKey old_key, TKey new_key)
	{
		return data.ReplaceKey(old_key, new_key);
	}

	MapIterator Begin()
	{
		return data.Begin();
	}

	MapIterator End()
	{
		return data.End();
	}

	MapIterator Next(MapIterator it)
	{
		return data.Next(it);
	}

	TKey GetIteratorKey(MapIterator it)
	{
		return data.GetIteratorKey(it);
	}

	TValue GetIteratorElement(MapIterator it)
	{
		return data.GetIteratorElement(it);
	}

	static void Encode(SSnapSerializerBase snapshot, ScriptCtx ctx, ScriptBitSerializer packet)
	{
		int count;
		snapshot.SerializeInt(count);
		packet.SerializeInt(count);
		for (int i = 0; i < count; i++)
		{
			TKey key;
			PS_Serializer<SSnapSerializerBase, TKey>.Serialize(snapshot, key);
			PS_Serializer<ScriptBitSerializer, TKey>.Serialize(packet, key);

			TValue value;
			PS_Serializer<SSnapSerializerBase, TValue>.Serialize(snapshot, value);
			PS_Serializer<ScriptBitSerializer, TValue>.Serialize(packet, value);
		}
	}

	static bool Decode(ScriptBitSerializer packet, ScriptCtx ctx, SSnapSerializerBase snapshot)
	{
		int count;
		packet.SerializeInt(count);
		snapshot.SerializeInt(count);
		for (int i = 0; i < count; i++)
		{
			TKey key;
			PS_Serializer<ScriptBitSerializer, TKey>.Serialize(packet, key);
			PS_Serializer<SSnapSerializerBase, TKey>.Serialize(snapshot, key);

			TValue value;
			PS_Serializer<ScriptBitSerializer, TValue>.Serialize(packet, value);
			PS_Serializer<SSnapSerializerBase, TValue>.Serialize(snapshot, value);
		}
		return true;
	}

	static bool SnapCompare(SSnapSerializerBase lhs, SSnapSerializerBase rhs, ScriptCtx ctx)
	{
		int countA, countB;
		lhs.SerializeInt(countA);
		rhs.SerializeInt(countB);

		if (countA != countB)
			return false;

		for (int i = 0; i < countA; i++)
		{
			TKey key;
			TValue val;
			PS_Comparator<TKey>.CompareSnap(lhs, rhs, key);
			PS_Comparator<TValue>.CompareSnap(lhs, rhs, val);
		}
		return true;
	}

	static bool PropCompare(ReplicatedBasicMap<TKey, TValue> rplMap, SSnapSerializerBase snapshot, ScriptCtx ctx)
	{
		int snapCount;
		snapshot.SerializeInt(snapCount);

		if (snapCount != rplMap.Count())
			return false;

		for (int i = 0; i < snapCount; i++)
		{
			TKey key;
			TValue value;
			PS_Serializer<SSnapSerializerBase, TKey>.Serialize(snapshot, key);
			PS_Serializer<SSnapSerializerBase, TValue>.Serialize(snapshot, value);

			TValue realValue;
			if (!rplMap.Find(key, realValue))
				return false;

			if (value != realValue)
				return false;
		}
		return true;
	}

	static bool Extract(ReplicatedBasicMap<TKey, TValue> rplMap, ScriptCtx ctx, SSnapSerializerBase snapshot)
	{
		int count = rplMap.Count();
		snapshot.SerializeInt(count);
		for (int i = 0; i < count; i++)
		{
			TKey key = rplMap.GetKey(i);
			TValue value = rplMap.Get(key);

			PS_Serializer<SSnapSerializerBase, TKey>.Serialize(snapshot, key);
			PS_Serializer<SSnapSerializerBase, TValue>.Serialize(snapshot, value);
		}
		return true;
	}

	static bool Inject(SSnapSerializerBase snapshot, ScriptCtx ctx, ReplicatedBasicMap<TKey, TValue> rplMap)
	{
		rplMap.Clear();
		int count;
		snapshot.SerializeInt(count);
		for (int i = 0; i < count; i++)
		{
			TKey key;
			TValue value;

			PS_Serializer<SSnapSerializerBase, TKey>.Serialize(snapshot, key);
			PS_Serializer<SSnapSerializerBase, TValue>.Serialize(snapshot, value);

			rplMap.Insert(key, value);
		}
		return true;
	}
};
