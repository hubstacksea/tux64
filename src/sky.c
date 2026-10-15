/* sky.c - FROSTCLOUD SKYWAY: free rocket flight along a looping sky path (NiGHTS-style 2.5D)
 *
 * Tux flies inside a vertical "ribbon" that follows a closed loop in the sky. The stick moves him
 * freely in that plane (the camera watches from the side), A dashes, and drawing a closed loop with
 * his trail (a "paraloop") scoops up every fish inside it. There is a time limit; each fish line
 * reached in a single flight earns its star once (fish do not carry over between flights). */
#include "game.h"
#include <string.h>
#include <stdio.h>

#define SKY_N     720
#define SKY_R     70.0f
#define SKY_YMIN  -13.0f
#define SKY_YMAX  15.0f
#define SKY_TIME  (120 * 60)
#define TRAIL_N   180

const int g_sky_lines[SKY_LINES] = { 60, 120, 200, 300 };
int g_sky_time, g_sky_state, g_sky_lines_hit;

static vec3 path_p[SKY_N + 1];
static float path_s[SKY_N + 1], path_len;
static int path_ok;

static float trail_s[TRAIL_N], trail_y[TRAIL_N];
static int trail_n, trail_tick;
static int result_t;
static const char *dialog_pending_text;

/* ---------------------------------------------------------------- the loop */
static vec3 raw_point(float th)
{
    float r = SKY_R + 12.0f * sinf(3 * th) + 5.0f * sinf(5 * th + 1.0f);
    return v3(sinf(th) * r, 34.0f + 7.0f * sinf(2 * th + 0.5f), cosf(th) * r);
}

static void build_path(void)
{
    int i;
    path_s[0] = 0;
    for (i = 0; i <= SKY_N; i++) {
        path_p[i] = raw_point(TAU_F * i / SKY_N);
        if (i > 0) path_s[i] = path_s[i - 1] + v3dist(path_p[i], path_p[i - 1]);
    }
    path_len = path_s[SKY_N];
    path_ok = 1;
}

float sky_len(void) { if (!path_ok) build_path(); return path_len; }

static float wrap_s(float s)
{
    float L = sky_len();
    s = fmodf(s, L);
    if (s < 0) s += L;
    return s;
}

/* point on the path centre line at arc length s (y = the ribbon's middle height) */
vec3 sky_point(float s)
{
    int lo = 0, hi = SKY_N;
    float t;
    s = wrap_s(s);
    while (hi - lo > 1) {
        int m = (lo + hi) / 2;
        if (path_s[m] <= s) lo = m; else hi = m;
    }
    t = (s - path_s[lo]) / maxf(path_s[hi] - path_s[lo], 1e-4f);
    return v3lerp(path_p[lo], path_p[hi], t);
}

/* horizontal unit tangent (direction of increasing s) */
vec3 sky_tangent(float s)
{
    vec3 d = v3sub(sky_point(s + 0.5f), sky_point(s - 0.5f));
    d.y = 0;
    return v3norm(d);
}

/* horizontal normal pointing out of the loop (the camera's side) */
vec3 sky_normal(float s)
{
    vec3 t = sky_tangent(s), n = v3(t.z, 0, -t.x), p = sky_point(s);
    if (n.x * p.x + n.z * p.z < 0) n = v3scale(n, -1);
    return n;
}

vec3 sky_world(float s, float y) { vec3 p = sky_point(s); p.y += y; return p; }

/* +1 if increasing s moves to the right of the screen (camera outside the loop looking in) */
float sky_screen_dir(float s)
{
    vec3 n = sky_normal(s), right = v3(n.z, 0, -n.x), t = sky_tangent(s);
    return v3dot(t, right) >= 0 ? 1.0f : -1.0f;
}

/* ---------------------------------------------------------------- level start */
void sky_reset(void)
{
    sky_len();
    g_sky_time = SKY_TIME;
    g_sky_state = 0;
    g_sky_lines_hit = 0;
    trail_n = 0;
    trail_tick = 0;
    result_t = 0;
}

/* ---------------------------------------------------------------- paraloop */
static int seg_hit(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy, float *ix, float *iy)
{
    float r1 = bx - ax, r2 = by - ay, q1 = dx - cx, q2 = dy - cy;
    float den = r1 * q2 - r2 * q1, t, u;
    if (fabsf(den) < 1e-6f) return 0;
    t = ((cx - ax) * q2 - (cy - ay) * q1) / den;
    u = ((cx - ax) * r2 - (cy - ay) * r1) / den;
    if (t < 0 || t > 1 || u < 0 || u > 1) return 0;
    *ix = ax + r1 * t; *iy = ay + r2 * t;
    return 1;
}

static int point_in_poly(const float *px, const float *py, int n, float x, float y)
{
    int i, j, in = 0;
    for (i = 0, j = n - 1; i < n; j = i++)
        if (((py[i] > y) != (py[j] > y)) && (x < (px[j] - px[i]) * (y - py[i]) / (py[j] - py[i]) + px[i])) in = !in;
    return in;
}

