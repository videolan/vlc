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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_input.h>
#include <vlc_plugin.h>

static int Demux( demux_t *p_demux )
{
    input_item_t *p_input = input_GetItem( p_demux->p_input );
    input_item_node_t *p_node = input_item_node_Create( p_input );

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

    if( vlc_stream_Control( p_demux->s, STREAM_IS_DIRECTORY ) )
        return VLC_EGENERIC;

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    return VLC_SUCCESS;
}

static const char *const psz_recursive_list[] = {
    "none", "collapse", "expand" };
static const char *const psz_recursive_list_text[] = {
    N_("None"), N_("Collapse"), N_("Expand") };

#define RECURSIVE_TEXT N_("Subdirectory behavior")
#define RECURSIVE_LONGTEXT N_( \
        "Select whether subdirectories must be expanded.\n" \
        "none: subdirectories do not appear in the playlist.\n" \
        "collapse: subdirectories appear but are expanded on first play.\n" \
        "expand: all subdirectories are expanded.\n" )

#define IGNORE_TEXT N_("Ignored extensions")
#define IGNORE_LONGTEXT N_( \
        "Files with these extensions will not be added to playlist when " \
        "opening a directory.\n" \
        "This is useful if you add directories that contain playlist files " \
        "for instance. Use a comma-separated list of extensions." )

#define SHOW_HIDDENFILES_TEXT N_("Show hidden files")
#define SHOW_HIDDENFILES_LONGTEXT N_( \
        "Ignore files starting with '.'" )

vlc_module_begin()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )

    set_shortname( N_("Directory") )
    set_description( N_("Directory import") )
    add_shortcut( "directory" )
    set_capability( "demux", 10 )
    set_callbacks( Import_Dir, NULL )

    add_string( "recursive", "collapse" , RECURSIVE_TEXT,
                RECURSIVE_LONGTEXT, false )
        change_string_list( psz_recursive_list, psz_recursive_list_text )
    add_string( "ignore-filetypes", "m3u,db,nfo,ini,jpg,jpeg,ljpg,gif,png,pgm,"
                "pgmyuv,pbm,pam,tga,bmp,pnm,xpm,xcf,pcx,tif,tiff,lbm,sfv,txt,"
                "sub,idx,srt,cue,ssa",
                IGNORE_TEXT, IGNORE_LONGTEXT, false )
    add_bool( "show-hiddenfiles", false,
              SHOW_HIDDENFILES_TEXT, SHOW_HIDDENFILES_LONGTEXT, false )
vlc_module_end()
