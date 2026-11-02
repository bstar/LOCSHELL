/* Read-only DOS filesystem diagnostics. GPL-3.0-or-later. */
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned long files=0,errors=0,table[256];static int hashing=0;
static unsigned long crc(const char *path){FILE *f;unsigned char b[2048];unsigned n,i;unsigned long c=0xffffffffUL;
 f=fopen(path,"rb");if(!f){errors++;printf("ERROR\t%s\tread\n",path);return 0;}
 while((n=fread(b,1,sizeof(b),f))!=0)for(i=0;i<n;i++)c=table[(c^b[i])&255]^(c>>8);
 if(ferror(f)){errors++;printf("ERROR\t%s\tread\n",path);}fclose(f);return c^0xffffffffUL;}
static void scan(const char *path,unsigned depth){struct find_t f;char pattern[84],child[84];unsigned len=(unsigned)strlen(path);unsigned rc;
 if(depth>32||len>75){printf("ERROR\t%s\tpath-limit\n",path);errors++;return;}
 if(!stricmp(path,"C:\\NET\\JOBS\\LOCK")){printf("SKIP\t%s\tactive-job\n",path);return;}
 strcpy(pattern,path);if(len&&path[len-1]!='\\')strcat(pattern,"\\");strcat(pattern,"*.*");
 printf("D\t%s\n",path);rc=_dos_findfirst(pattern,0x37,&f);
 if(rc){if(rc!=2&&rc!=18){errors++;printf("ERROR\t%s\tfind-%u\n",path,rc);}return;}
 do{if(!strcmp(f.name,".")||!strcmp(f.name,".."))continue;
 if(strlen(path)+strlen(f.name)+2>=sizeof(child)){errors++;printf("ERROR\t%s\tpath-limit\n",path);continue;}
 strcpy(child,path);if(child[strlen(child)-1]!='\\')strcat(child,"\\");strcat(child,f.name);
 if(f.attrib&_A_SUBDIR)scan(child,depth+1);else{files++;if(hashing)printf("F\t%s\t%lu\t%08lX\n",child,f.size,crc(child));else printf("F\t%s\t%lu\n",child,f.size);}
 }while(!_dos_findnext(&f));}
int main(int argc,char **argv){unsigned d,i,j;struct diskfree_t fs;unsigned long c;
 if(argc<2)return 2;
 if(!strcmp(argv[1],"--disks")){for(d=3;d<=6;d++){if(_dos_getdiskfree(d,&fs)){printf("ERROR\t%c:\\\tdisk-unavailable\n",'A'+d-1);errors++;continue;}
 printf("DISK\t%c\t%lu\t%lu\n",'A'+d-1,(unsigned long)fs.total_clusters*fs.sectors_per_cluster*fs.bytes_per_sector,(unsigned long)fs.avail_clusters*fs.sectors_per_cluster*fs.bytes_per_sector);}}
 else if(!strcmp(argv[1],"--scan")||!strcmp(argv[1],"--hash")){hashing=!strcmp(argv[1],"--hash");if(argc<3)return 2;
 if(hashing)for(i=0;i<256;i++){c=i;for(j=0;j<8;j++)c=c&1?(c>>1)^0xedb88320UL:c>>1;table[i]=c;}
 for(i=2;i<(unsigned)argc;i++){if(strlen(argv[i])<3||argv[i][1]!=':'||argv[i][2]!='\\'||strpbrk(argv[i],"*?\t\r\n"))return 2;scan(argv[i],0);}}
 else return 2;
 printf("END\t%lu\t%lu\n",files,errors);return errors?1:0;}
