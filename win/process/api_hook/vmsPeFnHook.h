#pragma once
#include "vmsPeTools.h"
class vmsPeFnHook
{
public:
	bool HookFunction(LPCSTR pszImportingModuleName, LPCSTR pszTargetFuncName, FARPROC pfnNew)
	{
		assert (pszImportingModuleName);
		assert (pszTargetFuncName);
		assert (pfnNew);

		if (findFunctionIndexByName (pszImportingModuleName, pszTargetFuncName) != -1)
			return true;

		HMODULE hMainModule = GetModuleHandle (NULL);
		HMODULE hThisModule = vmsGetThisModuleHandle ();

		FunctionInfo fi;
		fi.strImportingModuleName = pszImportingModuleName;
		fi.hImportingModule = GetModuleHandleA (pszImportingModuleName);
		assert (fi.hImportingModule != NULL);
		if (!fi.hImportingModule)
			return false;
		fi.strFunctionName = pszTargetFuncName;
		fi.pfnHook = pfnNew;

		vmsAUTOLOCKSECTION (m_csFunctions); // lock here for hook function to get a proper address of original fn
		m_vFunctions.push_back (fi);

		FunctionInfo &r_fi = m_vFunctions [m_vFunctions.size () - 1];

		FARPROC pfnTarget = vmsPeTools::GetProcAddress (r_fi.hImportingModule, pszTargetFuncName);
		FARPROC pfnOriginal = NULL;
		bool replaced = false;

		if (m_petools.ReplaceIATfunc (hMainModule, pszImportingModuleName, pszTargetFuncName, pfnTarget, pfnNew, &r_fi.pfnOriginal))
			replaced = true;
		if (m_petools.ReplaceDIATfunc (hMainModule, pszImportingModuleName, pszTargetFuncName, pfnTarget, pfnNew, &pfnOriginal))
			replaced = true;

		if (!r_fi.pfnOriginal)
			r_fi.pfnOriginal = pfnTarget;

		vmsAUTOLOCKSECTION_UNLOCK (m_csFunctions);

		// Get the list of modules in this process
		CToolhelp th (TH32CS_SNAPMODULE, GetCurrentProcessId ());

		MODULEENTRY32 me = { sizeof (me) };
		for (BOOL bOk = th.ModuleFirst (&me); bOk; bOk = th.ModuleNext (&me))
		{
			if (me.hModule != hThisModule && me.hModule != hMainModule)
			{
				if (m_petools.ReplaceIATfunc (me.hModule, pszImportingModuleName, pszTargetFuncName, pfnTarget, pfnNew, &pfnOriginal))
					replaced = true;
				if (m_petools.ReplaceDIATfunc (me.hModule, pszImportingModuleName, pszTargetFuncName, pfnTarget, pfnNew, &pfnOriginal))
					replaced = true;
				//assert (r_fi.pfnOriginal == NULL || pfnOriginal == NULL || pfnOriginal == r_fi.pfnOriginal); // it can differ btw... but get some notification just in case...
			}
		}

		return replaced;
	}

protected:
	struct FunctionInfo
	{
		std::string strImportingModuleName;
		HMODULE hImportingModule;
		std::string strFunctionName;
		FARPROC pfnHook;
		FARPROC pfnOriginal;
		FunctionInfo () : pfnHook (NULL), pfnOriginal (NULL), hImportingModule (NULL) {}
	};
	std::vector <FunctionInfo> m_vFunctions;
	vmsCriticalSection m_csFunctions;

protected:
	SSIZE_T findFunctionIndexByName(LPCSTR pszImportingModuleName, LPCSTR pszFuncName)
	{
		assert (pszImportingModuleName != NULL && pszFuncName != NULL);
		if (!pszImportingModuleName || !pszFuncName)
			return -1;

		for (size_t i = 0; i < m_vFunctions.size (); i++)
		{
			FunctionInfo &fi = m_vFunctions [i];
			if (!_stricmp (pszImportingModuleName, fi.strImportingModuleName.c_str ()) &&
				!_stricmp (pszFuncName, fi.strFunctionName.c_str ()))
				return i;
		}

		return -1;
	}

	SSIZE_T findFunctionIndexByName(HMODULE hImportingModule, LPCSTR pszFuncName)
	{
		assert (hImportingModule != NULL && pszFuncName != NULL);
		if (!hImportingModule || !pszFuncName)
			return -1;

		for (size_t i = 0; i < m_vFunctions.size (); i++)
		{
			FunctionInfo &fi = m_vFunctions [i];
			if (hImportingModule == fi.hImportingModule && !_stricmp (pszFuncName, fi.strFunctionName.c_str ()))
				return i;
		}

		return -1;
	}

