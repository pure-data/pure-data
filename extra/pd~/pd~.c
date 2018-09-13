/*
  pd~.c - embed a Pd process within Pd or Max.

  Copyright 2008 Miller Puckette
  BSD license; see README.txt in this distribution for details.
*/

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <process.h>
#include <windows.h>
typedef int socklen_t;
#define EADDRINUSE WSAEADDRINUSE
#else
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#pragma warning (disable: 4305 4244)
#define snprintf sprintf_s
#define stat _stat
#endif

#ifdef MSP
#include "ext.h"
#include "z_dsp.h"
#include "math.h"
#include "ext_support.h"
#include "ext_proto.h"
#include "ext_obex.h"

typedef double t_floatarg;
#define w_symbol w_sym
#define A_SYMBOL A_SYM
#define getbytes t_getbytes
#define freebytes t_freebytes
#define PDERROR error(
void *pd_tilde_class;
#define MAXPDSTRING 4096
#define DEFDACBLKSIZE 64
#endif /* MSP */

#ifdef PD
#include "m_pd.h"
#include "s_stuff.h"
static t_class *pd_tilde_class;
#define PDERROR pd_error(x, 

#endif

#if defined(__x86_64__) || defined(_M_X64)
# define ARCHEXT "amd64"
#elif defined(__i386__) || defined(_M_IX86)
# define ARCHEXT "i386"
#elif defined(__arm__)
# define ARCHEXT "arm"
#elif defined(__aarch64__)
# define ARCHEXT "arm64"
#elif defined(__ppc__)
# define ARCHEXT "ppc"
#endif

#ifdef ARCHEXT
#define ARCHDLLEXT(prefix) prefix ARCHEXT ,
#else
#define ARCHDLLEXT(prefix)
#endif


static const char*pd_tilde_dllextent[] = {
#if defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__GNU__) || defined(__FreeBSD__)
    ARCHDLLEXT(".l_")
    ".pd_linux",
    ".so",
#elif defined(__APPLE__)
    ".d_fat",
    ARCHDLLEXT(".d_")
    ".pd_darwin",
    ".so",
#elif defined(__OPENBSD__)
    ARCHDLLEXT(".o_")
    ".pd_openbsd",
    ".so",
#elif defined(_WIN32) || defined(__CYGWIN__)
    ARCHDLLEXT(".m_")
    ".dll",
#else
    ".so",
#endif
    0};

#define FOOFOO
#include "binarymsg.c"

/* ------------------------ pd_tilde~ ----------------------------- */

#define MSGBUFSIZE 65536

typedef struct _pd_tilde
{
#ifdef PD
    t_object x_obj;
    t_clock *x_clock;
    t_outlet *x_outlet1;        /* for messages back from subproc */
    t_canvas *x_canvas;
#endif /* PD */
#ifdef MSP
    t_pxobject x_obj;
    void *x_outlet1;
    void *x_clock;
#endif /* MSP */
    FILE *x_infd;
    FILE *x_outfd;
    t_binbuf *x_binbuf;
    int x_childpid;
    int x_ninsig;
    int x_noutsig;
    int x_fifo;
    int x_binary;
    t_float x_sr;
    t_symbol *x_pddir;
    t_symbol *x_schedlibdir;
    t_sample **x_insig;
    t_sample **x_outsig;
} t_pd_tilde;

#ifdef MSP
static void *pd_tilde_new(t_symbol *s, long ac, t_atom *av);
static void pd_tilde_tick(t_pd_tilde *x);
static t_int *pd_tilde_perform(t_int *w);
static void pd_tilde_dsp(t_pd_tilde *x, t_signal **sp);
void pd_tilde_assist(t_pd_tilde *x, void *b, long m, long a, char *s);
static void pd_tilde_free(t_pd_tilde *x);
void pd_tilde_setup(void);
int main();
void pd_tilde_minvel_set(t_pd_tilde *x, void *attr, long ac, t_atom *av);
char *strcpy(char *s1, const char *s2);
#endif

static void pd_tilde_tick(t_pd_tilde *x);
static void pd_tilde_close(t_pd_tilde *x)
{
#ifdef _WIN32
    int termstat;
#endif
    if (x->x_outfd)
        fclose(x->x_outfd);
    if (x->x_infd)
        fclose(x->x_infd);
    if (x->x_childpid > 0)
#ifdef _WIN32
        _cwait(&termstat, x->x_childpid, WAIT_CHILD);
#else
        waitpid(x->x_childpid, 0, 0);
#endif
    binbuf_clear(x->x_binbuf);
    x->x_infd = x->x_outfd = 0;
    x->x_childpid = -1;
}

