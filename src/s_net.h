/* Copyright (c) 2019 Dan Wilcox.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* networking headers and helper functions */

#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#endif

/* ----- socket address ----- */

    /** getaddrinfo() convenience wrapper which generates a list of IPv4 & IPv6
        addresses from a given address/hostname string, port, and protcol
        (SOCK_STREAM or SOCK_DGRAM), set hostname to NULL for "any" address

        returns 0 on success or < 0 on error

        the ailist must be freed after usage using freeaddrinfo(),
        status errno can be printed using gai_strerr()

        basic usage example:

        int sockfd, status;
        struct addrinfo *ailist = NULL, *ai;
        struct sockaddr_storage ss = {0}; // IPv4 or IPv6 addr

        // generate addrinfo list
        status = addrinfo_get_list(&ailist, "127.0.0.1", 5000, SOCK_DGRAM);
        if (status != 0)
        {
            printf("bad host or port? %s (%d)", gai_strerror(status), status);
            return;
        }

        // try each addr until we find one that works
        for (ai = ailist; ai != NULL; ai = ai->ai_next)
        {
            sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
            if (sockfd < 0) continue; // go to the next addr

            // perform socket setsockopt(), bind(), connect(), etc as required,
            // cleanup and continue to next addr on failure

            // save addr that worked and exit loop
            memcpy(&ss, ai->ai_addr, ai->ai_addrlen);
            break;
        }
        freeaddrinfo(ailist); // cleanup

        // confirm that socket setup succeeded
        if (sockfd == -1)
        {
            int err = socket_errno();
            printf("socket setup failed: %s (%d)", strerror(errno), errno);
            return;
        }
    */
int addrinfo_get_list(struct addrinfo **ailist, const char *hostname,
                      int port, int protocol);

    /** sort an address list with a compare function */
void addrinfo_sort_list(struct addrinfo **ailist,
    int (*compare)(const struct addrinfo*, const struct addrinfo*));

    /** compare function which puts IPv4 addresses first */
int addrinfo_ipv4_first(const struct addrinfo* ai1, const struct addrinfo* ai2);

    /** compare function which puts IPv6 addresses first */
int addrinfo_ipv6_first(const struct addrinfo* ai1, const struct addrinfo* ai2);

    /** print addrinfo linked list sockaddrs: IP version, hostname, port */
void addrinfo_print_list(const struct addrinfo *ailist);

    /** read address/hostname string from a sockaddr,
        fills addrstr and returns pointer on success or NULL on failure */
const char* sockaddr_get_addrstr(const struct sockaddr *sa,
                                 char *addrstr, int addrstrlen);

    /** returns sockaddr port or 0 on failure */
unsigned int sockaddr_get_port(const struct sockaddr *sa);

    /** sets sockaddr port */
void sockaddr_set_port(const struct sockaddr *sa, unsigned int port);

    /** returns 1 if address is a IPv4 or IPv6 multicast address, otherwise 0 */
int sockaddr_is_multicast(const struct sockaddr *sa);

/* ----- socket ----- */

    /** cross-platform initialization routine, returns -1 on failure */
int socket_init(void);

    /** connect a socket to an address with a settable timeout in seconds
        returns -1 on error, use socket_errno() to get the actual error code */
int socket_connect(int socket, const struct sockaddr *addr,
                   socklen_t addrlen, float timeout);

    /** cross-platform socket close() */
void socket_close(int socket);

    /** returns port or 0 on failure */
unsigned int socket_get_port(int socket);

    /** get number of immediately readable bytes for the socket
        returns -1 on error or bytes available on success */
int socket_bytes_available(int socket);

    /** setsockopt() convenience wrapper for socket bool options */
int socket_set_boolopt(int socket, int level, int option_name, int bool_value);

    /** enable/disable socket non-blocking mode */
int socket_set_nonblocking(int socket, int nonblocking);

    /** join a multicast group address, returns < 0 on error */
int socket_join_multicast_group(int socket, const struct sockaddr *sa);

    /** leave a multicast group address, returns < 0 on error */
int socket_leave_multicast_group(int socket, const struct sockaddr *sa);

    /** cross-platform socket errno() which catches
        WSAESOCKTNOSUPPORT on Windows */
int socket_errno(void);

    /** like socket_errno() but ignores WSAECONNRESET on Windows */
int socket_errno_udp(void);

    /** get an error string from errno */
void socket_strerror(int err, char *buf, int size);
