/*****************************************************************************
 * id3tag.c: id3/ape tag parser/skipper based on libid3tag
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <config.h>


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_demux.h>
#include <vlc_playlist.h>
#include <vlc_charset.h>

#include <sys/types.h>

#include <vlc_meta.h>

#include <id3tag.h>
#include "id3genres.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  ParseTags ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("ID3v1/2 and APEv1/2 tags parser" ) )
    set_capability( "meta reader", 70 )
    set_callbacks( ParseTags, NULL )
vlc_module_end ()

/*****************************************************************************
 * ParseID3Tag : parse an id3tag into the info structures
 *****************************************************************************/
static void ParseID3Tag( demux_t *p_demux, const uint8_t *p_data, int i_size )
{
    struct id3_tag   *p_id3_tag;
    struct id3_frame *p_frame;
    demux_meta_t     *p_demux_meta = (demux_meta_t*)p_demux->p_private;
    vlc_meta_t       *p_meta;
    int i;

    p_id3_tag = id3_tag_parse( p_data, i_size );
    if( !p_id3_tag )
        return;

    if( !p_demux_meta->p_meta )
        p_demux_meta->p_meta = vlc_meta_New();
    p_meta = p_demux_meta->p_meta;

#define ID_IS( a ) (!strcmp(  p_frame->id, a ))
#define DESCR_IS( a) strstr( (char*)p_frame->description, a )
#define GET_STRING(frame,fidx) id3_ucs4_latin1duplicate( id3_field_getstring( &(frame)->fields[fidx] ) )

    /* */
    for( i = 0; (p_frame = id3_tag_findframe( p_id3_tag, "UFID", i )) != NULL; i++ )
    {
        const char *psz_owner = id3_field_getlatin1( &p_frame->fields[0] );

        if( !strncmp( psz_owner, "http://musicbrainz.org", 22 ) )
        {
            id3_byte_t const * p_ufid;
            id3_length_t i_ufidlen;

            p_ufid = (id3_byte_t const *)
                        id3_field_getbinarydata(
                                &p_frame->fields[1],
                                &i_ufidlen );
            char *psz_ufid = strndup( p_ufid, i_ufidlen );

            vlc_meta_SetTrackID( p_meta, psz_ufid );
            free( psz_ufid );
        }
    }

    /* User defined text (TXXX) */
    for( i = 0; (p_frame = id3_tag_findframe( p_id3_tag, "TXXX", i )) != NULL; i++ )
    {
        /* 3 fields: 'encoding', 'description', 'value' */
        char *psz_name = GET_STRING( p_frame, 1 );
        char *psz_value = GET_STRING( p_frame, 2 );

        vlc_meta_AddExtra( p_meta, psz_name, psz_value );
#if 0
        if( !strncmp( psz_name, "MusicBrainz Artist Id", 21 ) )
            vlc_meta_SetArtistID( p_meta, psz_value );
        if( !strncmp( psz_desc, "MusicBrainz Album Id", 20 ) )
            vlc_meta_SetAlbumID( p_meta, psz_value );
#endif
        free( psz_name );
        free( psz_value );
    }

    /* Relative volume adjustment */
    for( i = 0; (p_frame = id3_tag_findframe( p_id3_tag, "RVA2", i )) != NULL; i++ )
    {
        /* 2 fields: 'latin1', 'binary' */
        const char *psz_type = id3_field_getlatin1( &p_frame->fields[0] );
        if( !strcasecmp( psz_type, "track" ) || !strcasecmp( psz_type, "album" ) ||
            !strcasecmp( psz_type, "normalize" ) )
        {
            id3_byte_t const * p_data;
            id3_length_t i_data;

            p_data = id3_field_getbinarydata( &p_frame->fields[1], &i_data );
            while( i_data >= 4 )
            {
                const unsigned int i_peak_size = p_data[3];
                const float f_gain = (float)GetWBE( &p_data[1] ) / 512.0;
                char psz_value[32];

                if( i_data < i_peak_size + 4 )
                    break;
                /* only master volume */
                if( p_data[0] == 0x01 )
                {
                    snprintf( psz_value, sizeof(psz_value), "%f", f_gain );
                    if( !strcasecmp( psz_type, "album" ) )
                        vlc_meta_AddExtra( p_meta, "REPLAYGAIN_ALBUM_GAIN", psz_value );
                    else
                        vlc_meta_AddExtra( p_meta, "REPLAYGAIN_TRACK_GAIN", psz_value );
                    /* XXX I have no idea what peak unit is ... */
                }
                i_data -= 4+i_peak_size;
            }
        }
    }

    /* TODO 'RGAD' if it is used somewhere */

    /* T--- Text informations */
    for( i = 0; (p_frame = id3_tag_findframe( p_id3_tag, "T", i )) != NULL; i++ )
    {
        int i_strings;
 
        /* Special case TXXX is not the same beast */
        if( ID_IS( "TXXX" ) )
            continue;

        i_strings = id3_field_getnstrings( &p_frame->fields[1] );
        while( i_strings > 0 )
        {
            char *psz_temp = id3_ucs4_utf8duplicate(
                id3_field_getstrings( &p_frame->fields[1], --i_strings ) );

            if( ID_IS( ID3_FRAME_GENRE ) )
            {
                char *psz_endptr;
                int i_genre = strtol( psz_temp, &psz_endptr, 10 );

                if( psz_temp != psz_endptr &&
                    i_genre >= 0 && i_genre < NUM_GENRES )
                {
                    vlc_meta_SetGenre( p_meta, ppsz_genres[atoi(psz_temp)]);
                }
                else
                {
                    /* Unknown genre */
                    vlc_meta_SetGenre( p_meta,psz_temp );
                }
            }
            else if( ID_IS( ID3_FRAME_TITLE ) )
            {
                vlc_meta_SetTitle( p_meta, psz_temp );
            }
            else if( ID_IS( ID3_FRAME_ARTIST ) )
            {
                vlc_meta_SetArtist( p_meta, psz_temp );
            }
            else if( ID_IS( ID3_FRAME_YEAR ) )
            {
                vlc_meta_SetDate( p_meta, psz_temp );
            }
            else if( ID_IS( ID3_FRAME_COMMENT ) )
            {
                vlc_meta_SetDescription( p_meta, psz_temp );
            }
            else if( DESCR_IS( "Copyright" ) )
            {
                vlc_meta_SetCopyright( p_meta, psz_temp );
            }
            else if( DESCR_IS( "Publisher" ) )
            {
                vlc_meta_SetPublisher( p_meta, psz_temp );
            }
            else if( DESCR_IS( "Track number/position in set" ) )
            {
                vlc_meta_SetTrackNum( p_meta, psz_temp );
            }
            else if( DESCR_IS( "Album/movie/show title" ) )
            {
                vlc_meta_SetAlbum( p_meta, psz_temp );
            }
            else if( DESCR_IS( "Encoded by" ) )
            {
                vlc_meta_SetEncodedBy( p_meta, psz_temp );
            }
            else if( ID_IS ( "APIC" ) )
            {
                msg_Dbg( p_demux, "** Has APIC **" );
            }
            else if( p_frame->description )
            {
                /* Unhandled meta */
                vlc_meta_AddExtra( p_meta, (char*)p_frame->description, psz_temp );
            }
            free( psz_temp );
        }
    }
    id3_tag_delete( p_id3_tag );
#undef GET_STRING
#undef DESCR_IS
#undef ID_IS
}
/*****************************************************************************
 * APEv1/2
 *****************************************************************************/
