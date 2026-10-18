/* game.h - shared game types and globals for TUX 64 */
#ifndef GAME_H
#define GAME_H
#include "mathx.h"
#include "gfx.h"
#include "audio.h"
#include "collision.h"
#include "platform.h"
#include "lang.h"

#define DT (1.0f / 60.0f)
#define FPS 60

/* ---------------- input ---------------- */
enum {
    BT_A = 1, BT_B = 2, BT_Z = 4, BT_START = 8,
    BT_CL = 16, BT_CR = 32, BT_CU = 64, BT_CD = 128,
    BT_UP = 256, BT_DOWN = 512, BT_LEFT = 1024, BT_RIGHT = 2048, BT_R = 4096
};
typedef struct {
    float sx, sy, mag;          /* move stick (+y forward), magnitude 0..1 */
    float cx, cy;               /* camera stick */
    unsigned held, pressed, released;
} Input;
extern Input in;
void input_update(void);
void input_clear(void);

/* ---------------- levels ---------------- */
enum { LV_GROUNDS, LV_LOBBY, LV_PEAK, LV_BAY, LV_FORT, LV_SLIDE, LV_LAIR, LV_DRIFTS, LV_MANOR, LV_SPIRE, LV_MAZE, LV_SEA, LV_SKY, LV_SPACE, LV_NANO, LV_HUGE, LV_CITY, LV_CAVE, LV_SIDE, LV_ICE, LV_COUNT };
enum { CRS_PEAK, CRS_BAY, CRS_FORT, CRS_SLIDE, CRS_LAIR, CRS_SECRET, CRS_DRIFTS, CRS_MANOR, CRS_SPIRE, CRS_MAZE, CRS_SEA, CRS_SKY, CRS_SPACE, CRS_NANO, CRS_HUGE, CRS_CITY, CRS_CAVE, CRS_SIDE, CRS_ICE, CRS_COUNT };
#define SAVE_COURSES 24   /* room in the save file for future courses */
#define MAX_SPAWNS 24
#define MAX_LABELS 24
#define MAX_STARS 7

typedef struct { float x0, z0, x1, z1, ybot, y; int warm; } WaterBox;

typedef struct {
    const char *name;
    int music;
    int course;                 /* CRS_* or -1 */
    float sky_top[3], sky_bot[3], fog[3];
    float fog_start, fog_end;
    float ambient, diffuse;
    float death_y;
    GLuint dl, dl_alpha;
    WaterBox water[8];
    int nwater;
    vec3 spawn[MAX_SPAWNS];
    float spawn_yaw[MAX_SPAWNS];
    struct { vec3 p; const char *text; int course; } label[MAX_LABELS];   /* floating window titles */
    int nlabels;
    int snow, clouds, stars_sky, dark;   /* snow: 0 none, 1 light snowfall, 2 blizzard */
    int aurora;                          /* northern lights in the sky */
    int space;                           /* outer space: stars all around, nebulae, no ground */
    int scale_world;                     /* 1 = NANO VILLAGE (Tux is a giant), 2 = HUGEPAGE HOUSE (Tux is tiny) */
    int tall_world;                      /* 1 = UPTIME HEIGHTS (skyscrapers), 2 = CORE DUMP CAVERNS (deep shaft) */
    float zfar;                          /* far clip plane (0 = default) */
    int side_view;                       /* TTY WORLD 1-1: fixed side camera, Tux moves in the plane z = 0 */
    int ice_mode;                        /* ICE STACK CLIMBER: a tile-grid mountain with its own physics (ice.c) */
    float wind_yaw;
} Level;
extern Level lv;
extern int cur_level;

typedef struct {
    const char *name;
    int nstars;
    const char *star_names[MAX_STARS];
    int level;
} CourseInfo;
extern const CourseInfo courses[CRS_COUNT];

