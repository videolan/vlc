/*****************************************************************************
 * libmp4.c : LibMP4 library for mp4 module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2004, 2010 VLC authors and VideoLAN
 *
 * Author: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_stream.h>                               /* vlc_stream_Peek*/
#include <vlc_strings.h>                              /* vlc_ascii_tolower */

#ifdef HAVE_ZLIB_H
#   include <zlib.h>                                  /* for compressed moov */
#endif

#include "libmp4.h"
#include "languages.h"
#include <math.h>
#include <assert.h>
#include <limits.h>

/* Some assumptions:
 * The input method HAS to be seekable
 */

/* convert 16.16 fixed point to floating point */
static double conv_fx( int32_t fx ) {
    double fp = fx;
    fp /= 65536.;
    return fp;
}

/* some functions for mp4 encoding of variables */
#ifdef MP4_VERBOSE
static void MP4_ConvertDate2Str( char *psz, uint64_t i_date, bool b_relative )
{
    int i_day;
    int i_hour;
    int i_min;
    int i_sec;

    /* date begin at 1 jan 1904 */
    if ( !b_relative )
        i_date += ((INT64_C(1904) * 365) + 17) * 24 * 60 * 60;

    i_day = i_date / ( 60*60*24);
    i_hour = ( i_date /( 60*60 ) ) % 60;
    i_min  = ( i_date / 60 ) % 60;
    i_sec =  i_date % 60;
    sprintf( psz, "%dd-%2.2dh:%2.2dm:%2.2ds", i_day, i_hour, i_min, i_sec );
}
#endif

#define MP4_GET1BYTE( dst )  MP4_GETX_PRIVATE( dst, *p_peek, 1 )
#define MP4_GET3BYTES( dst ) MP4_GETX_PRIVATE( dst, Get24bBE(p_peek), 3 )
#define MP4_GET4BYTES( dst ) MP4_GETX_PRIVATE( dst, GetDWBE(p_peek), 4 )
#define MP4_GET8BYTES( dst ) MP4_GETX_PRIVATE( dst, GetQWBE(p_peek), 8 )
#define MP4_GETFOURCC( dst ) MP4_GETX_PRIVATE( dst, \
                VLC_FOURCC(p_peek[0],p_peek[1],p_peek[2],p_peek[3]), 4)

#define MP4_GET2BYTESLE( dst ) MP4_GETX_PRIVATE( dst, GetWLE(p_peek), 2 )
#define MP4_GET4BYTESLE( dst ) MP4_GETX_PRIVATE( dst, GetDWLE(p_peek), 4 )
#define MP4_GET8BYTESLE( dst ) MP4_GETX_PRIVATE( dst, GetQWLE(p_peek), 8 )

#define MP4_GETVERSIONFLAGS( p_void ) \
    MP4_GET1BYTE( p_void->i_version ); \
    MP4_GET3BYTES( p_void->i_flags )

static char *mp4_getstringz( uint8_t **restrict in, uint64_t *restrict size )
{
    assert( *size <= SSIZE_MAX );

    size_t len = strnlen( (const char *)*in, *size );
    if( len == 0 || len >= *size )
        return NULL;

    len++;

    char *ret = malloc( len );
    if( likely(ret != NULL) )
        memcpy( ret, *in, len );
    *in += len;
    *size -= len;
    return ret;
}

#define MP4_GETSTRINGZ( p_str ) \
    do \
        (p_str) = mp4_getstringz( &p_peek, &i_read ); \
    while(0)

static uint8_t *mp4_readbox_enter_common( stream_t *s, MP4_Box_t *box,
                                          size_t typesize,
                                          void (*release)( MP4_Box_t * ),
                                          uint64_t readsize )
{
    const size_t headersize = mp4_box_headersize( box );

    if( unlikely(readsize < headersize) || unlikely(readsize > SSIZE_MAX) )
        return NULL;

    uint8_t *buf = malloc( readsize );
    if( unlikely(buf == NULL) )
        return NULL;

    ssize_t val = vlc_stream_Read( s, buf, readsize );
    if( (size_t)val != readsize )
    {
        msg_Warn( s, "mp4: wanted %"PRIu64" bytes, got %zd", readsize, val );
        goto error;
    }

    box->data.p_payload = malloc( typesize );
    if( unlikely(box->data.p_payload == NULL) )
        goto error;

    memset( box->data.p_payload, 0, typesize );
    box->pf_free = release;
    return buf;
error:
    free( buf );
    return NULL;
}

static uint8_t *mp4_readbox_enter_partial( stream_t *s, MP4_Box_t *box,
                                           size_t typesize,
                                           void (*release)( MP4_Box_t * ),
                                           uint64_t *restrict readsize )
{
    if( (uint64_t)*readsize > box->i_size )
        *readsize = box->i_size;

    return mp4_readbox_enter_common( s, box, typesize, release, *readsize );
}

static uint8_t *mp4_readbox_enter( stream_t *s, MP4_Box_t *box,
                                   size_t typesize,
                                   void (*release)( MP4_Box_t * ) )
{
    uint64_t readsize = box->i_size;
    return mp4_readbox_enter_common( s, box, typesize, release, readsize );
}


#define MP4_READBOX_ENTER_PARTIAL( MP4_Box_data_TYPE_t, maxread, release ) \
    uint64_t i_read = (maxread); \
    uint8_t *p_buff = mp4_readbox_enter_partial( p_stream, p_box, \
        sizeof( MP4_Box_data_TYPE_t ), release, &i_read ); \
    if( unlikely(p_buff == NULL) ) \
        return 0; \
    const size_t header_size = mp4_box_headersize( p_box ); \
    uint8_t *p_peek = p_buff + header_size; \
    i_read -= header_size

#define MP4_READBOX_ENTER( MP4_Box_data_TYPE_t, release ) \
    uint8_t *p_buff = mp4_readbox_enter( p_stream, p_box, \
        sizeof(MP4_Box_data_TYPE_t), release ); \
    if( unlikely(p_buff == NULL) ) \
        return 0; \
    uint64_t i_read = p_box->i_size; \
    const size_t header_size = mp4_box_headersize( p_box ); \
    uint8_t *p_peek = p_buff + header_size; \
    i_read -= header_size

#define MP4_READBOX_EXIT( i_code ) \
    do \
    { \
        free( p_buff ); \
        return( i_code ); \
    } while (0)

/*****************************************************************************
 * Some prototypes.
 *****************************************************************************/
static MP4_Box_t *MP4_ReadBox( stream_t *p_stream, MP4_Box_t *p_father );
static int MP4_Box_Read_Specific( stream_t *p_stream, MP4_Box_t *p_box, MP4_Box_t *p_father );
static void MP4_Box_Clean_Specific( MP4_Box_t *p_box );
static int MP4_PeekBoxHeader( stream_t *p_stream, MP4_Box_t *p_box );

int MP4_Seek( stream_t *p_stream, uint64_t i_pos )
{
    bool b_canseek = false;
    if ( vlc_stream_Control( p_stream, STREAM_CAN_SEEK, &b_canseek ) != VLC_SUCCESS ||
         b_canseek )
    {
        /* can seek or don't know */
        return vlc_stream_Seek( p_stream, i_pos );
    }
    /* obviously can't seek then */

    int64_t i_current_pos = vlc_stream_Tell( p_stream );
    if ( i_current_pos < 0 || i_pos < (uint64_t)i_current_pos )
        return VLC_EGENERIC;

    size_t i_toread = i_pos - i_current_pos;
    if( i_toread == 0 )
        return VLC_SUCCESS;
    else if( i_toread > (1<<17) )
        return VLC_EGENERIC;

    if( vlc_stream_Read( p_stream, NULL, i_toread ) != (ssize_t)i_toread )
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static void MP4_BoxAddChild( MP4_Box_t *p_parent, MP4_Box_t *p_childbox )
{
    if( !p_parent->p_first )
            p_parent->p_first = p_childbox;
    else
            p_parent->p_last->p_next = p_childbox;
    p_parent->p_last = p_childbox;
    p_childbox->p_father = p_parent;
}

MP4_Box_t * MP4_BoxExtract( MP4_Box_t **pp_chain, uint32_t i_type )
{
    MP4_Box_t *p_box = *pp_chain;
    while( p_box )
    {
        if( p_box->i_type == i_type )
        {
            *pp_chain = p_box->p_next;
            p_box->p_next = NULL;
            return p_box;
        }
        pp_chain = &p_box->p_next;
        p_box = p_box->p_next;
    }
    return NULL;
}

/* Don't use vlc_stream_Seek directly */
#undef vlc_stream_Seek
#define vlc_stream_Seek(a,b) __NO__

/*****************************************************************************
 * MP4_PeekBoxHeader : Load only common parameters for all boxes
 *****************************************************************************
 * p_box need to be an already allocated MP4_Box_t, and all data
 *  will only be peek not read
 *
 * RETURN : 0 if it fail, 1 otherwise
 *****************************************************************************/
static int MP4_PeekBoxHeader( stream_t *p_stream, MP4_Box_t *p_box )
{
    int      i_read;
    const uint8_t  *p_peek;

    if( ( ( i_read = vlc_stream_Peek( p_stream, &p_peek, 32 ) ) < 8 ) )
    {
        return 0;
    }
    p_box->i_pos = vlc_stream_Tell( p_stream );

    p_box->data.p_payload = NULL;
    p_box->p_father = NULL;
    p_box->p_first  = NULL;
    p_box->p_last  = NULL;
    p_box->p_next   = NULL;

    MP4_GET4BYTES( p_box->i_shortsize );
    MP4_GETFOURCC( p_box->i_type );

    /* Now special case */

    if( p_box->i_shortsize == 1 )
    {
        if( i_read < 8 )
            return 0;
        /* get the true size on 64 bits */
        MP4_GET8BYTES( p_box->i_size );
    }
    else
    {
        p_box->i_size = p_box->i_shortsize;
        /* XXX size of 0 means that the box extends to end of file */
    }

    if( UINT64_MAX - p_box->i_size < p_box->i_pos )
        return 0;

    if( p_box->i_type == ATOM_uuid )
    {
        if( i_read < 16 )
            return 0;
        /* get extented type on 16 bytes */
        GetUUID( &p_box->i_uuid, p_peek );
    }

#ifdef MP4_ULTRA_VERBOSE
    if( p_box->i_size )
    {
        if MP4_BOX_TYPE_ASCII()
            msg_Dbg( p_stream, "found Box: %4.4s size %"PRId64" %"PRId64,
                    (char*)&p_box->i_type, p_box->i_size, p_box->i_pos );
        else
            msg_Dbg( p_stream, "found Box: c%3.3s size %"PRId64,
                    (char*)&p_box->i_type+1, p_box->i_size );
    }
#endif

    return 1;
}

/*****************************************************************************
 * MP4_ReadBoxRestricted : Reads box from current position
 *****************************************************************************
 * if p_box == NULL, box is invalid or failed, position undefined
 * on success, position is past read box or EOF
 *****************************************************************************/
static MP4_Box_t *MP4_ReadBoxRestricted( stream_t *p_stream, MP4_Box_t *p_father,
                                         const uint32_t onlytypes[], const uint32_t nottypes[],
                                         bool *pb_restrictionhit )
{
    MP4_Box_t peekbox = { 0 };
    if ( !MP4_PeekBoxHeader( p_stream, &peekbox ) )
        return NULL;

    if( peekbox.i_size < 8 )
    {
        msg_Warn( p_stream, "found an invalid sized %"PRIu64" box %4.4s @%"PRIu64 ,
                  peekbox.i_size, (char *) &peekbox.i_type, vlc_stream_Tell(p_stream) );
        return NULL;
    }

    for( size_t i=0; nottypes && nottypes[i]; i++ )
    {
        if( nottypes[i] == peekbox.i_type )
        {
            *pb_restrictionhit = true;
            return NULL;
        }
    }

    for( size_t i=0; onlytypes && onlytypes[i]; i++ )
    {
        if( onlytypes[i] != peekbox.i_type )
        {
            *pb_restrictionhit = true;
            return NULL;
        }
    }

    /* if father's size == 0, it means unknown or infinite size,
     * and we skip the followong check */
    if( p_father && p_father->i_size > 0 )
    {
        const uint64_t i_box_next = peekbox.i_size + peekbox.i_pos;
        const uint64_t i_father_next = p_father->i_size + p_father->i_pos;
        /* check if it's within p-father */
        if( i_box_next > i_father_next )
        {
            msg_Warn( p_stream, "out of bound child %4.4s", (char*) &peekbox.i_type );
            return NULL; /* out of bound */
        }
    }

    /* Everything seems OK */
    MP4_Box_t *p_box = (MP4_Box_t *) malloc( sizeof(MP4_Box_t) );
    if( !p_box )
        return NULL;
    *p_box = peekbox;

    const uint64_t i_next = p_box->i_pos + p_box->i_size;
    p_box->p_father = p_father;
    if( MP4_Box_Read_Specific( p_stream, p_box, p_father ) != VLC_SUCCESS )
    {
        msg_Warn( p_stream, "Failed reading box %4.4s", (char*) &peekbox.i_type );
        MP4_BoxFree( p_box );
        p_box = NULL;
    }

    /* Check is we consumed all data */
    if( vlc_stream_Tell( p_stream ) < i_next )
    {
        MP4_Seek( p_stream, i_next - 1 ); /*  since past seek can fail when hitting EOF */
        MP4_Seek( p_stream, i_next );
        if( vlc_stream_Tell( p_stream ) < i_next - 1 ) /* Truncated box */
        {
            msg_Warn( p_stream, "truncated box %4.4s discarded", (char*) &peekbox.i_type );
            MP4_BoxFree( p_box );
            p_box = NULL;
        }
    }

    if ( p_box )
        MP4_BoxAddChild( p_father, p_box );

    return p_box;
}

/*****************************************************************************
 * For all known box a loader is given,
 * you have to be already read container header
 * without container size, file position on exit is unknown
 *****************************************************************************/
static int MP4_ReadBoxContainerChildrenIndexed( stream_t *p_stream,
               MP4_Box_t *p_container, const uint32_t stoplist[],
               const uint32_t excludelist[], bool b_indexed )
{
    /* Size of root container is set to 0 when unknown, for exemple
     * with a DASH stream. In that case, we skip the following check */
    if( (p_container->i_size || p_container->p_father)
            && ( vlc_stream_Tell( p_stream ) + ((b_indexed)?16:8) >
        (uint64_t)(p_container->i_pos + p_container->i_size) )
      )
    {
        /* there is no box to load */
        return 0;
    }

    uint64_t i_last_pos = 0; /* used to detect read failure loops */
    const uint64_t i_end = p_container->i_pos + p_container->i_size;
    MP4_Box_t *p_box = NULL;
    bool b_onexclude = false;
    bool b_continue;
    do
    {
        b_continue = false;
        if ( p_container->i_size )
        {
            const uint64_t i_tell = vlc_stream_Tell( p_stream );
            if( i_tell + ((b_indexed)?16:8) >= i_end )
                break;
        }

        uint32_t i_index = 0;
        if ( b_indexed )
        {
            uint8_t read[8];
            if ( vlc_stream_Read( p_stream, read, 8 ) < 8 )
                break;
            i_index = GetDWBE(&read[4]);
        }
        b_onexclude = false; /* If stopped due exclude list */
        if( (p_box = MP4_ReadBoxRestricted( p_stream, p_container, NULL, excludelist, &b_onexclude )) )
        {
            b_continue = true;
            p_box->i_index = i_index;
            for(size_t i=0; stoplist && stoplist[i]; i++)
            {
                if( p_box->i_type == stoplist[i] )
                    return 1;
            }
        }

        const uint64_t i_tell = vlc_stream_Tell( p_stream );
        if ( p_container->i_size && i_tell >= i_end )
        {
            assert( i_tell == i_end );
            break;
        }

        if ( !p_box )
        {
            /* Continue with next if box fails to load */
            if( i_last_pos == i_tell )
                break;
            i_last_pos = i_tell;
            b_continue = true;
        }

    } while( b_continue );

    /* Always move to end of container */
    if ( !b_onexclude &&  p_container->i_size )
    {
        const uint64_t i_tell = vlc_stream_Tell( p_stream );
        if ( i_tell != i_end )
            MP4_Seek( p_stream, i_end );
    }

    return 1;
}

int MP4_ReadBoxContainerRestricted( stream_t *p_stream, MP4_Box_t *p_container,
                                    const uint32_t stoplist[], const uint32_t excludelist[] )
{
    return MP4_ReadBoxContainerChildrenIndexed( p_stream, p_container,
                                                stoplist, excludelist, false );
}

int MP4_ReadBoxContainerChildren( stream_t *p_stream, MP4_Box_t *p_container,
                                  const uint32_t stoplist[] )
{
    return MP4_ReadBoxContainerChildrenIndexed( p_stream, p_container,
                                                stoplist, NULL, false );
}

static void MP4_BoxOffsetUp( MP4_Box_t *p_box, uint64_t i_offset )
{
    while(p_box)
    {
        p_box->i_pos += i_offset;
        MP4_BoxOffsetUp( p_box->p_first, i_offset );
        p_box = p_box->p_next;
    }
}

/* Reads within an already read/in memory box (containers without having to seek) */
static int MP4_ReadBoxContainerRawInBox( stream_t *p_stream, MP4_Box_t *p_container,
                                         uint8_t *p_buffer, uint64_t i_size, uint64_t i_offset )
{
    if(!p_container)
        return 0;
    stream_t *p_substream = vlc_stream_MemoryNew( p_stream, p_buffer, i_size,
                                                  true );
    if( !p_substream )
        return 0;
    MP4_Box_t *p_last = p_container->p_last;
    MP4_ReadBoxContainerChildren( p_substream, p_container, NULL );
    vlc_stream_Delete( p_substream );
    /* do pos fixup */
    if( p_container )
    {
        MP4_Box_t *p_box = p_last ? p_last : p_container->p_first;
        MP4_BoxOffsetUp(p_box, i_offset);
    }

    return 1;
}

static int MP4_ReadBoxContainer( stream_t *p_stream, MP4_Box_t *p_container )
{
    if( p_container->i_size &&
        ( p_container->i_size <= (size_t)mp4_box_headersize(p_container ) + 8 ) )
    {
        /* container is empty, 8 stand for the first header in this box */
        return 1;
    }

    /* enter box */
    if ( MP4_Seek( p_stream, p_container->i_pos +
                      mp4_box_headersize( p_container ) ) )
        return 0;
    return MP4_ReadBoxContainerChildren( p_stream, p_container, NULL );
}

static int MP4_ReadBoxSkip( stream_t *p_stream, MP4_Box_t *p_box )
{
    /* XXX sometime moov is hidden in a free box */
    if( p_box->p_father &&
        p_box->p_father->i_type == ATOM_root &&
        p_box->i_type == ATOM_free )
    {
        const uint8_t *p_peek;
        size_t header_size = mp4_box_headersize( p_box ) + 4;
        vlc_fourcc_t i_fcc;

        ssize_t i_read = vlc_stream_Peek( p_stream, &p_peek, 44 );
        if( unlikely(i_read < (ssize_t)header_size) )
            return 0;

        p_peek += header_size;
        i_read -= header_size;

        if( i_read >= 8 )
        {
            i_fcc = VLC_FOURCC( p_peek[0], p_peek[1], p_peek[2], p_peek[3] );

            if( i_fcc == ATOM_cmov || i_fcc == ATOM_mvhd )
            {
                msg_Warn( p_stream, "detected moov hidden in a free box ..." );

                p_box->i_type = ATOM_foov;
                return MP4_ReadBoxContainer( p_stream, p_box );
            }
        }
    }

    /* Nothing to do */
#ifdef MP4_ULTRA_VERBOSE
    if MP4_BOX_TYPE_ASCII()
        msg_Dbg( p_stream, "skip box: \"%4.4s\"", (char*)&p_box->i_type );
    else
        msg_Dbg( p_stream, "skip box: \"c%3.3s\"", (char*)&p_box->i_type+1 );
#endif
    return 1;
}

static int MP4_ReadBox_ilst( stream_t *p_stream, MP4_Box_t *p_box )
{
    if( p_box->i_size < 8 || vlc_stream_Read( p_stream, NULL, 8 ) < 8 )
        return 0;

    /* Find our handler */
    if ( !p_box->i_handler && p_box->p_father )
    {
        const MP4_Box_t *p_sibling = p_box->p_father->p_first;
        while( p_sibling )
        {
            if ( p_sibling->i_type == ATOM_hdlr && p_sibling->data.p_hdlr )
            {
                p_box->i_handler = p_sibling->data.p_hdlr->i_handler_type;
                break;
            }
            p_sibling = p_sibling->p_next;
        }
    }

    switch( p_box->i_handler )
    {
    case 0:
        msg_Warn( p_stream, "no handler for ilst atom" );
        return 0;
    case HANDLER_mdta:
        return MP4_ReadBoxContainerChildrenIndexed( p_stream, p_box, NULL, NULL, true );
    case HANDLER_mdir:
        return MP4_ReadBoxContainerChildren( p_stream, p_box, NULL );
    default:
        msg_Warn( p_stream, "Unknown ilst handler type '%4.4s'", (char*)&p_box->i_handler );
        return 0;
    }
}

static void MP4_FreeBox_ftyp( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_ftyp->i_compatible_brands );
}

