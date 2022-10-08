#pragma once

#include <unordered_set>
#include <string>
#include <shlwapi.h>

struct WstringNoCaseHash
{
    size_t operator()(const std::wstring &s) const
    {
        size_t h = 0;
        std::for_each(s.begin(), s.end(), [&](wchar_t c)
        {
            h += towlower(c);
        });
        return h;
    }
};

struct WstringNoCaseEqual
{
    bool operator()(const std::wstring &s1, const std::wstring &s2) const
    {
        return s1.size() == s2.size() && 
			!StrCmpIW(s1.c_str(), s2.c_str());
    }
};

using wstring_nocase_unordered_set = std::unordered_set<std::wstring, WstringNoCaseHash, WstringNoCaseEqual>;