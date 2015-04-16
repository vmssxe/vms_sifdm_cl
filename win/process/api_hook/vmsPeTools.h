#pragma once
#include "../Toolhelp.h"
#include "../../../../vms_sifdm_cl/win/process/util.h"

#define RvaToAddr(type, base, offset) ((type)(DWORD_PTR(base) + DWORD_PTR(offset)))
class vmsPeTools
{
public:
	static bool ReplaceEATfunc(HMODULE hTarget, FARPROC pfnTarget, FARPROC pfnNew)
	{
		PIMAGE_DOS_HEADER pDosHeader = PIMAGE_DOS_HEADER (hTarget);
		PIMAGE_NT_HEADERS pNTHeaders = RvaToAddr (PIMAGE_NT_HEADERS, hTarget, pDosHeader->e_lfanew);

		IMAGE_DATA_DIRECTORY &expDir =
			pNTHeaders->OptionalHeader.DataDirectory [IMAGE_DIRECTORY_ENTRY_EXPORT];

		if (expDir.VirtualAddress == NULL || expDir.Size == 0)
			return false;

		PIMAGE_EXPORT_DIRECTORY pExportDir = RvaToAddr (PIMAGE_EXPORT_DIRECTORY, hTarget, expDir.VirtualAddress);

		//LPCSTR pszModName = RvaToAddr (LPCSTR, hTarget, pExportDir->Name);

		LPDWORD pdwFuncAddrs = RvaToAddr (LPDWORD, hTarget, pExportDir->AddressOfFunctions);

		for (DWORD i = 0; i < pExportDir->NumberOfFunctions; i++)
		{
			FARPROC pfn = RvaToAddr (FARPROC, hTarget, pdwFuncAddrs [i]);
			if (pfn == pfnTarget)
			{
				DWORD dwDummy;
				if (!VirtualProtect (pdwFuncAddrs + i, sizeof (DWORD), PAGE_EXECUTE_READWRITE, &dwDummy))
					return false;
				DWORD dwNewRva = (DWORD)(DWORD_PTR (pfnNew) - DWORD_PTR (hTarget));
				if (!WriteProcessMemory (GetCurrentProcess (), pdwFuncAddrs + i, &dwNewRva, sizeof (DWORD), NULL))
					return false;
				VirtualProtect (pdwFuncAddrs + i, sizeof (DWORD), dwDummy, &dwDummy);
				return true;
			}
		}

		return false;
	}

