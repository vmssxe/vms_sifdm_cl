#pragma once

#pragma pack( push )
#pragma pack( 2 )
typedef struct
{
	BYTE	bWidth;               // Width of the image
	BYTE	bHeight;              // Height of the image (times 2)
	BYTE	bColorCount;          // Number of colors in image (0 if >=8bpp)
	BYTE	bReserved;            // Reserved
	WORD	wPlanes;              // Color Planes
	WORD	wBitCount;            // Bits per pixel
	DWORD	dwBytesInRes;         // how many bytes in this resource?
	WORD	nID;                  // the ID
} MEMICONDIRENTRY, * LPMEMICONDIRENTRY;
typedef struct
{
	WORD			idReserved;   // Reserved
	WORD			idType;       // resource type (1 for icons)
	WORD			idCount;      // how many images?
	MEMICONDIRENTRY	idEntries[1]; // the entries for each image
} MEMICONDIR, * LPMEMICONDIR;
#pragma pack( pop )

// These next two structs represent how the icon information is stored
// in an ICO file.
typedef struct
{
	BYTE	bWidth;               // Width of the image
	BYTE	bHeight;              // Height of the image (times 2)
	BYTE	bColorCount;          // Number of colors in image (0 if >=8bpp)
	BYTE	bReserved;            // Reserved
	WORD	wPlanes;              // Color Planes
	WORD	wBitCount;            // Bits per pixel
	DWORD	dwBytesInRes;         // how many bytes in this resource?
	DWORD	dwImageOffset;        // where in the file is this image
} ICONDIRENTRY, * LPICONDIRENTRY;
typedef struct
{
	WORD			idReserved;   // Reserved
	WORD			idType;       // resource type (1 for icons)
	WORD			idCount;      // how many images?
	ICONDIRENTRY	idEntries[1]; // the entries for each image
} ICONDIR, * LPICONDIR;


class CIconExtractor
{

public:

	CIconExtractor() = delete;

	/****************************************************************************
*
*     FUNCTION: ExtractIconFromExe
*
*     PURPOSE:  Extracts the specified icon usualy the application icon
*
*     PARAMS:   LPCSTR      SourceEXE    - the source exe
*               LPCSTR      TargetICON   - name of the icon file
*               UINT        IconIndex    - icon index < MAX_ICONS
*
*     RETURNS:  DWORD - Indicates the error code
*
*     History:
*
*               April 2005 - Created
*
\****************************************************************************/

	static DWORD ExtractIconFromExe(LPCTSTR SourceEXE,
		LPCTSTR TargetICON,
		int IconIndex);

private:
	struct IconResourceInfo {
		ULONG uIconID = 0; //valid if icon name is empty
		std::wstring strIconName;
	};

	struct BufferInMemory {
		LPCVOID buffer = 0;
		DWORD size = 0;
	};

private:
	static DWORD WriteIconToICOFile(LPMEMICONDIR iconDir, const std::vector<BufferInMemory>& iconBuffers, LPCTSTR szFileName);
	static DWORD WriteICOHeader(HANDLE hFile, UINT nNumEntries);
	static DWORD CalculateImageOffset(LPMEMICONDIR iconDir, UINT nIndex);
	static BOOL CALLBACK EnumResNameProc(
		HMODULE hModule,   // module handle
		LPCTSTR lpszType,  // resource type
		LPTSTR lpszName,   // resource name
		LONG_PTR lParam);    // application-defined parameter
};

inline DWORD CIconExtractor::ExtractIconFromExe(LPCTSTR SourceEXE,
	LPCTSTR TargetICON,
	int IconIndex)
{
	HINSTANCE        	hLibrary;
	DWORD               ret;

	if ((hLibrary = LoadLibraryEx(SourceEXE, NULL, LOAD_LIBRARY_AS_DATAFILE)) == NULL)
		return GetLastError();

	HRSRC        	hRsrc = NULL;
	HGLOBAL        	hGlobal = NULL;
	LPMEMICONDIR    iconDir = NULL;

	std::vector <IconResourceInfo> iconsInfos;

	ret = EnumResourceNames(hLibrary, RT_GROUP_ICON, EnumResNameProc, (LONG_PTR)&iconsInfos);

	if (!ret)
	{
		ret = GetLastError();
		FreeLibrary(hLibrary);
		return ret;
	}

	if (iconsInfos.empty())
	{
		FreeLibrary(hLibrary);
		return ERROR_NOT_FOUND;
	}

	if (IconIndex < 0)
	{
		auto id = -IconIndex;

		for (auto i = 0; i < iconsInfos.size(); i++)
		{
			if (iconsInfos[i].strIconName.empty() && iconsInfos[i].uIconID == id)
			{
				IconIndex = i;
				break;
			}
		}

		if (IconIndex < 0)
			IconIndex = 0;
	}
	else
	{
		assert(IconIndex < (int)iconsInfos.size());
		if (IconIndex >= (int)iconsInfos.size())
			IconIndex = 0;
	}

	const auto& iconInfo = iconsInfos[IconIndex];

	hRsrc = FindResource(
		hLibrary,
		iconInfo.strIconName.empty() ? MAKEINTRESOURCE(iconInfo.uIconID) : iconInfo.strIconName.c_str(),
		RT_GROUP_ICON);

	if (hRsrc == NULL)
	{
		ret = GetLastError();
		FreeLibrary(hLibrary);
		return ret;
	}

	if (SizeofResource(hLibrary, hRsrc) < sizeof(MEMICONDIR))
	{
		FreeLibrary(hLibrary);
		return ERROR_INVALID_PARAMETER;
	}

	if ((hGlobal = LoadResource(hLibrary, hRsrc)) == NULL ||
		(iconDir = (LPMEMICONDIR)LockResource(hGlobal)) == NULL)
	{
		ret = GetLastError();
		FreeLibrary(hLibrary);
		return ret;
	}

	if (IsBadReadPtr(iconDir, sizeof(MEMICONDIR)))
	{
		FreeLibrary(hLibrary);
		return ERROR_INVALID_PARAMETER;
	}

	std::vector<BufferInMemory> iconBuffers;
	iconBuffers.reserve(iconDir->idCount);

	// Loop through the images
	for (UINT i = 0; i < iconDir->idCount; i++)
	{
		const auto iconEntry = iconDir->idEntries[i];

		// Get the individual image
		if ((hRsrc = FindResource(hLibrary, MAKEINTRESOURCE(iconEntry.nID), RT_ICON)) == NULL ||
			(hGlobal = LoadResource(hLibrary, hRsrc)) == NULL)
		{
			ret = GetLastError();
			FreeLibrary(hLibrary);
			return ret;
		}

		BufferInMemory b;
		b.size = SizeofResource(hLibrary, hRsrc);
		b.buffer = LockResource(hGlobal);

		assert(b.size == iconEntry.dwBytesInRes);

		iconBuffers.push_back(b);
	}

	ret = WriteIconToICOFile(iconDir, iconBuffers, TargetICON);

	FreeLibrary(hLibrary);

	return (ret);
}