static int pd_tilde_readmessages(t_pd_tilde *x)
{
    t_atom at;
    binbuf_clear(x->x_binbuf);
    if (x->x_binary)
    {
        int nonempty = 0;
        while (1)
        {
            if (!pd_tilde_getatom(&at, x->x_infd))
                return 0;
            if (!nonempty && at.a_type == A_SEMI)
                break;
            nonempty = (at.a_type != A_SEMI);
            binbuf_add(x->x_binbuf, 1, &at);
        }
    }
    else    /* ASCII */
    {
        t_binbuf *tmpb = binbuf_new();
        while (1)
        {
            char msgbuf[MAXPDSTRING];
            int c, infill = 0, n;
            t_atom *vec;
            while (isspace((c = getc(x->x_infd))) && c != EOF)
                ;
            if (c == EOF)
                return 0;
            do
                msgbuf[infill++] = c;
            while (!isspace((c = getc(x->x_infd))) && c != ';' && c != EOF) ;
            binbuf_text(tmpb, msgbuf, infill);
            n = binbuf_getnatom(tmpb);
            vec = binbuf_getvec(tmpb);
            binbuf_add(x->x_binbuf, n, vec);
            if (!n)
            {
                bug("pd~");
                break;  /* shouldn't happen */
            }
            if (vec[0].a_type == A_SEMI)
                break;
        }
        binbuf_free(tmpb);
        /* if (binbuf_getnatom(x->x_binbuf) > 1)
            binbuf_print(x->x_binbuf); */
    }
    clock_delay(x->x_clock, 0);
    return 1;
}

#define FIXEDARG 13
#define MAXARG 100
#ifdef _WIN32
#define EXTENT ".com"
#else
#define EXTENT ""
#endif

static void pd_tilde_donew(t_pd_tilde *x, const char *pddir, const char *schedlibdir,
    const char *patchdir_c, int argc, t_atom *argv, int ninsig, int noutsig,
    int fifo, t_float samplerate)
{
    int i, pid, pipe1[2], pipe2[2];
    char cmdbuf[MAXPDSTRING], pdexecbuf[MAXPDSTRING], schedbuf[MAXPDSTRING],
        tmpbuf[MAXPDSTRING], patchdir[MAXPDSTRING];
    char *execargv[FIXEDARG+MAXARG+1], ninsigstr[20], noutsigstr[20],
        sampleratestr[40];
    const char**dllextent;
    struct stat statbuf;
    x->x_infd = x->x_outfd = 0;
    x->x_childpid = -1;
    if (argc > MAXARG)
    {
        post("pd~: args truncated to %d items", MAXARG);
        argc = MAXARG;
    }
    sprintf(ninsigstr, "%d", ninsig);
    sprintf(noutsigstr, "%d", noutsig);
    sprintf(sampleratestr, "%f", (float)samplerate);
    snprintf(tmpbuf, MAXPDSTRING, "%s/bin/pd" EXTENT, pddir);
    sys_bashfilename(tmpbuf, pdexecbuf);
    if (stat(pdexecbuf, &statbuf) < 0)
    {
        snprintf(tmpbuf, MAXPDSTRING, "%s/../../../bin/pd" EXTENT, pddir);
        sys_bashfilename(tmpbuf, pdexecbuf);
        if (stat(pdexecbuf, &statbuf) < 0)
        {
            snprintf(tmpbuf, MAXPDSTRING, "%s/pd" EXTENT, pddir);
            sys_bashfilename(tmpbuf, pdexecbuf);
            if (stat(pdexecbuf, &statbuf) < 0)
            {
                PDERROR "pd~: can't stat %s", pdexecbuf);
                goto fail1;
            }
        }
    }

        /* check that the scheduler dynamic linkable exists w either suffix */
    for(dllextent=pd_tilde_dllextent; *dllextent; dllextent++)
    {
      snprintf(tmpbuf, MAXPDSTRING, "%s/pdsched%s", schedlibdir, *dllextent);
      sys_bashfilename(tmpbuf, schedbuf);
      if (stat(schedbuf, &statbuf) >= 0)
        goto gotone;
    }
    PDERROR "pd~: can't stat %s", schedbuf);
    goto fail1;

