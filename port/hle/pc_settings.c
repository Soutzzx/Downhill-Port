/* PC SETTINGS: the page the front end's OPTIONS screen opens (item 9, port/patches.py patches
   func_001F0160), a page in the game's own popup menu style (menu_page.c, finding 050) as its
   AUDIO / OTHER page is; up/down, the cursor, the font, the panel and the sounds are the game's:
   - left/right change the row under the cursor; Cross (SELECT) keeps the changes and closes,
     Triangle (BACK) puts the values the page opened with back and closes, as AUDIO / OTHER does.
   Every change is saved to dhd.ini at once (host_setting_save keeps the other lines). Display,
   window size, picture shape, scaling, skip movies (the startup movies) and vibration apply at
   once; resolution, widescreen, the texture pack and the font at
   the next start ("*" and the footer say so). A setting the environment
   sets (DHD_*) is shown but can't be changed here. Nothing here writes game state: the page
   lives in host memory and the settings in dhd.ini, never in the memory card's save. */
#include "hle.h"
#include "menu_page.h"

#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <string.h>

void gs_window_apply(void);           /* port/gs/present.c */
void gs_present_options_apply(void);
int texpack_available(void);          /* port/gs/texpack.c */

enum { ROWS = 11 };

typedef struct { const char *text, *value; } Choice;
typedef struct {
    const char *label, *key, *env, *fallback;
    const Choice *choices;
    int count, live;
    int selected, running, opened;    /* choice indexes; -1: a value not in the list */
    char text[64];
} Row;

static const Choice scales[] = {{"1x", "1"}, {"2x", "2"}, {"3x", "3"}, {"4x", "4"}, {"Window", "window"}};
static const Choice displays[] = {{"Window", "windowed"}, {"Borderless", "borderless"}, {"Fullscreen", "fullscreen"}};
static const Choice sizes[] = {{"896x672", "896x672"}, {"1280x960", "1280x960"}, {"1440x1080", "1440x1080"},
                               {"1280x720", "1280x720"}, {"1920x1080", "1920x1080"}};
/* Wide: widescreen's shape (the monitor's, port/hle/widescreen.c); 4:3 with widescreen off. */
static const Choice shapes[] = {{"4:3", "4:3"}, {"Wide", "wide"}, {"16:9", "16:9"}, {"Stretch", "stretch"}, {"Pixels", "pixels"}};
static const Choice filters[] = {{"Smooth", "linear"}, {"Sharp", "point"}};
static const Choice switches[] = {{"OFF", "0"}, {"ON", "1"}};
static const Choice toggles[] = {{"OFF", "off"}, {"ON", "on"}};
static const Choice fonts[] = {{"Original", "original"}, {"Sharp", "sharp"}};
static const Choice rates[] = {{"60", "60"}, {"120", "120"}, {"144", "144"}, {"Monitor", "monitor"}};

#define CHOICES(c) c, (int)(sizeof c / sizeof *c)
enum { RESOLUTION, DISPLAY, WINDOW_SIZE, SHAPE, SCALING, WIDESCREEN, MOVIES, VIBRATION, TEXTURES, FONT };
static Row rows[ROWS] = {
    {"Resolution:", "scale", "DHD_SCALE", "2", CHOICES(scales), 0},
    {"Display:", "window", "DHD_WINDOW", "windowed", CHOICES(displays), 1},
    {"Window Size:", NULL, "DHD_WINDOW_WIDTH", "896x672", CHOICES(sizes), 1},  /* width and height */
    {"Picture:", "DHD_ASPECT", "DHD_ASPECT", "4:3", CHOICES(shapes), 1},
    {"Scaling:", "DHD_PRESENT_FILTER", "DHD_PRESENT_FILTER", "linear", CHOICES(filters), 1},
    {"Widescreen:", "widescreen", "DHD_WIDESCREEN", "0", CHOICES(switches), 0},
    {"Skip Movies:", "skip_movies", "DHD_SKIP_MOVIES", "0", CHOICES(switches), 1},  /* the startup movies */
    {"Vibration:", "vibration", "DHD_VIBRATION", "1", CHOICES(switches), 1},
    {"Texture Pack:", "textures", "DHD_TEXTURES", "on", CHOICES(toggles), 0},  /* port/gs/texpack.c */
    {"Font:", "font", "DHD_FONT", "sharp", CHOICES(fonts), 0},  /* port/gs/font_sharp.c */
    {"Frame Rate:", "fps", "DHD_FPS", "60", CHOICES(rates), 0},
};

