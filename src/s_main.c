/* Copyright (c) 1997-1999 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
#include "m_imp.h"
#include "s_stuff.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#include <winbase.h>
#endif
#ifdef _MSC_VER  /* This is only for Microsoft's compiler, not cygwin, e.g. */
#define snprintf _snprintf
#endif

#define stringify(s) str(s)
#define str(s) #s

char *pd_version = "Pd-" stringify(PD_MAJOR_VERSION) "." \
stringify(PD_MINOR_VERSION) "." stringify(PD_BUGFIX_VERSION) "\
 (" stringify(PD_TEST_VERSION) ")";

char pd_compiletime[] = __TIME__;
char pd_compiledate[] = __DATE__;

void pd_init(void);
int sys_argparse(int argc, const char **argv);
void sys_findprogdir(const char *progname);
void sys_setsignalhandlers(void);
int sys_startgui(const char *guipath);
void sys_setrealtime(const char *guipath);
int m_mainloop(void);
int m_batchmain(void);
void sys_addhelppath(char *p);
#ifdef USEAPI_ALSA
void alsa_adddev(const char *name);
#endif
int sys_oktoloadfiles(int done);

int sys_debuglevel;
int sys_verbose;
int sys_noloadbang;
static int sys_dontstartgui;
int sys_hipriority = -1;    /* -1 = not specified; 0 = no; 1 = yes */
int sys_guisetportnumber;   /* if started from the GUI, this is the port # */
int sys_nosleep = 0;  /* skip all "sleep" calls and spin instead */
int sys_defeatrt;       /* flag to cancel real-time */
t_symbol *sys_flags;    /* more command-line flags */

const char *sys_guicmd;
t_symbol *sys_libdir;
static t_namelist *sys_openlist;
static t_namelist *sys_messagelist;
static int sys_version;
int sys_oldtclversion;      /* hack to warn g_rtext.c about old text sel */

int sys_nmidiout = -1;
int sys_nmidiin = -1;
int sys_midiindevlist[MAXMIDIINDEV] = {1};
int sys_midioutdevlist[MAXMIDIOUTDEV] = {1};

#if __APPLE__
char sys_font[100] = "Menlo"; /* hack until DVSM bug is fixed on macOS 10.15+ */
char sys_fontweight[10] = "normal";
#else
char sys_font[100] = "DejaVu Sans Mono";
char sys_fontweight[10] = "bold";
#endif

static int sys_listplease;

int sys_externalschedlib;
char sys_externalschedlibname[MAXPDSTRING];
static int sys_batch;
int sys_extraflags;
char sys_extraflagsstring[MAXPDSTRING];
int sys_run_scheduler(const char *externalschedlibname,
    const char *sys_extraflagsstring);
int sys_noautopatch;    /* temporary hack to defeat new 0.42 editing */

t_sample *get_sys_soundout() { return STUFF->st_soundout; }
t_sample *get_sys_soundin() { return STUFF->st_soundin; }
double *get_sys_time_per_dsp_tick() { return &STUFF->st_time_per_dsp_tick; }
int *get_sys_schedblocksize() { return &STUFF->st_schedblocksize; }
double *get_sys_time() { return &pd_this->pd_systime; }
t_float *get_sys_dacsr() { return &STUFF->st_dacsr; }
int *get_sys_schedadvance() { return &sys_schedadvance; }
t_namelist *sys_searchpath;  /* so old versions of GEM might compile */

typedef struct _fontinfo
{
    int fi_pointsize;
    int fi_width;
    int fi_height;
} t_fontinfo;

    /* these give the nominal point size and maximum height of the characters
    in the six fonts.  */

static t_fontinfo sys_fontspec[] = {
    {8, 5, 11}, {10, 6, 13}, {12, 7, 16},
    {16, 10, 19}, {24, 14, 29}, {36, 22, 44}};
#define NFONT (sizeof(sys_fontspec)/sizeof(*sys_fontspec))
#define NZOOM 2
static t_fontinfo sys_gotfonts[NZOOM][NFONT];

/* here are the actual font size structs on msp's systems:
MSW:
font 8 5 9 8 5 11
font 10 7 13 10 6 13
font 12 9 16 14 8 16
font 16 10 20 16 10 18
font 24 15 25 16 10 18
font 36 25 42 36 22 41

linux:
font 8 5 9 8 5 9
font 10 7 13 12 7 13
font 12 9 16 14 9 15
font 16 10 20 16 10 19
font 24 15 25 24 15 24
font 36 25 42 36 22 41
*/

static int sys_findfont(int fontsize)
{
    unsigned int i;
    t_fontinfo *fi;
    for (i = 0, fi = sys_fontspec; i < (NFONT-1); i++, fi++)
        if (fontsize < fi[1].fi_pointsize) return (i);
    return ((NFONT-1));
}

int sys_nearestfontsize(int fontsize)
{
    return (sys_fontspec[sys_findfont(fontsize)].fi_pointsize);
}

int sys_hostfontsize(int fontsize, int zoom)
{
    zoom = (zoom < 1 ? 1 : (zoom > NZOOM ? NZOOM : zoom));
    return (sys_gotfonts[zoom-1][sys_findfont(fontsize)].fi_pointsize);
}

int sys_zoomfontwidth(int fontsize, int zoomarg, int worstcase)
{
    int zoom = (zoomarg < 1 ? 1 : (zoomarg > NZOOM ? NZOOM : zoomarg)), ret;
    if (worstcase)
        ret = zoom * sys_fontspec[sys_findfont(fontsize)].fi_width;
    else ret = sys_gotfonts[zoom-1][sys_findfont(fontsize)].fi_width;
    return (ret < 1 ? 1 : ret);
}

int sys_zoomfontheight(int fontsize, int zoomarg, int worstcase)
{
    int zoom = (zoomarg < 1 ? 1 : (zoomarg > NZOOM ? NZOOM : zoomarg)), ret;
    if (worstcase)
        ret = (zoom * sys_fontspec[sys_findfont(fontsize)].fi_height);
    else ret = sys_gotfonts[zoom-1][sys_findfont(fontsize)].fi_height;
    return (ret < 1 ? 1 : ret);
}

int sys_fontwidth(int fontsize) /* old version for extern compatibility */
{
    return (sys_zoomfontwidth(fontsize, 1, 0));
}

int sys_fontheight(int fontsize)
{
    return (sys_zoomfontheight(fontsize, 1, 0));
}

int sys_defaultfont;
#define DEFAULTFONT 12

static void openit(const char *dirname, const char *filename)
{
    char dirbuf[MAXPDSTRING], *nameptr;
    int fd = open_via_path(dirname, filename, "", dirbuf, &nameptr,
        MAXPDSTRING, 0);
    if (fd >= 0)
    {
        close (fd);
        glob_evalfile(0, gensym(nameptr), gensym(dirbuf));
    }
    else
        pd_error(0, "%s: can't open", filename);
}

