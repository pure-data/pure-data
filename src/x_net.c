/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* network */

#include "m_pd.h"
#include "s_stuff.h"

#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <netdb.h>
#include <stdio.h>
#include <fcntl.h>
#endif

#ifdef _WIN32
# include <malloc.h> /* MSVC or mingw on windows */
#elif defined(__linux__) || defined(__APPLE__)
# include <alloca.h> /* linux, mac, mingw, cygwin */
#else
# include <stdlib.h> /* BSDs for example */
#endif

/* print addrinfo lists for debugging */
//#define POST_ADDRINFO

/* ----------------------------- helpers ------------------------- */

/* Windows XP winsock doesn't provide inet_ntop */
#ifdef _WIN32
const char* INET_NTOP(int af, const void *src, char *dst, socklen_t size) {
    struct sockaddr_storage addr;
    sockklen_t addrlen;
    addr.ss_family = af;
    memset(&addr, 0, sizeof(struct sockaddr_storage));
    if (af == AF_INET6)
    {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&addr;
        memcpy(&(sa6->sin6_addr.s6_addr), src, sizeof(sa6->sin6_addr.s6_addr));
        addrlen = sizeof(struct sockaddr_in6);
    }
    else (af == AF_INET)
    {
        struct sockaddr_in *sa4 = (struct sockaddr_in *)&addr;
        memcpy(&(sa4->sin_addr.s_addr), src, sizeof(sa4->sin_addr.s_addr));
        addrlen = sizeof(struct sockaddr_in);
    }
    else
        return NULL;
    if (WSAAddressToString((struct sockaddr *)&addr, addrlen, 0, dst,
        (LPDWORD)&size) != 0)
        return NULL;
    return dst;
}
#else
#define INET_NTOP inet_ntop
#endif

void socketreceiver_free(t_socketreceiver *x);

// getaddrinfo() convenience wrapper
// addrinfo linked list must be freed with freeaddrinfo()
// returns errno which can be printed with gai_strerror()
static int addrinfo_get_list(struct addrinfo **ailist, const char *hostname,
                             int port, int protocol) {
    struct addrinfo hints;
    char portstr[32];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
    hints.ai_socktype = protocol;
    hints.ai_protocol = (protocol == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP);
    hints.ai_flags = AI_DEFAULT |
                     AI_PASSIVE;  // listen to any addr if hostname is NULL
    portstr[0] = '\0';
    snprintf(portstr, 32, "%d", port);
    return getaddrinfo(hostname, portstr, &hints, ailist);
}

#ifdef POST_ADDRINFO
// post addrinfo linked list for debugging
static void addrinfo_post_list(struct addrinfo **ailist)
{
    const struct addrinfo *ai;
    char addrstr[INET6_ADDRSTRLEN];
    for(ai = (*ailist); ai != NULL; ai = ai->ai_next)
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
        post("%s %s", ipver, addrstr);
    }
}
#endif

static int sockaddr_is_multicast(const struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;
        return (sa6->sin6_addr.s6_addr[0] == 0xFF);
    }
    else if(sa->sa_family == AF_INET)
    {
        struct sockaddr_in *sa4 = (struct sockaddr_in *)sa;
        return ((ntohl(sa4->sin_addr.s_addr) & 0xF0000000) == 0xE0000000);
    }
    return 0;
}

static void outlet_sockaddr(t_outlet *o, const struct sockaddr *sa)
{
    const void *addr;
    unsigned short port;
    char addrstr[INET6_ADDRSTRLEN];
    addrstr[0] = '\0';
    if (sa->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;
        addr = (const void *)&sa6->sin6_addr.s6_addr;
        port = ntohs(sa6->sin6_port);
    }
    else if (sa->sa_family == AF_INET)
    {
        struct sockaddr_in *sa4 = (struct sockaddr_in *)sa;
        addr = (const void *)&sa4->sin_addr.s_addr;
        port = ntohs(sa4->sin_port);
    }
    else return;
    if (INET_NTOP(sa->sa_family, addr, addrstr, INET6_ADDRSTRLEN))
    {
        t_atom ap[2];
        SETSYMBOL(&ap[0], gensym(addrstr));
        SETFLOAT(&ap[1], (float)port);
        outlet_list(o, NULL, 2, ap);
    }
}

