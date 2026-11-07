/* Exercise the actual dashboard and x87 renderer, including early wake. */
#define DA_GRAPHICS_SAVER
#define DA_TEST_SEED 0UL
#define DA_SAVER_CONFIG "C:\\SAVER.CFG"
#include "../management/DASHBOARD.H"
#include <assert.h>
#include <direct.h>
static unsigned short original[2000];
static unsigned char font[8192];
static void enter(unsigned choice);
static void restored(void);

static unsigned long seeded(unsigned long seed)
{
    unsigned i,control,after;unsigned long count=0,hash=5381,frame;FILE *movie;
    enter(1);assert(daMath);msEnd();assert(msBegin(1,seed));assert(msLastSeed==seed);
    _asm { fstcw control }
    while(msFrames<1UL&&count++<25000UL){
        daPoll(0);_asm { fstcw after }
        assert(control==after);
    }
    assert(msFrames==1UL);
    for(i=0;i<64000u;i++){assert(msImage[i]==daPixels[i]);hash=(hash*33)^msImage[i];}
    movie=fopen(seed==0x12345678UL?"SEEDA.BIN":"SEEDB.BIN","wb");assert(movie);
    assert(fwrite(msImage,1,64000u,movie)==64000u);
    while(msFrames<120UL&&count++<25000UL){
        frame=msFrames;daPoll(0);
        if(msFrames!=frame)assert(fwrite(msImage,1,64000u,movie)==64000u);
    }
    assert(!fclose(movie));
    assert(msFrames==120UL);
    assert(msBounces>0);
    for(i=0;i<64000u;i++)assert(msImage[i]==daPixels[i]);
    daKey();restored();return hash;
}

static void enter(unsigned choice)
{
    daSaver=choice;daTimeout=1;daTimeoutTicks=18;daIdle=daNow()-40;
    daPoll(0);assert(daSleeping);
}

static void restored(void)
{
    unsigned i;union REGS r;
    assert(!daSleeping);assert(!daMath);assert(msImage==NULL);
    assert(*((unsigned char far *)MK_FP(0x40,0x49))==3);
    for(i=0;i<2000;i++)assert(daVideo[i]==original[i]);
    daFontCopy(0);assert(!memcmp(font,daFont,8192));
    outp(0x3c7,2);assert(inp(0x3c9)==12);assert(inp(0x3c9)==23);assert(inp(0x3c9)==34);
    r.h.ah=3;r.h.bh=0;int86(0x10,&r,&r);assert(r.x.cx==0x040a);assert(r.x.dx==0x0c22);
}

