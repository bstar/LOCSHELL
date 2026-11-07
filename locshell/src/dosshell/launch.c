#include "launch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <io.h>
#include <time.h>
unsigned lp_recent_load(LPRecent*out,unsigned cap){FILE*f;char line[80],kind;unsigned count=0,read=0;unsigned long id,stamp;
 f=fopen("C:\\DOSSHELL\\RECENT.TXT","r");if(!f)return 0;
 if(!fgets(line,sizeof line,f)||strcmp(line,"LOC_RECENT 1\n")){fclose(f);return 0;}
 while(count<cap&&read++<32&&fgets(line,sizeof line,f))if(sscanf(line,"%c %lx %lx",&kind,&id,&stamp)==3&&(kind=='A'||kind=='T')&&id){unsigned i;for(i=0;i<count;i++)if(out[i].kind==kind&&out[i].id==id)break;if(i==count){out[count].kind=kind;out[count].id=id;out[count++].stamp=stamp;}}
 fclose(f);return count;
}
int lp_recent_add(char kind,unsigned long id,unsigned long stamp){LPRecent rows[LP_RECENT_MAX];unsigned count,i,written=1;FILE*f;int ok,exists;
 if((kind!='A'&&kind!='T')||!id)return 0;
 count=lp_recent_load(rows,LP_RECENT_MAX);f=fopen("C:\\DOSSHELL\\RECENT.NEW","w");if(!f)return 0;
 ok=fprintf(f,"LOC_RECENT 1\n%c %08lx %08lx\n",kind,id,stamp)>0;
 for(i=0;ok&&i<count&&written<LP_RECENT_MAX;i++)if(rows[i].kind!=kind||rows[i].id!=id){ok=fprintf(f,"%c %08lx %08lx\n",rows[i].kind,rows[i].id,rows[i].stamp)>0;written++;}
 if(fclose(f))ok=0;if(!ok)return 0;
 remove("C:\\DOSSHELL\\RECENT.BAK");exists=access("C:\\DOSSHELL\\RECENT.TXT",0)==0;
 if(exists&&rename("C:\\DOSSHELL\\RECENT.TXT","C:\\DOSSHELL\\RECENT.BAK"))return 0;
 if(rename("C:\\DOSSHELL\\RECENT.NEW","C:\\DOSSHELL\\RECENT.TXT")){if(exists)rename("C:\\DOSSHELL\\RECENT.BAK","C:\\DOSSHELL\\RECENT.TXT");return 0;}return 1;
}
static unsigned long script_hash(void){FILE*f=fopen("C:\\DOSSHELL\\RUN.BAT","rb");int c;unsigned long h=2166136261UL;if(!f)return 0;while((c=fgetc(f))!=EOF)h=(h^(unsigned char)c)*16777619UL;fclose(f);return h;}
int lp_prepare(const char*kind,unsigned long id){FILE*f;unsigned long h=script_hash();int ok;if(!h)return 0;f=fopen("C:\\DOSSHELL\\LAUNCH.NEW","w");if(!f)return 0;ok=fprintf(f,"LOC3 %s %08lx %08lx\n",kind,id,h)>0;if(fclose(f))ok=0;return ok;}
int lp_publish(int restart){FILE*in,*out;char line[100];int ok;if(access("C:\\NET\\AGENT\\LOCAL.REQ",0)==0)return 0;
 if(restart)strcpy(line,"LOC3 RESTART 00000000 00000000\n");else{in=fopen("C:\\DOSSHELL\\LAUNCH.NEW","r");if(!in)return 0;ok=fgets(line,sizeof line,in)!=0;fclose(in);if(!ok)return 0;}
 out=fopen("C:\\NET\\AGENT\\LOCAL.NEW","w");if(!out)return 0;ok=fputs(line,out)>=0;if(fclose(out))ok=0;if(!ok)return 0;return rename("C:\\NET\\AGENT\\LOCAL.NEW","C:\\NET\\AGENT\\LOCAL.REQ")==0;
}
#ifdef LOC_HANDOFF_MAIN
#include <sys/stat.h>
#define REQ "C:\\NET\\AGENT\\LOCAL.REQ"
#define ACT "C:\\NET\\AGENT\\LOCAL.ACT"
static void report(const char*phase,const char*message,int ask){FILE*f=fopen("C:\\DOSSHELL\\LAUNCH.RES","w");if(f){fprintf(f,"%s\n%s\n",phase,message);fclose(f);}f=fopen("C:\\DOSSHELL\\LAUNCH.LOG","a");if(f){fprintf(f,"%s | %s\n",phase,message);fclose(f);}if(ask){f=fopen("C:\\DOSSHELL\\LAUNCH.ASK","w");if(f)fclose(f);}}
static int preflight(void){FILE*f=fopen("C:\\DOSSHELL\\RUN.BAT","r");char line[256],cwd[128]="\\DOSSHELL",drive='C',path[260],token[128],*p;unsigned n;struct stat st;if(!f)return 0;
 while(fgets(line,sizeof line,f)){line[strcspn(line,"\r\n")]=0;p=line;while(*p==' '||*p=='@')p++;if(!*p||!strnicmp(p,"ECHO",4)||!strnicmp(p,"REM ",4)||!strnicmp(p,"SET ",4))continue;
  if(strlen(p)==2&&p[1]==':'){drive=p[0];continue;}if(!strnicmp(p,"CD ",3)){p+=3;while(*p==' ')p++;if(*p!='\\'||strlen(p)>=sizeof cwd)continue;strcpy(cwd,p);sprintf(path,"%c:%s",drive,cwd);if(stat(path,&st)||!(st.st_mode&S_IFDIR)){fclose(f);report("Cannot launch: working directory missing",path,1);return 0;}continue;}
  if(!strnicmp(p,"CALL ",5))p+=5;n=0;while(p[n]&&p[n]!=' '&&n<sizeof token-1){token[n]=p[n];n++;}token[n]=0;
  if(strpbrk(token,"<>|%&")||!strrchr(token,'.'))continue;
  if(stricmp(strrchr(token,'.'),".COM")&&stricmp(strrchr(token,'.'),".EXE")&&stricmp(strrchr(token,'.'),".BAT"))continue;
  if(token[1]==':')strcpy(path,token);else sprintf(path,"%c:%s\\%s",drive,cwd,token);
  if(stat(path,&st)){fclose(f);report("Cannot launch: program missing",path,1);return 0;}
 }
 fclose(f);return 1;}
