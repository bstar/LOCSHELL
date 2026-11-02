"""Read-only prototype protocol over a raw serial device or DOSBox TCP UART."""
import socket,struct,time,os,termios,select
from contextlib import contextmanager
@contextmanager
def transport(a):
 if a.tcp:
  host,port=a.tcp.rsplit(':',1);s=socket.create_connection((host,int(port)),timeout=a.timeout or 10);s.settimeout(a.timeout or 10)
  try:yield s.sendall,s.recv
  finally:s.close()
 else:
  fd=os.open(a.serial,os.O_RDWR|os.O_NOCTTY);old=termios.tcgetattr(fd);new=list(old);new[0]=0;new[1]=0;new[2]=termios.CS8|termios.CREAD|termios.CLOCAL;new[3]=0;new[4]=new[5]=termios.B9600;new[6]=list(old[6]);new[6][termios.VMIN]=0;new[6][termios.VTIME]=1;termios.tcsetattr(fd,termios.TCSANOW,new)
  def receive(n):
   if not select.select([fd],[],[],a.timeout or 10)[0]:raise TimeoutError('Serial observer timed out')
   return os.read(fd,n)
  try:yield lambda b:os.write(fd,b),receive
  finally:termios.tcsetattr(fd,termios.TCSANOW,old);os.close(fd)
def request(send,recv,op):
 send(op.encode())
 def exact(n):
  data=b''
  while len(data)<n:
   part=recv(n-len(data))
   if not part:raise ConnectionError('Observer stream ended')
   data+=part
  return data
 header=exact(12)
 if header[:4]!=b'LOC1' or header[5]!=1:raise ValueError('Unexpected observer protocol')
 length=struct.unpack_from('<H',header,6)[0]
 if length>4084:raise ValueError('Oversized observer response')
 payload=exact(length);sequence=struct.unpack_from('<I',header,8)[0]
 if header[4:5]==b'E' and len(payload)!=1:raise ValueError('Invalid observer error frame')
 if header[4:5]==b'E':return {'state':'unsupported-video-mode','mode':payload[0]}
 if header[4:5]!=op.encode():raise ValueError('Mismatched observer response')
 if op=='H':
  if len(payload)!=8:raise ValueError('Invalid heartbeat length')
  ticks,paras,mode,_=struct.unpack('<IHBB',payload);return {'state':'alive','ticks':ticks,'resident_bytes':paras*16,'video_mode':mode,'sequence':sequence}
 cols,rows=struct.unpack_from('<HH',payload)
 if len(payload)!=4+cols*rows*2:raise ValueError('Incomplete screen snapshot')
 return {'state':'captured','columns':cols,'rows':rows,'sequence':sequence,'text':'\n'.join(payload[4+y*cols*2:4+(y+1)*cols*2:2].decode('cp437') for y in range(rows)),'cells_hex':payload[4:].hex()}
def handle(a):
 with transport(a) as (send,recv):return request(send,recv,'H' if a.resident_command=='heartbeat' else 'S')
