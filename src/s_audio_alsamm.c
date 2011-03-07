/* Copyright (c) 1997-2003 Guenter Geiger, Miller Puckette, Larry Troxler,
* Winfried Ritsch, Karl MacMillan, and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* 
   this audiodriverinterface inputs and outputs audio data using 
   the ALSA MMAP API available on linux. 
   this is optimized for hammerfall cards and does not make an attempt to be general
   now, please adapt to your needs or let me know ...
   constrains now:
    - audio Card with ALSA-Driver > 1.0.3, 
    - alsa-device (preferable hw) with MMAP NONINTERLEAVED SIGNED-32Bit features
    - up to 4 cards with has to be hardwaresynced
   (winfried)
*/
#include <alsa/asoundlib.h>

#include "m_pd.h"
#include "s_stuff.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sched.h>
#include "s_audio_alsa.h"

/* needed for alsa 0.9 compatibility: */
#if (SND_LIB_MAJOR < 1)
#define ALSAAPI9
#endif
/* sample type magic ... 
   Hammerfall/HDSP/DSPMADI cards always 32Bit where lower 8Bit not used (played) in AD/DA, 
   but can have some bits set (subchannel coding)
*/
/* sample type magic ... 
   Hammerfall/HDSP/DSPMADI cards always 32Bit where lower 8Bit not used (played) in AD/DA, 
   but can have some bits set (subchannel coding)
*/

#define ALSAMM_SAMPLEWIDTH_32 sizeof(t_alsa_sample32)

#ifndef INT32_MAX
#define INT32_MAX 0x7fffffff
#endif

/* maybe:
    don't assume we can turn all 31 bits when doing float-to-fix; 
    otherwise some audio drivers (e.g. Midiman/ALSA) wrap around. 

    but not now on hammerfall (w)
*/

/* 24 Bit are used so MAX Samplevalue not INT32_MAX ??? */
#define F32MAX 0x7fffff00
#define CLIP32(x) (((x)>F32MAX)?F32MAX:((x) < -F32MAX)?-F32MAX:(x))

#define ALSAMM_FORMAT SND_PCM_FORMAT_S32
/*
   maximum of 4 devices 
   you can mix rme9632,hdsp9632 (18 chans) rme9652,hdsp9652 (26 chans), dsp-madi (64 chans) 
   if synced
*/

/* the concept is holding data for each device
   where a device is in and output and has one name.

   but since we can use only ins or only outs or both
   on each hardware we list them  in used_???device 
   for alsa seperated for inputs and outputs

   due the need to link in and out-device on one card
   and do only one mmap prepare and start for in and out 
   the concept turns out to be not very efficient,
   so  i will change it maybe in future...
*/

static int alsamm_incards = 0;
static t_alsa_dev *alsamm_indevice[ALSA_MAXDEV];
static int alsamm_outcards = 0;
static t_alsa_dev *alsamm_outdevice[ALSA_MAXDEV];

/*
   we need same samplerate, buffertime and so on for
   each card soo we use global vars...
   time is in us,   size in frames (i hope so ;-)
*/
static unsigned int alsamm_sr = 0; 
static unsigned int alsamm_buffertime = 0;
static unsigned int alsamm_buffersize = 0;
static int alsamm_transfersize = DEFDACBLKSIZE;

/* bad style: we asume all cards give the same answer at init so we make this vars global
   to have a faster access in writing reading during send_dacs */
static snd_pcm_sframes_t alsamm_period_size; 
static unsigned int alsamm_periods;
static snd_pcm_sframes_t alsamm_buffer_size;

/* if more than this sleep detected, should be more than periodsize/samplerate ??? */
static double sleep_time; 

/* now we just sum all inputs/outputs of used cards to a global count
   and use them all
   ... later we should just use some channels of each card for pd
   so we reduce the overhead of using alsways all channels,
   and zero the rest once at start,
   because rme9652 and hdsp forces us to use all channels 
   in mmap mode... 

Note on why:
   normally hdsp and dspmadi can handle channel
   count from one to all since they can switch on/off
   the dma for them to reduce pci load, but this is only
   implemented in alsa low level drivers for dspmadi now and maybe fixed for hdsp in future
*/

static int alsamm_inchannels = 0;
static int alsamm_outchannels = 0;

/* Defines */
/*#define ALSAMM_DEBUG */
#ifdef ALSAMM_DEBUG

 #define DEBUG(x) x
 #define DEBUG2(x) {x;}

 #define WATCH_PERIODS 90

 static int in_avail[WATCH_PERIODS];
 static  int out_avail[WATCH_PERIODS];
 static  int in_offset[WATCH_PERIODS];
 static  int out_offset[WATCH_PERIODS];
 static  int out_cm[WATCH_PERIODS];
 static  char *outaddr[WATCH_PERIODS];
 static  char *inaddr[WATCH_PERIODS];
 static  char *outaddr2[WATCH_PERIODS];
 static  char *inaddr2[WATCH_PERIODS];
 static  int xruns_watch[WATCH_PERIODS];
 static  int broken_opipe;

 static int dac_send = 0;
 static int alsamm_xruns = 0;

static void show_availist(void)
{
  int i;
  for(i=1;i<WATCH_PERIODS;i++){
    post("%2d:avail i=%7d %s o=%7d(%5d), offset i=%7d %s o=%7d, ptr i=%12p o=%12p, %d xruns ",
         i,in_avail[i],(out_avail[i] != in_avail[i])? "!=" : "==" , out_avail[i],out_cm[i],
         in_offset[i],(out_offset[i] != in_offset[i])? "!=" : "==" , out_offset[i],
         inaddr[i], outaddr[i], xruns_watch[i]);
  }
}
#else
 #define DEBUG(x) 
 #define DEBUG2(x) {}
