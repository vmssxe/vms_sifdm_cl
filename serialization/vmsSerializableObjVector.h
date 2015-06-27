#pragma once
template <class T>
class vmsSerializableObjVector :
	public vmsSerializable
{
public:
	using container_t = std::vector <T>;

public:
	void add_item (T obj)
	{
		{
			vmsLockableScope;
			add_item_ (obj);
		}
		setDirty ();
	}

	// not a threadsafe function
	void remove_item (typename container_t::iterator it)
	{
		m_items.erase (it);
		setDirty ();
	}

	container_t& container ()
	{
		return m_items;
	}

	virtual ~vmsSerializableObjVector () {}

protected:
	container_t m_items;

protected:
	// pStm can be used to identify the type of object
	// no serialization must be done here
	virtual T create_obj (vmsSerializationIoStream *pStm) = 0;

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
				auto obj = create_obj (node.get ());
				if (!obj)
					return false;
				if (!obj->Serialize (node.get (), flags))
					return false;
				add_item_ (std::move (obj));
			}
		}
		else
		{
			for (const auto &item : m_items)
			{
				auto node = pStm->SelectOrCreateNode (L"item");
				assert (node);
				if (!item->Serialize (node.get (), flags))
					return false;
			}
		}
		return true;
	}

protected:
	virtual bool add_item_ (T obj)
	{
		m_items.push_back (std::move (obj));
		return true;
	}
};


template <class T>
class vmsSerializableSharedObjVector :
	public vmsSerializableObjVector <std::shared_ptr <T>>
{
public:
	virtual std::shared_ptr <T> create_obj (vmsSerializationIoStream *pStm) override
	{
		return std::make_shared <T> ();
	}
};