/* net.c - open a local TCP socket to Pd */


#include <netdb.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#endif
#include "../../pd/src/s_net.h"

#include "pdgtk.h"
static int socket_fd = -1;
#define MAXPDARG 100

static void sys_sockerror(const char *s)
{
    char buf[BIGSTRING];
    int err = socket_errno();
    socket_strerror(err, buf, sizeof(buf));
    fprintf(stderr, "%s: %s (%d)", s, buf, err);
}

int socket_open(int portno)
{
    int sockfd, poodle = 1;
    struct sockaddr_in server;
    struct hostent *hp;

    server.sin_family = AF_INET;
    hp = gethostbyname("localhost");

    if (hp == 0)
    {
        fprintf(stderr, "localhost: unknown host name?\n");
        return (-1);
    }
    memcpy((char *)&server.sin_addr, (char *)hp->h_addr, hp->h_length);

        /* assign client port number */
    server.sin_port = htons((unsigned short)portno);

            /* create a socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return (sockfd);
#if 0
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &poodle, sizeof(poodle))
        < 0)
    {
        fprintf(stderr, "setsockopt (TCP_NODELAY) failed");
        close(sockfd);
        return (-1);
    }
#endif
            /* try to connect */
    if (connect(sockfd, (struct sockaddr *)(&server), sizeof(server)) < 0)
    {
        perror("connect");
        close(sockfd);
        return (-1);
     }
     socket_fd = sockfd;
     return (sockfd);
}



    /* send message to socket.  The string |s| is zero-terminated but the zero
    itself is not sent over the socket. */
int socket_send(char *s)
{
    return (send(socket_fd, s, strlen(s), 0) == strlen(s));
}

    /* call this if the GUI will start Pd.  This is cribbed from s_inter.c
    and probably has unneeded cruft. */

int socket_startpd(int argc, char **argv)
{
    char pdname[BIGSTRING], portname[BIGSTRING];
    char apibuf[256], apibuf2[256];
    struct addrinfo *ailist = NULL, *ai;
    int listenersocket = -1;
    int portno = -1;
    char *pdargs[MAXPDARG+10];
    struct sockaddr_storage addr;
    int status, i;
#ifndef _WIN32
    int stdinpipe[2];
    pid_t childpid;
    char scriptbuf[BIGSTRING+30], wishbuf[BIGSTRING+30];
#else
    const char *pdpath;
#endif

    /* get addrinfo list using hostname (get random port from OS) */
    status = addrinfo_get_list(&ailist, "localhost", 0, SOCK_STREAM);
    if (status != 0)
    {
        fprintf(stderr,
            "localhost not found (inet protocol not installed?)\n%s (%d)",
            gai_strerror(status), status);
        return (-1);
    }

    /* we prefer the IPv4 addresses because the GUI might not do IPv6. */
    addrinfo_sort_list(&ailist, addrinfo_ipv4_first);
    /* try each addr until we find one that works */
    for (ai = ailist; ai != NULL; ai = ai->ai_next)
    {
        listenersocket = socket(ai->ai_family, ai->ai_socktype,
            ai->ai_protocol);
        fprintf(stderr, "socket %d\n", listenersocket);
        if (listenersocket < 0)
            continue;
#ifndef _WIN32
        if(fcntl(listenersocket, F_SETFD, FD_CLOEXEC) < 0)
            perror("close-on-exec");
#endif
        /* ask OS to allow another process to reopen port after we close it */
        if (socket_set_boolopt(listenersocket, SOL_SOCKET, SO_REUSEADDR, 1) < 0)
            fprintf(stderr, "setsockopt (SO_REUSEADDR) failed\n");
        /* stream (TCP) sockets are set NODELAY */
        if (socket_set_boolopt(listenersocket, IPPROTO_TCP, TCP_NODELAY, 1) < 0)
            fprintf(stderr, "setsockopt (TCP_NODELAY) failed");
        /* name the socket */
        if (bind(listenersocket, ai->ai_addr, ai->ai_addrlen) < 0)
        {
            socket_close(listenersocket);
            listenersocket = -1;
            continue;
        }
        /* this addr worked */
        memcpy(&addr, ai->ai_addr, ai->ai_addrlen);
        break;
    }
    freeaddrinfo(ailist);

    /* confirm that socket/bind worked */
    if (listenersocket < 0)
    {
        sys_sockerror("bind");
        return (-1);
    }
    /* get the actual port number */
    portno = socket_get_port(listenersocket);
    if (tcl_debug)
        fprintf(stderr, "socket_get_port %d\n", portno);

#ifndef _WIN32
    if (getenv("PD"))
        pdargs[0] = getenv("PD");
    else if (getenv("HOME"))
    {
        snprintf(pdname, BIGSTRING, "%s/pd/bin/pd", getenv("HOME"));
        pdname[BIGSTRING-1] = 0;
        pdargs[0] = pdname;
    }
    else pdargs[0] = "../bin/pd";
    pdargs[1] = "-guiport";
    snprintf(portname, BIGSTRING, "%d", portno);
    portname[BIGSTRING-1] = 0;
    pdargs[2] = portname;
    for (i = 1; i < (argc > MAXPDARG ? MAXPDARG : argc); i++)
        pdargs[i+2] = argv[i];
    pdargs[i+3] = "";

    childpid = fork();
    if (childpid < 0)
    {
        if (errno)
            perror("socket_startgui");
        else fprintf(stderr, "socket_startgui() failed\n");
        socket_close(listenersocket);
        return (-1);
    }
    else if (!childpid)                     /* we're the child */
    {
        socket_close(listenersocket);     /* child doesn't listen */

        execv(pdargs[0], pdargs);
        perror("pd: exec");
        fprintf(stderr, "can't start pd executable\n");
        _exit(1);
   }
#else /* NOT _WIN32 */

#error windows code not implemented
    pd_snprintf(wishbuf, sizeof(wishbuf), "%s/" PDBINDIR WISH, libdir);
    sys_bashfilename(wishbuf, wishbuf);

    pd_snprintf(scriptbuf, sizeof(scriptbuf), "%s/" PDGUIDIR "/pd-gui.tcl",
        libdir);
    sys_bashfilename(scriptbuf, scriptbuf);

    pd_snprintf(cmdbuf, sizeof(cmdbuf), "%s \"%s\" %d", /* quote script path! */
        WISH, scriptbuf, portno);

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
        /* CHR: DETACHED_PROCESS makes sure that the child process cannot
        possibly interfere with us. */
    if (!CreateProcessA(wishbuf, cmdbuf, NULL, NULL, FALSE,
        DETACHED_PROCESS, NULL, NULL, &si, &pi))
    {
        char errbuf[BIGSTRING];
        socket_strerror(GetLastError(), errbuf, sizeof(errbuf));
        fprintf(stderr, "could not start %s: %s\n", wishbuf, errbuf);
        return (-1);
    }
#endif /* NOT _WIN32 */
    if (tcl_debug)
        fprintf(stderr, "Waiting for connection request... \n");
    if (listen(listenersocket, 5) < 0)
    {
        sys_sockerror("listen");
        socket_close(listenersocket);
        return (-1);
    }

    socket_fd = accept(listenersocket, 0, 0);

    socket_close(listenersocket);

    if (socket_fd < 0)
    {
        sys_sockerror("accept");
        return (-1);
    }
    if (tcl_debug)
        fprintf(stderr, "... connected\n");

    return (socket_fd);
}

