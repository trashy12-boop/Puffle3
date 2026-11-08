import subprocess,time,os
from pathlib import Path
w=Path('/workspace/scratch/85efbb1d386e');e=os.environ.copy()
e.update(DISPLAY='127.0.0.1:96',XDG_CONFIG_HOME=str(w/'emulator/config'),XDG_CACHE_HOME=str(w/'emulator/cache'),LIBGL_ALWAYS_SOFTWARE='1',QT_X11_NO_MITSHM='1',LD_LIBRARY_PATH=str(w/'tooling/squashfs-root/usr/lib')+':'+str(w/'tooling/desktop/usr/lib/x86_64-linux-gnu'),LIBGL_DRIVERS_PATH=str(w/'tooling/desktop/usr/lib/x86_64-linux-gnu/dri'))
logpath=w/'emulator/config/rpcs3/dev_hdd0/ruffle/ruffle-ps3-dev.log'
logpath.write_text('')
(w/'emulator/config/rpcs3/dev_hdd0/ruffle/ruffle-core.log').write_text('')
x=subprocess.Popen([str(w/'tooling/desktop/usr/bin/Xvfb'),':96','-screen','0','1280x720x24','-nolisten','unix','-nolisten','local','-listen','tcp','-ac'],env=e,stdout=open(w/'logs/xvfb.log','w'),stderr=subprocess.STDOUT);time.sleep(1)
r=subprocess.Popen([str(w/'tooling/squashfs-root/usr/bin/rpcs3'),'--no-gui',str(w/'emulator/config/rpcs3/dev_hdd0/game/RFLP00001/USRDIR/EBOOT.BIN')],env=e,stdout=open(w/'logs/rpcs3.log','w'),stderr=subprocess.STDOUT)
try:
 d=[str(w/'tooling/desktop/usr/bin/xdotool')]
 for attempt in range(100):
  if 'main menu ready' in logpath.read_text():break
  if r.poll() is not None:break
  time.sleep(1)
 print('menu ready:',attempt,logpath.read_text(),flush=True)
 ids=subprocess.run(d+['search','--name','Ruffle'],env=e,capture_output=True,text=True).stdout.strip().splitlines()
 if not ids:raise RuntimeError('no game window')
 subprocess.run(d+['windowfocus',ids[-1]],env=e,check=True)
 def keys(*ks):
  for k in ks:
   subprocess.run(d+['keydown',k],env=e,check=True);time.sleep(.25);subprocess.run(d+['keyup',k],env=e,check=True);time.sleep(.2)
 def shot(name):subprocess.run(['import','-display',e['DISPLAY'],'-window','root',str(w/'logs'/name)],env=e,check=True,timeout=10)
 def chord():
  subprocess.run(d+['keydown','f','g'],env=e,check=True);time.sleep(.3);subprocess.run(d+['keyup','f','g'],env=e,check=True);time.sleep(.5)
 def exit_game():chord();keys('Down','Down','Down','x');time.sleep(1)
 keys('Down','x');time.sleep(3);shot('interactive-before.png')
 subprocess.run(d+['keydown','z'],env=e);time.sleep(1.5);subprocess.run(d+['keyup','z'],env=e);time.sleep(.5);shot('interactive-after.png')
 chord();shot('pause.png');keys('Down','Down','x');shot('game-controls.png');keys('Right','c');chord()
 time.sleep(.5);chord();keys('Down','x');time.sleep(2);exit_game()
 keys('Up','x');time.sleep(3);exit_game()
 keys('Down','Down','x');time.sleep(3);exit_game()
 keys('Down','x');time.sleep(2);exit_game();keys('x');time.sleep(2);exit_game();keys('Down','x');time.sleep(2);subprocess.run(d+['keydown','d'],env=e);time.sleep(.25);subprocess.run(d+['keyup','d'],env=e);keys('x');time.sleep(1);shot('mouse-final.png');exit_game();shot('browser-after.png')
 print('RPCS3 status:',r.poll())
 subprocess.run(['import','-display',e['DISPLAY'],'-window','root',str(w/'logs/screen.png')],env=e,timeout=10)
finally:
 r.terminate();x.terminate()