static MenuPage page;
static int loaded;
static char footer[128];
enum { VIEW_PC, VIEW_MENU_KEYS, VIEW_GAME_KEYS_FIRST, VIEW_GAME_KEYS_MORE };
static int input_view, capture_index = -1, capture_phase, capture_menu;
static char game_text[19][80], menu_text[8][80];
static const char *const menu_game_next = "GAMEPLAY BINDINGS  >";
static const char *const menu_restore = "Restore menu defaults";
static const char *const menu_back = "Back to PC settings";
static const char *const game_more = "MORE GAMEPLAY BINDINGS  >";
static const char *const game_back = "Back to menu bindings";
static const char *const game_restore = "Restore gameplay defaults";
static const char *const game_prev = "Previous gameplay bindings";

static void save_bindings(void)
{
    for (int i = 0; i < keyboard_bind_count(); i++) {
        char key[24], value[16];
        snprintf(key, sizeof key, "bind_%02d", i);
        snprintf(value, sizeof value, "%d", keyboard_bind_key(i));
        if (!host_setting_save(key, value)) hle_log("bindings: failed to save %s\n", key);
    }
    for (int i = 0; i < keyboard_menu_count(); i++) {
        char key[24], value[16];
        snprintf(key, sizeof key, "menu_bind_%02d", i);
        snprintf(value, sizeof value, "%d", keyboard_menu_key(i));
        if (!host_setting_save(key, value)) hle_log("bindings: failed to save %s\n", key);
    }
}
static int game_focused(void)
{
    HWND foreground = GetForegroundWindow();
    DWORD process = 0;
    if (foreground) GetWindowThreadProcessId(foreground, &process);
    return process == GetCurrentProcessId();
}
static int capture_down(void)
{
    if (!game_focused()) return 0;
    /* Check mouse first: these virtual-key codes are below the old 8..255
       scan range. Use an edge-free snapshot; capture_phase waits for release. */
    const int mouse[] = {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};
    for (size_t i = 0; i < sizeof mouse / sizeof mouse[0]; i++)
        if (GetAsyncKeyState(mouse[i]) & 0x8000) return mouse[i];
    /* Prefer the physical sides of Ctrl over its generic alias. */
    if (GetAsyncKeyState(VK_LCONTROL) & 0x8000) return VK_LCONTROL;
    if (GetAsyncKeyState(VK_RCONTROL) & 0x8000) return VK_RCONTROL;
    for (int vk = 8; vk < 255; vk++)
        if ((keyboard_bind_valid(vk) || vk == VK_ESCAPE) && (GetAsyncKeyState(vk) & 0x8000))
            return vk;
    return 0;
}
static void capture_stop(void)
{
    capture_index = -1;
    capture_phase = 0;
    keyboard_bind_capture(0);
}
static void view_set(MenuPage *p, int view, int cursor)
{
    capture_stop();
    input_view = view;
    p->title = view == VIEW_PC ? "PC SETTINGS" :
               view == VIEW_MENU_KEYS ? "MENU BINDINGS" : "GAMEPLAY BINDINGS";
    menu_page_rebuild_at(p, cursor);
}
static void start_capture(MenuPage *p, int index, int is_menu)
{
    int count = is_menu ? keyboard_menu_count() : keyboard_bind_count();
    if (index < 0 || index >= count) return;
    capture_index = index;
    capture_menu = is_menu;
    capture_phase = 0;
    keyboard_bind_capture(1);
    p->footer = "Release keys, then press a new key (Esc cancels)";
}
static void update_bindings(void)
{
    for (int i = 0; i < keyboard_menu_count(); i++)
        snprintf(menu_text[i], sizeof menu_text[i], "%-19.19s %s",
                 keyboard_menu_action(i), keyboard_bind_name(keyboard_menu_key(i)));
    for (int i = 0; i < keyboard_bind_count(); i++)
        snprintf(game_text[i], sizeof game_text[i], "%-19.19s %s",
                 keyboard_bind_action(i), keyboard_bind_name(keyboard_bind_key(i)));
}

