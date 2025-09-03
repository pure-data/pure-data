/* Copyright (c) 1997-2003 Guenter Geiger, Miller Puckette, Larry Troxler,
* Winfried Ritsch, Karl MacMillan, and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* this file inputs and outputs audio using the pipewire API available on linux. */

/* support for pipewire 0.3 by Charles K. Neimog <charlesneimog@outlook.com> */

#ifdef USEAPI_PIPEWIRE

#include "m_pd.h"
#include "s_stuff.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/latency-utils.h>
#include <spa/param/param.h>

#include "m_private_utils.h"
#include "s_audio_paring.h"

/* enable thread signaling instead of polling (like JACK backend) */
#if 1
#define THREADSIGNAL
#endif

#define MAX_BUFFERS 4
#define MAX_ALLOCA_SAMPLES  (16*1024)

/* ------------------------------- Backend state ------------------------------- */
typedef struct _pw_state {
    int samplerate;
    int blocksize;
    int in_n_devices;
    int out_n_devices;

    unsigned in_channels;
    unsigned out_channels;

    unsigned desired_block;

    struct pw_thread_loop *tloop;
    struct pw_stream *stream_in;
    struct pw_stream *stream_out;

    struct spa_hook stream_in_listener;
    struct spa_hook stream_out_listener;

    /* sys_ringbuf-based FIFOs (interleaved t_sample audio) */
    sys_ringbuf inring;         /* PW -> Pd */
    sys_ringbuf outring;        /* Pd -> PW */
    char *inbuf;                /* backing storage for inring */
    char *outbuf;               /* backing storage for outring */

#ifdef THREADSIGNAL
    t_semaphore *sem;           /* wake Pd scheduler when audio thread has work */
#endif

    int running;
    volatile int dio_error;     /* set on over/underruns */
} t_pw_state;

static t_pw_state pw_state;
static int pw_inited = 0;

/* ------------------ PipeWire callbacks ------------------ */
static void pw_in_param_changed(void *data, uint32_t id, const struct spa_pod *param)
{
    (void)data; (void)id; (void)param;
    logpost(0, PD_DEBUG, "[pipewire] input param changed");
}

static void pw_in_process(void *data)
{
    t_pw_state *st = (t_pw_state *)data;
    if (!st || !st->stream_in) return;

    struct pw_buffer *b = pw_stream_dequeue_buffer(st->stream_in);
    if (!b) return;

    struct spa_buffer *buf = b->buffer;
    if (!buf || buf->n_datas <= 0 || buf->datas[0].data == NULL) {
        pw_stream_queue_buffer(st->stream_in, b);
        return;
    }

    void *base = buf->datas[0].data;
    size_t maxsize = (size_t)buf->datas[0].maxsize;
    size_t chunk_offset = 0;
    size_t chunk_size = 0;
    if (buf->datas[0].chunk) {
        chunk_offset = (size_t)buf->datas[0].chunk->offset;
        chunk_size   = (size_t)buf->datas[0].chunk->size;
    }
    if (chunk_offset > maxsize) {
        pw_stream_queue_buffer(st->stream_in, b);
        return;
    }
    size_t nbytes = chunk_size ? chunk_size : (maxsize - chunk_offset);
    if (nbytes == 0) {
        pw_stream_queue_buffer(st->stream_in, b);
        return;
    }

    const void *src = (const char *)base + chunk_offset;

    /* Write interleaved float/t_sample bytes into input ring */
    long written = sys_ringbuf_write(&st->inring, src, (long)nbytes, st->inbuf);
    if (written < (long)nbytes) {
        /* overflow: drop tail */
        st->dio_error = 1;
    }

#ifdef THREADSIGNAL
    if (st->sem) sys_semaphore_post(st->sem);
#endif
    pw_stream_queue_buffer(st->stream_in, b);
}

static const struct pw_stream_events stream_in_events = {
    PW_VERSION_STREAM_EVENTS,
    .param_changed = pw_in_param_changed,
    .process       = pw_in_process,
};

static void pw_out_param_changed(void *data, uint32_t id, const struct spa_pod *param)
{
    (void)data; (void)id; (void)param;
    logpost(0, PD_DEBUG, "[pipewire] output param changed");
}

