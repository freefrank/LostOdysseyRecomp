#!/usr/bin/env python3
"""Synthetic CPU-only merge CLI checks; no DXC, GPU or real shader corpus."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='lo-pack-merge-') as directory:
        root = Path(directory)
        subprocess.run([args.fixtures, root], check=True, capture_output=True)
        baseline = root / 'matching.lospv'
        image = root / 'image.bin'
        source = root / 'source.bin'
        hash_value = 0xcbf29ce484222325
        for byte in source.read_bytes():
            hash_value = ((hash_value ^ byte) * 0x100000001b3) & ((1 << 64) - 1)
        header = 'action\tstage\thash\tsource\tprovenance\n'
        row = f'include\tvs\t{hash_value:016x}\tsource.bin\tsynthetic-existing\n'
        excluded = 'exclude\tps\t0000000000000123\t-\taudited-exclusion\n'
        manifest = root / 'approved.tsv'
        output = root / 'merged'
        pack = output / 'portable_vk.lospv'
        report = output / 'merge-report.json'

        def invoke(*arguments, success):
            result = subprocess.run([args.tool, 'merge', baseline, image, manifest, *arguments],
                                    text=True, capture_output=True, timeout=30)
            if (result.returncode == 0) != success:
                raise AssertionError(f'unexpected merge result {result.returncode}: {result.stdout} {result.stderr}')
            return result

        manifest.write_text(header + row + excluded, encoding='utf-8')
        invoke(output, success=True)
        assert pack.read_bytes() == baseline.read_bytes(), 'no-add must copy baseline byte-identically'
        result = json.loads(report.read_text(encoding='utf-8'))
        assert result['records'] == result['baseline_records'] == 2
        assert (result['added'], result['skipped_existing'], result['excluded']) == (0, 1, 1)
        assert [entry['status'] for entry in result['candidates']] == ['skipped_existing', 'excluded']
        assert result['baseline_producer'] == 'matching'
        assert Path(result['baseline_path']).samefile(baseline)
        original = pack.read_bytes()
        original_report = report.read_bytes()
        mismatch_dir = root / 'wrong-image-output'
        mismatch = subprocess.run([args.tool, 'merge', baseline, root / 'wrong-image.bin', manifest, mismatch_dir],
                                  text=True, capture_output=True, timeout=30)
        assert mismatch.returncode and pack.read_bytes() == original and report.read_bytes() == original_report
        assert not mismatch_dir.exists()

        # The exact baseline file is protected even when the destination is a different path alias.
        exact_baseline = root / 'portable_vk.lospv'
        exact_baseline.write_bytes(original)
        result = subprocess.run([args.tool, 'merge', exact_baseline, image, manifest, root],
                                text=True, capture_output=True, timeout=30)
        assert result.returncode and exact_baseline.read_bytes() == original

        # Test both final names, including a report alias that used to be overwritten.
        inputs = (baseline, manifest, image, source)
        for name in ('portable_vk.lospv', 'merge-report.json'):
            for index, protected in enumerate(inputs):
                alias_dir = root / f'alias-{name}-{index}'
                alias_dir.mkdir()
                alias = alias_dir / name
                alias.hardlink_to(protected)
                before = protected.read_bytes()
                invoke(alias_dir, success=False)
                assert alias.read_bytes() == before and protected.read_bytes() == before
                assert sorted(p.name for p in alias_dir.iterdir()) == [name], 'alias rejection created files'

        # Both final outputs must be absent. A final report collision may not be replaced.
        existing_dir = root / 'existing-report'
        existing_dir.mkdir()
        (existing_dir / 'merge-report.json').write_bytes(b'preserve-report')
        invoke(existing_dir, success=False)
        assert (existing_dir / 'merge-report.json').read_bytes() == b'preserve-report'
        assert not (existing_dir / 'portable_vk.lospv').exists()

        # A failure after publishing the pack must remove only that invocation's final.
        failed_dir = root / 'publish-failure'
        failure = subprocess.run([args.tool, 'merge', baseline, image, manifest, failed_dir],
                                 text=True, capture_output=True, timeout=30,
                                 env={**os.environ, 'LO_PACK_MERGE_TEST_FAIL_REPORT_PUBLISH': '1'})
        assert failure.returncode and 'injected merge report publication failure' in failure.stderr
        assert not (failed_dir / 'portable_vk.lospv').exists()
        assert not (failed_dir / 'merge-report.json').exists()
        assert not list(failed_dir.iterdir()), 'failed publication left staged files'
        assert baseline.read_bytes() == original and manifest.read_text(encoding='utf-8') == header + row + excluded

        # Old fixed temporary names are someone else's files and must survive success.
        legacy_dir = root / 'legacy-temp'
        legacy_dir.mkdir()
        for name in ('portable_vk.lospv.merge.tmp', 'merge-report.json.tmp'):
            (legacy_dir / name).write_bytes(b'keep')
        invoke(legacy_dir, success=True)
        for name in ('portable_vk.lospv.merge.tmp', 'merge-report.json.tmp'):
            assert (legacy_dir / name).read_bytes() == b'keep'

        for index, (bad, diagnostic) in enumerate((
                (header + row + row, 'duplicate manifest shader key'),
                (header + row + 'include\tps\t0000000000000002\tsource.bin\tconflicting-source\n', 'source path assigned conflicting hashes'),
                (header + row.replace('source.bin', 'source-missing.bin'), 'source missing'),
                (header + row.replace(f'{hash_value:016x}', 'ffffffffffffffff'), 'invalid source size or renderer-byte FNV-1a hash'),
                (header + 'include\tvs\t0000000000000002\tsource.bin\twrong-key\n', 'invalid source size or renderer-byte FNV-1a hash'),
                (header + 'include\tvs\t0000000000000002\tshort.bin\tshort-source\n', 'invalid source size or renderer-byte FNV-1a hash'))):
            (root / 'short.bin').write_bytes(b'\x00' * 8)
            manifest.write_text(bad, encoding='utf-8')
            bad_dir = root / f'invalid-source-{index}'
            failure = invoke(bad_dir, success=False)
            assert diagnostic in failure.stderr and not bad_dir.exists(), failure.stderr
            assert pack.read_bytes() == original, 'failed merge changed pack'
            assert report.read_bytes() == original_report, 'failed merge changed report'

        manifest.write_text(header, encoding='utf-8')
        empty_dir = root / 'empty'
        invoke(empty_dir, success=True)
        assert json.loads((empty_dir / 'merge-report.json').read_text(encoding='utf-8'))['added'] == 0
        assert (empty_dir / 'portable_vk.lospv').read_bytes() == original, 'empty merge changed baseline'
        manifest.write_text(header + ''.join(f'exclude\tps\t{i:016x}\t-\tbounded-rows\n' for i in range(1, 48)), encoding='utf-8')
        many_dir = root / 'more-than-46'
        invoke(many_dir, success=True)
        assert json.loads((many_dir / 'merge-report.json').read_text(encoding='utf-8'))['excluded'] == 47
        print('PASS synthetic merge: alias guard, staged publication rollback, existing output preservation, bounded manifest')


if __name__ == '__main__':
    main()
