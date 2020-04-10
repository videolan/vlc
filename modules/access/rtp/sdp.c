/**
 * @file sdp.c
 * @brief Real-Time Protocol (RTP) demux module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2020 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "sdp.h"
#include <vlc_common.h>

static void vlc_sdp_conn_free(struct vlc_sdp_conn **conn)
{
    struct vlc_sdp_conn *c = *conn;

    *conn = c->next;
    free(c);
}

static struct vlc_sdp_conn *vlc_sdp_conn_parse(const char *str, size_t len)
{
    const char *end = str + len;
    const char *net_type = str;
    const char *addr_type = memchr(str, ' ', len);

    if (addr_type == NULL) {
bad:
        errno = EINVAL;
        return NULL;
    }

    addr_type++; /* skip white space */
    const char *addr = memchr(addr_type, ' ', end - addr_type);

    if (addr == NULL)
        goto bad;

    addr++; /* skip white space */

    if (memchr(addr, ' ', end - addr) != NULL)
        goto bad;

    size_t addrlen = end - addr;
    struct vlc_sdp_conn *c = malloc(sizeof (*c) + addrlen + 1);
    if (unlikely(c == NULL))
        return NULL;

    c->next = NULL;
    c->family = 0;
    c->ttl = 255;
    c->addr_count = 1;
    memcpy(c->addr, addr, addrlen);
    c->addr[addrlen] = '\0';

    if (len >= 7 && memcmp(net_type, "IN ", 3) == 0) {
        int offset, val = -1;

        if (memcmp(addr_type, "IP4 ", 4) == 0) {
            /* IPv4 */
            c->family = 4;
            val = sscanf(c->addr, "%*[^/]%n/%hhu/%hu", &offset, &c->ttl,
                         &c->addr_count);

        } else if (memcmp(addr_type, "IP6 ", 4) == 0) {
            /* IPv6 */
            c->family = 6;
            val = sscanf(c->addr, "%*[^/]%n/%hu", &offset, &c->addr_count);
        }

        if (val >= 0)
            c->addr[offset] = '\0';
    }

    return c;
}

static struct vlc_sdp_attr *vlc_sdp_attr_parse(const char *str, size_t len)
{
    struct vlc_sdp_attr *a = malloc(sizeof (*a) + len + 1);
    const char *sep = memchr(str, ':', len);
    size_t namelen = (sep != NULL) ? (size_t)(sep - str) : len;

    if (unlikely(a == NULL))
        return NULL;

    memcpy(a->name, str, len);
    a->name[namelen] = '\0';
    a->name[len] = '\0';
    a->value = (sep != NULL) ? a->name + namelen + 1 : NULL;
    a->next = NULL;
    return a;
}

static void vlc_sdp_attr_free(struct vlc_sdp_attr **attr)
{
    struct vlc_sdp_attr *a = *attr;

    *attr = a->next;
    free(a);
}

static void vlc_sdp_media_free(struct vlc_sdp_media **media)
{
    struct vlc_sdp_media *m = *media;

    while (m->conns != NULL)
        vlc_sdp_conn_free(&m->conns);
    while (m->attrs != NULL)
        vlc_sdp_attr_free(&m->attrs);

    *media = m->next;
    free(m->format);
    free(m->proto);
    free(m->type);
    free(m);
}

static struct vlc_sdp_media *vlc_sdp_media_parse(struct vlc_sdp *sdp,
                                                 const char *str, size_t len)
{
    const char *end = str + len;
    const char *media = str;
    const char *media_end = memchr(str, ' ', end - str);

    if (media_end == NULL) {
bad:
        errno = EINVAL;
        return NULL;
    }

    const char *port = media_end + 1;
    char *port_end = memchr(port, ' ', end - port);

    if (port_end == NULL)
        goto bad;

    const char *proto = port_end + 1;
    unsigned long port_start = strtoul(port, &port_end, 10);
    unsigned long port_count = 1;

    if (*port_end == '/')
        port_count = strtoul(port_end + 1, &port_end, 10);
    if (*port_end != ' ')
        goto bad;

    const char *proto_end = memchr(proto, ' ', end - proto);

    if (proto_end == NULL)
        goto bad;

    const char *format = proto_end + 1;

    if (format >= end)
        goto bad;

    struct vlc_sdp_media *m = malloc(sizeof (*m));
    if (unlikely(m == NULL))
        return NULL;

    m->next = NULL;
    m->session = sdp;
    m->conns = NULL;
    m->attrs = NULL;
    m->type = strndup(media, media_end - media);
    m->port = port_start;
    m->port_count = port_count;
    m->proto = strndup(proto, proto_end - proto);
    m->format = strndup(format, end - format);

    if (unlikely(m->type == NULL || m->proto == NULL || m->format == NULL))
        vlc_sdp_media_free(&m);

    return m;
}

struct vlc_sdp_input
{
    const char *cursor;
    const char *end;
};

static int vlc_sdp_getline(struct vlc_sdp_input *restrict in,
                           const char **restrict pp, size_t *restrict lenp)
{
    assert(in->end >= in->cursor);
    *lenp = 0;

    if (in->end == in->cursor)
        return 0; /* end */

    const char *lf = memchr(in->cursor, '\n', in->end - in->cursor);

    if (lf == NULL)
        goto error; /* cannot locate end of line */

    const char *end = memchr(in->cursor, '\r', lf - in->cursor);
    if (end != NULL) {
        /* CR should be present. If so, it must be right before LF. */
        if (end != lf - 1)
            goto error; /* CR within a line is not permitted. */
    } else
        end = lf;

    if ((end - in->cursor) < 2 || in->cursor[1] != '=')
        goto error;

    int c = (unsigned char)in->cursor[0];

    *pp = in->cursor + 2;
    *lenp = end - *pp;
    in->cursor = lf + 1;

    return c;

error:
    errno = EINVAL;
    return -1;
}

