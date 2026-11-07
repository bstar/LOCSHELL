/* Included after the existing field editor helpers. */
static void memory_edit(unsigned long id){DMRequirement r,*old=dm_find(id);char values[6][DM_SOURCE],line[100];unsigned pos=0;int field=0,k,i;const char*labels[]={"Conventional free KB","EMS free KB","XMS contiguous KB","Total free KB (all memory types)","Basis (D=min, B=budget, V=tested)","Source / configuration evidence"};
 if(locked){info("Unlock DOS SHELL to edit memory requirements.");return;}
 if(*dm_error){info(dm_error);return;}
 memset(&r,0,sizeof r);if(old)r=*old;r.id=id;
 for(i=0;i<4;i++)strcpy(values[i],"-");if(r.known&1)sprintf(values[0],"%u",r.conv);if(r.known&2)sprintf(values[1],"%u",r.ems);if(r.known&4)sprintf(values[2],"%u",r.xms);
 if(r.known&8)sprintf(values[3],"%u",r.total);
 strcpy(values[4],r.grade=='V'?"V":r.grade=='B'?"B":"D");strcpy(values[5],r.source);
 for(;;){main_draw(1);window(5,3,70,20,"Memory requirements");put(7,5,66,A_PANEL,"KB required at launch. Use - for unknown; 0 means none.");
  for(i=0;i<6;i++){put(7,7+i*2,34,A_PANEL,labels[i]);input_draw(42,7+i*2,30,1,values[i],pos,field==i);}
  put(7,19,66,A_PANEL,"Include music memory. Unknown audio allocation: use -.");
  put(7,20,66,A_PANEL,"Enter: next field / save   Esc: cancel   Tab: next field");
  k=event();if(k==27)return;
  if(k==9||k==13){if(k==13&&field==5)break;field=(field+1)%6;pos=0;}
  else if(k==K_SHIFT_TAB){field=(field+5)%6;pos=0;}
  else if(k!=-2)input_key(values[field],field==5?DM_SOURCE:8,&pos,k,0);
 }
 r.known=0;
 for(i=0;i<4;i++){char*end;unsigned long v;unsigned*dest=i==0?&r.conv:i==1?&r.ems:i==2?&r.xms:&r.total;if(!strcmp(values[i],"-")){*dest=0;continue;}v=strtoul(values[i],&end,10);if(!values[i][0]||values[i][0]<'0'||values[i][0]>'9'||*end||v>65535UL){info("Enter KB from 0 to 65535, or - for unknown.");return;}*dest=(unsigned)v;r.known|=1<<i;}
 if((strcmp(values[4],"D")&&strcmp(values[4],"V")&&strcmp(values[4],"B"))||!values[5][0]||strchr(values[5],'|')){info("Specify D, B or V and a source without the | character.");return;}r.grade=values[4][0];strcpy(r.source,values[5]);
 if(!dm_set(&r)||!dm_save("MEMORY.TSV")){dm_load("MEMORY.TSV");info("Could not save memory requirements; prior file retained.");}else strcpy(notice,"Memory requirement saved.");
 rebuild(); main_valid=0;
 (void)line;
}
static void memory_details(void){int item=current_item(),k;char l[11][100],num[3][12];DMRequirement*r;MemoryState state;unsigned long id;unsigned i;
 if(item<0){info("Select a program to inspect its memory requirements.");return;}id=lb_identity(lb_entries[item]);
 for(;;){DLStats*t;unsigned long conv;unsigned ems,xms;
  r=dm_find(id);memory_state(item,&state);t=&state.record.stats;
  sprintf(l[0],"%.65s",lb_entries[item]->title);
  sprintf(l[1],"Current profile: %.19s   Boot mode: %.8s",memory_profile(),memory_mode());
  sprintf(l[2],"RAM status: %s%s",state.label,state.review?" (setup review needed)":"");
  if(state.record.id){
   conv=dl_conv_budget(&state.record);
   ems=t->base_ems>=t->min_ems?t->base_ems-t->min_ems:0;xms=t->base_xms>=t->min_xms?t->base_xms-t->min_xms:0;
   sprintf(l[3],"DOS: budget %luK; total free %luK, largest block %luK",(conv+1023UL)/1024UL,dm_memory.conv_total_bytes/1024UL,dm_memory.launch_bytes/1024UL);
   sprintf(l[4],"EMS: recorded %uK; currently free %uK",ems,dm_memory.ems_kb);
   sprintf(l[5],"XMS: recorded %uK; largest free %uK, total %uK",xms,dm_memory.xms_largest_kb,dm_memory.xms_free_kb);
   strcpy(l[6],state.review?"RAM covers the prior run; this setup is not yet confirmed.":state.learned==DL_FIT_READY?"Basis: confirmed gameplay + music; this setup's RAM fits.":state.learned==DL_FIT_LESS_RAM?"Less RAM than the confirmed run; success is not established.":state.learned==DL_FIT_CHANGED?"Game, profile or sound setup differs from the recorded run.":state.learned==DL_FIT_FAILED?"The recorded gameplay or music failed on this setup.":state.learned==DL_FIT_CONFIRM?"Confirm gameplay + music after a learning run (F7).":"Recording is incomplete; use F7 to learn a complete session.");
   sprintf(l[7],"Recorded allocation probes that failed: DOS %u EMS %u XMS %u",t->fail_dos,t->fail_ems,t->fail_xms);
  }else{
   for(i=0;i<3;i++)strcpy(num[i],"unknown");if(r){if(r->known&1)sprintf(num[0],"%uK",r->conv);if(r->known&2)sprintf(num[1],"%uK",r->ems);if(r->known&4)sprintf(num[2],"%uK",r->xms);}
   sprintf(l[3],"DOS: needs %s; available after shell exits %luK",num[0],dm_memory.launch_bytes/1024UL);
   sprintf(l[4],"EMS: needs %s; currently free %uK",num[1],dm_memory.ems_kb);
   sprintf(l[5],"XMS: needs %s; largest free %uK, total %uK",num[2],dm_memory.xms_largest_kb,dm_memory.xms_free_kb);
   strcpy(l[6],r?"Basis: catalogue requirements; no confirmed recording yet.":"No requirements or confirmed recording; use F7 to learn.");
   sprintf(l[7],"Source: %.62s",r?r->source:"");
  }
  {char mode[9];if(game_boot_requirement(item,mode)){sprintf(l[6],"This game needs boot mode %s; current mode is %.8s.",mode,memory_mode());sprintf(l[7],"Restart and select %.19s, then boot mode %s.",memory_profile(),mode);}}
  strcpy(l[8],"Budget includes the monitor reserve; it is not an exact minimum.");
  strcpy(l[9],"Available DOS includes memory released by shell/server exit.");
  strcpy(l[10],"E Edit requirements   L Recordings   F5 Refresh   Esc Close");
  main_draw(1);window(3,4,74,16,"Current launch memory");for(i=0;i<11;i++)put(5,6+i,70,A_PANEL,l[i]);if(*dm_error)put(5,17,70,0x4f,dm_error);
  k=event();if(k==-3||k==-4||k==27||k==13)return;if(k=='l'||k=='L')learning_page();else if(k=='e'||k=='E')memory_edit(id);else if(k==319){dm_query();dl_index();main_valid=0;}
 }
}
