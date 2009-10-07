/*****************************************************************************
 * keys.c: test for key support
 *****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define __LIBVLC__
#include "../config/keys.c"

int main (void)
{
    bool ok = true;

    /* Make sure keys are sorted properly, so that bsearch() works */
    for (size_t i = 1; i < vlc_num_keys; i++)
        if (vlc_keys[i].i_key_code < vlc_keys[i - 1].i_key_code)
        {
            fprintf (stderr,
                     "%s (%06"PRIx32") should be before %s (%06"PRIx32")\n",
                     vlc_keys[i].psz_key_string, vlc_keys[i].i_key_code,
                     vlc_keys[i - 1].psz_key_string,
                     vlc_keys[i - 1].i_key_code);
            ok = false;
        }

    return !ok;
}