gotone:
        /* but the sub-process wants the scheduler name without the suffix */
    snprintf(tmpbuf, MAXPDSTRING, "%s/pdsched", schedlibdir);
    sys_bashfilename(tmpbuf, schedbuf);
    /* was: snprintf(cmdbuf, MAXPDSTRING,
"'%s' -schedlib '%s'/pdsched -path '%s' -inchannels %d -outchannels %d -r %g %s\n",
        pdexecbuf, schedlibdir, patchdir, ninsig, noutsig, samplerate, pdargs);
        */
    /* _spawnv wants the command without quotes */
    strcpy(cmdbuf, pdexecbuf);
    /* but in the argument vector paths must be quoted if they contain whitespace */
    if (strchr(pdexecbuf, ' ') && *pdexecbuf != '"' && *pdexecbuf != '\'')
    {
        snprintf(tmpbuf, MAXPDSTRING, "\"%s\"", pdexecbuf);
        strcpy(pdexecbuf, tmpbuf);
    }
    if (strchr(schedbuf, ' ') && *schedbuf != '"' && *schedbuf != '\'')
    {
        snprintf(tmpbuf, MAXPDSTRING, "\"%s\"", schedbuf);
        strcpy(schedbuf, tmpbuf);
    }
    if (strchr(patchdir_c, ' ') && *patchdir_c != '"' && *patchdir_c != '\'')
        snprintf(patchdir, MAXPDSTRING, "\"%s\"", patchdir_c);
    else
        snprintf(patchdir, MAXPDSTRING, "%s", patchdir_c);

    execargv[0] = pdexecbuf;
    execargv[1] = "-schedlib";
    execargv[2] = schedbuf;
    execargv[3] = "-extraflags";
    execargv[4] = (x->x_binary ? "b" : "a");
    execargv[5] = "-path";
    execargv[6] = patchdir;
    execargv[7] = "-inchannels";
    execargv[8] = ninsigstr;
    execargv[9] = "-outchannels";
    execargv[10] = noutsigstr;
    execargv[11] = "-r";
    execargv[12] = sampleratestr;

        /* convert atom arguments to strings (temporarily allocating space) */
    for (i = 0; i < argc; i++)
    {
#ifdef PD
        atom_string(&argv[i], tmpbuf, MAXPDSTRING);
#endif
#ifdef MSP
            /* because Mac pathnames sometimes have an evil preceeding
            colon character, we test for and silently eat them */
        if (argv[i].a_type == A_SYM)
            strncpy(tmpbuf, (*argv->a_w.w_sym->s_name == ':'?
                argv->a_w.w_sym->s_name+1 : argv->a_w.w_sym->s_name),
                MAXPDSTRING-3);
        else if (argv[i].a_type == A_LONG)
            sprintf(tmpbuf, "%ld", argv->a_w.w_long);
        else if (argv[i].a_type == A_FLOAT)
            sprintf(tmpbuf,  "%f", argv->a_w.w_float);
#endif
        execargv[FIXEDARG+i] = malloc(strlen(tmpbuf) + 1);
        strcpy(execargv[FIXEDARG+i], tmpbuf);
    }
    execargv[argc+FIXEDARG] = 0;
#if 0
    for (i = 0; i < argc+FIXEDARG; i++)
        fprintf(stderr, "%s ", execargv[i]);
    fprintf(stderr, "\n");
#endif
#ifdef _WIN32
    if (_pipe(pipe1, 65536, O_BINARY | O_NOINHERIT) < 0)   
#else
    if (pipe(pipe1) < 0)   
#endif
    {
        PDERROR "pd~: can't create pipe");
        goto fail1;
    }
#ifdef _WIN32
    if (_pipe(pipe2, 65536, O_BINARY | O_NOINHERIT) < 0)   
#else
    if (pipe(pipe2) < 0)   
#endif
    {
        PDERROR "pd~: can't create pipe");
        goto fail2;
    }
