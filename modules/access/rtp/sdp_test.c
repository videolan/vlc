/**
 * @file sdp_test.c
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

#undef NDEBUG
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "sdp.h"
#include <vlc_common.h>

static void test_sdp_valid(const char *str)
{
    size_t len;

    for (len = 0; str[len] != '\0'; len++) {
        struct vlc_sdp *sdp = vlc_sdp_parse(str, len);

        if (sdp != NULL)
            vlc_sdp_free(sdp);
    }

    struct vlc_sdp *sdp = vlc_sdp_parse(str, len);

    assert(sdp != NULL);
    vlc_sdp_free(sdp);
}

static void test_sdp_invalid(const char *base, size_t len)
{
    bool ok = false;

    for (size_t i = 0; i <= len; i++) {
        struct vlc_sdp *sdp = vlc_sdp_parse(base, i);

        if (sdp != NULL)
            vlc_sdp_free(sdp);

        ok = sdp != NULL;
    }

    assert(!ok);
}

int main(void)
{
    static const char example[] =
        "v=0\r\n"
        "o=- 1 1 IN IP4 192.0.2.1\r\n"
        "s=Title here\r\n"
        "i=Description here\r\n"
        "u=https://www.example.com/\r\n"
        "e=postmaster@example.com (Postmaster)\r\n"
        "p=+1 800-555-0100\r\n"
        "c=IN IP4 239.255.0.1/127\r\n"
        "b=AS:100\r\n"
        "t=3155673600 3155673600\r\n"
        "r=604800 3600 0 86400\r\n"
        "k=prompt\r\n"
        "a=recvonly\r\n"
        "m=text 5004 RTP/AVP 96\r\n"
        "i=Media title here\r\n"
        "c=IN IP4 239.255.0.2/127\r\n"
        "c=IN IP6 ff03::1/2\r\n"
        "b=AS:100\r\n"
        "k=prompt\r\n"
        "a=rtpmap:96 t140/1000\r\n";

    test_sdp_valid(example);

    struct vlc_sdp *sdp = vlc_sdp_parse(example, strlen(example));
    assert(sdp != NULL);
    assert(vlc_sdp_attr_present(sdp, "recvonly"));
    assert(!vlc_sdp_attr_present(sdp, "sendrecv"));
    assert(vlc_sdp_attr_value(sdp, "recvonly") == NULL);

    const struct vlc_sdp_media *m = sdp->media;
    assert(m != NULL);
    assert(m->next == NULL);
    assert(!strcmp(m->type, "text"));
    assert(m->port == 5004);
    assert(m->port_count == 1);
    assert(!strcmp(m->proto, "RTP/AVP"));
    assert(!strcmp(m->format, "96"));
    assert(vlc_sdp_media_attr_present(m, "rtpmap"));
    assert(!strcmp(vlc_sdp_media_attr_value(m, "rtpmap"), "96 t140/1000"));

    const struct vlc_sdp_conn *c = vlc_sdp_media_conn(m);
    assert(c != NULL);
    assert(c->family == 4);
    assert(!strcmp(c->addr, "239.255.0.2"));
    assert(c->ttl == 127);
    assert(c->addr_count == 1);
    c = c->next;
    assert(c != NULL);
    assert(c->family == 6);
    assert(!strcmp(c->addr, "ff03::1"));
    assert(c->ttl == 255);
    assert(c->addr_count == 2);

    vlc_sdp_free(sdp);

    char smallest[] =
        "v=0\n"
        "o=- 0 0 x y z\n"
        "s=\n"
        "t=0 0\n";

    test_sdp_valid(smallest);

    smallest[10] = 0;
    test_sdp_invalid(smallest, sizeof (smallest) - 1);
    smallest[10] = '\r';
    test_sdp_invalid(smallest, sizeof (smallest) - 1);

    static const char *const bad[] = {
        "\r\n",
        "v=0\r\no=- 1 1 x y z\r\ns=-\r\nm=\r\n",
        "v=0\r\no=- 1 1 x y z\r\ns=-\r\nm=text \r\n",
        "v=0\r\no=- 1 1 x y z\r\ns=-\r\nm=text 0 \r\n",
        "v=0\r\no=- 1 1 x y z\r\ns=-\r\nm=text 0/1 \r\n",
        "v=0\r\no=- 1 1 x y z\r\ns=-\r\nm=video x udp mpeg\r\n",
        "v=0\r\no=- 1 1 x y z\r\ns=-\r\nm=video 0 udp \r\n",
        "v=0\r\no=- 1 1 x y z\r\ns=-\r\nc=IN\r\n",
        "v=0\r\no=- 1 1 x y z\r\ns=-\r\nc=IN IP4\r\n",
        "v=0\r\no=- 1 1 x y z\r\ns=-\r\nc=IN IP4 example.com x\r\n",
    };

    for (size_t i = 0; i < ARRAY_SIZE(bad); i++)
        test_sdp_invalid(bad[i], strlen(bad[i]));
    return 0;
}
