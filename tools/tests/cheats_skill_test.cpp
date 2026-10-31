#include <debug/cheats.h>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <set>
using namespace debug_menu::cheats;

namespace {
unsigned checks = 0;
void Check(bool condition) {
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAIL skill check %u\n", checks); std::abort(); }
}
void Put(Memory m, uint32_t p, uint32_t value) {
    const Write write{p, value, 4}; Commit(m, {&write, 1});
}
void Number(Memory m, uint32_t p, float value) { Put(m, p, std::bit_cast<uint32_t>(value)); }
std::vector<uint8_t> Fixture() {
    // Trailing sentinels remain outside the guest span supplied to Plan.
    std::vector<uint8_t> bytes(data::DataSize + 32, 0x35);
    Memory m{std::span(bytes).first(data::DataSize)};
    Put(m, Gold, 1000);
    for (unsigned i = 0; i < 5; ++i) Put(m, Party + i * 4, i);
    for (unsigned c = 0; c < 9; ++c) {
        const auto b = c * data::CharacterStride;
        Number(m, Hp+b, 10); Number(m, MaxHp+b, 100+c);
        Number(m, Mp+b, 5); Number(m, MaxMp+b, 50+c); Number(m, Exp+b, 42);
    }
    for (const auto& e : data::Items) Number(m, e.value, 7);
    for (const auto& e : data::Materials) Number(m, e.value, 9);
    return bytes;
}
}

int main() {
    static_assert(ValidSkillCatalog());
    static_assert(std::size(data::LearnedSkills) == 172);
    constexpr uint32_t recovered[] = {0xEA8,0xEB8,0xEC8,0xED8,0xEE8,0xEF8,0xF08,0xF18,0xF58,0xF68,0xF98};
    unsigned partial = 0, ultimate = 0;
    for (const auto& e : data::LearnedSkills) {
        bool added = std::find(std::begin(recovered), std::end(recovered), e.value) != std::end(recovered);
        Check(e.value < data::CharacterStride);
        if (added) ++partial;
        if (e.value == 0xD08 || e.value == 0x1888) ++ultimate;
    }
    Check(partial == 11 && ultimate == 2);
    auto bytes = Fixture(); Memory m{std::span(bytes).first(data::DataSize)};
    Check(Validate(m));
    for (unsigned c = 0; c < 9; ++c) {
        for (unsigned i = 0; i < std::size(data::LearnedSkills); ++i) {
            bytes = Fixture(); m.bytes = std::span(bytes).first(data::DataSize); auto expected = bytes;
            auto plan = Plan(m, {Action::LearnSkill, c, i});
            const auto offset = c * data::CharacterStride + data::LearnedSkills[i].value;
            Check(plan && plan->size() == 1 && (*plan)[0].offset == offset && (*plan)[0].bytes == 1);
            Check(bytes == expected); expected[offset] |= 0x80;
            Commit(m, *plan); Check(bytes == expected);
            auto again = Plan(m, {Action::LearnSkill, c, i}); Check(bool(again));
            Commit(m, *again); Check(bytes == expected);
        }
        bytes = Fixture(); m.bytes = std::span(bytes).first(data::DataSize); auto expected = bytes;
        auto plan = Plan(m, {Action::LearnSupportedSkills, c});
        Check(plan && plan->size() == std::size(data::LearnedSkills));
        std::set<uint32_t> offsets, expectedOffsets;
        for (const auto& e : data::LearnedSkills) {
            const auto offset = c * data::CharacterStride + e.value;
            expectedOffsets.insert(offset); expected[offset] |= 0x80;
        }
        for (const auto& w : *plan) {
            Check(w.bytes == 1 && w.offset < data::DataSize); Check(offsets.insert(w.offset).second);
            Check(expectedOffsets.contains(w.offset)); Check(w.value == (bytes[w.offset] | 0x80u));
        }
        Check(offsets == expectedOffsets);
        Check(bytes != expected); Check(Validate(m)); Commit(m, *plan); Check(bytes == expected);
    }
    // Exercise every combination of non-learned bits, including already learned.
    for (unsigned bits = 0; bits < 256; ++bits) {
        const auto offset = data::LearnedSkills[0].value;
        bytes[offset] = uint8_t(bits);
        auto plan = Plan(m, {Action::LearnSkill, 0, 0}); Check(bool(plan)); Commit(m, *plan);
        Check(bytes[offset] == uint8_t(bits | 0x80));
    }
    const auto before = bytes;
    for (unsigned c : {9u, UINT32_MAX})
        for (auto action : {Action::LearnSkill, Action::LearnSupportedSkills}) Check(!Plan(m, {action,c}));
    Check(!Plan(m, {Action::LearnSkill,0,unsigned(std::size(data::LearnedSkills))}));
    Check(!Plan(m, {Action::LearnSkill,0,UINT32_MAX}));
    for (size_t size : {size_t(0), size_t(4), size_t(data::DataSize-1)})
        Check(!Plan(Memory{std::span(bytes).first(size)}, {Action::LearnSupportedSkills,0}));
    Check(bytes == before);
    Number(m, MaxHp + 8 * data::CharacterStride, 0);
    Check(!Plan(m, {Action::LearnSkill,8,0})); bytes = before;
    // Queue validation and cancellation still protect the new skills.
    Session session; const Context context{0x100008,0x200000,0x300000};
    session.Tick(context,m,true); session.Enable(true);
    const auto index = unsigned(std::find_if(std::begin(data::LearnedSkills), std::end(data::LearnedSkills),
        [](const auto& e) { return e.value == 0xF68; }) - std::begin(data::LearnedSkills));
    Request request{Action::LearnSkill,6,index,0,session.Get().generation};
    Check(session.Queue(request)); session.Tick(context,m,true);
    Check(session.Get().result == Result::Applied);
    Check(m.U8(6*data::CharacterStride+0xF68) == uint8_t(before[6*data::CharacterStride+0xF68] | 0x80));
    bytes = before;
    request.character = 0; Check(session.Queue(request));
    session.Tick({context.root,context.world+4,context.level},m,true);
    Check(session.Get().result == Result::Cancelled && bytes == before);
    // A representative unrelated action retains its prior result.
    auto gold = Plan(m, {Action::AddGold,0,0,100}); Check(bool(gold)); Commit(m,*gold);
    Check(m.U32(Gold) == 1100);
    std::printf("PASS: %u skill checks; all nine characters, individual/bulk bytes, bit preservation, cancellation\n", checks);
}