#endif

/* protos */
static char *alsamm_getdev(int nr);

static int set_hwparams(snd_pcm_t *handle,
                               snd_pcm_hw_params_t *params, int *chs);
static int set_swparams(snd_pcm_t *handle,
                               snd_pcm_sw_params_t *swparams, int playback);

static int alsamm_start(void);
static int alsamm_stop(void);

/* for debugging attach output of alsa mesages to stdout stream */
snd_output_t* alsa_stdout; 

static void check_error(int err, const char *why)
{
    if (err < 0)
        error("%s: %s\n", why, snd_strerror(err));
}

int alsamm_open_audio(int rate, int blocksize)
{
  int err;
  char devname[80];
  char *cardname;
  snd_pcm_hw_params_t* hw_params;
  snd_pcm_sw_params_t* sw_params;


  /* fragsize is an old concept now use periods, used to be called fragments. */
  /* Be aware in ALSA periodsize can be in bytes, where buffersize is in frames, 
     but sometimes buffersize is in bytes and periods in frames, crazy alsa...      
     ...we use periodsize and buffersize in frames */

  int i;
  short* tmp_buf;
  unsigned int tmp_uint;

  snd_pcm_hw_params_alloca(&hw_params);
  snd_pcm_sw_params_alloca(&sw_params);
  
  /* see add_devname */
  /* first have a look which cards we can get and
     set up device infos for them */

#ifdef ALSAMM_DEBUG
  if(sys_verbose)
    post("naudioindev=%d,  nchindev=%d, naudiooutdev=%d, nchoutdev=%d,rate=%d",
         naudioindev, nchindev,naudiooutdev, nchoutdev, rate);
#endif

  /* init some structures */
  for(i=0;i < ALSA_MAXDEV;i++){
    alsa_indev[i].a_synced=alsa_outdev[i].a_synced=0;
    alsa_indev[i].a_channels=alsa_outdev[i].a_channels=-1; /* query defaults */
  }
  alsamm_inchannels = 0;
  alsamm_outchannels = 0;

  /* opening alsa debug channel */
  err = snd_output_stdio_attach(&alsa_stdout, stdout, 0);
  if (err < 0) {
    check_error(err,"attaching alsa debug Output to stdout failed");
    /*    return; no so bad ... and never should happe */
  }

  
  /* 
     Weak failure prevention:
     first card found (out then in) is used as a reference for parameter,
     so this  set the globals and other cards hopefully dont change them
  */
  alsamm_sr = rate;
  
  /* set the asked buffer time (alsa buffertime in us)*/  
  alsamm_buffertime = alsamm_buffersize = 0;
  if(blocksize == 0)
    alsamm_buffertime = sys_schedadvance;
  else
    alsamm_buffersize = blocksize;
   
  if(sys_verbose)
    post("syschedadvance=%d us(%d Samples)so buffertime max should be this=%d" 
         "or sys_blocksize=%d (samples) to use buffersize=%d",
         sys_schedadvance,sys_advance_samples,alsamm_buffertime,
         blocksize,alsamm_buffersize);
  
  alsamm_periods = 0; /* no one wants periods setting from command line ;-) */

  for(i=0;i<alsa_noutdev;i++)
  {  
        /*   post("open audio out %d, of %lx, %d",i,&alsa_device[i],
                   alsa_outdev[i].a_handle); */
      if((err = set_hwparams(alsa_outdev[i].a_handle, hw_params,
                             &(alsa_outdev[i].a_channels))) < 0)
        {
          check_error(err,"playback device hwparam_set error:");        
          continue;
        }

      if((err = set_swparams(alsa_outdev[i].a_handle, sw_params,1)) < 0){
        check_error(err,"playback device swparam_set error:");  
        continue;
      }

      alsamm_outchannels += alsa_outdev[i].a_channels;

      alsa_outdev[i].a_addr = 
        (char **) malloc(sizeof(char *) *  alsa_outdev[i].a_channels);

      if(alsa_outdev[i].a_addr == NULL){
        check_error(errno,"playback device outaddr allocation error:"); 
        continue;
      }
      memset(alsa_outdev[i].a_addr, 0, sizeof(char*) * alsa_outdev[i].a_channels);

      post("playback device with %d channels and buffer_time %d us opened", 
           alsa_outdev[i].a_channels, alsamm_buffertime);
    }


  for(i=0;i<alsa_nindev;i++)
    {
      
      if(sys_verbose)
        post("capture card %d:--------------------",i);
      
      if((err = set_hwparams(alsa_indev[i].a_handle, hw_params,
                               &(alsa_indev[i].a_channels))) < 0)
        {
          check_error(err,"capture device hwparam_set error:"); 
          continue;
        }

      alsamm_inchannels += alsa_indev[i].a_channels; 

      if((err = set_swparams(alsa_indev[i].a_handle,
            sw_params,0)) < 0){
        check_error(err,"capture device swparam_set error:");   
        continue;
      }

      alsa_indev[i].a_addr = 
        (char **) malloc (sizeof(char*) *  alsa_indev[i].a_channels);

      if(alsa_indev[i].a_addr == NULL){
        check_error(errno,"capture device inaddr allocation error:");   
        continue;
      }
      
      memset(alsa_indev[i].a_addr, 0, sizeof(char*) * alsa_indev[i].a_channels);
      
      if(sys_verbose)
        post("capture device with %d channels and buffertime %d us opened\n", 
             alsa_indev[i].a_channels,alsamm_buffertime);
    }

  /* check for linked handles of input for each output*/
  
  for(i=0; i<(alsa_noutdev < alsa_nindev ? alsa_noutdev:alsa_nindev); i++){
    t_alsa_dev *ad = &alsa_outdev[i];

    if (alsa_outdev[i].a_devno == alsa_indev[i].a_devno){
      if ((err = snd_pcm_link (alsa_indev[i].a_handle,
                               alsa_outdev[i].a_handle)) == 0){
        alsa_indev[i].a_synced = alsa_outdev[i].a_synced = 1;
        if(sys_verbose)
          post("Linking in and outs of card %d",i);
      }
      else
        check_error(err,"could not link in and outs");
    }
  }

  /* some globals */
  sleep_time =  (float) alsamm_period_size/ (float) alsamm_sr;


#ifdef ALSAMM_DEBUG
  /* start ---------------------------- */
  if(sys_verbose)
    post("open_audio: after dacsend=%d (xruns=%d)done",dac_send,alsamm_xruns);
  alsamm_xruns = dac_send = 0; /* reset debug */

  /* start alsa in open or better in send_dacs once ??? we will see */

  for(i=0;i<alsa_noutdev;i++)
    snd_pcm_dump(alsa_outdev[i].a_handle, alsa_stdout);

  for(i=0;i<alsa_nindev;i++)
    snd_pcm_dump(alsa_indev[i].inhandle, alsa_stdout);

  fflush(stdout);
#endif

  sys_setchsr(alsamm_inchannels,  alsamm_outchannels, alsamm_sr);

  alsamm_start();

  /* report success  */   
  return (0);
}


