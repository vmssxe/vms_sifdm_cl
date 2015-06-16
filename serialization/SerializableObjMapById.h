#pragma once
#include "SerializableObjMap.h"
template <typename TObj, typename TKey = uint32_t>
class SerializableObjMapById :
	public SerializableObjMap <TKey, TObj>
{
public:
	virtual TKey add_item (const std::shared_ptr <TObj> &obj)
	{
		TKey result;
		{
			vmsLockableScope;
			add_item (result = m_next_id, obj);
		}
		setDirty ();
		return result;
	}

	virtual void remove_item (TKey id)
	{
		{
			vmsLockableScope;
			auto it = m_items.find (id);
			assert (it != m_items.end ());
			if (it == m_items.end ())
				return;
			m_items.erase (it);
		}
		setDirty ();
	}

protected:
	using base_t = SerializableObjMap <TKey, TObj>;

protected:
	TKey m_next_id = 1;

protected:
	virtual bool add_item (TKey id, const std::shared_ptr <TObj> &obj) override
	{
		m_next_id = max (m_next_id, id+1);
		return base_t::add_item (id, obj);
	}
};
	