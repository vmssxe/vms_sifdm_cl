#pragma once
#include <fstream>
#include "../vmsExceptionLog.h"
#include "exc_provider/vmsUnhandledExceptionProvider.h"
#include "exc_provider/vmsCrtFatalExceptionProvider.h"
#include "exc_provider/vmsVectoredFatalExceptionProvider.h"
#pragma comment (lib, "dbghelp.lib")
class vmsCrashCatcher
{
public:
	vmsCrashCatcher () : 
		m_dwpFaultModuleCrashAddress (NULL), 
		m_dwGettingFaultModuleNameError (0),
		m_bInitialized (false)
	{
	}

	virtual ~vmsCrashCatcher(void)
	{
	}

	void Initialize ()
	{
		assert (!m_bInitialized);
		if (m_bInitialized)
			return;

		m_excProviders.push_back (std::make_unique <vmsUnhandledExceptionProvider> ());
		m_excProviders.push_back (std::make_unique <vmsCrtFatalExceptionProvider> ());
		//m_excProviders.push_back (std::make_unique <vmsVectoredFatalExceptionProvider> ());

		auto fe_callback = [this](PEXCEPTION_POINTERS ep){return on_fatal_exception (ep);};

		for (const auto &prov : m_excProviders)
			prov->set_callback (fe_callback);

		m_bInitialized = true;
	}

	void setLogExceptions (bool bSet = true)
	{
		try {
			if (!bSet)
			{
				m_LogFile.close ();
				return;
			}
			TCHAR tszPath [MAX_PATH] = _T ("");
			GetTempPath (_countof (tszPath), tszPath);
			_tcscat (tszPath, _T ("\\CrashCatcher"));
			CreateDirectory (tszPath, NULL);
			TCHAR tszModule [MAX_PATH] = _T ("");
			GetModuleFileName (NULL, tszModule, _countof (tszModule));
			LPTSTR ptszExeName = _tcsrchr (tszModule, '\\');
			if (ptszExeName)
			{
				ptszExeName++;
				_tcscat (tszPath, _T ("\\"));
				_tcscat (tszPath, ptszExeName);
				_tcscat (tszPath, _T (".xml"));
				m_LogFile.open (tszPath, std::ios_base::out | std::ios_base::trunc);
			}
		}catch (...){}
	}

protected:
	std::vector <std::unique_ptr <vmsFatalExceptionProvider>> m_excProviders;
	tstring m_tstrDumpFile;
	tstring m_tstrFaultModuleName;
	DWORD_PTR m_dwpFaultModuleCrashAddress;
	DWORD m_dwGettingFaultModuleNameError;
	bool m_bInitialized;

protected:
	ULONG on_fatal_exception (PEXCEPTION_POINTERS pEP)
	{
		auto params = new _threadProcessExceptionParams;
		params->pthis = this;
		params->pEP = pEP;
		params->thread_id = GetCurrentThreadId ();

		WaitForSingleObject (
			CreateThread (NULL, 0, _threadProcessException, params, 0, NULL), INFINITE);

		return EXCEPTION_CONTINUE_SEARCH;
	}

	static tstring CreateDump (PEXCEPTION_POINTERS pEP, DWORD thread_id)
	{
		// create crash dump file

		MINIDUMP_EXCEPTION_INFORMATION eInfo;
		eInfo.ThreadId = thread_id;
		eInfo.ExceptionPointers = pEP;
		eInfo.ClientPointers = FALSE;

		TCHAR tszTmpFile [MAX_PATH] = _T ("");
		TCHAR tszTmpPath [MAX_PATH] = _T ("");
		GetTempPath (MAX_PATH, tszTmpPath);
		GetTempFileName (tszTmpPath, _T ("tmp"), 0, tszTmpFile);

		HANDLE hFile = CreateFile (tszTmpFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
			0, NULL);

		if (hFile == INVALID_HANDLE_VALUE)
			return 0;

		BOOL bDumpCreated = MiniDumpWriteDump (GetCurrentProcess(), GetCurrentProcessId(), hFile, 
			(MINIDUMP_TYPE) (MiniDumpScanMemory | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs | MiniDumpWithProcessThreadData),
			&eInfo, NULL, NULL);

		CloseHandle (hFile);

		if (!bDumpCreated)
			DeleteFile (tszTmpFile);
		
		return bDumpCreated ? tszTmpFile : _T ("");
	}

	void ProcessException (PEXCEPTION_POINTERS pEP, DWORD thread_id)
	{
		vmsAUTOLOCKSECTION (m_csExceptionHandler);

		LogException (pEP);

		m_tstrDumpFile = CreateDump (pEP, thread_id);

		// query basic crash info (module name and crash address)

		MEMORY_BASIC_INFORMATION mbi;
		SIZE_T nSize = VirtualQuery (pEP->ExceptionRecord->ExceptionAddress, &mbi, sizeof(mbi));
		if (nSize)
		{
			m_dwpFaultModuleCrashAddress = (DWORD_PTR)pEP->ExceptionRecord->ExceptionAddress - (DWORD_PTR)mbi.AllocationBase;
			TCHAR tszModule [MAX_PATH] = _T ("");
			GetModuleFileName ((HMODULE)mbi.AllocationBase, tszModule, _countof (tszModule));
			if (*tszModule)
				m_tstrFaultModuleName = _tcsrchr (tszModule, '\\') ? _tcsrchr (tszModule, '\\') + 1 : tszModule;			
			else
				m_dwGettingFaultModuleNameError = GetLastError ();
		}

		if (!m_tstrDumpFile.empty ())
			onCrashDumpCreated ();

		TerminateProcess (GetCurrentProcess (), ERROR_UNHANDLED_EXCEPTION);
	}

protected:
	struct _threadProcessExceptionParams
	{
		vmsCrashCatcher *pthis;
		PEXCEPTION_POINTERS pEP;
		DWORD thread_id;
	};

	static DWORD WINAPI _threadProcessException (LPVOID lp)
	{
		std::unique_ptr <_threadProcessExceptionParams> params (
			reinterpret_cast <_threadProcessExceptionParams*> (lp));
		params->pthis->ProcessException (params->pEP, params->thread_id);
		return 0;
	}

protected:
	virtual void onCrashDumpCreated () = NULL;


protected:
	inline void LogException (PEXCEPTION_POINTERS pEP)
	{
		if (m_LogFile)
		{
			vmsExceptionLog::LogException (m_LogFile, pEP);
			m_LogFile.flush ();
		}
	}

	std::ofstream m_LogFile;
	vmsCriticalSection m_csExceptionHandler;
};
