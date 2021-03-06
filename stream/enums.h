#pragma once

// make enum easy serializable

/* example:
enum eTEST = {E1, E2, E3};
ENUM_STREAM_SUPPORT_BEGIN (eTEST)
	ENUM_STREAM_SUPPORT_ENTRY (E1, L"E1")
	ENUM_STREAM_SUPPORT_ENTRY (E2, L"E2")
	ENUM_STREAM_SUPPORT_ENTRY (E3, L"E3")
ENUM_STREAM_SUPPORT_END (eTEST)
*/

#define ENUM_STREAM_SUPPORT_BEGIN(ENUM) __declspec(selectany) std::pair <ENUM, std::wstring> g_ssm_##ENUM [] = {
#define ENUM_STREAM_SUPPORT_ENTRY(val, str) std::make_pair (val, str),
#define ENUM_STREAM_SUPPORT_END(ENUM) };	\
template <class TStream>					\
TStream& operator<<(TStream& stm, ENUM en)	\
{	\
	for (size_t i = 0; i < _countof (g_ssm_##ENUM); i++)		\
	{	\
		if (g_ssm_##ENUM [i].first == en)		\
		{	\
			stm << g_ssm_##ENUM [i].second;	\
			return stm;	\
		}	\
	}	\
	stm.setstate (stm.failbit);	\
	return stm;	\
}	\
template <class TStream>	\
TStream& operator>>(TStream& stm, ENUM& en)	\
{	\
	std::wstring wstr;	\
	stm >> wstr;	\
	for (size_t i = 0; i < _countof (g_ssm_##ENUM); i++)	\
	{	\
		if (g_ssm_##ENUM [i].second == wstr)	\
		{	\
			en = g_ssm_##ENUM [i].first;	\
			return stm;	\
		}	\
	}	\
	stm.setstate (stm.failbit);	\
	return stm;	\
}