static int MP4_ReadBox_ftyp( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_ftyp_t, MP4_FreeBox_ftyp );

    MP4_GETFOURCC( p_box->data.p_ftyp->i_major_brand );
    MP4_GET4BYTES( p_box->data.p_ftyp->i_minor_version );

    p_box->data.p_ftyp->i_compatible_brands_count = i_read / 4;
    if( p_box->data.p_ftyp->i_compatible_brands_count > 0 )
    {
        uint32_t *tab = p_box->data.p_ftyp->i_compatible_brands =
            vlc_alloc( p_box->data.p_ftyp->i_compatible_brands_count,
                       sizeof(uint32_t) );

        if( unlikely( tab == NULL ) )
            MP4_READBOX_EXIT( 0 );

        for( unsigned i = 0; i < p_box->data.p_ftyp->i_compatible_brands_count; i++ )
        {
            MP4_GETFOURCC( tab[i] );
        }
    }
    else
    {
        p_box->data.p_ftyp->i_compatible_brands = NULL;
    }

    MP4_READBOX_EXIT( 1 );
}


static int MP4_ReadBox_mvhd(  stream_t *p_stream, MP4_Box_t *p_box )
{
#ifdef MP4_VERBOSE
    char s_creation_time[128];
    char s_modification_time[128];
    char s_duration[128];
#endif
    MP4_READBOX_ENTER( MP4_Box_data_mvhd_t, NULL );

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


    for( unsigned i = 0; i < 2; i++ )
    {
        MP4_GET4BYTES( p_box->data.p_mvhd->i_reserved2[i] );
    }
    for( unsigned i = 0; i < 9; i++ )
    {
        MP4_GET4BYTES( p_box->data.p_mvhd->i_matrix[i] );
    }
    for( unsigned i = 0; i < 6; i++ )
    {
        MP4_GET4BYTES( p_box->data.p_mvhd->i_predefined[i] );
    }

    MP4_GET4BYTES( p_box->data.p_mvhd->i_next_track_id );


#ifdef MP4_VERBOSE
    MP4_ConvertDate2Str( s_creation_time, p_box->data.p_mvhd->i_creation_time, false );
    MP4_ConvertDate2Str( s_modification_time,
                         p_box->data.p_mvhd->i_modification_time, false );
    if( p_box->data.p_mvhd->i_rate && p_box->data.p_mvhd->i_timescale )
    {
        MP4_ConvertDate2Str( s_duration, p_box->data.p_mvhd->i_duration / p_box->data.p_mvhd->i_timescale, true );
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

static int MP4_ReadBox_mfhd(  stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_mfhd_t, NULL );

    MP4_GETVERSIONFLAGS( p_box->data.p_mvhd );

    MP4_GET4BYTES( p_box->data.p_mfhd->i_sequence_number );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"mfhd\" sequence number %d",
                  p_box->data.p_mfhd->i_sequence_number );
#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_tfxd(  stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_tfxd_t, NULL );

    MP4_Box_data_tfxd_t *p_tfxd_data = p_box->data.p_tfxd;
    MP4_GETVERSIONFLAGS( p_tfxd_data );

    if( p_tfxd_data->i_version == 0 )
    {
        MP4_GET4BYTES( p_tfxd_data->i_fragment_abs_time );
        MP4_GET4BYTES( p_tfxd_data->i_fragment_duration );
    }
    else
    {
        MP4_GET8BYTES( p_tfxd_data->i_fragment_abs_time );
        MP4_GET8BYTES( p_tfxd_data->i_fragment_duration );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"tfxd\" version %d, flags 0x%x, "\
            "fragment duration %"PRIu64", fragment abs time %"PRIu64,
                p_tfxd_data->i_version,
                p_tfxd_data->i_flags,
                p_tfxd_data->i_fragment_duration,
                p_tfxd_data->i_fragment_abs_time
           );
#endif

    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_tfrf( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_tfrf->p_tfrf_data_fields );
}

static int MP4_ReadBox_tfrf(  stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_tfxd_t, MP4_FreeBox_tfrf );

    MP4_Box_data_tfrf_t *p_tfrf_data = p_box->data.p_tfrf;
    MP4_GETVERSIONFLAGS( p_tfrf_data );

    MP4_GET1BYTE( p_tfrf_data->i_fragment_count );

    p_tfrf_data->p_tfrf_data_fields = calloc( p_tfrf_data->i_fragment_count,
                                              sizeof( TfrfBoxDataFields_t ) );
    if( !p_tfrf_data->p_tfrf_data_fields )
        MP4_READBOX_EXIT( 0 );

    for( uint8_t i = 0; i < p_tfrf_data->i_fragment_count; i++ )
    {
        TfrfBoxDataFields_t *TfrfBoxDataField = &p_tfrf_data->p_tfrf_data_fields[i];
        if( p_tfrf_data->i_version == 0 )
        {
            MP4_GET4BYTES( TfrfBoxDataField->i_fragment_abs_time );
            MP4_GET4BYTES( TfrfBoxDataField->i_fragment_duration );
        }
        else
        {
            MP4_GET8BYTES( TfrfBoxDataField->i_fragment_abs_time );
            MP4_GET8BYTES( TfrfBoxDataField->i_fragment_duration );
        }
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"tfrf\" version %d, flags 0x%x, "\
            "fragment count %"PRIu8, p_tfrf_data->i_version,
                p_tfrf_data->i_flags, p_tfrf_data->i_fragment_count );

    for( uint8_t i = 0; i < p_tfrf_data->i_fragment_count; i++ )
    {
        TfrfBoxDataFields_t *TfrfBoxDataField = &p_tfrf_data->p_tfrf_data_fields[i];
        msg_Dbg( p_stream, "\"tfrf\" fragment duration %"PRIu64", "\
                                    "fragment abs time %"PRIu64,
                    TfrfBoxDataField->i_fragment_duration,
                    TfrfBoxDataField->i_fragment_abs_time );
    }

#endif

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_XML360( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_360_t, NULL );

    MP4_Box_data_360_t *p_360_data = p_box->data.p_360;

    /* Copy the string for pattern matching as it does not end
    with a '\0' in the stream. */
    char *psz_rdf = strndup((char *)p_peek, i_read);

    if ( unlikely( !psz_rdf ) )
        MP4_READBOX_EXIT( 0 );

    /* Try to find the string "GSpherical:Spherical" because the v1
    spherical video spec says the tag must be there. */

    if ( strcasestr( psz_rdf, "Gspherical:Spherical" ) )
        p_360_data->i_projection_mode = PROJECTION_MODE_EQUIRECTANGULAR;

    /* Try to find the stero mode. */
    if ( strcasestr( psz_rdf, "left-right" ) )
    {
        msg_Dbg( p_stream, "Left-right stereo mode" );
        p_360_data->e_stereo_mode = XML360_STEREOSCOPIC_LEFT_RIGHT;
    }

    if ( strcasestr( psz_rdf, "top-bottom" ) )
    {
        msg_Dbg( p_stream, "Top-bottom stereo mode" );
        p_360_data->e_stereo_mode = XML360_STEREOSCOPIC_TOP_BOTTOM;
    }

    free( psz_rdf );

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_st3d( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_st3d_t, NULL );

    uint8_t i_version;
    MP4_GET1BYTE( i_version );
    if ( i_version != 0 )
        MP4_READBOX_EXIT( 0 );

    uint32_t i_flags;
    VLC_UNUSED( i_flags );
    MP4_GET3BYTES( i_flags );

    MP4_Box_data_st3d_t *p_data = p_box->data.p_st3d;
    MP4_GET1BYTE( p_data->i_stereo_mode );

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_prhd( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_prhd_t, NULL );

    uint8_t i_version;
    MP4_GET1BYTE( i_version );
    if (i_version != 0)
        MP4_READBOX_EXIT( 0 );

    uint32_t i_flags;
    VLC_UNUSED( i_flags );
    MP4_GET3BYTES( i_flags );

    MP4_Box_data_prhd_t *p_data = p_box->data.p_prhd;
    int32_t fixed16_16;
    MP4_GET4BYTES( fixed16_16 );
    p_data->f_pose_yaw_degrees   = (float) fixed16_16 / 65536.0f;

    MP4_GET4BYTES( fixed16_16 );
    p_data->f_pose_pitch_degrees = (float) fixed16_16 / 65536.0f;

    MP4_GET4BYTES( fixed16_16 );
    p_data->f_pose_roll_degrees  = (float) fixed16_16 / 65536.0f;

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_equi( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_equi_t, NULL );

    uint8_t i_version;
    MP4_GET1BYTE( i_version );
    if (i_version != 0)
        MP4_READBOX_EXIT( 0 );

    uint32_t i_flags;
    VLC_UNUSED( i_flags );
    MP4_GET3BYTES( i_flags );

    MP4_Box_data_equi_t *p_data = p_box->data.p_equi;
    MP4_GET4BYTES( p_data->i_projection_bounds_top );
    MP4_GET4BYTES( p_data->i_projection_bounds_bottom );
    MP4_GET4BYTES( p_data->i_projection_bounds_left );
    MP4_GET4BYTES( p_data->i_projection_bounds_right );

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_cbmp( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_cbmp_t, NULL );

    uint8_t i_version;
    MP4_GET1BYTE( i_version );
    if (i_version != 0)
        MP4_READBOX_EXIT( 0 );

    uint32_t i_flags;
    VLC_UNUSED( i_flags );
    MP4_GET3BYTES( i_flags );

    MP4_Box_data_cbmp_t *p_data = p_box->data.p_cbmp;
    MP4_GET4BYTES( p_data->i_layout );
    MP4_GET4BYTES( p_data->i_padding );

    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_sidx( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_sidx->p_items );
}

static int MP4_ReadBox_sidx(  stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_sidx_t, MP4_FreeBox_sidx );

    MP4_Box_data_sidx_t *p_sidx_data = p_box->data.p_sidx;
    MP4_GETVERSIONFLAGS( p_sidx_data );

    MP4_GET4BYTES( p_sidx_data->i_reference_ID );
    MP4_GET4BYTES( p_sidx_data->i_timescale );

    if( p_sidx_data->i_version == 0 )
    {
        MP4_GET4BYTES( p_sidx_data->i_earliest_presentation_time );
        MP4_GET4BYTES( p_sidx_data->i_first_offset );
    }
    else
    {
        MP4_GET8BYTES( p_sidx_data->i_earliest_presentation_time );
        MP4_GET8BYTES( p_sidx_data->i_first_offset );
    }

    uint16_t i_reserved, i_count;

    VLC_UNUSED(i_reserved);
    MP4_GET2BYTES( i_reserved );
    MP4_GET2BYTES( i_count );
    if( i_count == 0 )
        MP4_READBOX_EXIT( 1 );

    p_sidx_data->i_reference_count = i_count;
    p_sidx_data->p_items = vlc_alloc( i_count, sizeof( MP4_Box_sidx_item_t ) );
    if( unlikely(p_sidx_data->p_items == NULL) )
        MP4_READBOX_EXIT( 0 );

    for( unsigned i = 0; i < i_count; i++ )
    {
        MP4_Box_sidx_item_t *item = p_sidx_data->p_items + i;
        uint32_t tmp;

        MP4_GET4BYTES( tmp );
        item->b_reference_type = tmp >> 31;
        item->i_referenced_size = tmp & 0x7fffffff;
        MP4_GET4BYTES( item->i_subsegment_duration );

        MP4_GET4BYTES( tmp );
        item->b_starts_with_SAP = tmp >> 31;
        item->i_SAP_type = (tmp >> 24) & 0x70;
        item->i_SAP_delta_time = tmp & 0xfffffff;
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"sidx\" version %d, flags 0x%x, "\
            "ref_ID %"PRIu32", timescale %"PRIu32", ref_count %"PRIu16", "\
            "first subsegmt duration %"PRIu32,
                p_sidx_data->i_version,
                p_sidx_data->i_flags,
                p_sidx_data->i_reference_ID,
                p_sidx_data->i_timescale,
                p_sidx_data->i_reference_count,
                p_sidx_data->p_items[0].i_subsegment_duration
           );
#endif

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_tfhd(  stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_tfhd_t, NULL );

    MP4_GETVERSIONFLAGS( p_box->data.p_tfhd );

    if( p_box->data.p_tfhd->i_version != 0 )
    {
        msg_Warn( p_stream, "'tfhd' box with version != 0. "\
                " Don't know what to do with that, please patch" );
        MP4_READBOX_EXIT( 0 );
    }

    MP4_GET4BYTES( p_box->data.p_tfhd->i_track_ID );

    if( p_box->data.p_tfhd->i_flags & MP4_TFHD_DURATION_IS_EMPTY )
    {
        msg_Dbg( p_stream, "'duration-is-empty' flag is present "\
                "=> no samples for this time interval." );
        p_box->data.p_tfhd->b_empty = true;
    }
    else
        p_box->data.p_tfhd->b_empty = false;

    if( p_box->data.p_tfhd->i_flags & MP4_TFHD_BASE_DATA_OFFSET )
        MP4_GET8BYTES( p_box->data.p_tfhd->i_base_data_offset );
    if( p_box->data.p_tfhd->i_flags & MP4_TFHD_SAMPLE_DESC_INDEX )
        MP4_GET4BYTES( p_box->data.p_tfhd->i_sample_description_index );
    if( p_box->data.p_tfhd->i_flags & MP4_TFHD_DFLT_SAMPLE_DURATION )
        MP4_GET4BYTES( p_box->data.p_tfhd->i_default_sample_duration );
    if( p_box->data.p_tfhd->i_flags & MP4_TFHD_DFLT_SAMPLE_SIZE )
        MP4_GET4BYTES( p_box->data.p_tfhd->i_default_sample_size );
    if( p_box->data.p_tfhd->i_flags & MP4_TFHD_DFLT_SAMPLE_FLAGS )
        MP4_GET4BYTES( p_box->data.p_tfhd->i_default_sample_flags );

#ifdef MP4_VERBOSE
    char psz_base[128] = "\0";
    char psz_desc[128] = "\0";
    char psz_dura[128] = "\0";
    char psz_size[128] = "\0";
    char psz_flag[128] = "\0";
    if( p_box->data.p_tfhd->i_flags & MP4_TFHD_BASE_DATA_OFFSET )
        snprintf(psz_base, sizeof(psz_base), "base offset %"PRId64, p_box->data.p_tfhd->i_base_data_offset);
    if( p_box->data.p_tfhd->i_flags & MP4_TFHD_SAMPLE_DESC_INDEX )
        snprintf(psz_desc, sizeof(psz_desc), "sample description index %d", p_box->data.p_tfhd->i_sample_description_index);
    if( p_box->data.p_tfhd->i_flags & MP4_TFHD_DFLT_SAMPLE_DURATION )
        snprintf(psz_dura, sizeof(psz_dura), "sample duration %d", p_box->data.p_tfhd->i_default_sample_duration);
    if( p_box->data.p_tfhd->i_flags & MP4_TFHD_DFLT_SAMPLE_SIZE )
        snprintf(psz_size, sizeof(psz_size), "sample size %d", p_box->data.p_tfhd->i_default_sample_size);
    if( p_box->data.p_tfhd->i_flags & MP4_TFHD_DFLT_SAMPLE_FLAGS )
        snprintf(psz_flag, sizeof(psz_flag), "sample flags 0x%x", p_box->data.p_tfhd->i_default_sample_flags);

    msg_Dbg( p_stream, "read box: \"tfhd\" version %d flags 0x%x track ID %d %s %s %s %s %s",
                p_box->data.p_tfhd->i_version,
                p_box->data.p_tfhd->i_flags,
                p_box->data.p_tfhd->i_track_ID,
                psz_base, psz_desc, psz_dura, psz_size, psz_flag );
#endif

    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_trun( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_trun->p_samples );
}

