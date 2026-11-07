 static const char *tool_names[]={"DHCP (explicit configuration change)","DNS lookup","FTP client","HTTP download","HTTP server","IRC client","Netcat","NetDrive (explicit resident/mount action)","Ping","Packet tools","SNTP clock","Speed test","Telnet","NetDrive read-ahead benchmark"};
 static const char *tool_apps[]={"DHCP","DNSTEST","FTP","HTGET","HTTPSERV","IRCJR","NC","NETDRIVE","PING","PKTTOOL","SNTP","SPDTEST","TELNET","ND_CACHE"};

static void network_utility_run(int selutil,int help){
 int k;unsigned pos=0;char args[100]="",cmd[240],path[80];
  sprintf(path,"C:\\NET\\BIN\\%s.EXE",tool_apps[selutil]);
  if(access(path,0)){info("Utility is not installed in C:\\NET\\BIN.");return;}
  if(help)strcpy(args,"-help");
  else {args[0]=0;pos=0;for(;;){main_draw(1);window(3,7,74,10,tool_names[selutil]);put(5,9,70,A_PANEL,"Command arguments (use the utility's documented syntax):");input_draw(5,11,68,1,args,pos,1);put(5,14,70,A_PANEL,"Enter Run   Esc Cancel");k=event();if(k==27||k==13)break;if(k!=-2)input_key(args,sizeof args,&pos,k,0);}if(k==27)return;}
  if(selutil==0||selutil==7||selutil==13){if(!confirm("This utility can change network settings or stay resident. Run it?"))return;}
  if(strchr(args,'\r')||strchr(args,'\n')){info("Arguments must be one line.");return;}
  sprintf(cmd,"C:\nCD \\NET\n%s %s\nPAUSE\n",path,args);
  if(run_context(cmd,"TOOL",(unsigned long)selutil+1,tool_names[selutil]))return;

}
static void network_utilities(void){shell_view=6;main_valid=0;}