extern int pdsockfd;
static char commands[BIGSTRING+1];
static int commandfill;

static char *whereterminater(char *input)
{
    char *s;
    int i, n = strlen(input), bracketcount = 0, escaped = 0;
    for (i = 0; i < n; i++)
    {
        if (escaped)
            escaped = 0;
        else if (input[i] == '\\')
            escaped = 1;
        else if (input[i] == '{')
            bracketcount++;
        else if (input[i] == '}')
            bracketcount--;
        else if (bracketcount == 0 && input[i] == ';')
            return (input + i);
    }
    return (0);
}

void socket_callback( void)
{
    int nbytes, i, nused = 0;
    char bashedchar1, bashedchar2, *wheresemi;
    // fprintf(stderr, "read from socket...\n");
    if ((nbytes = recv(pdsockfd, commands + commandfill,
        BIGSTRING - commandfill, MSG_DONTWAIT)) < 1)
    {
        if (nbytes < 0 && errno == EAGAIN)
            return;
        else if (nbytes < 0)
        {
            perror("socket");
            exit (1);
        }
        else exit (0);
    }
    commands[commandfill + nbytes] = 0;
    // fprintf(stderr, "read done: \'%s\'\n", commands);

        /* find first semicolon - LATER check for escaping */
    while ((wheresemi = whereterminater(commands+nused)))
    {
        bashedchar1 = wheresemi[1];
        bashedchar2 = wheresemi[2];
        wheresemi[1] = '\n';
        wheresemi[2] = 0;
        tcl_runcommand(commands+nused);
        nused = wheresemi - commands + 1;
        wheresemi[1] = bashedchar1;
        wheresemi[2] = bashedchar2;
    }
    while (nused < commandfill + nbytes &&
        ((commands[nused] == '\n') || (commands[nused] == 0)))
            nused++;
    if (nused < commandfill + nbytes)
    {
        memcpy(commands, commands + nused, commandfill + nbytes - nused);
        commandfill = commandfill + nbytes - nused;
        /* fprintf(stderr, "note: %d extra chars (%c) saved for next command\n",
            commandfill, commands[0]); */
    }
    else commandfill = 0;

    return;
}
