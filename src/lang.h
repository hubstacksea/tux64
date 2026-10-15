/* lang.h - UI language selection and string translation */
#ifndef LANG_H
#define LANG_H

enum { LANG_EN, LANG_JA, LANG_COUNT };
extern int g_lang;

/* returns the translation of an English UI string for the current language (or the string itself) */
const char *tr(const char *en);
void lang_load(void);          /* read tux64.cfg (falls back to the OS language) */
void lang_save(void);
const char *lang_name(int lang);

extern const char *const lang_ja_table[][2];
extern const int lang_ja_count;

#endif
