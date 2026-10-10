#include "lo_semantics/battle_scene_tasks61.h"
#include "lo_semantics/battle_manager_access61.h"
#include "lo_semantics/string_storage_context61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/memory_fill.h"
#include <utility>
namespace lo::semantic::gpu::battle_scene_tasks61 {
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
  using recovery_abi::Address;
  if (e == 0x82b037c8) {
    auto old = Address(s.r[1]), input = Address(s.r[4]),
         output = Address(s.r[5]);
    auto kind = std::int8_t(Address(s.r[3]));
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 28; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 544;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    for (auto off : {80u, 84u, 88u})
      m.WriteU32(sp + off, 0);
    struct Path {
      unsigned kind, method, argument, base, count, temps[4], suffix[4];
    };
    const Path paths[] = {
        {11,
         356,
         9,
         176,
         4,
         {400, 144, 304, 432},
         {0x82000ba4, 0x820c4b34, 0x82000ba4, input}},
        {13,
         356,
         9,
         368,
         4,
         {240, 464, 208, 336},
         {0x82000ba4, 0x820c4b24, 0x82000ba4, input}},
        {14,
         360,
         4,
         160,
         4,
         {128, 112, 96, 272},
         {0x82000ba4, 0x82041c10, 0x82000ba4, input}},
        {15, 372, 1, 256, 2, {224, 192, 0, 0}, {0x82000ba4, input, 0, 0}},
        {16, 364, 1, 352, 2, {320, 288, 0, 0}, {0x82000ba4, input, 0, 0}},
        {17,
         372,
         4,
         480,
         3,
         {448, 416, 384, 0},
         {0x82000ba4, input, 0x820c4b18, 0}}};
    auto call = [&](unsigned a) {
      if (!string_storage_context61::Apply(a, m, d, s))
        d.guest.CallDirect(a, m, s);
    };
    auto source = input;
    for (const auto &path : paths)
      if (unsigned(kind) == path.kind) {
        s.r[3] = sp + path.base;
        s.r[4] = m.ReadU32(0x83315fb4);
        s.r[5] = path.argument;
        auto method = m.ReadU32(m.ReadU32(Address(s.r[4])) + path.method);
        s.ctr = method;
        d.guest.CallIndirect(method & ~3u, m, s);
        auto current = Address(s.r[3]);
        for (unsigned i = 0; i < path.count; ++i) {
          s.r[3] = sp + path.temps[i];
          s.r[4] = current;
          s.r[5] = path.suffix[i];
          call(0x8232d418);
          current = Address(s.r[3]);
        }
        s.r[3] = sp + 80;
        s.r[4] = current;
        call(0x822b3f50);
        for (unsigned i = path.count; i > 0; --i) {
          s.r[3] = sp + path.temps[i - 1];
          call(0x82298938);
        }
        s.r[3] = sp + path.base;
        call(0x82298938);
        source = m.ReadU32(sp + 80);
        break;
      }
    s.r[3] = output;
    s.r[4] = source;
    call(0x8230bac0);
    s.r[3] = sp + 80;
    call(0x82298938);
    s.r[1] += 544;
    for (unsigned i = 28; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82b33570 || e == 0x82b040d0) {
    auto old = Address(s.r[1]), owner = Address(s.r[3]), kind = Address(s.r[4]),
         arg5 = Address(s.r[5]), arg6 = Address(s.r[6]), text = Address(s.r[7]),
         priority = Address(s.r[8]);
    unsigned frame = e == 0x82b33570 ? 192 : 688,
             first = e == 0x82b33570 ? 21 : 23;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= frame;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    auto call = [&](unsigned a) {
      if (!battle_scene_tasks61::Apply(a, m, d, s) &&
          !string_storage_context61::Apply(a, m, d, s))
        d.guest.CallDirect(a, m, s);
    };
    auto virt = [&](unsigned off) {
      auto method = m.ReadU32(m.ReadU32(owner) + off);
      s.r[3] = owner;
      s.ctr = method;
      d.guest.CallIndirect(method & ~3u, m, s);
    };
    if (e == 0x82b33570) {
      m.WriteU32(sp + 212, owner);
      m.WriteU32(owner, 0x8200341c);
      for (auto off : {12u, 24u, 44u, 56u, 92u}) {
        m.WriteU32(sp + 80, owner + off);
        for (auto word : {0u, 4u, 8u})
          m.WriteU32(owner + off + word, 0);
      }
      for (auto off : {4u, 5u, 6u, 7u, 8u, 10u})
        m.WriteU8(owner + off, 0);
      m.WriteU8(owner + 9, 255);
      for (auto off : {36u, 80u, 84u, 88u})
        m.WriteU32(owner + off, 0);
      for (auto off : {40u, 68u, 72u})
        m.WriteU32(owner + off, 0xffffffff);
      m.WriteU32(owner + 76, 4);
      s.r[3] = owner;
    } else {
      s.r[5] = text;
      virt(84);
      auto existing = Address(s.r[3]);
      if (std::int32_t(existing) >= 0) {
        s.r[4] = existing;
        s.r[5] = arg5;
        s.r[6] = arg6;
        virt(104);
        s.r[4] = existing;
        virt(72);
        if (std::int8_t(Address(s.r[3])) > std::int8_t(priority)) {
          s.r[4] = existing;
          s.r[5] = priority;
          virt(136);
        }
        s.r[3] = existing;
      } else {
        s.r[3] = kind;
        s.r[4] = text;
        s.r[5] = sp + 96;
        call(0x82b037c8);
        auto id = m.ReadU32(owner + 4);
        s.r[4] = id;
        virt(116);
        while (std::int32_t(Address(s.r[3])) >= 0) {
          id = (id + 1) & 65535;
          s.r[4] = id;
          virt(116);
        }
        auto index = m.ReadU32(owner + 12);
        s.r[3] = owner + 8;
        s.r[4] = 1;
        call(0x82b1a560);
        s.r[3] = 104;
        call(0x82486c88);
        m.WriteU32(sp + 80, Address(s.r[3]));
        if (Address(s.r[3]))
          call(0x82b33570);
        else
          s.r[3] = 0;
        m.WriteU32(m.ReadU32(owner + 8) + 4 * index, Address(s.r[3]));
        auto object = m.ReadU32(m.ReadU32(owner + 8) + 4 * index);
        m.WriteU32(object + 40, id);
        s.r[3] = m.ReadU32(m.ReadU32(owner + 8) + 4 * index);
        s.r[4] = kind;
        s.r[5] = arg5;
        s.r[6] = arg6;
        s.r[7] = sp + 96;
        s.r[8] = 16;
        call(0x82b33f70);
        object = m.ReadU32(m.ReadU32(owner + 8) + 4 * index);
        m.WriteU8(object + 10, priority);
        m.WriteU32(owner + 4, (id + 1) & 65535);
        s.r[3] = id;
      }
    }
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82b08318 || e == 0x82b34ee0 || e == 0x82b355d8) {
    auto old = Address(s.r[1]), owner = Address(s.r[3]), name = Address(s.r[4]),
         mode = Address(s.r[5]);
    unsigned frame = e == 0x82b08318   ? 128
                     : e == 0x82b34ee0 ? 96
                                       : 112,
             first = e == 0x82b08318   ? 27
                     : e == 0x82b34ee0 ? 31
                                       : 29;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= frame;
    m.WriteU32(Address(s.r[1]), old);
    auto call = [&](unsigned a) {
      if (!battle_scene_tasks61::Apply(a, m, d, s) &&
          !string_storage_context61::Apply(a, m, d, s))
        d.guest.CallDirect(a, m, s);
    };
    if (e == 0x82b34ee0) {
      s.r[3] = FillGuestMemory(m, owner, 0, 18044);
      for (auto off : {4u, 12u, 16u})
        m.WriteU32(owner + off, 0xffffffff);
      m.WriteU8(owner, 0);
    } else if (e == 0x82b355d8) {
      call(0x82b352e8);
      s.r[3] = 0x832cc05c;
      s.r[4] = 16;
      s.r[5] = 0;
      s.r[6] = 0;
      s.r[7] = name;
      s.r[8] = (mode & 255) ? 12 : 1;
      call(0x82b040d0);
      m.WriteU32(owner + 12, Address(s.r[3]));
      if (std::int32_t(Address(s.r[3])) >= 0)
        m.WriteU8(owner, 1);
    } else {
      auto before = m.ReadU32(owner + 8);
      while (true) {
        bool collision = false;
        if (std::int32_t(before) > 0)
          for (unsigned i = 0;
               std::int32_t(i) < std::int32_t(m.ReadU32(owner + 8)); ++i)
            if (m.ReadU32(m.ReadU32(owner + 4) + 18044 * i + 4) ==
                m.ReadU32(owner)) {
              collision = true;
              break;
            }
        if (!collision)
          break;
        m.WriteU32(owner, m.ReadU32(owner) + 1);
        m.WriteU16(owner, 0);
      }
      auto header = owner + 4, count = m.ReadU32(header + 4) + 1;
      m.WriteU32(header + 4, count);
      if (std::int32_t(count) > std::int32_t(m.ReadU32(header + 8))) {
        auto capacity =
            count + unsigned(std::int32_t(count + count * 2) / 8) + 32;
        m.WriteU32(header + 8, capacity);
        s.r[3] = header;
        s.r[4] = 18044;
        s.r[5] = 8;
        call(0x8229f678);
      }
      s.r[3] = m.ReadU32(header) + 18044 * before;
      call(0x82b34ee0);
      auto row = m.ReadU32(header) + 18044 * before;
      m.WriteU32(row + 4, m.ReadU32(owner));
      s.r[3] = row;
      s.r[4] = name;
      s.r[5] = mode;
      call(0x82b355d8);
      s.r[3] = m.ReadU32(owner);
    }
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82b35568 || e == 0x82b352e8 || e == 0x82b35228 ||
      e == 0x82b354c0 || e == 0x82b351b8 || e == 0x82b01ed0) {
    auto old = Address(s.r[1]), owner = Address(s.r[3]), arg4 = Address(s.r[4]),
         arg5 = Address(s.r[5]);
    unsigned frame = e == 0x82b01ed0                        ? 96
                     : (e == 0x82b35568 || e == 0x82b352e8) ? 112
                                                            : 128;
    unsigned first = e == 0x82b01ed0   ? 31
                     : e == 0x82b35568 ? 30
                     : e == 0x82b352e8 ? 29
                     : e == 0x82b354c0 ? 28
                                       : 27;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= frame;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    auto call = [&](unsigned a) {
      if (!battle_scene_tasks61::Apply(a, m, d, s) &&
          !string_storage_context61::Apply(a, m, d, s))
        d.guest.CallDirect(a, m, s);
    };
    auto virt = [&](unsigned object, unsigned off) {
      auto method = m.ReadU32(m.ReadU32(object) + off);
      s.r[3] = object;
      s.ctr = method;
      d.guest.CallIndirect(method & ~3u, m, s);
    };
    auto free = [&](unsigned data) {
      s.r[3] = m.ReadU32(0x8330b608);
      if (!s.r[3]) {
        call(0x827c5f38);
        s.r[3] = m.ReadU32(0x8330b608);
      }
      s.r[4] = data;
      virt(Address(s.r[3]), 12);
    };
    if (e == 0x82b01ed0) {
      virt(owner, 116);
      auto index = Address(s.r[3]);
      s.r[4] = s.r[3];
      if (std::int32_t(index) >= 0)
        virt(owner, 132);
    } else if (e == 0x82b35568) {
      m.WriteU32(sp + 132, owner);
      call(0x82b352e8);
      s.r[3] = owner + 18032;
      call(0x82b354c0);
    } else if (e == 0x82b352e8) {
      s.r[3] = owner + 18032;
      s.r[4] = 0;
      call(0x82b35228);
      auto id = m.ReadU32(owner + 12);
      if (std::int32_t(id) >= 0) {
        s.r[3] = 0x832cc05c;
        s.r[4] = id;
        call(0x82b01ed0);
      }
      m.WriteU32(owner + 12, 0xffffffff);
      auto data = m.ReadU32(owner + 24);
      if (data) {
        free(data);
        m.WriteU32(owner + 24, 0);
      }
      m.WriteU8(owner, 0);
    } else if (e == 0x82b354c0) {
      m.WriteU32(sp + 148, owner);
      s.r[4] = 0;
      s.r[5] = m.ReadU32(owner + 4);
      call(0x82b351b8);
      auto data = m.ReadU32(owner);
      if (data)
        free(data);
      for (auto off : {0u, 4u, 8u})
        m.WriteU32(owner + off, 0);
    } else if (e == 0x82b351b8) {
      auto end = arg4 + arg5;
      if (std::int32_t(arg4) < std::int32_t(end))
        for (unsigned i = 0; i < arg5; ++i)
          s.r[3] = FillGuestMemory(m, m.ReadU32(owner) + 18020 * (arg4 + i), 0,
                                   18020);
      s.r[3] = owner;
      s.r[4] = arg4;
      s.r[5] = arg5;
      s.r[6] = 18020;
      s.r[7] = 8;
      call(0x82298af8);
    } else {
      for (unsigned i = 0; std::int32_t(i) < std::int32_t(m.ReadU32(owner + 4));
           ++i)
        s.r[3] = FillGuestMemory(m, m.ReadU32(owner) + 18020 * i, 0, 18020);
      auto capacity = m.ReadU32(owner + 8);
      m.WriteU32(owner + 4, 0);
      if (capacity != arg4) {
        m.WriteU32(owner + 8, arg4);
        s.r[3] = owner;
        s.r[4] = 18020;
        s.r[5] = 8;
        call(0x8229f678);
      }
    }
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82aaf850) {
    auto old = Address(s.r[1]), object = Address(s.r[3]);
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = 26; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= 144;
    auto sp = Address(s.r[1]);
    m.WriteU32(sp, old);
    m.WriteU32(sp + 164, object);
    auto call = [&](unsigned a) {
      if (!string_storage_context61::Apply(a, m, d, s))
        d.guest.CallDirect(a, m, s);
    };
    s.r[3] = object + 12;
    s.r[4] = 0;
    call(0x823562a8);
    auto header = object + 24;
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(m.ReadU32(header + 4));
         ++i) {
      s.r[3] = m.ReadU32(header) + 12 * i;
      call(0x82298938);
    }
    auto capacity = m.ReadU32(header + 8);
    m.WriteU32(header + 4, 0);
    if (capacity) {
      auto data = m.ReadU32(header);
      m.WriteU32(header + 8, 0);
      if (data) {
        s.r[3] = header;
        s.r[4] = 12;
        s.r[5] = 8;
        call(0x8229f678);
      }
    }
    s.r[3] = header;
    call(0x82474348);
    s.r[3] = object + 12;
    call(0x82298938);
    s.r[1] += 144;
    for (unsigned i = 26; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82388348 || e == 0x823883f0 || e == 0x82b05b38) {
    auto old = Address(s.r[1]), owner = Address(s.r[3]), arg4 = Address(s.r[4]),
         arg5 = Address(s.r[5]);
    unsigned frame = e == 0x82388348   ? 112
                     : e == 0x823883f0 ? 144
                                       : 128,
             first = e == 0x82388348   ? 29
                     : e == 0x823883f0 ? 26
                                       : 27;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= frame;
    m.WriteU32(Address(s.r[1]), old);
    auto call = [&](unsigned a) {
      if (!battle_scene_tasks61::Apply(a, m, d, s) &&
          !string_storage_context61::Apply(a, m, d, s))
        d.guest.CallDirect(a, m, s);
    };
    if (e == 0x82b05b38) {
      auto end = arg4 + arg5;
      if (std::int32_t(arg4) < std::int32_t(end))
        for (unsigned i = 0; i < arg5; ++i) {
          s.r[3] = m.ReadU32(owner) + 44 * (arg4 + i);
          call(0x82aaf850);
        }
      s.r[3] = owner;
      s.r[4] = arg4;
      s.r[5] = arg5;
      s.r[6] = 44;
      s.r[7] = 8;
      call(0x82298af8);
    } else if (e == 0x823883f0) {
      auto type = std::int8_t(arg4);
      for (unsigned off : {36u, 48u}) {
        if (off == 48 && m.ReadU32(owner + 32) == 13)
          break;
        unsigned i = 0;
        while (std::int32_t(i) < std::int32_t(m.ReadU32(owner + off + 4))) {
          auto row = m.ReadU32(owner + off) + 44 * i;
          auto category = m.ReadU8(row + 1);
          bool matches = false;
          if (type == 0 || type == 6)
            matches = category == type && m.ReadU32(row + 36) == arg5;
          else if (type == 1)
            matches = category == 1 && m.ReadU32(row + 8) == arg5;
          else if (type == 4 || type == 5)
            matches =
                (category == 4 || category == 5) && m.ReadU32(row + 36) == arg5;
          if (matches) {
            s.r[3] = owner + off;
            s.r[4] = i;
            s.r[5] = 1;
            call(0x82b05b38);
          } else
            ++i;
        }
      }
    } else {
      auto object = m.ReadU32(m.ReadU32(owner + 8) + 4 * arg4);
      if (object) {
        auto priority = std::int8_t(m.ReadU8(object + 33));
        s.r[3] = 0x832cc05c;
        s.r[4] = priority >= 11 ? 4 : 5;
        s.r[5] = m.ReadU32(object + 8);
        call(0x823883f0);
        object = m.ReadU32(m.ReadU32(owner + 8) + 4 * arg4);
        if (object) {
          s.r[3] = object;
          s.r[4] = 1;
          auto method = m.ReadU32(m.ReadU32(object));
          s.ctr = method;
          d.guest.CallIndirect(method & ~3u, m, s);
        }
        m.WriteU32(m.ReadU32(owner + 8) + 4 * arg4, 0);
      }
      s.r[3] = owner + 8;
      s.r[4] = arg4;
      s.r[5] = 1;
      s.r[6] = 4;
      s.r[7] = 8;
      call(0x82298af8);
    }
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e == 0x82b35dd8) {
    auto object = Address(s.r[3]);
    auto tag = std::int8_t(Address(s.r[4]));
    auto key = Address(s.r[5]), count = m.ReadU32(object + 40);
    s.r[3] = std::uint64_t(-1);
    for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i)
      if (std::int8_t(m.ReadU8(m.ReadU32(object + 36) + i)) == tag &&
          m.ReadU32(m.ReadU32(object + 48) + 4 * i) == key) {
        s.r[3] = i;
        break;
      }
    return true;
  }
  if (e == 0x82b08410) {
    auto owner = Address(s.r[3]), id = Address(s.r[4]);
    if (std::int32_t(id) < 0) {
      s.r[3] = owner + 4;
      s.r[4] = 0;
      return battle_scene_tasks61::Apply(0x82b080c0, m, d, s);
    }
    auto count = m.ReadU32(owner + 8);
    if (std::int32_t(count) > 0) {
      s.r[3] = owner + 4;
      for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i)
        if (m.ReadU32(m.ReadU32(owner + 4) + 18044 * i + 4) == id) {
          s.r[4] = i;
          s.r[5] = 1;
          return battle_scene_tasks61::Apply(0x82b08058, m, d, s);
        }
    }
    return true;
  }
  if (e == 0x82b08058 || e == 0x82b080c0 || e == 0x82b35e38 ||
      e == 0x82b1a2d0) {
    auto owner = Address(s.r[3]);
    auto argument4 = s.r[4], argument5 = s.r[5], argument6 = s.r[6];
    auto old = Address(s.r[1]);
    unsigned frame = (e == 0x82b08058 || e == 0x82b080c0) ? 128 : 112,
             first = (e == 0x82b08058 || e == 0x82b080c0) ? 27 : 30;
    m.WriteU32(old - 8, Address(s.lr));
    for (unsigned i = first; i < 32; ++i)
      recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
    s.r[1] -= frame;
    m.WriteU32(Address(s.r[1]), old);
    auto call = [&](unsigned a) {
      if (!battle_scene_tasks61::Apply(a, m, d, s) &&
          !string_storage_context61::Apply(a, m, d, s))
        d.guest.CallDirect(a, m, s);
    };
    if (e == 0x82b08058) {
      auto index = Address(argument4), count = Address(argument5),
           end = index + count;
      if (std::int32_t(index) < std::int32_t(end))
        for (unsigned i = 0; i < count; ++i) {
          s.r[3] = m.ReadU32(owner) + 18044 * (index + i);
          call(0x82b35568);
        }
      s.r[3] = owner;
      s.r[4] = index;
      s.r[5] = count;
      s.r[6] = 18044;
      s.r[7] = 8;
      call(0x82298af8);
    } else if (e == 0x82b080c0) {
      for (unsigned i = 0; std::int32_t(i) < std::int32_t(m.ReadU32(owner + 4));
           ++i) {
        s.r[3] = m.ReadU32(owner) + 18044 * i;
        call(0x82b35568);
      }
      auto capacity = m.ReadU32(owner + 8);
      m.WriteU32(owner + 4, 0);
      if (capacity != Address(argument4)) {
        m.WriteU32(owner + 8, Address(argument4));
        s.r[3] = owner;
        s.r[4] = 18044;
        s.r[5] = 8;
        call(0x8229f678);
      }
    } else if (e == 0x82b35e38) {
      call(0x82b35dd8);
      auto index = Address(s.r[3]);
      if (std::int32_t(index) >= 0)
        for (auto pair : {std::pair{36u, 1u}, std::pair{48u, 4u}}) {
          s.r[3] = owner + pair.first;
          s.r[4] = index;
          s.r[5] = 1;
          s.r[6] = pair.second;
          s.r[7] = 8;
          call(0x82298af8);
        }
      s.r[3] = m.ReadU32(owner + 40) == 0;
    } else {
      auto count = m.ReadU32(owner + 12), data = m.ReadU32(owner + 8);
      s.r[4] = argument5;
      for (unsigned i = 0; std::int32_t(i) < std::int32_t(count); ++i) {
        auto object = m.ReadU32(data + 4 * i);
        if (m.ReadU32(object + 8) != Address(argument4))
          continue;
        bool remove = std::int8_t(Address(argument5)) < 0 ||
                      std::int32_t(Address(argument6)) < 0;
        if (!remove) {
          s.r[3] = object;
          s.r[5] = argument6;
          call(0x82b35e38);
          remove = (Address(s.r[3]) & 255) != 0;
        }
        if (remove) {
          s.r[3] = owner;
          s.r[4] = i;
          call(0x82388348);
        }
        break;
      }
    }
    s.r[1] += frame;
    for (unsigned i = first; i < 32; ++i)
      s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
    s.lr = m.ReadU32(old - 8);
    return true;
  }
  if (e != 0x82b035e0 && e != 0x82b04c50 && e != 0x82b1a560)
    return false;
  auto old = Address(s.r[1]), owner = Address(s.r[3]),
       argument = Address(s.r[4]);
  unsigned frame = e == 0x82b04c50   ? 192
                   : e == 0x82b035e0 ? 112
                                     : 96,
           first = e == 0x82b04c50   ? 20
                   : e == 0x82b035e0 ? 29
                                     : 31;
  m.WriteU32(old - 8, Address(s.lr));
  for (unsigned i = first; i < 32; ++i)
    recovery_abi::WriteU64(m, old - 16 - 8 * (31 - i), s.r[i]);
  s.r[1] -= frame;
  m.WriteU32(Address(s.r[1]), old);
  auto call = [&](unsigned a) {
    if (a == 0x82389aa0 && battle_manager_access61::Apply(a, m, d, s))
      return;
    if (!battle_scene_tasks61::Apply(a, m, d, s) &&
        !string_storage_context61::Apply(a, m, d, s))
      d.guest.CallDirect(a, m, s);
  };
  auto virt = [&](unsigned p, unsigned off) {
    auto a = m.ReadU32(m.ReadU32(p) + off);
    s.r[3] = p;
    s.ctr = a;
    d.guest.CallIndirect(a & ~3u, m, s);
  };
  auto grow = [&](unsigned header, unsigned extra, unsigned size) {
    auto before = m.ReadU32(header + 4), count = before + extra;
    m.WriteU32(header + 4, count);
    if (std::int32_t(count) > std::int32_t(m.ReadU32(header + 8))) {
      auto capacity =
          count + unsigned(std::int32_t(count + count * 2) / 8) + 32;
      m.WriteU32(header + 8, capacity);
      s.r[3] = header;
      s.r[4] = size;
      s.r[5] = 8;
      call(0x8229f678);
    }
    return before;
  };
  if (e == 0x82b1a560) {
    s.r[3] = grow(owner, argument, 4);
  } else if (e == 0x82b035e0) {
    auto id = m.ReadU32(owner + 116);
    if (std::int32_t(id) >= 0) {
      s.r[4] = id;
      virt(0x832cb670, 36);
    }
    m.WriteU32(owner + 116, 0xffffffff);
    id = m.ReadU32(owner + 120);
    if (std::int32_t(id) >= 0 && id != m.ReadU32(0x832cb6f0)) {
      s.r[3] = 0x832cb68c;
      s.r[4] = id;
      call(0x82b08410);
    }
    m.WriteU32(owner + 120, 0xffffffff);
    for (unsigned off : {124u, 136u}) {
      while (std::int32_t(m.ReadU32(owner + off + 4)) > 0) {
        auto row = m.ReadU32(owner + off), task = m.ReadU32(row + 4);
        if (std::int32_t(task) >= 0) {
          s.r[4] = task;
          if (off == 124)
            virt(owner, 48);
          else {
            s.r[3] = 0x832cc0fc;
            s.r[5] = 0;
            s.r[6] = m.ReadU32(row);
            call(0x82b1a2d0);
          }
        }
        s.r[3] = owner + off;
        s.r[4] = 0;
        s.r[5] = 1;
        s.r[6] = 8;
        s.r[7] = 8;
        call(0x82298af8);
      }
    }
    m.WriteU32(owner + 148, 0);
  } else {
    s.r[3] = owner;
    call(0x82b035e0);
    call(0x82380a18);
    call(0x82389aa0);
    auto profile = Address(s.r[3]);
    auto rank = std::int8_t(m.ReadU8(profile + 133));
    if (rank > 0 &&
        rank < std::int32_t(m.ReadU32(m.ReadU32(0x832ca0d0) + 452))) {
      auto row = m.ReadU32(m.ReadU32(0x832ca0d0) + 448) + 60 * unsigned(rank);
      auto name = [&](unsigned p) {
        return m.ReadU32(p + 4) ? m.ReadU32(p) : 0x821a83d0;
      };
      auto nonempty = [&](unsigned p) {
        s.r[3] = p;
        s.r[4] = 0x821a83d0;
        call(0x822971e0);
        return Address(s.r[3]) != 0;
      };
      if (nonempty(name(row + 12))) {
        s.r[4] = 0;
        s.r[5] = 999;
        s.r[6] = name(row + 12);
        s.r[7] = 11;
        virt(0x832cb670, 16);
        m.WriteU32(owner + 116, Address(s.r[3]));
      }
      unsigned secondary = 0;
      if (nonempty(name(row)))
        secondary = name(row);
      else if (nonempty(name(profile + 136)))
        secondary = name(profile + 136);
      if (secondary) {
        s.r[3] = 0x832cb68c;
        s.r[4] = secondary;
        s.r[5] = 0;
        call(0x82b08318);
        m.WriteU32(owner + 120, Address(s.r[3]));
      }
      auto task = m.ReadU32(owner + 120);
      if (std::int32_t(task) >= 0 && task != m.ReadU32(0x832cb6f0))
        m.WriteU32(0x832cb6f0, task);
      for (unsigned kind = 0; kind < 2; ++kind) {
        auto header = owner + (kind ? 136 : 124),
             listField = row + (kind ? 36 : 24);
        for (unsigned i = 0;
             std::int32_t(i) < std::int32_t(m.ReadU32(listField + 4)); ++i) {
          auto index = grow(header, 1, 8), dest = m.ReadU32(header) + 8 * index;
          for (unsigned j = 0; j < 8; ++j)
            m.WriteU8(dest + j, 0);
          auto def = m.ReadU32(listField) + 16 * i, key = m.ReadU32(def);
          m.WriteU32(m.ReadU32(header) + 8 * i, key);
          auto text = m.ReadU32(def + 8) ? m.ReadU32(def + 4) : 0x821a83d0;
          auto resultSlot = m.ReadU32(header) + 8 * i + 4;
          if (!kind) {
            s.r[4] = 8;
            s.r[5] = 0;
            s.r[6] = key;
            s.r[7] = text;
            s.r[8] = 11;
            virt(owner, 16);
          } else {
            s.r[3] = 0x832cc0fc;
            s.r[4] = 0;
            s.r[5] = key;
            s.r[6] = text;
            s.r[7] = 2;
            s.r[8] = 11;
            s.r[9] = 0;
            call(0x82b1a7f0);
          }
          m.WriteU32(resultSlot, Address(s.r[3]));
        }
      }
      m.WriteU32(owner + 148, row + 48);
    }
  }
  s.r[1] += frame;
  for (unsigned i = first; i < 32; ++i)
    s.r[i] = recovery_abi::ReadU64(m, old - 16 - 8 * (31 - i));
  s.lr = m.ReadU32(old - 8);
  return true;
}
} // namespace lo::semantic::gpu::battle_scene_tasks61