/* The value the program runs with now: the variable (set from dhd.ini or the environment). */
static void current(const Row *r, char *out, size_t size)
{
    if (r == &rows[WINDOW_SIZE]) {
        const char *w = getenv("DHD_WINDOW_WIDTH"), *h = getenv("DHD_WINDOW_HEIGHT");
        snprintf(out, size, "%dx%d", w && atoi(w) > 0 ? atoi(w) : 896, h && atoi(h) > 0 ? atoi(h) : 672);
        return;
    }
    const char *v = getenv(r->env);
    snprintf(out, size, "%s", v && *v ? v : r->fallback);
    if (r == &rows[WIDESCREEN] || r == &rows[MOVIES] || r == &rows[VIBRATION])
        snprintf(out, size, "%d", atoi(out) != 0);
    if (r == &rows[TEXTURES])
        snprintf(out, size, "%s", v && (!_stricmp(v, "off") || !strcmp(v, "0") || !_stricmp(v, "false") || !_stricmp(v, "no"))
                 ? "off" : "on");
    if (r == &rows[FONT])  /* as font_sharp_on reads it */
        snprintf(out, size, "%s", v && (!_stricmp(v, "original") || !_stricmp(v, "off") || !strcmp(v, "0")) ? "original" : "sharp");
}

static int find(const Row *r, const char *value)
{
    for (int i = 0; i < r->count; i++)
        if (!_stricmp(r->choices[i].value, value)) return i;
    return -1;
}

static int locked(const Row *r)
{
    return host_setting_locked(r->env) || (r == &rows[WINDOW_SIZE] && host_setting_locked("DHD_WINDOW_HEIGHT"));
}

static void format(Row *r)
{
    char value[32];
    if (r->selected >= 0) snprintf(value, sizeof value, "%s", r->choices[r->selected].text);
    else current(r, value, sizeof value);
    /* the game's own layout: the label padded, two tabs, the value ("Music Volume: \t\t1024") */
    snprintf(r->text, sizeof r->text, "%-14s\t\t%s%s", r->label, value, r->selected != r->running ? " *" : "");
}

/* Saves the row's choice and, for a live row, applies it. */
static void commit(Row *r, int apply)
{
    const Choice *c = &r->choices[r->selected];
    if (r == &rows[WINDOW_SIZE]) {
        char w[16], h[16];
        sscanf(c->value, "%15[0-9]x%15[0-9]", w, h);
        host_setting_save("width", w);
        host_setting_save("height", h);
        if (apply) { _putenv_s("DHD_WINDOW_WIDTH", w); _putenv_s("DHD_WINDOW_HEIGHT", h); }
    } else {
        host_setting_save(r->key, c->value);
        if (apply) _putenv_s(r->env, c->value);
    }
    if (!apply) return;
    r->running = r->selected;
    if (r == &rows[DISPLAY] || r == &rows[WINDOW_SIZE]) gs_window_apply();
    else if (r == &rows[SHAPE] || r == &rows[SCALING]) gs_present_options_apply();
    else if (r == &rows[VIBRATION]) host_pad_vibrate(0, 0, 0);
}

