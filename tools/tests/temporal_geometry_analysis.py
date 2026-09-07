"""CPU-only analysis of retained TemporalAA geometry fixture frames.

Run DIRECTORY --output NEW_JSON after the GPU fixture. Never rewrites images or
summary.json. Exit 2 means invalid/incomplete input; quality findings remain
explicit booleans, since the conservative resolver currently has known limits.
"""
import argparse
import csv
import hashlib
import json
import tempfile
from pathlib import Path

W, H, SLOPE, INTERCEPT = 128, 64, .375, 40.25


def coverage(x, y, offset):
    cuts = [float(y), float(y + 1)]
    for edge in (x, x + 1):
        t = (edge - INTERCEPT - offset) / SLOPE
        if y < t < y + 1:
            cuts.append(t)
    cuts.sort()
    f = lambda t: min(1., max(0., SLOPE * t + INTERCEPT + offset - x))
    return sum((b-a)*(f(a)+f(b))*.5 for a, b in zip(cuts, cuts[1:]))


def ppm(path):
    data = path.read_bytes()
    # Fixture emits exactly this header; reject incompatible inputs explicitly.
    header = f'P6\n{W} {H}\n255\n'.encode()
    if not data.startswith(header) or len(data) != len(header) + W*H*3:
        raise ValueError(f'invalid fixture PPM: {path}')
    return [v / 255. for v in data[len(header)::3]]


def roi(offset):
    return [y*W+x for y in range(4, H-4) for x in range(W)
            if abs(x+.5-(SLOPE*(y+.5)+INTERCEPT+offset)) < 3]


def variance(images, indices):
    if not images or not indices:
        raise ValueError('empty variance sample')
    n = len(images)
    return sum(sum((im[p] - sum(a[p] for a in images)/n)**2
                   for im in images)/n for p in indices)/len(indices)


