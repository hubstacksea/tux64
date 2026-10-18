/* audio.c - everything is synthesized at startup / in real time; no asset files */
#include "audio.h"
#include "platform.h"
#include "mathx.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define RATE 44100

/* ======================================================================
 * Sound effect generation
 * ====================================================================== */
typedef struct { float *data; int len; } Sample;
static Sample samples[SFX_COUNT];

enum { W_SQUARE, W_SQ25, W_SQ12, W_TRI, W_SINE, W_SAW, W_NOISE };

static unsigned noise_state = 0x9e3779b9u;
static float noisef(void)
{
    noise_state ^= noise_state << 13;
    noise_state ^= noise_state >> 17;
    noise_state ^= noise_state << 5;
    return (noise_state & 0xFFFF) / 32768.0f - 1.0f;
}

static float wavef(int w, float ph)
{
    ph -= floorf(ph);
    switch (w) {
    case W_SQUARE: return ph < 0.5f ? 0.8f : -0.8f;
    case W_SQ25: return ph < 0.25f ? 0.8f : -0.8f;
    case W_SQ12: return ph < 0.125f ? 0.8f : -0.8f;
    case W_TRI: return ph < 0.5f ? ph * 4 - 1 : 3 - ph * 4;
    case W_SINE: return sinf(ph * TAU_F);
    case W_SAW: return ph * 2 - 1;
    default: return noisef();
    }
}

static Sample *cur_s;
static void s_alloc(int id, float secs)
{
    cur_s = &samples[id];
    cur_s->len = (int)(secs * RATE);
    cur_s->data = (float *)calloc((size_t)cur_s->len, sizeof(float));
}

/* add a tone: start/dur seconds, freq sweep f0->f1 (exponential), simple AD envelope */
static void tone(float start, float dur, float f0, float f1, int wave, float vol, float attack, float vib)
{
    int i0 = (int)(start * RATE), n = (int)(dur * RATE), i;
    float ph = 0, lp = 0;
    for (i = 0; i < n && i0 + i < cur_s->len; i++) {
        float t = (float)i / n;
        float f = f0 * powf(f1 / f0, t);
        float env, s;
        if (vib > 0) f *= 1.0f + vib * sinf(i * TAU_F * 30.0f / RATE);
        ph += f / RATE;
        env = (t < attack) ? t / attack : 1.0f - (t - attack) / (1.0f - attack + 1e-6f);
        env = env * env;
        s = wavef(wave, ph);
        if (wave == W_NOISE) {        /* noise "frequency" acts as a low-pass amount */
            float k = clampf(f / 8000.0f, 0.02f, 1.0f);
            lp += (s - lp) * k;
            s = lp * 1.5f;
        }
        cur_s->data[i0 + i] += s * env * vol;
    }
}

