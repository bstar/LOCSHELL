/* Native DOS acceptance tests for partial painting and navigation cost. */
#define LOCSHELL_RENDER_TEST
#define main locshell_program_main
#include "../../src/locshell/locshell.c"
#undef main

static FILE *report;
static unsigned short __far reference[2000];
static int checks;
static int text_at(unsigned x,unsigned y,const char*s){while(*s){if((screen[y*80+x++]&255)!=(unsigned char)*s++)return 0;}return 1;}

static void require(int condition, const char *message) {
    checks++;
    if (!condition) {
        finish_screen();
        fprintf(report, "FAIL: %s\n", message);
        fclose(report);
        exit(1);
    }
}

static void same_as_full_draw(void) {
    int i;
    memcpy(reference, screen, sizeof(reference));
    main_valid = 0;
    main_draw(0);
    for (i = 0; i < 2000; i++)
        if (reference[i] != screen[i]) {
            fprintf(report, "DIFF check=%d x=%d y=%d partial=%04x full=%04x sel=%d scroll=%d group=%d tab=%d\n",
                    checks, i % 80, i / 80, reference[i], screen[i], sel, scroll, grouping, nav_focus);
            break;
        }
    require(!memcmp(reference, screen, sizeof(reference)),
            "partial frame differs from a complete redraw");
}

