#pragma once
template <typename TKey, typename TObj>
class SerializableObjMap :
	public vmsSerializable
{
public:
	using container_t = std::map <TKey, std::shared_ptr <TObj>>;

public:
	container_t& container ()
	{
		return m_items;
	}

protected:
	container_t m_items;

protected:
	// pStm can be used to identify the type of object
	// no serialization must be done here
	virtual std::shared_ptr <TObj> create_obj (vmsSerializationIoStream *pStm) = 0;

public:
	virtual bool Serialize (vmsSerializationIoStream *pStm, unsigned flags /* = 0 */) override
	{
		vmsLockableScope;

		if (pStm->isInputStream ())
		{
			m_items.clear ();
			auto nodes = pStm->SelectNodes (L"item");
			for (const auto &node : nodes)
			{
				TKey id;
				if (!node->SerializeValueS (L"item-id", id))
					return false;
				auto obj = create_obj (node.get ());
				if (!obj)
					return false;
				if (!obj->Serialize (node.get (), flags))
					return false;
				assert (m_items.find (id) == m_items.end ());
				add_item (id, obj);
			}
		}
		else
		{
			for (const auto &item : m_items)
			{
				auto node = pStm->SelectOrCreateNode (L"item");
				assert (node);
				auto id = item.first;
				if (!node->SerializeValueS (L"item-id", id) ||
					!item.second->Serialize (node.get (), flags))
				{
					return false;
				}
			}
		}

		return true;
	}

protected:
	virtual bool add_item (TKey id, const std::shared_ptr <TObj> &obj)
	{
		m_items [id] = obj;
		return true;
	}
};