static void socket_set_nonblocking(int sockfd, int nonblocking)
{
#ifdef _WIN32
    u_long modearg = nonblocking;
    if (ioctlsocket(sockfd, FIONBIO, &modearg) != NO_ERROR)
        post("ioctlsocket (FIONBIO %d) failed", nonblocking);
#else
    int sockflags = fcntl(sockfd, F_GETFL, 0);
    if (nonblocking)
        sockflags |= O_NONBLOCK;
    else
        sockflags &= ~O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, sockflags) < 0)
        post("fcntl (O_NONBLOCK %d) failed", nonblocking);
#endif
}

/* join a multicast group addr, returns < 0 on error */
static int socket_join_multicast_group(int sockfd, struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)&sa;
        struct ipv6_mreq mreq6;
        memcpy(&mreq6.ipv6mr_multiaddr, &sa6->sin6_addr,
            sizeof(struct in6_addr));
        mreq6.ipv6mr_interface = 0;
        return setsockopt(sockfd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
            (char *)&mreq6, sizeof(mreq6));
    }
    else if (sa->sa_family == AF_INET)
    {
        struct sockaddr_in *sa4 = (struct sockaddr_in *)&sa;
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = sa4->sin_addr.s_addr;
        mreq.imr_interface.s_addr = INADDR_ANY;
        return setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
            (char *)&mreq, sizeof(mreq));
    }
    return -1;
}

/* ----------------------------- net ------------------------- */

static t_class *netsend_class;

typedef struct _netsend
{
    t_object x_obj;
    t_outlet *x_msgout;
    t_outlet *x_connectout;
    t_outlet *x_fromout;
    int x_sockfd;
    int x_protocol;
    int x_bin;
    t_socketreceiver *x_receiver;
    struct sockaddr_storage x_server;
    t_float x_timeout; /* TCP connect timeout in seconds */
} t_netsend;

static t_class *netreceive_class;

typedef struct _netreceive
{
    t_netsend x_ns;
    int x_nconnections;
    int x_sockfd;
    int *x_connections;
    int x_old;
    t_socketreceiver **x_receivers;
    t_symbol *x_hostname; /* allowed or multicast hostname, NULL if not set */
} t_netreceive;

static void netreceive_notify(t_netreceive *x, int fd);

/* ----------------------------- netsend ------------------------- */

static void *netsend_new(t_symbol *s, int argc, t_atom *argv)
{
    t_netsend *x = (t_netsend *)pd_new(netsend_class);
    outlet_new(&x->x_obj, &s_float);
    x->x_protocol = SOCK_STREAM;
    x->x_bin = 0;
    if (argc && argv->a_type == A_FLOAT)
    {
        x->x_protocol = (argv->a_w.w_float != 0 ? SOCK_DGRAM : SOCK_STREAM);
        argc = 0;
    }
    else while (argc && argv->a_type == A_SYMBOL &&
        *argv->a_w.w_symbol->s_name == '-')
    {
        if (!strcmp(argv->a_w.w_symbol->s_name, "-b"))
            x->x_bin = 1;
        else if (!strcmp(argv->a_w.w_symbol->s_name, "-u"))
            x->x_protocol = SOCK_DGRAM;
        else
        {
            pd_error(x, "netsend: unknown flag ...");
            postatom(argc, argv); endpost();
        }
        argc--; argv++;
    }
    if (argc)
    {
        pd_error(x, "netsend: extra arguments ignored:");
        postatom(argc, argv); endpost();
    }
    x->x_sockfd = -1;
    x->x_receiver = NULL;
    x->x_msgout = outlet_new(&x->x_obj, &s_anything);
    x->x_connectout = NULL;
    x->x_fromout = NULL;
    x->x_timeout = 10;
    memset(&x->x_server, 0, sizeof(struct sockaddr_storage));
    return (x);
}