void alsamm_close_audio(void)
{
  int i,err;


#ifdef ALSAMM_DEBUG
  if(sys_verbose)
    post("closing devices");
#endif

  alsamm_stop();

  for(i=0;i< alsa_noutdev;i++){


#ifdef ALSAMM_DEBUG
    if(sys_verbose)
      post("unlink audio out %d, of %lx",i,used_outdevice[i]);
#endif

    if(alsa_outdev[i].a_synced != 0){
      if((err = snd_pcm_unlink(alsa_outdev[i].a_handle)) < 0)
        check_error(err, "snd_pcm_unlink (output)");
      alsa_outdev[i].a_synced = 0;
     }

    if((err = snd_pcm_close(alsa_outdev[i].a_handle)) <= 0)
      check_error(err, "snd_pcm_close (output)");

    if(alsa_outdev[i].a_addr){
      free(alsa_outdev[i].a_addr);
      alsa_outdev[i].a_addr = NULL;
    }
    alsa_outdev[i].a_channels = 0;
  }

  for(i=0;i< alsa_nindev;i++){

    err = snd_pcm_close(alsa_indev[i].a_handle);

    if(sys_verbose)
      check_error(err, "snd_pcm_close (input)");

    if(alsa_indev[i].a_addr){
      free(alsa_indev[i].a_addr);
      alsa_indev[i].a_addr = NULL;
    }

    alsa_indev[i].a_channels = 0;
  }
  alsa_nindev = alsa_noutdev = 0;
#ifdef ALSAMM_DEBUG
  if(sys_verbose)
    post("close_audio: after dacsend=%d (xruns=%d)done",dac_send,alsamm_xruns);
   alsamm_xruns = dac_send = 0;
#endif
}

