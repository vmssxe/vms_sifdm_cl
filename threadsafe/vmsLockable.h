#pragma once

template <class TLockable>
class vmsLockableAutolock
{
public:
	vmsLockableAutolock (const TLockable *pObject) : 
	  m_pObject (pObject)
	{
		if (m_pObject)
			m_pObject->Lock ();
	}

	~vmsLockableAutolock ()
	{
		if (m_pObject)
			m_pObject->Unlock ();
	}

protected:
	const TLockable *m_pObject;

private:
	vmsLockableAutolock (const vmsLockableAutolock&);
	vmsLockableAutolock& operator= (const vmsLockableAutolock&);
};


class vmsLockable
{
public:
	typedef vmsLockableAutolock <vmsLockable> tAutolock;
	typedef std::shared_ptr <tAutolock> tspAutolock;
public:
	// locking / unlocking
	// derived class must implement these function to be a thread safe
	virtual void Lock () const {}
	virtual void Unlock () const {}
	// a more convenient way to lock an object
	virtual tspAutolock LockAuto () const
	{
		return std::make_shared <tAutolock> (this);
	}
};


#define vmsLockable_ImplementDelegatedTo(obj) \
	virtual void Lock () const override {obj.Lock ();} \
	virtual void Unlock () const override {obj.Unlock ();}