#include "s_alsa_pcm_list.h"
#include <alsa/asoundlib.h>

alsa_pcm_list_t *alsa_pcm_list_init(alsa_pcm_list_dir dir)
{
    alsa_pcm_list_t *info = malloc(sizeof(alsa_pcm_list_t));
    if (info == NULL)
        return NULL;

    info->n         = NULL;
    info->name_prev = NULL;
    info->filter = dir == ALSA_PCM_OUTPUT_LIST ? "Output" : "Input";
    if (snd_device_name_hint(-1, "pcm", &info->hints) >= 0)
        info->n = info->hints;
    return info;
}

const char *alsa_pcm_list_get_next(alsa_pcm_list_t *info)
{
    char *name = NULL, *io;

    if (!info)
        return NULL;

    for (; *info->n != NULL; ++info->n) {
        io = snd_device_name_get_hint(*info->n, "IOID");
        if (io == NULL || strcmp(io, info->filter) == 0)
        {
            char *colon;
            int uniq = 0;
            name = snd_device_name_get_hint(*info->n, "NAME");
            colon = strchr(name, ':');
            if (colon) {
                *colon = '\0';
            }

            if (info->name_prev == NULL || strcmp(name, info->name_prev) != 0) {
                uniq = 1;
            }

            free(info->name_prev); // one behind on free (last one is freed in alsa_pcm_list_free())
            info->name_prev = name;
            if (uniq)
                break;
            else
                name = NULL;
        }
        free(io);
    }
    return name;
}

void alsa_pcm_list_free(alsa_pcm_list_t *info)
{
    if (info) {
        free(info->name_prev);
        snd_device_name_free_hint(info->hints);
        free(info);
    }
}