static void gen_sfx(void)
{
    int i;
    s_alloc(SFX_JUMP, 0.16f);
    tone(0, 0.16f, 280, 640, W_SQ25, 0.35f, 0.05f, 0);
    s_alloc(SFX_JUMP2, 0.18f);
    tone(0, 0.18f, 360, 820, W_SQ25, 0.35f, 0.05f, 0);
    s_alloc(SFX_JUMP3, 0.45f);
    tone(0, 0.12f, 400, 900, W_SQ25, 0.3f, 0.05f, 0);
    tone(0.1f, 0.35f, 700, 1300, W_SQUARE, 0.22f, 0.1f, 0.03f);
    tone(0.1f, 0.35f, 520, 640, W_TRI, 0.3f, 0.1f, 0.05f);
    s_alloc(SFX_LAND, 0.08f);
    tone(0, 0.08f, 1500, 300, W_NOISE, 0.35f, 0.02f, 0);
    s_alloc(SFX_COIN, 0.4f);
    tone(0, 0.06f, 1047, 1047, W_SQUARE, 0.22f, 0.02f, 0);
    tone(0.06f, 0.34f, 1568, 1568, W_SQUARE, 0.22f, 0.01f, 0);
    tone(0.06f, 0.34f, 1568 * 2, 1568 * 2, W_SINE, 0.15f, 0.01f, 0);
    s_alloc(SFX_REDCOIN, 0.45f);
    tone(0, 0.45f, 880, 880, W_SQ25, 0.25f, 0.01f, 0);
    tone(0, 0.45f, 1320, 1320, W_SINE, 0.2f, 0.01f, 0);
    s_alloc(SFX_BLUECOIN, 0.5f);
    for (i = 0; i < 4; i++) tone(i * 0.06f, 0.3f, 784 * powf(1.26f, (float)i), 784 * powf(1.26f, (float)i), W_SQUARE, 0.15f, 0.02f, 0);
    s_alloc(SFX_1UP, 0.8f);
    {
        static const float n[6] = { 659, 784, 1319, 1047, 1175, 1568 };
        for (i = 0; i < 6; i++) tone(i * 0.11f, 0.2f, n[i], n[i], W_SQUARE, 0.2f, 0.05f, 0);
    }
    s_alloc(SFX_STAR_APPEAR, 1.2f);
    for (i = 0; i < 10; i++) {
        float f = 880 * powf(2.0f, i / 6.0f);
        tone(i * 0.07f, 0.5f, f, f, W_SINE, 0.18f, 0.02f, 0);
        tone(i * 0.07f, 0.3f, f * 2, f * 2, W_SQ12, 0.05f, 0.02f, 0);
    }
    s_alloc(SFX_STOMP, 0.2f);
    tone(0, 0.18f, 500, 90, W_SQUARE, 0.35f, 0.02f, 0);
    tone(0, 0.1f, 3000, 500, W_NOISE, 0.3f, 0.01f, 0);
    s_alloc(SFX_PUNCH, 0.12f);
    tone(0, 0.12f, 4000, 400, W_NOISE, 0.5f, 0.02f, 0);
    tone(0, 0.08f, 180, 80, W_SINE, 0.4f, 0.02f, 0);
    s_alloc(SFX_KICK, 0.16f);
    tone(0, 0.16f, 6000, 600, W_NOISE, 0.5f, 0.02f, 0);
    tone(0, 0.1f, 250, 90, W_SINE, 0.4f, 0.02f, 0);
    s_alloc(SFX_HURT, 0.45f);
    tone(0, 0.45f, 900, 300, W_SQ25, 0.3f, 0.03f, 0.08f);
    tone(0, 0.15f, 3000, 500, W_NOISE, 0.3f, 0.02f, 0);
    s_alloc(SFX_EXPLODE, 0.9f);
    tone(0, 0.9f, 2500, 120, W_NOISE, 0.9f, 0.01f, 0);
    tone(0, 0.5f, 90, 30, W_SINE, 0.7f, 0.01f, 0);
    s_alloc(SFX_SPLASH, 0.5f);
    tone(0, 0.5f, 5000, 800, W_NOISE, 0.45f, 0.05f, 0);
    tone(0.05f, 0.3f, 2500, 400, W_NOISE, 0.25f, 0.2f, 0);
    s_alloc(SFX_SWIM, 0.25f);
    tone(0, 0.25f, 600, 1800, W_NOISE, 0.35f, 0.4f, 0);
    s_alloc(SFX_POUND, 0.35f);
    tone(0, 0.35f, 140, 40, W_SINE, 0.8f, 0.01f, 0);
    tone(0, 0.2f, 2000, 200, W_NOISE, 0.5f, 0.01f, 0);
    s_alloc(SFX_BLAST, 1.0f);
    tone(0, 1.0f, 1800, 100, W_NOISE, 0.9f, 0.01f, 0);
    tone(0, 0.6f, 70, 30, W_SINE, 0.8f, 0.01f, 0);
    s_alloc(SFX_DOOR, 0.6f);
    tone(0, 0.45f, 140, 110, W_SAW, 0.2f, 0.2f, 0.06f);
    tone(0.45f, 0.12f, 2000, 300, W_NOISE, 0.5f, 0.02f, 0);
    s_alloc(SFX_MENU_MOVE, 0.06f);
    tone(0, 0.06f, 990, 990, W_SQ25, 0.2f, 0.05f, 0);
    s_alloc(SFX_MENU_OK, 0.25f);
    tone(0, 0.08f, 660, 660, W_SQUARE, 0.2f, 0.05f, 0);
    tone(0.08f, 0.17f, 1320, 1320, W_SQUARE, 0.2f, 0.05f, 0);
    s_alloc(SFX_TEXT, 0.03f);
    tone(0, 0.03f, 1400, 1400, W_SQ25, 0.08f, 0.1f, 0);
    s_alloc(SFX_ROAR, 1.2f);
    tone(0, 1.2f, 110, 60, W_SAW, 0.45f, 0.1f, 0.08f);
    tone(0, 1.2f, 1500, 300, W_NOISE, 0.4f, 0.1f, 0);
    s_alloc(SFX_THROW, 0.3f);
    tone(0, 0.3f, 400, 3000, W_NOISE, 0.4f, 0.3f, 0);
    s_alloc(SFX_WALLKICK, 0.2f);
    tone(0, 0.05f, 3000, 800, W_NOISE, 0.4f, 0.02f, 0);
    tone(0.03f, 0.17f, 500, 1000, W_SQ25, 0.25f, 0.05f, 0);
    s_alloc(SFX_STEP, 0.05f);
    tone(0, 0.05f, 1200, 400, W_NOISE, 0.12f, 0.05f, 0);
    s_alloc(SFX_SKID, 0.25f);
    tone(0, 0.25f, 3500, 2500, W_NOISE, 0.25f, 0.1f, 0);
    s_alloc(SFX_SQUAWK, 0.2f);
    tone(0, 0.2f, 820, 560, W_SQ25, 0.25f, 0.05f, 0.12f);
    tone(0, 0.2f, 1640, 1100, W_SINE, 0.1f, 0.05f, 0.12f);
    s_alloc(SFX_GRAB, 0.08f);
    tone(0, 0.08f, 300, 450, W_SQUARE, 0.25f, 0.05f, 0);
    s_alloc(SFX_FIRE, 0.6f);
    tone(0, 0.6f, 800, 3000, W_NOISE, 0.4f, 0.3f, 0);
    s_alloc(SFX_BOUNCE, 0.2f);
    tone(0, 0.2f, 200, 700, W_SINE, 0.45f, 0.05f, 0);
    s_alloc(SFX_PAUSE, 0.3f);
    tone(0, 0.1f, 1320, 1320, W_SQ25, 0.2f, 0.05f, 0);
    tone(0.12f, 0.15f, 990, 990, W_SQ25, 0.2f, 0.05f, 0);
    s_alloc(SFX_WARP, 0.8f);
    tone(0, 0.8f, 200, 1800, W_SINE, 0.3f, 0.2f, 0.1f);
    tone(0, 0.8f, 300, 2400, W_SQ12, 0.08f, 0.2f, 0.1f);
    s_alloc(SFX_BUZZ, 0.3f);
    tone(0, 0.3f, 110, 100, W_SQUARE, 0.3f, 0.02f, 0);
    s_alloc(SFX_SLIDE, 0.3f);
    tone(0, 0.3f, 2000, 1500, W_NOISE, 0.15f, 0.3f, 0);
    s_alloc(SFX_THUD, 0.5f);
    tone(0, 0.5f, 90, 35, W_SINE, 0.9f, 0.01f, 0);
    tone(0, 0.3f, 1200, 100, W_NOISE, 0.6f, 0.01f, 0);
    s_alloc(SFX_BREAK, 0.4f);
    for (i = 0; i < 6; i++) tone(i * 0.05f, 0.1f, 5000, 1000, W_NOISE, 0.35f, 0.02f, 0);
    s_alloc(SFX_POP, 0.15f);
    tone(0, 0.15f, 900, 200, W_SINE, 0.45f, 0.02f, 0);
    s_alloc(SFX_DIVE, 0.25f);
    tone(0, 0.25f, 500, 250, W_SQ25, 0.25f, 0.05f, 0.05f);
    tone(0, 0.2f, 800, 2500, W_NOISE, 0.2f, 0.2f, 0);
    s_alloc(SFX_BACKFLIP, 0.35f);
    tone(0, 0.35f, 300, 1100, W_SQ25, 0.28f, 0.05f, 0.02f);
    s_alloc(SFX_LEDGE, 0.12f);
    tone(0, 0.12f, 600, 400, W_SQ25, 0.2f, 0.05f, 0);
    s_alloc(SFX_DROWN, 0.5f);
    tone(0, 0.5f, 600, 200, W_TRI, 0.35f, 0.05f, 0.15f);

    /* soft clip all */
    for (i = 0; i < SFX_COUNT; i++) {
        int j;
        for (j = 0; j < samples[i].len; j++) {
            float s = samples[i].data[j];
            samples[i].data[j] = s / (1.0f + fabsf(s) * 0.3f);
        }
    }
}

