/* Copyright (c) 2000 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in the Pd distribution.  */

/* the "pdreceive" command. This is a standalone program that receives messages
from Pd via the netsend/netreceive ("FUDI") protocol, and copies them to
standard output. */

/* May 2008 : fixed a buffer overflow problem; pdreceive sometimes
    repeated infinitely its buffer during high speed transfer.
    Moonix::Antoine Rousseau
*/

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include <s_net.h>

typedef struct _fdpoll
{
    int fdp_fd;
    char *fdp_outbuf; /*output message buffer*/
    int fdp_outlen;   /*length of output message*/
    int fdp_discard;  /*buffer overflow: output message is incomplete, discard it*/
    int fdp_gotsemi;  /*last char from input was a semicolon*/
} t_fdpoll;

static int nfdpoll;
static t_fdpoll *fdpoll;
static int maxfd;
static int sockfd;
static int protocol;

static void sockerror(char *s);
static void dopoll(void);
static void sockerror(char *s);

/* print addrinfo lists for debugging */
#define PRINT_ADDRINFO

#define BUFSIZE 4096

int main(int argc, char **argv)
{
    int status, portno;
    char *hostname = NULL;
    struct addrinfo *ailist = NULL, *ai;
    struct sockaddr_storage server;
    if (argc < 2 || sscanf(argv[1], "%d", &portno) < 1 || portno <= 0)
        goto usage;
    if (argc >= 3)
    {
        int index = (argc > 3 ? 3 : 2);
        if (!strcmp(argv[index], "tcp"))
            protocol = SOCK_STREAM;
        else if (!strcmp(argv[index], "udp"))
            protocol = SOCK_DGRAM;
        else goto usage;
        if (index == 3)
            hostname = argv[2];
    }
    else protocol = SOCK_STREAM;
    if (hostname && protocol == SOCK_STREAM)
    {
        fprintf(stderr, "ignoring host: %s\n", hostname);
        hostname = NULL;
    }
    if (socket_init())
    {
        sockerror("socket_init()");
        exit(EXIT_FAILURE);
    }
    status = addrinfo_get_list(&ailist, hostname, portno, protocol);
    if (status != 0)
    {
        fprintf(stderr, "bad host or port? %s (%d)\n",
            gai_strerror(status), status);
        exit(EXIT_FAILURE);
    }
    addrinfo_sort_list(&ailist, addrinfo_ipv6_first); /* IPv6 addresses first! */
#ifdef PRINT_ADDRINFO
    addrinfo_print_list(ailist);
#endif
    /* try each addr until we find one that works */
    for (ai = ailist; ai != NULL; ai = ai->ai_next)
    {
        sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sockfd < 0)
            continue;
    #if 1
        /* ask OS to allow another process to repoen this port after we close it */
        if (socket_set_boolopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 1) < 0)
            fprintf(stderr, "setsockopt (SO_REUSEADDR) failed\n");
    #endif
        if (protocol == SOCK_STREAM)
        {
            /* stream (TCP) sockets are set NODELAY */
            if (socket_set_boolopt(sockfd, IPPROTO_TCP, TCP_NODELAY, 1) < 0)
                fprintf(stderr, "netreceive: setsockopt (TCP_NODELAY) failed");
        }
        else if (protocol == SOCK_DGRAM && ai->ai_family == AF_INET)
        {
            /* enable IPv4 UDP broadcasting */
            if (socket_set_boolopt(sockfd, SOL_SOCKET, SO_BROADCAST, 1) < 0)
                fprintf(stderr, "netreceive: setsockopt (SO_BROADCAST) failed");
        }
        /* if this is an IPv6 address, also listen to IPv4 adapters
           (if not supported, fall back to IPv4) */
        if (ai->ai_family == AF_INET6 &&
                socket_set_boolopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, 0) < 0)
        {
            /* post("netreceive: setsockopt (IPV6_V6ONLY) failed"); */
            socket_close(sockfd);
            sockfd = -1;
            continue;
        }
        /* name the socket */
        if (bind(sockfd, ai->ai_addr, ai->ai_addrlen) < 0)
        {
            socket_close(sockfd);
            sockfd = -1;
            continue;
        }

        /* this addr worked */
        memcpy(&server, ai->ai_addr, ai->ai_addrlen);
        break;
    }
    freeaddrinfo(ailist);

    /* confirm that socket/bind worked */
    if (sockfd < 0)
    {
        int err = socket_errno();
        char buf[256];
        socket_strerror(err, buf, sizeof(buf));
        fprintf(stderr, "listen failed: %s (%d)\n", buf, err);
        exit(EXIT_FAILURE);
    }

    maxfd = sockfd + 1;

    if (protocol == SOCK_DGRAM) /* datagram protocol */
    {
        /* join multicast group */
        if (sockaddr_is_multicast((struct sockaddr *)&server))
        {
            if (socket_join_multicast_group(sockfd,
                (struct sockaddr *)&server) < 0)
            {
                int err = socket_errno();
                char buf[256];
                socket_strerror(err, buf, sizeof(buf));
                fprintf(stderr,
                    "netreceive: joining multicast group %s failed: %s (%d)\n",
                    hostname, buf, err);
            }
            else
                fprintf(stderr, "joined multicast group %s\n", hostname);
        }
    }
    else /* streaming protocol */
    {
        if (listen(sockfd, 5) < 0)
        {
            sockerror("listen");
            socket_close(sockfd);
            exit(EXIT_FAILURE);
        }
    }

    /* now loop forever selecting on sockets */
    while (1)
        dopoll();

