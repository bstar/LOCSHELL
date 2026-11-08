#ifndef LOC_LAUNCH_H
#define LOC_LAUNCH_H
#ifdef __cplusplus
extern "C" {
#endif
int lp_prepare(const char*,unsigned long);
int lp_publish(int);
#define LP_RECENT_MAX 8
typedef struct {unsigned long id,stamp;char kind;} LPRecent;
unsigned lp_recent_load(LPRecent*,unsigned);
int lp_recent_add(char,unsigned long,unsigned long);
#ifdef __cplusplus
}
#endif
#endif