def segment(images, baseline, offset, start):
    indices = roi(offset)
    truth = {p: coverage(p % W, p // W, offset) for p in indices}
    mse = lambda im: sum((im[p]-truth[p])**2 for p in indices)/len(indices)
    errors = [mse(im) for im in images]
    base_errors = [mse(im) for im in baseline]
    # For each stationary segment use its final eight phases, never a step.
    rvar, bvar = variance(images[-8:], indices), variance(baseline[-8:], indices)
    early, late = sum(errors[:8])/8, sum(errors[-8:])/8
    bmean = sum(base_errors[-8:])/8
    return dict(first_phase=start, last_phase=start+len(images)-1,
                object_offset=offset, roi_pixels=len(indices),
                resolved_first8_mse=early, resolved_last8_mse=late,
                baseline_last8_mse=bmean, resolved_last8_variance=rvar,
                baseline_last8_variance=bvar,
                improves_coverage_over_display_only=late < bmean,
                improves_phase_variance_over_display_only=rvar < bvar,
                converges_from_first8=late < early)


def retreat(images, baseline, old_offset, new_offset, start):
    # Entire newly revealed area with analytically zero current coverage.
    # Old-boundary subset is separated from the current edge/filter footprint.
    strip = [y*W+x for y in range(4, H-4) for x in range(W)
             if coverage(x, y, old_offset) > 0 and coverage(x, y, new_offset) == 0]
    old_edge = [p for p in strip if
                abs(p % W+.5-(SLOPE*(p//W+.5)+INTERCEPT+old_offset)) < 2]
    if not strip or not old_edge:
        raise ValueError('retreat does not reveal measurable background')
    rows = []
    for i, (im, base) in enumerate(zip(images, baseline)):
        rows.append(dict(phase=start+i,
            revealed_mae=sum(im[p] for p in strip)/len(strip),
            revealed_max=max(im[p] for p in strip),
            baseline_revealed_max=max(base[p] for p in strip),
            old_edge_residual_max=max(im[p] for p in old_edge),
            old_edge_excess_over_baseline_max=max(max(0., im[p]-base[p]) for p in old_edge)))
    # One 8-bit code is a reporting precision, not a shader acceptance threshold.
    tolerance = 1/255.
    clean = [r['old_edge_excess_over_baseline_max'] <= tolerance for r in rows]
    recovery = next((i for i in range(len(clean)) if all(clean[i:])), None)
    return dict(first_phase=start, pixels=len(strip), old_edge_pixels=len(old_edge),
                reporting_tolerance=tolerance, phases=rows,
                old_edge_excess_above_one_code=not all(clean),
                frames_until_sustained_old_edge_recovery=recovery)


def analyze(directory):
    rows = list(csv.DictReader((directory/'phases.csv').open(newline='')))
    if len(rows) not in (32, 48) or [int(r['phase']) for r in rows] != list(range(1, len(rows)+1)):
        raise ValueError('expected contiguous 32 static or 48 moving phases')
    if any('object_offset' not in r for r in rows):
        raise ValueError('missing object_offset; preserve legacy run, generate new fixture directory')
    offsets = [float(r['object_offset']) for r in rows]
    expected = [0.]*32 if len(rows) == 32 else [0.]*16+[8.]*16+[0.]*16
    if offsets != expected:
        raise ValueError('unexpected scene schedule')
    resolved, baseline, hashes = [], [], {}
    for i in range(1, len(rows)+1):
        for suffix, target in [('resolved', resolved), ('jitter-only', baseline)]:
            path = directory/f'phase-{i:02}-{suffix}.ppm'
            target.append(ppm(path))
            hashes[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    ranges = [(0, 32, 0.)] if len(rows) == 32 else [(0, 16, 0.), (16, 32, 8.), (32, 48, 0.)]
    segments = [segment(resolved[a:b], baseline[a:b], off, a+1) for a, b, off in ranges]
    result = dict(state='analyzed', source=str(directory.resolve()), frames=len(rows),
                  variance_contract='stationary segment, current-edge ROI, last 8 phases',
                  segments=segments, artifact_sha256=hashes,
                  static_contour_benefit=all(s['improves_coverage_over_display_only'] and
                      s['improves_phase_variance_over_display_only'] for s in segments))
    if len(rows) == 48:
        result['retreat'] = retreat(resolved[32:], baseline[32:], 8., 0., 33)
    return result


def self_test():
    black = [0.]*(W*H)
    half = [.5]*(W*H)
    assert variance([black, half], [0]) == .0625
    assert variance([black]*8, roi(8)) == 0
    assert coverage(0, 20, 0) == 1 and coverage(127, 20, 0) == 0
    truth = [coverage(x, y, 0) for y in range(H) for x in range(W)]
    s = segment([truth]*16, [half]*16, 0, 1)
    assert s['resolved_last8_mse'] == 0 and s['improves_coverage_over_display_only']
    clean = retreat([black]*16, [black]*16, 8, 0, 33)
    assert clean['frames_until_sustained_old_edge_recovery'] == 0
    ghost = [coverage(x, y, 8) for y in range(H) for x in range(W)]
    dirty = retreat([ghost, ghost]+[black]*14, [black]*16, 8, 0, 33)
    assert dirty['old_edge_excess_above_one_code']
    assert dirty['frames_until_sustained_old_edge_recovery'] == 2
    assert dirty['phases'][0]['old_edge_residual_max'] > 0
    with tempfile.TemporaryDirectory() as temp:
        path = Path(temp)/'sample.ppm'
        path.write_bytes(f'P6\n{W} {H}\n255\n'.encode()+bytes(W*H*3))
        assert ppm(path) == black
        path.write_bytes(b'P6\n1 1\n255\n')
        try:
            ppm(path)
            raise AssertionError('bad PPM accepted')
        except ValueError:
            pass
    print('PASS: PPM parsing/rejection, analytic coverage, stationary variance, exact convergence, retreat residual and recovery')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path, nargs='?')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
    else:
        if args.directory is None or args.output is None:
            parser.error('directory and --output required')
        try:
            result = analyze(args.directory)
        except (OSError, ValueError, KeyError) as exc:
            print(f'INVALID: {exc}')
            raise SystemExit(2)
        # Never overwrite previous evidence, including our own previous analysis.
        with args.output.open('x', encoding='utf-8') as output:
            json.dump(result, output, indent=2, allow_nan=False)
            output.write('\n')
        print(json.dumps({k: v for k, v in result.items() if k != 'artifact_sha256'}))
