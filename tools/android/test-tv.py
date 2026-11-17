#!/usr/bin/env python3
"""Run TV selection/sizing and existing touch visibility regressions without Android."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jdk', type=Path, default=os.environ.get('JAVA_HOME'))
    args = parser.parse_args()
    javac = args.jdk / 'bin/javac' if args.jdk else shutil.which('javac')
    java = args.jdk / 'bin/java' if args.jdk else shutil.which('java')
    if not javac or not java:
        parser.error('JDK required')
    sources = ROOT / 'packaging/android/runtime/src/main/java/io/github/freefrank/lostodyssey'
    with tempfile.TemporaryDirectory(prefix='lo-tv-test-') as temporary:
        out = Path(temporary)
        subprocess.run([str(javac), '-d', str(out), str(sources / 'TvModePolicy.java'),
                        str(sources / 'TouchControllerVisibility.java'), str(ROOT / 'tools/tests/TvModePolicyTest.java'),
                        str(ROOT / 'tools/tests/TouchControllerVisibilityTest.java')], check=True)
        for test in ('TvModePolicyTest', 'TouchControllerVisibilityTest'):
            subprocess.run([str(java), '-cp', str(out), 'io.github.freefrank.lostodyssey.' + test], check=True)
        for test in ('android_tv', 'aspect_layout', 'render_resolution', 'frame_pacer'):
            subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
                            '-I', str(ROOT / 'LostOdysseyRecomp'), str(ROOT / ('tools/tests/' + test + '_test.cpp')),
                            '-o', str(out / test)], check=True)
            subprocess.run([str(out / test)], check=True)


if __name__ == '__main__':
    main()
