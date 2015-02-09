#pragma once
#include <atlbase.h>

inline char * __cdecl strstri (const char * str1, const char * str2)
{
	char *cp = (char *) str1;
	char *s1, *s2;

	if ( !*str2 )
		return((char *)str1);

	while (*cp)
	{
		s1 = cp;
		s2 = (char *) str2;

		while ( *s1 && *s2 && !(tolower (*s1)-tolower (*s2)) )
			s1++, s2++;

		if (!*s2)
			return(cp);

		cp++;
	}

	return(NULL);
}

inline wchar_t * __cdecl strstri (const wchar_t * str1, const wchar_t * str2)
{
	wchar_t *cp = (wchar_t *) str1;
	wchar_t *s1, *s2;

	if ( !*str2 )
		return((wchar_t *)str1);

	while (*cp)
	{
		s1 = cp;
		s2 = (wchar_t *) str2;

		while ( *s1 && *s2 && !(towlower (*s1)-towlower (*s2)) )
			s1++, s2++;

		if (!*s2)
			return(cp);

		cp++;
	}

	return(NULL);

}

template<class T> inline void stringReplace (T& str, const T& strSearch, const T& strReplace)
{
	const T::size_type str2Size (strReplace.size());
	const T::size_type str1Size (strSearch.size());
	T::size_type n = 0;
	while (T::npos != (n = str.find (strSearch, n))) 
	{
		str.replace (n, str1Size, strReplace);
		n += str2Size;
	}
}

template <class tString> 
size_t stringFindI (const tString& str, const tString& strWhat, size_t nPos = 0)
{
	assert (nPos != tString::npos);
	if (nPos == tString::npos)
		return tString::npos;
	assert (nPos < str.length ());
	if (nPos >= str.length ())
		return tString::npos;
	auto res = strstri (str.c_str () + nPos, strWhat.c_str ());
	if (!res)
		return tString::npos;
	return res - str.c_str ();
}

#ifdef tstring
#define tstringReplace stringReplace<tstring>
#endif

inline std::wstring wideFromUtf8 (const std::string& str)
{
	return (wchar_t*) ATL::CA2WEX <1000> (str.c_str (), CP_UTF8);
}

inline std::string utf8FromWide (const std::wstring& wstr)
{
	return (char*) ATL::CW2AEX <1000> (wstr.c_str (), CP_UTF8);
}

template <class tString>
inline int stringCompareN (const tString& str, typename tString::size_type pos, const tString& str2)
{
	return str.compare (pos, str2.length (), str2);
}

template <class tString>
inline void word_wrap_string (tString& str, int line_size = 100)
{
	for (size_t i = line_size; i < str.size (); i += line_size)
	{
		const unsigned max_back = 20;
		for (unsigned backed = 0; !isspace (str [i]) && backed < max_back; ++backed, --i) {}
		if (!isspace (str [i]))
		{
			i += max_back;
			str.insert (str.begin () + i, '\n');
		}
		else
		{
			str [i] = '\n';
		}
	}
}