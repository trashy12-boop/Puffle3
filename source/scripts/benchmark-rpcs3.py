#!/usr/bin/env python3
"""Run one auditable native RPCS3 input route on Linux. Requires a prepared,
private RPCS3 config/firmware/game tree and keyboard pad mappings. No firmware,
game assets or emulator binaries are redistributed. Review screenshots before
accepting a scene as gameplay; a completed route alone is not correctness proof.
"""
import argparse,hashlib,json,os,re,shutil,subprocess,time
from pathlib import Path

def digest(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ['rpcs3','executable','swf','config-home','cache-home','scenario','output','xdotool']:
        p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--xvfb',type=Path);p.add_argument('--display',default='127.0.0.1:98')
    p.add_argument('--build-manifest',type=Path,help='JSON recording compiler versions, features and profile overrides')
    p.add_argument('--boot-timeout',type=float,default=600);p.add_argument('--max-seconds',type=float,default=900)
    p.add_argument('--max-rss-mib',type=int,default=8192)
    a=p.parse_args()
    for name in ['rpcs3','executable','swf','scenario','xdotool']:
        value=getattr(a,name).resolve()
        if not value.is_file():p.error(f'missing {name}: {value}')
        setattr(a,name,value)
    a.config_home=a.config_home.resolve();a.cache_home=a.cache_home.resolve();a.output=a.output.resolve()
    a.output.mkdir(parents=True,exist_ok=False)
    cfg=a.config_home/'rpcs3'; data=cfg/'dev_hdd0/ruffle';data.mkdir(parents=True,exist_ok=True)
    route=json.loads(a.scenario.read_text())
    if digest(a.swf)!=route['swf_sha256']:p.error('SWF does not match scenario')
    # Never replace a user's game library; the harness requires a pre-staged SWF.
    staged=data/'games'/route['swf_name']
    if not staged.is_file() or digest(staged)!=digest(a.swf):p.error('stage the matching SWF in the private games directory first')
    log=data/'ruffle-ps3-dev.log';core=data/'ruffle-core.log';capture=data/'benchmark-frames.csv'
    for f in [log,core,capture]:f.unlink(missing_ok=True)
    (data/'benchmark-enable').touch();(data/'benchmark-scene').write_text('startup\n')
    env=os.environ.copy();env.update(DISPLAY=a.display,XDG_CONFIG_HOME=str(a.config_home),XDG_CACHE_HOME=str(a.cache_home),QT_X11_NO_MITSHM='1')
    metadata={'environment':'rpcs3','status':'starting','scene_review':'required','route_is_simulation_frame_deterministic':False,
              'files':{n:{'path':str(getattr(a,n)),'sha256':digest(getattr(a,n))} for n in ['rpcs3','executable','swf','scenario']},'inputs':[]}
    if a.build_manifest:
        metadata['build_manifest']=json.loads(a.build_manifest.read_text())
        shutil.copyfile(a.build_manifest,a.output/'build-manifest.json')
    for name in ['config.yml','input_configs/global/Default.yml']:
        f=cfg/name
        if f.is_file():
            dest=a.output/name.replace('/','_');shutil.copyfile(f,dest);metadata['files'][name]={'sha256':digest(f)}
    metadata['configuration_hashes']={str(f.relative_to(cfg)):digest(f) for pattern in ['*.yml','input_configs/**/*.yml','custom_configs/**/*.yml'] for f in cfg.glob(pattern) if f.is_file()}
    # Record every profile and save hash: route comparisons must use identical
    # pre-run state; do not compare runs that created different starting saves.
    metadata['initial_state']={str(f.relative_to(data)):digest(f) for d in ['profiles','saves'] for f in (data/d).rglob('*') if f.is_file()}
    for name in ['LD_LIBRARY_PATH','LIBGL_DRIVERS_PATH','VK_ICD_FILENAMES','LIBGL_ALWAYS_SOFTWARE']:
        metadata[name]=env.get(name)
    repo=Path(__file__).resolve().parents[2]
    for key,args in [('source_commit',['rev-parse','HEAD']),('source_status',['status','--porcelain'])]:
        metadata[key]=subprocess.run(['git','-C',str(repo),*args],capture_output=True,text=True).stdout.strip()
    start=time.monotonic();r=None;x=None;held=set();last_memory_sample=0
    metadata['memory_samples']=[]
    def alive():
        nonlocal last_memory_sample
        if r.poll() is not None:raise RuntimeError(f'RPCS3 exited ({r.returncode})')
        if time.monotonic()-start>a.max_seconds:raise TimeoutError('run deadline exceeded')
        text=log.read_text(errors='replace') if log.exists() else ''
        if 'allocation failure' in text.lower():raise RuntimeError('native allocation failure')
        if time.monotonic()-last_memory_sample>=1:
            last_memory_sample=time.monotonic()
            status=Path(f'/proc/{r.pid}/status')
            if status.exists():
                match=re.search(r'VmRSS:\s+(\d+) kB',status.read_text())
                if match:
                    rss=int(match[1]);metadata['memory_samples'].append([last_memory_sample-start,rss])
                    if rss>a.max_rss_mib*1024:raise RuntimeError('RPCS3 exceeded RSS guard')
            emulog=a.cache_home/'rpcs3/RPCS3.log'
            if emulog.exists():
                with emulog.open('rb') as f:
                    f.seek(max(0,emulog.stat().st_size-16384));tail=f.read().decode(errors='replace')
                if 'Thread terminated due to fatal error' in tail:raise RuntimeError('RPCS3 fatal error; inspect RPCS3.log')
        return text
    def wait(seconds):
        end=time.monotonic()+seconds
        while time.monotonic()<end:alive();time.sleep(min(.1,max(0,end-time.monotonic())))
    def xd(*args):subprocess.run([str(a.xdotool),*map(str,args)],env=env,check=True,timeout=10)
    def wait_log(token,timeout):
        end=time.monotonic()+timeout
        while token not in alive():
            if time.monotonic()>=end:raise TimeoutError(f'log marker not reached: {token}')
            time.sleep(.2)
    def shot(name):
        from PIL import ImageGrab
        if not re.fullmatch(r'[a-zA-Z0-9_-]+',name):raise ValueError('invalid screenshot name')
        ImageGrab.grab(xdisplay=env['DISPLAY']).save(a.output/(name+'.png'))
    try:
        if a.xvfb:
            number=':'+a.display.rsplit(':',1)[1]
            x=subprocess.Popen([str(a.xvfb.resolve()),number,'-screen','0','1280x720x24','-nolisten','unix','-nolisten','local','-listen','tcp','-ac'],env=env,stdout=open(a.output/'xvfb.log','w'),stderr=subprocess.STDOUT)
            time.sleep(1)
            if x.poll() is not None:raise RuntimeError('Xvfb failed; inspect xvfb.log')
        r=subprocess.Popen([str(a.rpcs3),'--no-gui',str(a.executable)],env=env,stdout=open(a.output/'console.log','w'),stderr=subprocess.STDOUT)
        wait_log('main menu ready',a.boot_timeout)
        ids=subprocess.run([str(a.xdotool),'search','--name','Puffle|Ruffle'],env=env,capture_output=True,text=True,check=True).stdout.split()
        if not ids:raise RuntimeError('no native game window')
        xd('windowfocus',ids[-1]);shot('browser')
        for index,step in enumerate(route['steps']):
            metadata['inputs'].append({'host_seconds':time.monotonic()-start,'step':step})
            if 'wait_log' in step:wait_log(step['wait_log'],step.get('timeout',60))
            if 'scene' in step:
                scene=step['scene']
                if not re.fullmatch(r'[a-zA-Z0-9_-]+',scene):raise ValueError('invalid scene')
                (data/'benchmark-scene').write_text(scene+'\n');wait_log('Benchmark scene '+scene+' guest_us ',10)
            if 'keys' in step:
                keys=step['keys'];xd('keydown',*keys);held.update(keys)
                wait(step.get('hold',.3));xd('keyup',*keys);held.difference_update(keys)
            if 'wait' in step:wait(step['wait'])
            if 'capture' in step:shot(step['capture'])
        wait(2.5) # Persist the final CSV buffer at the next native report.
        metadata['status']='route-completed-review-required'
    except Exception as error:
        metadata['status']='blocked';metadata['error']=str(error)
        try:shot('failure')
        except Exception:pass
    finally:
        for key in held:
            try:xd('keyup',key)
            except Exception:pass
        if r is not None:
            r.terminate()
            try:r.wait(timeout=10)
            except subprocess.TimeoutExpired:r.kill();r.wait()
            metadata['exit_code']=r.returncode
        if x is not None:
            x.terminate()
            try:x.wait(timeout=5)
            except subprocess.TimeoutExpired:x.kill();x.wait()
        for f in [log,core,capture]:
            if f.exists():shutil.copyfile(f,a.output/f.name)
        emulog=a.cache_home/'rpcs3/RPCS3.log'
        if emulog.exists():shutil.copyfile(emulog,a.output/'RPCS3.log')
        (data/'benchmark-enable').unlink(missing_ok=True);(data/'benchmark-scene').unlink(missing_ok=True)
        metadata['elapsed_seconds']=time.monotonic()-start
        (a.output/'run.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print(metadata['status'],metadata.get('error',''))
    if metadata['status']=='blocked':raise SystemExit(1)
if __name__=='__main__':main()
