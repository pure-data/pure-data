/* Copyright (c) 2000 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in the Pd distribution.  */

/* the "pdsend" command.  This is a standalone program that forwards messages
from its standard input to Pd via the netsend/netreceive ("FUDI") protocol. */

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "s_net.h"

static void sockerror(char *s);

/* print addrinfo lists for debugging */
/* #define PRINT_ADDRINFO */

#define BUFSIZE 4096

int main(int argc, char **argv)
{
    int sockfd = -1, portno, protocol, status, multicast;
    struct sockaddr_storage server;
    struct addrinfo *ailist = NULL, *ai;
    float timeout = 10;
    char *hostname;
    if (argc < 2 || sscanf(argv[1], "%d", &portno) < 1 || portno <= 0)
        goto usage;
    if (argc >= 3)
        hostname = argv[2];
    else hostname = "localhost";
    if (argc >= 4)
    {
        if (!strcmp(argv[3], "tcp"))
            protocol = SOCK_STREAM;
        else if (!strcmp(argv[3], "udp"))
            protocol = SOCK_DGRAM;
        else goto usage;
    }
    else protocol = SOCK_STREAM;
    if (argc >= 5 && sscanf(argv[4], "%f", &timeout) < 1)
        goto usage;
    if (socket_init())
    {
        sockerror("socket_init()");
        exit(EXIT_FAILURE);
    }
    /* get addrinfo list using hostname & port */
    status = addrinfo_get_list(&ailist, hostname, portno, protocol);
    if (status != 0)
    {
        fprintf(stderr, "bad host or port? %s (%d)\n",
            gai_strerror(status), status);
        exit(EXIT_FAILURE);
    }
    addrinfo_sort_list(&ailist, addrinfo_ipv4_first); /* IPv4 first! */
#ifdef PRINT_ADDRINFO
    addrinfo_print_list(ailist);
#endif
    /* try each addr until we find one that works */
    for (ai = ailist; ai != NULL; ai = ai->ai_next)
    {
        char addrstr[256];
        /* create a socket */
        sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sockfd < 0)
            continue;
        /* for stream (TCP) sockets, specify "nodelay" */
        if (protocol == SOCK_STREAM)
        {
            if (socket_set_boolopt(sockfd, IPPROTO_TCP, TCP_NODELAY, 1) < 0)
                fprintf(stderr, "setsockopt (TCP_NODELAY) failed\n");
        }
        else /* datagram (UDP) broadcasting */
        {
            if (socket_set_boolopt(sockfd, SOL_SOCKET, SO_BROADCAST, 1) < 0)
                fprintf(stderr, "setsockopt (SO_BROADCAST) failed\n");
        }
        multicast = sockaddr_is_multicast(ai->ai_addr);
        if (protocol == SOCK_STREAM)
        {
            status = socket_connect(sockfd, ai->ai_addr, ai->ai_addrlen,
                                    timeout);
            if (status < 0){
                sockerror("connecting stream socket");
                socket_close(sockfd);
                freeaddrinfo(ailist);
                exit(EXIT_FAILURE);
            }
        }
        /* this addr worked */
        memcpy(&server, ai->ai_addr, ai->ai_addrlen);
        /* print address */
        sockaddr_get_addrstr((struct sockaddr *)&server,
                             addrstr, sizeof(addrstr));
        if (multicast)
            fprintf(stderr, "connected to %s (multicast)\n", addrstr);
        else
            fprintf(stderr, "connected to %s\n", addrstr);
        break;
    }
    freeaddrinfo(ailist);
    if (sockfd < 0)
    {
        fprintf(stderr, "couldn't create socket to connect to %s://%s:%d\n",
            (SOCK_STREAM==protocol)?"tcp":"udp", hostname, portno);
        exit(EXIT_FAILURE);
    }

    /* now loop reading stdin and sending it to socket */
    while (1)
    {
        char buf[BUFSIZE], *bp;
        int nsent, nsend;
        if (!fgets(buf, BUFSIZE, stdin))
            break;
        nsend = strlen(buf);
        for (bp = buf, nsent = 0; nsent < nsend;)
        {
            int res = 0;
            if (protocol == SOCK_DGRAM)
            {
                socklen_t addrlen = (server.ss_family == AF_INET6 ?
                    sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));
                res = (int)sendto(sockfd, bp, nsend-nsent, 0,
                    (struct sockaddr *)&server, addrlen);
            }
            else
                res = (int)send(sockfd, bp, nsend-nsent, 0);

            if (res < 0)
            {
                sockerror("send");
                goto done;
            }
            nsent += res;
            bp += res;
        }
    }
done:
    if (ferror(stdin))
        perror("stdin");
    socket_close(sockfd);
    exit(EXIT_SUCCESS);
usage:
    fprintf(stderr, "usage: pdsend <portnumber> [host] [udp|tcp] [timeout(s)]\n");
    fprintf(stderr, "(default is localhost and tcp with 10s timeout)\n");
    exit(EXIT_FAILURE);
}

void sockerror(char *s)
{
    char buf[256];
    int err = socket_errno();
    socket_strerror(err, buf, sizeof(buf));
    fprintf(stderr, "%s: %s (%d)\n", s, buf, err);
}
