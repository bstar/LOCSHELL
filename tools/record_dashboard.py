#!/usr/bin/env python3
"""Record real LOC SHELL FTP transfers in headless DOSBox-X for documentation.

Requires a LOC_SHELL_TEST build, a separately supplied NE2000 packet driver,
a DOSBox-X build with slirp, and ffmpeg. Never connects to a physical DOS PC.
"""
import argparse
import ftplib
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import zlib

R = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--packet-driver', type=Path, required=True)
p.add_argument('--output', type=Path, default=R / 'docs/assets/dashboard-transfers.gif')
p.add_argument('--dosbox', default='dosbox-x')
p.add_argument('--ffmpeg', default='ffmpeg')
p.add_argument('--port', type=int, default=15321)
a = p.parse_args()
exe = a.build.resolve() / 'LOCSHELL.EXE'
if b'TEST.KEY' not in exe.read_bytes():
    p.error('--build must contain an emulator test-hook build, not production')
if not a.packet_driver.is_file():
    p.error('Supply the external NE2000.COM packet driver; it is not bundled')
# Fail before starting DOS if another process owns the chosen control port.
with socket.socket() as probe:
    probe.bind(('127.0.0.1', a.port))
work = Path(tempfile.mkdtemp(prefix='dashboard-', dir=exe.parent))
c = work / 'drive'
dos, net, frames = c / 'DOSSHELL', c / 'NET', work / 'frames'
for d in (dos, net / 'AGENT', net / 'JOBS', c / 'GAMES/DEMO', frames):
    d.mkdir(parents=True, exist_ok=True)
shutil.copyfile(exe, dos / 'LOCSHELL.EXE')
shutil.copyfile(a.packet_driver, net / 'NE2000.COM')
(dos / 'LIBRARY.TXT').write_text(
    'Example game\n---FAVORITE---\nC:\nCD \\GAMES\\DEMO\nGAME.EXE\n\n')
identity = 2166136261
for byte in b'Example gameC:\nCD \\GAMES\\DEMO\nGAME.EXE\n':
    identity = ((identity ^ byte) * 16777619) & 0xffffffff
(dos / 'MEMORY.TSV').write_text('%08x|64|0|0|-|D|Example requirements\n' % identity)
(net / 'AGENT/SAVER.CFG').write_text('off\n')
(net / 'AGENT/IDLE.CFG').write_text('0\n')
(dos / 'TEST.KEY').write_text('000000\n')
# A genuine, empty disposable vault: no invented backup/sync success.
sys.path.insert(0, str(R / 'management'))
from saves.vault import Vault
from saves.dashboard import report, encode
vault = Vault(work / 'demo-vault')
vault.close()
(dos / 'VAULT.STA').write_bytes(encode(report(work / 'demo-vault')))
# Deliberately disposable emulator-only credentials. No access.json is read.
(net / 'USERS.TXT').write_text('demo sample [NONE] [ANY] ALL\n')
(net / 'MTCP.CFG').write_text(
    'PACKETINT 0x60\nIPADDR 10.0.2.15\nNETMASK 255.255.255.0\n'
    'GATEWAY 10.0.2.2\nNAMESERVER 10.0.2.3\nMTU 1500\nHOSTNAME LOCDEMO\n'
    'FTPSRV_PASSWORD_FILE C:\\NET\\USERS.TXT\nFTPSRV_CLIENTS 2\n'
    'FTPSRV_CLOBBER_FILES true\nFTPSRV_PASV_BASE 15400\nFTPSRV_PASV_PORTS 256\n')
conf = work / 'demo.conf'
conf.write_text(
    '[sdl]\noutput=surface\n[dosbox]\nquit warning=false\nmemsize=4\n'
    '[cpu]\ncore=normal\ncputype=386\ncycles=fixed 120000\nfpu=387\n'
    '[ne2000]\nne2000=true\nbackend=slirp\nnicbase=300\nnicirq=3\n'
    '[ethernet, slirp]\ntcp_port_forwards=' + str(a.port) + ':21 15400-15655\n'
    '[autoexec]\nmount c "' + str(c) + '"\nc:\n'
    'C:\\NET\\NE2000.COM 0x60 3 0x300 > C:\\NET\\DRIVER.TXT\n'
    'SET MTCPCFG=C:\\NET\\MTCP.CFG\nSET DOSAGENT_VERSION=2\n'
    'SET LOCSHELL_PROFILE=Demo PC\ncd \\dosshell\nLOCSHELL.EXE\nexit\n')


def snapshot():
    try:
        data = (dos / 'TEST.VID').read_bytes()
        return data if len(data) == 4000 else None
    except FileNotFoundError:
        return None


def wait_text(text):
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        data = snapshot()
        if data and text in data[::2].decode('cp437'):
            return
        if proc.poll() is not None:
            raise RuntimeError('DOS exited before ' + text)
        time.sleep(.05)
    raise RuntimeError('Timed out waiting for ' + text)


stop = threading.Event()
captured = []


def capture():
    while not stop.is_set():
        data = snapshot()
        if data:
            captured.append(data)
        stop.wait(.2)


ftp = None
recorder = None
log = (work / 'dosbox.log').open('w')
proc = subprocess.Popen([a.dosbox, '-fastlaunch', '-silent', '-nomenu', '-nogui',
                         '-conf', str(conf)], cwd=work,
                        env={**os.environ, 'SDL_VIDEODRIVER': 'dummy', 'SDL_AUDIODRIVER': 'dummy'},
                        stdout=log, stderr=subprocess.STDOUT)
