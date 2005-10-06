/*****************************************************************************
 * libmp4.c : LibMP4 library for mp4 module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
 * $Id$
 *
 * Author: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>

#ifdef HAVE_ZLIB_H
#   include <zlib.h>                                  /* for compressed moov */
#endif

#include "libmp4.h"
#include "drms.h"

/*****************************************************************************
 * Here are defined some macro to make life simpler but before using it
 *  *look* at the code.
 *
 *****************************************************************************/
#define MP4_BOX_HEADERSIZE( p_box ) \
  ( 8 + ( p_box->i_shortsize == 1 ? 8 : 0 ) \
      + ( p_box->i_type == FOURCC_uuid ? 16 : 0 ) )

#define MP4_GET1BYTE( dst ) \
    dst = *p_peek; p_peek++; i_read--

#define MP4_GET2BYTES( dst ) \
    dst = GetWBE( p_peek ); p_peek += 2; i_read -= 2

#define MP4_GET3BYTES( dst ) \
    dst = Get24bBE( p_peek ); p_peek += 3; i_read -= 3

#define MP4_GET4BYTES( dst ) \
    dst = GetDWBE( p_peek ); p_peek += 4; i_read -= 4

#define MP4_GETFOURCC( dst ) \
    dst = VLC_FOURCC( p_peek[0], p_peek[1], p_peek[2], p_peek[3] ); \
    p_peek += 4; i_read -= 4

#define MP4_GET8BYTES( dst ) \
    dst = GetQWBE( p_peek ); p_peek += 8; i_read -= 8

#define MP4_GETVERSIONFLAGS( p_void ) \
    MP4_GET1BYTE( p_void->i_version ); \
    MP4_GET3BYTES( p_void->i_flags )

#define MP4_GETSTRINGZ( p_str ) \
    if( ( i_read > 0 )&&(p_peek[0] ) ) \
    { \
        p_str = calloc( sizeof( char ), __MIN( strlen( p_peek ), i_read )+1);\
        memcpy( p_str, p_peek, __MIN( strlen( p_peek ), i_read ) ); \
        p_str[__MIN( strlen( p_peek ), i_read )] = 0; \
        p_peek += strlen( p_str ) + 1; \
        i_read -= strlen( p_str ) + 1; \
    } \
    else \
    { \
        p_str = NULL; \
    }


#define MP4_READBOX_ENTER( MP4_Box_data_TYPE_t ) \
    int64_t  i_read = p_box->i_size; \
    uint8_t *p_peek, *p_buff; \
    i_read = p_box->i_size; \
    if( !( p_peek = p_buff = malloc( i_read ) ) ) \
    { \
        return( 0 ); \
    } \
    if( stream_Read( p_stream, p_peek, i_read ) < i_read )\
    { \
        free( p_buff ); \
        return( 0 ); \
    } \
    p_peek += MP4_BOX_HEADERSIZE( p_box ); \
    i_read -= MP4_BOX_HEADERSIZE( p_box ); \
    if( !( p_box->data.p_data = malloc( sizeof( MP4_Box_data_TYPE_t ) ) ) ) \
    { \
      free( p_buff ); \
      return( 0 ); \
    }

#define MP4_READBOX_EXIT( i_code ) \
    free( p_buff ); \
    if( i_read < 0 ) \
    { \
        msg_Warn( p_stream, "Not enough data" ); \
    } \
    return( i_code )

#define FREE( p ) \
    if( p ) {free( p ); p = NULL; }



/* Some assumptions:
        * The input method HAVE to be seekable

*/

static uint32_t Get24bBE( uint8_t *p )
{
    return( ( p[0] <<16 ) + ( p[1] <<8 ) + p[2] );
}

static void GetUUID( UUID_t *p_uuid, uint8_t *p_buff )
{
    memcpy( p_uuid, p_buff, 16 );
}

static void CreateUUID( UUID_t *p_uuid, uint32_t i_fourcc )
{
    /* made by 0xXXXXXXXX-0011-0010-8000-00aa00389b71
            where XXXXXXXX is the fourcc */
    /* FIXME implement this */
}

/* some functions for mp4 encoding of variables */

static void MP4_ConvertDate2Str( char *psz, uint64_t i_date )
{
    int i_day;
    int i_hour;
    int i_min;
    int i_sec;

    /* date begin at 1 jan 1904 */
    i_date += ((I64C(1904) * 365) + 17) * 24 * 60 * 60;

    i_day = i_date / ( 60*60*24);
    i_hour = ( i_date /( 60*60 ) ) % 60;
    i_min  = ( i_date / 60 ) % 60;
    i_sec =  i_date % 60;
    sprintf( psz, "%dd-%2.2dh:%2.2dm:%2.2ds", i_day, i_hour, i_min, i_sec );
}

/*****************************************************************************
 * Some prototypes.
 *****************************************************************************/
static MP4_Box_t *MP4_ReadBox( stream_t *p_stream, MP4_Box_t *p_father );


/*****************************************************************************
 * MP4_ReadBoxCommon : Load only common parameters for all boxes
 *****************************************************************************
 * p_box need to be an already allocated MP4_Box_t, and all data
 *  will only be peek not read
 *
 * RETURN : 0 if it fail, 1 otherwise
 *****************************************************************************/
int MP4_ReadBoxCommon( stream_t *p_stream, MP4_Box_t *p_box )
{
    int      i_read;
    uint8_t  *p_peek;

    if( ( ( i_read = stream_Peek( p_stream, &p_peek, 32 ) ) < 8 ) )
    {
        return 0;
    }
    p_box->i_pos = stream_Tell( p_stream );

    p_box->data.p_data = NULL;
    p_box->p_father = NULL;
    p_box->p_first  = NULL;
    p_box->p_last  = NULL;
    p_box->p_next   = NULL;

    MP4_GET4BYTES( p_box->i_shortsize );
    MP4_GETFOURCC( p_box->i_type );

    /* Now special case */

    if( p_box->i_shortsize == 1 )
    {
        /* get the true size on 64 bits */
        MP4_GET8BYTES( p_box->i_size );
    }
    else
    {
        p_box->i_size = p_box->i_shortsize;
        /* XXX size of 0 means that the box extends to end of file */
    }

    if( p_box->i_type == FOURCC_uuid )
    {
        /* get extented type on 16 bytes */
        GetUUID( &p_box->i_uuid, p_peek );
        p_peek += 16; i_read -= 16;
    }
    else
    {
        CreateUUID( &p_box->i_uuid, p_box->i_type );
    }
#ifdef MP4_VERBOSE
    if( p_box->i_size )
    {
        msg_Dbg( p_stream, "found Box: %4.4s size "I64Fd,
                 (char*)&p_box->i_type, p_box->i_size );
    }
#endif

    return 1;
}

/*****************************************************************************
 * MP4_NextBox : Go to the next box
 *****************************************************************************
 * if p_box == NULL, go to the next box in witch we are( at the begining ).
 *****************************************************************************/
static int MP4_NextBox( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_Box_t box;

    if( !p_box )
    {
        MP4_ReadBoxCommon( p_stream, &box );
        p_box = &box;
    }

    if( !p_box->i_size )
    {
        return 2; /* Box with infinite size */
    }

    if( p_box->p_father )
    {
        /* check if it's within p-father */
        if( p_box->i_size + p_box->i_pos >=
            p_box->p_father->i_size + p_box->p_father->i_pos )
        {
            return 0; /* out of bound */
        }
    }
    if( stream_Seek( p_stream, p_box->i_size + p_box->i_pos ) )
    {
        return 0;
    }

    return 1;
}

/*****************************************************************************
 * For all known box a loader is given,
 *  XXX: all common struct have to be already read by MP4_ReadBoxCommon
 *       after called one of theses functions, file position is unknown
 *       you need to call MP4_GotoBox to go where you want
 *****************************************************************************/
static int MP4_ReadBoxContainerRaw( stream_t *p_stream, MP4_Box_t *p_container )
{
    MP4_Box_t *p_box;

    if( stream_Tell( p_stream ) + 8 >
        (off_t)(p_container->i_pos + p_container->i_size) )
    {
        /* there is no box to load */
        return 0;
    }

    do
    {
        if( ( p_box = MP4_ReadBox( p_stream, p_container ) ) == NULL ) break;

        /* chain this box with the father and the other at same level */
        if( !p_container->p_first ) p_container->p_first = p_box;
        else p_container->p_last->p_next = p_box;
        p_container->p_last = p_box;

    } while( MP4_NextBox( p_stream, p_box ) == 1 );

    return 1;
}

static int MP4_ReadBoxContainer( stream_t *p_stream, MP4_Box_t *p_container )
{
    if( p_container->i_size <= (size_t)MP4_BOX_HEADERSIZE(p_container ) + 8 )
    {
        /* container is empty, 8 stand for the first header in this box */
        return 1;
    }

    /* enter box */
    stream_Seek( p_stream, p_container->i_pos +
                 MP4_BOX_HEADERSIZE( p_container ) );

    return MP4_ReadBoxContainerRaw( p_stream, p_container );
}

static void MP4_FreeBox_Common( MP4_Box_t *p_box )
{
    /* Up to now do nothing */
}

static int MP4_ReadBoxSkip( stream_t *p_stream, MP4_Box_t *p_box )
{
    /* XXX sometime moov is hiden in a free box */
    if( p_box->p_father &&
        p_box->p_father->i_type == VLC_FOURCC( 'r', 'o', 'o', 't' ) &&
        p_box->i_type == FOURCC_free )
    {
        uint8_t *p_peek;
        int     i_read;
        vlc_fourcc_t i_fcc;

        i_read  = stream_Peek( p_stream, &p_peek, 44 );

        p_peek += MP4_BOX_HEADERSIZE( p_box ) + 4;
        i_read -= MP4_BOX_HEADERSIZE( p_box ) + 4;

        if( i_read >= 8 )
        {
            i_fcc = VLC_FOURCC( p_peek[0], p_peek[1], p_peek[2], p_peek[3] );

            if( i_fcc == FOURCC_cmov || i_fcc == FOURCC_mvhd )
            {
                msg_Warn( p_stream, "detected moov hidden in a free box ..." );

                p_box->i_type = FOURCC_foov;
                return MP4_ReadBoxContainer( p_stream, p_box );
            }
        }
    }

    /* Nothing to do */
#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "skip box: \"%4.4s\"", (char*)&p_box->i_type );
#endif
    return 1;
}

static int MP4_ReadBox_ftyp( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_ftyp_t );

    MP4_GETFOURCC( p_box->data.p_ftyp->i_major_brand );
    MP4_GET4BYTES( p_box->data.p_ftyp->i_minor_version );

    if( ( p_box->data.p_ftyp->i_compatible_brands_count = i_read / 4 ) )
    {
        unsigned int i;
        p_box->data.p_ftyp->i_compatible_brands =
            calloc( p_box->data.p_ftyp->i_compatible_brands_count, sizeof(uint32_t));

        for( i =0; i < p_box->data.p_ftyp->i_compatible_brands_count; i++ )
        {
            MP4_GETFOURCC( p_box->data.p_ftyp->i_compatible_brands[i] );
        }
    }
    else
    {
        p_box->data.p_ftyp->i_compatible_brands = NULL;
    }

    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_ftyp( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_ftyp->i_compatible_brands );
}


