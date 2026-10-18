/* bot.c - tiny debug autopilot used for automated play-testing (--bot "commands")
 *
 * commands, separated by ';' :
 *   g X Z [TOL]      walk/run to (X,Z)
 *   j X Z            run toward (X,Z) and jump once
 *   w N              idle N frames
 *   p KEYS N         hold buttons (a b z s) for N frames
 *   h X Z KEYS N     hold stick toward (X,Z) plus buttons for N frames
 *   b TYPE D         go behind the first object of TYPE at distance D, then face it
 *   f TYPE           face object of TYPE (a few frames of stick)
 *   s N              spin the stick in circles for N frames
 *   d                press A until no dialog is open
 *   u ACT N          wait until player action == ACT (max N frames)
 *   T X Y Z          teleport (debug)
 *   A TYPE N         walk up to the nearest object of TYPE and punch it (N frames)
 *   Q N              fight the watchdog: dodge its lunge, then kick it
 *   G TYPE           walk to the nearest object of TYPE and pick it up with B
 *   M X Z N          bounce on the trampoline with good timing, then steer to (X,Z)
 *   B X Z N          AURORA SEA: ride a whale until the spout fires, then steer to (X,Z)
 *   Y I              AURORA SEA debug: stand on the blowhole of whale I
 *   Z N DASH         FROSTCLOUD SKYWAY: chase fish until time is up (DASH 1 = use the dash)
 *   q S Y            FROSTCLOUD SKYWAY: fly to path coordinates (arc length S, height Y)
 *   D X Z            AURORA SEA: drive the dash boots to (X,Z)
 *   R N              AURORA SEA: drive the dash boots through the buoy gates until the rally ends
 *   o X Y Z B        STARDUST ORBIT: fly to (X,Y,Z), B 1 = boost
 *   U N              STARDUST ORBIT: fly the ring run (max N frames)
 *   c N              STARDUST ORBIT: chase the comet at full boost
 *   m N              STARDUST ORBIT: ram the ice meteors at full boost
 *   e N              STARDUST ORBIT: fly to the nearest red fish until none are left
 *   P                print state
 */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char buf[8192];
static char *cur;
static int active, have, t, stepno;
static char op;
static float a[4];
static char keys[16];
extern int g_frame;
extern int g_trace;

void bot_set(const char *s)
{
    strncpy(buf, s, sizeof buf - 1);
    cur = buf;
    active = 1;
    have = 0;
}

int bot_active(void) { return active; }

static void parse(void)
{
    char *e, line[256], *tk[8];
    int n = 0, len;
    while (*cur == ' ' || *cur == ';') cur++;
    if (!*cur) { active = 0; printf("BOT: done at frame %d\n", g_frame); return; }
    e = strchr(cur, ';');
    if (!e) e = cur + strlen(cur);
    len = (int)(e - cur);
    if (len > 255) len = 255;
    memcpy(line, cur, (size_t)len);
    line[len] = 0;
    cur = e;
    op = line[0];
    memset(a, 0, sizeof a);
    keys[0] = 0;
    {
        char *p = strtok(line + 1, " ");
        while (p && n < 8) { tk[n++] = p; p = strtok(NULL, " "); }
    }
    if (op == 'p') {
        if (n > 0) { strncpy(keys, tk[0], 15); keys[15] = 0; }
        if (n > 1) a[0] = (float)atof(tk[1]);
    } else if (op == 'h') {
        if (n > 0) a[0] = (float)atof(tk[0]);
        if (n > 1) a[1] = (float)atof(tk[1]);
        if (n > 2) { strncpy(keys, tk[2], 15); keys[15] = 0; }
        if (n > 3) a[2] = (float)atof(tk[3]);
    } else {
        int i;
        for (i = 0; i < n && i < 4; i++) a[i] = (float)atof(tk[i]);
    }
    have = 1;
    t = 0;
    stepno++;
}

static unsigned keymask(const char *k)
{
    unsigned m = 0;
    for (; *k; k++) {
        if (*k == 'a') m |= BT_A;
        if (*k == 'b') m |= BT_B;
        if (*k == 'z') m |= BT_Z;
        if (*k == 's') m |= BT_START;
    }
    return m;
}

static void stick_to(float x, float z, float mag)
{
    float y = atan2f(x - pl.pos.x, z - pl.pos.z);
    float d = camera_yaw() - y;
    in.sx = sinf(d) * mag;
    in.sy = cosf(d) * mag;
    in.mag = mag;
}