/* ======================================================================
 * Music
 * ====================================================================== */
typedef struct {
    int bpm;
    const char *chords;  /* each token = half bar (4 eighth steps) */
    const char *mel;     /* each token = one eighth step: c5 d#5 eb4  - (hold)  . (rest) */
    const char *mel2;    /* optional counter melody */
    int bass;            /* bass style */
    int drums;           /* drum style */
    int arp;             /* 0 none, 1 = 16th arpeggio, 2 = chord pad stabs */
    int lead_wave;
    int loop;
} SongDef;

enum { B_NONE, B_ROOT, B_WALK, B_POLKA, B_OCT, B_PULSE };
enum { D_NONE, D_ROCK, D_MARCH, D_SOFT, D_BOSS, D_SHUFFLE };

static const SongDef songdefs[MUS_COUNT] = {
    { 0 },
    /* TITLE */
    { 126, "C C F F G G C C",
      "e5 - g5 - c6 - g5 e5 f5 - a5 - c6 - a5 f5 g5 - b5 - d6 - b5 g5 c6 - - - g5 - c5 -",
      "c4 - - - e4 - - - f4 - - - a4 - - - g4 - - - b4 - - - c5 - - - - - - -",
      B_POLKA, D_ROCK, 1, W_SQ25, 1 },
    /* CASTLE */
    { 92, "F F C C Dm Dm Am Am Bb Bb F F Gm Gm C C",
      "a4 - c5 - f5 - e5 - g5 - - - e5 - c5 - d5 - f5 - a5 - g5 - e5 - - - c5 - a4 - "
      "bb4 - d5 - f5 - e5 d5 c5 - a4 - f4 - - - g4 - bb4 - d5 - c5 bb4 c5 - - - - - . .",
      NULL, B_ROOT, D_SOFT, 1, W_TRI, 1 },
    /* FIELD */
    { 144, "G G C C D D G G Em Em C C A7 A7 D D",
      "d5 . g5 - b5 - a5 g5 e5 - g5 - c6 - b5 a5 f#5 - a5 - d6 - c6 a5 b5 - g5 - d5 - . . "
      "e5 g5 b5 - a5 g5 e5 - c5 e5 g5 - a5 g5 e5 - c#5 e5 a5 - g5 - e5 - f#5 - a5 - d5 - . .",
      NULL, B_POLKA, D_ROCK, 0, W_SQ25, 1 },
    /* WATER */
    { 80, "Cmaj7 Cmaj7 Fmaj7 Fmaj7 Am7 Am7 G G Cmaj7 Cmaj7 Fmaj7 Fmaj7 Am7 Am7 G G",
      "e5 - - - g5 - - - a5 - - - g5 - e5 - c5 - - - e5 - g5 - d5 - - - - - . . "
      "g5 - - - e5 - - - f5 - - - a5 - c6 - b5 - - - a5 - e5 - g5 - - - - - . .",
      NULL, B_ROOT, D_NONE, 1, W_SINE, 1 },
    /* SNOW */
    { 120, "Am Am F F C C G G Am Am F F E E E E",
      "a4 - c5 - e5 - a5 - g5 - f5 - e5 - c5 - e5 - g5 - c6 - b5 - d6 - b5 - g5 - . . "
      "a5 - g5 a5 e5 - c5 - f5 - e5 f5 c5 - a4 - g#4 - b4 - e5 - g#5 - b5 - - - . . . .",
      NULL, B_WALK, D_MARCH, 1, W_SQ25, 1 },
    /* SLIDE */
    { 160, "F F Bb Bb C C F F F F Bb Bb G7 G7 C C",
      "f5 a5 c6 a5 f5 a5 c6 - d6 - bb5 - f5 - d5 - e5 g5 c6 g5 e5 g5 bb5 - a5 - f5 - c5 - . . "
      "c6 - a5 - c6 - a5 f5 d6 - - - bb5 - - - b5 - g5 - d5 - f5 - e5 - - - g5 - c6 -",
      NULL, B_POLKA, D_ROCK, 0, W_SQUARE, 1 },
    /* BOSS */
    { 152, "Em Em Em Em C C D D Em Em Em Em C C B B",
      "e5 . e5 . g5 - f#5 e5 b4 - - - b4 c5 d5 - c5 - e5 - g5 - c6 - b5 - a5 - f#5 - d5 - "
      "e5 . e5 . g5 - b5 - e6 - d6 - b5 - g5 - c6 - b5 - g5 - e5 - d#5 - f#5 - b5 - . .",
      NULL, B_OCT, D_BOSS, 0, W_SQ25, 1 },
    /* FINAL */
    { 112, "Cm Cm Ab Ab Fm Fm G G",
      "c5 - eb5 - g5 - - f5 eb5 - c5 - ab4 - - - f4 - ab4 - c5 - eb5 d5 d5 - b4 - g4 - . .",
      "c4 - - - - - - - ab3 - - - - - - - f3 - - - - - - - g3 - - - b3 - - -",
      B_PULSE, D_MARCH, 1, W_SAW, 1 },
    /* STARGET */
    { 150, "C C G C",
      "g5 c6 e6 g6 - e6 g6 - c7 - - - - - - -", NULL, B_ROOT, D_NONE, 1, W_SQUARE, 0 },
    /* GAMEOVER */
    { 84, "C C G G C C",
      "g5 - e5 - c5 - . . f5 - d5 - b4 - . . c5 - - - - - - -", NULL, B_ROOT, D_NONE, 0, W_TRI, 0 },
    /* DEATH */
    { 130, "C C",
      "e5 d#5 d5 c#5 c5 - - -", NULL, B_NONE, D_NONE, 0, W_SQ25, 0 },
    /* ENDING */
    { 84, "C C Am Am F F G G C C Am Am F G C C",
      "e5 - - - g5 - - - a5 - g5 - e5 - c5 - f5 - - - a5 - - - g5 - - - - - . . "
      "e5 - - - g5 - - - c6 - b5 - a5 - g5 - f5 - a5 - g5 - b5 - c6 - - - - - - -",
      "c5 - - - - - - - c5 - - - - - - - a4 - - - - - - - b4 - - - - - - - "
      "c5 - - - - - - - e5 - - - - - - - c5 - - - d5 - - - e5 - - - - - - -",
      B_ROOT, D_SOFT, 1, W_SQ25, 1 },
    /* SELECT */
    { 100, "C C Am Am F F G G",
      "e5 - g5 - - - . . c5 - e5 - - - . . a4 - c5 - f5 - e5 - d5 - - - . . . .",
      NULL, B_ROOT, D_SOFT, 1, W_TRI, 1 },
    /* RACE */
    { 176, "Dm Dm Bb Bb C C A A",
      "d5 f5 a5 f5 d6 - a5 - bb5 - f5 - d5 - f5 - e5 g5 c6 g5 e6 - c6 - c#6 - a5 - e5 - . .",
      NULL, B_OCT, D_ROCK, 0, W_SQ25, 1 },
    /* DRIFTS (snowy dunes) */
    { 112, "D D Bm Bm G G A A D D Bm Bm Em Em A A",
      "f#5 - a5 - d6 - a5 - b5 - a5 - f#5 - d5 - g5 - b5 - d6 - b5 g5 a5 - - - e5 - . . "
      "f#5 - e5 - d5 - a4 - b4 - d5 - f#5 - b5 - g5 - f#5 - e5 - g5 - a5 - c#6 - e6 - . .",
      NULL, B_ROOT, D_SOFT, 1, W_TRI, 1 },
    /* HAUNT */
    { 92, "Dm Dm Bb Bb Gm Gm A A",
      "d5 - - f5 a5 - g#5 - a5 - f5 - d5 - - - g4 - bb4 - d5 - c#5 - e5 - - - a4 - . .",
      "d4 - - - - - - - d4 - - - - - - - bb3 - - - - - - - c#4 - - - - - - -",
      B_ROOT, D_NONE, 1, W_TRI, 1 },
    /* SPIRE: climbing up and up */
    { 138, "C C G G Am Am F F",
      "c5 e5 g5 c6 - g5 e5 g5 d5 g5 b5 d6 - b5 g5 b5 e5 a5 c6 e6 - c6 a5 c6 f5 a5 c6 f6 - e6 d6 c6",
      NULL, B_POLKA, D_ROCK, 0, W_SQ25, 1 },
    /* MAZE: tense, a heartbeat bass */
    { 96, "Am Am Am Am F F E E",
      "a4 . . c5 . . b4 . a4 . . e5 . . d#5 . f4 . . a4 . . g#4 . e4 - - - . . . .",
      NULL, B_PULSE, D_SOFT, 0, W_TRI, 1 },
    /* SEA: a breezy, fast rally tune */
    { 168, "E E C#m C#m A A B B E E G#m G#m A A B B",
      "b4 e5 g#5 b5 - g#5 e5 g#5 c#6 - b5 - g#5 - e5 - a5 - c#6 - e6 - c#6 a5 b5 - d#6 - f#6 - . . "
      "e6 - b5 - g#5 - e5 - d#5 - f#5 - b5 - d#6 - c#6 - a5 - e5 - c#5 - d#5 - f#5 - b5 - - - . .",
      "e4 - - - - - - - c#4 - - - - - - - a3 - - - - - - - b3 - - - - - - - "
      "e4 - - - - - - - g#3 - - - - - - - a3 - - - - - - - b3 - - - d#4 - - -",
      B_OCT, D_ROCK, 1, W_SQ25, 1 },
    /* SKY: dreamy and airborne */
    { 132, "Fmaj7 Fmaj7 Em7 Em7 Dm7 Dm7 G G Fmaj7 Fmaj7 Em7 Em7 Am7 Am7 G G",
      "c6 - a5 - e5 - c6 - b5 - g5 - e5 - b5 - a5 - f5 - d5 - a5 - g5 - - - d6 - . . "
      "c6 - a5 - e5 - c6 - b5 - g5 - e6 - - - c6 - a5 - e5 - c6 - b5 - g5 - d5 - - - . .",
      "f4 - - - - - - - e4 - - - - - - - d4 - - - - - - - g4 - - - - - - - "
      "f4 - - - - - - - e4 - - - - - - - a4 - - - - - - - g4 - - - b4 - - -",
      B_ROOT, D_SOFT, 1, W_TRI, 1 },
    /* SPACE: a driving, starry chase */
    { 156, "Am Am F F G G Em Em Am Am F F G G E E",
      "a5 - e5 a5 c6 - b5 a5 f5 - a5 c6 f6 - e6 c6 g5 - b5 d6 g6 - f6 d6 e6 - b5 - g5 - . . "
      "a5 - c6 - e6 - a6 - f6 - e6 - c6 - a5 - g5 - b5 - d6 - g6 - g#5 - b5 - e6 - - - . .",
      "a4 - - - - - - - f4 - - - - - - - g4 - - - - - - - e4 - - - - - - - "
      "a4 - - - c5 - - - a4 - - - f4 - - - g4 - - - b4 - - - g#4 - - - b4 - - -",
      B_OCT, D_ROCK, 1, W_SQ25, 1 },
    /* NANO: a bouncy little village march, played as if by a tiny wind-up band */
    { 132, "G G D D G G C D G G D D C C D G",
      "g5 b5 d6 b5 g5 - d5 - a5 c6 d6 c6 a5 - f#5 - g5 b5 d6 g6 - d6 b5 g5 e6 - c6 - d6 - . . "
      "b5 - g5 - d5 - g5 - a5 - f#5 - d5 - a5 - c6 - e6 - c6 - g5 - a5 - d6 - f#6 - g6 -",
      "g4 - - - d4 - - - g4 - - - c4 - d4 - g4 - - - d4 - - - c4 - - - d4 - g4 -",
      B_POLKA, D_MARCH, 1, W_SQ25, 1 },
    /* HUGE: slow, heavy giant footsteps in a cosy room */
    { 96, "F F F F Bb Bb F F Gm Gm C C F F C C",
      "f4 - - - a4 - c5 - f5 - - - e5 - c5 - d5 - - - bb4 - g4 - a4 - - - c5 - - - "
      "bb4 - - - d5 - f5 - a4 - - - c5 - f5 - g4 - bb4 - c5 - e5 - f5 - - - . . . .",
      NULL, B_ROOT, D_SOFT, 1, W_TRI, 1 },
    /* CITY: a cool, jazzy walk among the towers at dusk */
    { 112, "Dm7 Dm7 G7 G7 Cmaj7 Cmaj7 Am7 Am7 Dm7 Dm7 G7 G7 Em7 Em7 A7 A7",
      "a5 - f5 - c6 - a5 - b5 - - g5 f5 - d5 - e5 - g5 - b5 - - - a5 - e5 - c5 - . . "
      "d5 - f5 - a5 - c6 - b5 - g5 - f5 - d5 - g5 - b5 - e6 - d6 - c#6 - a5 - e5 - g5 -",
      NULL, B_WALK, D_SHUFFLE, 2, W_TRI, 1 },
    /* CAVE: slow, echoing drips in the dark */
    { 76, "Em Em Em Em C C B B Em Em Em Em Am Am B B",
      "e5 - . . b4 - . . g5 - . . f#5 - e5 - c5 - . . e5 - . . d#5 - . . b4 - . . "
      "e5 - . . g5 - . . b5 - a5 - g5 - . . a4 - . . c5 - e5 - d#5 - - - . . . .",
      NULL, B_ROOT, D_NONE, 1, W_SINE, 1 },
    /* SIDE: a perky 8-bit overworld tune (original) */
    { 150, "C C Am Am F F G G C C Am Am F F G C",
      "c5 . e5 g5 c6 - g5 e5 a5 . c6 a5 e5 - c5 - f5 . a5 c6 f6 - c6 a5 g5 . b5 d6 g6 - . . "
      "e6 - d6 c6 g5 - e5 - a5 - c6 - e6 d6 c6 - f5 a5 c6 f5 a5 c6 - . g5 b5 d6 b5 c6 - - -",
      NULL, B_OCT, D_ROCK, 0, W_SQUARE, 1 },
    /* ICE: a brisk, cheerful climbing march (original) */
    { 144, "D D G G A A D D Bm Bm G G E E A A",
      "d5 f#5 a5 d6 - a5 f#5 a5 g5 b5 d6 g6 - d6 b5 g5 a5 c#6 e6 a6 - e6 c#6 a5 f#5 - a5 - d6 - . . "
      "b5 - f#5 - d5 - f#5 - g5 - b5 - d6 - g6 - e6 - g5 - b5 - e6 - c#6 e6 a6 e6 c#6 - a5 -",
      NULL, B_POLKA, D_MARCH, 1, W_SQ25, 1 },
};

