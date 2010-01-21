/*****************************************************************************
 * rar.c: uncompressed RAR stream filter (only the biggest file is extracted)
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Author: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>

#include <assert.h>
#include <limits.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_STREAM_FILTER )
    set_description( N_("Uncompressed RAR") )
    set_capability( "stream_filter", 1 )
    set_callbacks( Open, Close )
vlc_module_end()

/*****************************************************************************
 *
 *****************************************************************************/
static const uint8_t p_rar_marker[] = {
    0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00
};
static const int i_rar_marker = sizeof(p_rar_marker);

typedef struct
{
    uint64_t i_offset;
    uint64_t i_size;
    uint64_t i_cummulated_size;
} rar_file_chunk_t;
typedef struct
{
    char     *psz_name;
    uint64_t i_size;
    bool     b_complete;

    int              i_chunk;
    rar_file_chunk_t **pp_chunk;
    uint64_t         i_real_size;  /* Gathered size */
} rar_file_t;

static void RarFileDelete( rar_file_t * );

struct stream_sys_t
{
    rar_file_t *p_file;
    const rar_file_chunk_t *p_chunk;

    uint64_t i_position;

    uint8_t *p_peek_alloc;
    uint8_t *p_peek;
    unsigned int i_peek;
};


/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  Read   ( stream_t *, void *p_read, unsigned int i_read );
static int  Peek   ( stream_t *, const uint8_t **pp_peek, unsigned int i_peek );
static int  Control( stream_t *, int i_query, va_list );

static int  Parse  ( stream_t * );
static int  Seek   ( stream_t *s, uint64_t i_position );

/****************************************************************************
 * Open
 ****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    stream_t *s = (stream_t*)p_this;
    stream_sys_t *p_sys;

    /* */
    const uint8_t *p_peek;
    if( stream_Peek( s->p_source, &p_peek, i_rar_marker ) < i_rar_marker )
        return VLC_EGENERIC;
    if( memcmp( p_peek, p_rar_marker, i_rar_marker ) )
        return VLC_EGENERIC;

    /* */
    s->pf_read = Read;
    s->pf_peek = Peek;
    s->pf_control = Control;

    s->p_sys = p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* */
    p_sys->p_file = NULL;
    p_sys->i_position = 0;
    p_sys->p_chunk = NULL;

    p_sys->p_peek_alloc = NULL;
    p_sys->p_peek = NULL;
    p_sys->i_peek = 0;

    /* */
    if( Parse( s ) || !p_sys->p_file || p_sys->p_file->i_chunk <= 0 )
    {
        msg_Err( s, "Invalid or unsupported RAR archive" );
        if( p_sys->p_file )
            RarFileDelete( p_sys->p_file );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* */
    Seek( s, 0 );

    /* */
    const rar_file_t *p_file = p_sys->p_file;
    msg_Dbg( s, "Using RAR stream filter for '%s' %"PRId64"(expected %"PRId64") bytes in %d chunks",
             p_file->psz_name, p_file->i_real_size, p_file->i_size, p_file->i_chunk );

    return VLC_SUCCESS;
}

/****************************************************************************
 * Close
 ****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    stream_t *s = (stream_t*)p_this;
    stream_sys_t *p_sys = s->p_sys;

    RarFileDelete( p_sys->p_file );
    free( p_sys->p_peek_alloc );
    free( p_sys );
}

/****************************************************************************
 * Stream filters functions
 ****************************************************************************/
static int Read( stream_t *s, void *p_read, unsigned int i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    uint8_t *p_data = p_read;
    unsigned int i_total = 0;

    if( p_sys->i_peek > 0 && i_read > 0 )
    {
        const unsigned int i_copy = __MIN( i_read, p_sys->i_peek );

        if( p_data )
        {
            memcpy( p_data, p_sys->p_peek, i_copy );
            p_data += i_copy;
        }

        p_sys->i_peek -= i_copy;
        p_sys->p_peek += i_copy;
        i_total += i_copy;
    }

    while( i_total < i_read )
    {
        const uint64_t i_chunk_end = p_sys->p_chunk->i_cummulated_size + p_sys->p_chunk->i_size;

        int i_max = __MIN( i_read - i_total, i_chunk_end - p_sys->i_position );
        if( i_max <= 0 )
            break;

        int i_real = stream_Read( s->p_source, p_data, i_max );
        if( i_real <= 0 )
            break;

        i_total += i_real;
        if( p_data )
            p_data += i_real;
        p_sys->i_position += i_real;
        if( p_sys->i_position >= i_chunk_end )
        {
            if( Seek( s, p_sys->i_position ) )
                break;
        }
    }
    return i_total;
}

