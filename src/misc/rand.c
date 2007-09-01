/*****************************************************************************
 * rand.c : non-predictible random bytes generator
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
 * $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <vlc/vlc.h>
#include <vlc_rand.h>

#ifndef WIN32
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <vlc_md5.h>

/*
 * Pseudo-random number generator using a HMAC-MD5 in counter mode.
 * Probably not very secure (expert patches welcome) but definitely
 * better than rand() which is defined to be reproducible...
 */
#define BLOCK_SIZE 64

static uint8_t okey[BLOCK_SIZE], ikey[BLOCK_SIZE];

static void vlc_rand_init (void)
{
#if defined (__OpenBSD__) || defined (__OpenBSD_kernel__)
    static const char randfile[] = "/dev/random";
#else
    static const char randfile[] = "/dev/urandom";
#endif
    uint8_t key[BLOCK_SIZE];

    /* Get non-predictible value as key for HMAC */
    int fd = open (randfile, O_RDONLY);
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

    close (fd);
}


void vlc_rand_bytes (void *buf, size_t len)
{
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    static uint64_t counter = 0;

    uint64_t stamp = NTPtime64 ();

    while (len > 0)
    {
        uint64_t val;
        struct md5_s mdi, mdo;

        pthread_mutex_lock (&lock);
        if (counter == 0)
            vlc_rand_init ();
        val = counter++;
        pthread_mutex_unlock (&lock);

        InitMD5 (&mdi);
        AddMD5 (&mdi, ikey, sizeof (ikey));
        AddMD5 (&mdi, &stamp, sizeof (stamp));
        AddMD5 (&mdi, &val, sizeof (val));
        EndMD5 (&mdi);
        InitMD5 (&mdo);
        AddMD5 (&mdo, okey, sizeof (okey));
        AddMD5 (&mdo, mdi.p_digest, sizeof (mdi.p_digest));
        EndMD5 (&mdo);

        if (len < sizeof (mdo.p_digest))
        {
            memcpy (buf, mdo.p_digest, len);
            break;
        }

        memcpy (buf, mdo.p_digest, sizeof (mdo.p_digest));
        len -= sizeof (mdo.p_digest);
        buf = ((uint8_t *)buf) + sizeof (mdo.p_digest);
    }
}

#else /* WIN32 */
/* It would be better to use rand_s() instead of rand() but it's not possible 
 * while we support Win 2OOO and until it gets included in mingw */
/* #define _CRT_RAND_S*/
#include <stdlib.h>

void vlc_rand_bytes (void *buf, size_t len)
{
    while (len > 0)
    {
        unsigned int val;
        /*rand_s (&val);*/
        val = rand();

        if (len < sizeof (val))
        {
            memcpy (buf, &val, len);
            break;
        }

        memcpy (buf, &val, sizeof (val));
        len -= sizeof (val);
    }
}
#endif
