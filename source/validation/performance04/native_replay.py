import subprocess,time,os,sys
from pathlib import Path
display=':'+str(100+time.time_ns()%10000)
w=Path.cwd();e=os.environ.copy();e.update(DISPLAY='127.0.0.1'+display,XDG_CONFIG_HOME=str(w/'emulator/config'),XDG_CACHE_HOME=str(w/'emulator/cache'),LIBGL_ALWAYS_SOFTWARE='1',QT_X11_NO_MITSHM='1',LD_LIBRARY_PATH=str(w/'tooling/desktop/usr/lib/x86_64-linux-gnu'))
prefix=sys.argv[2];duration=int(sys.argv[3]) if len(sys.argv)>3 else 30
log=w/'emulator/config/rpcs3/dev_hdd0/ruffle/ruffle-ps3-dev.log';log.write_text('')
x=subprocess.Popen([str(w/'tooling/desktop/usr/bin/Xvfb'),display,'-screen','0','1280x720x24','-nolisten','unix','-nolisten','local','-listen','tcp','-ac'],env=e,stdout=open(prefix+'-xvfb.log','w'),stderr=subprocess.STDOUT)
r=None
xdpath=str(w/'tooling/desktop/usr/bin/xdotool')
def xd(*a):return subprocess.run([xdpath,*a],env=e,capture_output=True,text=True,check=True)
def press(k):xd('keydown',k);time.sleep(.25);xd('keyup',k);time.sleep(.3)
def shot(n):subprocess.run(['import','-display',e['DISPLAY'],'-window','root',str(w/(prefix+'-'+n+'.png'))],env=e,check=True,timeout=10)
try:
 time.sleep(1)
 r=subprocess.Popen([str(w/'squashfs-root/usr/bin/rpcs3'),'--no-gui',str(w/sys.argv[1])],env=e,stdout=open(prefix+'-rpcs3.log','w'),stderr=subprocess.STDOUT)
 for i in range(70):
  time.sleep(1)
  if 'main menu ready' in log.read_text():break
  if r.poll()!=None:raise RuntimeError('RPCS3 exited')
 else:raise RuntimeError('menu timeout')
 # Dismiss initial RPCS3 welcome if present (same fixed virtual display).
 windows=xd('search','--name','Welcome').stdout.strip().splitlines() if False else []
 xd('mousemove','550','478');xd('click','1');xd('mousemove','818','478');xd('click','1');xd('mousemove','369','478');xd('click','1');time.sleep(.5)
 ids=xd('search','--name','Ruffle').stdout.strip().splitlines();print('game windows',ids,flush=True);xd('windowfocus',ids[-1]);
 if len(sys.argv)>5:
  names=sorted(p.name for p in (w/'emulator/config/rpcs3/dev_hdd0/ruffle/games').iterdir() if p.suffix.lower()=='.swf')
  index=names.index(sys.argv[5]);print('requested SWF',sys.argv[5],'index',index,flush=True)
  for _ in range(index):press('Down')
 press('x')
 time.sleep(3);shot('launched');print(log.read_text(),flush=True)
 time.sleep(duration);shot('steady')
 if len(sys.argv)>4 and sys.argv[4]=='timing-only':
  Path(prefix+'-app.log').write_text(log.read_text());print(log.read_text(),flush=True);raise SystemExit(0)
 xd('keydown','f','g');time.sleep(.3);xd('keyup','f','g');time.sleep(.6);shot('pause');press('c');time.sleep(1)
 # Restart and exit confirm lifecycle after the measured scene.
 xd('keydown','f','g');time.sleep(.3);xd('keyup','f','g');time.sleep(.5);press('Down');press('x');time.sleep(2)
 xd('keydown','f','g');time.sleep(.3);xd('keyup','f','g');time.sleep(.5);press('Down');press('Down');press('Down');press('x');time.sleep(1);shot('browser')
 Path(prefix+'-app.log').write_text(log.read_text());print(log.read_text(),flush=True)
finally:
 if r is not None:
  r.terminate()
  try:r.wait(timeout=10)
  except subprocess.TimeoutExpired:r.kill()
 x.terminate()
