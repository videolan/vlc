/*****************************************************************************
 * inhibit.c: screen saver inhibition
 *****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
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
#include <vlc_modules.h>
#include "inhibit.h"
#include <libvlc.h>
#include <assert.h>

typedef struct
{
    vlc_inhibit_t ih;
    module_t *module;
} inhibit_t;

vlc_inhibit_t *vlc_inhibit_Create (vlc_object_t *parent)
{
    inhibit_t *priv = vlc_custom_create (parent, sizeof (*priv), "inhibit" );
    if (priv == NULL)
        return NULL;

    vlc_inhibit_t *ih = &priv->ih;
    ih->p_sys = NULL;
    ih->inhibit = NULL;

    priv->module = module_need (ih, "inhibit", NULL, false);
    if (priv->module == NULL)
    {
        vlc_object_release (ih);
        ih = NULL;
    }
    return ih;
}

void vlc_inhibit_Destroy (vlc_inhibit_t *ih)
{
    assert (ih != NULL);

    module_unneed (ih, ((inhibit_t *)ih)->module);
    vlc_object_release (ih);
}
