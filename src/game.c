/* game.c - game flow: input, menus, HUD, dialogs, saving, warps, rendering */
#include "game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

Input in;
Level lv;
int cur_level = -1;
int gstate = GS_TITLE;
int g_frame;
int paused;
SaveSlot save[3];
int cur_slot;
int g_course_timer, g_timer_on;
int g_star_dance_star = -1;
int g_entry_painting = 0;
int g_blue_switch_timer;
int g_race_state;
float g_fade;
int g_boss_hits;
vec3 g_wind;
float g_gust;
const char *g_hud_msg;
int g_hud_msg_t;
int g_coin_star_idx = -1;
int g_nosave;

static int coin_star_spawned;
static int menu_sel;
static int state_t;
static int erase_mode;
static int pause_sel;
static int pending_level = -1, pending_spawn, pending_select, pending_course_exit, pending_title;
static int fade_dir;
static int course_intro_t;
static int last_health_t;
static int star_select_course;
static int powermeter_show;
static int ending_t;
static int gameover_t;

const CourseInfo courses[CRS_COUNT] = {
    { "PENGUIN PEAK", 6, { "King Daemon of the Peak", "Race the Quick Gnu", "Red Fish on the Floating Isle",
                           "Crusher Bridge Crossing", "Behind the Watchdog's Gate", "100 Fish of the Peak" }, LV_PEAK },
    { "BITSTREAM BAY", 6, { "Treasure of the Sunken Ship", "Red Fish of the Reef", "Climb the Lighthouse",
                            "The Clam's Secret", "Swim Through the Rings", "100 Fish of the Bay" }, LV_BAY },
    { "BLIZZARD FORTRESS", 6, { "Top of the Fortress", "The Orphaned Child Process", "Red Fish on the Sliding Platforms",
                                 "Push the Ice Brute", "Atop the Frozen Pillar", "100 Fish of the Fortress" }, LV_FORT },
    { "SECRET ICE SLIDE", 2, { "Ice Slide Finish", "Ice Slide Under 21 Seconds" }, LV_SLIDE },
    { "SEGFAULT'S LAIR", 0, { 0 }, LV_LAIR },
    { "CASTLE SECRETS", 2, { "Rocket Ride to the Castle Roof", "The Penguin's Gift" }, -1 },
    { "DAEMON DRIFTS", 5, { "Top of the Ice Pyramid", "Red Fish of the Hot Spring", "Light the Four Ice Obelisks",
                            "Powder Snow Crossing", "100 Fish of the Drifts" }, LV_DRIFTS },
    { "ZOMBIE PROCESS MANOR", 5, { "Zombie King's Hall", "Red Fish in the Library", "Star on the Rooftop",
                                   "Bust the Yard Zombies", "100 Fish of the Manor" }, LV_MANOR },
    { "ICICLE SPIRE", 4, { "Halfway Up the Spire", "Top of the Icicle Spire", "Spire Climb Under 60 Seconds",
                           "Red Fish in the Chimneys" }, LV_SPIRE },
    { "WHITEOUT LABYRINTH", 3, { "Heart of the Labyrinth", "Red Fish in the Dead Ends", "Escape Unseen" }, LV_MAZE },
    { "AURORA SEA RALLY", 5, { "Beat the SELinux Seal", "Sea Rally Under 60 Seconds", "Red Fish on the Ice Floes",
                               "Ride the Whale's Spout", "100 Fish of the Sea" }, LV_SEA },
    { "FROSTCLOUD SKYWAY", 4, { "Sky Flight: 60 Fish", "Sky Flight: 120 Fish", "Sky Flight: 200 Fish",
                                "Sky Flight: 300 Fish" }, LV_SKY },
    { "STARDUST ORBIT", 6, { "Through the Station Core", "Comet Ring Run", "Catch the Ice Comet",
                             "Red Fish of the Asteroid Belt", "Smash the Ice Meteors", "100 Fish of Orbit" }, LV_SPACE },
    { "NANO VILLAGE", 5, { "Tow the Enemy Fleet", "Rain on the Forest Fires", "Rout the Tiny Army",
                           "Red Fish of the Tiny Kingdom", "100 Fish of the Village" }, LV_NANO },
    { "HUGEPAGE HOUSE", 6, { "Atop the Giant's Table", "Toaster Launch to the Top Shelf", "Distract the Giant Cat",
                             "Past the Giant's Slippers", "Red Fish in the Giant's Room", "100 Fish of the House" }, LV_HUGE },
    { "UPTIME HEIGHTS", 6, { "Top of the Antenna", "Ride the Tower Crane", "Girder Walk in the Wind",
                             "Skydive into the Rooftop Pool", "Red Fish Among the Rooftops", "100 Fish of the Heights" }, LV_CITY },
    { "CORE DUMP CAVERNS", 6, { "Bottom of the Abyss", "Mine Cart Plunge", "Bounce Up the Mushroom Chimney",
                                "Light the Five Crystals", "Red Fish of the Deep", "100 Fish of the Caverns" }, LV_CAVE },
};

/* ======================================================================
 * input
 * ====================================================================== */
static unsigned prev_held;
static int script_active;
static const char *script_p;
static int script_left;
static unsigned script_keys;
int g_trace;

void input_set_script(const char *s) { script_p = s; script_active = 1; script_left = 0; }

static void script_next(void)
{
    /* format: "frames:keys;frames:keys" keys: U D L R a b z s(start) q e */
    script_keys = 0;
    if (!script_p || !*script_p) { script_active = 0; return; }
    script_left = atoi(script_p);
    while (*script_p && *script_p != ':') script_p++;
    if (*script_p == ':') script_p++;
    while (*script_p && *script_p != ';') {
        switch (*script_p) {
        case 'U': script_keys |= 1u << 20; break;
        case 'D': script_keys |= 1u << 21; break;
        case 'L': script_keys |= 1u << 22; break;
        case 'R': script_keys |= 1u << 23; break;
        case 'a': script_keys |= BT_A; break;
        case 'b': script_keys |= BT_B; break;
        case 'z': script_keys |= BT_Z; break;
        case 's': script_keys |= BT_START; break;
        case 'q': script_keys |= BT_CL; break;
        case 'e': script_keys |= BT_CR; break;
        }
        script_p++;
    }
    if (*script_p == ';') script_p++;
}

void input_clear(void) { memset(&in, 0, sizeof in); prev_held = ~0u; }

void input_update(void)
{
    const unsigned char *k = plat_keys;
    float sx = 0, sy = 0, l;
    unsigned h = 0;
    if (k[K_LEFT] || k[K_A]) sx -= 1;
    if (k[K_RIGHT] || k[K_D]) sx += 1;
    if (k[K_UP] || k[K_W]) sy += 1;
    if (k[K_DOWN] || k[K_S]) sy -= 1;
    if (k[K_SPACE] || k[K_K]) h |= BT_A;
    if (k[K_J] || k[K_X] || k[K_LSHIFT]) h |= BT_B;
    if (k[K_Z] || k[K_L] || k[K_C] || k[K_LCTRL]) h |= BT_Z;
    if (k[K_ENTER] || k[K_ESC] || k[K_P]) h |= BT_START;
    if (k[K_Q]) h |= BT_CL;
    if (k[K_E]) h |= BT_CR;
    if (k[K_R] || k[K_PGUP]) h |= BT_CU;
    if (k[K_F] || k[K_PGDN]) h |= BT_CD;
    if (k[K_TAB] || k[K_V]) h |= BT_R;
    in.cx = in.cy = 0;
    if (plat_pad.connected) {
        Gamepad *p = &plat_pad;
        float m = sqrtf(p->lx * p->lx + p->ly * p->ly);
        if (m > 0.2f) { sx += p->lx; sy += p->ly; }
        if (p->btn & GP_A) h |= BT_A;
        if (p->btn & (GP_B | GP_X)) h |= BT_B;
        if (p->btn & (GP_LT | GP_RT | GP_LB)) h |= BT_Z;
        if (p->btn & GP_START) h |= BT_START;
        if (p->btn & GP_RB) h |= BT_R;
        if (p->btn & GP_Y) h |= BT_CU;
        if (p->btn & GP_BACK) h |= BT_CD;
        if (p->btn & GP_DLEFT) h |= BT_CL;
        if (p->btn & GP_DRIGHT) h |= BT_CR;
        if (p->btn & GP_DUP) h |= BT_CU;
        if (p->btn & GP_DDOWN) h |= BT_CD;
        if (fabsf(p->rx) > 0.25f) in.cx = p->rx;
        if (fabsf(p->ry) > 0.25f) in.cy = p->ry;
    }
    if (script_active) {
        if (script_left <= 0) script_next();
        script_left--;
        sx = sy = 0;
        h = script_keys & 0xFFFF;
        if (script_keys & (1u << 20)) sy += 1;
        if (script_keys & (1u << 21)) sy -= 1;
        if (script_keys & (1u << 22)) sx -= 1;
        if (script_keys & (1u << 23)) sx += 1;
    }
    l = sqrtf(sx * sx + sy * sy);
    if (l > 1) { sx /= l; sy /= l; l = 1; }
    in.sx = sx; in.sy = sy; in.mag = l;
    if (sy > 0.5f) h |= BT_UP;
    if (sy < -0.5f) h |= BT_DOWN;
    if (sx < -0.5f) h |= BT_LEFT;
    if (sx > 0.5f) h |= BT_RIGHT;
    in.pressed = h & ~prev_held;
    in.released = ~h & prev_held;
    in.held = h;
    prev_held = h;
}

/* ======================================================================
 * save
 * ====================================================================== */
#define SAVE_MAGIC 0x54555839u
#define SAVE_MAGIC_V8 0x54555838u   /* previous format: 16 courses */
#define SAVE_MAGIC_V7 0x54555837u   /* older format: 8 courses */
#define save_name plat_user_file("tux64.sav")

