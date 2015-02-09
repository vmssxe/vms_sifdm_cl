#pragma once

#define make_sure(eval) {assert (eval); if (!(eval)) throw std::exception ();}