static void paraloop_check(void)
{
    int n = trail_n, j;
    float ax, ay, bx, by;
    if (n < 6) return;
    ax = trail_s[n - 2]; ay = trail_y[n - 2];
    bx = trail_s[n - 1]; by = trail_y[n - 1];
    for (j = 0; j < n - 4; j++) {
        float ix, iy;
        if (seg_hit(ax, ay, bx, by, trail_s[j], trail_y[j], trail_s[j + 1], trail_y[j + 1], &ix, &iy)) {
            float px[TRAIL_N + 1], py[TRAIL_N + 1], area = 0, cs = 0, L = sky_len();
            int m = 0, k, got = 0;
            px[m] = ix; py[m] = iy; m++;
            for (k = j + 1; k < n - 1; k++) { px[m] = trail_s[k]; py[m] = trail_y[k]; m++; }
            for (k = 0; k < m; k++) {
                int k2 = (k + 1) % m;
                area += px[k] * py[k2] - px[k2] * py[k];
                cs += px[k];
            }
            area = fabsf(area) * 0.5f;
            cs /= m;
            trail_n = 0;
            if (area < 6.0f) return;
            /* everything inside the loop is pulled in */
            for (k = 0; k < nobj; k++) {
                Obj *o = &objs[k];
                float s;
                if (!o->active || o->type != OT_SKYCOIN || o->state != 0) continue;
                s = o->f[6] + L * floorf((cs - o->f[6]) / L + 0.5f);
                if (point_in_poly(px, py, m, s, o->f[7])) {
                    o->state = 2; o->timer = 0;      /* fly to Tux */
                    got++;
                }
            }
            sfx(SFX_WARP);
            fx_spawn(OT_FX_SPARKLE, pl.pos, 2.0f);
            if (got > 0) {
                static char msg[64];
                snprintf(msg, sizeof msg, tr("PARALOOP! +%d"), got);
                g_hud_msg = msg; g_hud_msg_t = 60;
            }
            return;
        }
    }
}

void sky_trail_push(float s, float y)
{
    if (++trail_tick < 2) return;
    trail_tick = 0;
    if (trail_n >= TRAIL_N) {
        memmove(trail_s, trail_s + 1, sizeof(float) * (TRAIL_N - 1));
        memmove(trail_y, trail_y + 1, sizeof(float) * (TRAIL_N - 1));
        trail_n--;
    }
    trail_s[trail_n] = s; trail_y[trail_n] = y; trail_n++;
    paraloop_check();
}

/* ---------------------------------------------------------------- timer and results */
static void finish_flight(void)
{
    static char msg[1024];
    int i, got = 0, c = lv.course;
    char *p = msg;
    size_t left = sizeof msg;
    int n;
    g_sky_state = 1;
    result_t = 0;
    pl.no_ctrl = 1;
    n = snprintf(p, left, tr("TIME UP!  You collected %d fish."), pl.coins); p += n; left -= (size_t)n;
    for (i = 0; i < SKY_LINES; i++) {
        if (pl.coins < g_sky_lines[i]) continue;
        if (save[cur_slot].stars[c] & (1 << i)) continue;     /* already owned: nothing new */
        save[cur_slot].stars[c] |= (unsigned char)(1 << i);
        n = snprintf(p, left, "\n%s \"%s\"", got == 0 ? tr("New Kernel Star!") : "             ", tr(courses[c].star_names[i]));
        if (n > 0 && (size_t)n < left) { p += n; left -= (size_t)n; }
        got++;
    }
    if (got == 0) {
        int next = -1;
        for (i = 0; i < SKY_LINES; i++) if (!(save[cur_slot].stars[c] & (1 << i))) { next = i; break; }
        if (next >= 0) snprintf(p, left, tr("\nNo new star this time. Next line: %d fish."), g_sky_lines[next]);
        else snprintf(p, left, "%s", tr("\nYou already own every star of the skyway!"));
    }
    save[cur_slot].lives = (unsigned char)(pl.lives > 99 ? 99 : pl.lives);
    save_write();
    music_play(got ? MUS_STARGET : MUS_GAMEOVER);
    if (got) { fx_spawn(OT_FX_SPARKLE, pl.pos, 3.0f); sfx(SFX_STAR_APPEAR); }
    g_hud_msg = tr("TIME UP!"); g_hud_msg_t = 90;
    result_t = 0;
    dialog_pending_text = msg;
}

void sky_update(void)
{
    int i;
    if (cur_level != LV_SKY) return;
    if (g_sky_state == 0) {
        if (g_sky_time > 0 && !paused) g_sky_time--;
        if (g_sky_time % 60 == 0 && g_sky_time > 0 && g_sky_time <= 10 * 60) sfx(SFX_MENU_MOVE);
        /* announce each fish line the first time this flight reaches it */
        for (i = 0; i < SKY_LINES; i++)
            if (!(g_sky_lines_hit & (1 << i)) && pl.coins >= g_sky_lines[i]) {
                static char msg[96];
                g_sky_lines_hit |= 1 << i;
                if (has_star(lv.course, i)) snprintf(msg, sizeof msg, tr("%d FISH! (star already owned)"), g_sky_lines[i]);
                else snprintf(msg, sizeof msg, tr("%d FISH! A star is waiting at the finish!"), g_sky_lines[i]);
                g_hud_msg = msg; g_hud_msg_t = 120;
                sfx(SFX_1UP);
            }
        if (g_sky_time == 0) finish_flight();
        return;
    }
    /* results: show the summary, then back to the castle */
    result_t++;
    if (result_t == 40 && dialog_pending_text) { dialog_open(dialog_pending_text); dialog_pending_text = NULL; }
    if (result_t > 42 && !dialog_active()) course_exit();
}