#define MAXSTEPS 512
typedef struct {
    int len;                  /* in steps */
    signed char mel[MAXSTEPS];  int mel_len;
    signed char mel2[MAXSTEPS]; int mel2_len;
    signed char chord[MAXSTEPS / 4][4];
    int nchords;
} Song;
static Song songs[MUS_COUNT];

#define N_HOLD  -2
#define N_REST  -1

static int parse_note(const char *t)
{
    static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 }; /* a b c d e f g */
    int n, oct;
    char c = (char)tolower((unsigned char)t[0]);
    if (c == '-') return N_HOLD;
    if (c == '.') return N_REST;
    if (c < 'a' || c > 'g') return N_REST;
    n = base[c - 'a'];
    t++;
    if (*t == '#') { n++; t++; }
    else if (*t == 'b' && isdigit((unsigned char)t[1])) { n--; t++; }
    oct = atoi(t);
    return (oct + 1) * 12 + n;   /* midi */
}

static int parse_seq(const char *s, signed char *out)
{
    char tok[16];
    int n = 0;
    while (s && *s && n < MAXSTEPS) {
        int k = 0;
        while (*s == ' ' || *s == '|') s++;
        if (!*s) break;
        while (*s && *s != ' ' && k < 15) tok[k++] = *s++;
        tok[k] = 0;
        out[n++] = (signed char)parse_note(tok);
    }
    return n;
}

