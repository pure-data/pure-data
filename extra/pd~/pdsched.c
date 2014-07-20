/* Copyright 2008 Miller Puckette.  Berkeley license; see the
file LICENSE.txt in this distribution. */

/* A plug-in scheduler that turns Pd into a filter that inputs and
outputs audio and messages. */

/* todo: 
    fix schedlib code to use extent2
    figure out about  if (sys_externalschedlib) { return; } in s_audio.c
    make buffer size ynamically growable

*/    
#include "m_pd.h"
#include "s_stuff.h"
#include "m_imp.h"
#include <stdio.h>

#define BUFSIZE 65536
static char inbuf[BUFSIZE];

#if defined(__linux__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__GNU__)
void glob_watchdog(t_pd *dummy);

static void pollwatchdog( void)
{
    static int sched_diddsp, sched_nextpingtime;
    sched_diddsp++;
    if (sys_nogui && sys_hipriority && (sched_diddsp - sched_nextpingtime > 0))
    {
        glob_watchdog(0);
            /* ping every 2 seconds */
        sched_nextpingtime = sched_diddsp +
            2 * (int)(sys_dacsr /(double)sys_schedblocksize);
    }
}
#endif

int pd_extern_sched(char *flags)
{
    int naudioindev, audioindev[MAXAUDIOINDEV], chindev[MAXAUDIOINDEV];
    int naudiooutdev, audiooutdev[MAXAUDIOOUTDEV], choutdev[MAXAUDIOOUTDEV];
    int i, j, rate, advance, callback, chin, chout, fill = 0, c, blocksize;
    t_binbuf *b = binbuf_new();

    sys_get_audio_params(&naudioindev, audioindev, chindev,
        &naudiooutdev, audiooutdev, choutdev, &rate, &advance, &callback,
            &blocksize);

    chin = (naudioindev < 1 ? 0 : chindev[0]);
    chout = (naudiooutdev < 1 ? 0 : choutdev[0]);

    /* fprintf(stderr, "Pd plug-in scheduler called, chans %d %d, sr %d\n",
        chin, chout, (int)rate); */
    sys_setchsr(chin, chout, rate);
    sys_audioapi = API_NONE;
    while ((c = getchar()) != EOF)
    {
        if (c == ';')
        {
            int n;
            t_atom *ap;
            binbuf_text(b, inbuf, fill);
            n = binbuf_getnatom(b);
            ap = binbuf_getvec(b);
            fill = 0;
            if (n > 0 && ap[0].a_type == A_FLOAT)
            {
                /* a list -- take it as incoming signals. */
                int chan, nchan = n/DEFDACBLKSIZE;
                t_sample *fp;
                for (i = chan = 0, fp = sys_soundin; chan < nchan; chan++)
                    for (j = 0; j < DEFDACBLKSIZE; j++)
                        *fp++ = atom_getfloat(ap++);
                for (; chan < chin; chan++)
                    for (j = 0; j < DEFDACBLKSIZE; j++)
                        *fp++ = 0;
                sched_tick();
                sys_pollgui();
#if defined(__linux__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__GNU__)
                pollwatchdog();
#endif
                printf(";\n");
                for (i = chout*DEFDACBLKSIZE, fp = sys_soundout; i--; fp++)
                {
                    printf("%g\n", *fp);
                    *fp = 0;
                }
                printf(";\n");
                fflush(stdout);
            }
            else if (n > 1 && ap[0].a_type == A_SYMBOL)
            {
                t_pd *whom = ap[0].a_w.w_symbol->s_thing;
                if (!whom)
                    error("%s: no such object", ap[0].a_w.w_symbol->s_name);
                else if (ap[1].a_type == A_SYMBOL)
                    typedmess(whom, ap[1].a_w.w_symbol, n-2, ap+2);
                else pd_list(whom, 0, n-1, ap+1);
            }
        }
        else if (fill < BUFSIZE)
            inbuf[fill++] = c;
        else if (fill == BUFSIZE)
            fprintf(stderr, "pd-extern: input buffer overflow\n");
    }
    return (0);
}