#ifdef _WIN32
    {
        int stdinwas = _dup(0), stdoutwas = _dup(1);
        if (pipe2[1] == 0)
            pipe2[1] = _dup(pipe2[1]);
        if (pipe1[0] != 0)
            _dup2(pipe1[0], 0);
        if (pipe2[1] != 1)
            _dup2(pipe2[1], 1);
        pid = _spawnv(P_NOWAIT, cmdbuf, (const char * const *)execargv);
        if (pid < 0)
        {
            post("%s: couldn't start subprocess (%s)\n", execargv[0],
                strerror(errno));
            goto fail1;
        }
        _dup2(stdinwas, 0);
        _dup2(stdoutwas, 1);
        _close(stdinwas);
        _close(stdoutwas);
    }
#else /* _WIN32 */
    if ((pid = fork()) < 0)
    {
        PDERROR "pd~: can't fork");
        goto fail3;
    }
    else if (pid == 0)
    {
        /* child process */
            /* the first dup2 below would bash pipe2[1] if it happens to be
                zero so in that case renumber it */
        if (pipe2[1] == 0)
            pipe2[1] = dup(pipe2[1]);
        if (pipe1[0] != 0)
        {
            dup2(pipe1[0], 0);
            close (pipe1[0]);
        }
        if (pipe2[1] != 1)
        {
            dup2(pipe2[1], 1);
            close (pipe2[1]);
        }
        if (pipe1[1] >= 2)
            close(pipe1[1]);
        if (pipe2[0] >= 2)
            close(pipe2[0]);
        execv(cmdbuf, execargv);
        _exit(1);
    }
    do {
      unsigned int i;
      for(i=FIXEDARG; execargv[i]; i++) {
        free(execargv[i]);
      }
    } while(0);

#endif /* _WIN32 */
        /* done with fork/exec or spawn; parent continues here */
    close(pipe1[0]);
    close(pipe2[1]);
#ifndef _WIN32      /* this was done in windows via the O_NOINHERIT flag */
    fcntl(pipe1[1],  F_SETFD, FD_CLOEXEC);
    fcntl(pipe2[0],  F_SETFD, FD_CLOEXEC);
#endif
    x->x_outfd = fdopen(pipe1[1], "w");
    x->x_infd = fdopen(pipe2[0], "r");
    x->x_childpid = pid;
    for (i = 0; i < fifo; i++)
        if (x->x_binary)
    {
        putc(A_SEMI, x->x_outfd);
        pd_tilde_putfloat(0, x->x_outfd);
        putc(A_SEMI, x->x_outfd);
    }
    else fprintf(x->x_outfd, "%s", ";\n0;\n");

    fflush(x->x_outfd);
    binbuf_clear(x->x_binbuf);
    pd_tilde_readmessages(x);
    return;
#ifndef _WIN32
fail3:
    close(pipe2[0]);
    close(pipe2[1]);
    if (x->x_childpid > 0)
    waitpid(x->x_childpid, 0, 0);
#endif
fail2:
    close(pipe1[0]);
    close(pipe1[1]);
fail1:
    x->x_infd = x->x_outfd = 0;
    x->x_childpid = -1;
    return;
}

