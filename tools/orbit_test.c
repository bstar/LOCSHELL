/* Compare resumable register orbits against explicit qword rounding.
 * Both paths are the actual production kernels. GPL-3.0-or-later. */
#include "../management/mathsave.c"
#include <stdio.h>
#include <assert.h>
static double expectedR,expectedI,actualR,actualI;
#undef assert
#define assert(condition) do{if(!(condition)){failure=__LINE__;goto failed;}}while(0)

int main(void)
{
    unsigned point,mode,original,control,after,budget,want,got,wantIteration;
    unsigned random=971,beforeStatus,afterStatus,failure=0,j;
    unsigned long comparisons=0;
    FILE *f;
    freopen("ORBIT.LOG","w",stderr);
    _asm { fstcw original }
    for(mode=0;mode<4;mode++){
        control=(original&~0x0f00u)|0x0300u|(mode<<10);
        _asm { fldcw control }
        for(point=0;point<1024;point++){
            if(point==0){cr=0.;ci=0.;}
            else if(point==1){cr=-1.;ci=0.;}
            else if(point==2){cr=-.75;ci=.1;}
            else if(point==3){cr=-.743643887037151;ci=.131825904205330;}
            else if(point==4){cr=-2.;ci=0.;}
            else if(point==5){cr=1.e-300;ci=-1.e-300;}
            else{
                random=random*25173u+13849u;cr=-2.2+(double)random*(3.4/65536.);
                random=random*25173u+13849u;ci=-1.275+(double)random*(2.55/65536.);
            }
            expectedR=expectedI=actualR=actualI=0.;iteration=0;
            registerOrbits=1;registerCoordinatesSafe=orbitCoordinateNormal(cr)&&orbitCoordinateNormal(ci);
            do{
                random=random*25173u+13849u;
                switch(random&7){case 0:budget=1;break;case 1:budget=2;break;case 2:budget=7;break;default:budget=64;}
                wantIteration=iteration;zr=expectedR;zi=expectedI;
                want=mandelOrbitMemory(budget);expectedR=zr;expectedI=zi;
                got=iteration;iteration=wantIteration;wantIteration=got;
                zr=actualR;zi=actualI;
                _asm { fnstsw beforeStatus }
                got=mandelBatch(budget);actualR=zr;actualI=zi;
                _asm { fstcw after }
                _asm { fnstsw afterStatus }
                assert(after==control);
                assert((beforeStatus&0x3800)==(afterStatus&0x3800));
                assert(got==want&&iteration==wantIteration);
                assert(!_fmemcmp(&expectedR,&actualR,8));
                assert(!_fmemcmp(&expectedI,&actualI,8));
                ++comparisons;
            }while(!(got&0x100));
        }
    }
    control=(original&~0x0300u)|0x0200u;
    _asm { fldcw control }
    assert(msBegin(2,0));assert(!registerOrbits);msEnd();
    control=(original&~0x0300u)|0x0300u;
    _asm { fldcw control }
    assert(msBegin(2,0));assert(registerOrbits);msEnd();
    _asm { fldcw original }
    f=fopen("ORBPASS.TXT","w");assert(f);
    fprintf(f,"PASS: %lu resumable orbit comparisons across 4096 point/rounding-mode cases; identical double states, work/iteration counts, FPU stack/control restoration and precision fallback\n",comparisons);
    fclose(f);return 0;
failed:
    _asm { fldcw original }
    f=fopen("ORBITERR.TXT","w");
    if(f){
        fprintf(f,"line=%u point=%u mode=%u got=%u want=%u iteration=%u expected=%u cw=%x after=%x top=%x/%x\n",failure,point,mode,got,want,iteration,wantIteration,control,after,beforeStatus,afterStatus);
        for(j=0;j<4;j++)fprintf(f,"R %04x/%04x I %04x/%04x\n",((unsigned far *)&expectedR)[j],((unsigned far *)&actualR)[j],((unsigned far *)&expectedI)[j],((unsigned far *)&actualI)[j]);
        fclose(f);
    }
    return 1;
}
