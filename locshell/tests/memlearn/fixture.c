#include <dos.h>
#include <stdio.h>
#include <string.h>
#include <io.h>
static void(__far*xms)(void);
static unsigned handle,ok;
static void xalloc(void);
#pragma aux xalloc = "mov ah,9" "mov dx,256" "call dword ptr ss:xms" "mov ss:ok,ax" "mov ss:handle,dx" modify [ax bx cx dx]
static void xfree(void);
#pragma aux xfree = "mov ah,10" "mov dx,ss:handle" "call dword ptr ss:xms" modify [ax bx cx dx]
int main(int argc,char**argv){union REGS r;struct SREGS sr;unsigned seg,largest,retained_shell=0,retained_agent=0,steps;unsigned long shell_named_bytes=0;FILE*f;unsigned flags;char name[9];unsigned char far*mcb;void(__interrupt __far*ems)(void);
 if(!_dos_allocmem(4096,&seg))_dos_freemem(seg);
 r.x.bx=65535;r.h.ah=0x48;int86(0x21,&r,&r);
 largest=r.x.bx;
 r.h.ah=0x52;int86x(0x21,&r,&r,&sr);seg=*(unsigned far*)MK_FP(sr.es,r.x.bx-2);
 for(steps=0;steps<4096;steps++) {
  unsigned next;mcb=MK_FP(seg,0);if(*mcb!='M'&&*mcb!='Z')break;
  _fmemcpy(name,mcb+8,8);name[8]=0;
  if(*(unsigned far*)(mcb+1)&&!memcmp(name,"LOCSHELL",8)){unsigned paras=*(unsigned far*)(mcb+3);shell_named_bytes+=(unsigned long)paras*16;if(paras>=8192)retained_shell++;}
  if(*(unsigned far*)(mcb+1)&&!memcmp(name,"DOSAGENT",8))retained_agent++;
  if(*mcb=='Z')break;next=seg+1+*(unsigned far*)(mcb+3);if(next<=seg)break;seg=next;
 }
 ems=_dos_getvect(0x67);
 if(ems&&_fmemcmp(MK_FP(FP_SEG(ems),10),"EMMXXXX0",8)==0){r.h.ah=0x43;r.x.bx=16;int86(0x67,&r,&r);if(!r.h.ah){r.h.ah=0x45;int86(0x67,&r,&r);}}
 r.x.ax=0x4300;int86(0x2f,&r,&r);if(r.h.al==0x80){r.x.ax=0x4310;int86x(0x2f,&r,&r,&sr);xms=(void(__far*)(void))MK_FP(sr.es,r.x.bx);xalloc();if(ok)xfree();}
 _asm {
 pushf
 pop ax
 mov flags,ax
 }
 f=fopen("FIXTURE.TXT","w");fprintf(f,"FIXTURE flags=%u XMS=%u largest_free=%lu retained_shell=%u retained_agent=%u shell_named_bytes=%lu\n",flags,ok,(unsigned long)largest*16,retained_shell,retained_agent,shell_named_bytes);fclose(f);
 if(!access("C:\\LOCSHELL\\NET\\WRITECFG.FLG",0)){f=fopen("CONFIG.U6","w");if(f){fputs("music=adlib\n",f);fclose(f);}}
 if(argc>1&&!strcmp(argv[1],"/JOB")){f=fopen("C:\\LOCSHELL\\NET\\ONCE.TXT","a");if(f){fputs("ONCE\n",f);fclose(f);}}puts("NATIVE-OUTPUT-PASS");return (flags&512)?0:3;
}