int main(void) {
    unsigned long before, rebuild_count, words;
    clock_t started, duration;
    int i, g, j;
    report = fopen("RENDER.TXT", "wt");
    if (!report) return 2;
    require(lb_load("LIBRARY.TXT"), "catalogue must load");
    require(lb_count > NROWS + 1, "acceptance catalogue must span pages");
    grouping = -1;
    shell_view = 0;
    sel = scroll = 0;
    rebuild();
    init_screen();
    main_draw(0);
    present();
    for (i = 0; i < 2000 && (screen[i] & 0xf000) != 0; i++) {}
    require(i == 2000, "full-screen layout must not leave black desktop margins");
    require(text_at(1,23,"Connections:")&&text_at(1,24,"Active job:"),"both status rows must be present");
    {
        union REGS r;
        unsigned short saved;
        require(has_mouse, "mouse driver must be available in acceptance emulator");
        r.x.cx = 160; r.x.dx = 80; mouse_fn(4, &r);
        saved = displayed[820];
        require(vram[820] == (saved ^ 0x7700), "software mouse pointer must be visible");
        ds_shell_mouse_suspend(1);
        require(vram[820] == saved, "hide mouse before saving the screensaver background");
        /* Reproduce an older driver's visibility reset across a mode switch. */
        mouse_fn(0, &r);
        ds_shell_mouse_suspend(0);
        require(vram[820] == (saved ^ 0x7700), "restore pointer after driver visibility reset");
        ds_shell_mouse_suspend(0);
        ds_shell_mouse_suspend(1);
        require(vram[820] == saved, "repeated resume must not unbalance show/hide calls");
        ds_shell_mouse_suspend(0);
    }
    before = video_words_written;
    main_draw(0);
    present();
    require(video_words_written == before, "unchanged frame must write no video cells");

    rebuild_count = catalog_rebuilds;
    sel = 1;
    selection_clamp();
    before = video_words_written;
    main_draw(0);
    present();
    require(video_words_written - before > 0 && video_words_written - before <= LIST_W * 2UL + CONTENT_W,
            "one row move must touch only list rows and selection detail");
    require(catalog_rebuilds == rebuild_count, "row movement must not sort the catalogue");
    same_as_full_draw();

    video_words_written = 0;
    started = clock();
    for (i = 0; i < 256; i++) {
        sel = i & 1;
        selection_clamp();
        main_draw(0);
        present();
    }
    duration = clock() - started;
    words = video_words_written;
    fprintf(report, "BENCH iterations=256 clock_ticks=%lu clock_hz=%lu video_words=%lu rebuilds=%lu\n",
            (unsigned long)duration, (unsigned long)CLOCKS_PER_SEC, words,
            catalog_rebuilds - rebuild_count);
    require(catalog_rebuilds == rebuild_count, "navigation benchmark must not rebuild");

    for (i = 0; i < 40; i++) {
        sel++;
        selection_clamp();
        before = video_words_written;
        main_draw(0);
        present();
        require(video_words_written - before <= LIST_W * (unsigned long)NROWS + CONTENT_W + 2,
                "scroll must stay inside the list and scrollbar");
        same_as_full_draw();
    }
    sel = nrows - 1;
    selection_clamp();
    main_draw(0);
    present();
    same_as_full_draw();
    sel += 20;
    selection_clamp();
    before = video_words_written;
    main_draw(0);
    present();
    require(video_words_written == before, "End boundary must not repaint");
    sel = -20;
    selection_clamp();
    main_draw(0);
    present();
    same_as_full_draw();

    for (i = 0; i < 14; i++) {
        nav_focus = i;
        main_draw(0);
        present();
        same_as_full_draw();
    }
    nav_focus = -1;
    require(view_input(9) && nav_focus == 0, "Tab must focus the current section");
    require(view_input(K_DOWN) && nav_focus == 1, "arrows must move within the sidebar");
    require(view_input(13) && shell_view == 1 && nav_focus == -1,
            "Enter must open the section and return focus to its controls");
    mx = 2; my = 13;
    require(view_click() && shell_view == 0 && grouping == 0,
            "clicking Genres must open game groups from any section");
    mx = 2; my = 12;
    require(view_click() && grouping == -1, "All games must restore the full catalogue");
    mx = 2; my = 1;
    require(!view_click(), "sidebar must not intercept the top application menus");
    main_draw(0); present();
    same_as_full_draw();
    strcpy(notice, "A status message to clear");
    main_draw(0);
    present();
    same_as_full_draw();
    notice[0] = 0;
    main_draw(0);
    present();
    same_as_full_draw();

    for (g = 0; g < LB_META; g++) {
        choose_group(g);
        main_draw(0);
        present();
        same_as_full_draw();
        require(nrows > 0, "group list must contain a value");
        sel = nrows - 1;
        selection_clamp();
        main_draw(0);
        present();
        same_as_full_draw();
        strcpy(group_value, groups[sel]);
        group_sel = sel;
        inside = 1;
        sel = scroll = 0;
        rebuild();
        main_draw(0);
        present();
        same_as_full_draw();
        for (j = 0; j < 4; j++) {
            sort_order = j;
            rebuild();
            main_draw(0);
            present();
            same_as_full_draw();
        }
        back();
        main_draw(0);
        present();
        same_as_full_draw();
    }
    choose_group(-1);
    main_draw(0);
    present();
    memcpy(reference, screen, sizeof(reference));
    before = video_words_written;
    main_draw(1);
    window(2, 8, 76, 7, "Acceptance dialog");
    put(4, 10, 72, A_PANEL, "The background must not be committed before this dialog.");
    button(35, 13, 9, "OK", 1);
    require(video_words_written == before, "dialog assembly must not write to video memory");
    present();
    require(text_at(1,23,"Connections:")&&text_at(1,24,"Active job:"),"dialogs must retain the status rows");
    main_draw(0);
    present();
    require(!memcmp(reference, screen, sizeof(reference)), "closing dialog must restore main frame");

    main_draw(1);
    window(4, 4, 70, 17, "Cursor acceptance");
    input_draw(6, 6, 20, 1, "Edit me", 3, 1);
    present();
    require(cursor_visible == 1 && drawn_cursor_x == 9 && drawn_cursor_y == 6,
            "focused edit field must show its cursor");
    main_draw(0);
    present();
    require(cursor_visible == 0, "list navigation must hide the edit cursor");

    lb_clear();
    groups_clear();
    grouping = -1;
    inside = 0;
    sel = scroll = 0;
    rebuild();
    main_draw(0);
    present();
    same_as_full_draw();
    require(sel == 0 && scroll == 0 && nrows == 0, "empty catalogue selection must stay valid");
    finish_screen();
    fprintf(report, "PASS checks=%d\n", checks);
    fclose(report);
    return 0;
}
