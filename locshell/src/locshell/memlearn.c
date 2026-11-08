/* Foreground collector. Invoked with absolute path to stop sampling before EXEC. */
#include "learn.h"
#include <dos.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <io.h>
int main(int argc,char**argv){union REGS r;struct SREGS s;DLRecord record;unsigned seg;FILE*f;memset(&record,0,sizeof record);
 r.x.ax=0xd74d;r.x.bx=0x4c4d;r.x.cx=0;int86x(0x2f,&r,&r,&s);if(r.x.ax!=0x4d4c){puts("Memory observer is not installed.");return 1;}seg=s.es;_fmemcpy(&record.stats,MK_FP(s.es,r.x.di),sizeof record.stats);
 if(record.stats.version!=1||record.stats.size!=sizeof record.stats){puts("Observer protocol mismatch; left resident.");return 2;}
 r.x.ax=0xd74d;r.x.bx=0x4c4d;r.x.cx=1;int86x(0x2f,&r,&r,&s);if(r.x.ax!=0x4d4c||r.x.cx){puts("Observer retained: another resident hook or TSR needs it. Reboot before another learning session.");return 2;}
 if(_dos_freemem(seg)){puts("Could not release observer memory.");return 2;}
 printf("Observed: resident=%u bytes, conventional peak=%lu bytes, EMS peak=%uK, XMS peak=%uK, failures=%u/%u/%u, samples=%u\n",record.stats.paras*16,(unsigned long)(record.stats.base_conv-record.stats.min_conv)*16UL,record.stats.base_ems-record.stats.min_ems,record.stats.base_xms-record.stats.min_xms,record.stats.fail_dos,record.stats.fail_ems,record.stats.fail_xms,record.stats.samples);
 if(argc>1&&!strcmp(argv[1],"/TEST"))return 0;
 f=fopen("C:\\LOCSHELL\\LEARN.REQ","r");if(!f){puts("No learning request; observation not attributed.");return 1;}if(fscanf(f,"%lx %lx",&record.id,&record.signature)!=2){fclose(f);return 1;}fclose(f);record.disposition=DL_PARTIAL;
 if(!dl_append(&record)){puts("Could not save observation; request retained.");return 1;}
 /* Every explicitly monitored run gets its own answer, including repeats
    where the game may have saved new sound/configuration settings. */
 f=fopen("C:\\LOCSHELL\\LEARN.ASK","w");if(f){fprintf(f,"%08lx\n",record.id);fclose(f);}remove("C:\\LOCSHELL\\LEARN.REQ");return 0;
}