typedef struct {
    unsigned char exists;
    unsigned char stars[8];
    unsigned short coin_hi[8];
    unsigned flags;
    unsigned char lives;
} SaveSlotV7;

typedef struct {
    unsigned char exists;
    unsigned char stars[16];
    unsigned short coin_hi[16];
    unsigned flags;
    unsigned char lives;
} SaveSlotV8;

void save_load(void)
{
    FILE *f = fopen(save_name, "rb");
    unsigned magic = 0;
    memset(save, 0, sizeof save);
    if (!f) return;
    if (fread(&magic, 4, 1, f) == 1) {
        if (magic == SAVE_MAGIC) {
            if (fread(save, sizeof save, 1, f) != 1) memset(save, 0, sizeof save);
        } else if (magic == SAVE_MAGIC_V8) {
            SaveSlotV8 old[3];
            int i, c;
            if (fread(old, sizeof old, 1, f) == 1)
                for (i = 0; i < 3; i++) {
                    save[i].exists = old[i].exists;
                    for (c = 0; c < 16; c++) { save[i].stars[c] = old[i].stars[c]; save[i].coin_hi[c] = old[i].coin_hi[c]; }
                    save[i].flags = old[i].flags;
                    save[i].lives = old[i].lives;
                }
        } else if (magic == SAVE_MAGIC_V7) {
            SaveSlotV7 old[3];
            int i, c;
            if (fread(old, sizeof old, 1, f) == 1)
                for (i = 0; i < 3; i++) {
                    save[i].exists = old[i].exists;
                    for (c = 0; c < 8; c++) { save[i].stars[c] = old[i].stars[c]; save[i].coin_hi[c] = old[i].coin_hi[c]; }
                    save[i].flags = old[i].flags;
                    save[i].lives = old[i].lives;
                }
        }
    }
    fclose(f);
}

void save_write(void)
{
    FILE *f;
    unsigned magic = SAVE_MAGIC;
    if (g_nosave) return;
    f = fopen(save_name, "wb");
    if (!f) return;
    fwrite(&magic, 4, 1, f);
    fwrite(save, sizeof save, 1, f);
    fclose(f);
}

static int slot_stars(int s)
{
    int c, b, n = 0;
    for (c = 0; c < CRS_COUNT; c++)
        for (b = 0; b < 8; b++) if (save[s].stars[c] & (1 << b)) n++;
    return n;
}
int total_stars(void) { return slot_stars(cur_slot); }
int star_course(void) { return lv.course >= 0 ? lv.course : CRS_SECRET; }
int has_star(int course, int star) { return course >= 0 && course < CRS_COUNT && (save[cur_slot].stars[course] & (1 << star)); }

/* ======================================================================
 * dialog
 * ====================================================================== */
static char dlg_buf[1024];
static int dlg_on, dlg_chars, dlg_page_start, dlg_t;

/* characters that must not start a line (Japanese kinsoku) */
static int no_line_start(unsigned c)
{
    return c == 0x3001 || c == 0x3002 || c == 0xFF01 || c == 0xFF1F || c == 0x300D || c == 0x300F ||
           c == 0x30FC || c == 0x2026 || c == 0xFF09 || c == 0x30A1 || c == 0x30C3 || c == 0x3063 ||
           c == '!' || c == '?' || c == ')' || c == '.' || c == ',';
}

/* word-wrap text into dlg_buf by pixel width (font units): explicit newlines are kept,
 * English wraps at spaces, Japanese may wrap between any two characters */
static void dialog_wrap(const char *text, float maxw)
{
    int o = 0;
    float col = 0;
    const char *p = text;
    while (*p && o < (int)sizeof dlg_buf - 8) {
        const char *q = p;
        unsigned c = utf8_next(&q);
        if (c == '\n') { dlg_buf[o++] = '\n'; col = 0; p = q; continue; }
        if (c == ' ') {
            /* measure the next ASCII word */
            const char *r = q;
            float ww = 0;
            while (*r && *r != ' ' && *r != '\n' && (unsigned char)*r < 0x80) { ww += 6; r++; }
            if (col == 0) { p = q; continue; }            /* no spaces at line start */
            if (col + 6 + ww > maxw) { dlg_buf[o++] = '\n'; col = 0; p = q; continue; }
            dlg_buf[o++] = ' ';
            col += 6;
            p = q;
            continue;
        }
        if (col + glyph_adv(c) > maxw && col > 0 && !no_line_start(c)) { dlg_buf[o++] = '\n'; col = 0; }
        while (p < q) dlg_buf[o++] = *p++;
        col += glyph_adv(c);
    }
    dlg_buf[o] = 0;
}

void dialog_open(const char *text)
{
    float w = minf(g_vw - 40, 600);
    float maxw = (w - 32) / 2.0f;
    text = tr(text);
    dialog_wrap(text, maxw);
    dlg_on = 1;
    dlg_chars = 0;
    dlg_page_start = 0;
    dlg_t = 0;
    if (pl.act != ACT_DEATH && pl.act != ACT_STAR_DANCE && gstate == GS_PLAY &&
        pl.act != ACT_SWIM && pl.act != ACT_SKYFLY && pl.act != ACT_SKATE && pl.act != ACT_SPACEFLY && !player_in_air())
        player_set_act(ACT_FROZEN);
}
int dialog_active(void) { return dlg_on; }

/* returns the end index (exclusive) of the page starting at s: max 4 lines */
static int page_end(int s)
{
    int lines = 0, i = s;
    while (dlg_buf[i]) {
        if (dlg_buf[i] == '\n') { lines++; if (lines == 4) return i; }
        i++;
    }
    return i;
}

static void dialog_update(void)
{
    int pe = page_end(dlg_page_start);
    dlg_t++;
    if (dlg_page_start + dlg_chars < pe) {
        dlg_chars += g_lang == LANG_JA ? 3 : 2;
        if (dlg_t % 4 == 0) sfx(SFX_TEXT);
        if (in.pressed & (BT_A | BT_B)) dlg_chars = pe - dlg_page_start;
        return;
    }
    if (in.pressed & (BT_A | BT_B | BT_START)) {
        if (dlg_buf[pe]) {
            dlg_page_start = pe + 1;
            dlg_chars = 0;
            sfx(SFX_MENU_MOVE);
        } else {
            dlg_on = 0;
            sfx(SFX_MENU_OK);
            input_clear();
            in.held = prev_held = BT_A | BT_B | BT_START | BT_Z;
        }
    }
}

static void dialog_draw(void)
{
    char tmp[1024];
    int pe = page_end(dlg_page_start), n;
    float w = minf(g_vw - 40, 600), x = (g_vw - w) * 0.5f, y = 330;
    n = pe - dlg_page_start;
    if (dlg_chars < n) n = dlg_chars;
    while (n > 0 && n < pe - dlg_page_start && ((unsigned char)dlg_buf[dlg_page_start + n] & 0xC0) == 0x80) n--;
    memcpy(tmp, dlg_buf + dlg_page_start, (size_t)n);
    tmp[n] = 0;
    gfx_rect(x, y, w, 130, 0.02f, 0.02f, 0.1f, 0.82f);
    gfx_rect(x, y, w, 3, 0.95f, 0.8f, 0.3f, 1);
    gfx_rect(x, y + 127, w, 3, 0.95f, 0.8f, 0.3f, 1);
    gfx_text_shadow(x + 16, y + 16, 2.0f, tmp, 1, 1, 1, 1);
    if (dlg_page_start + dlg_chars >= pe && (g_frame / 15) % 2)
        gfx_text(x + w - 30, y + 105, 2, dlg_buf[pe] ? "v" : "*", 1, 0.85f, 0.3f, 1);
}

void hud_message(const char *m, int frames)
{
    extern int g_trace;
    g_hud_msg = tr(m);
    g_hud_msg_t = frames;
    if (g_trace) printf("HUD f%d: %s\n", g_frame, m);   /* lets the play-tests see game events */
}

/* ======================================================================
 * level / warps
 * ====================================================================== */
void level_load(int level, int spawn)
{
    vec3 sp;
    float sy;
    int keep_coins = 0;
    if (lv.dl) glDeleteLists(lv.dl, 1);
    if (lv.dl_alpha) glDeleteLists(lv.dl_alpha, 1);
    memset(&lv, 0, sizeof lv);
    objs_clear();
    col_reset();
    g_race_n = 0;
    g_race_state = 0;
    g_blue_switch_timer = 0;
    g_timer_on = 0;
    g_course_timer = 0;
    g_coin_star_idx = -1;
    coin_star_spawned = 0;
    rng_state = 0x1234567u + (unsigned)level * 77u;
    cur_level = level;
    build_level(level);
    if (spawn < 0 || spawn >= MAX_SPAWNS) spawn = 0;
    sp = lv.spawn[spawn];
    sy = lv.spawn_yaw[spawn];
    if (!keep_coins) { pl.coins = 0; }
    pl.red_coins = 0;
    player_init(sp, sy);
    if (pl.health <= 0) pl.health = 8;
    camera_reset();
    music_play(lv.music);
    course_intro_t = lv.course >= 0 ? 180 : 0;
    paused = 0;
    dlg_on = 0;
    g_star_dance_star = -1;
    objs_collision();
    if (level == LV_SKY) {
        sky_reset();
        dialog_open("FROSTCLOUD SKYWAY\nFly free with the rocket! Move in any direction\nwith the stick, hold A to dash.\n"
                    "Draw a loop around fish with your trail to grab\nthem all at once. You have 2 minutes!\n"
                    "Fish lines in one flight: 60 / 120 / 200 / 300.\nEach line reached gives its star.");
    }
    if (level == LV_SPACE) {
        space_reset();
        camera_update();
        dialog_open("STARDUST ORBIT\nOut here the rocket never runs out of fuel!\n"
                    "Stick: steer (forward dives, back climbs)\nA: boost   B: brake   Z: quick turn\n"
                    "Fly through the rings, chase the comet and ram\nthe ice meteors at full boost to smash them!");
    }
    if (lv.scale_world) scale_reset();
    if (lv.tall_world) vert_reset();
    if (level == LV_NANO)
        dialog_open("NANO VILLAGE\nWhoa... everything here is TINY! To the little\npenguins of this kingdom, Tux is a giant.\n"
                    "Watch your step, help them out, and look:\nthe clouds only reach up to your knees!");
    if (level == LV_HUGE)
        dialog_open("HUGEPAGE HOUSE\nWhoa... everything here is HUGE! Tux is no\nbigger than a cup in this giant's house.\n"
                    "Climb the furniture, ride the toaster and\ndon't wake the cat sleeping on the rug!");
    if (level == LV_CITY)
        dialog_open("UPTIME HEIGHTS\nThe rooftops of a city of skyscrapers, 120 m\nabove the street - the clouds float below you!\n"
                    "Walk the girders, ride the lifts and the crane,\nand don't look down... too often.");
    if (level == LV_CAVE)
        dialog_open("CORE DUMP CAVERNS\nA giant shaft drills 290 m down into the\nearth, all the way to a lake of magma.\n"
                    "The deeper you go, the darker it gets. Follow\nthe ledge down, or take the mine cart!");
    if (level == LV_SEA) {
        sfx(SFX_1UP);
        dialog_open("You got the DASH BOOTS!\nThey make you run so fast that you skim over\nthe water... and they won't come off on this sea!\n"
                    "Left/right: steer   Back: brake   Z: carve\nA: jump   B in mid-air: spin trick\nSkate into the seal on the dock to race it!");
    }
    {
        extern int g_trace;
        if (g_trace) printf("level %d loaded: %d objects, %d collision tris\n", level, nobj, col_static_count());
    }
}

