#include "install/file_browser.h"

#include <chrono>
#include <fstream>
#include <iostream>

int main()
{
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() /
        ("lo-folder-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directory(root);
    int failures = 0;
    auto check = [&](bool condition, const char* name) {
        if (!condition) { std::cerr << name << " failed\n"; ++failures; }
    };
    const auto created = install::ui::CreateFolder(root, "New Folder");
    check(bool(created) && fs::is_directory(created.path), "create default name");
    check(!install::ui::CreateFolder(root, "New Folder") && fs::is_directory(created.path), "reject duplicate");
    check(!install::ui::CreateFolder(root, "../escape"), "reject traversal");
    check(!install::ui::CreateFolder(root, ""), "reject empty name");
    const auto unicode = install::ui::CreateFolder(root, "新建文件夹");
    check(bool(unicode) && fs::is_directory(unicode.path), "create UTF-8 name");
    const auto parentFile = root / "file";
    { std::ofstream out(parentFile); out << "file"; }
    check(!install::ui::CreateFolder(parentFile, "child"), "reject non-directory parent");
    fs::remove_all(root);
    return failures ? 1 : 0;
}
