/* Optional 387 VGA renderers. GPL-3.0-or-later. */
#ifndef LOC_MATHSAVE_H
#define LOC_MATHSAVE_H
#ifdef __cplusplus
extern "C" {
#endif
/* Called only after the dashboard has detected a 387 and entered VGA. */
int msBegin(unsigned kind, unsigned long seed);
void msStep(unsigned char far *video);
void msEnd(void);
extern unsigned long msFrames;
extern unsigned long msLastSeed;
extern unsigned long msElapsedTicks;
extern unsigned long msFirstFrameTicks;
extern unsigned msLastKind;
extern unsigned long msBounces;
extern unsigned char far *msImage;
#ifdef __cplusplus
}
#endif
#endif
