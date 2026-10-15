/* main.c - TUX 64 entry point and fixed-timestep main loop */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void game_init(void);
void game_update(void);
void game_render(void);
void game_debug_start(int level, int spawn, int slot);
void game_debug_state(int st);
void input_set_script(const char *s);
void bot_set(const char *s);
void bot_update(void);
extern int g_nosave;
extern int g_trace;

static void write_bmp(const char *name)
{
    int w = g_scr_w, h = g_scr_h, y, row = (w * 3 + 3) & ~3;
    unsigned char *px = (unsigned char *)malloc((size_t)w * h * 3);
    unsigned char hdr[54];
    FILE *f;
    if (!px) return;
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px);
    f = fopen(name, "wb");
    if (!f) { free(px); return; }
    memset(hdr, 0, sizeof hdr);
    hdr[0] = 'B'; hdr[1] = 'M';
    {
        unsigned size = 54 + (unsigned)row * h;
        hdr[2] = size & 255; hdr[3] = (size >> 8) & 255; hdr[4] = (size >> 16) & 255; hdr[5] = (size >> 24) & 255;
    }
    hdr[10] = 54; hdr[14] = 40;
    hdr[18] = w & 255; hdr[19] = (w >> 8) & 255;
    hdr[22] = h & 255; hdr[23] = (h >> 8) & 255;
    hdr[26] = 1; hdr[28] = 24;
    fwrite(hdr, 1, 54, f);
    for (y = 0; y < h; y++) {
        unsigned char line[3 * 4096 + 4];
        int x;
        memset(line, 0, sizeof line);
        for (x = 0; x < w && x < 4096; x++) {
            line[x * 3 + 0] = px[(y * w + x) * 3 + 2];
            line[x * 3 + 1] = px[(y * w + x) * 3 + 1];
            line[x * 3 + 2] = px[(y * w + x) * 3 + 0];
        }
        fwrite(line, 1, (size_t)row, f);
    }
    fclose(f);
    free(px);
}

int main(int argc, char **argv)
{
    double acc = 0, last;
    int dbg_state = -1;
    int i, dbg_level = -1, dbg_spawn = 0, shot_frame = -1, frames = 0, quit_frame = -1;
    int prev_f11 = 0, prev_f12 = 0, prev_enter_alt = 0, shot_n = 0, prev_m = 0, music_on = 1;
    const char *shot_name = "shot.bmp";
    int win_w = 960, win_h = 720, fullscreen = 0, mute = 0, setpos = 0, fast = 0;
    float px = 0, py = 0, pz = 0, pyaw = 0;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--level") && i + 1 < argc) dbg_level = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--spawn") && i + 1 < argc) dbg_spawn = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 2 < argc) { shot_frame = atoi(argv[++i]); shot_name = argv[++i]; }
        else if (!strcmp(argv[i], "--quit") && i + 1 < argc) quit_frame = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--script") && i + 1 < argc) input_set_script(argv[++i]);
        else if (!strcmp(argv[i], "--nosave")) g_nosave = 1;
        else if (!strcmp(argv[i], "--bot") && i + 1 < argc) bot_set(argv[++i]);
        else if (!strcmp(argv[i], "--pos") && i + 4 < argc) {
            setpos = 1; px = (float)atof(argv[++i]); py = (float)atof(argv[++i]); pz = (float)atof(argv[++i]); pyaw = (float)atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "--trace")) { g_trace = 30; if (i + 1 < argc && argv[i + 1][0] != '-') g_trace = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--mute")) mute = 1;
        else if (!strcmp(argv[i], "--fast")) fast = 1;
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) g_maze_seed = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--state") && i + 1 < argc) dbg_state = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fullscreen")) fullscreen = 1;
        else if (!strcmp(argv[i], "--size") && i + 2 < argc) { win_w = atoi(argv[++i]); win_h = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--wav") && i + 3 < argc) {
            int id = atoi(argv[i + 1]);
            float secs = (float)atof(argv[i + 2]);
            audio_dump_wav(id, secs, argv[i + 3]);
            return 0;
        }
        else if (!strcmp(argv[i], "--help")) {
            printf("TUX 64\n  --fullscreen  --size W H  --mute\n");
            return 0;
        }
    }
    if (!plat_init("TUX 64 - The Kernel Castle Adventure", win_w, win_h)) {
        fprintf(stderr, "Failed to initialise window / OpenGL\n");
        return 1;
    }
    if (fullscreen) plat_toggle_fullscreen();
    gfx_init();
    models_init();
    if (!mute) audio_init();
    save_load();
    lang_load();
    game_init();
    if (dbg_state >= 0) game_debug_state(dbg_state);
    if (dbg_level >= 0) {
        game_debug_start(dbg_level, dbg_spawn, 2);
        if (setpos) { player_init(v3(px, py, pz), RAD(pyaw)); camera_reset(); }
    }
    last = plat_time();
    while (plat_pump()) {
        double now = plat_time();
        int steps = 0;
        acc += now - last;
        last = now;
        if (acc > 0.2) acc = 0.2;
        if (shot_frame >= 0 || quit_frame >= 0) acc = DT;   /* deterministic when testing */
        if (fast) acc = DT * 8;
        while (acc >= DT - 1e-6 && steps < (fast ? 8 : 6)) {
            input_update();
            bot_update();
            game_update();
            acc -= DT;
            steps++;
            frames++;
            if (g_trace && frames % g_trace == 0)
                printf("f%d act=%d pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) fwd=%.2f hp=%d coins=%d lv=%d wn=(%.2f %.2f) fl=%.2f\n", frames, pl.act,
                       pl.pos.x, pl.pos.y, pl.pos.z, pl.vel.x, pl.vel.y, pl.vel.z, pl.fwd, pl.health, pl.coins, cur_level, pl.wall_n.x, pl.wall_n.z, pl.floor_y);
            if (g_trace) printf("   cam=(%.2f %.2f %.2f) at=(%.2f %.2f %.2f)\n", cam.pos.x, cam.pos.y, cam.pos.z, cam.at.x, cam.at.y, cam.at.z);
        }
        plat_size(&g_scr_w, &g_scr_h);
        game_render();
        if (shot_frame >= 0 && frames >= shot_frame) {
            glFinish();
            write_bmp(shot_name);
            shot_frame = -1;
            if (quit_frame < 0) break;
        }
        plat_swap();
        if (quit_frame >= 0 && frames >= quit_frame) break;
        if (plat_keys[K_F11] && !prev_f11) plat_toggle_fullscreen();
        prev_f11 = plat_keys[K_F11];
        if (plat_keys[K_ALT] && plat_keys[K_ENTER] && !prev_enter_alt) plat_toggle_fullscreen();
        prev_enter_alt = plat_keys[K_ALT] && plat_keys[K_ENTER];
        if (plat_keys[K_F12] && !prev_f12) {
            char name[64];
            snprintf(name, sizeof name, "screenshot_%03d.bmp", shot_n++);
            write_bmp(name);
        }
        prev_f12 = plat_keys[K_F12];
        if (plat_keys[K_M] && !prev_m) { music_on = !music_on; audio_volume(music_on ? 0.55f : 0.0f, 0.8f); }
        prev_m = plat_keys[K_M];
        if (steps == 0) plat_sleep_ms(1);
    }
    audio_shutdown();
    plat_shutdown();
    return 0;
}