/* this is called from the gui process.  The first argument is the cwd, and
succeeding args give the widths and heights of known fonts.  We wait until
these are known to open files and send messages specified on the command line.
We ask the GUI to specify the "cwd" in case we don't have a local OS to get it
from; for instance we could be some kind of RT embedded system.  However, to
really make this make sense we would have to implement
open(), read(), etc, calls to be served somehow from the GUI too. */

void glob_initfromgui(void *dummy, t_symbol *s, int argc, t_atom *argv)
{
    const char *cwd = atom_getsymbolarg(0, argc, argv)->s_name;
    t_namelist *nl;
    unsigned int i;
    int did_fontwarning = 0;
    int j;
    sys_oldtclversion = atom_getfloatarg(1, argc, argv);
    if (argc != 2 + 3 * NZOOM * NFONT)
        bug("glob_initfromgui");
    for (j = 0; j < NZOOM; j++)
        for (i = 0; i < NFONT; i++)
    {
        int size   = atom_getfloatarg(3 * (i + j * NFONT) + 2, argc, argv);
        int width  = atom_getfloatarg(3 * (i + j * NFONT) + 3, argc, argv);
        int height = atom_getfloatarg(3 * (i + j * NFONT) + 4, argc, argv);
        if (!(size && width && height))
        {
            size   = (j+1)*sys_fontspec[i].fi_pointsize;
            width  = (j+1)*sys_fontspec[i].fi_width;
            height = (j+1)*sys_fontspec[i].fi_height;
            if (!did_fontwarning)
            {
                logpost(NULL, PD_VERBOSE, "ignoring invalid font-metrics from GUI");
                did_fontwarning = 1;
            }
        }
        sys_gotfonts[j][i].fi_pointsize = size;
        sys_gotfonts[j][i].fi_width = width;
        sys_gotfonts[j][i].fi_height = height;
#if 0
            fprintf(stderr, "font (%d %d %d)\n",
                sys_gotfonts[j][i].fi_pointsize, sys_gotfonts[j][i].fi_width,
                    sys_gotfonts[j][i].fi_height);
#endif
    }
        /* load dynamic libraries specified with "-lib" args */
    if (sys_oktoloadfiles(0))
    {
        for  (nl = STUFF->st_externlist; nl; nl = nl->nl_next)
            if (!sys_load_lib(0, nl->nl_string))
                post("%s: can't load library", nl->nl_string);
        sys_oktoloadfiles(1);
    }
        /* open patches specifies with "-open" args */
    for  (nl = sys_openlist; nl; nl = nl->nl_next)
        openit(cwd, nl->nl_string);
    namelist_free(sys_openlist);
    sys_openlist = 0;
        /* send messages specified with "-send" args */
    for  (nl = sys_messagelist; nl; nl = nl->nl_next)
    {
        t_binbuf *b = binbuf_new();
        binbuf_text(b, nl->nl_string, strlen(nl->nl_string));
        binbuf_eval(b, 0, 0, 0);
        binbuf_free(b);
    }
    namelist_free(sys_messagelist);
    sys_messagelist = 0;
}

// font char metric triples: pointsize width(pixels) height(pixels)
static int defaultfontshit[] = {
  8,  5, 11,  10,  6, 13,  12,  7, 16,  16, 10, 19,  24, 14, 29,  36, 22, 44,
 16, 10, 22,  20, 12, 26,  24, 14, 32,  32, 20, 38,  48, 28, 58,  72, 44, 88
}; // normal & zoomed (2x)
#define NDEFAULTFONT (sizeof(defaultfontshit)/sizeof(*defaultfontshit))

static t_clock *sys_fakefromguiclk;
int socket_init(void);
static void sys_fakefromgui(void)
{
        /* fake the GUI's message giving cwd and font sizes in case
        we aren't starting the gui. */
    t_atom zz[NDEFAULTFONT+2];
    int i;
    char buf[MAXPDSTRING];
#ifdef _WIN32
    if (GetCurrentDirectory(MAXPDSTRING, buf) == 0)
        strcpy(buf, ".");
#else
    if (!getcwd(buf, MAXPDSTRING))
        strcpy(buf, ".");
#endif
    SETSYMBOL(zz, gensym(buf));
    SETFLOAT(zz+1, 0);
    for (i = 0; i < (int)NDEFAULTFONT; i++)
        SETFLOAT(zz+i+2, defaultfontshit[i]);
    glob_initfromgui(0, 0, 2+NDEFAULTFONT, zz);
    clock_free(sys_fakefromguiclk);
}

static void sys_afterargparse(void);
static void sys_printusage(void);

/* this is called from main() in s_entry.c */
int sys_main(int argc, const char **argv)
{
    int i, noprefs;
    const char *prefsfile = "";
    sys_externalschedlib = 0;
    sys_extraflags = 0;
#ifdef PD_DEBUG
    fprintf(stderr, "Pd: COMPILED FOR DEBUGGING\n");
#endif
    /* use Win32 "binary" mode by default since we don't want the
     * translation that Win32 does by default */
#ifdef _WIN32
# ifdef _MSC_VER /* MS Visual Studio */
    _set_fmode( _O_BINARY );
# else  /* MinGW */
    {
#ifndef _fmode
        extern int _fmode;
#endif
        _fmode = _O_BINARY;
    }
# endif /* _MSC_VER */
#endif  /* _WIN32 */
#ifndef _WIN32
    /* long ago Pd used setuid to promote itself to real-time priority.
    Just in case anyone's installation script still makes it setuid, we
    complain to stderr and lose setuid here. */
    if (getuid() != geteuid())
    {
        fprintf(stderr, "warning: canceling setuid privilege\n");
        setuid(getuid());
    }
#endif  /* _WIN32 */
    if (socket_init())
        sys_sockerror("socket_init()");
    pd_init();                                  /* start the message system */
    sys_findprogdir(argv[0]);                   /* set sys_progname, guipath */
    for (i = noprefs = 0; i < argc; i++)    /* prescan ... */
    {
        /* for prefs override */
        if (!strcmp(argv[i], "-noprefs"))
            noprefs = 1;
        else if (!strcmp(argv[i], "-prefsfile") && i < argc-1)
            prefsfile = argv[i+1];
        /* for external scheduler (to ignore audio api in sys_loadpreferences) */
        else if (!strcmp(argv[i], "-schedlib") && i < argc-1)
            sys_externalschedlib = 1;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help"))
        {
            sys_printusage();
            return (1);
        }
    }
    if (!noprefs)       /* load preferences before parsing args to allow ... */
        sys_loadpreferences(prefsfile, 1);  /* args to override prefs */
    if (sys_argparse(argc-1, argv+1))           /* parse cmd line args */
        return (1);
    if (sys_verbose || sys_version) fprintf(stderr, "%s compiled %s %s\n",
        pd_version, pd_compiletime, pd_compiledate);
    if (sys_verbose)
        fprintf(stderr, "float precision = %lu bits\n", sizeof(t_float)*8);
    if (sys_version)    /* if we were just asked our version, exit here. */
    {
        fflush(stderr);
        return (0);
    }
    sys_setsignalhandlers();
    sys_afterargparse();                    /* post-argparse settings */
    if (sys_dontstartgui)
        clock_set((sys_fakefromguiclk =
            clock_new(0, (t_method)sys_fakefromgui)), 0);
    else if (sys_startgui(sys_libdir->s_name)) /* start the gui */
        return (1);
    if (sys_hipriority)
        sys_setrealtime(sys_libdir->s_name); /* set desired process priority */
    if (sys_externalschedlib)
        return (sys_run_scheduler(sys_externalschedlibname,
            sys_extraflagsstring));
    else if (sys_batch)
        return (m_batchmain());
    else
    {
            /* open audio and MIDI */
        sys_reopen_midi();
        if (audio_shouldkeepopen())
            sys_reopen_audio();
            /* run scheduler until it quits */
        return (m_mainloop());
    }
}

