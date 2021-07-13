/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* network */

#include "m_pd.h"
#include "s_stuff.h"
#include "s_net.h"

#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#ifdef _WIN32
# include <malloc.h> /* MSVC or mingw on windows */
#elif defined(__linux__) || defined(__APPLE__)
# include <alloca.h> /* linux, mac, mingw, cygwin */
#endif

/* print addrinfo lists for debugging */
/* #define PRINT_ADDRINFO */

#define INBUFSIZE 4096

/* ----------------------------- helpers ------------------------- */

void socketreceiver_free(t_socketreceiver *x);

static void outlet_sockaddr(t_outlet *o, const struct sockaddr *sa)
{
    char addrstr[INET6_ADDRSTRLEN];
    unsigned short port = sockaddr_get_port(sa);
    if (sockaddr_get_addrstr(sa, addrstr, INET6_ADDRSTRLEN))
    {
        t_atom ap[2];
        SETSYMBOL(&ap[0], gensym(addrstr));
        SETFLOAT(&ap[1], (t_float)port);
        outlet_list(o, NULL, 2, ap);
    }
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
} t_netreceive;

static void netsend_disconnect(t_netsend *x);
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
    unsigned char inbuf[INBUFSIZE];
    int ret = 0, readbytes = 0, i;
    struct sockaddr_storage fromaddr = {0};
    socklen_t fromaddrlen = sizeof(struct sockaddr_storage);
    if (!x->x_msgout)
    {
        bug("netsend_readbin");
        return;
    }
    while (1)
    {
        if (x->x_protocol == SOCK_DGRAM)
            ret = (int)recvfrom(fd, inbuf, INBUFSIZE, 0,
                (struct sockaddr *)&fromaddr, &fromaddrlen);
        else
            ret = (int)recv(fd, inbuf, INBUFSIZE, 0);
        if (ret <= 0)
        {
            if (ret < 0)
            {
                /* socket_errno_udp() ignores some error codes */
                if (x->x_protocol == SOCK_DGRAM && !socket_errno_udp())
                    return;
                sys_sockerror("recv (bin)");
            }
            if (x->x_obj.ob_pd == netreceive_class)
            {
                    /* never close UDP socket because we can't really notify it */
                if (x->x_protocol != SOCK_DGRAM)
                {
                    sys_rmpollfn(fd);
                    sys_closesocket(fd);
                    netreceive_notify((t_netreceive *)x, fd);
                }
            }
            else /* properly shutdown netsend */
                netsend_disconnect(x);
            return;
        }
        if (x->x_protocol == SOCK_DGRAM)
        {
            t_atom *ap;
            if (x->x_fromout)
                outlet_sockaddr(x->x_fromout, (const struct sockaddr *)&fromaddr);
                /* handle too large UDP packets */
            if (ret > INBUFSIZE)
            {
                post("warning: incoming UDP packet truncated from %d to %d bytes.",
                    ret, INBUFSIZE);
                ret = INBUFSIZE;
            }
            ap = (t_atom *)alloca(ret * sizeof(t_atom));
            for (i = 0; i < ret; i++)
                SETFLOAT(ap+i, inbuf[i]);
            outlet_list(x->x_msgout, 0, ret, ap);
            readbytes += ret;
            /* throttle */
            if (readbytes >= INBUFSIZE)
                return;
            /* check for pending UDP packets */
            if (socket_bytes_available(fd) <= 0)
                return;
        }
        else
        {
            if (x->x_fromout &&
                !getpeername(fd, (struct sockaddr *)&fromaddr, &fromaddrlen))
                    outlet_sockaddr(x->x_fromout, (const struct sockaddr *)&fromaddr);
            for (i = 0; i < ret; i++)
                outlet_float(x->x_msgout, inbuf[i]);
            return;
        }
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

static void netsend_notify(void *z, int fd)
{
    t_netsend *x = (t_netsend *)z;
    if (x->x_sockfd >= 0)
    {
        /* sys_rmpollfn() and sys_closesocket() are
           already called in socketreceiver_read */
        x->x_sockfd = -1;
        if (x->x_receiver)
            socketreceiver_free(x->x_receiver);
        x->x_receiver = NULL;
        memset(&x->x_server, 0, sizeof(struct sockaddr_storage));
        outlet_float(x->x_obj.ob_outlet, 0);
    }
}

static void netsend_connect(t_netsend *x, t_symbol *s, int argc, t_atom *argv)
{
    int portno, sportno, sockfd, multicast = 0, status;
    struct addrinfo *ailist = NULL, *ai;
    const char *hostname = NULL;
    char hostbuf[256];

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
    addrinfo_sort_list(&ailist, addrinfo_ipv4_first); /* IPv4 first! */
#ifdef PRINT_ADDRINFO
    addrinfo_print_list(ailist);
#endif
    /* try each addr until we find one that works */
    for (ai = ailist; ai != NULL; ai = ai->ai_next)
    {
        /* create a socket */
        sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
#if 0
        fprintf(stderr, "send socket %d\n", sockfd);
#endif
        if (sockfd < 0)
            continue;

#if 0
        if (socket_set_boolopt(sockfd, SOL_SOCKET, SO_SNDBUF, 0) < 0)
            post("netsend: setsockopt (SO_RCVBUF) failed");
#endif

        /* for stream (TCP) sockets, specify "nodelay" */
        if (x->x_protocol == SOCK_STREAM)
        {
            if (socket_set_boolopt(sockfd, IPPROTO_TCP, TCP_NODELAY, 1) < 0)
                post("netsend: setsockopt (TCP_NODELAY) failed");
        }
        else /* datagram (UDP) broadcasting */
        {
            if (socket_set_boolopt(sockfd, SOL_SOCKET, SO_BROADCAST, 1) < 0)
                post("netsend: setsockopt (SO_BROADCAST) failed");
            multicast = sockaddr_is_multicast(ai->ai_addr);
        }
        /* if this is an IPv6 address, also listen to IPv4 adapters */
        if (ai->ai_family == AF_INET6 &&
            socket_set_boolopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, 0) < 0)
        {
            /* post("netreceive: setsockopt (IPV6_V6ONLY) failed"); */
        }

        sockaddr_get_addrstr(ai->ai_addr, hostbuf, sizeof(hostbuf));

        /* bind optional src listening port */
        if (sportno != 0)
        {
            int bound = 0;
            struct addrinfo *sailist = NULL, *sai;
            verbose(1, "connecting to %s %d, src port %d", hostbuf, portno, sportno);
            status = addrinfo_get_list(&sailist, NULL, sportno, x->x_protocol);
            if (status != 0)
            {
                pd_error(x, "netsend: could not set src port: %s (%d)",
                    gai_strerror(status), status);
                freeaddrinfo(sailist);
                goto connect_fail;
            }
            addrinfo_sort_list(&sailist, addrinfo_ipv6_first); /* IPv6 first! */
            for (sai = sailist; sai != NULL; sai = sai->ai_next)
            {
                /* if this is an IPv6 address, also listen to IPv4 adapters
                   (if not supported, fall back to IPv4) */
                if (sai->ai_family == AF_INET6 &&
                        socket_set_boolopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, 0) < 0)
                {
                    /* post("netreceive: setsockopt (IPV6_V6ONLY) failed"); */
                    continue;
                }
                if (bind(sockfd, sai->ai_addr, sai->ai_addrlen) < 0)
                    continue;
                bound = 1;
                break;
            }
            freeaddrinfo(sailist);
            if (!bound)
            {
                sys_sockerror("setting source port");
                goto connect_fail;
            }
        }
        else if (hostname && multicast)
            verbose(1, "connecting to %s %d (multicast)", hostbuf, portno);
        else
            verbose(1, "connecting to %s %d", hostbuf, portno);

        if (x->x_protocol == SOCK_STREAM)
        {
            if (socket_connect(sockfd, ai->ai_addr, ai->ai_addrlen,
                                    x->x_timeout) < 0)
            {
                sys_sockerror("connecting stream socket");
                sys_closesocket(sockfd);
                freeaddrinfo(ailist);
                /* output 0 on connection failure so the user
                   can easily retry in a loop */
                outlet_float(x->x_obj.ob_outlet, 0);
                return;
            }
        }

        /* this addr worked */
        memcpy(&x->x_server, ai->ai_addr, ai->ai_addrlen);
        break;
    }
    freeaddrinfo(ailist);

    /* confirm that socket worked */
    if (sockfd < 0)
    {
        int err = socket_errno();
        char buf[MAXPDSTRING];
        socket_strerror(err, buf, sizeof(buf));
        pd_error(x, "netsend: connect failed: %s (%d)", buf, err);
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
              socketreceiver_new((void *)x, netsend_notify, netsend_read,
                                 x->x_protocol == SOCK_DGRAM);
            sys_addpollfn(x->x_sockfd, (t_fdpollfn)socketreceiver_read, y);
            x->x_receiver = y;
        }
    }
    outlet_float(x->x_obj.ob_outlet, 1);
    return;
