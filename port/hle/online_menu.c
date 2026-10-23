/* Online rooms in the game's SharedMenu style, with physical keyboard text entry.
   A connected member chooses in the retail rider/bike/outfit screens, then returns
   here to ready up. The host chooses the level and starts when everyone is ready.
   The existing online.c setup event carries each complete preview choice. */
#include "hle.h"
#include "menu_page.h"
#include "text_input.h"
#include "../net/net.h"
#include "../net/player_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* Deterministic online compatibility ID, NOT the hash of the entire ISO.
   SHA-256 of the ASCII string:
   DHD-SCUS-97177-compatible-program-sha1:4e25a222ad8a475b5d26c2e928e90b5f2b7075eb

   The retail game's SCUS_971.77 executable is checked by game_data_read()
   before this ID is sent. This allows ISO dumps with different padding or
   other disc-level differences to join, without making arbitrary executable
   variants appear compatible. The wire field remains "isoSha256" until a
   coordinated server/client protocol upgrade; both clients compare this ID. */
static const char compatible_program_id[] =
    "ad32880e6bb3571bddd78cfc134f41d702345dc6d5e7689cec346dda554201ec";

/* The tracks with a 2-4 player level (online.c mp_level) and the riders every save has
   (online.c regular): the game's indexes and names, as the online launcher lists them. */
static const struct { int index; const char *name; } tracks[] = {
    {0, "ALPINE"}, {1, "MOAB"}, {2, "JUNGLE"}, {3, "PERU"}, {4, "PERU TDH"}, {5, "GLACIER"},
    {6, "ALPINE MX"}, {7, "MOAB MX"}, {8, "JUNGLE MX"}, {9, "PERU MX"}, {10, "JUNGLE TD"},
    {11, "AUBER ST"}, {12, "FUJI"}, {13, "MOAB TDH"}, {14, "GLACIER X"}, {15, "AUBER MX"},
    {16, "AUBER STD"}, {19, "BC"}, {20, "CITY"}, {22, "GLACIER T"}, {23, "MOSH"}, {24, "ALPINE T"},
    {25, "BC TDX"}};
static const struct { int index; const char *name; } riders[] = {
    {0, "COSMO"}, {1, "MAI"}, {2, "AJAX"}, {3, "JONNIE"}, {4, "KALOLO"}, {5, "T-BAG"}, {8, "DRUD"},
    {10, "RAKEL"}, {13, "JEN"}, {14, "WOODY"}};
#define COUNT(a) ((int)(sizeof a / sizeof *a))

static const char code_letters[] = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";  /* the server's join codes */
static const char name_letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -";
enum { NAME_MAX = 12, CODE_LENGTH = 6, ROW_TEXT = 96, MAX_ROWS = 16, LIST_MAX = 8, MEMBER_ROWS = NET_MAX_SLOTS };

enum { HOME, LIST, NAME, CODE, ROOM, HOST_BOOT };
/* The room's rows */
enum { R_CODE, R_TRACK, R_BOTS, R_SELECT, R_MEMBER };
/* Keep player entries consecutive; action rows follow the last joined
   player instead of eight empty slots. 8 + 4 + 3 = 15 rows maximum. */
static int room_members;
static int room_ready_row(void) { return R_MEMBER + room_members; }
static int room_start_row(void) { return room_ready_row() + 1; }
static int room_leave_row(void) { return room_ready_row() + 2; }
static int room_total_rows(void) { return room_ready_row() + 3; }
/* The room's options, carried in the server's "laps" setting (Downhill Domination has no laps):
   every PC builds the same race from them (online.c hle_online_ai_count). */
enum { ROOM_NO_BOTS = 1 };
static const char *const rider_names[16] = {
    "COSMO", "MAI", "AJAX", "JONNIE", "KALOLO", "T-BAG", "ERIC CARTER", "TARA LLANES", "DRUD",
    "BRIAN LOPES", "RAKEL", "MISSY GIOVE", "RICHIE SCHLEY", "JEN", "WOODY", "KINETICLOPS"};
static MenuPage page;
static int screen, race;
static int choosing, selected, selection_requested, room_focus;
/* The room over the rider screen (port_online_shell): in_shell while it runs; room_pending
   while the camera flies to the rider screen it opens over; hold_rider until the press that
   closed it is released; resume_home: back on the title, ONLINE opens again. */
static int in_shell, room_pending, hold_rider, resume_home;
/* 0 means the existing public server. Hosted mode is used only while
   creating a room on the local relay; code-based joining resolves both. */
static int hosted_mode;
/* Track whether the rider screen was really reached; intermediate menu 15
   is also used while navigating to it and must not close customization. */
static int visited_rider_screen, host_code_unresolved, host_code_direct;
enum { HOME_HOST = 5, HOME_UPDATE = 6 };
static int chosen_rider, chosen_bike, ready_track, ready_options;
static int public_room, room_settings_sent, room_ready_sent, room_track = 0, room_options;
static int32_t chosen_outfit[3], chosen_cycle[3];
#define SHELL_WORD(a) (*(int32_t *)(uintptr_t)(0x08000000u + (a)))
#define SETUP_WORD(offset) SHELL_WORD(0x4B25A0 + (offset))
void port_online_choose_begin(void);
void port_online_choose_end(void);
void func_001F67F0(int enabled);
void func_001F6798(int enabled);
void func_001F5B08(int enabled);
void func_001F6940(void);
void func_0021E428(int enabled);
void func_001F65E0(int enabled);
void func_001F5578(void);
void func_001F60C8(int rider);
void func_00203590(void *animation, int resource, int bank, int index);
void func_00203FE0(void *animation, int enabled);
void func_002038E0(void *animation, int enabled);
void func_001F50F0(int resource, int index);
void func_001F57D0(int resource, int enabled);
void func_001C59D8(void *sound);
void *func_0023D040(int view);
void func_0023D020(int view, void *camera);
void func_0023C8D8(int mode, int view);
void func_0023D898(int view, float *distance, float *aspect);
void func_0023D8D8(int view, float distance, float aspect);
float func_00203C28(void *animation);
void func_001F79D8(int level, int frames);  /* the shell's screen fade */
extern int D_00275CB0[];  /* each rider's camera take out of the shop (func_001F1500) */
int func_00220230(int port);  /* the buttons held on a pad */
static float shell_distance, shell_aspect;
static int offline_mode, offline_submode, offline_players;
void hle_online_selected_ride(int rider, const int32_t *outfit, const int32_t *bike);