/* ---------------- objects ---------------- */
enum {
    OT_NONE,
    OT_COIN, OT_REDCOIN, OT_BLUECOIN, OT_ONEUP, OT_STAR, OT_STARMARKER, OT_BLUESWITCH,
    OT_BUG, OT_FORKBOMB, OT_KING, OT_CRUSHER, OT_ROLLER, OT_ROLLERGEN, OT_PUFFER, OT_CRAB,
    OT_BRUTE, OT_SEGFAULT, OT_FIREBALL, OT_SPIKEBOMB, OT_FLAMEBAR,
    OT_PLATFORM, OT_ROTPLAT, OT_ELEVATOR, OT_FALLPLAT,
    OT_SIGN, OT_NPC, OT_BABY, OT_MOTHER, OT_RACER, OT_RACEFLAG,
    OT_TRAMPOLINE, OT_TREE, OT_POLE, OT_WARP, OT_DOOR, OT_STARDOOR,
    OT_CLAM, OT_RING, OT_CRATE, OT_GRANDSTAR, OT_TIMERSTART, OT_TIMERGOAL, OT_DECOR, OT_ROCKETBOX,
    OT_GHOST, OT_GHOSTKING, OT_TOPSWITCH, OT_HEART, OT_WATCHDOG, OT_CAGE, OT_STEAM, OT_OOM, OT_MAZEEXIT,
    OT_GATE, OT_ICECHUNK, OT_WHALE, OT_BOOSTPAD, OT_SEAL,
    OT_SKYCOIN, OT_SKYRING,
    OT_SPACERING, OT_COMET, OT_METEOR, OT_ASTEROID,
    OT_NANOFOLK, OT_NANOSHIP, OT_NANOCLOUD, OT_NANOFIRE, OT_NANOSOLDIER, OT_NANOCATAPULT, OT_NANOSHOT, OT_NANOTRAIN,
    OT_TOASTER, OT_GIANTCAT, OT_YARN, OT_GIANTFOOT,
    OT_MINECART, OT_STALACTITE, OT_MUSHROOM, OT_CAVECRYSTAL,
    OT_QBLOCK, OT_BRICK, OT_WALKER, OT_SIDECANNON, OT_SIDEBULLET, OT_PIPEWARP, OT_FLAGPOLE,
    OT_FX_SPARKLE, OT_FX_SMOKE, OT_FX_EXPLODE, OT_FX_BUBBLE, OT_FX_SNOWPUFF, OT_FX_SHOCKWAVE,
    OT_COUNT
};

#define OF_SOLID     1   /* provides dynamic collision */
#define OF_GRABBABLE 2
#define OF_SHADOW    4
#define OF_HELD      8
#define OF_NOCULL    16

#define MAX_OBJ 600

typedef struct Obj {
    int type, active;
    vec3 pos, vel, home;
    float yaw, pitch, roll, scale;
    int state, timer;
    float t;
    float radius, height;
    int hp;
    int param, param2;
    float f[8];
    int i[8];
    int flags;
    vec3 prev_pos;
    float prev_yaw;
    float floor_y;
    Surf *floor;
    GLuint dl;
    const char *text;
} Obj;
extern Obj objs[MAX_OBJ];
extern int nobj;

Obj *obj_spawn(int type, vec3 pos, float yaw, int param);
void obj_kill(Obj *o);
void objs_clear(void);
void objs_update(void);
void objs_draw(void);
void objs_draw_alpha(void);
void objs_collision(void);       /* rebuild dynamic collision from solid objects */
int  obj_index(Obj *o);
void spawn_coins_burst(vec3 p, int n);
void obj_set_box(Obj *o, float hx, float hy, float hz, int tt, int ts, float r, float g, float b);
extern vec3 g_race_path[40];
extern int g_race_n;
extern int g_coin_star_idx;
void fx_spawn(int type, vec3 p, float scale);
Obj *star_spawn(vec3 p, int star, int from_pos, vec3 from);

/* ---------------- player ---------------- */
enum {
    ACT_IDLE, ACT_WALK, ACT_SKID, ACT_LANDING, ACT_CROUCH, ACT_CRAWL, ACT_CROUCH_SLIDE,
    ACT_PUNCH, ACT_BELLY_SLIDE, ACT_KNOCKBACK, ACT_DEATH, ACT_STAR_DANCE, ACT_FROZEN,
    ACT_HOLD_IDLE, ACT_HOLD_WALK, ACT_THROW, ACT_LEDGE_HANG, ACT_LEDGE_CLIMB,
    ACT_POLE, ACT_GPOUND_LAND,
    /* airborne */
    ACT_JUMP, ACT_DJUMP, ACT_TJUMP, ACT_BACKFLIP, ACT_SIDEFLIP, ACT_LONGJUMP, ACT_FALL,
    ACT_WALLKICK, ACT_DIVE, ACT_JUMPKICK, ACT_GPOUND, ACT_AIR_KNOCK, ACT_TRAMP,
    ACT_HOLD_JUMP, ACT_HOLD_FALL, ACT_BURN_JUMP, ACT_ROLLOUT, ACT_BOUNCE, ACT_FLY,
    /* water */
    ACT_SWIM, ACT_DROWN,
    /* dash boots (AURORA SEA): skimming over water and ice; act_s 1 = airborne */
    ACT_SKATE,
    /* FROSTCLOUD SKYWAY: free rocket flight in the plane of the sky path */
    ACT_SKYFLY,
    /* STARDUST ORBIT: free 3D rocket flight with an endless fuel tank */
    ACT_SPACEFLY,
    ACT_COUNT
};
#define ACT_FIRST_AIR ACT_JUMP

