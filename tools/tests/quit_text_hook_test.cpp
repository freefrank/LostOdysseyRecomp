// Synthetic guest-memory contract for the original lookup wrapper; no game or
// game resource files are needed. Compile independently from the runtime.
#include <settings/quit_text_hook.h>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
union Register { uint64_t u64 = 0; uint32_t u32; };
struct Context { Register r3, r4; };
void Check(bool condition, const char* reason)
{
    if (!condition) throw std::runtime_error(reason);
}
struct Guest
{
    std::vector<uint8_t> memory = std::vector<uint8_t>(0x10000);
    uint32_t next = 0x2000;
    unsigned allocations = 0, originals = 0;
    settings::quit_text::Cache cache;

    void Lookup(Context& ctx, uint32_t language)
    {
        settings::quit_text::Lookup(ctx, memory.data(), language, cache,
            [this](Context& call, uint8_t*) {
                ++originals;
                const auto id = call.r4.u32;
                call.r4.u32 = 0xBADu; // guest call clobbers caller-saved r4
                call.r3.u64 = id == settings::quit_text::LabelId ? 0x1000 :
                    id == settings::quit_text::HelpId ? 0x1100 :
                    id == settings::quit_text::PromptId ? 0x1200 : 0x1300;
            },
            [this](size_t size) {
                ++allocations;
                if (next + size > memory.size()) throw std::runtime_error("guest allocation");
                const auto result = next;
                next += uint32_t(size + 15) & ~15u;
                return result;
            });
    }

    uint32_t CheckText(uint32_t id, uint32_t language)
    {
        Context ctx{};
        ctx.r4.u32 = id;
        Lookup(ctx, language);
        const auto text = settings::quit_text::Text(id, language);
        Check(ctx.r3.u32 != 0x1000 && ctx.r3.u32 != 0x1100 &&
            ctx.r3.u32 != 0x1200, "target must return private guest row");
        const auto* row = memory.data() + ctx.r3.u32;
        Check(settings::quit_text::Read32(row) == id, "row id");
        Check(settings::quit_text::Read32(row + 4) == 0x12345678, "other fields unchanged");
        Check(settings::quit_text::Read32(row + 12) == text.size() + 1,
            "copied UTF-16 count must include terminator");
        const auto chars = settings::quit_text::Read32(row + 8);
        Check(chars >= 0x2000 && chars < memory.size(), "guest pointer");
        for (size_t i = 0; i < text.size(); ++i)
            Check(memory[chars + 2 * i] == uint8_t(text[i] >> 8) &&
                memory[chars + 2 * i + 1] == uint8_t(text[i]), "UTF-16BE content");
        Check(memory[chars + text.size() * 2] == 0 &&
            memory[chars + text.size() * 2 + 1] == 0, "terminator");

        // Model 822B3F50: it deep-copies precisely (row+12)*2 bytes, with no
        // extra zero write. A destination with stale 'a0_m' after the copied
        // range exposes the original bug if the count excludes the terminator.
        const auto count = settings::quit_text::Read32(row + 12);
        std::vector<uint8_t> owned((count + 5) * 2, 0);
        for (size_t i = 0; i < 4; ++i)
            owned[text.size() * 2 + i * 2 + 1] = uint8_t("a0_m"[i]);
        std::memcpy(owned.data(), memory.data() + chars, count * 2);
        Check(owned[text.size() * 2] == 0 && owned[text.size() * 2 + 1] == 0,
            "deep copy must overwrite stale trailing bytes with terminator");
        std::u16string observed;
        for (size_t i = 0; i < owned.size() / 2; ++i)
        {
            const auto unit = char16_t(owned[i * 2] << 8 | owned[i * 2 + 1]);
            if (!unit) break;
            observed.push_back(unit);
        }
        Check(observed == text, "terminated deep copy must not expose stale suffix");
        return ctx.r3.u32;
    }
};
}

int main()
{
    using namespace settings::quit_text;
    Guest guest;
    for (size_t i = 0; i < std::size(TextIds); ++i)
    {
        auto* source = guest.memory.data() + 0x1000 + i * 0x100;
        Write32(source, TextIds[i]);
        Write32(source + 4, 0x12345678);
    }
    Write32(guest.memory.data() + 0x1300, 1234);

    Context unrelated{};
    unrelated.r4.u32 = 1234;
    guest.Lookup(unrelated, 1);
    Check(unrelated.r3.u32 == 0x1300 && guest.allocations == 0, "unrelated id original return");
    for (uint32_t language = 1; language <= 9; ++language)
        for (auto id : TextIds) Check(!Text(id, language).empty(), "all resource languages translated");
    Check(Text(LabelId, 1) == u"Quit to Desktop" && Text(LabelId, 2) == u"デスクトップに戻る" &&
        Text(LabelId, 8) == u"退出到桌面" && Text(LabelId, 9) == u"退出到桌面", "requested labels");
    Check(Text(PromptId, 1).find(u"Unsaved progress") != std::u16string_view::npos,
        "native confirmation must mention lost progress");

    const auto en = guest.CheckText(LabelId, 1);
    Check(guest.CheckText(LabelId, 1) == en && guest.allocations == 1, "stable owned row");
    const auto zh = guest.CheckText(LabelId, 9);
    Check(en != zh && guest.allocations == 2, "switch language with distinct guest row");
    Check(guest.CheckText(LabelId, 1) == en, "old language remains stable");
    for (uint32_t language = 1; language <= 9; ++language)
    {
        if (language != 1 && language != 9) guest.CheckText(LabelId, language);
        guest.CheckText(HelpId, language);
        guest.CheckText(PromptId, language);
    }
    Check(guest.allocations == 27 && guest.originals == 30,
        "original invoked every time; guest rows cached per id and language");

    Context unknown{};
    unknown.r4.u32 = LabelId;
    guest.Lookup(unknown, 10);
    Check(unknown.r3.u32 == 0x1000 && guest.allocations == 27, "unknown language fallback");
    Write32(guest.memory.data() + 0x1000, 0);
    Guest missing;
    missing.memory = guest.memory;
    Context unresolved{};
    unresolved.r4.u32 = LabelId;
    missing.Lookup(unresolved, 1);
    Check(unresolved.r3.u32 == 0x1000 && missing.allocations == 0,
        "missing label falls back to original row");
}