	// either pfnTarget or pszTargetFuncName must not be NULL
	// ppfnOriginal, out: original address of the function
	// pfnTarget - can be NULL. It's useful in case of dummy DLL redirectors.
	bool ReplaceIATfunc (HMODULE hTarget, LPCSTR pszImportingModuleName, LPCSTR pszTargetFuncName, FARPROC pfnTarget, FARPROC pfnNew, FARPROC *ppfnOriginal)
	{
		PIMAGE_DOS_HEADER pDosHeader = PIMAGE_DOS_HEADER (hTarget);
		PIMAGE_NT_HEADERS pNTHeaders = RvaToAddr (PIMAGE_NT_HEADERS, hTarget, pDosHeader->e_lfanew);

		IMAGE_DATA_DIRECTORY &impDir = pNTHeaders->OptionalHeader.DataDirectory [IMAGE_DIRECTORY_ENTRY_IMPORT];

		if (impDir.VirtualAddress == NULL || impDir.Size == 0)
			return false;

	_lTry:

		PIMAGE_IMPORT_DESCRIPTOR pImportDesc = RvaToAddr (PIMAGE_IMPORT_DESCRIPTOR, hTarget, impDir.VirtualAddress);
		bool bHaveNullOriginalThunk = false;

		bool bReplaced = false;

		CachedIatFuncNames *pFuncNames = NULL;
		if (!pImportDesc->OriginalFirstThunk)
		{
			for (size_t i = 0; i < m_vIatFuncNames.size (); i++)
			{
				if (m_vIatFuncNames [i].hModule == hTarget)
					pFuncNames = &m_vIatFuncNames [i];
			}
		}

		FARPROC pfnOriginal = NULL;

		for (int iImportingModule = 0; pImportDesc->Name; pImportDesc++, iImportingModule++)
		{
			LPCSTR pszModName = RvaToAddr (LPCSTR, hTarget, pImportDesc->Name);

			// we will replace IAT entries for module different from pszImportingModuleName in case
			// the function address of entry points to the target function
			// but, of course, we can't search by name if IAT's module name is not the target module name
			bool bSearchOnlyByAddr = _stricmp (pszModName, pszImportingModuleName) != 0;

			PIMAGE_THUNK_DATA pThunk = RvaToAddr (PIMAGE_THUNK_DATA, hTarget, pImportDesc->FirstThunk);
			PIMAGE_THUNK_DATA pOriginalThunk = pImportDesc->OriginalFirstThunk ? RvaToAddr (PIMAGE_THUNK_DATA, hTarget, pImportDesc->OriginalFirstThunk) : NULL;

			if (!bSearchOnlyByAddr)
			{
				if (!pImportDesc->OriginalFirstThunk && !pFuncNames)
					bHaveNullOriginalThunk = true;
			}

			for (int iFunction = 0; pThunk->u1.Function; pThunk++, iFunction++)
			{
				bool bNeedToReplace = false;

				if (!bSearchOnlyByAddr)
				{
					if (pszTargetFuncName && (pOriginalThunk || pFuncNames))
					{
						if (pOriginalThunk)
						{
							if (!(pOriginalThunk->u1.AddressOfData & IMAGE_ORDINAL_FLAG))
							{
								PIMAGE_IMPORT_BY_NAME pName = RvaToAddr (PIMAGE_IMPORT_BY_NAME, hTarget, pOriginalThunk->u1.AddressOfData);
								assert (pName != NULL);
								//LOG ("  module's function: %s", (LPCSTR)pName->Name);
								if (!_stricmp ((LPCSTR)pName->Name, pszTargetFuncName))
									bNeedToReplace = true;
							}

							pOriginalThunk++;
						}
						else
						{
							//LOG ("  module's function: %s", pFuncNames->vModules [iImportingModule][iFunction].c_str ());
							if (!_stricmp (pszTargetFuncName, pFuncNames->vModules [iImportingModule] [iFunction].c_str ()))
								bNeedToReplace = true;
						}
					}
				}

				if (!bNeedToReplace && pfnTarget && pThunk->u1.Function == DWORD_PTR (pfnTarget))
					bNeedToReplace = true;

				if (bNeedToReplace)
				{
					pfnOriginal = (FARPROC)pThunk->u1.Function;
					if (ppfnOriginal)
						*ppfnOriginal = pfnOriginal;
					PROC* ppfn = (PROC*)&pThunk->u1.Function;
					DWORD dwDummy;
					VirtualProtect (ppfn, sizeof (PROC), PAGE_EXECUTE_READWRITE, &dwDummy);
					BOOL bOK = WriteProcessMemory (GetCurrentProcess (), ppfn, &pfnNew, sizeof (PROC), NULL);
					VirtualProtect (ppfn, sizeof (PROC), dwDummy, &dwDummy);
					if (!bOK)
						return false;
					bReplaced = true;
				}
			}
		}

		if (!bReplaced && bHaveNullOriginalThunk && pszTargetFuncName)
		{
			assert (pFuncNames == NULL);
			if (pFuncNames == NULL)
				if (LoadModuleIATOriginalFirstThunk (hTarget))
					goto _lTry;
		}

		return bReplaced;
	}