/* ------- PCM INITS --------------------------------- */
static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params,int *chs)
{
#ifndef ALSAAPI9
  unsigned int rrate;
  int err, dir;
  int channels_allocated = 0;

  /* choose all parameters */
  err = snd_pcm_hw_params_any(handle, params);
  if (err < 0) {
    check_error(err,"Broken configuration: no configurations available"); 
    return err;
  }

  /* set the nointerleaved read/write format */
  err = snd_pcm_hw_params_set_access(handle, params, 
                                     SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
  if (err >= 0) {
#ifdef ALSAMM_DEBUG 
    if(sys_verbose)
      post("Access type %s available","SND_PCM_ACCESS_MMAP_NONINTERLEAVED"); 
#endif
  }
  else{
    check_error(err,"No Accesstype SND_PCM_ACCESS_MMAP_NONINTERLEAVED");
    return err;
  }

  /* set the sample format */
  err = snd_pcm_hw_params_set_format(handle, params, ALSAMM_FORMAT);
  if (err < 0) {
    check_error(err,"Sample format not available for playback");
    return err;
  }

#ifdef ALSAMM_DEBUG
  if(sys_verbose)
    post("Setting format to %s",snd_pcm_format_name(ALSAMM_FORMAT));
#endif

  /* first check samplerate since channels numbers 
     are samplerate dependend (double speed) */
  /* set the stream rate */

  rrate = alsamm_sr;

#ifdef ALSAMM_DEBUG
  if(sys_verbose)
    post("Samplerate request: %i Hz",rrate);
#endif

  dir=-1;
  err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, &dir);
  if (err < 0) {
    check_error(err,"Rate not available");
    return err;
  }
  
  if (rrate != alsamm_sr) {
    post("Warning: rate %iHz doesn't match requested %iHz", rrate,alsamm_sr);
    alsamm_sr = rrate;
  }
  else
    if(sys_verbose)
      post("Samplerate is set to %iHz",alsamm_sr);

  /* Info on channels */
  { 
    int maxchs,minchs,channels = *chs;

    if((err = snd_pcm_hw_params_get_channels_max(params,
        (unsigned int *)&maxchs)) < 0){
      check_error(err,"Getting channels_max not available");
      return err;
    }
    if((err = snd_pcm_hw_params_get_channels_min(params,
        (unsigned int *)&minchs)) < 0){
      check_error(err,"Getting channels_min not available");
      return err;
    }

#ifdef ALSAMM_DEBUG
    if(sys_verbose)
      post("Getting channels:min=%d, max= %d for request=%d",minchs,maxchs,channels);
#endif
    if(channels < 0)channels=maxchs;
    if(channels > maxchs)channels = maxchs;
    if(channels < minchs)channels = minchs;

    if(channels != *chs)
      post("requested channels=%d but used=%d",*chs,channels);
 
    *chs = channels;
#ifdef ALSAMM_DEBUG
    if(sys_verbose)
      post("trying to use channels: %d",channels);
#endif
  }

  /* set the count of channels */
  err = snd_pcm_hw_params_set_channels(handle, params, *chs);
  if (err < 0) {
    check_error(err,"Channels count not available");
    return err;
  }

  /* testing for channels */
  if((err = snd_pcm_hw_params_get_channels(params,(unsigned int *)chs)) < 0)
    check_error(err,"Get channels not available");
#ifdef ALSAMM_DEBUG
  else
    if(sys_verbose)
      post("When setting channels count and got %d",*chs);
#endif

  /* if buffersize is set use this instead buffertime */
  if(alsamm_buffersize > 0){

#ifdef ALSAMM_DEBUG
    if(sys_verbose)
      post("hw_params: ask for max buffersize of %d samples", 
           (unsigned int) alsamm_buffersize );
#endif

    alsamm_buffer_size = alsamm_buffersize;

    err = snd_pcm_hw_params_set_buffer_size_near(handle, params, 
        (unsigned long *)&alsamm_buffer_size);
    if (err < 0) {
      check_error(err,"Unable to set max buffer size");
      return err;
    }

  }
  else{
    if(alsamm_buffertime <= 0) /* should never happen, but use 20ms */
      alsamm_buffertime = 20000;

#ifdef ALSAMM_DEBUG
    if(sys_verbose)
      post("hw_params: ask for max buffertime of %d ms", 
           (unsigned int) (alsamm_buffertime*0.001) );
#endif

    err = snd_pcm_hw_params_set_buffer_time_near(handle, params,
        &alsamm_buffertime, &dir);
    if (err < 0) {
      check_error(err,"Unable to set max buffer time");
      return err;
    }
  }

  err = snd_pcm_hw_params_get_buffer_time(params, 
    (unsigned int *)&alsamm_buffertime, &dir);
  if (err < 0) {
    check_error(err,"Unable to get buffer time");
    return err;
  }

#ifdef ALSAMM_DEBUG
  if(sys_verbose)
    post("hw_params: got buffertime to %f ms",
         (float) (alsamm_buffertime*0.001));
#endif

  err = snd_pcm_hw_params_get_buffer_size(params, 
    (unsigned long *)&alsamm_buffer_size);
  if (err < 0) {
    check_error(err,"Unable to get buffer size");
    return err;
  }

#ifdef ALSAMM_DEBUG
  if(sys_verbose)
    post("hw_params: got  buffersize to %d samples",(int) alsamm_buffer_size);
#endif

  err = snd_pcm_hw_params_get_period_size(params, 
    (unsigned long *)&alsamm_period_size, &dir);
  if (err > 0) {
    check_error(err,"Unable to get period size");
    return err;
  }

#ifdef ALSAMM_DEBUG
  if(sys_verbose)
    post("Got period size of %d", (int) alsamm_period_size);
#endif  
  { 
    unsigned int pmin,pmax;

    err = snd_pcm_hw_params_get_periods_min(params, &pmin, &dir);
    if (err > 0) {
      check_error(err,"Unable to get period size");
      return err;
    }
    err = snd_pcm_hw_params_get_periods_min(params, &pmax, &dir);
    if (err > 0) {
      check_error(err,"Unable to get period size");
      return err;
    }

    /* use maximum of periods */
    if( alsamm_periods <= 0)
      alsamm_periods = pmax;
    alsamm_periods = (alsamm_periods > pmax)?pmax:alsamm_periods;
    alsamm_periods = (alsamm_periods < pmin)?pmin:alsamm_periods;

    err = snd_pcm_hw_params_set_periods(handle, params, alsamm_periods, dir);
    if (err > 0) {
      check_error(err,"Unable to set periods");
      return err;
    }


    err = snd_pcm_hw_params_get_periods(params, &pmin, &dir);
    if (err > 0) {
      check_error(err,"Unable to get periods");
      return err;
    }
#ifdef ALSAMM_DEBUG
    if(sys_verbose)
      post("Got periods of %d, where periodsmin=%d, periodsmax=%d", 
           alsamm_periods,pmin,pmax);
#endif
  }

  /* write the parameters to device */
  err = snd_pcm_hw_params(handle, params);
  if (err < 0) {
    check_error(err,"Unable to set hw params");
    return err;
  }
#endif /* ALSAAPI9 */
  return 0;
}