static void parse_chord(const char *t, signed char *out)
{
    static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 };
    int root = base[tolower((unsigned char)t[0]) - 'a'];
    int third = 4, fifth = 7, ext = 12;
    t++;
    if (*t == '#') { root++; t++; }
    else if (*t == 'b') { root--; t++; }
    if (strncmp(t, "maj7", 4) == 0) { ext = 11; }
    else if (strncmp(t, "m7", 2) == 0) { third = 3; ext = 10; }
    else if (*t == 'm') { third = 3; }
    else if (*t == '7') { ext = 10; }
    else if (strncmp(t, "dim", 3) == 0) { third = 3; fifth = 6; }
    root = (root + 12) % 12;
    out[0] = (signed char)(48 + root);
    out[1] = (signed char)(48 + root + third);
    out[2] = (signed char)(48 + root + fifth);
    out[3] = (signed char)(48 + root + ext);
}

static void parse_songs(void)
{
    int i;
    for (i = 1; i < MUS_COUNT; i++) {
        const SongDef *d = &songdefs[i];
        Song *s = &songs[i];
        const char *c = d->chords;
        char tok[16];
        s->nchords = 0;
        while (c && *c && s->nchords < MAXSTEPS / 4) {
            int k = 0;
            while (*c == ' ') c++;
            if (!*c) break;
            while (*c && *c != ' ' && k < 15) tok[k++] = *c++;
            tok[k] = 0;
            parse_chord(tok, s->chord[s->nchords++]);
        }
        s->len = s->nchords * 4;
        s->mel_len = parse_seq(d->mel, s->mel);
        s->mel2_len = d->mel2 ? parse_seq(d->mel2, s->mel2) : 0;
        if (s->mel_len > s->len) s->len = s->mel_len;
    }
}

