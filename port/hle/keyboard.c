/* Keyboard bindings are split by context: menus have independent arrow/accept/back/start
   keys and gameplay has its own steering/lean/pedal/brake/tricks/pause keys.
   The same key may be used in both contexts without swapping unrelated assignments.
   The old bind_00..bind_18 keys are retained for backward-compatible dhd.ini files.
   Esc is always an emergency BACK in menus and can be changed for gameplay pause.
   Holding a key while transitioning between race and menus does not activate the
   newly active context until that key is released. */
#include "hle.h"
#include "../include/fps.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

int gs_mouse_wheel_take(void);    /* port/gs/present.c */
int menu_page_mouse_active(void); /* port/hle/menu_page.c */

enum { VK_BACK_ = 0x08, VK_TAB_ = 0x09, VK_RETURN_ = 0x0D, VK_ESCAPE_ = 0x1B, VK_SPACE_ = 0x20,
       VK_LEFT_ = 0x25, VK_UP_ = 0x26, VK_RIGHT_ = 0x27, VK_DOWN_ = 0x28 };
enum { NO_AXIS, LEFT_X, LEFT_Y };
typedef struct {
    uint8_t key;
    uint16_t button;
    uint8_t axis; 
    int8_t dir;
} Key;

enum { GAME_KEYS = 19, MENU_KEYS = 8, MAX_TRACKED_KEYS = GAME_KEYS + MENU_KEYS + 1 };
static Key game_keys[GAME_KEYS] = {
    {VK_LEFT_, 0, LEFT_X, -1}, {VK_RIGHT_, 0, LEFT_X, 1},
    {VK_UP_, 0, LEFT_Y, -1}, {VK_DOWN_, 0, LEFT_Y, 1},
    {VK_SPACE_, PAD_CROSS}, {'X', PAD_CROSS},
    {'S', PAD_TRIANGLE}, {VK_BACK_, PAD_TRIANGLE},
    {VK_ESCAPE_, PAD_START}, {VK_RETURN_, PAD_START},
    {VK_TAB_, PAD_SELECT}, {'Z', PAD_SQUARE}, {'C', PAD_CIRCLE},
    {'A', PAD_L1}, {'D', PAD_R1}, {'Q', PAD_L2}, {'E', PAD_R2},
    {'W', PAD_R3}, {'F', PAD_L3}
};
static Key menu_keys[MENU_KEYS] = {
    {VK_UP_, PAD_UP}, {VK_DOWN_, PAD_DOWN},
    {VK_LEFT_, PAD_LEFT}, {VK_RIGHT_, PAD_RIGHT},
    {VK_SPACE_, PAD_CROSS}, {VK_BACK_, PAD_TRIANGLE},
    {VK_RETURN_, PAD_START}, {VK_TAB_, PAD_SELECT}
};
static const uint8_t game_defaults[GAME_KEYS] = {
    VK_LEFT_, VK_RIGHT_, VK_UP_, VK_DOWN_, VK_SPACE_, 'X', 'S', VK_BACK_, VK_ESCAPE_,
    VK_RETURN_, VK_TAB_, 'Z', 'C', 'A', 'D', 'Q', 'E', 'W', 'F'
};
static const uint8_t menu_defaults[MENU_KEYS] = {
    VK_UP_, VK_DOWN_, VK_LEFT_, VK_RIGHT_, VK_SPACE_, VK_BACK_, VK_RETURN_, VK_TAB_
};
static const char *const game_actions[GAME_KEYS] = {
    "Steer left", "Steer right", "Lean forward", "Lean back",
    "Pedal", "Pedal (alt)", "Brake", "Brake (alt)",
    "Pause", "Start / pause", "Camera", "Punch",
    "Kick", "Trick L1", "Trick R1", "Trick L2", "Trick R2",
    "Sprint", "Powerslide"
};
static const char *const menu_actions[MENU_KEYS] = {
    "Move up", "Move down", "Move left", "Move right",
    "Confirm", "Back", "Start", "Select"
};
static int suppress_keys;
void keyboard_reset(void);