static int Peek( stream_t *s, const uint8_t **pp_peek, unsigned int i_peek )
{
    stream_sys_t *p_sys = s->p_sys;

    if( i_peek <= p_sys->i_peek )
    {
        *pp_peek = p_sys->p_peek;
        return i_peek;
    }

    /* */
    uint8_t *p_peek = malloc( i_peek );
    if( !p_peek )
        return 0;

    /* XXX yes stream_Read on ourself */
    int i_read = stream_Read( s, p_peek, i_peek );
    if( i_read <= 0 )
    {
        free( p_peek );
        return i_read;
    }

    free( p_sys->p_peek_alloc );

    p_sys->p_peek_alloc =
    p_sys->p_peek       = p_peek;
    p_sys->i_peek       = i_read;

    *pp_peek = p_sys->p_peek;
    return p_sys->i_peek;
}

static int Control( stream_t *s, int i_query, va_list args )
{
    stream_sys_t *p_sys = s->p_sys;

    switch( i_query )
    {
    /* */
    case STREAM_SET_POSITION:
    {
        uint64_t i_position = va_arg( args, uint64_t );
        return Seek( s, i_position );
    }

    case STREAM_GET_POSITION:
    {
        uint64_t *pi_position = va_arg( args, uint64_t* );
        *pi_position = p_sys->i_position - p_sys->i_peek;
        return VLC_SUCCESS;
    }

    case STREAM_GET_SIZE:
    {
        uint64_t *pi_size = (uint64_t*)va_arg( args, uint64_t* );
        *pi_size = p_sys->p_file->i_real_size;
        return VLC_SUCCESS;
    }

    /* */
    case STREAM_GET_CONTENT_TYPE: /* arg1= char ** */
        return VLC_EGENERIC;

    case STREAM_UPDATE_SIZE: /* TODO maybe we should update i_real_size from file size and chunk offset ? */
    case STREAM_CONTROL_ACCESS:
    case STREAM_CAN_SEEK:
    case STREAM_CAN_FASTSEEK:
    case STREAM_SET_RECORD_STATE:
        return stream_vaControl( s->p_source, i_query, args );
    default:
        return VLC_EGENERIC;
    }
}

/****************************************************************************
 * Helpers
 ****************************************************************************/
static int Seek( stream_t *s, uint64_t i_position )
{
    stream_sys_t *p_sys = s->p_sys;

    if( i_position > p_sys->p_file->i_real_size )
        i_position = p_sys->p_file->i_real_size;

    /* Search the chunk */
    const rar_file_t *p_file = p_sys->p_file;
    for( int i = 0; i < p_file->i_chunk; i++ )
    {
        p_sys->p_chunk = p_file->pp_chunk[i];
        if( i_position < p_sys->p_chunk->i_cummulated_size + p_sys->p_chunk->i_size )
            break;
    }
    p_sys->i_position = i_position;
    p_sys->i_peek     = 0;

    const uint64_t i_seek = p_sys->p_chunk->i_offset +
                            ( i_position - p_sys->p_chunk->i_cummulated_size );
    return stream_Seek( s->p_source, i_seek );
}

static void RarFileDelete( rar_file_t *p_file )
{
    for( int i = 0; i < p_file->i_chunk; i++ )
        free( p_file->pp_chunk[i] );
    free( p_file->pp_chunk );
    free( p_file->psz_name );
    free( p_file );
}

typedef struct
{
    uint16_t i_crc;
    uint8_t  i_type;
    uint16_t i_flags;
    uint16_t i_size;
    uint32_t i_add_size;
} rar_block_t;

