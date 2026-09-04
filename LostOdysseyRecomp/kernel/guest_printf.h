#pragma once

#include <string>

// printf-style formatting driven by guest varargs: integer arguments come from
// r3.. r10 then the guest stack, doubles from f1.. f13. `firstArg` is the index
// (in integer-argument order) of the first vararg. Handles the subset of
// specifiers the Xbox 360 CRT exports (%d %i %u %x %X %o %c %s %S %p %f %g %e
// %ld %lld %I64d %hs %ls %%) with flags, width and precision.
std::string GuestFormat(PPCContext& ctx, uint8_t* base, const char* format, size_t firstArg, bool wideFormat = false);

// Variant that reads the arguments from a guest va_list (pointer to the
// register save area written by the caller's prologue), as used by vsnprintf.
std::string GuestFormatVaList(uint8_t* base, const char* format, uint32_t vaList, bool wideFormat = false);
