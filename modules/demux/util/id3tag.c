/*****************************************************************************
 * id3tag.c: id3 tag parser/skipper based on libid3tag
 *****************************************************************************
 * Copyright (C) 2002-2004 VideoLAN
 * $Id$
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/input.h>

#include <sys/types.h>

#include "vlc_meta.h"

#include <id3tag.h>
#include "id3genres.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  ParseID3Tags ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
set_description( _("ID3 tag parser using libid3tag" ) );
set_capability( "id3", 70 );
set_callbacks( ParseID3Tags, NULL );
vlc_module_end();

/*****************************************************************************
 * Definitions of structures  and functions used by this plugins
 *****************************************************************************/

/*****************************************************************************
 * ParseID3Tag : parse an id3tag into the info structures
 *****************************************************************************/
static void ParseID3Tag( demux_t *p_demux, uint8_t *p_data, int i_size )
{
    struct id3_tag   *p_id3_tag;
    struct id3_frame *p_frame;
    int i = 0;

    p_id3_tag = id3_tag_parse( p_data, i_size );
    if( !p_id3_tag ) return;

    while( ( p_frame = id3_tag_findframe( p_id3_tag , "T", i ) ) )
    {
        int i_strings = id3_field_getnstrings( &p_frame->fields[1] );

        while( i_strings > 0 )
        {
            char *psz_temp = id3_ucs4_utf8duplicate(
                id3_field_getstrings( &p_frame->fields[1], --i_strings ) );

            if( !strcmp( p_frame->id, ID3_FRAME_GENRE ) )
            {
                char *psz_endptr;
                int i_genre = strtol( psz_temp, &psz_endptr, 10 );

                if( psz_temp != psz_endptr &&
                    i_genre >= 0 && i_genre < NUM_GENRES )
                {
                    vlc_meta_Add( (vlc_meta_t *)p_demux->p_private,
                                  VLC_META_GENRE, ppsz_genres[atoi(psz_temp)]);
                }
                else
                {
                    /* Unknown genre */
                    vlc_meta_Add( (vlc_meta_t *)p_demux->p_private,
                                  VLC_META_GENRE, psz_temp );
                }
            }
            else if( !strcmp(p_frame->id, ID3_FRAME_TITLE ) )
            {
                vlc_meta_Add( (vlc_meta_t *)p_demux->p_private,
                              VLC_META_TITLE, psz_temp );
            }
            else if( !strcmp(p_frame->id, ID3_FRAME_ARTIST ) )
            {
                vlc_meta_Add( (vlc_meta_t *)p_demux->p_private,
                              VLC_META_ARTIST, psz_temp );
            }
            else
            {
                /* Unknown meta info */
                vlc_meta_Add( (vlc_meta_t *)p_demux->p_private,
                              (char *)p_frame->description, psz_temp );
            }
            free( psz_temp );
        }
        i++;
    }
    id3_tag_delete( p_id3_tag );
}

/*****************************************************************************
 * ParseID3Tags: check if ID3 tags at common locations. Parse them and skip it
 * if it's at the start of the file
 ****************************************************************************/
static int ParseID3Tags( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    uint8_t *p_peek;
    int i_size;
    int i_size2;
    vlc_bool_t b_seekable;

    p_demux->p_private = (void *)vlc_meta_New();

    msg_Dbg( p_demux, "checking for ID3 tag" );

    stream_Control( p_demux->s, STREAM_CAN_FASTSEEK, &b_seekable );
    if( b_seekable )
    {
        int64_t i_init;
        int64_t i_pos;

        /*look for a ID3v1 tag at the end of the file*/
        i_init = stream_Tell( p_demux->s );
        i_pos = stream_Size( p_demux->s );

        if ( i_pos >128 )
        {
            stream_Seek( p_demux->s, i_pos - 128 );

            /* get 10 byte id3 header */
            if( stream_Peek( p_demux->s, &p_peek, 10 ) < 10 )
            {
                msg_Err( p_demux, "cannot peek()" );
                return VLC_EGENERIC;
            }

            i_size2 = id3_tag_query( p_peek, 10 );
            if ( i_size2 == 128 )
            {
                /* peek the entire tag */
                if ( stream_Peek( p_demux->s, &p_peek, i_size2 ) < i_size2 )
                {
                    msg_Err( p_demux, "cannot peek()" );
                    return VLC_EGENERIC;
                }
                msg_Dbg( p_demux, "found ID3v1 tag" );
                ParseID3Tag( p_demux, p_peek, i_size2 );
            }

            /* look for ID3v2.4 tag at end of file */
            /* get 10 byte ID3 footer */
            if( stream_Peek( p_demux->s, &p_peek, 128 ) < 128 )
            {
                msg_Err( p_demux, "cannot peek()" );
                return VLC_EGENERIC;
            }
            i_size2 = id3_tag_query( p_peek + 118, 10 );
            if ( i_size2 < 0  && i_pos > -i_size2 )
            {                                        /* id3v2.4 footer found */
                stream_Seek( p_demux->s , i_pos + i_size2 );
                /* peek the entire tag */
                if ( stream_Peek( p_demux->s, &p_peek, i_size2 ) < i_size2 )
                {
                    msg_Err( p_demux, "cannot peek()" );
                    return VLC_EGENERIC;
                }
                msg_Dbg( p_demux, "found ID3v2 tag at end of file" );
                ParseID3Tag( p_demux, p_peek, i_size2 );
            }
        }
        stream_Seek( p_demux->s, i_init );
    }
    /* get 10 byte id3 header */
    if( stream_Peek( p_demux->s, &p_peek, 10 ) < 10 )
    {
        msg_Err( p_demux, "cannot peek()" );
        return VLC_EGENERIC;
    }

    i_size = id3_tag_query( p_peek, 10 );
    if ( i_size <= 0 )
    {
        return VLC_SUCCESS;
    }

    /* Read the entire tag */
    p_peek = malloc( i_size );
    if( !p_peek || stream_Read( p_demux->s, p_peek, i_size ) < i_size )
    {
        msg_Err( p_demux, "cannot read ID3 tag" );
        if( p_peek ) free( p_peek );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_demux, "found ID3v2 tag" );
    ParseID3Tag( p_demux, p_peek, i_size );

    free( p_peek );
    return VLC_SUCCESS;
}