enum
{
    RAR_BLOCK_MARKER = 0x72,
    RAR_BLOCK_ARCHIVE = 0x73,
    RAR_BLOCK_FILE = 0x74,
    RAR_BLOCK_END = 0x7b,
};
enum
{
    RAR_BLOCK_END_HAS_NEXT = 0x0001,
};
enum
{
    RAR_BLOCK_FILE_HAS_PREVIOUS = 0x0001,
    RAR_BLOCK_FILE_HAS_NEXT     = 0x0002,
    RAR_BLOCK_FILE_HAS_HIGH     = 0x0100,
};

static int PeekBlock( stream_t *s, rar_block_t *p_hdr )
{
    const uint8_t *p_peek;
    int i_peek = stream_Peek( s->p_source, &p_peek, 11 );

    if( i_peek < 7 )
        return VLC_EGENERIC;

    p_hdr->i_crc   = GetWLE( &p_peek[0] );
    p_hdr->i_type  = p_peek[2];
    p_hdr->i_flags = GetWLE( &p_peek[3] );
    p_hdr->i_size  = GetWLE( &p_peek[5] );
    p_hdr->i_add_size = 0;
    if( p_hdr->i_flags & 0x8000 )
    {
        if( i_peek < 11 )
            return VLC_EGENERIC;
        p_hdr->i_add_size = GetDWLE( &p_peek[7] );
    }

    if( p_hdr->i_size < 7 )
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}
static int SkipBlock( stream_t *s, const rar_block_t *p_hdr )
{
    uint64_t i_size = (uint64_t)p_hdr->i_size + p_hdr->i_add_size;

    while( i_size > 0 )
    {
        int i_skip = __MIN( i_size, INT_MAX );
        if( stream_Read( s->p_source, NULL, i_skip ) < i_skip )
            return VLC_EGENERIC;

        i_size -= i_skip;
    }
    return VLC_SUCCESS;
}

static int IgnoreBlock( stream_t *s, int i_block )
{
    /* */
    rar_block_t bk;
    if( PeekBlock( s, &bk ) || bk.i_type != i_block )
        return VLC_EGENERIC;
    return SkipBlock( s, &bk );
}