static int MP4_ReadBox_mvhd(  stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;
#ifdef MP4_VERBOSE
    char s_creation_time[128];
    char s_modification_time[128];
    char s_duration[128];
#endif
    MP4_READBOX_ENTER( MP4_Box_data_mvhd_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_mvhd );

    if( p_box->data.p_mvhd->i_version )
    {
        MP4_GET8BYTES( p_box->data.p_mvhd->i_creation_time );
        MP4_GET8BYTES( p_box->data.p_mvhd->i_modification_time );
        MP4_GET4BYTES( p_box->data.p_mvhd->i_timescale );
        MP4_GET8BYTES( p_box->data.p_mvhd->i_duration );
    }
    else
    {
        MP4_GET4BYTES( p_box->data.p_mvhd->i_creation_time );
        MP4_GET4BYTES( p_box->data.p_mvhd->i_modification_time );
        MP4_GET4BYTES( p_box->data.p_mvhd->i_timescale );
        MP4_GET4BYTES( p_box->data.p_mvhd->i_duration );
    }
    MP4_GET4BYTES( p_box->data.p_mvhd->i_rate );
    MP4_GET2BYTES( p_box->data.p_mvhd->i_volume );
    MP4_GET2BYTES( p_box->data.p_mvhd->i_reserved1 );


    for( i = 0; i < 2; i++ )
    {
        MP4_GET4BYTES( p_box->data.p_mvhd->i_reserved2[i] );
    }
    for( i = 0; i < 9; i++ )
    {
        MP4_GET4BYTES( p_box->data.p_mvhd->i_matrix[i] );
    }
    for( i = 0; i < 6; i++ )
    {
        MP4_GET4BYTES( p_box->data.p_mvhd->i_predefined[i] );
    }

    MP4_GET4BYTES( p_box->data.p_mvhd->i_next_track_id );


#ifdef MP4_VERBOSE
    MP4_ConvertDate2Str( s_creation_time, p_box->data.p_mvhd->i_creation_time );
    MP4_ConvertDate2Str( s_modification_time,
                         p_box->data.p_mvhd->i_modification_time );
    if( p_box->data.p_mvhd->i_rate )
    {
        MP4_ConvertDate2Str( s_duration,
                 p_box->data.p_mvhd->i_duration / p_box->data.p_mvhd->i_rate );
    }
    else
    {
        s_duration[0] = 0;
    }
    msg_Dbg( p_stream, "read box: \"mvhd\" creation %s modification %s time scale %d duration %s rate %f volume %f next track id %d",
                  s_creation_time,
                  s_modification_time,
                  (uint32_t)p_box->data.p_mvhd->i_timescale,
                  s_duration,
                  (float)p_box->data.p_mvhd->i_rate / (1<<16 ),
                  (float)p_box->data.p_mvhd->i_volume / 256 ,
                  (uint32_t)p_box->data.p_mvhd->i_next_track_id );
#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_tkhd(  stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;
#ifdef MP4_VERBOSE
    char s_creation_time[128];
    char s_modification_time[128];
    char s_duration[128];
#endif
    MP4_READBOX_ENTER( MP4_Box_data_tkhd_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_tkhd );

    if( p_box->data.p_tkhd->i_version )
    {
        MP4_GET8BYTES( p_box->data.p_tkhd->i_creation_time );
        MP4_GET8BYTES( p_box->data.p_tkhd->i_modification_time );
        MP4_GET4BYTES( p_box->data.p_tkhd->i_track_ID );
        MP4_GET4BYTES( p_box->data.p_tkhd->i_reserved );
        MP4_GET8BYTES( p_box->data.p_tkhd->i_duration );
    }
    else
    {
        MP4_GET4BYTES( p_box->data.p_tkhd->i_creation_time );
        MP4_GET4BYTES( p_box->data.p_tkhd->i_modification_time );
        MP4_GET4BYTES( p_box->data.p_tkhd->i_track_ID );
        MP4_GET4BYTES( p_box->data.p_tkhd->i_reserved );
        MP4_GET4BYTES( p_box->data.p_tkhd->i_duration );
    }

    for( i = 0; i < 2; i++ )
    {
        MP4_GET4BYTES( p_box->data.p_tkhd->i_reserved2[i] );
    }
    MP4_GET2BYTES( p_box->data.p_tkhd->i_layer );
    MP4_GET2BYTES( p_box->data.p_tkhd->i_predefined );
    MP4_GET2BYTES( p_box->data.p_tkhd->i_volume );
    MP4_GET2BYTES( p_box->data.p_tkhd->i_reserved3 );

    for( i = 0; i < 9; i++ )
    {
        MP4_GET4BYTES( p_box->data.p_tkhd->i_matrix[i] );
    }
    MP4_GET4BYTES( p_box->data.p_tkhd->i_width );
    MP4_GET4BYTES( p_box->data.p_tkhd->i_height );

#ifdef MP4_VERBOSE
    MP4_ConvertDate2Str( s_creation_time, p_box->data.p_mvhd->i_creation_time );
    MP4_ConvertDate2Str( s_modification_time, p_box->data.p_mvhd->i_modification_time );
    MP4_ConvertDate2Str( s_duration, p_box->data.p_mvhd->i_duration );

    msg_Dbg( p_stream, "read box: \"tkhd\" creation %s modification %s duration %s track ID %d layer %d volume %f width %f height %f",
                  s_creation_time,
                  s_modification_time,
                  s_duration,
                  p_box->data.p_tkhd->i_track_ID,
                  p_box->data.p_tkhd->i_layer,
                  (float)p_box->data.p_tkhd->i_volume / 256 ,
                  (float)p_box->data.p_tkhd->i_width / 65536,
                  (float)p_box->data.p_tkhd->i_height / 65536 );
#endif
    MP4_READBOX_EXIT( 1 );
}


static int MP4_ReadBox_mdhd( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;
    uint16_t i_language;
#ifdef MP4_VERBOSE
    char s_creation_time[128];
    char s_modification_time[128];
    char s_duration[128];
#endif
    MP4_READBOX_ENTER( MP4_Box_data_mdhd_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_mdhd );

    if( p_box->data.p_mdhd->i_version )
    {
        MP4_GET8BYTES( p_box->data.p_mdhd->i_creation_time );
        MP4_GET8BYTES( p_box->data.p_mdhd->i_modification_time );
        MP4_GET4BYTES( p_box->data.p_mdhd->i_timescale );
        MP4_GET8BYTES( p_box->data.p_mdhd->i_duration );
    }
    else
    {
        MP4_GET4BYTES( p_box->data.p_mdhd->i_creation_time );
        MP4_GET4BYTES( p_box->data.p_mdhd->i_modification_time );
        MP4_GET4BYTES( p_box->data.p_mdhd->i_timescale );
        MP4_GET4BYTES( p_box->data.p_mdhd->i_duration );
    }
    i_language = GetWBE( p_peek );
    for( i = 0; i < 3; i++ )
    {
        p_box->data.p_mdhd->i_language[i] =
                    ( ( i_language >> ( (2-i)*5 ) )&0x1f ) + 0x60;
    }

    MP4_GET2BYTES( p_box->data.p_mdhd->i_predefined );

#ifdef MP4_VERBOSE
    MP4_ConvertDate2Str( s_creation_time, p_box->data.p_mdhd->i_creation_time );
    MP4_ConvertDate2Str( s_modification_time, p_box->data.p_mdhd->i_modification_time );
    MP4_ConvertDate2Str( s_duration, p_box->data.p_mdhd->i_duration );
    msg_Dbg( p_stream, "read box: \"mdhd\" creation %s modification %s time scale %d duration %s language %c%c%c",
                  s_creation_time,
                  s_modification_time,
                  (uint32_t)p_box->data.p_mdhd->i_timescale,
                  s_duration,
                  p_box->data.p_mdhd->i_language[0],
                  p_box->data.p_mdhd->i_language[1],
                  p_box->data.p_mdhd->i_language[2] );
#endif
    MP4_READBOX_EXIT( 1 );
}


static int MP4_ReadBox_hdlr( stream_t *p_stream, MP4_Box_t *p_box )
{
    int32_t i_reserved;

    MP4_READBOX_ENTER( MP4_Box_data_hdlr_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_hdlr );

    MP4_GETFOURCC( p_box->data.p_hdlr->i_predefined );
    MP4_GETFOURCC( p_box->data.p_hdlr->i_handler_type );

    MP4_GET4BYTES( i_reserved );
    MP4_GET4BYTES( i_reserved );
    MP4_GET4BYTES( i_reserved );

    p_box->data.p_hdlr->psz_name = calloc( sizeof( char ), i_read + 1 );

    /* Yes, I love .mp4 :( */
    if( p_box->data.p_hdlr->i_predefined == VLC_FOURCC( 'm', 'h', 'l', 'r' ) )
    {
        uint8_t i_len;
        int i_copy;

        MP4_GET1BYTE( i_len );
        i_copy = __MIN( i_read, i_len );

        memcpy( p_box->data.p_hdlr->psz_name, p_peek, i_copy );
        p_box->data.p_hdlr->psz_name[i_copy] = '\0';
    }
    else
    {
        memcpy( p_box->data.p_hdlr->psz_name, p_peek, i_read );
        p_box->data.p_hdlr->psz_name[i_read] = '\0';
    }


#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"hdlr\" hanler type %4.4s name %s",
                       (char*)&p_box->data.p_hdlr->i_handler_type,
                       p_box->data.p_hdlr->psz_name );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_hdlr( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_hdlr->psz_name );
}

static int MP4_ReadBox_vmhd( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;

    MP4_READBOX_ENTER( MP4_Box_data_vmhd_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_vmhd );

    MP4_GET2BYTES( p_box->data.p_vmhd->i_graphics_mode );
    for( i = 0; i < 3; i++ )
    {
        MP4_GET2BYTES( p_box->data.p_vmhd->i_opcolor[i] );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"vmhd\" graphics-mode %d opcolor (%d, %d, %d)",
                      p_box->data.p_vmhd->i_graphics_mode,
                      p_box->data.p_vmhd->i_opcolor[0],
                      p_box->data.p_vmhd->i_opcolor[1],
                      p_box->data.p_vmhd->i_opcolor[2] );
#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_smhd( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_smhd_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_smhd );



    MP4_GET2BYTES( p_box->data.p_smhd->i_balance );

    MP4_GET2BYTES( p_box->data.p_smhd->i_reserved );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"smhd\" balance %f",
                      (float)p_box->data.p_smhd->i_balance / 256 );
#endif
    MP4_READBOX_EXIT( 1 );
}


static int MP4_ReadBox_hmhd( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_hmhd_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_hmhd );

    MP4_GET2BYTES( p_box->data.p_hmhd->i_max_PDU_size );
    MP4_GET2BYTES( p_box->data.p_hmhd->i_avg_PDU_size );

    MP4_GET4BYTES( p_box->data.p_hmhd->i_max_bitrate );
    MP4_GET4BYTES( p_box->data.p_hmhd->i_avg_bitrate );

    MP4_GET4BYTES( p_box->data.p_hmhd->i_reserved );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"hmhd\" maxPDU-size %d avgPDU-size %d max-bitrate %d avg-bitrate %d",
                      p_box->data.p_hmhd->i_max_PDU_size,
                      p_box->data.p_hmhd->i_avg_PDU_size,
                      p_box->data.p_hmhd->i_max_bitrate,
                      p_box->data.p_hmhd->i_avg_bitrate );
