#include "install/compaction_prompt.h"
#include "host_ui/rasterizer.h"
#include "settings/translations.h"
#include <cassert>
#include <iostream>
#include <utility>

using namespace install::compaction;

int main()
{
    assert(StartupMode(false, true, "") == Mode::Ask);
    assert(StartupMode(false, false, "") == Mode::Skip);
    assert(StartupMode(true, true, "") == Mode::Skip);
    assert(StartupMode(true, false, "") == Mode::Skip);
    for (bool automated : {false, true})
        for (bool ask : {false, true}) {
            assert(StartupMode(automated, ask, "0") == Mode::Skip);
            assert(StartupMode(automated, ask, "1") == Mode::Run);
        }
    assert(StartupMode(false, true, "invalid") == Mode::Ask);
    assert(StartupMode(true, true, "invalid") == Mode::Skip);

    Offer defaultOffer;
    assert(defaultOffer.selected == Choice::Later && !defaultOffer.finished);
    defaultOffer.Apply(Action::None);
    assert(defaultOffer.selected == Choice::Later && !defaultOffer.finished);
    defaultOffer.Apply(Action::Activate);
    assert(defaultOffer.selected == Choice::Later && defaultOffer.finished);

    Offer free;
    free.Apply(Action::Previous);
    assert(free.selected == Choice::Free && !free.finished);
    free.Apply(Action::Activate);
    assert(free.selected == Choice::Free && free.finished);
    free.Apply(Action::Never);
    free.Apply(Action::Next);
    assert(free.selected == Choice::Free && free.finished);

    Offer never;
    never.Apply(Action::Next);
    assert(never.selected == Choice::Never && !never.finished);
    never.Apply(Action::Activate);
    assert(never.selected == Choice::Never && never.finished);

    Offer cycle;
    cycle.Apply(Action::Next);
    cycle.Apply(Action::Next);
    assert(cycle.selected == Choice::Free);
    cycle.Apply(Action::Previous);
    assert(cycle.selected == Choice::Never);
    cycle.Apply(Action::Previous);
    assert(cycle.selected == Choice::Later);
    cycle.Apply(Action::Previous);
    cycle.Apply(Action::Later); // Escape / gamepad B cancels even with Free selected.
    assert(cycle.selected == Choice::Later && cycle.finished);

    for (const auto [action, choice] : {
            std::pair{Action::Free, Choice::Free},
            std::pair{Action::Later, Choice::Later},
            std::pair{Action::Never, Choice::Never},
            std::pair{Action::Close, Choice::Closed}}) {
        Offer offer;
        offer.Apply(action);
        assert(offer.finished && offer.selected == choice);
    }
    struct Line { const wchar_t* english; const wchar_t* traditional; float scale; int width; };
    const Line lines[] = {
        {L"Free disk space", L"釋放磁碟空間", 1, 1048},
        {L"Free about %s GB?", L"釋放約 %s GB？", 1.5f, 1048},
        {L"Your discs store some files twice.", L"您的光碟中儲存了部分重複檔案。", 1, 1048},
        {L"This takes a few minutes and runs once.", L"這需要幾分鐘，只需執行一次。", 1, 1048},
        {L"Free space (A)", L"釋放空間 (A)", 1, 328},
        {L"Not now (B)", L"暫時不要 (B)", 1, 328},
        {L"Don't ask again", L"不再詢問", 1, 328},
        {L"Comparing disc archives", L"正在比對光碟檔案", 1.2f, 1048},
        {L"Cancel (B)", L"取消 (B)", 1, 328},
        {L"Compaction cancelled.", L"已取消空間整理。", 1, 1048},
        {L"Game files were kept. You can try again later.", L"遊戲檔案已保留，可稍後再試。", 1, 1048},
        {L"Freed %s GB.", L"已釋放 %s GB。", 1, 1048},
        {L"Some files could not be shared.", L"部分檔案無法共用。", 1, 1048},
        {L"Continue (A)", L"繼續 (A)", 1, 328},
    };
    host_ui::PixelBuffer pixels;
    host_ui::Rasterizer rasterizer(pixels);
    for (uint32_t language = 0; language < 5; ++language)
        for (const auto& line : lines) {
            std::wstring text = settings::Translate(language, line.english, line.traditional);
            if (auto position = text.find(L"%s"); position != std::wstring::npos)
                text.replace(position, 2, L"999.9");
            const auto width = rasterizer.MeasureWString(text, line.scale);
            if (width > line.width) {
                std::cerr << "Prompt text overflows: language=" << language << " width=" << width
                          << " available=" << line.width << '\n';
                return 1;
            }
        }
    std::cout << "PASS 14 prompt strings in all five languages at production font scales\n";
    std::cout << "PASS compaction startup policy, safe default, choices, navigation and completed input\n";
}