static float midi_hz(int m) { return 440.0f * powf(2.0f, (m - 69) / 12.0f); }

typedef struct {
    float phase, freq, env, sustain, decay;
    int gate, wave;
    float vol;
} Voice;

typedef struct {
    float t, pitch, pitch_end, decay, vol;
    int kind; /* 0 none 1 kick 2 snare 3 hat */
    float ph;
} Drum;

static struct {
    int id;
    int step;
    double step_pos;    /* samples into current step */
    double step_len;
    int finished;
    int arp_idx;
    Voice lead, lead2, bass, arp;
    Drum drum[3];
} mus;

/* mixer voices */
typedef struct { const Sample *s; double pos, inc; float vol; } MixVoice;
static MixVoice mv[24];
static float vol_music = 0.55f, vol_sfx = 0.8f;
static int audio_ok;

static void voice_note(Voice *v, int note)
{
    if (note == N_HOLD) return;
    if (note == N_REST) { v->gate = 0; return; }
    v->freq = midi_hz(note);
    v->gate = 1;
    v->env = 1.0f;
}

static float voice_run(Voice *v)
{
    float s;
    if (v->env < 0.0005f && !v->gate) return 0;
    v->phase += v->freq / RATE;
    if (v->phase >= 1) v->phase -= 1;
    if (v->gate) v->env = v->sustain + (v->env - v->sustain) * v->decay;
    else v->env *= 0.9985f;
    s = wavef(v->wave, v->phase);
    return s * v->env * v->vol;
}

