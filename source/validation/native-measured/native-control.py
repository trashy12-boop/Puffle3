import subprocess,os,time,json
from pathlib import Path
w=Path.cwd();env=os.environ.copy();env.update(DISPLAY='127.0.0.1:116',XDG_CONFIG_HOME=str(w/'emulator/config'),XDG_CACHE_HOME=str(w/'emulator/cache-precise'),LD_LIBRARY_PATH=str(w/'tooling/desktop/usr/lib/x86_64-linux-gnu'),LIBGL_ALWAYS_SOFTWARE='1',QT_X11_NO_MITSHM='1',VK_ICD_FILENAMES=str(w/'tooling/desktop/usr/share/vulkan/icd.d/lvp_icd.json'))
cfg=w/'emulator/config/rpcs3/config.yml';s=cfg.read_text().replace('Max LLVM Compile Threads: 0','Max LLVM Compile Threads: 1').replace('Renderer: OpenGL','Renderer: Vulkan');cfg.write_text(s)
x=subprocess.Popen([str(w/'tooling/desktop/usr/bin/Xvfb'),':116','-extension','GLX','-screen','0','1280x720x24','-nolisten','unix','-listen','tcp','-ac'],env=env,stdout=open('evidence/xvfb.log','w'),stderr=subprocess.STDOUT)
time.sleep(1)
r=subprocess.Popen([str(w/'tooling/squashfs-root/usr/bin/rpcs3'),'--no-gui',str(w/'emulator/config/rpcs3/dev_hdd0/game/RFLP00001/USRDIR/EBOOT.BIN')],env=env,stdout=open('evidence/native-console.log','w'),stderr=subprocess.STDOUT)
last_command=None;launched=False;rows=[];start=time.monotonic();reason='timeout'
try:
 while time.monotonic()-start<900:
  if r.poll() is not None:reason='process exit';break
  values=subprocess.run(['ps','-C','rpcs3','-o','rss='],capture_output=True,text=True).stdout.split()
  rss=max([int(v) for v in values],default=0)
  rows.append({'seconds':round(time.monotonic()-start,3),'rss_kib':rss})
  if (w/'evidence/stop-native-116').exists() or rss>8*1024*1024:reason='8 GiB RSS guard';break
  app=w/'emulator/config/rpcs3/dev_hdd0/ruffle/ruffle-ps3-dev.log'
  if not launched and app.exists() and 'main menu ready' in app.read_text(errors='replace'):
   ids=subprocess.run([str(w/'tooling/desktop/usr/bin/xdotool'),'search','--name','Puffle|Ruffle'],env=env,capture_output=True,text=True).stdout.split()
   if ids:
    xd=str(w/'tooling/desktop/usr/bin/xdotool')
    subprocess.run([xd,'mousemove','551','479','click','1'],env=env)
    subprocess.run([xd,'mousemove','818','479','click','1'],env=env)
    subprocess.run([xd,'mousemove','365','479','click','1'],env=env)
    time.sleep(0.5)
    subprocess.run([str(w/'tooling/desktop/usr/bin/xdotool'),'windowfocus',ids[-1]],env=env)
    for step in range(int(os.environ.get('GAME_INDEX','1'))):
     subprocess.run([xd,'keydown','Down'],env=env)
     time.sleep(0.3)
     subprocess.run([xd,'keyup','Down'],env=env)
     time.sleep(0.2)
    subprocess.run([str(w/'tooling/desktop/usr/bin/xdotool'),'keydown','x'],env=env)
    time.sleep(0.5)
    subprocess.run([str(w/'tooling/desktop/usr/bin/xdotool'),'keyup','x'],env=env)
    launched=True
  command=w/'evidence/native-command.json'
  if command.exists() and command.stat().st_mtime_ns!=last_command:
   last_command=command.stat().st_mtime_ns
   c=json.loads(command.read_text())
   with open(w/'evidence/diagnostic-mario-inputs.jsonl','a') as history:history.write(json.dumps({'seconds':time.monotonic()-start,'command':c})+'\n')
   if 'click' in c:
    subprocess.run([str(w/'tooling/desktop/usr/bin/xdotool'),'mousemove',str(c['click'][0]),str(c['click'][1]),'click','1'],env=env)
   if 'shot' in c:
    from PIL import ImageGrab
    ImageGrab.grab(xdisplay=env['DISPLAY']).save(w/c['shot'])
   if 'key' in c:
    subprocess.run([str(w/'tooling/desktop/usr/bin/xdotool'),'keydown']+c['key'].split(),env=env)
    time.sleep(c.get('seconds',0.5))
    subprocess.run([str(w/'tooling/desktop/usr/bin/xdotool'),'keyup']+c['key'].split(),env=env)
  time.sleep(1)
finally:
 r.terminate();x.terminate()
 try:r.wait(timeout=5)
 except subprocess.TimeoutExpired:r.kill();r.wait()
 Path('evidence/native-memory.json').write_text(json.dumps({'reason':reason,'exit':r.returncode,'samples':rows},indent=2))
 print(reason,r.returncode,'peak MiB',max((a['rss_kib'] for a in rows),default=0)/1024,flush=True)
