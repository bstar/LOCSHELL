#include "library.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
LBEntry *lb_entries[LB_MAX];
int lb_count;
char lb_error[160];
static const char *tags[LB_META] = {"GENRE", "SERIES", "DEVELOPER", "PUBLISHER", "YEAR", "STATUS"};
const char *lb_str(const char *s) {
    return s ? s : "";
}
int lb_blank(const char *s) {
    while (*s) {
        if (!isspace((unsigned char)*s))
            return 0;
        s++;
    }
    return 1;
}
static int fail(const char *s) {
    strncpy(lb_error, s, sizeof(lb_error) - 1);
    lb_error[sizeof(lb_error) - 1] = 0;
    return 0;
}
LBEntry *lb_new(void) {
    LBEntry *e=(LBEntry *)calloc(1,sizeof(LBEntry));int i;
    if(!e)return 0;
    for(i=0;i<LB_META;i++){e->meta[i]=(char*)calloc(1,1);if(!e->meta[i]){lb_free(e);return 0;}}
    return e;
}
void lb_free(LBEntry *e) {
    int i;
    if (!e)
        return;
    for(i=0;i<LB_META;i++)free(e->meta[i]);
    free(e->run);
    free(e->setup);
    free(e->extra);
    free(e);
}
int lb_replace(char **dst, const char *src) {
    char *p = (char *)malloc(strlen(src) + 1);
    if (!p)
        return fail("Not enough memory.");
    strcpy(p, src);
    free(*dst);
    *dst = p;
    return 1;
}
LBEntry *lb_clone(const LBEntry *src) {
    LBEntry *e = lb_new();
    int i;
    if (!e)
        return 0;
    strcpy(e->title,src->title);e->favorite=src->favorite;
    for(i=0;i<LB_META;i++)if(!lb_replace(&e->meta[i],src->meta[i])){lb_free(e);return 0;}
    if (!lb_replace(&e->run, lb_str(src->run)) || !lb_replace(&e->setup, lb_str(src->setup)) ||
        !lb_replace(&e->extra, lb_str(src->extra))) {
        lb_free(e);
        return 0;
    }
    return e;
}
void lb_clear(void) {
    int i;
    for (i = 0; i < lb_count; i++)
        lb_free(lb_entries[i]);
    lb_count = 0;
}
static int append(char **dst, const char *s) {
    unsigned a = (unsigned)strlen(lb_str(*dst)), b = (unsigned)strlen(s);
    char *p;
    if (a + b + 1 > LB_COMMAND)
        return fail("A command block exceeds 8192 bytes; catalogue was not changed.");
    p = (char *)realloc(*dst, a + b + 2);
    if (!p)
        return fail("Not enough memory for catalogue commands.");
    *dst = p;
    memcpy(p + a, s, b);
    p[a + b] = '\n';
    p[a + b + 1] = 0;
    return 1;
}
int lb_load(const char *path) {
    FILE *f;
    LBEntry *e = 0;
    char *line, prefix[32];
    int setup = 0, i, known, ok = 1;
    lb_clear();
    lb_error[0] = 0;
    f = fopen(path, "rt");
    if (!f)
        return fail("Cannot open catalogue. Start LOCSHELL.BAT in its installed directory.");
    line = (char *)malloc(LB_COMMAND + 2);
    if (!line) {
        fclose(f);
        return fail("Not enough memory for catalogue input.");
    }
    while (fgets(line, LB_COMMAND + 2, f)) {
        if (!strchr(line, '\n') && !feof(f)) {
            ok = fail("A catalogue line exceeds 8192 bytes; no data was discarded or saved.");
            break;
        }
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0]) {
            e = 0;
            setup = 0;
            continue;
        }
        if (!e) {
            if (lb_count == LB_MAX) {
                ok = fail("Catalogue exceeds 512 entries.");
                break;
            }
            if (strlen(line) >= LB_TITLE) {
                ok = fail("A catalogue title is too long.");
                break;
            }
            e = lb_new();
            if (!e) {
                ok = fail("Not enough conventional memory for the catalogue.");
                break;
            }
            strcpy(e->title, line);
            lb_entries[lb_count++] = e;
            continue;
        }
        if (!strcmp(line, "---CONFIG---")) {
            setup = 1;
            continue;
        }
        if (!strncmp(line, "---FAVORITE---", 14)) {
            e->favorite = 1;
            continue;
        }
        known = 0;
        for (i = 0; i < LB_META; i++) {
            sprintf(prefix, "---%s---", tags[i]);
            if (!strncmp(line, prefix, strlen(prefix))) {
                if (strlen(line + strlen(prefix)) >= LB_VALUE) {
                    ok = fail("A metadata field is too long.");
                } else
                    ok=lb_replace(&e->meta[i], line + strlen(prefix));
                known = 1;
                break;
            }
        }
        if (!ok)
            break;
        if (known)
            continue;
        if (!strncmp(line, "---", 3)) {
            if (!append(&e->extra, line)) {
                ok = 0;
                break;
            }
        } else if (!append(setup ? &e->setup : &e->run, line)) {
            ok = 0;
            break;
        }
    }
    if (ferror(f))
        ok = fail("Could not read the complete catalogue.");
    fclose(f);
    free(line);
    if (!ok) {
        lb_clear();
        return 0;
    }
    return 1;
}
static int install_file(const char *path, const char *temp, const char *backup) {
    FILE *f;
    int existed = 0;
    f = fopen(path, "rb");
    if (f) {
        existed = 1;
        fclose(f);
        remove(backup);
        if (rename(path, backup))
            return fail("Cannot back up the current file; update stopped.");
    }
    if (rename(temp, path)) {
        if (existed && rename(backup, path))
            return fail("Update failed; restore the .BAK file before continuing.");
        return fail("Cannot install the new file; the old file was restored.");
    }
    return 1;
}
static void siblings(const char *path, char *temp, char *backup) {
    char *p;
    strcpy(temp, path);
    p = strrchr(temp, '.');
    if (p)
        strcpy(p, ".NEW");
    else
        strcat(temp, ".NEW");
    strcpy(backup, path);
    p = strrchr(backup, '.');
    if (p)
        strcpy(p, ".BAK");
    else
        strcat(backup, ".BAK");
}
int lb_save(const char *path) {
    FILE *f;
    char temp[160], backup[160];
    int i, j, ok;
    if (strlen(path) > 145)
        return fail("Catalogue path is too long.");
    siblings(path, temp, backup);
    if (!strcmp(path, temp) || !strcmp(path, backup))
        return fail("Reserved .NEW or .BAK target extension.");
    f = fopen(temp, "wt");
    if (!f)
        return fail("Cannot create a new catalogue; check free disk space.");
    for (i = 0; i < lb_count; i++) {
        LBEntry *e = lb_entries[i];
        fprintf(f, "%s\n", e->title);
        for (j = 0; j < LB_META; j++)
            if (e->meta[j][0])
                fprintf(f, "---%s---%s\n", tags[j], e->meta[j]);
        if (e->favorite)
            fputs("---FAVORITE---\n", f);
        fputs(lb_str(e->extra), f);
        fputs(lb_str(e->run), f);
        if (e->run && *e->run && e->run[strlen(e->run) - 1] != '\n')
            fputc('\n', f);
        if (e->setup && *e->setup) {
            fputs("---CONFIG---\n", f);
            fputs(e->setup, f);
            if (e->setup[strlen(e->setup) - 1] != '\n')
                fputc('\n', f);
        }
        fputc('\n', f);
    }
    ok = !ferror(f);
    if (fclose(f))
        ok = 0;
    if (!ok) {
        remove(temp);
        return fail("Catalogue write failed; the previous file is unchanged.");
    }
    return install_file(path, temp, backup);
}
unsigned long lb_identity(const LBEntry *e) {
    const unsigned char *p;
    unsigned long h = 2166136261UL;
    p = (const unsigned char *)e->title;
    while (*p)
        h = ((h ^ *p++) * 16777619UL) & 0xffffffffUL;
    p = (const unsigned char *)lb_str(e->run);
    while (*p)
        h = ((h ^ *p++) * 16777619UL) & 0xffffffffUL;
    return h;
}
int lb_write_run(const char *path, const char *commands) {
    FILE *f;
    char temp[160], backup[160];
    int ok;
    if (lb_blank(commands))
        return fail("This application has no commands assigned to it.");
    if (strlen(path) > 145)
        return fail("Launch path is too long.");
    siblings(path, temp, backup);
    if (!strcmp(path, temp) || !strcmp(path, backup))
        return fail("Reserved .NEW or .BAK target extension.");
    f = fopen(temp, "wt");
    if (!f)
        return fail("Cannot create the launch script.");
    fputs("@ECHO OFF\n", f);
    fputs(commands, f);
    if (commands[strlen(commands) - 1] != '\n')
        fputc('\n', f);
    ok = !ferror(f);
    if (fclose(f))
        ok = 0;
    if (!ok) {
        remove(temp);
        return fail("Launch script write failed.");
    }
    return install_file(path, temp, backup);
}
