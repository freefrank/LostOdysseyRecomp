#include "lo_semantics/string_storage_context61.h"
#include "lo_semantics/battle_action_destruction61.h"
#include "lo_semantics/battle_action_storage61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/allocation_array.h"
#include "lo_semantics/battle_action_records61.h"
#include <bit>
namespace lo::semantic::gpu::battle_action_storage61 {
namespace {
using recovery_abi::Address;
struct Runtime {
  GuestMemory &m;
  Dependencies d;
  Registers &s;
  unsigned owner;
  unsigned W(unsigned p) { return m.ReadU32(p); }
  void Call(unsigned e) {
    if (!string_storage_context61::Apply(e, m, d, s) &&
        !battle_action_storage61::Apply(e, m, d, s) &&
        !battle_action_destruction61::Apply(e, m, d, s))
      d.guest.CallDirect(e, m, s);
  }
  unsigned Manager(unsigned method) {
    Call(0x82380a18);
    Call(method);
    return Address(s.r[3]);
  }
  struct ResizeBridge final : ArrayResizeServices {
    Runtime &r;
    explicit ResizeBridge(Runtime &x) : r(x) {}
    void InitializeManager() override {
      r.d.guest.CallDirect(0x827c5f38, r.m, r.s);
    }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
                               GuestAddress old, unsigned bytes,
                               unsigned align) override {
      r.s.r[3] = manager;
      r.s.r[4] = old;
      r.s.r[5] = bytes;
      r.s.r[6] = align;
      r.s.ctr = method;
      r.d.guest.CallIndirect(method, r.m, r.s);
      return Address(r.s.r[3]);
    }
  };
  void Resize(unsigned header, unsigned stride, unsigned align) {
    ResizeBridge bridge(*this);
    ResizeArray(m, bridge, header, stride, align);
  }
  void CopyString(unsigned dest, unsigned sp) {
    s.r[3] = sp + 80;
    s.r[4] = 0x821a8f04;
    Call(0x822d02f8);
    if (dest != sp + 80) {
      auto count = W(sp + 84), old = W(dest);
      m.WriteU32(dest + 8, count);
      m.WriteU32(dest + 4, count);
      if (old || count) {
        auto manager = W(0x8330b608);
        if (!manager) {
          s.r[3] = 0;
          Call(0x827c5f38);
          manager = W(0x8330b608);
        }
        s.r[3] = manager;
        s.r[4] = old;
        s.r[5] = 2 * count;
        s.r[6] = 8;
        s.ctr = W(W(manager) + 8);
        d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        m.WriteU32(dest, Address(s.r[3]));
      }
      auto countNow = W(dest + 4);
      if (countNow) {
        s.r[3] = W(dest);
        s.r[4] = W(sp + 84) ? W(sp + 80) : 0x821a83d0;
        s.r[5] = 2 * countNow;
        Call(0x82b7a0b0);
      }
    }
    s.r[3] = sp + 80;
    Call(0x82298938);
  }
  void Run(unsigned e) {
    if (e == 0x822c42d8) {
      auto oldCount = W(owner + 4), add = Address(s.r[4]),
           stride = Address(s.r[5]), align = Address(s.r[6]),
           count = oldCount + add;
      m.WriteU32(owner + 4, count);
      if (std::int32_t(count) > std::int32_t(W(owner + 8))) {
        auto extra = std::int32_t(count * 3) / 8;
        m.WriteU32(owner + 8, count + unsigned(extra) + 32);
        Resize(owner, stride, align);
      }
      s.r[3] = oldCount;
      return;
    }
    if (e == 0x824061e0) {
      auto add = Address(s.r[4]), stride = Address(s.r[5]);
      s.r[3] = owner;
      Call(0x822c42d8);
      auto oldCount = Address(s.r[3]);
      s.r[3] = W(owner) + oldCount * stride;
      s.r[4] = 0;
      s.r[5] = add * stride;
      Call(0x82b7bc40);
      s.r[3] = oldCount;
      return;
    }
    if (e == 0x82a9b698) {
      auto capacity = Address(s.r[4]);
      for (unsigned i = 0; std::int32_t(i) < std::int32_t(W(owner + 4)); ++i) {
        s.r[3] = W(owner) + 124208 * i;
        Call(0x828ae428);
      }
      auto previous = W(owner + 8);
      m.WriteU32(owner + 4, 0);
      if (previous != capacity) {
        m.WriteU32(owner + 8, capacity);
        Resize(owner, 124208, 8);
      }
      return;
    }
    auto resource = owner, alternate = Address(s.r[4]) & 255,
         sp = Address(s.r[1]);
    auto header = resource + (alternate ? 14668 : 14656);
    s.r[3] = header;
    s.r[4] = 1;
    s.r[5] = 124208;
    s.r[6] = 8;
    Call(0x824061e0);
    auto record = W(header) + 124208 * W(header + 4) - 124208;
    if (s.cached_fp_control & 0x8040) {
      s.cached_fp_control &= ~0x8040u;
      d.fp.SetHostFpControl(s.cached_fp_control);
    }
    auto one = W(0x82000e40), zero = W(0x82000e50);
    s.fpr_bits[31] =
        std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(one)));
    s.fpr_bits[30] =
        std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(zero)));
    for (unsigned offset : {0u, 32u, 8u, 12u})
      m.WriteU32(record + offset, 0);
    for (unsigned i = 0; i < 32; ++i) {
      auto slot = record + 464 * i, p = slot + 252;
      m.WriteU32(slot + 36, 0xffffffff);
      for (unsigned k = 0; k < 4; ++k) {
        auto q = slot + 216 + 4 * k;
        m.WriteU32(p, 0);
        for (int off : {16, 0, 64, -176, -48, -32, -16, 40, 14864, 14848, 14912,
                        14672, 14800, 14816, 14832, 14888})
          m.WriteU32(q + unsigned(off), 0);
        m.WriteU32(p + 52, 0xffffffff);
        m.WriteU32(p + 48, 0xffffffff);
        m.WriteU32(p + 20, W(p + 20) & 0x7fffffffu);
        m.WriteU32(p + 14848, 0);
        m.WriteU32(p + 14900, 0xffffffff);
        m.WriteU32(p + 14896, 0xffffffff);
        m.WriteU32(p + 14868, W(p + 14868) & 0x7fffffffu);
        for (int off : {-64, -80, -96, -112, -128, -144, -160, 14784, 14768,
                        14752, 14736, 14720, 14704, 14688})
          m.WriteU32(q + unsigned(off), one);
      }
      for (unsigned j = 0; j < 16; ++j)
        CopyString(slot + 308 + 12 * j, sp);
      m.WriteU32(p + 14844, W(p + 14844) & 0x7fffffffu);
      // These shared record sections are reset on every source-slot iteration
      // in the PPC.
      for (unsigned j = 0; j < 43; ++j) {
        auto row = record + 29736 + 2192 * j;
        for (int off : {-4, 0, 4, 8})
          m.WriteU32(row + unsigned(off), 0);
        for (unsigned k = 0; k < 8; ++k) {
          auto q = row + 272 + 272 * k;
          for (int off : {-260, 0, 4, 8})
            m.WriteU32(q + unsigned(off), 0);
        }
        m.WriteU32(record + 124036 + 4 * j, 0xffffffff);
      }
      for (unsigned j = 0; j < 2; ++j) {
        auto q = record + 123992 + 24 * j;
        m.WriteU32(q - 4, zero);
        m.WriteU32(q, 0);
        m.WriteU32(q + 4, zero);
        m.WriteU32(q + 8, zero);
        m.WriteU32(q + 12, 0);
        m.WriteU32(q + 16, W(q + 16) & 0x7fffffffu);
      }
    }
    m.WriteU32(record + 36, W(resource + 64));
    if (W(resource + 212)) {
      if (!W(resource + 216)) {
        unsigned order = 1;
        auto list = Manager(0x8238e2f8);
        for (unsigned i = 0;
             std::int32_t(i) < std::int32_t(W(Manager(0x8238e2f8) + 4)); ++i) {
          auto peer = W(W(list) + 4 * i);
          if (W(peer + 212) == W(resource + 212) && W(peer + 216) == order) {
            s.r[3] = resource;
            s.r[4] = W(peer + 64);
            s.r[5] = 0;
            s.r[6] = 0;
            (void)battle_action_records61::Apply(0x82ab0b28, m, d, s);
            ++order;
          }
        }
      }
    } else if (W(resource + 204)) {
      auto list = Manager(0x8238e2f8);
      for (unsigned i = 0;
           std::int32_t(i) < std::int32_t(W(Manager(0x8238e2f8) + 4)); ++i) {
        auto peer = W(W(list) + 4 * i);
        if (W(peer + 64) != W(resource + 64) &&
            W(peer + 204) == W(resource + 204)) {
          s.r[3] = resource;
          s.r[4] = W(peer + 64);
          s.r[5] = 0;
          s.r[6] = 0;
          (void)battle_action_records61::Apply(0x82ab0b28, m, d, s);
        }
      }
    }
  }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  if (e == 0x82acd3c0) {
    auto resource = Address(s.r[4]);
    m.WriteU32(resource + 88, 0);
    m.WriteU32(resource + 92, 0);
    m.WriteU32(resource + 96, 0);
    m.WriteU32(resource + 100, m.ReadU32(resource + 100) & 0x3fffffffu);
    return true;
  }
  unsigned first = 28, frame = 128;
  switch (e) {
  case 0x822c42d8:
    first = 31;
    frame = 96;
    break;
  case 0x824061e0:
    break;
  case 0x82a9b698:
    first = 26;
    frame = 144;
    break;
  case 0x82ab2d88:
    first = 15;
    frame = 256;
    break;
  default:
    return false;
  }
  auto owner = Address(s.r[3]), old = Address(s.r[1]);
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  if (e == 0x82ab2d88) {
    recovery_abi::WriteU64(m, old - 160, s.fpr_bits[30]);
    recovery_abi::WriteU64(m, old - 152, s.fpr_bits[31]);
  }
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  Runtime{m, d, s, owner}.Run(e);
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  if (e == 0x82ab2d88) {
    s.fpr_bits[30] = recovery_abi::ReadU64(m, old - 160);
    s.fpr_bits[31] = recovery_abi::ReadU64(m, old - 152);
  }
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_action_storage61
