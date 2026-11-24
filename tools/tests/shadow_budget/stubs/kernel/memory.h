#pragma once
#include <cstddef>
struct Memory { void* Translate(size_t address) const; };
extern Memory g_memory;
