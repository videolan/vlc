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

#if VLC_WINSTORE_APP
# include <bcrypt.h>
#else
# include <wincrypt.h>
#endif

void vlc_rand_bytes (void *buf, size_t len)
{
#if VLC_WINSTORE_APP
    BCRYPT_ALG_HANDLE algo_handle;
    NTSTATUS ret = BCryptOpenAlgorithmProvider(&algo_handle, BCRYPT_RNG_ALGORITHM,
                                               MS_PRIMITIVE_PROVIDER, 0);
    if (BCRYPT_SUCCESS(ret))
    {
        BCryptGenRandom(algo_handle, buf, len, 0);
        BCryptCloseAlgorithmProvider(algo_handle, 0);
    }
#else
    size_t count = len;
    uint8_t *p_buf = (uint8_t *)buf;

    /* fill buffer with pseudo-random data */
    while (count > 0)
    {
        unsigned int val;
        val = rand();
        if (count < sizeof (val))
        {
            memcpy (p_buf, &val, count);
            break;
        }

        memcpy (p_buf, &val, sizeof (val));
        count -= sizeof (val);
        p_buf += sizeof (val);
    }

    HCRYPTPROV hProv;
    /* acquire default encryption context */
    if( CryptAcquireContext(
        &hProv,                 // Variable to hold returned handle.
        NULL,                   // Use default key container.
        MS_DEF_PROV,            // Use default CSP.
        PROV_RSA_FULL,          // Type of provider to acquire.
        CRYPT_VERIFYCONTEXT) )  // Flag values
    {
        /* fill buffer with pseudo-random data, initial buffer content
           is used as auxiliary random seed */
        CryptGenRandom(hProv, len, buf);
        CryptReleaseContext(hProv, 0);
    }
#endif /* VLC_WINSTORE_APP */
}
