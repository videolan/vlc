/*****************************************************************************
 * id3tag.c: id3 tag parser/skipper based on libid3tag
 *****************************************************************************
 * Copyright (C) 2002-2004 VideoLAN
 * $Id: id3tag.c,v 1.19 2004/01/25 20:05:29 hartman Exp $
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

#include "ninput.h"

#include <sys/types.h>

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
static void ParseID3Tag( input_thread_t *p_input, uint8_t *p_data, int i_size )
{
    playlist_t            *p_playlist;
    struct id3_tag        *p_id3_tag;
    struct id3_frame      *p_frame;
    input_info_category_t *p_category;
    char                  *psz_temp;
    vlc_value_t val;
    int i;

    var_Get( p_input, "demuxed-id3", &val );
    if( val.b_bool )
    {
        msg_Dbg( p_input, "the ID3 tag was already parsed" );
        return;
    }

    p_playlist = vlc_object_find( p_input, VLC_OBJECT_PLAYLIST, FIND_PARENT );

    val.b_bool = VLC_FALSE;
    p_id3_tag = id3_tag_parse( p_data, i_size );
    p_category = input_InfoCategory( p_input, "ID3" );
    i = 0;

    while ( ( p_frame = id3_tag_findframe( p_id3_tag , "T", i ) ) )
    {
        playlist_item_t *p_item = p_playlist ? p_playlist->pp_items[p_playlist->i_index] : NULL;

        int i_strings = id3_field_getnstrings( &p_frame->fields[1] );

        while ( i_strings > 0 )
        {
            psz_temp = id3_ucs4_utf8duplicate( id3_field_getstrings( &p_frame->fields[1], --i_strings ) );
            if ( !strcmp(p_frame->id, ID3_FRAME_GENRE ) )
            {
                int i_genre;
                char *psz_endptr;
                i_genre = strtol( psz_temp, &psz_endptr, 10 );
                if( psz_temp != psz_endptr && i_genre >= 0 && i_genre < NUM_GENRES )
                {
                    input_AddInfo( p_category, (char *)p_frame->description,
                                   ppsz_genres[atoi(psz_temp)]);
                    playlist_AddItemInfo( p_item, "ID3",
                                    (char *)p_frame->description,
                                    ppsz_genres[atoi(psz_temp)]);
                }
                else
                {
                    input_AddInfo( p_category, (char *)p_frame->description,
                                                psz_temp );
                    playlist_AddItemInfo( p_item, "ID3",
                                    (char *)p_frame->description,
                                    psz_temp);
                }
            }
            else if ( !strcmp(p_frame->id, ID3_FRAME_TITLE ) )
            {
                if( p_item )
                {
                    if( p_item->psz_name )
                    {
                        free( p_item->psz_name );
                    }
                    p_item->psz_name = strdup( psz_temp );;

                    val.b_bool = VLC_TRUE;
                }
                input_AddInfo( p_category, (char *)p_frame->description,
                                            psz_temp );
                playlist_AddItemInfo( p_item, "ID3",
                                        (char *)p_frame->description,
                                    psz_temp);
            }
            else if ( !strcmp(p_frame->id, ID3_FRAME_ARTIST ) )
            {
                if( p_item )
                {
                    playlist_AddItemInfo( p_item,
                                          _("General"), _("Author"), psz_temp);
                    val.b_bool = VLC_TRUE;
                }
                input_AddInfo( p_category, (char *)p_frame->description,
                                            psz_temp );
                playlist_AddItemInfo( p_item, "ID3",
                                           (char *)p_frame->description,
                                           psz_temp);
            }
            else
            {
                input_AddInfo( p_category, (char *)p_frame->description,
                                            psz_temp );
                playlist_AddItemInfo( p_item, "ID3",
                                           (char *)p_frame->description,
                                           psz_temp);
            }
            free( psz_temp );
        }
        i++;
    }
    id3_tag_delete( p_id3_tag );
    if(val.b_bool == VLC_TRUE )
    {
        if( p_playlist )
        {
            val.b_bool = p_playlist->i_index;
            var_Set( p_playlist, "item-change", val );
        }
    }
    val.b_bool = VLC_TRUE;
    var_Change( p_input, "demuxed-id3", VLC_VAR_SETVALUE, &val, NULL );

    if( p_playlist )
    {
        vlc_object_release( p_playlist );
    }
}

/*****************************************************************************
 * ParseID3Tags: check if ID3 tags at common locations. Parse them and skip it
 * if it's at the start of the file
 ****************************************************************************/