#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_url( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_url_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_url );
    MP4_GETSTRINGZ( p_box->data.p_url->psz_location );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"url\" url: %s",
                       p_box->data.p_url->psz_location );

#endif
    MP4_READBOX_EXIT( 1 );
}


static void MP4_FreeBox_url( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_url->psz_location )
}

static int MP4_ReadBox_urn( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_urn_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_urn );

    MP4_GETSTRINGZ( p_box->data.p_urn->psz_name );
    MP4_GETSTRINGZ( p_box->data.p_urn->psz_location );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"urn\" name %s location %s",
                      p_box->data.p_urn->psz_name,
                      p_box->data.p_urn->psz_location );
#endif
    MP4_READBOX_EXIT( 1 );
}
static void MP4_FreeBox_urn( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_urn->psz_name );
    FREE( p_box->data.p_urn->psz_location );
}


static int MP4_ReadBox_dref( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_dref_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_dref );

    MP4_GET4BYTES( p_box->data.p_dref->i_entry_count );

    stream_Seek( p_stream, p_box->i_pos + MP4_BOX_HEADERSIZE( p_box ) + 8 );
    MP4_ReadBoxContainerRaw( p_stream, p_box );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"dref\" entry-count %d",
                      p_box->data.p_dref->i_entry_count );

#endif
    MP4_READBOX_EXIT( 1 );
}


static int MP4_ReadBox_stts( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;
    MP4_READBOX_ENTER( MP4_Box_data_stts_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_stts );
    MP4_GET4BYTES( p_box->data.p_stts->i_entry_count );

    p_box->data.p_stts->i_sample_count =
        calloc( sizeof( uint32_t ), p_box->data.p_stts->i_entry_count );
    p_box->data.p_stts->i_sample_delta =
        calloc( sizeof( uint32_t ), p_box->data.p_stts->i_entry_count );

    for( i = 0; (i < p_box->data.p_stts->i_entry_count )&&( i_read >=8 ); i++ )
    {
        MP4_GET4BYTES( p_box->data.p_stts->i_sample_count[i] );
        MP4_GET4BYTES( p_box->data.p_stts->i_sample_delta[i] );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"stts\" entry-count %d",
                      p_box->data.p_stts->i_entry_count );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_stts( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_stts->i_sample_count );
    FREE( p_box->data.p_stts->i_sample_delta );
}

static int MP4_ReadBox_ctts( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;
    MP4_READBOX_ENTER( MP4_Box_data_ctts_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_ctts );

    MP4_GET4BYTES( p_box->data.p_ctts->i_entry_count );

    p_box->data.p_ctts->i_sample_count =
        calloc( sizeof( uint32_t ), p_box->data.p_ctts->i_entry_count );
    p_box->data.p_ctts->i_sample_offset =
        calloc( sizeof( uint32_t ), p_box->data.p_ctts->i_entry_count );

    for( i = 0; (i < p_box->data.p_ctts->i_entry_count )&&( i_read >=8 ); i++ )
    {
        MP4_GET4BYTES( p_box->data.p_ctts->i_sample_count[i] );
        MP4_GET4BYTES( p_box->data.p_ctts->i_sample_offset[i] );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"ctts\" entry-count %d",
                      p_box->data.p_ctts->i_entry_count );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_ctts( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_ctts->i_sample_count );
    FREE( p_box->data.p_ctts->i_sample_offset );
}

static int MP4_ReadLengthDescriptor( uint8_t **pp_peek, int64_t  *i_read )
{
    unsigned int i_b;
    unsigned int i_len = 0;
    do
    {
        i_b = **pp_peek;

        (*pp_peek)++;
        (*i_read)--;
        i_len = ( i_len << 7 ) + ( i_b&0x7f );
    } while( i_b&0x80 );
    return( i_len );
}

static int MP4_ReadBox_esds( stream_t *p_stream, MP4_Box_t *p_box )
{
#define es_descriptor p_box->data.p_esds->es_descriptor
    unsigned int i_len;
    unsigned int i_flags;
    unsigned int i_type;

    MP4_READBOX_ENTER( MP4_Box_data_esds_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_esds );


    MP4_GET1BYTE( i_type );
    if( i_type == 0x03 ) /* MP4ESDescrTag */
    {
        i_len = MP4_ReadLengthDescriptor( &p_peek, &i_read );

#ifdef MP4_VERBOSE
        msg_Dbg( p_stream, "found esds MPEG4ESDescr (%dBytes)",
                 i_len );
#endif

        MP4_GET2BYTES( es_descriptor.i_ES_ID );
        MP4_GET1BYTE( i_flags );
        es_descriptor.b_stream_dependence = ( (i_flags&0x80) != 0);
        es_descriptor.b_url = ( (i_flags&0x40) != 0);
        es_descriptor.b_OCRstream = ( (i_flags&0x20) != 0);

        es_descriptor.i_stream_priority = i_flags&0x1f;
        if( es_descriptor.b_stream_dependence )
        {
            MP4_GET2BYTES( es_descriptor.i_depend_on_ES_ID );
        }
        if( es_descriptor.b_url )
        {
            unsigned int i_len;

            MP4_GET1BYTE( i_len );
            es_descriptor.psz_URL = calloc( sizeof(char), i_len + 1 );
            memcpy( es_descriptor.psz_URL, p_peek, i_len );
            es_descriptor.psz_URL[i_len] = 0;
            p_peek += i_len;
            i_read -= i_len;
        }
        else
        {
            es_descriptor.psz_URL = NULL;
        }
        if( es_descriptor.b_OCRstream )
        {
            MP4_GET2BYTES( es_descriptor.i_OCR_ES_ID );
        }
        MP4_GET1BYTE( i_type ); /* get next type */
    }

    if( i_type != 0x04)/* MP4DecConfigDescrTag */
    {
         es_descriptor.p_decConfigDescr = NULL;
         MP4_READBOX_EXIT( 1 ); /* rest isn't interesting up to now */
    }

    i_len = MP4_ReadLengthDescriptor( &p_peek, &i_read );

#ifdef MP4_VERBOSE
        msg_Dbg( p_stream, "found esds MP4DecConfigDescr (%dBytes)",
                 i_len );
#endif

    es_descriptor.p_decConfigDescr =
            malloc( sizeof( MP4_descriptor_decoder_config_t ));

    MP4_GET1BYTE( es_descriptor.p_decConfigDescr->i_objectTypeIndication );
    MP4_GET1BYTE( i_flags );
    es_descriptor.p_decConfigDescr->i_streamType = i_flags >> 2;
    es_descriptor.p_decConfigDescr->b_upStream = ( i_flags >> 1 )&0x01;
    MP4_GET3BYTES( es_descriptor.p_decConfigDescr->i_buffer_sizeDB );
    MP4_GET4BYTES( es_descriptor.p_decConfigDescr->i_max_bitrate );
    MP4_GET4BYTES( es_descriptor.p_decConfigDescr->i_avg_bitrate );
    MP4_GET1BYTE( i_type );
    if( i_type !=  0x05 )/* MP4DecSpecificDescrTag */
    {
        es_descriptor.p_decConfigDescr->i_decoder_specific_info_len = 0;
        es_descriptor.p_decConfigDescr->p_decoder_specific_info  = NULL;
        MP4_READBOX_EXIT( 1 );
    }

    i_len = MP4_ReadLengthDescriptor( &p_peek, &i_read );

#ifdef MP4_VERBOSE
        msg_Dbg( p_stream, "found esds MP4DecSpecificDescr (%dBytes)",
                 i_len );
#endif

    es_descriptor.p_decConfigDescr->i_decoder_specific_info_len = i_len;
    es_descriptor.p_decConfigDescr->p_decoder_specific_info = malloc( i_len );
    memcpy( es_descriptor.p_decConfigDescr->p_decoder_specific_info,
            p_peek, i_len );

    MP4_READBOX_EXIT( 1 );

#undef es_descriptor
}

static void MP4_FreeBox_esds( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_esds->es_descriptor.psz_URL );
    if( p_box->data.p_esds->es_descriptor.p_decConfigDescr )
    {
        FREE( p_box->data.p_esds->es_descriptor.p_decConfigDescr->p_decoder_specific_info );
    }
    FREE( p_box->data.p_esds->es_descriptor.p_decConfigDescr );
}

static int MP4_ReadBox_avcC( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_Box_data_avcC_t *p_avcC;
    int i;

    MP4_READBOX_ENTER( MP4_Box_data_avcC_t );
    p_avcC = p_box->data.p_avcC;

    p_avcC->i_avcC = i_read;
    p_avcC->p_avcC = malloc( p_avcC->i_avcC );
    memcpy( p_avcC->p_avcC, p_peek, i_read );

    MP4_GET1BYTE( p_avcC->i_version );
    MP4_GET1BYTE( p_avcC->i_profile );
    MP4_GET1BYTE( p_avcC->i_profile_compatibility );
    MP4_GET1BYTE( p_avcC->i_level );
    MP4_GET1BYTE( p_avcC->i_reserved1 );
    p_avcC->i_length_size = (p_avcC->i_reserved1&0x03) + 1;
    p_avcC->i_reserved1 >>= 2;

    MP4_GET1BYTE( p_avcC->i_reserved2 );
    p_avcC->i_sps = p_avcC->i_reserved2&0x1f;
    p_avcC->i_reserved2 >>= 5;

    if( p_avcC->i_sps > 0 )
    {
        p_avcC->i_sps_length = malloc( p_avcC->i_sps * sizeof( uint16_t ) );
        p_avcC->sps = malloc( p_avcC->i_sps * sizeof( uint8_t* ) );

        for( i = 0; i < p_avcC->i_sps; i++ )
        {
            MP4_GET2BYTES( p_avcC->i_sps_length[i] );
            p_avcC->sps[i] = malloc( p_avcC->i_sps_length[i] );
            memcpy( p_avcC->sps[i], p_peek, p_avcC->i_sps_length[i] );

            p_peek += p_avcC->i_sps_length[i];
            i_read -= p_avcC->i_sps_length[i];
        }
    }

    MP4_GET1BYTE( p_avcC->i_pps );
    if( p_avcC->i_pps > 0 )
    {
        p_avcC->i_pps_length = malloc( p_avcC->i_pps * sizeof( uint16_t ) );
        p_avcC->pps = malloc( p_avcC->i_pps * sizeof( uint8_t* ) );

        for( i = 0; i < p_avcC->i_pps; i++ )
        {
            MP4_GET2BYTES( p_avcC->i_pps_length[i] );
            p_avcC->pps[i] = malloc( p_avcC->i_pps_length[i] );
            memcpy( p_avcC->pps[i], p_peek, p_avcC->i_pps_length[i] );

            p_peek += p_avcC->i_pps_length[i];
            i_read -= p_avcC->i_pps_length[i];
        }
    }
#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"avcC\" version=%d profile=0x%x level=0x%x length size=%d sps=%d pps=%d",
             p_avcC->i_version, p_avcC->i_profile, p_avcC->i_level,
             p_avcC->i_length_size,
             p_avcC->i_sps, p_avcC->i_pps );
    for( i = 0; i < p_avcC->i_sps; i++ )
    {
        msg_Dbg( p_stream, "         - sps[%d] length=%d",
                 i, p_avcC->i_sps_length[i] );
    }
    for( i = 0; i < p_avcC->i_pps; i++ )
    {
        msg_Dbg( p_stream, "         - pps[%d] length=%d",
                 i, p_avcC->i_pps_length[i] );
    }

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_avcC( MP4_Box_t *p_box )
{
    MP4_Box_data_avcC_t *p_avcC = p_box->data.p_avcC;
    int i;

    for( i = 0; i < p_avcC->i_sps; i++ )
    {
        FREE( p_avcC->sps[i] );
    }
    for( i = 0; i < p_avcC->i_pps; i++ )
    {
        FREE( p_avcC->pps[i] );
    }
    if( p_avcC->i_sps > 0 ) FREE( p_avcC->sps );
    if( p_avcC->i_pps > 0 ) FREE( p_avcC->pps );
}