	static FARPROC GetProcAddress(HMODULE hModule, LPCSTR pszProcName)
	{
		PIMAGE_DOS_HEADER pDosHeader = PIMAGE_DOS_HEADER (hModule);
		PIMAGE_NT_HEADERS pNTHeaders = RvaToAddr (PIMAGE_NT_HEADERS, hModule, pDosHeader->e_lfanew);

		IMAGE_DATA_DIRECTORY &expDir =
			pNTHeaders->OptionalHeader.DataDirectory [IMAGE_DIRECTORY_ENTRY_EXPORT];

		if (expDir.VirtualAddress == NULL || expDir.Size == 0)
			return NULL;

		PIMAGE_EXPORT_DIRECTORY pExportDir = RvaToAddr (PIMAGE_EXPORT_DIRECTORY, hModule, expDir.VirtualAddress);

		LPDWORD pdwFuncNames = RvaToAddr (LPDWORD, hModule, pExportDir->AddressOfNames);

		for (DWORD i = 0; i < pExportDir->NumberOfNames; i++)
		{
			LPCSTR pszName = RvaToAddr (LPCSTR, hModule, pdwFuncNames [i]);
			if (!_stricmp (pszName, pszProcName))
			{
				LPWORD pwOrdinals = RvaToAddr (LPWORD, hModule, pExportDir->AddressOfNameOrdinals);
				LPDWORD pdwFuncAddrs = RvaToAddr (LPDWORD, hModule, pExportDir->AddressOfFunctions);
				return RvaToAddr (FARPROC, hModule, pdwFuncAddrs [pwOrdinals [i]]);
			}
		}

		return NULL;
	}

protected:
	// some modules does not have original thunk specified. In such a case we need to load it from disk (and cache in memory for further use)
	bool LoadModuleIATOriginalFirstThunk(HMODULE hModule)
	{
		CachedIatFuncNames cfn;
		cfn.hModule = hModule;

		TCHAR tsz [MAX_PATH];
		GetModuleFileName (hModule, tsz, _countof (tsz));

		HANDLE hFile = CreateFile (tsz, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
			return false;

		IMAGE_DOS_HEADER dosHdr;
		DWORD dw;
		if (!ReadFile (hFile, &dosHdr, sizeof (dosHdr), &dw, NULL))
			goto _lErr;

		SetFilePointer (hFile, dosHdr.e_lfanew, NULL, FILE_BEGIN);

		IMAGE_NT_HEADERS ntHdr;
		if (!ReadFile (hFile, &ntHdr, sizeof (ntHdr), &dw, NULL))
			goto _lErr;

		IMAGE_DATA_DIRECTORY &impDir = ntHdr.OptionalHeader.DataDirectory [IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (impDir.VirtualAddress == NULL)
			goto _lErr;

		IMAGE_SECTION_HEADER sectHdr;
		bool bFoundImpSection = false;

		for (int i = 0; i < ntHdr.FileHeader.NumberOfSections; i++)
		{
			if (!ReadFile (hFile, &sectHdr, sizeof (sectHdr), &dw, NULL))
				goto _lErr;

			if (!sectHdr.SizeOfRawData)
				continue;

			// one section can contain several data directories (e.g. UPX does so)
			if (sectHdr.VirtualAddress <= impDir.VirtualAddress &&
				sectHdr.VirtualAddress + sectHdr.SizeOfRawData > impDir.VirtualAddress)
			{
				bFoundImpSection = true;
				break;
			}
		}

		if (!bFoundImpSection)
			goto _lErr;

		int iImpDirOffset = impDir.VirtualAddress - sectHdr.VirtualAddress;

		LPBYTE pbIAT = new BYTE [impDir.Size];
		SetFilePointer (hFile, sectHdr.PointerToRawData + iImpDirOffset, NULL, FILE_BEGIN);

		if (!ReadFile (hFile, pbIAT, impDir.Size, &dw, NULL))
			goto _lErr;

		CloseHandle (hFile);

		PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)pbIAT;

		#define IatRvaToAddr(type, rva) ((type)(pbIAT + DWORD_PTR (rva) - impDir.VirtualAddress))

		for (; pImportDesc->Name; pImportDesc++)
		{
			//LPCSTR pszModName = IatRvaToAddr (LPCSTR, pImportDesc->Name);

			std::vector <std::string> vFuncNames;

			if (pImportDesc->Name)
			{
				PIMAGE_THUNK_DATA pThunk = IatRvaToAddr (PIMAGE_THUNK_DATA, pImportDesc->FirstThunk);

				for (; pThunk->u1.Function; pThunk++)
				{
					if (!(pThunk->u1.AddressOfData & IMAGE_ORDINAL_FLAG))
					{
						PIMAGE_IMPORT_BY_NAME pName = IatRvaToAddr (PIMAGE_IMPORT_BY_NAME, pThunk->u1.AddressOfData);
						assert (pName != NULL);
						vFuncNames.push_back ((LPCSTR)pName->Name);
					}
					else
					{
						vFuncNames.push_back ("");
					}
				}
			}

			cfn.vModules.push_back (vFuncNames);
		}

		m_vIatFuncNames.push_back (cfn);

		return true;

	_lErr:
		CloseHandle (hFile);
		return false;
	}

protected:
	struct CachedIatFuncNames
	{
		HMODULE hModule;
		// array of symbol names for the each module hModule imports
		std::vector <std::vector <std::string> > vModules;
	};
	std::vector <CachedIatFuncNames> m_vIatFuncNames;

public:
	void ReplaceIATfuncInAllModules(LPCSTR pszImportingModuleName, LPCSTR pszTargetFuncName, FARPROC pfnNew)
	{
		HMODULE hMainMod = GetModuleHandle (NULL);
		HMODULE hThisMod = vmsGetThisModuleHandle ();

		FARPROC pfnTarget = GetProcAddress (GetModuleHandleA (pszImportingModuleName), pszTargetFuncName);

		FARPROC pfnOrig = NULL;
		ReplaceIATfunc (hMainMod, pszImportingModuleName, pszTargetFuncName, pfnTarget, pfnNew, &pfnOrig);

		// Get the list of modules in this process
		CToolhelp th (TH32CS_SNAPMODULE, GetCurrentProcessId ());

		MODULEENTRY32 me = { sizeof (me) };
		for (BOOL bOk = th.ModuleFirst (&me); bOk; bOk = th.ModuleNext (&me))
		{
			if (me.hModule != hThisMod && me.hModule != hMainMod)
				ReplaceIATfunc (me.hModule, pszImportingModuleName, pszTargetFuncName, pfnTarget, pfnNew, &pfnOrig);
		}
	}
};

