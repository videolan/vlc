/*
 * Secure RTP with libgcrypt
 * Copyright (C) 2007  Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdint.h>
#include <stddef.h>
#include "srtp.h"

#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <netinet/in.h>

int main (void)
{
    int fd = socket (AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    memset (&addr, 0, sizeof (addr));
    addr.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
    addr.sin_len = sizeof (addr);
#endif
    addr.sin_port = htons (10000);
    addr.sin_addr.s_addr = htonl (0x7f000001);
    if (bind (fd, (struct sockaddr *)&addr, sizeof (addr)))
        return 1;

    static const uint8_t key[16] =
        "\x12\x34\x56\x78\x9A\xBC\xDE\xF0"
        "\x12\x34\x56\x78\x9A\xBC\xDE\xF0";
    static const uint8_t salt[14] =
        "\x12\x34\x56\x78\x90" "\x12\x34\x56\x78\x90" "\x12\x34\x56\x78";

    srtp_session_t *s = srtp_create (SRTP_ENCR_AES_CM, SRTP_AUTH_HMAC_SHA1, 10,
                                     SRTP_PRF_AES_CM, 0);
    if (s == NULL)
        return 1;
    if (srtp_setkey (s, key, 16, salt, 14))
        goto error;

    uint8_t buf[1500];
    size_t len;
#if 0
    memset (buf, 0, sizeof (buf));
    buf[0] = 2 << 6;
    buf[1] = 1;
    memcpy (buf + 2, &(uint16_t){ htons (9527) }, 2);
    memcpy (buf + 8, "\xde\xad\xbe\xef", 4);
    memcpy (buf + 4, &(uint32_t){ htonl (1) }, 4);
    strcpy ((char *)buf + 12, "a\n");
    len = 12 + strlen ((char *)buf + 12) + 1;
#endif
    for (;;)
    {
        len = read (fd, buf, sizeof (buf));
        int val = srtp_recv (s, buf, &len);
        if (val)
        {
            fprintf (stderr, "Cannot decrypt: %s\n", strerror (val));
            continue;
        }

        puts ((char *)buf + 12);
        //if (srtp_send (s, buf, &len, sizeof (buf)) || srtp_recv (s, buf, &len))
        //    break;
        puts ((char *)buf + 12);
    }

error:
    srtp_destroy (s);
    close (fd);
    return 0;
}