static char *(usagemessage[]) = {
"usage: pd [-flags] [file]...\n",
"\naudio configuration flags:\n",
"-r <n>           -- specify sample rate\n",
"-audioindev ...  -- audio in devices; e.g., \"1,3\" for first and third\n",
"-audiooutdev ... -- audio out devices (same)\n",
"-audiodev ...    -- specify input and output together\n",
"-audioaddindev   -- add an audio input device by name\n",
"-audioaddoutdev  -- add an audio output device by name\n",
"-audioadddev     -- add an audio input and output device by name\n",
"-inchannels ...  -- audio input channels (by device, like \"2\" or \"16,8\")\n",
"-outchannels ... -- number of audio out channels (same)\n",
"-channels ...    -- specify both input and output channels\n",
"-audiobuf <n>    -- specify size of audio buffer in msec\n",
"-blocksize <n>   -- specify audio I/O block size in sample frames\n",
"-sleepgrain <n>  -- specify number of milliseconds to sleep when idle\n",
"-nodac           -- suppress audio output\n",
"-noadc           -- suppress audio input\n",
"-noaudio         -- suppress audio input and output (-nosound is synonym) \n",
"-callback        -- use callbacks if possible\n",
"-nocallback      -- use polling-mode (true by default)\n",
"-listdev         -- list audio and MIDI devices\n",

#ifdef USEAPI_OSS
"-oss             -- use OSS audio API\n",
#endif

#ifdef USEAPI_ALSA
"-alsa            -- use ALSA audio API\n",
"-alsaadd <name>  -- add an ALSA device name to list\n",
#endif

#ifdef USEAPI_JACK
"-jack            -- use JACK audio API\n",
"-jackname <name> -- a name for your JACK client\n",
"-nojackconnect   -- do not automatically connect pd to the JACK graph\n",
"-jackconnect     -- automatically connect pd to the JACK graph [default]\n",

#endif

#ifdef USEAPI_PORTAUDIO
#ifdef _WIN32
"-asio            -- use ASIO audio driver (via Portaudio)\n",
"-pa              -- synonym for -asio\n",
#else
"-pa              -- use Portaudio API\n",
#endif
#endif

#ifdef USEAPI_MMIO
"-mmio            -- use MMIO audio API (default for Windows)\n",
#endif

#ifdef USEAPI_AUDIOUNIT
"-audiounit       -- use Apple AudioUnit API\n",
#endif

#ifdef USEAPI_ESD
"-esd             -- use Enlightenment Sound Daemon (ESD) API\n",
#endif

"      (default audio API for this platform:  ", API_DEFSTRING, ")\n\n",

"\nMIDI configuration flags:\n",
"-midiindev ...   -- midi in device list; e.g., \"1,3\" for first and third\n",
"-midioutdev ...  -- midi out device list, same format\n",
"-mididev ...     -- specify -midioutdev and -midiindev together\n",
"-midiaddindev    -- add a MIDI input device by name\n",
"-midiaddoutdev   -- add a MIDI output device by name\n",
"-midiadddev      -- add a MIDI input and output device by name\n",
"-nomidiin        -- suppress MIDI input\n",
"-nomidiout       -- suppress MIDI output\n",
"-nomidi          -- suppress MIDI input and output\n",
#ifdef USEAPI_OSS
"-ossmidi         -- use OSS midi API\n",
#endif
#ifdef USEAPI_ALSA
"-alsamidi        -- use ALSA midi API\n",
#endif


"\nother flags:\n",
"-path <path>     -- add to file search path\n",
"-nostdpath       -- don't search standard (\"extra\") directory\n",
"-stdpath         -- search standard directory (true by default)\n",
"-helppath <path> -- add to help file search path\n",
"-open <file>     -- open file(s) on startup\n",
"-lib <file>      -- load object library(s) (omit file extensions)\n",
"-font-size <n>      -- specify default font size in points\n",
"-font-face <name>   -- specify default font\n",
"-font-weight <name> -- specify default font weight (normal or bold)\n",
"-verbose         -- extra printout on startup and when searching for files\n",
"-noverbose       -- no extra printout\n",
"-version         -- don't run Pd; just print out which version it is \n",
"-d <n>           -- specify debug level for inspecting the GUI communication\n",
"-loadbang        -- do not suppress all loadbangs (true by default)\n",
"-noloadbang      -- suppress all loadbangs\n",
"-stderr          -- send printout to standard error instead of GUI\n",
"-nostderr        -- send printout to GUI (true by default)\n",
"-gui             -- start GUI (true by default)\n",
"-nogui           -- suppress starting the GUI\n",
"-guiport <n>     -- connect to pre-existing GUI over port <n>\n",
"-guicmd \"cmd...\" -- start alternative GUI program (e.g., remote via ssh)\n",
"-send \"msg...\"   -- send a message at startup, after patches are loaded\n",
"-prefs           -- load preferences on startup (true by default)\n",
"-noprefs         -- suppress loading preferences on startup\n",
"-prefsfile <file>  -- load preferences from a file\n",
#ifdef HAVE_UNISTD_H
"-rt or -realtime -- use real-time priority\n",
"-nrt             -- don't use real-time priority\n",
#endif
"-sleep           -- sleep when idle, don't spin (true by default)\n",
"-nosleep         -- spin, don't sleep (may lower latency on multi-CPUs)\n",
"-schedlib <file> -- plug in external scheduler (omit file extensions)\n",
"-extraflags <s>  -- string argument to send schedlib\n",
"-batch           -- run off-line as a batch process\n",
"-nobatch         -- run interactively (true by default)\n",
"-autopatch       -- enable auto-patching to new objects (true by default)\n",
"-noautopatch     -- defeat auto-patching\n",
"-compatibility <f> -- set back-compatibility to version <f>\n",
};

