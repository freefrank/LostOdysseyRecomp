"""Summarize city rendering timing from one or more drive-city summaries."""

import argparse
import json, math, re, statistics
from pathlib import Path

def stats(values, frame_times=False, duration=True):
    a = sorted(values)
    if not a: return {}
    n = len(a)
    result = dict(n=n, mean=statistics.mean(a), p50=a[math.ceil(n*.5)-1],
                p99=a[math.ceil(n*.99)-1], max=a[-1])
    if duration:
        result.update(over_16_67=sum(x > 16.67 for x in a),
                      over_16_67_pct=100*sum(x > 16.67 for x in a)/n)
    if frame_times:
        result['low_1pct_fps'] = 1000/statistics.mean(a[-max(1, math.ceil(n*.01)):])
        result['mean_fps'] = 1000/statistics.mean(a)
    return result

def analyze(summary_path):
    summary_path=Path(summary_path).resolve()
    summary=json.loads(summary_path.read_text(encoding='utf-8-sig'))
    render={}; present={}; beats=[]; vertex_stages={}
    log_path=Path(summary['log'])
    if not log_path.is_absolute():
        log_path=summary_path.parent/log_path
    if not log_path.exists() and summary.get('retained_log'):
        log_path=Path(summary['retained_log'])
        if not log_path.is_absolute():
            log_path=summary_path.parent/log_path
    for line in log_path.read_text(encoding='utf-8', errors='replace').splitlines():
        d=dict(re.findall(r'(\w+)=([^\s]+)', line))
        if 'render timing frame=' in line and int(d['draws']) >= 800:
            render[int(d['frame'])]=d
        if 'present timing completed=' in line and d.get('sample_valid')=='true':
            present[int(d['completed'])]=d
        if 'vertex timing frame=' in line:
            vertex_stages[int(d['frame'])]=d
        m=re.search(r'heartbeat:.*?([\d.]+) fps.*xenon_scr.fpd',line)
        if m: beats.append(float(m[1]))
    def window(lo, hi):
        rows=[(f,d) for f,d in render.items() if lo<=f<=hi]
        paired=[present[f+1] for f,d in rows if f+1 in present]
        stages=[d for f,d in vertex_stages.items() if lo<=f<=hi]
        return dict(render_frame_range=[lo,hi], draw=stats([float(d['draw_ms']) for f,d in rows]),
            vertex=stats([float(d['vertex_ms']) for f,d in rows]),
            present=stats([float(d['frame_ms']) for d in paired], True),
            flush=stats([float(d['flush_ms']) for d in paired]),
            worst_present=sorted(paired,key=lambda d:float(d['frame_ms']),reverse=True)[:5],
            worst_vertex=[dict(frame=f,**{k:d[k] for k in ['draws','draw_ms','vertex_ms','bind_ms']})
                          for f,d in sorted(rows,key=lambda item:float(item[1]['vertex_ms']),reverse=True)[:5]],
            vertex_stage_hotspots=[dict(frame=f, vertex_ms=d['vertex_ms'], stages=vertex_stages[f])
                for f,d in sorted(rows,key=lambda item:float(item[1]['vertex_ms']),reverse=True)[:5]
                if f in vertex_stages],
            rehash_events=[d for d in stages if int(d['rehashes'])>0],
            vertex_cache=dict(
                samples=len(stages), peak_entries=max(int(d['cache_after']) for d in stages),
                rehashes=sum(int(d['rehashes']) for d in stages),
                evictions=sum(int(d.get('evictions',0)) for d in stages),
                upload_bytes=stats([float(d['bytes']) for d in stages], duration=False),
                uploads=stats([float(d['uploads']) for d in stages], duration=False),
                insert_ms=stats([float(d['insert_ms']) for d in stages]),
                insert_max_ms=max(float(d['insert_max_ms']) for d in stages)) if stages else {})
    return dict(summary=summary, pairing='render frame N -> present completed N+1',
        low_definition='1000 / mean(slowest ceil(N*0.01) frame times)',
        all_city=window(min(render),max(render)),
        stable_common=window(1600,2800),
        heartbeat=dict(n=len(beats), mean=statistics.mean(beats), min=min(beats), max=max(beats)) if beats else {})

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('summaries', nargs='+', type=Path, help='drive-summary.json files to compare')
    args = parser.parse_args(argv)
    result={p.stem:analyze(p) for p in args.summaries}
    print(json.dumps(result,indent=2))

if __name__ == '__main__':
    main()