static void netsend_readbin(t_netsend *x, int fd)
{
    unsigned char inbuf[MAXPDSTRING];
    int ret = 0, i;
    struct sockaddr_storage fromaddr = {0};
    socklen_t fromaddrlen = sizeof(struct sockaddr_storage);
    if (!x->x_msgout)
    {
        bug("netsend_readbin");
        return;
    }
    if (x->x_protocol == SOCK_DGRAM)
        ret = (int)recvfrom(fd, inbuf, MAXPDSTRING, 0,
            (struct sockaddr *)&fromaddr, &fromaddrlen);
    else
        ret = (int)recv(fd, inbuf, MAXPDSTRING, 0);
    if (ret <= 0)
    {
        if (ret < 0)
            sys_sockerror("recv (bin)");
        sys_rmpollfn(fd);
        sys_closesocket(fd);
        if (x->x_obj.ob_pd == netreceive_class)
            netreceive_notify((t_netreceive *)x, fd);
        return;
    }
    if (x->x_protocol == SOCK_DGRAM)
    {
        if (x->x_fromout)
            outlet_sockaddr(x->x_fromout, (const struct sockaddr *)&fromaddr);
        t_atom *ap = (t_atom *)alloca(ret * sizeof(t_atom));
        for (i = 0; i < ret; i++)
            SETFLOAT(ap+i, inbuf[i]);
        outlet_list(x->x_msgout, 0, ret, ap);
    }
    else
    {
        if (x->x_fromout &&
            !getpeername(fd, (struct sockaddr *)&fromaddr, &fromaddrlen))
                outlet_sockaddr(x->x_fromout, (const struct sockaddr *)&fromaddr);
        for (i = 0; i < ret; i++)
            outlet_float(x->x_msgout, inbuf[i]);
    }
}

static void netsend_read(void *z, t_binbuf *b)
{
    t_netsend *x = (t_netsend *)z;
    int msg, natom = binbuf_getnatom(b);
    t_atom *at = binbuf_getvec(b);
    for (msg = 0; msg < natom;)
    {
        int emsg;
        for (emsg = msg; emsg < natom && at[emsg].a_type != A_COMMA
             && at[emsg].a_type != A_SEMI; emsg++)
            ;
        if (emsg > msg)
        {
            int i;
            for (i = msg; i < emsg; i++)
                if (at[i].a_type == A_DOLLAR || at[i].a_type == A_DOLLSYM)
                {
                    pd_error(x, "netreceive: got dollar sign in message");
                    goto nodice;
                }
            if (at[msg].a_type == A_FLOAT)
            {
                if (emsg > msg + 1)
                    outlet_list(x->x_msgout, 0, emsg-msg, at + msg);
                else outlet_float(x->x_msgout, at[msg].a_w.w_float);
            }
            else if (at[msg].a_type == A_SYMBOL)
                outlet_anything(x->x_msgout, at[msg].a_w.w_symbol,
                    emsg-msg-1, at + msg + 1);
        }
    nodice:
        msg = emsg + 1;
    }
}

static void netsend_connect(t_netsend *x, t_symbol *s, int argc, t_atom *argv)
{
    int portno, sportno, sockfd, intarg, multicast = 0, status;
    struct addrinfo *ailist = NULL, *ai;
    const char *hostname = NULL;

    /* check argument types */
    if ((argc < 2) ||
        argv[0].a_type != A_SYMBOL ||
        argv[1].a_type != A_FLOAT ||
        ((argc > 2) && argv[2].a_type != A_FLOAT))
    {
        error("netsend: bad connect arguments");
        return;
    }
    hostname = argv[0].a_w.w_symbol->s_name;
    portno = (int)argv[1].a_w.w_float;
    sportno = (argc > 2 ? (int)argv[2].a_w.w_float : 0);
    if (x->x_sockfd >= 0)
    {
        error("netsend: already connected");
        return;
    }

    /* get addrinfo list using hostname & port */
    status = addrinfo_get_list(&ailist, hostname, portno, x->x_protocol);
    if (status != 0)
    {
        pd_error(x, "netsend: bad host or port? %s (%d)",
            gai_strerror(status), status);
        return;
    }
#ifdef POST_ADDRINFO
    addrinfo_post_list(&ailist);
#endif

    /* try each addr until we find one that works */
    for(ai = ailist; ai != NULL; ai = ai->ai_next) {

        /* create a socket */
        sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
#if 0
        fprintf(stderr, "send socket %d\n", sockfd);
#endif
        if (sockfd < 0)
            continue;

#if 0
        intarg = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF,
                     &intarg, sizeof(intarg)) < 0)
        post("setsockopt (SO_RCVBUF) failed");
