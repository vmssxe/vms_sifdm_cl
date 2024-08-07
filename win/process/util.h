#pragma once
#include <Psapi.h>
#include "../misc/vmsAvUtil.h"

#pragma comment(lib, "Psapi.lib")

inline HMODULE vmsModuleFromAddress (LPCVOID pv)
{
	MEMORY_BASIC_INFORMATION mbi;
	return VirtualQuery (pv, &mbi, sizeof (mbi)) != 0 ? (HMODULE)mbi.AllocationBase : NULL;
}

inline HMODULE vmsGetThisModuleHandle ()
{
	return vmsModuleFromAddress (vmsModuleFromAddress);
}

// returns HINSTANCE_ERROR in case the loaded module's handle can't be deteminated.
inline HMODULE vmsLoadDllIntoProcess (HANDLE hProcess, LPCTSTR ptszDllName)
{
	HANDLE hThread = NULL;

	void* pLibRemote = NULL;   // The address (in the remote process) where 
	// szLibPath will be copied to;
	HMODULE  hLibModule = 0; 
	HMODULE hKernel32 = ::GetModuleHandle(_T ("Kernel32"));

	// initialize szLibPath
	//...

	// 1. Allocate memory in the remote process for szLibPath
	// 2. Write szLibPath to the allocated memory
	pLibRemote = ::VirtualAllocEx (hProcess, NULL, (_tcslen (ptszDllName)+1) * sizeof (TCHAR), MEM_COMMIT, 
		PAGE_READWRITE);
	if (!pLibRemote)
		return NULL;
	if (!::WriteProcessMemory (hProcess, pLibRemote, (void*)ptszDllName, (_tcslen (ptszDllName)+1) * sizeof (TCHAR), 
		NULL))
	{
		::VirtualFreeEx (hProcess, pLibRemote, (_tcslen (ptszDllName)+1) * sizeof (TCHAR), MEM_RELEASE);
		return NULL;
	}

#ifdef UNICODE
	static const char _TMP_SZ [] = "_MY_DEALWITHKAV__LoadLibraryW";
#else
	static const char _TMP_SZ [] = "_MY_DEALWITHKAV__LoadLibraryA";
#endif
	int len = (int)strlen (_TMP_SZ);
	LPSTR psz = (LPSTR) GetModuleHandle (NULL);
	while (strncmp (psz, _TMP_SZ, len))
		psz++;
	psz += 17;

	// Load dll into the remote process
	// (via CreateRemoteThread & LoadLibrary)
	auto kernel_fn_addr = GetProcAddress (hKernel32, psz);
	const bool kernel_fn_addr_valid = vmsModuleFromAddress (kernel_fn_addr) == hKernel32;
	assert (kernel_fn_addr_valid);
	if (!kernel_fn_addr_valid)
		return NULL;

	hThread = vmsAvUtil::CreateRemoteThread (hProcess, NULL, 0, 
		(LPTHREAD_START_ROUTINE) kernel_fn_addr, pLibRemote, 0, NULL);
	if (hThread != NULL)
	{
		::WaitForSingleObject (hThread, INFINITE);

#ifndef _WIN64
		{
			// Get handle of the loaded module
			DWORD dw = 0;
			::GetExitCodeThread(hThread, &dw);
			hLibModule = reinterpret_cast<HMODULE>(dw);
		}
#else
		{
			HMODULE hMods[1024];
			DWORD cbNeeded;
			if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
			{
				for (UINT i = 0; i < (cbNeeded / sizeof(HMODULE)); ++i)
				{
					TCHAR szModName[MAX_PATH] = _T("");

					// Get the full path to the module's file.

					if (GetModuleFileNameEx(hProcess, hMods[i], szModName,
						sizeof(szModName) / sizeof(TCHAR)))
					{
						// Print the module name and handle value.

						if (!_tcscmp(ptszDllName, szModName))
						{
							hLibModule = hMods[i];
							break;
						}
					}
				}
			}
			assert(hLibModule);
			if (!hLibModule)
				hLibModule = (HMODULE)HINSTANCE_ERROR;
		}
#endif

		// Clean up
		::CloseHandle (hThread);
	}

	::VirtualFreeEx (hProcess, pLibRemote, (_tcslen (ptszDllName)+1) * sizeof (TCHAR), MEM_RELEASE);

	return hLibModule;
}

inline BOOL vmsFreeDllFromProcess (HANDLE hProcess, HMODULE hDll)
{
	HANDLE hThread = NULL;
	HMODULE hKernel32 = ::GetModuleHandle(_T ("Kernel32"));

	static const char _TMP_SZ [] = "_MY_DEALWITHKAV__FreeLibrary";
	int len = (int)strlen (_TMP_SZ);
	LPSTR psz = (LPSTR) GetModuleHandle (NULL);
	while (strncmp (psz, _TMP_SZ, len))
		psz++;
	psz += 17;

	// Load dll into the remote process
	// (via CreateRemoteThread & LoadLibrary)
	hThread = vmsAvUtil::CreateRemoteThread (hProcess, NULL, 0, 
		(LPTHREAD_START_ROUTINE)GetProcAddress (hKernel32, psz), 
		hDll, 0, NULL);

	DWORD dwResult = FALSE;

	if (hThread)
	{
		::WaitForSingleObject (hThread, INFINITE);

		// Get handle of the loaded module
		::GetExitCodeThread (hThread, &dwResult);

		// Clean up
		::CloseHandle (hThread);
	}

	return dwResult;
}

inline bool vmsCheckModuleHandleValid (HMODULE mod)
{
	MODULEINFO mi = {0};
	return FALSE != GetModuleInformation (GetCurrentProcess (),
		mod, &mi, sizeof (mi));
}