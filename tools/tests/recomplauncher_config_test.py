#!/usr/bin/env python3
"""Round-trip every launcher choice through production Read/Validate/WriteConfig.

The fixture uses the real config type and parser/writer bodies, with only the
renderer log sink stubbed. It needs a Linux C++20 compiler, no game data or GPU.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import sys

from guest_main_exit_regression import function

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from recomplauncher_linux import profile as linux_profile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cxx', default='c++')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = (ROOT / 'LostOdysseyRecomp/settings/config.cpp').read_text(encoding='utf-8')
    # Keep all helpers and validation exactly as compiled by the runtime.
    start = source.index('namespace settings\n')
    end = source.index('Config &Current()')
    harness = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <iostream>
#include "settings/config.h"
#include "settings/ini.h"
#include "os/user_paths.h"
#include "debug/fast_forward.h"
#include "gpu/frame_rate.h"
'''
    harness += source[start:end] + '\n} }\n'
    harness += '\nnamespace settings { void LogSettingsSaved(const Config&) {}\n'
    harness += function(source, 'static bool WriteConfig(const Config &value)') + '\n}\n'
    # Exercise the actual early Windows backend reader with only Win32 loading stubbed.
    harness += r'''
#define LOG_WARNING(...) ((void)0)
#define LOG_INFO(...) ((void)0)
using HMODULE = void*;
constexpr int LOAD_LIBRARY_SEARCH_SYSTEM32 = 0, MAX_PATH = 260;
bool loadedSystem = false;
HMODULE LoadLibraryExW(const wchar_t*, void*, int) { loadedSystem = true; return nullptr; }
unsigned GetModuleFileNameW(HMODULE, wchar_t*, int) { return 0; }
'''
    main_source = (ROOT / 'LostOdysseyRecomp/main.cpp').read_text(encoding='utf-8')
    harness += function(main_source, 'static void PreferSystemDxgiOnVulkan(')
    harness += r'''
int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "dxgi") {
        PreferSystemDxgiOnVulkan(std::filesystem::current_path());
        std::cout << loadedSystem;
        return 0;
    }
    return settings::WriteConfig(settings::Read()) ? 0 : 1;
}
'''
    fixture = out / 'config.cpp'
    fixture.write_text(harness, encoding='utf-8')
    executable = out / 'config-test'
    subprocess.run([args.cxx, '-std=c++20', '-pthread', '-O1', '-I',
                    str(ROOT / 'LostOdysseyRecomp'), str(fixture), '-o', str(executable)],
                   check=True, timeout=120)
    profile = json.loads((ROOT / 'recomplauncher.json').read_text(encoding='utf-8'))
    defaults = {}
    for row in profile['settings']:
        defaults.update(row.get('defaults', {}))
    count = 0
    with tempfile.TemporaryDirectory(prefix='cases-', dir=out) as directory:
        config = Path(directory) / 'settings.ini'

        def roundtrip(values):
            # The whitespace and CRLF are what exposed the fresh-install bug.
            config.write_bytes(''.join(f'\t{k} = {v} \r\n' for k, v in values.items()).encode('utf-8'))
            subprocess.run([str(executable)], cwd=directory, check=True, timeout=5)
            return dict(line.split('=', 1) for line in config.read_text(encoding='utf-8').splitlines())

        for platform_profile in (profile, linux_profile()):
            platform_defaults = {}
            for row in platform_profile['settings']:
                platform_defaults.update(row.get('defaults', {}))
            for row in platform_profile['settings']:
                for choice in row['choices']:
                    values = dict(platform_defaults, **row.get('when', {}), **choice['set'])
                    actual = roundtrip(values)
                    for key, expected in choice['set'].items():
                        # The Windows profile includes D3D12; POSIX selects Vulkan.
                        if key == 'graphics_backend':
                            expected = '1'
                        if actual.get(key) != expected:
                            raise AssertionError(f'{row["id"]}/{choice["label"]}: {key}={actual.get(key)!r}, expected {expected!r}')
                    count += 1
        actual = roundtrip(dict(defaults, frame_generation_provider='2', frame_generation_multiplier='3'))
        assert actual['frame_generation_multiplier'] == '2', 'FSR must normalize to 2x'
        actual = roundtrip(dict(defaults, frame_rate='invalid', hdr='2', display_brightness='-99'))
        assert actual['frame_rate'] == '30' and actual['hdr'] == '0' and actual['display_brightness'] == '-20'
        rows = {row['id']: row for row in profile['settings']}
        quality_ids = ['dlss_quality', 'fsr_quality', 'xess_quality']
        for provider, expected in [('0', []), ('1', ['dlss_quality']),
                                   ('2', ['fsr_quality']), ('4', ['xess_quality'])]:
            enabled = [key for key in quality_ids
                       if all({'upscaler': provider}.get(k) == v
                              for k, v in rows[key].get('when', {}).items())]
            assert enabled == expected, f'wrong quality rows enabled for provider {provider}: {enabled}'
        assert rows['fsr_quality']['choices'] == rows['xess_quality']['choices'], 'FSR/XeSS must share persisted quality values'
        provider = next(c for c in rows['frame_generation']['choices'] if c['label'] == 'DLSS')
        actual = roundtrip(dict(defaults, **provider['set'], frame_generation_multiplier='6'))
        assert all(actual[k] == v for k, v in provider['set'].items()), 'DLSS 6x must still match the DLSS provider row'
        adaptive = rows['vsync']['choices'][2]
        actual = roundtrip(dict(defaults, vsync='0', variable_refresh_rate='1'))
        assert all(actual[k] == v for k, v in adaptive['set'].items()), 'adaptive synchronization must match with ordinary V-Sync off'
        (Path(directory) / 'dxgi.dll').write_bytes(b'synthetic local loader')
        environment = dict(os.environ)
        environment.pop('LO_GRAPHICS_API', None)
        for value, expected in [('0', '0'), ('1', '1'), ('invalid', '1'), ('', '1')]:
            config.write_text(f' graphics_backend = {value} \n', encoding='utf-8')
            actual = subprocess.check_output([str(executable), 'dxgi'], cwd=directory,
                                             env=environment, text=True, timeout=5)
            assert actual == expected, f'early DXGI selection ignored spaced backend {value!r}'
    print(f'PASS: {count} launcher choices round-trip through production config; malformed values stay bounded')


if __name__ == '__main__':
    main()
