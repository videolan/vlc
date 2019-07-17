/*****************************************************************************
 * directory.c : Use access readdir to output folder content to playlist
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_input_item.h>
#include <vlc_plugin.h>

static int Demux( demux_t *p_demux )
{
    input_item_node_t *p_node = input_item_node_Create( p_demux->p_input_item );

    if( vlc_stream_ReadDir( p_demux->s, p_node ) )
    {
        msg_Warn( p_demux, "unable to read directory" );
        input_item_node_Delete( p_node );
        return VLC_EGENERIC;
    }

    if (es_out_Control(p_demux->out, ES_OUT_POST_SUBNODE, p_node))
        input_item_node_Delete(p_node);

    return VLC_SUCCESS;
}

static int Control(demux_t *demux, int query, va_list args)
{
    (void) demux;
    switch( query )
    {
        case DEMUX_IS_PLAYLIST:
        {
            bool *pb_bool = va_arg( args, bool * );
            *pb_bool = true;
            return VLC_SUCCESS;
        }
        case DEMUX_GET_META:
        {
            return vlc_stream_vaControl(demux->s, STREAM_GET_META, args);
        }
        case DEMUX_HAS_UNSUPPORTED_META:
        {
            *(va_arg( args, bool * )) = false;
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

static int Import_Dir( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    if( p_demux->s->pf_readdir == NULL )
        return VLC_EGENERIC;
    if( p_demux->p_input_item == NULL )
        return VLC_ETIMEOUT;

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_shortname( N_("Directory") )
    set_description( N_("Directory import") )
    add_shortcut( "directory" )
    set_capability( "demux", 10 )
    set_callback( Import_Dir )
vlc_module_end()
