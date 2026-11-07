/* Bounded, hardware-x87 mathematical screensavers. GPL-3.0-or-later.
 * This translation unit alone is compiled with -fpi87 -fp3. Never call it
 * without a detected 387. No timer/interrupt hooks or network ownership. */
#include <dos.h>
#include <conio.h>
#include <malloc.h>
#include <memory.h>
#include <stdlib.h>
#include "MATHSAVE.H"

#define WIDTH 320u
#define HEIGHT 200u
#define RINGS 14u
#define SIDES 8u
#define POINTS (RINGS * SIDES)
#define PI 3.14159265358979323846

unsigned char far *msImage = NULL;
unsigned long msFrames = 0;
unsigned long msLastSeed = 0;
unsigned long msElapsedTicks = 0;
unsigned long msFirstFrameTicks = 0;
unsigned msLastKind = 0;
unsigned long msBounces = 0;
static unsigned long lastTick;
static unsigned msKind, stage, item;
static double angle;
static double poseY, poseZ, wavePhase;
static float ca, sa, cb, sb, cc, sc;
static double ringU[RINGS];
/* Screen geometry only needs single-precision storage. The 387 still performs
 * arithmetic in its registers; 32-bit loads halve traffic on a 386SX bus. */
static float ringSin[RINGS], ringCos[RINGS], sideSin[SIDES], sideCos[SIDES];
static float radiusAt[RINGS], heightAt[RINGS], twistSin[RINGS], twistCos[RINGS];
static float basisX[RINGS], basisY[RINGS], basisZ[RINGS], matrix[9];
static unsigned oldLeft, oldTop, oldRight, oldBottom;
static int newLeft, newTop, newRight, newBottom;
static unsigned copyLeft, copyTop, copyRight, copyBottom, clearLimit;
static int centerX, centerY, velocityX, velocityY;
static int px[POINTS], py[POINTS];
static unsigned char pc[POINTS];
static double cx, cy, span, cr, ci, zr, zi;
static unsigned mx, my, block, iteration, pixelActive, holding, zoom;
static double mandelX[WIDTH],mandelY[HEIGHT];
static unsigned char analyticX[WIDTH],analyticY[HEIGHT];
static unsigned coordinatesReady,coordinateIndex;
static unsigned long holdStart;

static void sincos387(double value, float *s, float *c)
{
    double sineValue, cosineValue;
    _asm {
        fld value
        fsincos
        fstp cosineValue
        fstp sineValue
    }
    *s = sineValue; *c = cosineValue;
}

/* Use the 387 transcendental instructions explicitly. Arguments stay small;
 * the 8087/287-compatible library trig implementation is not used. */
static double sine(double value)
{
    _asm {
        fld value
        fsin
        fstp value
    }
    return value;
}

static unsigned long ticks(void)
{
    return *((unsigned long far *)MK_FP(0x40, 0x6c));
}

static void elapsed(void)
{
    unsigned long now=ticks();
    msElapsedTicks+=now>=lastTick?now-lastTick:0x1800b0UL-lastTick+now;
    lastTick=now;
}

static void palette(void)
{
    static const unsigned char anchors[5][3] = {
        {0, 54, 63}, {29, 10, 60}, {60, 8, 40}, {63, 44, 8}, {0, 54, 63}
    };
    unsigned i, component, segment, offset;
    outp(0x3c8, 32);
    for (i = 0; i < 224; ++i) {
        segment = i / 56; offset = i % 56;
        for (component = 0; component < 3; ++component)
            outp(0x3c9, anchors[segment][component] +
                 ((int)anchors[segment+1][component] - anchors[segment][component]) * (int)offset / 56);
    }
}

/* Combined edge setup and major-axis Bresenham. The register ABI avoids the
 * old C-to-assembly argument marshalling for every edge. Preserve caller CX,
 * SI, DI, BP and ES, leave interrupt/direction flags alone, and draw precisely
 * the same pixels in the same order as the scalar major-axis algorithm. */
