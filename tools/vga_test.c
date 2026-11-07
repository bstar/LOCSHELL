#define DA_GRAPHICS_SAVER
#include "../management/DASHBOARD.H"
#include <assert.h>
static unsigned short original[2000];
static unsigned char originalFont[8192];
int main(int argc,char **argv){union REGS r;unsigned i,k;FILE *f;int graphics,mode2=argc>1&&!strcmp(argv[1],"mode2");
 freopen(argc<2?"VGA.LOG":"FALL.LOG","w",stderr);r.x.ax=mode2?2:3;int86(0x10,&r,&r);daInit();
 graphics=argc<2||mode2;
 if(!graphics&&!strcmp(argv[1],"text"))putenv("DOSAGENT_VIDEO=text");
 daDraw("COMPLETED","JTEST","VGA restoration test",0,0,"192.168.86.48");
 r.h.ah=1;r.x.cx=0x040a;int86(0x10,&r,&r);r.h.ah=2;r.h.bh=0;r.x.dx=0x0c22;int86(0x10,&r,&r);
 for(i=0;i<2000;i++)original[i]=daVideo[i];
 if(graphics){daFontCopy(0);daFont[65*32]^=0x7e;daFontCopy(1);memcpy(originalFont,daFont,8192);outp(0x3c8,2);outp(0x3c9,12);outp(0x3c9,23);outp(0x3c9,34);}
 daTimeout=1;daTimeoutTicks=18;daIdle=daNow()-40;daPoll(0);assert(daSleeping);assert(daGraphics==graphics);
 if(graphics)assert(*((unsigned char far *)MK_FP(0x40,0x49))==0x13);
 for(k=0;k<1000;k++){daPitReady=0;daFrame=daNow()-3;daPoll(0);for(i=0;i<DA_STARS;i++)assert(daPos[i]<(graphics?64000u:2000u));}
 if(graphics)for(i=0;i<64000u;i++)assert(daPixels[i]<32);
 assert(daActivity());assert(!daSleeping);assert(*((unsigned char far *)MK_FP(0x40,0x49))==(mode2?2:3));
 for(i=0;i<2000;i++)assert(original[i]==daVideo[i]);
 r.h.ah=3;r.h.bh=0;int86(0x10,&r,&r);assert(r.x.cx==0x040a);assert(r.x.dx==0x0c22);
 daIdle=daNow()-40;assert(!daActivity());assert(daElapsed(daNow(),daIdle)<2);
 if(graphics){daFontCopy(0);assert(!memcmp(originalFont,daFont,8192));outp(0x3c7,2);assert(inp(0x3c9)==12);assert(inp(0x3c9)==23);assert(inp(0x3c9)==34);}
 f=fopen(mode2?"MODE2PAS.TXT":graphics?"VGAPASS.TXT":"FALLPASS.TXT","w");fputs("PASS: 1000 frames, bounds, wake, exact text/font/palette restoration or forced text fallback\n",f);fclose(f);return 0;}
