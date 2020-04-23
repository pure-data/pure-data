#ifndef ALSA_PCM_LIST_H
#define ALSA_PCM_LIST_H

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


#endif