static void sys_printusage(void)
{
    unsigned int i;
    for (i = 0; i < sizeof(usagemessage)/sizeof(*usagemessage); i++)
    {
        fprintf(stderr, "%s", usagemessage[i]);
        fflush(stderr);
    }
}

    /* parse a comma-separated numeric list, returning the number found */
static int sys_parsedevlist(int *np, int *vecp, int max, const char *str)
{
    int n = 0;
    while (n < max)
    {
        if (! *str) break;
        else
        {
            char *endp;
            vecp[n] = (int)strtol(str, &endp, 10);
            if (endp == str)
                break;
            n++;
            if (! *endp)
                break;
            str = endp + 1;
        }
    }
    return (*np = n);
}

static int sys_getmultidevchannels(int n, int *devlist)
{
    int sum = 0;
    if (n<0)return(-1);
    if (n==0)return 0;
    while(n--)sum+=*devlist++;
    return sum;
}


    /* this routine tries to figure out where to find the auxiliary files
    Pd will need to run.  This is either done by looking at the command line
    invocation for Pd, or if that fails, by consulting the variable
    INSTALL_PREFIX.  In MSW, we don't try to use INSTALL_PREFIX. */
void sys_findprogdir(const char *progname)
{
    char sbuf[MAXPDSTRING], sbuf2[MAXPDSTRING];
    char *lastslash;
#ifndef _WIN32
    struct stat statbuf;
#endif /* NOT _WIN32 */

    /* find out by what string Pd was invoked; put answer in "sbuf". */
#ifdef _WIN32
    GetModuleFileName(NULL, sbuf2, sizeof(sbuf2));
    sbuf2[MAXPDSTRING-1] = 0;
    sys_unbashfilename(sbuf2, sbuf);
#else
    strncpy(sbuf, progname, MAXPDSTRING);
    sbuf[MAXPDSTRING-1] = 0;
#endif /* _WIN32 */
    lastslash = strrchr(sbuf, '/');
    if (lastslash)
    {
            /* bash last slash to zero so that sbuf is directory pd was in,
                e.g., ~/pd/bin */
        *lastslash = 0;
            /* go back to the parent from there, e.g., ~/pd */
        lastslash = strrchr(sbuf, '/');
        if (lastslash)
        {
            strncpy(sbuf2, sbuf, lastslash-sbuf);
            sbuf2[lastslash-sbuf] = 0;
        }
        else strcpy(sbuf2, "..");
    }
    else
    {
            /* no slashes found.  Try INSTALL_PREFIX. */
#ifdef INSTALL_PREFIX
        strcpy(sbuf2, INSTALL_PREFIX);
#else
        strcpy(sbuf2, ".");
#endif
    }
        /* now we believe sbuf2 holds the parent directory of the directory
        pd was found in.  We now want to infer the "lib" directory and the
        "gui" directory.  In "simple" unix installations, the layout is
            .../bin/pd
            .../bin/pd-watchdog (etc)
            .../bin/pd-gui.tcl
            .../doc
        and in "complicated" unix installations, it's:
            .../bin/pd
            .../lib/pd/bin/pd-watchdog
            .../lib/pd/bin/pd-gui.tcl
            .../lib/pd/doc
        To decide which, we stat .../lib/pd; if that exists, we assume it's
        the complicated layout.  In MSW, it's the "simple" layout, but
        "wish" is found in bin:
            .../bin/pd
            .../bin/wish80.exe
            .../doc
        */
#ifdef _WIN32
    sys_libdir = gensym(sbuf2);
#else
    strncpy(sbuf, sbuf2, MAXPDSTRING-30);
    sbuf[MAXPDSTRING-30] = 0;
    strcat(sbuf, "/lib/pd");
    if (stat(sbuf, &statbuf) >= 0)
    {
            /* complicated layout: lib dir is the one we just stat-ed above */
        sys_libdir = gensym(sbuf);
    }
    else
    {
            /* simple layout: lib dir is the parent */
        sys_libdir = gensym(sbuf2);
    }
#endif
}

#ifdef _WIN32
static int sys_mmio = 1;
#else
static int sys_mmio = 0;
#endif