#endif

        /* for stream (TCP) sockets, specify "nodelay" */
        if (x->x_protocol == SOCK_STREAM)
        {
            intarg = 1;
            if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
                           (char *)&intarg, sizeof(intarg)) < 0)
                post("setsockopt (TCP_NODELAY) failed");
        }
        else { /* datagram (UDP) broadcasting */
            intarg = 1;
            if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST,
                      (const void *)&intarg, sizeof(intarg)) < 0)
                post("setsockopt (SO_BROADCAST) failed");

            /* multicast? */
            multicast = sockaddr_is_multicast(ai->ai_addr);
        }

        /* bind optional src listening port */
        if (sportno != 0) {
            int bound = 0;
            struct addrinfo *sailist = NULL, *sai;
            post("connecting to dest port %d, src port %d", portno, sportno);
            status = addrinfo_get_list(&sailist, NULL, sportno, x->x_protocol);
            if (status != 0)
            {
                pd_error(x, "netsend: could not set src port: %s (%d)",
                    gai_strerror(status), status);
                return;
            }
            for(sai = sailist; sai != NULL; sai = sai->ai_next) {
                if (bind(sockfd, sai->ai_addr, sai->ai_addrlen) < 0)
                    continue;
                bound = 1;
                break;
            }
            freeaddrinfo(sailist);
            if (!bound)
            {
                sys_sockerror("setting source port");
                sys_closesocket(sockfd);
                return;
            }
        }
        else if(hostname && multicast)
            post("connecting to port %d, multicast %s", portno, hostname);
        else
            post("connecting to port %d", portno);

        if (x->x_protocol == SOCK_STREAM)
        {
            int status;
            struct timeval timeout;

            /* set non-blocking */
            socket_set_nonblocking(sockfd, 1);

            /* connect */
            status = connect(sockfd, ai->ai_addr, ai->ai_addrlen);
            if (status < 0 && sys_sockerrno() != EINPROGRESS)
            {
                sys_sockerror("connecting stream socket");
                sys_closesocket(sockfd);
                return;
            }

            /* block with select using timeout */
            fd_set writefds;
            FD_ZERO(&writefds);
            FD_SET(sockfd, &writefds); /* socket is connected when writable */
            timeout.tv_sec = (int)x->x_timeout;
            timeout.tv_usec = (x->x_timeout - timeout.tv_sec) * 1000000;
            status = select(sockfd+1, NULL, &writefds, NULL, &timeout);
            if(status < 0 || !FD_ISSET(sockfd, &writefds))
            {
                error("connection timed out");
                sys_closesocket(sockfd);
                return;
            }

            /* set blocking again */
            socket_set_nonblocking(sockfd, 0);
        }

        /* this addr worked */
        memcpy(&x->x_server, ai->ai_addr, ai->ai_addrlen);
        break;
    }
    freeaddrinfo(ailist);

    /* confirm that socket & bind worked */
    if (sockfd == -1)
    {
        int err = sys_sockerrno();
        pd_error(x, "netsend: connect failed: %s (%d)", strerror(errno), errno);
        return;
    }

    x->x_sockfd = sockfd;
    if (x->x_msgout) /* add polling function for return messages */
    {
        if (x->x_bin)
            sys_addpollfn(x->x_sockfd, (t_fdpollfn)netsend_readbin, x);
        else
        {
            t_socketreceiver *y =
              socketreceiver_new((void *)x, 0, netsend_read,
                                 x->x_protocol == SOCK_DGRAM);
            sys_addpollfn(x->x_sockfd, (t_fdpollfn)socketreceiver_read, y);
            x->x_receiver = y;
        }
    }
    outlet_float(x->x_obj.ob_outlet, 1);
}

static void netsend_disconnect(t_netsend *x)
{
    if (x->x_sockfd >= 0)
    {
        sys_rmpollfn(x->x_sockfd);
        sys_closesocket(x->x_sockfd);
        x->x_sockfd = -1;
        if (x->x_receiver)
            socketreceiver_free(x->x_receiver);
        x->x_receiver = NULL;
        memset(&x->x_receiver, 0, sizeof(x->x_receiver));
        memset(&x->x_server, 0, sizeof(struct sockaddr_storage));
        outlet_float(x->x_obj.ob_outlet, 0);
    }
}

