/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#ifndef MSW     /* in unix this only works first; in MSW it only works last. */
#include "tk.h"
#endif

#include "t_tk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>

#ifndef MSW
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#ifdef HAVE_BSTRING_H
#include <bstring.h>
#endif
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#endif
#ifdef MSW
#include <winsock.h>
#include <io.h>
#endif

/* These pragmas are only used for MSVC, not MinGW or Cygwin <hans@at.or.at> */
#ifdef _MSC_VER
#pragma warning( disable : 4305 )  /* uncast const double to float */
#pragma warning( disable : 4244 )  /* uncast double to float */
#pragma warning( disable : 4101 )  /* unused local variables */
#endif

#ifdef MSW
#include "tk.h"
#endif

#ifdef __APPLE__
#define STARTGUI
#endif

#ifdef __linux__
#define STARTGUI
#endif

#define FIRSTPORTNUM 5600

void tcl_mess(char *s);
static Tcl_Interp *tk_pdinterp;
static int pd_portno = 0;


/***************** the socket setup code ********************/

/* If this is reset by pdgui_setsock(), it's the port number Pd will try to
connect to; but if zero, that means we should set it and start Pd ourselves. */


        /* some installations of linux don't know about "localhost" so give
        the loopback address; NT, on the other hand, can't understand the
        hostname "127.0.0.1". */
char hostname[100] =
#ifdef __linux__
    "127.0.0.1";
#else
    "localhost";
#endif

void pdgui_setsock(int port)
{
    pd_portno = port;
}

    /* why is this here??? probably never used (see t_main.c). */
void pdgui_sethost(char *name)
{
    strncpy(hostname, name, 100);
    hostname[99] = 0;
}

static void pdgui_sockerror(char *s)
{
#ifdef MSW
    int err = WSAGetLastError();
#endif
#ifndef MSW
    int err = errno;
#endif

    fprintf(stderr, "%s: %s (%d)\n", s, strerror(err), err);
    tcl_mess("exit\n");
    exit(1);
}

static int sockfd;

/* The "pd_readsocket" command, which polls the socket. */

#define CHUNKSIZE 20000 /* chunks to allocate memory for reading socket */
#define READSIZE 10000  /* size of read to issue */

static char *pd_tkbuf = 0;      /* buffer for reading */
static int pd_tkbufsize = 0;    /* current buffer size */
static int pd_buftail = 0;      /* number of bytes already in buffer */
static int pd_bufhead = 0;    /* index of first byte to read */

    /* mask argument unused but is here to follow tcl's prototype. */
static void pd_readsocket(ClientData cd, int mask)
{
    fd_set readset, writeset, exceptset;
    struct timeval timout;

    timout.tv_sec = 0;
    timout.tv_usec = 0;
    FD_ZERO(&writeset);
    FD_ZERO(&readset);
    FD_ZERO(&exceptset);
    FD_SET(sockfd, &readset);
    FD_SET(sockfd, &exceptset);
    if (!pd_tkbuf)
    {
        if (!(pd_tkbuf = malloc(CHUNKSIZE)))
        {
            fprintf(stderr, "pd-gui: out of memory\n");
            tcl_mess("exit\n");
        }
        pd_tkbufsize = CHUNKSIZE;
    }
    if (pd_buftail + READSIZE + 1 > pd_tkbufsize)
    {   
        int newsize = pd_tkbufsize + CHUNKSIZE;
        char *newbuf = realloc(pd_tkbuf, newsize);
        if (!newbuf)
        {
            fprintf(stderr, "pd-gui: out of memory\n");
            tcl_mess("exit\n");
        }
        pd_tkbuf = newbuf;
        pd_tkbufsize = newsize;     
    }
    if (select(sockfd+1, &readset, &writeset, &exceptset, &timout) < 0)
        perror("select");
    if (FD_ISSET(sockfd, &exceptset) || FD_ISSET(sockfd, &readset))
    {
        int ret;
        ret = recv(sockfd, pd_tkbuf + pd_buftail, READSIZE, 0);
        if (ret < 0)
            pdgui_sockerror("socket receive error");
        else if (ret == 0)
        {
            /* fprintf(stderr, "read %d\n", SOCKSIZE - pd_buftail); */
            fprintf(stderr, "pd_gui: pd process exited\n");
            tcl_mess("exit\n");
        }
        else
        {
            pd_buftail += ret;
            while (1)
            {
                char lastc = 0, *gotcr = 0, *bp = pd_tkbuf + pd_bufhead,
                    *ep = pd_tkbuf + pd_buftail;
                int brace = 0;
                    /* search for locations that terminate a complete TK
                    command.  These are carriage returns which are not inside
                    any braces.  Braces can be escaped with backslashes (but
                    backslashes themselves can't.) */
                while (bp < ep)
                {
                    char c = *bp;
                    if (c == '}' && brace)
                        brace--;
                    else if (c == '{')
                        brace++;
                    else if (!brace && c == '\n' && lastc != '\\')
                    {
                        gotcr = bp;
                        break;
                    }
                    lastc = c;
                    bp++;
                }
                    /* if gotcr is set there is at least one complete TK
                    command in the buffer, and gotcr terminates the first one.
                    Because sending the command to tcl may cause this code to
                    be reentered, we first copy the command and take it out of
                    the buffer, then execute the command.
                    Execute it and slide any
                    extra bytes to beginning of the buffer. */
                if (gotcr)
                {
                    int bytesincmd = (gotcr - (pd_tkbuf+pd_bufhead)) + 1;
                    char smallcmdbuf[1000], *realcmdbuf;
                    if (gotcr - (pd_tkbuf+pd_bufhead) < 998)
                        realcmdbuf = smallcmdbuf;
                    else realcmdbuf = malloc(bytesincmd+1);
                    if (realcmdbuf)
                    {
                        strncpy(realcmdbuf, pd_tkbuf+pd_bufhead, bytesincmd);
                        realcmdbuf[bytesincmd] = 0;
                    }
                    pd_bufhead += bytesincmd;
                    if (realcmdbuf)
                    {
                        tcl_mess(realcmdbuf);
                        if (realcmdbuf != smallcmdbuf)
                            free(realcmdbuf);
                    }
                    if (pd_buftail < pd_bufhead)
                        fprintf(stderr, "tkcmd bug\n");
                }
                else break;
            }
            if (pd_bufhead)
            {
                if (pd_buftail > pd_bufhead)
                    memmove(pd_tkbuf, pd_tkbuf + pd_bufhead,
                        pd_buftail-pd_bufhead);
                pd_buftail  -= pd_bufhead;
                pd_bufhead = 0;
            }
        }
    }
}

