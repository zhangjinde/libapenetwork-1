/*
   Copyright 2016 Nidium Inc. All rights reserved.
   Use of this source code is governed by a MIT license
   that can be found in the LICENSE file.
*/

#include "ape_dns.h"

#include <stdlib.h>
#include <fcntl.h>

#ifdef _WIN32
  #include <io.h>
  #include <winsock2.h>
#else
  #include <unistd.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
#endif

#ifdef FIONBIO
static __inline int setnonblocking(int fd)
{
    int ret = 1;
#ifdef _MSC_VER
    return ioctlsocket(fd, FIONBIO, &ret);
#define close _close
#else
    return ioctl(fd, FIONBIO, &ret);
#endif
}
#else
#define setnonblocking(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)
#endif

static void ares_io(int fd, int ev, void *data, ape_global *ape)
{
    ares_process_fd(ape->dns.channel, (ev & EVENT_READ ? fd : ARES_SOCKET_BAD),
                    (ev & EVENT_WRITE ? fd : ARES_SOCKET_BAD));
}

static void ares_socket_cb(void *data, ares_socket_t s, int read, int write)
{
    ape_global *ape = data;
    unsigned i, f   = 0u;

    int listenfor = read ? EVENT_READ : 0 | write ? EVENT_WRITE : 0;

    for (i = 0; i < ape->dns.sockets.size; i++) {
        if (!f && ape->dns.sockets.list[i].s.fd == 0) {
            f = i;
        } else if (ape->dns.sockets.list[i].s.fd == s) {
            /* Modify or delete the object (+ return) */
            if (!listenfor) {
                ape->dns.sockets.list[i].s.fd = 0;
                events_del(s, ape);
                close(s);
            } else {
                events_mod((ape_event_descriptor *)&ape->dns.sockets.list[i],
                           listenfor | EVENT_LEVEL, ape);
            }
            return;
        }
    }

    setnonblocking(s);
    ape->dns.sockets.list[f].s.fd   = s;
    ape->dns.sockets.list[f].s.type = APE_EVENT_DELEGATE;
    ape->dns.sockets.list[f].on_io  = ares_io;
    ape->dns.sockets.list[f].data   = NULL;

    events_add((ape_event_descriptor *)&ape->dns.sockets.list[f],
               listenfor | EVENT_LEVEL, ape);
}

int ape_dns_init(ape_global *ape)
{
    struct ares_options opt;
    int ret;

    if (ares_library_init(ARES_LIB_INIT_ALL) != 0) {
        return -1;
    }

    opt.sock_state_cb      = ares_socket_cb;
    opt.sock_state_cb_data = ape;

    /* At the moment we only use one dns channel */
    if ((ret = ares_init_options(&ape->dns.channel, &opt,
                                 0x00 | ARES_OPT_SOCK_STATE_CB))
        != ARES_SUCCESS) {

        return -1;
    }
    ape->dns.sockets.list = calloc(32, sizeof(struct _ares_sockets) * 32);
    ape->dns.sockets.size = 32;
    ape->dns.sockets.used = 0;

    return 0;
}

static int params_free(void *arg)
{
    free(arg);
    return 0;
}

static void ares_gethostbyname_cb(void *arg, int status, int timeout,
                                  struct hostent *host)
{
    struct _ape_dns_cb_argv *params = arg;
    char ret[46];

    params->done = 1;

    if (params->invalidate) {
        free(params);
        return;
    }

    if (status == ARES_SUCCESS) {
        /* only return the first h_addr_list element */
        inet_ntop(host->h_addrtype, *host->h_addr_list, ret, sizeof(ret));
        params->callback(ret, params->arg, status);
    } else {
        params->callback(NULL, params->arg, status);
    }

    ape_global *ape = params->ape;
    timer_dispatch_async(params_free, params);
}

ape_dns_state *ape_gethostbyname(const char *host,
                                 ape_gethostbyname_callback callback, void *arg,
                                 ape_global *ape)
{
    struct in_addr addr4;

    if (inet_pton(AF_INET, host, &addr4) == 1) {
        callback(host, arg, ARES_SUCCESS);

        return NULL;
    } else {
        struct _ape_dns_cb_argv *cb = malloc(sizeof(*cb));

        cb->ape        = ape;
        cb->callback   = callback;
        cb->origin     = host;
        cb->arg        = arg;
        cb->invalidate = 0;
        cb->done       = 0;

        ares_gethostbyname(ape->dns.channel, host, AF_INET,
                           ares_gethostbyname_cb, cb);

        if (cb->done) {
            return NULL;
        }

        return cb;
    }
}

void ape_dns_invalidate(ape_dns_state *state)
{
    if (state != NULL) {
        state->invalidate = 1;
    }
}