inline DWORD CIconExtractor::WriteIconToICOFile(
	LPMEMICONDIR iconDir,
	const std::vector<BufferInMemory> &iconBuffers,
	LPCTSTR szFileName)
{
	assert(iconDir->idCount == iconBuffers.size());
	if (iconDir->idCount != iconBuffers.size())
		return ERROR_INVALID_PARAMETER;

	HANDLE    	hFile;
	DWORD    	dwBytesWritten;
	DWORD		ret = 0;

	// open the file
	if ((hFile = CreateFile(szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
		return GetLastError();

	// Write the header
	if (auto e = WriteICOHeader(hFile, iconDir->idCount))
	{
		CloseHandle(hFile);
		return e;
	}

	for (UINT i = 0; i < iconDir->idCount; i++)
	{
		const auto& e = iconDir->idEntries[i];

		ICONDIRENTRY    ide = { 0 };

		// Convert internal format to ICONDIRENTRY
		ide.bWidth = e.bWidth;
		ide.bHeight = e.bHeight;
		ide.bReserved = e.bReserved;
		ide.wPlanes = e.wPlanes;
		ide.wBitCount = e.wBitCount;
		ide.bColorCount = e.bColorCount;
		ide.dwBytesInRes = e.dwBytesInRes;
		ide.dwImageOffset = CalculateImageOffset(iconDir, i);

		// Write the ICONDIRENTRY out to disk
		if (!WriteFile(hFile, &ide, sizeof(ICONDIRENTRY), &dwBytesWritten, NULL))
			return GetLastError();

		// Did we write a full ICONDIRENTRY ?
		if (dwBytesWritten != sizeof(ICONDIRENTRY))
			return GetLastError();
	}

	// Write the image bits for each image
	for (const auto &data : iconBuffers)
	{	
		// Write the image bits to file
		if (!WriteFile(hFile, data.buffer, data.size, &dwBytesWritten, NULL) ||
			data.size != dwBytesWritten)
		{
			return GetLastError();
		}
	}

	CloseHandle(hFile);

	return 0;
}

inline DWORD CIconExtractor::WriteICOHeader(HANDLE hFile, UINT nNumEntries)
{
	const auto writeWord = [hFile](WORD w)
		{
			DWORD	dwBytesWritten = 0;
			return WriteFile(hFile, &w, sizeof(WORD), &dwBytesWritten, NULL) &&
				dwBytesWritten == sizeof(WORD);
		};

	if (!writeWord(0) || 
		!writeWord(1) ||
		!writeWord(nNumEntries))
	{
		return GetLastError();
	}

	return 0;
}

inline DWORD CIconExtractor::CalculateImageOffset(
	LPMEMICONDIR iconDir, 
	UINT nIndex)
{
	// Calculate the ICO header size
	DWORD dwSize = 3 * sizeof(WORD);

	// Add the ICONDIRENTRY's
	dwSize += iconDir->idCount * sizeof(ICONDIRENTRY);

	// Add the sizes of the previous images
	for (auto i = 0; i < nIndex; i++)
		dwSize += iconDir->idEntries[i].dwBytesInRes;

	return dwSize;
}

inline BOOL CALLBACK CIconExtractor::EnumResNameProc(
	HMODULE hModule,   // module handle
	LPCTSTR lpszType,  // resource type
	LPTSTR lpszName,   // resource name
	LONG_PTR lParam)    // application-defined parameter
{
	auto* v = (std::vector<IconResourceInfo>*)lParam;

	if (lpszType == RT_GROUP_ICON)
	{
		IconResourceInfo icon;

		if ((ULONG)lpszName < 65536)//ID
			icon.uIconID = (ULONG)lpszName;
		else //NAME
			icon.strIconName = lpszName;

		v->push_back(icon);
	}

	return TRUE;
}
