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
#include <netdb.h>
#include <stdio.h>
#endif

#ifdef _WIN32
# include <malloc.h> /* MSVC or mingw on windows */
#elif defined(__linux__) || defined(__APPLE__)
# include <alloca.h> /* linux, mac, mingw, cygwin */
#else
# include <stdlib.h> /* BSDs for example */
#endif

/* Windows XP winsock doesn't provide inet_ntop */
#ifdef _WIN32
const char* INET_NTOP(int af, const void* src, char* dst, int cnt) {
    struct sockaddr_in srcaddr;
    memset(&srcaddr, 0, sizeof(struct sockaddr_in));
    memcpy(&(srcaddr.sin_addr), src, sizeof(srcaddr.sin_addr));
    srcaddr.sin_family = af;
    if(WSAAddressToString((struct sockaddr*) &srcaddr,
        sizeof(struct sockaddr_in), 0, dst, (LPDWORD) &cnt) != 0)
        return NULL;
    return dst;
}
#else
#define INET_NTOP inet_ntop
#endif

void socketreceiver_free(t_socketreceiver *x);

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
    struct sockaddr_in x_server;
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

static void netreceive_notify(t_netreceive *x, int fd);
static void outlet_sockaddr(t_outlet *o, const struct sockaddr *sa);

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
    memset(&x->x_server, 0, sizeof(struct sockaddr_in));
    return (x);
}

