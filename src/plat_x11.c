/* plat_x11.c - Linux backend: X11 + GLX, joystick API, ALSA (loaded at runtime) */
#include "platform.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <pthread.h>
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/joystick.h>

unsigned char plat_keys[K_COUNT];
Gamepad plat_pad;

static Display *dpy;
static Window win;
static GLXContext ctx;
static Atom wm_delete;
static int win_w = 640, win_h = 480;
static int is_fullscreen;
static int quit_req;

static int js_fd = -1;
static int js_naxes;
static double js_retry;
static float js_axis[8];
static unsigned js_btn;

static int map_key(KeySym k)
{
    switch (k) {
    case XK_Up: return K_UP; case XK_Down: return K_DOWN;
    case XK_Left: return K_LEFT; case XK_Right: return K_RIGHT;
    case XK_w: case XK_W: return K_W; case XK_a: case XK_A: return K_A;
    case XK_s: case XK_S: return K_S; case XK_d: case XK_D: return K_D;
    case XK_space: return K_SPACE;
    case XK_j: case XK_J: return K_J; case XK_k: case XK_K: return K_K;
    case XK_l: case XK_L: return K_L; case XK_i: case XK_I: return K_I;
    case XK_u: case XK_U: return K_U; case XK_o: case XK_O: return K_O;
    case XK_p: case XK_P: return K_P;
    case XK_z: case XK_Z: return K_Z; case XK_x: case XK_X: return K_X;
    case XK_c: case XK_C: return K_C; case XK_v: case XK_V: return K_V;
    case XK_q: case XK_Q: return K_Q; case XK_e: case XK_E: return K_E;
    case XK_r: case XK_R: return K_R; case XK_f: case XK_F: return K_F;
    case XK_Shift_L: return K_LSHIFT; case XK_Shift_R: return K_RSHIFT;
    case XK_Control_L: return K_LCTRL; case XK_Control_R: return K_RCTRL;
    case XK_Return: case XK_KP_Enter: return K_ENTER;
    case XK_Escape: return K_ESC; case XK_Tab: return K_TAB;
    case XK_F11: return K_F11; case XK_F12: return K_F12;
    case XK_BackSpace: return K_BACKSPACE;
    case XK_Alt_L: case XK_Alt_R: return K_ALT;
    case XK_Page_Up: return K_PGUP; case XK_Page_Down: return K_PGDN;
    case XK_m: case XK_M: return K_M;
    }
    return -1;
}

typedef int (*SwapIntervalEXT)(Display *, GLXDrawable, int);
typedef int (*SwapIntervalMESA)(unsigned);
typedef int (*SwapIntervalSGI)(int);

static void set_vsync(void)
{
    const char *ext = glXQueryExtensionsString(dpy, DefaultScreen(dpy));
    if (ext && strstr(ext, "GLX_EXT_swap_control")) {
        SwapIntervalEXT f = (SwapIntervalEXT)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalEXT");
        if (f) { f(dpy, win, 1); return; }
    }
    if (ext && strstr(ext, "GLX_MESA_swap_control")) {
        SwapIntervalMESA f = (SwapIntervalMESA)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalMESA");
        if (f) { f(1); return; }
    }
    if (ext && strstr(ext, "GLX_SGI_swap_control")) {
        SwapIntervalSGI f = (SwapIntervalSGI)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalSGI");
        if (f) f(1);
    }
}

int plat_init(const char *title, int w, int h)
{
    int attr[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8,
                   GLX_BLUE_SIZE, 8, GLX_DEPTH_SIZE, 24, None };
    int attr16[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 16, None };
    XVisualInfo *vi;
    XSetWindowAttributes swa;
    Window root;

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Cannot open X display\n"); return 0; }
    vi = glXChooseVisual(dpy, DefaultScreen(dpy), attr);
    if (!vi) vi = glXChooseVisual(dpy, DefaultScreen(dpy), attr16);
    if (!vi) { fprintf(stderr, "No suitable GLX visual\n"); return 0; }
    root = RootWindow(dpy, vi->screen);
    memset(&swa, 0, sizeof swa);
    swa.colormap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    swa.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask | FocusChangeMask;
    win = XCreateWindow(dpy, root, 0, 0, w, h, 0, vi->depth, InputOutput, vi->visual,
                        CWColormap | CWEventMask, &swa);
    win_w = w; win_h = h;
    XStoreName(dpy, win, title);
    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    XMapWindow(dpy, win);
    ctx = glXCreateContext(dpy, vi, NULL, True);
    if (!ctx) { fprintf(stderr, "glXCreateContext failed\n"); return 0; }
    glXMakeCurrent(dpy, win, ctx);
    XkbSetDetectableAutoRepeat(dpy, True, NULL);
    XFree(vi);
    set_vsync();
    return 1;
}

