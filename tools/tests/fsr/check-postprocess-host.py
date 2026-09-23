"""Independent host bloom area and final-mask snapshot evidence."""
import argparse
import json
from pathlib import Path
import numpy as np
from postprocess_mask_reference import area_max_reference
from importlib.util import spec_from_file_location, module_from_spec

spec = spec_from_file_location('capture_loader', Path(__file__).with_name('check-postprocess-capture.py'))
loader = module_from_spec(spec)
spec.loader.exec_module(loader)


def plane(event, label, root):
    if event.get('capture_failures'):
        raise ValueError(str(event['capture_failures']))
    matches = [s for s in event.get('snapshots', []) if s['label'] == label]
    if len(matches) != 1:
        raise ValueError('missing or duplicate snapshot: ' + label)
    snapshot = matches[0]
    data, evidence = loader.load_snapshot(root, snapshot)
    if snapshot['bytes_per_pixel'] != 1:
        raise ValueError('expected R8 mask')
    w, h = snapshot['extent']
    return np.frombuffer(data, np.uint8).reshape(h, w), evidence


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('trace', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    events = [json.loads(s) for s in args.trace.read_text().splitlines() if s.strip()]
    rows, errors = [], []
    for event in events:
        kind = event.get('kind')
        if kind not in ('host_bloom_prefilter', 'scene_copy_mask'):
            continue
        try:
            if event['status'] != 'available' or event['submission_serial'] <= 0:
                raise ValueError('unavailable or no completed submission')
            if kind == 'host_bloom_prefilter':
                source, src_evidence = plane(event, 'prefilter-input-mask', args.trace.parent)
                actual, dst_evidence = plane(event, 'prefilter-output-mask', args.trace.parent)
                if event['source_extent'] != list(reversed(source.shape)) or event['output_extent'] != [1280, 720]:
                    raise ValueError('host extent mismatch')
                reference, valid = area_max_reference(source, event['output_extent'], event['source_valid_rect'])
                if actual.shape != reference.shape:
                    raise ValueError('area output shape mismatch')
                mismatch = int(np.count_nonzero(actual != reference))
                unknown = int(np.count_nonzero(~valid))
                rows.append({'kind': kind, 'event_index': event['event_index'], 'mismatch_pixels': mismatch,
                             'unknown_pixels': unknown, 'nonzero_pixels': int(np.count_nonzero(actual)),
                             'snapshots': [src_evidence, dst_evidence]})
            else:
                actual, evidence = plane(event, 'final-scene-mask', args.trace.parent)
                if event['extent'] != list(reversed(actual.shape)) or not event['mask_image'] or not event['captured_color_image']:
                    raise ValueError('final scene binding/extent missing')
                rows.append({'kind': kind, 'event_index': event['event_index'],
                             'source_stage': event['source_stage'], 'source_revision': event['source_revision'],
                             'source_write_ordinal': event['source_write_ordinal'],
                             'nonzero_pixels': int(np.count_nonzero(actual)), 'snapshot': evidence})
        except (ValueError, KeyError, OSError) as error:
            errors.append({'kind': kind, 'event_index': event.get('event_index'), 'error': str(error)})
    kinds = {row['kind'] for row in rows}
    passed = not errors and kinds == {'host_bloom_prefilter', 'scene_copy_mask'} and not any(
        row.get('mismatch_pixels', 0) or row.get('unknown_pixels', 0) for row in rows)
    result = {'status': 'bounded_host_checks_passed' if passed else 'incomplete_or_failed',
              'rows': rows, 'errors': errors,
              'limits': ['Final scene snapshot presence is not proof of its complete source-version chain',
                         'No SDK mask binding or full quality acceptance']}
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'status': result['status'], 'rows': len(rows), 'errors': errors}))
    raise SystemExit(0 if passed else 1)


if __name__ == '__main__':
    main()