static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams, int playback)
{
#ifndef ALSAAPI9
  int err;
  snd_pcm_uframes_t ps,ops;
  snd_pcm_uframes_t bs,obs;

  /* get the current swparams */
  err = snd_pcm_sw_params_current(handle, swparams);
  if (err < 0) {
    check_error(err,"Unable to determine current swparams for playback");
    return err;
  }
  
  /* AUTOSTART: start the transfer on each write/commit ??? */
  snd_pcm_sw_params_get_start_threshold(swparams, &obs);

  err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 0U);
  if (err < 0) {
    check_error(err,"Unable to set start threshold mode");
    return err;
  }

  snd_pcm_sw_params_get_start_threshold(swparams, &bs);

#ifdef ALSAMM_DEBUG
  if(sys_verbose)
    post("sw_params: got start_thresh_hold= %d (was %d)",(int) bs,(int)obs);
#endif

  /* AUTOSTOP:  never stop the machine */

  snd_pcm_sw_params_get_stop_threshold(swparams, &obs);

  err = snd_pcm_sw_params_set_stop_threshold(handle, swparams, (snd_pcm_uframes_t)-1);
  if (err < 0) {
    check_error(err,"Unable to set stop threshold mode");
    return err;
  }

  snd_pcm_sw_params_get_stop_threshold(swparams, &bs);
#ifdef ALSAMM_DEBUG
  if(sys_verbose)
    post("sw_params: set stop_thresh_hold= %d (was %d)", (int) bs,(int)obs);
#endif


  /* AUTOSILENCE: silence if overrun.... */

  snd_pcm_sw_params_get_silence_threshold (swparams, &ops);
  if ((err = snd_pcm_sw_params_set_silence_threshold (handle, swparams, alsamm_period_size)) < 0) {
    check_error (err,"cannot set silence threshold for");
    return -1;
  }
  snd_pcm_sw_params_get_silence_threshold (swparams, &ps);
#ifdef ALSAMM_DEBUG
  if(sys_verbose)
    post("sw_params: set silence_threshold = %d (was %d)", (int) ps,(int)ops);
#endif

  snd_pcm_sw_params_get_silence_size (swparams, &ops);
  if ((err = snd_pcm_sw_params_set_silence_size(handle, swparams, alsamm_period_size)) < 0) {
    check_error (err,"cannot set silence size for");
    return -1;
  }
  snd_pcm_sw_params_get_silence_size (swparams, &ps);
#ifdef ALSAMM_DEBUG
  if(sys_verbose)
    post("sw_params: set silence_size = %d (was %d)", (int) ps,(int)ops);
#endif

  /* AVAIL: allow the transfer when at least period_size samples can be processed */
  
  snd_pcm_sw_params_get_avail_min(swparams, &ops);
 
  err = snd_pcm_sw_params_set_avail_min(handle, swparams, alsamm_transfersize/2);
  if (err < 0) {
    check_error(err,"Unable to set avail min for");
    return err;
    }

  snd_pcm_sw_params_get_avail_min(swparams, &ps);
#ifdef ALSAMM_DEBUG
  if(sys_verbose)
    post("sw_params: set avail_min= %d (was  %d)", (int) ps, (int) ops);
#endif
  
  /* write the parameters to the playback device */

  err = snd_pcm_sw_params(handle, swparams);
  if (err < 0) {
    check_error(err,"Unable to set sw params");
    return err;
  }

#ifdef ALSAMM_DEBUG
  if(sys_verbose)
    post("set sw finished");
#endif
#else
  post("alsa: need version 1.0 or above for mmap operation");
#endif /* ALSAAPI9 */
  return 0;
}

/* ALSA Transfer helps */

/* xrun_recovery is called if time to late or error 

  Note: use outhandle if synced i/o
        the devices are linked so prepare
        has only be called on out,
        hopefully resume too...
*/

static int xrun_recovery(snd_pcm_t *handle, int err)
{
#ifdef ALSAMM_DEBUG
  alsamm_xruns++; /* count xruns */
#endif

  if (err == -EPIPE) {    /* under-run */
    err = snd_pcm_prepare(handle);
    if (err < 0)
      check_error(err,"Can't recovery from underrun, prepare failed.");

    err = snd_pcm_start(handle);
    if (err < 0)
      check_error(err,"Can't start when recover from underrun.");

    return 0;
  } else if (err == -ESTRPIPE) {
    while ((err = snd_pcm_resume(handle)) == -EAGAIN)
      sleep(1);       /* wait until the suspend flag is released */
    if (err < 0) {
      err = snd_pcm_prepare(handle);
      if (err < 0)
        check_error(err,"Can't recovery from suspend, prepare failed.");

      err = snd_pcm_start(handle);
      if (err < 0)
        check_error(err,"Can't start when recover from underrun.");
    }
    return 0;
  }

  return err;
}

/* note that snd_pcm_avail has to be called before using this funtion */

static int alsamm_get_channels(snd_pcm_t *dev, 
                               snd_pcm_uframes_t *avail,
                               snd_pcm_uframes_t *offset,
                               int nchns, char **addr)
{
  int err = 0;
  int chn;
  const snd_pcm_channel_area_t *mm_areas;


  if (nchns > 0 && avail != NULL && offset != NULL) {

    if ((err = snd_pcm_mmap_begin(dev, &mm_areas, offset, avail)) < 0){
      check_error(err,"setmems: begin_mmap failure ???");
      return err;
    }
                
    for (chn = 0; chn < nchns; chn++) {
      const snd_pcm_channel_area_t *a = &mm_areas[chn];
      addr[chn] = (char *) a->addr + ((a->first + a->step * *offset) / 8);
    }

    return err;
  } 

  return -1;
}


