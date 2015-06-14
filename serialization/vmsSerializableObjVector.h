#pragma once
template <class T>
class vmsSerializableObjVector :
	public vmsSerializable
{
public:
	std::vector <T>& container ()
	{
		return m_items;
	}

	virtual ~vmsSerializableObjVector () {}

protected:
	std::vector <T> m_items;

protected:
	// pStm can be used to identify the type of object
	// no serialization must be done here
	virtual T create_obj (vmsSerializationIoStream *pStm) = 0;

public:
	virtual bool Serialize (vmsSerializationIoStream *pStm, unsigned flags /* = 0 */) override
	{
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
				add_item (std::move (obj));
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
	virtual bool add_item (T obj)
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