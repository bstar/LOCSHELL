/* Exercise the actual server saver lifecycle with the shared island renderer. */
#define DA_GRAPHICS_SAVER
#define DA_ISLAND_SAVER
#define DA_SAVER_CONFIG "C:\\SAVER.CFG"
#include "../management/DASHBOARD.H"
#include <assert.h>
#include <direct.h>
#include <malloc.h>
static unsigned short original[2000];
static unsigned char font[8192];
static void (__interrupt __far *irq0)(void);
static void restored(void) {
 unsigned i; union REGS r;
 assert(!daSleeping && !daIsland && !daGraphics);
 assert(_dos_getvect(8)==irq0);
 assert(*((unsigned char far *)MK_FP(0x40,0x49))==3);
 for(i=0;i<2000;i++)assert(daVideo[i]==original[i]);
 daFontCopy(0);assert(!memcmp(font,daFont,8192));
 outp(0x3c7,2);assert(inp(0x3c9)==12);assert(inp(0x3c9)==23);assert(inp(0x3c9)==34);
 r.h.ah=3;r.h.bh=0;int86(0x10,&r,&r);assert(r.x.cx==0x040a && r.x.dx==0x0c22);
}
static void enter(void) {
 daSaver=1;daTimeout=1;daTimeoutTicks=18;daIdle=daNow()-40;
 daPoll(0);assert(daSleeping && daIsland && !isError);
}
int main(void) {
 unsigned i,round,plane;unsigned long start;union REGS r;FILE *f;
 char before[128],after[128];unsigned char *tmp;
 void far *reserve[3];
 freopen("ISLTEST.LOG","w",stderr);
 r.x.ax=3;int86(0x10,&r,&r);remove(DA_SAVER_CONFIG);daInit();
 for(i=1;i<=DA_SAVER_COUNT;i++){assert(daSelectSaver());assert(daSaver==i%DA_SAVER_COUNT);daInit();assert(daSaver==i%DA_SAVER_COUNT);}
 daSaver=DA_SAVER_OFF;daPreview();daPoll(0);assert(!daSleeping);
 daDraw("READY","--","Island saver lifecycle",0,0,"TEST");
 r.h.ah=1;r.x.cx=0x040a;int86(0x10,&r,&r);r.h.ah=2;r.h.bh=0;r.x.dx=0x0c22;int86(0x10,&r,&r);
 daFontCopy(0);daFont[65*32]^=0x7e;daFontCopy(1);memcpy(font,daFont,8192);
 outp(0x3c8,2);outp(0x3c9,12);outp(0x3c9,23);outp(0x3c9,34);
 for(i=0;i<2000;i++)original[i]=daVideo[i];
 irq0=_dos_getvect(8);assert(getcwd(before,sizeof(before)));
 /* Leave less memory than an otherwise empty DOS test: approximate server footprint. */
 reserve[0]=_fmalloc(60000U);reserve[1]=_fmalloc(60000U);reserve[2]=_fmalloc(40000U);
 assert(reserve[0] && reserve[1] && reserve[2]);
 for(round=0;round<4;round++) {
  enter();start=daNow();
  while(isFrames<28)daPoll(0);
  assert(isAdlib && !isFault && daNow()-start>20);
  assert(daHandleKey('-') && daIsland);assert(daHandleKey('+') && daIsland);
  assert(daHandleKey('m') && daIsland);assert(daHandleKey('m') && daIsland);
  assert(daHandleKey('n') && daIsland);
  if(round==0) {
   f=fopen("ISLAND.RAW","wb");assert(f);tmp=malloc(8000);assert(tmp);
   for(plane=0;plane<4;plane++) {
    outp(0x3ce,4);outp(0x3cf,plane);
    _fmemcpy(tmp,(void far *)MK_FP(0xa000,((isFrames&1)?1:0)*8192),8000);
    assert(fwrite(tmp,1,8000,f)==8000);
   }
   free(tmp);fclose(f);f=fopen("ISLFRAME.TXT","w");fprintf(f,"%lu %lu\n",isSceneTicks,isFrames-1);fclose(f);
  }
  if(round&1)assert(daHandleKey(27));else daPoll(1);
  restored();assert(getcwd(after,sizeof(after)) && !strcmp(before,after));
 }
 for(i=0;i<3;i++)_ffree(reserve[i]);
 assert(!rename("C:\\ISLAND\\SCENE.EGA","C:\\ISLAND\\MISSING.EGA"));
 daSaver=1;daPreview();daPoll(0);assert(daSleeping && !daIsland && isError==1);
 daPoll(1);restored();
 assert(!rename("C:\\ISLAND\\MISSING.EGA","C:\\ISLAND\\SCENE.EGA"));
 daSaver=DA_SAVER_OFF;daPreviewChoice=1;daPreviewPending=1;daPoll(1);
 assert(!daSleeping && daPreviewPending && daSaver==DA_SAVER_OFF);
 daPoll(0);assert(daSleeping && daIsland && daSaver==1);
 daPoll(1);assert(daSaver==DA_SAVER_OFF);restored();
 remove(DA_SAVER_CONFIG);f=fopen("ISLPASS.TXT","w");assert(f);fputs("PASS repeated entry, AdLib controls, FTP/key wake, IRQ/font/DAC/text/cursor/cwd restoration and missing-asset fallback\n",f);fclose(f);
 return 0;
}
