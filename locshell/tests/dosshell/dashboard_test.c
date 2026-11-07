/* Real DOS renderer/input/model acceptance with controlled service snapshots. */
#define DOSSHELL_RENDER_TEST
#define DOSSHELL_INPUT_TEST
#define DS_NETWORK_EMBEDDED
#include "../../src/dosshell/dosshell.c"
#undef main
static DNSnapshot mock_net;
static DNWork mock_work;
static DNClient mock_client;
static DNFileEvent mock_change;
static unsigned show_client,show_change,checks;
static FILE*report;
static unsigned short __far saved_frame[2000];
int ds_test_event(void){return 27;}
int ds_net_poll(void){return 0;}
void ds_net_activity(void){}
void ds_net_snapshot(DNSnapshot*s){*s=mock_net;}
void ds_net_work(DNWork*w){*w=mock_work;}
int ds_net_client(unsigned i,DNClient*c){if(i||!show_client)return 0;*c=mock_client;return 1;}
int ds_net_log(unsigned i,char*b,unsigned n){(void)i;(void)b;(void)n;return 0;}
int ds_net_file_event(unsigned i,DNFileEvent*e){if(i||!show_change)return 0;*e=mock_change;return 1;}
DNResult ds_net_command(DNAction a,unsigned v,char*b,unsigned n){(void)a;(void)v;(void)n;b[0]=0;return DN_OK;}
void ds_net_restart_ready(void){}
int ds_net_drain(int cancel){(void)cancel;return 1;}
time_t time(time_t*p){time_t value=1700000000L;if(p)*p=value;return value;}
static void require(int ok,const char*message){checks++;if(!ok){finish_screen();fprintf(report,"FAIL %u %s\n",checks,message);fclose(report);exit(1);}}
static int at(unsigned x,unsigned y,const char*s){while(*s){if((screen[y*80+x++]&255)!=(unsigned char)*s++)return 0;}return 1;}
int main(void){LPRecent recent[LP_RECENT_MAX];unsigned n,i;unsigned long id,words;FILE*f;char line[120],*extra;
 report=fopen("DASHTEST.TXT","w");if(!report)return 2;
 require(lb_load("LIBRARY.TXT")&&dm_load("MEMORY.TSV"),"current catalogue and memory profiles load");
 rebuild();dm_query();memory_loaded=1;id=memory_ids[0];
 remove("C:\\DOSSHELL\\RECENT.TXT");remove("C:\\DOSSHELL\\RECENT.NEW");
 for(i=1;i<=10;i++)require(lp_recent_add('A',i,1700000000UL+i),"history writes atomically");
 n=lp_recent_load(recent,LP_RECENT_MAX);require(n==8&&recent[0].id==10&&recent[7].id==3,"history is bounded and most recent first");
 require(lp_recent_add('A',6,1700000050UL),"repeat history entry updates");n=lp_recent_load(recent,8);require(n==8&&recent[0].id==6&&recent[1].id==10,"repeat moves to front without duplicates");
 require(!lp_recent_add('T',0,1),"invalid identity rejected");
 remove("C:\\DOSSHELL\\RECENT.TXT");require(lp_recent_add('A',id,1700000000UL),"record actual catalogue identity");
 mock_net.state=DN_RUNNING;mock_net.ip[0]=198;mock_net.ip[1]=51;mock_net.ip[2]=100;mock_net.ip[3]=17;mock_net.port=21;mock_net.uptime=3661;
 f=fopen("C:\\DOSSHELL\\VAULT.STA","w");fputs("LOC_VAULT 1\nMODE=BACKUP\nSTATE=AVAILABLE\nGAMES=5\nSNAPSHOTS=5\nLAST=2026-11-03 03:25Z\nREPORTED=2026-11-07 04:00Z\nDETAIL=Backup snapshots only.\n",f);fclose(f);
 shell_view=7;init_screen();main_draw(0);present();
 require(at(2,2,"DASHBOARD")&&at(1,8,"RECENT LAUNCHES"),"Home layout and recent links visible");
 for(i=2;i<23;i++)require((screen[i*80+18]&255)!=179,"dashboard has no sidebar divider");
 for(i=0;i<2000;i++)require((screen[i]&0xf000)!=0,"dashboard uses all screen space without black margins");
 require(at(29,4,"Backups available")&&at(29,5,"5 games / 5 snapshots"),"vault reports actual backup mode and counts");
 require(at(1,23,"Connections:")&&at(1,24,"Active job:"),"both persistent status rows remain");
 words=video_words_written;dash_refresh(0);present();require(video_words_written==words,"unchanged dashboard writes no VGA cells");
 dashboard_input(9);require(dash_focus==1,"Tab moves dashboard focus");dashboard_input(K_UP);require(dash_focus==0,"arrows move dashboard focus");
 show_client=1;mock_client.transferring=1;mock_client.direction=DN_XFER_RECEIVE;mock_client.bytes=12345;strcpy(mock_client.file,"C:\\UPLOAD\\INCOMING.BIN");strcpy(mock_client.user,"tester");strcpy(mock_client.address,"10.0.2.2");strcpy(mock_client.status,"Receiving file");mock_net.uploaded=12345;
 dash_refresh(0);require(at(2,17,"RECEIVING")&&at(2,20,"File size not supplied"),"unknown upload total never fabricates percentage");
 mock_client.direction=DN_XFER_SEND;mock_client.bytes=50;mock_client.total=100;dash_refresh(0);require(at(2,17,"SENDING"),"download direction correct");
 for(i=0;i<40;i++)require((screen[18*80+2+i]&255)==(i<20?219:176),"known-size progress bar is exact");
 mock_client.bytes=4000000000UL;mock_client.total=4000000000UL;dash_refresh(0);require((screen[18*80+41]&255)==219,"large progress total does not overflow");
 window(8,8,50,10,"Modal stays intact");memcpy(saved_frame,screen,sizeof screen);dash_tick=0;dashboard_poll();require(!memcmp(saved_frame,screen,sizeof screen),"live dashboard never overwrites modal dialogs");
 show_client=0;show_change=1;strcpy(mock_change.action,"Received");strcpy(mock_change.file,"C:\\UPLOAD\\INCOMING.BIN");mock_change.ok=1;mock_change.bytes=12345;mock_change.stamp=1700000000UL;
 main_draw(0);require(at(47,10,"C:\\UPLOAD\\INCOMING.BIN"),"completed file paths persist visibly");
 f=fopen("C:\\DOSSHELL\\VAULT.STA","w");fputs("LOC_VAULT 1\nMODE=SYNC\nSTATE=SYNCED\nGAMES=5\nSNAPSHOTS=5\n",f);fclose(f);dash_vault_tick=0;dash_refresh(0);require(at(29,4,"No report available"),"unvalidated sync state cannot become a green synced claim");
 dashboard_input('g');require(shell_view==0,"Games quick link works");view_open(14);require(shell_view==7,"sidebar Home opens dashboard");
 main_draw(0);mx=32;my=15;dashboard_input(-1);require(shell_view==1,"mouse FTP shortcut works across full width");
 shell_view=7;main_draw(0);extra=strdup(lb_str(lb_entries[0]->extra));lb_replace(&lb_entries[0]->extra,"---BOOT---XM\n");putenv("CONFIG=N");
 dash_launch(0);require(!quit_app&&!launch_app,"dashboard respects boot-mode launch gate");lb_replace(&lb_entries[0]->extra,extra);free(extra);
 dash_launch(0);require(quit_app&&launch_app,"dashboard prepares the normal launch and return path");n=lp_recent_load(recent,8);require(n==1&&recent[0].id==id,"preparing a launch does not record a play");
 f=fopen("C:\\DOSSHELL\\LAUNCH.NEW","r");require(f!=NULL,"validated launch request produced");fgets(line,sizeof line,f);fclose(f);require(strstr(line,"LOC3 RUN")!=NULL,"game recent keeps game request type");
 quit_app=launch_app=0;remove("C:\\DOSSHELL\\RUN.BAT");
 require(run_context("C:\nCD \\DOSSHELL\nHELP.EXE\n","TOOL",9,"Ping"),"tool request prepared");
 f=fopen("C:\\DOSSHELL\\LAUNCH.NEW","r");fgets(line,sizeof line,f);fclose(f);require(strstr(line,"LOC3 TOOL 00000009")!=NULL,"utility is identified independently of selected game");
 finish_screen();fprintf(report,"DASHBOARD_OK checks=%u history,full-width,live-progress,modal,mouse,keys,boot-gate,typed-launch\n",checks);fclose(report);lb_clear();return 0;
}
