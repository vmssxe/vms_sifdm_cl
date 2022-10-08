#pragma once

#include <winternl.h>
#include "../fileio/util.h"

class vmsWinUtil
{
public:

	static tstring GetRegisteredExePath (LPCTSTR ptszExeName)
	{
		CRegKey key;
		tstring tstr = _T ("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\");
		tstr += ptszExeName;
		if (ERROR_SUCCESS != key.Open (HKEY_LOCAL_MACHINE, tstr.c_str (), KEY_READ))
		{
			if (ERROR_SUCCESS != key.Open (HKEY_CURRENT_USER, tstr.c_str (), KEY_READ))
				return _T ("");
		}
		TCHAR tsz [MAX_PATH] = _T ("");
		DWORD dw = _countof (tsz);
		key.QueryStringValue (_T ("Path"), tsz, &dw);
		return tsz;
	}


	static std::wstring GetProcessCommandLine (DWORD dwPID)
	{
		typedef NTSTATUS (NTAPI *FNNtQueryInformationProcess) (HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation,
			ULONG ProcessInformationLength, PULONG ReturnLength);

		FNNtQueryInformationProcess pfnQIP = (FNNtQueryInformationProcess) GetProcAddress (
			GetModuleHandle (_T ("ntdll.dll")), "NtQueryInformationProcess");
		if (!pfnQIP)
			return {};

		HANDLE hProcess = OpenProcess (PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwPID);
		if (hProcess == NULL)
			return {};

		PROCESS_BASIC_INFORMATION pbi;
		PEB peb;
		if (pfnQIP (hProcess, ProcessBasicInformation, &pbi, sizeof (pbi), NULL))
		{
			CloseHandle (hProcess);
			return {};
		}

		SIZE_T nSize = 0;

		if (!ReadProcessMemory (hProcess, pbi.PebBaseAddress, &peb, sizeof (peb), &nSize))
		{
			CloseHandle (hProcess);
			return {};
		}

		RTL_USER_PROCESS_PARAMETERS upp;
		if (!ReadProcessMemory (hProcess, peb.ProcessParameters, &upp, sizeof (upp), &nSize))
		{
			CloseHandle (hProcess);
			return {};
		}

		std::wstring path;
		std::wstring cmdline;

		if (upp.ImagePathName.Length > 0)
		{
			path.resize(upp.ImagePathName.Length);
			if (!ReadProcessMemory (hProcess, upp.ImagePathName.Buffer, &path.front(), upp.ImagePathName.Length, &nSize))
			{
				CloseHandle (hProcess);
				return {};
			}
			path.resize(wcslen(path.c_str()));
		}

		if (upp.CommandLine.Length > 0)
		{
			cmdline.resize(upp.CommandLine.Length);
			if (!ReadProcessMemory (hProcess, upp.CommandLine.Buffer, &cmdline.front(), upp.CommandLine.Length, &nSize))
			{
				CloseHandle (hProcess);
				return {};
			}
			cmdline.resize(wcslen(cmdline.c_str()));

			// Windows bug(?) workaround: fix UNC path.
			if (!wcsncmp(cmdline.c_str(), L"\\??\\", 4))
				cmdline[1] = '\\';
		}

		if (!path.empty())
		{
			size_t countTaken = 0;
			auto cmdline0 = getCmdLine1stPhrase(cmdline, path, countTaken);
			auto isCmdline0ImagePath = is1stPhraseImagePath(
				path, cmdline0);

			if (!isCmdline0ImagePath ||  // cmdline without image path
				StrCmpI(path.c_str(), cmdline0.c_str()) ||  // cmdline with a not well formed image path
				cmdline.front() != '"') // cmdline with non-quoted image path
			{
				// we need to rebuild the command line

				if (isCmdline0ImagePath)
					cmdline.erase(cmdline.begin(), cmdline.begin() + countTaken);

				std::wstring cl;
				cl += '"';
				cl += path;
				cl += '"';

				if (!cmdline.empty())
				{
					if (!isspace(cmdline.front()))
						cl += ' ';
					cl += cmdline;
				}

				cmdline.swap(cl);
			}
		}

		CloseHandle (hProcess);

		return cmdline;
	}


	static DWORD GetModuleFileNameEx (HANDLE hProcess, HMODULE hModule, LPTSTR ptszFileName, DWORD nSize)
	{
		typedef DWORD (WINAPI *FNGMFNEX)(HANDLE, HMODULE, LPTSTR, DWORD);
		HMODULE hDll = LoadLibrary (_T ("Psapi.dll"));
		if (hDll == NULL)
			return 0;

#if defined (UNICODE) || defined (_UNICODE)
		FNGMFNEX pfn = (FNGMFNEX) GetProcAddress (hDll, "GetModuleFileNameExW");
#else
		FNGMFNEX pfn = (FNGMFNEX) GetProcAddress (hDll, "GetModuleFileNameExA");
#endif

		DWORD dwRet = 0;

		if (pfn)
			dwRet = pfn (hProcess, hModule, ptszFileName, nSize);

		FreeLibrary (hDll);

		return dwRet;
	}


	// returns true if icon index is specified or false otherwise
	static bool ExtractIconAndId(tstring& tstrIcon, int* piIconIndex)
	{
		if (tstrIcon.empty ())
			return false;

		if (!tstrIcon.empty () && tstrIcon [0] == '"')
		{
			tstrIcon.erase (tstrIcon.begin ());
			size_t n = tstrIcon.find ('"');
			if (n < tstrIcon.length ())
				tstrIcon.erase (tstrIcon.begin () + n);
		}

		LPCTSTR ptsz = _tcsrchr (tstrIcon.c_str (), ',');
		if (ptsz)
		{
			LPCTSTR ptsz2 = ptsz + 1;
			if (*ptsz2 == '-')
				ptsz2++;
			while (*ptsz2 && _istdigit (*ptsz2))
				ptsz2++;
			if (*ptsz2 == 0)
			{
				if (piIconIndex)
					*piIconIndex = _ttoi (ptsz + 1);
				tstrIcon.erase (tstrIcon.end () - (ptsz2 - ptsz), tstrIcon.end ());
				return true; // index is specified
			}
		}

		return false; // index is not specified
	}


	static BOOL GetFileTime (LPCTSTR ptszFile, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime)
	{
		if (lpCreationTime)
			ZeroMemory (lpCreationTime, sizeof (FILETIME));
		if (lpLastAccessTime)
			ZeroMemory (lpLastAccessTime, sizeof (FILETIME));
		if (lpLastWriteTime)
			ZeroMemory (lpLastWriteTime, sizeof (FILETIME));
		HANDLE hFile = CreateFile (ptszFile, GENERIC_READ, 
			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
			return FALSE;
		BOOL bOK = ::GetFileTime (hFile, lpCreationTime, lpLastAccessTime, lpLastWriteTime);
		CloseHandle (hFile);
		return bOK;
	}


	static BOOL IsWow64()
	{
#ifdef _WIN64
		return FALSE;
#else
		typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE hProcess,PBOOL Wow64Process);
		LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)
			GetProcAddress (GetModuleHandle (_T ("kernel32")), "IsWow64Process");
		BOOL bIsWow64 = FALSE;

		if (NULL != fnIsWow64Process)
		{
			if (!fnIsWow64Process (GetCurrentProcess (), &bIsWow64))
			{
				// handle error
			}
		}

		return bIsWow64;
#endif
	}

private:
	static std::wstring getCmdLine1stPhrase(const std::wstring &s, const std::wstring &imagePath, size_t &countTaken)
	{
		countTaken = 0;

		if (s.empty())
			return {};

		std::wstring result;

		auto p = s.c_str();

		if (*p == '"')
		{
			++p; ++countTaken;

			while (*p && *p != '"')
			{
				result += *p++;
				++countTaken;
			}

			if (*p == '"')
				++countTaken;
		}
		else
		{
			// cmd line image path can contain spaces and be specified without quotas...
			// a simple check here, probably needs a better one...
			if (s.length() >= imagePath.length() && 
				!StrCmpNIW(s.c_str(), imagePath.c_str(), imagePath.length()))
			{
				countTaken = imagePath.length();
				return imagePath;
			}

			while (*p && !isspace(*p))
			{
				result += *p++;
				++countTaken;
			}
		}

		return result;
	}