static int alsamm_start()
{
  int err = 0;
  int devno;
  int chn,nchns;

  const snd_pcm_channel_area_t *mm_areas;

#ifdef ALSAMM_DEBUG
  if(sys_verbose)
    post("start audio with %d out cards and %d incards",alsamm_outcards,alsamm_incards);
#endif

  /* first prepare for in/out */
  for(devno = 0;devno < alsa_noutdev;devno++){

    snd_pcm_uframes_t offset, avail;
    t_alsa_dev *dev = &alsa_outdev[devno];

    /* snd_pcm_prepare also in xrun, but cannot harm here */
    if ((err = snd_pcm_prepare (dev->a_handle)) < 0) {
      check_error (err,"outcard prepare error for playback");
      return err;
    }

    offset = 0;
    avail = snd_pcm_avail_update(dev->a_handle);

    if (avail != (snd_pcm_uframes_t) alsamm_buffer_size) {
      check_error (avail,"full buffer not available at start");
    }

    /* cleaning out mmap buffer before start */

#ifdef ALSAMM_DEBUG
    if(sys_verbose)
      post("start: set mems for avail=%d,offset=%d at buffersize=%d",
           avail,offset,alsamm_buffer_size);
#endif

    if(avail > 0){

      int comitted = 0;

      if ((err = alsamm_get_channels(dev->a_handle, &avail, &offset,
                                     dev->a_channels,dev->a_addr)) < 0) {
        check_error(err,"setting initial out channelspointer failure ?");
        continue;
      }

      for (chn = 0; chn < dev->a_channels; chn++) 
        memset(dev->a_addr[chn],0,avail*ALSAMM_SAMPLEWIDTH_32); 
    
      comitted = snd_pcm_mmap_commit (dev->a_handle, offset, avail);

      avail = snd_pcm_avail_update(dev->a_handle);

#ifdef ALSAMM_DEBUG
      if(sys_verbose)
        post("start: now channels cleared, out with avail=%d, offset=%d,comitted=%d",
             avail,offset,comitted);
#endif      
    }
    /* now start, should be autostarted */
    avail = snd_pcm_avail_update(dev->a_handle);

#ifdef ALSAMM_DEBUG
    if(sys_verbose)
      post("start: finish start, out with avail=%d, offset=%d",avail,offset);
#endif

    /* we have no autostart so anyway start*/
    if ((err = snd_pcm_start (dev->a_handle)) < 0) {
      check_error (err,"could not start playback");
    }
  }

  for(devno = 0;devno < alsa_nindev;devno++){

    snd_pcm_uframes_t ioffset, iavail;
    t_alsa_dev *dev = &alsa_indev[devno];

    /* if devices are synced then dont need to prepare
       hopefully dma in aereas allready filled correct by the card */

    if(dev->a_synced == 0){
      if ((err = snd_pcm_prepare (dev->a_handle)) < 0) {
        check_error (err,"incard prepare error for capture");
        /*      return err;*/
      }
    }

    ioffset = 0;
    iavail = snd_pcm_avail_update (dev->a_handle);

    /* cleaning out mmap buffer before start */
#ifdef ALSAMM_DEBUG
    post("start in: set in mems for avail=%d,offset=%d at buffersize=%d",
         iavail,ioffset,alsamm_buffer_size);
#endif
    
    if (iavail > (snd_pcm_uframes_t) 0) {

#ifdef ALSAMM_DEBUG
      post("empty buffer not available at start, since avail %d != %d buffersize",
           iavail, alsamm_buffer_size);
#endif

      if ((err = alsamm_get_channels(dev->a_handle, &iavail, &ioffset,
                                     dev->a_channels,dev->a_addr)) < 0) {
        check_error(err,"getting in channelspointer failure ????");
        continue;
      }

      snd_pcm_mmap_commit (dev->a_handle, ioffset, iavail);

      iavail = snd_pcm_avail_update (dev->a_handle);
#ifdef ALSAMM_DEBUG
      post("start in now avail=%d",iavail);    
#endif
    }

#ifdef ALSAMM_DEBUG
     post("start: init inchannels with avail=%d, offset=%d",iavail,ioffset);
#endif
    
    /* if devices are synced then dont need to start */
    /* start with autostart , but anyway start */
    if(dev->a_synced == 0){
      if ((err = snd_pcm_start (dev->a_handle)) < 0) {
        check_error (err,"could not start capture");
        continue;
      }
    }

  }

  return err;
}

static int alsamm_stop()
{
  int err = 0;
  int devno;

  /* first stop in... */
  for(devno = 0;devno < alsa_nindev;devno++){

    t_alsa_dev *dev = &alsa_indev[devno];

    if(sys_verbose)
      post("stop in device %d",devno);

    if ((err = snd_pcm_drop(dev->a_handle)) < 0) {
      check_error (err,"channel flush for capture failed");
    }

  }

  /* then outs */
  for(devno = 0;devno < alsa_noutdev;devno++){


    t_alsa_dev *dev = &alsa_outdev[devno];
    if(sys_verbose)
      post("stop out device %d",devno);

    if ((err = snd_pcm_drop(dev->a_handle)) < 0) {
      check_error (err,"channel flush for playback failed");
    }

  }
  
#ifdef ALSAMM_DEBUG
  show_availist();
#endif

  return err;
}



