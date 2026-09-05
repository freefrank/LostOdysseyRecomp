#include <stdafx.h>
#include "guest_printf.h"
#include "function.h"

namespace
{
    struct ArgSource
    {
        virtual ~ArgSource() {}
        virtual uint64_t NextInt() = 0;
        virtual double NextDouble() = 0;
    };

    // Registers first, then the caller's stack (0x54 + 8*n like ArgTranslator).
    struct RegisterArgSource : ArgSource
    {
        PPCContext& ctx;
        uint8_t* base;
        size_t intIndex;
        size_t floatIndex = 0;

        RegisterArgSource(PPCContext& ctx, uint8_t* base, size_t firstArg) : ctx(ctx), base(base), intIndex(firstArg) {}

        uint64_t NextInt() override
        {
            return ArgTranslator::GetIntegerArgumentValue(ctx, base, intIndex++);
        }

        double NextDouble() override
        {
            // Varargs doubles are also copied into GPRs by the PPC ABI; the
            // integer slot is consumed as well so the ordering stays right.
            double d = ArgTranslator::GetPrecisionArgumentValue(ctx, base, floatIndex++);
            intIndex++;
            return d;
        }
    };

    // va_list on Xenon: pointer to 8-byte slots in the caller's home area.
    struct VaListArgSource : ArgSource
    {
        uint8_t* base;
        uint32_t ptr;

        VaListArgSource(uint8_t* base, uint32_t vaList) : base(base), ptr(vaList) {}

        uint64_t NextInt() override
        {
            uint64_t v = ByteSwap(*reinterpret_cast<uint64_t*>(base + ptr));
            ptr += 8;
            return v;
        }

        double NextDouble() override
        {
            uint64_t bits = NextInt();
            double d;
            memcpy(&d, &bits, sizeof(d));
            return d;
        }
    };

    std::string Format(uint8_t* base, const char* format, ArgSource& args, bool wideFormat)
    {
        std::string out;
        char spec[32];
        char buf[512];

        for (const char* p = format; *p; ++p)
        {
            if (*p != '%')
            {
                out += *p;
                continue;
            }

            const char* start = p++;
            if (*p == '%')
            {
                out += '%';
                continue;
            }

            // flags, width, precision
            std::string flags;
            while (*p && strchr("-+ #0", *p)) flags += *p++;
            std::string width;
            if (*p == '*') { width = std::to_string((int32_t)args.NextInt()); p++; }
            else while (isdigit((unsigned char)*p)) width += *p++;
            std::string precision;
            if (*p == '.')
            {
                p++;
                precision = ".";
                if (*p == '*') { precision += std::to_string((int32_t)args.NextInt()); p++; }
                else while (isdigit((unsigned char)*p)) precision += *p++;
            }

            // length modifiers
            bool isLongLong = false, isWide = false, isShort = false;
            if (*p == 'I' && p[1] == '6' && p[2] == '4') { isLongLong = true; p += 3; }
            else if (*p == 'l' && p[1] == 'l') { isLongLong = true; p += 2; }
            else if (*p == 'l') { isWide = true; p++; }
            else if (*p == 'h') { isShort = true; p++; }
            else if (*p == 'w') { isWide = true; p++; }
            else if (*p == 'z' || *p == 't') { p++; }

            char conv = *p;
            if (!conv) break;

            switch (conv)
            {
            case 'd': case 'i':
            {
                int64_t v = isLongLong ? (int64_t)args.NextInt() : (int64_t)(int32_t)args.NextInt();
                snprintf(spec, sizeof(spec), "%%%s%s%slld", flags.c_str(), width.c_str(), precision.c_str());
                snprintf(buf, sizeof(buf), spec, (long long)v);
                out += buf;
                break;
            }
            case 'u': case 'x': case 'X': case 'o':
            {
                uint64_t v = isLongLong ? args.NextInt() : (uint32_t)args.NextInt();
                snprintf(spec, sizeof(spec), "%%%s%s%sll%c", flags.c_str(), width.c_str(), precision.c_str(), conv);
                snprintf(buf, sizeof(buf), spec, (unsigned long long)v);
                out += buf;
                break;
            }
            case 'p':
            {
                snprintf(buf, sizeof(buf), "%08X", (uint32_t)args.NextInt());
                out += buf;
                break;
            }
            case 'c':
            {
                uint32_t v = (uint32_t)args.NextInt();
                out += (char)(v < 256 ? v : '?');
                break;
            }
            case 's': case 'S':
            {
                uint32_t ptr = (uint32_t)args.NextInt();
                std::string str;
                // Xenon CRT follows the function's width for %s, reverses it
                // for %S, and lets h/l/w override either default (as in Xenia).
                const bool wideString = isWide || (!isShort && ((conv == 'S') != wideFormat));
                if (ptr == 0)
                    str = "(null)";
                else if (wideString)
                {
                    auto* w = reinterpret_cast<const be<uint16_t>*>(base + ptr);
                    for (size_t i = 0; w[i] != 0 && i < 4096; i++)
                    {
                        uint16_t c = w[i];
                        str += c < 128 ? (char)c : '?';
                    }
                }
                else
                {
                    str = reinterpret_cast<const char*>(base + ptr);
                }
                snprintf(spec, sizeof(spec), "%%%s%s%ss", flags.c_str(), width.c_str(), precision.c_str());
                snprintf(buf, sizeof(buf), spec, str.c_str());
                out += buf;
                break;
            }
            case 'f': case 'F': case 'g': case 'G': case 'e': case 'E':
            {
                double v = args.NextDouble();
                snprintf(spec, sizeof(spec), "%%%s%s%s%c", flags.c_str(), width.c_str(), precision.c_str(), conv);
                snprintf(buf, sizeof(buf), spec, v);
                out += buf;
                break;
            }
            default:
                out.append(start, p - start + 1);
                break;
            }
        }
        return out;
    }

    std::string NarrowFormat(uint8_t* base, const char* format, bool wide)
    {
        if (!wide)
            return format;
        std::string s;
        auto* w = reinterpret_cast<const be<uint16_t>*>(format);
        for (size_t i = 0; w[i] != 0; i++)
        {
            uint16_t c = w[i];
            s += c < 128 ? (char)c : '?';
        }
        return s;
    }
}

std::string GuestFormat(PPCContext& ctx, uint8_t* base, const char* format, size_t firstArg, bool wideFormat)
{
    RegisterArgSource src(ctx, base, firstArg);
    return Format(base, NarrowFormat(base, format, wideFormat).c_str(), src, wideFormat);
}

std::string GuestFormatVaList(uint8_t* base, const char* format, uint32_t vaList, bool wideFormat)
{
    VaListArgSource src(base, vaList);
    return Format(base, NarrowFormat(base, format, wideFormat).c_str(), src, wideFormat);
}