static void drum_hit(int kind)
{
    Drum *d = &mus.drum[kind - 1];
    d->kind = kind;
    d->t = 0;
    d->ph = 0;
    if (kind == 1) { d->pitch = 160; d->pitch_end = 45; d->decay = 0.12f; d->vol = 0.9f; }
    else if (kind == 2) { d->pitch = 200; d->pitch_end = 150; d->decay = 0.14f; d->vol = 0.5f; }
    else { d->pitch = 0; d->decay = 0.035f; d->vol = 0.18f; }
}

static float drum_run(Drum *d)
{
    float s = 0, env;
    if (!d->kind) return 0;
    d->t += 1.0f / RATE;
    env = expf(-d->t / d->decay);
    if (env < 0.001f) { d->kind = 0; return 0; }
    if (d->kind == 1) {
        float f = d->pitch_end + (d->pitch - d->pitch_end) * expf(-d->t * 30);
        d->ph += f / RATE;
        s = sinf(d->ph * TAU_F) * env * d->vol;
    } else if (d->kind == 2) {
        d->ph += d->pitch / RATE;
        s = (noisef() * 0.7f + sinf(d->ph * TAU_F) * 0.3f) * env * d->vol;
    } else {
        static float last;
        float n = noisef();
        s = (n - last) * 0.5f * env * d->vol;
        last = n;
    }
    return s;
}

static void song_step(void)
{
    const SongDef *d = &songdefs[mus.id];
    Song *s = &songs[mus.id];
    int st = mus.step;
    int ci = (st / 4) % (s->nchords ? s->nchords : 1);
    signed char *ch = s->chord[ci];
    int beat = st % 8;

    if (s->mel_len) voice_note(&mus.lead, s->mel[st % s->mel_len]);
    if (s->mel2_len) voice_note(&mus.lead2, s->mel2[st % s->mel2_len]);

    /* bass */
    if (s->nchords) {
        int root = ch[0] - 12, n = N_HOLD;
        switch (d->bass) {
        case B_ROOT: if (st % 4 == 0) n = root; break;
        case B_WALK: n = (beat % 2 == 0) ? ((beat / 2) % 2 ? ch[2] - 12 : root) : N_HOLD; break;
        case B_POLKA: n = (beat % 2 == 0) ? ((beat % 4 == 0) ? root : ch[2] - 12) : N_REST; break;
        case B_OCT: n = (st % 2 == 0) ? root : root + 12; break;
        case B_PULSE: n = root; break;
        }
        if (d->bass != B_NONE) voice_note(&mus.bass, n);
    }
    /* drums */
    switch (d->drums) {
    case D_ROCK:
        if (beat == 0 || beat == 4 || beat == 5) drum_hit(1);
        if (beat == 2 || beat == 6) drum_hit(2);
        drum_hit(3);
        break;
    case D_MARCH:
        if (beat == 0 || beat == 4) drum_hit(1);
        if (beat == 2 || beat == 6 || beat == 7) drum_hit(2);
        if (beat % 2 == 0) drum_hit(3);
        break;
    case D_SOFT:
        if (beat == 0) drum_hit(1);
        if (beat % 2 == 1) drum_hit(3);
        break;
    case D_BOSS:
        if (beat % 2 == 0) drum_hit(1);
        if (beat == 2 || beat == 6) drum_hit(2);
        drum_hit(3);
        break;
    }
    mus.arp_idx = 0;
}

static float song_sample(void)
{
    const SongDef *d;
    float s = 0;
    int i;
    if (!mus.id || mus.finished) return 0;
    d = &songdefs[mus.id];
    if (mus.step_pos >= mus.step_len) {
        mus.step_pos -= mus.step_len;
        mus.step++;
        if (mus.step >= songs[mus.id].len) {
            if (d->loop) mus.step = 0;
            else {
                mus.finished = 1;
                mus.lead.gate = mus.lead2.gate = mus.bass.gate = mus.arp.gate = 0;
                return 0;
            }
        }
        song_step();
    }
    /* arpeggio: 16th notes */
    if (d->arp == 1 && songs[mus.id].nchords) {
        int sub = (int)(mus.step_pos * 2 / mus.step_len);
        int idx = (mus.step % 4) * 2 + sub;
        if (idx != mus.arp_idx || mus.step_pos < 1) {
            static const int pat[8] = { 0, 1, 2, 3, 2, 1, 0, 2 };
            Song *so = &songs[mus.id];
            signed char *ch = so->chord[(mus.step / 4) % so->nchords];
            mus.arp_idx = idx;
            mus.arp.freq = midi_hz(ch[pat[idx & 7]] + 12);
            mus.arp.gate = 1;
            mus.arp.env = 1;
        }
    }
    mus.step_pos += 1.0;
    s += voice_run(&mus.lead);
    s += voice_run(&mus.lead2);
    s += voice_run(&mus.bass);
    s += voice_run(&mus.arp);
    for (i = 0; i < 3; i++) s += drum_run(&mus.drum[i]);
    return s;
}

