#pragma once

// crash the app if eval is false
#define make_sure(eval) {bool result = static_cast<bool>(eval); assert (result); if (!(result)) {char*p=0;++p;*p=1;}}