#ifdef MSW
    /* if we're in Gatesland, we add a tcl command to poll the
    socket for data.  */
static int pd_pollsocketCmd(ClientData cd, Tcl_Interp *interp,
    int argc, char **argv)
{
    pd_readsocket(cd, 0);
    return (TCL_OK);
}
#endif

static void pd_sockerror(char *s)
{
#ifdef MSW
    int err = WSAGetLastError();
    if (err == 10054) return;
    else if (err == 10044)
    {
        fprintf(stderr,
            "Warning: you might not have TCP/IP \"networking\" turned on\n");
        fprintf(stderr, "which is needed for Pd to talk to its GUI layer.\n");
    }
#else
    int err = errno;
#endif
    fprintf(stderr, "%s: %s (%d)\n", s, strerror(err), err);
}

static void pdgui_connecttosocket(void)
{
    struct sockaddr_in server;
    struct hostent *hp;
#ifndef MSW
    int retry = 10;
#else
    int retry = 1;
#endif
#ifdef MSW
    short version = MAKEWORD(2, 0);
    WSADATA nobby;

    if (WSAStartup(version, &nobby)) pdgui_sockerror("setup");
#endif

    /* create a socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) pdgui_sockerror("socket");

    /* connect socket using hostname provided in command line */
    server.sin_family = AF_INET;

    hp = gethostbyname(hostname);

    if (hp == 0)
    {
        fprintf(stderr,
            "localhost not found (inet protocol not installed?)\n");
        exit(1);
    }
    memcpy((char *)&server.sin_addr, (char *)hp->h_addr, hp->h_length);

    /* assign client port number */
    server.sin_port = htons((unsigned short)pd_portno);

        /* try to connect */
    while (1)
    {
        if (connect(sockfd, (struct sockaddr *) &server, sizeof (server)) >= 0)
            goto gotit;
        retry--;
        if (retry <= 0)
            break;
          /* In unix there's a race condition; the child won't be
          able to connect before the parent (pd) has shed its
          setuid-ness.  In case this is the problem, sleep and
          retry. */
        else
        {
#ifndef MSW
            fd_set readset, writeset, exceptset;
            struct timeval timout;

            timout.tv_sec = 0;
            timout.tv_usec = 100000;
            FD_ZERO(&writeset);
            FD_ZERO(&readset);
            FD_ZERO(&exceptset);
            fprintf(stderr, "retrying connect...\n");
            if (select(1, &readset, &writeset, &exceptset, &timout) < 0)
                perror("select");
#endif /* !MSW */
        }
    }
    pdgui_sockerror("connecting stream socket");
gotit: ;
#ifndef MSW
        /* normally we ask TK to call us back; but in MSW we have to poll. */
    Tk_CreateFileHandler(sockfd, TK_READABLE | TK_EXCEPTION,
        pd_readsocket, 0);
#endif /* !MSW */
}

#ifdef STARTGUI

/* #define DEBUGCONNECT */

