/* net.c - open a local TCP socket to Pd */


#include <netdb.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#endif

#include "pdgtk.h"
static int socket_fd = -1;

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

