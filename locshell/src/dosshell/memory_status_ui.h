/* Optional catalogue boot modes are checked before offering a positive RAM fit.
   A game may have enough RAM yet be incompatible with the current manager. */
static int game_boot_requirement(int item,char*first){const char*p=lb_str(lb_entries[item]->extra),*mode=getenv("CONFIG");unsigned n,len;int found=0;
 first[0]=0;
 while(*p){const char*end=strchr(p,'\n');if(!end)end=p+strlen(p);
  if(end-p>10&&!strncmp(p,"---BOOT---",10)){p+=10;
   while(p<end){while(p<end&&(*p==','||*p==' '||*p=='\r'))p++;n=0;while(p+n<end&&p[n]!=','&&p[n]!=' '&&p[n]!='\r')n++;
    if(n&&n<=8){if(!found){memcpy(first,p,n);first[n]=0;found=1;}len=mode?strlen(mode):0;if(len==n&&!strnicmp(p,mode,n))return 0;}
    p+=n;if(!n&&p<end)p++;
   }
   return found;
  }
  p=*end?end+1:end;
 }
 return 0;
}
typedef struct {DLFit learned;DLRecord record;const char*label;unsigned ink,review;char marker;char boot[14];} MemoryState;
static void memory_state(int item,MemoryState*s){char documented;
 s->record.id=0;s->review=0;
 {char mode[9];if(game_boot_requirement(item,mode)){s->learned=DL_FIT_NONE;sprintf(s->boot,"Boot %s",mode);s->label=s->boot;s->ink=14;s->marker='!';return;}}
 if(memory_context_dirty){s->learned=DL_FIT_INCOMPLETE;s->ink=14;s->marker='!';s->label="Updating";return;}
 s->learned=dl_has(memory_ids[item])?dl_assess(memory_ids[item],lb_str(lb_entries[item]->run),&dm_memory,&s->record):DL_FIT_NONE;
 documented=dm_status(memory_profiles[item],&dm_memory);
 s->ink=14;s->marker='!';
 if(s->learned==DL_FIT_READY){s->label="Runs";s->ink=10;s->marker=(char)251;}
 else if(documented=='-'){s->label="Low RAM";s->ink=12;s->marker='X';}
 else if(s->learned==DL_FIT_CHANGED&&s->record.disposition==DL_CONFIRMED&&dl_budget(&s->record,&dm_memory)==DL_FIT_READY){s->label="Runs";s->ink=10;s->marker=(char)251;s->review=1;}
 else if(s->learned==DL_FIT_LESS_RAM)s->label="Less RAM";
 else if(s->learned==DL_FIT_CHANGED)s->label="Setup diff";
 else if(s->learned==DL_FIT_CONFIRM)s->label="Confirm";
 else if(s->learned==DL_FIT_FAILED)s->label="Run failed";
 else if(s->learned==DL_FIT_INCOMPLETE)s->label="Incomplete";
 else if(documented=='+'){s->label="Runs";s->ink=10;s->marker=(char)251;}
 else {s->label="Unknown";s->ink=7;s->marker='?';}
}
static const char*memory_profile(void){const char*p=strstr(loc_shell_title()," / ");return p?p+3:"Unspecified";}
static const char*memory_mode(void){const char*p=getenv("CONFIG");return p&&*p?p:"unspecified";}
