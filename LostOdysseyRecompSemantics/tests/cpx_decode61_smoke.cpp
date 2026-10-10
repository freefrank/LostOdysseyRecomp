#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/cpx_decode61.h"
#include <map>
int main() {
  try {
    using namespace cook_main_smoke;
    test::GuestWindow w(cook_main_smoke::Regions);
    w.Fill(0);
    auto m = w.Memory();
    cook_main_smoke::Environment env(w);
    auto deps = env.Deps();
    env.guest.geometryDeps = &deps;
    auto s = sort_engine61_oracle::Initial(0);
    auto initial = s;
    constexpr unsigned context = 0x60000, input = 0x61000, output = 0x62000,
                       state = 0x63000, params = 0x64000;
    unsigned decodeEntry = 0x82857b00;
    auto call = [&](unsigned first, unsigned second, unsigned count) {
      m.WriteU8(input, first);
      m.WriteU8(input + 1, second);
      m.WriteU8(input + 2, (count - 1) & 255);
      m.WriteU8(input + 3, (count - 1) >> 8);
      s.r[3] = context;
      s.r[4] = output;
      s.r[5] = input;
      (void)cpx_decode61::Apply(decodeEntry, m, s);
      if (s.r[3] != count)
        throw std::runtime_error("CPX decoded count");
    };
    for (unsigned i = 0; i < 4; ++i)
      m.WriteU8(input + 4 + i, 'A' + i);
    call(255, 0, 4);
    for (unsigned i = 0; i < 4; ++i)
      if (m.ReadU8(output + i) != 'A' + i)
        throw std::runtime_error("CPX stored copy");
    for (unsigned i = 0; i < 4; ++i)
      m.WriteU8(output + i, 0x5a);
    call(255, 1, 4);
    for (unsigned i = 0; i < 4; ++i)
      if (m.ReadU8(output + i) != 0x5a)
        throw std::runtime_error("CPX stored no-copy flag");
    auto packed = [&](const char *bits) {
      unsigned word = 0, count = 0, p = input + 4;
      for (auto q = bits; *q; ++q) {
        word = (word << 1) | (*q == '1');
        if (++count == 16) {
          m.WriteU16(p, word);
          p += 2;
          word = 0;
          count = 0;
        }
      }
      if (count) {
        m.WriteU16(p, word << (16 - count));
        p += 2;
      }
      m.WriteU16(p, 0);
      m.WriteU16(p + 2, 0);
    };
    packed("0010000011000010");
    call(0x50, 0x02, 6);
    for (unsigned i = 0; i < 6; ++i)
      if (m.ReadU8(output + i) != 'A')
        throw std::runtime_error("CPX overlapping mode1 match");
    packed("00100000110000011");
    call(0xa0, 0x22, 7);
    for (unsigned i = 0; i < 7; ++i)
      if (m.ReadU8(output + i) != 'A')
        throw std::runtime_error("CPX overlapping mode2 match");
    m.WriteU16(input + 4, 0x20c0);
    m.WriteU16(input + 6, 0);
    m.WriteU16(input + 8, 0);
    m.WriteU16(input + 10, 0);
    call(0, 0, 4);
    for (unsigned i = 0; i < 4; ++i)
      if (m.ReadU8(output + i) != 'A')
        throw std::runtime_error("CPX raw-distance match");
    if (m.ReadU16(context + 16) != 5 || m.ReadU32(context + 32) != 25)
      throw std::runtime_error("CPX block counters");
    decodeEntry = 0x82857d90;
    packed("0010000011000010");
    call(0x50, 0x02, 6);
    for (unsigned i = 0; i < 6; ++i)
      if (m.ReadU8(output + i) != 'A')
        throw std::runtime_error("CPX byte mode1");
    packed("00100000110000011");
    call(0xa0, 0x22, 7);
    for (unsigned i = 0; i < 7; ++i)
      if (m.ReadU8(output + i) != 'A')
        throw std::runtime_error("CPX byte mode2");
    packed("00100000110000000000000000000000000");
    call(0, 0, 4);
    for (unsigned i = 0; i < 4; ++i)
      if (m.ReadU8(output + i) != 'A')
        throw std::runtime_error("CPX byte raw distance");
    if (m.ReadU16(context + 16) != 8 || m.ReadU32(context + 32) != 42)
      throw std::runtime_error("CPX combined counters");
    // A cross-word request consumes the current suffix and the next word
    // prefix.
    m.WriteU32(state, input + 4);
    m.WriteU16(state + 4, 0x000b);
    m.WriteU16(state + 6, 4);
    m.WriteU16(input + 4, 0xa123);
    s.r[3] = state;
    s.r[4] = 8;
    (void)cpx_decode61::Apply(0x82857520, m, s);
    if (s.r[3] != 0xba || m.ReadU16(state + 6) != 12 ||
        m.ReadU32(state) != input + 6)
      throw std::runtime_error("CPX bit refill");
    m.WriteU8(input, 0x68);
    m.WriteU8(input + 1, 0x43);
    m.WriteU8(input + 2, 9);
    m.WriteU8(input + 3, 0);
    s.r[3] = params;
    s.r[4] = input;
    s.r[5] = 1;
    (void)cpx_decode61::Apply(0x828576e8, m, s);
    if (m.ReadU8(params) != 1 || m.ReadU8(params + 1) != 2 ||
        m.ReadU8(params + 4) != 16 || m.ReadU8(params + 7) != 2 ||
        m.ReadU8(params + 8) != 6 || m.ReadU8(params + 9) != 7 ||
        m.ReadU32(params + 12) != 10)
      throw std::runtime_error("CPX alternate parameter table");
    std::puts("PASS CPX stored/no-copy, overlapping LZ modes, bit refill and "
              "counters");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
