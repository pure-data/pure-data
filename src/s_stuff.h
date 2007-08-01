/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* Audio and MIDI I/O, and other scheduling and system stuff. */

/* NOTE: this file describes Pd implementation details which may change
in future releases.  The public (stable) API is in m_pd.h. */  

/* in s_path.c */

typedef struct _namelist    /* element in a linked list of stored strings */
{
    struct _namelist *nl_next;  /* next in list */
    char *nl_string;            /* the string */
} t_namelist;

t_namelist *namelist_append(t_namelist *listwas, const char *s, int allowdup);
t_namelist *namelist_append_files(t_namelist *listwas, const char *s);
void namelist_free(t_namelist *listwas);
char *namelist_get(t_namelist *namelist, int n);
void sys_setextrapath(const char *p);
extern int sys_usestdpath;
extern t_namelist *sys_externlist;
extern t_namelist *sys_searchpath;
extern t_namelist *sys_helppath;
int sys_open_absolute(const char *name, const char* ext,
    char *dirresult, char **nameresult, unsigned int size, int bin, int *fdp);
int sys_trytoopenone(const char *dir, const char *name, const char* ext,
    char *dirresult, char **nameresult, unsigned int size, int bin);


/* s_file.c */

void sys_loadpreferences( void);
void sys_savepreferences( void);
extern int sys_defeatrt;
extern t_symbol *sys_flags;

/* s_main.c */
extern int sys_debuglevel;
extern int sys_verbose;
extern int sys_noloadbang;
extern int sys_nogui;
extern char *sys_guicmd;

EXTERN int sys_nearestfontsize(int fontsize);
EXTERN int sys_hostfontsize(int fontsize);

extern int sys_defaultfont;
extern t_symbol *sys_libdir;    /* library directory for auxilliary files */

/* s_loader.c */
int sys_load_lib(t_canvas *canvas, char *filename);

/* s_audio.c */

#define SENDDACS_NO 0           /* return values for sys_send_dacs() */
#define SENDDACS_YES 1 
#define SENDDACS_SLEPT 2

#define DEFDACBLKSIZE 64
extern int sys_schedblocksize;  /* audio block size for scheduler */
extern int sys_hipriority;      /* real-time flag, true if priority boosted */
extern t_sample *sys_soundout;
extern t_sample *sys_soundin;
extern int sys_inchannels;
extern int sys_outchannels;
extern int sys_advance_samples; /* scheduler advance in samples */
extern int sys_blocksize;       /* audio I/O block size in sample frames */
extern float sys_dacsr;
extern int sys_schedadvance;
extern int sys_sleepgrain;
void sys_open_audio(int naudioindev, int *audioindev,
    int nchindev, int *chindev,
    int naudiooutdev, int *audiooutdev, int nchoutdev, int *choutdev,
    int srate, int advance, int enable);
void sys_reopen_audio( void);
void sys_close_audio(void);


int sys_send_dacs(void);
void sys_reportidle(void);
void sys_set_priority(int higher);
void sys_audiobuf(int nbufs);
void sys_getmeters(float *inmax, float *outmax);
void sys_listdevs(void);
void sys_setblocksize(int n);

/* s_midi.c */
#define MAXMIDIINDEV 16         /* max. number of input ports */
#define MAXMIDIOUTDEV 16        /* max. number of output ports */
extern int sys_nmidiin;
extern int sys_nmidiout;
extern int sys_midiindevlist[];
extern int sys_midioutdevlist[];

void sys_open_midi(int nmidiin, int *midiinvec,
    int nmidiout, int *midioutvec, int enable);
void sys_get_midi_params(int *pnmidiindev, int *pmidiindev,
    int *pnmidioutdev, int *pmidioutdev);

void sys_get_midi_apis(char *buf);