#define APE_TAG_HEADERSIZE (32)
static size_t GetAPEvXSize( const uint8_t *p_data, int i_data )
{
    uint32_t flags;
    size_t i_body;

    if( i_data < APE_TAG_HEADERSIZE ||
        ( GetDWLE( &p_data[8] ) != 1000 && GetDWLE( &p_data[8] ) != 2000 ) || /* v1/v2 only */
        strncmp( (char*)p_data, "APETAGEX", 8 ) ||
        GetDWLE( &p_data[8+4+4] ) <= 0 )
        return 0;

    i_body = GetDWLE( &p_data[8+4] );
    flags = GetDWLE( &p_data[8+4+4] );

    /* is it the header */
    if( flags & (1<<29) )
        return i_body + ( (flags&(1<<30)) ? APE_TAG_HEADERSIZE : 0 );

    /* it is the footer */
    return i_body + ( (flags&(1<<31)) ? APE_TAG_HEADERSIZE : 0 );
}
static void ParseAPEvXTag( demux_t *p_demux, const uint8_t *p_data, int i_data )
{
    demux_meta_t     *p_demux_meta = (demux_meta_t*)p_demux->p_private;
    vlc_meta_t       *p_meta;
    bool b_start;
    bool b_end;
    const uint8_t *p_header = NULL;
    int i_entry;

    if( i_data < APE_TAG_HEADERSIZE )
        return;

    b_start = !strncmp( (char*)&p_data[0], "APETAGEX", 8 );
    b_end = !strncmp( (char*)&p_data[i_data-APE_TAG_HEADERSIZE], "APETAGEX", 8 );
    if( !b_end && !b_start )
        return;

    if( !p_demux_meta->p_meta )
        p_demux_meta->p_meta = vlc_meta_New();
    p_meta = p_demux_meta->p_meta;

    if( b_start )
    {
        p_header = &p_data[0];
        p_data += APE_TAG_HEADERSIZE;
        i_data -= APE_TAG_HEADERSIZE;
    }
    if( b_end )
    {
        p_header = &p_data[i_data-APE_TAG_HEADERSIZE];
        i_data -= APE_TAG_HEADERSIZE;
    }
    if( i_data <= 0 )
        return;

    i_entry = GetDWLE( &p_header[8+4+4] );
    if( i_entry <= 0 )
        return;

    while( i_entry > 0 && i_data >= 10 )
    {
        const int i_size = GetDWLE( &p_data[0] );
        const uint32_t flags = GetDWLE( &p_data[4] );
        char psz_name[256];
        int n;

        strlcpy( psz_name, (char*)&p_data[8], sizeof(psz_name) );
        n = strlen( psz_name );
        if( n <= 0 )
            break;

        p_data += 8+n+1;
        i_data -= 8+n+1;
        if( i_data < i_size )
            break;

        /* Retreive UTF-8 fields only */
        if( ((flags>>1) & 0x03) == 0x00 )
        {
            /* FIXME list are separated by '\0' */
            char *psz_value = strndup( (char*)&p_data[0], i_size );

            EnsureUTF8( psz_name );
            EnsureUTF8( psz_value );
#define IS(s) (!strcasecmp( psz_name, s ) )
            if( IS( "Title" ) )
                vlc_meta_SetTitle( p_meta, psz_value );
            else  if( IS( "Artist" ) )
                vlc_meta_SetArtist( p_meta, psz_value );
            else  if( IS( "Album" ) )
                vlc_meta_SetAlbum( p_meta, psz_value );
            else  if( IS( "Publisher" ) )
                vlc_meta_SetPublisher( p_meta, psz_value );
            else  if( IS( "Track" ) )
            {
                char *p = strchr( psz_value, '/' );
                if( p )
                    *p++ = '\0';
                vlc_meta_SetTrackNum( p_meta, psz_value );
            }
            else  if( IS( "Comment" ) )
                vlc_meta_SetDescription( p_meta, psz_value );
            else  if( IS( "Copyright" ) )
                vlc_meta_SetCopyright( p_meta, psz_value );
            else  if( IS( "Year" ) )
                vlc_meta_SetDate( p_meta, psz_value );
            else  if( IS( "Genre" ) )
                vlc_meta_SetGenre( p_meta, psz_value );
            else  if( IS( "Language" ) )
                vlc_meta_SetLanguage( p_meta, psz_value );
            else
                vlc_meta_AddExtra( p_meta, psz_name, psz_value );
#undef IS
            free( psz_value );
        }

        p_data += i_size;
        i_data -= i_size;
        i_entry--;
    }
}