static void netsend_readbin(t_netsend *x, int fd)
{
    unsigned char inbuf[MAXPDSTRING];
    int ret = 0, i;
    struct sockaddr_in fromaddr = {0};
    socklen_t fromaddrlen = sizeof(struct sockaddr_in);
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
    t_symbol *hostname;
    int fportno, sportno, sockfd, portno, intarg;
    struct sockaddr_in srcaddr = {0};
    struct hostent *hp;

    /* check argument types */
    if ((argc < 2) ||
        argv[0].a_type != A_SYMBOL ||
        argv[1].a_type != A_FLOAT ||
        ((argc > 2) && argv[2].a_type != A_FLOAT))
    {
        error("netsend: bad connect arguments");
        return;
    }
    hostname = argv[0].a_w.w_symbol;
    fportno = (int)argv[1].a_w.w_float;
    sportno = (argc > 2 ? (int)argv[2].a_w.w_float : 0);
    portno = fportno;
    if (x->x_sockfd >= 0)
    {
        error("netsend: already connected");
        return;
    }

    /* create a socket */
    sockfd = socket(AF_INET, x->x_protocol, 0);
#if 0
    fprintf(stderr, "send socket %d\n", sockfd);
#endif
    if (sockfd < 0)
    {
        sys_sockerror("socket");
        return;
    }

    /* connect socket using hostname provided in command line */
    x->x_server.sin_family = AF_INET;
    hp = gethostbyname(hostname->s_name);
    if (hp == 0)
    {
        post("netsend: bad host?");
        sys_closesocket(sockfd);
        return;
    }
    memcpy((char *)&x->x_server.sin_addr, (char *)hp->h_addr, hp->h_length);

    /* assign client port number */
    x->x_server.sin_port = htons((u_short)portno);

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
        if(setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST,
                  (const void *)&intarg, sizeof(intarg)) < 0)
            post("setsockopt (SO_BROADCAST) failed");
    }

    /* bind optional src listening port */
    if (sportno != 0) {
        post("connecting to dest port %d, src port %d", portno, sportno);
        memset(&srcaddr, 0, sizeof(srcaddr));
        srcaddr.sin_family = AF_INET;
        srcaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        srcaddr.sin_port = htons((u_short)sportno);

        if (bind(sockfd, (struct sockaddr *) &srcaddr, sizeof (srcaddr)) < 0)
        {
            sys_sockerror("setting source port");
            sys_closesocket(sockfd);
            return;
        }
    }
    else {
        post("connecting to port %d", portno);
    }

    if(x->x_protocol == SOCK_STREAM) {
        /* try to connect.  LATER make a separate thread to do this
           because it might block */
        if (connect(sockfd, (struct sockaddr *)&x->x_server,
            sizeof(x->x_server)) < 0)
        {
            sys_sockerror("connecting stream socket");
            sys_closesocket(sockfd);
            return;
        }
    }

    x->x_sockfd = sockfd;
    if (x->x_msgout) /* add polling function for return messages */
    {
        if (x->x_bin)
            sys_addpollfn(sockfd, (t_fdpollfn)netsend_readbin, x);
        else
        {
            t_socketreceiver *y =
              socketreceiver_new((void *)x, 0, netsend_read,
                                 x->x_protocol == SOCK_DGRAM);
            sys_addpollfn(sockfd, (t_fdpollfn)socketreceiver_read, y);
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
        if(x->x_receiver)
            socketreceiver_free(x->x_receiver);
        x->x_receiver = NULL;
        memset(&x->x_receiver, 0, sizeof(x->x_receiver));
        memset(&x->x_server, 0, sizeof(struct sockaddr_in));
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
            res = (int)sendto(sockfd, bp, length-sent, 0,
                (struct sockaddr *)&x->x_server, sizeof(struct sockaddr_in));
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

            if(x->x_receivers[i])
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

    /* socketreceiver UDP recvfrom sockaddr_in */
static void netreceive_fromaddr(void *z, const void *fromaddr)
{
    t_netreceive *x = (t_netreceive *)z;
    if(x->x_ns.x_fromout)
        outlet_sockaddr(x->x_ns.x_fromout, (const struct sockaddr *)fromaddr);
}

static void netreceive_connectpoll(t_netreceive *x)
{
    struct sockaddr_in fromaddr = {0};
    socklen_t fromaddrlen = sizeof(struct sockaddr_in);
    int fd = 0;
    if (x->x_ns.x_fromout)
        fd = accept(x->x_ns.x_sockfd, (struct sockaddr *)&fromaddr,
            &fromaddrlen);
    else
        fd = accept(x->x_ns.x_sockfd, 0, 0);
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
            sys_addpollfn(fd, (t_fdpollfn)socketreceiver_read, y);
            x->x_receivers[x->x_nconnections] = y;
        }
        if (x->x_ns.x_fromout)
        {
                /* output from addr for new TCP connection */
            outlet_sockaddr(x->x_ns.x_fromout, (const void *)&fromaddr);
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
        if(x->x_receivers[i]) {
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
    if(x->x_ns.x_receiver)
        socketreceiver_free(x->x_ns.x_receiver);
    x->x_ns.x_receiver = NULL;
    if (x->x_ns.x_connectout)
        outlet_float(x->x_ns.x_connectout, x->x_nconnections);
}

static void netreceive_listen(t_netreceive *x, t_floatarg fportno)
{
    int portno = fportno, intarg;
    struct sockaddr_in server = {0};
    netreceive_closeall(x);
    if (portno <= 0)
        return;
    x->x_ns.x_sockfd = socket(AF_INET, x->x_ns.x_protocol, 0);
    if (x->x_ns.x_sockfd < 0)
    {
        sys_sockerror("socket");
        return;
    }
#if 0
    fprintf(stderr, "receive socket %d\n", x->x_ sockfd);
#endif

#if 1
        /* ask OS to allow another Pd to repoen this port after we close it. */
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
    intarg = 1;
    if (setsockopt(x->x_ns.x_sockfd, SOL_SOCKET, SO_BROADCAST,
        (const void *)&intarg, sizeof(intarg)) < 0)
            post("netreceive: failed to set SO_BROADCAST");
        /* Stream (TCP) sockets are set NODELAY */
    if (x->x_ns.x_protocol == SOCK_STREAM)
    {
        intarg = 1;
        if (setsockopt(x->x_ns.x_sockfd, IPPROTO_TCP, TCP_NODELAY,
            (char *)&intarg, sizeof(intarg)) < 0)
                post("netreceive: setsockopt (TCP_NODELAY) failed");
    }
        /* assign server port number etc */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons((u_short)portno);

        /* name the socket */
    if (bind(x->x_ns.x_sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        sys_sockerror("bind");
        sys_closesocket(x->x_ns.x_sockfd);
        x->x_ns.x_sockfd = -1;
        return;
    }

    if (x->x_ns.x_protocol == SOCK_DGRAM) /* datagram protocol */
    {
        if (x->x_ns.x_bin)
            sys_addpollfn(x->x_ns.x_sockfd, (t_fdpollfn)netsend_readbin, x);
        else
        {
            t_socketreceiver *y = socketreceiver_new((void *)x,
                (t_socketnotifier)netreceive_notify,
                    (x->x_ns.x_msgout ? netsend_read : 0), 1);
            if (x->x_ns.x_protocol == SOCK_DGRAM)
            {
                if (x->x_ns.x_fromout)
                    socketreceiver_set_fromaddrfn(y,
                        (t_socketfromaddrfn)netreceive_fromaddr);
            }
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

static void netreceive_setup(void)
{
    netreceive_class = class_new(gensym("netreceive"),
        (t_newmethod)netreceive_new, (t_method)netreceive_closeall,
        sizeof(t_netreceive), 0, A_GIMME, 0);
    class_addmethod(netreceive_class, (t_method)netreceive_listen,
        gensym("listen"), A_FLOAT, 0);
    class_addmethod(netreceive_class, (t_method)netreceive_send,
        gensym("send"), A_GIMME, 0);
}

static void outlet_sockaddr(t_outlet *o, const struct sockaddr *sa)
{
    struct sockaddr_in *addr = (struct sockaddr_in *)sa;
    char addrstr[INET6_ADDRSTRLEN];
    addrstr[0] = '\0';
    if(INET_NTOP(AF_INET, &addr->sin_addr.s_addr, addrstr, INET6_ADDRSTRLEN))
        outlet_symbol(o, gensym(addrstr));
}

void x_net_setup(void)
{
    netsend_setup();
    netreceive_setup();
}