int sys_argparse(int argc, const char **argv)
{
    t_audiosettings as;
            /* get the current audio parameters.  These are set
            by the preferences mechanism (sys_loadpreferences()) or
            else are the default.  Overwrite them with any results
            of argument parsing, and store them again. */
    sys_get_audio_settings(&as);
    while ((argc > 0) && **argv == '-')
    {
                /* audio flags */
        if (!strcmp(*argv, "-r") && argc > 1 &&
            sscanf(argv[1], "%d", &as.a_srate) >= 1)
        {
            argc -= 2;
            argv += 2;
        }
        else if (!strcmp(*argv, "-inchannels"))
        {
            if (argc < 2 ||
                !sys_parsedevlist(&as.a_nchindev,
                    as.a_chindevvec, MAXAUDIOINDEV, argv[1]))
                        goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-outchannels"))
        {
            if (argc < 2 ||
                !sys_parsedevlist(&as.a_nchoutdev,
                    as.a_choutdevvec, MAXAUDIOINDEV, argv[1]))
                        goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-channels"))
        {
            if (argc < 2 ||
                !sys_parsedevlist(&as.a_nchindev,
                    as.a_chindevvec, MAXAUDIOINDEV, argv[1]) ||
                !sys_parsedevlist(&as.a_nchoutdev,
                    as.a_choutdevvec, MAXAUDIOINDEV, argv[1]))
                        goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-soundbuf") || (!strcmp(*argv, "-audiobuf")))
        {
            if (argc < 2)
                goto usage;

            as.a_advance = atoi(argv[1]);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-callback"))
        {
            as.a_callback = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nocallback"))
        {
            as.a_callback = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-blocksize"))
        {
            as.a_blocksize = atoi(argv[1]);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-sleepgrain"))
        {
            if (argc < 2)
                goto usage;
            sys_sleepgrain = 1000 * atof(argv[1]);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-nodac"))
        {
            as.a_noutdev = as.a_nchoutdev = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-noadc"))
        {
            as.a_nindev = as.a_nchindev = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nosound") || !strcmp(*argv, "-noaudio"))
        {
            as.a_noutdev = as.a_nchoutdev =  as.a_nindev = as.a_nchindev = 0;
            argc--; argv++;
        }
#ifdef USEAPI_OSS
        else if (!strcmp(*argv, "-oss"))
        {
            as.a_api = API_OSS;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-ossmidi"))
        {
            sys_set_midi_api(API_OSS);
            argc--; argv++;
        }
#else
        else if ((!strcmp(*argv, "-oss")) || (!strcmp(*argv, "-ossmidi")))
        {
            fprintf(stderr, "Pd compiled without OSS-support, ignoring '%s' flag\n", *argv);
            argc--; argv++;
        }
#endif
#ifdef USEAPI_ALSA
        else if (!strcmp(*argv, "-alsa"))
        {
            as.a_api = API_ALSA;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-alsaadd"))
        {
            if (argc < 2)
                goto usage;
            as.a_api = API_ALSA;
            alsa_adddev(argv[1]);
            argc -= 2; argv +=2;
        }
        else if (!strcmp(*argv, "-alsamidi"))
        {
            sys_set_midi_api(API_ALSA);
            argc--; argv++;
        }
#else
        else if ((!strcmp(*argv, "-alsa")) || (!strcmp(*argv, "-alsamidi")))
        {
            fprintf(stderr, "Pd compiled without ALSA-support, ignoring '%s' flag\n", *argv);
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-alsaadd"))
        {
            if (argc < 2)
                goto usage;
            fprintf(stderr, "Pd compiled without ALSA-support, ignoring '%s' flag\n", *argv);
            argc -= 2; argv +=2;
        }
#endif
#ifdef USEAPI_JACK
        else if (!strcmp(*argv, "-jack"))
        {
            as.a_api = API_JACK;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nojackconnect"))
        {
            as.a_api = API_JACK;
            jack_autoconnect(0);
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-jackconnect"))
        {
            as.a_api = API_JACK;
            jack_autoconnect(1);
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-jackname"))
        {
            if (argc < 2)
                goto usage;
            as.a_api = API_JACK;
            jack_client_name(argv[1]);
            argc -= 2; argv +=2;
        }
#else
        else if ((!strcmp(*argv, "-jack"))
            || (!strcmp(*argv, "-jackconnect"))
            || (!strcmp(*argv, "-nojackconnect")))
        {
            fprintf(stderr, "Pd compiled without JACK-support, ignoring '%s' flag\n", *argv);
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-jackname"))
        {
            if (argc < 2)
                goto usage;
            fprintf(stderr, "Pd compiled without JACK-support, ignoring '%s' flag\n", *argv);
            argc -= 2; argv +=2;
        }
#endif
#ifdef USEAPI_PORTAUDIO
        else if (!strcmp(*argv, "-pa") || !strcmp(*argv, "-portaudio")
            || !strcmp(*argv, "-asio"))
        {
            as.a_api = API_PORTAUDIO;
            sys_mmio = 0;
            argc--; argv++;
        }
#else
        else if ((!strcmp(*argv, "-pa")) || (!strcmp(*argv, "-portaudio")))
        {
            fprintf(stderr, "Pd compiled without PortAudio-support, ignoring '%s' flag\n", *argv);
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-asio"))
        {
            fprintf(stderr, "Pd compiled without ASIO-support, ignoring '%s' flag\n", *argv);
            argc -= 2; argv +=2;
        }
#endif
#ifdef USEAPI_MMIO
        else if (!strcmp(*argv, "-mmio"))
        {
            as.a_api = API_MMIO;
            sys_mmio = 1;
            argc--; argv++;
        }
#else
        else if (!strcmp(*argv, "-mmio"))
        {
            fprintf(stderr, "Pd compiled without MMIO-support, ignoring '%s' flag\n", *argv);
            argc--; argv++;
        }
#endif
#ifdef USEAPI_AUDIOUNIT
        else if (!strcmp(*argv, "-audiounit"))
        {
            as.a_api = API_AUDIOUNIT;
            argc--; argv++;
        }
#else
        else if (!strcmp(*argv, "-audiounit"))
        {
            fprintf(stderr, "Pd compiled without AudioUnit-support, ignoring '%s' flag\n", *argv);
            argc--; argv++;
        }
#endif
#ifdef USEAPI_ESD
        else if (!strcmp(*argv, "-esd"))
        {
            as.a_api = API_ESD;
            argc--; argv++;
        }
#else
        else if (!strcmp(*argv, "-esd"))
        {
            fprintf(stderr, "Pd compiled without ESD-support, ignoring '%s' flag\n", *argv);
            argc--; argv++;
        }
#endif
        else if (!strcmp(*argv, "-listdev"))
        {
            sys_listplease = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-soundindev") ||
            !strcmp(*argv, "-audioindev"))
        {
            if (argc < 2 ||
                !sys_parsedevlist(&as.a_nindev, as.a_indevvec,
                    MAXAUDIOINDEV, argv[1]))
                        goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-soundoutdev") ||
            !strcmp(*argv, "-audiooutdev"))
        {
            if (argc < 2 ||
                !sys_parsedevlist(&as.a_noutdev, as.a_outdevvec,
                    MAXAUDIOOUTDEV, argv[1]))
                        goto usage;
            argc -= 2; argv += 2;
        }
        else if ((!strcmp(*argv, "-sounddev") || !strcmp(*argv, "-audiodev")))
        {
            if (argc < 2 ||
                !sys_parsedevlist(&as.a_nindev, as.a_indevvec,
                    MAXAUDIOINDEV, argv[1]) ||
                !sys_parsedevlist(&as.a_noutdev, as.a_outdevvec,
                    MAXAUDIOOUTDEV, argv[1]))
                        goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-audioaddindev"))
        {
            if (argc < 2)
                goto usage;

            if (as.a_nindev < 0)
                as.a_nindev = 0;
            if (as.a_nindev < MAXAUDIOINDEV)
            {
                int devn = sys_audiodevnametonumber(0, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find audio input device: %s\n",
                        argv[1]);
                else as.a_indevvec[as.a_nindev++] = devn + 1;
            }
            else fprintf(stderr, "number of audio devices limited to %d\n",
                MAXAUDIOINDEV);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-audioaddoutdev"))
        {
            if (argc < 2)
                goto usage;

            if (as.a_noutdev < 0)
                as.a_noutdev = 0;
            if (as.a_noutdev < MAXAUDIOINDEV)
            {
                int devn = sys_audiodevnametonumber(1, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find audio output device: %s\n",
                        argv[1]);
                else as.a_outdevvec[as.a_noutdev++] = devn + 1;
            }
            else fprintf(stderr, "number of audio devices limited to %d\n",
                MAXAUDIOINDEV);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-audioadddev"))
        {
            if (argc < 2)
                goto usage;

            if (as.a_nindev < 0)
                as.a_nindev = 0;
            if (as.a_noutdev < 0)
                as.a_noutdev = 0;
            if (as.a_nindev < MAXAUDIOINDEV && as.a_noutdev < MAXAUDIOINDEV)
            {
                int devn = sys_audiodevnametonumber(0, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find audio input device: %s\n",
                        argv[1]);
                else as.a_indevvec[as.a_nindev++] = devn + 1;
                devn = sys_audiodevnametonumber(1, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find audio output device: %s\n",
                        argv[1]);
                else as.a_outdevvec[as.a_noutdev++] = devn + 1;
            }
            else fprintf(stderr, "number of audio devices limited to %d",
                MAXAUDIOINDEV);
            argc -= 2; argv += 2;
        }
                /* MIDI flags */
        else if (!strcmp(*argv, "-nomidiin"))
        {
            sys_nmidiin = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nomidiout"))
        {
            sys_nmidiout = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nomidi"))
        {
            sys_nmidiin = sys_nmidiout = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-midiindev"))
        {
            if (argc < 2)
                goto usage;

            sys_parsedevlist(&sys_nmidiin, sys_midiindevlist, MAXMIDIINDEV,
                argv[1]);
            if (!sys_nmidiin)
                goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-midioutdev"))
        {
            if (argc < 2)
                goto usage;

            sys_parsedevlist(&sys_nmidiout, sys_midioutdevlist, MAXMIDIOUTDEV,
                argv[1]);
            if (!sys_nmidiout)
                goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-mididev"))
        {
            if (argc < 2)
                goto usage;

            sys_parsedevlist(&sys_nmidiin, sys_midiindevlist, MAXMIDIINDEV,
                argv[1]);
            sys_parsedevlist(&sys_nmidiout, sys_midioutdevlist, MAXMIDIOUTDEV,
                argv[1]);
            if (!sys_nmidiout)
                goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-midiaddindev"))
        {
            if (argc < 2)
                goto usage;

            if (sys_nmidiin < 0)
                sys_nmidiin = 0;
            if (sys_nmidiin < MAXMIDIINDEV)
            {
                int devn = sys_mididevnametonumber(0, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find MIDI input device: %s\n",
                        argv[1]);
                else sys_midiindevlist[sys_nmidiin++] = devn + 1;
            }
            else fprintf(stderr, "number of MIDI devices limited to %d\n",
                MAXMIDIINDEV);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-midiaddoutdev"))
        {
            if (argc < 2)
                goto usage;

            if (sys_nmidiout < 0)
                sys_nmidiout = 0;
            if (sys_nmidiout < MAXMIDIINDEV)
            {
                int devn = sys_mididevnametonumber(1, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find MIDI output device: %s\n",
                        argv[1]);
                else sys_midioutdevlist[sys_nmidiout++] = devn + 1;
            }
            else fprintf(stderr, "number of MIDI devices limited to %d\n",
                MAXMIDIINDEV);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-midiadddev"))
        {
            if (argc < 2)
                goto usage;

            if (sys_nmidiin < 0)
                sys_nmidiin = 0;
            if (sys_nmidiout < 0)
                sys_nmidiout = 0;
            if (sys_nmidiin < MAXMIDIINDEV && sys_nmidiout < MAXMIDIINDEV)
            {
                int devn = sys_mididevnametonumber(1, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find MIDI output device: %s\n",
                        argv[1]);
                else sys_midioutdevlist[sys_nmidiin++] = devn + 1;
                devn = sys_mididevnametonumber(1, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find MIDI output device: %s\n",
                        argv[1]);
                else sys_midioutdevlist[sys_nmidiout++] = devn + 1;
            }
            else fprintf(stderr, "number of MIDI devices limited to %d",
                MAXMIDIINDEV);
            argc -= 2; argv += 2;
        }
                /* other flags */
        else if (!strcmp(*argv, "-path"))
        {
            if (argc < 2)
                goto usage;
            STUFF->st_temppath =
                namelist_append_files(STUFF->st_temppath, argv[1]);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-nostdpath"))
        {
            sys_usestdpath = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-stdpath"))
        {
            sys_usestdpath = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-helppath"))
        {
            if (argc < 2)
                goto usage;
            STUFF->st_helppath =
                namelist_append_files(STUFF->st_helppath, argv[1]);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-open"))
        {
            if (argc < 2)
                goto usage;

            sys_openlist = namelist_append_files(sys_openlist, argv[1]);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-lib"))
        {
            if (argc < 2)
                goto usage;

            STUFF->st_externlist =
                namelist_append_files(STUFF->st_externlist, argv[1]);
            argc -= 2; argv += 2;
        }
        else if ((!strcmp(*argv, "-font-size") || !strcmp(*argv, "-font")))
        {
            if (argc < 2)
                goto usage;

            sys_defaultfont = sys_nearestfontsize(atoi(argv[1]));
            argc -= 2;
            argv += 2;
        }
        else if ((!strcmp(*argv, "-font-face") || !strcmp(*argv, "-typeface")))
        {
            if (argc < 2)
                goto usage;

            strncpy(sys_font,*(argv+1),sizeof(sys_font)-1);
            sys_font[sizeof(sys_font)-1] = 0;
            argc -= 2;
            argv += 2;
        }
        else if (!strcmp(*argv, "-font-weight"))
        {
            if (argc < 2)
                goto usage;

            strncpy(sys_fontweight,*(argv+1),sizeof(sys_fontweight)-1);
            sys_fontweight[sizeof(sys_fontweight)-1] = 0;
            argc -= 2;
            argv += 2;
        }
        else if (!strcmp(*argv, "-verbose"))
        {
            sys_verbose++;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-noverbose"))
        {
            sys_verbose=0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-version"))
        {
            sys_version = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-d") && argc > 1 &&
            sscanf(argv[1], "%d", &sys_debuglevel) >= 1)
        {
            argc -= 2;
            argv += 2;
        }
        else if (!strcmp(*argv, "-loadbang"))
        {
            sys_noloadbang = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-noloadbang"))
        {
            sys_noloadbang = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-gui"))
        {
            sys_dontstartgui = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nogui"))
        {
            sys_dontstartgui = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-guiport") && argc > 1 &&
            sscanf(argv[1], "%d", &sys_guisetportnumber) >= 1)
        {
            argc -= 2;
            argv += 2;
        }
        else if (!strcmp(*argv, "-nostderr"))
        {
            sys_printtostderr = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-stderr"))
        {
            sys_printtostderr = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-guicmd"))
        {
            if (argc < 2)
                goto usage;

            sys_guicmd = argv[1];
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-send"))
        {
            if (argc < 2)
                goto usage;

            sys_messagelist = namelist_append(sys_messagelist, argv[1], 1);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-schedlib"))
        {
            if (argc < 2)
                goto usage;

            sys_externalschedlib = 1;
            strncpy(sys_externalschedlibname, argv[1],
                sizeof(sys_externalschedlibname) - 1);
#ifndef  __APPLE__
                /* no audio I/O please, unless overwritten by later args.
                This is to circumvent a problem running pd~ subprocesses
                with -nogui; they would open an audio device before pdsched.c
                could set the API to nothing.  For some reason though, on
                MACOSX this causes Pd to switch to JACK so we just give up
                and suppress the workaround there. */
            as.a_api = 0;
#endif
            argv += 2;
            argc -= 2;
        }
        else if (!strcmp(*argv, "-extraflags"))
        {
            if (argc < 2)
                goto usage;

            sys_extraflags = 1;
            strncpy(sys_extraflagsstring, argv[1],
                sizeof(sys_extraflagsstring) - 1);
            argv += 2;
            argc -= 2;
        }
        else if (!strcmp(*argv, "-batch"))
        {
            sys_batch = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nobatch"))
        {
            sys_batch = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-autopatch"))
        {
            sys_noautopatch = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-noautopatch"))
        {
            sys_noautopatch = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-compatibility"))
        {
            float f;
            if (argc < 2)
                goto usage;

            if (sscanf(argv[1], "%f", &f) < 1)
                goto usage;
            pd_compatibilitylevel = 0.5 + 100. * f; /* e.g., 2.44 --> 244 */
            argv += 2;
            argc -= 2;
        }
#ifdef HAVE_UNISTD_H
        else if (!strcmp(*argv, "-rt") || !strcmp(*argv, "-realtime"))
        {
            sys_hipriority = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nrt") || !strcmp(*argv, "-nort") || !strcmp(*argv, "-norealtime"))
        {
            sys_hipriority = 0;
            argc--; argv++;
        }
#else
        else if (!strcmp(*argv, "-rt") || !strcmp(*argv, "-realtime")
                 || !strcmp(*argv, "-nrt") || !strcmp(*argv, "-nort")
                 || !strcmp(*argv, "-norealtime"))
        {
            fprintf(stderr, "Pd compiled without realtime priority-support, ignoring '%s' flag\n", *argv);
            argc--; argv++;
        }
#endif
        else if (!strcmp(*argv, "-sleep"))
        {
            sys_nosleep = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nosleep"))
        {
            sys_nosleep = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-noprefs")) /* did this earlier */
            argc--, argv++;
        else if (!strcmp(*argv, "-prefsfile") && argc > 1) /* this too */
            argc -= 2, argv +=2;
        else
        {
        usage:
            sys_printusage();
            return (1);
        }
    }
    if (sys_batch)
        sys_dontstartgui = 1;
    if (sys_dontstartgui)
        sys_printtostderr = 1;
#ifdef _WIN32
    if (sys_printtostderr)
        /* we need to tell Windows to output UTF-8 */
        SetConsoleOutputCP(CP_UTF8);
#endif
    if (!sys_defaultfont)
        sys_defaultfont = DEFAULTFONT;
    for (; argc > 0; argc--, argv++)
        sys_openlist = namelist_append_files(sys_openlist, *argv);

    sys_set_audio_settings(&as);
    return (0);
}

int sys_getblksize(void)
{
    return (DEFDACBLKSIZE);
}

void sys_init_audio(void);

    /* stuff to do, once, after calling sys_argparse() -- which may itself
    be called more than once (first from "settings, second from .pdrc, then
    from command-line arguments */
static void sys_afterargparse(void)
{
    char sbuf[MAXPDSTRING];
    int i;
    t_audiosettings as;
    int nmidiindev = 0, midiindev[MAXMIDIINDEV];
    int nmidioutdev = 0, midioutdev[MAXMIDIOUTDEV];
            /* add "extra" library to path */
    strncpy(sbuf, sys_libdir->s_name, MAXPDSTRING-30);
    sbuf[MAXPDSTRING-30] = 0;
    strcat(sbuf, "/extra");
    sys_setextrapath(sbuf);
            /* add "doc/5.reference" library to helppath */
    strncpy(sbuf, sys_libdir->s_name, MAXPDSTRING-30);
    sbuf[MAXPDSTRING-30] = 0;
    strcat(sbuf, "/doc/5.reference");
    STUFF->st_helppath = namelist_append_files(STUFF->st_helppath, sbuf);

    for (i = 0; i < sys_nmidiin; i++)
        sys_midiindevlist[i]--;
    for (i = 0; i < sys_nmidiout; i++)
        sys_midioutdevlist[i]--;

    if (sys_listplease)
        sys_listdevs();

    sys_get_midi_params(&nmidiindev, midiindev, &nmidioutdev, midioutdev);
    if (sys_nmidiin >= 0)
    {
        nmidiindev = sys_nmidiin;
        for (i = 0; i < nmidiindev; i++)
            midiindev[i] = sys_midiindevlist[i];
    }
    if (sys_nmidiout >= 0)
    {
        nmidioutdev = sys_nmidiout;
        for (i = 0; i < nmidioutdev; i++)
            midioutdev[i] = sys_midioutdevlist[i];
    }
    sys_open_midi(nmidiindev, midiindev, nmidioutdev, midioutdev, 0);

    sys_init_audio();
}

static void sys_addreferencepath(void)
{
    char sbuf[MAXPDSTRING];
}

int sys_argparse(int argc, const char **argv);
static int string2args(const char * cmd, int * retArgc, const char *** retArgv);

void sys_doflags(void)
{
    int rcargc=0;
    const char**rcargv = NULL;
    int len;
    int rcode = 0;
    if (!sys_flags)
        sys_flags = &s_;
    len = (int)strlen(sys_flags->s_name);
    if (len > MAXPDSTRING)
    {
        pd_error(0, "flags: %s: too long", sys_flags->s_name);
        return;
    }
    rcode = string2args(sys_flags->s_name, &rcargc, &rcargv);
    if(rcode < 0) {
        pd_error(0, "error#%d while parsing flags", rcode);
        return;
    }

    if (sys_argparse(rcargc, rcargv))
        pd_error(0, "error parsing startup arguments");

    for(len=0; len<rcargc; len++)
        free((void*)rcargv[len]);
    free(rcargv);
}

/* undo pdtl_encodedialog.  This allows dialogs to send spaces, commas,
    dollars, and semis down here. */
t_symbol *sys_decodedialog(t_symbol *s)
{
    char buf[MAXPDSTRING];
    const char *sp = s->s_name;
    int i;
    if (*sp != '+')
        bug("sys_decodedialog: %s", sp);
    else sp++;
    for (i = 0; i < MAXPDSTRING-1; i++, sp++)
    {
        if (!sp[0])
            break;
        if (sp[0] == '+')
        {
            if (sp[1] == '_')
                buf[i] = ' ', sp++;
            else if (sp[1] == '+')
                buf[i] = '+', sp++;
            else if (sp[1] == 'c')
                buf[i] = ',', sp++;
            else if (sp[1] == 's')
                buf[i] = ';', sp++;
            else if (sp[1] == 'd')
                buf[i] = '$', sp++;
            else buf[i] = sp[0];
        }
        else buf[i] = sp[0];
    }
    buf[i] = 0;
    return (gensym(buf));
}

static void namelist2gui(const char*name, t_namelist*namelist)
{
    const size_t allocchunk = 32;
    int i;
    t_namelist *nl;

    size_t namesize = allocchunk;
    const char**names=(const char**)getbytes(namesize*sizeof(const char*));

    for (nl = namelist, i = 0; nl; nl = nl->nl_next, i++)
    {
        if(i>=namesize) {
            size_t newsize = namesize + allocchunk;
            const char**newnames = (const char**)resizebytes(
                names,
                namesize*sizeof(const char*),
                newsize*sizeof(const char*));
            if (!newnames)
                break;
            names = newnames;
            namesize = newsize;
        }
        names[i] = nl->nl_string;
    }
    pdgui_vmess("set", "rS",
              name,
              i, names);
    freebytes(names, namesize*sizeof(const char*));
}

    /* send the user-specified search path to pd-gui */
void sys_set_searchpath(void)
{
    namelist2gui("::sys_searchpath", STUFF->st_searchpath);
}

    /* send the temp paths from the commandline to pd-gui */
void sys_set_temppath(void)
{
    namelist2gui("::sys_temppath", STUFF->st_temppath);
}

    /* send the hard-coded search path to pd-gui */
void sys_set_extrapath(void)
{
    namelist2gui("::sys_staticpath", STUFF->st_staticpath);
}

    /* start a search path dialog window */
void glob_start_path_dialog(t_pd *dummy)
{
    sys_set_searchpath();
    pdgui_stub_vnew(
        &glob_pdobject,
        "pdtk_path_dialog", (void *)glob_start_path_dialog,
        "ii", sys_usestdpath, sys_verbose);
}

    /* new values from dialog window */
void glob_path_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    namelist_free(STUFF->st_searchpath);
    STUFF->st_searchpath = 0;
    sys_usestdpath = atom_getfloatarg(0, argc, argv);
    sys_verbose = atom_getfloatarg(1, argc, argv);
    for (i = 0; i < argc-2; i++)
    {
        t_symbol *s = sys_decodedialog(atom_getsymbolarg(i+2, argc, argv));
        if (*s->s_name)
            STUFF->st_searchpath =
                namelist_append_files(STUFF->st_searchpath, s->s_name);
    }
}

    /* add one item to search path (intended for use by Deken plugin).
    if "saveit" is > 0, this also saves all settings,
    if "saveit" is < 0, the path is only added temporarily */
void glob_addtopath(t_pd *dummy, t_symbol *path, t_float saveit)
{
    int saveflag = (int)saveit;
    t_symbol *s = sys_decodedialog(path);
    if (*s->s_name)
    {
        if (saveflag < 0)
            STUFF->st_temppath =
                namelist_append_files(STUFF->st_temppath, s->s_name);
        else
            STUFF->st_searchpath =
                namelist_append_files(STUFF->st_searchpath, s->s_name);
        if (saveit > 0)
            sys_savepreferences(0);
    }
}

    /* set the global list vars for startup libraries and flags */
void sys_set_startup(void)
{
    int i;
    t_namelist *nl;
    char obuf[MAXPDSTRING];

    pdgui_vmess("set", "rs", "::startup_flags", (sys_flags ? sys_flags->s_name : ""));

    namelist2gui("::startup_libraries", STUFF->st_externlist);
}

    /* start a startup dialog window */
void glob_start_startup_dialog(t_pd *dummy)
{
    sys_set_startup();
    pdgui_stub_vnew(
        &glob_pdobject,
        "pdtk_startup_dialog", (void *)glob_start_path_dialog,
        "is", sys_defeatrt, sys_flags?sys_flags->s_name:0);
}

    /* new values from dialog window */
void glob_startup_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    namelist_free(STUFF->st_externlist);
    STUFF->st_externlist = 0;
    sys_defeatrt = atom_getfloatarg(0, argc, argv);
    sys_flags = sys_decodedialog(atom_getsymbolarg(1, argc, argv));
    for (i = 0; i < argc-2; i++)
    {
        t_symbol *s = sys_decodedialog(atom_getsymbolarg(i+2, argc, argv));
        if (*s->s_name)
            STUFF->st_externlist =
                namelist_append_files(STUFF->st_externlist, s->s_name);
    }
}


/*
 * the following string2args function is based on from sash-3.8 (the StandAlone SHell)
 * Copyright (c) 2014 by David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 */
#define	isBlank(ch)	(((ch) == ' ') || ((ch) == '\t'))
int string2args(const char * cmd, int * retArgc, const char *** retArgv)
{
    int errCode = 1;
    int len = strlen(cmd), argCount = 0;
    char strings[MAXPDSTRING], *cp;
    const char **argTable = 0, **newArgTable;

    if(retArgc) *retArgc = 0;
    if(retArgv) *retArgv = NULL;

        /*
         * Copy the command string into a buffer that we can modify,
         * reallocating it if necessary.
         */
    if(len >= MAXPDSTRING) {
        errCode = 1; goto ouch;
    }
    memset(strings, 0, MAXPDSTRING);
    memcpy(strings, cmd, len);
    cp = strings;

        /* Keep parsing the command string as long as there are any arguments left. */
    while (*cp) {
        const char *cpIn = cp;
        char *cpOut = cp, *argument;
        int quote = '\0';

            /*
             * Loop over the string collecting the next argument while
             * looking for quoted strings or quoted characters.
             */
        while (*cp) {
            int ch = *cp++;

                /* If we are not in a quote and we see a blank then this argument is done. */
            if (isBlank(ch) && (quote == '\0'))
                break;

                /* If we see a backslash then accept the next character no matter what it is. */
            if (ch == '\\') {
                ch = *cp++;
                if (ch == '\0') { /* but only if there is a next char */
                    errCode = 10; goto ouch;
                }
                *cpOut++ = ch;
                continue;
            }

                /* If we were in a quote and we saw the same quote character again then the quote is done. */
            if (ch == quote) {
                quote = '\0';
                continue;
            }

                /* If we weren't in a quote and we see either type of quote character,
                 * then remember that we are now inside of a quote. */
            if ((quote == '\0') && ((ch == '\'') || (ch == '"')))  {
                quote = ch;
                continue;
            }

                /* Store the character. */
            *cpOut++ = ch;
        }

        if (quote) { /* Unmatched quote character */
            errCode = 11; goto ouch;
        }

            /*
             * Null terminate the argument if it had shrunk, and then
             * skip over all blanks to the next argument, nulling them
             * out too.
             */
        if (cp != cpOut)
            *cpOut = '\0';
        while (isBlank(*cp))
            *cp++ = '\0';

        if (!(argument = calloc(1+cpOut-cpIn, 1))) {
            errCode = 22; goto ouch;
        }
        memcpy(argument, cpIn, cpOut-cpIn);

            /* Now reallocate the argument table to hold the argument, add add it. */
        if (!(newArgTable = (const char **) realloc(argTable, (sizeof(const char *) * (argCount + 1))))) {
            free(argument);
            errCode= 23; goto ouch;
        } else argTable = newArgTable;

        argTable[argCount] = argument;

        argCount++;
    }

        /*
         * Null terminate the argument list and return it.
         */
    if (!(newArgTable = (const char **) realloc(argTable, (sizeof(const char *) * (argCount + 1))))) {
        errCode = 23; goto ouch;
    } else argTable = newArgTable;

    argTable[argCount] = NULL;

    if(retArgc) *retArgc = argCount;
    if(retArgv)
        *retArgv = argTable;
    else
        free(argTable);
    return argCount;

 ouch:
    free(argTable);
    return -errCode;
}