static int MP4_ReadBox_trun(  stream_t *p_stream, MP4_Box_t *p_box )
{
    uint32_t count;

    MP4_READBOX_ENTER( MP4_Box_data_trun_t, MP4_FreeBox_trun );
    MP4_Box_data_trun_t *p_trun = p_box->data.p_trun;
    MP4_GETVERSIONFLAGS( p_trun );
    MP4_GET4BYTES( count );

    if( p_trun->i_flags & MP4_TRUN_DATA_OFFSET )
        MP4_GET4BYTES( p_trun->i_data_offset );
    if( p_trun->i_flags & MP4_TRUN_FIRST_FLAGS )
        MP4_GET4BYTES( p_trun->i_first_sample_flags );

    uint64_t i_entry_size =
        !!(p_trun->i_flags & MP4_TRUN_SAMPLE_DURATION) +
        !!(p_trun->i_flags & MP4_TRUN_SAMPLE_SIZE) +
        !!(p_trun->i_flags & MP4_TRUN_SAMPLE_FLAGS) +
        !!(p_trun->i_flags & MP4_TRUN_SAMPLE_TIME_OFFSET);

    if( i_entry_size * 4 * count > i_read )
        MP4_READBOX_EXIT( 0 );

    p_trun->p_samples = vlc_alloc( count, sizeof(MP4_descriptor_trun_sample_t) );
    if ( p_trun->p_samples == NULL )
        MP4_READBOX_EXIT( 0 );
    p_trun->i_sample_count = count;

    for( unsigned int i = 0; i < count; i++ )
    {
        MP4_descriptor_trun_sample_t *p_sample = &p_trun->p_samples[i];
        if( p_trun->i_flags & MP4_TRUN_SAMPLE_DURATION )
            MP4_GET4BYTES( p_sample->i_duration );
        if( p_trun->i_flags & MP4_TRUN_SAMPLE_SIZE )
            MP4_GET4BYTES( p_sample->i_size );
        if( p_trun->i_flags & MP4_TRUN_SAMPLE_FLAGS )
            MP4_GET4BYTES( p_sample->i_flags );
        if( p_trun->i_flags & MP4_TRUN_SAMPLE_TIME_OFFSET )
            MP4_GET4BYTES( p_sample->i_composition_time_offset.v0 );
    }

#ifdef MP4_ULTRA_VERBOSE
    msg_Dbg( p_stream, "read box: \"trun\" version %u flags 0x%x sample count %"PRIu32,
                  p_trun->i_version,
                  p_trun->i_flags,
                  p_trun->i_sample_count );

    for( unsigned int i = 0; i < count; i++ )
    {
        MP4_descriptor_trun_sample_t *p_sample = &p_trun->p_samples[i];
        msg_Dbg( p_stream, "read box: \"trun\" sample %4.4u flags 0x%x "\
            "duration %"PRIu32" size %"PRIu32" composition time offset %"PRIu32,
                        i, p_sample->i_flags, p_sample->i_duration,
                        p_sample->i_size, p_sample->i_composition_time_offset );
    }
#endif

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_tfdt( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_tfdt_t, NULL );
    if( i_read < 8 )
        MP4_READBOX_EXIT( 0 );

    MP4_GETVERSIONFLAGS( p_box->data.p_tfdt );

    if( p_box->data.p_tfdt->i_version == 0 )
        MP4_GET4BYTES( p_box->data.p_tfdt->i_base_media_decode_time );
    else if( p_box->data.p_tfdt->i_version == 1 )
        MP4_GET8BYTES( p_box->data.p_tfdt->i_base_media_decode_time );
    else
        MP4_READBOX_EXIT( 0 );

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_tkhd(  stream_t *p_stream, MP4_Box_t *p_box )
{
#ifdef MP4_VERBOSE
    char s_creation_time[128];
    char s_modification_time[128];
    char s_duration[128];
#endif
    MP4_READBOX_ENTER( MP4_Box_data_tkhd_t, NULL );

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

    for( unsigned i = 0; i < 2; i++ )
    {
        MP4_GET4BYTES( p_box->data.p_tkhd->i_reserved2[i] );
    }
    MP4_GET2BYTES( p_box->data.p_tkhd->i_layer );
    MP4_GET2BYTES( p_box->data.p_tkhd->i_predefined );
    MP4_GET2BYTES( p_box->data.p_tkhd->i_volume );
    MP4_GET2BYTES( p_box->data.p_tkhd->i_reserved3 );

    for( unsigned i = 0; i < 9; i++ )
    {
        MP4_GET4BYTES( p_box->data.p_tkhd->i_matrix[i] );
    }
    MP4_GET4BYTES( p_box->data.p_tkhd->i_width );
    MP4_GET4BYTES( p_box->data.p_tkhd->i_height );

    double rotation = 0;//angle in degrees to be rotated clockwise
    double scale[2];    // scale factor; sx = scale[0] , sy = scale[1]
    int32_t *matrix = p_box->data.p_tkhd->i_matrix;

    scale[0] = sqrt(conv_fx(matrix[0]) * conv_fx(matrix[0]) +
                    conv_fx(matrix[3]) * conv_fx(matrix[3]));
    scale[1] = sqrt(conv_fx(matrix[1]) * conv_fx(matrix[1]) +
                    conv_fx(matrix[4]) * conv_fx(matrix[4]));

    if( likely(scale[0] > 0 && scale[1] > 0) )
    {
        rotation = atan2(conv_fx(matrix[1]) / scale[1],
                         conv_fx(matrix[0]) / scale[0]) * 180 / M_PI;
        if (rotation < 0)
            rotation += 360.;
    }

    p_box->data.p_tkhd->f_rotation = rotation;

#ifdef MP4_VERBOSE
    double translate[2];// amount to translate; tx = translate[0] , ty = translate[1]

    translate[0] = conv_fx(matrix[6]);
    translate[1] = conv_fx(matrix[7]);

    MP4_ConvertDate2Str( s_creation_time, p_box->data.p_mvhd->i_creation_time, false );
    MP4_ConvertDate2Str( s_modification_time, p_box->data.p_mvhd->i_modification_time, false );
    MP4_ConvertDate2Str( s_duration, p_box->data.p_mvhd->i_duration, true );

    msg_Dbg( p_stream, "read box: \"tkhd\" creation %s modification %s duration %s track ID %d layer %d volume %f rotation %f scaleX %f scaleY %f translateX %f translateY %f width %f height %f. "
            "Matrix: %i %i %i %i %i %i %i %i %i",
                  s_creation_time,
                  s_modification_time,
                  s_duration,
                  p_box->data.p_tkhd->i_track_ID,
                  p_box->data.p_tkhd->i_layer,
                  (float)p_box->data.p_tkhd->i_volume / 256 ,
                  rotation,
                  scale[0],
                  scale[1],
                  translate[0],
                  translate[1],
                  (float)p_box->data.p_tkhd->i_width / BLOCK16x16,
                  (float)p_box->data.p_tkhd->i_height / BLOCK16x16,
                  p_box->data.p_tkhd->i_matrix[0],
                  p_box->data.p_tkhd->i_matrix[1],
                  p_box->data.p_tkhd->i_matrix[2],
                  p_box->data.p_tkhd->i_matrix[3],
                  p_box->data.p_tkhd->i_matrix[4],
                  p_box->data.p_tkhd->i_matrix[5],
                  p_box->data.p_tkhd->i_matrix[6],
                  p_box->data.p_tkhd->i_matrix[7],
                  p_box->data.p_tkhd->i_matrix[8] );
#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_load( stream_t *p_stream, MP4_Box_t *p_box )
{
    if ( p_box->i_size != 24 )
        return 0;
    MP4_READBOX_ENTER( MP4_Box_data_load_t, NULL );
    MP4_GET4BYTES( p_box->data.p_load->i_start_time );
    MP4_GET4BYTES( p_box->data.p_load->i_duration );
    MP4_GET4BYTES( p_box->data.p_load->i_flags );
    MP4_GET4BYTES( p_box->data.p_load->i_hints );
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_mdhd( stream_t *p_stream, MP4_Box_t *p_box )
{
    uint16_t i_language;
#ifdef MP4_VERBOSE
    char s_creation_time[128];
    char s_modification_time[128];
    char s_duration[128];
#endif
    MP4_READBOX_ENTER( MP4_Box_data_mdhd_t, NULL );

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

    MP4_GET2BYTES( i_language );
    decodeQtLanguageCode( i_language, p_box->data.p_mdhd->rgs_language,
                          &p_box->data.p_mdhd->b_mac_encoding );

    MP4_GET2BYTES( p_box->data.p_mdhd->i_quality );

#ifdef MP4_VERBOSE
    MP4_ConvertDate2Str( s_creation_time, p_box->data.p_mdhd->i_creation_time, false );
    MP4_ConvertDate2Str( s_modification_time, p_box->data.p_mdhd->i_modification_time, false );
    MP4_ConvertDate2Str( s_duration, p_box->data.p_mdhd->i_duration, true );
    msg_Dbg( p_stream, "read box: \"mdhd\" creation %s modification %s time scale %d duration %s language %3.3s",
                  s_creation_time,
                  s_modification_time,
                  (uint32_t)p_box->data.p_mdhd->i_timescale,
                  s_duration,
                  (char*) &p_box->data.p_mdhd->rgs_language );
#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_hdlr( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_hdlr->psz_name );
}

static int MP4_ReadBox_hdlr( stream_t *p_stream, MP4_Box_t *p_box )
{
    int32_t i_reserved;
    VLC_UNUSED(i_reserved);

    MP4_READBOX_ENTER( MP4_Box_data_hdlr_t, MP4_FreeBox_hdlr );

    MP4_GETVERSIONFLAGS( p_box->data.p_hdlr );

    MP4_GETFOURCC( p_box->data.p_hdlr->i_predefined );
    MP4_GETFOURCC( p_box->data.p_hdlr->i_handler_type );

    MP4_GET4BYTES( i_reserved );
    MP4_GET4BYTES( i_reserved );
    MP4_GET4BYTES( i_reserved );
    p_box->data.p_hdlr->psz_name = NULL;

    if( i_read >= SSIZE_MAX )
        MP4_READBOX_EXIT( 0 );

    if( i_read > 0 )
    {
        size_t i_copy;

        /* Yes, I love .mp4 :( */
        if( p_box->data.p_hdlr->i_predefined == VLC_FOURCC( 'm', 'h', 'l', 'r' ) )
        {
            uint8_t i_len;

            MP4_GET1BYTE( i_len );
            i_copy = (i_len <= i_read) ? i_len : i_read;
        }
        else
            i_copy = i_read;

        uint8_t *psz = p_box->data.p_hdlr->psz_name = malloc( i_copy + 1 );
        if( unlikely( psz == NULL ) )
            MP4_READBOX_EXIT( 0 );

        memcpy( psz, p_peek, i_copy );
        p_box->data.p_hdlr->psz_name[i_copy] = '\0';
    }

#ifdef MP4_VERBOSE
        msg_Dbg( p_stream, "read box: \"hdlr\" handler type: \"%4.4s\" name: \"%s\"",
                   (char*)&p_box->data.p_hdlr->i_handler_type,
                   p_box->data.p_hdlr->psz_name );

#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_vmhd( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_vmhd_t, NULL );

    MP4_GETVERSIONFLAGS( p_box->data.p_vmhd );

    MP4_GET2BYTES( p_box->data.p_vmhd->i_graphics_mode );
    for( unsigned i = 0; i < 3; i++ )
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
    MP4_READBOX_ENTER( MP4_Box_data_smhd_t, NULL );

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
    MP4_READBOX_ENTER( MP4_Box_data_hmhd_t, NULL );

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

static void MP4_FreeBox_url( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_url->psz_location );
}

static int MP4_ReadBox_url( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_url_t, MP4_FreeBox_url );

    MP4_GETVERSIONFLAGS( p_box->data.p_url );
    MP4_GETSTRINGZ( p_box->data.p_url->psz_location );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"url\" url: %s",
                       p_box->data.p_url->psz_location );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_urn( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_urn->psz_name );
    FREENULL( p_box->data.p_urn->psz_location );
}

static int MP4_ReadBox_urn( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_urn_t, MP4_FreeBox_urn );

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

static int MP4_ReadBox_LtdContainer( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER_PARTIAL( MP4_Box_data_lcont_t, 16, NULL );
    if( i_read < 8 )
        MP4_READBOX_EXIT( 0 );

    MP4_GETVERSIONFLAGS( p_box->data.p_lcont );
    if( p_box->data.p_lcont->i_version != 0 )
        MP4_READBOX_EXIT( 0 );
    MP4_GET4BYTES( p_box->data.p_lcont->i_entry_count );

    uint32_t i_entry = 0;
    i_read = p_box->i_size - 16;
    while (i_read > 8 && i_entry < p_box->data.p_lcont->i_entry_count )
    {
        MP4_Box_t *p_childbox = MP4_ReadBox( p_stream, p_box );
        if( !p_childbox )
            break;
        MP4_BoxAddChild( p_box, p_childbox );
        i_entry++;

        if( i_read < p_childbox->i_size )
            MP4_READBOX_EXIT( 0 );

        i_read -= p_childbox->i_size;
    }

    if (i_entry != p_box->data.p_lcont->i_entry_count)
        p_box->data.p_lcont->i_entry_count = i_entry;

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"%4.4s\" entry-count %d", (char *)&p_box->i_type,
                        p_box->data.p_lcont->i_entry_count );

#endif

    if ( MP4_Seek( p_stream, p_box->i_pos + p_box->i_size ) )
        MP4_READBOX_EXIT( 0 );

    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_stts( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_stts->pi_sample_count );
    FREENULL( p_box->data.p_stts->pi_sample_delta );
}

static int MP4_ReadBox_stts( stream_t *p_stream, MP4_Box_t *p_box )
{
    uint32_t count;

    MP4_READBOX_ENTER( MP4_Box_data_stts_t, MP4_FreeBox_stts );

    MP4_GETVERSIONFLAGS( p_box->data.p_stts );
    MP4_GET4BYTES( count );

    if( UINT64_C(8) * count > i_read )
    {
        /*count = i_read / 8;*/
        MP4_READBOX_EXIT( 0 );
    }

    p_box->data.p_stts->pi_sample_count = vlc_alloc( count, sizeof(uint32_t) );
    p_box->data.p_stts->pi_sample_delta = vlc_alloc( count, sizeof(int32_t) );
    p_box->data.p_stts->i_entry_count = count;

    if( p_box->data.p_stts->pi_sample_count == NULL
     || p_box->data.p_stts->pi_sample_delta == NULL )
    {
        MP4_READBOX_EXIT( 0 );
    }

    for( uint32_t i = 0; i < count; i++ )
    {
        MP4_GET4BYTES( p_box->data.p_stts->pi_sample_count[i] );
        MP4_GET4BYTES( p_box->data.p_stts->pi_sample_delta[i] );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"stts\" entry-count %d",
                      p_box->data.p_stts->i_entry_count );

#endif
    MP4_READBOX_EXIT( 1 );
}


static void MP4_FreeBox_ctts( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_ctts->pi_sample_count );
    FREENULL( p_box->data.p_ctts->pi_sample_offset );
}

static int MP4_ReadBox_ctts( stream_t *p_stream, MP4_Box_t *p_box )
{
    uint32_t count;

    MP4_READBOX_ENTER( MP4_Box_data_ctts_t, MP4_FreeBox_ctts );

    MP4_GETVERSIONFLAGS( p_box->data.p_ctts );
    MP4_GET4BYTES( count );

    if( UINT64_C(8) * count > i_read )
        MP4_READBOX_EXIT( 0 );

    p_box->data.p_ctts->pi_sample_count = vlc_alloc( count, sizeof(uint32_t) );
    p_box->data.p_ctts->pi_sample_offset = vlc_alloc( count, sizeof(int32_t) );
    if( unlikely(p_box->data.p_ctts->pi_sample_count == NULL
              || p_box->data.p_ctts->pi_sample_offset == NULL) )
        MP4_READBOX_EXIT( 0 );
    p_box->data.p_ctts->i_entry_count = count;

    for( uint32_t i = 0; i < count; i++ )
    {
        MP4_GET4BYTES( p_box->data.p_ctts->pi_sample_count[i] );
        MP4_GET4BYTES( p_box->data.p_ctts->pi_sample_offset[i] );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"ctts\" entry-count %"PRIu32, count );

#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_cslg( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_cslg_t, NULL );

    unsigned i_version, i_flags;
    MP4_GET1BYTE( i_version );
    MP4_GET3BYTES( i_flags );
    VLC_UNUSED(i_flags);

    if( i_version > 1 )
        MP4_READBOX_EXIT( 0 );

#define READ_CSLG(readbytes) {\
    readbytes( p_box->data.p_cslg->ct_to_dts_shift );\
    readbytes( p_box->data.p_cslg->i_least_delta );\
    readbytes( p_box->data.p_cslg->i_max_delta );\
    readbytes( p_box->data.p_cslg->i_composition_starttime );\
    readbytes( p_box->data.p_cslg->i_composition_endtime ); }

    if( i_version == 0 )
        READ_CSLG(MP4_GET4BYTES)
    else
        READ_CSLG(MP4_GET8BYTES)

    MP4_READBOX_EXIT( 1 );
}

static uint64_t MP4_ReadLengthDescriptor( uint8_t **restrict bufp,
                                          uint64_t *restrict lenp )
{
    unsigned char *buf = *bufp;
    uint64_t len = *lenp;
    unsigned char b;
    uint64_t value = 0;

    do
    {
        if (unlikely(len == 0))
            return -1; /* end of bit stream */
        if (unlikely(value > (UINT64_MAX >> 7)))
            return -1; /* integer overflow */

        b = *(buf++);
        len--;
        value = (value << 7) + (b & 0x7f);
    }
    while (b & 0x80);

    *bufp = buf;
    *lenp = len;
    return value;
}


static void MP4_FreeBox_esds( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_esds->es_descriptor.psz_URL );
    if( p_box->data.p_esds->es_descriptor.p_decConfigDescr )
    {
        FREENULL( p_box->data.p_esds->es_descriptor.p_decConfigDescr->p_decoder_specific_info );
        FREENULL( p_box->data.p_esds->es_descriptor.p_decConfigDescr );
    }
}

static int MP4_ReadBox_esds( stream_t *p_stream, MP4_Box_t *p_box )
{
#define es_descriptor p_box->data.p_esds->es_descriptor
    uint64_t i_len;
    unsigned int i_flags;
    unsigned int i_type;

    MP4_READBOX_ENTER( MP4_Box_data_esds_t, MP4_FreeBox_esds );

    MP4_GETVERSIONFLAGS( p_box->data.p_esds );


    MP4_GET1BYTE( i_type );
    if( i_type == 0x03 ) /* MP4ESDescrTag ISO/IEC 14496-1 8.3.3 */
    {
        i_len = MP4_ReadLengthDescriptor( &p_peek, &i_read );
        if( unlikely(i_len == UINT64_C(-1)) )
            MP4_READBOX_EXIT( 0 );

#ifdef MP4_VERBOSE
        msg_Dbg( p_stream, "found esds MPEG4ESDescr (%"PRIu64" bytes)",
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
        if( es_descriptor.b_url && i_read > 0 )
        {
            uint8_t i_url;

            MP4_GET1BYTE( i_url );
            if( i_url > i_read )
                MP4_READBOX_EXIT( 1 );
            es_descriptor.psz_URL = malloc( (unsigned) i_url + 1 );
            if( es_descriptor.psz_URL )
            {
                memcpy( es_descriptor.psz_URL, p_peek, i_url );
                es_descriptor.psz_URL[i_url] = 0;
            }
            p_peek += i_url;
            i_read -= i_url;
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

    if( i_type != 0x04)/* MP4DecConfigDescrTag ISO/IEC 14496-1 8.3.4 */
    {
         es_descriptor.p_decConfigDescr = NULL;
         MP4_READBOX_EXIT( 1 ); /* rest isn't interesting up to now */
    }

    i_len = MP4_ReadLengthDescriptor( &p_peek, &i_read );
    if( unlikely(i_len == UINT64_C(-1)) )
        MP4_READBOX_EXIT( 0 );
#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "found esds MP4DecConfigDescr (%"PRIu64" bytes)",
             i_len );
#endif

    es_descriptor.p_decConfigDescr =
            calloc( 1, sizeof( MP4_descriptor_decoder_config_t ));
    if( unlikely( es_descriptor.p_decConfigDescr == NULL ) )
        MP4_READBOX_EXIT( 0 );

    MP4_GET1BYTE( es_descriptor.p_decConfigDescr->i_objectProfileIndication );
    MP4_GET1BYTE( i_flags );
    es_descriptor.p_decConfigDescr->i_streamType = i_flags >> 2;
    es_descriptor.p_decConfigDescr->b_upStream = ( i_flags >> 1 )&0x01;
    MP4_GET3BYTES( es_descriptor.p_decConfigDescr->i_buffer_sizeDB );
    MP4_GET4BYTES( es_descriptor.p_decConfigDescr->i_max_bitrate );
    MP4_GET4BYTES( es_descriptor.p_decConfigDescr->i_avg_bitrate );
    MP4_GET1BYTE( i_type );
    if( i_type !=  0x05 )/* MP4DecSpecificDescrTag ISO/IEC 14496-1 8.3.5 */
    {
        es_descriptor.p_decConfigDescr->i_decoder_specific_info_len = 0;
        es_descriptor.p_decConfigDescr->p_decoder_specific_info  = NULL;
        MP4_READBOX_EXIT( 1 );
    }

    i_len = MP4_ReadLengthDescriptor( &p_peek, &i_read );
    if( unlikely(i_len == UINT64_C(-1)) )
        MP4_READBOX_EXIT( 0 );
#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "found esds MP4DecSpecificDescr (%"PRIu64" bytes)",
             i_len );
#endif
    if( i_len > i_read )
        MP4_READBOX_EXIT( 0 );

    es_descriptor.p_decConfigDescr->i_decoder_specific_info_len = i_len;
    es_descriptor.p_decConfigDescr->p_decoder_specific_info = malloc( i_len );
    if( unlikely( es_descriptor.p_decConfigDescr->p_decoder_specific_info == NULL ) )
        MP4_READBOX_EXIT( 0 );

    memcpy( es_descriptor.p_decConfigDescr->p_decoder_specific_info,
            p_peek, i_len );

    MP4_READBOX_EXIT( 1 );
#undef es_descriptor
}

static void MP4_FreeBox_avcC( MP4_Box_t *p_box )
{
    MP4_Box_data_avcC_t *p_avcC = p_box->data.p_avcC;
    int i;

    if( p_avcC->i_avcC > 0 ) FREENULL( p_avcC->p_avcC );

    if( p_avcC->sps )
    {
        for( i = 0; i < p_avcC->i_sps; i++ )
            FREENULL( p_avcC->sps[i] );
    }
    if( p_avcC->pps )
    {
        for( i = 0; i < p_avcC->i_pps; i++ )
            FREENULL( p_avcC->pps[i] );
    }
    if( p_avcC->i_sps > 0 ) FREENULL( p_avcC->sps );
    if( p_avcC->i_sps > 0 ) FREENULL( p_avcC->i_sps_length );
    if( p_avcC->i_pps > 0 ) FREENULL( p_avcC->pps );
    if( p_avcC->i_pps > 0 ) FREENULL( p_avcC->i_pps_length );
}

static int MP4_ReadBox_avcC( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_Box_data_avcC_t *p_avcC;
    int i;

    MP4_READBOX_ENTER( MP4_Box_data_avcC_t, MP4_FreeBox_avcC );
    p_avcC = p_box->data.p_avcC;

    p_avcC->i_avcC = i_read;
    if( p_avcC->i_avcC > 0 )
    {
        uint8_t * p = p_avcC->p_avcC = malloc( p_avcC->i_avcC );
        if( p )
            memcpy( p, p_peek, i_read );
    }

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
        p_avcC->i_sps_length = calloc( p_avcC->i_sps, sizeof( uint16_t ) );
        p_avcC->sps = calloc( p_avcC->i_sps, sizeof( uint8_t* ) );

        if( !p_avcC->i_sps_length || !p_avcC->sps )
            goto error;

        for( i = 0; i < p_avcC->i_sps && i_read > 2; i++ )
        {
            MP4_GET2BYTES( p_avcC->i_sps_length[i] );
            if ( p_avcC->i_sps_length[i] > i_read )
                goto error;
            p_avcC->sps[i] = malloc( p_avcC->i_sps_length[i] );
            if( p_avcC->sps[i] )
                memcpy( p_avcC->sps[i], p_peek, p_avcC->i_sps_length[i] );

            p_peek += p_avcC->i_sps_length[i];
            i_read -= p_avcC->i_sps_length[i];
        }
        if ( i != p_avcC->i_sps )
            goto error;
    }

    MP4_GET1BYTE( p_avcC->i_pps );
    if( p_avcC->i_pps > 0 )
    {
        p_avcC->i_pps_length = calloc( p_avcC->i_pps, sizeof( uint16_t ) );
        p_avcC->pps = calloc( p_avcC->i_pps, sizeof( uint8_t* ) );

        if( !p_avcC->i_pps_length || !p_avcC->pps )
            goto error;

        for( i = 0; i < p_avcC->i_pps && i_read > 2; i++ )
        {
            MP4_GET2BYTES( p_avcC->i_pps_length[i] );
            if( p_avcC->i_pps_length[i] > i_read )
                goto error;
            p_avcC->pps[i] = malloc( p_avcC->i_pps_length[i] );
            if( p_avcC->pps[i] )
                memcpy( p_avcC->pps[i], p_peek, p_avcC->i_pps_length[i] );

            p_peek += p_avcC->i_pps_length[i];
            i_read -= p_avcC->i_pps_length[i];
        }
        if ( i != p_avcC->i_pps )
            goto error;
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

error:
    MP4_READBOX_EXIT( 0 );
}

static void MP4_FreeBox_vpcC( MP4_Box_t *p_box )
{
    free( p_box->data.p_vpcC->p_codec_init_data );
}

static int MP4_ReadBox_vpcC( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_vpcC_t, MP4_FreeBox_vpcC );
    MP4_Box_data_vpcC_t *p_vpcC = p_box->data.p_vpcC;

    if( p_box->i_size < 6 )
        MP4_READBOX_EXIT( 0 );

    uint8_t i_version;
    MP4_GET1BYTE( i_version );
    if( i_version != 0 )
        MP4_READBOX_EXIT( 0 );

    MP4_GET1BYTE( p_vpcC->i_profile );
    MP4_GET1BYTE( p_vpcC->i_level );
    MP4_GET1BYTE( p_vpcC->i_bit_depth );
    p_vpcC->i_color_space = p_vpcC->i_bit_depth & 0x0F;
    p_vpcC->i_bit_depth >>= 4;
    MP4_GET1BYTE( p_vpcC->i_chroma_subsampling );
    p_vpcC->i_xfer_function = ( p_vpcC->i_chroma_subsampling & 0x0F ) >> 1;
    p_vpcC->i_fullrange = p_vpcC->i_chroma_subsampling & 0x01;
    p_vpcC->i_chroma_subsampling >>= 4;
    MP4_GET2BYTES( p_vpcC->i_codec_init_datasize );
    if( p_vpcC->i_codec_init_datasize > i_read )
        p_vpcC->i_codec_init_datasize = i_read;

    if( p_vpcC->i_codec_init_datasize )
    {
        p_vpcC->p_codec_init_data = malloc( i_read );
        if( !p_vpcC->p_codec_init_data )
            MP4_READBOX_EXIT( 0 );
        memcpy( p_vpcC->p_codec_init_data, p_peek, i_read );
    }

    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_WMA2( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_WMA2->p_extra );
}

