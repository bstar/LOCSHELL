/* Persist DOS find state between bounded, read-only directory batches. */
struct chunk_state {char magic[8];unsigned long generation,read_offset,queue_length,files;unsigned active,done;char directory[80];struct find_t find;};
static struct chunk_state chunk;
static int chunk_run(unsigned limit){FILE *q,*f;char path[84],pattern[84];unsigned count=0,rc,len;long at;
 memset(&chunk,0,sizeof(chunk));f=fopen("CURSOR.BIN","rb");
 if(f){if(fread(&chunk,sizeof(chunk),1,f)!=1||memcmp(chunk.magic,"DOSCH01",8)){fclose(f);return 2;}fclose(f);}
 else{if(!access("CURSOR.BAK",0))return 2;f=fopen("ROOTS.TXT","rb");if(!f)return 2;q=fopen("QUEUE.TXT","wb");if(!q){fclose(f);return 2;}
  while(fgets(path,sizeof(path),f)){path[strcspn(path,"\r\n")]=0;if(strlen(path)<3||path[1]!=':'||path[2]!='\\'||strpbrk(path,"*?\t")){fclose(q);fclose(f);return 2;}fprintf(q,"%s\n",path);}
  if(ferror(f)||fflush(q)){fclose(q);fclose(f);return 1;}chunk.queue_length=ftell(q);fclose(q);fclose(f);memcpy(chunk.magic,"DOSCH01",8);}
 q=fopen("QUEUE.TXT","r+b");if(!q)return 1;
 if(chsize(fileno(q),chunk.queue_length)){fclose(q);return 1;}
 while(count<limit&&!chunk.done){
  if(!chunk.active){fseek(q,chunk.read_offset,SEEK_SET);if(!fgets(path,sizeof(path),q)){if(ferror(q)){fclose(q);return 1;}chunk.done=1;break;}
   chunk.read_offset=ftell(q);path[strcspn(path,"\r\n")]=0;strcpy(chunk.directory,path);len=strlen(path);count++;
   if(!stricmp(path,"C:\\LOCSHELL\\NET\\JOBS\\LOCK")){printf("SKIP\t%s\tactive-job\n",path);continue;}
   printf("D\t%s\n",path);strcpy(pattern,path);if(pattern[len-1]!='\\')strcat(pattern,"\\");strcat(pattern,"*.*");rc=_dos_findfirst(pattern,0x37,&chunk.find);
   if(rc){if(rc!=2&&rc!=18){printf("ERROR\t%s\tfind-%u\n",path,rc);errors++;}continue;}chunk.active=1;continue;
  }
  count++;
  if(strcmp(chunk.find.name,".")&&strcmp(chunk.find.name,"..")){
   len=strlen(chunk.directory);if(len+strlen(chunk.find.name)+2>=80){printf("ERROR\t%s\tpath-limit\n",chunk.directory);errors++;}
   else{strcpy(path,chunk.directory);if(path[len-1]!='\\')strcat(path,"\\");strcat(path,chunk.find.name);
    if(chunk.find.attrib&_A_SUBDIR){fseek(q,0,SEEK_END);if(fprintf(q,"%s\n",path)<0){fclose(q);return 1;}}
    else{printf("F\t%s\t%lu\n",path,chunk.find.size);chunk.files++;}
   }
  }
  rc=_dos_findnext(&chunk.find);if(rc){chunk.active=0;if(rc!=2&&rc!=18){printf("ERROR\t%s\tfindnext-%u\n",chunk.directory,rc);errors++;}}
 }
 if(fflush(q)){fclose(q);return 1;}fseek(q,0,SEEK_END);at=ftell(q);fclose(q);if(at<0)return 1;chunk.queue_length=at;
 if(fflush(stdout)||ferror(stdout))return 1;
 chunk.generation++;f=fopen("CURSOR.NEW","wb");if(!f)return 1;
 if(fwrite(&chunk,sizeof(chunk),1,f)!=1){fclose(f);return 1;}if(fclose(f))return 1;
 remove("CURSOR.BAK");if(!access("CURSOR.BIN",0)&&rename("CURSOR.BIN","CURSOR.BAK"))return 1;
 if(rename("CURSOR.NEW","CURSOR.BIN")){rename("CURSOR.BAK","CURSOR.BIN");return 1;}
 printf("CHUNK\t%lu\t%u\t%lu\t%u\t%lu\n",chunk.generation,chunk.done,chunk.files,count,errors);return errors?1:0;}