static t_int *pd_tilde_perform(t_int *w)
{
    t_pd_tilde *x = (t_pd_tilde *)(w[1]);
    int n = (int)(w[2]), i, j, nsigs, numbuffill = 0, c;
    char numbuf[80];
    FILE *infd = x->x_infd;
    if (!infd)
        goto zeroit;
    if (x->x_binary)
    {
        putc(A_SEMI, x->x_outfd);
        if (!x->x_ninsig)
            pd_tilde_putfloat(0, x->x_outfd);
        else for (i = 0; i < x->x_ninsig; i++)
        {
            t_sample *fp = x->x_insig[i];
            for (j = 0; j < n; j++)
                pd_tilde_putfloat(*fp++, x->x_outfd);
            for (; j < DEFDACBLKSIZE; j++)
                pd_tilde_putfloat(0, x->x_outfd);
        }
        putc(A_SEMI, x->x_outfd);
    }
    else
    {
        fprintf(x->x_outfd, ";\n");
        if (!x->x_ninsig)
            fprintf(x->x_outfd, "0\n");
        else for (i = 0; i < x->x_ninsig; i++)
        {
            t_sample *fp = x->x_insig[i];
            for (j = 0; j < n; j++)
                fprintf(x->x_outfd, "%g\n", *fp++);
            for (; j < DEFDACBLKSIZE; j++)
                fprintf(x->x_outfd, "0\n");
        }
        fprintf(x->x_outfd, ";\n");
    }
    fflush(x->x_outfd);
    nsigs = j = 0;
    if (x->x_binary)
    {
        while (1)
        {
            t_atom at;
            if (!pd_tilde_getatom(&at, infd))
            {
                if (errno)
                    PDERROR "pd~: %s", strerror(errno));
                else PDERROR "pd~: subprocess exited");
                pd_tilde_close(x);
                goto zeroit;
            }
            if (at.a_type == A_SEMI)
                break;
            else if (at.a_type == A_FLOAT)
            {
                if (nsigs < x->x_noutsig)
                    x->x_outsig[nsigs][j] = at.a_w.w_float;
                if (++j >= DEFDACBLKSIZE)
                    j = 0, nsigs++;
            }
            else PDERROR "pd~: subprocess returned malformed audio");
        }
    }
    else
    {
        while (1)
        {
            while (1)
            {
                c = getc(infd);
                if (c == EOF)
                {
                    if (errno)
                        PDERROR "pd~: %s", strerror(errno));
                    else PDERROR "pd~: subprocess exited");
                    pd_tilde_close(x);
                    goto zeroit;
                }
                else if (!isspace(c) && c != ';')
                {
                    if (numbuffill < (80-1))
                        numbuf[numbuffill++] = c;
                }
                else
                {
                    t_sample z;
                    if (numbuffill)
                    {
                        numbuf[numbuffill] = 0;
#if PD_FLOATSIZE == 32
                        if (sscanf(numbuf, "%f", &z) < 1)
#else
                        if (sscanf(numbuf, "%lf", &z) < 1)
#endif
                            continue;
                        if (nsigs < x->x_noutsig)
                            x->x_outsig[nsigs][j] = z;
                        if (++j >= DEFDACBLKSIZE)
                            j = 0, nsigs++;
                    }
                    numbuffill = 0;
                    break;
                }
            }
            /* message terminated */
            if (c == ';')
                break;
        }
    }
    if (nsigs < x->x_noutsig)
        post("sigs %d, j %d", nsigs, j);
    for (; nsigs < x->x_noutsig; nsigs++, j = 0)
    {
        for (; j < DEFDACBLKSIZE; j++)
            x->x_outsig[nsigs][j] = 0;
    }
    if (!pd_tilde_readmessages(x))
    {
        if (errno)
            PDERROR "pd~: %s", strerror(errno));
        else PDERROR "pd~: subprocess exited");
        pd_tilde_close(x);
    }
    return (w+3);
zeroit:
    for (i = 0; i < x->x_noutsig; i++)
    {
        for (j = 0; j < DEFDACBLKSIZE; j++)
            x->x_outsig[i][j] = 0;
    }
    return (w+3);
}

static void pd_tilde_dsp(t_pd_tilde *x, t_signal **sp)
{
    int i, n = (x->x_ninsig || x->x_noutsig ? sp[0]->s_n : 1);
    t_sample **g;
        
    for (i = 0, g = x->x_insig; i < x->x_ninsig; i++, g++)
        *g = (*(sp++))->s_vec;
        /* if there were no input signals Pd still provided us with one,
        which we ignore: */
    if (!x->x_ninsig)
        sp++;
    for (i = 0, g = x->x_outsig; i < x->x_noutsig; i++, g++)
        *g = (*(sp++))->s_vec;
    
    dsp_add(pd_tilde_perform, 2, x, n);
}

