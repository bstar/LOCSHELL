/* Compare the assembly edge renderer against scalar major-axis Bresenham.
 * Includes every octant, degenerate edges, screen borders and rejected inputs.
 * GPL-3.0-or-later. */
#include "../management/mathsave.c"
#include <assert.h>
#include <stdio.h>

static void reference(unsigned char far *image,int x,int y,int endx,int endy,unsigned char color)
{
    int dx=abs(endx-x),dy=abs(endy-y),sx=x<endx?1:-1,sy=y<endy?1:-1;
    int error,n;
    if((unsigned)x>=320||(unsigned)y>=200||(unsigned)endx>=320||(unsigned)endy>=200)return;
    if(dx>=dy){
        error=dx/2;
        for(n=0;n<=dx;n++){
            image[(unsigned)y*320+(unsigned)x]=color;x+=sx;error-=dy;
            if(error<0){error+=dx;y+=sy;}
        }
    }else{
        error=dy/2;
        for(n=0;n<=dy;n++){
            image[(unsigned)y*320+(unsigned)x]=color;y+=sy;error-=dx;
            if(error<0){error+=dy;x+=sx;}
        }
    }
}

int main(void)
{
    static const int edges[][4]={
        {0,0,0,0},{319,199,319,199},{0,0,319,0},{319,199,0,199},
        {0,0,0,199},{319,199,319,0},{0,0,199,199},{199,199,0,0},
        {319,0,120,199},{0,199,199,0},{-1,0,319,199},{0,-1,319,199},
        {0,0,320,199},{0,0,319,200},{-32768,0,32767,199}
    };
    unsigned n,i,random=971;unsigned char color;
    unsigned char far *actual=_fmalloc(64032u),far *expected=_fmalloc(64032u);
    FILE *pass;
    assert(actual&&expected);freopen("RASTER.LOG","w",stderr);
    for(n=0;n<1039;n++){
        _fmemset(actual,0x6d,64032u);_fmemset(expected,0x6d,64032u);
        _fmemset(actual+16,0,64000u);_fmemset(expected+16,0,64000u);
        if(n<15){px[0]=edges[n][0];py[0]=edges[n][1];px[1]=edges[n][2];py[1]=edges[n][3];}
        else{
            random=random*25173u+13849u;px[0]=random%320;
            random=random*25173u+13849u;py[0]=random%200;
            random=random*25173u+13849u;px[1]=random%320;
            random=random*25173u+13849u;py[1]=random%200;
        }
        color=(unsigned char)(1+n%255);msImage=actual+16;
        line(0,1,color);reference(expected+16,px[0],py[0],px[1],py[1],color);
        assert(!_fmemcmp(actual,expected,64032u));
        for(i=0;i<16;i++)assert(actual[i]==0x6d&&actual[64016u+i]==0x6d);
    }
    _ffree(actual);_ffree(expected);msImage=NULL;
    pass=fopen("RASTPASS.TXT","w");assert(pass);
    fputs("PASS: 1039 edge cases match scalar reference; octants, borders, zero-length and rejected edges; allocation guards intact\n",pass);
    fclose(pass);return 0;
}
