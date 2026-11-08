/* Persistent status cells; no input ownership and no full-screen repaint. */
static unsigned long status_tick,status_bytes,status_progress,status_packets,status_traffic;
static char status_name[80],status_connections[64],status_job[64],status_ram[39];
static unsigned long status_cache_tick;
static int status_cached,status_indicator,status_connection_indicator;
static void status_refresh(void) {
    DNSnapshot s;DNWork w;DNClient c;char line[180],who[64];
    const char *state,*name,*prefix;unsigned long now=*(unsigned long __far*)MK_FP(0x40,0x6c);
    unsigned long packets;int indicator=' ',traffic=' ';
    ds_net_snapshot(&s);ds_net_work(&w);
    state=s.state==DN_RUNNING?"Running":s.state==DN_STARTING?"Starting":s.state==DN_STOPPING?"Stopping":s.state==DN_STOPPED?"Stopped":"Unavailable";
    if(s.clients&&ds_net_client(0,&c)) {
        sprintf(who,"%.15s@%.23s",*c.user?c.user:"(login)",c.address);
        sprintf(line,"FTP %s | %u client%s | %s",state,s.clients,s.clients==1?"":"s",who);
    } else if(s.state==DN_RUNNING)
        sprintf(line,"FTP ready %u.%u.%u.%u:%u | No clients",s.ip[0],s.ip[1],s.ip[2],s.ip[3],s.port);
    else sprintf(line,"FTP %s | No clients",state);
    if(strcmp(status_connections,line)){strncpy(status_connections,line,sizeof status_connections-1);status_needs_paint=1;}
    packets=s.sent+s.received;
    if(packets!=status_packets){status_packets=packets;status_traffic=now;}
    if(status_traffic&&now-status_traffic<6UL)traffic='*';
    if(status_connection_indicator!=traffic)status_needs_paint=1;status_connection_indicator=traffic;
    if(*pending_job&&(w.state==DN_WORK_IDLE||w.state==DN_WORK_DONE)) {
        memset(&w,0,sizeof w);w.state=DN_WORK_WAITING;
        strcpy(w.name,pending_job);strcpy(w.detail,"Launch queued");
    }
    if(strcmp(status_name,w.name)){strncpy(status_name,w.name,sizeof status_name-1);status_bytes=0;status_progress=0;}
    if(w.state==DN_WORK_ACTIVE&&w.bytes!=status_bytes){status_bytes=w.bytes;status_progress=now;}
    prefix=w.state==DN_WORK_DONE?"Done: ":w.state==DN_WORK_WAITING||(w.state==DN_WORK_ACTIVE&&(!status_progress||now-status_progress>=18UL))?"Waiting: ":"";
    if(w.transfer) {
        name=strrchr(w.name,'\\');if(!name)name=strrchr(w.name,'/');name=name&&name[1]?name+1:w.name;
        if(w.total)sprintf(line,"%s%s %.20s | %lu/%lu B",prefix,w.detail,name,w.bytes,w.total);
        else sprintf(line,"%s%s %.30s | %lu B",prefix,w.detail,name,w.bytes);
    } else if(w.state!=DN_WORK_IDLE) {
        if(*w.id)sprintf(line,"%.20s: %.30s [%.8s]",w.detail,w.name,w.id);
        else sprintf(line,"%.22s: %.38s",w.detail,w.name);
    } else strcpy(line,"Idle");
    if(w.state==DN_WORK_ACTIVE)indicator=status_progress&&now-status_progress<9UL?"|/-\\"[(now/4UL)%4]:'.';
    else if(w.state==DN_WORK_WAITING)indicator='.';
    else if(w.state==DN_WORK_DONE)indicator='+';
    else if(w.state==DN_WORK_FAILED)indicator='!';
    if(strcmp(status_job,line)){strncpy(status_job,line,sizeof status_job-1);status_needs_paint=1;}
    if(status_indicator!=indicator)status_needs_paint=1;status_indicator=indicator;
    if(dm_memory.conv_valid)sprintf(line,"DOS %luK EMS %uK XMS %uK",dm_memory.launch_bytes/1024UL,dm_memory.ems_kb,dm_memory.xms_largest_kb);
    else strcpy(line,"RAM: checking");
    if(strcmp(status_ram,line)){strncpy(status_ram,line,sizeof status_ram-1);status_needs_paint=1;}
    status_cache_tick=now;status_cached=1;
}
static void status_draw(void) {
    unsigned long now=*(unsigned long __far*)MK_FP(0x40,0x6c);
    if(!status_cached||now-status_cache_tick>=4UL)status_refresh();
    if(!status_needs_paint)return;
    fill(0,LOC_CONNECTION_ROW,80,2,A_PANEL);
    put(1,LOC_CONNECTION_ROW,12,A_PANEL,LOC_CONNECTION_LABEL);
    put(1,LOC_JOB_ROW,12,A_PANEL,LOC_JOB_LABEL);
    put(14,LOC_CONNECTION_ROW,63,A_PANEL,status_connections);
    put(14,LOC_JOB_ROW,63,A_PANEL,status_job);
    cell(78,LOC_CONNECTION_ROW,A_PANEL,status_connection_indicator);
    cell(78,LOC_JOB_ROW,A_PANEL,status_indicator);
    put(37,0,38,A_PANEL,status_ram);
    status_needs_paint=0;
}
static void status_poll(void) {
    unsigned long now=*(unsigned long __far*)MK_FP(0x40,0x6c);
    if(now-status_tick<4UL)return;status_tick=now;dashboard_poll();present();
}
