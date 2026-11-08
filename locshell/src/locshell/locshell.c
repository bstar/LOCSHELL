/* LOC SHELL: source-owned, 8086 real-mode DOS front end. */
#include "library.h"
#include "memory.h"
#include "network.h"
#include "learn.h"
#include "launch.h"
#include "style.h"
#include "profile.h"
#include <conio.h>
#include <ctype.h>
#include <direct.h>
#include <dos.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define NROWS 18
#define LIST_X 20
#define LIST_Y 4
#define LIST_W 58
#define CONTENT_W 59
#define SCROLL_X 79
#define K_UP 328
#define K_DOWN 336
#define K_LEFT 331
#define K_RIGHT 333
#define K_HOME 327
#define K_END 335
#define K_PGUP 329
#define K_PGDN 337
#define K_DELETE 339
#define K_SHIFT_TAB 271
#define K_F1 315
#define K_ALT_F 289
#define K_ALT_V 303
#define K_ALT_E 274
#define K_ALT_H 291
#define A_NORMAL LOC_BODY_ATTR
#define A_IDLE 0x90
#define A_PANEL LOC_PANEL_ATTR
#define A_SELECT 0xf9
#define A_BUTTON 0x70
#define A_FOCUS 0x0f
#define A_BORDER 0x97
static unsigned short __far *vram;
static unsigned short original[2000];
static unsigned short screen[2000], displayed[2000];
static unsigned old_cursor, old_pos;
static int has_mouse, buttons, screen_active;
static int mouse_suspended, mouse_saved_x, mouse_saved_y;
static int mouse_last_x,mouse_last_y,mouse_sleep_x,mouse_sleep_y,mouse_sleep_buttons,mouse_ignore_buttons;
static unsigned long mouse_sleep_tick;
static int mouse_visible, cursor_visible = -1, want_cursor, cursor_x, cursor_y;
static int drawn_cursor_x = -1, drawn_cursor_y = -1;
static int main_valid, drawn_sel, drawn_scroll, drawn_nav;
static char drawn_notice[112];
#ifdef LOCSHELL_RENDER_TEST
static unsigned long video_words_written, catalog_rebuilds;
#endif
static int mx, my, click, mouse_row = -1;
static clock_t mouse_when;
static int rows[LB_MAX], nrows, sel, scroll, grouping = -1, inside, sort_order, locked,
                                             nav_focus = -1;
static char *groups[LB_MAX], group_value[LB_VALUE];
static int ngroups, group_sel;
static char catalog[128] = "LIBRARY.TXT";
static char basepath[128];
static int quit_app, launch_app,shell_view=7;
static int dashboard_live;
static void dashboard_draw(int);
static void dashboard_poll(void);
static int dashboard_input(int);
static int dashboard_current_item(void);
static void network_view_draw(int);
static void view_nav(void);
static int view_click(void);
static int view_input(int);
static void choose_group(int);
static void shell_frame(int);
static void view_footer(const char *);
static void status_draw(void);
static void status_poll(void);
static int status_needs_paint=1;
static char pending_job[80];
static int network_input(int);
static char notice[112];
static unsigned long memory_ids[LB_MAX];
static DMRequirement *memory_profiles[LB_MAX];
static unsigned long memory_tick;
static int memory_loaded;
static int memory_context_dirty;
void ds_shell_memory_changed(void){memory_context_dirty=1;}
static void memory_details(void);
static void network_page(int);
static void learning_page(void);
static void learning_return(void);
static int learning_run;
static int network_wait(void);
static void network_utilities(void);
static const char *group_names[] = {"Genre",     "Series",       "Developer",
                                    "Publisher", "Release Year", "Catalog status"};
static const char *plural_names[] = {"Genres",     "Series",        "Developers",
                                     "Publishers", "Release Years", "Statuses"};
static const char *browse_names[] = {"All games", "Genres", "Series", "Developers", "Publishers", "Years", "Catalogue"};
static void main_draw(int inactive);
static void info(const char *s);
static int confirm(const char *s);
static int editor(int index);
static int browser(char *result);
static int run_commands(const char *s);
static int run_context(const char*,const char*,unsigned long,const char*);
static void action(int id);
static int prefs_save(void);
static void put(int x, int y, int w, unsigned a, const char *s) {
    int i;
    if (y < 0 || y >= 25)
        return;
    if(y>=LOC_CONNECTION_ROW)status_needs_paint=1;
    for (i = 0; i < w && x + i < 80; i++) {
        unsigned char c = ' ';
        if (*s)
            c = (unsigned char)*s++;
        if (x + i >= 0)
            screen[y * 80 + x + i] = (a << 8) | c;
    }
}
static void cell(int x, int y, unsigned a, unsigned char c) {
    char s[2];
    s[0] = (char)c;
    s[1] = 0;
    put(x, y, 1, a, s);
}
static void fill(int x, int y, int w, int h, unsigned a) {
    int i;
    for (i = 0; i < h; i++)
        put(x, y + i, w, a, "");
}
static void frame(int x, int y, int w, int h, unsigned a) {
    int i;
    for (i = 1; i < w - 1; i++) {
        cell(x + i, y, a, 196);
        cell(x + i, y + h - 1, a, 196);
    }
    for (i = 1; i < h - 1; i++) {
        cell(x, y + i, a, 179);
        cell(x + w - 1, y + i, a, 179);
    }
    cell(x, y, a, 218);
    cell(x + w - 1, y, a, 191);
    cell(x, y + h - 1, a, 192);
    cell(x + w - 1, y + h - 1, a, 217);
}
static void hide_cursor(void) {
    want_cursor = 0;
}
static void cursor_at(int x, int y) {
    want_cursor = 1;
    cursor_x = x;
    cursor_y = y;
}
static void mouse_fn(unsigned fn, union REGS *r) {
    r->x.ax = fn;
    int86(0x33, r, r);
}
static void mouse_setup(void) {
    union REGS r;
    memset(&r, 0, sizeof(r));
    mouse_fn(0, &r);
    has_mouse = r.x.ax == 0xffff;
    mouse_visible = 0;
    buttons = 0;
    if (has_mouse) {
        /* Use a software pointer, independent of the editor's BIOS cursor. */
        r.x.bx = 0;
        r.x.cx = 0xffff;
        r.x.dx = 0x7700;
        mouse_fn(10, &r);
        r.x.cx = 0;
        r.x.dx = 639;
        mouse_fn(7, &r);
        r.x.cx = 0;
        r.x.dx = 199;
        mouse_fn(8, &r);
        mouse_fn(3, &r);
        mouse_last_x=r.x.cx;mouse_last_y=r.x.dx;
    }
}
/* Graphics mode changes can reset a resident mouse driver's show/hide count.
   Hide before saving text VRAM, then rebuild the text pointer after restoring
   it. Otherwise the driver still reports clicks with an invisible pointer. */
void ds_shell_mouse_suspend(int suspend) {
    union REGS r;
    if (!screen_active || mouse_suspended == suspend)
        return;
    mouse_suspended = suspend;
    if (suspend) {
        if (has_mouse) {
            mouse_fn(3, &r);
            mouse_saved_x = r.x.cx;
            mouse_saved_y = r.x.dx;
            if (mouse_visible)
                mouse_fn(2, &r);
            mouse_visible = 0;
        }
    } else {
        if(has_mouse){mouse_fn(3,&r);if(r.x.bx&7)mouse_ignore_buttons=1;}
        mouse_setup();
        if (has_mouse) {
            r.x.cx = mouse_saved_x;
            r.x.dx = mouse_saved_y;
            mouse_fn(4, &r);
            mouse_fn(3, &r);
            buttons = r.x.bx;
            mouse_last_x=r.x.cx;mouse_last_y=r.x.dx;
            mouse_fn(1, &r);
            mouse_visible = 1;
        }
        cursor_visible = -1;
        mouse_row=-1;
    }
}
/* Snapshot after the final graphics mode switch, so the driver's mode-change
   coordinate reset cannot masquerade as physical mouse activity. */
void ds_shell_mouse_sleep_started(void){union REGS r;if(!has_mouse)return;mouse_fn(3,&r);mouse_sleep_x=r.x.cx;mouse_sleep_y=r.x.dx;mouse_sleep_buttons=r.x.bx;mouse_sleep_tick=*(unsigned long __far*)MK_FP(0x40,0x6c);}
int ds_shell_mouse_wake(void){union REGS r;unsigned long now;if(!has_mouse||!mouse_suspended)return 0;now=*(unsigned long __far*)MK_FP(0x40,0x6c);if(now-mouse_sleep_tick<2UL)return 0;mouse_sleep_tick=now;mouse_fn(3,&r);if(r.x.cx!=mouse_sleep_x||r.x.dx!=mouse_sleep_y||r.x.bx!=mouse_sleep_buttons){if(r.x.bx&7)mouse_ignore_buttons=1;return 1;}return 0;}
/* Finish the entire frame in RAM before touching video memory. In particular,
   dialogs must not expose the intermediate inactive background beneath them. */