typedef struct {
    vec3 pos, vel;
    float fwd, yaw;
    float pitch, roll;           /* visual only (swim / slide) */
    int act, prev_act, act_t, act_s;
    float act_f;
    int health;                  /* 0..8 */
    int invinc;
    int hurt_queue, heal_queue;
    int lives, coins, red_coins;
    Surf *floor; float floor_y;
    float ceil_y;
    vec3 wall_n; int wall_timer;
    int jump_chain, chain_timer;
    float peak_y;
    int held;                    /* object index or -1 */
    int platform;                /* object index we stand on, -1 */
    int pole;                    /* object index */
    float pole_h;
    float anim;
    int punch_n;
    int kicked;                  /* used jumpkick in this jump */
    float water_y; int in_water;
    float breath_t;
    int tramp_combo;             /* consecutive well-timed trampoline bounces */
    int a_age;                   /* frames since A was last pressed */
    vec3 ledge_pos;
    int visible;
    int squish;
    int sleep_t;
    float step_t;
    float slide_snd;
    int no_ctrl;
    int rocket_t;                /* rocket pack fuel (frames left) */
    float sink;                  /* depth sunk into powder snow */
    int water_warm;              /* swimming in a hot spring */
    int boots;                   /* dash boots on (they never come off on the AURORA SEA) */
    int boost_t;                 /* frames of extra dash speed left */
    float ground_vy;             /* vertical speed while skating up/down slopes (ramps launch Tux) */
    int trick;                   /* 0 none, 1 spinning in mid-air, 2 trick finished */
    float trick_t;
    float sky_s, sky_y, sky_vs, sky_vy;   /* sky path position (arc length, height) and velocity */
    float sky_dash;              /* dash gauge 0..1 */
    float sp_speed;              /* space flight: current cruise speed */
    int sp_stun;                 /* frames of lost control after a crash */
    int sp_turn;                 /* frames left of a Z quick turn */
} Player;
extern Player pl;

void player_init(vec3 pos, float yaw);
void player_update(void);
void player_draw(void);
void player_hurt(int dmg, vec3 from);
void player_heal(int n);
int  player_is_attacking(void);
int  player_in_air(void);
void player_set_act(int act);
void player_bounce(float vy);
int  player_rocket_firing(void);
float water_level_at(float x, float z, float y);

/* ---------------- camera ---------------- */
enum { CAM_FOLLOW, CAM_DANCE, CAM_FIXED, CAM_DEATH };
typedef struct {
    vec3 pos, at;
    float yaw;                   /* yaw from camera toward player */
    float yaw_target_off;
    float dist, dist_target;
    int zoom;
    int mode;
    float shake;
    vec3 fixed_pos;
    float fov;                   /* vertical field of view (degrees) */
} Camera;
extern Camera cam;
void camera_reset(void);
void camera_update(void);
float camera_yaw(void);

/* ---------------- models ---------------- */
void models_init(void);
typedef struct {
    float body_pitch, body_roll, body_squash, lift, flip, spin;
    float head_pitch, head_yaw;
    float flipL_x, flipL_z, flipR_x, flipR_z;
    float footL, footR;
    int eyes_closed;
    int rocket;                  /* 0 none, 1 rocket pack, 2 pack firing */
    int boots;                   /* 0 none, 1 dash boots, 2 boots boosting */
} TuxPose;
enum { TUXV_TUX, TUXV_BABY, TUXV_MOTHER, TUXV_NPC, TUXV_COUNT };
void draw_tux_model(vec3 pos, float yaw, const TuxPose *tp, float scale, int variant);
GLuint model_box(float sx, float sy, float sz, int tex_top, int tex_side, float r, float g, float b);
GLuint model_cyl(float r, float h, int tex_top, int tex_side, float cr, float cg, float cb);
enum {
    MDL_COIN, MDL_STAR, MDL_BUG, MDL_BOMB, MDL_BOMB_LIT, MDL_KING, MDL_CRUSHER, MDL_ROLLER,
    MDL_PUFFER, MDL_CRAB, MDL_BRUTE, MDL_SEG_BODY, MDL_SEG_HEAD, MDL_SPIKEBOMB,
    MDL_SIGN, MDL_GNU, MDL_TRAMPOLINE, MDL_PINE, MDL_PINE_SNOW, MDL_TREE, MDL_DOOR, MDL_STARDOOR,
    MDL_CLAM_BOT, MDL_CLAM_TOP, MDL_RING, MDL_CRATE, MDL_FISH, MDL_SWITCH, MDL_POLE,
    MDL_FLAG, MDL_KEY, MDL_COIN_RED, MDL_COIN_BLUE, MDL_FISH_GREEN, MDL_STAR_GHOST, MDL_CROWN,
    MDL_SNOWMAN, MDL_ROCK, MDL_BUSH, MDL_ROCKETBOX, MDL_GHOST, MDL_GHOST_FADE, MDL_HEART, MDL_ICESPIRE,
    MDL_CRYSTAL, MDL_DEADTREE, MDL_TOPSWITCH, MDL_TOPSWITCH_ON, MDL_WATCHDOG, MDL_CAGE, MDL_MOUND, MDL_OOM, MDL_CYDOOR, MDL_CYSTARDOOR, MDL_CHILDPROC, MDL_INIT,
    MDL_WHALE, MDL_BUOY, MDL_BUOY_NEXT, MDL_ICECHUNK, MDL_BOOSTPAD, MDL_SEAL, MDL_COUNT
};
extern GLuint mdl[MDL_COUNT];