/*****************************************************************************
 * CheckFooter: check for ID3/APE at the end of the file
 * CheckHeader: check for ID3/APE at the begining of the file
 *****************************************************************************/
static void CheckFooter( demux_t *p_demux )
{
    const int64_t i_pos = stream_Size( p_demux->s );
    const size_t i_peek = 128+APE_TAG_HEADERSIZE;
    const uint8_t *p_peek;
    const uint8_t *p_peek_id3;
    int64_t i_id3v2_pos = -1;
    int64_t i_apevx_pos = -1;
    int i_id3v2_size;
    int i_apevx_size;
    size_t i_id3v1_size;

    if( i_pos < i_peek )
        return;
    if( stream_Seek( p_demux->s, i_pos - i_peek ) )
        return;

    if( stream_Peek( p_demux->s, &p_peek, i_peek ) < i_peek )
        return;
    p_peek_id3 = &p_peek[APE_TAG_HEADERSIZE];

    /* Check/Parse ID3v1 */
    i_id3v1_size = id3_tag_query( p_peek_id3, ID3_TAG_QUERYSIZE );
    if( i_id3v1_size == 128 )
    {
        msg_Dbg( p_demux, "found ID3v1 tag" );
        ParseID3Tag( p_demux, p_peek_id3, i_id3v1_size );
    }

    /* Compute ID3v2 position */
    i_id3v2_size = -id3_tag_query( &p_peek_id3[128-ID3_TAG_QUERYSIZE], ID3_TAG_QUERYSIZE );
    if( i_id3v2_size > 0 )
        i_id3v2_pos = i_pos - i_id3v2_size;

    /* Compute APE2v2 position */
    i_apevx_size = GetAPEvXSize( &p_peek[128+0], APE_TAG_HEADERSIZE );
    if( i_apevx_size > 0 )
    {
        i_apevx_pos = i_pos - i_apevx_size;
    }
    else if( i_id3v1_size > 0 )
    {
        /* it can be before ID3v1 */
        i_apevx_size = GetAPEvXSize( p_peek, APE_TAG_HEADERSIZE );
        if( i_apevx_size > 0 )
            i_apevx_pos = i_pos - 128 - i_apevx_size;
    }

    if( i_id3v2_pos > 0 && i_apevx_pos > 0 )
    {
        msg_Warn( p_demux,
                  "Both ID3v2 and APEv1/2 at the end of file, ignoring APEv1/2" );
        i_apevx_pos = -1;
    }

    /* Parse ID3v2.4 */
    if( i_id3v2_pos > 0 )
    {
        if( !stream_Seek( p_demux->s, i_id3v2_pos ) &&
            stream_Peek( p_demux->s, &p_peek, i_id3v2_size ) == i_id3v2_size )
        {
            msg_Dbg( p_demux, "found ID3v2 tag at end of file" );
            ParseID3Tag( p_demux, p_peek, i_id3v2_size );
        }
    }

    /* Parse APEv1/2 */
    if( i_apevx_pos > 0 )
    {
        if( !stream_Seek( p_demux->s, i_apevx_pos ) &&
            stream_Peek( p_demux->s, &p_peek, i_apevx_size ) == i_apevx_size )
        {
            msg_Dbg( p_demux, "found APEvx tag at end of file" );
            ParseAPEvXTag( p_demux, p_peek, i_apevx_size );
        }
    }
}
static void CheckHeader( demux_t *p_demux )
{
    const uint8_t *p_peek;
    int i_size;

    if( stream_Seek( p_demux->s, 0 ) )
        return;

    /* Test ID3v2 first */
    if( stream_Peek( p_demux->s, &p_peek, ID3_TAG_QUERYSIZE ) != ID3_TAG_QUERYSIZE )
        return;
    i_size = id3_tag_query( p_peek, ID3_TAG_QUERYSIZE );
    if( i_size > 0 &&
        stream_Peek( p_demux->s, &p_peek, i_size ) == i_size )
    {
        msg_Dbg( p_demux, "found ID3v2 tag" );
        ParseID3Tag( p_demux, p_peek, i_size );
        return;
    }

    /* Test APEv1 */
    if( stream_Peek( p_demux->s, &p_peek, APE_TAG_HEADERSIZE ) != APE_TAG_HEADERSIZE )
        return;
    i_size = GetAPEvXSize( p_peek, APE_TAG_HEADERSIZE );
    if( i_size > 0 &&
        stream_Peek( p_demux->s, &p_peek, i_size ) == i_size )
    {
        msg_Dbg( p_demux, "found APEv1/2 tag" );
        ParseAPEvXTag( p_demux, p_peek, i_size );
    }
}

/*****************************************************************************
 * ParseTags: check if ID3/APE tags at common locations.
 ****************************************************************************/
static int ParseTags( vlc_object_t *p_this )
{
    demux_t      *p_demux = (demux_t *)p_this;
    demux_meta_t *p_demux_meta = (demux_meta_t*)p_demux->p_private;
    bool    b_seekable;
    int64_t       i_init;

    msg_Dbg( p_demux, "checking for ID3v1/2 and APEv1/2 tags" );
    stream_Control( p_demux->s, STREAM_CAN_FASTSEEK, &b_seekable );
    if( !b_seekable )
        return VLC_EGENERIC;

    i_init = stream_Tell( p_demux->s );

    TAB_INIT( p_demux_meta->i_attachments, p_demux_meta->attachments );
    p_demux_meta->p_meta = NULL;

    /* */
    CheckFooter( p_demux );

    /* */
    CheckHeader( p_demux );

    /* Restore position
     *  Demuxer will not see tags at the start as src/input/demux.c skips it
     *  for them
     */
    stream_Seek( p_demux->s, i_init );
    if( !p_demux_meta->p_meta && p_demux_meta->i_attachments <= 0 )
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

