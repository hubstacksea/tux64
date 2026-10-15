/* audio.h - software mixer, synthesized sound effects and chiptune music */
#ifndef AUDIO_H
#define AUDIO_H

enum {
    SFX_JUMP, SFX_JUMP2, SFX_JUMP3, SFX_LAND, SFX_COIN, SFX_REDCOIN, SFX_BLUECOIN, SFX_1UP,
    SFX_STAR_APPEAR, SFX_STOMP, SFX_PUNCH, SFX_KICK, SFX_HURT, SFX_EXPLODE, SFX_SPLASH,
    SFX_SWIM, SFX_POUND, SFX_BLAST, SFX_DOOR, SFX_MENU_MOVE, SFX_MENU_OK, SFX_TEXT,
    SFX_ROAR, SFX_THROW, SFX_WALLKICK, SFX_STEP, SFX_SKID, SFX_SQUAWK, SFX_GRAB, SFX_FIRE,
    SFX_BOUNCE, SFX_PAUSE, SFX_WARP, SFX_BUZZ, SFX_SLIDE, SFX_THUD, SFX_BREAK, SFX_POP,
    SFX_DIVE, SFX_BACKFLIP, SFX_LEDGE, SFX_DROWN, SFX_COUNT
};

enum {
    MUS_NONE, MUS_TITLE, MUS_CASTLE, MUS_FIELD, MUS_WATER, MUS_SNOW, MUS_SLIDE, MUS_BOSS,
    MUS_FINAL, MUS_STARGET, MUS_GAMEOVER, MUS_DEATH, MUS_ENDING, MUS_SELECT, MUS_RACE, MUS_DRIFTS, MUS_HAUNT, MUS_SPIRE, MUS_MAZE, MUS_SEA, MUS_SKY,
    MUS_COUNT
};

void audio_init(void);
void audio_shutdown(void);
void sfx(int id);
void sfx_ex(int id, float vol, float pitch);
void music_play(int id);
void music_stop(void);
int  music_current(void);
int  music_finished(void);   /* non-looping song reached its end */
void audio_volume(float music, float effects);
void audio_dump_wav(int id, float secs, const char *path);

#endif