int keyboard_bind_count(void) { return GAME_KEYS; }
int keyboard_menu_count(void) { return MENU_KEYS; }
int keyboard_bind_key(int index) { return index >= 0 && index < GAME_KEYS ? game_keys[index].key : 0; }
int keyboard_menu_key(int index) { return index >= 0 && index < MENU_KEYS ? menu_keys[index].key : 0; }
const char *keyboard_bind_action(int index)
{
    return index >= 0 && index < GAME_KEYS ? game_actions[index] : "";
}
const char *keyboard_menu_action(int index)
{
    return index >= 0 && index < MENU_KEYS ? menu_actions[index] : "";
}
int keyboard_bind_valid(int key)
{
    if (key < 8 || key >= 255) return 0;
    switch (key) {
    case VK_ESCAPE_: /* reserved for menu escape during remapping */
    case 0x10: case 0x11: case 0x12:  /* modifiers */
    case 0x5B: case 0x5C: case 0x14: case 0x90: /* Win / Caps / Num lock */
    case 0x01: case 0x02: case 0x04: /* mouse buttons */
        return 0;
    }
    return 1;
}
static int bind_load(Key *keys, int count, int index, int key)
{
    if (index < 0 || index >= count || !keyboard_bind_valid(key)) return 0;
    keys[index].key = (uint8_t)key;
    keyboard_reset();
    return 1;
}
/* Allow the legacy/default Esc on gameplay Pause, but do not capture Esc
   (it always cancels remapping). */
int keyboard_bind_load(int index, int key)
{
    if (index == 8 && key == VK_ESCAPE_) {
        game_keys[index].key = VK_ESCAPE_;
        keyboard_reset();
        return 1;
    }
    return bind_load(game_keys, GAME_KEYS, index, key);
}
int keyboard_menu_load(int index, int key) { return bind_load(menu_keys, MENU_KEYS, index, key); }
static int bind_set(Key *keys, int count, int index, int key)
{
    if (index < 0 || index >= count || !keyboard_bind_valid(key)) return 0;
    int old = keys[index].key;
    if (old == key) return 1;
    /* Conflicts swap only within the active context, not across menu and gameplay. */
    for (int i = 0; i < count; i++) {
        if (i != index && keys[i].key == key) {
            keys[i].key = (uint8_t)old;
            break;
        }
    }
    keys[index].key = (uint8_t)key;
    keyboard_reset();
    return 1;
}
int keyboard_bind_set(int index, int key) { return bind_set(game_keys, GAME_KEYS, index, key); }
int keyboard_menu_set(int index, int key) { return bind_set(menu_keys, MENU_KEYS, index, key); }
void keyboard_bind_defaults(void)
{
    for (int i = 0; i < GAME_KEYS; i++) game_keys[i].key = game_defaults[i];
    keyboard_reset();
}
void keyboard_menu_defaults(void)
{
    for (int i = 0; i < MENU_KEYS; i++) menu_keys[i].key = menu_defaults[i];
    keyboard_reset();
}
void keyboard_bind_capture(int active) { suppress_keys = !!active; keyboard_reset(); }
int keyboard_bind_suppressed(void) { return suppress_keys; }

const char *keyboard_bind_name(int key)
{
    static char names[8][24];
    static unsigned slot;
    char *out = names[(slot++) & 7];
    switch (key) {
    case VK_LEFT_: return "LEFT"; case VK_RIGHT_: return "RIGHT";
    case VK_UP_: return "UP"; case VK_DOWN_: return "DOWN";
    case VK_SPACE_: return "SPACE"; case VK_BACK_: return "BACKSPACE";
    case VK_ESCAPE_: return "ESC"; case VK_RETURN_: return "ENTER";
    case VK_TAB_: return "TAB";
    case 0x2E: return "DELETE"; case 0x2D: return "INSERT";
    case 0x24: return "HOME"; case 0x23: return "END";
    case 0x21: return "PAGE UP"; case 0x22: return "PAGE DOWN";
    }
    if (key >= 'A' && key <= 'Z') { snprintf(out, 24, "%c", key); return out; }
    if (key >= '0' && key <= '9') { snprintf(out, 24, "%c", key); return out; }
    if (key >= 0x70 && key <= 0x87) { snprintf(out, 24, "F%d", key - 0x6F); return out; }
    snprintf(out, 24, "VK %d", key);
    return out;
}