static void line(unsigned a, unsigned b, unsigned char color);
#pragma aux line = \
    "push bp" \
    "mov bp,sp" \
    "push es" \
    "push cx" \
    "push si" \
    "push di" \
    "push bx" \
    "mov si,ax" \
    "add si,si" \
    "mov di,ss:px[si]" \
    "mov ax,ss:py[si]" \
    "cmp di,320" \
    "jae line_done" \
    "cmp ax,200" \
    "jae line_done" \
    "add dx,dx" \
    "mov si,dx" \
    "mov cx,ss:px[si]" \
    "mov dx,ss:py[si]" \
    "cmp cx,320" \
    "jae line_done" \
    "cmp dx,200" \
    "jae line_done" \
    "mov si,ax" \
    "imul ax,ax,320" \
    "add ax,di" \
    "push ax" \
    "sub cx,di" \
    "sub dx,si" \
    "mov ax,1" \
    "test cx,cx" \
    "jge line_dx_ready" \
    "neg cx" \
    "neg ax" \
    "line_dx_ready:" \
    "push ax" \
    "mov ax,320" \
    "test dx,dx" \
    "jge line_dy_ready" \
    "neg dx" \
    "neg ax" \
    "line_dy_ready:" \
    "push ax" \
    "cmp cx,dx" \
    "jae line_x_major" \
    "xchg cx,dx" \
    "mov si,ss:[bp-16]" \
    "mov ax,ss:[bp-14]" \
    "jmp line_setup" \
    "line_x_major:" \
    "mov si,ss:[bp-14]" \
    "mov ax,ss:[bp-16]" \
    "line_setup:" \
    "push ax" \
    "push cx" \
    "mov bx,cx" \
    "shr bx,1" \
    "inc cx" \
    "les di,ss:msImage" \
    "add di,ss:[bp-12]" \
    "mov ax,ss:[bp-10]" \
    "line_pixel:" \
    "mov es:[di],al" \
    "add di,si" \
    "sub bx,dx" \
    "jge line_straight" \
    "add bx,ss:[bp-20]" \
    "add di,ss:[bp-18]" \
    "line_straight:" \
    "loop line_pixel" \
    "line_done:" \
    "lea sp,ss:[bp-8]" \
    "pop di" \
    "pop si" \
    "pop cx" \
    "pop es" \
    "pop bp" \
    parm [ax] [dx] [bl] modify [ax bx dx];