void plat_set_title(const char *t) { if (dpy) XStoreName(dpy, win, t); }

int plat_system_lang_ja(void)
{
    static const char *vars[3] = { "LC_ALL", "LC_MESSAGES", "LANG" };
    int i;
    for (i = 0; i < 3; i++) {
        const char *v = getenv(vars[i]);
        if (v && *v) return strncmp(v, "ja", 2) == 0;
    }
    return 0;
}

void plat_shutdown(void)
{
    plat_audio_shutdown();
    if (js_fd >= 0) close(js_fd);
    if (dpy) {
        glXMakeCurrent(dpy, None, NULL);
        if (ctx) glXDestroyContext(dpy, ctx);
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
        dpy = NULL;
    }
}

static void poll_joystick(void)
{
    struct js_event e;
    if (js_fd < 0) {
        double t = plat_time();
        if (t < js_retry) return;
        js_retry = t + 2.0;
        js_fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
        if (js_fd < 0) { plat_pad.connected = 0; return; }
        {
            char n = 0;
            ioctl(js_fd, JSIOCGAXES, &n);
            js_naxes = n;
        }
        memset(js_axis, 0, sizeof js_axis);
        js_btn = 0;
    }
    for (;;) {
        ssize_t r = read(js_fd, &e, sizeof e);
        if (r != (ssize_t)sizeof e) {
            if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
            if (r <= 0) { close(js_fd); js_fd = -1; plat_pad.connected = 0; return; }
            break;
        }
        e.type &= ~JS_EVENT_INIT;
        if (e.type == JS_EVENT_AXIS && e.number < 8) {
            js_axis[e.number] = e.value / 32767.0f;
        } else if (e.type == JS_EVENT_BUTTON) {
            static const unsigned map[11] = { GP_A, GP_B, GP_X, GP_Y, GP_LB, GP_RB, GP_BACK, GP_START, 0, 0, 0 };
            if (e.number < 11 && map[e.number]) {
                if (e.value) js_btn |= map[e.number]; else js_btn &= ~map[e.number];
            }
        }
    }
    plat_pad.connected = 1;
    plat_pad.lx = js_axis[0];
    plat_pad.ly = -js_axis[1];
    plat_pad.btn = js_btn;
    if (js_naxes >= 6) {
        /* xbox-like layout: 0 lx, 1 ly, 2 lt, 3 rx, 4 ry, 5 rt, 6/7 dpad hat */
        plat_pad.rx = js_axis[3];
        plat_pad.ry = -js_axis[4];
        if (js_axis[2] > 0.2f) plat_pad.btn |= GP_LT;
        if (js_axis[5] > 0.2f) plat_pad.btn |= GP_RT;
        if (js_naxes >= 8) {
            if (js_axis[6] < -0.5f) plat_pad.btn |= GP_DLEFT;
            if (js_axis[6] > 0.5f) plat_pad.btn |= GP_DRIGHT;
            if (js_axis[7] < -0.5f) plat_pad.btn |= GP_DUP;
            if (js_axis[7] > 0.5f) plat_pad.btn |= GP_DDOWN;
        }
    } else if (js_naxes >= 4) {
        plat_pad.rx = js_axis[2];
        plat_pad.ry = -js_axis[3];
    }
}

int plat_pump(void)
{
    while (XPending(dpy)) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        switch (ev.type) {
        case KeyPress:
        case KeyRelease: {
            int k = map_key(XLookupKeysym(&ev.xkey, 0));
            if (k >= 0) plat_keys[k] = (ev.type == KeyPress);
            break;
        }
        case ConfigureNotify:
            win_w = ev.xconfigure.width;
            win_h = ev.xconfigure.height;
            break;
        case FocusOut:
            memset(plat_keys, 0, sizeof plat_keys);
            break;
        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == wm_delete) quit_req = 1;
            break;
        }
    }
    poll_joystick();
    return !quit_req;
}

