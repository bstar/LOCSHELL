#ifndef DS_MEMORY_H
#define DS_MEMORY_H
#define DM_MAX 512
#define DM_SOURCE 80
typedef struct { unsigned long id; unsigned conv,ems,xms,total,known; char grade,source[DM_SOURCE]; } DMRequirement;
typedef struct {unsigned conv_valid,ems_valid,xms_valid;unsigned long launch_bytes,own_bytes;unsigned ems_kb,xms_largest_kb,xms_free_kb;unsigned long conv_total_bytes;} DMMemory;
extern DMMemory dm_memory;
extern char dm_error[100];
int dm_load(const char*);
int dm_save(const char*);
DMRequirement *dm_find(unsigned long);
int dm_set(const DMRequirement*);
char dm_status(const DMRequirement*,const DMMemory*);
void dm_query(void);
#endif