	static bool is1stPhraseImagePath(
		const std::wstring &path, 
		const std::wstring &phrase)
	{
		if (path.empty() || phrase.empty())
			return false;

		// exact match or part of UNC path?
		if (StrStrIW(phrase.c_str(), path.c_str()))
			return true;

		// just name without path?
		{
			auto name = vmsFileNameFromPath(path);
			if (!name.empty())
			{
				if (name.length() == phrase.length())
				{
					if (!StrCmpI(name.c_str(), phrase.c_str()))
						return true;
				}
				else if (name.length() == phrase.length()+4) // phrase is the name without file suffix (e.g. ".exe")?
				{
					if (!StrCmpNIW(name.c_str(), phrase.c_str(), phrase.length()))
						return true;
				}
			}
		}

		// non canonicalized path? 
		if (!wcschr(phrase.c_str(), ':'))
			return false; // it's not a local path at all
		
		std::wstring cp;
		cp.resize(phrase.size() * 3);

		if (PathCanonicalizeW(&cp.front(), phrase.c_str()))
		{
			if (StrStrIW(cp.c_str(), path.c_str()))
				return true;

			// last chance: fix "invalid" slashes

			for (auto &c : cp)
			{
				if (c == '/')
					c = '\\';
			}

			if (StrStrIW(cp.c_str(), path.c_str()))
				return true;
		}

		return false;
	}
};