static int MP4_ReadBox_WMA2( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_WMA2_t, MP4_FreeBox_WMA2 );

    MP4_Box_data_WMA2_t *p_WMA2 = p_box->data.p_WMA2;

    MP4_GET2BYTESLE( p_WMA2->Format.wFormatTag );
    MP4_GET2BYTESLE( p_WMA2->Format.nChannels );
    MP4_GET4BYTESLE( p_WMA2->Format.nSamplesPerSec );
    MP4_GET4BYTESLE( p_WMA2->Format.nAvgBytesPerSec );
    MP4_GET2BYTESLE( p_WMA2->Format.nBlockAlign );
    MP4_GET2BYTESLE( p_WMA2->Format.wBitsPerSample );

    uint16_t i_cbSize;
    MP4_GET2BYTESLE( i_cbSize );

    if( i_cbSize > i_read )
        goto error;

    p_WMA2->i_extra = i_cbSize;
    if ( p_WMA2->i_extra )
    {
        p_WMA2->p_extra = malloc( p_WMA2->i_extra );
        if ( ! p_WMA2->p_extra )
            goto error;
        memcpy( p_WMA2->p_extra, p_peek, p_WMA2->i_extra );
    }

    MP4_READBOX_EXIT( 1 );

error:
    MP4_READBOX_EXIT( 0 );
}

static void MP4_FreeBox_strf( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_strf->p_extra );
}

static int MP4_ReadBox_strf( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_strf_t, MP4_FreeBox_strf );

    MP4_Box_data_strf_t *p_strf = p_box->data.p_strf;

    if( i_read < 40 )
        goto error;

    MP4_GET4BYTESLE( p_strf->bmiHeader.biSize );
    MP4_GET4BYTESLE( p_strf->bmiHeader.biWidth );
    MP4_GET4BYTESLE( p_strf->bmiHeader.biHeight );
    MP4_GET2BYTESLE( p_strf->bmiHeader.biPlanes );
    MP4_GET2BYTESLE( p_strf->bmiHeader.biBitCount );
    MP4_GETFOURCC( p_strf->bmiHeader.biCompression );
    MP4_GET4BYTESLE( p_strf->bmiHeader.biSizeImage );
    MP4_GET4BYTESLE( p_strf->bmiHeader.biXPelsPerMeter );
    MP4_GET4BYTESLE( p_strf->bmiHeader.biYPelsPerMeter );
    MP4_GET4BYTESLE( p_strf->bmiHeader.biClrUsed );
    MP4_GET4BYTESLE( p_strf->bmiHeader.biClrImportant );

    p_strf->i_extra = i_read;
    if ( p_strf->i_extra )
    {
        p_strf->p_extra = malloc( p_strf->i_extra );
        if ( ! p_strf->p_extra )
            goto error;
        memcpy( p_strf->p_extra, p_peek, i_read );
    }

    MP4_READBOX_EXIT( 1 );

error:
    MP4_READBOX_EXIT( 0 );
}

static int MP4_ReadBox_ASF( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_ASF_t, NULL );

    MP4_Box_data_ASF_t *p_asf = p_box->data.p_asf;

    if (i_read != 8)
        MP4_READBOX_EXIT( 0 );

    MP4_GET1BYTE( p_asf->i_stream_number );
    /* remaining is unknown */

    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_sbgp( MP4_Box_t *p_box )
{
    MP4_Box_data_sbgp_t *p_sbgp = p_box->data.p_sbgp;
    free( p_sbgp->entries.pi_sample_count );
    free( p_sbgp->entries.pi_group_description_index );
}

static int MP4_ReadBox_sbgp( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_sbgp_t, MP4_FreeBox_sbgp );
    MP4_Box_data_sbgp_t *p_sbgp = p_box->data.p_sbgp;
    uint32_t i_flags;

    if ( i_read < 12 )
        MP4_READBOX_EXIT( 0 );

    MP4_GET1BYTE( p_sbgp->i_version );
    MP4_GET3BYTES( i_flags );
    if( i_flags != 0 )
        MP4_READBOX_EXIT( 0 );

    MP4_GETFOURCC( p_sbgp->i_grouping_type );

    if( p_sbgp->i_version == 1 )
    {
        if( i_read < 8 )
            MP4_READBOX_EXIT( 0 );
        MP4_GET4BYTES( p_sbgp->i_grouping_type_parameter );
    }

    MP4_GET4BYTES( p_sbgp->i_entry_count );
    if( p_sbgp->i_entry_count > i_read / (4 + 4) )
        p_sbgp->i_entry_count = i_read / (4 + 4);

    p_sbgp->entries.pi_sample_count = vlc_alloc( p_sbgp->i_entry_count, sizeof(uint32_t) );
    p_sbgp->entries.pi_group_description_index = vlc_alloc( p_sbgp->i_entry_count, sizeof(uint32_t) );

    if( !p_sbgp->entries.pi_sample_count || !p_sbgp->entries.pi_group_description_index )
    {
        MP4_FreeBox_sbgp( p_box );
        MP4_READBOX_EXIT( 0 );
    }

    for( uint32_t i=0; i<p_sbgp->i_entry_count; i++ )
    {
        MP4_GET4BYTES( p_sbgp->entries.pi_sample_count[i] );
        MP4_GET4BYTES( p_sbgp->entries.pi_group_description_index[i] );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
        "read box: \"sbgp\" grouping type %4.4s", (char*) &p_sbgp->i_grouping_type );
 #ifdef MP4_ULTRA_VERBOSE
    for (uint32_t i = 0; i < p_sbgp->i_entry_count; i++)
        msg_Dbg( p_stream, "\t samples %" PRIu32 " group %" PRIu32,
                 p_sbgp->entries.pi_sample_count[i],
                 p_sbgp->entries.pi_group_description_index[i] );
 #endif
#endif

    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_sgpd( MP4_Box_t *p_box )
{
    MP4_Box_data_sgpd_t *p_sgpd = p_box->data.p_sgpd;
    free( p_sgpd->p_entries );
}

static int MP4_ReadBox_sgpd( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_sgpd_t, MP4_FreeBox_sgpd );
    MP4_Box_data_sgpd_t *p_sgpd = p_box->data.p_sgpd;
    uint32_t i_flags;
    uint32_t i_default_length = 0;

    if ( i_read < 8 )
        MP4_READBOX_EXIT( 0 );

    MP4_GET1BYTE( p_sgpd->i_version );
    MP4_GET3BYTES( i_flags );
    if( i_flags != 0 )
        MP4_READBOX_EXIT( 0 );

    MP4_GETFOURCC( p_sgpd->i_grouping_type );

    switch( p_sgpd->i_grouping_type )
    {
        case SAMPLEGROUP_rap:
            break;

        default:
#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
        "read box: \"sgpd\" grouping type %4.4s (unimplemented)", (char*) &p_sgpd->i_grouping_type );
#endif
            MP4_READBOX_EXIT( 1 );
    }

    if( p_sgpd->i_version == 1 )
    {
        if( i_read < 8 )
            MP4_READBOX_EXIT( 0 );
        MP4_GET4BYTES( i_default_length );
    }
    else if( p_sgpd->i_version >= 2 )
    {
        if( i_read < 8 )
            MP4_READBOX_EXIT( 0 );
        MP4_GET4BYTES( p_sgpd->i_default_sample_description_index );
    }

    MP4_GET4BYTES( p_sgpd->i_entry_count );

    p_sgpd->p_entries = vlc_alloc( p_sgpd->i_entry_count, sizeof(*p_sgpd->p_entries) );
    if( !p_sgpd->p_entries )
        MP4_READBOX_EXIT( 0 );

    uint32_t i = 0;
    for( ; i<p_sgpd->i_entry_count; i++ )
    {
        uint32_t i_description_length = i_default_length;
        if( p_sgpd->i_version == 1 && i_default_length == 0 )
        {
            if( i_read < 4 )
                break;
            MP4_GET4BYTES( i_description_length );
        }

        if( p_sgpd->i_version == 1 && i_read < i_description_length )
            break;

        switch( p_sgpd->i_grouping_type )
        {
            case SAMPLEGROUP_rap:
                {
                    if( i_read < 1 )
                    {
                        p_sgpd->i_entry_count = 0;
                        MP4_FreeBox_sgpd( p_box );
                        MP4_READBOX_EXIT( 0 );
                    }
                    uint8_t i_data;
                    MP4_GET1BYTE( i_data );
                    p_sgpd->p_entries[i].rap.i_num_leading_samples_known = i_data & 0x80;
                    p_sgpd->p_entries[i].rap.i_num_leading_samples = i_data & 0x7F;
                }
                break;

            default:
                assert(0);
        }
    }

    if( i != p_sgpd->i_entry_count )
        p_sgpd->i_entry_count = i;

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
        "read box: \"sgpd\" grouping type %4.4s", (char*) &p_sgpd->i_grouping_type );
#endif

    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_stsdext_chan( MP4_Box_t *p_box )
{
    MP4_Box_data_chan_t *p_chan = p_box->data.p_chan;
    free( p_chan->layout.p_descriptions );
}

static int MP4_ReadBox_stsdext_chan( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_chan_t, MP4_FreeBox_stsdext_chan );
    MP4_Box_data_chan_t *p_chan = p_box->data.p_chan;

    if ( i_read < 16 )
        MP4_READBOX_EXIT( 0 );

    MP4_GET1BYTE( p_chan->i_version );
    MP4_GET3BYTES( p_chan->i_channels_flags );
    MP4_GET4BYTES( p_chan->layout.i_channels_layout_tag );
    MP4_GET4BYTES( p_chan->layout.i_channels_bitmap );
    MP4_GET4BYTES( p_chan->layout.i_channels_description_count );

    size_t i_descsize = 8 + 3 * sizeof(float);
    if ( i_read < p_chan->layout.i_channels_description_count * i_descsize )
        MP4_READBOX_EXIT( 0 );

    p_chan->layout.p_descriptions =
        vlc_alloc( p_chan->layout.i_channels_description_count, i_descsize );

    if ( !p_chan->layout.p_descriptions )
        MP4_READBOX_EXIT( 0 );

    uint32_t i;
    for( i=0; i<p_chan->layout.i_channels_description_count; i++ )
    {
        if ( i_read < 20 )
            break;
        MP4_GET4BYTES( p_chan->layout.p_descriptions[i].i_channel_label );
        MP4_GET4BYTES( p_chan->layout.p_descriptions[i].i_channel_flags );
        MP4_GET4BYTES( p_chan->layout.p_descriptions[i].f_coordinates[0] );
        MP4_GET4BYTES( p_chan->layout.p_descriptions[i].f_coordinates[1] );
        MP4_GET4BYTES( p_chan->layout.p_descriptions[i].f_coordinates[2] );
    }
    if ( i<p_chan->layout.i_channels_description_count )
        p_chan->layout.i_channels_description_count = i;

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"chan\" flags=0x%x tag=0x%x bitmap=0x%x descriptions=%u",
             p_chan->i_channels_flags, p_chan->layout.i_channels_layout_tag,
             p_chan->layout.i_channels_bitmap, p_chan->layout.i_channels_description_count );
#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_dec3( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_dec3_t, NULL );

    MP4_Box_data_dec3_t *p_dec3 = p_box->data.p_dec3;

    unsigned i_header;
    MP4_GET2BYTES( i_header );

    p_dec3->i_data_rate = i_header >> 3;
    p_dec3->i_num_ind_sub = (i_header & 0x7) + 1;
    for (uint8_t i = 0; i < p_dec3->i_num_ind_sub; i++) {
        MP4_GET3BYTES( i_header );
        p_dec3->stream[i].i_fscod = ( i_header >> 22 ) & 0x03;
        p_dec3->stream[i].i_bsid  = ( i_header >> 17 ) & 0x01f;
        p_dec3->stream[i].i_bsmod = ( i_header >> 12 ) & 0x01f;
        p_dec3->stream[i].i_acmod = ( i_header >> 9 ) & 0x07;
        p_dec3->stream[i].i_lfeon = ( i_header >> 8 ) & 0x01;
        p_dec3->stream[i].i_num_dep_sub = (i_header >> 1) & 0x0f;
        if (p_dec3->stream[i].i_num_dep_sub) {
            MP4_GET1BYTE( p_dec3->stream[i].i_chan_loc );
            p_dec3->stream[i].i_chan_loc |= (i_header & 1) << 8;
        } else
            p_dec3->stream[i].i_chan_loc = 0;
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
        "read box: \"dec3\" bitrate %dkbps %d independent substreams",
            p_dec3->i_data_rate, p_dec3->i_num_ind_sub);

    for (uint8_t i = 0; i < p_dec3->i_num_ind_sub; i++)
        msg_Dbg( p_stream,
                "\tstream %d: bsid=0x%x bsmod=0x%x acmod=0x%x lfeon=0x%x "
                "num dependent subs=%d chan_loc=0x%x",
                i, p_dec3->stream[i].i_bsid, p_dec3->stream[i].i_bsmod, p_dec3->stream[i].i_acmod,
                p_dec3->stream[i].i_lfeon, p_dec3->stream[i].i_num_dep_sub, p_dec3->stream[i].i_chan_loc );
#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_dac3( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_Box_data_dac3_t *p_dac3;
    MP4_READBOX_ENTER( MP4_Box_data_dac3_t, NULL );

    p_dac3 = p_box->data.p_dac3;

    unsigned i_header;
    MP4_GET3BYTES( i_header );

    p_dac3->i_fscod = ( i_header >> 22 ) & 0x03;
    p_dac3->i_bsid  = ( i_header >> 17 ) & 0x01f;
    p_dac3->i_bsmod = ( i_header >> 14 ) & 0x07;
    p_dac3->i_acmod = ( i_header >> 11 ) & 0x07;
    p_dac3->i_lfeon = ( i_header >> 10 ) & 0x01;
    p_dac3->i_bitrate_code = ( i_header >> 5) & 0x1f;

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"dac3\" fscod=0x%x bsid=0x%x bsmod=0x%x acmod=0x%x lfeon=0x%x bitrate_code=0x%x",
             p_dac3->i_fscod, p_dac3->i_bsid, p_dac3->i_bsmod, p_dac3->i_acmod, p_dac3->i_lfeon, p_dac3->i_bitrate_code );
#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_dvc1( MP4_Box_t *p_box )
{
    free( p_box->data.p_dvc1->p_vc1 );
}

static int MP4_ReadBox_dvc1( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_dvc1_t, MP4_FreeBox_dvc1 );
    if( i_read < 7 )
        MP4_READBOX_EXIT( 0 );

    MP4_Box_data_dvc1_t *p_dvc1 = p_box->data.p_dvc1;
    MP4_GET1BYTE( p_dvc1->i_profile_level );
    p_dvc1->i_vc1 = i_read; /* Header + profile_level */
    if( p_dvc1->i_vc1 > 0 && (p_dvc1->p_vc1 = malloc( p_dvc1->i_vc1 )) )
        memcpy( p_dvc1->p_vc1, p_peek, i_read );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"dvc1\" profile=%"PRIu8, (p_dvc1->i_profile_level & 0xf0) >> 4 );
#endif

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_fiel( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_Box_data_fiel_t *p_fiel;
    MP4_READBOX_ENTER( MP4_Box_data_fiel_t, NULL );
    p_fiel = p_box->data.p_fiel;
    if(i_read < 2)
        MP4_READBOX_EXIT( 0 );
    if(p_peek[0] == 1)
    {
        p_fiel->i_flags = BLOCK_FLAG_SINGLE_FIELD;
    }
    else if(p_peek[0] == 2) /* Interlaced */
    {
        /*
         * 0 – There is only one field.
         * 1 – T is displayed earliest, T is stored first in the file.
         * 6 – B is displayed earliest, B is stored first in the file.
         * 9 – B is displayed earliest, T is stored first in the file.
         * 14 – T is displayed earliest, B is stored first in the file.
        */
        if(p_peek[1] == 1 || p_peek[1] == 9)
            p_fiel->i_flags = BLOCK_FLAG_TOP_FIELD_FIRST;
        else if(p_peek[1] == 6 || p_peek[1] == 14)
            p_fiel->i_flags = BLOCK_FLAG_BOTTOM_FIELD_FIRST;
    }
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_enda( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_Box_data_enda_t *p_enda;
    MP4_READBOX_ENTER( MP4_Box_data_enda_t, NULL );

    p_enda = p_box->data.p_enda;

    MP4_GET2BYTES( p_enda->i_little_endian );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"enda\" little_endian=%d", p_enda->i_little_endian );
#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_sample_soun( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_sample_soun->p_qt_description );
}

static int MP4_ReadBox_sample_soun( stream_t *p_stream, MP4_Box_t *p_box )
{
    p_box->i_handler = ATOM_soun;
    MP4_READBOX_ENTER( MP4_Box_data_sample_soun_t, MP4_FreeBox_sample_soun );
    p_box->data.p_sample_soun->p_qt_description = NULL;

    size_t i_actually_read = i_read + header_size;

    /* Sanity check needed because the "wave" box does also contain an
     * "mp4a" box that we don't understand. */
    if( i_read < 28 )
    {
        MP4_READBOX_EXIT( 1 );
    }

    for( unsigned i = 0; i < 6 ; i++ )
    {
        MP4_GET1BYTE( p_box->data.p_sample_soun->i_reserved1[i] );
    }

    MP4_GET2BYTES( p_box->data.p_sample_soun->i_data_reference_index );

    /*
     * XXX hack -> produce a copy of the nearly complete chunk
     */
    p_box->data.p_sample_soun->i_qt_description = 0;
    p_box->data.p_sample_soun->p_qt_description = NULL;
    if( i_read > 0 )
    {
        p_box->data.p_sample_soun->p_qt_description = malloc( i_read );
        if( p_box->data.p_sample_soun->p_qt_description )
        {
            p_box->data.p_sample_soun->i_qt_description = i_read;
            memcpy( p_box->data.p_sample_soun->p_qt_description, p_peek, i_read );
        }
    }

    MP4_GET2BYTES( p_box->data.p_sample_soun->i_qt_version );
    MP4_GET2BYTES( p_box->data.p_sample_soun->i_qt_revision_level );
    MP4_GET4BYTES( p_box->data.p_sample_soun->i_qt_vendor );

    MP4_GET2BYTES( p_box->data.p_sample_soun->i_channelcount );
    MP4_GET2BYTES( p_box->data.p_sample_soun->i_samplesize );
    MP4_GET2BYTES( p_box->data.p_sample_soun->i_compressionid );
    MP4_GET2BYTES( p_box->data.p_sample_soun->i_reserved3 );
    MP4_GET2BYTES( p_box->data.p_sample_soun->i_sampleratehi );
    MP4_GET2BYTES( p_box->data.p_sample_soun->i_sampleratelo );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"soun\" stsd qt_version %"PRIu16" compid=%"PRIx16,
             p_box->data.p_sample_soun->i_qt_version,
             p_box->data.p_sample_soun->i_compressionid );
#endif
    /* @36 bytes */
    if( p_box->data.p_sample_soun->i_qt_version == 1 && i_read >= 16 )
    {
        /* SoundDescriptionV1 */
        MP4_GET4BYTES( p_box->data.p_sample_soun->i_sample_per_packet );
        MP4_GET4BYTES( p_box->data.p_sample_soun->i_bytes_per_packet );
        MP4_GET4BYTES( p_box->data.p_sample_soun->i_bytes_per_frame );
        MP4_GET4BYTES( p_box->data.p_sample_soun->i_bytes_per_sample );

#ifdef MP4_VERBOSE
        msg_Dbg( p_stream,
                 "read box: \"soun\" V1 sample/packet=%d bytes/packet=%d "
                 "bytes/frame=%d bytes/sample=%d",
                 p_box->data.p_sample_soun->i_sample_per_packet,
                 p_box->data.p_sample_soun->i_bytes_per_packet,
                 p_box->data.p_sample_soun->i_bytes_per_frame,
                 p_box->data.p_sample_soun->i_bytes_per_sample );
#endif
        /* @52 bytes */
    }
    else if( p_box->data.p_sample_soun->i_qt_version == 2 && i_read >= 36 )
    {
        /* SoundDescriptionV2 */
        double f_sample_rate;
        int64_t i_dummy64;
        uint32_t i_channel, i_extoffset, i_dummy32;

        /* Checks */
        if ( p_box->data.p_sample_soun->i_channelcount != 0x3  ||
             p_box->data.p_sample_soun->i_samplesize != 0x0010 ||
             p_box->data.p_sample_soun->i_compressionid != 0xFFFE ||
             p_box->data.p_sample_soun->i_reserved3 != 0x0     ||
             p_box->data.p_sample_soun->i_sampleratehi != 0x1  ||//65536
             p_box->data.p_sample_soun->i_sampleratelo != 0x0 )  //remainder
        {
            msg_Err( p_stream, "invalid stsd V2 box defaults" );
            MP4_READBOX_EXIT( 0 );
        }
        /* !Checks */

        MP4_GET4BYTES( i_extoffset ); /* offset to stsd extentions */
        MP4_GET8BYTES( i_dummy64 );
        memcpy( &f_sample_rate, &i_dummy64, 8 );
        msg_Dbg( p_stream, "read box: %f Hz", f_sample_rate );
        /* Rounding error with lo, but we don't care since we do not support fractional audio rate */
        p_box->data.p_sample_soun->i_sampleratehi = (uint32_t)f_sample_rate;
        p_box->data.p_sample_soun->i_sampleratelo = (f_sample_rate - p_box->data.p_sample_soun->i_sampleratehi);

        MP4_GET4BYTES( i_channel );
        p_box->data.p_sample_soun->i_channelcount = i_channel;

        MP4_GET4BYTES( i_dummy32 );
        if ( i_dummy32 != 0x7F000000 )
        {
            msg_Err( p_stream, "invalid stsd V2 box" );
            MP4_READBOX_EXIT( 0 );
        }

        MP4_GET4BYTES( p_box->data.p_sample_soun->i_constbitsperchannel );
        MP4_GET4BYTES( p_box->data.p_sample_soun->i_formatflags );
        MP4_GET4BYTES( p_box->data.p_sample_soun->i_constbytesperaudiopacket );
        MP4_GET4BYTES( p_box->data.p_sample_soun->i_constLPCMframesperaudiopacket );

#ifdef MP4_VERBOSE
        msg_Dbg( p_stream, "read box: \"soun\" V2 rate=%f bitsperchannel=%u "
                           "flags=%u bytesperpacket=%u lpcmframesperpacket=%u",
                 f_sample_rate,
                 p_box->data.p_sample_soun->i_constbitsperchannel,
                 p_box->data.p_sample_soun->i_formatflags,
                 p_box->data.p_sample_soun->i_constbytesperaudiopacket,
                 p_box->data.p_sample_soun->i_constLPCMframesperaudiopacket );
#endif
        /* @72 bytes + */
        if( i_extoffset > i_actually_read )
            i_extoffset = i_actually_read;
        p_peek = &p_buff[i_extoffset];
        i_read = i_actually_read - i_extoffset;
    }
    else
    {
        p_box->data.p_sample_soun->i_sample_per_packet = 0;
        p_box->data.p_sample_soun->i_bytes_per_packet = 0;
        p_box->data.p_sample_soun->i_bytes_per_frame = 0;
        p_box->data.p_sample_soun->i_bytes_per_sample = 0;

#ifdef MP4_VERBOSE
        msg_Dbg( p_stream, "read box: \"soun\" V0 or qt1/2 (rest=%"PRIu64")",
                 i_read );
#endif
        /* @36 bytes */
    }

    if( p_box->i_type == ATOM_drms )
    {
        msg_Warn( p_stream, "DRM protected streams are not supported." );
        MP4_READBOX_EXIT( 0 );
    }

    if( p_box->i_type == ATOM_samr || p_box->i_type == ATOM_sawb )
    {
        /* Ignore channelcount for AMR (3gpp AMRSpecificBox) */
        p_box->data.p_sample_soun->i_channelcount = 1;
    }

    /* Loads extensions */
    MP4_ReadBoxContainerRawInBox( p_stream, p_box, p_peek, i_read,
                                  p_box->i_pos + p_peek - p_buff ); /* esds/wave/... */

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"soun\" in stsd channel %d "
             "sample size %d sample rate %f",
             p_box->data.p_sample_soun->i_channelcount,
             p_box->data.p_sample_soun->i_samplesize,
             (float)p_box->data.p_sample_soun->i_sampleratehi +
             (float)p_box->data.p_sample_soun->i_sampleratelo / BLOCK16x16 );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_sample_vide( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_sample_vide->p_qt_image_description );
}

