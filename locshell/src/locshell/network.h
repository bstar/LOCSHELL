#ifndef DS_NETWORK_H
#define DS_NETWORK_H
#include <string.h>
#define DN_REDRAW 1
#define DN_SLEEP 2
#define DN_JOB 4
#define DN_RELOAD 8
typedef enum {DN_UNAVAILABLE,DN_STARTING,DN_RUNNING,DN_STOPPING,DN_STOPPED} DNState;
typedef enum {DN_START=1,DN_STOP,DN_RESTART,DN_SAVER,DN_PREVIEW,DN_IDLE,DN_BEEP,DN_AUTOSTART,DN_DISCONNECT} DNAction;
typedef enum {DN_ERROR=-1,DN_OK,DN_PENDING} DNResult;
typedef struct {DNState state;unsigned clients,transfers,closing,port,saver,idle,beeper,autostart,reload;unsigned char ip[4];unsigned long sessions,timeouts,sent,received,retransmits,uploaded,downloaded,files_in,files_out,failed,uptime;char reason[80];} DNSnapshot;
typedef enum {DN_XFER_NONE,DN_XFER_SEND,DN_XFER_RECEIVE,DN_XFER_LIST} DNDirection;
typedef struct {unsigned long id,bytes,total,seconds;unsigned state,transferring,direction;char user[20],file[80],status[24],address[24];} DNClient;
typedef struct {unsigned ok;unsigned long stamp,bytes;char action[12],file[80];} DNFileEvent;
typedef enum {DN_WORK_IDLE,DN_WORK_WAITING,DN_WORK_ACTIVE,DN_WORK_DONE,DN_WORK_FAILED} DNWorkState;
typedef struct {DNWorkState state;unsigned transfer;unsigned long bytes,total;char name[80],id[12],detail[32];} DNWork;
#ifdef __cplusplus
extern "C" {
#endif
#ifdef DS_NETWORK_EMBEDDED
int ds_net_poll(void);
void ds_net_activity(void);
void ds_net_snapshot(DNSnapshot*);
void ds_net_work(DNWork*);
int ds_net_client(unsigned,DNClient*);
int ds_net_log(unsigned,char*,unsigned);
int ds_net_file_event(unsigned,DNFileEvent*);
DNResult ds_net_command(DNAction,unsigned,char*,unsigned);
void ds_net_restart_ready(void);
int ds_net_drain(int);
#else
static int ds_net_poll(void){return 0;}
static void ds_net_activity(void){}
static void ds_net_snapshot(DNSnapshot*s){memset(s,0,sizeof *s);strcpy(s->reason,"Standalone shell: use LOCSHELL.BAT for networking.");}
static void ds_net_work(DNWork*w){memset(w,0,sizeof *w);}
static int ds_net_client(unsigned i,DNClient*c){(void)i;(void)c;return 0;}
static int ds_net_log(unsigned i,char*b,unsigned n){(void)i;(void)b;(void)n;return 0;}
static int ds_net_file_event(unsigned i,DNFileEvent*e){(void)i;(void)e;return 0;}
static DNResult ds_net_command(DNAction a,unsigned v,char*b,unsigned n){(void)a;(void)v;(void)n;strcpy(b,"Integrated networking is not loaded.");return DN_ERROR;}
static void ds_net_restart_ready(void){}
static int ds_net_drain(int cancel){(void)cancel;return 1;}
#endif
#ifdef __cplusplus
}
#endif
#endif
