/*****************************************************************************
 * rand.c : non-predictible random bytes generator
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_rand.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <vlc_fs.h>

#include <vlc_hash.h>

/*
 * Pseudo-random number generator using a HMAC-MD5 in counter mode.
 * Probably not very secure (expert patches welcome) but definitely
 * better than rand() which is defined to be reproducible...
 */
#define BLOCK_SIZE 64

static uint8_t okey[BLOCK_SIZE], ikey[BLOCK_SIZE];

static void vlc_rand_init (void)
{
    uint8_t key[BLOCK_SIZE];

    /* Get non-predictible value as key for HMAC */
    int fd = vlc_open ("/dev/urandom", O_RDONLY);
    if (fd == -1)
        return; /* Uho! */

    for (size_t i = 0; i < sizeof (key);)
    {
         ssize_t val = read (fd, key + i, sizeof (key) - i);
         if (val > 0)
             i += val;
    }

    /* Precompute outer and inner keys for HMAC */
    for (size_t i = 0; i < sizeof (key); i++)
    {
        okey[i] = key[i] ^ 0x5c;
        ikey[i] = key[i] ^ 0x36;
    }

    vlc_close (fd);
}


void vlc_rand_bytes (void *buf, size_t len)
{
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    static uint64_t counter = 0;

    uint64_t stamp = NTPtime64 ();

    while (len > 0)
    {
        uint64_t val;
        vlc_hash_md5_t mdi, mdo;
        uint8_t mdi_buf[VLC_HASH_MD5_DIGEST_SIZE];
        uint8_t mdo_buf[VLC_HASH_MD5_DIGEST_SIZE];

        vlc_hash_md5_Init (&mdi);
        vlc_hash_md5_Init (&mdo);

        pthread_mutex_lock (&lock);
        if (counter == 0)
            vlc_rand_init ();
        val = counter++;

        vlc_hash_md5_Update (&mdi, ikey, sizeof (ikey));
        vlc_hash_md5_Update (&mdo, okey, sizeof (okey));
        pthread_mutex_unlock (&lock);

        vlc_hash_md5_Update (&mdi, &stamp, sizeof (stamp));
        vlc_hash_md5_Update (&mdi, &val, sizeof (val));
        vlc_hash_md5_Finish (&mdi, mdi_buf, sizeof(mdi_buf));
        vlc_hash_md5_Update (&mdo, mdi_buf, sizeof(mdi_buf));
        vlc_hash_md5_Finish (&mdo, mdo_buf, sizeof(mdo_buf));

        memcpy (buf, mdo_buf, (len < sizeof(mdo_buf)) ? len : sizeof(mdo_buf));

        if (len < sizeof(mdo_buf))
            break;

        len -= 16;
        buf = ((uint8_t *)buf) + 16;
    }
}
