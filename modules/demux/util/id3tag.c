/*****************************************************************************
 * id3tag.c: id3 tag parser/skipper based on libid3tag
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: id3tag.c,v 1.5 2003/03/13 22:35:51 sigmunau Exp $
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
#include <vlc/input.h>

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
set_description( _("id3 tag parser using libid3tag" ) );
set_capability( "id3", 70 );
set_callbacks( ParseID3Tags, NULL );
vlc_module_end();

/*****************************************************************************
 * Definitions of structures  and functions used by this plugins 
 *****************************************************************************/

/*****************************************************************************
 * ParseID3Tag : parse an id3tag into the info structures
 *****************************************************************************/
static void ParseID3Tag( input_thread_t *p_input, u8 *p_data, int i_size )
{
    struct id3_tag * p_id3_tag;
    struct id3_frame * p_frame;
    input_info_category_t * p_category;
    int i_strings;
    char * psz_temp;
    int i;
    
    p_id3_tag = id3_tag_parse( p_data, i_size );
    p_category = input_InfoCategory( p_input, "ID3" );
    i = 0;
    while ( ( p_frame = id3_tag_findframe( p_id3_tag , "T", i ) ) )
    {
        i_strings = id3_field_getnstrings( &p_frame->fields[1] );
        while ( i_strings > 0 )
        {
            psz_temp = id3_ucs4_latin1duplicate( id3_field_getstrings( &p_frame->fields[1], --i_strings ) );
            if ( !strcmp(p_frame->id, ID3_FRAME_GENRE ) )
            {
                int i_genre;
                char *psz_endptr;
                i_genre = strtol( psz_temp, &psz_endptr, 10 );
                if( psz_temp != psz_endptr && i_genre >= 0 && i_genre < NUM_GENRES )
                {
                    input_AddInfo( p_category, (char *)p_frame->description, ppsz_genres[atoi(psz_temp)]);
                }
                else
                {
                    input_AddInfo( p_category, (char *)p_frame->description, psz_temp );
                }
            }
            else
            {
                input_AddInfo( p_category, (char *)p_frame->description, psz_temp );
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
    input_thread_t *p_input;
    u8  *p_peek;
    int i_size;
    int i_size2;
    stream_position_t * p_pos;

    if ( p_this->i_object_type != VLC_OBJECT_INPUT )
    {
        return( VLC_EGENERIC );
    }
    p_input = (input_thread_t *)p_this;

    msg_Dbg( p_input, "Checking for ID3 tag" );

    if ( p_input->stream.b_seekable )
    {        
        /*look for a id3v1 tag at the end of the file*/
        p_pos = malloc( sizeof( stream_position_t ) );
        if ( p_pos == 0 )
        {
            msg_Err( p_input, "no mem" );
        }
        input_Tell( p_input, p_pos );
        if ( p_pos->i_size >128 )
        {
            input_AccessReinit( p_input );
            p_input->pf_seek( p_input, p_pos->i_size - 128 );
            
            /* get 10 byte id3 header */    
            if( input_Peek( p_input, &p_peek, 10 ) < 10 )
            {
                msg_Err( p_input, "cannot peek()" );
                return( VLC_EGENERIC );
            }
            i_size2 = id3_tag_query( p_peek, 10 );
            if ( i_size2 == 128 )
            {
                /* peek the entire tag */
                if ( input_Peek( p_input, &p_peek, i_size2 ) < i_size2 )
                {
                    msg_Err( p_input, "cannot peek()" );
                    return( VLC_EGENERIC );
                }
                ParseID3Tag( p_input, p_peek, i_size2 );
            }
        }

        /* look for id3v2.4 tag at end of file */
        if ( p_pos->i_size > 10 )
        {
            input_AccessReinit( p_input );
            p_input->pf_seek( p_input, p_pos->i_size - 10 );
            /* get 10 byte id3 footer */    
            if( input_Peek( p_input, &p_peek, 10 ) < 10 )
            {
                msg_Err( p_input, "cannot peek()" );
                return( VLC_EGENERIC );
            }
            i_size2 = id3_tag_query( p_peek, 10 );
            if ( i_size2 < 0  && p_pos->i_size > -i_size2 )
            {                                          /* id3v2.4 footer found */
                input_AccessReinit( p_input );
                p_input->pf_seek( p_input, p_pos->i_size + i_size2 );
                /* peek the entire tag */
                if ( input_Peek( p_input, &p_peek, i_size2 ) < i_size2 )
                {
                    msg_Err( p_input, "cannot peek()" );
                    return( VLC_EGENERIC );
                }
                ParseID3Tag( p_input, p_peek, i_size2 );
            }
        }
        free( p_pos );
        input_AccessReinit( p_input );    
        p_input->pf_seek( p_input, 0 );
    }
    /* get 10 byte id3 header */    
    if( input_Peek( p_input, &p_peek, 10 ) < 10 )
    {
        msg_Err( p_input, "cannot peek()" );
        return( VLC_EGENERIC );
    }

    i_size = id3_tag_query( p_peek, 10 );
    if ( i_size <= 0 )
    {
        return( VLC_SUCCESS );
    }

    /* peek the entire tag */
    if ( input_Peek( p_input, &p_peek, i_size ) < i_size )
    {
        msg_Err( p_input, "cannot peek()" );
        return( VLC_EGENERIC );
    }

    ParseID3Tag( p_input, p_peek, i_size );
    msg_Dbg( p_input, "ID3 tag found, skiping %d bytes", i_size );
    p_input->p_current_data += i_size; /* seek passed end of ID3 tag */
    return( VLC_SUCCESS );
}
