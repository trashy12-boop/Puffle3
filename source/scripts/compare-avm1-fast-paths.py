#!/usr/bin/env python3
"""Differential AVM1 compatibility checks against the pinned pre-v5 runtime.
Usage: compare-avm1-fast-paths.py BASELINE OPTIMIZED OUTPUT_DIRECTORY
Game SWFs are not required. Existing upstream failures are reported separately.
The five tick-count fixtures are differential frame-step probes; this harness
is not a substitute for upstream's tick/input-aware test runner.
"""
from pathlib import Path
import argparse
import json
import subprocess
import re

NAMES = [
    'add_property', 'object_resolve', 'prototype_properties',
    'movieclip_prototype_extension', 'as2_super_via_manual_prototype',
    'get_variable_in_scope', 'closure_scope', 'set_variable_scope',
    'as2_super_and_this_v8', 'as2_super_and_this_v6', 'swf4_function_calls',
    'virtual_property_recursion_scope', 'define_function2_preload_order',
    'function_suppress_and_preload', 'getproperty', 'getproperty_swf5',
    'getproperty_swf4', 'object_function', 'swf6_case_insensitive',
    'with_variable_scopes', 'new_class_prototype_getter', 'function_base_clip',
    'prototype_delete', 'swf7_case_sensitive', 'define_function2_preload',
    'define_function_case_sensitive', 'watch_virtual_property_proto',
    'object_prototypes', 'funky_function_calls', 'coerce_to_primitive_resolve',
    'has_own_property', 'super_edge_cases', 'constructor_function',
    'watch_virtual_property', 'removed_target_clip_scope', 'define_function2',
    'property_invalid_base_clip', 'path_string', 'target_clip_removed',
    'target_clip_swf5', 'target_clip_swf6', 'movieclip_name_from_timeline',
    'remove_movie_clip', 'duplicate_movie_clip', 'string_paths_basic',
    'string_paths_hidden', 'string_paths_variable_alias',
    'string_paths_reference_launder', 'function_base_clip_removed',
    'removed_clip_function_scope', 'removed_base_clip_tell_target',
]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('optimized', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).resolve().parent.parent
    fixtures = source / 'deps/ruffle/tests/tests/swfs/avm1'
    results = []
    for name in NAMES:
        fixture = fixtures / name
        # Upstream uses TOML 1.1 multiline inline tables; Python 3.12's
        # tomllib only supports 1.0. Read just the two top-level scalars.
        config = (fixture / 'test.toml').read_text()
        match = re.search(r'^(num_frames|num_ticks)\s*=\s*(\d+)\s*$', config, re.M)
        if not match:
            raise RuntimeError(f'{name}: requires a tick/input-aware test runner')
        frames = int(match[2])
        traces = []
        for label, binary in [('baseline', args.baseline), ('optimized', args.optimized)]:
            prefix = args.output / f'{name}-{label}'
            run = subprocess.run(
                [str(binary.resolve()), str(fixture / 'test.swf'), str(frames),
                 'cache', str(prefix.resolve())],
                capture_output=True, text=True, check=True, timeout=60)
            prefix.with_suffix('.log').write_text(run.stdout + run.stderr)
            traces.append([line[6:] for line in run.stdout.splitlines()
                           if line.startswith('TRACE ')])
        if traces[0] != traces[1]:
            raise RuntimeError(f'AVM1 trace regression: {name}')
        expected = (fixture / 'output.txt').read_text().splitlines()
        results.append(dict(test=name, frames=frames, upstream_unit=match[1],
                            trace_lines=len(traces[1]),
                            before_after_match=True,
                            matches_upstream_output=traces[1] == expected,
                            upstream_known_failure=bool(re.search(r'^known_failure\s*=\s*true', config, re.M))))
    (args.output / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
    print(f'{len(results)} AVM1 fixtures have identical before/after traces; '
          f'{sum(r["matches_upstream_output"] for r in results)} match upstream output')

if __name__ == '__main__':
    main()
