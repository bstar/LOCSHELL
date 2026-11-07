#define DOSSHELL_RENDER_TEST
#define DOSSHELL_INPUT_TEST
#define main shell_main
#include "../../src/dosshell/dosshell.c"
#undef main
static int keys[80],keycount,keypos;
int ds_test_event(void){if(keypos>=keycount){puts("UI_INPUT_EXHAUSTED");exit(2);}return keys[keypos++];}
static void send(const char*s){while(*s)keys[keycount++]=(unsigned char)*s++;}
static int verify(int b){if(!b){finish_screen();puts("MEMORY_UI_FAILED");return 0;}return 1;}
int main(void){unsigned long id;DMRequirement before,*r;FILE*f;
 shell_view=0;
 if(!lb_load("LIBRARY.TXT")||!dm_load("MEMORY.TSV"))return 1;dm_query();rebuild();init_screen();id=memory_ids[rows[0]];before=*dm_find(id);
 keys[0]=27;keycount=1;keypos=0;memory_details();if(!verify(keypos==1))return 1;
 keypos=keycount=0;send("\033");locked=0;memory_edit(id);if(!verify(!memcmp(&before,dm_find(id),sizeof before)))return 1;
 keypos=keycount=0;send("\001384\t\0010\t\0010\t\001-\t\001D\t\001fixture\r");memory_edit(id);r=dm_find(id);if(!verify(r&&r->conv==384&&r->ems==0&&r->xms==0&&r->known==7&&r->grade=='D'&&!strcmp(r->source,"fixture")))return 1;
 if(!dm_load("MEMORY.TSV")||!verify(dm_find(id)->conv==384))return 1;
 locked=1;keypos=keycount=0;send("\r");memory_edit(id);if(!verify(dm_find(id)->conv==384))return 1;
 keypos=keycount=0;send("e\r\033");memory_details();
 {int item=current_item();char mode[9];MemoryState state;char*extra=strdup(lb_str(lb_entries[item]->extra));
  if(!verify(extra!=NULL&&lb_replace(&lb_entries[item]->extra,"---BOOT---XM,X,CM,C,XMC\n")))return 1;
  putenv("CONFIG=N");if(!verify(game_boot_requirement(item,mode)&&!strcmp(mode,"XM")))return 1;
  memory_state(item,&state);if(!verify(state.marker=='!'&&!strcmp(state.label,"Boot XM")))return 1;
  keypos=keycount=0;send("\r");action(ACT_RUN);if(!verify(!quit_app&&!launch_app&&keypos==1))return 1;
  putenv("CONFIG=XMC");if(!verify(!game_boot_requirement(item,mode)))return 1;
  putenv("CONFIG=XM2");if(!verify(game_boot_requirement(item,mode)))return 1;
  putenv("CONFIG=");if(!verify(game_boot_requirement(item,mode)))return 1;
  lb_replace(&lb_entries[item]->extra,extra);free(extra);
 }
 finish_screen();f=fopen("MEMUI.TXT","w");fputs("MEMORY_UI_OK details,cancel,save,reload,lock,boot-mode-gate\n",f);fclose(f);lb_clear();return 0;
}
