#include "updater/apply_arguments.h"

#include <iostream>

int main()
{
    struct Case { const wchar_t *command; bool requested; };
    const Case cases[] = {
        {LR"("C:\Games\LO--apply-plan\LostOdysseyRecomp.exe")", false},
        {LR"(game.exe --game "C:\Games\disc --apply-plan backup")", false},
        {LR"(game.exe --apply-plan-backup plan.json)", false},
        {LR"(game.exe --apply-plan=plan.json)", false},
        {LR"(game.exe --apply-plan plan.json --wait-process 123)", true},
        {LR"(game.exe "--apply-plan" "C:\Games\staged plan.json")", true},
        {LR"(game.exe --apply-plan)", true},
        {LR"("--apply-plan")", false},
    };
    for (const auto &test : cases)
    {
        if (updater::RequestsApplyMode(test.command) != test.requested)
        {
            std::wcerr << L"FAIL: " << test.command << L'\n';
            return 1;
        }
    }
    std::cout << "8 apply argument checks, 0 failures\n";
    return 0;
}
