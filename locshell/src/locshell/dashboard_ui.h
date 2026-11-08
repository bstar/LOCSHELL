/* Full-width Home view. Cached disk data; foreground snapshots and cell diffs. */
static LPRecent dash_recent[LP_RECENT_MAX],dash_links[6];
static unsigned dash_loaded,dash_count,dash_focus,dash_recent_count;
static unsigned long dash_tick,dash_rate_tick,dash_rx,dash_tx,dash_rate_in,dash_rate_out;
static char dash_vault_mode[16],dash_vault_state[16],dash_vault_last[24],dash_vault_report[24],dash_vault_detail[80];
static unsigned dash_vault_games,dash_vault_snapshots,dash_vault_known;
static unsigned long dash_vault_tick;
static int dash_find(unsigned long id){int i;for(i=0;i<lb_count;i++)if(memory_ids[i]==id)return i;return -1;}
static int dashboard_current_item(void){return dash_focus<dash_count&&dash_links[dash_focus].kind=='A'?dash_find(dash_links[dash_focus].id):-1;}
static void dash_bytes(char*out,unsigned long bytes){if(bytes>=1048576UL)sprintf(out,"%lu.%luM",bytes/1048576UL,(bytes%1048576UL)/104858UL);else if(bytes>=1024UL)sprintf(out,"%lu.%luK",bytes/1024UL,(bytes%1024UL)/103UL);else sprintf(out,"%luB",bytes);}
static const char*dash_title(const LPRecent*r){int i;if(r->kind=='T')return r->id>=1&&r->id<=14?tool_names[(unsigned)r->id-1]:"Unavailable tool";i=dash_find(r->id);return i>=0?lb_entries[i]->title:"Unavailable application";}
static void dash_load_links(void){unsigned i,j;int item;
 if(!dash_loaded){dash_recent_count=lp_recent_load(dash_recent,LP_RECENT_MAX);dash_loaded=1;}
 dash_count=0;
 for(i=0;i<dash_recent_count&&dash_count<6;i++){
  if(dash_recent[i].kind=='T'){if(dash_recent[i].id<1||dash_recent[i].id>14)continue;}
  else {item=dash_find(dash_recent[i].id);if(item<0||lb_blank(lb_str(lb_entries[item]->run)))continue;}
  dash_links[dash_count++]=dash_recent[i];
 }
 for(item=0;item<lb_count&&dash_count<6;item++)if(lb_entries[item]->favorite&&!lb_blank(lb_str(lb_entries[item]->run))){MemoryState fit;memory_state(item,&fit);if(fit.ink!=10)continue;
  for(j=0;j<dash_count;j++)if(dash_links[j].kind=='A'&&dash_links[j].id==memory_ids[item])break;
  if(j==dash_count){dash_links[dash_count].kind='A';dash_links[dash_count].id=memory_ids[item];dash_links[dash_count++].stamp=0;}
 }
 if(dash_focus>=dash_count+5)dash_focus=0;
}
static void dash_read_vault(unsigned long now){FILE*f;char line[128],*value;unsigned n=0,fields=0;char mode[16]="",state[16]="",last[24]="",reported[24]="",detail[80]="";unsigned games=0,snaps=0;
 if(dash_vault_tick&&now-dash_vault_tick<36UL)return;dash_vault_tick=now?now:1;
 f=fopen("C:\\LOCSHELL\\VAULT.STA","r");if(!f){dash_vault_known=0;return;}
 if(!fgets(line,sizeof line,f)||strcmp(line,"LOC_VAULT 1\n")){fclose(f);dash_vault_known=0;return;}
 while(n++<16&&fgets(line,sizeof line,f)){line[strcspn(line,"\r\n")]=0;value=strchr(line,'=');if(!value)continue;*value++=0;
  if(!strcmp(line,"MODE")){strncpy(mode,value,sizeof mode-1);fields|=1;}
  else if(!strcmp(line,"STATE")){strncpy(state,value,sizeof state-1);fields|=2;}
  else if(!strcmp(line,"LAST"))strncpy(last,value,sizeof last-1);
  else if(!strcmp(line,"REPORTED"))strncpy(reported,value,sizeof reported-1);
  else if(!strcmp(line,"DETAIL"))strncpy(detail,value,sizeof detail-1);
  else if(!strcmp(line,"GAMES")){char*end;unsigned long v=strtoul(value,&end,10);if(*value&&!*end&&v<=65535UL){games=(unsigned)v;fields|=4;}}
  else if(!strcmp(line,"SNAPSHOTS")){char*end;unsigned long v=strtoul(value,&end,10);if(*value&&!*end&&v<=65535UL){snaps=(unsigned)v;fields|=8;}}
 }
 fclose(f);dash_vault_known=fields==15&&!strcmp(mode,"BACKUP")&&(!strcmp(state,"AVAILABLE")||!strcmp(state,"EMPTY")||!strcmp(state,"ERROR"));
 if(dash_vault_known){strcpy(dash_vault_mode,mode);strcpy(dash_vault_state,state);strcpy(dash_vault_last,last);strcpy(dash_vault_report,reported);strcpy(dash_vault_detail,detail);dash_vault_games=games;dash_vault_snapshots=snaps;}
}
static void dashboard_vault(void){const char*lines[10];char counts[80],last[80],reported[80];
 if(dash_vault_known){sprintf(counts,"%u games; %u backup snapshots",dash_vault_games,dash_vault_snapshots);sprintf(last,"Last backup: %.23s",*dash_vault_last?dash_vault_last:"None");sprintf(reported,"Report issued: %.23s",dash_vault_report);
  lines[0]=!strcmp(dash_vault_state,"AVAILABLE")?"Save vault: backup snapshots available":!strcmp(dash_vault_state,"ERROR")?"Save vault report: error":"Save vault: no backup snapshots";
  lines[1]=counts;lines[2]=last;lines[3]=reported;lines[4]="";lines[5]=dash_vault_detail;lines[6]="";lines[7]="Automatic two-way sync is not active.";lines[8]="Local changes and conflicts have not been checked.";lines[9]="The report timestamp is not a live connection check.";
 }else{lines[0]="No save-vault report is available on this machine.";lines[1]="";lines[2]="Backup and sync state are unknown.";lines[3]="";lines[4]="A vault report will appear here when published.";lines[5]="";lines[6]="";lines[7]="";lines[8]="";lines[9]="";}
 text_page("Save vault",lines,10);main_valid=0;
}
static void dash_refresh(int inactive){DNSnapshot s;DNWork w;DNClient c;DNFileEvent change;unsigned long now=*(unsigned long __far*)MK_FP(0x40,0x6c),elapsed;unsigned a=inactive?A_IDLE:A_NORMAL;char line[160],size[24],other[24],clocktext[32];int i,client=-1;time_t when;struct tm*tm;const char*event_path;unsigned event_length;
 ds_net_snapshot(&s);ds_net_work(&w);dash_read_vault(now);
 if(!dash_rate_tick){dash_rate_tick=now;dash_rx=s.uploaded;dash_tx=s.downloaded;}
 elapsed=now-dash_rate_tick;
 if(elapsed>=18UL){dash_rate_in=(s.uploaded-dash_rx)*18UL/elapsed;dash_rate_out=(s.downloaded-dash_tx)*18UL/elapsed;dash_rx=s.uploaded;dash_tx=s.downloaded;dash_rate_tick=now;}
 when=time(NULL);tm=localtime(&when);if(tm)strftime(clocktext,sizeof clocktext,"%a %d %b  %H:%M:%S",tm);else strcpy(clocktext,"Clock unavailable");
 put(53,2,26,a,clocktext);
 sprintf(line,"%s",s.state==DN_RUNNING?"Ready for connections":s.state==DN_STARTING?"Starting FTP":s.state==DN_STOPPING?"Finishing connections":s.state==DN_STOPPED?"FTP stopped":"Network unavailable");put(2,4,24,a,line);
 if(s.state!=DN_UNAVAILABLE)sprintf(line,"%u.%u.%u.%u:%u",s.ip[0],s.ip[1],s.ip[2],s.ip[3],s.port);else strcpy(line,"No network address");put(2,5,24,a,line);
 sprintf(line,"%u client%s / %u transfer%s",s.clients,s.clients==1?"":"s",s.transfers,s.transfers==1?"":"s");put(2,6,24,a,line);
 put(29,4,24,a,!dash_vault_known?"No report available":!strcmp(dash_vault_state,"AVAILABLE")?"Backups available":!strcmp(dash_vault_state,"ERROR")?"Report needs attention":"No backups reported");
 if(dash_vault_known)sprintf(line,"%u games / %u snapshots",dash_vault_games,dash_vault_snapshots);else strcpy(line,"Sync status unknown");put(29,5,24,a,line);
 if(dash_vault_known&&*dash_vault_last)sprintf(line,"Last: %.18s",dash_vault_last);else strcpy(line,"V  View vault details");put(29,6,24,a,line);
 sprintf(line,"Uptime %lu:%02lu:%02lu",s.uptime/3600UL,(s.uptime/60UL)%60UL,s.uptime%60UL);put(56,4,23,a,line);
 sprintf(line,"%lu in / %lu out files",s.files_in,s.files_out);put(56,5,23,a,line);
 dash_bytes(size,s.uploaded);dash_bytes(other,s.downloaded);sprintf(line,"In %s / Out %s",size,other);put(56,6,23,a,line);
 for(i=0;i<6;i++){
  if((unsigned)i<dash_count){MemoryState fit;unsigned attr=!inactive&&dash_focus==(unsigned)i?A_SELECT:a;int item=dash_links[i].kind=='A'?dash_find(dash_links[i].id):-1;char marker='T';
   if(item>=0){memory_state(item,&fit);marker=fit.marker;}
   sprintf(line,"%u %c %.38s",i+1,marker,dash_title(&dash_links[i]));put(1,9+i,43,attr,line);
   if(!inactive&&item>=0){cell(3,9+i,(attr&0xf0)|(dash_focus==(unsigned)i&&fit.ink>=10?fit.ink-8:fit.ink),(unsigned char)marker);if(fit.review)cell(4,9+i,(attr&0xf0)|6,'!');}
  }
  else put(1,9+i,43,a,i==0?"No launches yet. G Games / T Tools":"");
 }
 for(i=0;i<3;i++){
  put(47,9+i*2,32,a,"");put(47,10+i*2,32,a,"");
  if(ds_net_file_event(i,&change)){when=(time_t)change.stamp;tm=localtime(&when);if(tm)strftime(clocktext,sizeof clocktext,"%H:%M:%S",tm);else strcpy(clocktext,"--:--:--");
   event_path=change.file;event_length=strlen(event_path);if(event_length>32)event_path+=event_length-32;
   dash_bytes(size,change.bytes);sprintf(line,"%.8s %c %.11s %s",clocktext,change.ok?'+':'!',change.action,size);put(47,9+i*2,32,a,line);put(47,10+i*2,32,a,event_path);
  }else if(i==0)put(47,10,32,a,"Waiting for file changes...");
 }
 for(i=0;i<5;i++){static const char*links[]={"G Games","T Tools","F FTP","A Activity","V Vault"};static const int xx[]={1,16,31,46,63};put(xx[i],15,i==4?16:14,!inactive&&dash_focus==dash_count+(unsigned)i?A_SELECT:A_PANEL,links[i]);}
 for(i=17;i<=21;i++)put(1,i,78,a,"");
 for(i=0;ds_net_client(i,&c);i++)if(c.transferring&&c.direction!=DN_XFER_LIST){client=i;break;}
 if(client>=0){unsigned bar=0,j;char progress[41];
  sprintf(line,"%s  %.62s",c.direction==DN_XFER_RECEIVE?"RECEIVING":"SENDING",c.file);put(2,17,76,a,line);
  if(c.total){unsigned long clamped=c.bytes>c.total?c.total:c.bytes;bar=(unsigned)(c.total<107374182UL?clamped*40UL/c.total:clamped/(c.total/40UL+1UL));if(c.bytes>=c.total)bar=40;}
  for(j=0;j<40;j++)progress[j]=(char)(c.total?(j<bar?219:176):176);progress[40]=0;
  if(!c.total&&c.bytes&&((s.uploaded!=dash_rx)||(s.downloaded!=dash_tx)))progress[(now/4UL)%40]=(char)219;
  put(2,18,40,a,progress);dash_bytes(size,c.bytes);dash_bytes(other,c.direction==DN_XFER_RECEIVE?dash_rate_in:dash_rate_out);
  if(c.total){char total[24];dash_bytes(total,c.total);sprintf(line,"%s / %s  %s/s",size,total,other);}else sprintf(line,"%s %s  %s/s",size,c.direction==DN_XFER_RECEIVE?"received":"sent",other);put(44,18,34,a,line);
  sprintf(line,"%.19s @ %.23s  |  %.23s",*c.user?c.user:"Signing in",c.address,c.status);put(2,19,76,a,line);
  put(2,20,76,a,c.total?"File size known; bar shows bytes transferred.":"File size not supplied; received bytes update as data arrives.");
 }else if(w.state!=DN_WORK_IDLE){sprintf(line,"%s: %.62s",w.state==DN_WORK_FAILED?"FAILED":w.state==DN_WORK_DONE?"COMPLETED":"QUEUED",w.name);put(2,17,76,a,line);sprintf(line,"%.31s %.11s",w.detail,w.id);put(2,18,76,a,line);}
 else {put(2,17,76,a,s.state==DN_RUNNING?"Ready for incoming files":"No active work");put(2,19,76,a,"FTP continues while you navigate this dashboard.");}
 if(s.reload)put(2,21,76,a,"Update queued: LOC SHELL restarts after connections and open edits finish.");
 else {sprintf(line,"%d catalogue entries  |  Boot %.8s  |  %lu failed transfers",lb_count,memory_mode(),s.failed);put(2,21,76,a,line);}
 if(dash_focus<dash_count){LPRecent*r=&dash_links[dash_focus];if(r->stamp){when=(time_t)r->stamp;tm=localtime(&when);if(tm)strftime(clocktext,sizeof clocktext,"%d %b %H:%M",tm);else strcpy(clocktext,"Unknown time");sprintf(line,"%.57s | %s",dash_title(r),clocktext);}else sprintf(line,"Favorite: %.67s",dash_title(r));}
 else strcpy(line,"Tab / arrows select   Enter opens   1-6 launch   F2 Home   Esc Games");
 put(1,22,78,a,*notice?notice:line);
}
static void dashboard_draw(int inactive){int i;unsigned a=inactive?A_IDLE:A_NORMAL;
 dashboard_live=0;shell_frame(inactive);put(2,2,49,a,"DASHBOARD");
 for(i=1;i<79;i++){cell(i,7,A_BORDER,196);cell(i,16,A_BORDER,196);}
 put(2,3,24,a,"FTP / CONNECTIONS [F]");put(29,3,24,a,"SAVE VAULT  [V]");put(56,3,23,a,"THIS SESSION");
 put(1,8,43,a,"RECENT LAUNCHES / READY FAVORITES");put(47,8,32,a,"RECENT FILE CHANGES");
 for(i=8;i<15;i++)cell(45,i,A_BORDER,179);
 if(memory_loaded)dash_load_links();dash_refresh(inactive);
 view_footer("F2 Home  G Games  T Tools  F FTP  A Activity");
 dashboard_live=!inactive;main_valid=0;
}
static void dashboard_poll(void){unsigned long now=*(unsigned long __far*)MK_FP(0x40,0x6c);if(!dashboard_live||shell_view!=7||now-dash_tick<4UL)return;dash_tick=now;dash_refresh(0);}
static void dash_launch(unsigned n){int item,i;if(n>=dash_count)return;
 dash_focus=n;
 if(dash_links[n].kind=='T'){network_utility_run((int)dash_links[n].id-1,0);return;}
 item=dash_find(dash_links[n].id);if(item<0){info("This recent application is no longer in the catalogue.");return;}
 grouping=-1;inside=0;rebuild();for(i=0;i<nrows;i++)if(rows[i]==item){sel=i;selection_clamp();break;}action(ACT_RUN);
}
static void dash_open(unsigned n){if(n<dash_count){dash_launch(n);return;}switch(n-dash_count){case 0:shell_view=0;break;case 1:shell_view=6;break;case 2:shell_view=1;break;case 3:shell_view=4;break;default:dashboard_vault();break;}nav_focus=-1;main_valid=0;}
static int dashboard_input(int k){unsigned count=dash_count+5;int i;
 if(k==27||k=='g'||k=='G'){shell_view=0;main_valid=0;return 1;}
 if(k=='t'||k=='T'){dash_open(dash_count+1);return 1;}
 if(k=='f'||k=='F'){dash_open(dash_count+2);return 1;}
 if(k=='a'||k=='A'){dash_open(dash_count+3);return 1;}
 if(k=='v'||k=='V'){dashboard_vault();return 1;}
 if(k==18){dash_open(dash_focus);return 1;}
 if(k==318){memory_details();main_valid=0;return 1;}
 if(k==321){learning_page();main_valid=0;return 1;}
 if(k==319){dash_loaded=0;dash_vault_tick=0;dm_query();dl_index();main_valid=0;return 1;}
 if(k==3){action(ACT_SETUP);return 1;}
 if(k>='1'&&k<='6'){dash_launch((unsigned)(k-'1'));return 1;}
 if(k==9||k==K_DOWN||k==K_RIGHT)dash_focus=(dash_focus+1)%count;
 else if(k==K_SHIFT_TAB||k==K_UP||k==K_LEFT)dash_focus=(dash_focus+count-1)%count;
 else if(k==K_HOME)dash_focus=0;else if(k==K_END)dash_focus=count-1;
 else if(k==13)dash_open(dash_focus);
 else if(k==K_F1){dashboard_vault();}
 else if(k==-1){if(my>=9&&my<15&&mx>=1&&mx<44){dash_focus=my-9;if(dash_focus<dash_count)dash_launch(dash_focus);else dash_focus=0;}
  else if(my==15){static const int xx[]={1,16,31,46,63};for(i=4;i>=0;i--)if(mx>=xx[i]){dash_focus=dash_count+i;dash_open(dash_focus);break;}}
  else if(my>=3&&my<=6&&mx>=1&&mx<27){shell_view=1;main_valid=0;}
  else if(my>=3&&my<=6&&mx>=29&&mx<54)dashboard_vault();
  else if(my>=8&&my<15&&mx>=47){shell_view=4;main_valid=0;}
 }
 return 1;
}