static void audio_callback(short *out, int frames)
{
    int i, j;
    plat_lock();
    for (i = 0; i < frames; i++) {
        float m = song_sample() * vol_music, e = 0, o;
        for (j = 0; j < 24; j++) {
            MixVoice *v = &mv[j];
            int p;
            float f, a;
            if (!v->s) continue;
            p = (int)v->pos;
            if (p + 1 >= v->s->len) { v->s = NULL; continue; }
            f = (float)(v->pos - p);
            a = v->s->data[p] * (1 - f) + v->s->data[p + 1] * f;
            e += a * v->vol;
            v->pos += v->inc;
        }
        o = m + e * vol_sfx;
        o = o / (1.0f + fabsf(o) * 0.25f);
        if (o > 1) o = 1;
        if (o < -1) o = -1;
        out[i * 2] = out[i * 2 + 1] = (short)(o * 30000);
    }
    plat_unlock();
}

/* offline render (used for testing): plays song id for secs seconds into a WAV file */
void audio_dump_wav(int id, float secs, const char *path)
{
    int frames = (int)(secs * RATE), i;
    short *buf = (short *)malloc(sizeof(short) * 2 * (size_t)frames);
    FILE *f;
    unsigned hdr[11];
    if (!samples[0].data) { gen_sfx(); parse_songs(); }
    music_play(id);
    for (i = 0; i < frames; i += 512) {
        int n = frames - i < 512 ? frames - i : 512;
        if (i == RATE / 2) sfx(SFX_JUMP);
        if (i == RATE) sfx(SFX_COIN);
        audio_callback(buf + i * 2, n);
    }
    f = fopen(path, "wb");
    if (f) {
        hdr[0] = 0x46464952; hdr[1] = 36 + frames * 4; hdr[2] = 0x45564157; hdr[3] = 0x20746d66; hdr[4] = 16;
        hdr[5] = 1 | (2 << 16); hdr[6] = RATE; hdr[7] = RATE * 4; hdr[8] = 4 | (16 << 16); hdr[9] = 0x61746164; hdr[10] = frames * 4;
        fwrite(hdr, 4, 11, f);
        fwrite(buf, 4, (size_t)frames, f);
        fclose(f);
    }
    free(buf);
}

void audio_init(void)
{
    gen_sfx();
    parse_songs();
    memset(&mus, 0, sizeof mus);
    audio_ok = plat_audio_init(RATE, audio_callback);
}

void audio_shutdown(void)
{
    int i;
    plat_audio_shutdown();
    for (i = 0; i < SFX_COUNT; i++) { free(samples[i].data); samples[i].data = NULL; }
}

void sfx_ex(int id, float vol, float pitch)
{
    int i, best = 0;
    double bestpos = -1;
    if (id < 0 || id >= SFX_COUNT || !samples[id].data) return;
    plat_lock();
    for (i = 0; i < 24; i++) {
        if (!mv[i].s) { best = i; bestpos = 1e18; break; }
        if (mv[i].pos > bestpos) { bestpos = mv[i].pos; best = i; }
    }
    mv[best].s = &samples[id];
    mv[best].pos = 0;
    mv[best].inc = pitch;
    mv[best].vol = vol;
    plat_unlock();
}

void sfx(int id) { sfx_ex(id, 1.0f, 1.0f); }

void music_play(int id)
{
    const SongDef *d;
    plat_lock();
    if (id == mus.id && !mus.finished) { plat_unlock(); return; }
    memset(&mus, 0, sizeof mus);
    mus.id = id;
    if (id > 0 && id < MUS_COUNT) {
        d = &songdefs[id];
        mus.step_len = RATE * 60.0 / (d->bpm * 2.0);
        mus.lead.wave = d->lead_wave; mus.lead.vol = 0.22f; mus.lead.sustain = 0.55f; mus.lead.decay = 0.9997f;
        mus.lead2.wave = W_TRI; mus.lead2.vol = 0.2f; mus.lead2.sustain = 0.6f; mus.lead2.decay = 0.9997f;
        mus.bass.wave = W_TRI; mus.bass.vol = 0.38f; mus.bass.sustain = 0.7f; mus.bass.decay = 0.9998f;
        mus.arp.wave = W_SQ12; mus.arp.vol = 0.07f; mus.arp.sustain = 0.0f; mus.arp.decay = 0.9993f;
        if (d->lead_wave == W_SINE) mus.lead.vol = 0.3f;
        if (d->lead_wave == W_SAW) mus.lead.vol = 0.16f;
        song_step();
    }
    plat_unlock();
}

void music_stop(void) { music_play(MUS_NONE); }
int music_current(void) { return mus.id; }
int music_finished(void) { return mus.finished || mus.id == 0; }
void audio_volume(float m, float e) { vol_music = m; vol_sfx = e; }