static void pw_out_process(void *data)
{
    t_pw_state *st = (t_pw_state *)data;
    if (!st || !st->stream_out) return;

    struct pw_buffer *b = pw_stream_dequeue_buffer(st->stream_out);
    if (!b) return;

    struct spa_buffer *buf = b->buffer;
    if (!buf || buf->n_datas <= 0 || buf->datas[0].data == NULL) {
        pw_stream_queue_buffer(st->stream_out, b);
        return;
    }

    char *dst = (char *)buf->datas[0].data;
    uint32_t max_bytes = buf->datas[0].maxsize;

    /* read interleaved bytes from output ring */
    long got = sys_ringbuf_read(&st->outring, dst, (long)max_bytes, st->outbuf);
    if (got < (long)max_bytes) {
        /* underrun: zero remainder */
        memset(dst + got, 0, (size_t)max_bytes - (size_t)got);
        st->dio_error = 1;
    }

    if (buf->datas[0].chunk) {
        buf->datas[0].chunk->offset = 0;
        buf->datas[0].chunk->stride = (int)st->out_channels * (int)sizeof(t_sample);
        buf->datas[0].chunk->size   = max_bytes;
    }

#ifdef THREADSIGNAL
    if (st->sem) sys_semaphore_post(st->sem);
#endif
    pw_stream_queue_buffer(st->stream_out, b);
}

static const struct pw_stream_events stream_out_events = {
    PW_VERSION_STREAM_EVENTS,
    .param_changed = pw_out_param_changed,
    .process       = pw_out_process,
};

/* -------------------------- Helpers: make/conn streams -------------------------- */
static int pw_make_capture_stream(t_pw_state *st)
{
    unsigned sr = (unsigned)(st->samplerate > 0 ? st->samplerate : 48000);

    char rate_prop[32];
    snprintf(rate_prop, sizeof(rate_prop), "%u/1", sr);
    char latency_prop[32];
    snprintf(latency_prop, sizeof(latency_prop), "%u/%u", (unsigned)st->desired_block, sr);

    struct pw_properties *props =
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                          PW_KEY_MEDIA_CATEGORY, "Capture",
                          PW_KEY_MEDIA_ROLE, "Production",
                          NULL);
    pw_properties_set(props, PW_KEY_NODE_RATE,    rate_prop);
    pw_properties_set(props, PW_KEY_NODE_LATENCY, latency_prop);

    st->stream_in = pw_stream_new_simple(pw_thread_loop_get_loop(st->tloop),
                                         "info.puredata.Pd", props,
                                         &stream_in_events, st);
    if (!st->stream_in) return -1;

    struct spa_audio_info_raw info;
    spa_zero(info);
    info.format   = SPA_AUDIO_FORMAT_F32;
    info.channels = st->in_channels > 0 ? (uint32_t)st->in_channels : 0;
    info.rate     = sr;

    uint8_t buf[512];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, sizeof(buf));
    const struct spa_pod *params[3];
    uint32_t n_params = 0;

    /* 1) format */
    params[n_params++] = spa_pod_builder_add_object(
        &b, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
        SPA_FORMAT_mediaType,      SPA_POD_Id(SPA_MEDIA_TYPE_audio),
        SPA_FORMAT_mediaSubtype,   SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_AUDIO_format,   SPA_POD_Id(info.format),
        SPA_FORMAT_AUDIO_channels, SPA_POD_Int((int)info.channels),
        SPA_FORMAT_AUDIO_rate,     SPA_POD_Int((int)info.rate));

    /* 2) latency param: request exact blocksize on INPUT */
    struct spa_latency_info lat;
    spa_zero(lat);
    lat.direction   = SPA_DIRECTION_INPUT;
    lat.min_quantum = (float)st->desired_block;
    lat.max_quantum = (float)st->desired_block;

    uint8_t lbuf[256];
    struct spa_pod_builder lb = SPA_POD_BUILDER_INIT(lbuf, sizeof(lbuf));
    params[n_params++] = spa_latency_build(&lb, SPA_PARAM_Latency, &lat);

    /* 3) buffers sized for one block */
    int frames     = st->blocksize;
    int stride     = (int)st->in_channels * (int)sizeof(t_sample);
    int size_bytes = frames * stride;

    params[n_params++] = spa_pod_builder_add_object(
        &b, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
        SPA_PARAM_BUFFERS_buffers, SPA_POD_Int(MAX_BUFFERS),
        SPA_PARAM_BUFFERS_blocks,  SPA_POD_Int(1),
        SPA_PARAM_BUFFERS_size,    SPA_POD_Int(size_bytes),
        SPA_PARAM_BUFFERS_stride,  SPA_POD_Int(stride));

    uint32_t flags = PW_STREAM_FLAG_AUTOCONNECT |
                     PW_STREAM_FLAG_MAP_BUFFERS |
                     PW_STREAM_FLAG_RT_PROCESS;

    if (pw_stream_connect(st->stream_in, PW_DIRECTION_INPUT, PW_ID_ANY, flags, params, n_params) < 0) {
        return -1;
    }
    return 0;
}