/* ---------- ADC/DAC tranfer in  the main loop ------- */

/* I see: (a guess as a documentation)

   all DAC data is in sys_soundout array with 
   DEFDACBLKSIZE (mostly 64) for each channels which
   if we have more channels opened then dac-channels = sys_outchannels
   we have to zero (silence them), which should be done once.

Problems to solve:

   a) Since in ALSA MMAP, the MMAP reagion can change (dont ask me why)
   we have to do it each cycle or we say on RME HAMMERFALL/HDSP/DSPMADI 
   it never changes to it once. so maybe we can do it once in open

   b) we never know if inputs are synced and zero them if not, 
   except we use the control interface to check for, but this is 
   a systemcall we cannot afford in RT Loops so we just dont
   and if not it will click... users fault ;-)))
 
*/

int alsamm_send_dacs(void)
{
  
  static double timenow,timelast; 
  
  t_sample *fpo, *fpi, *fp1, *fp2;
  int i, err, devno; 

  const snd_pcm_channel_area_t *my_areas;
  snd_pcm_sframes_t size;
  snd_pcm_sframes_t commitres;
  snd_pcm_state_t state;
  snd_pcm_sframes_t ooffset, oavail;
  snd_pcm_sframes_t ioffset, iavail;

  /* 
     unused channels should be zeroed out on startup (open) and stay this
  */
  int inchannels = sys_inchannels;
  int outchannels = sys_outchannels;

  timelast = sys_getrealtime();

#ifdef ALSAMM_DEBUG
  if(dac_send++ < 0)
    post("dac send called in %d, out %d, xrun %d",inchannels,outchannels, alsamm_xruns);

  if(alsamm_xruns && (alsamm_xruns % 1000) == 0) 
    post("1000 xruns accoured");

  if(dac_send < WATCH_PERIODS){
    out_cm[dac_send] = -1;
    in_avail[dac_send] = out_avail[dac_send] = -1;
    in_offset[dac_send] = out_offset[dac_send] = -1;
    outaddr[dac_send] = inaddr[dac_send] = NULL;
    xruns_watch[dac_send] = alsamm_xruns;
  }
#endif

  if (!inchannels && !outchannels)
    {
      return SENDDACS_NO;
    }

  /* here we should check if in and out samples are here.
     but, the point is if out samples available also in sample should,
     so we dont make a precheck of insamples here and let outsample check be the
     the first of the forst card.
  */


  /* OUTPUT Transfer */
  fpo = sys_soundout;
  for(devno = 0;devno < alsa_noutdev;devno++){

    t_alsa_dev *dev = &alsa_outdev[devno];
    snd_pcm_t *out = dev->a_handle;
    int ochannels =dev->a_channels;



    /* how much samples available ??? */
    oavail = snd_pcm_avail_update(out); 
    
    /* only one reason i can think about,
       the driver stopped and says broken pipe
       so this should not happen if we have enough stopthreshhold
       but if try to restart with next commit
    */
    if (oavail < 0) {

#ifdef ALSAMM_DEBUG
      broken_opipe++;
#endif
      err = xrun_recovery(out, -EPIPE); 
      if (err < 0) {
        check_error(err,"otavail<0 recovery failed");
        return SENDDACS_NO;     
      }
      oavail = snd_pcm_avail_update(out); 
    }

    /* check if we are late and have to (able to) catch up */
    /* xruns will be ignored since you cant do anything since already happend */
    state = snd_pcm_state(out);    
    if (state == SND_PCM_STATE_XRUN) {
      err = xrun_recovery(out, -EPIPE);
      if (err < 0) {
        check_error(err,"DAC XRUN recovery failed");
        return SENDDACS_NO;
      }
      oavail = snd_pcm_avail_update(out); 

    } else if (state == SND_PCM_STATE_SUSPENDED) {
      err = xrun_recovery(out, -ESTRPIPE);
      if (err < 0) {
        check_error(err,"DAC SUSPEND recovery failed");
        return SENDDACS_NO;
      }
      oavail = snd_pcm_avail_update(out); 
    }

#ifdef ALSAMM_DEBUG
    if(dac_send < WATCH_PERIODS){
      out_avail[dac_send] = oavail;
    }
#endif

    /* we only transfer transfersize of bytes request,
       this should only happen on first card otherwise we got a problem :-(()*/

    if(oavail < alsamm_transfersize){
      return SENDDACS_NO;
    }
    
    /* transfer now */
    size = alsamm_transfersize; 
    fp1 = fpo;
    ooffset = 0;

    /* since this can go over a buffer boundery we maybe need two steps to
       transfer (normally when buffersize is a multiple of transfersize
       this should never happen) */

    while (size > 0) {
      
      int chn;
      snd_pcm_sframes_t oframes;

      oframes = size;

      err =  alsamm_get_channels(out, (unsigned long *)&oframes, 
        (unsigned long *)&ooffset,ochannels,dev->a_addr);

#ifdef ALSAMM_DEBUG
      if(dac_send < WATCH_PERIODS){
        out_offset[dac_send] = ooffset;
        outaddr[dac_send] = (char *) dev->a_addr[0];
      }
#endif
      
      if (err < 0){
        if ((err = xrun_recovery(out, err)) < 0) {
          check_error(err,"MMAP begins avail error");
          break; /* next card please */
        }
      }
      
      /* transfer into memory */    
      for (chn = 0; chn < ochannels; chn++) {
        
        t_alsa_sample32 *buf = (t_alsa_sample32 *)dev->a_addr[chn];

        /*
        osc(buf, oframes, (dac_send%1000 < 500)?-100.0:-10.0,440,&(indexes[chn]));
        */
                
        for (i = 0, fp2 = fp1 + chn*alsamm_transfersize; i < oframes; i++,fp2++)
          {
            float s1 = *fp2 * F32MAX;
            /* better but slower, better never clip ;-)
               buf[i]= CLIP32(s1); */
            buf[i]= ((int) s1 & 0xFFFFFF00);
            *fp2 = 0.0;
          }
      }

      commitres = snd_pcm_mmap_commit(out, ooffset, oframes);
      if (commitres < 0 || commitres != oframes) {
        if ((err = xrun_recovery(out, commitres >= 0 ? -EPIPE : commitres)) < 0) {
          check_error(err,"MMAP commit error");
          return SENDDACS_NO;
        }    
      }

#ifdef ALSAMM_DEBUG
      if(dac_send < WATCH_PERIODS)
        out_cm[dac_send] = oframes;
#endif

      fp1 += oframes;
      size -= oframes;
    } /* while size */
    fpo += ochannels*alsamm_transfersize;

  }/* for devno */


  fpi = sys_soundin; /* star first card first channel */
  
  for(devno = 0;devno < alsa_nindev;devno++){

    t_alsa_dev *dev = &alsa_indev[devno];
    snd_pcm_t *in = dev->a_handle;
    int ichannels = dev->a_channels;

    iavail = snd_pcm_avail_update(in);

    if (iavail < 0) {
      err = xrun_recovery(in, iavail);
      if (err < 0) {
        check_error(err,"input avail update failed");
        return SENDDACS_NO;
      }
      iavail=snd_pcm_avail_update(in); 
    }

    state = snd_pcm_state(in);
    
    if (state == SND_PCM_STATE_XRUN) {
      err = xrun_recovery(in, -EPIPE);
      if (err < 0) {
        check_error(err,"ADC XRUN recovery failed");
        return SENDDACS_NO;
      }
      iavail=snd_pcm_avail_update(in); 

    } else if (state == SND_PCM_STATE_SUSPENDED) {
      err = xrun_recovery(in, -ESTRPIPE);
      if (err < 0) {
        check_error(err,"ADC SUSPEND recovery failed");
        return SENDDACS_NO;
      }
      iavail=snd_pcm_avail_update(in); 
    }
    
    /* only transfer full transfersize or nothing */
    if(iavail < alsamm_transfersize){
      return SENDDACS_NO;
    }
    size = alsamm_transfersize; 
    fp1 = fpi;
    ioffset = 0;

    /* since sysdata can go over a driver buffer boundery we maybe need two steps to
       transfer (normally when buffersize is a multiple of transfersize
       this should never happen) */

    while(size > 0){
      int chn;
      snd_pcm_sframes_t iframes = size;

      err =  alsamm_get_channels(in, 
        (unsigned long *)&iframes, (unsigned long *)&ioffset,ichannels,dev->a_addr);
      if (err < 0){
        if ((err = xrun_recovery(in, err)) < 0) {
          check_error(err,"MMAP begins avail error");
          return SENDDACS_NO;
        }
      }

#ifdef ALSAMM_DEBUG
      if(dac_send < WATCH_PERIODS){
        in_avail[dac_send] = iavail;
        in_offset[dac_send] = ioffset;
        inaddr[dac_send] = dev->a_addr[0];
      }
#endif
      /* transfer into memory */    
      
      for (chn = 0; chn < ichannels; chn++) {
        
        t_alsa_sample32 *buf = (t_alsa_sample32 *) dev->a_addr[chn];
      
        for (i = 0, fp2 = fp1 + chn*alsamm_transfersize; i < iframes; i++,fp2++)
          {
            /* mask the lowest bits, since subchannels info can make zero samples nonzero */
            *fp2 = (float) ((t_alsa_sample32) (buf[i] & 0xFFFFFF00))  
              * (1.0 / (float) INT32_MAX);
          }      
      }

      commitres = snd_pcm_mmap_commit(in, ioffset, iframes);
      if (commitres < 0 || commitres != iframes) {
        post("please never");
        if ((err = xrun_recovery(in, commitres >= 0 ? -EPIPE : commitres)) < 0) {
          check_error(err,"MMAP synced in commit error");
          return SENDDACS_NO;
        }
      }
      fp1 += iframes;
      size -= iframes;
    }
    fpi += ichannels*alsamm_transfersize;
  } /* for out devno < alsamm_outcards*/
  
  
  if ((timenow = sys_getrealtime()) > (timelast + sleep_time))
    {

#ifdef ALSAMM_DEBUG      
      if(dac_send < 10 && sys_verbose)
        post("slept %f > %f + %f (=%f)",
             timenow,timelast,sleep_time,(timelast + sleep_time)); 
#endif
      return (SENDDACS_SLEPT);
    }
  
  return SENDDACS_YES;
}


/* extra debug info */

void alsamm_showstat(snd_pcm_t *handle)
{
  int err;
  snd_pcm_status_t *status;
  snd_output_t *output = NULL;

  snd_pcm_status_alloca(&status);
  if ((err = snd_pcm_status(handle, status)) < 0) {
    check_error(err, "Get Stream status error");
    return;
  }
  snd_pcm_status_dump(status, alsa_stdout);
}