static int MP4_ReadBox_sample_soun( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;

    MP4_READBOX_ENTER( MP4_Box_data_sample_soun_t );
    p_box->data.p_sample_soun->p_qt_description = NULL;

    /* Sanity check needed because the "wave" box does also contain an
     * "mp4a" box that we don't understand. */
    if( i_read < 28 )
    {
        i_read -= 30;
        MP4_READBOX_EXIT( 1 );
    }

    for( i = 0; i < 6 ; i++ )
    {
        MP4_GET1BYTE( p_box->data.p_sample_soun->i_reserved1[i] );
    }

    MP4_GET2BYTES( p_box->data.p_sample_soun->i_data_reference_index );

    /*
     * XXX hack -> produce a copy of the nearly complete chunk
     */
    if( i_read > 0 )
    {
        p_box->data.p_sample_soun->i_qt_description = i_read;
        p_box->data.p_sample_soun->p_qt_description = malloc( i_read );
        memcpy( p_box->data.p_sample_soun->p_qt_description, p_peek, i_read );
    }
    else
    {
        p_box->data.p_sample_soun->i_qt_description = 0;
        p_box->data.p_sample_soun->p_qt_description = NULL;
    }

    MP4_GET2BYTES( p_box->data.p_sample_soun->i_qt_version );
    MP4_GET2BYTES( p_box->data.p_sample_soun->i_qt_revision_level );
    MP4_GET4BYTES( p_box->data.p_sample_soun->i_qt_vendor );

    MP4_GET2BYTES( p_box->data.p_sample_soun->i_channelcount );
    MP4_GET2BYTES( p_box->data.p_sample_soun->i_samplesize );
    MP4_GET2BYTES( p_box->data.p_sample_soun->i_predefined );
    MP4_GET2BYTES( p_box->data.p_sample_soun->i_reserved3 );
    MP4_GET2BYTES( p_box->data.p_sample_soun->i_sampleratehi );
    MP4_GET2BYTES( p_box->data.p_sample_soun->i_sampleratelo );

    if( p_box->data.p_sample_soun->i_qt_version == 1 && i_read >= 16 )
    {
        /* SoundDescriptionV1 */
        MP4_GET4BYTES( p_box->data.p_sample_soun->i_sample_per_packet );
        MP4_GET4BYTES( p_box->data.p_sample_soun->i_bytes_per_packet );
        MP4_GET4BYTES( p_box->data.p_sample_soun->i_bytes_per_frame );
        MP4_GET4BYTES( p_box->data.p_sample_soun->i_bytes_per_sample );

#ifdef MP4_VERBOSE
        msg_Dbg( p_stream,
                 "read box: \"soun\" qt3+ sample/packet=%d bytes/packet=%d "
                 "bytes/frame=%d bytes/sample=%d",
                 p_box->data.p_sample_soun->i_sample_per_packet,
                 p_box->data.p_sample_soun->i_bytes_per_packet,
                 p_box->data.p_sample_soun->i_bytes_per_frame,
                 p_box->data.p_sample_soun->i_bytes_per_sample );
#endif
        stream_Seek( p_stream, p_box->i_pos +
                        MP4_BOX_HEADERSIZE( p_box ) + 44 );
    }
    else if( p_box->data.p_sample_soun->i_qt_version == 2 && i_read >= 36 )
    {
        /* SoundDescriptionV2 */
        double f_sample_rate;
        uint32_t i_channel;

        MP4_GET4BYTES( p_box->data.p_sample_soun->i_sample_per_packet );
        MP4_GET8BYTES( (*(int64_t *)&f_sample_rate) );

        msg_Dbg( p_stream, "read box: %f Hz", f_sample_rate );
        p_box->data.p_sample_soun->i_sampleratehi = (int)f_sample_rate % 65536;
        p_box->data.p_sample_soun->i_sampleratelo = f_sample_rate / 65536;

        MP4_GET4BYTES( i_channel );
        p_box->data.p_sample_soun->i_channelcount = i_channel;

#ifdef MP4_VERBOSE
        msg_Dbg( p_stream, "read box: \"soun\" V2" );
#endif
        stream_Seek( p_stream, p_box->i_pos +
                        MP4_BOX_HEADERSIZE( p_box ) + 28 + 36 );
    }
    else
    {
        p_box->data.p_sample_soun->i_sample_per_packet = 0;
        p_box->data.p_sample_soun->i_bytes_per_packet = 0;
        p_box->data.p_sample_soun->i_bytes_per_frame = 0;
        p_box->data.p_sample_soun->i_bytes_per_sample = 0;

        msg_Dbg( p_stream, "read box: \"soun\" mp4 or qt1/2 (rest="I64Fd")",
                 i_read );
        stream_Seek( p_stream, p_box->i_pos +
                        MP4_BOX_HEADERSIZE( p_box ) + 28 );
    }

    if( p_box->i_type == FOURCC_drms )
    {
        p_box->data.p_sample_soun->p_drms =
            drms_alloc( p_stream->p_vlc->psz_homedir );

        if( p_box->data.p_sample_soun->p_drms == NULL )
        {
            msg_Err( p_stream, "drms_alloc() failed" );
        }
    }

    if( p_box->i_type == FOURCC_samr || p_box->i_type == FOURCC_sawb )
    {
        /* Ignore channelcount for AMR (3gpp AMRSpecificBox) */
        p_box->data.p_sample_soun->i_channelcount = 1;
    }

    if( p_box->i_type == FOURCC_alac )
    {
        if( p_box->data.p_sample_soun->p_qt_description )
            free( p_box->data.p_sample_soun->p_qt_description );

        p_box->data.p_sample_soun->p_qt_description = malloc( i_read );
        p_box->data.p_sample_soun->i_qt_description = i_read;
        memcpy( p_box->data.p_sample_soun->p_qt_description, p_peek, i_read );
    }
    else
    {
        MP4_ReadBoxContainerRaw( p_stream, p_box ); /* esds */
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"soun\" in stsd channel %d "
             "sample size %d sample rate %f",
             p_box->data.p_sample_soun->i_channelcount,
             p_box->data.p_sample_soun->i_samplesize,
             (float)p_box->data.p_sample_soun->i_sampleratehi +
             (float)p_box->data.p_sample_soun->i_sampleratelo / 65536 );

#endif
    MP4_READBOX_EXIT( 1 );
}


static void MP4_FreeBox_sample_soun( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_sample_soun->p_qt_description );

    if( p_box->i_type == FOURCC_drms )
    {
        if( p_box->data.p_sample_soun->p_drms )
        {
            drms_free( p_box->data.p_sample_soun->p_drms );
        }
    }
}


int MP4_ReadBox_sample_vide( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;

    MP4_READBOX_ENTER( MP4_Box_data_sample_vide_t );

    for( i = 0; i < 6 ; i++ )
    {
        MP4_GET1BYTE( p_box->data.p_sample_vide->i_reserved1[i] );
    }

    MP4_GET2BYTES( p_box->data.p_sample_vide->i_data_reference_index );

    /*
     * XXX hack -> produce a copy of the nearly complete chunk
     */
    if( i_read > 0 )
    {
        p_box->data.p_sample_vide->i_qt_image_description = i_read;
        p_box->data.p_sample_vide->p_qt_image_description = malloc( i_read );
        memcpy( p_box->data.p_sample_vide->p_qt_image_description,
                p_peek, i_read );
    }
    else
    {
        p_box->data.p_sample_vide->i_qt_image_description = 0;
        p_box->data.p_sample_vide->p_qt_image_description = NULL;
    }

    MP4_GET2BYTES( p_box->data.p_sample_vide->i_qt_version );
    MP4_GET2BYTES( p_box->data.p_sample_vide->i_qt_revision_level );
    MP4_GET4BYTES( p_box->data.p_sample_vide->i_qt_vendor );

    MP4_GET4BYTES( p_box->data.p_sample_vide->i_qt_temporal_quality );
    MP4_GET4BYTES( p_box->data.p_sample_vide->i_qt_spatial_quality );

    MP4_GET2BYTES( p_box->data.p_sample_vide->i_width );
    MP4_GET2BYTES( p_box->data.p_sample_vide->i_height );

    MP4_GET4BYTES( p_box->data.p_sample_vide->i_horizresolution );
    MP4_GET4BYTES( p_box->data.p_sample_vide->i_vertresolution );

    MP4_GET4BYTES( p_box->data.p_sample_vide->i_qt_data_size );
    MP4_GET2BYTES( p_box->data.p_sample_vide->i_qt_frame_count );

    memcpy( &p_box->data.p_sample_vide->i_compressorname, p_peek, 32 );
    p_peek += 32; i_read -= 32;

    MP4_GET2BYTES( p_box->data.p_sample_vide->i_depth );
    MP4_GET2BYTES( p_box->data.p_sample_vide->i_qt_color_table );

    stream_Seek( p_stream, p_box->i_pos + MP4_BOX_HEADERSIZE( p_box ) + 78);
    MP4_ReadBoxContainerRaw( p_stream, p_box );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"vide\" in stsd %dx%d depth %d",
                      p_box->data.p_sample_vide->i_width,
                      p_box->data.p_sample_vide->i_height,
                      p_box->data.p_sample_vide->i_depth );

#endif
    MP4_READBOX_EXIT( 1 );
}


void MP4_FreeBox_sample_vide( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_sample_vide->p_qt_image_description );
}

static int MP4_ReadBox_sample_mp4s( stream_t *p_stream, MP4_Box_t *p_box )
{
    stream_Seek( p_stream, p_box->i_pos + MP4_BOX_HEADERSIZE( p_box ) + 8 );
    MP4_ReadBoxContainerRaw( p_stream, p_box );
    return 1;
}

