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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
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
#include <errno.h>

#undef NDEBUG
#include <assert.h>


int main (void)
{
    static const char key[] =
        "123456789ABCDEF0" "123456789ABCDEF0";
    static const char salt[] =
        "1234567890" "1234567890" "12345678";
    int val;
    srtp_session_t *sd, *se;

    /* Too big tag length */
    se = srtp_create (SRTP_ENCR_AES_CM, SRTP_AUTH_HMAC_SHA1, 21,
                      SRTP_PRF_AES_CM, 0);
    assert (se == NULL);

    /* Too short tag length */
    se = srtp_create (SRTP_ENCR_AES_CM, SRTP_AUTH_HMAC_SHA1, 3,
                      SRTP_PRF_AES_CM, SRTP_RCC_MODE1);
    assert (se == NULL);

    /* Initializes encryption and decryption contexts */
    se = srtp_create (SRTP_ENCR_AES_CM, SRTP_AUTH_HMAC_SHA1, 20,
                      SRTP_PRF_AES_CM, SRTP_RCC_MODE1);
    assert (se != NULL);

    sd = srtp_create (SRTP_ENCR_AES_CM, SRTP_AUTH_HMAC_SHA1, 20,
                      SRTP_PRF_AES_CM, SRTP_RCC_MODE1);
    assert (sd != NULL);

    srtp_setrcc_rate (se, 1);
    srtp_setrcc_rate (sd, 1);

    val = srtp_setkeystring (se, key, salt);
    assert (val == 0);
    val = srtp_setkeystring (sd, key, salt);
    assert (val == 0);

    uint8_t buf[1500], buf2[1500];
    size_t len;

    /* Invalid SRTP packet */
    len = 12;
    memset (buf, 0, len);
    val = srtp_send (se, buf, &len, sizeof (buf));
    assert (val == EINVAL);

    len = 32;
    memset (buf, 0, len);
    srtp_recv (sd, buf, &len);
    assert (val == EINVAL);

    /* Too short packet */
    len = 11;
    buf[0] = 0x80;
    val = srtp_send (se, buf, &len, sizeof (buf));
    assert (val == EINVAL);

    len = 11;
    val = srtp_recv (sd, buf, &len);
    assert (val == EINVAL);

    /* Too short when taking tag into account */
    len = 31;
    val = srtp_recv (sd, buf, &len);
    assert (val == EINVAL);

    /* Too short when taking RTP extensions into account */
    len = 15;
    buf[0] = 0x90;
    val = srtp_send (se, buf, &len, sizeof (buf));
    assert (val == EINVAL);

    len = 16;
    buf[0] = 0x90;
    buf[15] = 1;
    val = srtp_send (se, buf, &len, sizeof (buf));
    assert (val == EINVAL);

    /* Too small buffer (seq=1) */
    len = 20;
    memset (buf, 0, len);
    buf[0] = 0x80;
    buf[3] = 1;
    val = srtp_send (se, buf, &len, 39);
    assert (val == ENOSPC);
    assert (len == 40);

    len = 31;
    val = srtp_recv (sd, buf, &len);
    assert (val == EINVAL);

    /* OK (seq=3) */
    buf[0] = 0x80;
    buf[3] = 3;
    for (unsigned i = 0; i < 256; i++)
        buf[i + 12] = i;
    len = 0x10c;
    val = srtp_send (se, buf, &len, 0x120);
    assert (val == 0);
    assert (len == 0x120);

    memcpy (buf2, buf, len);
    val = srtp_recv (sd, buf2, &len);
    assert (val == 0);
    assert (len == 0x10c);
    assert (!memcmp (buf2, "\x80\x00\x00\x03" "\x00\x00\x00\x00"
                           "\x00\x00\x00\x00", 12));
    for (unsigned i = 0; i < 256; i++)
        assert (buf2[i + 12] == i); // test actual decryption

    /* Replay attack (seq=3) */
    len = 0x120;
    val = srtp_recv (sd, buf, &len);
    assert (val == EACCES);
    assert (len == 0x10c);

    /* OK but late (seq=2) */
    buf[0] = 0x80;
    buf[3] = 2;
    val = srtp_send (se, buf, &len, 0x120);
    assert (val == 0);
    assert (len == 0x120);

    memcpy (buf2, buf, len);
    val = srtp_recv (sd, buf2, &len);
    assert (val == 0);
    assert (len == 0x10c);

    /* Late replay attack (seq=3) */
    len = 0x120;
    val = srtp_recv (sd, buf, &len);
    assert (val == EACCES);
    assert (len == 0x10c);

    srtp_destroy (se);
    srtp_destroy (sd);
    return 0;
}
