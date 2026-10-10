#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_script_history61.h"
#include <iostream>
struct HistoryGuest final : manager_release_context61::GuestServices {
  unsigned completed = 0;
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18 || e == 0x82389b78) {
      s.r[3] = 0x70000;
      return;
    }
    if (e == 0x8238e2f8) {
      s.r[3] = 0x71000;
      return;
    }
    if (e == 0x82ab36c8 || e == 0x82ab38f0) {
      if (s.r[3] != 0x80000 || s.r[4] != ~std::uint64_t(0) ||
          s.r[5] != ~std::uint64_t(0) || s.r[6] != ~std::uint64_t(0) ||
          s.r[7] != 7)
        throw std::runtime_error("completion callback arguments");
      ++completed;
      return;
    }
    throw std::runtime_error("history direct boundary");
  }
  void CallIndirect(GuestAddress, GuestMemory &,
                    manager_release_context61::Registers &) override {
    throw std::runtime_error("history indirect boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    test::GuestWindow w(cook_main_smoke::Regions);
    w.Fill(0);
    auto m = w.Memory();
    HistoryGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    constexpr unsigned owner = 0x60000, actor = 0x62000, state = 0x63000,
                       code = 0x64000, vars = 0x65000, resource = 0x80000;
    m.WriteU32(owner + 24, actor);
    m.WriteU32(owner + 44, state);
    m.WriteU32(actor + 36, code);
    m.WriteU32(actor + 12, vars);
    m.WriteU32(actor + 4, resource);
    auto le = [&](unsigned p, unsigned x) {
      m.WriteU8(p, x);
      m.WriteU8(p + 1, x >> 8);
    };
    auto operands = [&]() {
      for (unsigned i = 0; i < 6; ++i)
        le(code + 1 + 2 * i, i);
    };
    auto op = [&](unsigned e) {
      s.r[3] = owner;
      m.WriteU32(actor + 52, 0);
      (void)battle_script_history61::Apply(e, m, {guest, native}, s);
      if (s.r[1] != initial.r[1] || s.r[27] != initial.r[27] ||
          s.r[31] != initial.r[31] || s.fpr_bits[30] != initial.fpr_bits[30] ||
          s.fpr_bits[31] != initial.fpr_bits[31])
        throw std::runtime_error("history ABI");
    };
    operands();
    m.WriteU32(0x82000d6c, std::bit_cast<unsigned>(.5f));
    m.WriteU32(0x82000d48, std::bit_cast<unsigned>(2.f));
    m.WriteU32(vars, 10);
    m.WriteU32(vars + 4, 20);
    m.WriteU32(vars + 8, unsigned(-30));
    op(0x82af72a8);
    if (std::bit_cast<float>(m.ReadU32(resource + 76332)) != -15.f)
      throw std::runtime_error("resource vector set");
    op(0x82af7198);
    if (m.ReadU32(vars) != 10 || m.ReadU32(vars + 4) != 20 ||
        m.ReadU32(vars + 8) != unsigned(-30))
      throw std::runtime_error("resource vector get");
    m.WriteU32(vars, 3);
    op(0x82af9800);
    if (m.ReadU16(0x70000 + 148) != 3)
      throw std::runtime_error("manager half flags");
    m.WriteU32(vars, 0);
    op(0x82af9800);
    if (m.ReadU16(0x70000 + 148))
      throw std::runtime_error("manager half clear");
    m.WriteU32(vars, 9);
    m.WriteU32(vars + 4, 1);
    op(0x82af73a0);
    if (m.ReadU32(resource + 196) != 9 ||
        !(m.ReadU32(resource + 200) & 0x80000000))
      throw std::runtime_error("resource numeric flag");
    op(0x82af9cb0);
    if (m.ReadU32(actor + 296) || m.ReadU32(actor + 104) ||
        m.ReadU32(actor + 292))
      throw std::runtime_error("history clear");
    for (unsigned i = 0; i < 3; ++i) {
      auto p = actor + 104 + 24 * i;
      m.WriteU32(p, 10 + i);
      m.WriteU32(p + 4, 7);
      m.WriteU32(p + 8, 8);
      m.WriteU32(p + 12, std::bit_cast<unsigned>(4.75f));
      m.WriteU32(p + 16, 50 + i);
      m.WriteU32(p + 20, 60 + i);
    }
    m.WriteU32(actor + 296, 3);
    op(0x82af7410);
    unsigned expected[]{12, 7, 8, 4, 52, 62};
    for (unsigned i = 0; i < 6; ++i)
      if (m.ReadU32(vars + 4 * i) != expected[i])
        throw std::runtime_error("latest history record");
    m.WriteU8(code + 1, 3);
    le(code + 2, 0);
    le(code + 4, 1);
    le(code + 6, 2);
    m.WriteU32(vars + 4, 7);
    m.WriteU32(vars + 8, 8);
    op(0x82af9d08);
    if (m.ReadU32(vars) != 1)
      throw std::runtime_error("descending search retains oldest match");
    m.WriteU32(actor + 296, 1);
    op(0x82af9d08);
    if (m.ReadU32(vars) != 0xffffffff)
      throw std::runtime_error("history slot zero excluded");
    m.WriteU8(code + 1, 0);
    op(0x82af9e68);
    m.WriteU8(code + 1, 1);
    op(0x82af9e68);
    if ((m.ReadU32(0x70000 + 148) & 0xc000) != 0xc000)
      throw std::runtime_error("manager mode flags");
    m.WriteU32(0x71000, 0x71100);
    m.WriteU32(0x71004, 2);
    m.WriteU32(0x71100, resource);
    m.WriteU32(0x71104, 0xa0000);
    m.WriteU32(vars, 42);
    le(code + 2, 0);
    m.WriteU8(code + 1, 0);
    op(0x82afebc0);
    m.WriteU32(0xa0000 + 204, 42);
    m.WriteU32(0xa0000 + 208, 0xc0000007);
    m.WriteU32(vars, 0);
    op(0x82afebc0);
    if (m.ReadU32(resource + 204) || m.ReadU32(0xa0000 + 204) ||
        m.ReadU32(0xa0000 + 208) != 7)
      throw std::runtime_error("resource group clear");
    m.WriteU8(code + 1, 0);
    op(0x82af85b8);
    m.WriteU32(actor + 96, 7);
    m.WriteU32(actor + 60, 1);
    m.WriteU8(code + 1, 1);
    op(0x82af85b8);
    if (guest.completed != 1 || (m.ReadU32(actor + 64) & 0x01000000))
      throw std::runtime_error("action completion state");
    m.WriteU32(actor + 64, 0x0c000000);
    op(0x82af7518);
    if ((m.ReadU32(actor + 64) & 0x0c000000) != 0x04000000)
      throw std::runtime_error("two-bit actor mode");
    m.WriteU32(actor + 4, 0);
    le(code + 1, 100);
    op(0x82af7540);
    if (m.ReadU32(actor + 52) != 100)
      throw std::runtime_error("missing resource branch");
    std::cout << "battle_script_history61 smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