void sys_reopen_midi( void);
void sys_close_midi( void);
EXTERN void sys_putmidimess(int portno, int a, int b, int c);
EXTERN void sys_putmidibyte(int portno, int a);
EXTERN void sys_poll_midi(void);
EXTERN void sys_setmiditimediff(double inbuftime, double outbuftime);
EXTERN void sys_midibytein(int portno, int byte);

    /* implemented in the system dependent MIDI code (s_midi_pm.c, etc. ) */
void midi_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int maxndev, int devdescsize);
void sys_do_open_midi(int nmidiindev, int *midiindev,
    int nmidioutdev, int *midioutdev);

#ifdef USEAPI_ALSA
EXTERN void sys_alsa_putmidimess(int portno, int a, int b, int c);
EXTERN void sys_alsa_putmidibyte(int portno, int a);
EXTERN void sys_alsa_poll_midi(void);
EXTERN void sys_alsa_setmiditimediff(double inbuftime, double outbuftime);
EXTERN void sys_alsa_midibytein(int portno, int byte);
EXTERN void sys_alsa_close_midi( void);


    /* implemented in the system dependent MIDI code (s_midi_pm.c, etc. ) */
void midi_alsa_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int maxndev, int devdescsize);
void sys_alsa_do_open_midi(int nmidiindev, int *midiindev,
    int nmidioutdev, int *midioutdev);
#endif

/* m_sched.c */
EXTERN void sys_log_error(int type);
#define ERR_NOTHING 0
#define ERR_ADCSLEPT 1
#define ERR_DACSLEPT 2
#define ERR_RESYNC 3
#define ERR_DATALATE 4
void sched_set_using_dacs(int flag);

/* s_inter.c */

EXTERN void sys_microsleep(int microsec);

EXTERN void sys_bail(int exitcode);
EXTERN int sys_pollgui(void);

EXTERN_STRUCT _socketreceiver;
#define t_socketreceiver struct _socketreceiver

typedef void (*t_socketnotifier)(void *x);
typedef void (*t_socketreceivefn)(void *x, t_binbuf *b);

EXTERN t_socketreceiver *socketreceiver_new(void *owner,
    t_socketnotifier notifier, t_socketreceivefn socketreceivefn, int udp);
EXTERN void socketreceiver_read(t_socketreceiver *x, int fd);
EXTERN void sys_sockerror(char *s);
EXTERN void sys_closesocket(int fd);

typedef void (*t_fdpollfn)(void *ptr, int fd);
EXTERN void sys_addpollfn(int fd, t_fdpollfn fn, void *ptr);
EXTERN void sys_rmpollfn(int fd);
#ifdef UNIX
void sys_setalarm(int microsec);
void sys_setvirtualalarm( void);
#endif

#define API_ALSA 1
#define API_OSS 2
#define API_MMIO 3
#define API_PORTAUDIO 4
#define API_JACK 5
#define API_SGI 6

#ifdef __linux__
#define API_DEFAULT API_OSS
#define API_DEFSTRING "OSS"
#endif
#ifdef MSW
#define API_DEFAULT API_MMIO
#define API_DEFSTRING "MMIO"
#endif
#ifdef __APPLE__
#define API_DEFAULT API_PORTAUDIO
#define API_DEFSTRING "portaudio"
#endif
#ifdef IRIX
#define API_DEFAULT API_SGI
#define API_DEFSTRING "SGI Digital Media"
#endif
#define DEFAULTAUDIODEV 0

#define MAXAUDIOINDEV 4
#define MAXAUDIOOUTDEV 4

#define DEFMIDIDEV 0

#define DEFAULTSRATE 44100
#ifdef MSW
#define DEFAULTADVANCE 70
#else
#define DEFAULTADVANCE 50
#endif

int pa_open_audio(int inchans, int outchans, int rate, t_sample *soundin,
    t_sample *soundout, int framesperbuf, int nbuffers,
    int indeviceno, int outdeviceno);
void pa_close_audio(void);
int pa_send_dacs(void);
void sys_reportidle(void);
void pa_listdevs(void);
void pa_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti, 
        int maxndev, int devdescsize);