try:
    wait_text('DASHBOARD')
    deadline = time.monotonic() + 20
    while True:
        ftp = ftplib.FTP()
        try:
            ftp.connect('127.0.0.1', a.port, timeout=5)
            ftp.login('demo', 'sample')
            ftp.sock.settimeout(30)
            break
        except (OSError, ftplib.Error):
            ftp.close()
            if time.monotonic() > deadline:
                raise
            time.sleep(.1)
    ftp.voidcmd('TYPE I')
    recorder = threading.Thread(target=capture)
    recorder.start()
    time.sleep(1.2)
    payload = bytes(range(256)) * 2048
    data = ftp.transfercmd('STOR /DRIVE_C/GAMES/DEMO/GAME.DAT')
    for at in range(0, len(payload), 8192):
        data.sendall(payload[at:at + 8192])
        time.sleep(.09)
    data.close()
    ftp.voidresp()
    wait_text('Received')
    time.sleep(.8)
    import io
    ftp.storbinary('STOR /DRIVE_C/GAMES/DEMO/SOUND.CFG', io.BytesIO(b'MUSIC=ADLIB\r\n'))
    time.sleep(1)
    # Set a small receive window before RETR, keeping actual DOS send progress
    # visible. Both directions are real FTP, deliberately paced for readability.
    host, port = ftp.makepasv()
    data = socket.socket()
    data.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
    data.settimeout(20)
    data.connect((host, port))
    assert ftp.sendcmd('RETR /DRIVE_C/GAMES/DEMO/GAME.DAT').startswith('150')
    received = bytearray()
    while True:
        part = data.recv(8192)
        if not part:
            break
        received.extend(part)
        time.sleep(.08)
    data.close()
    ftp.voidresp()
    assert bytes(received) == payload, 'FTP round-trip byte mismatch'
    metrics = ftp.sendcmd('SITE DASHBOARD')
    assert 'files_in=2 ' in metrics and 'files_out=1 ' in metrics, metrics
    wait_text('Sent')
    time.sleep(2.5)
    stop.set()
    recorder.join()
    ftp.sendcmd('SITE QUIT')
    ftp.close()
    ftp = None
    proc.wait(timeout=20)
    assert proc.returncode == 0
finally:
    stop.set()
    if recorder:
        recorder.join(timeout=3)
    if ftp:
        ftp.close()
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
    log.close()

assert len(captured) > 30
texts = [frame[::2].decode('cp437') for frame in captured]
for expected in ('DASHBOARD', 'RECEIVING', 'SENDING', 'File size not supplied', 'GAME.DAT', 'SOUND.CFG'):
    assert any(expected in text for text in texts), 'Not captured: ' + expected
font = (dos / 'TEST.FNT').read_bytes()
assert len(font) == 4096
palette = bytes(channel for color in (
    (0,0,0), (0,0,170), (0,170,0), (0,170,170), (170,0,0), (170,0,170), (170,85,0), (170,170,170),
    (85,85,85), (85,85,255), (85,255,85), (85,255,255), (255,85,85), (255,85,255), (255,255,85), (255,255,255)) for channel in color)


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)


# Reconstruct exactly the captured VGA text cells using the emulator's font.
# No UI/status animation is simulated or painted over the recording.
for index, cells in enumerate(captured):
    image = bytearray(400 * 641)
    for row in range(25):
        for y in range(16):
            out = (row * 16 + y) * 641 + 1
            for col in range(80):
                char, attr = cells[(row * 80 + col) * 2:(row * 80 + col) * 2 + 2]
                bits = font[char * 16 + y]
                for x in range(8):
                    image[out + col * 8 + x] = (attr & 15) if bits & (128 >> x) else ((attr >> 4) & 15)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 640, 400, 8, 3, 0, 0, 0))
    png += chunk(b'PLTE', palette) + chunk(b'IDAT', zlib.compress(bytes(image), 6)) + chunk(b'IEND', b'')
    (frames / ('%05d.png' % index)).write_bytes(png)
a.output = a.output.resolve()
a.output.parent.mkdir(parents=True, exist_ok=True)
subprocess.run([a.ffmpeg, '-hide_banner', '-loglevel', 'error', '-y', '-framerate', '5',
                '-i', str(frames / '%05d.png'), '-filter_complex',
                'scale=1280:800:flags=neighbor,split[a][b];[a]palettegen=max_colors=32[p];[b][p]paletteuse=dither=none',
                '-loop', '0', str(a.output)], check=True)
result = {'frames': len(captured), 'fps': 5, 'duration_seconds': len(captured) / 5,
          'capture': 'Real DOS VGA text cells and BIOS font; emulated Ethernet FTP',
          'sample_files_only': True, 'transfers_paced_for_readability': True,
          'round_trip_sha256': hashlib.sha256(payload).hexdigest(), 'metrics': metrics,
          'gif_bytes': a.output.stat().st_size,
          'packet_driver_sha256': hashlib.sha256(a.packet_driver.read_bytes()).hexdigest(),
          'test_executable_sha256': hashlib.sha256(exe.read_bytes()).hexdigest()}
(work / 'recording.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({**result, 'work_directory': str(work), 'output': str(a.output)}, indent=2))
