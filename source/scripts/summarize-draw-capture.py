#!/usr/bin/env python3
"""Summarize opt-in native draw-completion samples between guest scene markers.
These are draw completion gaps, not monitor scanout or simulation frame times.
"""
import argparse,csv,json,math,re,statistics
from pathlib import Path

def summarize(capture,log,scene):
    markers=[(m[0],int(m[1])) for m in re.findall(r'Benchmark scene (\S+) guest_us (\d+)',log)]
    starts=[i for i,(name,_) in enumerate(markers) if name==scene]
    if len(starts)!=1:raise ValueError('scene needs exactly one start marker')
    i=starts[0]
    if i+1==len(markers):raise ValueError('scene needs a following end marker')
    start,end=markers[i][1],markers[i+1][1]
    # Include a gap only if BOTH endpoints lie within the scene. In particular,
    # never include a loading/menu transition crossing the start boundary.
    rows=[r for r in capture if r['draw_gap_us']>0 and start<=r['completed_us']-r['draw_gap_us'] and r['completed_us']<=end]
    if len(rows)<2:raise ValueError('insufficient complete gaps in scene')
    gaps=[r['draw_gap_us']/1000 for r in rows];g=sorted(gaps)
    p99=g[math.ceil(len(g)*.99)-1]
    return dict(environment='rpcs3',scene=scene,guest_range_us=[start,end],
        draw_gap_samples=len(rows),observed_gap_seconds=sum(gaps)/1000,
        drawn_fps_over_complete_gaps=1000/statistics.mean(gaps),
        median_draw_gap_ms=statistics.median(gaps),p99_draw_gap_ms=p99,
        p99_equivalent_fps=1000/p99,worst_draw_gap_ms=max(gaps),
        mean_render_cpu_ms=statistics.mean(r['render_cpu_us']/1000 for r in rows),
        note='Draw-completion gaps, not scanout. p99-equivalent is not average-of-slowest-1% FPS. Capture adds overhead. Gameplay and scene identity require screenshot/input review; no GPU execution or simulation FPS is inferred.')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('capture',type=Path);p.add_argument('log',type=Path);p.add_argument('--scene',required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    try:
        with a.capture.open() as f:rows=[{k:int(v) for k,v in r.items()} for r in csv.DictReader(f)]
        result=summarize(rows,a.log.read_text(errors='replace'),a.scene)
    except (ValueError,KeyError) as e:p.error(str(e))
    a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
