/* Typed UI snapshots; no console drawing or input ownership. */
static DNWork netLastTransfer,netManagedWork;
static unsigned long netTransferWhen=0,netManagedCheck=0;
static unsigned long netUploaded=0,netDownloaded=0,netFilesIn=0,netFilesOut=0,netFailed=0,netStarted=0;
static DNFileEvent far netFiles[8];static unsigned netFileHead=0,netFileCount=0;
static void shellFileChanged(const char*action,const char*path,unsigned long bytes,int ok){DNFileEvent*e=&netFiles[netFileHead];memset(e,0,sizeof *e);e->ok=ok;e->bytes=bytes;e->stamp=(unsigned long)time(NULL);strncpy(e->action,action,sizeof e->action-1);strncpy(e->file,path,sizeof e->file-1);netFileHead=(netFileHead+1)%8;if(netFileCount<8)netFileCount++;}
extern "C" int ds_net_file_event(unsigned i,DNFileEvent*e){if(i>=netFileCount)return 0;*e=netFiles[(netFileHead+7-i)%8];return 1;}
static void netReadLine(const char*path,char*out,unsigned cap){if(!cap)return;FILE*f=fopen(path,"r");out[0]=0;if(f){if(fgets(out,cap,f))out[strcspn(out,"\r\n")]=0;fclose(f);}}
static void netTransferWork(FtpClient*c,DNWork*w){memset(w,0,sizeof *w);w->transfer=1;w->bytes=c->observedBytes;w->total=c->observedTotal;
 const char*verb=c->dataXferType==FtpClient::Retr?"Send":c->dataXferType>=FtpClient::Stor?"Receive":"List";
 strncpy(w->detail,verb,sizeof(w->detail)-1);strncpy(w->name,*c->filespec?c->filespec:"Directory",sizeof(w->name)-1);
 w->state=c->dataXferState<FtpClient::DL_Active||c->dataXferState==FtpClient::DL_Closing?DN_WORK_WAITING:DN_WORK_ACTIVE;
}
static void netTransferFinished(FtpClient*c,int ok){if(c->dataXferType<FtpClient::Retr)return;netTransferWork(c,&netLastTransfer);netLastTransfer.state=ok?DN_WORK_DONE:DN_WORK_FAILED;netTransferWhen=daNow();char path[USR_MAX_PATHFILE_LENGTH];if(formFullPath(c,path,sizeof path,c->filespec))strncpy(path,c->filespec,sizeof path-1);
 if(c->dataXferType==FtpClient::Retr){netDownloaded+=c->observedBytes;if(ok)netFilesOut++;shellFileChanged(ok?"Sent":"Send",path,c->observedBytes,ok);}
 else {netUploaded+=c->observedBytes;if(ok)netFilesIn++;shellFileChanged(ok?"Received":"Receive",path,c->observedBytes,ok);}
 if(!ok)netFailed++;
}
static void shellTransferFinished(FtpClient*c){netTransferFinished(c,1);}
static void shellTransferFailed(FtpClient*c){netTransferFinished(c,0);}
static void netManagedPoll(void){unsigned long now=daNow();if(netManagedCheck&&now-netManagedCheck<18UL)return;netManagedCheck=now;memset(&netManagedWork,0,sizeof netManagedWork);
 union REGS rr;struct SREGS sr;segread(&sr);rr.h.ah=0x2f;int86x(0x21,&rr,&rr,&sr);unsigned dtaSeg=sr.es,dtaOff=rr.x.bx;
 char state[40]="";netReadLine("C:\\LOCSHELL\\NET\\JOBS\\LOCK\\STATE.TXT",state,sizeof state);
 int ready=access("C:\\LOCSHELL\\NET\\JOBS\\LOCK\\READY",0)==0;
 if(ready||!stricmp(state,"interrupted")||!stricmp(state,"failed-to-start")){
  netManagedWork.state=ready?DN_WORK_WAITING:DN_WORK_FAILED;
  strcpy(netManagedWork.detail,ready?"Queued":!stricmp(state,"interrupted")?"Interrupted":"Failed to start");
  netReadLine("C:\\LOCSHELL\\NET\\JOBS\\LOCK\\ID.TXT",netManagedWork.id,sizeof netManagedWork.id);
  netReadLine("C:\\LOCSHELL\\NET\\JOBS\\LOCK\\LABEL.TXT",netManagedWork.name,sizeof netManagedWork.name);
  if(!*netManagedWork.name){FILE*f=fopen("C:\\LOCSHELL\\NET\\JOBS\\LOCK\\REQUEST.TXT","r");char skip[128];if(f){fgets(skip,sizeof skip,f);fgets(skip,sizeof skip,f);if(fgets(netManagedWork.name,sizeof netManagedWork.name,f))netManagedWork.name[strcspn(netManagedWork.name,"\r\n")]=0;fclose(f);}}
  if(!*netManagedWork.name)strcpy(netManagedWork.name,"Remote management job");
 }else{
  struct stat st;time_t nowTime=time(NULL);
  if(!stat("C:\\LOCSHELL\\NET\\JOBS\\LAST.TXT",&st)&&nowTime>=st.st_mtime&&nowTime-st.st_mtime<=20){
   FILE*f=fopen("C:\\LOCSHELL\\NET\\JOBS\\LAST.TXT","r");if(f){fgets(netManagedWork.id,sizeof netManagedWork.id,f);fgets(netManagedWork.detail,sizeof netManagedWork.detail,f);fgets(netManagedWork.name,sizeof netManagedWork.name,f);fclose(f);
    netManagedWork.id[strcspn(netManagedWork.id,"\r\n")]=0;netManagedWork.detail[strcspn(netManagedWork.detail,"\r\n")]=0;netManagedWork.name[strcspn(netManagedWork.name,"\r\n")]=0;
    netManagedWork.state=!strnicmp(netManagedWork.detail,"FAILED",6)?DN_WORK_FAILED:DN_WORK_DONE;
   }
  }
 }
 segread(&sr);sr.ds=dtaSeg;rr.h.ah=0x1a;rr.x.dx=dtaOff;int86x(0x21,&rr,&rr,&sr);
}
extern "C" void ds_net_work(DNWork*w){memset(w,0,sizeof *w);if(shellStarting){w->state=DN_WORK_WAITING;strcpy(w->name,"FTP service");strcpy(w->detail,"Starting");return;}netManagedPoll();
 for(unsigned i=0;i<FtpClient::activeClients;i++){FtpClient*c=FtpClient::activeClientsTable[i];if(c->dataXferState!=FtpClient::DL_NotActive&&!(c->dataXferState==FtpClient::DL_Closing&&c->noMoreData)){netTransferWhen=0;netTransferWork(c,w);return;}}
 if(netManagedWork.state==DN_WORK_WAITING||netManagedWork.state==DN_WORK_FAILED){*w=netManagedWork;return;}
 if(shellReload){w->state=DN_WORK_WAITING;strcpy(w->name,"Reload LOC SHELL");strcpy(w->detail,"Waiting for clients / edits");return;}
 if(netTransferWhen&&daNow()-netTransferWhen<90UL){*w=netLastTransfer;return;}
 *w=netManagedWork;
}
static char far netEvents[32][96];static unsigned netEventHead=0,netEventCount=0;
static void shellLogEvent(const char*line){unsigned i=0;char *out=netEvents[netEventHead];while(*line&&i<95){char c=*line++;if(c=='\r'||c=='\n'){if(i&&out[i-1]!=' ')out[i++]=' ';}else if((unsigned char)c>=32)out[i++]=c;}out[i]=0;netEventHead=(netEventHead+1)%32;if(netEventCount<32)netEventCount++;}
extern "C" void ds_net_snapshot(DNSnapshot*s){memset(s,0,sizeof *s);s->state=shellStarting?DN_STARTING:!shellOnline?DN_UNAVAILABLE:(shellDrain||shuttingDown||ShutdownRequested>1)?((shellTransfers()||FtpClient::activeClients)?DN_STOPPING:DN_STOPPED):(shellEnabled&&ListeningSocket)?DN_RUNNING:DN_STOPPED;s->clients=FtpClient::activeClients;s->port=FtpSrv_Control_Port;s->saver=daSaver;s->idle=(unsigned)daTimeout;s->beeper=Sound;s->autostart=shellAutoStart;s->reload=shellReload;memcpy(s->ip,MyIpAddr,4);s->sessions=SessionCounter;s->timeouts=Stat_SessionTimeouts;s->sent=Tcp::Packets_Sent;s->received=Tcp::Packets_Received;s->retransmits=Tcp::Packets_Retransmitted;s->uploaded=netUploaded;s->downloaded=netDownloaded;s->files_in=netFilesIn;s->files_out=netFilesOut;s->failed=netFailed;if(!netStarted)netStarted=daNow();s->uptime=daElapsed(daNow(),netStarted)/18UL;strcpy(s->reason,shellReason);
 for(unsigned i=0;i<FtpClient::activeClients;i++){FtpClient*c=FtpClient::activeClientsTable[i];if(c->dataXferState!=FtpClient::DL_NotActive)s->transfers++;if(c->dataXferState==FtpClient::DL_Active){if(c->dataXferType==FtpClient::Retr)s->downloaded+=c->observedBytes;else if(c->dataXferType>=FtpClient::Stor)s->uploaded+=c->observedBytes;}if(c->state>=FtpClient::ClosingPushOutput)s->closing++;}
}
extern "C" int ds_net_client(unsigned index,DNClient*out){if(index>=FtpClient::activeClients)return 0;FtpClient*c=FtpClient::activeClientsTable[index];memset(out,0,sizeof *out);out->id=c->sessionId;strncpy(out->user,c->user.userName,19);strncpy(out->file,c->filespec,79);sprintf(out->address,"%u.%u.%u.%u",c->cs->dstHost[0],c->cs->dstHost[1],c->cs->dstHost[2],c->cs->dstHost[3]);out->seconds=Timer_diff(c->sessionTicks,TIMER_GET_CURRENT())/18UL;out->state=c->state;out->transferring=c->dataXferState!=FtpClient::DL_NotActive;out->direction=!out->transferring?DN_XFER_NONE:c->dataXferType==FtpClient::Retr?DN_XFER_SEND:c->dataXferType>=FtpClient::Stor?DN_XFER_RECEIVE:DN_XFER_LIST;out->bytes=c->observedBytes;out->total=c->observedTotal;
 const char*state=c->state>=FtpClient::ClosingPushOutput?"Closing":c->state<FtpClient::CommandLine?"Signing in":!out->transferring?"Idle":c->dataXferState==FtpClient::DL_Closing?"Finishing":c->dataXferState<FtpClient::DL_Active?"Connecting":c->dataXferType==FtpClient::Retr?"Sending file":c->dataXferType>=FtpClient::Stor?"Receiving file":"Directory listing";strcpy(out->status,state);return 1;}
