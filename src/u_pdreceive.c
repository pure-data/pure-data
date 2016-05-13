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
#ifdef _WIN32
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#define SOCKET_ERROR -1
#endif

typedef struct _fdpoll
{
    int fdp_fd;
    char *fdp_outbuf;/*output message buffer*/
    int fdp_outlen;     /*length of output message*/
    int fdp_discard;/*buffer overflow: output message is incomplete, discard it*/
    int fdp_gotsemi;/*last char from input was a semicolon*/
} t_fdpoll;

static int nfdpoll;
static t_fdpoll *fdpoll;
static int maxfd;
static int sockfd;
static int protocol;

static void sockerror(char *s);
static void x_closesocket(int fd);
static void dopoll(void);
#define BUFSIZE 4096

int main(int argc, char **argv)
{
    int portno;
    struct sockaddr_in server = {0};
    int nretry = 10;
#ifdef _WIN32
    short version = MAKEWORD(2, 0);
    WSADATA nobby;
#endif
    if (argc < 2 || sscanf(argv[1], "%d", &portno) < 1 || portno <= 0)
        goto usage;
    if (argc >= 3)
    {
        if (!strcmp(argv[2], "tcp"))
            protocol = SOCK_STREAM;
        else if (!strcmp(argv[2], "udp"))
            protocol = SOCK_DGRAM;
        else goto usage;
    }
    else protocol = SOCK_STREAM;
#ifdef _WIN32
    if (WSAStartup(version, &nobby)) sockerror("WSAstartup");
#endif
    sockfd = socket(AF_INET, protocol, 0);
    if (sockfd < 0)
    {
        sockerror("socket()");
        exit(1);
    }
    maxfd = sockfd + 1;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;

        /* assign client port number */
    server.sin_port = htons((unsigned short)portno);

        /* name the socket */
    if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        sockerror("bind");
        x_closesocket(sockfd);
        return (0);
    }
    if (protocol == SOCK_STREAM)
    {
        if (listen(sockfd, 5) < 0)
        {
            sockerror("listen");
            x_closesocket(sockfd);
            exit(1);
        }
    }
        /* now loop forever selecting on sockets */
    while (1)
        dopoll();

usage:
    fprintf(stderr, "usage: pdreceive <portnumber> [udp|tcp]\n");
    fprintf(stderr, "(default is tcp)\n");
    exit(1);
}

static void addport(int fd)
{
    int nfd = nfdpoll;
    t_fdpoll *fp;
    fdpoll = (t_fdpoll *)realloc(fdpoll,
        (nfdpoll+1) * sizeof(t_fdpoll));
    fp = fdpoll + nfdpoll;
    fp->fdp_fd = fd;
    nfdpoll++;
    if (fd >= maxfd) maxfd = fd + 1;
    fp->fdp_outlen = fp->fdp_discard = fp->fdp_gotsemi = 0;
    if (!(fp->fdp_outbuf = (char*) malloc(BUFSIZE)))
    {
        fprintf(stderr, "out of memory");
        exit(1);
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
            x_closesocket(fp->fdp_fd);
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
        exit(1);
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
        x_closesocket(sockfd);
        exit(1);
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
        exit(1);
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


static void sockerror(char *s)
{
#ifdef _WIN32
    int err = WSAGetLastError();
    if (err == 10054) return;
    else if (err == 10044)
    {
        fprintf(stderr,
            "Warning: you might not have TCP/IP \"networking\" turned on\n");
    }
#else
    int err = errno;
#endif
    fprintf(stderr, "%s: %s (%d)\n", s, strerror(err), err);
}

static void x_closesocket(int fd)
{
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
}
