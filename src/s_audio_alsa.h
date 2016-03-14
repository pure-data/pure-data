/* Copyright (c) 1997- Guenter Geiger, Miller Puckette, Larry Troxler,
* Winfried Ritsch, Karl MacMillan, and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */


typedef int16_t t_alsa_sample16;
typedef int32_t t_alsa_sample32;
#define ALSA_SAMPLEWIDTH_16 sizeof(t_alsa_sample16)
#define ALSA_SAMPLEWIDTH_32 sizeof(t_alsa_sample32)
#define ALSA_XFERSIZE16  (signed int)(sizeof(t_alsa_sample16) * DEFDACBLKSIZE)
#define ALSA_XFERSIZE32  (signed int)(sizeof(t_alsa_sample32) * DEFDACBLKSIZE)
#define ALSA_MAXDEV 4
#define ALSA_JITTER 1024
#define ALSA_EXTRABUFFER 2048
#define ALSA_DEFFRAGSIZE 64
#define ALSA_DEFNFRAG 12

#ifndef INT32_MAX
#define INT32_MAX 0x7fffffff
#endif

typedef struct _alsa_dev
{
    snd_pcm_t *a_handle;
    int a_devno;
    int a_sampwidth;
    int a_channels;
    char **a_addr;
    int a_synced;
} t_alsa_dev;

extern t_alsa_dev alsa_indev[ALSA_MAXDEV];
extern t_alsa_dev alsa_outdev[ALSA_MAXDEV];
extern int alsa_nindev;
extern int alsa_noutdev;

int alsamm_open_audio(int rate, int blocksize);
void alsamm_close_audio(void);
int alsamm_send_dacs(void);
