#pragma once

class vmsThread;
class vmsCreatesThreads;

namespace ThreadEntryPoint
{
	struct ThreadParams
	{
		vmsThread *thread;
		vmsCreatesThreads *threadOwner;
		unsigned(__stdcall *routine)(void*);
		void *routineParams;
	};

	static unsigned _stdcall threadMain(void *pv);
}

class vmsThread
{
public:
	vmsThread(vmsCreatesThreads *threadOwner, unsigned(__stdcall *start_address)(void*), void *pvParam)
	{
		auto *params = new ThreadEntryPoint::ThreadParams;
		params->thread = this;
		params->threadOwner = threadOwner;
		params->routine = start_address;
		params->routineParams = pvParam;
		m_spHandle = std::make_shared<vmsWinHandle>((HANDLE)_beginthreadex(NULL, 0, ThreadEntryPoint::threadMain, params, 0, NULL));
	}

	~vmsThread() {}

	vmsWinHandle::tSP handle() const
	{
		return m_spHandle;
	}

private:
	vmsWinHandle::tSP m_spHandle;
};