static int ParseID3Tags( vlc_object_t *p_this )
{
    input_thread_t *p_input;
    uint8_t *p_peek;
    int i_size;
    int i_size2;

    if ( p_this->i_object_type != VLC_OBJECT_INPUT )
    {
        return( VLC_EGENERIC );
    }
    p_input = (input_thread_t *)p_this;

    msg_Dbg( p_input, "checking for ID3 tag" );

    if ( p_input->stream.b_seekable &&
         p_input->stream.i_method != INPUT_METHOD_NETWORK )
    {
        int64_t i_pos;

        /*look for a ID3v1 tag at the end of the file*/
        i_pos = stream_Size( p_input->s );

        if ( i_pos >128 )
        {
            input_AccessReinit( p_input );
            p_input->pf_seek( p_input, i_pos - 128 );

            /* get 10 byte id3 header */
            if( stream_Peek( p_input->s, &p_peek, 10 ) < 10 )
            {
                msg_Err( p_input, "cannot peek()" );
                return( VLC_EGENERIC );
            }

            i_size2 = id3_tag_query( p_peek, 10 );
            if ( i_size2 == 128 )
            {
                /* peek the entire tag */
                if ( stream_Peek( p_input->s, &p_peek, i_size2 ) < i_size2 )
                {
                    msg_Err( p_input, "cannot peek()" );
                    return( VLC_EGENERIC );
                }
                msg_Dbg( p_input, "found ID3v1 tag" );
                ParseID3Tag( p_input, p_peek, i_size2 );
            }

            /* look for ID3v2.4 tag at end of file */
            /* get 10 byte ID3 footer */
            if( stream_Peek( p_input->s, &p_peek, 128 ) < 128 )
            {
                msg_Err( p_input, "cannot peek()" );
                return( VLC_EGENERIC );
            }
            i_size2 = id3_tag_query( p_peek + 118, 10 );
            if ( i_size2 < 0  && i_pos > -i_size2 )
            {                                        /* id3v2.4 footer found */
                input_AccessReinit( p_input );
                p_input->pf_seek( p_input, i_pos + i_size2 );
                /* peek the entire tag */
                if ( stream_Peek( p_input->s, &p_peek, i_size2 ) < i_size2 )
                {
                    msg_Err( p_input, "cannot peek()" );
                    return( VLC_EGENERIC );
                }
                msg_Dbg( p_input, "found ID3v2 tag at end of file" );
                ParseID3Tag( p_input, p_peek, i_size2 );
            }
        }
        input_AccessReinit( p_input );
        p_input->pf_seek( p_input, 0 );
    }
    /* get 10 byte id3 header */
    if( stream_Peek( p_input->s, &p_peek, 10 ) < 10 )
    {
        msg_Err( p_input, "cannot peek()" );
        return( VLC_EGENERIC );
    }

    i_size = id3_tag_query( p_peek, 10 );
    if ( i_size <= 0 )
    {
        return( VLC_SUCCESS );
    }

    /* Read the entire tag */
    p_peek = malloc( i_size );
    if( !p_peek || stream_Read( p_input->s, p_peek, i_size ) < i_size )
    {
        msg_Err( p_input, "cannot read ID3 tag" );
        if( p_peek ) free( p_peek );
        return( VLC_EGENERIC );
    }

    ParseID3Tag( p_input, p_peek, i_size );
    msg_Dbg( p_input, "found ID3v2 tag" );

    free( p_peek );
    return( VLC_SUCCESS );
}
