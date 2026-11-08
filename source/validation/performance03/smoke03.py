import subprocess,time,os
from pathlib import Path
w=Path('/workspace/scratch/85efbb1d386e');e=os.environ.copy()
e.update(DISPLAY='127.0.0.1:96',XDG_CONFIG_HOME=str(w/'emulator/config'),XDG_CACHE_HOME=str(w/'emulator/cache'),LIBGL_ALWAYS_SOFTWARE='1',QT_X11_NO_MITSHM='1',QT_PLUGIN_PATH=str(w/'tooling/squashfs-root/usr/plugins'),LD_LIBRARY_PATH=str(w/'tooling/squashfs-root/usr/lib')+':'+str(w/'tooling/desktop/usr/lib/x86_64-linux-gnu'))
log=w/'emulator/config/rpcs3/dev_hdd0/ruffle/ruffle-ps3-dev.log'
x=subprocess.Popen([str(w/'tooling/desktop/usr/bin/Xvfb'),':96','-screen','0','1280x720x24','-nolisten','unix','-nolisten','local','-listen','tcp','-ac'],env=e,stdout=open(w/'tooling/xvfb03.log','w'),stderr=subprocess.STDOUT)
time.sleep(1)
r=subprocess.Popen([str(w/'tooling/squashfs-root/usr/bin/rpcs3'),'--no-gui',str(w/'emulator/config/rpcs3/dev_hdd0/game/RFLP00001/USRDIR/EBOOT.BIN')],env=e,stdout=open(w/'tooling/rpcs3-smoke03.log','w'),stderr=subprocess.STDOUT)
def xd(*args):return subprocess.run([str(w/'tooling/desktop/usr/bin/xdotool'),*args],env=e,capture_output=True,text=True,check=True)
def key(*ks):
 for k in ks:xd('keydown',k);time.sleep(.2);xd('keyup',k);time.sleep(.2)
def chord():xd('keydown','f','g');time.sleep(.25);xd('keyup','f','g');time.sleep(.4)
def shot(name):subprocess.run(['import','-display',e['DISPLAY'],'-window','root',str(w/'tooling'/name)],env=e,check=True)
try:
 for _ in range(60):
  if log.exists() and 'main menu ready' in log.read_text():break
  if r.poll() is not None:raise RuntimeError('RPCS3 exited')
  time.sleep(1)
 else:raise RuntimeError('menu timeout')
 print('main menu ready',flush=True)
 ids=xd('search','--name','Ruffle').stdout.strip().splitlines();xd('windowfocus',ids[-1]);key('x');time.sleep(3);shot('before03.png')
 xd('keydown','z');time.sleep(1);xd('keyup','z');time.sleep(.4);shot('after03.png');chord();shot('pause03.png');time.sleep(1);chord();time.sleep(1);chord();key('Down','x');time.sleep(2);chord();key('Down','Down','Down','x');time.sleep(1)
 print(log.read_text(),flush=True)
 assert 'Ruffle SWF launched' in log.read_text();assert 'Draw ' in log.read_text();assert 'player closed' in log.read_text();assert r.poll() is None
 print('packaged player smoke passed',flush=True)
finally:r.terminate();x.terminate()
