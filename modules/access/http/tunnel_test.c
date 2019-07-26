/*****************************************************************************
 * tunnel_test.c: HTTP CONNECT
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#undef NDEBUG

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifndef SOCK_CLOEXEC
# define SOCK_CLOEXEC 0
# define accept4(a,b,c,d) accept(a,b,c)
#endif
#ifdef _WIN32
# include <winsock2.h>
#else
# include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <vlc_common.h>
#include <vlc_tls.h>
#include "transport.h"

const char vlc_module_name[] = "test_http_tunnel";

static void proxy_client_process(int fd)
{
    char buf[1024];
    size_t buflen = 0;
    ssize_t val;

    /* Read request (and possibly more) */
    while (strnstr(buf, "\r\n\r\n", buflen) == NULL)
    {
        val = recv(fd, buf + buflen, sizeof (buf) - buflen - 1, 0);
        if (val <= 0)
            assert(!"Incomplete request");
        buflen += val;
    }

    buf[buflen] = '\0';

    char host[64];
    unsigned port, ver;
    int offset;

    assert(sscanf(buf, "CONNECT %63[^:]:%u HTTP/1.%1u%n", host, &port, &ver,
                  &offset) == 3);
    assert(!strcmp(host, "www.example.com"));
    assert(port == 443);
    assert(ver == 1);

    assert(sscanf(buf + offset + 2, "Host: %63[^:]:%u", host, &port) == 2);
    assert(!strcmp(host, "www.example.com"));
    assert(port == 443);

    assert(strstr(buf, "\r\nProxy-Authorization: Basic "
                  "QWxhZGRpbjpvcGVuIHNlc2FtZQ==\r\n") != NULL);

    const char resp[] = "HTTP/1.1 500 Failure\r\n\r\n";

    val = write(fd, resp, strlen(resp));
    assert((size_t)val == strlen(resp));
    shutdown(fd, SHUT_WR);
}

static unsigned connection_count = 0;

static void *proxy_thread(void *data)
{
    int *lfd = data;

    for (;;)
    {
        int cfd = accept4(*lfd, NULL, NULL, SOCK_CLOEXEC);
        if (cfd == -1)
            continue;

        int canc = vlc_savecancel();
        proxy_client_process(cfd);
        vlc_close(cfd);
        connection_count++;
        vlc_restorecancel(canc);
    }
    vlc_assert_unreachable();
}

static int server_socket(unsigned *port)
{
    int fd = socket(PF_INET6, SOCK_STREAM|SOCK_CLOEXEC, IPPROTO_TCP);
    if (fd == -1)
        return -1;

    struct sockaddr_in6 addr = {
        .sin6_family = AF_INET6,
#ifdef HAVE_SA_LEN
        .sin6_len = sizeof (addr),
#endif
        .sin6_addr = in6addr_loopback,
    };
    socklen_t addrlen = sizeof (addr);

    if (bind(fd, (struct sockaddr *)&addr, addrlen)
     || getsockname(fd, (struct sockaddr *)&addr, &addrlen))
    {
        vlc_close(fd);
        return -1;
    }

    *port = ntohs(addr.sin6_port);
    return fd;
}

int main(void)
{
    char *url;
    unsigned port;
    bool two = false;

    /* Test bad URLs */
    vlc_https_connect_proxy(NULL, NULL, "www.example.com", 0, &two,
                            "/test");
    vlc_https_connect_proxy(NULL, NULL, "www.example.com", 0, &two,
                            "ftp://proxy.example.com/");

    int *lfd = malloc(sizeof (int));
    assert(lfd != NULL);
    *lfd = server_socket(&port);
    if (*lfd == -1)
        return 77;

    if (asprintf(&url, "http://Aladdin:open%%20sesame@[::1]:%u", port) < 0)
        url = NULL;

    assert(url != NULL);

    /* Test connection failure */
    vlc_https_connect_proxy(NULL, NULL, "www.example.com", 0, &two, url);

    if (listen(*lfd, 255))
    {
        vlc_close(*lfd);
        return 77;
    }

    vlc_thread_t th;
    if (vlc_clone(&th, proxy_thread, lfd, VLC_THREAD_PRIORITY_LOW))
        assert(!"Thread error");

    /* Test proxy error */
    vlc_https_connect_proxy(NULL, NULL, "www.example.com", 0, &two, url);

    vlc_cancel(th);
    vlc_join(th, NULL);
    assert(connection_count > 0);
    free(url);
    vlc_close(*lfd);
    free(lfd);
    return 0;
}
