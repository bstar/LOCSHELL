#include "learn.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) if(!(x)){printf("LEARN_FAIL %d\n",__LINE__);return 1;}
int main(void){unsigned long sig,changed;DLRecord r,out;DMMemory m;FILE*f;
 const char*commands="C:\nCD \\LOCSHELL\nFIXTURE.EXE\n";
 remove("C:\\LOCSHELL\\LEARN.DAT");remove("C:\\LOCSHELL\\GAME.CFG");
 sig=dl_signature(commands);CHECK(sig!=0);CHECK(sig==dl_signature(commands));
 f=fopen("GAME.CFG","w");fputs("music=adlib\n",f);fclose(f);changed=dl_signature(commands);CHECK(changed!=sig);remove("GAME.CFG");CHECK(dl_signature(commands)==sig);
 CHECK(dl_signature("CALL UNKNOWN.BAT\n")==0);
 memset(&r,0,sizeof r);r.id=0x12345678UL;r.signature=sig;r.stats.version=1;r.stats.size=sizeof(DLStats);r.stats.valid=7;r.stats.samples=1;r.stats.base_conv=30000;r.stats.paras=88;r.stats.base_ems=r.stats.base_xms=1024;
 CHECK(dl_append(&r));CHECK(dl_latest(r.id,&out));CHECK(out.disposition==0);
 memset(&m,0,sizeof m);m.conv_valid=m.ems_valid=m.xms_valid=1;m.launch_bytes=500UL*1024UL;m.ems_kb=m.xms_largest_kb=m.xms_free_kb=2048;
 CHECK(dl_marker(r.id,sig,&m)=='?');CHECK(dl_confirm(&r,DL_CONFIRMED));CHECK(dl_marker(r.id,sig,&m)=='~');CHECK(dl_marker(r.id,sig+1,&m)=='?');CHECK(dl_marker(r.id,0,&m)=='?');m.launch_bytes=400UL*1024UL;CHECK(dl_marker(r.id,sig,&m)=='?');
 CHECK(dl_assess(r.id,commands,&m,&out)==DL_FIT_LESS_RAM);
 m.launch_bytes=481424UL;CHECK(dl_assess(r.id,commands,&m,&out)==DL_FIT_READY);
 m.launch_bytes--;CHECK(dl_assess(r.id,commands,&m,&out)==DL_FIT_LESS_RAM);m.launch_bytes++;
 m.ems_kb=1023;CHECK(dl_assess(r.id,commands,&m,&out)==DL_FIT_LESS_RAM);m.ems_kb=2048;
 m.xms_largest_kb=1023;CHECK(dl_assess(r.id,commands,&m,&out)==DL_FIT_LESS_RAM);m.xms_largest_kb=2048;
 r.stats.fail_dos=10;CHECK(dl_confirm(&r,DL_CONFIRMED));CHECK(dl_assess(r.id,commands,&m,&out)==DL_FIT_READY);
 r.stats.valid|=0x4000;CHECK(dl_budget(&r,&m)==DL_FIT_INCOMPLETE);r.stats.valid=7;
 CHECK(dl_confirm(&r,DL_RESET));CHECK(dl_assess(r.id,commands,&m,&out)==DL_FIT_CONFIRM);
 CHECK(dl_confirm(&r,DL_CONFIRMED));
 f=fopen("GAME.CFG","w");fputs("music=changed\n",f);fclose(f);dl_index();CHECK(dl_assess(r.id,commands,&m,&out)==DL_FIT_CHANGED);
 remove("GAME.CFG");dl_index();CHECK(dl_assess(r.id,commands,&m,&out)==DL_FIT_READY);
 /* A torn last record is discarded when writing the next complete journal. */
 f=fopen("LEARN.DAT","ab");fputs("torn",f);fclose(f);CHECK(dl_confirm(&r,DL_REJECTED));CHECK(dl_latest(r.id,&out));CHECK(out.disposition==DL_REJECTED);CHECK(dl_marker(r.id,sig,&m)=='?');CHECK(dl_assess(r.id,commands,&m,&out)==DL_FIT_FAILED);
 dl_index();CHECK(dl_has(r.id));CHECK(!dl_has(1));f=fopen("C:\\CONFIG.SYS","w");fputs("[menu]\nmenuitem=EM,First label\nmenudefault=EM,10\n[EM]\nDEVICE=HIMEM.SYS\n",f);fclose(f);sig=dl_signature(commands);
 f=fopen("C:\\CONFIG.SYS","w");fputs("[menu]\nmenuitem=EM,Renamed label\nmenudefault=EM,5\n[EM]\nDEVICE=HIMEM.SYS\n",f);fclose(f);CHECK(dl_signature(commands)==sig);
 f=fopen("C:\\CONFIG.SYS","a");fputs("DEVICE=EMM386.EXE\n",f);fclose(f);CHECK(dl_signature(commands)!=sig);
 {unsigned j;char data[512];memset(data,0,sizeof data);sig=dl_signature(commands);f=fopen("CONFIG.EXE","wb");for(j=0;j<80;j++)fwrite(data,1,sizeof data,f);fclose(f);CHECK(dl_signature(commands)==sig);remove("CONFIG.EXE");}
 puts("LEARN_MODEL_OK context fingerprints, confirmation, budgets, atomic recovery");return 0;
}
