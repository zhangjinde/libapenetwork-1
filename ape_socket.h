/*
    APE Network Library
    Copyright (C) 2010-2013 Anthony Catel <paraboul@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef __APE_SOCKET_H
#define __APE_SOCKET_H

#include "common.h"
#include "ape_buffer.h"
#include "ape_pool.h"

#ifdef __WIN32

#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

#if 0
#define ECONNRESET WSAECONNRESET
#define EINPROGRESS WSAEINPROGRESS
#define EALREADY WSAEALREADY
#define ECONNABORTED WSAECONNABORTED
#endif
#define ioctl ioctlsocket
#define hstrerror(x) ""
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include <netdb.h>
#endif

#define APE_SOCKET_BACKLOG 511

/* get a ape_socket pointer from event returns */
#define APE_SOCKET(attach) ((ape_socket *)attach)

#ifdef _HAVE_SSL_SUPPORT  
  #define APE_SOCKET_ISSECURE(socket) socket->SSL.issecure
#else
  #define APE_SOCKET_ISSECURE(socket) 0
#endif

#ifdef __WIN32
struct iovec
{
  unsigned long iov_len;
  char*         iov_base;
};

#endif

/* TODO: TCP_NOPUSH  */

#ifdef TCP_CORK
    #define PACK_TCP(fd) \
    do { \
        int __state = 1; \
        setsockopt(fd, IPPROTO_TCP, TCP_CORK, &__state, sizeof(__state)); \
    } while(0)

    #define FLUSH_TCP(fd) \
    do { \
        int __state = 0; \
        setsockopt(fd, IPPROTO_TCP, TCP_CORK, &__state, sizeof(__state)); \
    } while(0)
#else
    #define PACK_TCP(fd)
    #define FLUSH_TCP(fd)
#endif


enum ape_socket_flags {
    APE_SOCKET_WOULD_BLOCK  = (1 << 0),
    APE_SOCKET_CORK         = (1 << 1)
};

enum ape_socket_proto {
    APE_SOCKET_PT_TCP,
    APE_SOCKET_PT_UDP,
	APE_SOCKET_PT_SSL
};

enum ape_socket_type {
    APE_SOCKET_TP_UNKNOWN,
    APE_SOCKET_TP_SERVER,
    APE_SOCKET_TP_CLIENT
};

enum ape_socket_state {
    APE_SOCKET_ST_ONLINE,
    APE_SOCKET_ST_PROGRESS,
    APE_SOCKET_ST_PENDING,
    APE_SOCKET_ST_OFFLINE
};

typedef enum _ape_socket_data_autorelease {
	APE_DATA_STATIC,
	APE_DATA_GLOBAL_STATIC,
	APE_DATA_AUTORELEASE,
	APE_DATA_OWN,
	APE_DATA_COPY
} ape_socket_data_autorelease;

typedef struct _ape_socket ape_socket;


typedef struct {
    void (*on_read)         (ape_socket *, ape_global *, void *arg);
    void (*on_disconnect)   (ape_socket *, ape_global *, void *arg);
    void (*on_connect)      (ape_socket *, ape_socket *, ape_global *, void *arg);
    void (*on_connected)    (ape_socket *, ape_global *, void *arg);
    void *arg;
} ape_socket_callbacks;


/* Jobs pool */
/* (1 << 0) is reserved */
#define APE_SOCKET_JOB_WRITEV   (1 << 1)
#define APE_SOCKET_JOB_SENDFILE (1 << 2)
#define APE_SOCKET_JOB_SHUTDOWN (1 << 3)
#define APE_SOCKET_JOB_ACTIVE   (1 << 4)
#define APE_SOCKET_JOB_IOV      (1 << 5)

typedef struct _ape_socket_jobs_t {
    union {
        void *data;
        int fd;
        buffer *buf;
    } ptr; /* public */
    struct _ape_pool *next;
    uint32_t flags;
	off_t offset;
} ape_socket_jobs_t;


struct _ape_socket {
    ape_fds s;
	
    buffer data_in;
	
    ape_pool_list_t jobs;

    void *ctx;  /* public pointer */
    void *_ctx; /* internal public pointer */
    ape_global *ape;
    ape_socket *parent; /* server socket in case of client */

    ape_socket_callbacks    callbacks;

    struct _ape_dns_cb_argv *dns_state;

    struct {
        uint8_t flags;
        uint8_t proto;
        uint8_t type;
        uint8_t state;
    } states;
#ifdef _HAVE_SSL_SUPPORT  	
	struct {
		uint8_t issecure;
		struct _ape_ssl *ssl;
	} SSL;
#endif
    uint16_t    remote_port;
};

#define APE_SOCKET_FD(socket) socket->s.fd

#define APE_SOCKET_PACKET_FREE (1 << 1)

struct _ape_socket_packet {
    /* inherit from ape_pool_t (same first sizeof(ape_pool_t) bytes memory-print) */
    ape_pool_t pool;
    size_t len;
    size_t offset;
	ape_socket_data_autorelease data_type;
} typedef ape_socket_packet_t;

#ifdef __cplusplus
extern "C" {
#endif

ape_socket *APE_socket_new(uint8_t pt, int from, ape_global *ape);

int APE_socket_listen(ape_socket *socket, uint16_t port,
        const char *local_ip);
int APE_socket_connect(ape_socket *socket, uint16_t port,
        const char *remote_ip_host);
int APE_socket_write(ape_socket *socket, void *data,
	size_t len, ape_socket_data_autorelease data_type);
int APE_socket_destroy(ape_socket *socket);
void APE_socket_shutdown(ape_socket *socket);
void APE_socket_shutdown_now(ape_socket *socket);
int APE_sendfile(ape_socket *socket, const char *file);
int ape_socket_do_jobs(ape_socket *socket);
int ape_socket_accept(ape_socket *socket);
int ape_socket_read(ape_socket *socket);
void ape_socket_connected(ape_socket *socket);

#ifdef __cplusplus
}
#endif

/*int ape_socket_write_file(ape_socket *socket, const char *file,
        ape_global *ape);
*/
#endif

// vim: ts=4 sts=4 sw=4 et