static void change(int index, int step)
{
    if (index < 0 || index >= ROWS) return;
    Row *r = &rows[index];
    if (locked(r)) return;
    r->selected = r->selected < 0 ? (step > 0 ? 0 : r->count - 1) : (r->selected + step + r->count) % r->count;
    menu_page_sound(step > 0);
    commit(r, r->live);
    /* Widescreen brings its picture shape (the monitor's, "wide"); saved for the next start,
       when the wider view comes, not shown stretched now. */
    if (r == &rows[WIDESCREEN] && !locked(&rows[SHAPE])) {
        rows[SHAPE].selected = find(&rows[SHAPE], r->selected ? "wide" : "4:3");
        commit(&rows[SHAPE], 0);
        format(&rows[SHAPE]);
    }
    format(r);
    hle_log("pc settings: %s %s\n", r->label, r->choices[r->selected].value);
}

/* ---- the page ---- */

static void page_build(MenuPage *p)
{
    update_bindings();
    if (input_view == VIEW_MENU_KEYS) {
        for (int i = 0; i < keyboard_menu_count(); i++) menu_page_row(p, menu_text[i], 1);
        menu_page_row(p, menu_game_next, 1);
        menu_page_row(p, menu_restore, 1);
        menu_page_row(p, menu_back, 1);
    } else if (input_view == VIEW_GAME_KEYS_FIRST) {
        for (int i = 0; i < 10; i++) menu_page_row(p, game_text[i], 1);
        menu_page_row(p, game_more, 1);
        menu_page_row(p, game_back, 1);
    } else if (input_view == VIEW_GAME_KEYS_MORE) {
        for (int i = 10; i < keyboard_bind_count(); i++) menu_page_row(p, game_text[i], 1);
        menu_page_row(p, game_restore, 1);
        menu_page_row(p, game_prev, 1);
    } else {
        for (int i = 0; i < ROWS; i++) {
            format(&rows[i]);
            menu_page_row(p, rows[i].text, 1);
        }
        menu_page_row(p, "KEYBOARD BINDINGS  >", 1);
    }
}

static void page_update(MenuPage *p, int cursor)
{
    if (input_view != VIEW_PC) {
        if (capture_index >= 0) {
            int key = capture_down();
            if (capture_phase == 0) {
                if (!key) capture_phase = 1;
            } else if (capture_phase == 1 && key) {
                if (key == VK_ESCAPE) {
                    capture_phase = 2; /* cancel safely */
                } else if (capture_menu && key >= VK_LBUTTON && key <= VK_XBUTTON2) {
                    /* Menu actions stay keyboard/controller-only; user can
                       press another key without leaving the capture screen. */
                    p->footer = "Mouse buttons: gameplay bindings only";
                } else {
                    if (capture_menu ? keyboard_menu_set(capture_index, key) :
                                       keyboard_bind_set(capture_index, key))
                        save_bindings();
                    capture_phase = 2;
                }
            } else if (capture_phase == 2 && !key) {
                int index = capture_index;
                int row = capture_menu || input_view == VIEW_GAME_KEYS_FIRST ? index : index - 10;
                capture_stop();
                menu_page_rebuild_at(p, row);
            }
            if (!(capture_phase == 1 && key >= VK_LBUTTON &&
                  key <= VK_XBUTTON2 && capture_menu))
                p->footer = capture_phase == 0 ? "Release all keys" :
                            capture_phase == 1 ? "Press key/mouse, Esc cancels" : "Release key...";
        } else if (input_view == VIEW_MENU_KEYS && cursor == keyboard_menu_count()) {
            p->footer = "Gameplay can use the same keys as the menu";
        } else {
            p->footer = "Cross: change key / Esc: back";
        }
        update_bindings();
        return;
    }
    if (cursor == ROWS) { p->footer = "Change menu keys and gameplay keys separately"; return; }
    const Row *r = cursor >= 0 && cursor < ROWS ? &rows[cursor] : NULL;
    p->footer = NULL;
    if (r && locked(r)) {
        snprintf(footer, sizeof footer, "Set by %s outside the game", r == &rows[WINDOW_SIZE] ? "DHD_WINDOW_WIDTH" : r->env);
        p->footer = footer;
    } else if (r && r->selected != r->running) p->footer = "* Applies when the game starts again";
    else if (r == &rows[TEXTURES] && !texpack_available()) p->footer = "No pack: see texture_pack in dhd.ini";
}

