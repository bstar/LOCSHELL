#include "../management/DASHBOARD.H"
#include <assert.h>
int main(void){FILE *f;unsigned i,k;static unsigned short snapshot[2000];union REGS r;r.x.ax=3;int86(0x10,&r,&r);daInit();daDraw("COMPLETED (rc=0)","J50723F3","Inventory C: D: E: F:",0,0,"192.168.86.48");daDraw("COMPLETED (rc=0)","J50723F3","Inventory C: D: E: F:",0,0,"192.168.86.48");assert(daHeaderWrites==0);
daDraw("COMPLETED (rc=0)","J50723F3","Inventory C: D: E: F:",0,1,"192.168.86.48");assert(daHeaderWrites==1);
daDraw("READY","--","x",0,1,"192.168.86.48");assert((daVideo[4*80+9]&255)==' ');
daDraw("COMPLETED (rc=0)","J50723F3","Inventory C: D: E: F:",0,0,"192.168.86.48");
for(i=0;i<2000;i++)snapshot[i]=daVideo[i];f=fopen("SCREEN.BIN","wb");fwrite(snapshot,2,2000,f);fclose(f);daTimeout=1;daTimeoutTicks=18;daIdle=daNow()-40;daPoll(0);assert(daSleeping);daPitReady=0;daFrame=daNow()-3;daPoll(0);/* Nearer stars must project farther from the center. */
{int farpos,nearpos;daX[0]=50;daY[0]=30;daZ[0]=200;farpos=daProject(0);daZ[0]=100;nearpos=daProject(0);assert(nearpos%80>farpos%80);assert(nearpos/80>farpos/80);}
for(k=0;k<1000;k++){daPitReady=0;daFrame=daNow()-3;daPoll(0);for(i=0;i<2000;i++){unsigned c=daVideo[i]&255;assert(c==' '||c=='.'||c==249);assert(!(daVideo[i]&0x8000));}for(i=0;i<DA_STARS;i++)assert(daPos[i]>=0&&daPos[i]<2000);}
assert(daKey());assert(!daSleeping);for(i=0;i<2000;i++)assert(snapshot[i]==daVideo[i]);daIdle=daNow()-40;daPoll(1);assert(!daSleeping);daTimeout=0;daIdle=0;daPoll(0);assert(!daSleeping);f=fopen("PASS.TXT","w");fputs("PASS idempotent header (zero writes), single-cell counter, shorter labels, perspective projection, 1000 bounded text-free frames, wake/restore, active-session inhibition\n",f);fclose(f);return 0;}