void warp_to(int level, int spawn, int star_select)
{
    if (pending_level >= 0 || fade_dir) return;
    pending_level = level;
    pending_spawn = spawn;
    pending_select = star_select;
    fade_dir = 1;
    pl.no_ctrl = 1;
}

void course_exit(void)
{
    if (fade_dir) return;
    pending_course_exit = 1;
    fade_dir = 1;
    pl.no_ctrl = 1;
}

static void return_to_lobby(void)
{
    level_load(LV_LOBBY, g_entry_painting);
    pl.health = 8;
}

void game_add_coins(int n, vec3 p)
{
    int before = pl.coins;
    (void)p;
    pl.coins += n;
    player_heal(n);
    sfx(n >= 5 ? SFX_BLUECOIN : SFX_COIN);
    if (pl.coins / 50 > before / 50) {
        pl.lives++;
        sfx(SFX_1UP);
        hud_message("1-UP!", 90);
    }
    if (lv.course >= 0 && lv.course < CRS_COUNT) {
        if (pl.coins > save[cur_slot].coin_hi[lv.course]) save[cur_slot].coin_hi[lv.course] = (unsigned short)pl.coins;
    }
    if (g_coin_star_idx >= 0 && pl.coins >= 100 && !coin_star_spawned) {
        coin_star_spawned = 1;
        star_spawn(v3add(pl.pos, v3(0, 3.5f, 0)), g_coin_star_idx, 1, pl.pos);
        hud_message("100 FISH! A Kernel Star appeared!", 150);
    }
}

void game_red_coin(vec3 p)
{
    static char msg[256];
    int i;
    pl.red_coins++;
    pl.coins += 2;
    {
        int before = pl.coins - 2;
        if (pl.coins / 50 > before / 50) { pl.lives++; sfx(SFX_1UP); }
    }
    if (g_coin_star_idx >= 0 && pl.coins >= 100 && !coin_star_spawned) {
        coin_star_spawned = 1;
        star_spawn(v3add(pl.pos, v3(0, 3.5f, 0)), g_coin_star_idx, 1, pl.pos);
    }
    player_heal(2);
    sfx_ex(SFX_REDCOIN, 1.0f, powf(1.0595f, (float)(pl.red_coins * 2)));
    snprintf(msg, sizeof msg, tr("RED FISH %d/8"), pl.red_coins);
    hud_message(msg, 90);
    if (pl.red_coins == 8) {
        for (i = 0; i < nobj; i++) {
            if (objs[i].active && objs[i].type == OT_STARMARKER) {
                star_spawn(objs[i].pos, objs[i].param, 1, p);
                hud_message("All 8 red fish! A Kernel Star appeared!", 150);
                break;
            }
        }
    }
}

void game_star_collected(int star, vec3 p)
{
    int c = star_course();
    (void)p;
    if (!(save[cur_slot].stars[c] & (1 << star))) {
        save[cur_slot].stars[c] |= (unsigned char)(1 << star);
    }
    save[cur_slot].lives = (unsigned char)(pl.lives > 99 ? 99 : pl.lives);
    save_write();
    g_star_dance_star = star;
    if (pl.held >= 0) { objs[pl.held].flags &= ~OF_HELD; pl.held = -1; }
    pl.pole = -1;
    if (pl.act == ACT_SWIM) pl.vel = v3(0, 0, 0);
    player_set_act(ACT_STAR_DANCE);
    pl.visible = 1;
    pl.invinc = 0;
    cam.mode = CAM_DANCE;
    music_play(MUS_STARGET);
    g_timer_on = 0;
}

void game_grand_star(void)
{
    save[cur_slot].flags |= FL_BEAT_GAME;
    save[cur_slot].lives = (unsigned char)(pl.lives > 99 ? 99 : pl.lives);
    save_write();
    player_set_act(ACT_STAR_DANCE);
    cam.mode = CAM_DANCE;
    music_play(MUS_STARGET);
    g_star_dance_star = 99;
}

void game_player_died(void)
{
    if (fade_dir) return;
    pl.lives--;
    if (pl.lives < 0) {
        pending_title = 2;   /* game over */
    } else if (lv.course >= 0) {
        pending_course_exit = 2;
    } else {
        pending_level = cur_level;
        pending_spawn = 0;
        pending_select = 0;
    }
    fade_dir = 1;
}

/* ======================================================================
 * game update
 * ====================================================================== */
static void start_new_file(int slot)
{
    cur_slot = slot;
    if (!save[slot].exists) {
        memset(&save[slot], 0, sizeof save[slot]);
        save[slot].exists = 1;
        save[slot].lives = 4;
        save_write();
    }
    pl.lives = save[slot].lives ? save[slot].lives : 4;
    pl.health = 8;
    gstate = GS_PLAY;
    level_load(LV_GROUNDS, 0);
    if (!(save[slot].flags & FL_SEEN_INTRO)) {
        save[slot].flags |= FL_SEEN_INTRO;
        save_write();
        dialog_open("Dear Tux,\n"
                    "The Glitch King SEGFAULT has invaded Kernel\n"
                    "Castle! He corrupted the castle and scattered\n"
                    "its Kernel Stars across the portal worlds.\n"
                    "Without the stars the kernel cannot boot...\n"
                    "Please recover the Kernel Stars and defeat\n"
                    "Segfault in his lair beneath the castle!\n"
                    "              - The Penguins of Kernel Castle\n"
                    "TIP: Walk up to signs and penguins and\npress B to read or talk.");
    }
}

static void fade_finished(void)
{
    if (pending_title == 2) {
        pending_title = 0;
        gstate = GS_GAMEOVER;
        gameover_t = 0;
        music_play(MUS_GAMEOVER);
        save[cur_slot].lives = 4;
        save_write();
        return;
    }
    if (pending_title == 1) {
        pending_title = 0;
        gstate = GS_TITLE;
        state_t = 0;
        music_play(MUS_TITLE);
        return;
    }
    if (pending_course_exit) {
        pending_course_exit = 0;
        return_to_lobby();
        pl.no_ctrl = 0;
        return;
    }
    if (pending_level >= 0) {
        int lvl = pending_level, sp = pending_spawn;
        pending_level = -1;
        if (pending_select) {
            int c;
            gstate = GS_STARSELECT;
            star_select_course = 0;
            for (c = 0; c < CRS_COUNT; c++) if (courses[c].level == lvl) star_select_course = c;
            state_t = 0;
            menu_sel = 0;
            music_play(MUS_SELECT);
            pending_spawn = sp;
            return;
        }
        if (lvl != LV_LOBBY && lvl != LV_GROUNDS && cur_level == LV_LOBBY) { /* entering a course from the lobby */ }
        level_load(lvl, sp);
        pl.no_ctrl = 0;
    }
}