static void pd_tilde_pdtilde(t_pd_tilde *x, t_symbol *s,
    int argc, t_atom *argv)
{
    t_symbol *sel = ((argc > 0 && argv->a_type == A_SYMBOL) ?
        argv->a_w.w_symbol : gensym("?")), *schedlibdir;
    const char *patchdir;
    if (sel == gensym("start"))
    {
        char pdargstring[MAXPDSTRING];
        if (x->x_infd)
            pd_tilde_close(x);
        pdargstring[0] = 0;
        argc--; argv++;
#ifdef PD
        patchdir = canvas_getdir(x->x_canvas)->s_name;
#endif
#ifdef MSP
        patchdir = ".";
#endif
        schedlibdir = x->x_schedlibdir;
        if (schedlibdir == gensym(".") && x->x_pddir != gensym("."))
        {
            const char *pds = x->x_pddir->s_name;
            char scheddirstring[MAXPDSTRING];
            int l = strlen(pds);
            if (l >= 4 && (!strcmp(pds+l-3, "bin") || !strcmp(pds+l-4, "bin/")))
                snprintf(scheddirstring, MAXPDSTRING, "%s/../extra/pd~", pds);
            else snprintf(scheddirstring, MAXPDSTRING, "%s/extra/pd~", pds);
            schedlibdir = gensym(scheddirstring);
        }
        pd_tilde_donew(x, x->x_pddir->s_name, schedlibdir->s_name,
            patchdir, argc, argv, x->x_ninsig, x->x_noutsig, x->x_fifo,
                x->x_sr);
    }
    else if (sel == gensym("stop"))
    {
        if (x->x_infd)
            pd_tilde_close(x);
    }
    else if (sel == gensym("pddir"))
    {
        if ((argc > 1) && argv[1].a_type == A_SYMBOL)
        {
            t_symbol *sym = argv[1].a_w.w_symbol;
#ifdef MSP
            if (sym->s_name[0] == ':')
                sym = gensym(s->s_name+1);
#endif
            x->x_pddir = sym;
        }
        else PDERROR "pd~ pddir: needs symbol argument");
    }
    else PDERROR "pd~: unknown control message: %s", sel->s_name);
}

static void pd_tilde_free(t_pd_tilde *x)
{
#ifdef MSP
    dsp_free((t_pxobject *)x);
#endif
    pd_tilde_close(x);
    clock_free(x->x_clock);
}

/* -------------------------- Pd glue ------------------------- */
#ifdef PD

static void pd_tilde_tick(t_pd_tilde *x)
{
    int messstart = 0, i, n;
    t_atom *vec;
    /* binbuf_print(b); */
    n = binbuf_getnatom(x->x_binbuf);
    vec = binbuf_getvec(x->x_binbuf);
    for (i = 0; i < n; i++)
    {
        if (vec[i].a_type == A_SEMI)
        {
            if (i > messstart && vec[messstart].a_type == A_SYMBOL)
                outlet_anything(x->x_outlet1, vec[messstart].a_w.w_symbol,
                    i-(messstart+1), vec+(messstart+1));
            else if (i > messstart)
                outlet_list(x->x_outlet1, 0, i-messstart, vec+messstart);
            messstart = i+1;
        }
    }
    binbuf_clear(x->x_binbuf);
}

static void pd_tilde_anything(t_pd_tilde *x, t_symbol *s,
    int argc, t_atom *argv)
{
    char msgbuf[MAXPDSTRING];
    if (!x->x_outfd)
        return;
    if (x->x_binary)
    {
        pd_tilde_putsymbol(s, x->x_outfd);
        for (; argc--; argv++)
        {
            if (argv->a_type == A_FLOAT)
                pd_tilde_putfloat(argv->a_w.w_float, x->x_outfd);
            else if (argv->a_type == A_SYMBOL)
                pd_tilde_putsymbol(argv->a_w.w_symbol, x->x_outfd);
        }
        putc(A_SEMI, x->x_outfd);
    }
    else
    {
        fprintf(x->x_outfd, "%s ", s->s_name);
        while (argc--)
        {
            atom_string(argv++, msgbuf, MAXPDSTRING);
            fprintf(x->x_outfd, "%s ", msgbuf);
        }
        fprintf(x->x_outfd, ";\n");
    }
}

