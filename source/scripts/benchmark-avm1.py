#!/usr/bin/env python3
"""Run matched, serial host replays; never converts host timings to PS3 FPS.
Build binaries with scripts/build-host-benchmarks.sh before invoking this.
"""
import argparse
import csv
import hashlib
import json
import math
import os
import platform
from pathlib import Path
import statistics
import subprocess

SCENES = {
    'title': (600, 950), 'file_selection': (1150, 1350),
    'startup': (1350, 1700), 'castle_intro': (1700, 2400),
    'tutorial': (2400, 2800), 'moving_jumping_scrolling': (2800, 3400),
}
COUNTS = ['opcodes', 'gets', 'sets', 'calls', 'hit_tests', 'sim_frames', 'vm_entries']

def rows(prefix, suffix=''):
    with Path(str(prefix) + suffix + '.csv').open() as f:
        return list(csv.DictReader(f))

def summary(prefix):
    timing, vm, rsx = rows(prefix), rows(prefix, '-vm'), rows(prefix, '-rsx')
    result = {}
    for name, (start, end) in SCENES.items():
        if len(timing) < end:
            continue
        a, b, c = timing[start:end], vm[start:end], rsx[start:end]
        total = sorted(sum(float(r[k]) for k in ['logic_ms', 'render_ms', 'audio_ms']) for r in a)
        result[name] = {k: statistics.mean(float(r[k]) for r in sample)
                       for sample, keys in [(a, ['logic_ms', 'render_ms', 'audio_ms']),
                                            (b, ['vm_us', 'gc_us', 'collision_us'])]
                       for k in keys}
        result[name].update(
            mean_host_work_ms=statistics.mean(total),
            p99_host_work_ms=total[math.ceil(len(total) * .99) - 1],
            rsx_accepted_frames=sum(int(r['accepted']) for r in c),
            rsx_rejected_frames=sum(not int(r['accepted']) for r in c),
            simulation_steps=sum(int(r['sim_frames']) for r in b))
    return result

def compare(before, after):
    a, b = rows(before, '-vm'), rows(after, '-vm')
    if len(a) != len(b) or any(x[k] != y[k] for x, y in zip(a, b) for k in COUNTS):
        raise RuntimeError('simulation counters changed')
    if Path(str(before) + '-rsx.csv').read_bytes() != Path(str(after) + '-rsx.csv').read_bytes():
        raise RuntimeError('RSX command stream changed')
    images = list(before.parent.glob(before.name + '-*.rgba'))
    if not images:
        raise RuntimeError('reference images missing')
    for image in images:
        other = after.parent / image.name.replace(before.name, after.name, 1)
        if image.read_bytes() != other.read_bytes():
            raise RuntimeError(f'reference image changed: {image.name}')
    return dict(vm_counter_rows=len(a), rsx_rows=len(rows(before, '-rsx')),
                identical_reference_images=len(images))

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('swf', type=Path)
    p.add_argument('baseline', type=Path)
    p.add_argument('optimized', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--pairs', type=int, default=2)
    args = p.parse_args()
    if args.pairs < 1:
        p.error('--pairs must be positive')
    args.output.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).resolve().parent.parent
    replay = source / 'validation/performance04/replays/mario-gameplay-v3.txt'
    results = dict(environment='host', machine=platform.platform(),
                   swf_sha256=hashlib.sha256(args.swf.read_bytes()).hexdigest(),
                   replay_sha256=hashlib.sha256(replay.read_bytes()).hexdigest(),
                   note='320x240, fixed 32 Hz movie clock, sound mixer, C RSX command recorder plus software reference images; no GPU or console FPS measurement',
                   runs=[], comparisons=[])
    for pair in range(args.pairs):
        order = ['baseline', 'optimized'] if pair % 2 == 0 else ['optimized', 'baseline']
        prefixes = {}
        for label in order:
            binary = getattr(args, label).resolve()
            prefix = (args.output / f'{label}-{pair}').resolve()
            prefixes[label] = prefix
            with Path(str(prefix) + '.log').open('w') as log:
                command = [str(binary), str(args.swf.resolve()), '3400',
                           'cache', str(prefix), str(replay)]
                process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
                _, status, usage = os.wait4(process.pid, 0)
                process.returncode = os.waitstatus_to_exitcode(status)
                if process.returncode:
                    raise subprocess.CalledProcessError(process.returncode, command)
            results['runs'].append(dict(label=label, pair=pair,
                binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                peak_process_rss_kib=usage.ru_maxrss, scenes=summary(prefix)))
            print(f'{label} pair {pair} finished', flush=True)
        results['comparisons'].append(compare(prefixes['baseline'], prefixes['optimized']))
        (args.output / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
    print('All reference images, VM counts and RSX command rows match.')

if __name__ == '__main__':
    main()