/* ---------------- game flow ---------------- */
enum { GS_TITLE, GS_FILESELECT, GS_STARSELECT, GS_PLAY, GS_GAMEOVER, GS_ENDING };
extern int gstate;
extern int g_frame;
extern int paused;

typedef struct {
    unsigned char exists;
    unsigned char stars[SAVE_COURSES];     /* bitmask per course */
    unsigned short coin_hi[SAVE_COURSES];
    unsigned flags;
    unsigned char lives;
} SaveSlot;
#define FL_BEAT_GAME   1
#define FL_SEEN_INTRO  2
#define FL_CANNON_PEAK 4
extern SaveSlot save[3];
extern int cur_slot;
int  total_stars(void);
int  has_star(int course, int star);
int  star_course(void);
void save_write(void);
void save_load(void);

/* dialog / messages */
void dialog_open(const char *text);
int  dialog_active(void);

/* level / warp control */
void level_load(int level, int spawn);
void warp_to(int level, int spawn, int star_select);
void course_exit(void);           /* back to lobby after star / exit */
void game_star_collected(int star, vec3 pos);
void game_player_died(void);
void game_add_coins(int n, vec3 p);
void game_red_coin(vec3 p);
void game_grand_star(void);
extern int g_course_timer;        /* frames, used by slide & race */
extern int g_timer_on;
extern int g_star_dance_star;
extern int g_entry_painting;      /* spawn index to return to in the lobby */
extern int g_blue_switch_timer;
extern int g_race_state;          /* 0 idle 1 racing 2 won 3 lost */
extern float g_fade;
extern int g_boss_hits;
/* random labyrinth (levels.c); walls per cell: 1 = north (-z), 2 = south (+z), 4 = west (-x), 8 = east (+x) */
#define MAZE_MAX 16
typedef struct { int w, h; float cell, x0, z0; unsigned char wall[MAZE_MAX][MAZE_MAX]; } Maze;
extern Maze g_maze;
extern int g_maze_spotted;     /* the OOM Killer saw Tux during this visit */
extern unsigned g_maze_seed;   /* 0 = random every visit */
int  maze_cell(vec3 p, int *cx, int *cz);
vec3 maze_center(int cx, int cz);
int  maze_next_step(int fx, int fz, int tx, int tz, int *nx, int *nz);
/* AURORA SEA rally (objects.c) */
#define SEA_LAPS 2
extern int g_gate_n, g_gate_next, g_lap, g_sea_power, g_gate_missed;
extern int g_race_time;          /* frames of the last finished rally */
void sea_player_moved(vec3 from, vec3 to);   /* gate crossing checks */
Obj *sea_gate(int k);
/* FROSTCLOUD SKYWAY (sky.c) */
#define SKY_LINES 4
extern const int g_sky_lines[SKY_LINES];   /* fish needed in one flight for each star */
extern int g_sky_time, g_sky_state, g_sky_lines_hit;
float sky_len(void);
vec3 sky_point(float s);
vec3 sky_tangent(float s);
vec3 sky_normal(float s);
vec3 sky_world(float s, float y);
float sky_screen_dir(float s);
float sky_clamp_y(float y);
void sky_reset(void);
void sky_update(void);
void sky_trail_push(float s, float y);
void sky_obj_update(Obj *o);
void sky_obj_draw(Obj *o);
void sky_draw_alpha(void);
void sky_camera(void);
/* STARDUST ORBIT (space.c) */
#define SPACE_RINGS 20
#define SPACE_RING_TIME (35 * 60)
#define SPACE_METEORS 6
extern int g_ring_run_next, g_ring_run_t, g_meteors_left;
void space_reset(void);
void space_update(void);
void space_obj_update(Obj *o);
void space_obj_draw(Obj *o);
void space_draw_alpha(void);
void space_draw_sky(void);
void space_camera(void);
vec3 space_ring_pos(int k);
Obj *space_ring(int k);
/* NANO VILLAGE (Tux is a giant) and HUGEPAGE HOUSE (Tux is tiny): scale.c */
extern int g_nano_ships_left, g_nano_fires_left, g_nano_catapults_left;
extern vec3 g_nano_harbor, g_nano_windmill;
vec3 nano_track_point(float u);
void scale_reset(void);
void scale_update(void);
void scale_obj_update(Obj *o);
void scale_obj_draw(Obj *o);
void scale_draw_alpha(void);
int  scale_obj_solid(Obj *o);
/* UPTIME HEIGHTS and CORE DUMP CAVERNS: vertical.c */
#define CAVE_WALK_TURNS 2
#define CAVE_TRACK_ANGLE (3 * PI_F + 0.6f)
extern int g_crystals_left;
extern int g_ring_next;     /* objects.c: next ring to pass */
float cave_walk_y(float a);
vec3 cave_polar(float a, float r, float y);
vec3 cave_track_point(float u);
void vert_reset(void);
void vert_update(void);
void vert_obj_update(Obj *o);
void vert_obj_draw(Obj *o);
void vert_draw_alpha(void);
void vert_hud(void);
/* TTY WORLD 1-1: side.c */
#define SIDE_TILE 1.6f
void side_reset(void);
void side_camera(void);
void side_obj_update(Obj *o);
void side_obj_draw(Obj *o);
int  side_obj_solid(Obj *o);
void side_draw_sky(void);
/* ICE STACK CLIMBER: ice.c */
extern int g_ice_score, g_ice_mountain;
extern int g_selected_star;     /* the star chosen on the star select screen */
void ice_reset(void);
void ice_update(void);
void ice_camera(void);
void ice_draw(void);
void ice_hud(void);
int  ice_debug_floor(void);
void ice_bot(float *sx, unsigned *buttons);
int  ice_debug_bonus(void);     /* adds the dynamic collision of a scale-world object; 1 if handled */
extern vec3 g_wind;          /* blizzard wind velocity (u/s), zero when calm */
extern float g_gust;         /* 0..1 gust strength */
extern const char *g_hud_msg;
extern int g_hud_msg_t;
void hud_message(const char *m, int frames);

