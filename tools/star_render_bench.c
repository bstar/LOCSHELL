#define DA_GRAPHICS_SAVER
#include "../management/DASHBOARD.H"
int main(void){unsigned i;unsigned long start,ticks,frames;int hires,graphics;
 daInit();daTimeout=1;daTimeoutTicks=18;daIdle=daNow()-40;daPoll(0);hires=daPitReady;graphics=daGraphics;
 daPitReady=0;start=daNow();for(i=0;i<1000;i++){daFrame=daNow()-3;daPoll(0);}ticks=daElapsed(daNow(),start);
 daPitReady=daPitRead(&daPitLast);daPitBudget=0;start=daNow();frames=daRendered;
 while(daElapsed(daNow(),start)<91)daPoll(0);
 frames=daRendered-frames;daWake();printf("1000 full sparse-VRAM frames: %lu BIOS ticks\n",ticks);printf("Timed animation: %lu frames in 91 BIOS ticks (~5 seconds); PIT=%d VGA13=%d stars=%d\n",frames,hires,graphics,DA_STARS);return 0;}