int oss_open_audio(int naudioindev, int *audioindev, int nchindev,
    int *chindev, int naudiooutdev, int *audiooutdev, int nchoutdev,
    int *choutdev, int rate);
void oss_close_audio(void);
int oss_send_dacs(void);
void oss_reportidle(void);
void oss_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti, 
        int maxndev, int devdescsize);

int alsa_open_audio(int naudioindev, int *audioindev, int nchindev,
    int *chindev, int naudiooutdev, int *audiooutdev, int nchoutdev,
    int *choutdev, int rate);
void alsa_close_audio(void);
int alsa_send_dacs(void);
void alsa_reportidle(void);
void alsa_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti, 
        int maxndev, int devdescsize);

int jack_open_audio(int wantinchans, int wantoutchans, int srate);
void jack_close_audio(void);
int jack_send_dacs(void);
void jack_reportidle(void);
void jack_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti, 
        int maxndev, int devdescsize);
void jack_listdevs(void);

void mmio_open_audio(int naudioindev, int *audioindev,
    int nchindev, int *chindev, int naudiooutdev, int *audiooutdev,
    int nchoutdev, int *choutdev, int rate);
void mmio_close_audio( void);
void mmio_reportidle(void);
int mmio_send_dacs(void);
void mmio_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int *canmulti, 
        int maxndev, int devdescsize);

void sys_listmididevs(void);
void sys_set_midi_api(int whichapi);
void sys_set_audio_api(int whichapi);
void sys_get_audio_apis(char *buf);
extern int sys_audioapi;
void sys_set_audio_state(int onoff);

/* API dependent audio flags and settings */
void oss_set32bit( void);
void linux_alsa_devname(char *devname);

void sys_get_audio_params(
    int *pnaudioindev, int *paudioindev, int *chindev,
    int *pnaudiooutdev, int *paudiooutdev, int *choutdev,
    int *prate, int *padvance);
void sys_save_audio_params(
    int naudioindev, int *audioindev, int *chindev,
    int naudiooutdev, int *audiooutdev, int *choutdev,
    int rate, int advance);

/* s_file.c */

typedef void (*t_printhook)(const char *s);
extern t_printhook sys_printhook;  /* set this to override printing */
extern int sys_printtostderr;
#ifdef MSW
#define vsnprintf  _vsnprintf /* jsarlo -- alias this name for msw */
#endif

/* jsarlo { */

EXTERN double sys_time;
EXTERN double sys_time_per_dsp_tick;
EXTERN int sys_externalschedlib;

EXTERN t_sample* get_sys_soundout(void ) ;
EXTERN t_sample* get_sys_soundin(void ) ;
EXTERN int* get_sys_main_advance(void ) ;
EXTERN double* get_sys_time_per_dsp_tick(void ) ;
EXTERN int* get_sys_schedblocksize(void ) ;
EXTERN double* get_sys_time(void ) ;
EXTERN float* get_sys_dacsr(void ) ;
EXTERN int* get_sys_sleepgrain(void ) ;
EXTERN int* get_sys_schedadvance(void ) ;

EXTERN void sys_clearhist(void );
EXTERN void sys_initmidiqueue(void );
EXTERN int sys_addhist(int phase);
EXTERN void sys_setmiditimediff(double inbuftime, double outbuftime);
EXTERN void sched_tick(double next_sys_time);
EXTERN void sys_pollmidiqueue(void );
EXTERN int sys_pollgui(void );
EXTERN void sys_setchsr(int chin, int chout, int sr);

EXTERN void inmidi_noteon(int portno, int channel, int pitch, int velo);
EXTERN void inmidi_controlchange(int portno,
                                 int channel,
                                 int ctlnumber,
                                 int value);
EXTERN void inmidi_programchange(int portno, int channel, int value);
EXTERN void inmidi_pitchbend(int portno, int channel, int value);
EXTERN void inmidi_aftertouch(int portno, int channel, int value);
EXTERN void inmidi_polyaftertouch(int portno,
                                  int channel,
                                  int pitch,
                                  int value);
/* } jsarlo */
