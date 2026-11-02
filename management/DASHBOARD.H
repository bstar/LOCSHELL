/* LoC management display. GPL-3.0-or-later. Foreground only; no TSR hooks. */
#ifndef LOC_DASHBOARD_H
#define LOC_DASHBOARD_H
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned short far *daVideo;
static unsigned short daSaved[4000];
static unsigned daRows=25, daCols=80;
static unsigned long daIdle=0,daFrame=0,daTimeout=120;
static int daSleeping=0;
static int daX[48],daY[48],daSpeed[48];
static unsigned long daNow(void){return *((unsigned long far *)MK_FP(0x40,0x6c));}
static unsigned long daElapsed(unsigned long a,unsigned long b){return a>=b?a-b:0x1800b0UL-b+a;}
static void daText(int x,int y,int attr,const char *s){while(*s&&x<(int)daCols){daVideo[y*daCols+x++]=(attr<<8)|(unsigned char)*s++;}}
static void daLine(int row,int attr,int ch){unsigned x;for(x=0;x<daCols;x++)daVideo[row*daCols+x]=(attr<<8)|ch;}
static void daInit(void){unsigned mode=*((unsigned char far *)MK_FP(0x40,0x49));char *e=getenv("DOSAGENT_IDLE");
 daCols=*((unsigned short far *)MK_FP(0x40,0x4a));daRows=*((unsigned char far *)MK_FP(0x40,0x84))+1;
 if(daCols!=80||daRows<25||daRows>50){daCols=80;daRows=25;}
 daVideo=(unsigned short far *)MK_FP(mode==7?0xb000:0xb800,0);daIdle=daNow();
 if(e){long v=atol(e);if(v>=0&&v<=3600)daTimeout=(unsigned long)v;}
}
static void daWake(void){unsigned i;if(!daSleeping)return;for(i=0;i<daCols*daRows;i++)daVideo[i]=daSaved[i];daSleeping=0;daIdle=daNow();}
static int daKey(void){int was=daSleeping;daWake();daIdle=daNow();return was;}
static void daDraw(const char *state,const char *id,const char *command,unsigned clients,const char *ip){
 unsigned maxblock=0,i,lit;char b[100];union REGS r;
 if(daSleeping)return;
 for(i=0;i<8;i++)daLine(i,i==0?0x5f:0x07,' ');
 daText(2,0,0x5f,"LoSs oF CoNtRoL  //  MACHINE OPERATIONS");
 daText(2,1,0x0b,"[ DOSAGENT ]");daText(18,1,0x0f,"JOB CONTROL / FILE LINK");
 sprintf(b,"LINK %-15.15s   CLIENTS %u",ip,clients);daText(2,2,0x0b,b);
 sprintf(b,"STATE %-20.20s JOB %.12s",state,id);daText(2,3,0x0f,b);
 daText(2,4,0x07,"TASK  ");daText(8,4,0x0d,command);
 _dos_allocmem(0xffff,&maxblock);r.h.ah=0;int86(0x12,&r,&r);
 lit=r.x.ax?(unsigned)(((unsigned long)maxblock*16UL*24UL)/((unsigned long)r.x.ax*1024UL)):0;if(lit>24)lit=24;
 daText(2,5,0x07,"FREE DOS BLOCK [");for(i=0;i<24;i++)daVideo[5*80+18+i]=((i<lit?0x0a:0x08)<<8)|(i<lit?219:176);
 sprintf(b,"] %lu KB",(unsigned long)maxblock/64UL);daText(42,5,0x0f,b);
 sprintf(b,"Alt-H Help  Alt-X Exit  |  Idle stars: %lu s",daTimeout);daText(2,6,0x08,b);
 daLine(7,0x05,196);
}
static void daPoll(int active){unsigned long now=daNow();unsigned i;
 if(active){daWake();daIdle=now;return;}
 if(!daTimeout)return;
 if(!daSleeping&&daElapsed(now,daIdle)>=daTimeout*182UL/10UL){
  for(i=0;i<daCols*daRows;i++)daSaved[i]=daVideo[i];
  for(i=0;i<48;i++){daX[i]=rand()%80;daY[i]=rand()%((daRows-2)*4);daSpeed[i]=1+rand()%3;}
  daSleeping=1;daFrame=now;
 }
 if(!daSleeping||daElapsed(now,daFrame)<2)return;daFrame=now;
 for(i=0;i<daRows;i++)daLine(i,0,' ');
 for(i=0;i<48;i++){daY[i]+=daSpeed[i];if(daY[i]>=(int)(daRows-2)*4){daY[i]=0;daX[i]=rand()%80;}daVideo[(daY[i]/4)*80+daX[i]]=((daSpeed[i]==3?0x0f:daSpeed[i]==2?0x07:0x08)<<8)|(daSpeed[i]==3?'*':'.');}
 daText(23,daRows-1,0x05,"LoC // SYSTEM ONLINE // ANY KEY");
}
#endif