/* level builders (levels.c) */
void build_level(int level);
float terrain_h(float x, float z);

/* level builder helpers */
void lb_begin(void);
void lb_end(GLuint *dl);
void lb_end_alpha(GLuint *dl);
void lb_tri(int tex, vec3 a, vec3 b, vec3 c, float r, float g, float bb, int flags, int collide);
void lb_quad(int tex, vec3 a, vec3 b, vec3 c, vec3 d, float r, float g, float bb, int flags, int collide);
void lb_box(float x0, float y0, float z0, float x1, float y1, float z1, int tex_top, int tex_side,
            float r, float g, float b, int flags);
void lb_obox(vec3 c, vec3 half, float yaw, float pitch, int tex_top, int tex_side, float r, float g, float b, int flags);
void lb_cyl(float x, float y, float z, float rad, float h, int seg, int tex_top, int tex_side,
            float r, float g, float b, int flags);
void lb_cone(float x, float y, float z, float rad, float h, int seg, int tex, float r, float g, float b, int collide);
void lb_terrain(float x0, float z0, float x1, float z1, int nx, int nz, float (*hf)(float, float),
                void (*style)(float x, float y, float z, vec3 n, int *tex, float *col, int *flags));
void lb_water(float x0, float z0, float x1, float z1, float y, float ybot);
void lb_ramp(float x0, float z0, float x1, float z1, float y0, float y1, int axis, float thick, int tex_top,
             int tex_side, float r, float g, float b, int flags);
void lb_stairs(float x0, float z0, float x1, float z1, float y0, float y1, int axis, int steps, int tex,
               float r, float g, float b);
void lb_appwindow(vec3 c, float yaw, float w, float h, int tex, float r, float g, float b);
void lb_track(const vec3 *pts, int n, float width, float wall_h, int sub, int tex, float r, float g, float b, int flags);
extern int lb_alpha_mode;
extern float lb_uv_mul;

#endif