static void present(void) {
    union REGS r;
    int i = 0;
    if(screen_active)status_draw();
    while (i < 2000 && screen[i] == displayed[i])
        i++;
    if (i < 2000) {
        if (has_mouse && mouse_visible)
            mouse_fn(2, &r);
        for (; i < 2000; i++)
            if (screen[i] != displayed[i]) {
                vram[i] = screen[i];
                displayed[i] = screen[i];
#ifdef LOCSHELL_RENDER_TEST
                video_words_written++;
#endif
            }
        if (has_mouse) {
            mouse_fn(1, &r);
            mouse_visible = 1;
        }
    }
    if (cursor_visible != want_cursor) {
        r.h.ah = 1;
        r.x.cx = want_cursor ? 0x0607 : 0x2000;
        int86(0x10, &r, &r);
        cursor_visible = want_cursor;
    }
    if (want_cursor && (drawn_cursor_x != cursor_x || drawn_cursor_y != cursor_y)) {
        r.h.ah = 2;
        r.h.bh = 0;
        r.h.dl = (unsigned char)cursor_x;
        r.h.dh = (unsigned char)cursor_y;
        int86(0x10, &r, &r);
        drawn_cursor_x = cursor_x;
        drawn_cursor_y = cursor_y;
    }
}
static int event(void) {
#ifdef LOCSHELL_INPUT_TEST
    extern int ds_test_event(void);
    return ds_test_event();
#else
    union REGS r;
    int c,notify=0;
    present();
    for (;;) {
        click = 0;
        {int net=ds_net_poll();if(net==2)continue;if(net==1){main_valid=0;return -2;}if(net&DN_JOB)notify=3;else if(net&DN_RELOAD)notify=4;}
        if(memory_context_dirty){DNSnapshot state;ds_net_snapshot(&state);
            if(!state.transfers){memory_context_dirty=0;dl_index();main_valid=0;if(!notify)return -2;}
            else if(memory_context_dirty==1){memory_context_dirty=2;main_valid=0;if(!notify)return -2;}
        }
        status_poll();
#ifndef LOCSHELL_RENDER_TEST
        if (memory_loaded) {
            unsigned long now = *(unsigned long __far *)MK_FP(0x40,0x6c);
            if (now-memory_tick >= 18UL) {
                DMMemory old=dm_memory; memory_tick=now; dm_query();
                if(memcmp(&old,&dm_memory,sizeof old))main_valid=0;
                if(!notify)return -2;
            }
        }
#endif
        if (kbhit()) {
            ds_net_activity();
            c = getch();
            if (!c || c == 224)
                return 256 + getch();
            return c;
        }
        if (has_mouse) {
            mouse_fn(3, &r);
            if(r.x.cx!=mouse_last_x||r.x.dx!=mouse_last_y||r.x.bx!=buttons)ds_net_activity();
            mouse_last_x=r.x.cx;mouse_last_y=r.x.dx;
            mx = r.x.cx / 8;
            my = r.x.dx / 8;
            if(mouse_ignore_buttons){buttons=r.x.bx;if(!(buttons&7))mouse_ignore_buttons=0;}
            else if ((r.x.bx & 1) && !(buttons & 1)) {
                buttons = r.x.bx;
                ds_net_activity();
                click = 1;
                return -1;
            }
            buttons = r.x.bx;
        }
        if(notify)return -notify;
        r.x.ax = 0x1680;
        int86(0x2f, &r, &r);
    }
#endif
}
static void init_screen(void) {
    union REGS r;
    int i;
    r.h.ah = 0x0f;
    int86(0x10, &r, &r);
    if (r.h.al != 3) {
        r.x.ax = 3;
        int86(0x10, &r, &r);
    }
    vram = (unsigned short __far *)MK_FP(0xb800, 0);
    for (i = 0; i < 2000; i++)
        original[i] = screen[i] = displayed[i] = vram[i];
    r.h.ah = 3;
    r.h.bh = 0;
    int86(0x10, &r, &r);
    old_cursor = r.x.cx;
    old_pos = r.x.dx;
    hide_cursor();
    r.x.ax = 0x1003;
    r.h.bl = 0;
    r.h.bh = 0;
    int86(0x10, &r, &r);
    mouse_setup();
    mouse_suspended = 0;
    screen_active = 1;
}
static void finish_screen(void) {
    union REGS r;
    int i;
    if (!screen_active)
        return;
    if (has_mouse && mouse_visible)
        mouse_fn(2, &r);
    mouse_visible = 0;
    for (i = 0; i < 2000; i++)
        vram[i] = original[i];
    r.x.ax = 0x1003;
    r.h.bl = 1;
    r.h.bh = 0;
    int86(0x10, &r, &r);
    r.h.ah = 1;
    r.x.cx = old_cursor;
    int86(0x10, &r, &r);
    r.h.ah = 2;
    r.h.bh = 0;
    r.x.dx = old_pos;
    int86(0x10, &r, &r);
    screen_active = 0;
}
static void shadow(int x, int y, int w, int h) {
    int row, col;
    if(y+h>=LOC_CONNECTION_ROW)status_needs_paint=1;
    for (row = y + 1; row <= y + h && row < 25; row++)
        for (col = x + 2; col < x + w + 2 && col < 80; col++)
            if (row >= 0 && col >= 0)
                screen[row * 80 + col] = (screen[row * 80 + col] & 0x00ff) | 0x0800;
}
static void window(int x, int y, int w, int h, const char *title) {
    int len = (int)strlen(title);
    dashboard_live=0;
    hide_cursor();
    shadow(x, y, w, h);
    fill(x, y, w, h, A_PANEL);
    frame(x, y, w, h, A_PANEL);
    if (len > w - 6)
        len = w - 6;
    put(x + 2, y, w - 4, 0x0f, "");
    put(x + (w - len) / 2, y, len, 0x0f, title);
    cell(x + 1, y, 0x70, 254);
}
static void button(int x, int y, int w, const char *name, int focused) {
    fill(x, y, w, 1, focused ? 0x0f : 0x70);
    put(x + (w - (int)strlen(name)) / 2, y, (int)strlen(name), focused ? 0x0f : 0x70, name);
    cell(x - 1, y, 0x70, focused ? 91 : 32);
    cell(x + w, y, 0x70, focused ? 93 : 32);
}
static int over(int x, int y, int w, int h) {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}
static int word_compare(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}
static int item_compare(const void *a, const void *b) {
    LBEntry *x = lb_entries[*(const int *)a], *y = lb_entries[*(const int *)b];
    int n = 0;
    if (sort_order == 1)
        n = y->favorite - x->favorite;
    else if (sort_order == 2)
        n = strcmp(x->meta[4], y->meta[4]);
    else if (sort_order == 3)
        n = strcmp(x->meta[5], y->meta[5]);
    return n ? n : strcmp(x->title, y->title);
}
static void groups_clear(void) {
    int i;
    for (i = 0; i < ngroups; i++)
        free(groups[i]);
    ngroups = 0;
}
static void selection_clamp(void) {
    if (sel >= nrows)
        sel = nrows - 1;
    if (sel < 0)
        sel = 0;
    if (scroll > sel)
        scroll = sel;
    if (sel >= scroll + NROWS)
        scroll = sel - NROWS + 1;
    if (scroll < 0)
        scroll = 0;
}
static void rebuild(void) {
    int i, j;
    const char *s;
    main_valid = 0;
#ifdef LOCSHELL_RENDER_TEST
    catalog_rebuilds++;
#endif
    for (i=0;i<lb_count;i++){memory_ids[i]=lb_identity(lb_entries[i]);memory_profiles[i]=dm_find(memory_ids[i]);}
    groups_clear();
    nrows = 0;
    if (grouping >= 0 && !inside) {
        for (i = 0; i < lb_count; i++) {
            s = lb_entries[i]->meta[grouping];
            if (lb_blank(s))
                s = "";
            for (j = 0; j < ngroups; j++)
                if (!strcmp(groups[j], s))
                    break;
            if (j == ngroups) {
                groups[ngroups] = (char *)malloc(strlen(s) + 1);
                if (!groups[ngroups]) {
                    strcpy(notice, "Not enough memory to group this catalogue.");
                    break;
                }
                strcpy(groups[ngroups++], s);
            }
        }
        qsort(groups, ngroups, sizeof(groups[0]), word_compare);
        nrows = ngroups;
    } else {
        for (i = 0; i < lb_count; i++) {
            s = grouping < 0 ? "" : lb_entries[i]->meta[grouping];
            if (lb_blank(s))
                s = "";
            if (grouping >= 0 && strcmp(s, group_value))
                continue;
            rows[nrows++] = i;
        }
        qsort(rows, nrows, sizeof(rows[0]), item_compare);
    }
    selection_clamp();
}
static int current_item(void) {
    if(shell_view==7)return dashboard_current_item();
    if (!nrows || (grouping >= 0 && !inside))
        return -1;
    return rows[sel];
}
#include "memory_status_ui.h"
static void list_row(int row, int inactive) {
    int idx = scroll + row;
    char line[160];
    unsigned attr = inactive ? A_IDLE : A_NORMAL;
    if (row < 0 || row >= NROWS)
        return;
    if (idx >= nrows)
        put(LIST_X, LIST_Y + row, LIST_W, attr, "");
    else {
        if (grouping >= 0 && !inside)
            sprintf(line, " %s", *groups[idx] ? groups[idx] : "(None)");
        else {
            MemoryState state;unsigned row_attr;
            memory_state(rows[idx],&state);
            sprintf(line,"%s%-44.44s %c%c%-10.10s",lb_entries[rows[idx]]->favorite?"*":" ",lb_entries[rows[idx]]->title,state.marker,state.review?'!':' ',state.label);
            row_attr=idx==sel?(inactive?0x78:A_SELECT):attr;
            put(LIST_X,LIST_Y+row,LIST_W,row_attr,line);
            if(!inactive)cell(LIST_X+46,LIST_Y+row,(row_attr&0xf0)|(idx==sel&&state.ink>=10?state.ink-8:state.ink),(unsigned char)state.marker);
            if(!inactive&&state.review)cell(LIST_X+47,LIST_Y+row,(row_attr&0xf0)|(idx==sel?6:14),'!');
            return;
        }
        put(LIST_X, LIST_Y + row, LIST_W, idx == sel ? (inactive ? 0x78 : A_SELECT) : attr, line);
    }
}
static void list_scrollbar(void) {
    int i;
    cell(SCROLL_X, LIST_Y - 1, A_BORDER, 24);
    cell(SCROLL_X, LIST_Y + NROWS, A_BORDER, 25);
    for (i = LIST_Y; i < LIST_Y + NROWS; i++)
        cell(SCROLL_X, i, A_BORDER, 176);
    if (nrows > NROWS)
        cell(SCROLL_X, LIST_Y + (int)((long)scroll * (NROWS - 1) / (nrows - NROWS)), A_NORMAL, 219);
}
static void list_detail(int inactive) {
    const char *text = "Enter opens a group; Backspace returns.";
    int item = current_item();
    if (item >= 0) text = lb_entries[item]->title;
    put(LIST_X, 22, CONTENT_W, inactive ? A_IDLE : A_NORMAL, text);
}
static void main_draw(int inactive) {
    int i;
    char line[160];
    unsigned attr = inactive ? A_IDLE : A_NORMAL;
    hide_cursor();
    if(shell_view==7){dashboard_draw(inactive);return;}
    dashboard_live=0;
    if(shell_view){network_view_draw(inactive);return;}
    if (!inactive && main_valid) {
        if (scroll != drawn_scroll) {
            for (i = 0; i < NROWS; i++)
                list_row(i, 0);
            list_scrollbar();
        } else if (sel != drawn_sel) {
            list_row(drawn_sel - scroll, 0);
            list_row(sel - scroll, 0);
        }
        if (nav_focus != drawn_nav)
            view_nav();
        if (sel != drawn_sel || scroll != drawn_scroll)
            list_detail(0);
        if (nav_focus != drawn_nav || strcmp(notice, drawn_notice)) {
            if (*notice)
                view_footer(notice);
            else
                view_footer("Tab Menu Enter Run F4 RAM F6 FTP F7 Learn F9 Report");
        }
        goto remember;
    }
    main_valid = 0;
    shell_frame(inactive);
    if (grouping < 0)
        sprintf(line, "All Items (%d)", nrows);
    else if (!inside)
        sprintf(line, "All %s (%d)", plural_names[grouping], nrows);
    else
        sprintf(line, "> %s: %.80s (%d)", group_names[grouping],
                *group_value ? group_value : "(None)", nrows);
    put(LIST_X, 2, CONTENT_W, attr, line);
    for (i = 0; i < NROWS; i++)
        list_row(i, inactive);
    list_scrollbar();
    list_detail(inactive);
    if (*notice) view_footer(notice);
    else view_footer("Tab Menu Enter Run F4 RAM F6 FTP F7 Learn F9 Report");
remember:
    if (!inactive) {
        main_valid = 1;
        drawn_sel = sel;
        drawn_scroll = scroll;
        drawn_nav = nav_focus;
        strcpy(drawn_notice, notice);
    }
}
static void choose_group(int g) {
    shell_view=0;
    grouping = g;
    inside = 0;
    sel = scroll = 0;
    nav_focus = -1;
    group_value[0] = 0;
    rebuild();
}
static void back(void) {
    if (grouping >= 0 && inside) {
        inside = 0;
        sel = group_sel;
        scroll = 0;
        rebuild();
    }
}
static int confirm(const char *s) {
    int k, yes = 1;
    for (;;) {
        main_draw(1);
        window(1, 8, 78, 8, "");
        put(3, 10, 74, A_PANEL, s);
        if (strlen(s) > 74)
            put(3, 11, 74, A_PANEL, s + 74);
        button(30, 14, 9, "Yes", yes);
        button(42, 14, 9, "No", !yes);
        k = event();
        if (k == -3 || k == -4 || k == 27)
            return 0;
        if (k == 9 || k == K_SHIFT_TAB || k == K_LEFT || k == K_RIGHT)
            yes = !yes;
        else if (k == 13)
            return yes;
        else if (k == -1) {
            if (over(29, 14, 11, 1))
                return 1;
            if (over(41, 14, 11, 1))
                return 0;
        }
    }
}
static void info(const char *s) {
    int k;
    for (;;) {
        main_draw(1);
        window(2, 8, 76, 7, "");
        put(4, 10, 72, A_PANEL, s);
        if (strlen(s) > 72)
            put(4, 11, 72, A_PANEL, s + 72);
        button(35, 13, 9, "OK", 1);
        k = event();
        if (k == -3 || k == -4 || k == 13 || k == 27 || k == ' ' || (k == -1 && over(34, 13, 11, 1)))
            break;
    }
}
static void text_page(const char *title, const char **lines, int count) {
    int k, i, top = 0;
    for (;;) {
        main_draw(1);
        window(4, 4, 70, 17, title);
        for (i = 0; i < 13; i++)
            if (top + i < count)
                put(6, 6 + i, 65, A_PANEL, lines[top + i]);
        cell(72, 5, 0x70, 24);
        cell(72, 19, 0x70, 25);
        k = event();
        if (k == -3 || k == -4 || k == 27 || k == 13)
            break;
        if (k == K_DOWN && top + 13 < count)
            top++;
        if (k == K_UP && top)
            top--;
        if (k == K_PGDN) {
            top += 12;
            if (top > count - 13)
                top = count > 13 ? count - 13 : 0;
        }
        if (k == K_PGUP) {
            top -= 12;
            if (top < 0)
                top = 0;
        }
        if (k == -1 && over(4, 4, 2, 1))
            break;
    }
}
/* Edits operate on a copy. Only the outer OK commits it to disk. */
static void input_draw(int x, int y, int w, int h, const char *text, unsigned pos, int focused) {
    unsigned i = 0, start = 0, col = 0, row = 0, paintrow = 0;
    int k;
    if (!focused)
        pos = 0;
    if (pos > strlen(text))
        pos = (unsigned)strlen(text);
    fill(x, y, w, h, A_PANEL);
    while (text[i] && i < pos) {
        if (text[i++] == '\n') {
            row++;
            col = 0;
        } else
            col++;
    }
    if (h == 1) {
        start = pos >= (unsigned)w ? pos - w + 1 : 0;
        put(x, y, w, A_PANEL, text + start);
        if (focused)
            cursor_at(x + pos - start, y);
        return;
    }
    if (row >= (unsigned)h) {
        unsigned skip = row - h + 1;
        while (skip && text[start])
            if (text[start++] == '\n')
                skip--;
        paintrow = row - h + 1;
    }
    i = start;
    for (k = 0; k < h; k++) {
        char buf[80];
        unsigned n = 0;
        while (text[i] && text[i] != '\n') {
            if (n < (unsigned)w)
                buf[n++] = text[i];
            i++;
        }
        buf[n] = 0;
        put(x, y + k, w, A_PANEL, buf);
        if (text[i] == '\n')
            i++;
    }
    if (focused)
        cursor_at(x + (col < (unsigned)w ? col : w - 1), y + row - paintrow);
}
static int input_key(char *s, unsigned cap, unsigned *pos, int key, int multiline) {
    unsigned n = (unsigned)strlen(s), p = *pos, start, target, col;
    int step;
    if (p > n)
        p = n;
    if (key == 8 && p) {
        memmove(s + p - 1, s + p, n - p + 1);
        p--;
    } else if (key == K_DELETE && p < n)
        memmove(s + p, s + p + 1, n - p);
    else if (key == K_LEFT && p)
        p--;
    else if (key == K_RIGHT && p < n)
        p++;
    else if (key == K_HOME) {
        while (p && s[p - 1] != '\n')
            p--;
    } else if (key == K_END) {
        while (s[p] && s[p] != '\n')
            p++;
    } else if ((key == K_UP || key == K_DOWN) && multiline) {
        start = p;
        while (start && s[start - 1] != '\n')
            start--;
        col = p - start;
        if (key == K_UP && start) {
            target = start - 1;
            while (target && s[target - 1] != '\n')
                target--;
        } else if (key == K_DOWN) {
            target = p;
            while (s[target] && s[target] != '\n')
                target++;
            if (s[target])
                target++;
        } else
            target = start;
        p = target;
        for (step = 0; step < (int)col && s[p] && s[p] != '\n'; step++)
            p++;
    } else if (key == 1) {
        s[0] = 0;
        p = 0;
    } else if ((key >= 32 && key <= 255) || (key == 13 && multiline)) {
        if (n + 1 < cap) {
            memmove(s + p + 1, s + p, n - p + 1);
            s[p++] = (char)(key == 13 ? '\n' : key);
        }
    }
    *pos = p;
    return 1;
}
#include "learn_ui.h"
#include "memory_ui.h"
static void details(LBEntry *e) {
    int field = 0, k, i, x, y;
    unsigned pos = 0;
    const char *labels[] = {"Genre", "Series", "Developer", "Publisher", "Release Year", "Catalog status"};
    for(i=0;i<LB_META;i++){char*p=(char*)realloc(e->meta[i],LB_VALUE);if(!p){info("Not enough memory for metadata editing.");return;}e->meta[i]=p;}
    for (;;) {
        main_draw(1);
        window(4, 2, 71, 21, "Application Details");
        for (i = 0; i < 6; i++) {
            x = i % 2 ? 40 : 7;
            y = 4 + (i / 2) * 5;
            put(x, y, 29, A_PANEL, labels[i]);
            frame(x, y + 1, 31, 3, A_PANEL);
            input_draw(x + 1, y + 2, 29, 1, e->meta[i], field == i ? pos : 0, field == i);
        }
        button(8, 20, 10, "Back", field == 6);
        put(40, 20, 22, field == 7 ? 0x0f : 0x70, e->favorite ? "[X] Favorite" : "[ ] Favorite");
        k = event();
        if (k == 27 || (k == 13 && field == 6))
            break;
        if (k == 9 || k == K_SHIFT_TAB) {
            field = (field + (k == 9 ? 1 : 7)) % 8;
            pos = field < 6 ? (unsigned)strlen(e->meta[field]) : 0;
        } else if (field == 7 && (k == ' ' || k == 13))
            e->favorite = !e->favorite;
        else if (k == -1) {
            if (over(7, 20, 12, 1))
                break;
            if (over(40, 20, 22, 1))
                e->favorite = !e->favorite;
            for (i = 0; i < 6; i++) {
                x = i % 2 ? 40 : 7;
                y = 6 + (i / 2) * 5;
                if (over(x, y, 31, 1)) {
                    field = i;
                    pos = (unsigned)strlen(e->meta[i]);
                }
            }
        } else if (field < 6)
            input_key(e->meta[field], field == 4 ? 8 : LB_VALUE, &pos, k, 0);
    }
}
static int editor(int index) {
    LBEntry *e = index < 0 ? lb_new() : lb_clone(lb_entries[index]), *old = 0;
    char *run, *setup, *dest;
    unsigned pos = 0, cap;
    int field = 0, k, accept = 0, ok;
    char chosen[160], commands[512], *p, *name;
    int drive;
    if (!e) {
        info("Not enough conventional memory for the editor.");
        return 0;
    }
    run = (char *)calloc(LB_COMMAND + 1, 1);
    setup = (char *)calloc(LB_COMMAND + 1, 1);
    if (!run || !setup) {
        free(run);
        free(setup);
        lb_free(e);
        info("Not enough memory for command editing.");
        return 0;
    }
    strcpy(run, lb_str(e->run));
    strcpy(setup, lb_str(e->setup));
    pos = (unsigned)strlen(e->title);
    for (;;) {
        main_draw(1);
        window(4, 2, 71, 21, index < 0 ? "Add Application" : "Edit Application");
        put(7, 5, 10, A_PANEL, "Title");
        frame(17, 4, 55, 3, A_PANEL);
        input_draw(18, 5, 53, 1, e->title, pos, field == 0);
        put(7, 8, 10, A_PANEL, "Run App");
        put(7, 9, 10, A_PANEL, "Commands");
        frame(17, 7, 42, 6, A_PANEL);
        input_draw(18, 8, 40, 4, run, pos, field == 1);
        button(61, 8, 11, "Browse...", field == 2);
        put(7, 14, 10, A_PANEL, "Configure");
        put(7, 15, 10, A_PANEL, "Commands");
        frame(17, 13, 42, 6, A_PANEL);
        input_draw(18, 14, 40, 4, setup, pos, field == 3);
        button(61, 14, 11, "Browse...", field == 4);
        button(18, 20, 11, "OK", field == 5);
        button(32, 20, 11, "Cancel", field == 6);
        button(61, 20, 11, "Details...", field == 7);
        k = event();
        if (k == 27 || (k == 13 && field == 6))
            break;
        if (k == 9 || k == K_SHIFT_TAB) {
            field = (field + (k == 9 ? 1 : 7)) % 8;
            pos = field == 0   ? (unsigned)strlen(e->title)
                  : field == 1 ? (unsigned)strlen(run)
                  : field == 3 ? (unsigned)strlen(setup)
                               : 0;
            continue;
        }
        if (k == -1) {
            if (over(18, 5, 53, 1))
                field = 0;
            else if (over(18, 8, 40, 4))
                field = 1;
            else if (over(60, 8, 13, 1)) {
                field = 2;
                k = 13;
            } else if (over(18, 14, 40, 4))
                field = 3;
            else if (over(60, 14, 13, 1)) {
                field = 4;
                k = 13;
            } else if (over(17, 20, 13, 1)) {
                field = 5;
                k = 13;
            } else if (over(31, 20, 13, 1)) {
                field = 6;
                break;
            } else if (over(60, 20, 13, 1)) {
                field = 7;
                k = 13;
            }
            pos = field == 0   ? (unsigned)strlen(e->title)
                  : field == 1 ? (unsigned)strlen(run)
                  : field == 3 ? (unsigned)strlen(setup)
                               : 0;
        }
        if (k == 13 && (field == 2 || field == 4)) {
            dest = field == 2 ? run : setup;
            if (*dest && !confirm("The current commands will be overwritten. Proceed?"))
                continue;
            if (browser(chosen)) {
                drive = toupper((unsigned char)chosen[0]);
                p = strrchr(chosen, '\\');
                if (p) {
                    name = p + 1;
                    *p = 0;
                    sprintf(commands, "%c:\nCD %s\n%s%s\n", drive, chosen[2] ? chosen + 2 : "\\",
                            stricmp(strrchr(name, '.') ? strrchr(name, '.') : "", ".BAT") == 0
                                ? "CALL "
                                : "",
                            name);
                    strcpy(dest, commands);
                }
            }
            continue;
        }
        if (k == 13 && field == 7) {
            details(e);
            continue;
        }
        if (k == 13 && (field == 5 || field == 0)) {
            if (lb_blank(e->title)) {
                field = 0;
                pos = 0;
                continue;
            }
            if (!lb_replace(&e->run, run) || !lb_replace(&e->setup, setup)) {
                info(lb_error);
                continue;
            }
            if (index < 0) {
                if (lb_count == LB_MAX) {
                    info("The catalogue is full.");
                    continue;
                }
                lb_entries[lb_count++] = e;
            } else {
                old = lb_entries[index];
                lb_entries[index] = e;
            }
            ok = lb_save(catalog);
            if (!ok) {
                if (index < 0)
                    lb_count--;
                else
                    lb_entries[index] = old;
                info(lb_error);
                continue;
            }
            if (old)
                lb_free(old);
            accept = 1;
            break;
        }
        if (field == 0 || field == 1 || field == 3) {
            dest = field == 0 ? e->title : field == 1 ? run : setup;
            cap = field == 0 ? LB_TITLE : LB_COMMAND;
            input_key(dest, cap, &pos, k, field != 0);
        }
    }
    free(run);
    free(setup);
    if (!accept)
        lb_free(e);
    return accept;
}
static char file_names[256][13], dir_names[256][13];
static int files, dirs;
static int match_pattern(const char *p, const char *s) {
    if (!strcmp(p, "*.*"))
        return 1;
    while (*p) {
        if (*p == '*') {
            p++;
            if (!*p)
                return 1;
            do {
                if (match_pattern(p, s))
                    return 1;
            } while (*s++);
            return 0;
        }
        if (!*s)
            return 0;
        if (*p != '?' && toupper((unsigned char)*p) != toupper((unsigned char)*s))
            return 0;
        p++;
        s++;
    }
    return !*s;
}
static int mask_matches(const char *mask, const char *name) {
    char part[128];
    unsigned i;
    while (*mask) {
        i = 0;
        while (*mask && *mask != ';') {
            if (i < sizeof(part) - 1)
                part[i++] = *mask;
            mask++;
        }
        part[i] = 0;
        if (match_pattern(part, name))
            return 1;
        if (*mask)
            mask++;
    }
    return 0;
}
static int names_compare(const void *a, const void *b) {
    return stricmp((const char *)a, (const char *)b);
}
static void join_path(char *dest, const char *dir, const char *name) {
    strcpy(dest, dir);
    if (dest[strlen(dest) - 1] != '\\')
        strcat(dest, "\\");
    strcat(dest, name);
}
static int scan_dir(const char *dir, const char *mask) {
    struct find_t f;
    char path[160];
    unsigned rc;
    files = dirs = 0;
    join_path(path, dir, "*.*");
    rc = _dos_findfirst(path, _A_NORMAL | _A_RDONLY | _A_HIDDEN | _A_SYSTEM | _A_SUBDIR | _A_ARCH,
                        &f);
    while (!rc) {
        if (f.attrib & _A_SUBDIR) {
            if (strcmp(f.name, ".") && dirs < 256)
                strcpy(dir_names[dirs++], f.name);
        } else if (files < 256 && mask_matches(mask, f.name))
            strcpy(file_names[files++], f.name);
        rc = _dos_findnext(&f);
    }
    qsort(file_names, files, sizeof(file_names[0]), names_compare);
    qsort(dir_names, dirs, sizeof(dir_names[0]), names_compare);
    return files || dirs;
}
static void up_dir(char *dir) {
    char *p = strrchr(dir, '\\');
    if (p && p > dir + 2)
        *p = 0;
    else if (p)
        dir[3] = 0;
}
static int browser(char *result) {
    char dir[128], name[128] = "*.EXE;*.COM;*.BAT", mask[128] = "*.EXE;*.COM;*.BAT", path[160];
    unsigned pos = (unsigned)strlen(name);
    int field = 0, fi = 0, di = 0, ft = 0, dt = 0, k, i, choose = 0, dirty = 1, first = 1;
    FILE *f;
    struct find_t ent;
    strcpy(dir, basepath);
    for (;;) {
        if (dirty) {
            scan_dir(dir, mask);
            fi = di = ft = dt = 0;
            dirty = 0;
        }
        if (fi < ft)
            ft = fi;
        if (fi >= ft + 10)
            ft = fi - 9;
        if (di < dt)
            dt = di;
        if (di >= dt + 8)
            dt = di - 7;
        main_draw(1);
        window(14, 1, 52, 20, "Select Startup File");
        put(16, 4, 10, A_PANEL, "File Name:");
        frame(27, 3, 23, 3, A_PANEL);
        input_draw(28, 4, 21, 1, name, pos, field == 0);
        button(53, 4, 10, "OK", field == 4);
        button(53, 7, 10, "Cancel", field == 5);
        put(17, 6, 33, A_PANEL, dir);
        frame(17, 7, 16, 12, A_PANEL);
        frame(35, 9, 16, 10, A_PANEL);
        for (i = 0; i < 10; i++)
            if (ft + i < files)
                put(18, 8 + i, 14, field == 1 && fi == ft + i ? 0x0f : A_PANEL, file_names[ft + i]);
        for (i = 0; i < 8; i++)
            if (dt + i < dirs)
                put(36, 10 + i, 14, field == 2 && di == dt + i ? 0x0f : A_PANEL, dir_names[dt + i]);
        sprintf(path, "[%c:]  Change drive", dir[0]);
        put(35, 8, 26, field == 3 ? 0x0f : A_PANEL, path);
        k = event();
        choose = 0;
        if (k == -3 || k == -4 || k == 27 || (k == 13 && field == 5))
            return 0;
        if (k == 9 || k == K_SHIFT_TAB) {
            field = (field + (k == 9 ? 1 : 5)) % 6;
            first = 1;
            continue;
        }
        if (k == -1) {
            if (over(28, 4, 21, 1)) {
                field = 0;
                pos = (unsigned)strlen(name);
            } else if (over(52, 4, 12, 1))
                choose = 1;
            else if (over(52, 7, 12, 1))
                return 0;
            else if (over(18, 8, 14, 10) && ft + my - 8 < files) {
                if (field == 1 && fi == ft + my - 8 && clock() - mouse_when < CLOCKS_PER_SEC / 2)
                    choose = 1;
                field = 1;
                fi = ft + my - 8;
                strcpy(name, file_names[fi]);
                pos = (unsigned)strlen(name);
                mouse_when = clock();
            } else if (over(36, 10, 14, 8) && dt + my - 10 < dirs) {
                field = 2;
                di = dt + my - 10;
            } else if (over(35, 8, 26, 1))
                field = 3;
        }
        if (field == 1) {
            if (k == K_UP && fi)
                fi--;
            if (k == K_DOWN && fi + 1 < files)
                fi++;
            if (k == K_HOME)
                fi = 0;
            if (k == K_END && files)
                fi = files - 1;
            if (files && (k == K_UP || k == K_DOWN || k == K_HOME || k == K_END || k == 13)) {
                strcpy(name, file_names[fi]);
                pos = (unsigned)strlen(name);
            }
            if (k == 13 && files)
                choose = 1;
        }
        if (field == 2) {
            if (k == K_UP && di)
                di--;
            if (k == K_DOWN && di + 1 < dirs)
                di++;
            if (k == 13 && dirs) {
                if (!strcmp(dir_names[di], ".."))
                    up_dir(dir);
                else {
                    join_path(path, dir, dir_names[di]);
                    if (strlen(path) < sizeof(dir))
                        strcpy(dir, path);
                }
                dirty = 1;
                continue;
            }
        }
        if (field == 3) {
            if (k == K_LEFT || k == K_RIGHT) {
                int d = dir[0] + (k == K_RIGHT ? 1 : -1);
                if (d < 'A')
                    d = 'Z';
                if (d > 'Z')
                    d = 'A';
                sprintf(dir, "%c:\\", d);
                dirty = 1;
            } else if (k >= 'a' && k <= 'z') {
                sprintf(dir, "%c:\\", toupper(k));
                dirty = 1;
            } else if (k >= 'A' && k <= 'Z') {
                sprintf(dir, "%c:\\", k);
                dirty = 1;
            }
        }
        if (field == 0) {
            if (k >= 32 && k < 256 && first) {
                name[0] = 0;
                pos = 0;
                first = 0;
            }
            if (k != 13)
                input_key(name, sizeof(name), &pos, k, 0);
            else
                choose = 1;
        }
        if (field == 4 && k == 13)
            choose = 1;
        if (choose) {
            if (strchr(name, '*') || strchr(name, '?')) {
                strcpy(mask, name);
                dirty = 1;
                continue;
            }
            if (strlen(name) > 120) {
                info("File name is too long.");
                continue;
            }
            if (strlen(name) + strlen(dir) + 2 >= sizeof(path)) {
                info("Path is too long.");
                continue;
            }
            if (name[1] == ':')
                strcpy(path, name);
            else if (name[0] == '\\')
                sprintf(path, "%c:%s", dir[0], name);
            else
                join_path(path, dir, name);
            if (!_dos_findfirst(path, _A_SUBDIR, &ent) && (ent.attrib & _A_SUBDIR)) {
                if (strlen(path) >= sizeof(dir)) {
                    info("Directory path is too long.");
                    continue;
                }
                strcpy(dir, path);
                dirty = 1;
                continue;
            }
            f = fopen(path, "rb");
            if (!f) {
                info("That file could not be opened. Check the drive and file name.");
                continue;
            }
            fclose(f);
            strcpy(result, path);
            return 1;
        }
    }
}
#define ACT_RUN 1
#define ACT_SETUP 2
#define ACT_BROWSE 3
#define ACT_LOCK 4
#define ACT_EXIT 5
#define ACT_BACK 6
#define ACT_ADD 7
#define ACT_EDIT 8
#define ACT_DELETE 9
#define ACT_ABOUT 10
#define ACT_LOG 11
static int disabled(int a) {
    return ((a == ACT_EDIT || a == ACT_DELETE || a == ACT_ADD) && locked) ||
           ((a == ACT_RUN || a == ACT_SETUP || a == ACT_EDIT || a == ACT_DELETE) &&
            current_item() < 0) ||
           (a == ACT_BACK && !(grouping >= 0 && inside));
}
static void launch_report(void){FILE*f;char a[100]="No launch report yet.",b[160]="";const char*lines[5];f=fopen("C:\\LOCSHELL\\LAUNCH.RES","r");if(f){fgets(a,sizeof a,f);fgets(b,sizeof b,f);fclose(f);}a[strcspn(a,"\r\n")]=0;b[strcspn(b,"\r\n")]=0;lines[0]=a;lines[1]="";lines[2]=b;lines[3]=strlen(b)>65?b+65:"";lines[4]="Full history: C:\\LOCSHELL\\LAUNCH.LOG";text_page("Last launch report",lines,5);}
static int run_commands(const char *s) {
    int item=current_item();
    return run_context(s,learning_run?"LEARN":"RUN",item<0?0:lb_identity(lb_entries[item]),item<0?"Selected command":lb_entries[item]->title);
}
static int run_context(const char*s,const char*kind,unsigned long id,const char*name) {
    if (lb_blank(s)) {
        info("This application does not have any commands assigned to it.");
        return 0;
    }
    if(access("C:\\LOCSHELL\\LEARN.REQ",0)==0){info("Pending learning request: use F7 Reset before launching another session.");return 0;}
    strncpy(pending_job,name,sizeof pending_job-1);pending_job[sizeof pending_job-1]=0;
    if (!network_wait()){pending_job[0]=0;return 0;}
    pending_job[0]=0;
    if(learning_run){int item=current_item();if(item<0||!dl_prepare(lb_identity(lb_entries[item]),s)){ds_net_drain(1);info("Cannot prepare learning. Resolve an earlier LEARN.REQ before retrying.");return 0;}}

    if (!lb_write_run("RUN.BAT", s)) {
        if(learning_run)remove("C:\\LOCSHELL\\LEARN.REQ");ds_net_drain(1);
        info(lb_error);
        return 0;
    }
    if(!lp_prepare(kind,id)){if(learning_run)remove("C:\\LOCSHELL\\LEARN.REQ");ds_net_drain(1);info("Could not prepare a validated launch request.");return 0;}
    launch_app = quit_app = 1;
    return 1;
}
#include "utilities_ui.h"
#include "status_ui.h"
#include "network_ui.h"
#include "dashboard_ui.h"
static void action(int a) {
    int i, j;
    char msg[240], path[160], cmd[512], *p, *name;
    LBEntry *old;
    const char *about[] = {"LOC SHELL 2.3",
                           "",
                           "LoSs oF CoNtRoL",
                           "Sysop: bstar",
                           "",
                           "A native DOS application manager.",
                           "Built from our own C source with Open Watcom.",
                           "",
                           "Use the File, View, Edit and Help menus.",
                           "Start with LOCSHELL.BAT for launch-and-return.",
                           "",
                           "Press Enter or Escape to return."};
    const char *log[] = {"LOC SHELL 2.3",
                         "",
                         "Initial feature-parity build:",
                         "- Existing catalogue and multiline commands",
                         "- Genre, series, developer and publisher groups",
                         "- Release-year and status groups",
                         "- Title, favorite, year and status sorting",
                         "- Add/edit, metadata and Favorite checkbox",
                         "- DOS file selection and separate setup commands",
                         "- Lock and confirmation dialogs",
                         "- Nonresident batch launch-and-return",
                         "- Backups before catalogue changes",
                         "",
                         "- Integrated FTP and complete mTCP utility menu",
                         "- Shared screensavers, optional memory learning",
                         "",
                         "LoSs oF CoNtRoL / bstar"};
    if (disabled(a))
        return;
    i = current_item();
    if (a == ACT_RUN){char mode[9];if(game_boot_requirement(i,mode)){const char*lines[5];char required[80],current[80];sprintf(required,"Restart and select %.19s, then boot mode %s.",memory_profile(),mode);sprintf(current,"Current boot mode: %.8s",memory_mode());lines[0]="This game requires a different memory/boot configuration.";lines[1]=required;lines[2]=current;lines[3]="F4 shows the game's memory and profile requirements.";lines[4]="The game has not been started.";text_page("Game boot mode",lines,5);return;}learning_run=learn_next==lb_identity(lb_entries[i]);
        if(run_commands(lb_str(lb_entries[i]->run)))learn_next=0;learning_run=0;}

    else if (a == ACT_SETUP) {
        if (lb_blank(lb_str(lb_entries[i]->setup)))
            info("This application does not have configuration commands assigned to it.");
        else
            run_context(lb_entries[i]->setup,"SETUP",0,lb_entries[i]->title);
    } else if (a == ACT_BROWSE) {
        if (browser(path)) {
            p = strrchr(path, '\\');
            if (p) {
                name = p + 1;
                *p = 0;
                sprintf(cmd, "%c:\nCD %s\n%s%s\n", path[0], path[2] ? path + 2 : "\\",
                        stricmp(strrchr(name, '.') ? strrchr(name, '.') : "", ".BAT") == 0 ? "CALL "
                                                                                           : "",
                        name);
                run_context(cmd,"FILE",0,name);
            }
        }
    } else if (a == ACT_LOCK) {
        if (confirm(locked ? "LOC SHELL is locked. Unlock it and allow catalogue changes?"
                           : "Lock LOC SHELL to prevent accidental catalogue changes?")) {
            locked = !locked;
            if (!prefs_save()) {
                locked = !locked;
                info("Could not save the lock setting.");
            }
        }
    } else if (a == ACT_EXIT){
        if(network_wait())quit_app = 1;
    }
    else if (a == ACT_BACK)
        back();
    else if (a == ACT_ADD || a == ACT_EDIT) {
        if (editor(a == ACT_ADD ? -1 : i)) {
            rebuild();
            if (a == ACT_EDIT && !(grouping >= 0 && !inside))
                for (j = 0; j < nrows; j++)
                    if (rows[j] == i) {
                        sel = j;
                        break;
                    }
            rebuild();
        }
    } else if (a == ACT_DELETE) {
        sprintf(msg, "Delete \"%.111s\"? This cannot be undone.", lb_entries[i]->title);
        if (confirm(msg)) {
            old = lb_entries[i];
            for (j = i; j < lb_count - 1; j++)
                lb_entries[j] = lb_entries[j + 1];
            lb_count--;
            if (!lb_save(catalog)) {
                for (j = lb_count; j > i; j--)
                    lb_entries[j] = lb_entries[j - 1];
                lb_entries[i] = old;
                lb_count++;
                info(lb_error);
            } else
                lb_free(old);
            rebuild();
        }
    } else if (a == ACT_ABOUT)
        text_page("About LOC SHELL", about, sizeof(about) / sizeof(about[0]));
    else if (a == ACT_LOG)
        text_page("Change Log", log, sizeof(log) / sizeof(log[0]));
}
static int menu(int root) {
    static const char *file[] = {"Run Selected...       Ctrl+R", "Configure Selected... Ctrl+C",
                                 "Browse and Run...     Ctrl+F", "Lock...",
                                 "Exit                  Ctrl+X"};
    static const char *view[] = {"Back", "Arrange By          >", "Sort By             >", "Dashboard            F2"};
    static const char *edit[] = {"Add...                Ctrl+A", "Edit...               Ctrl+E",
                                 "Delete...             Ctrl+D"};
    static const char *help[] = {"About                 Ctrl+B", "Change Log...         Ctrl+L"};
    static const int file_a[] = {1, 2, 3, 4, 5}, edit_a[] = {7, 8, 9}, help_a[] = {10, 11};
    int x, wide = 31, items = 0, choice = 0, sub = -1, sub_choice = 0, k, i, id = 0, go = 0;
    const char **labels;
    char line[64];
    for (;;) {
        x = root == 0 ? 1 : root == 1 ? 8 : root == 2 ? 14 : 20;
        labels = root == 0 ? file : root == 1 ? view : root == 2 ? edit : help;
        items = root == 0 ? 5 : root == 1 ? 4 : root == 2 ? 3 : 2;
        if (choice >= items)
            choice = 0;
        main_draw(1);
        put(x, 1,
            root == 0   ? 4
            : root == 1 ? 4
            : root == 2 ? 4
                        : 4,
            0x0f,
            root == 0   ? "File"
            : root == 1 ? "View"
            : root == 2 ? "Edit"
                        : "Help");
        fill(x + 1, 3, wide, items + 2, 0x08);
        window(x, 2, wide, items + 2, "");
        for (i = 0; i < items; i++) {
            id = root == 0   ? file_a[i]
                 : root == 2 ? edit_a[i]
                 : root == 3 ? help_a[i]
                 : i == 0    ? ACT_BACK
                             : 0;
            strcpy(line, root == 0 && i == 3 && locked ? "Unlock..." : labels[i]);
            put(x + 1, 3 + i, wide - 2, disabled(id) ? 0x78 : i == choice ? 0x0f : 0x70, line);
        }
        if (sub >= 0) {
            int sx = x + wide - 1, sy = 4 + sub, ns = sub == 0 ? 7 : 4;
            window(sx, sy, 23, ns + 2, "");
            for (i = 0; i < ns; i++) {
                const char *s = sub == 0 ? (i < 6 ? group_names[i] : "None")
                                         : (i == 0   ? "Title"
                                            : i == 1 ? "Favorite"
                                            : i == 2 ? "Release Year"
                                                     : "Status");
                sprintf(line, "%c %s",
                        (sub == 0 ? (i < 6 ? grouping == i : grouping < 0) : sort_order == i) ? 7
                                                                                              : ' ',
                        s);
                put(sx + 1, sy + 1 + i, 21, i == sub_choice ? 0x0f : 0x70, line);
            }
        }
        k = event();
        go = 0;
        if (k == -3 || k == -4 || k == 27)
            return 0;
        if (k == K_ALT_F || k == K_ALT_V || k == K_ALT_E || k == K_ALT_H) {
            root = k == K_ALT_F ? 0 : k == K_ALT_V ? 1 : k == K_ALT_E ? 2 : 3;
            choice = 0;
            sub = -1;
            continue;
        }
        if (k == K_UP || k == K_DOWN) {
            int n = sub >= 0 ? (sub == 0 ? 7 : 4) : items;
            int *q = sub >= 0 ? &sub_choice : &choice;
            *q = (*q + (k == K_DOWN ? 1 : n - 1)) % n;
        } else if (k == K_LEFT) {
            if (sub >= 0)
                sub = -1;
            else {
                root = (root + 3) % 4;
                choice = 0;
            }
        } else if (k == K_RIGHT) {
            if (root == 1 && choice > 0 && choice<3 && sub < 0) {
                sub = choice - 1;
                sub_choice = 0;
            } else if (sub < 0) {
                root = (root + 1) % 4;
                choice = 0;
            }
        } else if (k == 13)
            go = 1;
        else if (k == -1) {
            if (my == 1) {
                if (mx >= 1 && mx < 5)
                    root = 0;
                else if (mx >= 8 && mx < 12)
                    root = 1;
                else if (mx >= 14 && mx < 18)
                    root = 2;
                else if (mx >= 20 && mx < 24)
                    root = 3;
                choice = 0;
                sub = -1;
                continue;
            }
            if (sub >= 0 && over(x + wide, 5 + sub, 21, sub == 0 ? 7 : 4)) {
                sub_choice = my - (5 + sub);
                go = 1;
            } else if (over(x + 1, 3, wide - 2, items)) {
                choice = my - 3;
                sub = -1;
                go = 1;
            } else
                return 0;
        }
        if (go) {
            if (sub >= 0) {
                if (sub == 0)
                    choose_group(sub_choice == 6 ? -1 : sub_choice);
                else {
                    sort_order = sub_choice;
                    rebuild();
                }
                return 0;
            }
            if(root==1&&choice==3){shell_view=7;nav_focus=-1;main_valid=0;return 0;}
            if (root == 1 && choice > 0) {
                sub = choice - 1;
                sub_choice = 0;
                continue;
            }
            id = root == 0   ? file_a[choice]
                 : root == 1 ? ACT_BACK
                 : root == 2 ? edit_a[choice]
                             : help_a[choice];
            if (!disabled(id))
                return id;
        }
    }
}
static void prefs_load(void) {
    FILE *f = fopen("LOCSHELL.CFG", "rt");
    char line[160];
    unsigned long selected_id = 0;
    int i,dashboard_seen=0;
    if (!f)
        return;
    while (fgets(line, sizeof(line), f)) {
        if(!strncmp(line,"DASHBOARD=1",11))dashboard_seen=1;
        else if(!strncmp(line,"VIEW=",5)){int v=atoi(line+5);if(v>=0&&v<8)shell_view=v;}
        else if (!strncmp(line, "LOCK=", 5))
            locked = atoi(line + 5) != 0;
        else if (!strncmp(line, "SORT=", 5))
            sort_order = atoi(line + 5);
        else if (!strncmp(line, "GROUP=", 6))
            grouping = atoi(line + 6);
        else if (!strncmp(line, "INSIDE=", 7))
            inside = atoi(line + 7) != 0;
        else if (!strncmp(line, "VALUE=", 6)) {
            line[strcspn(line, "\r\n")] = 0;
            strncpy(group_value, line + 6, sizeof(group_value) - 1);
        } else if (!strncmp(line, "SELECT=", 7))
            selected_id = strtoul(line + 7, 0, 16);
    }
    fclose(f);
    if(!dashboard_seen)shell_view=7;
    if (sort_order < 0 || sort_order > 3)
        sort_order = 0;
    if (grouping < -1 || grouping > 5)
        grouping = -1;
    rebuild();
    if (selected_id && !(grouping >= 0 && !inside))
        for (i = 0; i < nrows; i++)
            if (lb_identity(lb_entries[rows[i]]) == selected_id) {
                sel = i;
                break;
            }
    rebuild();
}
static int prefs_save(void) {
    FILE *f = fopen("LOCSHELL.NEW", "wt");
    int i = current_item(), ok;
    if (!f)
        return 0;
    fprintf(f, "LOC SHELL 1\nDASHBOARD=1\nVIEW=%d\nLOCK=%d\nSORT=%d\nGROUP=%d\nINSIDE=%d\nVALUE=%s\nSELECT=%08lx\n",
            shell_view, locked, sort_order, grouping, inside, group_value,
            i < 0 ? 0UL : lb_identity(lb_entries[i]));
    ok = !ferror(f);
    if (fclose(f))
        ok = 0;
    if (!ok) {
        remove("LOCSHELL.NEW");
        return 0;
    }
    remove("LOCSHELL.BAK");
    if (access("LOCSHELL.CFG", 0) == 0 && rename("LOCSHELL.CFG", "LOCSHELL.BAK"))
        return 0;
    if (rename("LOCSHELL.NEW", "LOCSHELL.CFG")) {
        rename("LOCSHELL.BAK", "LOCSHELL.CFG");
        return 0;
    }
    return 1;
}
#ifdef DS_NETWORK_EMBEDDED
#define main ds_shell_main
#endif
void ds_shell_boot(const char*text){if(!screen_active)init_screen();main_draw(1);window(10,8,60,7,"LOC SHELL");put(13,11,54,A_PANEL,text);present();}
/* Catalogue strings can live in DOS upper memory. Scope the allocation policy
   to loading them, restoring both DOS settings before networking or launching. */