static int SkipEnd( stream_t *s, const rar_block_t *p_hdr )
{
    if( !(p_hdr->i_flags & RAR_BLOCK_END_HAS_NEXT) )
        return VLC_EGENERIC;

    if( SkipBlock( s, p_hdr ) )
        return VLC_EGENERIC;

    /* Now, we need to look for a marker block,
     * It seems that there is garbage at EOF */
    for( ;; )
    {
        const uint8_t *p_peek;

        if( stream_Peek( s->p_source, &p_peek, i_rar_marker ) < i_rar_marker )
            return VLC_EGENERIC;

        if( !memcmp( p_peek, p_rar_marker, i_rar_marker ) )
            break;

        if( stream_Read( s->p_source, NULL, 1 ) != 1 )
            return VLC_EGENERIC;
    }

    /* Skip marker and archive blocks */
    if( IgnoreBlock( s, RAR_BLOCK_MARKER ) )
        return VLC_EGENERIC;
    if( IgnoreBlock( s, RAR_BLOCK_ARCHIVE ) )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static int SkipFile( stream_t *s,const rar_block_t *p_hdr )
{
    stream_sys_t *p_sys = s->p_sys;
    const uint8_t *p_peek;

    int i_min_size = 7+21;
    if( p_hdr->i_flags & RAR_BLOCK_FILE_HAS_HIGH )
        i_min_size += 8;
    if( p_hdr->i_size < (unsigned)i_min_size )
        return VLC_EGENERIC;

    if( stream_Peek( s->p_source, &p_peek, i_min_size ) < i_min_size )
        return VLC_EGENERIC;

    /* */
    uint32_t i_file_size_low = GetDWLE( &p_peek[7+4] );
    uint8_t  i_method = p_peek[7+18];
    uint16_t i_name_size = GetWLE( &p_peek[7+19] );
    uint32_t i_file_size_high = 0;
    if( p_hdr->i_flags & RAR_BLOCK_FILE_HAS_HIGH )
        i_file_size_high = GetDWLE( &p_peek[7+25] );

    char *psz_name = calloc( 1, i_name_size + 1 );
    if( !psz_name )
        return VLC_EGENERIC;

    const int i_name_offset = (p_hdr->i_flags & RAR_BLOCK_FILE_HAS_HIGH) ? (7+33) : (7+25);
    if( i_name_offset + i_name_size <= p_hdr->i_size )
    {
        const int i_max_size = i_name_offset + i_name_size;
        if( stream_Peek( s->p_source, &p_peek, i_max_size ) < i_max_size )
        {
            free( psz_name );
            return VLC_EGENERIC;
        }
        memcpy( psz_name, &p_peek[i_name_offset], i_name_size );
    }

    if( i_method != 0x30 )
    {
        msg_Warn( s, "Ignoring compressed file %s (method=0x%2.2x)", psz_name, i_method );
        goto exit;
    }

    /* Ignore smaller files */
    const uint64_t i_file_size = ((uint64_t)i_file_size_high << 32) | i_file_size_low;
    if( p_sys->p_file &&
        p_sys->p_file->i_size < i_file_size )
    {
        RarFileDelete( p_sys->p_file );
        p_sys->p_file = NULL;
    }
    /* */
    rar_file_t *p_current = p_sys->p_file;
    if( !p_current )
    {
        p_sys->p_file = p_current = malloc( sizeof( *p_sys->p_file ) );
        if( !p_current )
            goto exit;

        /* */
        p_current->psz_name = psz_name;
        p_current->i_size = i_file_size;
        p_current->b_complete = false;
        p_current->i_real_size = 0;
        TAB_INIT( p_current->i_chunk, p_current->pp_chunk );

        psz_name = NULL;
    }

    /* Append chunks */
    if( !p_current->b_complete )
    {
        bool b_append = false;
        /* Append if first chunk */
        if( p_current->i_chunk <= 0 )
            b_append = true;
        /* Append if it is really a continuous chunck */
        if( p_current->i_size == i_file_size &&
            ( !psz_name || !strcmp( p_current->psz_name, psz_name ) ) &&
            ( p_hdr->i_flags & RAR_BLOCK_FILE_HAS_PREVIOUS ) )
            b_append = true;

        if( b_append )
        {
            rar_file_chunk_t *p_chunk = malloc( sizeof( *p_chunk ) );
            if( p_chunk )
            {
                p_chunk->i_offset = stream_Tell( s->p_source ) + p_hdr->i_size;
                p_chunk->i_size = p_hdr->i_add_size;
                p_chunk->i_cummulated_size = 0;
                if( p_current->i_chunk > 0 )
                {
                    rar_file_chunk_t *p_previous = p_current->pp_chunk[p_current->i_chunk-1];

                    p_chunk->i_cummulated_size += p_previous->i_cummulated_size +
                                                  p_previous->i_size;
                }

                TAB_APPEND( p_current->i_chunk, p_current->pp_chunk, p_chunk );

                p_current->i_real_size += p_hdr->i_add_size;
            }
        }

        if( !(p_hdr->i_flags & RAR_BLOCK_FILE_HAS_NEXT ) )
            p_current->b_complete = true;
    }

exit:
    /* */
    free( psz_name );

    /* We stop on the first non empty file if we cannot seek */
    if( p_sys->p_file )
    {
        bool b_can_seek = false;
        stream_Control( s->p_source, STREAM_CAN_SEEK, &b_can_seek );
        if( !b_can_seek && p_current->i_size > 0 )
            return VLC_EGENERIC;
    }

    if( SkipBlock( s, p_hdr ) )
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static int Parse( stream_t *s )
{
    /* Skip marker */
    if( IgnoreBlock( s, RAR_BLOCK_MARKER ) )
        return VLC_EGENERIC;

    /* Skip archive  */
    if( IgnoreBlock( s, RAR_BLOCK_ARCHIVE ) )
        return VLC_EGENERIC;

    /* */
    for( ;; )
    {
        rar_block_t bk;
        int i_ret;

        if( PeekBlock( s, &bk ) )
            break;

        switch( bk.i_type )
        {
        case RAR_BLOCK_END:
            i_ret = SkipEnd( s, &bk );
            break;
        case RAR_BLOCK_FILE:
            i_ret = SkipFile( s, &bk );
            break;
        default:
            i_ret = SkipBlock( s, &bk );
            break;
        }
        if( i_ret )
            break;
    }

    return VLC_SUCCESS;
}