static Obj *find(int type)
{
    int i;
    for (i = 0; i < nobj; i++) if (objs[i].active && objs[i].type == type) return &objs[i];
    return NULL;
}

static void done(void)
{
    have = 0;
    if (g_frame % 1 == 0) {}
}

static unsigned prev;

/* STARDUST ORBIT: point the rocket at tg */
static float space_steer(vec3 tg)
{
    vec3 c = v3add(pl.pos, v3(0, 0.8f, 0)), d = v3sub(tg, c);
    float wy = atan2f(d.x, d.z), wp = atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z));
    float dy = angle_diff(wy, pl.yaw), dp = wp - pl.pitch;
    in.sx = clampf(-dy * 3, -1, 1);
    in.sy = clampf(-dp * 3, -1, 1);
    in.mag = 1;
    return fabsf(dy) + fabsf(dp);
}

static Obj *nearest(int type)
{
    int i;
    Obj *b = NULL;
    float bd = 1e9f;
    for (i = 0; i < nobj; i++)
        if (objs[i].active && objs[i].type == type && v3dist(objs[i].pos, pl.pos) < bd) { b = &objs[i]; bd = v3dist(b->pos, pl.pos); }
    return b;
}

void bot_update(void)
{
    unsigned h = 0;
    if (!active) return;
    if (!have) parse();
    if (!active) return;
    t++;
    in.sx = in.sy = in.mag = 0;
    if (dialog_active() && op != 'd') {
        h = (g_frame % 8 == 0) ? BT_A : 0;
        in.pressed = h & ~prev; in.held = h; prev = h;
        t--;
        return;
    }
    switch (op) {
    case 'g': case 'j': {
        float tol = a[2] > 0 ? a[2] : 0.8f;
        float dx = a[0] - pl.pos.x, dz = a[1] - pl.pos.z, d = sqrtf(dx * dx + dz * dz);
        stick_to(a[0], a[1], d < 2 ? 0.6f : 1.0f);
        if (op == 'j' && t == 20) h |= BT_A;
        if (op == 'j' && t > 20 && t < 40) h |= BT_A;
        if ((op == 'g' && d < tol) || (op == 'j' && t > 45 && !player_in_air())) {
            printf("BOT %d: reached (%.1f %.1f) pos=(%.2f %.2f %.2f) f%d\n", stepno, a[0], a[1], pl.pos.x, pl.pos.y, pl.pos.z, g_frame);
            done();
        }
        if (t > 1500) { printf("BOT %d: TIMEOUT going to (%.1f %.1f) pos=(%.2f %.2f %.2f)\n", stepno, a[0], a[1], pl.pos.x, pl.pos.y, pl.pos.z); done(); }
        break;
    }
    case 'w': if (t >= (int)a[0]) done(); break;
    case 'p': h |= keymask(keys); if (t >= (int)a[0]) done(); break;
    case 'h': stick_to(a[0], a[1], 1); h |= keymask(keys); if (t >= (int)a[2]) done(); break;
    case 'b': {
        Obj *o = find((int)a[0]);
        if (!o) { printf("BOT %d: object %d not found\n", stepno, (int)a[0]); done(); break; }
        {
            vec3 tgt = v3mad(o->pos, yaw_dir(o->yaw), -a[1]);
            float d = v3distxz(tgt, pl.pos);
            if (d > 0.5f && t < 900) stick_to(tgt.x, tgt.z, d < 2 ? 0.5f : 1.0f);
            else {
                pl.yaw = yaw_to(pl.pos, o->pos);
                printf("BOT %d: behind obj type %d (d=%.2f) f%d\n", stepno, (int)a[0], d, g_frame);
                done();
            }
        }
        break;
    }
    case 'F': {   /* face away from object */
        Obj *o = find((int)a[0]);
        if (o) pl.yaw = wrap_angle(yaw_to(pl.pos, o->pos) + PI_F);
        done();
        break;
    }
    case 'N': {   /* wait until object TYPE within D of player */
        int i, ok = 0;
        for (i = 0; i < nobj; i++)
            if (objs[i].active && objs[i].type == (int)a[0] && v3dist(objs[i].pos, pl.pos) < a[1]) ok = 1;
        if (ok || t > 1200) { if (!ok) printf("BOT %d: N TIMEOUT\n", stepno); done(); }
        break;
    }
    case 'L': {   /* lure Segfault's charge into a spike mine (max N frames) */
        static int mine = -1, dodge, hp0;
        Obj *b = find(OT_SEGFAULT), *m;
        vec3 c = v3(0, 0, -150);
        if (t == 1) {
            int i;
            float bd = 1e9f;
            mine = -1; dodge = 0; hp0 = b ? b->hp : 0;
            for (i = 0; i < nobj; i++)
                if (objs[i].active && objs[i].type == OT_SPIKEBOMB && objs[i].state == 0 && v3dist(objs[i].pos, pl.pos) < bd) {
                    bd = v3dist(objs[i].pos, pl.pos); mine = i;
                }
        }
        if (!b || mine < 0 || b->hp != hp0 || t > a[0]) { printf("BOT %d: lure end hp=%d f%d\n", stepno, b ? b->hp : -1, g_frame); done(); break; }
        m = &objs[mine];
        if (g_trace && t % 30 == 0) printf("L t%d seg st%d tm%d pos(%.1f %.1f) d=%.1f mine(%.1f %.1f) st%d pl(%.1f %.1f %.1f) hp%d\n", t, b->state, b->timer, b->pos.x, b->pos.z, v3distxz(b->pos, pl.pos), m->pos.x, m->pos.z, m->state, pl.pos.x, pl.pos.y, pl.pos.z, pl.health);
        if (b->state == 3 && b->timer >= 40 && v3distxz(b->pos, pl.pos) < 8) dodge = 25;
        if (dodge > 0) {
            vec3 r = v3norm(v3sub(m->pos, c));
            dodge--;
            stick_to(pl.pos.x - r.z * 5 + r.x, pl.pos.z + r.x * 5 + r.z, 1);
        } else {
            vec3 r = v3norm(v3sub(m->pos, c));
            vec3 tp = v3mad(m->pos, r, 3.5f);
            if (v3distxz(tp, pl.pos) > 0.5f) stick_to(tp.x, tp.z, minf(1, v3distxz(tp, pl.pos)));
        }
        break;
    }
    case 'A': {
        static int tgt;
        Obj *o;
        if (t == 1) {
            int i;
            float bd = 1e9f;
            tgt = -1;
            for (i = 0; i < nobj; i++)
                if (objs[i].active && objs[i].type == (int)a[0] && objs[i].state != 90 && v3dist(objs[i].pos, pl.pos) < bd) {
                    bd = v3dist(objs[i].pos, pl.pos); tgt = i;
                }
        }
        o = tgt >= 0 ? &objs[tgt] : NULL;
        if (o && (!o->active || o->type != (int)a[0] || o->state == 90)) { printf("BOT %d: target down f%d\n", stepno, g_frame); done(); break; }
        if (!o || t > a[1]) { done(); break; }
        if (v3distxz(o->pos, pl.pos) > 1.6f) stick_to(o->pos.x, o->pos.z, 1);
        else { pl.yaw = yaw_to(pl.pos, o->pos); if (t % 8 == 0) h |= BT_B; }
        break;
    }
    case 'Q': {
        Obj *o = find(OT_WATCHDOG);
        static int hp0;
        if (t == 1 && o) hp0 = o->hp;
        if (!o || o->hp != hp0 || t > a[0]) { printf("BOT %d: watchdog hp=%d f%d\n", stepno, o ? o->hp : -1, g_frame); done(); break; }
        if (o->state == 2) {
            stick_to(o->pos.x, o->pos.z, 1);
            if (v3distxz(o->pos, pl.pos) < 2.6f) { pl.yaw = yaw_to(pl.pos, o->pos); in.mag = 0; in.sx = in.sy = 0; if (t % 6 == 0) h |= BT_B; }
        } else if (o->state == 1 && o->timer >= 8 && o->timer < 30) {
            vec3 d = v3norm(v3sub(pl.pos, o->pos));
            stick_to(pl.pos.x - d.z * 5, pl.pos.z + d.x * 5, 1);   /* sidestep */
        } else {
            vec3 d = v3norm(v3sub(pl.pos, o->home));
            vec3 tp = v3mad(o->home, d, 9.5f);
            if (v3distxz(tp, pl.pos) > 0.5f) stick_to(tp.x, tp.z, minf(1, v3distxz(tp, pl.pos)));
        }
        break;
    }
    case 'G': {
        Obj *o = NULL;
        int i;
        float bd = 1e9f;
        for (i = 0; i < nobj; i++)
            if (objs[i].active && objs[i].type == (int)a[0] && objs[i].state != 90 && objs[i].state != 101 && fabsf(objs[i].pos.y - pl.pos.y) < 1.2f && v3dist(objs[i].pos, pl.pos) < bd && v3dist(objs[i].pos, pl.pos) < 12) {
                bd = v3dist(objs[i].pos, pl.pos); o = &objs[i];
            }
        if (!o && t <= 400) break;
        if (!o || pl.held >= 0 || t > 400) { printf("BOT %d: grab %s f%d\n", stepno, pl.held >= 0 ? "ok" : "failed", g_frame); done(); break; }
        if (v3distxz(o->pos, pl.pos) > 1.2f) stick_to(o->pos.x, o->pos.z, 1);
        else { pl.yaw = yaw_to(pl.pos, o->pos); if (t % 4 == 0) { h |= BT_B; if (g_trace) printf("G: d=%.2f dy=%.2f st=%d act=%d\n", v3distxz(o->pos, pl.pos), o->pos.y - pl.pos.y, o->state, pl.act); } }
        break;
    }
    case 'f': {
        Obj *o = find((int)a[0]);
        if (o) pl.yaw = yaw_to(pl.pos, o->pos);
        done();
        break;
    }
    case 's': {
        float ang = t * 0.35f;
        in.sx = sinf(ang); in.sy = cosf(ang); in.mag = 1;
        if (t >= (int)a[0]) done();
        break;
    }
    case 'S': {   /* swim to 3D point */
        float dx = a[0] - pl.pos.x, dz = a[2] - pl.pos.z, dxz = sqrtf(dx * dx + dz * dz);
        float dy = a[1] - (pl.pos.y + 0.8f);
        if (dxz > 0.4f) stick_to(a[0], a[2], dxz < 2 ? 0.5f : 1.0f);
        if (dy > 0.3f) h |= BT_A;
        if (dy < -0.3f) h |= BT_Z;
        if (dxz < 0.8f && fabsf(dy) < 0.8f) {
            printf("BOT %d: swam to (%.1f %.1f %.1f) f%d hp=%d\n", stepno, a[0], a[1], a[2], g_frame, pl.health);
            done();
        }
        if (t > 1500) { printf("BOT %d: SWIM TIMEOUT pos=(%.2f %.2f %.2f)\n", stepno, pl.pos.x, pl.pos.y, pl.pos.z); done(); }
        break;
    }
    case 'n': {   /* wait until an object of TYPE is within D of (X,Z):  n TYPE X Z D */
        int i, ok = 0;
        for (i = 0; i < nobj; i++) {
            Obj *o = &objs[i];
            if (o->active && o->type == (int)a[0] && v3distxz(o->pos, v3(a[1], 0, a[2])) < a[3]) ok = 1;
        }
        if (ok || t > 1200) { if (!ok) printf("BOT %d: wait-near TIMEOUT\n", stepno); done(); }
        break;
    }
    case 'r': {   /* wait until object TYPE yaw ~ DEG (mod 180):  r TYPE DEG TOL */
        Obj *o = find((int)a[0]);
        if (!o) { done(); break; }
        {
            float d = fabsf(angle_diff(o->yaw * 2, RAD(a[1]) * 2)) * 0.5f;
            if (d < RAD(a[2]) || t > 1200) done();
        }
        break;
    }
    case 'J': {   /* run toward (X,Z) and do a triple jump */
        static int lastact;
        stick_to(a[0], a[1], 1);
        if (t == 25) h |= BT_A;
        if (t > 25 && pl.act == ACT_LANDING && lastact != ACT_LANDING) h |= BT_A;
        if (t > 25 && (pl.act == ACT_JUMP || pl.act == ACT_DJUMP || pl.act == ACT_TJUMP) && pl.vel.y > 0) h |= BT_A;
        lastact = pl.act;
        if (pl.act == ACT_FLY || t > 200) {
            printf("BOT %d: triple jump end act=%d f%d\n", stepno, pl.act, g_frame);
            done();
        }
        break;
    }
    case 'M': {   /* trampoline: time A presses, after 3 good bounces steer to (X,Z) */
        static int pressed_this_fall;
        Obj *o = find(OT_TRAMPOLINE);
        if (!o) { done(); break; }
        if (pl.tramp_combo >= 3 && pl.act == ACT_TRAMP && pl.pos.y > o->pos.y + 6) stick_to(a[0], a[1], 1);
        else if (v3distxz(pl.pos, o->pos) > 0.3f) stick_to(o->pos.x, o->pos.z, minf(1, v3distxz(pl.pos, o->pos)));
        if (pl.vel.y > 0) pressed_this_fall = 0;
        if (pl.vel.y < 0 && !pressed_this_fall && pl.pos.y < o->pos.y + 0.9f + 2.0f && player_in_air()) {
            h |= BT_A; pressed_this_fall = 1;
        }
        if (t > a[2]) done();
        break;
    }
    case 'X': { /* debug: set state of first object of TYPE:  X TYPE STATE */
        Obj *o = find((int)a[0]);
        if (o) { o->state = (int)a[1]; o->timer = 0; }
        done();
        break;
    }
    case 'W':   /* wait for a calm moment in the blizzard */
        if (g_gust < 0.05f || t > 900) done();
        break;
    case 'k':   /* raw stick sx sy for N frames */
        in.sx = a[0]; in.sy = a[1]; in.mag = sqrtf(a[0] * a[0] + a[1] * a[1]);
        if (in.mag > 1) { in.sx /= in.mag; in.sy /= in.mag; in.mag = 1; }
        if (t >= (int)a[2]) done();
        break;
    case '*': { /* debug: give the save file at least N stars */
        int c, b, n = (int)a[0];
        for (c = 0; c < CRS_COUNT && total_stars() < n; c++)
            for (b = 0; b < 7 && total_stars() < n; b++) save[cur_slot].stars[c] |= (unsigned char)(1 << b);
        done();
        break;
    }
    case 'O': { /* debug: place the OOM Killer at cell (CX, CZ) facing YAWDEG and freeze its brain for a moment */
        Obj *o = find(OT_OOM);
        if (o) { o->pos = maze_center((int)a[0], (int)a[1]); o->yaw = RAD(a[2]); o->state = 0; o->timer = 0;
                 o->i[0] = (int)a[0]; o->i[1] = (int)a[1]; }
        g_maze_spotted = 0;
        done();
        break;
    }
    case 'C': { /* count fish in the level */
        int i, c = 0, r = 0, b = 0;
        for (i = 0; i < nobj; i++) {
            if (!objs[i].active) continue;
            if (objs[i].type == OT_COIN || objs[i].type == OT_SKYCOIN) c++;
            if (objs[i].type == OT_REDCOIN) r++;
            if (objs[i].type == OT_ICECHUNK) b++;
        }
        printf("BOT %d: fish=%d red=%d chunks=%d total=%d\n", stepno, c, r, b, c + r * 2 + b);
        done();
        break;
    }
    case 'V':   /* print whether the OOM Killer has spotted Tux */
        printf("BOT %d: spotted=%d\n", stepno, g_maze_spotted);
        done();
        break;
    case 'K': { /* wall-kick climb:  K DIRX KICKS  - face +x/-x, run, jump, then kick each time a wall is touched */
        static int kicks, lastA;
        if (t == 1) { kicks = 0; lastA = 0; pl.yaw = a[0] > 0 ? PI_F / 2 : -PI_F / 2; }
        if (t == 1) lastA = 0;
        if (kicks == 0 && pl.wall_timer == 0 && t < 120) stick_to(pl.pos.x + a[0] * 5, pl.pos.z, 1);
        if (t == 14) h |= BT_A;
        if (t > 14 && t < 22) h |= BT_A;
        if (t > 22 && pl.wall_timer > 0 && !lastA && kicks < (int)a[1]) { h |= BT_A; kicks++; }
        lastA = (h & BT_A) && pl.wall_timer > 0;
        if ((t > 40 && !player_in_air()) || t > 900) {
            printf("BOT %d: wall kicks %d -> pos=(%.2f %.2f %.2f) f%d\n", stepno, kicks, pl.pos.x, pl.pos.y, pl.pos.z, g_frame);
            done();
        }
        break;
    }
    case 'B': {   /* ride a whale (brake) until its spout launches Tux, then steer to (X,Z):  B X Z N */
        static int launched;
        if (t == 1) launched = 0;
        if (!launched && pl.act == ACT_TRAMP) launched = t;
        if (!launched) { in.sx = 0; in.sy = -1; in.mag = 1; }
        else stick_to(a[0], a[1], 1);
        if ((launched && t - launched > 20 && !player_in_air()) || t > a[2]) {
            printf("BOT %d: whale ride launched=%d pos=(%.2f %.2f %.2f) act=%d f%d\n", stepno, launched, pl.pos.x, pl.pos.y, pl.pos.z, pl.act, g_frame);
            done();
        }
        break;
    }
    case 'Y': {   /* debug: stand on the blowhole of whale number I */
        int i, n = 0;
        for (i = 0; i < nobj; i++)
            if (objs[i].active && objs[i].type == OT_WHALE && n++ == (int)a[0]) {
                Obj *w = &objs[i];
                vec3 hp = v3add(v3mad(w->pos, yaw_dir(w->yaw), 3.2f), v3(0, 2.2f, 0));
                player_init(hp, w->yaw);
            }
        done();
        break;
    }
    case 'Z': {   /* FROSTCLOUD SKYWAY autopilot: chase the nearest fish ahead until time is up (max N frames) */
        int i, best = -1;
        float bs = 1e9f, L = sky_len(), dir = sky_screen_dir(pl.sky_s);
        float ps = fmodf(pl.sky_s, L);
        if (ps < 0) ps += L;
        for (i = 0; i < nobj; i++) {
            Obj *o = &objs[i];
            float ds;
            if (!o->active || o->type != OT_SKYCOIN || o->state != 0) continue;
            ds = o->f[6] - ps;
            if (ds < -L * 0.5f) ds += L;
            if (ds > L * 0.5f) ds -= L;
            if (ds < -0.5f || ds > 30) continue;
            ds = ds + fabsf(o->f[7] - pl.sky_y) * 0.8f;
            if (ds < bs) { bs = ds; best = i; }
        }
        if (best >= 0) {
            Obj *o = &objs[best];
            float ds = o->f[6] - ps, dy = o->f[7] - pl.sky_y, l;
            if (ds < -L * 0.5f) ds += L;
            if (ds > L * 0.5f) ds -= L;
            l = sqrtf(ds * ds + dy * dy);
            if (l > 0.01f) { in.sx = ds / l * dir; in.sy = dy / l; }
        } else { in.sx = dir; in.sy = -pl.sky_y * 0.1f; }
        in.mag = 1;
        if (pl.sky_dash > 0.3f && a[1] > 0) h |= BT_A;
        if (g_sky_state == 1 || t > a[0]) {
            printf("BOT %d: sky flight end fish=%d s=%.1f f%d\n", stepno, pl.coins, pl.sky_s, g_frame);
            done();
        }
        break;
    }
    case 'q': {   /* FROSTCLOUD SKYWAY: fly to path coordinates (S, Y) */
        float ds = a[0] - pl.sky_s, dy = a[1] - pl.sky_y, l = sqrtf(ds * ds + dy * dy);
        if (l > 0.01f) { in.sx = ds / l * sky_screen_dir(pl.sky_s) * minf(1, l / 2); in.sy = dy / l * minf(1, l / 2); in.mag = minf(1, l / 2); }
        if (l < 1.0f || t > 600) { printf("BOT %d: flew to s=%.1f y=%.1f (%.1f %.1f) fish=%d f%d\n", stepno, a[0], a[1], pl.sky_s, pl.sky_y, pl.coins, g_frame); done(); }
        break;
    }
    case 'D': {   /* dash boots: drive to (X,Z) */
        float d = angle_diff(atan2f(a[0] - pl.pos.x, a[1] - pl.pos.z), pl.yaw), dist = v3distxz(pl.pos, v3(a[0], 0, a[1]));
        in.sx = clampf(-d * 2.5f, -1, 1); in.sy = fabsf(d) > 1.5f && dist < 10 ? -0.2f : 1; in.mag = 1;
        if (dist < 1.5f || t > 1500) { printf("BOT %d: drove to (%.1f %.1f) pos=(%.2f %.2f %.2f) f%d\n", stepno, a[0], a[1], pl.pos.x, pl.pos.y, pl.pos.z, g_frame); done(); }
        break;
    }
    case 'R': {   /* dash-boots race autopilot: steer at the next gate like a jet ski */
        Obj *g = sea_gate(g_gate_next % (g_gate_n > 0 ? g_gate_n : 1));
        static int last_state;
        if (t == 1) last_state = g_race_state;
        if (g && pl.act == ACT_SKATE) {
            vec3 tg = v3mad(g->pos, yaw_dir(g->yaw), 3.0f);
            float d = angle_diff(atan2f(tg.x - pl.pos.x, tg.z - pl.pos.z), pl.yaw);
            in.sx = clampf(-d * 2.5f, -1, 1);
            in.sy = 1;
            in.mag = 1;
            if (fabsf(d) > 1.1f && pl.fwd > 8) h |= BT_Z;
        } else if (pl.act == ACT_SWIM && t % 20 == 0) h |= BT_A;
        if (g_trace && t % 60 == 0)
        {
            Obj *sl = find(OT_SEAL);
            printf("R t%d pos=(%.1f %.1f %.1f) fwd=%.1f gate=%d lap=%d pow=%d race=%d act=%d seal=(%.1f %.1f) g%d l%d\n", t, pl.pos.x, pl.pos.y, pl.pos.z, pl.fwd, g_gate_next, g_lap, g_sea_power, g_race_state, pl.act,
                   sl ? sl->pos.x : 0, sl ? sl->pos.z : 0, sl ? sl->i[0] : 0, sl ? sl->i[1] : 0);
        }
        if ((g_race_state >= 2 && last_state == 1) || t > a[0]) {
            printf("BOT %d: rally end race=%d time=%d missed=%d lap=%d f%d\n", stepno, g_race_state, g_race_time, g_gate_missed, g_lap, g_frame);
            done();
        }
        last_state = g_race_state;
        break;
    }
    case 'o': {   /* STARDUST ORBIT: fly to a point */
        vec3 tg = v3(a[0], a[1], a[2]);
        space_steer(tg);
        if (a[3] > 0) h |= BT_A;
        if (v3dist(v3add(pl.pos, v3(0, 0.8f, 0)), tg) < 3.0f || t > 1800) {
            printf("BOT %d: flew to (%.0f %.0f %.0f) pos=(%.1f %.1f %.1f) f%d\n", stepno, a[0], a[1], a[2], pl.pos.x, pl.pos.y, pl.pos.z, g_frame);
            done();
        }
        break;
    }
    case 'y': {   /* CORE DUMP CAVERNS: bounce up the mushroom chimney, shelf by shelf (max N frames) */
        static int k;   /* the shelf of the last bounce */
        float base = cave_walk_y(11.0f);
        int nk;
        if (t == 1 || pl.vel.y > 22) k = (int)floorf((pl.pos.y - base + 1.0f) / 7.0f);
        nk = k + 1 > 5 ? 5 : k + 1;
        float r = (nk & 1) ? 25.0f : 27.4f, cr = (k & 1) ? 25.0f : 27.4f;
        vec3 next = cave_polar(11.0f, r, base + nk * 7), cur = cave_polar(11.0f, cr, base + k * 7);
        if (pl.vel.y > 2 || pl.pos.y > next.y + 0.5f) stick_to(next.x, next.z, 1.0f);
        else stick_to(cur.x, cur.z, 0.6f);
        if (t >= (int)a[0]) done();
        break;
    }
    case 'I': {   /* UPTIME HEIGHTS skydive: steer toward the next ring for up to N frames */
        int i;
        for (i = 0; i < nobj; i++)
            if (objs[i].active && objs[i].type == OT_RING && objs[i].param == g_ring_next) { stick_to(objs[i].pos.x, objs[i].pos.z, 1.0f); break; }
        if (t >= (int)a[0] || (t > 30 && !player_in_air())) done();
        break;
    }
    case 'U': {   /* STARDUST ORBIT: ring run autopilot */
        Obj *r = space_ring(g_ring_run_t > 0 ? g_ring_run_next : 0);
        static int started;
        if (t == 1) started = 0;
        if (g_ring_run_t > 0) started = 1;
        if (r && pl.act == ACT_SPACEFLY) {
            float err = space_steer(r->pos), dist = v3dist(r->pos, pl.pos);
            if (err < 0.2f && dist > 30) h |= BT_A;
            if (g_trace && t % 30 == 0) printf("U t%d next=%d left=%d dist=%.1f err=%.2f sp=%.1f pos=(%.1f %.1f %.1f) ring=(%.1f %.1f %.1f)\n", t, g_ring_run_next, g_ring_run_t, dist, err, pl.sp_speed, pl.pos.x, pl.pos.y, pl.pos.z, r->pos.x, r->pos.y, r->pos.z);
        }
        if ((started && g_ring_run_t == 0) || pl.act == ACT_STAR_DANCE || t > a[0]) {
            printf("BOT %d: ring run end next=%d left=%d act=%d f%d\n", stepno, g_ring_run_next, g_ring_run_t, pl.act, g_frame);
            done();
        }
        break;
    }
    case 'c': {   /* STARDUST ORBIT: chase the comet */
        Obj *o = find(OT_COMET);
        if (o && pl.act == ACT_SPACEFLY) {
            float d = v3dist(o->pos, pl.pos);
            space_steer(v3add(o->pos, v3scale(v3sub(o->pos, o->prev_pos), minf(d / 50.0f, 1.5f) * 60)));
            h |= BT_A;
        }
        if (!o || pl.act == ACT_STAR_DANCE || t > a[0]) {
            printf("BOT %d: comet chase end comet=%d act=%d f%d\n", stepno, o != NULL, pl.act, g_frame);
            done();
        }
        break;
    }
    case 'm': {   /* STARDUST ORBIT: smash the ice meteors */
        Obj *o = nearest(OT_METEOR);
        if (o && pl.act == ACT_SPACEFLY) {
            float err = space_steer(o->pos);
            if (err < 0.5f || v3dist(o->pos, pl.pos) > 40) h |= BT_A;
        }
        if (!o || t > a[0]) {
            printf("BOT %d: meteors end left=%d f%d\n", stepno, g_meteors_left, g_frame);
            done();
        }
        break;
    }
    case 'e': {   /* STARDUST ORBIT: collect the red fish */
        Obj *o = nearest(OT_REDCOIN);
        if (o && pl.act == ACT_SPACEFLY) {
            float err = space_steer(o->pos), dist = v3dist(o->pos, pl.pos);
            if (err < 0.3f && dist > 40) h |= BT_A;
            if (err > 1.2f && dist < 20) h |= BT_B;
        }
        if (!o || t > a[0]) {
            printf("BOT %d: red fish end red=%d f%d\n", stepno, pl.red_coins, g_frame);
            done();
        }
        break;
    }
    case 'd':
        if (dialog_active()) { if (t % 6 == 0) h |= BT_A; }
        else if (t > 3) done();
        break;
    case 'u':
        if (pl.act == (int)a[0] || t > (int)a[1]) {
            printf("BOT %d: wait act %d -> act=%d after %d frames\n", stepno, (int)a[0], pl.act, t);
            done();
        }
        break;
    case 'T':
        player_init(v3(a[0], a[1], a[2]), pl.yaw);
        done();
        break;
    case 'P':
        printf("BOT %d: f%d act=%d pos=(%.2f %.2f %.2f) hp=%d coins=%d stars=%d lv=%d rocket=%d fwd=%.1f red=%d\n", stepno, g_frame, pl.act,
               pl.pos.x, pl.pos.y, pl.pos.z, pl.health, pl.coins, total_stars(), cur_level, pl.rocket_t, pl.fwd, pl.red_coins);
        {
            int i;
            for (i = 0; i < nobj; i++) {
                Obj *o = &objs[i];
                if (!o->active) continue;
                if (o->type == OT_KING || o->type == OT_SEGFAULT || o->type == OT_BRUTE || o->type == OT_RACER || o->type == OT_STAR || o->type == OT_BABY || o->type == OT_MOTHER || o->type == OT_GHOSTKING || (o->type == OT_GHOST && v3dist(o->pos, pl.pos) < 12) || o->type == OT_WATCHDOG || o->type == OT_CAGE || o->type == OT_OOM || o->type == OT_SEAL || o->type == OT_WHALE ||
                    o->type == OT_NANOSHIP || o->type == OT_NANOFIRE || o->type == OT_NANOCATAPULT || o->type == OT_GIANTCAT || o->type == OT_TOASTER || o->type == OT_YARN)
                    printf("   obj type %d state %d hp %d pos=(%.2f %.2f %.2f) param %d\n", o->type, o->state, o->hp, o->pos.x, o->pos.y, o->pos.z, o->param);
            }
        }
        done();
        break;
    default: done(); break;
    }
    if (in.sy > 0.5f) h |= BT_UP;
    if (in.sy < -0.5f) h |= BT_DOWN;
    in.pressed = h & ~prev;
    in.released = ~h & prev;
    in.held = h;
    prev = h;
}
