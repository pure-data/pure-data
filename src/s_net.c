/* Copyright (c) 2019 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "s_net.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#endif

// Windows XP winsock doesn't provide inet_ntop
#ifdef _WIN32
const char* INET_NTOP(int af, const void *src, char *dst, socklen_t size) {
    struct sockaddr_storage addr;
    socklen_t addrlen;
    memset(&addr, 0, sizeof(struct sockaddr_storage));
    addr.ss_family = af;
    if (af == AF_INET6)
    {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&addr;
        memcpy(&(sa6->sin6_addr.s6_addr), src, sizeof(sa6->sin6_addr.s6_addr));
        addrlen = sizeof(struct sockaddr_in6);
    }
    else if (af == AF_INET)
    {
        struct sockaddr_in *sa4 = (struct sockaddr_in *)&addr;
        memcpy(&(sa4->sin_addr.s_addr), src, sizeof(sa4->sin_addr.s_addr));
        addrlen = sizeof(struct sockaddr_in);
    }
    else
        return NULL;
    if (WSAAddressToStringA((struct sockaddr *)&addr, addrlen, 0, dst,
        (LPDWORD)&size) != 0)
        return NULL;
    return dst;
}
#else /* _WIN32 */
#define INET_NTOP inet_ntop
#endif

int addrinfo_get_list(struct addrinfo **ailist, const char *hostname,
                             int port, int protocol) {
    struct addrinfo hints;
    char portstr[10]; // largest port is 65535
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
    hints.ai_socktype = protocol;
    hints.ai_protocol = (protocol == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP);
    hints.ai_flags = AI_ALL |        // both IPv4 and IPv6 addrs
                     AI_V4MAPPED |   // fallback to IPv4-mapped IPv6 addrs
                     AI_PASSIVE;     // listen to any addr if hostname is NULL
    portstr[0] = '\0';
    sprintf(portstr, "%d", port);
    return getaddrinfo(hostname, portstr, &hints, ailist);
}

static void ailist_append(struct addrinfo **ailist, struct addrinfo *elem)
{
    if (*ailist)
    {
        struct addrinfo *ap = *ailist;
        while (ap->ai_next)
            ap = ap->ai_next;
        ap->ai_next = elem;
    }
    else
        (*ailist) = elem;
}

static struct addrinfo * ailist_join(struct addrinfo *ailist1, struct addrinfo *ailist2)
{
    if (!ailist1)
        return ailist2;
    else
    {
        struct addrinfo *aip = ailist1;
        while (aip->ai_next)
            aip = aip->ai_next;
        aip->ai_next = ailist2;
        return ailist1;
    }
}

void addrinfo_sort_list(struct addrinfo **ailist, addrinfo_order order)
{
    struct addrinfo *result = NULL, *ipv4 = NULL, *ipv6 = NULL, *other = NULL, *ai;
    for (ai = (*ailist); ai != NULL;)
    {
        struct addrinfo *temp = ai;
        ai = ai->ai_next;
        temp->ai_next = NULL;
        switch (temp->ai_family)
        {
        case AF_INET:
            ailist_append(&ipv4, temp);
            break;
        case AF_INET6:
            ailist_append(&ipv6, temp);
            break;
        default:
            ailist_append(&other, temp);
            break;
        }

    }
    if (order == ADDR_ORDER_IPv4)
        result = ailist_join(ipv4, ipv6);
    else if (order == ADDR_ORDER_IPv6)
        result = ailist_join(ipv6, ipv4);
    else
    {
        fprintf(stderr, "addrinfo_sort_list: unknown ordering");
        return;
    }
    result = ailist_join(result, other);
    *ailist = result;
}

void addrinfo_print_list(const struct addrinfo *ailist)
{
    const struct addrinfo *ai;
    char addrstr[INET6_ADDRSTRLEN];
    for (ai = ailist; ai != NULL; ai = ai->ai_next)
    {
        void *addr;
        char *ipver;
        if (ai->ai_family == AF_INET6)
        {
            struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)ai->ai_addr;
            addr = &(sa6->sin6_addr);
            ipver = "IPv6";
        }
        else if (ai->ai_family == AF_INET)
        {
            struct sockaddr_in *sa4 = (struct sockaddr_in *)ai->ai_addr;
            addr = &(sa4->sin_addr);
            ipver = "IPv4";
        }
        else continue;
        INET_NTOP(ai->ai_family, addr, addrstr, INET6_ADDRSTRLEN);
        printf("%s %s\n", ipver, addrstr);
    }
}

const char* sockaddr_get_addrstr(const struct sockaddr *sa, char *addrstr,
    int addrstrlen)
{
	const void *addr;
	addrstr[0] = '\0';
	if (sa->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;
        addr = (const void *)&sa6->sin6_addr.s6_addr;
    }
    else if (sa->sa_family == AF_INET)
    {
        struct sockaddr_in *sa4 = (struct sockaddr_in *)sa;
        addr = (const void *)&sa4->sin_addr.s_addr;
    }
    else return NULL;
	return INET_NTOP(sa->sa_family, addr, addrstr, addrstrlen);
}

unsigned int sockaddr_get_port(const struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;
        return ntohs(sa6->sin6_port);
    }
    else if (sa->sa_family == AF_INET)
    {
        struct sockaddr_in *sa4 = (struct sockaddr_in *)sa;
        return ntohs(sa4->sin_port);
    }
    else return 0;
}