#define RAMP_SECONDS 0.1
#define FIELD_SECONDS (port_fps_high() ? port_frame_period() : 1.0 / 60)

static struct {
    int mode;                 /* -1 before first poll, 0 menu, 1 riding */
    uint8_t ignore[256];      /* held keys from the previous context */
    int8_t dir[3];
    double since[3];
} state = {.mode = -1};

const uint8_t *keyboard_keys(int *count)
{
    static uint8_t list[MAX_TRACKED_KEYS];
    for (int i = 0; i < GAME_KEYS; i++) list[i] = game_keys[i].key;
    for (int i = 0; i < MENU_KEYS; i++) list[GAME_KEYS + i] = menu_keys[i].key;
    list[GAME_KEYS + MENU_KEYS] = VK_ESCAPE_;
    *count = MAX_TRACKED_KEYS;
    return list;
}

void keyboard_reset(void)
{
    memset(&state, 0, sizeof state);
    state.mode = -1;
}

/* What the game makes of a raw stick byte: func_00220AE0 (riding, D_0029E290 = 1). */
int keyboard_game_axis(int raw)
{
    int value;
    if (raw >= 226) {
        value = (int)(((float)raw - 225.0f) * 2.133333444595337f + 64.0f);
        if (value >= 128) value = 127;
    } else if (raw >= 163) {
        value = raw - 162;
        if (value >= 65) value = 64;
    } else if (raw < 30) {
        value = (int)((float)raw * 2.133333444595337f + -127.0f);
        if (value <= -128) value = -127;
    } else if (raw < 92) {
        value = (int)((92.0f - (float)raw) * -1.0322580337524414f);
        if (value <= -65) value = -64;
    } else
        value = 0;
    if (value >= 128) value = 127;
    else if (value <= -128) value = -127;
    return abs(value) <= 1 ? 0 : value;
}

/* The raw byte for a fraction (0..1] of full steering in direction dir: the first one out from
   the centre whose steering (value squared / 127, func_0017E1C0) reaches it. */
static uint8_t ramp_raw(double fraction, int dir)
{
    int want = (int)ceil(fraction * 127.0 - 1e-9), raw = 128;
    if (want < 1) want = 1;
    for (int step = 1; step <= 128; step++) {
        raw = dir > 0 ? 127 + step : 128 - step;
        if (raw > 255) raw = 255;
        if (raw < 0) raw = 0;
        int v = abs(keyboard_game_axis(raw));
        if (v * v / 127 >= want || (v >= 127)) break;
    }
    return (uint8_t)raw;
}

/* The original game's title/options menus still use their own selection system.
   Mouse wheel navigates its currently selected row, left click confirms, right
   click goes back. Host popup pages do their own cursor hit testing and clicks.
   No mouse buttons or wheel are sent to gameplay. */
static int mouse_left_prev, mouse_right_prev;
static uint16_t keyboard_menu_mouse(int riding)
{
    int wheel = gs_mouse_wheel_take();
    HWND hwnd = GetForegroundWindow();
    DWORD pid = 0;
    char cls[64] = "";
    if (hwnd) GetWindowThreadProcessId(hwnd, &pid);
    int active = hwnd && pid == GetCurrentProcessId() &&
                 GetClassNameA(hwnd, cls, sizeof cls) && !strcmp(cls, "DHDNativeGS");
    int left = active && !!(GetAsyncKeyState(VK_LBUTTON) & 0x8000);
    int right = active && !!(GetAsyncKeyState(VK_RBUTTON) & 0x8000);
    uint16_t buttons = 0;
    if (!riding && active && !suppress_keys) {
        if (wheel > 0) buttons |= PAD_UP;
        else if (wheel < 0) buttons |= PAD_DOWN;
        if (!menu_page_mouse_active()) {
            if (left && !mouse_left_prev) buttons |= PAD_CROSS;
            if (right && !mouse_right_prev) buttons |= PAD_TRIANGLE;
        }
    }
    mouse_left_prev = left;
    mouse_right_prev = right;
    return buttons;
}

/* Converts keyboard input into pad input for the active context only.
   Menu UP/DOWN never depends on the gameplay steering bindings. */
