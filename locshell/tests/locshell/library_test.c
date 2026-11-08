#include "library.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void write_text(const char *p, const char *s) {
    FILE *f = fopen(p, "wb");
    assert(f);
    assert(fwrite(s, 1, strlen(s), f) == strlen(s));
    assert(!fclose(f));
}
static char *read_text(const char *p) {
    FILE *f = fopen(p, "rb");
    long n;
    char *s;
    assert(f);
    assert(!fseek(f, 0, SEEK_END));
    n = ftell(f);
    rewind(f);
    s = (char *)malloc((size_t)n + 1);
    assert(s);
    assert(fread(s, 1, (size_t)n, f) == (size_t)n);
    s[n] = 0;
    fclose(f);
    return s;
}
int main(int argc, char **argv) {
    LBEntry *copy, *added;
    char *saved, *now, *large;
    unsigned long id;
    FILE *f;
    int i;
    write_text(
        "INPUT.TXT",
        "First Game\r\n---GENRE---RPG\r\n---SERIES---The Series\r\n---DEVELOPER---The "
        "Developer\r\n---PUBLISHER---The "
        "Publisher\r\n---YEAR---1991\r\n---STATUS---Game\r\n---FAVORITE---\r\n---CUSTOM---Preserve "
        "this\r\nD:\r\nCD \\GAMES\\FIRST\r\nCALL START.BAT\r\n---CONFIG---\r\nD:\r\nCD "
        "\\GAMES\\FIRST\r\nSETUP.EXE\r\n\r\nSecond Game\r\nC:\r\nSECOND.EXE\r\n\r\n");
    assert(lb_load("INPUT.TXT"));
    assert(lb_count == 2);
    assert(lb_entries[0]->favorite);
    assert(!strcmp(lb_entries[0]->meta[0], "RPG"));
    assert(!strcmp(lb_entries[0]->run, "D:\nCD \\GAMES\\FIRST\nCALL START.BAT\n"));
    assert(!strcmp(lb_entries[0]->setup, "D:\nCD \\GAMES\\FIRST\nSETUP.EXE\n"));
    assert(strstr(lb_entries[0]->extra, "CUSTOM"));
    copy = lb_clone(lb_entries[0]);
    assert(copy);
    assert(lb_replace(&copy->meta[0],"Independent clone"));
    assert(!strcmp(lb_entries[0]->meta[0],"RPG"));
    assert(lb_replace(&copy->meta[0],"RPG"));
    id = lb_identity(copy);
    assert(id == lb_identity(lb_entries[0]));
    assert(lb_save("ROUND.TXT"));
    lb_clear();
    assert(lb_load("ROUND.TXT"));
    assert(!strcmp(copy->run, lb_entries[0]->run));
    assert(!strcmp(copy->setup, lb_entries[0]->setup));
    assert(!strcmp(copy->extra, lb_entries[0]->extra));
    for (i = 0; i < LB_META; i++)
        assert(!strcmp(copy->meta[i], lb_entries[0]->meta[i]));
    lb_free(copy);
    strcpy(lb_entries[0]->title, "Renamed Game");
    assert(lb_identity(lb_entries[0]) != id);
    lb_entries[0]->favorite = 0;
    added = lb_new();
    assert(added);
    strcpy(added->title, "Added Game");
    assert(lb_replace(&added->run, "CALL ADDED.BAT\n"));
    lb_entries[lb_count++] = added;
    assert(lb_save("ROUND.TXT"));
    assert(lb_load("ROUND.TXT"));
    assert(lb_count == 3);
    assert(!strcmp(lb_entries[0]->title, "Renamed Game"));
    assert(!lb_entries[0]->favorite);
    assert(!strcmp(lb_entries[2]->title, "Added Game"));
    lb_free(lb_entries[1]);
    lb_entries[1] = lb_entries[2];
    lb_count--;
    assert(lb_save("ROUND.TXT"));
    assert(lb_load("ROUND.TXT"));
    assert(lb_count == 2);
    assert(!strcmp(lb_entries[1]->title, "Added Game"));
    saved = read_text("ROUND.TXT");
    assert(!lb_save("missing/ROUND.TXT"));
    now = read_text("ROUND.TXT");
    assert(!strcmp(saved, now));
    free(saved);
    free(now);
    assert(lb_write_run("RUN.BAT", "D:\nCD \\GAME\nCALL GAME.BAT\n"));
    saved = read_text("RUN.BAT");
    assert(strstr(saved, "@ECHO OFF\nD:\nCD \\GAME\nCALL GAME.BAT\n"));
    assert(!lb_write_run("RUN.BAT", ""));
    now = read_text("RUN.BAT");
    assert(!strcmp(saved, now));
    free(now);
    write_text("RESERVE.NEW", "Do not overwrite\n");
    assert(!lb_write_run("RESERVE.NEW", "GAME.EXE"));
    now = read_text("RESERVE.NEW");
    assert(!strcmp(now, "Do not overwrite\n"));
    free(now);
    free(saved);
    large = (char *)malloc(LB_COMMAND + 3);
    assert(large);
    memset(large, 'X', LB_COMMAND - 1);
    large[LB_COMMAND - 1] = '\n';
    large[LB_COMMAND] = 0;
    f = fopen("LARGE.TXT", "wb");
    assert(f);
    fputs("Large Commands\n", f);
    fputs(large, f);
    fputs("\n", f);
    fclose(f);
    assert(lb_load("LARGE.TXT"));
    assert(strlen(lb_entries[0]->run) == LB_COMMAND);
    assert(lb_save("LARGE2.TXT"));
    assert(lb_load("LARGE2.TXT"));
    assert(strlen(lb_entries[0]->run) == LB_COMMAND);
    memset(large, 'Y', LB_COMMAND);
    large[LB_COMMAND] = '\n';
    large[LB_COMMAND + 1] = 0;
    f = fopen("OVER.TXT", "wb");
    assert(f);
    fputs("Too Large\n", f);
    fputs(large, f);
    fclose(f);
    assert(!lb_load("OVER.TXT"));
    assert(!lb_count);
    free(large);
    write_text("BAD.TXT", "");
    f = fopen("BAD.TXT", "ab");
    assert(f);
    for (i = 0; i < LB_TITLE; i++)
        fputc('Z', f);
    fputc('\n', f);
    fclose(f);
    assert(!lb_load("BAD.TXT"));
    assert(!lb_count);
    assert(lb_load("INPUT.TXT"));
    assert(lb_save("ROUND.TXT"));
    saved = read_text("ROUND.BAK");
    assert(strstr(saved, "Renamed Game"));
    free(saved);
    lb_clear();
    if (argc > 1) {
        assert(lb_load(argv[1]));
        printf("External catalogue: %d records parsed without modifying the input.\n", lb_count);
        lb_clear();
    }
    puts("PASS: multiline import/roundtrip, metadata/favorites/extensions, CRUD, backups, failed "
         "writes, command handoff, size boundaries, reserved targets.");
    return 0;
}
