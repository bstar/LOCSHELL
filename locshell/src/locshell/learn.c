/* Allocation observations are evidence of a session, never a minimum. */
#include "learn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <io.h>
#include <ctype.h>
#include <sys/stat.h>
static unsigned long hash(unsigned long h,const unsigned char *s,unsigned n){while(n--)h=(h^*s++)*16777619UL;return h;}
static unsigned long hashfile(unsigned long h,const char*path){FILE*f;unsigned char b[512];unsigned n;f=fopen(path,"rb");if(!f)return hash(h,(const unsigned char*)"MISSING",7);while((n=fread(b,1,sizeof b,f))!=0)h=hash(h,b,n);fclose(f);return h;}
/* The generated root menu also describes other machines. Their labels and
   defaults do not change this machine's allocations. Hash the selected
   profile's driver definitions, excluding menu text and comments. */
static unsigned long hashconfig(unsigned long h,const char*path){FILE*f;char line[256],*p,*end;int menu=0;
 f=fopen(path,"r");if(!f)return hash(h,(const unsigned char*)"MISSING",7);
 while(fgets(line,sizeof line,f)){p=line;while(*p==' '||*p=='\t')p++;p[strcspn(p,"\r\n")]=0;
  end=p+strlen(p);while(end>p&&(end[-1]==' '||end[-1]=='\t'))*--end=0;
  if(*p=='['){menu=!stricmp(p,"[menu]");}
  if(menu||!*p||*p==';'||!strnicmp(p,"REM ",4))continue;
  h=hash(h,(const unsigned char*)p,strlen(p));h=hash(h,(const unsigned char*)"\0",1);
 }fclose(f);return h;
}
/* A known working allocation budget is comparable across restarts when the
   retained conventional-memory blocks have the same layout. Abstract away
   all free blocks and the shell/supervisor allocations that exit for a game. */
static unsigned long hashlayout(unsigned long h){union REGS r;struct SREGS s;unsigned psp,parent=0,seg,owner,size,i,type;unsigned long end;unsigned char name[8];
 r.h.ah=0x51;int86(0x21,&r,&r);psp=r.x.bx;
 if(getenv("DOSAGENT_VERSION")){unsigned p=*(unsigned __far*)MK_FP(psp,0x16);if(p>0x40&&p<0xa000&&!_fmemcmp(MK_FP(p-1,8),"DOSAGENT",8))parent=p;}
 segread(&s);r.h.ah=0x52;int86x(0x21,&r,&r,&s);seg=*(unsigned __far*)MK_FP(s.es,r.x.bx-2);
 for(i=0;i<4096&&seg>=0x40&&seg<0xa000;i++){
  type=*(unsigned char __far*)MK_FP(seg,0);owner=*(unsigned __far*)MK_FP(seg,1);size=*(unsigned __far*)MK_FP(seg,3);end=(unsigned long)seg+size+1UL;
  if((type!='M'&&type!='Z')||end>0xa000UL||end<=seg)return 0;
  if(owner&&owner!=psp&&(!parent||owner!=parent)){
   h=hash(h,(const unsigned char*)&seg,sizeof seg);h=hash(h,(const unsigned char*)&size,sizeof size);h=hash(h,(const unsigned char*)&owner,sizeof owner);
   _fmemcpy(name,MK_FP(seg,8),sizeof name);h=hash(h,name,sizeof name);
  }
  if(type=='Z'||end==0xa000UL)return h?h:1;seg=(unsigned)end;
 }return 0;
}
/* Resolve only simple catalog launch blocks. Complex shell syntax is partial.
   Executable bytes, local CFG/INI/SET files and boot/sound context are hashed.
   Missing or ambiguous targets return 0 and cannot receive a learned fit. */
