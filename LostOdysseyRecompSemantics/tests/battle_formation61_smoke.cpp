#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/battle_formation61.h"
#include "battle_profile_fixture.h"
struct FormationGuest final : manager_release_context61::GuestServices {
  void CallDirect(GuestAddress e, GuestMemory &,
                  manager_release_context61::Registers &s) override {
    if (e == 0x82380a18) {
      s.r[3] = 0x90000;
      return;
    }
    throw std::runtime_error("unexpected formation direct boundary");
  }
  void CallIndirect(GuestAddress e, GuestMemory &,
                    manager_release_context61::Registers &s) override {
    if (profile_fixture::Indirect(e, s))
      return;
    throw std::runtime_error("unexpected formation indirect boundary");
  }
};
int main() {
  try {
    using namespace cook_main_smoke;
    std::vector<test::Region> regions(cook_main_smoke::Regions.begin(),
                                      cook_main_smoke::Regions.end());
    for (auto p : {0x83315000u, 0x832c1000u, 0x83264000u, 0x832ca000u})
      regions.push_back({p, 0x1000});
    test::GuestWindow w(regions);
    w.Fill(0);
    auto m = w.Memory();
    FormationGuest guest;
    auto s = sort_engine61_oracle::Initial(0), initial = s;
    auto path = std::getenv("LO_HULL_INCREMENTAL_CONSTANTS");
    if (!path)
      throw std::runtime_error("LO_HULL_INCREMENTAL_CONSTANTS");
    std::array<unsigned char, 192> data{};
    std::ifstream f(path, std::ios::binary);
    f.read(reinterpret_cast<char *>(data.data()), data.size());
    if (f.gcount() != data.size())
      throw std::runtime_error("private trig fixture size");
    for (unsigned i = 0; i < 120; ++i)
      m.WriteU8(0x83214d80 + i, data[i]);
    for (unsigned i = 0; i < 8; ++i) {
      m.WriteU8(0x83215508 + i, data[120 + i]);
      m.WriteU8(0x82000f28 + i, data[128 + i]);
    }
    auto put = [&](unsigned p, float v) {
      m.WriteU32(p, std::bit_cast<unsigned>(v));
    };
    auto get = [&](unsigned p) { return std::bit_cast<float>(m.ReadU32(p)); };
    auto check = [&](bool ok, const char *message) {
      if (!ok)
        throw std::runtime_error(message);
    };
    constexpr unsigned owner = 0x50000, list = 0x51000, resources = 0x52000,
                       descriptor = 0x53000, rows = 0x54000, points = 0x55000,
                       profile = 0x56000, a = 0x100000, b = 0x120000,
                       c = 0x140000, other = 0x160000;
    profile_fixture::Setup(m, profile);
    put(profile + 28, 10);
    put(profile + 32, 20);
    put(profile + 36, 30);
    put(profile + 40, 0);
    recovery_abi::WriteU64(m, 0x82001030, std::bit_cast<std::uint64_t>(1.0));
    put(0x82000bb8, 2);
    m.WriteU32(owner + 20, list);
    m.WriteU32(list, resources);
    m.WriteU32(list + 4, 4);
    unsigned items[] = {a, b, other, c};
    for (unsigned i = 0; i < 4; ++i)
      m.WriteU32(resources + 4 * i, items[i]);
    m.WriteU32(a + 124, 0x48000000);
    m.WriteU32(b + 124, 0x08000000);
    m.WriteU32(c + 124, 0x48000000);
    m.WriteU32(other + 124, 0x40000000);
    auto call = [&](unsigned entry) {
      check(battle_formation61::Apply(entry, m, {guest, native}, s),
            "formation entry");
      check(s.r[1] == initial.r[1], "formation stack");
      for (unsigned i = 18; i < 32; ++i)
        check(s.r[i] == initial.r[i], "formation nonvolatile GPR");
      for (unsigned i = 26; i < 32; ++i)
        check(s.fpr_bits[i] == initial.fpr_bits[i],
              "formation nonvolatile FPR");
    };
    s.r[3] = owner;
    s.r[4] = 257;
    s.r[5] = 0x57000;
    s.r[6] = 0x57004;
    call(0x82acf5d8);
    check(m.ReadU32(0x57000) == 2 && m.ReadU32(0x57004) == 1,
          "side and class counts");
    m.WriteU32(descriptor + 16, rows);
    m.WriteU32(descriptor + 20, 2);
    m.WriteU32(rows + 16, points);
    m.WriteU32(rows + 20, 1);
    m.WriteU32(rows + 28 + 16, points);
    m.WriteU32(rows + 28 + 20, 4);
    unsigned types[] = {0, 9, 2, 0};
    for (unsigned i = 0; i < 4; ++i) {
      m.WriteU32(points + 20 * i, types[i]);
      put(points + 20 * i + 4, float(i + 1));
      put(points + 20 * i + 8, float(i + 2));
      put(points + 20 * i + 12, float(i + 3));
      put(points + 20 * i + 16, float(i + 4));
    }
    s.r[3] = descriptor;
    s.r[4] = 2;
    s.r[5] = 1;
    call(0x82af5b20);
    check(s.r[3] == 1, "matching formation row");
    s.r[3] = rows + 28;
    s.r[4] = 1;
    s.r[5] = 1;
    call(0x82af5498);
    check(s.r[3] == 3, "ordinal among matching slots");
    s.r[3] = rows + 28;
    s.r[4] = 0;
    s.r[5] = 1;
    call(0x82af5498);
    check(s.r[3] == std::uint64_t(-1), "missing slot sentinel");
    s.r[3] = descriptor;
    s.r[4] = 7;
    s.r[5] = 1;
    call(0x82af5b20);
    check(s.r[3] == std::uint64_t(-1), "missing row sentinel");
    m.WriteU32(0x832ca0d0, 0x58000);
    m.WriteU32(0x58000 + 84, descriptor);
    s.r[3] = owner;
    s.r[4] = 1;
    s.r[5] = 0;
    call(0x82af5ba8);
    check(s.r[3] == 1, "placement return");
    unsigned placed[] = {a, b, c}, slots[] = {0, 2, 3};
    for (unsigned i = 0; i < 3; ++i) {
      auto r = placed[i], k = slots[i];
      check(get(r + 76324) == float(k + 11) &&
                get(r + 76328) == float(k + 22) &&
                get(r + 76332) == float(k + 33) &&
                get(r + 76344) == float(2 * (k + 4)),
            "translated formation coordinates");
    }
    check(m.ReadU32(other + 76324) == 0, "other side untouched");
    // A quarter turn exercises recovered guest sine/cosine, with public fixture
    // values.
    put(profile + 40, float(1.5707963267948966));
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(2.0);
    s.fpr_bits[2] = std::bit_cast<std::uint64_t>(3.0);
    s.fpr_bits[3] = std::bit_cast<std::uint64_t>(4.0);
    s.fpr_bits[4] = std::bit_cast<std::uint64_t>(5.0);
    s.r[7] = 0x59000;
    s.r[8] = 0x59004;
    s.r[9] = 0x59008;
    s.r[10] = 0x5900c;
    call(0x82af5320);
    check(std::abs(get(0x59000) - 7) < 0.00001f &&
              std::abs(get(0x59004) - 22) < 0.00001f && get(0x59008) == 34 &&
              std::abs(get(0x5900c) - 6.5707965f) < 0.00001f,
          "rotated formation coordinates");
    std::puts("PASS formation selection, side counts, ordinal placement, "
              "profile transform and nonvolatile state");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