static int netsend_dosend(t_netsend *x, int sockfd,
    t_symbol *s, int argc, t_atom *argv)
{
    char *buf, *bp;
    int length, sent, fail = 0;
    t_binbuf *b = 0;
    if (x->x_bin)
    {
        int i;
        buf = alloca(argc);
        for (i = 0; i < argc; i++)
            ((unsigned char *)buf)[i] = atom_getfloatarg(i, argc, argv);
        length = argc;
    }
    else
    {
        t_atom at;
        b = binbuf_new();
        binbuf_add(b, argc, argv);
        SETSEMI(&at);
        binbuf_add(b, 1, &at);
        binbuf_gettext(b, &buf, &length);
    }
    for (bp = buf, sent = 0; sent < length;)
    {
        static double lastwarntime;
        static double pleasewarn;
        double timebefore = sys_getrealtime();

        int res = 0;
        if (x->x_protocol == SOCK_DGRAM)
        {
            socklen_t addrlen = (x->x_server.ss_family == AF_INET6 ?
                sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));
            res = (int)sendto(sockfd, bp, length-sent, 0,
                (struct sockaddr *)&x->x_server, addrlen);
        }
        else
            res = (int)send(sockfd, bp, length-sent, 0);

        double timeafter = sys_getrealtime();
        int late = (timeafter - timebefore > 0.005);
        if (late || pleasewarn)
        {
            if (timeafter > lastwarntime + 2)
            {
                post("netsend/netreceive blocked %d msec",
                     (int)(1000 * ((timeafter - timebefore) +
                     pleasewarn)));
                pleasewarn = 0;
                lastwarntime = timeafter;
            }
            else if (late) pleasewarn += timeafter - timebefore;
        }
        if (res <= 0)
        {
            sys_sockerror("send");
            fail = 1;
            break;
        }
        else
        {
            sent += res;
            bp += res;
        }
    }
    done:
    if (!x->x_bin)
    {
        t_freebytes(buf, length);
        binbuf_free(b);
    }
    return (fail);
}

static void netsend_send(t_netsend *x, t_symbol *s, int argc, t_atom *argv)
{
    if (x->x_sockfd >= 0)
    {
        if (netsend_dosend(x, x->x_sockfd, s, argc, argv))
            netsend_disconnect(x);
    }
}

static void netsend_timeout(t_netsend *x, t_float timeout)
{
    if (timeout >= 0)
        x->x_timeout = timeout;
}

static void netsend_free(t_netsend *x)
{
    netsend_disconnect(x);
}

static void netsend_setup(void)
{
    netsend_class = class_new(gensym("netsend"), (t_newmethod)netsend_new,
        (t_method)netsend_free,
        sizeof(t_netsend), 0, A_GIMME, 0);
    class_addmethod(netsend_class, (t_method)netsend_connect,
        gensym("connect"), A_GIMME, 0);
    class_addmethod(netsend_class, (t_method)netsend_disconnect,
        gensym("disconnect"), 0);
    class_addmethod(netsend_class, (t_method)netsend_send,
        gensym("send"), A_GIMME, 0);
    class_addmethod(netsend_class, (t_method)netsend_timeout,
        gensym("timeout"), A_DEFFLOAT, 0);
}

/* ----------------------------- netreceive ------------------------- */

static void netreceive_notify(t_netreceive *x, int fd)
{
    int i;
    for (i = 0; i < x->x_nconnections; i++)
    {
        if (x->x_connections[i] == fd)
        {
            memmove(x->x_connections+i, x->x_connections+(i+1),
                sizeof(int) * (x->x_nconnections - (i+1)));
            x->x_connections = (int *)t_resizebytes(x->x_connections,
                x->x_nconnections * sizeof(int),
                    (x->x_nconnections-1) * sizeof(int));
            memmove(x->x_receivers+i, x->x_receivers+(i+1),
                sizeof(t_socketreceiver*) * (x->x_nconnections - (i+1)));

            if (x->x_receivers[i])
                socketreceiver_free(x->x_receivers[i]);
            x->x_receivers[i] = NULL;
            x->x_receivers = (t_socketreceiver **)t_resizebytes(x->x_receivers,
                x->x_nconnections * sizeof(t_socketreceiver*),
                    (x->x_nconnections-1) * sizeof(t_socketreceiver*));
            x->x_nconnections--;
        }
    }
    outlet_float(x->x_ns.x_connectout, x->x_nconnections);
}

    /* socketreceiver from sockaddr_in */
