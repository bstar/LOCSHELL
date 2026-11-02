/* LoC management display. GPL-3.0-or-later. Foreground only; no TSR hooks. */
#ifndef LOC_DASHBOARD_H
#define LOC_DASHBOARD_H
#include <dos.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
static unsigned short far *daVideo;
static unsigned short daSaved[4000];
/* Compose the complete header off-screen, then commit only changed cells. */
static unsigned short daHeader[640];
static unsigned daHeaderWrites=0;
static int daComposing=0;
static void daCell(unsigned pos,unsigned short value){if(daComposing)daHeader[pos]=value;else daVideo[pos]=value;}
static unsigned daRows=25, daCols=80;
static unsigned long daIdle=0,daFrame=0,daTimeout=120,daTimeoutTicks=2184;
static int daSleeping=0;
#ifdef DA_GRAPHICS_SAVER
#define DA_STARS 96
static int daGraphics=0;
static unsigned char daVgaState[4096],daFont[8192];
static unsigned char far *daPixels=(unsigned char far *)MK_FP(0xa000,0);
static unsigned daOldMode=3;
static unsigned daReg(unsigned port,unsigned index){outp(port,index);return inp(port+1);}
static void daSetReg(unsigned port,unsigned index,unsigned value){outp(port,index);outp(port+1,value);}
/* Map VGA font plane temporarily, preserving every register we change. */
static void daFontCopy(int restore){unsigned s2,s4,g4,g5,g6,si,gi;unsigned i;
 unsigned char far *v=(unsigned char far *)MK_FP(0xa000,0);
 si=inp(0x3c4);gi=inp(0x3ce);s2=daReg(0x3c4,2);s4=daReg(0x3c4,4);
 g4=daReg(0x3ce,4);g5=daReg(0x3ce,5);g6=daReg(0x3ce,6);
 daSetReg(0x3c4,2,4);daSetReg(0x3c4,4,6);daSetReg(0x3ce,4,2);daSetReg(0x3ce,5,0);daSetReg(0x3ce,6,4);
 for(i=0;i<8192;i++){if(restore)v[i]=daFont[i];else daFont[i]=v[i];}
 daSetReg(0x3c4,2,s2);daSetReg(0x3c4,4,s4);daSetReg(0x3ce,4,g4);daSetReg(0x3ce,5,g5);daSetReg(0x3ce,6,g6);outp(0x3c4,si);outp(0x3ce,gi);
}
static void daGraphicsLeave(void);
static int daGraphicsEnter(void){union REGS r;struct SREGS seg;unsigned si,map,i;char *backend=getenv("DOSAGENT_VIDEO");
 if(backend&&!stricmp(backend,"text"))return 0;
 r.x.ax=0x1a00;int86(0x10,&r,&r);if(r.h.al!=0x1a||(r.h.bl!=7&&r.h.bl!=8))return 0;
 daOldMode=*((unsigned char far *)MK_FP(0x40,0x49));
 if(daOldMode!=3||daRows!=25)return 0;
 /* Nonstandard font banks keep the compatible text saver. */
 si=inp(0x3c4);map=daReg(0x3c4,3);outp(0x3c4,si);if(map)return 0;
 r.x.ax=0x1c00;r.x.cx=7;int86(0x10,&r,&r);if(r.h.al!=0x1c||r.x.bx>64)return 0;
 segread(&seg);seg.es=FP_SEG(daVgaState);r.x.ax=0x1c01;r.x.cx=7;r.x.bx=FP_OFF(daVgaState);int86x(0x10,&r,&r,&seg);if(r.h.al!=0x1c)return 0;
 daFontCopy(0);r.x.ax=0x13;int86(0x10,&r,&r);
 if(*((unsigned char far *)MK_FP(0x40,0x49))!=0x13){daGraphicsLeave();return 0;}
 /* One-byte pixels on GD5422, standard VGA timings, no bank switches/clock tweaks. */
 outp(0x3c8,0);outp(0x3c9,0);outp(0x3c9,0);outp(0x3c9,0);
 for(i=1;i<32;i++){outp(0x3c9,i*2+1);outp(0x3c9,i*2+1);outp(0x3c9,i*2+1);}
 return 1;
}
static void daGraphicsLeave(void){union REGS r;struct SREGS seg;
 r.x.ax=daOldMode;int86(0x10,&r,&r);daFontCopy(1);
 segread(&seg);seg.es=FP_SEG(daVgaState);r.x.ax=0x1c02;r.x.cx=7;r.x.bx=FP_OFF(daVgaState);int86x(0x10,&r,&r,&seg);daGraphics=0;
}
#else
#define DA_STARS 48
#endif
static int daX[DA_STARS],daY[DA_STARS];
static unsigned char daZ[DA_STARS];
static unsigned daPos[DA_STARS];
static unsigned daRandom=173,daCursor=0;
static unsigned daPitLast=0;
static unsigned long daPitBudget=0;
static unsigned long daRendered=0;
static int daPitReady=0;
/* 8254 read-back latches channel0 status and count, without reprogramming it.
   Mode3 needs OUT to distinguish its two half-cycles. */