static int MP4_ReadBox_sample_text( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;
    int32_t t;

    MP4_READBOX_ENTER( MP4_Box_data_sample_text_t );

    MP4_GET4BYTES( p_box->data.p_sample_text->i_reserved1 );
    MP4_GET2BYTES( p_box->data.p_sample_text->i_reserved2 );

    MP4_GET2BYTES( p_box->data.p_sample_text->i_data_reference_index );

    MP4_GET4BYTES( p_box->data.p_sample_text->i_display_flags );

    MP4_GET4BYTES( t );
    switch( t )
    {
        /* FIXME search right signification */
        case 1: // Center
            p_box->data.p_sample_text->i_justification_horizontal = 1;
            p_box->data.p_sample_text->i_justification_vertical = 1;
            break;
        case -1:    // Flush Right
            p_box->data.p_sample_text->i_justification_horizontal = -1;
            p_box->data.p_sample_text->i_justification_vertical = -1;
            break;
        case -2:    // Flush Left
            p_box->data.p_sample_text->i_justification_horizontal = 0;
            p_box->data.p_sample_text->i_justification_vertical = 0;
            break;
        case 0: // Flush Default
        default:
            p_box->data.p_sample_text->i_justification_horizontal = 1;
            p_box->data.p_sample_text->i_justification_vertical = -1;
            break;
    }

    MP4_GET2BYTES( p_box->data.p_sample_text->i_background_color[0] );
    MP4_GET2BYTES( p_box->data.p_sample_text->i_background_color[1] );
    MP4_GET2BYTES( p_box->data.p_sample_text->i_background_color[2] );
    p_box->data.p_sample_text->i_background_color[3] = 0;

    MP4_GET2BYTES( p_box->data.p_sample_text->i_text_box_top );
    MP4_GET2BYTES( p_box->data.p_sample_text->i_text_box_left );
    MP4_GET2BYTES( p_box->data.p_sample_text->i_text_box_bottom );
    MP4_GET2BYTES( p_box->data.p_sample_text->i_text_box_right );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"text\" in stsd text" );
#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_sample_tx3g( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;
    int32_t t;

    MP4_READBOX_ENTER( MP4_Box_data_sample_text_t );

    MP4_GET4BYTES( p_box->data.p_sample_text->i_reserved1 );
    MP4_GET2BYTES( p_box->data.p_sample_text->i_reserved2 );

    MP4_GET2BYTES( p_box->data.p_sample_text->i_data_reference_index );

    MP4_GET4BYTES( p_box->data.p_sample_text->i_display_flags );

    MP4_GET1BYTE ( p_box->data.p_sample_text->i_justification_horizontal );
    MP4_GET1BYTE ( p_box->data.p_sample_text->i_justification_vertical );

    MP4_GET1BYTE ( p_box->data.p_sample_text->i_background_color[0] );
    MP4_GET1BYTE ( p_box->data.p_sample_text->i_background_color[1] );
    MP4_GET1BYTE ( p_box->data.p_sample_text->i_background_color[2] );
    MP4_GET1BYTE ( p_box->data.p_sample_text->i_background_color[3] );

    MP4_GET2BYTES( p_box->data.p_sample_text->i_text_box_top );
    MP4_GET2BYTES( p_box->data.p_sample_text->i_text_box_left );
    MP4_GET2BYTES( p_box->data.p_sample_text->i_text_box_bottom );
    MP4_GET2BYTES( p_box->data.p_sample_text->i_text_box_right );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"text\" in stsd text" );
#endif
    MP4_READBOX_EXIT( 1 );
}


#if 0
/* We can't easily call it, and anyway ~ 20 bytes lost isn't a real problem */
static void MP4_FreeBox_sample_text( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_sample_text->psz_text_name );
}
#endif


static int MP4_ReadBox_stsd( stream_t *p_stream, MP4_Box_t *p_box )
{

    MP4_READBOX_ENTER( MP4_Box_data_stsd_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_stsd );

    MP4_GET4BYTES( p_box->data.p_stsd->i_entry_count );

    stream_Seek( p_stream, p_box->i_pos + MP4_BOX_HEADERSIZE( p_box ) + 8 );

    MP4_ReadBoxContainerRaw( p_stream, p_box );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"stsd\" entry-count %d",
                      p_box->data.p_stsd->i_entry_count );

#endif
    MP4_READBOX_EXIT( 1 );
}


static int MP4_ReadBox_stsz( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;

    MP4_READBOX_ENTER( MP4_Box_data_stsz_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_stsz );

    MP4_GET4BYTES( p_box->data.p_stsz->i_sample_size );

    MP4_GET4BYTES( p_box->data.p_stsz->i_sample_count );

    p_box->data.p_stsz->i_entry_size =
        calloc( sizeof( uint32_t ), p_box->data.p_stsz->i_sample_count );

    if( !p_box->data.p_stsz->i_sample_size )
    {
        for( i=0; (i<p_box->data.p_stsz->i_sample_count)&&(i_read >= 4 ); i++ )
        {
            MP4_GET4BYTES( p_box->data.p_stsz->i_entry_size[i] );
        }
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"stsz\" sample-size %d sample-count %d",
                      p_box->data.p_stsz->i_sample_size,
                      p_box->data.p_stsz->i_sample_count );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_stsz( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_stsz->i_entry_size );
}

static int MP4_ReadBox_stsc( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;

    MP4_READBOX_ENTER( MP4_Box_data_stsc_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_stsc );

    MP4_GET4BYTES( p_box->data.p_stsc->i_entry_count );

    p_box->data.p_stsc->i_first_chunk =
        calloc( sizeof( uint32_t ), p_box->data.p_stsc->i_entry_count );
    p_box->data.p_stsc->i_samples_per_chunk =
        calloc( sizeof( uint32_t ), p_box->data.p_stsc->i_entry_count );
    p_box->data.p_stsc->i_sample_description_index =
        calloc( sizeof( uint32_t ), p_box->data.p_stsc->i_entry_count );

    for( i = 0; (i < p_box->data.p_stsc->i_entry_count )&&( i_read >= 12 );i++ )
    {
        MP4_GET4BYTES( p_box->data.p_stsc->i_first_chunk[i] );
        MP4_GET4BYTES( p_box->data.p_stsc->i_samples_per_chunk[i] );
        MP4_GET4BYTES( p_box->data.p_stsc->i_sample_description_index[i] );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"stsc\" entry-count %d",
                      p_box->data.p_stsc->i_entry_count );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_stsc( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_stsc->i_first_chunk );
    FREE( p_box->data.p_stsc->i_samples_per_chunk );
    FREE( p_box->data.p_stsc->i_sample_description_index );
}

static int MP4_ReadBox_stco_co64( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;

    MP4_READBOX_ENTER( MP4_Box_data_co64_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_co64 );

    MP4_GET4BYTES( p_box->data.p_co64->i_entry_count );

    p_box->data.p_co64->i_chunk_offset =
        calloc( sizeof( uint64_t ), p_box->data.p_co64->i_entry_count );

    for( i = 0; i < p_box->data.p_co64->i_entry_count; i++ )
    {
        if( p_box->i_type == FOURCC_stco )
        {
            if( i_read < 4 )
            {
                break;
            }
            MP4_GET4BYTES( p_box->data.p_co64->i_chunk_offset[i] );
        }
        else
        {
            if( i_read < 8 )
            {
                break;
            }
            MP4_GET8BYTES( p_box->data.p_co64->i_chunk_offset[i] );
        }
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"co64\" entry-count %d",
                      p_box->data.p_co64->i_entry_count );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_stco_co64( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_co64->i_chunk_offset );
}

static int MP4_ReadBox_stss( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;

    MP4_READBOX_ENTER( MP4_Box_data_stss_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_stss );

    MP4_GET4BYTES( p_box->data.p_stss->i_entry_count );

    p_box->data.p_stss->i_sample_number =
        calloc( sizeof( uint32_t ), p_box->data.p_stss->i_entry_count );

    for( i = 0; (i < p_box->data.p_stss->i_entry_count )&&( i_read >= 4 ); i++ )
    {

        MP4_GET4BYTES( p_box->data.p_stss->i_sample_number[i] );
        /* XXX in libmp4 sample begin at 0 */
        p_box->data.p_stss->i_sample_number[i]--;
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"stss\" entry-count %d",
                      p_box->data.p_stss->i_entry_count );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_stss( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_stss->i_sample_number )
}

static int MP4_ReadBox_stsh( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;

    MP4_READBOX_ENTER( MP4_Box_data_stsh_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_stsh );


    MP4_GET4BYTES( p_box->data.p_stsh->i_entry_count );

    p_box->data.p_stsh->i_shadowed_sample_number =
        calloc( sizeof( uint32_t ), p_box->data.p_stsh->i_entry_count );

    p_box->data.p_stsh->i_sync_sample_number =
        calloc( sizeof( uint32_t ), p_box->data.p_stsh->i_entry_count );


    for( i = 0; (i < p_box->data.p_stss->i_entry_count )&&( i_read >= 8 ); i++ )
    {

        MP4_GET4BYTES( p_box->data.p_stsh->i_shadowed_sample_number[i] );
        MP4_GET4BYTES( p_box->data.p_stsh->i_sync_sample_number[i] );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"stsh\" entry-count %d",
                      p_box->data.p_stsh->i_entry_count );
#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_stsh( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_stsh->i_shadowed_sample_number )
    FREE( p_box->data.p_stsh->i_sync_sample_number )
}


static int MP4_ReadBox_stdp( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;

    MP4_READBOX_ENTER( MP4_Box_data_stdp_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_stdp );

    p_box->data.p_stdp->i_priority =
        calloc( sizeof( uint16_t ), i_read / 2 );

    for( i = 0; i < i_read / 2 ; i++ )
    {

        MP4_GET2BYTES( p_box->data.p_stdp->i_priority[i] );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"stdp\" entry-count "I64Fd,
                      i_read / 2 );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_stdp( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_stdp->i_priority )
}

static int MP4_ReadBox_padb( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;

    MP4_READBOX_ENTER( MP4_Box_data_padb_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_padb );


    MP4_GET4BYTES( p_box->data.p_padb->i_sample_count );

    p_box->data.p_padb->i_reserved1 =
        calloc( sizeof( uint16_t ), ( p_box->data.p_padb->i_sample_count + 1 ) / 2 );
    p_box->data.p_padb->i_pad2 =
        calloc( sizeof( uint16_t ), ( p_box->data.p_padb->i_sample_count + 1 ) / 2 );
    p_box->data.p_padb->i_reserved2 =
        calloc( sizeof( uint16_t ), ( p_box->data.p_padb->i_sample_count + 1 ) / 2 );
    p_box->data.p_padb->i_pad1 =
        calloc( sizeof( uint16_t ), ( p_box->data.p_padb->i_sample_count + 1 ) / 2 );


    for( i = 0; i < i_read / 2 ; i++ )
    {
        p_box->data.p_padb->i_reserved1[i] = ( (*p_peek) >> 7 )&0x01;
        p_box->data.p_padb->i_pad2[i] = ( (*p_peek) >> 4 )&0x07;
        p_box->data.p_padb->i_reserved1[i] = ( (*p_peek) >> 3 )&0x01;
        p_box->data.p_padb->i_pad1[i] = ( (*p_peek) )&0x07;

        p_peek += 1; i_read -= 1;
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"stdp\" entry-count "I64Fd,
                      i_read / 2 );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_padb( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_padb->i_reserved1 );
    FREE( p_box->data.p_padb->i_pad2 );
    FREE( p_box->data.p_padb->i_reserved2 );
    FREE( p_box->data.p_padb->i_pad1 );
}

