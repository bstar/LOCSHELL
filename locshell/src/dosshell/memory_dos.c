/* Read-only DOS/EMS/XMS observations. Largest block includes shell-owned MCBs
 * which DOS releases when DOSSHELL.BAT launches the selected program. */
#include "memory.h"
#include <dos.h>
#include <string.h>
#include <stdlib.h>
static void (__far *xms_entry)(void);
static unsigned xms_big,xms_total,xms_error;
static void xms_query(void);
/* Large-model callers may keep a far-data segment in DS. These globals live
 * in DGROUP, addressed through SS even when the caller's DS is floating. */
#pragma aux xms_query = "mov ah,8" "call dword ptr ss:xms_entry" "mov ss:xms_big,ax" "mov ss:xms_total,dx" "mov ss:xms_error,bx" modify [ax bx cx dx]
void dm_query(void){union REGS r;struct SREGS sr;unsigned seg,owner,size,psp,parent=0,seen=0,run=0,i,type;unsigned long end,largest=0,own=0,total=0;void(__interrupt __far *vector)(void);
 memset(&dm_memory,0,sizeof dm_memory);r.h.ah=0x51;int86(0x21,&r,&r);psp=r.x.bx;
 if(getenv("DOSAGENT_VERSION")){unsigned p=*(unsigned __far*)MK_FP(psp,0x16);if(p>0x40&&p<0xa000&&_fmemcmp(MK_FP(p-1,8),"DOSAGENT",8)==0)parent=p;}
 r.h.ah=0x52;int86x(0x21,&r,&r,&sr);seg=*(unsigned __far*)MK_FP(sr.es,r.x.bx-2);
 for(i=0;i<4096&&seg>=0x40&&seg<0xa000;i++){
  type=*(unsigned char __far*)MK_FP(seg,0);owner=*(unsigned __far*)MK_FP(seg,1);size=*(unsigned __far*)MK_FP(seg,3);end=(unsigned long)seg+size+1UL;
  if((type!='M'&&type!='Z')||end>0xa000UL||end<=seg)break;
  seen=1;if(owner==psp||(parent&&owner==parent))own+=(unsigned long)size*16UL;
  if(owner==0||owner==psp||(parent&&owner==parent)){unsigned long bytes;if(!run)run=seg;else total+=16UL;total+=(unsigned long)size*16UL;bytes=(end-run-1UL)*16UL;if(bytes>largest)largest=bytes;}else run=0;
  if(type=='Z'||end==0xa000UL){dm_memory.conv_valid=1;break;}seg=(unsigned)end;
 }
 if(seen&&dm_memory.conv_valid){dm_memory.launch_bytes=largest;dm_memory.own_bytes=own;dm_memory.conv_total_bytes=total;}
 dm_memory.ems_valid=1;vector=_dos_getvect(0x67);
 if(vector&&_fmemcmp(MK_FP(FP_SEG(vector),10),"EMMXXXX0",8)==0){r.h.ah=0x40;int86(0x67,&r,&r);if(r.h.ah)dm_memory.ems_valid=0;else{r.h.ah=0x42;int86(0x67,&r,&r);if(r.h.ah)dm_memory.ems_valid=0;else dm_memory.ems_kb=r.x.bx>4095?65535:r.x.bx*16;}}
 dm_memory.xms_valid=1;r.x.ax=0x4300;int86(0x2f,&r,&r);if(r.h.al==0x80){r.x.ax=0x4310;int86x(0x2f,&r,&r,&sr);xms_entry=(void(__far*)(void))MK_FP(sr.es,r.x.bx);xms_query();if((xms_error&255)&&(xms_error&255)!=0xa0&&!xms_big)dm_memory.xms_valid=0;else{dm_memory.xms_largest_kb=xms_big;dm_memory.xms_free_kb=xms_total;}}
}
