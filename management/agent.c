/* DOS foreground job supervisor, GPL-3.0-or-later. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#include <dos.h>
#include "DASHBOARD.H"
#ifdef LOC_AGENT_SCREEN
#include "LOCSTYLE.H"
#include "LOCPROF.H"
static int locShellMode;
static unsigned locCursor,locCursorPos;
static void locText(unsigned x,unsigned y,unsigned width,const char*s,unsigned attr){unsigned i;for(i=0;i<width&&x+i<80;i++){unsigned c=' ';if(*s)c=(unsigned char)*s++;daVideo[y*80+x+i]=(attr<<8)|c;}}
static void locJobDraw(const char*phase,const char*id,const char*label,int starting){union REGS r;unsigned i;char line[160];
 if(starting){r.h.ah=3;r.h.bh=0;int86(0x10,&r,&r);locCursor=r.x.cx;locCursorPos=r.x.dx;}
 r.x.ax=0x1003;r.h.bl=0;r.h.bh=0;int86(0x10,&r,&r);
 r.h.ah=1;r.x.cx=0x2000;int86(0x10,&r,&r);
 for(i=0;i<2000;i++)daVideo[i]=((unsigned)LOC_BODY_ATTR<<8)|' ';
 locText(0,0,80,"",LOC_PANEL_ATTR);locText(2,0,LOC_TITLE_WIDTH,loc_shell_title(),LOC_PANEL_ATTR);
 locText(0,1,80,"",LOC_PANEL_ATTR);locText(2,1,76,"Management job",LOC_PANEL_ATTR);
 sprintf(line,"%s: %.90s",phase,label);locText(2,4,76,line,LOC_BODY_ATTR);
 sprintf(line,"Job ID: %.12s",id);locText(2,6,76,line,LOC_BODY_ATTR);
 locText(2,9,76,"FTP pauses until this foreground job finishes.",LOC_BODY_ATTR);
 locText(2,11,76,"LOC SHELL returns automatically with the result.",LOC_BODY_ATTR);
 locText(0,LOC_CONNECTION_ROW,80,"",LOC_PANEL_ATTR);locText(1,LOC_CONNECTION_ROW,12,LOC_CONNECTION_LABEL,LOC_PANEL_ATTR);
 locText(14,LOC_CONNECTION_ROW,63,"FTP paused | Foreground management job",LOC_PANEL_ATTR);
 locText(0,LOC_JOB_ROW,80,"",LOC_PANEL_ATTR);locText(1,LOC_JOB_ROW,12,LOC_JOB_LABEL,LOC_PANEL_ATTR);
 sprintf(line,"%.20s: %.30s [%.8s]",phase,label,id);locText(14,LOC_JOB_ROW,63,line,LOC_PANEL_ATTR);
 locText(78,LOC_JOB_ROW,1,!strnicmp(phase,"Failed",6)?"!":starting?">":"+",LOC_PANEL_ATTR);
#ifdef LOC_AGENT_TEST
 {FILE*f=fopen(starting?"C:\\DOSSHELL\\JOBRUN.VID":"C:\\DOSSHELL\\JOBDONE.VID","wb");unsigned short row[80];if(f){for(i=0;i<25;i++){_fmemcpy(row,daVideo+i*80,sizeof row);fwrite(row,sizeof row,1,f);}fclose(f);}}
#endif
}
static void locRestoreCursor(void){union REGS r;r.h.ah=1;r.x.cx=locCursor;int86(0x10,&r,&r);r.h.ah=2;r.h.bh=0;r.x.dx=locCursorPos;int86(0x10,&r,&r);r.x.ax=0x1003;r.h.bl=1;r.h.bh=0;int86(0x10,&r,&r);}
#endif
#define ROOT "C:\\NET\\JOBS"
static int state(const char *s) { FILE *f=fopen("STATE.TXT","w"); int ok; if(!f)return 0;ok=fputs(s,f)>=0;if(fclose(f))ok=0;return ok; }
static int execute(void) {
 char cwd[80],prog[128],label[96],args[128],kind[16],*v[32],*p; int n=1,out,old1,old2,old0,nul,rc; FILE *f;
 f=fopen("REQUEST.TXT","r"); if(!f)return -1;
 if(!fgets(kind,sizeof(kind),f)||!fgets(cwd,sizeof(cwd),f)||!fgets(prog,sizeof(prog),f)||!fgets(args,sizeof(args),f)){fclose(f);state("failed-to-start\n");return -1;}
 fclose(f); kind[strcspn(kind,"\r\n")]=0;cwd[strcspn(cwd,"\r\n")]=0;prog[strcspn(prog,"\r\n")]=0;args[strcspn(args,"\r\n")]=0;
 strncpy(label,prog,sizeof(label)-1);label[sizeof(label)-1]=0;f=fopen("LABEL.TXT","r");if(f){fgets(label,sizeof(label),f);label[strcspn(label,"\r\n")]=0;fclose(f);}
 out=open("OUTPUT.TXT",O_CREAT|O_TRUNC|O_WRONLY|O_BINARY,S_IREAD|S_IWRITE);if(out<0){state("failed-to-start\n");return -1;}
 if(!state("running\n")){close(out);return -1;}old0=dup(0);nul=open("NUL",O_RDONLY);dup2(nul,0);close(nul);old1=dup(1);old2=dup(2);dup2(out,1);dup2(out,2);close(out);
 {char id[20]="--";FILE *j=fopen("ID.TXT","r");if(j){fgets(id,sizeof(id),j);fclose(j);}id[strcspn(id,"\r\n")]=0;daInit();
#ifdef LOC_AGENT_SCREEN
 if(locShellMode)locJobDraw("Running",id,label,1);else
#endif
 daDraw("RUNNING (foreground)",id,label,0,0,"FTP paused");}
 if(cwd[1]!=':'||_chdrive((cwd[0]&~32)-'A'+1)||chdir(cwd)){rc=-1;}
 else if(!strcmp(kind,"BAT")) { p=getenv("COMSPEC");rc=spawnl(P_WAIT,p?p:"C:\\COMMAND.COM",p?p:"COMMAND.COM","/C",prog,NULL); }
 else {v[0]=prog;p=strtok(args,"\t");while(p&&n<31){v[n++]=p;p=strtok(NULL,"\t");}v[n]=NULL;rc=spawnv(P_WAIT,prog,(const char * const *)v);}
 fflush(stdout);fflush(stderr);dup2(old1,1);dup2(old2,2);close(old1);close(old2);dup2(old0,0);close(old0);
 _chdrive(3);chdir(ROOT);chdir("LOCK");
#ifdef LOC_AGENT_SCREEN
 if(locShellMode){char id[20]="--",phase[32];FILE*j=fopen("ID.TXT","r");if(j){fgets(id,sizeof id,j);fclose(j);}id[strcspn(id,"\r\n")]=0;if(rc<0)strcpy(phase,"Failed to start");else sprintf(phase,"Returned (rc=%d)",rc);locJobDraw(phase,id,label,0);locRestoreCursor();}
#endif
 f=fopen("RESULT.TXT","w");if(f){fprintf(f,"%s\n%d\n",!strcmp(kind,"BAT")?"shell_exit_code":"program_exit_code",rc);fclose(f);}
 f=fopen("C:\\NET\\JOBS\\LAST.TXT","w");if(f){char id[20]="--";FILE *j=fopen("ID.TXT","r");if(j){fgets(id,sizeof(id),j);fclose(j);}fprintf(f,"%.8s\n%s (rc=%d)\n%.90s\n",id,rc<0?"FAILED TO START":"COMPLETED",rc,label);fclose(f);}
 state(rc<0?"failed-to-start\n":"completed\n");return rc;
}
int main(int argc,char **argv) {
 const char *server=argc>1?argv[1]:"C:\\NET\\FTPSRV.EXE";int rc;FILE *f;
#ifdef LOC_AGENT_SCREEN
 {const char*name=strrchr(server,'\\');locShellMode=!stricmp(name?name+1:server,"LOCSHELL.EXE");putenv(locShellMode?"LOC_AGENT_UI=1":"LOC_AGENT_UI=0");}
#endif
 if(_chdrive(3)||chdir(ROOT)){puts("Cannot open C:\\NET\\JOBS");return 1;}
 putenv("DOSAGENT_VERSION=2");
 /* Never replay work whose execution may already have begun. */
 if(!chdir("LOCK")){f=fopen("STATE.TXT","r");if(f){char s[40];fgets(s,sizeof(s),f);fclose(f);if(!strncmp(s,"running",7))state("interrupted\n");}chdir(ROOT);}
 for(;;){rc=spawnl(P_WAIT,server,server,NULL);if(rc) return 1;
  chdir(ROOT);if(chdir("LOCK"))return 0;
  if(access("READY",0)){chdir(ROOT);return 0;}
  if(rename("READY","CLAIMED")){return 1;}
  execute();_chdrive(3);chdir(ROOT);
 }
}
