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
            m.WriteU32(reader + 4, cook_main_smoke::Buffer);
            call(0x82bade98,
                 {'B', 'A', 'D', '!', cook_main_smoke::Count, cook_main_smoke::Count + 4, reader});
            if (s.r[3] != 0 || m.ReadU32(reader + 4) != cook_main_smoke::Buffer + 8)
                throw std::runtime_error("wrong tag consumption");
        }
        std::puts(
            "PASS NXS and scalar/array stream roundtrips in both byte orders, wrong-tag rejection");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