/* SharedMenu can change a one-view shell's render mode in place; the stock popup
   manager only restores its view descriptor when it began with multiple views. */
static void run_page(void)
{
    void *camera = func_0023D040(0);
    int mode = SHELL_WORD(0x75A9D8);
    float distance, aspect;
    func_0023D898(0, &distance, &aspect);
    menu_page_run(&page);
    func_0023D020(0, camera);
    func_0023C8D8(mode, 0);
    func_0023D8D8(0, distance, aspect);
    text_input_end();
}

/* The retail rider screen's Back cleanup (func_001F24B8), also needed when
   intercepting GET READY: changing only MENU bypasses all of these side effects. */
static void selection_end(void)
{
    void *animation = (void *)(uintptr_t)(0x08000000u + 0x4C6370);
    void *sound = (void *)(uintptr_t)(0x08000000u + 0x4B2A90);
    func_001F67F0(0);
    func_001F6798(0);
    func_001F5B08(0);
    func_001F6940();
    SHELL_WORD(0x29E024) = 0;
    func_0021E428(0);
    func_001F65E0(0);
    func_001F5578();
    func_001F60C8(0);
    for (int i = 0; i < 7; i++) SETUP_WORD(0x2A8 + 4 * i) = -1;
    SHELL_WORD(0x29DF68) = 0;
    SHELL_WORD(0x4C6910) = 1;
    func_00203590(animation, 0x2330, 0, 1);
    func_00203FE0(animation, 1);
    func_002038E0(animation, 0);
    func_001F50F0(0x2330, 1);
    func_001F57D0(0x232B, 0);
    func_001F57D0(0x233D, 0);
    func_001F57D0(0x235F, 0);
    func_001F57D0(0x2329, 1);
    func_001C59D8(sound);
    uintptr_t music = (uint32_t)SHELL_WORD(0x4B2A90 + 0x1310);
    if (music) *(float *)(music + 0xB8) = 1.0f;
    func_0023D8D8(0, shell_distance, shell_aspect);
    SHELL_WORD(0x29DED8) = 15;
    SHELL_WORD(0x29DEEC) = 3;
    port_online_choose_end();
}

/* Online everyone has every rider, bike and track, as retail's unlock-all code gives them
   (func_001F0780: D_004325A0.x2180 the 16 riders, x2188 the 18 bikes, x2114 the tracks but 21
   and 23, D_004225A0.x12190 23 more flags), and the rider screen's pros too: their statues come
   alive from the save's rider bits D_004225A0.x12110 (0-13), which func_001BB670 copies into the
   screen's table (D_004B25A0.x3C). The player's own progress comes back when they leave online
   play, and the memory card keeps it meanwhile: its writes are held (mc.c). */
enum { UNLOCK_PROS = 0x4346B0, UNLOCK_TRACKS = 0x4346B4, UNLOCK_RIDERS = 0x434720, UNLOCK_BIKES = 0x434728,
       UNLOCK_FLAGS = 0x434730 };
void func_001BB670(void *game);
static int unlocked;
static int32_t kept_pros, kept_tracks, kept_riders, kept_bikes;
static uint8_t kept_flags[23];

static void unlock_all(void)
{
    if (unlocked) return;
    uint8_t *flags = (uint8_t *)(uintptr_t)(0x08000000u + UNLOCK_FLAGS);
    kept_pros = SHELL_WORD(UNLOCK_PROS), kept_tracks = SHELL_WORD(UNLOCK_TRACKS);
    kept_riders = SHELL_WORD(UNLOCK_RIDERS), kept_bikes = SHELL_WORD(UNLOCK_BIKES);
    memcpy(kept_flags, flags, sizeof kept_flags);
    SHELL_WORD(UNLOCK_PROS) |= 0x3FFF;
    func_001BB670((void *)(uintptr_t)(0x08000000u + 0x4B25A0));
    SHELL_WORD(UNLOCK_TRACKS) |= 0x035FFFFF;
    SHELL_WORD(UNLOCK_RIDERS) |= 0xFFFF;
    SHELL_WORD(UNLOCK_BIKES) |= 0x3FFFF;
    memset(flags, 1, sizeof kept_flags);
    hle_mc_hold = unlocked = 1;
    hle_log("online menu: everything unlocked while online\n");
}

/* Back to offline play: the player's own unlocks again. */
void port_online_relock(void)
{
    if (!unlocked) return;
    SHELL_WORD(UNLOCK_PROS) = kept_pros, SHELL_WORD(UNLOCK_TRACKS) = kept_tracks;
    SHELL_WORD(UNLOCK_RIDERS) = kept_riders, SHELL_WORD(UNLOCK_BIKES) = kept_bikes;
    func_001BB670((void *)(uintptr_t)(0x08000000u + 0x4B25A0));
    memcpy((uint8_t *)(uintptr_t)(0x08000000u + UNLOCK_FLAGS), kept_flags, sizeof kept_flags);
    hle_mc_hold = unlocked = 0;
    hle_log("online menu: the player's own unlocks again\n");
}

