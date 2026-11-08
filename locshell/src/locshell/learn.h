#ifndef DS_LEARN_H
#define DS_LEARN_H
#include "memory.h"
#define DL_PARTIAL 1
#define DL_CONFIRMED 2
#define DL_REJECTED 3
#define DL_RESET 4
typedef struct {unsigned version,size,paras,valid,base_conv,min_conv,base_ems,min_ems,base_xms,min_xms,fail_dos,fail_ems,fail_xms,samples;} DLStats;
typedef struct {unsigned long id,signature;unsigned disposition;DLStats stats;} DLRecord;
typedef enum {DL_FIT_NONE,DL_FIT_READY,DL_FIT_LESS_RAM,DL_FIT_CHANGED,
              DL_FIT_CONFIRM,DL_FIT_FAILED,DL_FIT_INCOMPLETE} DLFit;
void dl_index(void);
int dl_has(unsigned long id);
int dl_latest(unsigned long id,DLRecord *out);
int dl_append(const DLRecord *r);
unsigned long dl_signature(const char *commands);
unsigned long dl_legacy_signature(const char *commands,const char *boot_config);
int dl_prepare(unsigned long id,const char *commands);
unsigned dl_confirmation(unsigned long id,unsigned long signature);
int dl_confirm(const DLRecord*,unsigned disposition);
char dl_marker(unsigned long id,unsigned long signature,const DMMemory*memory);
DLFit dl_assess(unsigned long id,const char *commands,const DMMemory*,DLRecord*);
DLFit dl_budget(const DLRecord*,const DMMemory*);
unsigned long dl_conv_budget(const DLRecord*);
#endif