static void sculpture(unsigned char far *video)
{
    unsigned end, i, ring, side, row, control, truncateControl, next;
    int shiftX, shiftY;
    double wave;
    float cv, sv, radial, vertical, x, y, z, scale;
    if (stage == 4) {
        /* Static basis angles are computed once with the actual 387. */
        if (item < RINGS) {
            ringU[item] = item*(2.0*PI/RINGS);
            sincos387(ringU[item], &ringSin[item], &ringCos[item]);
        } else {
            side = item-RINGS;
            sincos387(side*(2.0*PI/SIDES), &sideSin[side], &sideCos[side]);
        }
        if (++item == RINGS+SIDES) { item = 0; stage = 5; }
        return;
    }
    if (stage == 5) {
        _fmemset(msImage+item*WIDTH, 0, 8u*WIDTH);
        item += 8;
        if (item == HEIGHT) { item = stage = 0; }
        return;
    }
    if (stage == 0) {
        if (!item) {
            sincos387(angle, &sa, &ca);
            sincos387(angle*.63+poseY, &sb, &cb);
            sincos387(angle*.31+poseZ, &sc, &cc);
            matrix[0]=cc*cb; matrix[1]=cc*sb*sa-sc*ca; matrix[2]=cc*sb*ca+sc*sa;
            matrix[3]=sc*cb; matrix[4]=sc*sb*sa+cc*ca; matrix[5]=sc*sb*ca-cc*sa;
            matrix[6]=-sb; matrix[7]=cb*sa; matrix[8]=cb*ca;
            newLeft=WIDTH; newTop=HEIGHT; newRight=newBottom=0;
            clearLimit=RINGS*8u;
            if(oldBottom-oldTop>clearLimit)clearLimit=(oldBottom-oldTop+7u)&~7u;
        }
        /* Each clear slice also prepares one ring, sharing its changing wave,
         * radius and rotated basis across all eight vertices. */
        ring = item/8;
        if (ring < RINGS) {
            wave=sine(2.0*ringU[ring]+angle+wavePhase);
            heightAt[ring]=.12*wave;
            radiusAt[ring]=.34+.10*sine(3.0*ringU[ring]+angle*.8+wavePhase);
            sincos387(.35*wave, &twistSin[ring], &twistCos[ring]);
            basisX[ring]=matrix[0]*ringCos[ring]+matrix[1]*ringSin[ring];
            basisY[ring]=matrix[3]*ringCos[ring]+matrix[4]*ringSin[ring];
            basisZ[ring]=matrix[6]*ringCos[ring]+matrix[7]*ringSin[ring];
        }
        end = oldTop+item+8;
        if (end > oldBottom) end=oldBottom;
        for(row=oldTop+item;row<end;row++)
            _fmemset(msImage+row*WIDTH+oldLeft,0,oldRight-oldLeft);
        item += 8;
        if (item >= clearLimit) {
            item = 0; stage = 1;
        }
    } else if (stage == 1) {
        /* -zro emits direct FISTP conversions. Select C's truncation mode once
         * for this bounded batch, then restore the caller's control word. */
        _asm { fstcw control }
        truncateControl=control|0x0c00;
        _asm { fldcw truncateControl }
        end = item + 8;
        if (end > POINTS) end = POINTS;
        for (i = item; i < end; ++i) {
            ring = i / SIDES; side = i % SIDES;
            cv = sideCos[side]*twistCos[ring]-sideSin[side]*twistSin[ring];
            sv = sideSin[side]*twistCos[ring]+sideCos[side]*twistSin[ring];
            radial = 1.0f+radiusAt[ring]*cv;
            vertical = radiusAt[ring]*sv+heightAt[ring];
            x = radial*basisX[ring]+vertical*matrix[2];
            y = radial*basisY[ring]+vertical*matrix[5];
            z = radial*basisZ[ring]+vertical*matrix[8];
            scale = 1.0f/(4.0f+z);
            px[i] = centerX + (int)(x*205.0f*scale);
            py[i] = centerY + (int)(y*172.0f*scale);
            if (px[i]<newLeft) newLeft=px[i];
            if (px[i]+1>newRight) newRight=px[i]+1;
            if (py[i]<newTop) newTop=py[i];
            if (py[i]+1>newBottom) newBottom=py[i]+1;
            pc[i] = (unsigned char)(32u + (ring*(168u/RINGS) + (unsigned)(int)((1.5f-z)*24.0f)) % 224u);
        }
        _asm { fldcw control }
        item = end;
        if (item == POINTS) {
            /* Reflect against the actual rotated mesh bounds. Correct all
             * projected points before rasterization, keeping every write in
             * the viewport even while the changing surface expands. */
            shiftX=shiftY=0;
            if(newLeft<0){shiftX=-newLeft;velocityX=abs(velocityX);}
            else if(newRight>320){shiftX=320-newRight;velocityX=-abs(velocityX);}
            if(newTop<0){shiftY=-newTop;velocityY=abs(velocityY);}
            else if(newBottom>200){shiftY=200-newBottom;velocityY=-abs(velocityY);}
            if(shiftX||shiftY){
                for(i=0;i<POINTS;i++){px[i]+=shiftX;py[i]+=shiftY;}
                centerX+=shiftX;centerY+=shiftY;
                newLeft+=shiftX;newRight+=shiftX;newTop+=shiftY;newBottom+=shiftY;
                ++msBounces;
            }
            item = 0; stage = 2;
        }
    } else if (stage == 2) {
        end = item + 8;
        if (end > POINTS) end = POINTS;
        for (i = item; i < end; ++i) {
            line(i, (i&~(SIDES-1u))|((i+1u)&(SIDES-1u)), pc[i]);
            next=i+SIDES;if(next>=POINTS)next-=POINTS;
            line(i, next, pc[i]);
        }
        item = end;
        if (item == POINTS) {
            copyLeft=newLeft; copyRight=newRight; copyTop=newTop; copyBottom=newBottom;
            if(oldRight>oldLeft){
                if(oldLeft<copyLeft)copyLeft=oldLeft;
                if(oldRight>copyRight)copyRight=oldRight;
                if(oldTop<copyTop)copyTop=oldTop;
                if(oldBottom>copyBottom)copyBottom=oldBottom;
            }
            item = copyTop; stage = 3;
        }
    } else {
        end=item+8;if(end>copyBottom)end=copyBottom;
        for(row=item;row<end;row++)
            _fmemcpy(video+row*WIDTH+copyLeft,msImage+row*WIDTH+copyLeft,copyRight-copyLeft);
        item=end;
        if (item == copyBottom) {
            oldLeft=newLeft;oldRight=newRight;oldTop=newTop;oldBottom=newBottom;
            item = stage = 0; ++msFrames;
            centerX+=velocityX;centerY+=velocityY;
            angle += .065;
            if (angle > 200.0*PI) angle -= 200.0*PI;
        }
    }
}