static unsigned long signature(const char*commands,int legacy,const char*boot){unsigned long h=2166136261UL;const char *names[]={"BLASTER","SOUND","ULTRASND","CONFIG","MACHINE"};unsigned i,n=0,targets=0;const char*s;char drive='C',cwd[128]="\\LOCSHELL",line[256],path[260],token[130],*p;struct find_t ent;const char*patterns[]={"*.CFG","*.INI","*.SET","CONFIG.*"};
 if(!legacy)h=hash(h,(const unsigned char*)"LOC RAM 2",9);
 h=hash(h,(const unsigned char*)commands,strlen(commands));
 for(i=0;i<5;i++){s=getenv(names[i]);if(s)h=hash(h,(const unsigned char*)s,strlen(s));h=hash(h,(const unsigned char*)"\0",1);}
 if(legacy)h=hashfile(h,boot?boot:"C:\\CONFIG.SYS");
 else {
  s=getenv("MACHINE");n=0;
  if(s)while(n<9&&s[n]&&(isalnum((unsigned char)s[n])||s[n]=='_'))n++;
  if(s&&n&&n<=8&&!s[n]){sprintf(path,"C:\\BOOT\\%s\\CONFIG.SYS",s);h=hashconfig(h,path);sprintf(path,"C:\\BOOT\\%s\\AUTOEXEC.BAT",s);h=hashfile(h,path);}
  else h=hashconfig(h,"C:\\CONFIG.SYS");
 }
 h=hashfile(h,"C:\\AUTOEXEC.BAT");
 if(!legacy){h=hashlayout(h);if(!h)return 0;}
 s=commands;
 while(*s){n=0;while(*s&&*s!='\n'){if(n>=sizeof(line)-1)return 0;line[n++]=*s++;}if(*s)s++;line[n]=0;p=line;while(*p==' '||*p=='\t'||*p=='@')p++;if(!*p)continue;
  if(strlen(p)==2&&p[1]==':'){drive=(char)toupper(p[0]);continue;}
  if(!strnicmp(p,"CD ",3)){p+=3;while(*p==' ')p++;if(*p!='\\'||strlen(p)>=sizeof cwd)return 0;strcpy(cwd,p);continue;}
  if(!strnicmp(p,"REM ",4)||!strnicmp(p,"ECHO ",5)||!strnicmp(p,"SET ",4))continue;
  if(!strnicmp(p,"CALL ",5))p+=5;
  n=0;while(p[n]&&p[n]!=' '&&p[n]!='\t'){if(n>=sizeof token-1)return 0;token[n]=p[n];n++;}token[n]=0;
  if(strpbrk(token,"|<>%&")||strchr(token,34))return 0;
  if(token[1]==':')strcpy(path,token);else sprintf(path,"%c:%s\\%s",drive,cwd,token);
  if(!strrchr(token,'.')){strcat(path,".EXE");if(access(path,0))return 0;}
  if(access(path,0))return 0;
  if(stricmp(strrchr(path,'.'),".EXE")&&stricmp(strrchr(path,'.'),".COM"))return 0;
  h=hashfile(h,path);targets++;
  if(!legacy&&!stricmp(strrchr(path,'.'),".COM")){strcpy(strrchr(path,'.'),".EXE");if(!access(path,0))h=hashfile(h,path);}
 }
 if(!targets)return 0;
 for(i=0;i<(legacy?3:4);i++){sprintf(path,"%c:%s\\%s",drive,cwd,patterns[i]);if(!_dos_findfirst(path,0,&ent)){unsigned files=0;do{if(++files>64)return 0;sprintf(path,"%c:%s\\%s",drive,cwd,ent.name);if(!legacy&&i==3){const char*ext=strrchr(ent.name,'.');if(ext&&(!stricmp(ext,".EXE")||!stricmp(ext,".COM")||!stricmp(ext,".BAT")||!stricmp(ext,".BAK")||!stricmp(ext,".OLD")||!stricmp(ext,".TMP")||!stricmp(ext,".SAV")))continue;}if(ent.size>32768UL)return 0;h=hash(h,(const unsigned char*)ent.name,strlen(ent.name));h=hashfile(h,path);}while(!_dos_findnext(&ent));}}
 return h?h:1;
}
static unsigned long context_signature(const char*commands,int legacy,const char*boot){union REGS r;struct SREGS s;unsigned seg,off;unsigned long result;unsigned char work[128];
 segread(&s);r.h.ah=0x2f;int86x(0x21,&r,&r,&s);seg=s.es;off=r.x.bx;
 /* Isolate DOS searches without reading/writing a caller's DTA, which can
    refer to temporary library storage after a rename or stat operation. */
 segread(&s);s.ds=FP_SEG((void __far*)work);r.h.ah=0x1a;r.x.dx=FP_OFF((void __far*)work);int86x(0x21,&r,&r,&s);
 result=signature(commands,legacy,boot);
 segread(&s);s.ds=seg;r.h.ah=0x1a;r.x.dx=off;int86x(0x21,&r,&r,&s);return result;
}
unsigned long dl_signature(const char*commands){return context_signature(commands,0,NULL);}
unsigned long dl_legacy_signature(const char*commands,const char*boot){return context_signature(commands,1,boot);}
/* Keep only distinct launch contexts, in small blocks. Visible-row painting
   does no journal I/O and hashes each recorded launch once per refresh. */