static void netreceive_fromaddr(void *z, const void *fromaddr)
{
    t_netreceive *x = (t_netreceive *)z;
    if (x->x_ns.x_fromout)
        outlet_sockaddr(x->x_ns.x_fromout, (const struct sockaddr *)fromaddr);
}

static void netreceive_connectpoll(t_netreceive *x)
{
    int fd = accept(x->x_ns.x_sockfd, 0, 0);
    if (fd < 0) post("netreceive: accept failed");
    else
    {
        int nconnections = x->x_nconnections+1;

        x->x_connections = (int *)t_resizebytes(x->x_connections,
            x->x_nconnections * sizeof(int), nconnections * sizeof(int));
        x->x_connections[x->x_nconnections] = fd;
        x->x_receivers = (t_socketreceiver **)t_resizebytes(x->x_receivers,
            x->x_nconnections * sizeof(t_socketreceiver*),
            nconnections * sizeof(t_socketreceiver*));
        x->x_receivers[x->x_nconnections] = NULL;
        if (x->x_ns.x_bin)
            sys_addpollfn(fd, (t_fdpollfn)netsend_readbin, x);
        else
        {
            t_socketreceiver *y = socketreceiver_new((void *)x,
            (t_socketnotifier)netreceive_notify,
                (x->x_ns.x_msgout ? netsend_read : 0), 0);
            if (x->x_ns.x_fromout)
                socketreceiver_set_fromaddrfn(y,
                    (t_socketfromaddrfn)netreceive_fromaddr);
            sys_addpollfn(fd, (t_fdpollfn)socketreceiver_read, y);
            x->x_receivers[x->x_nconnections] = y;
        }
        outlet_float(x->x_ns.x_connectout, (x->x_nconnections = nconnections));
    }
}

static void netreceive_closeall(t_netreceive *x)
{
    int i;
    for (i = 0; i < x->x_nconnections; i++)
    {
        sys_rmpollfn(x->x_connections[i]);
        sys_closesocket(x->x_connections[i]);
        if (x->x_receivers[i]) {
            socketreceiver_free(x->x_receivers[i]);
            x->x_receivers[i] = NULL;
        }
    }
    x->x_connections = (int *)t_resizebytes(x->x_connections,
        x->x_nconnections * sizeof(int), 0);
    x->x_receivers = (t_socketreceiver**)t_resizebytes(x->x_receivers,
                x->x_nconnections * sizeof(t_socketreceiver*), 0);
    x->x_nconnections = 0;
    if (x->x_ns.x_sockfd >= 0)
    {
        sys_rmpollfn(x->x_ns.x_sockfd);
        sys_closesocket(x->x_ns.x_sockfd);
    }
    x->x_ns.x_sockfd = -1;
    if (x->x_ns.x_receiver)
        socketreceiver_free(x->x_ns.x_receiver);
    x->x_ns.x_receiver = NULL;
    if (x->x_ns.x_connectout)
        outlet_float(x->x_ns.x_connectout, x->x_nconnections);
}