int MP4_ReadBox_sample_vide( stream_t *p_stream, MP4_Box_t *p_box )
{
    p_box->i_handler = ATOM_vide;
    MP4_READBOX_ENTER( MP4_Box_data_sample_vide_t, MP4_FreeBox_sample_vide );

    size_t i_actually_read = i_read + header_size;

    for( unsigned i = 0; i < 6 ; i++ )
    {
        MP4_GET1BYTE( p_box->data.p_sample_vide->i_reserved1[i] );
    }

    MP4_GET2BYTES( p_box->data.p_sample_vide->i_data_reference_index );

    /*
     * XXX hack -> produce a copy of the nearly complete chunk
     */
    if( i_read > 0 )
    {
        p_box->data.p_sample_vide->p_qt_image_description = malloc( i_read );
        if( unlikely( p_box->data.p_sample_vide->p_qt_image_description == NULL ) )
            MP4_READBOX_EXIT( 0 );
        p_box->data.p_sample_vide->i_qt_image_description = i_read;
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

    if ( i_read < 32 )
        MP4_READBOX_EXIT( 0 );
    if( p_peek[0] <= 31 ) // Must be Pascal String
    {
        memcpy( &p_box->data.p_sample_vide->sz_compressorname, &p_peek[1], p_peek[0] );
        p_box->data.p_sample_vide->sz_compressorname[p_peek[0]] = 0;
    }
    p_peek += 32; i_read -= 32;

    MP4_GET2BYTES( p_box->data.p_sample_vide->i_depth );
    MP4_GET2BYTES( p_box->data.p_sample_vide->i_qt_color_table );

    if( p_box->i_type == ATOM_drmi )
    {
        msg_Warn( p_stream, "DRM protected streams are not supported." );
        MP4_READBOX_EXIT( 0 );
    }

    if( i_actually_read > 78 && p_peek - p_buff > 78 )
    {
        MP4_ReadBoxContainerRawInBox( p_stream, p_box, p_peek, i_read,
                                      p_box->i_pos + p_peek - p_buff );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"vide\" in stsd %dx%d depth %d (%s)",
                      p_box->data.p_sample_vide->i_width,
                      p_box->data.p_sample_vide->i_height,
                      p_box->data.p_sample_vide->i_depth,
                      p_box->data.p_sample_vide->sz_compressorname );

#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_sample_mp4s( stream_t *p_stream, MP4_Box_t *p_box )
{
    p_box->i_handler = ATOM_text;
    MP4_READBOX_ENTER_PARTIAL( MP4_Box_data_sample_text_t, 16, NULL );
    (void) p_peek;
    if( i_read < 8 )
        MP4_READBOX_EXIT( 0 );

    MP4_ReadBoxContainerChildren( p_stream, p_box, NULL );

    if ( MP4_Seek( p_stream, p_box->i_pos + p_box->i_size ) )
        MP4_READBOX_EXIT( 0 );

    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_sample_hint( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_sample_hint->p_data );
}

static int MP4_ReadBox_sample_hint8( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER_PARTIAL( MP4_Box_data_sample_hint_t, 24, MP4_FreeBox_sample_hint );

    for( unsigned i = 0; i < 6 ; i++ )
    {
        MP4_GET1BYTE( p_box->data.p_sample_hint->i_reserved1[i] );
    }

    MP4_GET2BYTES( p_box->data.p_sample_hint->i_data_reference_index );

    if( !(p_box->data.p_sample_hint->p_data = malloc(8)) )
        MP4_READBOX_EXIT( 0 );

    MP4_GET8BYTES( *(p_box->data.p_sample_hint->p_data) );

    MP4_ReadBoxContainerChildren(p_stream, p_box, NULL);

    if ( MP4_Seek( p_stream, p_box->i_pos + p_box->i_size ) )
        MP4_READBOX_EXIT( 0 );

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_sample_text( stream_t *p_stream, MP4_Box_t *p_box )
{
    int32_t t;

    p_box->i_handler = ATOM_text;
    MP4_READBOX_ENTER( MP4_Box_data_sample_text_t, NULL );

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
    p_box->data.p_sample_text->i_background_color[3] = 0xFF;

    MP4_GET2BYTES( p_box->data.p_sample_text->i_text_box_top );
    MP4_GET2BYTES( p_box->data.p_sample_text->i_text_box_left );
    MP4_GET2BYTES( p_box->data.p_sample_text->i_text_box_bottom );
    MP4_GET2BYTES( p_box->data.p_sample_text->i_text_box_right );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"text\" in stsd text" );
#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_sample_clcp( stream_t *p_stream, MP4_Box_t *p_box )
{
    p_box->i_handler = ATOM_clcp;
    MP4_READBOX_ENTER( MP4_Box_data_sample_clcp_t, NULL );

    if( i_read < 8 )
        MP4_READBOX_EXIT( 0 );

    for( int i=0; i<6; i++ )
        MP4_GET1BYTE( p_box->data.p_sample_clcp->i_reserved1[i] );
    MP4_GET2BYTES( p_box->data.p_sample_clcp->i_data_reference_index );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"clcp\" in stsd" );
#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_sample_tx3g( stream_t *p_stream, MP4_Box_t *p_box )
{
    p_box->i_handler = ATOM_text;
    MP4_READBOX_ENTER( MP4_Box_data_sample_text_t, NULL );

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

    MP4_GET4BYTES( p_box->data.p_sample_text->i_reserved3 );

    MP4_GET2BYTES( p_box->data.p_sample_text->i_font_id );
    MP4_GET1BYTE ( p_box->data.p_sample_text->i_font_face );
    MP4_GET1BYTE ( p_box->data.p_sample_text->i_font_size );
    MP4_GET4BYTES( p_box->data.p_sample_text->i_font_color );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"tx3g\" in stsd text" );
#endif
    MP4_READBOX_EXIT( 1 );
}


#if 0
/* We can't easily call it, and anyway ~ 20 bytes lost isn't a real problem */
static void MP4_FreeBox_sample_text( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_sample_text->psz_text_name );
}
#endif

static void MP4_FreeBox_stsz( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_stsz->i_entry_size );
}

static int MP4_ReadBox_stsz( stream_t *p_stream, MP4_Box_t *p_box )
{
    uint32_t count;

    MP4_READBOX_ENTER( MP4_Box_data_stsz_t, MP4_FreeBox_stsz );

    MP4_GETVERSIONFLAGS( p_box->data.p_stsz );

    MP4_GET4BYTES( p_box->data.p_stsz->i_sample_size );
    MP4_GET4BYTES( count );
    p_box->data.p_stsz->i_sample_count = count;

    if( p_box->data.p_stsz->i_sample_size == 0 )
    {
        if( UINT64_C(4) * count > i_read )
            MP4_READBOX_EXIT( 0 );

        p_box->data.p_stsz->i_entry_size =
            vlc_alloc( count, sizeof(uint32_t) );
        if( unlikely( !p_box->data.p_stsz->i_entry_size ) )
            MP4_READBOX_EXIT( 0 );

        for( uint32_t i = 0; i < count; i++ )
        {
            MP4_GET4BYTES( p_box->data.p_stsz->i_entry_size[i] );
        }
    }
    else
        p_box->data.p_stsz->i_entry_size = NULL;

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"stsz\" sample-size %d sample-count %d",
                      p_box->data.p_stsz->i_sample_size,
                      p_box->data.p_stsz->i_sample_count );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_stsc( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_stsc->i_first_chunk );
    FREENULL( p_box->data.p_stsc->i_samples_per_chunk );
    FREENULL( p_box->data.p_stsc->i_sample_description_index );
}

static int MP4_ReadBox_stsc( stream_t *p_stream, MP4_Box_t *p_box )
{
    uint32_t count;

    MP4_READBOX_ENTER( MP4_Box_data_stsc_t, MP4_FreeBox_stsc );

    MP4_GETVERSIONFLAGS( p_box->data.p_stsc );
    MP4_GET4BYTES( count );

    if( UINT64_C(12) * count > i_read )
        MP4_READBOX_EXIT( 0 );

    p_box->data.p_stsc->i_first_chunk = vlc_alloc( count, sizeof(uint32_t) );
    p_box->data.p_stsc->i_samples_per_chunk = vlc_alloc( count,
                                                         sizeof(uint32_t) );
    p_box->data.p_stsc->i_sample_description_index = vlc_alloc( count,
                                                            sizeof(uint32_t) );
    if( unlikely( p_box->data.p_stsc->i_first_chunk == NULL
     || p_box->data.p_stsc->i_samples_per_chunk == NULL
     || p_box->data.p_stsc->i_sample_description_index == NULL ) )
    {
        MP4_READBOX_EXIT( 0 );
    }
    p_box->data.p_stsc->i_entry_count = count;

    for( uint32_t i = 0; i < count;i++ )
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

static void MP4_FreeBox_sdp( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_sdp->psz_text );
}

static int MP4_ReadBox_sdp( stream_t *p_stream, MP4_Box_t *p_box )
{
   MP4_READBOX_ENTER( MP4_Box_data_sdp_t, MP4_FreeBox_sdp );

   MP4_GETSTRINGZ( p_box->data.p_sdp->psz_text );

   MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_rtp( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_moviehintinformation_rtp->psz_text );
}

static int MP4_ReadBox_rtp( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_moviehintinformation_rtp_t, MP4_FreeBox_rtp );

    MP4_GET4BYTES( p_box->data.p_moviehintinformation_rtp->i_description_format );

    MP4_GETSTRINGZ( p_box->data.p_moviehintinformation_rtp->psz_text );

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_tims( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_tims_t, NULL );

    MP4_GET4BYTES( p_box->data.p_tims->i_timescale );

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_tsro( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_tsro_t, NULL );

    MP4_GET4BYTES( p_box->data.p_tsro->i_offset );

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_tssy( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_tssy_t,  NULL );

    MP4_GET1BYTE( p_box->data.p_tssy->i_reserved_timestamp_sync );

    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_stco_co64( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_co64->i_chunk_offset );
}

static int MP4_ReadBox_stco_co64( stream_t *p_stream, MP4_Box_t *p_box )
{
    const bool sixtyfour = p_box->i_type != ATOM_stco;
    uint32_t count;

    MP4_READBOX_ENTER( MP4_Box_data_co64_t, MP4_FreeBox_stco_co64 );

    MP4_GETVERSIONFLAGS( p_box->data.p_co64 );
    MP4_GET4BYTES( count );

    if( (sixtyfour ? UINT64_C(8) : UINT64_C(4)) * count > i_read )
        MP4_READBOX_EXIT( 0 );

    p_box->data.p_co64->i_chunk_offset = vlc_alloc( count, sizeof(uint64_t) );
    if( unlikely(p_box->data.p_co64->i_chunk_offset == NULL) )
        MP4_READBOX_EXIT( 0 );
    p_box->data.p_co64->i_entry_count = count;

    for( uint32_t i = 0; i < count; i++ )
    {
        if( sixtyfour )
            MP4_GET8BYTES( p_box->data.p_co64->i_chunk_offset[i] );
        else
            MP4_GET4BYTES( p_box->data.p_co64->i_chunk_offset[i] );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"co64\" entry-count %d",
                      p_box->data.p_co64->i_entry_count );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_stss( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_stss->i_sample_number );
}

static int MP4_ReadBox_stss( stream_t *p_stream, MP4_Box_t *p_box )
{
    uint32_t count;

    MP4_READBOX_ENTER( MP4_Box_data_stss_t, MP4_FreeBox_stss );

    MP4_GETVERSIONFLAGS( p_box->data.p_stss );
    MP4_GET4BYTES( count );

    if( UINT64_C(4) * count > i_read )
        MP4_READBOX_EXIT( 0 );

    p_box->data.p_stss->i_sample_number = vlc_alloc( count, sizeof(uint32_t) );
    if( unlikely( p_box->data.p_stss->i_sample_number == NULL ) )
        MP4_READBOX_EXIT( 0 );
    p_box->data.p_stss->i_entry_count = count;

    for( uint32_t i = 0; i < count; i++ )
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

static void MP4_FreeBox_stsh( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_stsh->i_shadowed_sample_number );
    FREENULL( p_box->data.p_stsh->i_sync_sample_number );
}

static int MP4_ReadBox_stsh( stream_t *p_stream, MP4_Box_t *p_box )
{
    uint32_t count;

    MP4_READBOX_ENTER( MP4_Box_data_stsh_t, MP4_FreeBox_stsh );

    MP4_GETVERSIONFLAGS( p_box->data.p_stsh );
    MP4_GET4BYTES( count );

    if( UINT64_C(8) * count > i_read )
        MP4_READBOX_EXIT( 0 );

    p_box->data.p_stsh->i_shadowed_sample_number = vlc_alloc( count,
                                                            sizeof(uint32_t) );
    p_box->data.p_stsh->i_sync_sample_number = vlc_alloc( count,
                                                          sizeof(uint32_t) );
    if( p_box->data.p_stsh->i_shadowed_sample_number == NULL
     || p_box->data.p_stsh->i_sync_sample_number == NULL )
        MP4_READBOX_EXIT( 0 );
    p_box->data.p_stsh->i_entry_count = count;

    for( uint32_t i = 0; i < p_box->data.p_stss->i_entry_count; i++ )
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

static void MP4_FreeBox_stdp( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_stdp->i_priority );
}

static int MP4_ReadBox_stdp( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_stdp_t, MP4_FreeBox_stdp );

    MP4_GETVERSIONFLAGS( p_box->data.p_stdp );

    p_box->data.p_stdp->i_priority =
        calloc( i_read / 2, sizeof(uint16_t) );

    if( unlikely( !p_box->data.p_stdp->i_priority ) )
        MP4_READBOX_EXIT( 0 );

    for( unsigned i = 0; i < i_read / 2 ; i++ )
    {
        MP4_GET2BYTES( p_box->data.p_stdp->i_priority[i] );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"stdp\" entry-count %"PRId64,
                      i_read / 2 );

#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_elst( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_elst->i_segment_duration );
    FREENULL( p_box->data.p_elst->i_media_time );
    FREENULL( p_box->data.p_elst->i_media_rate_integer );
    FREENULL( p_box->data.p_elst->i_media_rate_fraction );
}

static int MP4_ReadBox_elst( stream_t *p_stream, MP4_Box_t *p_box )
{
    uint32_t count;

    MP4_READBOX_ENTER( MP4_Box_data_elst_t, MP4_FreeBox_elst );

    MP4_GETVERSIONFLAGS( p_box->data.p_elst );
    MP4_GET4BYTES( count );

    if( count == 0 )
        MP4_READBOX_EXIT( 1 );

    uint32_t i_entries_max = i_read / ((p_box->data.p_elst->i_version == 1) ? 20 : 12);
    if( count > i_entries_max )
        count = i_entries_max;

    p_box->data.p_elst->i_segment_duration = vlc_alloc( count,
                                                        sizeof(uint64_t) );
    p_box->data.p_elst->i_media_time = vlc_alloc( count, sizeof(int64_t) );
    p_box->data.p_elst->i_media_rate_integer = vlc_alloc( count,
                                                          sizeof(uint16_t) );
    p_box->data.p_elst->i_media_rate_fraction = vlc_alloc( count,
                                                           sizeof(uint16_t) );
    if( p_box->data.p_elst->i_segment_duration == NULL
     || p_box->data.p_elst->i_media_time == NULL
     || p_box->data.p_elst->i_media_rate_integer == NULL
     || p_box->data.p_elst->i_media_rate_fraction == NULL )
    {
        MP4_READBOX_EXIT( 0 );
    }
    p_box->data.p_elst->i_entry_count = count;

    for( uint32_t i = 0; i < count; i++ )
    {
        uint64_t segment_duration;
        int64_t media_time;

        if( p_box->data.p_elst->i_version == 1 )
        {
            union { int64_t s; uint64_t u; } u;

            MP4_GET8BYTES( segment_duration );
            MP4_GET8BYTES( u.u );
            media_time = u.s;
        }
        else
        {
            union { int32_t s; uint32_t u; } u;

            MP4_GET4BYTES( segment_duration );
            MP4_GET4BYTES( u.u );
            media_time = u.s;
        }

        p_box->data.p_elst->i_segment_duration[i] = segment_duration;
        p_box->data.p_elst->i_media_time[i] = media_time;
        MP4_GET2BYTES( p_box->data.p_elst->i_media_rate_integer[i] );
        MP4_GET2BYTES( p_box->data.p_elst->i_media_rate_fraction[i] );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"elst\" entry-count %" PRIu32,
             p_box->data.p_elst->i_entry_count );
#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_cprt( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_cprt->psz_notice );
}

static int MP4_ReadBox_cprt( stream_t *p_stream, MP4_Box_t *p_box )
{
    uint16_t i_language;
    bool b_mac;

    MP4_READBOX_ENTER( MP4_Box_data_cprt_t, MP4_FreeBox_cprt );

    MP4_GETVERSIONFLAGS( p_box->data.p_cprt );

    MP4_GET2BYTES( i_language );
    decodeQtLanguageCode( i_language, p_box->data.p_cprt->rgs_language, &b_mac );

    MP4_GETSTRINGZ( p_box->data.p_cprt->psz_notice );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"cprt\" language %3.3s notice %s",
                      p_box->data.p_cprt->rgs_language,
                      p_box->data.p_cprt->psz_notice );

#endif
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_dcom( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_dcom_t, NULL );

    MP4_GETFOURCC( p_box->data.p_dcom->i_algorithm );
#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"dcom\" compression algorithm : %4.4s",
                      (char*)&p_box->data.p_dcom->i_algorithm );
#endif
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_cmvd( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_cmvd->p_data );
}

static int MP4_ReadBox_cmvd( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_cmvd_t, MP4_FreeBox_cmvd );

    MP4_GET4BYTES( p_box->data.p_cmvd->i_uncompressed_size );

    p_box->data.p_cmvd->i_compressed_size = i_read;

    if( !( p_box->data.p_cmvd->p_data = malloc( i_read ) ) )
        MP4_READBOX_EXIT( 0 );

    /* now copy compressed data */
    memcpy( p_box->data.p_cmvd->p_data, p_peek,i_read);

    p_box->data.p_cmvd->b_compressed = 1;

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"cmvd\" compressed data size %d",
                      p_box->data.p_cmvd->i_compressed_size );
#endif

    MP4_READBOX_EXIT( 1 );
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

    if( !( p_box->data.p_cmov = calloc(1, sizeof( MP4_Box_data_cmov_t ) ) ) )
        return 0;

    if( !p_box->p_father ||
        ( p_box->p_father->i_type != ATOM_moov &&
          p_box->p_father->i_type != ATOM_foov ) )
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
        return 0;
    }

    if( p_dcom->data.p_dcom->i_algorithm != ATOM_zlib )
    {
        msg_Dbg( p_stream, "read box: \"cmov\" compression algorithm : %4.4s "
                 "not supported", (char*)&p_dcom->data.p_dcom->i_algorithm );
        return 0;
    }

#ifndef HAVE_ZLIB_H
    msg_Dbg( p_stream, "read box: \"cmov\" zlib unsupported" );
    return 0;

#else
    /* decompress data */
    /* allocate a new buffer */
    if( !( p_data = malloc( p_cmvd->data.p_cmvd->i_uncompressed_size ) ) )
        return 0;
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
        return 0;
    }

    /* uncompress */
    i_result = inflate( &z_data, Z_NO_FLUSH );
    if( i_result != Z_OK && i_result != Z_STREAM_END )
    {
        msg_Err( p_stream, "read box: \"cmov\" error while uncompressing" );
        free( p_data );
        return 0;
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

    msg_Dbg( p_stream, "read box: \"cmov\" box successfully uncompressed" );

    /* now create a memory stream */
    p_stream_memory =
        vlc_stream_MemoryNew( VLC_OBJECT(p_stream),
                              p_cmvd->data.p_cmvd->p_data,
                              p_cmvd->data.p_cmvd->i_uncompressed_size, true );

    /* and read uncompressd moov */
    p_box->data.p_cmov->p_moov = MP4_ReadBox( p_stream_memory, NULL );

    vlc_stream_Delete( p_stream_memory );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"cmov\" compressed movie header completed");