#define DL_BLOCK 16
typedef struct {DLRecord record;unsigned long current,legacy,order;unsigned checked;} DLCache;
static DLCache *observed[DM_MAX/DL_BLOCK];
static unsigned observed_count,index_ready,index_error;
static DLCache *dl_at(unsigned i){return observed[i/DL_BLOCK]+i%DL_BLOCK;}
void dl_index(void){FILE*f;DLRecord r;unsigned i,n;unsigned long order=0;
 for(i=0;i<DM_MAX/DL_BLOCK;i++){free(observed[i]);observed[i]=NULL;}
 observed_count=0;index_ready=1;index_error=0;
 f=fopen("C:\\LOCSHELL\\LEARN.DAT","rb");if(!f)return;
 while((n=fread(&r,1,sizeof r,f))!=0){DLCache*c;
  if(n!=sizeof r||r.stats.version!=1||r.stats.size!=sizeof(DLStats)){index_error=1;break;}
  for(i=0;i<observed_count;i++)if(dl_at(i)->record.id==r.id&&dl_at(i)->record.signature==r.signature)break;
  if(i==observed_count){
   if(i==DM_MAX){index_error=1;break;}
   if(!(i%DL_BLOCK)&&!(observed[i/DL_BLOCK]=(DLCache*)calloc(DL_BLOCK,sizeof(DLCache)))){index_error=1;break;}
   observed_count++;
  }
  c=dl_at(i);order++;
  /* A pending repeat does not discard an earlier successful, confirmed run.
     Explicit failure/reset records do invalidate it. */
  if(r.disposition!=DL_PARTIAL||c->record.disposition!=DL_CONFIRMED){c->record=r;c->order=order;}
 }
 if(ferror(f))index_error=1;fclose(f);
}
int dl_has(unsigned long id){unsigned i;if(!index_ready)dl_index();for(i=0;i<observed_count;i++)if(dl_at(i)->record.id==id)return 1;return 0;}
int dl_latest(unsigned long id,DLRecord*out){FILE*f;DLRecord r;int found=0;f=fopen("C:\\LOCSHELL\\LEARN.DAT","rb");if(!f)return 0;while(fread(&r,sizeof r,1,f)==1){if(r.id==id&&r.stats.version==1&&r.stats.size==sizeof(DLStats)){*out=r;found=1;}}fclose(f);return found;}
int dl_append(const DLRecord*r){FILE*src,*dst;DLRecord old;int ok=1,exists=0;src=fopen("C:\\LOCSHELL\\LEARN.DAT","rb");if(src)exists=1;
 dst=fopen("C:\\LOCSHELL\\LEARN.TMP","wb");if(!dst){if(src)fclose(src);return 0;}
 if(src){while(fread(&old,sizeof old,1,src)==1){if(old.stats.version!=1||old.stats.size!=sizeof(DLStats)){ok=0;break;}if(fwrite(&old,sizeof old,1,dst)!=1){ok=0;break;}}if(ferror(src))ok=0;fclose(src);}
 if(ok&&fwrite(r,sizeof *r,1,dst)!=1)ok=0;if(fclose(dst))ok=0;if(!ok)return 0;
 remove("C:\\LOCSHELL\\LEARN.BAK");if(exists&&rename("C:\\LOCSHELL\\LEARN.DAT","C:\\LOCSHELL\\LEARN.BAK"))return 0;
 if(rename("C:\\LOCSHELL\\LEARN.TMP","C:\\LOCSHELL\\LEARN.DAT")){if(exists)rename("C:\\LOCSHELL\\LEARN.BAK","C:\\LOCSHELL\\LEARN.DAT");return 0;}dl_index();return 1;
}
static int dl_verified(unsigned long id,unsigned long signature,DLRecord*out){FILE*f;DLRecord r;int found=0;f=fopen("C:\\LOCSHELL\\LEARN.DAT","rb");if(!f)return 0;while(fread(&r,sizeof r,1,f)==1){if(r.id!=id||r.signature!=signature)continue;if(r.disposition==DL_REJECTED||r.disposition==DL_RESET)found=0;else if(r.disposition==DL_CONFIRMED){*out=r;found=1;}}fclose(f);return found;}
unsigned dl_confirmation(unsigned long id,unsigned long signature){DLRecord r;return dl_verified(id,signature,&r)?DL_CONFIRMED:DL_PARTIAL;}
int dl_prepare(unsigned long id,const char*commands){FILE*f;int ok;union REGS q;q.x.ax=0xd74d;q.x.bx=0x4c4d;q.x.cx=0;int86(0x2f,&q,&q);if(q.x.ax==0x4d4c)return 0;f=fopen("C:\\LOCSHELL\\LEARN.NEW","w");if(!f)return 0;ok=fprintf(f,"%08lx %08lx\n",id,dl_signature(commands))>0;if(fclose(f))ok=0;if(!ok)return 0;if(access("C:\\LOCSHELL\\LEARN.REQ",0)==0)return 0;return rename("C:\\LOCSHELL\\LEARN.NEW","C:\\LOCSHELL\\LEARN.REQ")==0;}
int dl_confirm(const DLRecord*record,unsigned disposition){DLRecord r=*record;r.disposition=disposition;return dl_append(&r);}
unsigned long dl_conv_budget(const DLRecord*r){const DLStats*s=&r->stats;if(s->min_conv>=s->base_conv)return 0;return ((unsigned long)(s->base_conv-s->min_conv)+s->paras)*16UL+16UL;}
DLFit dl_budget(const DLRecord*r,const DMMemory*m){const DLStats*s=&r->stats;unsigned ems,xms;unsigned long conv;
 if(s->version!=1||s->size!=sizeof(DLStats)||(s->valid&0xc007)!=7||!s->samples||s->min_conv>=s->base_conv||s->min_ems>s->base_ems||s->min_xms>s->base_xms)return DL_FIT_INCOMPLETE;
 conv=dl_conv_budget(r);ems=s->base_ems-s->min_ems;xms=s->base_xms-s->min_xms;
 if(!m->conv_valid||(ems&&!m->ems_valid)||(xms&&!m->xms_valid))return DL_FIT_INCOMPLETE;
 if((m->conv_total_bytes?m->conv_total_bytes:m->launch_bytes)<conv||m->ems_kb<ems||m->xms_free_kb<xms||m->xms_largest_kb<xms)return DL_FIT_LESS_RAM;
 /* EMS and XMS may draw from the same memory. Require enough backing for
    both recorded peaks instead of counting the same free pool twice. */
 if(ems&&xms&&(unsigned long)m->xms_free_kb<(unsigned long)ems+xms)return DL_FIT_LESS_RAM;
 return DL_FIT_READY;
}
DLFit dl_assess(unsigned long id,const char*commands,const DMMemory*m,DLRecord*out){unsigned i;DLCache*c,*found=NULL;unsigned long sig=0,old=0;int checked=0;
 if(out)out->id=0;if(!index_ready)dl_index();
 for(i=0;i<observed_count;i++){c=dl_at(i);if(c->record.id!=id)continue;
  if(!found||(c->record.disposition==DL_CONFIRMED&&found->record.disposition!=DL_CONFIRMED)||((c->record.disposition==DL_CONFIRMED)==(found->record.disposition==DL_CONFIRMED)&&c->order>found->order))found=c;
  if(c->checked){sig=c->current;old=c->legacy;checked=1;}}
 if(out&&found)*out=found->record;
 if(!found)return index_error?DL_FIT_INCOMPLETE:DL_FIT_NONE;
 if(index_error)return DL_FIT_INCOMPLETE;
 if(!checked){sig=dl_signature(commands);old=dl_legacy_signature(commands,NULL);for(i=0;i<observed_count;i++){c=dl_at(i);if(c->record.id==id){c->current=sig;c->legacy=old;c->checked=1;}}}
 if(!sig)return DL_FIT_INCOMPLETE;
 found=NULL;
 for(i=0;i<observed_count;i++){c=dl_at(i);if(c->record.id!=id)continue;
  if(c->record.signature==sig){found=c;break;}
  if(c->record.signature==old)found=c;
 }
 if(!found)return DL_FIT_CHANGED;
 if(out)*out=found->record;
 if(found->record.disposition==DL_REJECTED)return DL_FIT_FAILED;
 if(found->record.disposition!=DL_CONFIRMED)return DL_FIT_CONFIRM;
 /* Older fingerprints did not describe fragmentation. Keep their positive
    match conservative; a review-qualified capacity comparison is separate. */
 if(found->record.signature!=sig&&m->launch_bytes<dl_conv_budget(&found->record))return DL_FIT_LESS_RAM;
 return dl_budget(&found->record,m);
}
char dl_marker(unsigned long id,unsigned long signature,const DMMemory*m){DLRecord r;if(!signature||!dl_verified(id,signature,&r))return '?';return dl_budget(&r,m)==DL_FIT_READY?'~':'?';}