static int daPitRead(unsigned *phase){unsigned status,count;outp(0x43,0xc2);
 status=inp(0x40);count=inp(0x40);count|=(unsigned)inp(0x40)<<8;
 if((status&0x30)!=0x30)return 0;
 if((status&14)==6)*phase=(unsigned)(((unsigned)(0-count)>>1)+((status&128)?0:32768u));
 else if((status&14)==4)*phase=(unsigned)(0-count);else return 0;
 return 1;}

/* Q8 perspective scale: 32 world units focal length, no division per frame.
   Same x/z,y/z geometry as standard starfield demos; original DOS implementation.
   Reference: raylib/examples/shapes/shapes_starfield_effect.c (not copied). */
static const unsigned daScale[256]={
 512,512,512,512,512,512,512,512,512,512,512,512,512,512,512,512,
 512,481,455,431,409,390,372,356,341,327,315,303,292,282,273,264,
 256,248,240,234,227,221,215,210,204,199,195,190,186,182,178,174,
 170,167,163,160,157,154,151,148,146,143,141,138,136,134,132,130,
 128,126,124,122,120,118,117,115,113,112,110,109,107,106,105,103,
 102,101,99,98,97,96,95,94,93,92,91,90,89,88,87,86,
 85,84,83,82,81,81,80,79,78,78,77,76,75,75,74,73,
 73,72,71,71,70,70,69,68,68,67,67,66,66,65,65,64,
 64,63,63,62,62,61,61,60,60,59,59,58,58,58,57,57,
 56,56,56,55,55,54,54,54,53,53,53,52,52,52,51,51,
 51,50,50,50,49,49,49,49,48,48,48,47,47,47,47,46,
 46,46,46,45,45,45,45,44,44,44,44,43,43,43,43,42,
 42,42,42,42,41,41,41,41,40,40,40,40,40,39,39,39,
 39,39,39,38,38,38,38,38,37,37,37,37,37,37,36,36,
 36,36,36,36,35,35,35,35,35,35,35,34,34,34,34,34,
 34,33,33,33,33,33,33,33,33,32,32,32,32,32,32,32
};
static unsigned daRand(void){daRandom=daRandom*25173u+13849u;return daRandom>>8;}
static void daResetStar(unsigned i,unsigned depth){daX[i]=(int)(daRand()&127)-64;daY[i]=(int)(daRand()&127)-64;daZ[i]=(unsigned char)depth;}
static unsigned daProject(unsigned i){int x,y;
#ifdef DA_GRAPHICS_SAVER
 if(daGraphics){x=160+((daX[i]*(int)daScale[daZ[i]])>>5);y=100+((daY[i]*(int)daScale[daZ[i]])>>5);
 return x>=0&&x<320&&y>=0&&y<200?(unsigned)y*320u+(unsigned)x:65535u;}
#endif
#ifdef DA_FPU_PROJECTION
 x=40+(int)((float)daX[i]*64.0f/(float)daZ[i]);
 y=(int)(daRows/2)+(int)((float)daY[i]*32.0f/(float)daZ[i]);
#else
 /* Products fit signed16 bits, including -64*512. No 32-bit helpers. */
 x=40+((daX[i]*(int)daScale[daZ[i]])>>7);
 y=(int)(daRows/2)+((daY[i]*(int)daScale[daZ[i]])>>8);
#endif
 return x>=0&&x<80&&y>=0&&y<(int)daRows?y*80+x:-1;}


static unsigned long daNow(void){return *((unsigned long far *)MK_FP(0x40,0x6c));}
static unsigned long daElapsed(unsigned long a,unsigned long b){return a>=b?a-b:0x1800b0UL-b+a;}
static void daText(int x,int y,int attr,const char *s){while(*s&&x<(int)daCols){daCell(y*daCols+x++,(attr<<8)|(unsigned char)*s++);}}
static void daLine(int row,int attr,int ch){unsigned x;for(x=0;x<daCols;x++)daCell(row*daCols+x,(attr<<8)|ch);}
static void daInit(void){unsigned mode=*((unsigned char far *)MK_FP(0x40,0x49));char *e=getenv("DOSAGENT_IDLE");
 daCols=*((unsigned short far *)MK_FP(0x40,0x4a));daRows=*((unsigned char far *)MK_FP(0x40,0x84))+1;
 if(daCols!=80||daRows<25||daRows>50){daCols=80;daRows=25;}
 daVideo=(unsigned short far *)MK_FP(mode==7?0xb000:0xb800,0);daIdle=daNow();
 if(e){long v=atol(e);if(v>=0&&v<=3600)daTimeout=(unsigned long)v;}
 daTimeoutTicks=daTimeout*182UL/10UL;
}
static void daWake(void){unsigned i;union REGS r;if(!daSleeping)return;
#ifdef DA_GRAPHICS_SAVER
 if(daGraphics)daGraphicsLeave();
#endif
 for(i=0;i<daCols*daRows;i++)daVideo[i]=daSaved[i];r.h.ah=1;r.x.cx=daCursor;int86(0x10,&r,&r);daSleeping=0;daIdle=daNow();}
