from pathlib import Path
p = Path('tools/tests/anisotropic_filtering/CMakeLists.txt')
s = p.read_text()
needle = '    target_compile_features(${target} PRIVATE cxx_std_20)'
assert s.count(needle) == 1
p.write_text(s.replace(needle, needle + '\n    target_compile_definitions(${target} PRIVATE NOMINMAX)'))
p = Path('.github/workflows/af-regression.yml')
s = p.read_text()
needle = '        run: git submodule update --init --depth 1 thirdparty/plume\n'
assert s.count(needle) == 1
s = s.replace(needle, '''        shell: bash
        run: |
          git submodule update --init --depth 1 thirdparty/plume
          git -C thirdparty/plume apply ../../tools/patches/plume-lostodyssey.patch
          if [ "$RUNNER_OS" = Linux ]; then sudo apt-get update -qq && sudo apt-get install -y libx11-dev; fi
''')
p.write_text(s)
