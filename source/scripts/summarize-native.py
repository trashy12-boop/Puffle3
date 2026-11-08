#!/usr/bin/env python3
"""Summarize selected Puffle timing intervals without conflating draw and sim FPS.
Select a scene by 1-based log line range; retain the raw log with the report.
"""
import argparse
import json
import re
import statistics
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('log', type=Path)
p.add_argument('--environment', choices=['rpcs3', 'physical-ps3'], required=True)
p.add_argument('--scene', required=True)
p.add_argument('--start-line', type=int, default=1)
p.add_argument('--end-line', type=int)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
all_lines = a.log.read_text(errors='replace').splitlines()
prefix_text = '\n'.join(all_lines[:a.start_line-1])
lines = all_lines[a.start_line-1:a.end_line]
text = '\n'.join(lines)
draw = [tuple(map(float, m)) for m in re.findall(
    r'Draw ([\d.]+)/s ([\d.]+)ms \| logic ([\d.]+)ms \| audio ([\d.]+)ms', text)]
sim = [float(m) for m in re.findall(r'VM profile sim ([\d.]+)/s', text)]
vm = [tuple(map(int, m)) for m in re.findall(r'vm (\d+)us collision (\d+)us GC (\d+)us', text)]
gaps = [tuple(map(float, m)) for m in re.findall(
    r'Draw intervals samples (\d+) mean ([\d.]+)ms p99 ([\d.]+)ms p99-equivalent ([\d.]+)/s dropped (\d+)', text)]
memory = [tuple(map(int, m)) for m in re.findall(r'Rust heap live (\d+) KiB peak (\d+) KiB', text)]
sync = [tuple(map(int, m)) for m in re.findall(
    r'RSX interval: reused (\d+), convert (\d+)us, fence wait (\d+)us, sync (\d+)us, failures (\d+)', text)]
# Cumulative counters require a sample preceding the selected scene window.
def counter_delta(pattern):
    before = re.findall(pattern, prefix_text)
    during = re.findall(pattern, text)
    if not during:
        return None
    end = tuple(map(int, during[-1]))
    start = tuple(map(int, before[-1])) if before else (0,) * len(end)
    return [b-a for a, b in zip(start, end)]

if not draw:
    p.error('no native timing intervals in this line range')
report = dict(environment=a.environment, scene=a.scene, source_log=str(a.log),
              line_range=[a.start_line, a.end_line], timing_intervals=len(draw),
              mean_reported_drawn_fps=statistics.mean(r[0] for r in draw),
              mean_render_cpu_ms=statistics.mean(r[1] for r in draw),
              mean_tick_cpu_ms=statistics.mean(r[2] for r in draw),
              mean_audio_pump_ms=statistics.mean(r[3] for r in draw),
              mean_reported_simulation_fps=statistics.mean(sim) if sim else None,
              note='Each timing row covers approximately two seconds. Tick can run multiple simulation steps. VM includes collision. CPU render time includes submission/waits; no GPU execution timer is available.')
frame_delta = counter_delta(r'RSX rendered frames (\d+) \| software rendered frames (\d+)')
if frame_delta:
    report['rendered_frame_counter_delta'] = dict(zip(['rsx', 'software_fallback'], frame_delta))
reason_delta = counter_delta(r'RSX fallback gradients (\d+) masks (\d+) blend (\d+) lines/cache (\d+) other (\d+)')
if reason_delta:
    report['fallback_reason_counter_delta'] = dict(zip(['gradients', 'masks', 'blend', 'lines_or_cache_entries', 'other'], reason_delta))
    report['fallback_note'] = 'Reason counters may include offscreen cache activity; they are not mutually exclusive rejected-frame counts.'
if vm:
    report['profile_totals_us'] = dict(zip(['vm', 'collision_included_in_vm', 'gc'], map(sum, zip(*vm))))
if gaps:
    report['worst_interval_p99_draw_gap_ms'] = max(r[2] for r in gaps)
    report['draw_gap_samples'] = int(sum(r[0] for r in gaps))
    report['dropped_gap_samples'] = int(sum(r[4] for r in gaps))
    report['percentile_note'] = 'Worst per-interval p99, not the p99 of the complete scene and not an average-of-slowest-1-percent FPS.'
if memory:
    report['rust_requested_payload_peak_kib'] = max(r[1] for r in memory)
    report['memory_note'] = 'Excludes native C allocations, allocator metadata, VRAM and fragmentation.'
if sync:
    report['rsx_totals'] = dict(zip(['reused_textures', 'conversion_us', 'fence_wait_us', 'texture_sync_us', 'failed_draws'], map(sum, zip(*sync))))
a.output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