#ifdef DEBUGCONNECT
static FILE *debugfd;
#endif


static void pd_startfromgui( void)
{
    pid_t childpid;
    char cmdbuf[1000], pdbuf[1000], *lastchar;
    const char *arg0;
    struct sockaddr_in server;
    int msgsock;
    int len = sizeof(server), nchar;
    int ntry = 0, portno = FIRSTPORTNUM;
    int xsock = -1;
    char morebuf[256];
#ifdef MSW
    short version = MAKEWORD(2, 0);
    WSADATA nobby;
    char scriptbuf[1000], wishbuf[1000], portbuf[80];
    int spawnret;
    char intarg;
#else
    int intarg;
#endif

    arg0 = Tcl_GetVar(tk_pdinterp, "argv0", 0);
    if (!arg0)
    {
        fprintf(stderr, "Pd-gui: can't get arg 0\n");
        return;
    }
    lastchar = strrchr(arg0, '/');
    if (lastchar)
        snprintf(pdbuf, lastchar - arg0 + 1, "%s", arg0);
    else strcpy(pdbuf, ".");
    strcat(pdbuf, "/../bin/pd");
#ifdef DEBUGCONNECT     
    fprintf(stderr, "pdbuf is %s\n", pdbuf);
#endif

#ifdef MSW
    if (WSAStartup(version, &nobby))
        pd_sockerror("WSAstartup");
#endif

    /* create a socket */
    xsock = socket(AF_INET, SOCK_STREAM, 0);
    if (xsock < 0) pd_sockerror("socket");
    intarg = 1;
    if (setsockopt(xsock, IPPROTO_TCP, TCP_NODELAY,
        &intarg, sizeof(intarg)) < 0)
            fprintf(stderr, "setsockopt (TCP_NODELAY) failed\n");

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;

    /* assign server port number */
    server.sin_port =  htons((unsigned short)portno);

        /* name the socket */
    while (bind(xsock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
#ifdef MSW
        int err = WSAGetLastError();
#else
        int err = errno;
#endif
        if ((ntry++ > 20) || (err != EADDRINUSE))
        {
            perror("bind");
            fprintf(stderr,
                "couldn't open GUI-to-pd network connection\n");
            return;
        }
        portno++;
        server.sin_port = htons((unsigned short)(portno));
    }

#ifdef DEBUGCONNECT     
    fprintf(debugfd, "port %d\n", portno);
        fflush(debugfd);
#endif

#ifdef UNISTD
    childpid = fork();
    if (childpid < 0)
    {
        if (errno) perror("sys_startgui");
        else fprintf(stderr, "sys_startgui failed\n");
        return;
    }
    else if (!childpid)                 /* we're the child */
    {
        sprintf(cmdbuf, "\"%s\" -guiport %d\n", pdbuf, portno);
#ifdef DEBUGCONNECT     
        fprintf(debugfd, "%s", cmdbuf);
        fflush(debugfd);
#endif
        execl("/bin/sh", "sh", "-c", cmdbuf, (char*)0);
        perror("pd: exec");
        _exit(1);
    }
#endif /* UNISTD */

#ifdef MSW       

#error not yet used.... sys_bashfilename() not filled in here

    strcpy(cmdbuf, pdcmd);
    strcat(cmdbuf, "/pd.exe");
    sys_bashfilename(scriptbuf, scriptbuf);

    sprintf(portbuf, "%d", portno);

    spawnret = _spawnl(P_NOWAIT, cmdbuf, "pd.exe", "-port", portbuf, 0);
    if (spawnret < 0)
    {
        perror("spawnl");
        fprintf(stderr, "%s: couldn't start\n", cmdbuf);
        return;
    }

#endif /* MSW */

#ifdef DEBUGCONNECT
        fprintf(stderr, "Waiting for connection request... \n");
#endif
    if (listen(xsock, 5) < 0) pd_sockerror("listen");
    sockfd = accept(xsock, (struct sockaddr *) &server, (unsigned int *)&len);
    if (sockfd < 0) pd_sockerror("accept");
#ifdef DEBUGCONNECT
        fprintf(stderr, "... connected\n");
#endif

#ifndef MSW
        /* normally we ask TK to call us back; but in MSW we have to poll. */
    Tk_CreateFileHandler(sockfd, TK_READABLE | TK_EXCEPTION,
        pd_readsocket, 0);
#endif /* !MSW */
}

#endif /* STARTGUI */

static void pdgui_setupsocket(void)
{
#ifdef MSW
        pdgui_connecttosocket();
#else
    if (pd_portno)
        pdgui_connecttosocket();
    else pd_startfromgui() ;
#endif
}

/**************************** commands ************************/
static char *pdgui_path;

/* The "pd" command, which cats its args together and throws the result
* at the Pd interpreter.
*/
#define MAXWRITE 1024

static int pdCmd(ClientData cd, Tcl_Interp *interp, int argc,  char **argv)
{
    if (argc == 2)
    {
        int n = strlen(argv[1]);
        if (send(sockfd, argv[1], n, 0) < n)
        {
            perror("stdout");
            tcl_mess("exit\n");
        }
    }
    else
    {
        int i;
        char buf[MAXWRITE];
        buf[0] = 0;
        for (i = 1; i < argc; i++)
        {
            if (strlen(argv[i]) + strlen(buf) + 2 > MAXWRITE)
            {
                interp->result = "pd: arg list too long";
                return (TCL_ERROR);     
            }
            if (i > 1) strcat(buf, " ");
            strcat(buf, argv[i]);
        }
        if (send(sockfd, buf, strlen(buf), 0) < 0)
        {
            perror("stdout");
            tcl_mess("exit\n");
        }
    }
    return (TCL_OK);
}

/***********  "c" level access to tk functions. ******************/

void tcl_mess(char *s)
{
    int result;
    result = Tcl_Eval(tk_pdinterp,  s);
    if (result != TCL_OK)
    {
        if (*tk_pdinterp->result) printf("%s\n",  tk_pdinterp->result);
    }
}

    /* in linux, we load the tk code from here (in MSW and MACOS, this
    is done by passing the name of the file as a startup argument to
    the wish shell.) */
#if !defined(MSW) && !defined(__APPLE__)
void pdgui_doevalfile(Tcl_Interp *interp, char *s)
{
    char buf[GUISTRING];
    sprintf(buf, "set pd_guidir \"%s\"\n", pdgui_path);
    tcl_mess(buf);
    strcpy(buf, pdgui_path);
    strcat(buf, "/bin/");
    strcat(buf, s);
    if (Tcl_EvalFile(interp, buf) != TCL_OK)
    {
        char buf2[1000];
        sprintf(buf2, "puts [concat tcl: %s: can't open script]\n",
            buf);
        tcl_mess(buf2);
    }
}

void pdgui_evalfile(char *s)
{
    pdgui_doevalfile(tk_pdinterp, s);
}
#endif

void pdgui_startup(Tcl_Interp *interp)
{
        /* save pointer to the main interpreter */
    tk_pdinterp = interp;

        /* add our own TK commands */
    Tcl_CreateCommand(interp, "pd",  (Tcl_CmdProc*)pdCmd, (ClientData)NULL, 
        (Tcl_CmdDeleteProc *)NULL); 
#ifdef MSW
    Tcl_CreateCommand(interp, "pd_pollsocket",(Tcl_CmdProc*)  pd_pollsocketCmd,
        (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
#endif
    pdgui_setupsocket();
        /* read in the startup file */
#if !defined(MSW) && !defined(__APPLE__)
    pdgui_evalfile("pd.tk");
#endif
}

#ifndef MSW
void pdgui_setname(char *s)
{
    char *t;
    char *str;
    int n;
    if (t = strrchr(s, '/')) str = s, n = (t-s) + 1;
    else str = "./", n = 2;
    if (n > GUISTRING-100) n = GUISTRING-100;
    pdgui_path = malloc(n+9);

    strncpy(pdgui_path, str, n);
    while (strlen(pdgui_path) > 0 && pdgui_path[strlen(pdgui_path)-1] == '/')
        pdgui_path[strlen(pdgui_path)-1] = 0;
    if (t = strrchr(pdgui_path, '/'))
        *t = 0;
}
#endif

    /* this is called when an off-the-shelf "wish" has to "load" this module
    at runtime.  In Linux, this module is linked in and Pdtcl_Init() is not
    called; instead, the code in t_main.c calls pdgui_setsock() and
    pdgui_startup(). */ 

int Pdtcl_Init(Tcl_Interp *interp)
{
    const char *argv = Tcl_GetVar(interp, "argv", 0);
    int portno, argno = 0;
    if (argv && (portno = atoi(argv)) > 1)
        pdgui_setsock(portno);
#ifdef DEBUGCONNECT
        pd_portno = portno;
    debugfd = fopen("/tmp/bratwurst", "w");
    fprintf(debugfd, "turning stderr back on\n");
    fflush(debugfd);
    dup2(fileno(debugfd), 2);
    fprintf(stderr, "duped to stderr?\n");
    fprintf(stderr, "portno %d\n", pd_portno);
    fprintf(stderr, "argv %s\n", argv);
#endif
    tk_pdinterp = interp;
    pdgui_startup(interp);
    interp->result = "loaded pdtcl_init";

    return (TCL_OK);
}

#if 0
int Pdtcl_SafeInit(Tcl_Interp *interp) {
    fprintf(stderr, "Pdtcl_Safeinit 51\n");
        return (TCL_OK);
}
#endif

