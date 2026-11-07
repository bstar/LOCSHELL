#ifndef LOCBOX_LIBRARY_H
#define LOCBOX_LIBRARY_H
#define LB_MAX 512
#define LB_META 6
#define LB_TITLE 112
#define LB_VALUE 96
#define LB_COMMAND 8192
/* Metadata order: genre, series, developer, publisher, year, status. */
typedef struct {
    char title[LB_TITLE];
    char *meta[LB_META];
    char *run, *setup, *extra;
    int favorite;
} LBEntry;
extern LBEntry *lb_entries[LB_MAX];
extern int lb_count;
extern char lb_error[160];
const char *lb_str(const char *s);
int lb_blank(const char *s);
LBEntry *lb_new(void);
LBEntry *lb_clone(const LBEntry *src);
void lb_free(LBEntry *e);
int lb_replace(char **dst, const char *src);
int lb_load(const char *path);
int lb_save(const char *path);
void lb_clear(void);
unsigned long lb_identity(const LBEntry *e);
int lb_write_run(const char *path, const char *commands);
#endif