static int load_catalog(void) {
    union REGS r;
    unsigned strategy=0, linked=0;
    int changed=0, ok;
    r.x.ax=0x5800;int86(0x21,&r,&r);
    if(!r.x.cflag) {
        strategy=r.x.ax;
        r.x.ax=0x5802;int86(0x21,&r,&r);
        if(!r.x.cflag) {
            linked=r.h.al;
            r.x.ax=0x5803;r.x.bx=1;int86(0x21,&r,&r);
            if(!r.x.cflag) {
                changed=1;
                r.x.ax=0x5801;r.x.bx=0x80|(strategy&3);int86(0x21,&r,&r);
            }
        }
    }
    ok=lb_load(catalog);
    if(changed) {
        r.x.ax=0x5801;r.x.bx=strategy;int86(0x21,&r,&r);
        r.x.ax=0x5803;r.x.bx=linked;int86(0x21,&r,&r);
    }
    return ok;
}
int main(int argc, char **argv) {
    int k, i, a, iarg, check = 0, from_batch = 0;
    const char *s;
    for (iarg = 1; iarg < argc; iarg++) {
        if (!stricmp(argv[iarg], "/CHECK"))
            check = 1;
        else if (!stricmp(argv[iarg], "/FROMBATCH"))
            from_batch = 1;
        else if (!strnicmp(argv[iarg], "/CAT:", 5)) {
            if (strlen(argv[iarg] + 5) >= sizeof(catalog)) {
                puts("Catalogue path is too long.");
                return 2;
            }
            strcpy(catalog, argv[iarg] + 5);
        }
    }
    if (!getcwd(basepath, sizeof(basepath))) {
        puts("Cannot read the application directory.");
        return 2;
    }
    if (!load_catalog()) {
        puts(lb_error);
        return 2;
    }
    if (check) {
        dm_load("MEMORY.TSV"); dm_query();
        printf("MEMORY launch_bytes=%lu own_bytes=%lu conv_valid=%u EMS_KB=%u XMS_largest_KB=%u XMS_free_KB=%u\n",dm_memory.launch_bytes,dm_memory.own_bytes,dm_memory.conv_valid,dm_memory.ems_kb,dm_memory.xms_largest_kb,dm_memory.xms_free_kb);
        printf("LOC SHELL: %d catalogue entries\n", lb_count);
        printf("PROFILE %s / %s\n",memory_profile(),memory_mode());dl_index();
        for (i = 0; i < lb_count; i++) {
            MemoryState state;memory_ids[i]=lb_identity(lb_entries[i]);memory_profiles[i]=dm_find(memory_ids[i]);memory_state(i,&state);
            printf("%03d [%c] %08lx %s | %s%s\n", i, dm_status(memory_profiles[i], &dm_memory), memory_ids[i], lb_entries[i]->title,state.label,state.review?" (setup review)":"");
        }
        lb_clear();
        return 0;
    }
    dm_load("MEMORY.TSV");
    dm_query(); memory_loaded=1; memory_tick=*(unsigned long __far*)MK_FP(0x40,0x6c);
    prefs_load();
    {FILE*f=fopen("C:\\LOCSHELL\\LEARN.ARM","r");if(f){fscanf(f,"%lx",&learn_next);fclose(f);remove("C:\\LOCSHELL\\LEARN.ARM");}}
    dl_index();
    rebuild();
    if(!screen_active)init_screen();
    {FILE*f=fopen("C:\\LOCSHELL\\LAUNCH.RES","r");char stage[100],detail[160];if(f){if(fgets(stage,sizeof stage,f)){stage[strcspn(stage,"\r\n")]=0;strncpy(notice,stage,sizeof notice-1);if(access("C:\\LOCSHELL\\LAUNCH.ASK",0)==0){if(fgets(detail,sizeof detail,f))info(detail);remove("C:\\LOCSHELL\\LAUNCH.ASK");}}fclose(f);}}
    learning_return();
    while (!quit_app) {
        main_draw(0);
        k = event();
        notice[0] = 0;
        a = 0;
        if(k==-3){quit_app=1;break;}
        if(k==-4){if(network_wait()){ds_net_restart_ready();quit_app=1;break;}continue;}
        if(k==-2)continue;
        if(k==323){launch_report();main_valid=0;continue;}
        if(k==316){shell_view=7;nav_focus=-1;main_valid=0;continue;}
        if(k==322){shell_view=(shell_view+1)%8;nav_focus=-1;main_valid=0;continue;}
        if(k==-1&&over(76,0,3,1)){action(ACT_EXIT);continue;}
        if(k==-1&&my==1&&mx>=1&&mx<24){
            a=menu(mx<8?0:mx<14?1:mx<20?2:3);
            if(a)action(a);
            continue;
        }
        if(k==-1){
            if(view_click())continue;
            if(nav_focus>=0){nav_focus=-1;main_valid=0;}
        }
        if(k==320){shell_view=1;nav_focus=-1;main_valid=0;continue;}
        if(shell_view!=7&&view_input(k))continue;
        if (k == K_ALT_F || k == K_ALT_V || k == K_ALT_E || k == K_ALT_H) {
            a = menu(k == K_ALT_F ? 0 : k == K_ALT_V ? 1 : k == K_ALT_E ? 2 : 3);
            if (a)
                action(a);
            continue;
        }
        if(shell_view==7&&k!=24){dashboard_input(k);continue;}
        if(shell_view&&k!=24){network_input(k);continue;}
        if(k==318){memory_details();main_valid=0;continue;}
        if(k==321){learning_page();main_valid=0;continue;}
        if(k==320){network_page(0);main_valid=0;continue;}
        if(k==319){dm_query();dl_index();main_valid=0;strcpy(notice,"Memory and recorded setups refreshed.");continue;}
        if (k == 24 || k == 27)
            a = ACT_EXIT;
        else if (k == 18)
            a = ACT_RUN;
        else if (k == 3)
            a = ACT_SETUP;
        else if (k == 6)
            a = ACT_BROWSE;
        else if (k == 1)
            a = ACT_ADD;
        else if (k == 5)
            a = ACT_EDIT;
        else if (k == 4)
            a = ACT_DELETE;
        else if (k == 2 || k == K_F1)
            a = ACT_ABOUT;
        else if (k == 12)
            a = ACT_LOG;
        else if (k == 8)
            a = ACT_BACK;
        else if (k == K_UP) {
            sel--;
            nav_focus = -1;
        } else if (k == K_DOWN) {
            sel++;
            nav_focus = -1;
        } else if (k == K_PGUP)
            sel -= NROWS;
        else if (k == K_PGDN)
            sel += NROWS;
        else if (k == K_HOME)
            sel = 0;
        else if (k == K_END)
            sel = nrows - 1;
        else if (k == 13) {
            if (grouping >= 0 && !inside && nrows) {
                strcpy(group_value, groups[sel]);
                group_sel = sel;
                inside = 1;
                sel = scroll = 0;
                rebuild();
            } else
                a = ACT_RUN;
        } else if (k >= 32 && k < 127) {
            for (i = 0; i < nrows; i++) {
                s = grouping >= 0 && !inside ? groups[i] : lb_entries[rows[i]]->title;
                if (toupper((unsigned char)*s) == toupper(k)) {
                    sel = i;
                    break;
                }
            }
        } else if (k == -1) {
            if (over(76, 0, 3, 1))
                a = ACT_EXIT;
            else if (over(LIST_X, LIST_Y, LIST_W, NROWS) && scroll + my - LIST_Y < nrows) {
                i = scroll + my - LIST_Y;
                if (mouse_row == i && clock() - mouse_when < CLOCKS_PER_SEC / 2) {
                    if (grouping >= 0 && !inside) {
                        strcpy(group_value, groups[i]);
                        group_sel = i;
                        inside = 1;
                        sel = scroll = 0;
                        rebuild();
                    } else {
                        sel = i;
                        a = ACT_RUN;
                    }
                    mouse_row = -1;
                } else {
                    sel = i;
                    mouse_row = i;
                    mouse_when = clock();
                }
                nav_focus = -1;
            } else if (mx == SCROLL_X && my == LIST_Y - 1)
                sel--;
            else if (mx == SCROLL_X && my == LIST_Y + NROWS)
                sel++;
        }
        if (a)
            action(a);
        selection_clamp();
    }
    if (!prefs_save()) {
        finish_screen();
        puts("Could not save LOC SHELL preferences; the catalogue remains intact.");
        groups_clear();
        lb_clear();
        return 2;
    }
    {FILE*f;if(learn_next){f=fopen("C:\\LOCSHELL\\LEARN.ARM","w");if(f){fprintf(f,"%08lx\n",learn_next);fclose(f);}}else remove("C:\\LOCSHELL\\LEARN.ARM");}
    finish_screen();
    groups_clear();
    lb_clear();
    if (launch_app && !from_batch) {
        puts("Commands are ready in RUN.BAT. Use LOCSHELL.BAT for automatic launch-and-return.");
        return 1;
    }
    return launch_app ? 1 : 0;
}
