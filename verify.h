#pragma once
#include <assert.h>

#ifndef _DEBUG
#define dbgverify(x) (x)
#else
#define dbgverify(x) assert(x)
#endif

//TODO: replace all verify in the existing code with dbgverify and remove verify macro
#define verify(x) dbgverify(x)