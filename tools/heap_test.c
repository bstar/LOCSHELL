/* DOS regression: a valid far heap must pass, damaged metadata must fail. */
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <dos.h>
static int failures;
static int valid(void){int s=_heapchk();return s==_HEAPOK||s==_HEAPEMPTY;}
int main(void){void far *p[4];unsigned i,old;unsigned short far *tag,*header;
 struct _heapinfo h;int status,entries=0,node_status,header_status;FILE *f;
 if(!valid())failures++;
 for(i=0;i<4;i++){p[i]=_fmalloc(i==0?100:50000U);if(!p[i])return 2;if(!valid())failures++;}
 h._pentry=0;while((status=_fheapwalk(&h))==_HEAPOK)entries++;
 if(status!=_HEAPEND||entries<4)failures++;
 tag=(unsigned short far*)MK_FP(FP_SEG(p[0]),FP_OFF(p[0])-2);old=*tag;*tag=0;
 node_status=_heapchk();*tag=old;if(node_status!=_HEAPBADNODE||!valid())failures++;
 header=(unsigned short far*)MK_FP(FP_SEG(p[0]),16);old=*header;*header=2;
 header_status=_heapchk();*header=old;if(header_status==_HEAPOK||!valid())failures++;
 for(i=0;i<4;i++){_ffree(p[3-i]);if(!valid())failures++;}
 f=fopen("HEAPTEST.LOG","w");if(!f)return 3;
 fprintf(f,"failures=%d walk=%d entries=%d damaged_node=%d damaged_header=%d\n",failures,status,entries,node_status,header_status);fclose(f);
 return failures?1:0;
}
