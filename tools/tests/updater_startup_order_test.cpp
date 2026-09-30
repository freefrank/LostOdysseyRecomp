#include "updater/update.h"

#include <iostream>
#include <filesystem>
#include <fstream>

int main()
{
    std::string error;
    const auto release = updater::ParseGitHubRelease(
        R"({"tag_name":"v9.9.9","body":"### English\n\nEnglish release note.\n\n### 简体中文\n\n中文发布说明。\n","assets":[]})",
        error);
    if (!release || release->changelogEnglish.find("English release note") == std::string::npos ||
        release->changelogChinese.find("中文发布说明") == std::string::npos)
    {
        std::cerr << "FAIL: release changelog language sections were not selected\n";
        return 1;
    }
    if (release->changelogEnglish.find("中文发布说明") != std::string::npos ||
        release->changelogChinese.find("English release note") != std::string::npos)
    {
        std::cerr << "FAIL: release changelog sections were mixed\n";
        return 1;
    }
    if (updater::ReleaseChangelog(*release, 0) != release->changelogEnglish ||
        updater::ReleaseChangelog(*release, 4) != release->changelogChinese ||
        updater::ReleaseChangelog(*release, 1) != release->changelogEnglish)
    {
        std::cerr << "FAIL: persisted UI language did not choose CN/EN release notes\n";
        return 1;
    }
    const auto reverse = updater::ParseGitHubRelease(
        R"({"tag_name":"v9.9.9","body":"### 简体中文\n\n中文在前。\n\n## Notes\nignored\n### English\n\nEnglish after Chinese.\n","assets":[]})",
        error);
    if (!reverse || reverse->changelogChinese.find("中文在前") == std::string::npos ||
        reverse->changelogChinese.find("ignored") != std::string::npos ||
        reverse->changelogEnglish.find("English after Chinese") == std::string::npos)
    {
        std::cerr << "FAIL: release sections were not bounded by the next heading\n";
        return 1;
    }
    const auto preferencesPath = std::filesystem::temp_directory_path() / "lo-startup-preferences-test.ini";
    {
        std::ofstream output(preferencesPath);
        output << "ui_language=4\nautomatic_updates=0\ngame_language=6\n";
    }
    const auto preferences = updater::ReadStartupPreferences(preferencesPath);
    std::filesystem::remove(preferencesPath);
    if (preferences.uiLanguage != 4 || preferences.automaticUpdates)
    {
        std::cerr << "FAIL: startup preference reader did not preserve Europe UI settings\n";
        return 1;
    }
    const auto planRoot = std::filesystem::temp_directory_path() / "lo-startup-plan-test";
    std::filesystem::create_directories(planRoot / "stage");
    updater::StagedUpdate staged;
    staged.version = "v9.9.9";
    staged.installRoot = planRoot;
    staged.operationRoot = planRoot / ".update";
    staged.stageRoot = planRoot / ".update" / "stage";
    staged.planPath = staged.operationRoot / "apply-plan.json";
    std::filesystem::create_directories(staged.operationRoot);
    if (!updater::WriteApplyPlan(staged, planRoot / "LostOdysseyRecomp.exe", {}, error, true))
    {
        std::cerr << "FAIL: startup apply plan could not be written\n";
        return 1;
    }
    const auto plan = updater::ReadApplyPlan(staged.planPath, error);
    std::filesystem::remove_all(planRoot);
    if (!plan || !plan->launchAfterApply)
    {
        std::cerr << "FAIL: startup apply plan did not retain automatic restart intent\n";
        return 1;
    }
    std::cout << "5 checks, 0 failures\n";
    return 0;
}