static int pw_make_playback_stream(t_pw_state *st)
{
    unsigned sr = (unsigned)(st->samplerate > 0 ? st->samplerate : 48000);

    char rate_prop[32];
    snprintf(rate_prop, sizeof(rate_prop), "%u/1", sr);
    char latency_prop[32];
    snprintf(latency_prop, sizeof(latency_prop), "%u/%u", (unsigned)st->desired_block, sr);

    struct pw_properties *props =
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                          PW_KEY_MEDIA_CATEGORY, "Playback",
                          PW_KEY_MEDIA_ROLE, "Production",
                          NULL);
    pw_properties_set(props, PW_KEY_NODE_RATE,    rate_prop);
    pw_properties_set(props, PW_KEY_NODE_LATENCY, latency_prop);

    st->stream_out = pw_stream_new_simple(pw_thread_loop_get_loop(st->tloop),
                                          "info.puredata.Pd", props,
                                          &stream_out_events, st);
    if (!st->stream_out) return -1;

    struct spa_audio_info_raw info;
    spa_zero(info);
    info.format   = SPA_AUDIO_FORMAT_F32;
    info.channels = st->out_channels > 0 ? (uint32_t)st->out_channels : 0;
    info.rate     = sr;

    uint8_t buf[512];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, sizeof(buf));
    const struct spa_pod *params[3];
    uint32_t n_params = 0;

    /* 1) format */
    params[n_params++] = spa_pod_builder_add_object(
        &b, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
        SPA_FORMAT_mediaType,      SPA_POD_Id(SPA_MEDIA_TYPE_audio),
        SPA_FORMAT_mediaSubtype,   SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_AUDIO_format,   SPA_POD_Id(info.format),
        SPA_FORMAT_AUDIO_channels, SPA_POD_Int((int)info.channels),
        SPA_FORMAT_AUDIO_rate,     SPA_POD_Int((int)info.rate));

    /* 2) latency param: request exact blocksize on OUTPUT */
    struct spa_latency_info lat;
    spa_zero(lat);
    lat.direction   = SPA_DIRECTION_OUTPUT;
    lat.min_quantum = (float)st->desired_block;
    lat.max_quantum = (float)st->desired_block;

    uint8_t lbuf[256];
    struct spa_pod_builder lb = SPA_POD_BUILDER_INIT(lbuf, sizeof(lbuf));
    params[n_params++] = spa_latency_build(&lb, SPA_PARAM_Latency, &lat);

    /* 3) buffers sized for one block */
    int frames     = st->blocksize;
    int stride     = (int)st->out_channels * (int)sizeof(t_sample);
    int size_bytes = frames * stride;

    params[n_params++] = spa_pod_builder_add_object(
        &b, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
        SPA_PARAM_BUFFERS_buffers, SPA_POD_Int(MAX_BUFFERS),
        SPA_PARAM_BUFFERS_blocks,  SPA_POD_Int(1),
        SPA_PARAM_BUFFERS_size,    SPA_POD_Int(size_bytes),
        SPA_PARAM_BUFFERS_stride,  SPA_POD_Int(stride));

    uint32_t flags = PW_STREAM_FLAG_AUTOCONNECT |
                     PW_STREAM_FLAG_MAP_BUFFERS |
                     PW_STREAM_FLAG_RT_PROCESS;

    if (pw_stream_connect(st->stream_out, PW_DIRECTION_OUTPUT, PW_ID_ANY, flags, params, n_params) < 0) {
        return -1;
    }
    return 0;
}

