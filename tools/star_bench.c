#include "../management/DASHBOARD.H"
#include <float.h>
int main(void){unsigned i,k,phase;unsigned long start,ticks;volatile int result=0;FILE *f;
 daInit();start=daNow();for(k=0;k<1000;k++)for(i=0;i<48;i++){daX[i]=(i&127)-24;daY[i]=(i&63)-24;daZ[i]=16+(i*3);result=daProject(i);}
 ticks=daElapsed(daNow(),start);
 f=fopen("C:\\NET\\JOBS\\BENCH.TXT","a");if(!f)return 1;
 fprintf(f,"%s projection: 1000 frames, %lu BIOS ticks; FPU=%u PIT=%d\n",
#ifdef DA_FPU_PROJECTION
 "387",
#else
 "integer16",
#endif
 ticks,(unsigned)((*((unsigned short far *)MK_FP(0x40,0x10))&2)!=0),daPitRead(&phase));fclose(f);return 0;}