connect_fail:
    freeaddrinfo(ailist);
    if (sockfd > 0)
        sys_closesocket(sockfd);
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
        memset(&x->x_server, 0, sizeof(struct sockaddr_storage));
        outlet_float(x->x_obj.ob_outlet, 0);
    }
}

static int netsend_dosend(t_netsend *x, int sockfd, int argc, t_atom *argv)
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
        double timebefore = sys_getrealtime(), timeafter;
        int late;

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

        timeafter = sys_getrealtime();
        late = (timeafter - timebefore > 0.005);
        if (late || pleasewarn)
        {
            if (timeafter > lastwarntime + 2)
            {
                verbose(0, "netsend/netreceive: blocked %d msec",
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
        if (netsend_dosend(x, x->x_sockfd, argc, argv))
            netsend_disconnect(x);
    }
}

static void netsend_timeout(t_netsend *x, t_float timeout)
{
    if (timeout >= 0)
        x->x_timeout = timeout * 0.001;
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
    class_addlist(netsend_class, (t_method)netsend_send);
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

            if (x->x_receivers[i])
                socketreceiver_free(x->x_receivers[i]);
            memmove(x->x_receivers+i, x->x_receivers+(i+1),
                sizeof(t_socketreceiver*) * (x->x_nconnections - (i+1)));

            x->x_receivers = (t_socketreceiver **)t_resizebytes(x->x_receivers,
                x->x_nconnections * sizeof(t_socketreceiver*),
                    (x->x_nconnections-1) * sizeof(t_socketreceiver*));
            x->x_nconnections--;
        }
    }
    if (x->x_ns.x_connectout)
    {
        outlet_float(x->x_ns.x_connectout, x->x_nconnections);
    }
    else
        bug("netreceive_notify");
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
        if (x->x_receivers[i])
        {
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

static void netreceive_listen(t_netreceive *x, t_symbol *s, int argc, t_atom *argv)
{
    int portno = 0, sockfd, status, protocol = x->x_ns.x_protocol, multicast = 0;
    struct addrinfo *ailist = NULL, *ai;
    const char *hostname = NULL; /* allowed or UDP multicast hostname */

    netreceive_closeall(x);

    if (argc && argv->a_type == A_FLOAT)
        portno = argv->a_w.w_float, argc--, argv++;
    if (argc && argv->a_type == A_SYMBOL)
    {
        hostname = argv->a_w.w_symbol->s_name;
        argv++; argc--;
    }
    if (argc)
    {
        pd_error(x, "netreceive: extra arguments ignored:");
        postatom(argc, argv); endpost();
    }
    if (portno <= 0)
        return;
    status = addrinfo_get_list(&ailist, hostname, portno, protocol);
    if (status != 0)
    {
        pd_error(x, "netreceive: bad host or port? %s (%d)",
            gai_strerror(status), status);
        return;
    }
    if (hostname)
    {
        /* If we specify a hostname or IP address we can only listen to a single adapter.
         * For host names, we prefer IPv4 for now. LATER we might create several sockets */
        addrinfo_sort_list(&ailist, addrinfo_ipv4_first);
    }
    else
    {
        /* For the "any" address we want to prefer IPv6, so we can create a dual stack socket */
        addrinfo_sort_list(&ailist, addrinfo_ipv6_first);
    }
#ifdef PRINT_ADDRINFO
    addrinfo_print_list(ailist);
#endif
    /* try each addr until we find one that works */
    for (ai = ailist; ai != NULL; ai = ai->ai_next)
    {
        /* create a socket */
        sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sockfd < 0)
            continue;
    #if 0
        fprintf(stderr, "receive socket %d\n", sockfd);
    #endif

    #if 1
        /* ask OS to allow another Pd to reopen this port after we close it */
        if (socket_set_boolopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 1) < 0)
            post("netreceive: setsockopt (SO_REUSEADDR) failed");
    #endif
    #if 0
        intarg = 0;
        if (socket_set_boolopt(sockfd, SOL_SOCKET, SO_RCVBUF, 0) < 0)
            post("netreceive: setsockopt (SO_RCVBUF) failed");
    #endif
        if (protocol == SOCK_STREAM)
        {
            /* stream (TCP) sockets are set NODELAY */
            if (socket_set_boolopt(sockfd, IPPROTO_TCP, TCP_NODELAY, 1) < 0)
                post("netreceive: setsockopt (TCP_NODELAY) failed");
        }
        else if (protocol == SOCK_DGRAM && ai->ai_family == AF_INET)
        {
            /* enable IPv4 UDP broadcasting */
            if (socket_set_boolopt(sockfd, SOL_SOCKET, SO_BROADCAST, 1) < 0)
                post("netreceive: setsockopt (SO_BROADCAST) failed");
        }
        /* if this is the IPv6 "any" address, also listen to IPv4 adapters
           (if not supported, fall back to IPv4) */
        if (!hostname && ai->ai_family == AF_INET6 &&
                socket_set_boolopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, 0) < 0)
        {
            /* post("netreceive: setsockopt (IPV6_V6ONLY) failed"); */
            sys_closesocket(sockfd);
            sockfd = -1;
            continue;
        }
        multicast = sockaddr_is_multicast(ai->ai_addr);