static void reset_selection(void)
{
    selected = selection_requested = room_focus = 0;
    room_settings_sent = room_ready_sent = 0;
    room_members = 0;
}

static char text[MAX_ROWS][ROW_TEXT], status[128], footer[128];
static int outdated;  /* the server refused this release: HOME offers Update Now (row 5) */
/* 44 characters: the footer fits about 45 at 640 wide (0.1.0 shows the server's copy there too) */
static const char outdated_text[] = "A new version is out. Update to play online.";

static const char *server(void)
{
    if (hosted_mode) return "ws://127.0.0.1:8787";
    const char *s = getenv("DHD_NET_SERVER");
    return s && *s ? s : NULL;
}

/* ---- Check the disc's executable, not the entire ISO, on a worker thread. ---- */

static volatile LONG iso_state; /* 0 not started, 1 checking, 2 compatible, 3 rejected */
static char iso_hash[65];

static DWORD WINAPI check_iso_program(void *arg)
{
    (void)arg;
    const char *path = getenv("DHD_ISO");
    /* game_data_read verifies SCUS_971.77's size, SHA-1 and exact original
       program bytes. Other game regions or altered executables cannot use this
       port safely, even when the file still happens to be named SCUS_971.77.
       No cache is used: stale iso-check.txt hashes must never bypass checking. */
    const char *problem = path ? game_data_read(path, NULL) : "No ISO selected.";
    if (problem) {
        hle_log("online menu: incompatible ISO: %s\n", problem);
        InterlockedExchange(&iso_state, 3);
        return 0;
    }
    snprintf(iso_hash, sizeof iso_hash, "%s", compatible_program_id);
    hle_log("online menu: compatible SCUS_971.77 executable verified\n");
    InterlockedExchange(&iso_state, 2);
    return 0;
}

static void start_iso_check(void)
{
    if (iso_state) return;
    /* Never trust a cached or launcher-provided raw ISO SHA-256: the network
       comparison now uses the verified executable's compatibility identity. */
    iso_state = 1;
    HANDLE t = CreateThread(NULL, 0, check_iso_program, NULL, 0, NULL);
    if (t) CloseHandle(t);
    else iso_state = 3;
}

/* ---- the public list, in a thread ---- */

static volatile LONG list_state;  /* 0 none, 1 asking, 2 done */
static NetLobbyInfo list[LIST_MAX];
static int list_count, list_shown;  /* list_shown: the rows have the answer */

static DWORD WINAPI fetch_list(void *arg)
{
    (void)arg;
    list_count = net_list_lobbies(list, LIST_MAX);
    InterlockedExchange(&list_state, 2);
    return 0;
}

/* ---- the player ---- */

static char name[NAME_MAX + 1], version[64];
static int rider = 0;  /* index into riders[] */

static void load_player(void)
{
    const char *n = getenv("DHD_NET_NAME"), *r = getenv("DHD_NET_RIDER");
    if (!name[0]) {
        int k = 0;
        for (const char *c = n ? n : ""; *c && k < NAME_MAX; c++) {
            char up = (char)(*c >= 'a' && *c <= 'z' ? *c - 32 : *c);
            if (strchr(name_letters, up)) name[k++] = up;
        }
        name[k] = 0;
        if (!k) snprintf(name, sizeof name, "RIDER");
        for (int i = 0; r && *r && i < COUNT(riders); i++)
            if (riders[i].index == atoi(r)) rider = i;
    }
    if (!version[0]) hle_online_version(version, sizeof version);
}

/* Saved to dhd.ini and set for this run, as the online launcher does. */
static void save(const char *key, const char *env, const char *value)
{
    host_setting_save(key, value);
    _putenv_s(env, value);
}

/* What stops this PC from joining a race, or NULL. */
static const char *not_ready(void)
{
    const char *render = getenv("DHD_RENDER");
    if (!server()) return "No server: set server = ... in dhd.ini";
    if (!render || strcmp(render, "native")) return "Online races need the native renderer";
    if (iso_state == 1) return "Checking game compatibility...";
    if (iso_state != 2) return "Requires compatible USA SCUS-97177 ISO";
    return NULL;
}

/* ---- the screens ---- */

static int entry_pick;           /* the letter on offer */
static char entry[16];
static int home_row;  /* HOME's row last chosen: the cursor goes back to it */

static void go(int to)
{
    text_input_end();
    screen = to;
    if (to == NAME || to == CODE) text_input_begin();
    if (to == ROOM) room_focus = selected;
    if (to == HOME) {
        player_host_stop();
        hosted_mode = 0;
        visited_rider_screen = 0;
        host_code_unresolved = host_code_direct = 0;
    }
    status[0] = 0;
    menu_page_rebuild_at(&page, to == HOME ? home_row : 0);
}

static void start_list(void)
{
    if (list_state == 1) return;
    list_state = 1;
    net_init(server(), name, version, iso_hash);
    HANDLE t = CreateThread(NULL, 0, fetch_list, NULL, 0, NULL);
    if (t) CloseHandle(t);
    else list_state = 2, list_count = -1;
}