int pipewire_open_audio(int naudioindev, int *audioindev, int nchindev,
    int *chindev, int naudiooutdev, int *audiooutdev, int nchoutdev,
    int *choutdev, int rate, int blocksize)
{
    if (!pw_inited) {
        pw_init(NULL, NULL);
        pw_inited = 1;
    }

    // count chs
    unsigned in_ch = 0, out_ch = 0;
    if (chindev && naudioindev > 0)
        for (int i = 0; i < naudioindev; i++)
            in_ch += (unsigned)((chindev[i] > 0) ? chindev[i] : 0);
    if (choutdev && naudiooutdev > 0)
        for (int i = 0; i < naudiooutdev; i++)
            out_ch += (unsigned)((choutdev[i] > 0) ? choutdev[i] : 0);


    // configure
    memset(&pw_state, 0, sizeof(pw_state));
    pw_state.samplerate    = rate;
    pw_state.blocksize     = blocksize;
    pw_state.desired_block = (unsigned)blocksize;
    pw_state.in_n_devices  = naudioindev;
    pw_state.out_n_devices = naudiooutdev;
    pw_state.in_channels   = in_ch;
    pw_state.out_channels  = out_ch;

#ifdef THREADSIGNAL
    pw_state.sem = sys_semaphore_create();
#endif

    pw_state.tloop = pw_thread_loop_new("Pure Data", NULL);
    if (!pw_state.tloop) {
        logpost(0, PD_CRITICAL, "pipewire failed to create thread loop");
        goto fail;
    }
    if (pw_thread_loop_start(pw_state.tloop) != 0) {
        logpost(0, PD_CRITICAL, "[pipewire] failed to start thread loop");
        goto fail;
    }

    pw_thread_loop_lock(pw_state.tloop);

    // compute advance_samples and align to blocksize
    int advance_samples = (int)(sys_schedadvance * (double)rate / 1.e6);
    if (DEFDACBLKSIZE > 0)
        advance_samples -= (advance_samples % DEFDACBLKSIZE);
    if (advance_samples < blocksize)
        advance_samples = blocksize;

    /* Create sys_ringbufs with backing buffers.
       IMPORTANT: use pw_state.{in,out}_channels here and sizes in BYTES. */
    if (pw_state.in_channels > 0) {
        pw_state.inbuf = malloc(sizeof(t_sample) * STUFF->st_inchannels * advance_samples);
        if (!pw_state.inbuf) {
            pw_thread_loop_unlock(pw_state.tloop);
            goto fail;
        }
        /* start empty; if you want to prefill, memset to 0 and pass insize_bytes as 4th arg */
        sys_ringbuf_init(&pw_state.inring,
            sizeof(t_sample) * STUFF->st_inchannels * advance_samples,
                         pw_state.inbuf,
                         sizeof(t_sample) * STUFF->st_inchannels * advance_samples);
    }

    if (pw_state.out_channels > 0) {
        pw_state.outbuf = malloc(sizeof(t_sample) * STUFF->st_outchannels * advance_samples);
        if (!pw_state.outbuf) {
            pw_thread_loop_unlock(pw_state.tloop);
            goto fail;
        }
        sys_ringbuf_init(&pw_state.outring,
            sizeof(t_sample) * STUFF->st_outchannels * advance_samples,
                         pw_state.outbuf, 0);
    }

    if (pw_state.in_channels > 0) {
        if (pw_make_capture_stream(&pw_state) < 0) {
            logpost(0, PD_CRITICAL, "[pipewire] failed to create capture stream");
            pw_thread_loop_unlock(pw_state.tloop);
            goto fail;
        }
    }
    if (pw_state.out_channels > 0) {
        if (pw_make_playback_stream(&pw_state) < 0) {
            logpost(0, PD_CRITICAL, "[pipewire] failed to create playback stream");
            pw_thread_loop_unlock(pw_state.tloop);
            goto fail;
        }
    }
    pw_thread_loop_unlock(pw_state.tloop);

    pw_state.running = 1;
    pw_state.dio_error = 0;
    return 0;

fail:
    if (pw_state.tloop) {
        pw_thread_loop_unlock(pw_state.tloop);
        pw_thread_loop_stop(pw_state.tloop);
        pw_thread_loop_destroy(pw_state.tloop);
        pw_state.tloop = NULL;
    }
    if (pw_state.stream_in) {
        pw_stream_destroy(pw_state.stream_in);
        pw_state.stream_in = NULL;
    }
    if (pw_state.stream_out){
        pw_stream_destroy(pw_state.stream_out);
        pw_state.stream_out = NULL;
    }

#ifdef THREADSIGNAL
    if (pw_state.sem) { sys_semaphore_destroy(pw_state.sem); pw_state.sem = NULL; }
#endif

    if (pw_state.inbuf)  {
        free(pw_state.inbuf);
        pw_state.inbuf = NULL;
    }
    if (pw_state.outbuf) {
        free(pw_state.outbuf);
        pw_state.outbuf = NULL;
    }
    pw_state.running = 0;
    return 1;
}

