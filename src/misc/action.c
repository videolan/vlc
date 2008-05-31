/*****************************************************************************
 * action.c: key to action mapping
 *****************************************************************************
 * Copyright © 2008 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "../libvlc.h"

int vlc_key_to_action (vlc_object_t *libvlc, const char *varname,
                       vlc_value_t prevkey, vlc_value_t curkey, void *priv)
{
    const struct hotkey *key = priv;

    (void)varname;
    (void)prevkey;

    while (key->i_key != curkey.i_int)
    {
        if (key->psz_action == NULL)
            return VLC_SUCCESS; /* key is not mapped to anything */

        key++;
    }

    return var_SetInteger (libvlc, "key-action", key->i_action);
}

