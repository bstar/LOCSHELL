#include <dos.h>
#include <stdio.h>
int main(void){unsigned long start=*((unsigned long far *)MK_FP(0x40,0x6c));volatile unsigned long count=0;unsigned short far *v=(unsigned short far *)MK_FP(0xb800,1600);char *s="FOREGROUND BUSY: observer must work without DOS/network servicing";unsigned i;FILE *f=fopen("C:\\BUSY.RUN","w");fputs("running",f);fclose(f);for(i=0;s[i];i++)v[i]=0x0f00|s[i];while(*((unsigned long far *)MK_FP(0x40,0x6c))-start<220)count++;f=fopen("C:\\BUSY.DON","w");fprintf(f,"%lu",count);fclose(f);return 0;}
