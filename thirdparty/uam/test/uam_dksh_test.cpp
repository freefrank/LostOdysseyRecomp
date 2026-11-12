// uam_dksh_test: compiles GLSL files with uamlib (uam_api.h) and checks the DKSH bytes against reference
// .dksh files written by the uam command line tool for the same inputs (name_vs.glsl -> vertex, else
// fragment; <ref>/<name>.dksh). Also checks that a broken shader fails with a log and leaves the
// compiler usable. Exit status 0 only if everything matched.
//   uam_dksh_test <glsl dir> <reference dksh dir> [--every N] [--transient]
//   --every N    only every Nth file (by name), plus the 10 largest
//   --transient  init(false): rebuild Mesa's built-ins around every compile, as the uam tool does
#include "uam_api.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::vector<uint8_t> read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <glsl dir> <reference dksh dir> [--every N] [--transient]\n", argv[0]);
        return 2;
    }
    const fs::path glsl_dir = argv[1], ref_dir = argv[2];
    size_t every = 1;
    bool resident = true;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--every") && i + 1 < argc) every = std::max(1, atoi(argv[++i]));
        else if (!strcmp(argv[i], "--transient")) resident = false;
    }

    std::vector<fs::path> all;
    for (auto& e : fs::directory_iterator(glsl_dir))
        if (e.path().extension() == ".glsl" && e.path().filename().string()[0] != '.') all.push_back(e.path());
    std::sort(all.begin(), all.end());
    std::vector<fs::path> files;
    for (size_t i = 0; i < all.size(); i += every) files.push_back(all[i]);
    if (every > 1) {
        std::vector<fs::path> by_size = all;
        std::sort(by_size.begin(), by_size.end(),
                  [](const fs::path& a, const fs::path& b) { return fs::file_size(a) > fs::file_size(b); });
        for (size_t i = 0; i < std::min<size_t>(10, by_size.size()); i++)
            if (std::find(files.begin(), files.end(), by_size[i]) == files.end()) files.push_back(by_size[i]);
    }

    uam::init(resident);
    size_t same = 0, differ = 0, failed = 0, no_ref = 0, logged = 0, bytes = 0;
    double total_ms = 0, max_ms = 0;
    for (const fs::path& p : files) {
        const std::string name = p.stem().string();
        const bool vs = name.size() > 3 && name.compare(name.size() - 3, 3, "_vs") == 0;
        std::vector<uint8_t> src = read_file(p);
        src.push_back(0);
        const auto t0 = std::chrono::steady_clock::now();
        uam::Result r = uam::compile(vs ? uam::Stage::Vertex : uam::Stage::Fragment, (const char*)src.data());
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        total_ms += ms;
        max_ms = std::max(max_ms, ms);
        if (!r.log.empty()) logged++;
        if (!r.ok) {
            failed++;
            printf("FAIL %s: %s\n", name.c_str(), r.log.c_str());
            continue;
        }
        bytes += r.dksh.size();
        const fs::path ref = ref_dir / (name + ".dksh");
        if (!fs::exists(ref)) {
            no_ref++;
            printf("NOREF %s\n", name.c_str());
            continue;
        }
        if (read_file(ref) == r.dksh) {
            same++;
        } else {
            differ++;
            printf("DIFF %s (%zu bytes, reference %zu)\n", name.c_str(), r.dksh.size(), (size_t)fs::file_size(ref));
        }
    }

    // the error path: a compile error is reported in the log, and the next compile still works
    uam::Result bad = uam::compile(uam::Stage::Fragment, "#version 460\nvoid main() { undefined_thing = 1; }\n");
    const bool bad_ok = !bad.ok && bad.dksh.empty() && bad.log.find("undefined_thing") != std::string::npos;
    uam::Result good = uam::compile(uam::Stage::Fragment,
                                    "#version 460\nlayout(location=0) out vec4 c;\nvoid main() { c = vec4(1.0); }\n");
    const bool recover_ok = good.ok && !good.dksh.empty();
    printf("error path: %s (log: %s)", bad_ok ? "ok" : "WRONG", bad.log.c_str());
    printf("compile after the error: %s\n", recover_ok ? "ok" : "WRONG");
    uam::shutdown();

    printf("%zu shaders (%s frontend): %zu identical, %zu differ, %zu failed, %zu without reference, "
           "%zu with log output; %zu DKSH bytes; compile %.0f ms total, mean %.1f ms, max %.1f ms\n",
           files.size(), resident ? "resident" : "transient", same, differ, failed, no_ref, logged, bytes,
           total_ms, files.empty() ? 0.0 : total_ms / files.size(), max_ms);
    return (differ || failed || no_ref || !bad_ok || !recover_ok || files.empty()) ? 1 : 0;
}
