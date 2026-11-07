static const char *view_names[]={"Games","FTP server","Clients","Transfers","Activity","Screensavers","Tools","Dashboard"};
static int view_sel[8],view_top[8];
static int saver_selection_loaded;
static const char*saver_names[]={"Starfield","387 Sculpture","387 Mandelbrot","Island","Off"};
static void view_footer(const char *text) {
 put(26,1,54,A_PANEL,nav_focus>=0?"Up/Down Choose Enter Open Tab/Esc Content":text);
}
static void view_nav(void) {
 int i;char label[24];
 fill(0,2,18,21,A_NORMAL);
 for(i=2;i<23;i++)cell(18,i,A_BORDER,179);
 put(1,2,16,nav_focus==14?A_SELECT:A_NORMAL," Home / Dashboard");
 put(2,11,15,A_NORMAL,"BROWSE GAMES");
 for(i=0;i<14;i++) {
  int active=i<7?shell_view==i:(!shell_view&&grouping==i-8);
  unsigned a=nav_focus==i?A_SELECT:A_NORMAL;
  sprintf(label,"%c%s",active?16:' ',i<7?view_names[i]:browse_names[i-7]);
  put(1,i<7?3+i:12+i-7,16,a,label);
 }
}
static void shell_frame(int inactive) {
 int i;
 fill(0,0,80,25,inactive?A_IDLE:A_NORMAL);
 fill(0,0,80,2,A_PANEL);
 put(2,0,LOC_TITLE_WIDTH,A_PANEL,loc_shell_title());
 put(76,0,3,A_PANEL,"[X]");
 put(1,1,78,A_PANEL,"File   View  Edit  Help");
 if(shell_view!=7){for(i=19;i<80;i++)cell(i,3,A_BORDER,196);view_nav();}
 status_draw();
}
static void view_open(int index) {
 nav_focus=-1;mouse_row=-1;
 if(index==14)shell_view=7;
 else if(index<7)shell_view=index;
 else {shell_view=0;choose_group(index-8);}
 main_valid=0;
}
static int view_click(void) {
 if(shell_view==7)return 0;
 if(mx<0||mx>=18||my<2||my>=23)return 0;
 if(my==2){view_open(14);return 1;}
 if(my>=3&&my<=9){view_open(my-3);return 1;}
 if(my>=12&&my<=18){view_open(my-12+7);return 1;}
 return 1; /* Sidebar padding never falls through to content actions. */
}
static int view_input(int k) {
 if(k==9||k==K_SHIFT_TAB){nav_focus=nav_focus<0?shell_view:-1;main_valid=0;return 1;}
 if(nav_focus<0) {
  if(k==K_LEFT){nav_focus=shell_view;main_valid=0;return 1;}
  return 0;
 }
 if(k==K_UP||k==K_DOWN){nav_focus=(nav_focus+(k==K_DOWN?1:14))%15;main_valid=0;return 1;}
 if(k==K_HOME){nav_focus=0;main_valid=0;return 1;}
 if(k==K_END){nav_focus=14;main_valid=0;return 1;}
 if(k==13||k==K_RIGHT){view_open(nav_focus);return 1;}
 if(k==27){nav_focus=-1;main_valid=0;return 1;}
 return 0;
}
static void network_action(DNAction action,unsigned value){char msg[100];DNResult r=ds_net_command(action,value,msg,sizeof msg);main_valid=0;if(r==DN_ERROR)info(msg);else if(*msg)strncpy(notice,msg,sizeof notice-1);}
static void network_idle_edit(void){DNSnapshot s;char text[6];unsigned pos=0;int k;ds_net_snapshot(&s);sprintf(text,"%u",s.idle);for(;;){main_draw(1);window(8,7,64,10,"Screensaver timeout");put(11,9,58,A_PANEL,"Seconds before starting (0 disables, maximum 3600)");input_draw(11,11,8,1,text,pos,1);put(11,14,58,A_PANEL,"Enter Save   Esc Cancel");k=event();if(k==27)return;if(k==13){char*end;unsigned long n=strtoul(text,&end,10);if(!*text||*end||n>3600){info("Enter 0 to 3600 seconds.");continue;}network_action(DN_IDLE,(unsigned)n);return;}if(k>=0)input_key(text,sizeof text,&pos,k,0);}}
static void network_view_draw(int inactive) {
 DNSnapshot s;DNClient c;int i,row=0,n=0;char line[160];
 unsigned a=inactive?A_IDLE:A_NORMAL;
 static const char *states[]={"Unavailable","Starting","Running","Stopping","Stopped"};
 ds_net_snapshot(&s);shell_frame(inactive);
 if(shell_view==5&&!saver_selection_loaded){view_sel[5]=s.saver<5?s.saver:0;saver_selection_loaded=1;}
 put(LIST_X,2,CONTENT_W,a,view_names[shell_view]);

 if(shell_view==1) {
  sprintf(line,"FTP server: %s",states[s.state<=DN_STOPPED?s.state:DN_UNAVAILABLE]);put(LIST_X,4,CONTENT_W,a,line);
  if(s.state==DN_UNAVAILABLE)sprintf(line,"Address unavailable       Port %u",s.port);
  else sprintf(line,"%u.%u.%u.%u      Port %u",s.ip[0],s.ip[1],s.ip[2],s.ip[3],s.port);
  put(LIST_X,6,CONTENT_W,a,line);
  sprintf(line,"Clients %u   Transfers %u   Closing %u",s.clients,s.transfers,s.closing);put(LIST_X,8,CONTENT_W,a,line);
  sprintf(line,"Sessions %lu   Timeouts %lu",s.sessions,s.timeouts);put(LIST_X,9,CONTENT_W,a,line);
  sprintf(line,"[%c] Start FTP with LOC SHELL",s.autostart?'X':' ');put(LIST_X,10,CONTENT_W,a,line);
  sprintf(line,"[%c] Connection beeper",s.beeper?'X':' ');put(LIST_X,11,CONTENT_W,a,line);
  put(LIST_X,12,CONTENT_W,a,s.reload?"Update queued; waiting for clients / open edits.":s.reason);
  button(23,15,15,"Start",0);button(43,15,15,"Stop",0);button(63,15,15,"Restart",0);
  sprintf(line,"RX %lu  TX %lu  Retry %lu",s.received,s.sent,s.retransmits);put(LIST_X,18,CONTENT_W,a,line);
  view_footer("Tab Menu S Start X Stop R Restart A Startup B Beep");
 } else if(shell_view==2||shell_view==3) {
  put(LIST_X,4,CONTENT_W,a,shell_view==2?"User / address                      State":"File                Bytes / total          State");
  for(i=0;ds_net_client(i,&c);i++) {
   if(shell_view==3&&!c.transferring)continue;
   if(n++<view_top[shell_view])continue;
   if(row>=17)continue;
   if(shell_view==2)sprintf(line,"%-10.10s %-21.21s %-26.26s",*c.user?c.user:"(login)",c.address,c.status);
   else if(c.total)sprintf(line,"%-24.24s %10lu/%-10lu %.11s",c.file,c.bytes,c.total,c.status);
   else sprintf(line,"%-24.24s %10lu bytes  %.16s",c.file,c.bytes,c.status);
   put(LIST_X,6+row,CONTENT_W,n-1==view_sel[shell_view]?A_SELECT:a,line);row++;
  }
  if(!n)put(LIST_X,8,CONTENT_W,a,shell_view==2?"No connected clients.":"No file transfers in progress.");
  view_footer("Tab Menu Arrows Select Enter Details D Disconnect");
 } else if(shell_view==4) {
  for(i=0;i<19;i++)if(ds_net_log(view_top[4]+i,line,sizeof line)){put(LIST_X,4+i,CONTENT_W,a,line);n++;}
  if(!n)put(LIST_X,7,CONTENT_W,a,"No server activity recorded in this session.");
  view_footer("Tab Menu Arrows Scroll PgUp/PgDn Page");
 } else if(shell_view==5) {
  for(i=0;i<5;i++){sprintf(line,"%c %s",i==(int)s.saver?'*':' ',saver_names[i]);put(LIST_X,5+i,CONTENT_W,i==view_sel[5]?A_SELECT:a,line);}
  sprintf(line,"Idle timeout: %u seconds%s",s.idle,s.idle?"":" (disabled)");put(LIST_X,12,CONTENT_W,a,line);
  button(23,15,22,"Preview",0);button(51,15,26,"Set timeout",0);
  view_footer("Tab Menu Enter Apply P Preview I Timeout");
 } else if(shell_view==6) {
  for(i=view_top[6];i<14&&row<19;i++,row++)put(LIST_X,4+row,CONTENT_W,i==view_sel[6]?A_SELECT:a,tool_names[i]);
  view_footer("Tab Menu Arrows Select Enter Run H Help");
 }
 if(*notice)put(LIST_X,22,CONTENT_W,a,notice);
 main_valid=0;
}
static int network_input(int k){DNSnapshot s;DNClient c;int *sel=&view_sel[shell_view],*top=&view_top[shell_view],count=0,i,found=-1;char msg[140];ds_net_snapshot(&s);
 if(k==27){shell_view=0;main_valid=0;return 1;}
 if(shell_view==1){if(k=='s'||k=='S')network_action(DN_START,0);else if(k=='x'||k=='X')network_action(DN_STOP,0);else if(k=='r'||k=='R')network_action(DN_RESTART,0);else if(k=='a'||k=='A')network_action(DN_AUTOSTART,!s.autostart);else if(k=='b'||k=='B')network_action(DN_BEEP,!s.beeper);else if(k==-1){if(over(LIST_X,10,CONTENT_W,1))network_action(DN_AUTOSTART,!s.autostart);if(over(LIST_X,11,CONTENT_W,1))network_action(DN_BEEP,!s.beeper);if(over(23,15,15,1))network_action(DN_START,0);if(over(43,15,15,1))network_action(DN_STOP,0);if(over(63,15,15,1))network_action(DN_RESTART,0);}return 1;}
 if(shell_view==2||shell_view==3){for(i=0;ds_net_client(i,&c);i++)if(shell_view==2||c.transferring){if(count==*sel)found=i;count++;}}
 else count=shell_view==5?5:shell_view==6?14:32;
 if(shell_view==4){if(k==K_UP&&*top)(*top)--;if(k==K_DOWN&&*top<31)(*top)++;if(k==K_PGUP)*top=*top>18?*top-18:0;if(k==K_PGDN)*top=*top<13?*top+18:31;while(*top&&!ds_net_log(*top,msg,sizeof msg))(*top)--;return 1;}
 if(k==K_UP&&*sel>0)(*sel)--;if(k==K_DOWN&&*sel+1<count)(*sel)++;if(k==K_PGUP)*sel=*sel>10?*sel-10:0;if(k==K_PGDN)*sel=*sel+10<count?*sel+10:count?count-1:0;
 if(*sel>=count)*sel=count?count-1:0;if(*top>*sel)*top=*sel;if(*sel>=*top+(shell_view==2||shell_view==3?17:19))*top=*sel-(shell_view==2||shell_view==3?16:18);
 if(k==-1){if(shell_view==5){if(mx>=LIST_X&&mx<80&&my>=5&&my<10){*sel=my-5;network_action(DN_SAVER,*sel);}if(over(23,15,22,1))network_action(DN_PREVIEW,(unsigned)view_sel[5]);if(over(51,15,26,1))network_idle_edit();}else {int first=(shell_view==2||shell_view==3)?6:4,rows=(shell_view==2||shell_view==3)?17:19;if(mx>=LIST_X&&mx<80&&my>=first&&my<first+rows&&*top+my-first<count)*sel=*top+my-first;}return 1;}
 if(shell_view==5){if(k==13)network_action(DN_SAVER,*sel);if(k=='p'||k=='P')network_action(DN_PREVIEW,(unsigned)view_sel[5]);if(k=='i'||k=='I')network_idle_edit();}
 if(shell_view==6&&(k==13||k=='h'||k=='H'))network_utility_run(*sel,k!=13);
 if((shell_view==2||shell_view==3)&&found>=0){if(k=='d'||k=='D')network_action(DN_DISCONNECT,found);if(k==13&&ds_net_client(found,&c)){sprintf(msg,"%s at %s: %s. %lu bytes, %lu seconds. %s",c.user,c.address,c.status,c.bytes,c.seconds,c.file);info(msg);}}
 return 1;
}
static void network_page(int page){shell_view=page==1?2:page==2?1:1;main_valid=0;}
static int network_wait(void){int k,s;for(;;){s=ds_net_drain(0);if(s==1)return 1;main_draw(1);window(3,8,74,7,"Finishing FTP transfers");put(5,10,70,A_PANEL,"Waiting for transfers and connections before launching.");put(5,12,70,A_PANEL,"Esc Cancel launch and keep FTP running");k=event();if(k==-3){ds_net_drain(1);quit_app=1;return 0;}if(k==27){ds_net_drain(1);main_valid=0;return 0;}}}