static void *pd_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_pd_tilde *x = (t_pd_tilde *)pd_new(pd_tilde_class);
    int ninsig = 2, noutsig = 2, j, fifo = 5, binary = 1;
    t_float sr = sys_getsr();
    t_sample **g;
    t_symbol *pddir = sys_libdir,
        *scheddir = gensym(class_gethelpdir(pd_tilde_class));
    /* fprintf(stderr, "pd %s, sched %s\n", pddir->s_name, scheddir->s_name); */
    while (argc > 0)
    {
        t_symbol *firstarg = atom_getsymbolarg(0, argc, argv);
        if (!strcmp(firstarg->s_name, "-sr") && argc > 1)
        {
            sr = atom_getfloatarg(1, argc, argv);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-ninsig") && argc > 1)
        {
            ninsig = atom_getfloatarg(1, argc, argv);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-noutsig") && argc > 1)
        {
            noutsig = atom_getfloatarg(1, argc, argv);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-fifo") && argc > 1)
        {
            fifo = atom_getfloatarg(1, argc, argv);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-pddir") && argc > 1)
        {
            pddir = atom_getsymbolarg(1, argc, argv);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-scheddir") && argc > 1)
        {
            scheddir = atom_getsymbolarg(1, argc, argv);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(firstarg->s_name, "-ascii"))
        {
            binary = 0;
            argc--; argv++;
        }
        else break;
    }

    if (argc)
    {
        pd_error(x,
"usage: pd~ [-sr #] [-ninsig #] [-noutsig #] [-fifo #] [-pddir <>]");
        post(
"... [-scheddir <>]");
    }

    x->x_clock = clock_new(x, (t_method)pd_tilde_tick);
    x->x_insig = (t_sample **)getbytes(ninsig * sizeof(*x->x_insig));
    x->x_outsig = (t_sample **)getbytes(noutsig * sizeof(*x->x_outsig));
    x->x_ninsig = ninsig;
    x->x_noutsig = noutsig;
    x->x_fifo = fifo;
    x->x_sr = sr;
    x->x_pddir = pddir;
    x->x_schedlibdir = scheddir;
    x->x_infd = 0;
    x->x_outfd = 0;
    x->x_outfd = 0;
    x->x_childpid = -1;
    x->x_canvas = canvas_getcurrent();
    x->x_binbuf = binbuf_new();
    x->x_binary = binary;
    for (j = 1, g = x->x_insig; j < ninsig; j++, g++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_outlet1 = outlet_new(&x->x_obj, 0);
    for (j = 0, g = x->x_outsig; j < noutsig; j++, g++)
        outlet_new(&x->x_obj, &s_signal);
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    return (x);
}

void pd_tilde_setup(void)
{
    pd_tilde_class = class_new(gensym("pd~"), (t_newmethod)pd_tilde_new,
        (t_method)pd_tilde_free, sizeof(t_pd_tilde), 0, A_GIMME, 0);
    class_addmethod(pd_tilde_class, nullfn, gensym("signal"), 0);
    class_addmethod(pd_tilde_class, (t_method)pd_tilde_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(pd_tilde_class, (t_method)pd_tilde_pdtilde, gensym("pd~"), A_GIMME, 0);
    class_addanything(pd_tilde_class, pd_tilde_anything);
    post("pd~ version 0.3");
}
#endif

/* -------------------------- Max/MSP glue ------------------------- */
#ifdef MSP

#define LOTS 10000

static void pd_tilde_tick(t_pd_tilde *x)
{
    int messstart = 0, i, n = 0;
    t_atom vec[LOTS];
    long z1 = 0, z2 = 0;
    /* binbuf_print(b); */
    while (!binbuf_getatom(x->x_binbuf, &z1, &z2, vec+n))
    if (++n >= LOTS)
        break;
    for (i = 0; i < n; i++)
    {
        if (vec[i].a_type == A_SEMI)
        {
            if (i > messstart + 1)
            {
                void *whom;
                if (vec[messstart].a_type == A_SYM)
                    outlet_anything(x->x_outlet1, vec[messstart].a_w.w_sym,
                        i-messstart-1, vec+(messstart+1));
                else if (vec[messstart].a_type == A_FLOAT && i == messstart+1)
                    outlet_float(x->x_outlet1, vec[messstart].a_w.w_float);
                else if (vec[messstart].a_type == A_LONG && i == messstart+1)
                    outlet_int(x->x_outlet1, vec[messstart].a_w.w_long);
                else outlet_list(x->x_outlet1, gensym("list"),
                    i-messstart, vec+(messstart));
            }
            messstart = i+1;
        }
    }
    binbuf_clear(b);
}

static void pd_tilde_anything(t_pd_tilde *x, t_symbol *s,
    long ac, t_atom *av)
{
    char msgbuf[MAXPDSTRING], *sp, *ep = msgbuf+MAXPDSTRING;
    if (!x->x_outfd)
        return;
    msgbuf[0] = 0;
    strncpy(msgbuf, s->s_name, MAXPDSTRING);
    msgbuf[MAXPDSTRING-1] = 0;
    sp = msgbuf + strlen(msgbuf);
    while (ac--)
    {
        if (sp < ep-1)
            sp[0] = ' ', sp[1] = 0, sp++;
        if (sp < ep - 80)
        {
            if (av->a_type == A_SYM && strlen(av->a_w.w_sym->s_name) < ep - sp-20)
                strcpy(sp, av->a_w.w_sym->s_name);
            else if (av->a_type == A_LONG)
                sprintf(sp, "%ld" ,av->a_w.w_long);
            else if (av->a_type == A_FLOAT)
                sprintf(sp, "%g" ,av->a_w.w_float);
        }
        sp += strlen(sp);
        av++;
    }
    fprintf(x->x_outfd, "%s;\n", msgbuf);
}

int main()
{       
    t_class *c;

    c = class_new("pd_tilde~", (method)pd_tilde_new, (method)pd_tilde_free, sizeof(t_pd_tilde), (method)0L, A_GIMME, 0);

    class_addmethod(c, (method)pd_tilde_dsp, "dsp", A_CANT, 0);
    class_addmethod(c, (method)pd_tilde_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)pd_tilde_pdtilde, "pd~", A_GIMME, 0);
    class_addmethod(c, (method)pd_tilde_anything, "anything", A_GIMME, 0);
    class_dspinit(c);

    class_register(CLASS_BOX, c);
    pd_tilde_class = c;
    post("pd~ version 0.3");
    return (0);
}

static void *pd_tilde_new(t_symbol *s, long ac, t_atom *av)
{
    int ninsig = 2, noutsig = 2, fifo = 5, j;
    t_float sr = sys_getsr();
    t_symbol *pddir = gensym("."), *scheddir = gensym(".");
    t_pd_tilde *x;

    if (x = (t_pd_tilde *)object_alloc(pd_tilde_class))
    {
        while (ac > 0 && av[0].a_type == A_SYM)
        {
            const char *flag = av[0].a_w.w_sym->s_name;
            if (!strcmp(flag, "-sr") && ac > 1)
            {
                sr = (av[1].a_type == A_FLOAT ? av[1].a_w.w_float :
                    (av[1].a_type == A_LONG ? av[1].a_w.w_long : 0));
                ac -= 2; av += 2;
            }
            else if (!strcmp(flag, "-ninsig") && ac > 1)
            {
                ninsig = (av[1].a_type == A_FLOAT ? av[1].a_w.w_float :
                    (av[1].a_type == A_LONG ? av[1].a_w.w_long : 0));
                ac -= 2; av += 2;
            }
            else if (!strcmp(flag, "-noutsig") && ac > 1)
            {
                noutsig = (av[1].a_type == A_FLOAT ? av[1].a_w.w_float :
                    (av[1].a_type == A_LONG ? av[1].a_w.w_long : 0));
                ac -= 2; av += 2;
            }
            else if (!strcmp(flag, "-fifo") && ac > 1)
            {
                fifo = (av[1].a_type == A_FLOAT ? av[1].a_w.w_float :
                    (av[1].a_type == A_LONG ? av[1].a_w.w_long : 0));
                ac -= 2; av += 2;
            }
            else if (!strcmp(flag, "-pddir") && ac > 1)
            {
                pddir = (av[1].a_type == A_SYM ? av[1].a_w.w_sym : gensym("."));
                ac -= 2; av += 2;
            }
            else if (!strcmp(flag, "-scheddir") && ac > 1)
            {
                scheddir = (av[1].a_type == A_SYM ? av[1].a_w.w_sym : gensym("."));
                ac -= 2; av += 2;
            }
            else break;
        }
        if (ac)
            post("pd~: warning: ignoring extra arguments");
        dsp_setup((t_pxobject *)x, ninsig);
        x->x_outlet1 = outlet_new(&x->x_obj, 0);
        for (j = 0; j < noutsig; j++)
            outlet_new((t_pxobject *)x, "signal");
        x->x_clock = clock_new(x, (method)pd_tilde_tick);
        x->x_insig = (t_sample **)getbytes(ninsig * sizeof(*x->x_insig));
        x->x_outsig = (t_sample **)getbytes(noutsig * sizeof(*x->x_outsig));
        x->x_ninsig = ninsig;
        x->x_noutsig = noutsig;
        x->x_fifo = fifo;
        x->x_sr = sr;
        x->x_pddir = pddir;
        x->x_schedlibdir = scheddir;
        x->x_infd = 0;
        x->x_outfd = 0;
        x->x_outfd = 0;
        x->x_childpid = -1;
        x->x_binbuf = binbuf_new();
        x->x_binary = binary;
    }
    return (x);
}

void pd_tilde_assist(t_pd_tilde *x, void *b, long m, long a, char *s)
{
}

#endif /* MSP */