static int daActivity(void){int was=daSleeping;daWake();daIdle=daNow();return was;}
static int daKey(void){return daActivity();}
static void daDraw(const char *state,const char *id,const char *command,unsigned clients,unsigned long sessions,const char *ip){
 unsigned maxblock=0,i,lit;char b[100];union REGS r;
 if(daSleeping)return;
 daComposing=1;
 for(i=0;i<8;i++)daLine(i,i==0?0x5f:0x07,' ');
 daText(2,0,0x5f,"LoSs oF CoNtRoL  //  MACHINE OPERATIONS");
 daText(2,1,0x0b,"[ DOSAGENT ]");daText(18,1,0x0f,"JOB CONTROL / FILE LINK");
 sprintf(b,"SESS %lu",sessions);daText(60,1,0x08,b);
 sprintf(b,"LINK %-15.15s   CLIENTS %u",ip,clients);daText(2,2,0x0b,b);
 sprintf(b,"STATE %-20.20s JOB %.12s",state,id);daText(2,3,0x0f,b);
 daText(2,4,0x07,"TASK  ");daText(8,4,0x0d,command);
 _dos_allocmem(0xffff,&maxblock);r.h.ah=0;int86(0x12,&r,&r);
 lit=r.x.ax?(unsigned)(((unsigned long)maxblock*16UL*24UL)/((unsigned long)r.x.ax*1024UL)):0;if(lit>24)lit=24;
 daText(2,5,0x07,"FREE DOS BLOCK [");for(i=0;i<24;i++)daCell(5*80+18+i,((i<lit?0x0a:0x08)<<8)|(i<lit?219:176));
 sprintf(b,"] %lu KB",(unsigned long)maxblock/64UL);daText(42,5,0x0f,b);
 sprintf(b,"Alt-H Help  Alt-X Exit  |  Idle stars: %lu s",daTimeout);daText(2,6,0x08,b);
 daLine(7,0x05,196);
 daComposing=0;daHeaderWrites=0;
 for(i=0;i<640;i++)if(daVideo[i]!=daHeader[i]){daVideo[i]=daHeader[i];daHeaderWrites++;}
}
static void daPoll(int active){unsigned long now=daNow();unsigned i;
 if(active){daActivity();return;}
 if(!daTimeout)return;
 if(!daSleeping&&daElapsed(now,daIdle)>=daTimeoutTicks){
  for(i=0;i<daCols*daRows;i++)daSaved[i]=daVideo[i];
  #ifdef DA_GRAPHICS_SAVER
  daGraphics=daGraphicsEnter();
  if(!daGraphics)
#endif
  for(i=0;i<daRows;i++)daLine(i,0,' ');
  {union REGS r;r.h.ah=3;r.h.bh=0;int86(0x10,&r,&r);daCursor=r.x.cx;r.h.ah=1;r.x.cx=0x2000;int86(0x10,&r,&r);}
  for(i=0;i<DA_STARS;i++){daResetStar(i,16+((daRand()*240u)>>8));daPos[i]=-1;}
  daSleeping=1;daFrame=now;daPitBudget=0;daPitReady=daPitRead(&daPitLast);
 }
 if(!daSleeping)return;
 if(daPitReady){unsigned phase,delta;if(!daPitRead(&phase)){daPitReady=0;return;}
  delta=phase-daPitLast;daPitLast=phase;daPitBudget+=delta;
  if(daPitBudget<39773UL)return;daPitBudget-=39773UL;
  if(daPitBudget>39773UL)daPitBudget=0;
 }else {if(daElapsed(now,daFrame)<1)return;}
 daFrame=now;daRendered++;
 /* Erase only previous stars, then redraw. At most 96 text-cell writes/frame.
    Erase all first so overlaps cannot erase another star's current position. */
 for(i=0;i<DA_STARS;i++)if(daPos[i]!=65535u){
#ifdef DA_GRAPHICS_SAVER
 if(daGraphics)daPixels[daPos[i]]=0;else
#endif
 daVideo[daPos[i]]=0x0020;}
 for(i=0;i<DA_STARS;i++){
  if(daZ[i]<=18)daResetStar(i,255);else daZ[i]-=2;
  daPos[i]=daProject(i);
  if(daPos[i]==65535u){daResetStar(i,255);daPos[i]=daProject(i);}
  if(daPos[i]!=65535u){
#ifdef DA_GRAPHICS_SAVER
  if(daGraphics)daPixels[daPos[i]]=1+((255-daZ[i])>>3);else
#endif
  daVideo[daPos[i]]=((daZ[i]>160?0x08:daZ[i]>80?0x07:0x0f)<<8)|(daZ[i]>80?'.':249);}
 }
}
#endif
