/* lang.c - UI language: English (built in) / Japanese (lang_ja.c, generated) */
#include "lang.h"
#include "platform.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int g_lang = LANG_EN;

#define HSIZE 1024
static int hash_idx[HSIZE];
static int hash_built;

static unsigned hstr(const char *s)
{
    unsigned h = 2166136261u;
    for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}

static void build(void)
{
    int i;
    for (i = 0; i < HSIZE; i++) hash_idx[i] = -1;
    for (i = 0; i < lang_ja_count; i++) {
        unsigned h = hstr(lang_ja_table[i][0]) & (HSIZE - 1);
        while (hash_idx[h] >= 0) h = (h + 1) & (HSIZE - 1);
        hash_idx[h] = i;
    }
    hash_built = 1;
}

const char *tr(const char *en)
{
    unsigned h;
    if (!en || g_lang == LANG_EN) return en;
    if (!hash_built) build();
    h = hstr(en) & (HSIZE - 1);
    while (hash_idx[h] >= 0) {
        const char *k = lang_ja_table[hash_idx[h]][0];
        if (k == en || strcmp(k, en) == 0) return lang_ja_table[hash_idx[h]][1];
        h = (h + 1) & (HSIZE - 1);
    }
    return en;
}

const char *lang_name(int lang) { return lang == LANG_JA ? "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" : "English"; }

#define cfg_name plat_user_file("tux64.cfg")

void lang_load(void)
{
    FILE *f = fopen(cfg_name, "r");
    char line[64];
    g_lang = plat_system_lang_ja() ? LANG_JA : LANG_EN;
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "lang=ja", 7) == 0) g_lang = LANG_JA;
        else if (strncmp(line, "lang=en", 7) == 0) g_lang = LANG_EN;
    }
    fclose(f);
}

void lang_save(void)
{
    FILE *f = fopen(cfg_name, "w");
    if (!f) return;
    fprintf(f, "lang=%s\n", g_lang == LANG_JA ? "ja" : "en");
    fclose(f);
}
