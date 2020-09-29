#ifndef ALSA_PCM_LIST_H
#define ALSA_PCM_LIST_H

#include <alsa/asoundlib.h>

typedef struct alsa_pcm_list {
    const char *filter;
    void **hints;
    void **n;
    char *name_prev;
} alsa_pcm_list_t;

typedef enum {
    ALSA_PCM_INPUT_LIST,
    ALSA_PCM_OUTPUT_LIST
} alsa_pcm_list_dir;

alsa_pcm_list_t *alsa_pcm_list_init(alsa_pcm_list_dir dir);
const char *alsa_pcm_list_get_next(alsa_pcm_list_t *info);
void alsa_pcm_list_free(alsa_pcm_list_t *info);






typedef struct {
    // members needed for query
    alsa_pcm_list_dir dir;
    int card;
    int dev;
    snd_ctl_t *handle;
    snd_ctl_card_info_t *info;
    snd_pcm_info_t *pcminfo;

    // buffer for holding results
    char hwname[11]; // e.g. "hw:0,0"
} alsa_hw_list_t;

alsa_hw_list_t *alsa_hw_list_init(alsa_pcm_list_dir dir);
const char *alsa_hw_list_get_next(alsa_hw_list_t *hw, const char **cardname, const char **pcmname);
void alsa_hw_list_free(alsa_hw_list_t *hw);

#endif
