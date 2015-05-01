#pragma once
#include <csignal>
#include <fstream>
#include "../vmsExceptionLog.h"
#include "../getexcptrs.h"
#pragma comment (lib, "dbghelp.lib")
class vmsCrashCatcher
{
public:
	vmsCrashCatcher () : 
		m_dwpFaultModuleCrashAddress (NULL), 
		m_dwGettingFaultModuleNameError (0),
		m_bInitialized (false),
		m_abort_prev_handler (nullptr)
	{
		assert (m_pThis == NULL);
		if (m_pThis != NULL)
			throw 0;
		m_pThis = this;
	}

	virtual ~vmsCrashCatcher(void)
	{
		if (m_pThis == this)
		{
			if (m_bInitialized)
			{
				SetUnhandledExceptionFilter (m_pPrevFilter);
				m_pPrevFilter = NULL;
				if (m_pVEH)
				{
					RemoveVectoredExceptionHandler (m_pVEH);
					m_pVEH = NULL;
				}
				if (m_abort_prev_handler)
					signal (SIGABRT, m_abort_prev_handler);
			}

			m_pThis = NULL;
		}
	}

	void Initialize ()
	{
		assert (!m_bInitialized);
		if (m_bInitialized)
			return;
		m_pPrevFilter = SetUnhandledExceptionFilter (_UnhandledExceptionFilter);
		//m_pVEH = AddVectoredExceptionHandler (TRUE, _VectoredExceptionHandler);
		_set_invalid_parameter_handler (_crt_InvalidParameterHandler);
		m_abort_prev_handler = signal (SIGABRT, signal_handler);
		m_bInitialized = true;
	}

	void setLogVehExceptions (bool bSet = true)
	{
		try {
			if (!bSet)
			{
				m_VehLogFile.close ();
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
				m_VehLogFile.open (tszPath, std::ios_base::out | std::ios_base::trunc);
			}
		}catch (...){}
	}

protected:
	static vmsCrashCatcher* m_pThis;
	static LPTOP_LEVEL_EXCEPTION_FILTER m_pPrevFilter;
	static PVOID m_pVEH;
	tstring m_tstrDumpFile;
	tstring m_tstrFaultModuleName;
	DWORD_PTR m_dwpFaultModuleCrashAddress;
	DWORD m_dwGettingFaultModuleNameError;
	bool m_bInitialized;
	typedef void (__cdecl *FNSignalHandler)(int);
	FNSignalHandler m_abort_prev_handler;

	struct ExceptionInfo
	{
		DWORD dwThreadId;
		PEXCEPTION_POINTERS pEP;
		ExceptionInfo () : dwThreadId (0), pEP (NULL) {}
	};
	ExceptionInfo m_ei;

	static tstring CreateDump (const ExceptionInfo &ei)
	{
		// create crash dump file

		MINIDUMP_EXCEPTION_INFORMATION eInfo;
		eInfo.ThreadId = ei.dwThreadId;
		eInfo.ExceptionPointers = ei.pEP;
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

	void ProcessException ()
	{
		vmsAUTOLOCKSECTION (m_csExceptionHandler);

		LogException (m_ei.pEP);

		m_tstrDumpFile = CreateDump (m_ei);

		// query basic crash info (module name and crash address)

		MEMORY_BASIC_INFORMATION mbi;
		SIZE_T nSize = VirtualQuery (m_ei.pEP->ExceptionRecord->ExceptionAddress, &mbi, sizeof(mbi));
		if (nSize)
		{
			m_dwpFaultModuleCrashAddress = (DWORD_PTR)m_ei.pEP->ExceptionRecord->ExceptionAddress - (DWORD_PTR)mbi.AllocationBase;
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

	static DWORD WINAPI _threadProcessException (LPVOID lp)
	{
		vmsCrashCatcher *pthis = (vmsCrashCatcher*)lp;
		pthis->ProcessException ();
		return 0;
	}

	static LONG WINAPI _UnhandledExceptionFilter (PEXCEPTION_POINTERS pEP)
	{
		m_pThis->m_ei.dwThreadId = GetCurrentThreadId ();
		m_pThis->m_ei.pEP = pEP;

		WaitForSingleObject (
			CreateThread (NULL, 0, _threadProcessException, m_pThis, 0, NULL), INFINITE);

		return EXCEPTION_EXECUTE_HANDLER;
	}

	static LONG WINAPI _VectoredExceptionHandler (PEXCEPTION_POINTERS pEP)
	{
		const BYTE bCode = pEP->ExceptionRecord->ExceptionCode >> 24;

		if (bCode == 0xC0 || bCode == 0x40 || bCode == 0x80)
		{
			m_pThis->m_ei.dwThreadId = GetCurrentThreadId ();
			m_pThis->m_ei.pEP = pEP;

			WaitForSingleObject (
				CreateThread (NULL, 0, _threadProcessException, m_pThis, 0, NULL), INFINITE);
		}
		else
		{
			m_pThis->LogException (pEP);
		}

		return EXCEPTION_CONTINUE_SEARCH;
	}


	static void _crt_InvalidParameterHandler (const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved)
	{
		on_crt_fatal_error ();
	}


	static void signal_handler (int)
	{
		on_crt_fatal_error ();
	}


	static void on_crt_fatal_error ()
	{
		PEXCEPTION_POINTERS pEP = NULL;
		GetExceptionPointers (0, &pEP);
		assert (pEP != NULL);
		if (pEP)
		{
			m_pThis->m_ei.dwThreadId = GetCurrentThreadId ();
			m_pThis->m_ei.pEP = pEP;
			m_pThis->ProcessException ();
		}
		else
		{
			throw 0; // proceed with SEH handler
		}
	}

	virtual void onCrashDumpCreated () = NULL;


protected:
	inline void LogException (PEXCEPTION_POINTERS pEP)
	{
		if (m_VehLogFile)
		{
			vmsExceptionLog::LogException (m_VehLogFile, pEP);
			m_VehLogFile.flush ();
		}
	}

	std::ofstream m_VehLogFile;
	vmsCriticalSection m_csExceptionHandler;
};

__declspec(selectany) vmsCrashCatcher* vmsCrashCatcher::m_pThis = NULL;
__declspec(selectany) LPTOP_LEVEL_EXCEPTION_FILTER vmsCrashCatcher::m_pPrevFilter = NULL;
__declspec(selectany) PVOID vmsCrashCatcher::m_pVEH = NULL;