#endif

    return p_box->data.p_cmov->p_moov ? 1 : 0;
#endif /* HAVE_ZLIB_H */
}

static void MP4_FreeBox_rdrf( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_rdrf->psz_ref );
}

static int MP4_ReadBox_rdrf( stream_t *p_stream, MP4_Box_t *p_box )
{
    uint32_t i_len;
    MP4_READBOX_ENTER( MP4_Box_data_rdrf_t, MP4_FreeBox_rdrf );

    MP4_GETVERSIONFLAGS( p_box->data.p_rdrf );
    MP4_GETFOURCC( p_box->data.p_rdrf->i_ref_type );
    MP4_GET4BYTES( i_len );
    i_len++;

    if( i_len > 0 )
    {
        p_box->data.p_rdrf->psz_ref = malloc( i_len );
        if( p_box->data.p_rdrf->psz_ref == NULL )
            MP4_READBOX_EXIT( 0 );
        i_len--;

        for( unsigned i = 0; i < i_len; i++ )
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



static int MP4_ReadBox_rmdr( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_rmdr_t, NULL );

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
    MP4_READBOX_ENTER( MP4_Box_data_rmqu_t, NULL );

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
    MP4_READBOX_ENTER( MP4_Box_data_rmvc_t, NULL );
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

static int MP4_ReadBox_frma( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_frma_t, NULL );

    MP4_GETFOURCC( p_box->data.p_frma->i_type );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"frma\" i_type:%4.4s",
             (char *)&p_box->data.p_frma->i_type );
#endif

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_skcr( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_skcr_t, NULL );

    MP4_GET4BYTES( p_box->data.p_skcr->i_init );
    MP4_GET4BYTES( p_box->data.p_skcr->i_encr );
    MP4_GET4BYTES( p_box->data.p_skcr->i_decr );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"skcr\" i_init:%d i_encr:%d i_decr:%d",
             p_box->data.p_skcr->i_init,
             p_box->data.p_skcr->i_encr,
             p_box->data.p_skcr->i_decr );
#endif

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_drms( stream_t *p_stream, MP4_Box_t *p_box )
{
    VLC_UNUSED(p_box);
    /* ATOMs 'user', 'key', 'iviv', and 'priv' will be skipped,
     * so unless data decrypt itself by magic, there will be no playback,
     * but we never know... */
    msg_Warn( p_stream, "DRM protected streams are not supported." );
    return 1;
}

static void MP4_FreeBox_Binary( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_binary->p_blob );
    p_box->data.p_binary->i_blob = 0;
}

static int MP4_ReadBox_Binary( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_binary_t, MP4_FreeBox_Binary );
    i_read = __MIN( i_read, UINT32_MAX );
    if ( i_read > 0 )
    {
        p_box->data.p_binary->p_blob = malloc( i_read );
        if ( p_box->data.p_binary->p_blob )
        {
            memcpy( p_box->data.p_binary->p_blob, p_peek, i_read );
            p_box->data.p_binary->i_blob = i_read;
        }
    }
    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_data( MP4_Box_t *p_box )
{
    free( p_box->data.p_data->p_blob );
}

static int MP4_ReadBox_data( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_data_t, MP4_FreeBox_data );
    MP4_Box_data_data_t *p_data = p_box->data.p_data;

    if ( i_read < 8 || i_read - 8 > UINT32_MAX )
        MP4_READBOX_EXIT( 0 );

    uint8_t i_type;
    MP4_GET1BYTE( i_type );
    if ( i_type != 0 )
    {
#ifdef MP4_VERBOSE
        msg_Dbg( p_stream, "skipping unknown 'data' atom with type %"PRIu8, i_type );
#endif
        MP4_READBOX_EXIT( 0 );
    }

    MP4_GET3BYTES( p_data->e_wellknowntype );
    MP4_GET2BYTES( p_data->locale.i_country );
    MP4_GET2BYTES( p_data->locale.i_language );
#ifdef MP4_VERBOSE
        msg_Dbg( p_stream, "read 'data' atom: knowntype=%"PRIu32", country=%"PRIu16" lang=%"PRIu16
                 ", size %"PRIu64" bytes", p_data->e_wellknowntype,
                 p_data->locale.i_country, p_data->locale.i_language, i_read );
#endif
    p_box->data.p_data->p_blob = malloc( i_read );
    if ( !p_box->data.p_data->p_blob )
        MP4_READBOX_EXIT( 0 );

    p_box->data.p_data->i_blob = i_read;
    memcpy( p_box->data.p_data->p_blob, p_peek, i_read);

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_Metadata( stream_t *p_stream, MP4_Box_t *p_box )
{
    const uint8_t *p_peek;
    if ( vlc_stream_Peek( p_stream, &p_peek, 16 ) < 16 )
        return 0;
    if ( vlc_stream_Read( p_stream, NULL, 8 ) < 8 )
        return 0;
    const uint32_t stoplist[] = { ATOM_data, 0 };
    return MP4_ReadBoxContainerChildren( p_stream, p_box, stoplist );
}

/* Chapter support */
static void MP4_FreeBox_chpl( MP4_Box_t *p_box )
{
    MP4_Box_data_chpl_t *p_chpl = p_box->data.p_chpl;
    for( unsigned i = 0; i < p_chpl->i_chapter; i++ )
        free( p_chpl->chapter[i].psz_name );
}

static int MP4_ReadBox_chpl( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_Box_data_chpl_t *p_chpl;
    uint32_t i_dummy;
    VLC_UNUSED(i_dummy);
    int i;
    MP4_READBOX_ENTER( MP4_Box_data_chpl_t, MP4_FreeBox_chpl );

    p_chpl = p_box->data.p_chpl;

    MP4_GETVERSIONFLAGS( p_chpl );

    if ( i_read < 5 || p_chpl->i_version != 0x1 )
        MP4_READBOX_EXIT( 0 );

    MP4_GET4BYTES( i_dummy );

    MP4_GET1BYTE( p_chpl->i_chapter );

    for( i = 0; i < p_chpl->i_chapter; i++ )
    {
        uint64_t i_start;
        uint8_t i_len;
        int i_copy;
        if ( i_read < 9 )
            break;
        MP4_GET8BYTES( i_start );
        MP4_GET1BYTE( i_len );

        p_chpl->chapter[i].psz_name = malloc( i_len + 1 );
        if( !p_chpl->chapter[i].psz_name )
            MP4_READBOX_EXIT( 0 );

        i_copy = __MIN( i_len, i_read );
        if( i_copy > 0 )
            memcpy( p_chpl->chapter[i].psz_name, p_peek, i_copy );
        p_chpl->chapter[i].psz_name[i_copy] = '\0';
        p_chpl->chapter[i].i_start = i_start;

        p_peek += i_copy;
        i_read -= i_copy;
    }

    if ( i != p_chpl->i_chapter )
        p_chpl->i_chapter = i;

    /* Bubble sort by increasing start date */
    do
    {
        for( i = 0; i < p_chpl->i_chapter - 1; i++ )
        {
            if( p_chpl->chapter[i].i_start > p_chpl->chapter[i+1].i_start )
            {
                char *psz = p_chpl->chapter[i+1].psz_name;
                int64_t i64 = p_chpl->chapter[i+1].i_start;

                p_chpl->chapter[i+1].psz_name = p_chpl->chapter[i].psz_name;
                p_chpl->chapter[i+1].i_start = p_chpl->chapter[i].i_start;

                p_chpl->chapter[i].psz_name = psz;
                p_chpl->chapter[i].i_start = i64;

                i = -1;
                break;
            }
        }
    } while( i == -1 );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"chpl\" %d chapters",
                       p_chpl->i_chapter );
#endif
    MP4_READBOX_EXIT( 1 );
}

/* GoPro HiLight tags support */
static void MP4_FreeBox_HMMT( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_hmmt->pi_chapter_start );
}

static int MP4_ReadBox_HMMT( stream_t *p_stream, MP4_Box_t *p_box )
{
#define MAX_CHAPTER_COUNT 100

    MP4_Box_data_HMMT_t *p_hmmt;
    MP4_READBOX_ENTER( MP4_Box_data_HMMT_t, MP4_FreeBox_HMMT );

    if( i_read < 4 )
        MP4_READBOX_EXIT( 0 );

    p_hmmt = p_box->data.p_hmmt;

    MP4_GET4BYTES( p_hmmt->i_chapter_count );

    if( p_hmmt->i_chapter_count <= 0 )
    {
        p_hmmt->pi_chapter_start = NULL;
        MP4_READBOX_EXIT( 1 );
    }

    if( ( i_read / sizeof(uint32_t) ) < p_hmmt->i_chapter_count )
        MP4_READBOX_EXIT( 0 );

    /* Cameras are allowing a maximum of 100 tags */
    if( p_hmmt->i_chapter_count > MAX_CHAPTER_COUNT )
        p_hmmt->i_chapter_count = MAX_CHAPTER_COUNT;

    p_hmmt->pi_chapter_start = vlc_alloc( p_hmmt->i_chapter_count, sizeof(uint32_t) );
    if( p_hmmt->pi_chapter_start == NULL )
        MP4_READBOX_EXIT( 0 );

    for( uint32_t i = 0; i < p_hmmt->i_chapter_count; i++ )
    {
        MP4_GET4BYTES( p_hmmt->pi_chapter_start[i] );
    }

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "read box: \"HMMT\" %d HiLight tags", p_hmmt->i_chapter_count );
#endif

    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_tref_generic( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_tref_generic->i_track_ID );
}

static int MP4_ReadBox_tref_generic( stream_t *p_stream, MP4_Box_t *p_box )
{
    uint32_t count;

    MP4_READBOX_ENTER( MP4_Box_data_tref_generic_t, MP4_FreeBox_tref_generic );

    p_box->data.p_tref_generic->i_track_ID = NULL;
    count = i_read / sizeof(uint32_t);
    p_box->data.p_tref_generic->i_entry_count = count;
    p_box->data.p_tref_generic->i_track_ID = vlc_alloc( count,
                                                        sizeof(uint32_t) );
    if( p_box->data.p_tref_generic->i_track_ID == NULL )
        MP4_READBOX_EXIT( 0 );

    for( unsigned i = 0; i < count; i++ )
    {
        MP4_GET4BYTES( p_box->data.p_tref_generic->i_track_ID[i] );
    }
#ifdef MP4_VERBOSE
        msg_Dbg( p_stream, "read box: \"chap\" %d references",
                 p_box->data.p_tref_generic->i_entry_count );
#endif

    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_keys( MP4_Box_t *p_box )
{
    for( uint32_t i=0; i<p_box->data.p_keys->i_entry_count; i++ )
        free( p_box->data.p_keys->p_entries[i].psz_value );
    free( p_box->data.p_keys->p_entries );
}

static int MP4_ReadBox_keys( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_keys_t, MP4_FreeBox_keys );

    if ( i_read < 8 )
        MP4_READBOX_EXIT( 0 );

    uint32_t i_count;
    MP4_GET4BYTES( i_count ); /* reserved + flags */
    if ( i_count != 0 )
        MP4_READBOX_EXIT( 0 );

    MP4_GET4BYTES( i_count );
    p_box->data.p_keys->p_entries = calloc( i_count, sizeof(*p_box->data.p_keys->p_entries) );
    if ( !p_box->data.p_keys->p_entries )
        MP4_READBOX_EXIT( 0 );
    p_box->data.p_keys->i_entry_count = i_count;

    uint32_t i=0;
    for( ; i < i_count; i++ )
    {
        if ( i_read < 8 )
            break;
        uint32_t i_keysize;
        MP4_GET4BYTES( i_keysize );
        if ( (i_keysize < 8) || (i_keysize - 4 > i_read) )
            break;
        MP4_GETFOURCC( p_box->data.p_keys->p_entries[i].i_namespace );
        i_keysize -= 8;
        p_box->data.p_keys->p_entries[i].psz_value = malloc( i_keysize + 1 );
        if ( !p_box->data.p_keys->p_entries[i].psz_value )
            break;
        memcpy( p_box->data.p_keys->p_entries[i].psz_value, p_peek, i_keysize );
        p_box->data.p_keys->p_entries[i].psz_value[i_keysize] = 0;
        p_peek += i_keysize;
        i_read -= i_keysize;
#ifdef MP4_ULTRA_VERBOSE
        msg_Dbg( p_stream, "read box: \"keys\": %u '%s'", i + 1,
                 p_box->data.p_keys->p_entries[i].psz_value );
#endif
    }
    if ( i < i_count )
        p_box->data.p_keys->i_entry_count = i;

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_colr( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_colr_t, NULL );
    MP4_GETFOURCC( p_box->data.p_colr->i_type );
    if ( p_box->data.p_colr->i_type == VLC_FOURCC( 'n', 'c', 'l', 'c' ) ||
         p_box->data.p_colr->i_type == VLC_FOURCC( 'n', 'c', 'l', 'x' ) )
    {
        MP4_GET2BYTES( p_box->data.p_colr->nclc.i_primary_idx );
        MP4_GET2BYTES( p_box->data.p_colr->nclc.i_transfer_function_idx );
        MP4_GET2BYTES( p_box->data.p_colr->nclc.i_matrix_idx );
        if ( p_box->data.p_colr->i_type == VLC_FOURCC( 'n', 'c', 'l', 'x' ) )
            MP4_GET1BYTE( p_box->data.p_colr->nclc.i_full_range );
    }
    else
    {
#ifdef MP4_VERBOSE
        msg_Warn( p_stream, "Unhandled colr type: %4.4s", (char*)&p_box->data.p_colr->i_type );
#endif
    }
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_meta( stream_t *p_stream, MP4_Box_t *p_box )
{
    const uint8_t *p_peek;
    const size_t i_headersize = mp4_box_headersize( p_box );

    if( p_box->i_size < 16 || p_box->i_size - i_headersize < 8 )
        return 0;

    /* skip over box header */
    if( vlc_stream_Read( p_stream, NULL, i_headersize ) < (ssize_t) i_headersize )
        return 0;

    /* meta content starts with a 4 byte version/flags value (should be 0) */
    if( vlc_stream_Peek( p_stream, &p_peek, 8 ) < 8 )
        return 0;

    if( !memcmp( p_peek, "\0\0\0", 4 ) ) /* correct header case */
    {
        if( vlc_stream_Read( p_stream, NULL, 4 ) < 4 )
            return 0;
    }
    else if( memcmp( &p_peek[4], "hdlr", 4 ) ) /* Broken, headerless ones */
    {
       return 0;
    }

    /* load child atoms up to the handler (which should be next anyway) */
    const uint32_t stoplist[] = { ATOM_hdlr, 0 };
    if ( !MP4_ReadBoxContainerChildren( p_stream, p_box, stoplist ) )
        return 0;

    /* Mandatory */
    const MP4_Box_t *p_hdlr = MP4_BoxGet( p_box, "hdlr" );
    if ( p_hdlr && BOXDATA(p_hdlr) && BOXDATA(p_hdlr)->i_version == 0 )
    {
        p_box->i_handler = BOXDATA(p_hdlr)->i_handler_type;
        switch( p_box->i_handler )
        {
            case HANDLER_mdta:
            case HANDLER_mdir:
                /* then it behaves like a container */
                return MP4_ReadBoxContainerChildren( p_stream, p_box, NULL );
            default:
                /* skip parsing, will be seen as empty container */
                break;
        }
    }

    return 1;
}

static int MP4_ReadBox_iods( stream_t *p_stream, MP4_Box_t *p_box )
{
    char i_unused;
    VLC_UNUSED(i_unused);

    MP4_READBOX_ENTER( MP4_Box_data_iods_t, NULL );
    MP4_GETVERSIONFLAGS( p_box->data.p_iods );

    MP4_GET1BYTE( i_unused ); /* tag */
    MP4_GET1BYTE( i_unused ); /* length */

    MP4_GET2BYTES( p_box->data.p_iods->i_object_descriptor ); /* 10bits, 6 other bits
                                                              are used for other flags */
    MP4_GET1BYTE( p_box->data.p_iods->i_OD_profile_level );
    MP4_GET1BYTE( p_box->data.p_iods->i_scene_profile_level );
    MP4_GET1BYTE( p_box->data.p_iods->i_audio_profile_level );
    MP4_GET1BYTE( p_box->data.p_iods->i_visual_profile_level );
    MP4_GET1BYTE( p_box->data.p_iods->i_graphics_profile_level );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"iods\" objectDescriptorId: %i, OD: %i, scene: %i, audio: %i, "
             "visual: %i, graphics: %i",
             p_box->data.p_iods->i_object_descriptor >> 6,
             p_box->data.p_iods->i_OD_profile_level,
             p_box->data.p_iods->i_scene_profile_level,
             p_box->data.p_iods->i_audio_profile_level,
             p_box->data.p_iods->i_visual_profile_level,
             p_box->data.p_iods->i_graphics_profile_level );
#endif

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_btrt( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_btrt_t, NULL );

    if(i_read != 12)
        MP4_READBOX_EXIT( 0 );

    MP4_GET4BYTES( p_box->data.p_btrt->i_buffer_size );
    MP4_GET4BYTES( p_box->data.p_btrt->i_max_bitrate );
    MP4_GET4BYTES( p_box->data.p_btrt->i_avg_bitrate );

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_pasp( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_pasp_t, NULL );

    MP4_GET4BYTES( p_box->data.p_pasp->i_horizontal_spacing );
    MP4_GET4BYTES( p_box->data.p_pasp->i_vertical_spacing );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"paps\" %dx%d",
             p_box->data.p_pasp->i_horizontal_spacing,
             p_box->data.p_pasp->i_vertical_spacing);
#endif

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_mehd( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_mehd_t, NULL );

    MP4_GETVERSIONFLAGS( p_box->data.p_mehd );
    if( p_box->data.p_mehd->i_version == 1 )
        MP4_GET8BYTES( p_box->data.p_mehd->i_fragment_duration );
    else /* version == 0 */
        MP4_GET4BYTES( p_box->data.p_mehd->i_fragment_duration );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"mehd\" frag dur. %"PRIu64"",
             p_box->data.p_mehd->i_fragment_duration );
#endif

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_trex( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_trex_t, NULL );
    MP4_GETVERSIONFLAGS( p_box->data.p_trex );

    MP4_GET4BYTES( p_box->data.p_trex->i_track_ID );
    MP4_GET4BYTES( p_box->data.p_trex->i_default_sample_description_index );
    MP4_GET4BYTES( p_box->data.p_trex->i_default_sample_duration );
    MP4_GET4BYTES( p_box->data.p_trex->i_default_sample_size );
    MP4_GET4BYTES( p_box->data.p_trex->i_default_sample_flags );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"trex\" trackID: %"PRIu32"",
             p_box->data.p_trex->i_track_ID );
#endif

    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_sdtp( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_sdtp->p_sample_table );
}

static int MP4_ReadBox_sdtp( stream_t *p_stream, MP4_Box_t *p_box )
{
    uint32_t i_sample_count;
    MP4_READBOX_ENTER( MP4_Box_data_sdtp_t, MP4_FreeBox_sdtp );
    MP4_Box_data_sdtp_t *p_sdtp = p_box->data.p_sdtp;
    MP4_GETVERSIONFLAGS( p_box->data.p_sdtp );
    i_sample_count = i_read;

    p_sdtp->p_sample_table = malloc( i_sample_count );
    if( unlikely(p_sdtp->p_sample_table == NULL) )
        MP4_READBOX_EXIT( 0 );

    for( uint32_t i = 0; i < i_sample_count; i++ )
        MP4_GET1BYTE( p_sdtp->p_sample_table[i] );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "i_sample_count is %"PRIu32"", i_sample_count );
    if ( i_sample_count > 3 )
        msg_Dbg( p_stream,
             "read box: \"sdtp\" head: %"PRIx8" %"PRIx8" %"PRIx8" %"PRIx8"",
                 p_sdtp->p_sample_table[0],
                 p_sdtp->p_sample_table[1],
                 p_sdtp->p_sample_table[2],
                 p_sdtp->p_sample_table[3] );
#endif

    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_tsel( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_tsel_t, NULL );
    uint32_t i_version;
    MP4_GET4BYTES( i_version );
    if ( i_version != 0 || i_read < 4 )
        MP4_READBOX_EXIT( 0 );
    MP4_GET4BYTES( p_box->data.p_tsel->i_switch_group );
    /* ignore list of attributes as es are present before switch */
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_mfro( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_mfro_t, NULL );

    MP4_GETVERSIONFLAGS( p_box->data.p_mfro );
    MP4_GET4BYTES( p_box->data.p_mfro->i_size );

#ifdef MP4_VERBOSE
    msg_Dbg( p_stream,
             "read box: \"mfro\" size: %"PRIu32"",
             p_box->data.p_mfro->i_size);
#endif

    MP4_READBOX_EXIT( 1 );
}

static void MP4_FreeBox_tfra( MP4_Box_t *p_box )
{
    FREENULL( p_box->data.p_tfra->p_time );
    FREENULL( p_box->data.p_tfra->p_moof_offset );
    FREENULL( p_box->data.p_tfra->p_traf_number );
    FREENULL( p_box->data.p_tfra->p_trun_number );
    FREENULL( p_box->data.p_tfra->p_sample_number );
}

