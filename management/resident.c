/* Experimental serial observer TSR. GPL-3.0-or-later. No DOS calls in ISR.
   Explicit opt-in COM2/IRQ3 only; not loaded by production startup. */
#include <dos.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
static unsigned base=0x2f8,txpos=0,txlen=0,paras=0;
static unsigned long sequence=0;
static unsigned char buffer[4096];
static unsigned ss_value(void);
#pragma aux ss_value = "mov ax,ss" value [ax] modify exact [ax]
static unsigned sp_value(void);
#pragma aux sp_value = "mov ax,sp" value [ax] modify exact [ax]
static void response(unsigned char op){unsigned n=0,i,rows,cols,offset;unsigned mode;unsigned short far *screen;unsigned long ticks;
 if(txlen)return;
 buffer[0]='L';buffer[1]='O';buffer[2]='C';buffer[3]='1';buffer[4]=op;buffer[5]=1;
 sequence++;for(i=0;i<4;i++)buffer[8+i]=(unsigned char)(sequence>>(i*8));
 ticks=*((unsigned long far *)MK_FP(0x40,0x6c));mode=*((unsigned char far *)MK_FP(0x40,0x49));
 if(op=='H'){for(i=0;i<4;i++)buffer[12+i]=(unsigned char)(ticks>>(i*8));buffer[16]=paras;buffer[17]=paras>>8;buffer[18]=mode;buffer[19]=0;n=8;}
 else if(op=='S'){
  cols=*((unsigned short far *)MK_FP(0x40,0x4a));rows=*((unsigned char far *)MK_FP(0x40,0x84))+1;
  if((mode!=3&&mode!=7)||cols!=80||rows!=25){buffer[4]='E';buffer[12]=mode;n=1;}
  else{offset=*((unsigned short far *)MK_FP(0x40,0x4e));screen=(unsigned short far *)MK_FP(mode==7?0xb000:0xb800,offset);
   buffer[12]=80;buffer[13]=0;buffer[14]=25;buffer[15]=0;
   for(i=0;i<2000;i++){buffer[16+i*2]=(unsigned char)screen[i];buffer[17+i*2]=(unsigned char)(screen[i]>>8);}n=4004;}
 }else return;
 buffer[6]=n;buffer[7]=n>>8;txpos=0;txlen=n+12;outp(base+1,3);
}
static void __interrupt __far serial_irq(void){unsigned iir,guard=0;unsigned char c;
 while(guard++<8){iir=inp(base+2);if(iir&1)break;
  switch(iir&14){case 4:case 12:c=inp(base);response(c);break;
   case 2:if(txpos<txlen)outp(base,buffer[txpos++]);else{txlen=txpos=0;outp(base+1,1);}break;
   case 6:inp(base+5);break;default:inp(base+6);break;}
 }
 outp(0x20,0x20);
}
int main(int argc,char **argv){unsigned env;
 if(argc!=4||strcmp(argv[1],"--install")||strtoul(argv[2],NULL,16)!=0x2f8||atoi(argv[3])!=3){puts("Prototype: RXMON --install 2f8 3 (exclusive COM2 IRQ3,9600baud). Reboot to remove.");return 2;}
 if(*((unsigned short far *)MK_FP(0x40,2))!=base){puts("COM2 not listed by BIOS; not installed.");return 1;}
 _disable();outp(base+1,0);outp(base+3,0x80);outp(base,12);outp(base+1,0);outp(base+3,3);outp(base+2,0);outp(base+4,0x0b);inp(base+5);inp(base);inp(base+6);
 _dos_setvect(0x0b,serial_irq);outp(0x21,inp(0x21)&~8);outp(base+1,1);_enable();
 paras=ss_value()-_psp+((sp_value()+15)>>4);printf("RXMON resident paragraphs=%u bytes=%lu\n",paras,(unsigned long)paras*16UL);
 env=*((unsigned short far *)MK_FP(_psp,0x2c));if(env)_dos_freemem(env);
 fflush(stdout);close(0);close(1);close(2);_dos_keep(0,paras);return 0;
}