#if 1
        if (multicast)
        {
            /* binding to the multicast address doesn't work on Windows and on Linux
               it doesn't seem to work for IPv6 multicast addresses, so we bind to
               the "any" address instead */
            struct addrinfo *any;
            int status = addrinfo_get_list(&any,
                (ai->ai_family == AF_INET6) ? "::" : "0.0.0.0", portno, protocol);
            if (status != 0)
            {
                pd_error(x,
                    "netreceive: getting \"any\" address for multicast failed %s (%d)",
                    gai_strerror(status), status);
                sys_closesocket(sockfd);
                return;
            }
            /* name the socket */
            status = bind(sockfd, any->ai_addr, any->ai_addrlen);
            freeaddrinfo(any);
            if (status < 0)
            {
                sys_closesocket(sockfd);
                sockfd = -1;
                continue;
            }
        }
        else
#endif
        {
            /* name the socket */
            if (bind(sockfd, ai->ai_addr, ai->ai_addrlen) < 0)
            {
                sys_closesocket(sockfd);
                sockfd = -1;
                continue;
            }
        }
        /* join multicast group */
        if (multicast && socket_join_multicast_group(sockfd, ai->ai_addr) < 0)
        {
            int err = socket_errno();
            char buf[MAXPDSTRING];
            socket_strerror(err, buf, sizeof(buf));
            pd_error(x,
                "netreceive: joining multicast group %s failed: %s (%d)",
                hostname, buf, err);
        }
        /* this addr worked */
        if (hostname)
        {
            char hostbuf[256];
            sockaddr_get_addrstr(ai->ai_addr,
                hostbuf, sizeof(hostbuf));
            verbose(1, "listening on %s %d%s", hostbuf, portno,
                (multicast ? " (multicast)" : ""));
        }
        else
            verbose(1, "listening on %d", portno);
        break;
    }
    freeaddrinfo(ailist);

    /* confirm that socket/bind worked */
    if (sockfd < 0)
    {
        int err = socket_errno();
        char buf[MAXPDSTRING];
        socket_strerror(err, buf, sizeof(buf));
        pd_error(x, "netreceive: listen failed: %s (%d)",
            buf, err);
        return;
    }
    x->x_ns.x_sockfd = sockfd;

    if (protocol == SOCK_DGRAM) /* datagram protocol */
    {
        if (x->x_ns.x_bin)
            sys_addpollfn(x->x_ns.x_sockfd, (t_fdpollfn)netsend_readbin, x);
        else
        {
                /* a UDP receiver doesn't get notifications! */
            t_socketreceiver *y = socketreceiver_new(x, 0,
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
    if (x->x_ns.x_protocol != SOCK_STREAM)
    {
        pd_error(x, "netreceive: 'send' only works for TCP");
        return;
    }
    for (i = 0; i < x->x_nconnections; i++)
    {
        if (netsend_dosend(&x->x_ns, x->x_connections[i], argc, argv))
            pd_error(x, "netreceive: send message failed");
                /* should we now close the connection? */
    }
}

static void *netreceive_new(t_symbol *s, int argc, t_atom *argv)
{
    t_netreceive *x = (t_netreceive *)pd_new(netreceive_class);
    int from = 0;
    x->x_ns.x_protocol = SOCK_STREAM;
    x->x_old = 0;
    x->x_ns.x_bin = 0;
    x->x_nconnections = 0;
    x->x_connections = (int *)t_getbytes(0);
    x->x_receivers = (t_socketreceiver **)t_getbytes(0);
    x->x_ns.x_sockfd = -1;
    if (argc && argv->a_type == A_FLOAT)
    {
        /* port argument is later passed to netreceive_listen */
        x->x_ns.x_protocol = (atom_getfloatarg(1, argc, argv) != 0 ?
            SOCK_DGRAM : SOCK_STREAM);
        x->x_old = (!strcmp(atom_getsymbolarg(2, argc, argv)->s_name, "old"));
        argc = 1; /* don't pass other arguments */
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
    netreceive_listen(x, 0, argc, argv); /* pass arguments */

    return (x);
}

static void netreceive_free(t_netreceive *x)
{
    netreceive_closeall(x);
}

static void netreceive_setup(void)
{
    netreceive_class = class_new(gensym("netreceive"),
        (t_newmethod)netreceive_new, (t_method)netreceive_free,
        sizeof(t_netreceive), 0, A_GIMME, 0);
    class_addmethod(netreceive_class, (t_method)netreceive_listen,
        gensym("listen"), A_GIMME, 0);
    class_addmethod(netreceive_class, (t_method)netreceive_send,
        gensym("send"), A_GIMME, 0);
    class_addlist(netreceive_class, (t_method)netreceive_send);
}

void x_net_setup(void)
{
    netsend_setup();
    netreceive_setup();
}