const struct vlc_sdp_attr *vlc_sdp_attr_first_by_name(
    struct vlc_sdp_attr *const *ap, const char *name)
{
    for (const struct vlc_sdp_attr *a = *ap; a != NULL; a = a->next)
        if (!strcmp(a->name, name))
            return a;

    return NULL;
}

void vlc_sdp_free(struct vlc_sdp *sdp)
{
    while (sdp->media != NULL)
        vlc_sdp_media_free(&sdp->media);

    while (sdp->attrs != NULL)
        vlc_sdp_attr_free(&sdp->attrs);

    if (sdp->conn != NULL)
        vlc_sdp_conn_free(&sdp->conn);

    free(sdp->info);
    free(sdp->name);
    free(sdp);
}

struct vlc_sdp *vlc_sdp_parse(const char *str, size_t length)
{
    if (memchr(str, 0, length) != NULL) {
        /* Nul byte inside the SDP is not permitted. */
        errno = EINVAL;
        return NULL;
    }

    struct vlc_sdp_input in = { str, str + length };
    const char *line;
    size_t linelen;
    int c;

    /* Version line, must be "0" */
    if (vlc_sdp_getline(&in, &line, &linelen) != 'v'
     || linelen != 1 || memcmp(line, "0", 1)) {
        errno = EINVAL;
        return NULL;
    }

    /* Origin line (ignored for now) */
    if (vlc_sdp_getline(&in, &line, &linelen) != 'o') {
        errno = EINVAL;
        return NULL;
    }

    struct vlc_sdp *sdp = malloc(sizeof (*sdp));
    if (unlikely(sdp == NULL))
        return NULL;

    sdp->name = NULL;
    sdp->info = NULL;
    sdp->conn = NULL;
    sdp->attrs = NULL;
    sdp->media = NULL;

    /* Session name line */
    if (vlc_sdp_getline(&in, &line, &linelen) != 's')
        goto bad;

    sdp->name = strndup(line, linelen);
    if (unlikely(sdp->name == NULL))
        goto error;

    c = vlc_sdp_getline(&in, &line, &linelen);

    /* Session information line (optional) */
    if (c == 'i') {
        sdp->info = strndup(line, linelen);
        if (unlikely(sdp->info == NULL))
            goto error;

        c = vlc_sdp_getline(&in, &line, &linelen);
    }

    /* URL line (optional) */
    if (c == 'u')
        c = vlc_sdp_getline(&in, &line, &linelen);

    /* Email lines */
    while (c == 'e')
        c = vlc_sdp_getline(&in, &line, &linelen);

    /* Phone number lines */
    while (c == 'p')
        c = vlc_sdp_getline(&in, &line, &linelen);

    /* Session connection line (optional) */
    if (c == 'c') {
        sdp->conn = vlc_sdp_conn_parse(line, linelen);
        if (sdp->conn == NULL)
            goto error;

        c = vlc_sdp_getline(&in, &line, &linelen);
    }

    /* Session bandwidth lines */
    while (c == 'b')
        c = vlc_sdp_getline(&in, &line, &linelen);

    /* Time descriptions / Session time lines */
    while (c == 't') {
        c = vlc_sdp_getline(&in, &line, &linelen);

        /* Repeat lines */
        while (c == 'r')
            c = vlc_sdp_getline(&in, &line, &linelen);
    }

    /* Time adjustment lines */
    while (c == 'z')
        c = vlc_sdp_getline(&in, &line, &linelen);

    /* Session encryption key line (unused in real life) */
    if (c == 'k')
        c = vlc_sdp_getline(&in, &line, &linelen);

    /* Session attribute lines */
    for (struct vlc_sdp_attr **ap = &sdp->attrs; c == 'a';) {
        struct vlc_sdp_attr *a = vlc_sdp_attr_parse(line, linelen);
        if (a == NULL)
            goto error;

        *ap = a;
        ap = &a->next;
        c = vlc_sdp_getline(&in, &line, &linelen);
    }

    /* Media descriptions / Media lines */
    for (struct vlc_sdp_media **mp = &sdp->media; c == 'm';) {
        struct vlc_sdp_media *m = vlc_sdp_media_parse(sdp, line, linelen);
        if (m == NULL)
            goto error;

        *mp = m;
        mp = &m->next;
        c = vlc_sdp_getline(&in, &line, &linelen);

        /* Media title line */
        if (c == 'i')
            c = vlc_sdp_getline(&in, &line, &linelen);

        /* Media connection lines */
        for (struct vlc_sdp_conn **cp = &m->conns; c == 'c';) {
             struct vlc_sdp_conn *conn = vlc_sdp_conn_parse(line, linelen);
             if (conn == NULL)
                 goto error;

             *cp = conn;
             cp = &conn->next;
             c = vlc_sdp_getline(&in, &line, &linelen);
        }

        /* Media bandwidth lines */
        while (c == 'b')
            c = vlc_sdp_getline(&in, &line, &linelen);

        /* Media encryption key line (unused in real life) */
        if (c == 'k')
            c = vlc_sdp_getline(&in, &line, &linelen);

        /* Session attribute lines */
        for (struct vlc_sdp_attr **ap = &m->attrs; c == 'a';) {
            struct vlc_sdp_attr *a = vlc_sdp_attr_parse(line, linelen);
            if (a == NULL)
                goto error;

            *ap = a;
            ap = &a->next;
            c = vlc_sdp_getline(&in, &line, &linelen);
        }
    }

    if (c == 0)
        return sdp;

bad:
    errno = EINVAL;
error:
    vlc_sdp_free(sdp);
    return NULL;
}
