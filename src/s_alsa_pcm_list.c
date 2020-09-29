#include "s_alsa_pcm_list.h"

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




alsa_hw_list_t *alsa_hw_list_init(alsa_pcm_list_dir dir)
{
    alsa_hw_list_t *hw = malloc(sizeof(alsa_hw_list_t));
    if (hw == NULL)
        goto err_malloc;
    if (snd_ctl_card_info_malloc(&hw->info) < 0)
        goto err_ctl_card_info;
    if (snd_pcm_info_malloc(&hw->pcminfo) < 0)
        goto err_pcm_info;

    hw->dir = dir;
    hw->card = -1;
    hw->dev  = -1;

    return hw;

    // error handling
err_pcm_info:
    snd_ctl_card_info_free(hw->info);
err_ctl_card_info:
    free(hw);
err_malloc:
    return NULL;
}

const char *alsa_hw_list_get_next(alsa_hw_list_t *hw, const char **cardname, const char **pcmname)
{
    int err;
    if (!hw)
        return NULL;

    if (hw->card >= 0)
        goto continue_from_before; // i.e. not the first call into this function

    while (snd_card_next(&hw->card) >= 0 && hw->card >= 0) {
        {
            char namefragment[7];
            snprintf(namefragment, sizeof(namefragment), "hw:%d", hw->card);
            if ((err = snd_ctl_open(&hw->handle, namefragment, 0)) < 0) {
                continue;
            }
        }
        if ((err = snd_ctl_card_info(hw->handle, hw->info)) < 0) {
            snd_ctl_close(hw->handle);
            continue;
        }
        hw->dev = -1;
    continue_from_before:
        while (1) {
            unsigned int count;
            const snd_pcm_stream_t stream = ((hw->dir == ALSA_PCM_OUTPUT_LIST) ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE);
            if (snd_ctl_pcm_next_device(hw->handle, &hw->dev)<0 || hw->dev < 0)
                break;
            snd_pcm_info_set_device(hw->pcminfo, hw->dev);
            snd_pcm_info_set_subdevice(hw->pcminfo, 0);
            snd_pcm_info_set_stream(hw->pcminfo, stream);
            if ((err = snd_ctl_pcm_info(hw->handle, hw->pcminfo)) < 0) {
                continue;
            }
            snprintf(hw->hwname, sizeof(hw->hwname), "hw:%d,%d", hw->card, hw->dev);
            if (cardname)
                *cardname = snd_ctl_card_info_get_id(hw->info);
            if (pcmname)
                *pcmname = snd_pcm_info_get_name(hw->pcminfo);
            return hw->hwname;
        }
        snd_ctl_close(hw->handle);
    }
    if (cardname)
        *cardname = "";
    if (pcmname)
        *pcmname = "";
    return NULL;
}


void alsa_hw_list_free(alsa_hw_list_t *hw)
{
    if (hw) {
        snd_pcm_info_free(hw->pcminfo);
        snd_ctl_card_info_free(hw->info);
        free(hw);
    }
}
