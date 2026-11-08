#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
DMMemory dm_memory;
char dm_error[100];
/* Stable chunks avoid reserving all 512 records in the executable image.
   Existing record addresses remain valid when an editor adds a requirement. */
#define DM_CHUNK 32
static DMRequirement *requirements[(DM_MAX+DM_CHUNK-1)/DM_CHUNK];
static unsigned count;
static DMRequirement*dm_at(unsigned i){return requirements[i/DM_CHUNK]+i%DM_CHUNK;}
DMRequirement*dm_find(unsigned long id){unsigned i;for(i=0;i<count;i++)if(dm_at(i)->id==id)return dm_at(i);return 0;}
int dm_set(const DMRequirement*r){DMRequirement*p=dm_find(r->id);if(!p){unsigned chunk=count/DM_CHUNK;if(count==DM_MAX)return 0;if(!requirements[chunk]){requirements[chunk]=(DMRequirement*)calloc(DM_CHUNK,sizeof(DMRequirement));if(!requirements[chunk])return 0;}p=dm_at(count++);}*p=*r;return 1;}
static int number(const char*s,unsigned*v,unsigned*known,unsigned bit){char*end;unsigned long n;if(!strcmp(s,"-")){*v=0;return 1;}if(!*s||*s<'0'||*s>'9')return 0;n=strtoul(s,&end,10);if(*end||n>65535UL)return 0;*v=(unsigned)n;*known|=bit;return 1;}
int dm_load(const char*path){FILE*f;char line[256],*p[7],*s,*end;unsigned fields;DMRequirement r;
 count=0;dm_error[0]=0;f=fopen(path,"rt");if(!f)return 1;
 while(fgets(line,sizeof line,f)){if(line[0]=='#'||line[0]=='\n'||line[0]=='\r')continue;if(!strchr(line,'\n')&&!feof(f))goto bad;line[strcspn(line,"\r\n")]=0;s=line;
  fields=0;while(fields<7){p[fields++]=s;s=strchr(s,'|');if(!s)break;*s++=0;}if(s||fields<6)goto bad;memset(&r,0,sizeof r);r.id=strtoul(p[0],&end,16);if(strlen(p[0])!=8||*end||dm_find(r.id))goto bad;
  if(!number(p[1],&r.conv,&r.known,1)||!number(p[2],&r.ems,&r.known,2)||!number(p[3],&r.xms,&r.known,4)||strlen(p[fields-2])!=1||!strchr("UDVLB",p[fields-2][0])||strlen(p[fields-1])>=DM_SOURCE||strchr(p[fields-1],'|'))goto bad;
  if(fields==7&&!number(p[4],&r.total,&r.known,8))goto bad;r.grade=p[fields-2][0];strcpy(r.source,p[fields-1]);if(!dm_set(&r))goto bad;
 }if(ferror(f))goto bad;fclose(f);return 1;
bad:fclose(f);count=0;strcpy(dm_error,"MEMORY.TSV is invalid; memory requirements are unknown.");return 0;
}
int dm_save(const char*path){char tmp[160],bak[160],*dot;FILE*f;unsigned i;int exists,ok;char a[12],b[12],c[12],d[12];
 if(*dm_error||strlen(path)>145)return 0;strcpy(tmp,path);dot=strrchr(tmp,'.');if(!dot)dot=tmp+strlen(tmp);strcpy(dot,".NEW");strcpy(bak,path);dot=strrchr(bak,'.');if(!dot)dot=bak+strlen(bak);strcpy(dot,".BAK");
 f=fopen(tmp,"wt");if(!f)return 0;fputs("# identity|conventional KB|EMS KB|XMS KB|total free KB|U/D/V/L|source\n",f);
 for(i=0;i<count;i++){DMRequirement*r=dm_at(i);strcpy(a,"-");strcpy(b,"-");strcpy(c,"-");strcpy(d,"-");if(r->known&1)sprintf(a,"%u",r->conv);if(r->known&2)sprintf(b,"%u",r->ems);if(r->known&4)sprintf(c,"%u",r->xms);if(r->known&8)sprintf(d,"%u",r->total);fprintf(f,"%08lx|%s|%s|%s|%s|%c|%s\n",r->id,a,b,c,d,r->grade,r->source);}
 ok=!ferror(f);if(fclose(f))ok=0;if(!ok){remove(tmp);return 0;}f=fopen(path,"rb");exists=f!=0;if(f)fclose(f);remove(bak);if(exists&&rename(path,bak))return 0;if(rename(tmp,path)){if(exists)rename(bak,path);return 0;}return 1;
}
char dm_status(const DMRequirement*r,const DMMemory*m){if(!r)return '?';if((r->known&1)&&m->conv_valid&&(unsigned long)r->conv*1024UL>m->launch_bytes)return r->grade=='B'?'?':'-';if((r->known&2)&&m->ems_valid&&r->ems>m->ems_kb)return '-';if((r->known&4)&&m->xms_valid&&r->xms>m->xms_largest_kb)return '-';
 if((r->known&8)&&r->total&&m->conv_valid&&m->ems_valid&&m->xms_valid&&(unsigned long)r->total>m->launch_bytes/1024UL+(unsigned long)m->ems_kb+m->xms_free_kb)return '-';
 if((r->known&7)!=7||((r->known&8)&&r->total)||!m->conv_valid||!m->ems_valid||!m->xms_valid||(r->grade!='D'&&r->grade!='V'&&r->grade!='B')||!r->source[0])return '?';
 if(r->ems&&r->xms)return '?';return '+';
}