/* ---------------------------------------------------------------- objects */
void sky_obj_update(Obj *o)
{
    o->t += DT;
    if (o->type == OT_SKYCOIN) {
        if (o->state == 1) {       /* collected: comes back a while later */
            if (++o->timer > 30 * 60 && v3dist(o->pos, pl.pos) > 12) { o->state = 0; o->timer = 0; }
            return;
        }
        if (o->state == 2) {       /* pulled in by a paraloop */
            vec3 d = v3sub(v3add(pl.pos, v3(0, 0.8f, 0)), o->pos);
            float l = v3len(d);
            o->timer++;
            if (l < 1.0f || o->timer > 40) {
                o->state = 1; o->timer = 0;
                game_add_coins(1, o->pos);
                o->pos = o->home;
                return;
            }
            o->pos = v3mad(o->pos, d, minf(1.0f, (8 + o->timer * 1.5f) * DT / maxf(l, 0.1f)));
            return;
        }
        if (g_sky_state == 0 && pl.act == ACT_SKYFLY && v3dist(o->pos, v3add(pl.pos, v3(0, 0.8f, 0))) < 1.4f) {
            o->state = 1; o->timer = 0;
            game_add_coins(1, o->pos);
            fx_spawn(OT_FX_SPARKLE, o->pos, 0.6f);
        }
        return;
    }
    if (o->type == OT_SKYRING) {   /* fly through: dash gauge refilled */
        if (o->timer > 0) o->timer--;
        if (o->timer == 0 && pl.act == ACT_SKYFLY && v3dist(o->pos, v3add(pl.pos, v3(0, 0.8f, 0))) < 2.0f) {
            o->timer = 90;
            pl.sky_dash = 1.0f;
            game_add_coins(1, o->pos);
            sfx(SFX_BLUECOIN);
            fx_spawn(OT_FX_SPARKLE, o->pos, 1.5f);
        }
    }
}

void sky_obj_draw(Obj *o)
{
    if (o->type == OT_SKYCOIN) {
        if (o->state == 1) return;
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y + sinf(o->t * 2.5f + o->home.x) * 0.1f, o->pos.z);
        glRotatef(o->t * 160 + o->home.z * 20, 0, 1, 0);
        glScalef(1.15f, 1.15f, 1.15f);
        glCallList(mdl[MDL_COIN]);
        glPopMatrix();
        return;
    }
    if (o->type == OT_SKYRING) {
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glRotatef(90, 1, 0, 0);
        glScalef(1.4f, 1.4f, 1.4f);
        if (o->timer > 0) glColor3f(0.5f, 0.5f, 0.5f);
        glCallList(mdl[MDL_RING]);
        glPopMatrix();
    }
}

/* the glowing trail behind Tux (shows the loop being drawn) */
void sky_draw_alpha(void)
{
    int i;
    if (cur_level != LV_SKY || trail_n < 2) return;
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i < trail_n; i++) {
        float a = (float)i / trail_n;
        vec3 p = sky_world(trail_s[i], trail_y[i] + 0.8f);
        glColor4f(0.4f + 0.5f * a, 0.8f, 1.0f, 0.55f * a);
        glVertex3f(p.x, p.y - 0.25f, p.z);
        glVertex3f(p.x, p.y + 0.25f, p.z);
    }
    glEnd();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
}

/* side-view camera: outside the loop, level with Tux */
void sky_camera(void)
{
    static float cy;
    vec3 n = sky_normal(pl.sky_s), want, at;
    float lead = clampf(pl.sky_vs, -20, 20) * 0.25f;
    vec3 t = sky_tangent(pl.sky_s);
    if (cam.mode != CAM_FOLLOW) return;
    cy += (pl.pos.y - cy) * minf(1.0f, 4.0f * DT);
    if (fabsf(pl.pos.y - cy) > 20) cy = pl.pos.y;
    at = v3add(v3mad(pl.pos, t, lead), v3(0, 1.0f, 0));
    at.y = cy + 1.0f;
    want = v3add(v3mad(at, n, 17.0f), v3(0, 2.5f, 0));
    cam.pos = v3lerp(cam.pos, want, v3dist(cam.pos, want) > 40 ? 1.0f : minf(1.0f, 5.0f * DT));
    cam.at = v3lerp(cam.at, at, minf(1.0f, 8.0f * DT));
    cam.yaw = yaw_to(cam.pos, pl.pos);
}

int sky_ymin_ok(float y) { return y >= SKY_YMIN && y <= SKY_YMAX; }
float sky_clamp_y(float y) { return clampf(y, SKY_YMIN, SKY_YMAX); }
