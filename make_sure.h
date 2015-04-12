#pragma once

// crash the app if eval is false
#define make_sure(eval) {assert (eval); if (!(eval)) {char*p=0;++p;*p=1;}}