static int MP4_ReadBox_elst( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i;

    MP4_READBOX_ENTER( MP4_Box_data_padb_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_elst );


    MP4_GET4BYTES( p_box->data.p_elst->i_entry_count );

    p_box->data.p_elst->i_segment_duration =
        calloc( sizeof( uint64_t ), p_box->data.p_elst->i_entry_count );
    p_box->data.p_elst->i_media_time =
        calloc( sizeof( int64_t ),  p_box->data.p_elst->i_entry_count );
    p_box->data.p_elst->i_media_rate_integer =
        calloc( sizeof( uint16_t ), p_box->data.p_elst->i_entry_count );
    p_box->data.p_elst->i_media_rate_fraction=
        calloc( sizeof( uint16_t ), p_box->data.p_elst->i_entry_count );


    for( i = 0; i < p_box->data.p_elst->i_entry_count; i++ )
    {
        if( p_box->data.p_elst->i_version == 1 )
        {

            MP4_GET8BYTES( p_box->data.p_elst->i_segment_duration[i] );

            MP4_GET8BYTES( p_box->data.p_elst->i_media_time[i] );
        }
        else
        {

            MP4_GET4BYTES( p_box->data.p_elst->i_segment_duration[i] );

            MP4_GET4BYTES( p_box->data.p_elst->i_media_time[i] );
            p_box->data.p_elst->i_media_time[i] = (int32_t)p_box->data.p_elst->i_media_time[i];
        }

        MP4_GET2BYTES( p_box->data.p_elst->i_media_rate_integer[i] );
        MP4_GET2BYTES( p_box->data.p_elst->i_media_rate_fraction[i] );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"elst\" entry-count "I64Fd,
                      i_read / 2 );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_elst( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_elst->i_segment_duration );
    FREE( p_box->data.p_elst->i_media_time );
    FREE( p_box->data.p_elst->i_media_rate_integer );
    FREE( p_box->data.p_elst->i_media_rate_fraction );
}

static int MP4_ReadBox_cprt( stream_t *p_stream, MP4_Box_t *p_box )
{
    unsigned int i_language;
    unsigned int i;

    MP4_READBOX_ENTER( MP4_Box_data_cprt_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_cprt );

    i_language = GetWBE( p_peek );
    for( i = 0; i < 3; i++ )
    {
        p_box->data.p_cprt->i_language[i] =
            ( ( i_language >> ( (2-i)*5 ) )&0x1f ) + 0x60;
    }
    p_peek += 2; i_read -= 2;
    MP4_GETSTRINGZ( p_box->data.p_cprt->psz_notice );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"cprt\" language %c%c%c notice %s",
                      p_box->data.p_cprt->i_language[0],
                      p_box->data.p_cprt->i_language[1],
                      p_box->data.p_cprt->i_language[2],
                      p_box->data.p_cprt->psz_notice );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_cprt( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_cprt->psz_notice );
}


static int MP4_ReadBox_dcom( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_dcom_t );

    MP4_GETFOURCC( p_box->data.p_dcom->i_algorithm );
#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"dcom\" compression algorithm : %4.4s",
                      (char*)&p_box->data.p_dcom->i_algorithm );
#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_cmvd( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_cmvd_t );

    MP4_GET4BYTES( p_box->data.p_cmvd->i_uncompressed_size );

    p_box->data.p_cmvd->i_compressed_size = i_read;

    if( !( p_box->data.p_cmvd->p_data = malloc( i_read ) ) )
    {
        msg_Dbg( p_stream, "read box: \"cmvd\" not enough memory to load data" );
        return( 1 );
    }

    /* now copy compressed data */
    memcpy( p_box->data.p_cmvd->p_data,
            p_peek,
            i_read);

    p_box->data.p_cmvd->b_compressed = 1;

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"cmvd\" compressed data size %d",
                      p_box->data.p_cmvd->i_compressed_size );
#endif

    MP4_READBOX_EXIT( 1 );
}
static void MP4_FreeBox_cmvd( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_cmvd->p_data );
}


static int MP4_ReadBox_cmov( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_Box_t *p_dcom;
    MP4_Box_t *p_cmvd;

#ifdef HAVE_ZLIB_H
    stream_t *p_stream_memory;
    z_stream z_data;
    uint8_t *p_data;
    int i_result;
#endif

    if( !( p_box->data.p_cmov = malloc( sizeof( MP4_Box_data_cmov_t ) ) ) )
    {
        msg_Err( p_stream, "out of memory" );
        return 0;
    }
    memset( p_box->data.p_cmov, 0, sizeof( MP4_Box_data_cmov_t ) );

    if( !p_box->p_father ||
        ( p_box->p_father->i_type != FOURCC_moov &&
          p_box->p_father->i_type != FOURCC_foov ) )
    {
        msg_Warn( p_stream, "Read box: \"cmov\" box alone" );
        return 1;
    }

    if( !MP4_ReadBoxContainer( p_stream, p_box ) )
    {
        return 0;
    }

    if( ( p_dcom = MP4_BoxGet( p_box, "dcom" ) ) == NULL ||
        ( p_cmvd = MP4_BoxGet( p_box, "cmvd" ) ) == NULL ||
        p_cmvd->data.p_cmvd->p_data == NULL )
    {
        msg_Warn( p_stream, "read box: \"cmov\" incomplete" );
        return 1;
    }

    if( p_dcom->data.p_dcom->i_algorithm != FOURCC_zlib )
    {
        msg_Dbg( p_stream, "read box: \"cmov\" compression algorithm : %4.4s "
                 "not supported", (char*)&p_dcom->data.p_dcom->i_algorithm );
        return 1;
    }

#ifndef HAVE_ZLIB_H
    msg_Dbg( p_stream, "read box: \"cmov\" zlib unsupported" );
    return 1;

#else
    /* decompress data */
    /* allocate a new buffer */
    if( !( p_data = malloc( p_cmvd->data.p_cmvd->i_uncompressed_size ) ) )
    {
        msg_Err( p_stream, "read box: \"cmov\" not enough memory to "
                 "uncompress data" );
        return 1;
    }
    /* init default structures */
    z_data.next_in   = p_cmvd->data.p_cmvd->p_data;
    z_data.avail_in  = p_cmvd->data.p_cmvd->i_compressed_size;
    z_data.next_out  = p_data;
    z_data.avail_out = p_cmvd->data.p_cmvd->i_uncompressed_size;
    z_data.zalloc    = (alloc_func)Z_NULL;
    z_data.zfree     = (free_func)Z_NULL;
    z_data.opaque    = (voidpf)Z_NULL;

    /* init zlib */
    if( inflateInit( &z_data ) != Z_OK )
    {
        msg_Err( p_stream, "read box: \"cmov\" error while uncompressing" );
        free( p_data );
        return 1;
    }

    /* uncompress */
    i_result = inflate( &z_data, Z_NO_FLUSH );
    if( i_result != Z_OK && i_result != Z_STREAM_END )
    {
        msg_Err( p_stream, "read box: \"cmov\" error while uncompressing" );
        free( p_data );
        return 1;
    }

    if( p_cmvd->data.p_cmvd->i_uncompressed_size != z_data.total_out )
    {
        msg_Warn( p_stream, "read box: \"cmov\" uncompressing data size "
                  "mismatch" );
    }
    p_cmvd->data.p_cmvd->i_uncompressed_size = z_data.total_out;

    /* close zlib */
    if( inflateEnd( &z_data ) != Z_OK )
    {
        msg_Warn( p_stream, "read box: \"cmov\" error while uncompressing "
                  "data (ignored)" );
    }

    free( p_cmvd->data.p_cmvd->p_data );
    p_cmvd->data.p_cmvd->p_data = p_data;
    p_cmvd->data.p_cmvd->b_compressed = 0;

    msg_Dbg( p_stream, "read box: \"cmov\" box succesfully uncompressed" );

    /* now create a memory stream */
    p_stream_memory =
        stream_MemoryNew( VLC_OBJECT(p_stream), p_cmvd->data.p_cmvd->p_data,
                          p_cmvd->data.p_cmvd->i_uncompressed_size, VLC_TRUE );

    /* and read uncompressd moov */
    p_box->data.p_cmov->p_moov = MP4_ReadBox( p_stream_memory, NULL );

    stream_Delete( p_stream_memory );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"cmov\" compressed movie header completed");
#endif

    return p_box->data.p_cmov->p_moov ? 1 : 0;
#endif /* HAVE_ZLIB_H */
}

static int MP4_ReadBox_rdrf( stream_t *p_stream, MP4_Box_t *p_box )
{
    uint32_t i_len;
    MP4_READBOX_ENTER( MP4_Box_data_rdrf_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_rdrf );
    MP4_GETFOURCC( p_box->data.p_rdrf->i_ref_type );
    MP4_GET4BYTES( i_len );
    if( i_len > 0 )
    {
        uint32_t i;
        p_box->data.p_rdrf->psz_ref = malloc( i_len  + 1);
        for( i = 0; i < i_len; i++ )
        {
            MP4_GET1BYTE( p_box->data.p_rdrf->psz_ref[i] );
        }
        p_box->data.p_rdrf->psz_ref[i_len] = '\0';
    }
    else
    {
        p_box->data.p_rdrf->psz_ref = NULL;
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"rdrf\" type:%4.4s ref %s",
             (char*)&p_box->data.p_rdrf->i_ref_type,
             p_box->data.p_rdrf->psz_ref );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_rdrf( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_rdrf->psz_ref )
}


static int MP4_ReadBox_rmdr( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_rmdr_t );

    MP4_GETVERSIONFLAGS( p_box->data.p_rmdr );

    MP4_GET4BYTES( p_box->data.p_rmdr->i_rate );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"rmdr\" rate:%d",
             p_box->data.p_rmdr->i_rate );
#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_rmqu( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_rmqu_t );

    MP4_GET4BYTES( p_box->data.p_rmqu->i_quality );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"rmqu\" quality:%d",
             p_box->data.p_rmqu->i_quality );
#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_rmvc( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_rmvc_t );
    MP4_GETVERSIONFLAGS( p_box->data.p_rmvc );

    MP4_GETFOURCC( p_box->data.p_rmvc->i_gestaltType );
    MP4_GET4BYTES( p_box->data.p_rmvc->i_val1 );
    MP4_GET4BYTES( p_box->data.p_rmvc->i_val2 );
    MP4_GET2BYTES( p_box->data.p_rmvc->i_checkType );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"rmvc\" gestaltType:%4.4s val1:0x%x val2:0x%x checkType:0x%x",
             (char*)&p_box->data.p_rmvc->i_gestaltType,
             p_box->data.p_rmvc->i_val1,p_box->data.p_rmvc->i_val2,
             p_box->data.p_rmvc->i_checkType );