int main(int argc,char **argv)
{
    unsigned i,kind,nonzero,view;unsigned long steps,a,b,c,last;FILE *f;union REGS r;char viewName[13];
    int fallback=argc>1;
    freopen("MATH.LOG","w",stderr);
    r.x.ax=3;int86(0x10,&r,&r);remove(DA_SAVER_CONFIG);daInit();
    assert(daSaver==0);assert(daFpu==!fallback);
    last=daFreshSeed();for(i=0;i<100;i++){a=daFreshSeed();assert(a&&a!=last);last=a;}
    for(i=1;i<=4;i++){
        assert(daSelectSaver());assert(daSaver==i%4);daInit();assert(daSaver==i%4);
    }
    remove(DA_SAVER_CONFIG);assert(!mkdir(DA_SAVER_CONFIG));
    assert(!daSelectSaver());assert(daSaver==0);assert(!rmdir(DA_SAVER_CONFIG));
    daSaver=3;daTimeout=1;daIdle=0;daPoll(0);assert(!daSleeping);
    daSaver=0;
    daDraw("READY","--","Math saver restoration test",0,0,"TEST");
    r.h.ah=1;r.x.cx=0x040a;int86(0x10,&r,&r);
    r.h.ah=2;r.h.bh=0;r.x.dx=0x0c22;int86(0x10,&r,&r);
    daFontCopy(0);daFont[65*32]^=0x7e;daFontCopy(1);memcpy(font,daFont,8192);
    outp(0x3c8,2);outp(0x3c9,12);outp(0x3c9,23);outp(0x3c9,34);
    for(i=0;i<2000;i++)original[i]=daVideo[i];
    daSaver=3;daPreview();daPoll(0);assert(!daSleeping);
    daSaver=1;daTimeout=0;daPreview();daPoll(0);assert(!daSleeping);
    daTimeout=1;daTimeoutTicks=18;
    daPreview();daPoll(1);assert(!daSleeping);
    daPreview();daPoll(0);assert(daSleeping);daKey();restored();
    daPreviewPending=1;daPoll(1);assert(!daSleeping&&daPreviewPending);
    daPoll(0);assert(daSleeping&&!daPreviewPending);daKey();restored();
    for(kind=1;kind<=2;kind++){
        enter(kind);assert(daGraphics);assert(daMath==!fallback);
        if(!fallback){
            /* Wake during incomplete rendering, then enter afresh. */
            for(i=0;i<11;i++)daPoll(0);
            daPoll(1);restored();enter(kind);assert(daMath);
            steps=0;
            while(msFrames<1UL&&steps++<250000UL)daPoll(0);
            if(kind==1){
                f=fopen("SCULPT0.RAW","wb");assert(f);
                assert(fwrite(msImage,1,64000u,f)==64000u);fclose(f);
                while(msFrames<12UL&&steps++<250000UL)daPoll(0);
            }
            assert(steps<250000UL);nonzero=0;
            for(i=0;i<64000u;i++){
                assert(msImage[i]==daPixels[i]);
                if(msImage[i]){assert(msImage[i]>=32);++nonzero;}
            }
            assert(nonzero>1000&&nonzero<63000u);
            if(kind==2){
                assert(msImage[100u*320u+200u]==0); /* c near 0: interior */
                assert(msImage[0]!=0);              /* far corner: exterior */
            }
            f=fopen(kind==1?"SCULPT.RAW":"MANDEL.RAW","wb");assert(f);
            assert(fwrite(msImage,1,64000u,f)==64000u);fclose(f);
            f=fopen("MATH.PAL","wb");assert(f);outp(0x3c7,0);
            for(i=0;i<768;i++)fputc(inp(0x3c9),f);
            fclose(f);
            f=fopen("MATHSTAT.TXT","a");assert(f);
            fprintf(f,"kind=%u frames=%lu polls=%lu pixels=%u\n",kind,msFrames,steps,nonzero);fclose(f);
            if(kind==2){
                steps=0;
                while(msFrames<2UL&&steps++<25000000UL)daPoll(0);
                assert(msFrames==2UL);
                f=fopen("ZOOM.RAW","wb");assert(f);
                assert(fwrite(msImage,1,64000u,f)==64000u);fclose(f);
                for(view=3;view<=6;view++){
                    steps=0;while(msFrames<(unsigned long)view&&steps++<25000000UL)daPoll(0);
                    assert(msFrames==(unsigned long)view);
                    sprintf(viewName,"MVIEW%u.RAW",view);f=fopen(viewName,"wb");assert(f);
                    assert(fwrite(msImage,1,64000u,f)==64000u);fclose(f);
                }
            }
        }
        assert(daKey());restored();
    }
    if(!fallback){
        a=seeded(0x12345678UL);b=seeded(0xabcdef01UL);c=seeded(0x12345678UL);
        assert(a!=b&&a==c);
    }
    remove(DA_SAVER_CONFIG);
    f=fopen(fallback?"NOFPUPAS.TXT":"MATHPASS.TXT","w");assert(f);
    fputs("PASS selection/persistence/off/preview, fresh/repeatable seeds, 120-frame motion/bounces, FPU control restore, rendered frames, mid-render client wake, exact text/font/palette/cursor restoration, memory release, FPU fallback\n",f);fclose(f);
    return 0;
}