int sockaddr_is_multicast(const struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;
        return (sa6->sin6_addr.s6_addr[0] == 0xFF);
    }
    else if (sa->sa_family == AF_INET)
    {
        struct sockaddr_in *sa4 = (struct sockaddr_in *)sa;
        return ((ntohl(sa4->sin_addr.s_addr) & 0xF0000000) == 0xE0000000);
    }
    return 0;
}

int socket_init(void)
{
#ifdef _WIN32
    static int initialized = 0;
    if (!initialized)
    {
        short version = MAKEWORD(2, 0);
        WSADATA nobby;
        if (WSAStartup(version, &nobby))
            return -1;
        initialized = 1;
    }
#endif
    return 0;
}

// kudos to https://stackoverflow.com/a/46062474/6063908
int socket_connect(int socket, const struct sockaddr *addr,
                   socklen_t addrlen, float timeout)
{
    // set nonblocking and connect
    socket_set_nonblocking(socket, 1);

    if (connect(socket, addr, addrlen) < 0)
    {
        int status;
        struct timeval timeoutval;
        fd_set writefds, errfds;
    #ifdef _WIN32
        if (socket_errno() != WSAEWOULDBLOCK)
    #else
        if (socket_errno() != EINPROGRESS)
    #endif
            return -1; // break on "real" error

        // block with select using timeout
        if (timeout < 0) timeout = 0;
        timeoutval.tv_sec = (int)timeout;
        timeoutval.tv_usec = (timeout - timeoutval.tv_sec) * 1000000;
        FD_ZERO(&writefds);
        FD_SET(socket, &writefds); // socket is connected when writable
        FD_ZERO(&errfds);
        FD_SET(socket, &errfds); // catch exceptions

        status = select(socket+1, NULL, &writefds, &errfds, &timeoutval);
        if (status < 0) // select failed
        {
            fprintf(stderr, "socket_connect: select failed");
            return -1;
        }
        else if (status == 0) // connection timed out
        {
        #ifdef _WIN32
            WSASetLastError(WSAETIMEDOUT);
        #else
            errno = ETIMEDOUT;
        #endif
            return -1;
        }

        if (FD_ISSET(socket, &errfds)) // connection failed
        {
            int err; socklen_t len = sizeof(err);
            getsockopt(socket, SOL_SOCKET, SO_ERROR, (void *)&err, &len);
        #ifdef _WIN32
            WSASetLastError(err);
        #else
            errno = err;
        #endif
            return -1;
        }
    }
    // done, set blocking again
    socket_set_nonblocking(socket, 0);
    return 0;
}

void socket_close(int socket)
{
#ifdef _WIN32
    closesocket(socket);
#else
    if (socket < 0) return;
    close(socket);
#endif
}

int socket_set_boolopt(int socket, int level, int option_name, int bool_value)
{
    return setsockopt(socket, level, option_name,
        (void *)&bool_value, sizeof(int));
}

int socket_set_nonblocking(int socket, int nonblocking)
{
#ifdef _WIN32
    u_long modearg = nonblocking;
    if (ioctlsocket(socket, FIONBIO, &modearg) != NO_ERROR)
        return -1;
#else
    int sockflags = fcntl(socket, F_GETFL, 0);
    if (nonblocking)
        sockflags |= O_NONBLOCK;
    else
        sockflags &= ~O_NONBLOCK;
    if (fcntl(socket, F_SETFL, sockflags) < 0)
        return -1;
#endif
    return 0;
}

int socket_bytes_available(int socket)
{
#ifdef _WIN32
    u_long n = 0;
    if (ioctlsocket(socket, FIONREAD, &n) != NO_ERROR)
        return -1;
    return n;
#else
    int n = 0;
    if (ioctl(socket, FIONREAD, &n) < 0)
        return -1;
    return n;
#endif
}

int socket_join_multicast_group(int socket, struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&sa;
        struct ipv6_mreq mreq6;
        memcpy(&mreq6.ipv6mr_multiaddr, &sa6->sin6_addr,
            sizeof(struct in6_addr));
        mreq6.ipv6mr_interface = 0;
        return setsockopt(socket, IPPROTO_IPV6, IPV6_JOIN_GROUP,
            (char *)&mreq6, sizeof(mreq6));
    }
    else if (sa->sa_family == AF_INET)
    {
        struct sockaddr_in *sa4 = (struct sockaddr_in *)&sa;
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = sa4->sin_addr.s_addr;
        mreq.imr_interface.s_addr = INADDR_ANY;
        return setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
            (char *)&mreq, sizeof(mreq));
    }
    return -1;
}

int socket_errno(void)
{
#ifdef _WIN32
    int err = WSAGetLastError();
    if (err == 10044)      // WSAESOCKTNOSUPPORT
    {
        fprintf(stderr,
            "Warning: you might not have TCP/IP \"networking\" turned on\n");
    }
    return err;
#else
    return errno;
#endif
}

int socket_errno_udp(void)
{
#ifdef _WIN32
    int err = socket_errno();
    // ignore WSAECONNRESET to prevent UDP sockets
    // from closing in case of a missing receiver
    if (err == 10054)
        return 0;
    else
        return err;
#else
    return socket_errno();
#endif
}

void socket_strerror(int err, char *buf, int size)
{
    if (size <= 0)
        return;
#ifdef _WIN32
    buf[0] = '\0';
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   0, err, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), buf,
                   size, NULL);
#else
    snprintf(buf, size, "%s", strerror(err));
#endif
}