static void enter_room(int create, const char *code)
{
    const char *why = not_ready();
    if (why) {
        snprintf(status, sizeof status, "%s", why);
        return;
    }
    /* H-prefixed codes ALWAYS target Player Hosted and are NEVER routed
       to the Cloudflare race relay. The public server remains available
       through its own Create/Join options and regular room codes. */
    if (!create && !hosted_mode) {
        char direct[192];
        int is_host_code = code && strlen(code) == CODE_LENGTH &&
                           (code[0] == 'H' || code[0] == 'h');
        host_code_direct = is_host_code &&
                           net_find_player_host(code, direct, sizeof direct);
        host_code_unresolved = is_host_code && !host_code_direct;
        if (host_code_direct) {
            net_init(direct, name, version, iso_hash);
            hle_log("online menu: joining native Player Hosted room %s via %s\n", code, direct);
        } else if (host_code_unresolved) {
            snprintf(status, sizeof status, "Host code not online. Check host / directory");
            hle_log("online menu: native H-code %s is not registered; no cloud fallback\n", code);
            return;
        } else {
            net_init(server(), name, version, iso_hash);
        }
    } else {
        host_code_unresolved = host_code_direct = 0;
        /* The native host runs INSIDE dhd.exe. Creating the room against
           Cloudflare instead would give us a cloud code and leave the
           native host with zero members, never published in the directory. */
        net_init(hosted_mode && create ? "ws://127.0.0.1:8787" : server(),
                 name, version, iso_hash);
    }
    if (create) net_create_room();
    else if (net_join_room(code) < 0) {
        snprintf(status, sizeof status, "That code can't be one");
        return;
    }
    reset_selection();
    choosing = 0;
    visited_rider_screen = 0;
    selection_requested = 1;
    public_room = !hosted_mode && create == 2;
    room_settings_sent = !create;  /* a guest who becomes host keeps the room as it is */
    room_ready_sent = 0;
    hle_log("online menu: %s %s\n", create ? "creating a room" : "joining", create ? (public_room ? "(public)" : "(private)") : code);
    go(ROOM);
}

static void page_build(MenuPage *p)
{
    switch (screen) {
    case HOME:
        p->title = "ONLINE";
        snprintf(text[0], ROW_TEXT, "Name:\t\t%s", name);
        menu_page_row(p, text[0], 1);
        menu_page_row(p, "Public Races", 1);
        menu_page_row(p, "Create A Public Race", 1);
        menu_page_row(p, "Create A Private Race", 1);
        menu_page_row(p, "Join With A Code", 1);
        menu_page_row(p, "Host A Private Game (Your PC)", 1);
        if (outdated) menu_page_row(p, "Update Now", 1);
        break;
    case LIST:
        p->title = "PUBLIC RACES";
        list_shown = list_state == 2;
        if (list_state != 2) menu_page_row(p, "Looking for races...", 0);
        else if (list_count < 0) menu_page_row(p, "The server didn't answer", 0);
        else if (list_count == 0) menu_page_row(p, "No public races right now", 0);
        for (int i = 0; list_state == 2 && i < list_count; i++) {
            const NetLobbyInfo *l = &list[i];
            const char *track = "?";
            for (int k = 0; k < COUNT(tracks); k++)
                if (tracks[k].index == l->track) track = tracks[k].name;
            int same = !strcmp(l->version, version);
            snprintf(text[i], ROW_TEXT, "%.14s\t\t%d/%d %s%s", l->room_name[0] ? l->room_name : l->host_name,
                     l->players, l->max_players, track, same ? "" : " (other version)");
            menu_page_row(p, text[i], same && l->players < l->max_players);
        }
        menu_page_row(p, "Look Again", list_state == 2);
        break;
    case NAME:
    case CODE:
        p->title = screen == NAME ? "YOUR NAME" : "JOIN WITH A CODE";
        menu_page_row(p, text[0], 1);  /* written every frame */
        menu_page_row(p, screen == NAME ? "Done" : "Join", 1);
        break;
    case HOST_BOOT:
        p->title = "PLAYER HOSTED";
        menu_page_row(p, "Cancel", 1);
        break;
    case ROOM:
        p->title = "RACE ROOM";
        for (int i = 0; i < room_total_rows(); i++) {
            text[i][0] = 0;
            menu_page_row(p, text[i], i == R_SELECT || i == room_ready_row() ||
                                       i == room_start_row() || i == room_leave_row());
        }
        break;
    }
}

static void entry_text(void)
{
    const char *letters = screen == NAME ? name_letters : code_letters;
    int max = screen == NAME ? NAME_MAX : CODE_LENGTH;
    char shown = letters[entry_pick] == ' ' ? '-' : letters[entry_pick];
    if ((int)strlen(entry) < max)
        snprintf(text[0], ROW_TEXT, "%s\t\t%s[%c]", screen == NAME ? "Name:" : "Code:", entry, shown);
    else
        snprintf(text[0], ROW_TEXT, "%s\t\t%s", screen == NAME ? "Name:" : "Code:", entry);
}