static void netreceive_listen(t_netreceive *x, t_floatarg fportno)
{
    int portno = fportno, status, intarg;
    struct addrinfo *ailist = NULL, *ai;
    struct sockaddr_storage server = {0};
    const char *hostname = NULL;

    netreceive_closeall(x);
    if (portno <= 0)
        return;
    if (x->x_hostname)
        hostname = x->x_hostname->s_name;
    status = addrinfo_get_list(&ailist, hostname, portno, x->x_ns.x_protocol);
    if (status != 0)
    {
        pd_error(x, "netreceive: bad host or port? %s (%d)",
            gai_strerror(status), status);
        return;
    }
#ifdef POST_ADDRINFO
    addrinfo_post_list(&ailist);
#endif

    /* try each addr until we find one that works */
    for(ai = ailist; ai != NULL; ai = ai->ai_next) {
        x->x_ns.x_sockfd =
            socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (x->x_ns.x_sockfd < 0)
            continue;
    #if 0
        fprintf(stderr, "receive socket %d\n", x->x-ns.x_sockfd);
    #endif

    #if 1
        /* ask OS to allow another Pd to repoen this port after we close it */
        intarg = 1;
        if (setsockopt(x->x_ns.x_sockfd, SOL_SOCKET, SO_REUSEADDR,
            (char *)&intarg, sizeof(intarg)) < 0)
                post("netreceive: setsockopt (SO_REUSEADDR) failed");
    #endif
    #if 0
        intarg = 0;
        if (setsockopt(x->x_ns.x_sockfd, SOL_SOCKET, SO_RCVBUF,
            &intarg, sizeof(intarg)) < 0)
                post("setsockopt (SO_RCVBUF) failed");
    #endif
        if (ai->ai_protocol == SOCK_STREAM)
        {
            /* stream (TCP) sockets are set NODELAY */
            intarg = 1;
            if (setsockopt(x->x_ns.x_sockfd, IPPROTO_TCP, TCP_NODELAY,
                (char *)&intarg, sizeof(intarg)) < 0)
                    post("netreceive: setsockopt (TCP_NODELAY) failed");
        }
        else if (ai->ai_protocol == SOCK_DGRAM && ai->ai_family == AF_INET)
        {
            /* enable IPv4 UDP broadcasting */
            intarg = 1;
            if (setsockopt(x->x_ns.x_sockfd, SOL_SOCKET, SO_BROADCAST,
                (const void *)&intarg, sizeof(intarg)) < 0)
                    post("netreceive: setsockopt (SO_BROADCAST) failed");
        }

        /* name the socket */
        if (bind(x->x_ns.x_sockfd, ai->ai_addr, ai->ai_addrlen) < 0)
        {
            sys_closesocket(x->x_ns.x_sockfd);
            x->x_ns.x_sockfd = -1;
            continue;
        }

        /* this addr worked */
        memcpy(&server, ai->ai_addr, ai->ai_addrlen);
        break;
    }
    freeaddrinfo(ailist);

    /* confirm that socket/bind worked */
    if (x->x_ns.x_sockfd == -1)
    {
        int err = sys_sockerrno();
        pd_error(x, "netreceive: listen failed: %s (%d)",
            strerror(errno), errno);
        return;
    }

    if (x->x_ns.x_protocol == SOCK_DGRAM) /* datagram protocol */
    {
        /* join multicast group */
        if (sockaddr_is_multicast((struct sockaddr *)&server))
        {
            if (socket_join_multicast_group(x->x_ns.x_sockfd,
                (struct sockaddr *)&server) < 0)
            {
                int err = sys_sockerrno();
                pd_error(x, "netreceive: joining multicast group %s failed: %s (%d)",
                    hostname, strerror(errno), errno);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            
            }
            else
                post("netreceive: joined multicast group %s", hostname);
        }

        if (x->x_ns.x_bin)
            sys_addpollfn(x->x_ns.x_sockfd, (t_fdpollfn)netsend_readbin, x);
        else
        {
            t_socketreceiver *y = socketreceiver_new((void *)x,
                (t_socketnotifier)netreceive_notify,
                    (x->x_ns.x_msgout ? netsend_read : 0), 1);
            if (x->x_ns.x_fromout)
                socketreceiver_set_fromaddrfn(y,
                    (t_socketfromaddrfn)netreceive_fromaddr);
            sys_addpollfn(x->x_ns.x_sockfd, (t_fdpollfn)socketreceiver_read, y);
            x->x_ns.x_connectout = 0;
            x->x_ns.x_receiver = y;
        }
    }
    else /* streaming protocol */
    {
        if (listen(x->x_ns.x_sockfd, 5) < 0)
        {
            sys_sockerror("listen");
            sys_closesocket(x->x_ns.x_sockfd);
            x->x_ns.x_sockfd = -1;
        }
        else
        {
            sys_addpollfn(x->x_ns.x_sockfd,
                (t_fdpollfn)netreceive_connectpoll,x);
        }
    }
}