static void play_update(void)
{
    if (dlg_on) { dialog_update(); return; }
    if (paused) {
        int nopt = lv.course >= 0 ? 3 : 2;
        if (in.pressed & BT_UP) { pause_sel = (pause_sel + nopt - 1) % nopt; sfx(SFX_MENU_MOVE); }
        if (in.pressed & BT_DOWN) { pause_sel = (pause_sel + 1) % nopt; sfx(SFX_MENU_MOVE); }
        if (in.pressed & (BT_START | BT_A)) {
            int opt = pause_sel;
            if (lv.course < 0 && opt == 1) opt = 2;
            paused = 0;
            sfx(SFX_PAUSE);
            if (opt == 1) { course_exit(); }
            else if (opt == 2) {
                save[cur_slot].lives = (unsigned char)(pl.lives > 99 ? 99 : pl.lives);
                save_write();
                pending_title = 1;
                fade_dir = 1;
            }
        }
        return;
    }
    if ((in.pressed & BT_START) && !fade_dir && pl.act != ACT_DEATH && pl.act != ACT_STAR_DANCE) {
        paused = 1;
        pause_sel = 0;
        sfx(SFX_PAUSE);
        return;
    }
    /* blizzard gusts: build up every ~9 seconds, the direction wobbles a little */
    if (lv.snow == 2) {
        float t = g_frame / 60.0f, s = sinf(t * TAU_F / 9.0f), prev = g_gust;
        float yaw = lv.wind_yaw + sinf(t * 0.23f) * 0.4f;
        g_gust = s > 0 ? s * s : 0;
        g_wind = v3scale(yaw_dir(yaw), 3.0f * g_gust);
        if (prev < 0.35f && g_gust >= 0.35f) sfx_ex(SFX_THROW, 0.9f, 0.35f);
        if (g_gust > 0.35f && g_frame % 25 == 0) sfx_ex(SFX_SLIDE, 0.5f * g_gust, 0.5f);
    } else { g_wind = v3(0, 0, 0); g_gust = 0; }
    sky_update();
    space_update();
    scale_update();
    vert_update();
    objs_update();
    objs_collision();
    player_update();
    camera_update();
    if (g_timer_on) g_course_timer++;
    if (course_intro_t > 0) course_intro_t--;
    if (g_hud_msg_t > 0) g_hud_msg_t--;
    if (pl.act == ACT_STAR_DANCE && g_star_dance_star >= 0) {
        if (g_star_dance_star == 99) {
            if (pl.act_t > 200 && !fade_dir) {
                gstate = GS_ENDING;
                ending_t = 0;
                music_play(MUS_ENDING);
            }
        } else if (pl.act_t > 200) {
            static char msg[256];
            if (pl.act_t == 201) {
                snprintf(msg, sizeof msg, tr("You got a Kernel Star!\n\"%s\""),
                         tr(courses[star_course()].star_names[g_star_dance_star]));
                dialog_open(msg);
                pl.act_t = 202;
            } else if (!dlg_on && pl.act_t > 203) {
                g_star_dance_star = -1;
                if (lv.course >= 0) course_exit();
                else {
                    /* castle secret star: keep playing */
                    player_set_act(ACT_IDLE);
                    cam.mode = CAM_FOLLOW;
                    music_play(lv.music);
                }
            }
        }
    }
    if (pl.health < 8 || pl.act == ACT_SWIM) { powermeter_show = 1; last_health_t = 120; }
    else if (last_health_t > 0) last_health_t--;
    else powermeter_show = 0;
}

void game_update(void)
{
    g_frame++;
    state_t++;
    /* fades */
    if (fade_dir == 1) {
        g_fade += DT * 2.5f;
        if (g_fade >= 1) { g_fade = 1; fade_dir = -1; fade_finished(); }
        if (gstate == GS_PLAY && !dlg_on) {
            objs_collision();
            camera_update();
        }
        return;
    } else if (fade_dir == -1) {
        g_fade -= DT * 2.5f;
        if (g_fade <= 0) { g_fade = 0; fade_dir = 0; }
    }
    switch (gstate) {
    case GS_TITLE:
        if (in.pressed & (BT_LEFT | BT_RIGHT)) {
            g_lang = (g_lang + ((in.pressed & BT_RIGHT) ? 1 : LANG_COUNT - 1)) % LANG_COUNT;
            lang_save();
            sfx(SFX_MENU_MOVE);
        }
        if (state_t > 30 && (in.pressed & (BT_START | BT_A))) {
            gstate = GS_FILESELECT;
            state_t = 0;
            menu_sel = 0;
            erase_mode = 0;
            sfx(SFX_MENU_OK);
            sfx_ex(SFX_SQUAWK, 0.8f, 1.2f);
            music_play(MUS_SELECT);
        }
        break;
    case GS_FILESELECT:
        if (in.pressed & BT_UP) { menu_sel = (menu_sel + 3) % 4; sfx(SFX_MENU_MOVE); }
        if (in.pressed & BT_DOWN) { menu_sel = (menu_sel + 1) % 4; sfx(SFX_MENU_MOVE); }
        if (in.pressed & BT_B) {
            if (erase_mode) erase_mode = 0;
            else { gstate = GS_TITLE; state_t = 0; music_play(MUS_TITLE); }
            sfx(SFX_MENU_MOVE);
        }
        if (in.pressed & (BT_A | BT_START)) {
            if (menu_sel == 3) { erase_mode = !erase_mode; sfx(SFX_MENU_OK); }
            else if (erase_mode) {
                if (save[menu_sel].exists) {
                    memset(&save[menu_sel], 0, sizeof save[menu_sel]);
                    save_write();
                    sfx(SFX_BREAK);
                }
                erase_mode = 0;
            } else {
                sfx(SFX_MENU_OK);
                start_new_file(menu_sel);
            }
        }
        break;
    case GS_STARSELECT: {
        const CourseInfo *c = &courses[star_select_course];
        if (c->nstars > 0) {
            if (in.pressed & (BT_LEFT | BT_UP)) { menu_sel = (menu_sel + c->nstars - 1) % c->nstars; sfx(SFX_MENU_MOVE); }
            if (in.pressed & (BT_RIGHT | BT_DOWN)) { menu_sel = (menu_sel + 1) % c->nstars; sfx(SFX_MENU_MOVE); }
        }
        if (state_t > 20 && (in.pressed & (BT_A | BT_START))) {
            sfx(SFX_MENU_OK);
            sfx_ex(SFX_SQUAWK, 0.8f, 1.3f);
            gstate = GS_PLAY;
            level_load(c->level, 0);
            pl.no_ctrl = 0;
        }
        if (in.pressed & BT_B) {
            gstate = GS_PLAY;
            level_load(LV_LOBBY, g_entry_painting);
            pl.no_ctrl = 0;
        }
        break;
    }
    case GS_PLAY:
        play_update();
        break;
    case GS_GAMEOVER:
        gameover_t++;
        if (gameover_t > 120 && (in.pressed & (BT_A | BT_START))) {
            gstate = GS_TITLE;
            state_t = 0;
            music_play(MUS_TITLE);
        }
        break;
    case GS_ENDING:
        ending_t++;
        if (ending_t > 600 && (in.pressed & (BT_A | BT_START))) {
            gstate = GS_TITLE;
            state_t = 0;
            music_play(MUS_TITLE);
        }
        break;
    }
}

/* ======================================================================
 * rendering
 * ====================================================================== */