static void room_update(MenuPage *p, NetLobby *l)
{
    /* Reflow as players join or leave. Ready/Start/Leave always appear
       immediately after the final occupied member slot. */
    int count = l->nmembers < 0 ? 0 : l->nmembers > MEMBER_ROWS ? MEMBER_ROWS : l->nmembers;
    if (count != room_members) {
        room_members = count;
        menu_page_rebuild_at(p, R_SELECT);
    }
    if (l->state == NET_FAILED || l->state == NET_IDLE) {
        int too_old = !strcmp(l->error_code, "client_outdated");
        if (too_old) outdated = 1;
        snprintf(status, sizeof status, "%s",
                 too_old ? outdated_text : l->error[0] ? l->error : "The connection to the server failed");
        if (host_code_direct && !too_old && !l->error_code[0])
            snprintf(status, sizeof status, "Cannot reach host: check TCP 8787 / firewall");
        hle_log("online menu: refused: %s (%s)\n", l->error, l->error_code);
        net_leave();
        player_host_stop(); /* only our local server; public games unaffected */
        reset_selection();
        if (too_old) home_row = HOME_UPDATE;
        if (in_shell) {  /* the rider screen's Back, then ONLINE again with the reason */
            race = 0;
            menu_page_close(p);
            return;
        }
        screen = HOME;
        hosted_mode = 0;
        menu_page_rebuild_at(p, home_row);
        return;
    }
    int in = l->state == NET_LOBBY || l->state == NET_RACING;
    if (in && l->is_host && !room_settings_sent) {
        char room_name[40];
        snprintf(room_name, sizeof room_name, "%s", name);
        NetRoomSettings s = {tracks[room_track].index, room_options, public_room, NET_MAX_SLOTS, -1, room_name};
        net_set_room(&s);
        room_settings_sent = 1;
    }
    if (in && !room_ready_sent) {
        hle_log("online menu: room %s%s, version %s\n", l->code, l->is_host ? " (host)" : "", version);
        net_set_ready(0);
        room_ready_sent = 1;
    }
    if (in)
        for (int k = 0; k < COUNT(tracks); k++)
            if (tracks[k].index == l->track) room_track = k;
    if (l->started || l->state == NET_RACING) {
        hle_log("online menu: room %s: the race starts\n", l->code);
        race = 1;
        hle_online_selected_ride(chosen_rider, chosen_outfit, chosen_cycle);
        hle_online_from_menu();
        menu_page_close(p);
        return;
    }
    int self_ready = 0;
    for (int i = 0; i < l->nmembers; i++)
        if (l->member[i].id == l->member_id) self_ready = l->member[i].ready;
    /* Wait for the server to acknowledge unready before handing input to the picker.
       If a start won the race, the earlier started branch loads the committed selection. */
    if (in && selection_requested && !self_ready) {
        selection_requested = 0;
        visited_rider_screen = 0; /* each new visit must reach retail picker */
        choosing = 1;
        unlock_all();
        race = 2;
        menu_page_close(p);
        return;
    }
    int own_ready = 0, conflict = 0;
    for (int i = 0; i < l->nmembers; i++) {
        if (l->member[i].id == l->member_id) own_ready = l->member[i].ready;
        else if (selected && l->member[i].rider == chosen_rider / 32) conflict = 1;
    }
    if (in) room_options = l->laps;
    if (own_ready && (conflict || ready_track != l->track || ready_options != l->laps)) net_set_ready(0);
    int ready = 0;
    for (int i = 0; i < l->nmembers; i++) ready += l->member[i].ready;
    int can_start = in && l->is_host && l->nmembers >= 2 && ready == l->nmembers;
    if (in) snprintf(text[R_CODE], ROW_TEXT, "Code: %s\t\t%d/%d PLAYERS", l->code, l->nmembers, l->max_players);
    else snprintf(text[R_CODE], ROW_TEXT, "Connecting...");
    const char *track_name = NULL;
    for (int k = 0; k < COUNT(tracks); k++) if (tracks[k].index == l->track) track_name = tracks[k].name;
    char track_number[8];
    snprintf(track_number, sizeof track_number, "%d", l->track);
    if (!track_name) track_name = track_number;
    const char *bots = l->laps & ROOM_NO_BOTS ? "OFF" : "ON";
    if (in && l->is_host) {
        snprintf(text[R_TRACK], ROW_TEXT, "Track:\t\t< %s >", track_name);
        snprintf(text[R_BOTS], ROW_TEXT, "Computer Riders:\t\t< %s >", bots);
    } else {
        snprintf(text[R_TRACK], ROW_TEXT, "Track:\t\t%s", track_name);
        snprintf(text[R_BOTS], ROW_TEXT, "Computer Riders:\t\t%s", bots);
    }
    snprintf(text[R_SELECT], ROW_TEXT, "%s", selected ? "Change Rider / Bike / Outfit" : "Choose Rider / Bike / Outfit");
    for (int i = 0; i < MEMBER_ROWS; i++) {
        const NetMember *m = i < l->nmembers ? &l->member[i] : NULL;
        char who[24], ping[16] = "";
        if (!in || i >= l->nmembers) {
            text[R_MEMBER + i][0] = 0;
            continue;
        }
        snprintf(who, sizeof who, "%.12s%s", m->name, m->id == l->host_id ? " (host)" : "");
        for (char *c = who; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
        if (m->rtt_ms >= 0) snprintf(ping, sizeof ping, " %d ms", m->rtt_ms);
        snprintf(text[R_MEMBER + i], ROW_TEXT, "%s\t\t%s%s%s", who,
                 m->rider >= 1 && m->rider <= 16 ? rider_names[m->rider - 1] : "", m->ready ? "  READY" : "", ping);
    }
    int missing = (l->nmembers < 2 ? 2 : l->nmembers) - ready;
    snprintf(text[room_ready_row()], ROW_TEXT, "%s", own_ready ? "Not Ready" : "Ready");
    snprintf(text[room_start_row()], ROW_TEXT, "%s", l->is_host ? "Start The Race" : "The host starts the race");
    snprintf(text[room_leave_row()], ROW_TEXT, "Leave Room");
    menu_page_enable(p, R_TRACK, in && l->is_host);
    menu_page_enable(p, R_BOTS, in && l->is_host);
    menu_page_enable(p, R_SELECT, in);
    menu_page_enable(p, room_ready_row(), in && selected && !conflict);
    menu_page_enable(p, room_start_row(), can_start);
    menu_page_enable(p, room_leave_row(), 1);
    if (room_focus) {
        menu_page_set_cursor(p, conflict ? R_SELECT : room_ready_row());
        room_focus = 0;
    }
    /* what to do next, else what everyone waits for */
    if (status[0]) p->footer = status;
    else if (hosted_mode && in && player_host_directory_state() == -2)
        p->footer = "H-code rejected: update online directory";
    else if (hosted_mode && in && player_host_directory_state() < 0)
        p->footer = "H-code offline: check directory/internet";
    else if (hosted_mode && in && !player_host_directory_state())
        p->footer = "H-code registering; local join still works";
    else if (!in) p->footer = "No connection: Leave Room to give up";
    else if (conflict) p->footer = "Rider taken: change your selection";
    else if (!selected) p->footer = "Choose your rider, bike and outfit first";
    else if (l->nmembers < 2) {
        if (hosted_mode)
            snprintf(footer, sizeof footer, "Host: TCP 8787 must be reachable remotely");
        else
            snprintf(footer, sizeof footer, "Waiting for players: friends join with code %s", l->code);
        p->footer = footer;
    } else if (!own_ready) p->footer = "Ready when you like your selection";
    else if (missing > 0) {
        snprintf(footer, sizeof footer, "Waiting for %d player%s to get ready", missing, missing == 1 ? "" : "s");
        p->footer = footer;
    } else p->footer = l->is_host ? "Everyone is ready: start when you like" : "Everyone is ready: the host starts";
}

static void page_accept(MenuPage *p, int row);
static void page_step(MenuPage *p, int row, int by);

/* Over the rider screen the room closes (port_online_shell takes the rider screen's Back to
   the title and opens ONLINE there); over the title it is ONLINE's own page. */
static void leave_room(MenuPage *p)
{
    hle_log("online menu: leaving the room\n");
    net_leave();
    player_host_stop(); /* leave also closes all child server processes */
    reset_selection();
    if (in_shell) {
        race = 0;
        menu_page_close(p);
    } else
        go(HOME);
}

static void page_update(MenuPage *p, int cursor)
{
    if (getenv("DHD_ONLINE_MENU_TRACE")) {
        static char last[160];
        char trace[160];
        snprintf(trace, sizeof trace, "view=%d row=%d entry=%s", screen, cursor,
                 screen == NAME || screen == CODE ? entry : "");
        if (strcmp(last, trace)) {
            snprintf(last, sizeof last, "%s", trace);
            hle_log("online menu: %s\n", trace);
        }
    }
    p->footer = status[0] ? status : NULL;
    switch (screen) {
    case HOME:
        if (!status[0] && cursor == HOME_HOST)
            p->footer = "Private native host (no Cloudflare relay)";
        else if (!status[0] && cursor >= 2 && cursor <= 4 && not_ready())
            p->footer = not_ready();
        break;
    case LIST:
        if (list_state == 2 && !list_shown) menu_page_rebuild(p);  /* the answer came: the rows again */
        break;
    case NAME:
    case CODE:
        {
            int action = text_input_edit(entry, (screen == NAME ? NAME_MAX : CODE_LENGTH) + 1,
                                         screen == NAME ? name_letters : code_letters);
            if (action < 0) { go(HOME); return; }
            if (action > 0) { page_accept(p, 1); return; }
        }
        entry_text();
        if (!status[0])
            p->footer = screen == CODE ? "TYPE/CTRL+V - ENTER JOIN - ESC BACK" : "TYPE OR PAD - ENTER DONE - ESC CANCEL";
        break;
    case HOST_BOOT: {
        int state = player_host_state();
        if (state == PLAYER_HOST_FAILED) {
            char reason[200];
            snprintf(reason, sizeof reason, "%s", player_host_error());
            player_host_stop();
            go(HOME);
            snprintf(status, sizeof status, "%.120s", reason);
        } else if (state == PLAYER_HOST_READY) {
            /* Native hosting never needs the directory or public race
               server to start. Local joining works offline. Registration
               affects only finding this H-code from another network. */
            enter_room(1, NULL);
        } else p->footer = "Starting built-in multiplayer server...";
        break;
    }
    case ROOM: {
        static NetLobby l;
        net_get_lobby(&l);
        room_update(p, &l);
        break;
    }
    }
}

static void page_accept(MenuPage *p, int row)
{
    status[0] = 0;
    switch (screen) {
    case HOME:
        home_row = row;
        if (row == 0) {
            snprintf(entry, sizeof entry, "%s", name);
            entry_pick = 0;
            go(NAME);
        } else if (row == 1) {
            if (!server()) snprintf(status, sizeof status, "No server: set server = ... in dhd.ini");
            else {
                start_list();
                go(LIST);
            }
        } else if (row == 2 || row == 3)
            enter_room(row == 2 ? 2 : 1, NULL);
        else if (row == 4) {
            entry[0] = 0;
            entry_pick = 0;
            go(CODE);
        } else if (row == HOME_HOST) {
            hosted_mode = 1;
            if (not_ready()) {
                snprintf(status, sizeof status, "%s", not_ready());
                hosted_mode = 0;
            } else if (player_host_start()) {
                go(HOST_BOOT);
            }
            else {
                snprintf(status, sizeof status, "%.120s", player_host_error());
                hosted_mode = 0;
            }
        } else if (row == HOME_UPDATE && outdated) {
            hle_log("online menu: Update Now\n");
            update_request();  /* ends the game; the first dhd.exe updates and starts it again */
        }
        break;
    case HOST_BOOT:
        player_host_stop();
        go(HOME);
        break;
    case LIST:
        if (list_state == 2 && row < list_count && list_count > 0) enter_room(0, list[row].code);
        else if (list_state == 2) {
            start_list();
            menu_page_rebuild(p);
        }
        break;
    case NAME:
    case CODE: {
        const char *letters = screen == NAME ? name_letters : code_letters;
        int max = screen == NAME ? NAME_MAX : CODE_LENGTH;
        size_t n = strlen(entry);
        if (row == 0) {
            if ((int)n < max) entry[n] = letters[entry_pick], entry[n + 1] = 0;
            if (screen == CODE && (int)n + 1 == max) menu_page_set_cursor(p, 1);  /* the code is whole: Join */
        } else if (screen == NAME) {
            while (n && entry[n - 1] == ' ') entry[--n] = 0;
            if (n) {
                snprintf(name, sizeof name, "%s", entry);
                save("name", "DHD_NET_NAME", name);
            }
            go(HOME);
        } else if ((int)n == CODE_LENGTH)
            enter_room(0, entry);
        else
            snprintf(status, sizeof status, "A code has %d letters and digits", CODE_LENGTH);
        break;
    }
    case ROOM: {
        NetLobby l;
        net_get_lobby(&l);
        if (row == R_TRACK || row == R_BOTS) {
            page_step(p, row, 1);
            break;
        }
        if (row == R_SELECT) {
            net_set_ready(0);
            selection_requested = 1;
        } else if (row == room_ready_row() && !strcmp(text[room_ready_row()], "Not Ready"))  /* the label shows the last snapshot */
            net_set_ready(0);
        else if (row == room_ready_row()) { ready_track = l.track, ready_options = l.laps; net_set_ready(1); }
        else if (row == room_start_row() && l.is_host) net_start_race();
        else if (row == room_leave_row()) leave_room(p);
        break;
    }
    }
}

static void page_back(MenuPage *p)
{
    switch (screen) {
    case HOME:
        menu_page_cancel(p);
        break;
    case NAME:
    case CODE: {
        size_t n = strlen(entry);
        if (n) entry[n - 1] = 0;
        else go(HOME);
        break;
    }
    case LIST:
        go(HOME);
        break;
    case HOST_BOOT:
        player_host_stop();
        go(HOME);
        break;
    case ROOM:
        leave_room(p);
        break;
    }
}

static void page_step(MenuPage *p, int row, int by)
{
    if (screen == ROOM && (row == R_TRACK || row == R_BOTS)) {  /* the host's track and options */
        NetLobby l;
        net_get_lobby(&l);
        if (!l.is_host) return;
        NetRoomSettings s = {-1, -1, -1, -1, -1, NULL};
        if (row == R_TRACK) {
            room_track = (room_track + by + COUNT(tracks)) % COUNT(tracks);
            s.track = tracks[room_track].index;
        } else
            s.laps = room_options = l.laps ^ ROOM_NO_BOTS;
        net_set_room(&s);
        menu_page_sound(by > 0);
        return;
    }
    if ((screen == NAME || screen == CODE) && row == 0) {
        int n = screen == NAME ? (int)sizeof name_letters - 1 : (int)sizeof code_letters - 1;
        entry_pick = (entry_pick + by + n) % n;
        menu_page_sound(by > 0);
    }
    (void)p;
}

static void page_left(MenuPage *p, int row) { page_step(p, row, -1); }
static void page_right(MenuPage *p, int row) { page_step(p, row, 1); }

/* The main menu's ONLINE row (patched func_001ED5B8), also when port_online_resume opens it
   again: the page until it closes; 2 when a room was joined (the rider screen next, as SINGLE
   PLAYER's), 0 when the player backed out. */
int port_online_menu(void)
{
    int resumed = resume_home;
    resume_home = 0;
    choosing = 0;
    reset_selection();
    load_player();
    offline_mode = SETUP_WORD(0x10C);
    offline_submode = SETUP_WORD(0x110);
    offline_players = SETUP_WORD(0x288);
    func_0023D898(0, &shell_distance, &shell_aspect);
    start_iso_check();
    page.build = page_build;
    page.update = page_update;
    page.accept = page_accept;
    page.back = page_back;
    page.left = page_left;
    page.right = page_right;
    page.title = "ONLINE";
    screen = HOME;
    visited_rider_screen = 0;
    if (!resumed) {
        home_row = 0;
        status[0] = 0;  /* a room that closed leaves its reason for HOME */
    }
    race = 0;
    in_shell = 0;
    hosted_mode = 0;
    run_page();
    if (!race) {
        net_leave();
        player_host_stop();
        reset_selection();
    }
    return race;
}

/* The main menu (patched func_001ED5B8), every frame: 1 once to choose ONLINE again after a
   room closed, when the camera is back on the title. */
int port_online_resume(void)
{
    void *animation = (void *)(uintptr_t)(0x08000000u + 0x4C6370);
    if (!resume_home) return 0;
    func_00203590(animation, 0x2330, 0, 1);
    float t = func_00203C28(animation);
    return t <= 0.0f || t >= 1.0f;
}

/* The main menu's labels (patched func_001ED5B8): its table has three; the fourth is ONLINE. */
int port_main_label(const int *table, int index)
{
    return index < 3 ? table[index] : (int)(uintptr_t)"ONLINE";
}

static void keep_selection(void)
{
    chosen_rider = SETUP_WORD(0x290);
    chosen_bike = SETUP_WORD(0x11C);
    for (int i = 0; i < 3; i++) {
        chosen_outfit[i] = SETUP_WORD(0x1B8 + 4 * i);
        chosen_cycle[i] = SETUP_WORD(0x11C + 4 * i);
    }
    selected = 1;
    net_pick(-1, chosen_rider / 32, chosen_bike);
    net_set_ready(0);
    hle_log("online menu: selection rider %d, outfit %d %d %d, bike %d %d %d\n",
            chosen_rider, chosen_outfit[0], chosen_outfit[1], chosen_outfit[2],
            chosen_cycle[0], chosen_cycle[1], chosen_cycle[2]);
}

/* The shell's frame, before its menu runs; 1 skips that menu this frame. Only online selection
   enters here. The room is a page over the rider screen (menu 3):
   - GET READY (menu 9) keeps the choice and flies the camera back to the rider screen, as the
     main menu's SINGLE PLAYER does, instead of loading a race before everyone is ready;
   - the rider screen's Back (Triangle) opens the room instead of leaving for the title;
   - Change Selection closes the room on the rider screen itself;
   - leaving the room (or losing it) takes the rider screen's own Back to the title, where the
     main menu opens ONLINE again (port_online_resume). */
int port_online_shell(void)
{
    if (!choosing) return 0;
    int menu = SHELL_WORD(0x29DED8);
    if (menu == 16) {
        /* Single races only: bypass the career's NEW / CONTINUE and mode menus. */
        SHELL_WORD(0x29DED8) = 11;
        SHELL_WORD(0x29DEEC) = 1;
        SETUP_WORD(0x10C) = 12;
        SETUP_WORD(0x110) = 0;
        return 0;
    }
    NetLobby connection;
    net_get_lobby(&connection);
    int lost = connection.state == NET_FAILED || connection.state == NET_IDLE;
    if (menu == 18) {
        /* The shop's START: when its camera has flown out, retail goes on to the level chooser,
           which sets up the race's loading (func_00139BF8) and takes the camera. The choice is
           complete here: keep it and leave as the rider screen's Back does; the room (with the
           host's track) opens over the rider screen. */
        void *animation = (void *)(uintptr_t)(0x08000000u + 0x4C6370);
        int rider_index = SHELL_WORD(0x4C6910 + 4 * SHELL_WORD(0x29DF68));
        func_00203590(animation, 0x2330, 0, D_00275CB0[rider_index]);
        if (func_00203C28(animation) < 1.0f) return 0;
        if (!lost) keep_selection();
        selection_end();
        func_001F79D8(0, 0x3C);  /* the shop's exit faded to black; the level chooser fades back */
        return 1;  /* the main menu next frame: the flight to the rider screen below */
    }
    /* Reaching the retail rider screen is required before treating the
       transitional 15/0 menu as 'Back'. Without this guard, clicking Choose
       immediately reopens RACE ROOM and skips the rider/bike/outfit shop. */
    if (menu == 3) visited_rider_screen = 1;
    if (room_pending && (menu == 15 || menu == 0)) room_pending = 0;  /* backed out meanwhile */
    if (room_pending) {
        /* The rider screen's own frames put the camera on it; the room's page freezes it. */
        if (menu != 3 || ++room_pending < 45) return 0;
        room_pending = 0;
    } else if (menu == 9 || menu == 15 || menu == 0) {
        /* These menu IDs also appear *before* the retail rider picker is
           loaded. Do not steal its input or consider it a selection then. */
        if (!visited_rider_screen) return 0;
        if (menu == 9 && !lost) keep_selection();
        func_0023D8D8(0, shell_distance, shell_aspect);
        port_online_choose_begin();
        room_pending = 1;
        return 0;
    } else if (menu == 3) {
        /* Read the pads, not func_001F4A98: its press latch would take the rider screen's own. */
        static int was_held;
        int held = (func_00220230(0) | func_00220230(1)) & 0xFFFF;
        int back = held & 0x1000 && !(was_held & 0x1000);  /* Triangle */
        was_held = held;
        if (hold_rider) {
            if (held) return 1;
            hold_rider = 0;
        }
        if (!back && !lost) return 0;
    } else
        return 0;

    in_shell = 1;
    func_001F57D0(0x232B, 0);  /* the rider screen's name plates, which would show through */
    func_001F57D0(0x233D, 0);
    func_001F57D0(0x235F, 0);
    go(ROOM);
    race = 0;
    run_page();
    in_shell = 0;
    if (race == 2) {
        hold_rider = 1;  /* the rider screen is here: choose on it */
        return 1;
    }
    choosing = 0;
    visited_rider_screen = 0;
    if (race == 1) {
        SHELL_WORD(0x29DED8) = 9;
        SHELL_WORD(0x29DEEC) = 8;
        SHELL_WORD(0x29DF40) = 120;
        SHELL_WORD(0x29DF44) = 0;
        return 1;
    }
    reset_selection();
    net_leave();
    player_host_stop(); /* player exited hosted race back to single-player */
    selection_end();
    SETUP_WORD(0x10C) = offline_mode;
    SETUP_WORD(0x110) = offline_submode;
    SETUP_WORD(0x288) = offline_players;
    resume_home = 1;
    port_online_relock();
    return 1;
}

/* Guests use the same retail handoff into the shop, without choosing a competing track. */
int port_online_guest_track(void)
{
    if (!choosing) return 0;
    NetLobby lobby;
    net_get_lobby(&lobby);
    return !lobby.is_host;
}

int port_online_single_race(void) { return choosing; }
