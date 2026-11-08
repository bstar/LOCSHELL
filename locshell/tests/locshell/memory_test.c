#include "memory.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(c) do{if(!(c)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char**argv){DMRequirement r;DMMemory m;FILE*f;
 memset(&r,0,sizeof r);memset(&m,0,sizeof m);r.id=0x12345678UL;r.conv=500;r.ems=0;r.xms=0;r.known=7;r.grade='D';strcpy(r.source,"Test manual");m.conv_valid=m.ems_valid=m.xms_valid=1;m.launch_bytes=500UL*1024UL;
 CHECK(dm_status(0,&m)=='?');CHECK(dm_status(&r,&m)=='+');m.launch_bytes--;CHECK(dm_status(&r,&m)=='-');r.grade='B';CHECK(dm_status(&r,&m)=='?');m.launch_bytes++;CHECK(dm_status(&r,&m)=='+');r.grade='L';CHECK(dm_status(&r,&m)=='?');r.grade='D';r.known=1;CHECK(dm_status(&r,&m)=='?');r.known=7;r.ems=1024;CHECK(dm_status(&r,&m)=='-');m.ems_kb=1024;r.xms=1024;m.xms_largest_kb=1024;CHECK(dm_status(&r,&m)=='?');r.ems=0;CHECK(dm_status(&r,&m)=='+');m.xms_valid=0;CHECK(dm_status(&r,&m)=='?');r.xms=0;m.xms_valid=1;
 CHECK(dm_load("TESTMEM.TSV"));CHECK(dm_set(&r));CHECK(dm_save("TESTMEM.TSV"));CHECK(dm_load("TESTMEM.TSV"));CHECK(dm_find(r.id)!=0);CHECK(dm_find(r.id)->conv==500);CHECK(!strcmp(dm_find(r.id)->source,"Test manual"));
 f=fopen("TESTMEM.TSV","w");fputs("12345678|640|0|0|D|Manual\n12345678|1|0|0|D|Duplicate\n",f);fclose(f);CHECK(!dm_load("TESTMEM.TSV"));CHECK(dm_find(r.id)==0);CHECK(!dm_save("TESTMEM.TSV"));
 f=fopen("TESTMEM.TSV","w");fputs("12345678|65536|0|0|D|Overflow\n",f);fclose(f);CHECK(!dm_load("TESTMEM.TSV"));remove("TESTMEM.TSV");remove("TESTMEM.BAK");
 {unsigned i;DMRequirement*first;
  f=fopen("TESTMEM.TSV","w");CHECK(f!=0);fclose(f);CHECK(dm_load("TESTMEM.TSV"));
  r.id=1;r.conv=1;CHECK(dm_set(&r));first=dm_find(1);
  for(i=2;i<=DM_MAX;i++){r.id=i;r.conv=i;CHECK(dm_set(&r));CHECK(dm_find(1)==first);}
  r.id=DM_MAX+1;CHECK(!dm_set(&r));CHECK(dm_save("TESTMEM.TSV"));CHECK(dm_load("TESTMEM.TSV"));
  CHECK(dm_find(1)==first);
  for(i=1;i<=DM_MAX;i++)CHECK(dm_find(i)!=0&&dm_find(i)->conv==i);
  remove("TESTMEM.TSV");remove("TESTMEM.BAK");
 }
 if(argc>1){unsigned i;unsigned long ids[]={0xbc9c9b7dUL,0xe6c1d80fUL,0x6673eefaUL};DMRequirement*p;
  CHECK(dm_load(argv[1]));m.launch_bytes=640UL*1024UL;m.ems_kb=4096;m.xms_largest_kb=m.xms_free_kb=4096;
  for(i=0;i<3;i++){p=dm_find(ids[i]);CHECK(p!=0);CHECK(!(p->known&2));CHECK(dm_status(p,&m)=='?');m.launch_bytes=300UL*1024UL;CHECK(dm_status(p,&m)=='-');m.launch_bytes=640UL*1024UL;}
  p=dm_find(0xc1b09238UL);CHECK(p!=0);CHECK(strstr(p->source,"music")!=0);CHECK(dm_status(p,&m)=='+');
 }
 puts("MEMORY_MODEL_OK");return 0;
}
