#!/usr/bin/env python3
"""Build the standalone TV diagnostic APK using only JDK 17 and Android SDK 35.

No Gradle, NDK, native libraries, private input, or game files required.
The APK has its own package ID and never replaces the installed game.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import zipfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', default=os.environ.get('ANDROID_HOME') or os.environ.get('ANDROID_SDK_ROOT'), type=Path)
    parser.add_argument('--jdk', default=os.environ.get('JAVA_HOME'), type=Path)
    parser.add_argument('--out', default=ROOT / 'out/tv-probe', type=Path)
    args = parser.parse_args()
    if not args.sdk or not args.jdk:
        parser.error('provide --sdk and --jdk (or ANDROID_HOME and JAVA_HOME)')
    sdk, jdk, out = args.sdk.resolve(), args.jdk.resolve(), args.out.resolve()
    tools = sdk / 'build-tools/35.0.0'
    android = sdk / 'platforms/android-35/android.jar'
    probe = ROOT / 'packaging/android/tvprobe/src/main'
    shared = ROOT / 'packaging/android/runtime/src/main'
    out.mkdir(parents=True, exist_ok=True)
    manifest = ET.parse(probe / 'AndroidManifest.xml')
    manifest.getroot().set('package', 'io.github.freefrank.lostodyssey.tvprobe')
    manifest.getroot().find('application').set('{http://schemas.android.com/apk/res/android}debuggable', 'true')
    manifest.write(out / 'AndroidManifest.xml', encoding='utf-8', xml_declaration=True)
    res = out / 'res'
    for locale in ('values', 'values-it'):
        (res / locale).mkdir(parents=True, exist_ok=True)
        shutil.copy2(shared / 'res' / locale / 'tv.xml', res / locale / 'tv.xml')
    run(tools / 'aapt2', 'compile', '--dir', res, '-o', out / 'resources.zip')
    run(tools / 'aapt2', 'link', '-I', android, '--manifest', out / 'AndroidManifest.xml',
        '--custom-package', 'io.github.freefrank.lostodyssey', '--min-sdk-version', '26',
        '--target-sdk-version', '35', '--version-code', '1', '--version-name', '0.1.0-tv',
        '--java', out / 'gen', '-o', out / 'unsigned.apk', out / 'resources.zip')
    sources = list((probe / 'java').rglob('*.java')) + list((out / 'gen').rglob('*.java'))
    sources += [shared / 'java/io/github/freefrank/lostodyssey' / (name + '.java') for name in
                ('TvModePolicy', 'TvOutputController', 'TvDisplayDiagnostics', 'TvSettingsDialog', 'TvAudioProbe')]
    run(jdk / 'bin/javac', '-source', '17', '-target', '17', '-classpath', android,
        '-d', out / 'classes', *sources)
    dex = out / 'dex'
    dex.mkdir(exist_ok=True)
    run(jdk / 'bin/java', '-cp', tools / 'lib/d8.jar', 'com.android.tools.r8.D8', '--min-api', '26',
        '--lib', android, '--output', dex, *sorted((out / 'classes').rglob('*.class')))
    with zipfile.ZipFile(out / 'unsigned.apk', 'a', zipfile.ZIP_DEFLATED) as apk:
        for path in dex.glob('*.dex'):
            apk.write(path, path.name)
    run(tools / 'zipalign', '-f', '4', out / 'unsigned.apk', out / 'aligned.apk')
    keystore = out / 'diagnostic.keystore'
    if not keystore.exists():
        run(jdk / 'bin/keytool', '-genkeypair', '-keystore', keystore, '-storepass', 'android', '-keypass', 'android',
            '-alias', 'tvprobe', '-dname', 'CN=Lost Odyssey TV Diagnostic', '-keyalg', 'RSA', '-validity', '10000')
    result = out / 'LostOdyssey-TV-Probe.apk'
    run(jdk / 'bin/java', '-jar', tools / 'lib/apksigner.jar', 'sign', '--ks', keystore, '--ks-pass', 'pass:android',
        '--out', result, out / 'aligned.apk')
    run(jdk / 'bin/java', '-jar', tools / 'lib/apksigner.jar', 'verify', '--verbose', result)
    run(tools / 'zipalign', '-c', '4', result)
    print(result)


if __name__ == '__main__':
    main()