int main(int argc,char**argv){FILE*f;char magic[8],kind[12];unsigned long id,h;int valid;
 if(argc<2)return 4;
 if(!strcmp(argv[1],"/RECOVER")){const char*paths[]={REQ,ACT};unsigned i;for(i=0;i<2;i++)if(access(paths[i],0)==0){char bad[80];strcpy(bad,paths[i]);strcpy(strrchr(bad,'.'),i?".OLD":".ERR");if(access(bad,0)==0||rename(paths[i],bad)){puts("Interrupted launch retained; review LOCAL.REQ/ACT and archived evidence.");return 4;}if(access("C:\\DOSSHELL\\RUN.BAT",0)==0){if(access("C:\\DOSSHELL\\RUN.ERR",0)==0||rename("C:\\DOSSHELL\\RUN.BAT","C:\\DOSSHELL\\RUN.ERR"))return 4;}report("Interrupted launch retained","Previous request was not replayed. Review launch history before retrying.",1);}return 0;}
 if(!strcmp(argv[1],"/CLAIM")){f=fopen(REQ,"r");if(!f)return 0;valid=fscanf(f,"%7s %11s %lx %lx",magic,kind,&id,&h)==4;fclose(f);if(!valid||strcmp(magic,"LOC3")||access(ACT,0)==0){report("Launch handoff rejected","Invalid or already-active request; no commands were executed.",1);return 4;}if(rename(REQ,ACT))return 4;
  if(!strcmp(kind,"RESTART")){report("Restarting LOC SHELL","Settings saved; restarting the foreground service.",0);return 3;}
  if(strcmp(kind,"RUN")&&strcmp(kind,"LEARN")&&strcmp(kind,"TOOL")&&strcmp(kind,"SETUP")&&strcmp(kind,"FILE")){report("Launch handoff rejected","Unknown request type.",1);remove(ACT);return 4;}
  if(h!=script_hash()){report("Launch handoff rejected","The prepared commands changed. Select the program again.",1);remove(ACT);return 4;}
  if(!preflight()){remove(ACT);if(!strcmp(kind,"LEARN"))remove("C:\\DOSSHELL\\LEARN.REQ");return 4;}report("Launch ready","Supervisor returned; command script validated.",0);return !strcmp(kind,"LEARN")?2:1;
 }
 if(!strcmp(argv[1],"/ENTER")){f=fopen(ACT,"r");if(f){valid=fscanf(f,"%7s %11s %lx %lx",magic,kind,&id,&h)==4;fclose(f);if(valid&&!strcmp(magic,"LOC3")&&h==script_hash()){if(!strcmp(kind,"RUN")||!strcmp(kind,"LEARN"))lp_recent_add('A',id,(unsigned long)time(NULL));else if(!strcmp(kind,"TOOL"))lp_recent_add('T',id,(unsigned long)time(NULL));}}
  report("Starting program","Network and launcher have exited. Running the selected command script.",0);return 0;}
 if(!strcmp(argv[1],"/RETURN")){int nonzero=argc>2&&atoi(argv[2]);report(nonzero?"Program returned a nonzero status":"Program returned",nonzero?"The command script returned a nonzero status. It was not automatically retried.":"Returning to LOC SHELL. This does not verify gameplay or music.",nonzero);return 0;}
 if(!strcmp(argv[1],"/RESUME")){remove(ACT);f=fopen("C:\\DOSSHELL\\RESUME.REQ","w");if(!f)return 4;fclose(f);return 0;}
 if(!strcmp(argv[1],"/FAIL")){report("Launcher stopped","The supervisor or memory observer could not start. No automatic retry was made.",1);return 4;}
 return 4;
}
#endif