static void mandelPixel(unsigned char far *video, unsigned char color)
{
    unsigned x, y, offset;
    for (y = my; y < my+block; ++y) {
        offset = y*WIDTH+mx;
        for (x = 0; x < block; ++x) {
            msImage[offset+x] = color;
            video[offset+x] = color;
        }
    }
    mx += block;
    if (mx == WIDTH) { mx = 0; my += block; }
    if (my == HEIGHT) {
        my = 0;
        if (block > 1) {block /= 2;coordinatesReady=0;coordinateIndex=0;}
        else {
            ++msFrames;
            if(msFrames==1){elapsed();msFirstFrameTicks=msElapsedTicks;}
            holding = 1; holdStart = ticks();
        }
    }
    pixelActive = 0;
}

static double orbitLimit=4.0;
/* Keep zi/zr/ci/cr on the 387 stack throughout a bounded orbit batch. Explicit
 * qword stores retain every rounding point of the original scalar loop:
 * x2, y2, next real and next imaginary all round to double each iteration.
 * Return remaining work in the low byte, bit8 when this pixel is complete. */
static unsigned mandelOrbitMemory(unsigned budget);
#pragma aux mandelOrbitMemory = \
    "push bp" \
    "mov bp,sp" \
    "sub sp,32" \
    "mov bx,ss:iteration" \
    "fld qword ptr ss:cr" \
    "fld qword ptr ss:ci" \
    "fld qword ptr ss:zr" \
    "fld qword ptr ss:zi" \
    "orbit_again:" \
    "fld st(1)" \
    "fmul st,st" \
    "fstp qword ptr ss:[bp-8]" \
    "fld st(0)" \
    "fmul st,st" \
    "fstp qword ptr ss:[bp-16]" \
    "fld qword ptr ss:[bp-8]" \
    "fadd qword ptr ss:[bp-16]" \
    "fcomp qword ptr ss:orbitLimit" \
    "fnstsw ax" \
    "sahf" \
    "ja orbit_finished" \
    "cmp bx,128" \
    "je orbit_finished" \
    "fld qword ptr ss:[bp-8]" \
    "fsub qword ptr ss:[bp-16]" \
    "fadd st,st(4)" \
    "fstp qword ptr ss:[bp-24]" \
    "fld st(1)" \
    "fadd st,st" \
    "fmul st,st(1)" \
    "fadd st,st(3)" \
    "fstp qword ptr ss:[bp-32]" \
    "fstp st(0)" \
    "fstp st(0)" \
    "fld qword ptr ss:[bp-24]" \
    "fld qword ptr ss:[bp-32]" \
    "inc bx" \
    "dec cx" \
    "jnz orbit_again" \
    "xor dx,dx" \
    "jmp orbit_save" \
    "orbit_finished:" \
    "dec cx" \
    "mov dx,0x100" \
    "orbit_save:" \
    "fstp qword ptr ss:zi" \
    "fstp qword ptr ss:zr" \
    "fstp st(0)" \
    "fstp st(0)" \
    "mov ss:iteration,bx" \
    "mov ax,cx" \
    "or ax,dx" \
    "mov sp,bp" \
    "pop bp" \
    parm [cx] value [ax] modify [ax bx cx dx __8087];

