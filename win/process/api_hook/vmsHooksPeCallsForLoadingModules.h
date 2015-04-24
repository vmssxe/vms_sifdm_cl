#pragma once
#include "vmsHooksPeCalls.h"
class vmsHooksPeCallsForLoadingModules :
	public vmsHooksPeCalls
{
public:
	vmsHooksPeCallsForLoadingModules (std::shared_ptr <vmsPeFnHook> hooker) :
		vmsHooksPeCalls (hooker) 
	{
		assert (!ms_hooker); // singleton
		ms_hooker = hooker;

		m_hook_functions.push_back (
			hook_fn_info ("kernel32.dll", "GetProcAddress", (FARPROC)myGetProcAddress));
		m_hook_functions.push_back (
			hook_fn_info ("kernel32.dll", "LoadLibraryA", (FARPROC)myLoadLibraryA));
		m_hook_functions.push_back (
			hook_fn_info ("kernel32.dll", "LoadLibraryW", (FARPROC)myLoadLibraryW));
		m_hook_functions.push_back (
			hook_fn_info ("kernel32.dll", "LoadLibraryExA", (FARPROC)myLoadLibraryExA));
		m_hook_functions.push_back (
			hook_fn_info ("kernel32.dll", "LoadLibraryExW", (FARPROC)myLoadLibraryExW));
	}

	~vmsHooksPeCallsForLoadingModules ()
	{
		ms_hooker = nullptr;
	}

protected:
	static std::shared_ptr <vmsPeFnHook> ms_hooker;

protected:
	static HMODULE WINAPI myLoadLibraryA (LPCSTR lpFileName)
	{
		return onLoadLibrary (lpFileName, false);
	}

	static HMODULE WINAPI myLoadLibraryW (LPCWSTR lpFileName)
	{
		return onLoadLibrary (lpFileName, true);
	}

	static HMODULE onLoadLibrary (LPCVOID lpFileName, bool bUnicode)
	{
		typedef HMODULE (WINAPI *FNLoadLibrary)(LPCVOID lpFileName);

		assert (ms_hooker);
		FNLoadLibrary pfn = (FNLoadLibrary)ms_hooker->getOriginalFunction (bUnicode ? (FARPROC)myLoadLibraryW : (FARPROC)myLoadLibraryA);
		assert (pfn != NULL);

		vmsPeFnHook::loaded_modules_data_t modulesWas;
		ms_hooker->onBeforeNewModuleLoaded (modulesWas);

		HMODULE hMod = pfn (lpFileName);
		if (!hMod)
			return NULL;

		ms_hooker->onAfterNewModuleLoaded (modulesWas);

		return hMod;
	}

	static HMODULE WINAPI myLoadLibraryExA (LPCSTR lpFileName, HANDLE hFile, DWORD dwFlags)
	{
		return onLoadLibraryEx (lpFileName, hFile, dwFlags, false);
	}

	static HMODULE WINAPI myLoadLibraryExW (LPCWSTR lpFileName, HANDLE hFile, DWORD dwFlags)
	{
		return onLoadLibraryEx (lpFileName, hFile, dwFlags, true);
	}

	static HMODULE onLoadLibraryEx (LPCVOID lpFileName, HANDLE hFile, DWORD dwFlags, bool bUnicode)
	{
		typedef HMODULE (WINAPI *FNLoadLibraryEx)(LPCVOID lpFileName, HANDLE hFile, DWORD dwFlags);
		assert (ms_hooker);
		FNLoadLibraryEx pfn = (FNLoadLibraryEx)ms_hooker->getOriginalFunction (bUnicode ? (FARPROC)myLoadLibraryExW : (FARPROC)myLoadLibraryExA);
		assert (pfn != NULL);

		const bool needHook = 0 == (dwFlags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE));

		vmsPeFnHook::loaded_modules_data_t modulesWas;
		if (needHook)
			ms_hooker->onBeforeNewModuleLoaded (modulesWas);

		HMODULE hMod = pfn (lpFileName, hFile, dwFlags);
		if (!hMod)
			return NULL;

		if (needHook)
			ms_hooker->onAfterNewModuleLoaded (modulesWas);

		return hMod;
	}

	static FARPROC WINAPI myGetProcAddress (HMODULE hModule, LPCSTR pszProcName)
	{
		assert (ms_hooker);
		FARPROC pfn = ms_hooker->onGetProcAddress (hModule, pszProcName);
		if (pfn)
		{
			// make sure it's not a madness call
			// (e.g. Firefox make such calls)
			// if not - return hooked fn, if yes (oops) - return the original one to avoid possible crash
			if (stricmp (pszProcName, "GetProcAddress") || hModule != GetModuleHandle (L"kernel32.dll"))
				return pfn;
		}
		typedef FARPROC (WINAPI *FNGetProcAddress)(HMODULE hModule, LPCSTR pszProcName);
		FNGetProcAddress pfnGA = (FNGetProcAddress)ms_hooker->getOriginalFunction ((FARPROC)myGetProcAddress);
		assert (pfnGA != NULL);
		return pfnGA (hModule, pszProcName);
	}
};

__declspec (selectany) std::shared_ptr <vmsPeFnHook> vmsHooksPeCallsForLoadingModules::ms_hooker;

