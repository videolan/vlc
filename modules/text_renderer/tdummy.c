/*****************************************************************************
 * tdummy.c : dummy text rendering functions
 *****************************************************************************
 * Copyright (C) 2000, 2001 VLC authors and VideoLAN
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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
#include <vlc_plugin.h>
#include <vlc_filter.h>

static int OpenRenderer( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("Dummy") )
    set_description( N_("Dummy font renderer") )
    set_capability( "text renderer", 1 )
    set_callback( OpenRenderer )
vlc_module_end ()


static int RenderText( filter_t *p_filter, subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in,
                       const vlc_fourcc_t *p_chroma_list )
{
    VLC_UNUSED(p_filter); VLC_UNUSED(p_region_out); VLC_UNUSED(p_region_in);
    VLC_UNUSED(p_chroma_list);
    return VLC_EGENERIC;
}

static int OpenRenderer( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    p_filter->pf_render = RenderText;
    return VLC_SUCCESS;
}