static void netreceive_send(t_netreceive *x,
    t_symbol *s, int argc, t_atom *argv)
{
    int i;
    for (i = 0; i < x->x_nconnections; i++)
    {
        if (netsend_dosend(&x->x_ns, x->x_connections[i], s, argc, argv))
            pd_error(x, "netreceive: send message failed");
                /* should we now close the connection? */
    }
}

static void *netreceive_new(t_symbol *s, int argc, t_atom *argv)
{
    t_netreceive *x = (t_netreceive *)pd_new(netreceive_class);
    int portno = 0;
    unsigned int from = 0;
    x->x_ns.x_protocol = SOCK_STREAM;
    x->x_old = 0;
    x->x_ns.x_bin = 0;
    x->x_nconnections = 0;
    x->x_connections = (int *)t_getbytes(0);
    x->x_receivers = (t_socketreceiver **)t_getbytes(0);
    x->x_hostname = NULL;
    x->x_ns.x_sockfd = -1;
    if (argc && argv->a_type == A_FLOAT)
    {
        portno = atom_getfloatarg(0, argc, argv);
        x->x_ns.x_protocol = (atom_getfloatarg(1, argc, argv) != 0 ?
            SOCK_DGRAM : SOCK_STREAM);
        x->x_old = (!strcmp(atom_getsymbolarg(2, argc, argv)->s_name, "old"));
        argc = 0;
    }
    else
    {
        while (argc && argv->a_type == A_SYMBOL &&
            *argv->a_w.w_symbol->s_name == '-')
        {
            if (!strcmp(argv->a_w.w_symbol->s_name, "-b"))
                x->x_ns.x_bin = 1;
            else if (!strcmp(argv->a_w.w_symbol->s_name, "-u"))
                x->x_ns.x_protocol = SOCK_DGRAM;
            else if (!strcmp(argv->a_w.w_symbol->s_name, "-f"))
                from = 1;
            else
            {
                pd_error(x, "netreceive: unknown flag ...");
                postatom(argc, argv); endpost();
            }
            argc--; argv++;
        }
    }
    if (argc && argv->a_type == A_FLOAT)
        portno = argv->a_w.w_float, argc--, argv++;
    if (argc && argv->a_type == A_SYMBOL)
    {
        if (x->x_ns.x_protocol == SOCK_DGRAM)
            x->x_hostname = atom_getsymbol(argv);
        else
        {
            pd_error(x, "netreceive: hostname argument ignored:");
            postatom(argc, argv); endpost();
        }
        argc--, argv++;
    }
    if (argc)
    {
        pd_error(x, "netreceive: extra arguments ignored:");
        postatom(argc, argv); endpost();
    }
    if (x->x_old)
    {
        /* old style, nonsecure version */
        x->x_ns.x_msgout = 0;
    }
    else x->x_ns.x_msgout = outlet_new(&x->x_ns.x_obj, &s_anything);
    if (x->x_ns.x_protocol == SOCK_STREAM)
        x->x_ns.x_connectout = outlet_new(&x->x_ns.x_obj, &s_float);
    else
        x->x_ns.x_connectout = 0;
    if (from)
        x->x_ns.x_fromout = outlet_new(&x->x_ns.x_obj, &s_symbol);
    else
        x->x_ns.x_fromout = NULL;
        /* create a socket */
    if (portno > 0)
        netreceive_listen(x, portno);

    return (x);
}

static void netreceive_free(t_netreceive *x)
{
    netreceive_closeall(x);
    if (x->x_hostname) free(x->x_hostname);
}

static void netreceive_setup(void)
{
    netreceive_class = class_new(gensym("netreceive"),
        (t_newmethod)netreceive_new, (t_method)netreceive_free,
        sizeof(t_netreceive), 0, A_GIMME, 0);
    class_addmethod(netreceive_class, (t_method)netreceive_listen,
        gensym("listen"), A_FLOAT, 0);
    class_addmethod(netreceive_class, (t_method)netreceive_send,
        gensym("send"), A_GIMME, 0);
}

void x_net_setup(void)
{
    netsend_setup();
    netreceive_setup();
}
