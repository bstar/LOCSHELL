#!/usr/bin/env python3
"""Read H/S through a resident UART observer while DOS is busy in another EXE."""
from pathlib import Path
import os,socket,subprocess,time,sys,json,shutil
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'management'));import resident_client as client
B=R/'build/resident-test';D=B/'drive';D.mkdir(parents=True,exist_ok=True)
subprocess.run(['nix','--extra-experimental-features','nix-command flakes','develop','path:'+str(R/'nix'),'--command','wcl','-q','-bt=dos','-0','-ms','-os','-fe='+str(D/'BUSY.EXE'),str(R/'tools/resident_busy.c')],cwd=B,check=True)
shutil.copyfile(R/'build/RXMON.EXE',D/'RXMON.EXE')
probe=socket.socket();probe.bind(('127.0.0.1',0));port=probe.getsockname()[1];probe.listen(1);probe.settimeout(10)
conf=B/'test.conf';conf.write_text('[sdl]\noutput=surface\n[cpu]\ncputype=386\ncycles=fixed 12000\n[serial]\nserial1=dummy\nserial2=nullmodem server:127.0.0.1 port:'+str(port)+' transparent:1\n[mixer]\nnosound=true\n[autoexec]\nmount c '+str(D)+'\nc:\nRXMON.EXE --install 2f8 3 > MON.TXT\nBUSY.EXE\nexit\n')
for name in ['BUSY.RUN','BUSY.DON']:(D/name).unlink(missing_ok=True)
log=(B/'emulator.log').open('w');p=subprocess.Popen([os.environ.get('DOSBOX',shutil.which('dosbox-x') or 'dosbox-x'),'-conf',str(conf)],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy'},stdout=log,stderr=log)
try:
 deadline=time.monotonic()+10;s,_=probe.accept();probe.close()
 assert s is not None;s.settimeout(8)
 while not (D/'BUSY.RUN').exists() and time.monotonic()<deadline:time.sleep(.05)
 assert (D/'BUSY.RUN').exists(),'foreground child did not start'
 first=client.request(s.sendall,s.recv,'H');assert not (D/'BUSY.DON').exists()
 screen=client.request(s.sendall,s.recv,'S');assert 'FOREGROUND BUSY' in screen['text'];assert not (D/'BUSY.DON').exists(),'snapshot arrived only after child exited'
 second=client.request(s.sendall,s.recv,'H');assert second['ticks']>first['ticks'];assert not (D/'BUSY.DON').exists()
 s.close();p.wait(timeout=20);result={'passed':True,'heartbeat':first,'second_heartbeat':second,'snapshot_while_foreground_running':True,'screen':screen['text'],'ethernet_calls':0,'physical_uart_acceptance':False}
 result['busy_iterations_with_observer']=int((D/'BUSY.DON').read_text())
 (D/'BUSY.DON').unlink();baseline=B/'baseline.conf';baseline.write_text(conf.read_text().replace('serial2=nullmodem server:127.0.0.1 port:'+str(port)+' transparent:1','serial2=dummy').replace('RXMON.EXE --install 2f8 3 > MON.TXT','REM baseline without observer'))
 subprocess.run([os.environ.get('DOSBOX',shutil.which('dosbox-x') or 'dosbox-x'),'-conf',str(baseline)],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy'},stdout=log,stderr=log,timeout=25,check=True)
 result['busy_iterations_baseline']=int((D/'BUSY.DON').read_text());result['emulated_workload_reduction_percent']=100*(1-result['busy_iterations_with_observer']/result['busy_iterations_baseline'])
 (B/'results.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS resident heartbeat and text snapshot while unrelated DOS EXE is busy; resident bytes',first['resident_bytes'])
finally:
 if p.poll() is None:p.terminate();p.wait(timeout=5)
 log.close()
