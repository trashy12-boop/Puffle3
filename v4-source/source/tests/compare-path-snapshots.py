#!/usr/bin/env python3
"""Compare AVM1 reference semantics with the optimization off/on.
Uses the pinned upstream regression SWFs (not copyrighted game assets).
Arguments: baseline-benchmark optimized-benchmark output-directory
"""
from pathlib import Path
import subprocess,sys,json,tomllib
source=Path(__file__).resolve().parent.parent
before,after=map(lambda p:Path(p).resolve(),sys.argv[1:3])
out=Path(sys.argv[3]).resolve();out.mkdir(parents=True,exist_ok=True)
fixtures=source/'deps/ruffle/tests/tests/swfs/avm1'
names=['path_string','target_clip_removed','target_clip_swf5','target_clip_swf6',
       'movieclip_name_from_timeline','remove_movie_clip','duplicate_movie_clip',
       'string_paths_basic','string_paths_hidden','string_paths_variable_alias',
       'string_paths_reference_launder','function_base_clip_removed',
       'removed_clip_function_scope','removed_base_clip_tell_target']
results=[]
for name in names:
    fixture=fixtures/name
    config=tomllib.loads((fixture/'test.toml').read_text())
    frames=config.get('num_frames',2)
    traces=[]
    for variant,binary in [('before',before),('after',after)]:
        prefix=out/f'{name}-{variant}'
        result=subprocess.run([str(binary),str(fixture/'test.swf'),str(frames),'cache',str(prefix)],capture_output=True,text=True,check=True,timeout=60)
        (prefix.with_suffix('.log')).write_text(result.stdout+result.stderr)
        traces.append([line[6:] for line in result.stdout.splitlines() if line.startswith('TRACE ')])
    assert traces[0]==traces[1],f'AVM1 behavior changed: {name}'
    expected=(fixture/'output.txt').read_text().splitlines()
    results.append({'test':name,'frames':frames,'trace_lines':len(traces[0]),'before_after_match':True,
                    'matches_upstream_output':traces[1]==expected,'upstream_known_failure':config.get('known_failure',False)})
# Exercise immutable snapshots independently from mutable reference resolution:
# leaf rename, removal/recreation and ancestor rename.
fixture=source/'tests/fixtures/path-snapshots.swf'
expected=(fixture.with_name('path-snapshots-output.txt')).read_text().splitlines()
for variant,binary in [('before',before),('after',after)]:
    prefix=out/f'path-snapshot-semantics-{variant}'
    run=subprocess.run([str(binary),str(fixture),'1','cache',str(prefix)],capture_output=True,text=True,check=True,timeout=60)
    prefix.with_suffix('.log').write_text(run.stdout+run.stderr)
    assert [l[6:] for l in run.stdout.splitlines() if l.startswith('TRACE ')]==expected
results.append({'test':'path-snapshot-semantics','frames':1,'trace_lines':len(expected),'before_after_match':True,'matches_upstream_output':True,'upstream_known_failure':False,'original_fixture':True})
(out/'summary.json').write_text(json.dumps(results,indent=2))
print(f'{len(results)} AVM1 fixtures: before/after traces identical; {sum(r["matches_upstream_output"] for r in results)} match upstream expected output')
