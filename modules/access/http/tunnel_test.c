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
#include <sys/socket.h>
#ifndef SOCK_CLOEXEC
# define SOCK_CLOEXEC 0
# define accept4(a,b,c,d) accept(a,b,c)
#endif
#include <netinet/in.h>
#include <arpa/inet.h>

#include <vlc_common.h>
#include <vlc_tls.h>
#include "transport.h"

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

    const char resp[] = "HTTP/1.1 500 Failure\r\n\r\n";

    val = write(fd, resp, strlen(resp));
    assert((size_t)val == strlen(resp));
    shutdown(fd, SHUT_WR);
}

static void *proxy_thread(void *data)
{
    int lfd = (intptr_t)data;

    for (;;)
    {
        int cfd = accept4(lfd, NULL, NULL, SOCK_CLOEXEC);
        if (cfd == -1)
            continue;

        int canc = vlc_savecancel();
        proxy_client_process(cfd);
        close(cfd);
        vlc_restorecancel(canc);
    }
    vlc_assert_unreachable();
}

int main(void)
{
    int lfd = socket(PF_INET6, SOCK_STREAM|SOCK_CLOEXEC, IPPROTO_TCP);
    if (lfd == -1)
        return 77;

    struct sockaddr_in6 addr = {
        .sin6_family = AF_INET6,
#ifdef HAVE_SA_LEN
        .sin6_len = sizeof (addr),
#endif
        .sin6_addr = in6addr_loopback,
    };
    socklen_t addrlen = sizeof (addr);

    if (bind(lfd, (struct sockaddr *)&addr, addrlen)
     || listen(lfd, 255))
    {
        close(lfd);
        return 77;
    }

    char *url;
    bool two;

    getsockname(lfd, &addr, &addrlen);
    if (asprintf(&url, "https://[::1]:%u", ntohs(addr.sin6_port)) < 0)
        url = NULL;
    assert(url != NULL);

    vlc_thread_t th;

    if (vlc_clone(&th, proxy_thread, (void*)(intptr_t)lfd,
                  VLC_THREAD_PRIORITY_LOW))
        assert(!"Thread error");

    vlc_https_connect_proxy(NULL, "www.example.com", 0, &two, url);

    vlc_cancel(th);
    vlc_join(th, NULL);
    free(url);
    close(lfd);
}