/* PC64 computes the original products and extended intermediate sums; PC53
 * then reproduces each qword rounding, including double-rounding edge cases.
 * The escape comparison retains PC64. This removes per-iteration
 * qword RAM spills. Restore the incoming control word before every return. */
static unsigned mandelOrbitRegisters(unsigned budget);
#pragma aux mandelOrbitRegisters = \
    "push bp" \
    "mov bp,sp" \
    "sub sp,6" \
    "fstcw word ptr ss:[bp-2]" \
    "mov ax,ss:[bp-2]" \
    "or ax,0x0300" \
    "mov ss:[bp-4],ax" \
    "and ax,0xfeff" \
    "mov ss:[bp-6],ax" \
    "fldcw word ptr ss:[bp-4]" \
    "mov bx,ss:iteration" \
    "fld qword ptr ss:cr" \
    "fld qword ptr ss:ci" \
    "fld qword ptr ss:zr" \
    "fld qword ptr ss:zi" \
    "register_orbit_again:" \
    "fld st(1)" \
    "fmul st,st" \
    "fld st(1)" \
    "fmul st,st" \
    "fldcw word ptr ss:[bp-6]" \
    "fld1" \
    "fmul st(1),st" \
    "fmul st(2),st" \
    "fstp st(0)" \
    "fldcw word ptr ss:[bp-4]" \
    "fld st(1)" \
    "fadd st,st(1)" \
    "fcomp qword ptr ss:orbitLimit" \
    "fnstsw ax" \
    "sahf" \
    "ja register_orbit_finished" \
    "cmp bx,128" \
    "je register_orbit_finished" \
    "fxch st(1)" \
    "fsub st,st(1)" \
    "fadd st,st(5)" \
    "fstp st(1)" \
    "fld st(2)" \
    "fadd st,st" \
    "fmul st,st(2)" \
    "fadd st,st(4)" \
    "fstp st(2)" \
    "fstp st(2)" \
    "fldcw word ptr ss:[bp-6]" \
    "fld1" \
    "fmul st(1),st" \
    "fmul st(2),st" \
    "fstp st(0)" \
    "fldcw word ptr ss:[bp-4]" \
    "inc bx" \
    "dec cx" \
    "jnz register_orbit_again" \
    "xor dx,dx" \
    "jmp register_orbit_save" \
    "register_orbit_finished:" \
    "fstp st(0)" \
    "fstp st(0)" \
    "dec cx" \
    "mov dx,0x100" \
    "register_orbit_save:" \
    "fstp qword ptr ss:zi" \
    "fstp qword ptr ss:zr" \
    "fstp st(0)" \
    "fstp st(0)" \
    "fldcw word ptr ss:[bp-2]" \
    "mov ss:iteration,bx" \
    "mov ax,cx" \
    "or ax,dx" \
    "mov sp,bp" \
    "pop bp" \
    parm [cx] value [ax] modify [ax bx cx dx __8087];
static unsigned registerOrbits=0;
static unsigned registerCoordinatesSafe=1;

static int orbitCoordinateNormal(double value)
{
    /* PC rounding retains the extended exponent. Tiny nonzero coordinates
     * use explicit qword stores to reproduce subnormal behavior as well. */
    return value==0.0||value>=1.e-100||value<=-1.e-100;
}

static unsigned mandelBatch(unsigned budget)
{
    return registerOrbits&&registerCoordinatesSafe?
        mandelOrbitRegisters(budget):mandelOrbitMemory(budget);
}

/* Coordinates repeat throughout each progressive pass. Generate the same
 * expression once per sampled column/row, in bounded batches. Conservative
 * analytic bounds omit tests only where both existing interior tests must
 * fail; all other points retain the original exact inequalities. */
static void mandelCoordinates(void)
{
    unsigned n,index;
    double value;
    if(!coordinateIndex)registerCoordinatesSafe=1;
    for(n=0;n<16&&coordinateIndex<WIDTH+HEIGHT;n++){
        if(coordinateIndex<WIDTH){
            index=coordinateIndex;
            value=cx+(((double)index+block*.5)/320.0-.5)*span;
            mandelX[index]=value;
            analyticX[index]=value>=-1.5&&value<=.5;
        }else{
            index=coordinateIndex-WIDTH;
            value=cy+(((double)index+block*.5)/200.0-.5)*span*.75;
            mandelY[index]=value;
            analyticY[index]=value>=-.75&&value<=.75;
        }
        if(!orbitCoordinateNormal(value))registerCoordinatesSafe=0;
        coordinateIndex+=block;
    }
    if(coordinateIndex==WIDTH+HEIGHT)coordinatesReady=1;
}

