#include "launch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) if(!(x)){printf("HANDOFF_FAIL %d\n",__LINE__);return 1;}
int main(int argc,char**argv){FILE*f;char text[100];remove("C:\\NET\\AGENT\\LOCAL.REQ");
 f=fopen("C:\\DOSSHELL\\RUN.BAT","w");fputs("@ECHO OFF\nD:\nCD \\GAMES\\DEMO\\ONE\nDEMO.COM\n",f);fclose(f);
 if(argc>1){if(!strcmp(argv[1],"TOOL"))return lp_prepare("TOOL",9)&&lp_publish(0)?0:4;if(!strcmp(argv[1],"SETUP"))return lp_prepare("SETUP",0)&&lp_publish(0)?0:4; if(!strcmp(argv[1],"MISSING")){f=fopen("C:\\DOSSHELL\\RUN.BAT","w");fputs("D:\nCD \\ABSENT\nNO.COM\n",f);fclose(f);}if(!strcmp(argv[1],"RESTART"))return lp_publish(1)?0:4;return lp_prepare("RUN",0x12345678UL)&&lp_publish(0)?0:4;}
 CHECK(lp_prepare("RUN",0x12345678UL));CHECK(lp_publish(0));CHECK(!lp_publish(0));f=fopen("C:\\NET\\AGENT\\LOCAL.REQ","r");CHECK(f!=0);fgets(text,sizeof text,f);fclose(f);CHECK(strstr(text,"LOC3 RUN 12345678")!=0);
 remove("C:\\NET\\AGENT\\LOCAL.REQ");CHECK(lp_publish(1));f=fopen("C:\\NET\\AGENT\\LOCAL.REQ","r");fgets(text,sizeof text,f);fclose(f);CHECK(strstr(text,"RESTART")!=0);remove("C:\\NET\\AGENT\\LOCAL.REQ");puts("HANDOFF_MODEL_OK typed requests and collision protection");return 0;}