static void page_back(MenuPage *p)
{
    if (capture_index >= 0) {
        int index = capture_index;
        int row = capture_menu || input_view == VIEW_GAME_KEYS_FIRST ? index : index - 10;
        capture_stop();
        menu_page_rebuild_at(p, row);
        return;
    }
    if (input_view == VIEW_MENU_KEYS) { view_set(p, VIEW_PC, ROWS); return; }
    if (input_view == VIEW_GAME_KEYS_FIRST) { view_set(p, VIEW_MENU_KEYS, 8); return; }
    if (input_view == VIEW_GAME_KEYS_MORE) { view_set(p, VIEW_GAME_KEYS_FIRST, 10); return; }
    for (int i = 0; i < ROWS; i++) {
        Row *r = &rows[i];
        if (r->selected == r->opened || r->opened < 0) continue;
        r->selected = r->opened;
        commit(r, r->live || r->opened == r->running);
    }
    menu_page_cancel(p);
}

static void page_left(MenuPage *p, int index)
{
    (void)p;
    if (input_view == VIEW_PC) change(index, -1);
}
static void page_right(MenuPage *p, int index)
{
    (void)p;
    if (input_view == VIEW_PC) change(index, 1);
}
static void page_accept(MenuPage *p, int index)
{
    if (capture_index >= 0) return;
    if (input_view == VIEW_PC) {
        if (index == ROWS) view_set(p, VIEW_MENU_KEYS, 0);
        else menu_page_close(p);
    } else if (input_view == VIEW_MENU_KEYS) {
        if (index < keyboard_menu_count()) start_capture(p, index, 1);
        else if (index == 8) view_set(p, VIEW_GAME_KEYS_FIRST, 0);
        else if (index == 9) {
            keyboard_menu_defaults();
            save_bindings();
            menu_page_rebuild_at(p, 9);
        } else view_set(p, VIEW_PC, ROWS);
    } else if (input_view == VIEW_GAME_KEYS_FIRST) {
        if (index < 10) start_capture(p, index, 0);
        else if (index == 10) view_set(p, VIEW_GAME_KEYS_MORE, 0);
        else view_set(p, VIEW_MENU_KEYS, 8);
    } else if (input_view == VIEW_GAME_KEYS_MORE) {
        if (index < 9) start_capture(p, index + 10, 0);
        else if (index == 9) {
            keyboard_bind_defaults();
            save_bindings();
            menu_page_rebuild_at(p, 9);
        } else view_set(p, VIEW_GAME_KEYS_FIRST, 10);
    }
}

/* OPTIONS, item 9: the game's built-in option item still opens this page. */
void port_pc_settings(void)
{
    capture_stop();
    input_view = VIEW_PC;
    page.title = "PC SETTINGS";
    page.build = page_build;
    page.update = page_update;
    page.accept = page_accept;
    page.back = page_back;
    page.left = page_left;
    page.right = page_right;
    if (!loaded) {
        char value[32];
        for (int i = 0; i < ROWS; i++) {
            current(&rows[i], value, sizeof value);
            rows[i].selected = rows[i].running = find(&rows[i], value);
        }
        loaded = 1;
    }
    for (int i = 0; i < ROWS; i++) rows[i].opened = rows[i].selected;
    menu_page_run(&page);
}

/* OPTIONS' rows (patched func_001F0160): row 9 is this page; the game's table has an unused
   "UNLOCK RIDERS/BIKES(test)" there, which stays as it is. */
char *port_options_label(char *label, int index)
{
    return index == 9 ? (char *)"PC SETTINGS" : label;
}
