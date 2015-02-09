#pragma once

#ifndef tstring

#include <string>

#if defined (UNICODE) || defined (_UNICODE)
#define tstring std::wstring
#else
#define tstring std::string
#endif

#endif // tstring

#ifndef tstringstream

#include <sstream>

#if defined (UNICODE) || defined (_UNICODE)
#define tstringstream std::wstringstream
#else
#define tstringstream std::stringstream
#endif

#endif // tstring
