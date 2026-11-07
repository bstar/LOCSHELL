#define DS_NETWORK_EMBEDDED
#include "LOCNET.H"
#include "LOCLAUNCH.H"
#include <process.h>
/* Included in the generated FTPSRV translation unit. GPL-3.0-or-later. */
static int shellWasEnabled=1,shellRestart=0,shellManualStop=0,shellReload=0,shellJobReady=0;
static struct stat shellImage;
static clockTicks_t shellImageCheck=0,shellReloadPause=0;
static void shellWatchImage(void){
 if(Timer_diff(shellImageCheck,TIMER_GET_CURRENT())<TIMER_MS_TO_TICKS(2000))return;
 shellImageCheck=TIMER_GET_CURRENT();struct stat now;
 if(!stat("C:\\DOSSHELL\\LOCSHELL.EXE",&now)&&(now.st_size!=shellImage.st_size||now.st_mtime!=shellImage.st_mtime))shellReload=1;
 if(access("C:\\DOSSHELL\\RELOAD.REQ",0)==0)shellReload=1;
}

static void shellContextWritten(const char*path){const char*ext=strrchr(path,'.'),*name=strrchr(path,'\\');name=name?name+1:path;if(!strnicmp(name,"CONFIG.",7)||(ext&&(!stricmp(ext,".EXE")||!stricmp(ext,".COM")||!stricmp(ext,".CFG")||!stricmp(ext,".INI")||!stricmp(ext,".SET")||!stricmp(ext,".SYS")||!stricmp(ext,".BAT"))))ds_shell_memory_changed();}
static void shellImageWritten(FtpClient*c){char path[USR_MAX_PATHFILE_LENGTH];if(!formFullPath(c,path,sizeof(path),c->filespec)){if(!stricmp(path,"C:\\DOSSHELL\\LOCSHELL.EXE"))shellReload=1;shellContextWritten(path);}}
static int shellListen(void){if(ListeningSocket)return 1;ListeningSocket=TcpSocketMgr::getSocket();if(!ListeningSocket)return 0;if(ListeningSocket->listen(FtpSrv_Control_Port,512)){TcpSocketMgr::freeSocket(ListeningSocket);ListeningSocket=NULL;return 0;}return 1;}
static void shellUnlisten(void){if(ListeningSocket){ListeningSocket->close();TcpSocketMgr::freeSocket(ListeningSocket);ListeningSocket=NULL;}}
static int shellSaveState(void){FILE*f=fopen("C:\\DOSSHELL\\NETSTATE.NEW","w");if(!f)return 0;int ok=fprintf(f,"%d\n",shellEnabled)>0;if(fclose(f))ok=0;if(!ok)return 0;remove("C:\\DOSSHELL\\NETSTATE.BAK");if(access("C:\\DOSSHELL\\NETSTATE.CFG",0)==0&&rename("C:\\DOSSHELL\\NETSTATE.CFG","C:\\DOSSHELL\\NETSTATE.BAK"))return 0;if(rename("C:\\DOSSHELL\\NETSTATE.NEW","C:\\DOSSHELL\\NETSTATE.CFG")){rename("C:\\DOSSHELL\\NETSTATE.BAK","C:\\DOSSHELL\\NETSTATE.CFG");return 0;}return 1;}