static int MP4_ReadBox_tfra( stream_t *p_stream, MP4_Box_t *p_box )
{
#define READ_VARIABLE_LENGTH(lengthvar, p_array) switch (lengthvar)\
{\
    case 0:\
        MP4_GET1BYTE( p_array[i] );\
        break;\
    case 1:\
        MP4_GET2BYTES( *((uint16_t *)&p_array[i*2]) );\
        break;\
    case 2:\
        MP4_GET3BYTES( *((uint32_t *)&p_array[i*4]) );\
        break;\
    case 3:\
        MP4_GET4BYTES( *((uint32_t *)&p_array[i*4]) );\
        break;\
    default:\
        goto error;\
}
#define FIX_VARIABLE_LENGTH(lengthvar) if ( lengthvar == 3 ) lengthvar = 4

    uint32_t i_number_of_entries;
    MP4_READBOX_ENTER( MP4_Box_data_tfra_t, MP4_FreeBox_tfra );
    MP4_Box_data_tfra_t *p_tfra = p_box->data.p_tfra;
    MP4_GETVERSIONFLAGS( p_box->data.p_tfra );
    if ( p_tfra->i_version > 1 )
        MP4_READBOX_EXIT( 0 );
    MP4_GET4BYTES( p_tfra->i_track_ID );
    uint32_t i_lengths = 0;
    MP4_GET4BYTES( i_lengths );
    MP4_GET4BYTES( p_tfra->i_number_of_entries );
    i_number_of_entries = p_tfra->i_number_of_entries;
    p_tfra->i_length_size_of_traf_num = i_lengths >> 4;
    p_tfra->i_length_size_of_trun_num = ( i_lengths & 0x0c ) >> 2;
    p_tfra->i_length_size_of_sample_num = i_lengths & 0x03;

    size_t size = 4 + 4*p_tfra->i_version; /* size in {4, 8} */
    p_tfra->p_time = calloc( i_number_of_entries, size );
    p_tfra->p_moof_offset = calloc( i_number_of_entries, size );

    size = 1 + p_tfra->i_length_size_of_traf_num; /* size in [|1, 4|] */
    if ( size == 3 ) size++;
    p_tfra->p_traf_number = calloc( i_number_of_entries, size );
    size = 1 + p_tfra->i_length_size_of_trun_num;
    if ( size == 3 ) size++;
    p_tfra->p_trun_number = calloc( i_number_of_entries, size );
    size = 1 + p_tfra->i_length_size_of_sample_num;
    if ( size == 3 ) size++;
    p_tfra->p_sample_number = calloc( i_number_of_entries, size );

    if( !p_tfra->p_time || !p_tfra->p_moof_offset || !p_tfra->p_traf_number
                        || !p_tfra->p_trun_number || !p_tfra->p_sample_number )
        goto error;

    unsigned i_fields_length = 3 + p_tfra->i_length_size_of_traf_num
            + p_tfra->i_length_size_of_trun_num
            + p_tfra->i_length_size_of_sample_num;

    uint32_t i;
    for( i = 0; i < i_number_of_entries; i++ )
    {

        if( p_tfra->i_version == 1 )
        {
            if ( i_read < i_fields_length + 16 )
                break;
            MP4_GET8BYTES( *((uint64_t *)&p_tfra->p_time[i*2]) );
            MP4_GET8BYTES( *((uint64_t *)&p_tfra->p_moof_offset[i*2]) );
        }
        else
        {
            if ( i_read < i_fields_length + 8 )
                break;
            MP4_GET4BYTES( p_tfra->p_time[i] );
            MP4_GET4BYTES( p_tfra->p_moof_offset[i] );
        }

        READ_VARIABLE_LENGTH(p_tfra->i_length_size_of_traf_num, p_tfra->p_traf_number);
        READ_VARIABLE_LENGTH(p_tfra->i_length_size_of_trun_num, p_tfra->p_trun_number);
        READ_VARIABLE_LENGTH(p_tfra->i_length_size_of_sample_num, p_tfra->p_sample_number);
    }
    if ( i < i_number_of_entries )
        i_number_of_entries = i;

    FIX_VARIABLE_LENGTH(p_tfra->i_length_size_of_traf_num);
    FIX_VARIABLE_LENGTH(p_tfra->i_length_size_of_trun_num);
    FIX_VARIABLE_LENGTH(p_tfra->i_length_size_of_sample_num);

#ifdef MP4_ULTRA_VERBOSE
    for( i = 0; i < i_number_of_entries; i++ )
    {
        if( p_tfra->i_version == 0 )
        {
            msg_Dbg( p_stream, "tfra[%"PRIu32"] time[%"PRIu32"]: %"PRIu32", "
                               "moof_offset[%"PRIu32"]: %"PRIu32"",
                     p_tfra->i_track_ID,
                     i, p_tfra->p_time[i],
                     i, p_tfra->p_moof_offset[i] );
        }
        else
        {
            msg_Dbg( p_stream, "tfra[%"PRIu32"] time[%"PRIu32"]: %"PRIu64", "
                               "moof_offset[%"PRIu32"]: %"PRIu64"",
                     p_tfra->i_track_ID,
                     i, ((uint64_t *)(p_tfra->p_time))[i],
                     i, ((uint64_t *)(p_tfra->p_moof_offset))[i] );
        }
    }
#endif
#ifdef MP4_VERBOSE
    msg_Dbg( p_stream, "tfra[%"PRIu32"] %"PRIu32" entries",
             p_tfra->i_track_ID, i_number_of_entries );
#endif

    MP4_READBOX_EXIT( 1 );
error:
    MP4_READBOX_EXIT( 0 );

#undef READ_VARIABLE_LENGTH
#undef FIX_VARIABLE_LENGTH
}

static int MP4_ReadBox_pnot( stream_t *p_stream, MP4_Box_t *p_box )
{
    if ( p_box->i_size != 20 )
        return 0;
    MP4_READBOX_ENTER( MP4_Box_data_pnot_t, NULL );
    MP4_GET4BYTES( p_box->data.p_pnot->i_date );
    uint16_t i_version;
    MP4_GET2BYTES( i_version );
    if ( i_version != 0 )
        MP4_READBOX_EXIT( 0 );
    MP4_GETFOURCC( p_box->data.p_pnot->i_type );
    MP4_GET2BYTES( p_box->data.p_pnot->i_index );
    MP4_READBOX_EXIT( 1 );
}

static int MP4_ReadBox_SA3D( stream_t *p_stream, MP4_Box_t *p_box )
{
    MP4_READBOX_ENTER( MP4_Box_data_SA3D_t, NULL );

    uint8_t i_version;
    MP4_GET1BYTE( i_version );
    if ( i_version != 0 )
        MP4_READBOX_EXIT( 0 );

    MP4_GET1BYTE( p_box->data.p_SA3D->i_ambisonic_type );
    MP4_GET4BYTES( p_box->data.p_SA3D->i_ambisonic_order );
    MP4_GET1BYTE( p_box->data.p_SA3D->i_ambisonic_channel_ordering );
    MP4_GET1BYTE( p_box->data.p_SA3D->i_ambisonic_normalization );
    MP4_GET4BYTES( p_box->data.p_SA3D->i_num_channels );
    MP4_READBOX_EXIT( 1 );
}

/* For generic */
static int MP4_ReadBox_default( stream_t *p_stream, MP4_Box_t *p_box )
{
    if( !p_box->p_father )
    {
        goto unknown;
    }
    if( p_box->p_father->i_type == ATOM_stsd )
    {
        MP4_Box_t *p_mdia = MP4_BoxGet( p_box, "../../../.." );
        MP4_Box_t *p_hdlr;

        if( p_mdia == NULL || p_mdia->i_type != ATOM_mdia ||
            (p_hdlr = MP4_BoxGet( p_mdia, "hdlr" )) == NULL )
        {
            goto unknown;
        }
        switch( p_hdlr->data.p_hdlr->i_handler_type )
        {
            case ATOM_soun:
                return MP4_ReadBox_sample_soun( p_stream, p_box );
            case ATOM_vide:
                return MP4_ReadBox_sample_vide( p_stream, p_box );
            case ATOM_hint:
                return MP4_ReadBox_sample_hint8( p_stream, p_box );
            case ATOM_text:
                return MP4_ReadBox_sample_text( p_stream, p_box );
            case ATOM_tx3g:
            case ATOM_sbtl:
                return MP4_ReadBox_sample_tx3g( p_stream, p_box );
            default:
                msg_Warn( p_stream,
                          "unknown handler type in stsd (incompletely loaded)" );
                return 1;
        }
    }

unknown:
    if MP4_BOX_TYPE_ASCII()
        msg_Warn( p_stream,
                "unknown box type %4.4s (incompletely loaded)",
                (char*)&p_box->i_type );
    else
        msg_Warn( p_stream,
                "unknown box type c%3.3s (incompletely loaded)",
                (char*)&p_box->i_type+1 );
    p_box->e_flags |= BOX_FLAG_INCOMPLETE;

    return 1;
}

/**** ------------------------------------------------------------------- ****/

static int MP4_ReadBox_uuid( stream_t *p_stream, MP4_Box_t *p_box )
{
    if( !CmpUUID( &p_box->i_uuid, &TfrfBoxUUID ) )
        return MP4_ReadBox_tfrf( p_stream, p_box );
    if( !CmpUUID( &p_box->i_uuid, &TfxdBoxUUID ) )
        return MP4_ReadBox_tfxd( p_stream, p_box );
    if( !CmpUUID( &p_box->i_uuid, &XML360BoxUUID ) )
        return MP4_ReadBox_XML360( p_stream, p_box );
    if( !CmpUUID( &p_box->i_uuid, &PS3DDSBoxUUID ) && p_box->i_size == 28 )
        return MP4_ReadBox_Binary( p_stream, p_box );

#ifdef MP4_VERBOSE
    msg_Warn( p_stream, "Unknown uuid type box: "
    "%2.2x%2.2x%2.2x%2.2x-%2.2x%2.2x-%2.2x%2.2x-"
    "%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
    p_box->i_uuid.b[0],  p_box->i_uuid.b[1],  p_box->i_uuid.b[2],  p_box->i_uuid.b[3],
    p_box->i_uuid.b[4],  p_box->i_uuid.b[5],  p_box->i_uuid.b[6],  p_box->i_uuid.b[7],
    p_box->i_uuid.b[8],  p_box->i_uuid.b[9],  p_box->i_uuid.b[10], p_box->i_uuid.b[11],
    p_box->i_uuid.b[12], p_box->i_uuid.b[13], p_box->i_uuid.b[14], p_box->i_uuid.b[15] );
#else
    msg_Warn( p_stream, "Unknown uuid type box" );
#endif
    return 1;
}

/**** ------------------------------------------------------------------- ****/
/****                   "Higher level" Functions                          ****/
/**** ------------------------------------------------------------------- ****/