usage:
    fprintf(stderr, "usage: pdreceive <portnumber> [udphost] [udp|tcp]\n");
    fprintf(stderr, "(default is tcp)\n");
    exit(EXIT_FAILURE);
}

static void addport(int fd)
{
    int nfd = nfdpoll;
    t_fdpoll *fp;
    t_fdpoll *fdtmp = (t_fdpoll *)realloc(fdpoll,
        (nfdpoll+1) * sizeof(t_fdpoll));
    if (!fdtmp)
    {
        free(fdpoll);
        fprintf(stderr, "out of memory!");
        exit(EXIT_FAILURE);
    }
    fdpoll = fdtmp;
    fp = fdpoll + nfdpoll;
    fp->fdp_fd = fd;
    nfdpoll++;
    if (fd >= maxfd) maxfd = fd + 1;
    fp->fdp_outlen = fp->fdp_discard = fp->fdp_gotsemi = 0;
    if (!(fp->fdp_outbuf = (char*) malloc(BUFSIZE)))
    {
        fprintf(stderr, "out of memory");
        exit(EXIT_FAILURE);
    }
    printf("number_connected %d;\n", nfdpoll);
}

static void rmport(t_fdpoll *x)
{
    int nfd = nfdpoll;
    int i, size = nfdpoll * sizeof(t_fdpoll);
    t_fdpoll *fp;
    for (i = nfdpoll, fp = fdpoll; i--; fp++)
    {
        if (fp == x)
        {
            socket_close(fp->fdp_fd);
            free(fp->fdp_outbuf);
            while (i--)
            {
                fp[0] = fp[1];
                fp++;
            }
            fdpoll = (t_fdpoll *)realloc(fdpoll,
                (nfdpoll-1) * sizeof(t_fdpoll));
            nfdpoll--;
            printf("number_connected %d;\n", nfdpoll);
            return;
        }
    }
    fprintf(stderr, "warning: item removed from poll list but not found");
}

static void doconnect(void)
{
    int fd = accept(sockfd, 0, 0);
    if (fd < 0)
        perror("accept");
    else addport(fd);
}

static void makeoutput(char *buf, int len)
{
#ifdef _WIN32
    int j;
    for (j = 0; j < len; j++)
        putchar(buf[j]);
#else
    if (write(1, buf, len) < len)
    {
        perror("write");
        exit(EXIT_FAILURE);
    }
#endif
}

static void udpread(void)
{
    char buf[BUFSIZE];
    int ret = recv(sockfd, buf, BUFSIZE, 0);
    if (ret < 0)
    {
        sockerror("recv (udp)");
        socket_close(sockfd);
        exit(EXIT_FAILURE);
    }
    else if (ret > 0)
        makeoutput(buf, ret);
}

static int tcpmakeoutput(t_fdpoll *x, char *inbuf, int len)
{
    int i;
    int outlen = x->fdp_outlen;
    char *outbuf = x->fdp_outbuf;

    for (i = 0 ; i < len ; i++)
    {
        char c = inbuf[i];

        if((c != '\n') || (!x->fdp_gotsemi))
            outbuf[outlen++] = c;
        x->fdp_gotsemi = 0;
        if (outlen >= (BUFSIZE-1)) /*output buffer overflow; reserve 1 for '\n' */
        {
            fprintf(stderr, "pdreceive: message too long; discarding\n");
            outlen = 0;
            x->fdp_discard = 1;
        }
            /* search for a semicolon.   */
        if (c == ';')
        {
            outbuf[outlen++] = '\n';
            if (!x->fdp_discard)
               makeoutput(outbuf, outlen);

            outlen = 0;
            x->fdp_discard = 0;
            x->fdp_gotsemi = 1;
        } /* if (c == ';') */
    } /* for */

    x->fdp_outlen = outlen;
    return (0);
}

static void tcpread(t_fdpoll *x)
{
    int  ret;
    char inbuf[BUFSIZE];

    ret = recv(x->fdp_fd, inbuf, BUFSIZE, 0);
    if (ret < 0)
    {
        sockerror("recv (tcp)");
        rmport(x);
    }
    else if (ret == 0)
        rmport(x);
    else tcpmakeoutput(x, inbuf, ret);
}

static void dopoll(void)
{
    int i;
    t_fdpoll *fp;
    fd_set readset, writeset, exceptset;
    FD_ZERO(&writeset);
    FD_ZERO(&readset);
    FD_ZERO(&exceptset);

    FD_SET(sockfd, &readset);
    if (protocol == SOCK_STREAM)
    {
        for (fp = fdpoll, i = nfdpoll; i--; fp++)
            FD_SET(fp->fdp_fd, &readset);
    }
    if (select(maxfd+1, &readset, &writeset, &exceptset, 0) < 0)
    {
        perror("select");
        exit(EXIT_FAILURE);
    }
    if (protocol == SOCK_STREAM)
    {
        for (i = 0; i < nfdpoll; i++)
            if (FD_ISSET(fdpoll[i].fdp_fd, &readset))
                tcpread(&fdpoll[i]);
        if (FD_ISSET(sockfd, &readset))
            doconnect();
    }
    else
    {
        if (FD_ISSET(sockfd, &readset))
            udpread();
    }
}


void sockerror(char *s)
{
    char buf[256];
    int err = socket_errno();
    socket_strerror(err, buf, sizeof(buf));
    fprintf(stderr, "%s: %s (%d)\n", s, buf, err);
}