extern "C" int ds_net_log(unsigned index,char*out,unsigned cap){if(index>=netEventCount||!cap)return 0;unsigned n=(netEventHead+31-index)%32;strncpy(out,netEvents[n],cap-1);out[cap-1]=0;return 1;}
static int netWrite(const char*path,unsigned value){char tmp[80],bak[80];strcpy(tmp,path);strcpy(strrchr(tmp,'.'),".NEW");strcpy(bak,path);strcpy(strrchr(bak,'.'),".BAK");FILE*f=fopen(tmp,"w");if(!f)return 0;int ok=fprintf(f,"%u\n",value)>0;if(fclose(f))ok=0;if(!ok)return 0;remove(bak);int exists=access(path,0)==0;if(exists&&rename(path,bak))return 0;if(rename(tmp,path)){if(exists)rename(bak,path);return 0;}return 1;}
extern "C" DNResult ds_net_command(DNAction action,unsigned value,char*message,unsigned cap){const char*error="Could not save the setting.";if(cap)message[0]=0;
 if(action==DN_START){if(shuttingDown||ShutdownRequested>1){error="A management handoff is already in progress.";goto failed;}if(!shellOnline){shellRestart=1;error="Retrying network initialization.";goto pending;}if(!shellListen()){error="No listening socket is available.";goto failed;}shellEnabled=1;shellManualStop=shellDrain=0;return DN_OK;}
 if(action==DN_STOP){shellEnabled=0;shellManualStop=shellDrain=1;shellUnlisten();error="Stopping after transfers and connections finish.";goto pending;}
 if(action==DN_RESTART){shellReload=1;error="Restart queued; finish edits and disconnect FTP clients.";goto pending;}
 if(action==DN_AUTOSTART){if(!netWrite("C:\\LOCSHELL\\AUTONET.CFG",value!=0))goto failed;shellAutoStart=value!=0;return DN_OK;}
 if(action==DN_BEEP){if(!netWrite("C:\\LOCSHELL\\NET\\AGENT\\BEEP.CFG",value!=0))goto failed;Sound=value!=0;return DN_OK;}
 if(action==DN_IDLE){if(value>3600){error="Idle timeout must be 0 to 3600 seconds.";goto failed;}if(!netWrite("C:\\LOCSHELL\\NET\\AGENT\\IDLE.CFG",value))goto failed;daActivity();daTimeout=value;daTimeoutTicks=daTimeout*182UL/10UL;return DN_OK;}
 if(action==DN_SAVER){if(value>=DA_SAVER_COUNT){error="Unknown screensaver.";goto failed;}daActivity();FILE*f=fopen("C:\\LOCSHELL\\NET\\AGENT\\SAVER.NEW","w");if(!f)goto failed;int ok=fprintf(f,"%s\n",daSaverKeys[value])>0;if(fclose(f))ok=0;if(!ok)goto failed;remove("C:\\LOCSHELL\\NET\\AGENT\\SAVER.BAK");int exists=access(DA_SAVER_CONFIG,0)==0;if(exists&&rename(DA_SAVER_CONFIG,"C:\\LOCSHELL\\NET\\AGENT\\SAVER.BAK"))goto failed;if(rename("C:\\LOCSHELL\\NET\\AGENT\\SAVER.NEW",DA_SAVER_CONFIG)){if(exists)rename("C:\\LOCSHELL\\NET\\AGENT\\SAVER.BAK",DA_SAVER_CONFIG);goto failed;}daSaver=value;return DN_OK;}
 if(action==DN_PREVIEW){if(!daTimeout||value>=DA_SAVER_COUNT||value==DA_SAVER_OFF){error="Enable a screensaver and a nonzero idle timeout first.";goto failed;}daPreviewChoice=(int)value;daPreviewPending=1;error="Preview queued until connected clients disconnect.";goto pending;}
 if(action==DN_DISCONNECT){if(value>=FtpClient::activeClients){error="The connection has already closed.";goto failed;}FtpClient*c=FtpClient::activeClientsTable[value];if(c->dataXferState!=FtpClient::DL_NotActive){error="Wait for this transfer to finish before disconnecting.";goto failed;}endSession(c);return DN_PENDING;}
 error="Unsupported network action.";
failed:if(cap){strncpy(message,error,cap-1);message[cap-1]=0;}return DN_ERROR;
pending:if(cap){strncpy(message,error,cap-1);message[cap-1]=0;}return DN_PENDING;
}
