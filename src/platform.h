/* platform.h - thin OS abstraction (window + GL context, input, timer, audio)
 * Implementations: plat_x11.c (Linux), plat_win32.c (Windows / MinGW) */
#ifndef PLATFORM_H
#define PLATFORM_H

/* glibc 2.38 added a new fmodf symbol version; bind to the old one so a Linux build
 * also runs on older distributions (Debian 12, Ubuntu 22.04, RHEL 9 ...) */
#if defined(__linux__) && defined(__x86_64__)
__asm__(".symver fmodf,fmodf@GLIBC_2.2.5");
#endif

enum {
    K_UP, K_DOWN, K_LEFT, K_RIGHT,
    K_W, K_A, K_S, K_D,
    K_SPACE, K_J, K_K, K_L, K_I, K_U, K_O, K_P,
    K_Z, K_X, K_C, K_V, K_Q, K_E, K_R, K_F,
    K_LSHIFT, K_RSHIFT, K_LCTRL, K_RCTRL,
    K_ENTER, K_ESC, K_TAB, K_F11, K_F12, K_BACKSPACE, K_ALT,
    K_PGUP, K_PGDN, K_M,
    K_COUNT
};

enum {
    GP_A = 1, GP_B = 2, GP_X = 4, GP_Y = 8,
    GP_LB = 16, GP_RB = 32, GP_BACK = 64, GP_START = 128,
    GP_DUP = 256, GP_DDOWN = 512, GP_DLEFT = 1024, GP_DRIGHT = 2048,
    GP_LT = 4096, GP_RT = 8192
};

typedef struct {
    int connected;
    float lx, ly;   /* left stick, -1..1, +y = up/forward */
    float rx, ry;   /* right stick */
    unsigned btn;   /* GP_* bitmask */
} Gamepad;

extern unsigned char plat_keys[K_COUNT];
extern Gamepad plat_pad;

int    plat_init(const char *title, int w, int h);
void   plat_shutdown(void);
int    plat_pump(void);            /* handle OS events; returns 0 when quit was requested */
void   plat_swap(void);
void   plat_size(int *w, int *h);
double plat_time(void);            /* seconds, monotonic */
void   plat_sleep_ms(int ms);
void   plat_toggle_fullscreen(void);
void   plat_set_title(const char *t);
int    plat_system_lang_ja(void);     /* 1 if the OS user interface language is Japanese */
const char *plat_user_file(const char *name);   /* where to keep the save / config file */

typedef void (*AudioCallback)(short *out, int frames);  /* stereo interleaved S16 */
int    plat_audio_init(int rate, AudioCallback cb);
void   plat_audio_shutdown(void);
void   plat_lock(void);
void   plat_unlock(void);

#endif