static void draw_sky(void)
{
    int i;
    float top[3], bot[3];
    memcpy(top, lv.sky_top, sizeof top);
    memcpy(bot, lv.sky_bot, sizeof bot);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glPushMatrix();
    glTranslatef(cam.pos.x, cam.pos.y, cam.pos.z);
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= 24; i++) {
        float a = TAU_F * i / 24;
        glColor3fv(bot);
        glVertex3f(sinf(a) * 50, -20, cosf(a) * 50);
        glColor3fv(top);
        glVertex3f(sinf(a) * 50, 40, cosf(a) * 50);
    }
    glEnd();
    glBegin(GL_TRIANGLE_FAN);
    glColor3fv(top);
    glVertex3f(0, 50, 0);
    for (i = 0; i <= 24; i++) {
        float a = TAU_F * i / 24;
        glVertex3f(sinf(a) * 50, 40, cosf(a) * 50);
    }
    glEnd();
    glBegin(GL_TRIANGLE_FAN);
    glColor3fv(bot);
    glVertex3f(0, -50, 0);
    for (i = 0; i <= 24; i++) {
        float a = -TAU_F * i / 24;
        glVertex3f(sinf(a) * 50, -20, cosf(a) * 50);
    }
    glEnd();
    if (lv.space) space_draw_sky();
    if (lv.stars_sky) {
        glPointSize(2);
        glBegin(GL_POINTS);
        for (i = 0; i < 300; i++) {
            unsigned h = (unsigned)i * 2654435761u;
            float a = (h & 0xFFFF) / 65536.0f * TAU_F, b = ((h >> 16) & 0x7FFF) / 32768.0f * 1.4f + 0.05f;
            float tw = 0.5f + 0.5f * sinf(g_frame * 0.05f + i);
            glColor3f(tw, tw, tw * 0.9f + 0.1f);
            glVertex3f(sinf(a) * cosf(b) * 45, sinf(b) * 45, cosf(a) * cosf(b) * 45);
        }
        glEnd();
    }
    if (lv.aurora) {
        /* northern lights: waving curtains, bright at the bottom edge and fading upward */
        int band;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        for (band = 0; band < 3; band++) {
            float t = g_frame / 60.0f, base = 4 + band * 3, hgt = 13 + band * 3;
            float a0 = band * 2.1f, span = 2.4f;
            glBegin(GL_QUAD_STRIP);
            for (i = 0; i <= 40; i++) {
                float u = (float)i / 40, a = a0 + span * u;
                float r = 44 - band * 2 + 3 * sinf(u * 9 + t * 0.4f + band);
                float y = base + 3 * sinf(u * 6 + t * 0.3f + band * 1.7f);
                float k = sinf(u * PI_F) * (0.55f + 0.45f * sinf(u * 17 + t * 1.3f + band));
                float g = band == 1 ? 0.6f : 1.0f;
                glColor4f(0.15f + 0.3f * (band == 2), g, 0.55f + 0.35f * (band == 1), 0.8f * k);
                glVertex3f(sinf(a) * r, y, cosf(a) * r);
                glColor4f(0.4f, 0.3f, 0.9f, 0);
                glVertex3f(sinf(a) * (r + 4), y + hgt, cosf(a) * (r + 4));
            }
            glEnd();
        }
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    if (lv.clouds) {
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBindTexture(GL_TEXTURE_2D, g_tex[TEX_CLOUD]);
        for (i = 0; i < 14; i++) {
            float a = TAU_F * i / 14 + g_frame * 0.00005f, h = 8 + (i % 4) * 4, s = 6 + (i % 3) * 3;
            vec3 c = v3(sinf(a) * 45, h, cosf(a) * 45);
            vec3 r = v3(cosf(a), 0, -sinf(a)), u = v3(0, 1, 0);
            glColor4f(1, 1, 1, 0.85f);
            glBegin(GL_QUADS);
            glTexCoord2f(0, 1); glVertex3f(c.x - r.x * s, c.y - s * 0.4f, c.z - r.z * s);
            glTexCoord2f(1, 1); glVertex3f(c.x + r.x * s, c.y - s * 0.4f, c.z + r.z * s);
            glTexCoord2f(1, 0); glVertex3f(c.x + r.x * s, c.y + s * 0.4f, c.z + r.z * s);
            glTexCoord2f(0, 0); glVertex3f(c.x - r.x * s, c.y + s * 0.4f, c.z - r.z * s);
            glEnd();
            (void)u;
        }
        glDisable(GL_TEXTURE_2D);
    }
    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

static void draw_snow(void)
{
    int i;
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    if (lv.snow == 3) {
        /* cyberspace: falling streams of data */
        glLineWidth(2);
        glBegin(GL_LINES);
        for (i = 0; i < 260; i++) {
            unsigned h = (unsigned)i * 2654435761u;
            float x = ((h & 0x3FF) / 1024.0f) * 44 - 22, z = (((h >> 10) & 0x3FF) / 1024.0f) * 44 - 22;
            float sp = 0.06f + ((h >> 26) & 7) * 0.012f;
            float y = 14 - fmodf(g_frame * sp + ((h >> 20) & 0xFF) / 12.0f, 18.0f);
            float wx = cam.pos.x + x, wz = cam.pos.z + z, len = 0.5f + ((h >> 23) & 3) * 0.3f;
            if (wx < -29.5f || wx > 29.5f || wz < -39.5f || wz > 9.5f) continue;   /* only inside the hall */
            glColor4f(0.2f, 1.0f, 0.8f, 0.0f);
            glVertex3f(wx, y + len, wz);
            glColor4f(0.4f, 1.0f, 0.9f, 0.8f);
            glVertex3f(wx, y, wz);
        }
        glEnd();
        glLineWidth(1);
        glEnable(GL_LIGHTING);
        return;
    }
    if (lv.snow == 2) {
        /* blizzard: dense flakes streaking along the wind */
        vec3 wd = yaw_dir(lv.wind_yaw + sinf(g_frame / 60.0f * 0.23f) * 0.4f);
        float sp = 3 + 15 * g_gust;
        glLineWidth(2);
        glColor4f(1, 1, 1, 0.75f);
        glBegin(GL_LINES);
        for (i = 0; i < 1200; i++) {
            unsigned h = (unsigned)i * 2654435761u;
            float ox = ((h & 0x3FF) / 1024.0f) * 36 - 18, oz = (((h >> 10) & 0x3FF) / 1024.0f) * 36 - 18;
            float ph = ((h >> 20) & 0xFF) / 255.0f;
            float y = 16 - fmodf(g_frame * 0.07f + ph * 20, 22.0f);
            float d = fmodf(g_frame / 60.0f * sp + ph * 36, 36.0f) - 18;
            float x = ox + wd.x * d, z = oz + wd.z * d;
            float len = 0.25f + 0.5f * g_gust;
            vec3 p = v3(cam.pos.x + x, cam.pos.y + y - 4, cam.pos.z + z);
            glVertex3f(p.x, p.y, p.z);
            glVertex3f(p.x - wd.x * len, p.y + 0.08f, p.z - wd.z * len);
        }
        glEnd();
        glLineWidth(1);
        glEnable(GL_LIGHTING);
        return;
    }
    glPointSize(3);
    glColor4f(1, 1, 1, 0.9f);
    glBegin(GL_POINTS);
    for (i = 0; i < 400; i++) {
        unsigned h = (unsigned)i * 2654435761u;
        float x = ((h & 0x3FF) / 1024.0f) * 40 - 20, z = (((h >> 10) & 0x3FF) / 1024.0f) * 40 - 20;
        float y = 20 - fmodf(g_frame * (0.03f + (h >> 28) * 0.004f) + ((h >> 20) & 0xFF) / 10.0f, 25.0f);
        x += sinf(g_frame * 0.02f + i) * 0.5f;
        glVertex3f(cam.pos.x + x, cam.pos.y + y - 5, cam.pos.z + z);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

static float view_mv[16], view_pr[16];

static void render_world(void)
{
    float fog[3];
    float wy;
    int underwater;
    gfx_begin_3d(cam.fov > 1 ? cam.fov : 55.0f, 0.3f, lv.zfar > 0 ? lv.zfar : (lv.space ? 1000.0f : 400.0f));
    wy = water_level_at(cam.pos.x, cam.pos.z, cam.pos.y);
    underwater = wy > -1e8f && cam.pos.y < wy;
    memcpy(fog, lv.fog, sizeof fog);
    glClearColor(fog[0], fog[1], fog[2], 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gfx_lookat(cam.pos, cam.at);
    glGetFloatv(GL_MODELVIEW_MATRIX, view_mv);
    glGetFloatv(GL_PROJECTION_MATRIX, view_pr);
    if (!underwater) draw_sky();
    gfx_light(lv.ambient, lv.diffuse);
    if (underwater) gfx_set_fog(0.1f, 0.3f, 0.55f, 2, 45);
    else gfx_set_fog(fog[0], fog[1], fog[2], lv.fog_start, lv.fog_end);
    if (underwater) { glClearColor(0.1f, 0.3f, 0.55f, 1); }
    glEnable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glCallList(lv.dl);
    objs_draw();
    glEnable(GL_LIGHTING);
    player_draw();
    /* held object drawn by objs_draw; transparent stuff last */
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    if (lv.dl_alpha) {
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glTranslatef(g_frame * 0.0015f, g_frame * 0.001f, 0);
        glMatrixMode(GL_MODELVIEW);
        glCallList(lv.dl_alpha);
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
    }
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    objs_draw_alpha();
    sky_draw_alpha();
    space_draw_alpha();
    scale_draw_alpha();
    vert_draw_alpha();
    if (lv.snow) draw_snow();
    glDisable(GL_FOG);
    if (underwater) {
        gfx_begin_2d();
        gfx_rect(0, 0, g_vw, 480, 0.1f, 0.3f, 0.7f, 0.25f);
    }
}

/* floating labels (titles of the app windows in the castle), projected onto the screen */
static void draw_labels(void)
{
    const float *mv = view_mv, *pr = view_pr;
    int i;
    if (!lv.nlabels) return;
    gfx_begin_2d();
    for (i = 0; i < lv.nlabels; i++) {
        vec3 p = lv.label[i].p, n;
        float e[4], c[4], d = v3dist(p, cam.pos), t, sc, sx, sy;
        int k;
        char buf[128];
        if (d > 34) continue;
        if (col_ray(cam.pos, p, &t, &n, 1) && t < 0.97f) continue;   /* hidden behind a wall */
        for (k = 0; k < 4; k++) e[k] = mv[k] * p.x + mv[4 + k] * p.y + mv[8 + k] * p.z + mv[12 + k];
        for (k = 0; k < 4; k++) c[k] = pr[k] * e[0] + pr[4 + k] * e[1] + pr[8 + k] * e[2] + pr[12 + k] * e[3];
        if (c[3] <= 0.1f) continue;
        sx = (c[0] / c[3] * 0.5f + 0.5f) * g_vw;
        sy = (1 - (c[1] / c[3] * 0.5f + 0.5f)) * 480;
        sc = clampf(26.0f / d, 0.9f, 2.4f);
        if (lv.label[i].course >= 0) {
            const CourseInfo *ci = &courses[lv.label[i].course];
            if (ci->nstars > 0)
                snprintf(buf, sizeof buf, "%s  [%d/%d]", tr(lv.label[i].text), __builtin_popcount(save[cur_slot].stars[lv.label[i].course]), ci->nstars);
            else snprintf(buf, sizeof buf, "%s", tr(lv.label[i].text));
        } else snprintf(buf, sizeof buf, "%s", tr(lv.label[i].text));
        sy = clampf(sy - 4 * sc, 76, 440);
        {
            float a = clampf((34 - d) / 8, 0, 1), tw = gfx_text_width(buf, sc), lh = gfx_line_height(buf, sc);
            gfx_rect(sx - tw * 0.5f - 8, sy - 4, tw + 16, lh + 6, 0.0f, 0.05f, 0.12f, 0.7f * a);
            gfx_rect(sx - tw * 0.5f - 8, sy + lh + 1, tw + 16, 2, 0.2f, 0.9f, 1.0f, a);
            gfx_text_center(sx, sy, sc, buf, 0.75f, 1, 1, a);
        }
    }
}

static void draw_hud(void)
{
    char buf[160];
    gfx_begin_2d();
    /* lives */
    gfx_circle2d(34, 32, 15, 0.1f, 0.1f, 0.13f, 1);
    gfx_circle2d(34, 36, 8, 1, 1, 1, 1);
    gfx_circle2d(29, 28, 3, 1, 1, 1, 1);
    gfx_circle2d(39, 28, 3, 1, 1, 1, 1);
    gfx_rect(30, 33, 8, 3, 1, 0.65f, 0.1f, 1);
    snprintf(buf, sizeof buf, "x%d", pl.lives < 0 ? 0 : pl.lives);
    gfx_text_shadow(54, 22, 3, buf, 1, 1, 1, 1);
    /* coins */
    gfx_fish2d(g_vw * 0.5f - 160, 34, 30, 1, 0.82f, 0.15f, 1);
    snprintf(buf, sizeof buf, "x%d", pl.coins);
    gfx_text_shadow(g_vw * 0.5f - 138, 22, 3, buf, 1, 1, 1, 1);
    /* stars */
    gfx_star2d(g_vw - 130, 33, 16, 1, 0.85f, 0.1f, 1);
    snprintf(buf, sizeof buf, "x%d", total_stars());
    gfx_text_shadow(g_vw - 110, 22, 3, buf, 1, 1, 1, 1);
    /* power meter */
    if (powermeter_show) {
        float r = 0.2f, g = 0.5f, b = 1.0f;
        if (pl.health <= 2) { r = 1.0f; g = 0.15f; b = 0.1f; }
        else if (pl.health <= 4) { r = 1.0f; g = 0.85f; b = 0.1f; }
        else if (pl.health <= 6) { r = 0.2f; g = 0.85f; b = 0.3f; }
        /* battery gauge with 8 cells */
        {
            float bx = g_vw * 0.5f - 10, by = 28;
            int k;
            gfx_rect(bx - 3, by - 3, 96, 38, 0.05f, 0.05f, 0.08f, 0.85f);
            gfx_rect(bx + 93, by + 8, 8, 16, 0.05f, 0.05f, 0.08f, 0.85f);
            gfx_rect(bx - 1, by - 1, 92, 34, 0.85f, 0.88f, 0.95f, 1);
            gfx_rect(bx + 93, by + 10, 6, 12, 0.85f, 0.88f, 0.95f, 1);
            gfx_rect(bx + 2, by + 2, 86, 28, 0.08f, 0.09f, 0.12f, 1);
            for (k = 0; k < 8; k++) {
                if (k < pl.health) gfx_rect(bx + 4 + k * 10.6f, by + 4, 9, 24, r, g, b, 1);
                else gfx_rect(bx + 4 + k * 10.6f, by + 4, 9, 24, 0.18f, 0.19f, 0.24f, 1);
            }
        }
        if (pl.act == ACT_SWIM && pl.pos.y + 1.4f < pl.water_y)
            gfx_text_center(g_vw * 0.5f + 40, 82, 1.5f, tr("AIR"), 0.7f, 0.9f, 1, 1);
    }
    /* rocket fuel */
    if (pl.rocket_t > 0 && cur_level != LV_SKY && !lv.space) {
        float x = g_vw - 120, y = 440;
        gfx_rect(x - 6, y - 14, 12, 24, 0.9f, 0.15f, 0.12f, 1);
        gfx_rect(x - 3, y - 20, 6, 6, 1, 1, 1, 1);
        gfx_rect(x - 11, y + 2, 5, 9, 0.95f, 0.8f, 0.2f, 1);
        gfx_rect(x + 6, y + 2, 5, 9, 0.95f, 0.8f, 0.2f, 1);
        if (player_rocket_firing()) gfx_rect(x - 4, y + 10, 8, 6 + (g_frame % 3) * 3, 1, 0.6f, 0.1f, 1);
        snprintf(buf, sizeof buf, "%d", (pl.rocket_t + 59) / 60);
        gfx_text_shadow(x + 28, y - 10, 2.5f, buf, 1, 1, 1, 1);
    }
    /* red coins */
    if (pl.red_coins > 0) {
        int i;
        for (i = 0; i < pl.red_coins; i++) gfx_fish2d(30 + i * 26, 452, 20, 0.95f, 0.15f, 0.12f, 1);
    }
    /* timer */
    if (g_timer_on || (cur_level == LV_SLIDE && g_course_timer > 0)) {
        int t = g_course_timer;
        snprintf(buf, sizeof buf, "TIME %d'%02d\"%d", t / 3600, (t / 60) % 60, (t % 60) / 6);
        gfx_text_shadow(g_vw - 230, 66, 2.5f, buf, 1, 1, 1, 1);
    }
    /* FROSTCLOUD SKYWAY: time left, fish lines and the dash gauge */
    if (cur_level == LV_SKY) {
        int t = g_sky_time, k, next = -1;
        float x = g_vw * 0.5f;
        snprintf(buf, sizeof buf, "%d:%02d", t / 3600, (t / 60) % 60);
        gfx_text_center(x, 72, 4, buf, t < 600 ? 1.0f : 1.0f, t < 600 ? 0.3f : 1.0f, t < 600 ? 0.3f : 1.0f, 1);
        for (k = 0; k < SKY_LINES; k++) {
            float sx = 30 + k * 80;
            int own = has_star(lv.course, k), hit = pl.coins >= g_sky_lines[k];
            if (!hit && next < 0) next = k;
            gfx_star2d(sx + 8, 412, 11, own ? 1.0f : 0.35f, own ? 0.85f : 0.35f, own ? 0.1f : 0.45f, 1);
            snprintf(buf, sizeof buf, "%d", g_sky_lines[k]);
            gfx_text_shadow(sx + 22, 403, 2, buf, hit ? 0.4f : 0.85f, hit ? 1.0f : 0.85f, hit ? 0.4f : 0.85f, 1);
        }
        if (next >= 0) {
            snprintf(buf, sizeof buf, tr("NEXT LINE: %d more fish"), g_sky_lines[next] - pl.coins);
            gfx_text_shadow(30, 375, 2, buf, 1, 1, 1, 1);
        }
        gfx_rect(30, 440, 204, 16, 0.05f, 0.05f, 0.1f, 0.8f);
        gfx_rect(32, 442, 200 * pl.sky_dash, 12, 0.4f, 0.85f + 0.15f * sinf(g_frame * 0.2f), 1.0f, 1);
        gfx_text_shadow(240, 438, 2, tr("DASH"), 0.7f, 0.95f, 1, 1);
    }
    vert_hud();
    /* STARDUST ORBIT: speed, ring run, meteors and a marker on the next ring */
    if (lv.space && pl.act == ACT_SPACEFLY) {
        float x = 24, y = 420, k = clampf(pl.sp_speed / 78.0f, 0, 1);
        snprintf(buf, sizeof buf, "%d km/h", (int)(pl.sp_speed * 36));
        gfx_rect(x, y + 26, 204, 12, 0.05f, 0.05f, 0.1f, 0.8f);
        gfx_rect(x + 2, y + 28, 200 * k, 8, 0.3f + 0.7f * k, 0.9f - 0.4f * k * k, 1.0f - 0.8f * k, 1);
        gfx_text_shadow(x, y, 2, buf, pl.boost_t > 0 ? 1.0f : 0.8f, pl.boost_t > 0 ? 0.7f : 0.95f, pl.boost_t > 0 ? 0.3f : 1, 1);
        if (g_meteors_left < SPACE_METEORS && g_meteors_left > 0) {
            snprintf(buf, sizeof buf, tr("ICE METEORS LEFT: %d"), g_meteors_left);
            gfx_text_shadow(x, y - 30, 2, buf, 0.6f, 0.85f, 1, 1);
        }
        if (g_ring_run_t > 0) {
            int t = g_ring_run_t;
            snprintf(buf, sizeof buf, tr("RING %d/%d"), g_ring_run_next, SPACE_RINGS);
            gfx_text_shadow(g_vw - 230, 100, 2.5f, buf, 1, 0.9f, 0.4f, 1);
            snprintf(buf, sizeof buf, "%d\"%d", t / 60, (t % 60) / 6);
            gfx_text_center(g_vw * 0.5f, 72, 4, buf, 1, t < 600 ? 0.35f : 1, t < 600 ? 0.3f : 1, 1);
        }
        {
            Obj *r = space_ring(g_ring_run_t > 0 ? g_ring_run_next : 0);
            if (r && (g_ring_run_t > 0 || v3dist(r->pos, pl.pos) < 140)) {
                const float *mv = view_mv, *pr = view_pr;
                vec3 p = r->pos;
                float e[4], c[4], sx, sy, cx = g_vw * 0.5f, cy = 240;
                int i2, behind;
                for (i2 = 0; i2 < 4; i2++) e[i2] = mv[i2] * p.x + mv[4 + i2] * p.y + mv[8 + i2] * p.z + mv[12 + i2];
                for (i2 = 0; i2 < 4; i2++) c[i2] = pr[i2] * e[0] + pr[4 + i2] * e[1] + pr[8 + i2] * e[2] + pr[12 + i2] * e[3];
                behind = c[3] <= 0.1f;
                if (behind) { c[0] = -c[0]; c[1] = -c[1]; c[3] = maxf(fabsf(c[3]), 0.1f); }
                sx = (c[0] / c[3] * 0.5f + 0.5f) * g_vw;
                sy = (1 - (c[1] / c[3] * 0.5f + 0.5f)) * 480;
                if (behind || sx < 30 || sx > g_vw - 30 || sy < 40 || sy > 450) {
                    /* off screen: an arrow on the edge pointing at it */
                    float dx = sx - cx, dy = sy - cy, l = sqrtf(dx * dx + dy * dy), f;
                    if (l < 1) { dx = 0; dy = 1; l = 1; }
                    dx /= l; dy /= l;
                    f = minf((cx - 40) / maxf(fabsf(dx), 1e-3f), (cy - 50) / maxf(fabsf(dy), 1e-3f));
                    sx = cx + dx * f; sy = cy + dy * f;
                    gfx_circle2d(sx, sy, 11, 0, 0, 0, 0.5f);
                    gfx_circle2d(sx, sy, 8, g_ring_run_t > 0 ? 1 : 0.3f, g_ring_run_t > 0 ? 0.85f : 1, 0.3f, 0.9f);
                    gfx_circle2d(sx + dx * 12, sy + dy * 12, 4, 1, 1, 1, 0.9f);
                } else {
                    float pulse = 1 + 0.15f * sinf(g_frame * 0.3f);
                    gfx_circle2d(sx, sy - 30 * pulse, 6, g_ring_run_t > 0 ? 1 : 0.3f, g_ring_run_t > 0 ? 0.85f : 1, 0.3f, 0.9f);
                }
            }
        }
    }
    /* AURORA SEA rally: lap, gate power and speed */
    if (cur_level == LV_SEA && pl.boots) {
        float x = 24, y = 400;
        int k;
        if (g_race_state == 1) {
            snprintf(buf, sizeof buf, tr("LAP %d/%d"), g_lap + 1 > SEA_LAPS ? SEA_LAPS : g_lap + 1, SEA_LAPS);
            gfx_text_shadow(g_vw - 230, 100, 2.5f, buf, 1, 1, 1, 1);
        }
        gfx_text_shadow(x, y - 26, 2, tr("POWER"), 0.7f, 0.95f, 1, 1);
        for (k = 0; k < 5; k++) {
            int on = k < g_sea_power;
            gfx_rect(x + k * 22, y, 18, 14, 0.05f, 0.05f, 0.1f, 0.8f);
            if (on) gfx_rect(x + k * 22 + 2, y + 2, 14, 10, 0.3f + k * 0.14f, 1.0f - k * 0.1f, 0.4f, 1);
        }
        snprintf(buf, sizeof buf, "%d km/h", (int)(pl.act == ACT_SKATE ? pl.fwd * 3.6f : 0));
        gfx_text_shadow(x, y + 22, 2, buf, pl.boost_t > 0 ? 0.4f : 1, pl.boost_t > 0 ? 1 : 1, 1, 1);
    }
    /* messages */
    if (g_hud_msg_t > 0 && g_hud_msg) {
        float a = clampf(g_hud_msg_t / 20.0f, 0, 1);
        float sc = 3, w = gfx_text_width(g_hud_msg, 3);
        if (w > g_vw - 40) sc = 3 * (g_vw - 40) / w;   /* long messages shrink to fit */
        gfx_text_center(g_vw * 0.5f, 150 + (3 - sc) * 4, sc, g_hud_msg, 1, 0.95f, 0.5f, a);
    }
    if (course_intro_t > 0 && lv.course >= 0 && lv.name && !paused) {
        float a = clampf(course_intro_t / 30.0f, 0, 1);
        gfx_text_center(g_vw * 0.5f, 190, 4.5f, tr(lv.name), 1, 0.85f, 0.25f, a);
        if (lv.course < CRS_COUNT && courses[lv.course].nstars > 0) {
            snprintf(buf, sizeof buf, tr("Stars: %d / %d"), __builtin_popcount(save[cur_slot].stars[lv.course]), courses[lv.course].nstars);
            gfx_text_center(g_vw * 0.5f, 240, 2.5f, buf, 1, 1, 1, a);
        }
    }
}

static void draw_pause(void)
{
    static const char *opts_c[3] = { "CONTINUE", "EXIT COURSE", "SAVE & QUIT" };
    static const char *opts_h[2] = { "CONTINUE", "SAVE & QUIT" };
    int n = lv.course >= 0 ? 3 : 2, i;
    gfx_begin_2d();
    gfx_rect(0, 0, g_vw, 480, 0, 0, 0, 0.5f);
    gfx_text_center(g_vw * 0.5f, 100, 5, tr("PAUSE"), 1, 0.9f, 0.3f, 1);
    if (lv.course >= 0 && lv.course < CRS_COUNT) {
        const CourseInfo *c = &courses[lv.course];
        char buf[80];
        gfx_text_center(g_vw * 0.5f, 160, 2.5f, tr(c->name), 1, 1, 1, 1);
        for (i = 0; i < c->nstars; i++) {
            float x = g_vw * 0.5f - (c->nstars - 1) * 20 + i * 40;
            if (has_star(lv.course, i)) gfx_star2d(x, 205, 14, 1, 0.85f, 0.1f, 1);
            else gfx_star2d(x, 205, 14, 0.3f, 0.3f, 0.4f, 1);
        }
        snprintf(buf, sizeof buf, tr("Best fish: %d"), save[cur_slot].coin_hi[lv.course]);
        gfx_text_center(g_vw * 0.5f, 232, 2, buf, 0.9f, 0.9f, 0.9f, 1);
    }
    for (i = 0; i < n; i++) {
        const char *s = lv.course >= 0 ? opts_c[i] : opts_h[i];
        float y = 280 + i * 40;
        s = tr(s);
        if (i == pause_sel) gfx_text_center(g_vw * 0.5f, y, 3, s, 1, 0.85f, 0.2f, 1);
        else gfx_text_center(g_vw * 0.5f, y, 3, s, 0.8f, 0.8f, 0.8f, 1);
    }
}

static void draw_controls_help(float y)
{
    static const char *lines[3] = {
        "MOVE: WASD / Arrows    JUMP (A): Space / K    ACTION (B): J / X / Shift",
        "CROUCH / GROUND POUND (Z): Z / L / Ctrl    PAUSE: Enter / Esc",
        "CAMERA: Q / E rotate, R / F zoom, Tab = behind   F11: fullscreen   M: music",
    };
    int i;
    float sc = 1.6f;
    for (i = 0; i < 3; i++) {
        float w = gfx_text_width(tr(lines[i]), 1.6f);
        if (w > g_vw - 20) sc = minf(sc, 1.6f * (g_vw - 20) / w);
    }
    for (i = 0; i < 3; i++) gfx_text_center(g_vw * 0.5f, y + i * 18, sc, tr(lines[i]), 0.85f, 0.85f, 0.9f, 1);
}

/* 2D snowfall overlay for menus */
static void snow2d(int n, float speed, float alpha)
{
    int i;
    for (i = 0; i < n; i++) {
        unsigned h = (unsigned)i * 2654435761u;
        float x = (h & 0x3FF) / 1024.0f * g_vw, sp = speed * (0.5f + ((h >> 10) & 0xFF) / 255.0f);
        float y = fmodf(((h >> 18) & 0x1FF) + state_t * sp, 500.0f) - 10;
        x += sinf(state_t * 0.03f + i) * 8;
        gfx_circle2d(x, y, 1.2f + (h >> 29) * 0.5f, 1, 1, 1, alpha);
    }
}

static void draw_title(void)
{
    TuxPose tp;
    float t = state_t / 60.0f;
    gfx_begin_3d(45, 0.3f, 100);
    glClearColor(0.12f, 0.22f, 0.45f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gfx_lookat(v3(0, 1.3f, 5.2f), v3(0, 1.0f, 0));
    /* background stars */
    {
        int i;
        glDisable(GL_LIGHTING);
        glPointSize(2);
        glBegin(GL_POINTS);
        for (i = 0; i < 200; i++) {
            unsigned h = (unsigned)i * 2654435761u;
            float x = ((h & 0x3FF) / 1024.0f - 0.5f) * 30, y = (((h >> 10) & 0x3FF) / 1024.0f - 0.3f) * 18;
            float z = -10 - ((h >> 20) & 0xFF) / 25.0f;
            float tw = 0.5f + 0.5f * sinf(t * 3 + i);
            glColor3f(tw, tw, 1);
            glVertex3f(x + sinf(t * 0.2f) * 0.5f, y, z);
        }
        glEnd();
    }
    gfx_light(0.45f, 0.75f);
    glEnable(GL_LIGHTING);
    memset(&tp, 0, sizeof tp);
    tp.flipL_z = 2.6f + sinf(t * 6) * 0.3f;
    tp.flipR_z = 0.3f;
    tp.head_yaw = sinf(t * 0.8f) * 0.3f;
    tp.body_roll = sinf(t * 2) * 0.05f;
    tp.lift = fabsf(sinf(t * 2.5f)) * 0.15f;
    draw_tux_model(v3(0, -0.2f, 0), sinf(t * 0.6f) * 0.6f, &tp, 1.2f, TUXV_TUX);
    /* snow drift Tux stands on, and ice crystals */
    glPushMatrix();
    glTranslatef(0, -0.45f, 0);
    glScalef(2.4f, 0.3f, 1.4f);
    glCallList(mdl[MDL_MOUND]);
    glPopMatrix();
    glPushMatrix(); glTranslatef(-2.1f, -0.5f, -0.8f); glScalef(0.45f, 0.45f, 0.45f); glCallList(mdl[MDL_CRYSTAL]); glPopMatrix();
    glPushMatrix(); glTranslatef(2.5f, -0.5f, -2.2f); glScalef(0.3f, 0.3f, 0.3f); glCallList(mdl[MDL_ICESPIRE]); glPopMatrix();
    /* spinning star */
    glPushMatrix();
    glTranslatef(1.9f, 2.0f + sinf(t * 2) * 0.1f, -1);
    glRotatef(t * 90, 0, 1, 0);
    glScalef(0.5f, 0.5f, 0.5f);
    glCallList(mdl[MDL_STAR]);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(-1.9f, 0.6f + sinf(t * 2 + 1) * 0.1f, -1);
    glRotatef(t * 120, 0, 1, 0);
    glCallList(mdl[MDL_COIN]);
    glPopMatrix();
    gfx_begin_2d();
    snow2d(120, 0.8f, 0.85f);
    {
        float s = 9 + sinf(t * 2) * 0.3f;
        gfx_text_center(g_vw * 0.5f + 4, 44, s, "TUX 64", 0.05f, 0.15f, 0.35f, 1);
        gfx_text_center(g_vw * 0.5f, 40, s, "TUX 64", 0.75f, 0.92f, 1.0f, 1);
    }
    gfx_text_center(g_vw * 0.5f, 120, 2, tr("THE KERNEL CASTLE ADVENTURE"), 0.8f, 0.9f, 1, 1);
    {
        char lbuf[96];
        float pulse = 0.75f + 0.25f * sinf(state_t * 0.1f);
        snprintf(lbuf, sizeof lbuf, "LANGUAGE / \xE3\x81\x92\xE3\x82\x93\xE3\x81\x94 :  <  %s  >", lang_name(g_lang));
        gfx_rect(g_vw * 0.5f - 175, 318, 350, 26, 0.05f, 0.1f, 0.25f, 0.6f);
        gfx_text_center(g_vw * 0.5f, 324, 2.0f, lbuf, pulse, pulse, 1, 1);
    }
    if ((state_t / 30) % 2) gfx_text_center(g_vw * 0.5f, 360, 3, tr("PRESS START (ENTER) / A"), 1, 1, 1, 1);
    draw_controls_help(404);
    gfx_text(8, 466, 1.2f, tr("A fan-made penguin platformer. Tux (c) Larry Ewing, made with The GIMP."), 0.6f, 0.6f, 0.7f, 1);
}

static void draw_fileselect(void)
{
    int i;
    char buf[160];
    gfx_begin_2d();
    glClearColor(0.05f, 0.08f, 0.2f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    snow2d(90, 0.6f, 0.5f);
    (void)i;
    gfx_text_center(g_vw * 0.5f, 40, 4.5f, tr(erase_mode ? "ERASE WHICH FILE?" : "SELECT FILE"), 1, 0.85f, 0.2f, 1);
    for (i = 0; i < 3; i++) {
        float x = g_vw * 0.5f - 200, y = 120 + i * 80;
        int sel = menu_sel == i;
        gfx_rect(x, y, 400, 64, sel ? 0.3f : 0.12f, sel ? 0.3f : 0.12f, sel ? 0.55f : 0.25f, 0.9f);
        if (sel) gfx_rect(x, y, 6, 64, 1, 0.8f, 0.2f, 1);
        snprintf(buf, sizeof buf, tr("FILE %c"), 'A' + i);
        gfx_text_shadow(x + 20, y + 12, 3, buf, 1, 1, 1, 1);
        if (save[i].exists) {
            gfx_star2d(x + 250, y + 24, 14, 1, 0.85f, 0.1f, 1);
            snprintf(buf, sizeof buf, "x %d", slot_stars(i));
            gfx_text_shadow(x + 270, y + 12, 3, buf, 1, 1, 1, 1);
            if (save[i].flags & FL_BEAT_GAME) gfx_text(x + 20, y + 44, 1.5f, tr("* KERNEL RESTORED *"), 1, 0.85f, 0.3f, 1);
        } else gfx_text_shadow(x + 250, y + 16, 2.5f, tr("NEW"), 0.7f, 0.9f, 0.7f, 1);
    }
    {
        int sel = menu_sel == 3;
        gfx_text_center(g_vw * 0.5f, 380, 3, tr(erase_mode ? "CANCEL ERASE" : "ERASE FILE"), sel ? 1 : 0.7f, sel ? 0.85f : 0.7f, sel ? 0.2f : 0.7f, 1);
    }
    gfx_text_center(g_vw * 0.5f, 440, 1.8f, tr("A / Enter: select     B / J: back"), 0.7f, 0.7f, 0.8f, 1);
}

static void draw_starselect(void)
{
    const CourseInfo *c = &courses[star_select_course];
    int i;
    char buf[200];
    gfx_begin_2d();
    glClearColor(0.05f, 0.08f, 0.2f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    snow2d(80, 0.6f, 0.45f);
    gfx_text_center(g_vw * 0.5f, 50, 2.5f, tr("COURSE"), 0.8f, 0.8f, 1, 1);
    gfx_text_center(g_vw * 0.5f, 80, 4.5f, tr(c->name), 1, 0.85f, 0.25f, 1);
    if (c->nstars == 0) {
        gfx_text_center(g_vw * 0.5f, 200, 3, tr("Defeat Segfault!"), 1, 0.4f, 0.4f, 1);
    }
    for (i = 0; i < c->nstars; i++) {
        float x = g_vw * 0.5f - (c->nstars - 1) * 45 + i * 90;
        float bob = (i == menu_sel) ? sinf(state_t * 0.15f) * 5 : 0;
        if (has_star(star_select_course, i)) gfx_star2d(x, 190 + bob, 30, 1, 0.85f, 0.1f, 1);
        else gfx_star2d(x, 190 + bob, 30, 0.3f, 0.35f, 0.5f, 1);
        snprintf(buf, sizeof buf, "%d", i + 1);
        gfx_text_center(x, 230, 2, buf, 1, 1, 1, 1);
    }
    if (c->nstars > 0) {
        snprintf(buf, sizeof buf, "%d: %s", menu_sel + 1, tr(c->star_names[menu_sel]));
        gfx_text_center(g_vw * 0.5f, 290, 2.5f, buf, 1, 1, 1, 1);
        if (star_select_course < CRS_COUNT) {
            snprintf(buf, sizeof buf, tr("Best fish: %d"), save[cur_slot].coin_hi[star_select_course]);
            gfx_text_center(g_vw * 0.5f, 330, 2, buf, 0.9f, 0.9f, 0.6f, 1);
        }
    }
    if ((state_t / 25) % 2) gfx_text_center(g_vw * 0.5f, 400, 3, tr("PRESS A TO ENTER"), 1, 1, 1, 1);
    gfx_text_center(g_vw * 0.5f, 445, 1.8f, tr("B: back to the castle"), 0.7f, 0.7f, 0.8f, 1);
}

static const char *ending_text =
    "THE KERNEL IS RESTORED!\n\n"
    "Segfault has been dumped and the Kernel Stars\n"
    "shine once more over the snowy Kernel Castle.\n\n"
    "The penguins cheer, the daemons go back to\n"
    "their background tasks, and Tux finally gets\n"
    "to eat a well-deserved (frozen) herring.\n\n\n"
    "- STAFF -\n\n"
    "TUX 64 - The Kernel Castle Adventure\n"
    "Game design, code, models, music: generated\n"
    "Engine: C + OpenGL 1.1, no external assets\n"
    "Tux the penguin by Larry Ewing\n\n"
    "Thank you for playing!\n\n"
    "Press A to return to the title.";

static void draw_ending(void)
{
    TuxPose tp;
    float t = ending_t / 60.0f;
    gfx_begin_3d(45, 0.3f, 100);
    glClearColor(0.15f, 0.25f, 0.55f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gfx_lookat(v3(3.5f, 1.6f, 4.5f), v3(2.0f, 1.0f, 0));
    gfx_light(0.5f, 0.7f);
    glEnable(GL_LIGHTING);
    memset(&tp, 0, sizeof tp);
    tp.flipL_z = 2.5f + sinf(t * 8) * 0.4f;
    tp.flipR_z = 2.5f + sinf(t * 8 + 1) * 0.4f;
    tp.lift = fabsf(sinf(t * 4)) * 0.4f;
    draw_tux_model(v3(2.0f, 0, 0), 0.4f, &tp, 1, TUXV_TUX);
    memset(&tp, 0, sizeof tp);
    tp.flipL_z = tp.flipR_z = 1.5f + sinf(t * 6) * 0.5f;
    tp.lift = fabsf(sinf(t * 4 + 1)) * 0.3f;
    draw_tux_model(v3(3.6f, 0, -1.2f), -0.3f, &tp, 0.9f, TUXV_NPC);
    tp.lift = fabsf(sinf(t * 5)) * 0.2f;
    draw_tux_model(v3(1.0f, 0, -1.0f), 0.6f, &tp, 0.6f, TUXV_BABY);
    glPushMatrix();
    glTranslatef(2.6f, 3.0f, -1.5f);
    glRotatef(t * 60, 0, 1, 0);
    glScalef(1.2f, 1.2f, 1.2f);
    glCallList(mdl[MDL_STAR]);
    glPopMatrix();
    gfx_begin_2d();
    snow2d(100, 0.7f, 0.7f);
    {
        /* centred scrolling credits */
        const char *p = tr(ending_text);
        char line[96];
        float y = 480 - ending_t * 0.55f, total = 0;
        const char *q;
        int n;
        for (q = tr(ending_text); *q; q++) if (*q == '\n') total += 22;
        if (y < 470 - total - 22) y = 470 - total - 22;
        gfx_rect(0, 0, g_vw, 480, 0, 0, 0.05f, 0.35f);
        while (*p) {
            n = 0;
            while (p[n] && p[n] != '\n' && n < 95) n++;
            while (n > 0 && p[n] && ((unsigned char)p[n] & 0xC0) == 0x80) n--;   /* keep UTF-8 whole */
            memcpy(line, p, (size_t)n);
            line[n] = 0;
            if (y > -20 && y < 480) {
                int title = (line[0] == '-' || p - n == tr(ending_text));
                gfx_text_center(g_vw * 0.5f, y, title ? 2.4f : 1.9f, line, 1, title ? 0.85f : 1, title ? 0.3f : 1, 1);
            }
            y += 22;
            p += n;
            if (*p == '\n') p++;
        }
    }
}

static void draw_gameover(void)
{
    gfx_begin_2d();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    gfx_text_center(g_vw * 0.5f, 200, 6, tr("GAME OVER"), 0.9f, 0.2f, 0.2f, clampf(gameover_t / 60.0f, 0, 1));
    if (gameover_t > 120) gfx_text_center(g_vw * 0.5f, 300, 2.5f, tr("Press A"), 1, 1, 1, 1);
}

void game_render(void)
{
    switch (gstate) {
    case GS_TITLE: draw_title(); break;
    case GS_FILESELECT: draw_fileselect(); break;
    case GS_STARSELECT: draw_starselect(); break;
    case GS_PLAY:
        render_world();
        draw_labels();
        draw_hud();
        if (dlg_on) dialog_draw();
        if (paused) draw_pause();
        break;
    case GS_GAMEOVER: draw_gameover(); break;
    case GS_ENDING: draw_ending(); break;
    }
    if (g_fade > 0) {
        gfx_begin_2d();
        gfx_rect(0, 0, g_vw, 480, 0, 0, 0, g_fade);
    }
}

void game_init(void)
{
    gstate = GS_TITLE;
    state_t = 0;
    music_play(MUS_TITLE);
    pending_level = -1;
}

void game_debug_state(int st)
{
    gstate = st;
    state_t = 0;
    ending_t = 0;
    gameover_t = 0;
    if (st == GS_ENDING) music_play(MUS_ENDING);
    if (st == GS_GAMEOVER) music_play(MUS_GAMEOVER);
    if (st == GS_FILESELECT) music_play(MUS_SELECT);
    if (st == GS_STARSELECT) { star_select_course = 0; menu_sel = 0; }
}

void game_debug_start(int level, int spawn, int slot)
{
    cur_slot = slot;
    if (!save[slot].exists) { save[slot].exists = 1; save[slot].lives = 4; }
    save[slot].flags |= FL_SEEN_INTRO;
    pl.lives = 4;
    pl.health = 8;
    gstate = GS_PLAY;
    level_load(level, spawn);
}
