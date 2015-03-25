/*****************************************************************************
 * directory.c : Use access readdir to output folder content to playlist
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Julien 'Lta' BALLET <contact # lta . io >
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include "playlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux );


int Import_Dir ( vlc_object_t *p_this)
{
    demux_t  *p_demux = (demux_t *)p_this;

    bool b_is_dir = false;
    int i_err = stream_Control( p_demux->s, STREAM_IS_DIRECTORY, &b_is_dir );

    if ( !( i_err == VLC_SUCCESS && b_is_dir ) )
        return VLC_EGENERIC;

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;
    msg_Dbg( p_demux, "reading directory content" );

    return VLC_SUCCESS;
}

void Close_Dir ( vlc_object_t *p_this )
{
    VLC_UNUSED(p_this);
}

static int Demux( demux_t *p_demux )
{
    input_item_t *p_input = GetCurrentItem(p_demux);
    input_item_node_t *p_node = input_item_node_Create( p_input );
    input_item_Release(p_input);

    if( stream_ReadDir( p_demux->s, p_node ) )
    {
        msg_Warn( p_demux, "unable to read directory" );
        input_item_node_Delete( p_node );
        return VLC_EGENERIC;
    }

    input_item_node_PostAndDelete( p_node );
    return VLC_SUCCESS;
}