static unsigned shellAutoStart=1;
static char shellReason[80]="";
static int shellStarting=1;
static int shellTransfers(void){unsigned i,n=0;for(i=0;i<FtpClient::activeClients;i++){
 FtpClient*c=FtpClient::activeClientsTable[i];if(c->dataXferState!=FtpClient::DL_NotActive||c->file||c->pendingOutput())n++;
}return n;}
#ifdef LOC_SHELL_TEST
#include "SHELLTEST.H"
#endif
extern "C" int ds_net_poll(void){
#ifdef LOC_SHELL_TEST
 shellTestPoll();
#endif
 if(shellRestart)return DN_JOB;
 if(!shellOnline){static unsigned long previous=0;unsigned long now=daNow();int slept=daSleeping;
  if(now-previous>=36){previous=now;struct stat image;if(!stat("C:\\DOSSHELL\\LOCSHELL.EXE",&image)&&(image.st_size!=shellImage.st_size||image.st_mtime!=shellImage.st_mtime))shellReload=1;if(access("C:\\DOSSHELL\\RELOAD.REQ",0)==0)shellReload=1;}
  if(shellReload){daActivity();return DN_RELOAD;}
  daPoll(0);if(daSleeping){if(ds_shell_mouse_wake())daActivity();else if(biosIsKeyReady()){daHandleKey(biosKeyRead());}else return 2;}return slept&&!daSleeping?1:0;
 }
 int slept=daSleeping,rc;unsigned dtaSeg=0,dtaOff=0;
 union REGS rr;struct SREGS ss;
 if(FtpClient::activeClients){rr.h.ah=0x2f;int86x(0x21,&rr,&rr,&ss);dtaSeg=ss.es;dtaOff=rr.x.bx;}
 rc=shellService();
 if(dtaSeg){segread(&ss);ss.ds=dtaSeg;rr.h.ah=0x1a;rr.x.dx=dtaOff;int86x(0x21,&rr,&rr,&ss);}
 if(rc){shellJobReady=access("C:\\NET\\JOBS\\LOCK\\READY",0)==0;return DN_JOB;}
 shellWatchImage();
 if(shellDrain&&!shellTransfers()){
  for(unsigned i=0;i<FtpClient::activeClients;i++)if(FtpClient::activeClientsTable[i]->state!=FtpClient::Closed&&FtpClient::activeClientsTable[i]->state<FtpClient::ClosingPushOutput)endSession(FtpClient::activeClientsTable[i]);
 }

 if(shellReload&&!FtpClient::activeClients&&(!shellReloadPause||Timer_diff(shellReloadPause,TIMER_GET_CURRENT())>TIMER_MS_TO_TICKS(30000))){daActivity();return DN_RELOAD;}
 if(daSleeping){if(ds_shell_mouse_wake())daActivity();else if(biosIsKeyReady()){uint16_t k=biosKeyRead();daHandleKey(k);}else return 2;}

 return slept&&!daSleeping?1:0;
}
extern "C" void ds_net_activity(void){daActivity();}
extern "C" int ds_net_drain(int cancel){
 if(!shellOnline)return 1;
 if(!cancel&&access("C:\\NET\\JOBS\\LOCK",0)==0)return 0;
 if(cancel){if(shellReload)shellReloadPause=TIMER_GET_CURRENT();shellDrain=shellManualStop;shellEnabled=shellWasEnabled;if(shellEnabled)shellListen();return -1;}
 if(!shellDrain){shellWasEnabled=shellEnabled;shellDrain=1;shellUnlisten();}
 if(shellTransfers()||FtpClient::activeClients)return 0;
 return 1;
}
extern "C" void ds_net_restart_ready(void){shellRestart=1;}
#include "NETVIEWS.H"