static const struct
{
    uint32_t i_type;
    int  (*MP4_ReadBox_function )( stream_t *p_stream, MP4_Box_t *p_box );
    uint32_t i_parent; /* set parent to restrict, duplicating if needed; 0 for any */
} MP4_Box_Function [] =
{
    /* Containers */
    { ATOM_moov,    MP4_ReadBoxContainer,     0 },
    { ATOM_foov,    MP4_ReadBoxContainer,     0 },
    { ATOM_trak,    MP4_ReadBoxContainer,     ATOM_moov },
    { ATOM_trak,    MP4_ReadBoxContainer,     ATOM_foov },
    { ATOM_mdia,    MP4_ReadBoxContainer,     ATOM_trak },
    { ATOM_moof,    MP4_ReadBoxContainer,     0 },
    { ATOM_minf,    MP4_ReadBoxContainer,     ATOM_mdia },
    { ATOM_stbl,    MP4_ReadBoxContainer,     ATOM_minf },
    { ATOM_dinf,    MP4_ReadBoxContainer,     ATOM_minf },
    { ATOM_dinf,    MP4_ReadBoxContainer,     ATOM_meta },
    { ATOM_edts,    MP4_ReadBoxContainer,     ATOM_trak },
    { ATOM_udta,    MP4_ReadBoxContainer,     0 },
    { ATOM_nmhd,    MP4_ReadBoxContainer,     ATOM_minf },
    { ATOM_hnti,    MP4_ReadBoxContainer,     ATOM_udta },
    { ATOM_rmra,    MP4_ReadBoxContainer,     ATOM_moov },
    { ATOM_rmda,    MP4_ReadBoxContainer,     ATOM_rmra },
    { ATOM_tref,    MP4_ReadBoxContainer,     ATOM_trak },
    { ATOM_gmhd,    MP4_ReadBoxContainer,     ATOM_minf },
    { ATOM_wave,    MP4_ReadBoxContainer,     ATOM_stsd },
    { ATOM_wave,    MP4_ReadBoxContainer,     ATOM_mp4a }, /* some quicktime mp4a/wave/mp4a.. */
    { ATOM_wave,    MP4_ReadBoxContainer,     ATOM_WMA2 }, /* flip4mac */
    { ATOM_wave,    MP4_ReadBoxContainer,     ATOM_in24 },
    { ATOM_wave,    MP4_ReadBoxContainer,     ATOM_in32 },
    { ATOM_wave,    MP4_ReadBoxContainer,     ATOM_fl32 },
    { ATOM_wave,    MP4_ReadBoxContainer,     ATOM_fl64 },
    { ATOM_wave,    MP4_ReadBoxContainer,     ATOM_QDMC },
    { ATOM_wave,    MP4_ReadBoxContainer,     ATOM_QDM2 },
    { ATOM_wave,    MP4_ReadBoxContainer,     ATOM_XiFL }, /* XiphQT */
    { ATOM_wave,    MP4_ReadBoxContainer,     ATOM_XiVs }, /* XiphQT */
    { ATOM_ilst,    MP4_ReadBox_ilst,         ATOM_meta },
    { ATOM_mvex,    MP4_ReadBoxContainer,     ATOM_moov },
    { ATOM_mvex,    MP4_ReadBoxContainer,     ATOM_ftyp },

    /* specific box */
    { ATOM_ftyp,    MP4_ReadBox_ftyp,         0 },
    { ATOM_styp,    MP4_ReadBox_ftyp,         0 },
    { ATOM_cmov,    MP4_ReadBox_cmov,         0 },
    { ATOM_mvhd,    MP4_ReadBox_mvhd,         ATOM_moov },
    { ATOM_mvhd,    MP4_ReadBox_mvhd,         ATOM_foov },
    { ATOM_tkhd,    MP4_ReadBox_tkhd,         ATOM_trak },
    { ATOM_load,    MP4_ReadBox_load,         ATOM_trak },
    { ATOM_mdhd,    MP4_ReadBox_mdhd,         ATOM_mdia },
    { ATOM_hdlr,    MP4_ReadBox_hdlr,         ATOM_mdia },
    { ATOM_hdlr,    MP4_ReadBox_hdlr,         ATOM_meta },
    { ATOM_hdlr,    MP4_ReadBox_hdlr,         ATOM_minf },
    { ATOM_vmhd,    MP4_ReadBox_vmhd,         ATOM_minf },
    { ATOM_smhd,    MP4_ReadBox_smhd,         ATOM_minf },
    { ATOM_hmhd,    MP4_ReadBox_hmhd,         ATOM_minf },
    { ATOM_alis,    MP4_ReadBoxSkip,          ATOM_dref },
    { ATOM_url,     MP4_ReadBox_url,          0 },
    { ATOM_urn,     MP4_ReadBox_urn,          0 },
    { ATOM_dref,    MP4_ReadBox_LtdContainer, 0 },
    { ATOM_stts,    MP4_ReadBox_stts,         ATOM_stbl },
    { ATOM_ctts,    MP4_ReadBox_ctts,         ATOM_stbl },
    { ATOM_cslg,    MP4_ReadBox_cslg,         ATOM_stbl },
    { ATOM_stsd,    MP4_ReadBox_LtdContainer, ATOM_stbl },
    { ATOM_stsz,    MP4_ReadBox_stsz,         ATOM_stbl },
    { ATOM_stsc,    MP4_ReadBox_stsc,         ATOM_stbl },
    { ATOM_stco,    MP4_ReadBox_stco_co64,    ATOM_stbl },
    { ATOM_co64,    MP4_ReadBox_stco_co64,    ATOM_stbl },
    { ATOM_stss,    MP4_ReadBox_stss,         ATOM_stbl },
    { ATOM_stsh,    MP4_ReadBox_stsh,         ATOM_stbl },
    { ATOM_stdp,    MP4_ReadBox_stdp,         0 },
    { ATOM_elst,    MP4_ReadBox_elst,         ATOM_edts },
    { ATOM_cprt,    MP4_ReadBox_cprt,         0 },
    { ATOM_esds,    MP4_ReadBox_esds,         ATOM_wave }, /* mp4a in wave chunk */
    { ATOM_esds,    MP4_ReadBox_esds,         ATOM_mp4a },
    { ATOM_esds,    MP4_ReadBox_esds,         ATOM_mp4v },
    { ATOM_esds,    MP4_ReadBox_esds,         ATOM_mp4s },
    { ATOM_dcom,    MP4_ReadBox_dcom,         0 },
    { ATOM_dfLa,    MP4_ReadBox_Binary,       ATOM_fLaC },
    { ATOM_cmvd,    MP4_ReadBox_cmvd,         0 },
    { ATOM_avcC,    MP4_ReadBox_avcC,         ATOM_avc1 },
    { ATOM_avcC,    MP4_ReadBox_avcC,         ATOM_avc3 },
    { ATOM_hvcC,    MP4_ReadBox_Binary,       0 },
    { ATOM_vpcC,    MP4_ReadBox_vpcC,         ATOM_vp08 },
    { ATOM_vpcC,    MP4_ReadBox_vpcC,         ATOM_vp09 },
    { ATOM_vpcC,    MP4_ReadBox_vpcC,         ATOM_vp10 },
    { ATOM_dac3,    MP4_ReadBox_dac3,         0 },
    { ATOM_dec3,    MP4_ReadBox_dec3,         0 },
    { ATOM_dvc1,    MP4_ReadBox_dvc1,         ATOM_vc1  },
    { ATOM_fiel,    MP4_ReadBox_fiel,         0 },
    { ATOM_glbl,    MP4_ReadBox_Binary,       ATOM_FFV1 },
    { ATOM_enda,    MP4_ReadBox_enda,         0 },
    { ATOM_iods,    MP4_ReadBox_iods,         0 },
    { ATOM_pasp,    MP4_ReadBox_pasp,         0 },
    { ATOM_btrt,    MP4_ReadBox_btrt,         0 }, /* codecs bitrate stsd/????/btrt */
    { ATOM_keys,    MP4_ReadBox_keys,         ATOM_meta },
    { ATOM_colr,    MP4_ReadBox_colr,         0 },

    /* XiphQT */
    { ATOM_vCtH,    MP4_ReadBox_Binary,       ATOM_wave },
    { ATOM_vCtC,    MP4_ReadBox_Binary,       ATOM_wave },
    { ATOM_vCtd,    MP4_ReadBox_Binary,       ATOM_wave },
    { ATOM_fCtS,    MP4_ReadBox_Binary,       ATOM_wave },

    /* Samples groups specific information */
    { ATOM_sbgp,    MP4_ReadBox_sbgp,         ATOM_stbl },
    { ATOM_sbgp,    MP4_ReadBox_sbgp,         ATOM_traf },
    { ATOM_sgpd,    MP4_ReadBox_sgpd,         ATOM_stbl },
    { ATOM_sgpd,    MP4_ReadBox_sgpd,         ATOM_traf },

    /* Quicktime preview atoms, all at root */
    { ATOM_pnot,    MP4_ReadBox_pnot,         0 },
    { ATOM_pict,    MP4_ReadBox_Binary,       0 },
    { ATOM_PICT,    MP4_ReadBox_Binary,       0 },

    /* Nothing to do with this box */
    { ATOM_mdat,    MP4_ReadBoxSkip,          0 },
    { ATOM_skip,    MP4_ReadBoxSkip,          0 },
    { ATOM_free,    MP4_ReadBoxSkip,          0 },
    { ATOM_wide,    MP4_ReadBoxSkip,          0 },
    { ATOM_binm,    MP4_ReadBoxSkip,          0 },

    /* Subtitles */
    { ATOM_tx3g,    MP4_ReadBox_sample_tx3g,      0 },
    { ATOM_c608,    MP4_ReadBox_sample_clcp,      ATOM_stsd },
    //{ ATOM_text,    MP4_ReadBox_sample_text,    0 },
    /* In sample WebVTT subtitle atoms. No ATOM_wvtt in normal parsing */
    { ATOM_vttc,    MP4_ReadBoxContainer,         ATOM_wvtt },
    { ATOM_payl,    MP4_ReadBox_Binary,           ATOM_vttc },

    /* for codecs */
    { ATOM_soun,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_agsm,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_ac3,     MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_eac3,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_fLaC,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_lpcm,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_ms02,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_ms11,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_ms55,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM__mp3,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_mp4a,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_twos,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_sowt,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_QDMC,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_QDM2,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_ima4,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_IMA4,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_dvi,     MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_alaw,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_ulaw,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_MAC3,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_MAC6,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_Qclp,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_samr,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_sawb,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_OggS,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_alac,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    { ATOM_WMA2,    MP4_ReadBox_sample_soun,  ATOM_stsd }, /* flip4mac */
    { ATOM_wma,     MP4_ReadBox_sample_soun,  ATOM_stsd }, /* ismv wmapro */
    { ATOM_Opus,    MP4_ReadBox_sample_soun,  ATOM_stsd },
    /* Sound extensions */
    { ATOM_chan,    MP4_ReadBox_stsdext_chan, 0 },
    { ATOM_WMA2,    MP4_ReadBox_WMA2,         ATOM_wave }, /* flip4mac */
    { ATOM_dOps,    MP4_ReadBox_Binary,       ATOM_Opus },
    { ATOM_wfex,    MP4_ReadBox_WMA2,         ATOM_wma  }, /* ismv formatex */

    /* Both uncompressed sound and video */
    { ATOM_raw,     MP4_ReadBox_default,      ATOM_stsd },

    { ATOM_drmi,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_vide,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_mp4v,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_SVQ1,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_SVQ3,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_ZyGo,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_DIVX,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_XVID,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_h263,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_s263,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_cvid,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_3IV1,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_3iv1,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_3IV2,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_3iv2,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_3IVD,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_3ivd,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_3VID,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_3vid,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_FFV1,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_mjpa,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_mjpb,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_qdrw,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_mp2v,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_hdv2,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_WMV3,    MP4_ReadBox_sample_vide,  ATOM_stsd },

    { ATOM_mjqt,    MP4_ReadBox_default,      0 }, /* found in mjpa/b */
    { ATOM_mjht,    MP4_ReadBox_default,      0 },

    { ATOM_dvc,     MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_dvp,     MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_dv5n,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_dv5p,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_VP31,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_vp31,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_h264,    MP4_ReadBox_sample_vide,  ATOM_stsd },

    { ATOM_jpeg,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_vc1,     MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_avc1,    MP4_ReadBox_sample_vide,  ATOM_stsd },
    { ATOM_avc3,    MP4_ReadBox_sample_vide,  ATOM_stsd },

    { ATOM_rrtp,    MP4_ReadBox_sample_hint8,  ATOM_stsd },

    { ATOM_yv12,    MP4_ReadBox_sample_vide,  0 },
    { ATOM_yuv2,    MP4_ReadBox_sample_vide,  0 },

    { ATOM_strf,    MP4_ReadBox_strf,         ATOM_WVC1 }, /* MS smooth */
    { ATOM_strf,    MP4_ReadBox_strf,         ATOM_H264 }, /* MS smooth */

    { ATOM_strf,    MP4_ReadBox_strf,         ATOM_WMV3 }, /* flip4mac */
    { ATOM_ASF ,    MP4_ReadBox_ASF,          ATOM_WMV3 }, /* flip4mac */
    { ATOM_ASF ,    MP4_ReadBox_ASF,          ATOM_wave }, /* flip4mac */

    { ATOM_mp4s,    MP4_ReadBox_sample_mp4s,  ATOM_stsd },

    /* XXX there is 2 box where we could find this entry stbl and tref*/
    { ATOM_hint,    MP4_ReadBox_default,      0 },

    /* found in tref box */
    { ATOM_dpnd,    MP4_ReadBox_default,      0 },
    { ATOM_ipir,    MP4_ReadBox_default,      0 },
    { ATOM_mpod,    MP4_ReadBox_default,      0 },
    { ATOM_chap,    MP4_ReadBox_tref_generic, 0 },

    /* found in hnti */
    { ATOM_rtp,     MP4_ReadBox_rtp,          ATOM_hnti },
    { ATOM_sdp,     MP4_ReadBox_sdp,          ATOM_hnti },

    /* found in rrtp sample description */
    { ATOM_tims,     MP4_ReadBox_tims,        0 },
    { ATOM_tsro,     MP4_ReadBox_tsro,        0 },
    { ATOM_tssy,     MP4_ReadBox_tssy,        0 },

    /* found in rmra/rmda */
    { ATOM_rdrf,    MP4_ReadBox_rdrf,         ATOM_rmda },
    { ATOM_rmdr,    MP4_ReadBox_rmdr,         ATOM_rmda },
    { ATOM_rmqu,    MP4_ReadBox_rmqu,         ATOM_rmda },
    { ATOM_rmvc,    MP4_ReadBox_rmvc,         ATOM_rmda },

    { ATOM_drms,    MP4_ReadBox_sample_soun,  0 },
    { ATOM_sinf,    MP4_ReadBoxContainer,     0 },
    { ATOM_schi,    MP4_ReadBoxContainer,     0 },
    { ATOM_user,    MP4_ReadBox_drms,         0 },
    { ATOM_key,     MP4_ReadBox_drms,         0 },
    { ATOM_iviv,    MP4_ReadBox_drms,         0 },
    { ATOM_priv,    MP4_ReadBox_drms,         0 },
    { ATOM_frma,    MP4_ReadBox_frma,         ATOM_sinf }, /* and rinf */
    { ATOM_frma,    MP4_ReadBox_frma,         ATOM_wave }, /* flip4mac */
    { ATOM_skcr,    MP4_ReadBox_skcr,         0 },

    /* ilst meta tags */
    { ATOM_0xa9ART, MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_0xa9alb, MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_0xa9cmt, MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_0xa9com, MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_0xa9cpy, MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_0xa9day, MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_0xa9des, MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_0xa9enc, MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_0xa9gen, MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_0xa9grp, MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_0xa9lyr, MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_0xa9nam, MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_0xa9too, MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_0xa9trk, MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_0xa9wrt, MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_aART,    MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_atID,    MP4_ReadBox_Metadata,    ATOM_ilst }, /* iTunes */
    { ATOM_cnID,    MP4_ReadBox_Metadata,    ATOM_ilst }, /* iTunes */
    { ATOM_covr,    MP4_ReadBoxContainer,    ATOM_ilst },
    { ATOM_desc,    MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_disk,    MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_flvr,    MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_gnre,    MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_rtng,    MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_trkn,    MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_xid_,    MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_gshh,    MP4_ReadBox_Metadata,    ATOM_ilst }, /* YouTube gs?? */
    { ATOM_gspm,    MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_gspu,    MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_gssd,    MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_gsst,    MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_gstd,    MP4_ReadBox_Metadata,    ATOM_ilst },
    { ATOM_ITUN,    MP4_ReadBox_Metadata,    ATOM_ilst }, /* iTunesInfo */

    /* udta */
    { ATOM_0x40PRM, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0x40PRQ, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9ART, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9alb, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9ard, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9arg, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9aut, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9cak, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9cmt, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9con, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9com, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9cpy, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9day, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9des, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9dir, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9dis, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9dsa, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9fmt, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9gen, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9grp, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9hst, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9inf, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9isr, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9lab, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9lal, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9lnt, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9lyr, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9mak, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9mal, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9mod, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9nam, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9ope, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9phg, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9PRD, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9prd, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9prf, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9pub, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9req, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9sne, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9snm, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9sol, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9src, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9st3, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9swr, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9thx, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9too, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9trk, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9url, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9wrn, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9xpd, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_0xa9xyz, MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_chpl,    MP4_ReadBox_chpl,      ATOM_udta }, /* nero unlabeled chapters list */
    { ATOM_MCPS,    MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_name,    MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_vndr,    MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_SDLN,    MP4_ReadBox_Binary,    ATOM_udta },
    { ATOM_HMMT,    MP4_ReadBox_HMMT,      ATOM_udta }, /* GoPro HiLight tags */

    /* udta, non meta */
    { ATOM_tsel,    MP4_ReadBox_tsel,    ATOM_udta },

    /* iTunes/Quicktime meta info */
    { ATOM_meta,    MP4_ReadBox_meta,    0 },
    { ATOM_ID32,    MP4_ReadBox_Binary,  ATOM_meta }, /* ID3v2 in 3GPP / ETSI TS 126 244 8.3 */
    { ATOM_data,    MP4_ReadBox_data,    0 }, /* ilst/@too and others, ITUN/data */
    { ATOM_mean,    MP4_ReadBox_Binary,  ATOM_ITUN },
    { ATOM_name,    MP4_ReadBox_Binary,  ATOM_ITUN },

    /* found in smoothstreaming */
    { ATOM_traf,    MP4_ReadBoxContainer,    ATOM_moof },
    { ATOM_mfra,    MP4_ReadBoxContainer,    0 },
    { ATOM_mfhd,    MP4_ReadBox_mfhd,        ATOM_moof },
    { ATOM_sidx,    MP4_ReadBox_sidx,        0 },
    { ATOM_tfhd,    MP4_ReadBox_tfhd,        ATOM_traf },
    { ATOM_trun,    MP4_ReadBox_trun,        ATOM_traf },
    { ATOM_tfdt,    MP4_ReadBox_tfdt,        ATOM_traf },
    { ATOM_trex,    MP4_ReadBox_trex,        ATOM_mvex },
    { ATOM_mehd,    MP4_ReadBox_mehd,        ATOM_mvex },
    { ATOM_sdtp,    MP4_ReadBox_sdtp,        0 },
    { ATOM_tfra,    MP4_ReadBox_tfra,        ATOM_mfra },
    { ATOM_mfro,    MP4_ReadBox_mfro,        ATOM_mfra },
    { ATOM_uuid,    MP4_ReadBox_uuid,        0 },

    /* spatial/360°/VR */
    { ATOM_st3d,    MP4_ReadBox_st3d,        ATOM_avc1 },
    { ATOM_st3d,    MP4_ReadBox_st3d,        ATOM_mp4v },
    { ATOM_sv3d,    MP4_ReadBoxContainer,    ATOM_avc1 },
    { ATOM_sv3d,    MP4_ReadBoxContainer,    ATOM_mp4v },
    { ATOM_proj,    MP4_ReadBoxContainer,    ATOM_sv3d },
    { ATOM_prhd,    MP4_ReadBox_prhd,        ATOM_proj },
    { ATOM_equi,    MP4_ReadBox_equi,        ATOM_proj },
    { ATOM_cbmp,    MP4_ReadBox_cbmp,        ATOM_proj },

    /* Ambisonics */
    { ATOM_SA3D,    MP4_ReadBox_SA3D,        0 },

    /* Last entry */
    { 0,              MP4_ReadBox_default,   0 }
};

static int MP4_Box_Read_Specific( stream_t *p_stream, MP4_Box_t *p_box, MP4_Box_t *p_father )
{
    int i_index;

    for( i_index = 0; ; i_index++ )
    {
        if ( MP4_Box_Function[i_index].i_parent &&
             p_father && p_father->i_type != MP4_Box_Function[i_index].i_parent )
            continue;

        if( ( MP4_Box_Function[i_index].i_type == p_box->i_type )||
            ( MP4_Box_Function[i_index].i_type == 0 ) )
        {
            break;
        }
    }

    if( !(MP4_Box_Function[i_index].MP4_ReadBox_function)( p_stream, p_box ) )
    {
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void MP4_Box_Clean_Specific( MP4_Box_t *p_box )
{
    if( p_box->pf_free )
        p_box->pf_free( p_box );
}

/*****************************************************************************
 * MP4_ReadBox : parse the actual box and the children
 *  XXX : Do not go to the next box
 *****************************************************************************/
static MP4_Box_t *MP4_ReadBox( stream_t *p_stream, MP4_Box_t *p_father )
{
    MP4_Box_t *p_box = calloc( 1, sizeof( MP4_Box_t ) ); /* Needed to ensure simple on error handler */
    if( p_box == NULL )
        return NULL;

    if( !MP4_PeekBoxHeader( p_stream, p_box ) )
    {
        msg_Warn( p_stream, "cannot read one box" );
        free( p_box );
        return NULL;
    }

    if( p_father && p_father->i_size > 0 &&
        p_father->i_pos + p_father->i_size < p_box->i_pos + p_box->i_size )
    {
        msg_Dbg( p_stream, "out of bound child" );
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

    if( MP4_Box_Read_Specific( p_stream, p_box, p_father ) != VLC_SUCCESS )
    {
        uint64_t i_end = p_box->i_pos + p_box->i_size;
        MP4_BoxFree( p_box );
        MP4_Seek( p_stream, i_end ); /* Skip the failed box */
        return NULL;
    }

    return p_box;
}

/*****************************************************************************
 * MP4_BoxNew : creates and initializes an arbitrary box
 *****************************************************************************/
MP4_Box_t * MP4_BoxNew( uint32_t i_type )
{
    MP4_Box_t *p_box = calloc( 1, sizeof( MP4_Box_t ) );
    if( likely( p_box != NULL ) )
    {
        p_box->i_type = i_type;
    }
    return p_box;
}

/*****************************************************************************
 * MP4_FreeBox : free memory after read with MP4_ReadBox and all
 * the children
 *****************************************************************************/
void MP4_BoxFree( MP4_Box_t *p_box )
{
    MP4_Box_t    *p_child;

    if( !p_box )
        return; /* hehe */

    for( p_child = p_box->p_first; p_child != NULL; )
    {
        MP4_Box_t *p_next;

        p_next = p_child->p_next;
        MP4_BoxFree( p_child );
        p_child = p_next;
    }

    MP4_Box_Clean_Specific( p_box );

    if( p_box->data.p_payload )
        free( p_box->data.p_payload );

    free( p_box );
}

MP4_Box_t *MP4_BoxGetNextChunk( stream_t *s )
{
    /* p_chunk is a virtual root container for the moof and mdat boxes */
    MP4_Box_t *p_fakeroot;
    MP4_Box_t *p_tmp_box;

    p_fakeroot = MP4_BoxNew( ATOM_root );
    if( unlikely( p_fakeroot == NULL ) )
        return NULL;
    p_fakeroot->i_shortsize = 1;

    const uint32_t stoplist[] = { ATOM_moov, ATOM_moof, 0 };
    MP4_ReadBoxContainerChildren( s, p_fakeroot, stoplist );

    p_tmp_box = p_fakeroot->p_first;
    if( p_tmp_box == NULL )
    {
        MP4_BoxFree( p_fakeroot );
        return NULL;
    }
    else while( p_tmp_box )
    {
        p_fakeroot->i_size += p_tmp_box->i_size;
        p_tmp_box = p_tmp_box->p_next;
    }

    return p_fakeroot;
}

/*****************************************************************************
 * MP4_BoxGetRoot : Parse the entire file, and create all boxes in memory
 *****************************************************************************
 *  The first box is a virtual box "root" and is the father for all first
 *  level boxes for the file, a sort of virtual contener
 *****************************************************************************/
MP4_Box_t *MP4_BoxGetRoot( stream_t *p_stream )
{
    int i_result;

    MP4_Box_t *p_vroot = MP4_BoxNew( ATOM_root );
    if( p_vroot == NULL )
        return NULL;

    p_vroot->i_shortsize = 1;
    uint64_t i_size;
    if( vlc_stream_GetSize( p_stream, &i_size ) == 0 )
        p_vroot->i_size = i_size;

    /* First get the moov */
    {
        const uint32_t stoplist[] = { ATOM_moov, ATOM_mdat, 0 };
        i_result = MP4_ReadBoxContainerChildren( p_stream, p_vroot, stoplist );
    }

    /* mdat appeared first */
    if( i_result && !MP4_BoxGet( p_vroot, "moov" ) )
    {
        bool b_seekable;
        if( vlc_stream_Control( p_stream, STREAM_CAN_SEEK, &b_seekable ) != VLC_SUCCESS || !b_seekable )
        {
            msg_Err( p_stream, "no moov before mdat and the stream is not seekable" );
            goto error;
        }

        /* continue loading up to moov */
        const uint32_t stoplist[] = { ATOM_moov, 0 };
        i_result = MP4_ReadBoxContainerChildren( p_stream, p_vroot, stoplist );
    }

    if( !i_result )
        goto error;

    /* If there is a mvex box, it means fragmented MP4, and we're done */
    if( MP4_BoxCount( p_vroot, "moov/mvex" ) > 0 )
    {
        /* Read a bit more atoms as we might have an index between moov and moof */
        const uint32_t stoplist[] = { ATOM_sidx, 0 };
        const uint32_t excludelist[] = { ATOM_moof, ATOM_mdat, 0 };
        MP4_ReadBoxContainerChildrenIndexed( p_stream, p_vroot, stoplist, excludelist, false );
        return p_vroot;
    }

    if( vlc_stream_Tell( p_stream ) + 8 < (uint64_t) stream_Size( p_stream ) )
    {
        /* Get the rest of the file */
        i_result = MP4_ReadBoxContainerChildren( p_stream, p_vroot, NULL );

        if( !i_result )
            goto error;
    }

    MP4_Box_t *p_moov;
    MP4_Box_t *p_cmov;

    /* check if there is a cmov, if so replace
      compressed moov by  uncompressed one */
    if( ( ( p_moov = MP4_BoxGet( p_vroot, "moov" ) ) &&
          ( p_cmov = MP4_BoxGet( p_vroot, "moov/cmov" ) ) ) ||
        ( ( p_moov = MP4_BoxGet( p_vroot, "foov" ) ) &&
          ( p_cmov = MP4_BoxGet( p_vroot, "foov/cmov" ) ) ) )
    {
        /* rename the compressed moov as a box to skip */
        p_moov->i_type = ATOM_skip;

        /* get uncompressed p_moov */
        p_moov = p_cmov->data.p_cmov->p_moov;
        p_cmov->data.p_cmov->p_moov = NULL;

        /* make p_root father of this new moov */
        p_moov->p_father = p_vroot;

        /* insert this new moov box as first child of p_root */
        p_moov->p_next = p_vroot->p_first;
        p_vroot->p_first = p_moov;
    }

    return p_vroot;

error:
    MP4_BoxFree( p_vroot );
    MP4_Seek( p_stream, 0 );
    return NULL;
}


static void MP4_BoxDumpStructure_Internal( stream_t *s, const MP4_Box_t *p_box,
                                           unsigned int i_level )
{
    const MP4_Box_t *p_child;
    uint32_t i_displayedtype = p_box->i_type;
    if( ! MP4_BOX_TYPE_ASCII() ) ((char*)&i_displayedtype)[0] = 'c';

    if( !i_level )
    {
        msg_Dbg( s, "dumping root Box \"%4.4s\"",
                          (char*)&i_displayedtype );
    }
    else
    {
        char str[512];
        if( i_level >= (sizeof(str) - 1)/4 )
            return;

        memset( str, ' ', sizeof(str) );
        for( unsigned i = 0; i < i_level; i++ )
        {
            str[i*4] = '|';
        }

        snprintf( &str[i_level * 4], sizeof(str) - 4*i_level,
                  "+ %4.4s size %"PRIu64" offset %" PRIuMAX "%s",
                    (char*)&i_displayedtype, p_box->i_size,
                  (uintmax_t)p_box->i_pos,
                p_box->e_flags & BOX_FLAG_INCOMPLETE ? " (\?\?\?\?)" : "" );
        msg_Dbg( s, "%s", str );
    }
    p_child = p_box->p_first;
    while( p_child )
    {
        MP4_BoxDumpStructure_Internal( s, p_child, i_level + 1 );
        p_child = p_child->p_next;
    }
}

void MP4_BoxDumpStructure( stream_t *s, const MP4_Box_t *p_box )
{
    MP4_BoxDumpStructure_Internal( s, p_box, 0 );
}


/*****************************************************************************
 *****************************************************************************
 **
 **  High level methods to acces an MP4 file
 **
 *****************************************************************************
 *****************************************************************************/
static bool get_token( char **ppsz_path, char **ppsz_token, int *pi_number )
{
    size_t i_len ;
    if( !*ppsz_path[0] )
    {
        *ppsz_token = NULL;
        *pi_number = 0;
        return true;
    }
    i_len = strcspn( *ppsz_path, "/[" );
    if( !i_len && **ppsz_path == '/' )
    {
        i_len = 1;
    }
    *ppsz_token = strndup( *ppsz_path, i_len );
    if( unlikely(!*ppsz_token) )
        return false;

    *ppsz_path += i_len;

    /* Parse the token number token[n] */
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

    /* Forward to start of next token */
    while( **ppsz_path == '/' )
    {
        (*ppsz_path)++;
    }

    return true;
}

static void MP4_BoxGet_Internal( const MP4_Box_t **pp_result, const MP4_Box_t *p_box,
                                 const char *psz_fmt, va_list args)
{
    char *psz_dup;
    char *psz_path;
    char *psz_token = NULL;

    if( !p_box )
    {
        *pp_result = NULL;
        return;
    }

    if( vasprintf( &psz_path, psz_fmt, args ) == -1 )
        psz_path = NULL;

    if( !psz_path || !psz_path[0] )
    {
        free( psz_path );
        *pp_result = NULL;
        return;
    }

//    fprintf( stderr, "path:'%s'\n", psz_path );
    psz_dup = psz_path; /* keep this pointer, as it need to be unallocated */
    for( ; ; )
    {
        int i_number;

        if( !get_token( &psz_path, &psz_token, &i_number ) )
            goto error_box;
//        fprintf( stderr, "path:'%s', token:'%s' n:%d\n",
//                 psz_path,psz_token,i_number );
        if( !psz_token )
        {
            free( psz_dup );
            *pp_result = p_box;
            return;
        }
        else
        if( !strcmp( psz_token, "/" ) )
        {
            /* Find root box */
            while( p_box && p_box->i_type != ATOM_root )
            {
                p_box = p_box->p_father;
            }
            if( !p_box )
            {
                goto error_box;
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
                goto error_box;
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
                    goto error_box;
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
        if( *psz_token == '\0' )
        {
            p_box = p_box->p_first;
            for( ; ; )
            {
                if( !p_box )
                {
                    goto error_box;
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
            goto error_box;
        }

        FREENULL( psz_token );
    }

    return;

error_box:
    free( psz_token );
    free( psz_dup );
    *pp_result = NULL;
    return;
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
MP4_Box_t *MP4_BoxGet( const MP4_Box_t *p_box, const char *psz_fmt, ... )
{
    va_list args;
    const MP4_Box_t *p_result;

    va_start( args, psz_fmt );
    MP4_BoxGet_Internal( &p_result, p_box, psz_fmt, args );
    va_end( args );

    return( (MP4_Box_t *) p_result );
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
unsigned MP4_BoxCount( const MP4_Box_t *p_box, const char *psz_fmt, ... )
{
    va_list args;
    unsigned i_count;
    const MP4_Box_t *p_result, *p_next;

    va_start( args, psz_fmt );
    MP4_BoxGet_Internal( &p_result, p_box, psz_fmt, args );
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