static void mandelbrot(unsigned char far *video)
{
    /* Both iteration count and completed pixels are bounded per server poll.
     * Carry difficult pixels across calls instead of blocking on an interior. */
    unsigned budget = 64, pixels = 0, result;
    double q, x;
    if (holding) {
        unsigned long now = ticks();
        if ((now >= holdStart ? now-holdStart : 0x1800b0UL-holdStart+now) < 73UL) return;
        if (++zoom == 18) { zoom = 0; cx = -.5; cy = 0.0; span = 3.4; }
        else {
            cx = -.743643887037151 + (cx+.743643887037151)*.67;
            cy = .131825904205330 + (cy-.131825904205330)*.67;
            span *= .67;
        }
        holding = 0; block = 4; mx = my = 0;
        coordinatesReady=0;coordinateIndex=0;
    }
    if(!coordinatesReady){mandelCoordinates();return;}
    while (budget && pixels < 8 && !holding && coordinatesReady) {
        if (!pixelActive) {
            cr=mandelX[mx];ci=mandelY[my];
            zr = zi = 0.0; iteration = 0; pixelActive = 1;
            /* Exact analytic tests for the main cardioid and period-2 bulb. */
            if(analyticX[mx]&&analyticY[my]){
                x = cr-.25; q = x*x+ci*ci;
                if (q*(q+x) <= .25*ci*ci || (cr+1.0)*(cr+1.0)+ci*ci <= .0625) {
                    mandelPixel(video, 0); ++pixels; --budget; continue;
                }
            }
        }
        result=mandelBatch(budget);budget=result&255u;
        if (result&0x100u) {
            mandelPixel(video, iteration == 128 ? 0 : (unsigned char)(32+(iteration*5u)%224u));
            ++pixels;
        }
    }
}

static unsigned randomValue(unsigned long *state)
{
    unsigned long value=*state;
    value^=value<<13;value^=value>>17;value^=value<<5;*state=value;
    return (unsigned)(value>>16);
}

static double randomAngle(unsigned long *state)
{
    return (double)randomValue(state)*(2.0*PI/65536.0);
}

int msBegin(unsigned kind, unsigned long seed)
{
    unsigned control;
    msImage = _fmalloc(64000u);
    if (!msImage) return 0;
    msKind = kind; msFrames = 0; stage = item = 0; angle = .6;
    msLastSeed=seed;poseY=poseZ=wavePhase=0.0;
    msLastKind=kind;msElapsedTicks=0;msFirstFrameTicks=0;lastTick=ticks();
    _asm { fstcw control }
    registerOrbits=(control&0x0300)==0x0300;
    centerX=160;centerY=100;velocityX=2;velocityY=1;msBounces=0;
    if(kind==1&&seed){
        angle+=randomAngle(&seed);poseY=randomAngle(&seed);
        poseZ=randomAngle(&seed);wavePhase=randomAngle(&seed);
        centerX=100+randomValue(&seed)%121;centerY=80+randomValue(&seed)%41;
        velocityX=(randomValue(&seed)&1)?2:-2;velocityY=(randomValue(&seed)&1)?1:-1;
    }
    if(kind==1)stage=4;
    oldLeft=oldRight=oldTop=oldBottom=0;
    cx = -.5; cy = 0.0; span = 3.4;
    mx = my = iteration = pixelActive = holding = zoom = 0; block = 4;
    coordinatesReady=0;coordinateIndex=0;
    palette();
    return 1;
}

void msStep(unsigned char far *video)
{
    elapsed();
    if (msKind == 1) sculpture(video);
    else mandelbrot(video);
}

void msEnd(void)
{
    elapsed();
    if (msImage) _ffree(msImage);
    msImage = NULL;
}