void plat_swap(void) { glXSwapBuffers(dpy, win); }
void plat_size(int *w, int *h) { *w = win_w; *h = win_h; }

double plat_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void plat_sleep_ms(int ms)
{
    struct timespec ts;
    if (ms <= 0) return;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void plat_toggle_fullscreen(void)
{
    XEvent xev;
    Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom fs = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    is_fullscreen = !is_fullscreen;
    memset(&xev, 0, sizeof xev);
    xev.type = ClientMessage;
    xev.xclient.window = win;
    xev.xclient.message_type = wm_state;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = is_fullscreen ? 1 : 0;
    xev.xclient.data.l[1] = (long)fs;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 1;
    XSendEvent(dpy, DefaultRootWindow(dpy), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XFlush(dpy);
}

/* ---------------- audio (ALSA via dlopen so the game runs without it) ---------------- */
typedef struct _snd_pcm snd_pcm_t;
static int  (*a_open)(snd_pcm_t **, const char *, int, int);
static int  (*a_set_params)(snd_pcm_t *, int, int, unsigned, unsigned, int, unsigned);
static long (*a_writei)(snd_pcm_t *, const void *, unsigned long);
static int  (*a_recover)(snd_pcm_t *, int, int);
static int  (*a_close)(snd_pcm_t *);
static int  (*a_drop)(snd_pcm_t *);
static void *alsa_lib;
static snd_pcm_t *pcm;
static pthread_t audio_thread;
static volatile int audio_run;
static AudioCallback audio_cb;
static pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *audio_main(void *arg)
{
    short buf[512 * 2];
    (void)arg;
    while (audio_run) {
        long r;
        audio_cb(buf, 512);
        r = a_writei(pcm, buf, 512);
        if (r < 0) a_recover(pcm, (int)r, 1);
    }
    return NULL;
}

int plat_audio_init(int rate, AudioCallback cb)
{
    alsa_lib = dlopen("libasound.so.2", RTLD_NOW);
    if (!alsa_lib) alsa_lib = dlopen("libasound.so", RTLD_NOW);
    if (!alsa_lib) { fprintf(stderr, "ALSA not available - no sound\n"); return 0; }
    a_open = (int (*)(snd_pcm_t **, const char *, int, int))dlsym(alsa_lib, "snd_pcm_open");
    a_set_params = (int (*)(snd_pcm_t *, int, int, unsigned, unsigned, int, unsigned))dlsym(alsa_lib, "snd_pcm_set_params");
    a_writei = (long (*)(snd_pcm_t *, const void *, unsigned long))dlsym(alsa_lib, "snd_pcm_writei");
    a_recover = (int (*)(snd_pcm_t *, int, int))dlsym(alsa_lib, "snd_pcm_recover");
    a_close = (int (*)(snd_pcm_t *))dlsym(alsa_lib, "snd_pcm_close");
    a_drop = (int (*)(snd_pcm_t *))dlsym(alsa_lib, "snd_pcm_drop");
    if (!a_open || !a_set_params || !a_writei || !a_recover || !a_close) return 0;
    if (a_open(&pcm, "default", 0 /*PLAYBACK*/, 0) < 0) { pcm = NULL; fprintf(stderr, "No audio device\n"); return 0; }
    /* format S16_LE = 2, access RW_INTERLEAVED = 3 */
    if (a_set_params(pcm, 2, 3, 2, (unsigned)rate, 1, 60000) < 0) {
        a_close(pcm); pcm = NULL; return 0;
    }
    audio_cb = cb;
    audio_run = 1;
    if (pthread_create(&audio_thread, NULL, audio_main, NULL) != 0) {
        audio_run = 0; a_close(pcm); pcm = NULL; return 0;
    }
    return 1;
}

void plat_audio_shutdown(void)
{
    if (audio_run) {
        audio_run = 0;
        pthread_join(audio_thread, NULL);
    }
    if (pcm) {
        if (a_drop) a_drop(pcm);
        a_close(pcm);
        pcm = NULL;
    }
}

void plat_lock(void) { pthread_mutex_lock(&audio_mutex); }
void plat_unlock(void) { pthread_mutex_unlock(&audio_mutex); }