#endif

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_drms( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_Box_t *p_drms_box = p_box;

    MP4_READBOX_ENTER( uint8_t );

    do
    {
        p_drms_box = p_drms_box->p_father;
    } while( p_drms_box && p_drms_box->i_type != FOURCC_drms );

    if( p_drms_box && p_drms_box->data.p_sample_soun->p_drms )
    {
        if( drms_init( p_drms_box->data.p_sample_soun->p_drms,
                       p_box->i_type, p_peek, i_read ) )
        {
            msg_Err( p_stream, "drms_init( %4.4s ) failed",
                     (char *)&p_box->i_type );

            drms_free( p_drms_box->data.p_sample_soun->p_drms );
            p_drms_box->data.p_sample_soun->p_drms = NULL;
        }
    }

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_0xa9xxx( stream_t *p_stream, MP4_Box_t *p_box )
{
    int16_t i_length, i_dummy;

    MP4_READBOX_ENTER( MP4_Box_data_0xa9xxx_t );

    p_box->data.p_0xa9xxx->psz_text = NULL;

    MP4_GET2BYTES( i_length );
    MP4_GET2BYTES( i_dummy );

    if( i_length > 0 )
    {
        if( i_length > i_read ) i_length = i_read;

        p_box->data.p_0xa9xxx->psz_text = malloc( i_length + 1 );

        memcpy( p_box->data.p_0xa9xxx->psz_text,
                p_peek, i_length );
        p_box->data.p_0xa9xxx->psz_text[i_length] = '\0';

#ifdef MP4_VERBOSE
        msg_Dbg( p_stream,
                 "read box: \"%4.4s\" text=`%s'",
                 (char*)&p_box->i_type,
                 p_box->data.p_0xa9xxx->psz_text );
#endif
    }

    MP4_READBOX_EXIT( 1 );
}
static void MP4_FreeBox_0xa9xxx( MP4_Box_t *p_box )
{
    FREE( p_box->data.p_0xa9xxx->psz_text );
}

/* For generic */
static int MP4_ReadBox_default( stream_t *p_stream, MP4_Box_t *p_box )
{
    if( !p_box->p_father )
    {
        goto unknown;
    }
    if( p_box->p_father->i_type == FOURCC_stsd )
    {
        MP4_Box_t *p_mdia = MP4_BoxGet( p_box, "../../../.." );
        MP4_Box_t *p_hdlr;

        if( p_mdia == NULL || p_mdia->i_type != FOURCC_mdia ||
            (p_hdlr = MP4_BoxGet( p_mdia, "hdlr" )) == NULL )
        {
            goto unknown;
        }
        switch( p_hdlr->data.p_hdlr->i_handler_type )
        {
            case FOURCC_soun:
                return MP4_ReadBox_sample_soun( p_stream, p_box );
            case FOURCC_vide:
                return MP4_ReadBox_sample_vide( p_stream, p_box );
            case FOURCC_text:
                return MP4_ReadBox_sample_text( p_stream, p_box );
            case FOURCC_tx3g:
                return MP4_ReadBox_sample_tx3g( p_stream, p_box );
            default:
                msg_Warn( p_stream,
                          "unknown handler type in stsd (uncompletetly loaded)" );
                return 1;
        }
    }

unknown:
    msg_Warn( p_stream,
              "unknown box type %4.4s (uncompletetly loaded)",
              (char*)&p_box->i_type );

    return 1;
}

/**** ------------------------------------------------------------------- ****/
/****                   "Higher level" Functions                          ****/
/**** ------------------------------------------------------------------- ****/

static struct
{
    uint32_t i_type;
    int  (*MP4_ReadBox_function )( stream_t *p_stream, MP4_Box_t *p_box );
    void (*MP4_FreeBox_function )( MP4_Box_t *p_box );
} MP4_Box_Function [] =
{
    /* Containers */
    { FOURCC_moov,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },
    { FOURCC_trak,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },
    { FOURCC_mdia,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },
    { FOURCC_moof,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },
    { FOURCC_minf,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },
    { FOURCC_stbl,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },
    { FOURCC_dinf,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },
    { FOURCC_edts,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },
    { FOURCC_udta,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },
    { FOURCC_nmhd,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },
    { FOURCC_hnti,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },
    { FOURCC_rmra,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },
    { FOURCC_rmda,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },
    { FOURCC_tref,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },
    { FOURCC_gmhd,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },
    { FOURCC_wave,  MP4_ReadBoxContainer,   MP4_FreeBox_Common },

    /* specific box */
    { FOURCC_ftyp,  MP4_ReadBox_ftyp,       MP4_FreeBox_ftyp },
    { FOURCC_cmov,  MP4_ReadBox_cmov,       MP4_FreeBox_Common },
    { FOURCC_mvhd,  MP4_ReadBox_mvhd,       MP4_FreeBox_Common },
    { FOURCC_tkhd,  MP4_ReadBox_tkhd,       MP4_FreeBox_Common },
    { FOURCC_mdhd,  MP4_ReadBox_mdhd,       MP4_FreeBox_Common },
    { FOURCC_hdlr,  MP4_ReadBox_hdlr,       MP4_FreeBox_hdlr },
    { FOURCC_vmhd,  MP4_ReadBox_vmhd,       MP4_FreeBox_Common },
    { FOURCC_smhd,  MP4_ReadBox_smhd,       MP4_FreeBox_Common },
    { FOURCC_hmhd,  MP4_ReadBox_hmhd,       MP4_FreeBox_Common },
    { FOURCC_url,   MP4_ReadBox_url,        MP4_FreeBox_url },
    { FOURCC_urn,   MP4_ReadBox_urn,        MP4_FreeBox_urn },
    { FOURCC_dref,  MP4_ReadBox_dref,       MP4_FreeBox_Common },
    { FOURCC_stts,  MP4_ReadBox_stts,       MP4_FreeBox_stts },
    { FOURCC_ctts,  MP4_ReadBox_ctts,       MP4_FreeBox_ctts },
    { FOURCC_stsd,  MP4_ReadBox_stsd,       MP4_FreeBox_Common },
    { FOURCC_stsz,  MP4_ReadBox_stsz,       MP4_FreeBox_stsz },
    { FOURCC_stsc,  MP4_ReadBox_stsc,       MP4_FreeBox_stsc },
    { FOURCC_stco,  MP4_ReadBox_stco_co64,  MP4_FreeBox_stco_co64 },
    { FOURCC_co64,  MP4_ReadBox_stco_co64,  MP4_FreeBox_stco_co64 },
    { FOURCC_stss,  MP4_ReadBox_stss,       MP4_FreeBox_stss },
    { FOURCC_stsh,  MP4_ReadBox_stsh,       MP4_FreeBox_stsh },
    { FOURCC_stdp,  MP4_ReadBox_stdp,       MP4_FreeBox_stdp },
    { FOURCC_padb,  MP4_ReadBox_padb,       MP4_FreeBox_padb },
    { FOURCC_elst,  MP4_ReadBox_elst,       MP4_FreeBox_elst },
    { FOURCC_cprt,  MP4_ReadBox_cprt,       MP4_FreeBox_cprt },
    { FOURCC_esds,  MP4_ReadBox_esds,       MP4_FreeBox_esds },
    { FOURCC_dcom,  MP4_ReadBox_dcom,       MP4_FreeBox_Common },
    { FOURCC_cmvd,  MP4_ReadBox_cmvd,       MP4_FreeBox_cmvd },
    { FOURCC_avcC,  MP4_ReadBox_avcC,       MP4_FreeBox_avcC },

    /* Nothing to do with this box */
    { FOURCC_mdat,  MP4_ReadBoxSkip,        MP4_FreeBox_Common },
    { FOURCC_skip,  MP4_ReadBoxSkip,        MP4_FreeBox_Common },
    { FOURCC_free,  MP4_ReadBoxSkip,        MP4_FreeBox_Common },
    { FOURCC_wide,  MP4_ReadBoxSkip,        MP4_FreeBox_Common },

    /* for codecs */
    { FOURCC_soun,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_ms02,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_ms11,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_ms55,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC__mp3,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_mp4a,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_twos,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_sowt,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_QDMC,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_QDM2,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_ima4,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_IMA4,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_dvi,   MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_alaw,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_ulaw,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_raw,   MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_MAC3,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_MAC6,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_Qclp,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_samr,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_sawb,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_OggS,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_alac,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },

    { FOURCC_vide,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_mp4v,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_SVQ1,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_SVQ3,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_ZyGo,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_DIVX,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_XVID,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_h263,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_s263,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_cvid,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_3IV1,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_3iv1,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_3IV2,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_3iv2,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_3IVD,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_3ivd,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_3VID,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_3vid,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_mjpa,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_mjpb,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_qdrw,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_mp2v,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_hdv2,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },

    { FOURCC_mjqt,  MP4_ReadBox_default,        NULL }, /* found in mjpa/b */
    { FOURCC_mjht,  MP4_ReadBox_default,        NULL },

    { FOURCC_dvc,   MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_dvp,   MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_VP31,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_vp31,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_h264,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },

    { FOURCC_jpeg,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },
    { FOURCC_avc1,  MP4_ReadBox_sample_vide,    MP4_FreeBox_sample_vide },

    { FOURCC_mp4s,  MP4_ReadBox_sample_mp4s,    MP4_FreeBox_Common },

    /* XXX there is 2 box where we could find this entry stbl and tref*/
    { FOURCC_hint,  MP4_ReadBox_default,        MP4_FreeBox_Common },

    /* found in tref box */
    { FOURCC_dpnd,  MP4_ReadBox_default,   NULL },
    { FOURCC_ipir,  MP4_ReadBox_default,   NULL },
    { FOURCC_mpod,  MP4_ReadBox_default,   NULL },

    /* found in hnti */
    { FOURCC_rtp,   MP4_ReadBox_default,   NULL },

    /* found in rmra */
    { FOURCC_rdrf,  MP4_ReadBox_rdrf,           MP4_FreeBox_rdrf   },
    { FOURCC_rmdr,  MP4_ReadBox_rmdr,           MP4_FreeBox_Common },
    { FOURCC_rmqu,  MP4_ReadBox_rmqu,           MP4_FreeBox_Common },
    { FOURCC_rmvc,  MP4_ReadBox_rmvc,           MP4_FreeBox_Common },

    { FOURCC_drms,  MP4_ReadBox_sample_soun,    MP4_FreeBox_sample_soun },
    { FOURCC_sinf,  MP4_ReadBoxContainer,       MP4_FreeBox_Common },
    { FOURCC_schi,  MP4_ReadBoxContainer,       MP4_FreeBox_Common },
    { FOURCC_user,  MP4_ReadBox_drms,           MP4_FreeBox_Common },
    { FOURCC_key,   MP4_ReadBox_drms,           MP4_FreeBox_Common },
    { FOURCC_iviv,  MP4_ReadBox_drms,           MP4_FreeBox_Common },
    { FOURCC_name,  MP4_ReadBox_drms,           MP4_FreeBox_Common },
    { FOURCC_priv,  MP4_ReadBox_drms,           MP4_FreeBox_Common },

    /* found in udta */
    { FOURCC_0xa9nam,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9aut,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9cpy,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9swr,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9inf,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9ART,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9dir,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9cmt,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9req,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9day,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9des,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9fmt,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9prd,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9prf,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9src,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9alb,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9dis,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9enc,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9gen,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9trk,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9dsa,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9hst,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9url,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9ope,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },
    { FOURCC_0xa9com,MP4_ReadBox_0xa9xxx,       MP4_FreeBox_0xa9xxx },

    /* Last entry */
    { 0,             MP4_ReadBox_default,       NULL }
};



/*****************************************************************************
 * MP4_ReadBox : parse the actual box and the children
 *  XXX : Do not go to the next box
 *****************************************************************************/
static MP4_Box_t *MP4_ReadBox( stream_t *p_stream, MP4_Box_t *p_father )
{
    MP4_Box_t    *p_box = malloc( sizeof( MP4_Box_t ) );
    unsigned int i_index;

    if( !MP4_ReadBoxCommon( p_stream, p_box ) )
    {
        msg_Warn( p_stream, "cannot read one box" );
        free( p_box );
        return NULL;
    }
    if( !p_box->i_size )
    {
        msg_Dbg( p_stream, "found an empty box (null size)" );
        free( p_box );
        return NULL;
    }
    p_box->p_father = p_father;

    /* Now search function to call */
    for( i_index = 0; ; i_index++ )
    {
        if( ( MP4_Box_Function[i_index].i_type == p_box->i_type )||
            ( MP4_Box_Function[i_index].i_type == 0 ) )
        {
            break;
        }
    }

    if( !(MP4_Box_Function[i_index].MP4_ReadBox_function)( p_stream, p_box ) )
    {
        free( p_box );
        return NULL;
    }

    return p_box;
}

/*****************************************************************************
 * MP4_FreeBox : free memory after read with MP4_ReadBox and all
 * the children
 *****************************************************************************/
void MP4_BoxFree( stream_t *s, MP4_Box_t *p_box )
{
    unsigned int i_index;
    MP4_Box_t    *p_child;

    if( !p_box )
    {
        return; /* hehe */
    }

    for( p_child = p_box->p_first; p_child != NULL; )
    {
        MP4_Box_t *p_next;

        p_next = p_child->p_next;
        MP4_BoxFree( s, p_child );
        p_child = p_next;
    }

    /* Now search function to call */
    if( p_box->data.p_data )
    {
        for( i_index = 0; ; i_index++ )
        {
            if( ( MP4_Box_Function[i_index].i_type == p_box->i_type )||
                ( MP4_Box_Function[i_index].i_type == 0 ) )
            {
                break;
            }
        }
        if( MP4_Box_Function[i_index].MP4_FreeBox_function == NULL )
        {
            /* Should not happen */
            msg_Warn( s,
                      "cannot free box %4.4s, type unknown",
                      (char*)&p_box->i_type );
        }
        else
        {
            MP4_Box_Function[i_index].MP4_FreeBox_function( p_box );
        }

        free( p_box->data.p_data );
    }

    free( p_box );
}

/*****************************************************************************
 * MP4_BoxGetRoot : Parse the entire file, and create all boxes in memory
 *****************************************************************************
 *  The first box is a virtual box "root" and is the father for all first
 *  level boxes for the file, a sort of virtual contener
 *****************************************************************************/
MP4_Box_t *MP4_BoxGetRoot( stream_t *s )
{
    MP4_Box_t *p_root;
    stream_t *p_stream;
    int i_result;

    p_root = malloc( sizeof( MP4_Box_t ) );
    p_root->i_pos = 0;
    p_root->i_type = VLC_FOURCC( 'r', 'o', 'o', 't' );
    p_root->i_shortsize = 1;
    p_root->i_size = stream_Size( s );
    CreateUUID( &p_root->i_uuid, p_root->i_type );

    p_root->data.p_data = NULL;
    p_root->p_father = NULL;
    p_root->p_first  = NULL;
    p_root->p_last  = NULL;
    p_root->p_next   = NULL;

    p_stream = s;

    i_result = MP4_ReadBoxContainerRaw( p_stream, p_root );

    if( i_result )
    {
        MP4_Box_t *p_child;
        MP4_Box_t *p_moov;
        MP4_Box_t *p_cmov;

        /* check if there is a cmov, if so replace
          compressed moov by  uncompressed one */
        if( ( ( p_moov = MP4_BoxGet( p_root, "moov" ) ) &&
              ( p_cmov = MP4_BoxGet( p_root, "moov/cmov" ) ) ) ||
            ( ( p_moov = MP4_BoxGet( p_root, "foov" ) ) &&
              ( p_cmov = MP4_BoxGet( p_root, "foov/cmov" ) ) ) )
        {
            /* rename the compressed moov as a box to skip */
            p_moov->i_type = FOURCC_skip;

            /* get uncompressed p_moov */
            p_moov = p_cmov->data.p_cmov->p_moov;
            p_cmov->data.p_cmov->p_moov = NULL;

            /* make p_root father of this new moov */
            p_moov->p_father = p_root;

            /* insert this new moov box as first child of p_root */
            p_moov->p_next = p_child = p_root->p_first;
            p_root->p_first = p_moov;
        }
    }

    return p_root;
}


static void __MP4_BoxDumpStructure( stream_t *s,
                                    MP4_Box_t *p_box, unsigned int i_level )
{
    MP4_Box_t *p_child;

    if( !i_level )
    {
        msg_Dbg( s, "dumping root Box \"%4.4s\"",
                          (char*)&p_box->i_type );
    }
    else
    {
        char str[512];
        unsigned int i;
        memset( str, (uint8_t)' ', 512 );
        for( i = 0; i < i_level; i++ )
        {
            str[i*5] = '|';
        }
        sprintf( str + i_level * 5, "+ %4.4s size %d",
                      (char*)&p_box->i_type,
                      (uint32_t)p_box->i_size );

        msg_Dbg( s, "%s", str );
    }
    p_child = p_box->p_first;
    while( p_child )
    {
        __MP4_BoxDumpStructure( s, p_child, i_level + 1 );
        p_child = p_child->p_next;
    }
}

void MP4_BoxDumpStructure( stream_t *s, MP4_Box_t *p_box )
{
    __MP4_BoxDumpStructure( s, p_box, 0 );
}



/*****************************************************************************
 *****************************************************************************
 **
 **  High level methods to acces an MP4 file
 **
 *****************************************************************************
 *****************************************************************************/
static void __get_token( char **ppsz_path, char **ppsz_token, int *pi_number )
{
    size_t i_len ;
    if( !*ppsz_path[0] )
    {
        *ppsz_token = NULL;
        *pi_number = 0;
        return;
    }
    i_len = 0;
    while(  (*ppsz_path)[i_len] &&
            (*ppsz_path)[i_len] != '/' && (*ppsz_path)[i_len] != '[' )
    {
        i_len++;
    }
    if( !i_len && **ppsz_path == '/' )
    {
        i_len = 1;
    }
    *ppsz_token = malloc( i_len + 1 );

    memcpy( *ppsz_token, *ppsz_path, i_len );

    (*ppsz_token)[i_len] = '\0';

    *ppsz_path += i_len;

    if( **ppsz_path == '[' )
    {
        (*ppsz_path)++;
        *pi_number = strtol( *ppsz_path, NULL, 10 );
        while( **ppsz_path && **ppsz_path != ']' )
        {
            (*ppsz_path)++;
        }
        if( **ppsz_path == ']' )
        {
            (*ppsz_path)++;
        }
    }
    else
    {
        *pi_number = 0;
    }
    while( **ppsz_path == '/' )
    {
        (*ppsz_path)++;
    }
}

static void __MP4_BoxGet( MP4_Box_t **pp_result,
                          MP4_Box_t *p_box, char *psz_fmt, va_list args)
{
    char    *psz_path;

    if( !p_box )
    {
        *pp_result = NULL;
        return;
    }

    vasprintf( &psz_path, psz_fmt, args );

    if( !psz_path || !psz_path[0] )
    {
        FREE( psz_path );
        *pp_result = NULL;
        return;
    }

//    fprintf( stderr, "path:'%s'\n", psz_path );
    psz_fmt = psz_path; /* keep this pointer, as it need to be unallocated */
    for( ; ; )
    {
        char *psz_token;
        int i_number;

        __get_token( &psz_path, &psz_token, &i_number );
//        fprintf( stderr, "path:'%s', token:'%s' n:%d\n",
//                 psz_path,psz_token,i_number );
        if( !psz_token )
        {
            FREE( psz_token );
            free( psz_fmt );
            *pp_result = p_box;
            return;
        }
        else
        if( !strcmp( psz_token, "/" ) )
        {
            /* Find root box */
            while( p_box && p_box->i_type != VLC_FOURCC( 'r', 'o', 'o', 't' ) )
            {
                p_box = p_box->p_father;
            }
            if( !p_box )
            {
                free( psz_token );
                free( psz_fmt );
                *pp_result = NULL;
                return;
            }
        }
        else
        if( !strcmp( psz_token, "." ) )
        {
            /* Do nothing */
        }
        else
        if( !strcmp( psz_token, ".." ) )
        {
            p_box = p_box->p_father;
            if( !p_box )
            {
                free( psz_token );
                free( psz_fmt );
                *pp_result = NULL;
                return;
            }
        }
        else
        if( strlen( psz_token ) == 4 )
        {
            uint32_t i_fourcc;
            i_fourcc = VLC_FOURCC( psz_token[0], psz_token[1],
                                   psz_token[2], psz_token[3] );
            p_box = p_box->p_first;
            for( ; ; )
            {
                if( !p_box )
                {
                    free( psz_token );
                    free( psz_fmt );
                    *pp_result = NULL;
                    return;
                }
                if( p_box->i_type == i_fourcc )
                {
                    if( !i_number )
                    {
                        break;
                    }
                    i_number--;
                }
                p_box = p_box->p_next;
            }
        }
        else
        if( strlen( psz_token ) == 0 )
        {
            p_box = p_box->p_first;
            for( ; ; )
            {
                if( !p_box )
                {
                    free( psz_token );
                    free( psz_fmt );
                    *pp_result = NULL;
                    return;
                }
                if( !i_number )
                {
                    break;
                }
                i_number--;
                p_box = p_box->p_next;
            }
        }
        else
        {
//            fprintf( stderr, "Argg malformed token \"%s\"",psz_token );
            FREE( psz_token );
            free( psz_fmt );
            *pp_result = NULL;
            return;
        }

        free( psz_token );
    }
}

/*****************************************************************************
 * MP4_BoxGet: find a box given a path relative to p_box
 *****************************************************************************
 * Path Format: . .. / as usual
 *              [number] to specifie box number ex: trak[12]
 *
 * ex: /moov/trak[12]
 *     ../mdia
 *****************************************************************************/
MP4_Box_t *MP4_BoxGet( MP4_Box_t *p_box, char *psz_fmt, ... )
{
    va_list args;
    MP4_Box_t *p_result;

    va_start( args, psz_fmt );
    __MP4_BoxGet( &p_result, p_box, psz_fmt, args );
    va_end( args );

    return( p_result );
}

/*****************************************************************************
 * MP4_BoxCount: count box given a path relative to p_box
 *****************************************************************************
 * Path Format: . .. / as usual
 *              [number] to specifie box number ex: trak[12]
 *
 * ex: /moov/trak[12]
 *     ../mdia
 *****************************************************************************/
int MP4_BoxCount( MP4_Box_t *p_box, char *psz_fmt, ... )
{
    va_list args;
    int     i_count;
    MP4_Box_t *p_result, *p_next;

    va_start( args, psz_fmt );
    __MP4_BoxGet( &p_result, p_box, psz_fmt, args );
    va_end( args );
    if( !p_result )
    {
        return( 0 );
    }

    i_count = 1;
    for( p_next = p_result->p_next; p_next != NULL; p_next = p_next->p_next)
    {
        if( p_next->i_type == p_result->i_type)
        {
            i_count++;
        }
    }
    return( i_count );
}