void pipewire_close_audio(void)
{
    if (!pw_state.tloop && !pw_state.stream_in && !pw_state.stream_out && !pw_state.inbuf && !pw_state.outbuf)
        return;

    if (pw_state.tloop) {
        pw_thread_loop_lock(pw_state.tloop);
        if (pw_state.stream_in) {
            pw_stream_disconnect(pw_state.stream_in);
            pw_stream_destroy(pw_state.stream_in);
            pw_state.stream_in = NULL;
        }
        if (pw_state.stream_out) {
            pw_stream_disconnect(pw_state.stream_out);
            pw_stream_destroy(pw_state.stream_out);
            pw_state.stream_out = NULL;
        }
        pw_thread_loop_unlock(pw_state.tloop);

        pw_thread_loop_stop(pw_state.tloop);
        pw_thread_loop_destroy(pw_state.tloop);
        pw_state.tloop = NULL;
    }

#ifdef THREADSIGNAL
    if (pw_state.sem) {
        sys_semaphore_destroy(pw_state.sem);
        pw_state.sem = NULL;
    }
#endif

    if (pw_state.inbuf) {
        free(pw_state.inbuf);
        pw_state.inbuf = NULL;
    }
    if (pw_state.outbuf){
        free(pw_state.outbuf);
        pw_state.outbuf = NULL;
    }

    pw_state.running = 0;
    pw_state.dio_error = 0;

    pw_deinit();
    pw_inited = 0;
}

int sched_idletask(void);

/* Move one Pd block between Pd and PipeWire using sys_ringbuf like JACK's polling mode. */
int pipewire_send_dacs(void)
{
    t_sample *muxbuffer;
    t_sample *fp, *fp2, *jp;
    int j, ch;
    const size_t muxbufsize =
        DEFDACBLKSIZE * (pw_state.in_channels > pw_state.out_channels ?
                         pw_state.in_channels : pw_state.out_channels);
    int retval = SENDDACS_YES;

    if (!pw_state.running || (!pw_state.in_channels && !pw_state.out_channels))
        return SENDDACS_NO;

    /* compute required/available in bytes */
    long need_in_bytes  = (long)pw_state.in_channels  * (long)DEFDACBLKSIZE * (long)sizeof(t_sample);
    long need_out_bytes = (long)pw_state.out_channels * (long)DEFDACBLKSIZE * (long)sizeof(t_sample);

    while (
        (pw_state.in_channels  && sys_ringbuf_getreadavailable(&pw_state.inring)  < need_in_bytes) ||
        (pw_state.out_channels && sys_ringbuf_getwriteavailable(&pw_state.outring) < need_out_bytes))
    {
#ifdef THREADSIGNAL
        if (sched_idletask())
            continue;
        if (pw_state.sem)
            sys_semaphore_wait(pw_state.sem);
        retval = SENDDACS_SLEPT;
#else
        return SENDDACS_NO;
#endif
    }

    if (pw_state.dio_error) {
        /* log a resync like JACK backend does */
        sys_log_error(ERR_RESYNC);
        pw_state.dio_error = 0;
    }

    ALLOCA(t_sample, muxbuffer, muxbufsize, MAX_ALLOCA_SAMPLES);

    /* Input: de-interleave from ring to STUFF->st_soundin */
    if (pw_state.in_channels)
    {
        sys_ringbuf_read(&pw_state.inring, muxbuffer, need_in_bytes, pw_state.inbuf);
        for (fp = muxbuffer, ch = 0; ch < (int)pw_state.in_channels; ch++, fp++)
        {
            jp = STUFF->st_soundin + ch * DEFDACBLKSIZE;
            for (j = 0, fp2 = fp; j < DEFDACBLKSIZE;
                 j++, fp2 += pw_state.in_channels)
            {
                jp[j] = *fp2;
            }
        }
    }

    /* Output: interleave from STUFF->st_soundout to ring */
    if (pw_state.out_channels)
    {
        for (fp = muxbuffer, ch = 0; ch < (int)pw_state.out_channels; ch++, fp++)
        {
            jp = STUFF->st_soundout + ch * DEFDACBLKSIZE;
            for (j = 0, fp2 = fp; j < DEFDACBLKSIZE;
                 j++, fp2 += pw_state.out_channels)
            {
                *fp2 = jp[j];
            }
        }
        sys_ringbuf_write(&pw_state.outring, muxbuffer, need_out_bytes, pw_state.outbuf);
    }

    /* clear Pd's output buffer for next block (as in JACK backend) */
    if (pw_state.out_channels && STUFF->st_soundout)
        memset(STUFF->st_soundout, 0,
               (size_t)DEFDACBLKSIZE * (size_t)pw_state.out_channels * sizeof(t_sample));

    FREEA(t_sample, muxbuffer, muxbufsize, MAX_ALLOCA_SAMPLES);
    return retval;
}

void pipewire_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti,
        int maxndev, int devdescsize)
{
    int i, ndev;
    *canmulti = 0;  /* supports multiple devices */
    ndev = 1;
    for (i = 0; i < ndev; i++)
    {
        sprintf(indevlist + i * devdescsize, "PipeWire");
        sprintf(outdevlist + i * devdescsize, "PipeWire");
    }
    *nindevs = *noutdevs = ndev;
}
#endif

