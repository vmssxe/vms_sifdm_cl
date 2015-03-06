#pragma once
#include "unzip.h"
#include "errors/unzip_error.h"
class vmsUnZip  
{
public:
	static BOOL Unpack(LPCTSTR ptszFileName, LPCTSTR ptszDstFolder, vmsError& error)
	{
		USES_CONVERSION;

		error.clear();

		BOOL result = FALSE;

		unzFile zipFile = unzOpen(T2A((LPTSTR)ptszFileName));

		if (NULL != zipFile)
		{
			if (UNZ_OK == unzGoToFirstFile(zipFile))
			{
				BOOL bContinue = TRUE;
				while (bContinue)
				{
					result = FALSE;
					unz_file_info fi;
					char filename[MAX_PATH] = { 0 };
					if (UNZ_OK == unzGetCurrentFileInfo(zipFile, &fi,
						filename, sizeof(filename), 0, 0, 0, 0))
					{
						if (UNZ_OK == unzOpenCurrentFile(zipFile))
						{
							UINT dataLen = fi.uncompressed_size;
							BYTE* fileData = new BYTE[dataLen];
							if (!fileData)
								break;
							if (dataLen == unzReadCurrentFile(zipFile, fileData, dataLen))
							{
								char filePathName[MAX_PATH] = { 0 };
								strcat(filePathName, T2A((LPTSTR)ptszDstFolder));
								strcat(filePathName, "\\");
								strcat(filePathName, filename);
								FILE* pFile = fopen(filePathName, "wb");
								if (pFile)
								{
									result = (dataLen == fwrite(fileData, 1, dataLen, pFile));
									result = result && (0 == fclose(pFile));
								}
								else
								{
									result = FALSE;
								}

								if (!result)
									error = std::error_code(errno, std::generic_category());
							}
							delete[] fileData;
						}
						result = result && (UNZ_OK == unzCloseCurrentFile(zipFile));
					}
					if (!result)
						break;
					if (UNZ_END_OF_LIST_OF_FILE == unzGoToNextFile(zipFile))
						bContinue = FALSE;
				}
			}
			result = result && (UNZ_OK == unzClose(zipFile));
		}

		if (!result && !error)
			error = unzip_error::unknown_error;

		return result;
	}
};
