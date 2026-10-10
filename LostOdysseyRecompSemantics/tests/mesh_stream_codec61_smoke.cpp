#define main cook_main_fixture_main
#include "mesh_cook_main61_smoke.cpp"
#undef main
#include "lo_semantics/mesh_stream_codec61.h"
int main() {
    try {
        using namespace cook_main_smoke;
        for (unsigned swap : {0u, 1u}) {
            test::GuestWindow w(cook_main_smoke::Regions);
            w.Fill(0);
            auto m = w.Memory();
            cook_main_smoke::Environment env(w);
            m.WriteU32(Writer, Table);
            m.WriteU32(Writer + 8, 8192);
            m.WriteU32(Writer + 12, cook_main_smoke::Buffer);
            constexpr unsigned writers[]{0x82bde330, 0x82bde378, 0x82bde3c0,
                                         0x82bde408, 0x82bde450, 0x82bde498};
            for (unsigned i = 0; i < 6; ++i)
                m.WriteU32(Table + 28 + 4 * i, writers[i]);
            constexpr unsigned reader = 0x60000, readerTable = 0x61000;
            constexpr unsigned readers[]{0x82bde550, 0x82bde568, 0x82bde580,
                                         0x82bde598, 0x82bde5b8, 0x82bde5d8};
            m.WriteU32(reader, readerTable);
            m.WriteU32(reader + 4, cook_main_smoke::Buffer);
            for (unsigned i = 0; i < 6; ++i)
                m.WriteU32(readerTable + 4 + 4 * i, readers[i]);
            auto s = sort_engine61_oracle::Initial(0), initial = s;
            auto call = [&](unsigned e, std::initializer_list<unsigned> args) {
                unsigned r = 3;
                for (auto a : args)
                    s.r[r++] = a;
                if (!mesh_stream_codec61::Apply(e, m, {env.guest, native}, s))
                    throw std::runtime_error("codec dispatch");
            };
            call(0x82badd60, {'C', 'V', 'X', 'M', 7, swap, Writer});
            call(0x82bad9a0, {0x1234, swap, Writer});
            call(0x82bada00, {0x12345678, swap, Writer});
            s.fpr_bits[1] = std::bit_cast<std::uint64_t>(1.25);
            call(0x82bada70, {0, swap, Writer});
            for (unsigned i = 0; i < 3; ++i)
                m.WriteU32(Positions + 4 * i, std::bit_cast<unsigned>(float(i + 2)));
            call(0x82badcd0, {Positions, 3, swap, Writer});
            call(0x82bade98,
                 {'C', 'V', 'X', 'M', cook_main_smoke::Count, cook_main_smoke::Count + 4, reader});
            if (s.r[3] != 1 || m.ReadU32(cook_main_smoke::Count) != 7 ||
                m.ReadU8(cook_main_smoke::Count + 4) != swap)
                throw std::runtime_error("NXS header roundtrip");
            call(0x82bad858, {swap, reader});
            if (s.r[3] != 0x1234)
                throw std::runtime_error("u16 roundtrip");
            call(0x82bad8b8, {swap, reader});
            if (s.r[3] != 0x12345678)
                throw std::runtime_error("u32 roundtrip");
            call(0x82bad928, {swap, reader});
            if (std::bit_cast<double>(s.fpr_bits[1]) != 1.25)
                throw std::runtime_error("f32 roundtrip");
            call(0x82badae0, {Triangles, 3, swap, reader});
            for (unsigned i = 0; i < 3; ++i)
                if (m.ReadU32(Triangles + 4 * i) != m.ReadU32(Positions + 4 * i))
                    throw std::runtime_error("word array roundtrip");
            if (m.ReadU32(reader + 4) != cook_main_smoke::Buffer + m.ReadU32(Writer + 4) ||
                !env.guest.live.empty())
                throw std::runtime_error("stream cursor/ownership");
            if (s.r[1] != initial.r[1] || s.lr != Address(initial.lr))
                throw std::runtime_error("codec ABI");
            constexpr unsigned view = 0x62000, viewTable = 0x63000;
            constexpr unsigned adapters[]{0x82b9d4c8u, 0x824b9f18u, 0x824b9f30u,
                                          0x82b9c788u, 0x82b9c7a0u, 0x82b9d4e0u};
            m.WriteU32(view, viewTable);
            m.WriteU32(view + 4, reader);
            for (unsigned j = 0; j < 6; ++j)
                m.WriteU32(viewTable + 4 + 4 * j, adapters[j]);
            m.WriteU8(cook_main_smoke::Buffer, 'I');
            m.WriteU8(cook_main_smoke::Buffer + 1, 'C');
            m.WriteU8(cook_main_smoke::Buffer + 2, 'E');
            m.WriteU32(reader + 4, cook_main_smoke::Buffer);
            call(0x82bd81b0,
                 {'C', 'V', 'X', 'M', cook_main_smoke::Count, cook_main_smoke::Count + 4, view});
            if (s.r[3] != 1 || m.ReadU32(cook_main_smoke::Count) != 7)
                throw std::runtime_error("ICE borrowed adapter header");
            for (unsigned width : {1u, 2u, 4u}) {
                unsigned maximum = width == 1 ? 255 : (width == 2 ? 65535 : 65536);
                for (unsigned j = 0; j < 3; ++j) {
                    unsigned value = j + 1;
                    if (width == 1)
                        m.WriteU8(Positions + j, value);
                    else if (width == 2)
                        m.WriteU16(Positions + 2 * j,
                                   swap ? __builtin_bswap16(std::uint16_t(value)) : value);
                    else
                        m.WriteU32(Positions + 4 * j, swap ? __builtin_bswap32(value) : value);
                }
                m.WriteU32(reader + 4, Positions);
                call(0x82bd8748, {maximum, 3, Triangles, view, swap});
                for (unsigned j = 0; j < 3; ++j)
                    if (m.ReadU32(Triangles + 4 * j) != j + 1)
                        throw std::runtime_error("adaptive index decode");
            }
            m.WriteU8(cook_main_smoke::Buffer, 'N');
            m.WriteU8(cook_main_smoke::Buffer + 1, 'X');
            m.WriteU8(cook_main_smoke::Buffer + 2, 'S');
            m.WriteU32(reader + 4, cook_main_smoke::Buffer);
            call(0x82bade98,
                 {'B', 'A', 'D', '!', cook_main_smoke::Count, cook_main_smoke::Count + 4, reader});
            if (s.r[3] != 0 || m.ReadU32(reader + 4) != cook_main_smoke::Buffer + 8)
                throw std::runtime_error("wrong tag consumption");
            m.WriteU32(0x83216624, AllocatorTable);
            m.WriteU32(AllocatorTable, Allocate | 1);
            m.WriteU32(AllocatorTable + 12, Free | 3);
            m.WriteU32(0x832dc180, swap ? 0 : 1);
            m.WriteU32(Writer + 4, 0);
            m.WriteU32(reader + 4, cook_main_smoke::Buffer);
            constexpr unsigned source = Owner + 88, destination = Owner + 200;
            m.WriteU32(source + 4, 3);
            m.WriteU32(source + 8, 6);
            m.WriteU32(source + 12, Polygons);
            m.WriteU32(source + 16, Positions);
            constexpr unsigned degree[]{2, 3, 1}, prefix[]{0, 2, 5};
            for (unsigned j = 0; j < 3; ++j) {
                m.WriteU16(Polygons + 4 * j, degree[j]);
                m.WriteU16(Polygons + 4 * j + 2, prefix[j]);
            }
            for (unsigned j = 0; j < 6; ++j)
                m.WriteU8(Positions + j, j + 10);
            s.r[3] = source;
            s.r[4] = Writer;
            (void)mesh_valence_stream61::Apply(0x82bbcc28, m, {env.guest, native}, s);
            if (s.r[3] != 1)
                throw std::runtime_error("valence write");
            s.r[3] = destination;
            s.r[4] = view;
            (void)mesh_valence_stream61::Apply(0x82bc7f98, m, {env.guest, native}, s);
            if (s.r[3] != 1 || m.ReadU32(destination + 4) != 3 || m.ReadU32(destination + 8) != 6)
                throw std::runtime_error("valence read counts");
            for (unsigned j = 0; j < 3; ++j)
                if (m.ReadU16(m.ReadU32(destination + 12) + 4 * j) != degree[j] ||
                    m.ReadU16(m.ReadU32(destination + 12) + 4 * j + 2) != prefix[j])
                    throw std::runtime_error("valence degree/prefix");
            for (unsigned j = 0; j < 6; ++j)
                if (m.ReadU8(m.ReadU32(destination + 16) + j) != j + 10)
                    throw std::runtime_error("valence edges");
            s.r[4] = m.ReadU32(destination);
            env.guest.CallIndirect(Free, m, s);
            if (!env.guest.live.empty() ||
                m.ReadU32(reader + 4) != cook_main_smoke::Buffer + m.ReadU32(Writer + 4))
                throw std::runtime_error("valence cursor/cleanup");
        }
        std::puts(
            "PASS NXS/ICE/adaptive indices and VALE adjacency roundtrips in both byte orders");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
