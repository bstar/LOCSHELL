static unsigned long learn_next;
static void learning_page(void){int item=current_item(),k;unsigned long id;DLRecord r;char line[100];
 if(item<0)return;id=lb_identity(lb_entries[item]);
 for(;;){MemoryState state;int have;memory_state(item,&state);main_draw(1);window(2,3,76,19,"Observed memory / learning");
  put(4,5,72,A_PANEL,lb_entries[item]->title);
  sprintf(line,"Profile: %.19s   Boot mode: %.8s",memory_profile(),memory_mode());put(4,6,72,A_PANEL,line);
  have=(state.learned!=DL_FIT_CHANGED&&state.record.id)?(r=state.record,1):dl_latest(id,&r);
  if(have){
   sprintf(line,"Observed allocation peaks: DOS %luK EMS %uK XMS %uK",(unsigned long)(r.stats.base_conv-r.stats.min_conv)/64UL,r.stats.base_ems-r.stats.min_ems,r.stats.base_xms-r.stats.min_xms);put(4,7,72,A_PANEL,line);
   sprintf(line,"Available now: DOS %luK EMS %uK XMS %uK",dm_memory.launch_bytes/1024UL,dm_memory.ems_kb,dm_memory.xms_largest_kb);put(4,8,72,A_PANEL,line);
   sprintf(line,"Observer %u bytes; failures DOS/EMS/XMS %u/%u/%u",r.stats.paras*16,r.stats.fail_dos,r.stats.fail_ems,r.stats.fail_xms);put(4,9,72,A_PANEL,line);
   sprintf(line,"Gameplay + music: %s; current RAM status: %s",dl_confirmation(id,r.signature)==DL_CONFIRMED?"confirmed":r.disposition==DL_REJECTED?"failed":"unverified",state.label);put(4,11,72,A_PANEL,line);
  }else put(4,7,72,A_PANEL,"No learning sessions recorded for this launch configuration.");
  put(4,13,72,A_PANEL,"Peaks are tested budgets; extenders may bypass the allocation monitor.");
  put(4,14,72,A_PANEL,"Confirmation applies to the played session, not every level or save.");
  put(4,16,72,A_PANEL,learn_next==id?"Learning is armed for this game's next Run Selected.":"Ordinary launches have no monitor. L arms one learning run.");
  put(4,18,72,A_PANEL,"L Learn run   C Confirm current setup   R Reset   Esc Back");
  k=event();if(k==-3||k==-4||k==27||k==13)return;
  if(k=='l'||k=='L'){
#ifdef DS_NETWORK_EMBEDDED
   learn_next=id;return;
#else
   info("Learning requires the integrated LOCSHELL.BAT launcher.");
#endif
  }
  if(k=='r'||k=='R'){if(confirm("Reset confirmation and discard any pending learning request?")){if(have)dl_confirm(&r,DL_RESET);remove("C:\\DOSSHELL\\LEARN.REQ");learn_next=0;}}
  if(k=='c'||k=='C'){
   unsigned long signature;
   if(!have){info("Record a learning run first.");continue;}
   if(r.disposition==DL_REJECTED){info("That run failed. Learn a successful run before confirming it.");continue;}
   if(dl_budget(&r,&dm_memory)!=DL_FIT_READY){info("Current RAM does not cover a complete recorded run.");continue;}
   if(!confirm("Confirm that recorded gameplay and music worked with the current setup?"))continue;
   signature=dl_signature(lb_str(lb_entries[item]->run));
   if(!signature){info("Cannot identify this launch configuration. Learn a simple launch first.");continue;}
   r.signature=signature;
   if(!dl_confirm(&r,DL_CONFIRMED))info("Could not save confirmation; prior records retained.");
   main_valid=0;
  }
 }
}
static void learning_return(void){FILE*f;unsigned long id;DLRecord r;int k,choice=2;
 f=fopen("C:\\DOSSHELL\\LEARN.ASK","r");if(!f)return;if(fscanf(f,"%lx",&id)!=1){fclose(f);return;}fclose(f);if(!dl_latest(id,&r))return;
 if(r.disposition!=DL_PARTIAL){remove("C:\\DOSSHELL\\LEARN.ASK");return;}
 for(;;){main_draw(1);window(2,7,76,11,"Learning session returned");
  put(4,9,72,A_PANEL,"Did gameplay and music work correctly during that session?");
  put(4,10,72,A_PANEL,"For games without music, confirm that expected sound worked.");
  button(14,14,10,"Yes",choice==0);button(34,14,10,"No",choice==1);button(54,14,12,"Not sure",choice==2);
  k=event();if(k==K_LEFT||k==K_RIGHT||k==9)choice=(choice+(k==K_LEFT?2:1))%3;
  if(k==-3||k==-4)return; /* Keep LEARN.ASK for the next shell session. */
  if(k==27)choice=2;if(k==27||k==13){
   if(choice!=2){int i;unsigned long current;
    /* Games can save sound/configuration settings during play. The user's
       answer describes the setup just played, not its pre-launch defaults. */
    for(i=0;i<lb_count;i++)if(lb_identity(lb_entries[i])==id){current=dl_signature(lb_str(lb_entries[i]->run));if(current)r.signature=current;break;}
   }
   if(choice==2||dl_confirm(&r,choice==0?DL_CONFIRMED:DL_REJECTED))remove("C:\\DOSSHELL\\LEARN.ASK");main_valid=0;return;
  }
 }
}