int main(int argc,char **argv){
 int diagnose=argc>1&&!stricmp(argv[1],"/CHECKNET");
 if(argc>1&&!stricmp(argv[1],"/CHECK"))return ds_shell_main(argc,argv);
 if(!(argc>1&&!stricmp(argv[1],"/CHECKNET"))&&!getenv("DOSAGENT_VERSION")){
  const char*c=getenv("COMSPEC");spawnl(P_OVERLAY,c?c:"C:\\COMMAND.COM",c?c:"COMMAND.COM","/C","C:\\DOSSHELL\\LOCSHELL.BAT",NULL);puts("Start C:\\DOSSHELL\\LOCSHELL.BAT for launch and return.");return 2;
 }

 if(!diagnose)remove("C:\\DOSSHELL\\RELOAD.REQ");
 stat("C:\\DOSSHELL\\LOCSHELL.EXE",&shellImage);
 daInit();Screen_base=(uint8_t far*)MK_FP(0xb800,0);ScreenCols=80;ScreenRows=25;
 if(!(argc>1&&!stricmp(argv[1],"/CHECKNET")))ds_shell_boot("Starting FTP service...");
 if(!getenv("DOSAGENT_IDLE")){FILE*f=fopen("C:\\NET\\AGENT\\IDLE.CFG","r");unsigned n;if(f){if(fscanf(f,"%u",&n)==1&&n<=3600){daTimeout=n;daTimeoutTicks=daTimeout*182UL/10UL;}fclose(f);}}
 int stackReady=0;
 if(!initSrv()){stackReady=1;shellOnline=shellListen();}
 shellStarting=0;
 if(!shellOnline)strcpy(shellReason,"Network initialization failed. Check packet driver and mTCP configuration.");
 lastTimeoutSweep=lastKeyboardCheck=lastDnsCheck=TIMER_GET_CURRENT();
 {FILE*f=fopen("C:\\DOSSHELL\\AUTONET.CFG","r");unsigned on=1;if(f){fscanf(f,"%u",&on);fclose(f);}shellAutoStart=on!=0;int enabled=shellAutoStart;
  if(access("C:\\DOSSHELL\\RESUME.REQ",0)==0){f=fopen("C:\\DOSSHELL\\NETSTATE.CFG","r");if(f){fscanf(f,"%d",&enabled);fclose(f);}if(!diagnose)remove("C:\\DOSSHELL\\RESUME.REQ");}
  if(!enabled){shellEnabled=0;shellManualStop=shellDrain=1;shellUnlisten();}}

 {FILE*f=fopen("C:\\NET\\AGENT\\BEEP.CFG","r");unsigned n;if(f){if(fscanf(f,"%u",&n)==1)Sound=n!=0;fclose(f);}}
 shellVisible=1;
 _chdrive(3);chdir("C:\\DOSSHELL");
 int managed=getenv("DOSAGENT_VERSION")!=NULL;
 char*shellArgs[]={"LOCSHELL.EXE",managed?"/FROMBATCH":"/CHECK",NULL};
 int shellArgc=managed?2:1;
 if(diagnose){shellArgs[1]="/CHECK";shellArgc=2;printf("NETWORK online=%d enabled=%d\n",shellOnline,shellEnabled);}
#ifdef LOC_SHELL_TEST
 {FILE*f=fopen("C:\\DOSSHELL\\TRACE.TXT","w");if(f){fprintf(f,"before shell heap=%lu\n",(unsigned long)_memavl());fclose(f);}}
#endif
#ifdef LOC_SHELL_TEST
 {unsigned count=0;FILE*f=fopen("C:\\DOSSHELL\\STARTS.TXT","r");if(f){fscanf(f,"%u",&count);fclose(f);}f=fopen("C:\\DOSSHELL\\STARTS.TXT","w");if(f){fprintf(f,"%u\n",count+1);fclose(f);}}
#endif
 int rc=ds_shell_main(shellArgc,shellArgs);
#ifdef LOC_SHELL_TEST
 {FILE*f=fopen("C:\\DOSSHELL\\TRACE.TXT","a");if(f){fprintf(f,"shell returned %d\n",rc);fclose(f);}}
#endif
 if(!diagnose&&shellSaveState()&&shellJobReady){FILE*f=fopen("C:\\DOSSHELL\\RESUME.REQ","w");if(f)fclose(f);}
 daActivity();shellVisible=0;
 if(stackReady){for(unsigned i=0;i<FtpClient::activeClients;i++)endSession(FtpClient::activeClientsTable[i]);Utils::endStack();}
 if(LogFile)fclose(LogFile);
 /* The supervisor exits before START.BAT executes this request. */
 if(shellRestart)remove("C:\\DOSSHELL\\RUN.BAT");
 if(managed&&(rc==1||shellRestart)&&!lp_publish(shellRestart))return 2;

 return rc==2?2:0;
}