	SSIZE_T findFunctionIndexByHookFnAddr(FARPROC pfnHook)
	{
		for (size_t i = 0; i < m_vFunctions.size (); i++)
		{
			if (m_vFunctions [i].pfnHook == pfnHook)
				return i;
		}

		return -1;
	}

protected:
	vmsPeTools m_petools;

public:
	FARPROC getOriginalFunction(FARPROC pfnHook)
	{
		vmsAUTOLOCKSECTION (m_csFunctions);
		SSIZE_T nIndex = findFunctionIndexByHookFnAddr (pfnHook);
		if (nIndex == -1)
			return NULL;
		return m_vFunctions [nIndex].pfnOriginal;
	}

	// can be called to hook all functions in the new loaded module
	void onNewModuleLoaded(HMODULE hModule)
	{
		vmsAUTOLOCKSECTION (m_csFunctions);
		std::vector <FunctionInfo> vFunctions = m_vFunctions;
		vmsAUTOLOCKSECTION_UNLOCK (m_csFunctions);

		for (size_t i = 0; i < vFunctions.size (); i++)
		{
			const FunctionInfo &fi = vFunctions [i];
			FARPROC pfnOriginal = NULL;
			FARPROC pfnTarget = GetProcAddress (GetModuleHandleA (fi.strImportingModuleName.c_str ()), fi.strFunctionName.c_str ());
			m_petools.ReplaceIATfunc (hModule, fi.strImportingModuleName.c_str (), fi.strFunctionName.c_str (), pfnTarget,
				fi.pfnHook, &pfnOriginal);
		}
	}

	typedef std::set <HMODULE> loaded_modules_data_t;

	// can be called to hook all functions in the new loaded module
	// supports modules with additional dependencies

	void onBeforeNewModuleLoaded (loaded_modules_data_t &data)
	{
		get_loaded_modules_data (data);
	}

	void onAfterNewModuleLoaded (const loaded_modules_data_t &data)
	{
		loaded_modules_data_t dataNow;
		get_loaded_modules_data (dataNow);

		for (auto it = dataNow.begin (); it != dataNow.end (); ++it)
		{
			if (data.find (*it) == data.end ())
				onNewModuleLoaded (*it);
		}
	}

	// returns NULL if the function was not hooked or address of the hook function otherwise
	FARPROC onGetProcAddress(HMODULE hModule, LPCSTR pszFuncName)
	{
		if (DWORD_PTR (pszFuncName) <= _UI16_MAX)
			return NULL;
		vmsAUTOLOCKSECTION (m_csFunctions);
		auto nIndex = findFunctionIndexByName (hModule, pszFuncName);
		return nIndex != -1 ? m_vFunctions [nIndex].pfnHook : NULL;
	}

	void RemoveAllHooks(void)
	{
		vmsAUTOLOCKSECTION (m_csFunctions);
		for (size_t i = 0; i < m_vFunctions.size (); i++)
			RemoveHook (m_vFunctions [i]);
		m_vFunctions.clear ();
	}

protected:
	void RemoveHook(FunctionInfo& fi)
	{
		HMODULE hThisModule = vmsGetThisModuleHandle ();

		// Get the list of modules in this process
		CToolhelp th (TH32CS_SNAPMODULE, GetCurrentProcessId ());

		MODULEENTRY32 me = { sizeof (me) };
		for (BOOL bOk = th.ModuleFirst (&me); bOk; bOk = th.ModuleNext (&me))
		{
			if (me.hModule != hThisModule)
			{
				FARPROC pfn = NULL;
				m_petools.ReplaceIATfunc (me.hModule, fi.strImportingModuleName.c_str (),
					fi.strFunctionName.c_str (), fi.pfnHook, fi.pfnOriginal, &pfn);
			}
		}
	}

	void get_loaded_modules_data (loaded_modules_data_t &data)
	{
		data.clear ();
		CToolhelp th (TH32CS_SNAPMODULE, GetCurrentProcessId ());
		MODULEENTRY32 me = { sizeof (me) };
		for (BOOL bOk = th.ModuleFirst (&me); bOk; bOk = th.ModuleNext (&me)) 
			data.insert (me.hModule);
	}
};