void keyboard_pad(const uint8_t down[256], int riding, double now, HostPad *out)
{
    static const uint16_t pressure_order[12] = {
        PAD_RIGHT, PAD_LEFT, PAD_UP, PAD_DOWN, PAD_TRIANGLE, PAD_CIRCLE,
        PAD_CROSS, PAD_SQUARE, PAD_L1, PAD_R1, PAD_L2, PAD_R2
    };
    uint16_t buttons = 0;
    int dir[3] = {0};
    riding = !!riding;
    uint16_t mouse_buttons = keyboard_menu_mouse(riding);
    if (suppress_keys) { keyboard_reset(); return; }
    if (state.mode >= 0 && state.mode != riding)
        for (int vk = 0; vk < 256; vk++)
            if (down[vk]) state.ignore[vk] = 1;
    state.mode = riding;
    for (int vk = 0; vk < 256; vk++)
        if (!down[vk]) state.ignore[vk] = 0;

    if (riding) {
        for (int i = 0; i < GAME_KEYS; i++) {
            const Key *k = &game_keys[i];
            if (!down[k->key] || state.ignore[k->key]) continue;
            if (k->axis) dir[k->axis] += k->dir;
            else buttons |= k->button;
        }
    } else {
        for (int i = 0; i < MENU_KEYS; i++) {
            const Key *k = &menu_keys[i];
            if (down[k->key] && !state.ignore[k->key]) buttons |= k->button;
        }
        /* Esc always offers a way out, even after a badly chosen menu binding. */
        if (down[VK_ESCAPE_] && !state.ignore[VK_ESCAPE_]) buttons |= PAD_TRIANGLE;
    }

    for (int axis = LEFT_X; axis <= LEFT_Y; axis++) {
        if (dir[axis] != state.dir[axis]) {
            state.dir[axis] = (int8_t)dir[axis];
            state.since[axis] = now;
        }
        if (!dir[axis]) continue;
        double fraction = (now - state.since[axis] + FIELD_SECONDS) / RAMP_SECONDS;
        if (fraction > 1) fraction = 1;
        uint8_t raw = ramp_raw(fraction, dir[axis]);
        if (axis == LEFT_X) out->left_x = raw;
        else out->left_y = raw;
    }
    buttons |= mouse_buttons;
    out->buttons |= buttons;
    for (int i = 0; i < 12; i++)
        if (buttons & pressure_order[i]) out->pressure[i] = 255;
}

/* DHD_KEYS (tests): "start:duration:key[+key],..." as DHD_RACE_PAD, seconds on the race clock
   after GO; keys: up down left right space enter esc tab backspace, or a letter. Sets down[]
   for the given time; 0 if the script is invalid. */
int keyboard_script(const char *script, double seconds, uint8_t down[256])
{
    static const struct { const char *name; uint8_t key; } names[] = {
        {"up", VK_UP_}, {"down", VK_DOWN_}, {"left", VK_LEFT_}, {"right", VK_RIGHT_}, {"space", VK_SPACE_},
        {"enter", VK_RETURN_}, {"esc", VK_ESCAPE_}, {"tab", VK_TAB_}, {"backspace", VK_BACK_},
    };
    memset(down, 0, 256);
    while (script && *script) {
        double start, duration;
        char chord[128];
        int consumed = 0;
        if (sscanf(script, "%lf:%lf:%127[a-z+]%n", &start, &duration, chord, &consumed) != 3 || start < 0 ||
            duration <= 0)
            return 0;
        int active = seconds >= start && seconds < start + duration;
        for (char *name = chord; *name;) {
            char *end = strchr(name, '+');
            size_t n = end ? (size_t)(end - name) : strlen(name);
            int key = -1;
            if (n == 1) key = name[0] - 'a' + 'A';
            for (size_t i = 0; i < sizeof names / sizeof *names; i++)
                if (strlen(names[i].name) == n && !strncmp(names[i].name, name, n)) key = names[i].key;
            if (key < 0) return 0;
            if (active) down[key] = 1;
            name += n + (end != NULL);
        }
        script += consumed;
        if (*script == ',') script++;
        else if (*script) return 0;
    }
    return 1;
}
