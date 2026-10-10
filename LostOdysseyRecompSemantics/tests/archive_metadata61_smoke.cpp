#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_triangle_storage61.h"
#include "lo_semantics/archive_metadata61.h"
#include <map>
struct MetadataGuest final : manager_release_context61::GuestServices {
  bool accept = true;
  unsigned errors = 0;
  void CallDirect(GuestAddress e, GuestMemory &m,
                  manager_release_context61::Registers &s) override {
    if (e == 0x830da31c) {
      auto p = Address(s.r[3]);
      unsigned expected[]{2025, 2, 8, 12, 34, 56, 0};
      for (unsigned i = 0; i < 7; ++i)
        if (m.ReadU16(p + 2 * i) != expected[i])
          throw std::runtime_error("FILETIME input field order");
      if (accept)
        recovery_abi::WriteU64(m, Address(s.r[4]), 0x1122334455667788ull);
      s.r[3] = accept;
      return;
    }
    if (e == 0x827ca628) {
      if (Address(s.r[3]) != 0xc000000d)
        throw std::runtime_error("time conversion error status");
      ++errors;
      return;
    }
    throw std::runtime_error("metadata direct boundary");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("metadata indirect boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    regions.push_back({0x82046000, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    MetadataGuest guest;
    auto s = sort_engine61_oracle::Initial(0);
    constexpr unsigned input = 0x60000, fields = 0x61000, time = 0x62000,
                       path = 0x63000;
    unsigned months[]{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (unsigned i = 0; i < 12; ++i)
      m.WriteU8(0x82046150 + i, months[i]);
    unsigned value = 2678400 + 7 * 86400 + 12 * 3600 + 34 * 60 + 56;
    m.WriteU32(input, (25u << 25) | value);
    s.r[3] = fields;
    s.r[4] = input;
    (void)archive_metadata61::Apply(0x828527a0, m, {guest, native}, s);
    // Weekday follows the source's month-table accumulation, not a corrected
    // calendar.
    unsigned expected[]{2025, 2, 3, 8, 12, 34, 56, 0};
    for (unsigned i = 0; i < 8; ++i)
      if (m.ReadU16(fields + 2 * i) != expected[i])
        throw std::runtime_error("packed archive date fields");
    s.r[3] = fields;
    s.r[4] = time;
    (void)archive_metadata61::Apply(0x82be2f60, m, {guest, native}, s);
    if (s.r[3] != 1 || recovery_abi::ReadU64(m, time) != 0x1122334455667788ull)
      throw std::runtime_error("FILETIME guest bridge result");
    guest.accept = false;
    recovery_abi::WriteU64(m, time, 0xaabbccdd12345678ull);
    s.r[3] = fields;
    s.r[4] = time;
    (void)archive_metadata61::Apply(0x82be2f60, m, {guest, native}, s);
    if (s.r[3] || guest.errors != 1 ||
        recovery_abi::ReadU64(m, time) != 0xaabbccdd12345678ull)
      throw std::runtime_error("FILETIME failure output preservation");
    std::string pattern = "abc.xyz", text = "prefixABC.XYZ";
    for (unsigned i = 0; i <= pattern.size(); ++i)
      m.WriteU8(0x8204615c + i, i == pattern.size() ? 0 : pattern[i]);
    for (unsigned i = 0; i <= text.size(); ++i)
      m.WriteU8(path + i, i == text.size() ? 0 : text[i]);
    s.r[3] = path;
    (void)archive_metadata61::Apply(0x82852db8, m, {guest, native}, s);
    if (m.ReadU8(path + text.size() - 5) != '1')
      throw std::runtime_error("guest suffix rewrite");
    std::puts("PASS packed archive date, guest time conversion/error bridge "
              "and suffix rewrite");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
