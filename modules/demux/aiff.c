/*****************************************************************************
 * aiff.c: Audio Interchange File Format demuxer
 *****************************************************************************
 * Copyright (C) 2004-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_demux.h>

/* TODO:
 *  - ...
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_description( N_("AIFF demuxer" ) );
    set_capability( "demux", 10 );
    set_callbacks( Open, Close );
    add_shortcut( "aiff" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

struct demux_sys_t
{
    es_format_t  fmt;
    es_out_id_t *es;

    int64_t     i_ssnd_pos;
    int64_t     i_ssnd_size;
    int         i_ssnd_offset;
    int         i_ssnd_blocksize;

    /* real data start */
    int64_t     i_ssnd_start;
    int64_t     i_ssnd_end;

    int         i_ssnd_fsize;

    int64_t     i_time;
};

static int Demux  ( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

/* GetF80BE: read a 80 bits float in big endian */
static unsigned int GetF80BE( const uint8_t p[10] )
{
    unsigned int i_mantissa = GetDWBE( &p[2] );
    int          i_exp = 30 - p[1];
    unsigned int i_last = 0;

    while( i_exp-- > 0 )
    {
        i_last = i_mantissa;
        i_mantissa >>= 1;
    }
    if( i_last&0x01 )
    {
        i_mantissa++;
    }
    return i_mantissa;
}

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    const uint8_t *p_peek;

    if( stream_Peek( p_demux->s, &p_peek, 12 ) < 12 )
        return VLC_EGENERIC;
    if( memcmp( p_peek, "FORM", 4 )
     || memcmp( &p_peek[8], "AIFF", 4 ) )
        return VLC_EGENERIC;

    /* skip aiff header */
    stream_Read( p_demux->s, NULL, 12 );

    /* Fill p_demux field */
    DEMUX_INIT_COMMON(); p_sys = p_demux->p_sys;
    es_format_Init( &p_sys->fmt, UNKNOWN_ES, 0 );
    p_sys->i_time = 1;
    p_sys->i_ssnd_pos = -1;

    for( ;; )
    {
        uint32_t i_size;

        CHECK_PEEK_GOTO( p_peek, 8 );
        i_size = GetDWBE( &p_peek[4] );

        msg_Dbg( p_demux, "chunk fcc=%4.4s size=%d", p_peek, i_size );

        if( !memcmp( p_peek, "COMM", 4 ) )
        {
            CHECK_PEEK_GOTO( p_peek, 18+8 );
            es_format_Init( &p_sys->fmt, AUDIO_ES, VLC_FOURCC( 't', 'w', 'o', 's' ) );
            p_sys->fmt.audio.i_channels = GetWBE( &p_peek[8] );
            p_sys->fmt.audio.i_bitspersample = GetWBE( &p_peek[14] );
            p_sys->fmt.audio.i_rate     = GetF80BE( &p_peek[16] );

            msg_Dbg( p_demux, "COMM: channels=%d samples_frames=%d bits=%d rate=%d",
                     GetWBE( &p_peek[8] ), GetDWBE( &p_peek[10] ), GetWBE( &p_peek[14] ), GetF80BE( &p_peek[16] ) );
        }
        else if( !memcmp( p_peek, "SSND", 4 ) )
        {
            CHECK_PEEK_GOTO( p_peek, 8+8 );
            p_sys->i_ssnd_pos = stream_Tell( p_demux->s );
            p_sys->i_ssnd_size = i_size;
            p_sys->i_ssnd_offset = GetDWBE( &p_peek[8] );
            p_sys->i_ssnd_blocksize = GetDWBE( &p_peek[12] );

            msg_Dbg( p_demux, "SSND: (offset=%d blocksize=%d)",
                     p_sys->i_ssnd_offset, p_sys->i_ssnd_blocksize );
        }
        if( p_sys->i_ssnd_pos >= 12 && p_sys->fmt.i_cat == AUDIO_ES )
        {
            /* We have found the 2 needed chunks */
            break;
        }

        /* Skip this chunk */
        i_size += 8;
        if( (i_size % 2) != 0 )
            i_size++;
        if( stream_Read( p_demux->s, NULL, i_size ) != (int)i_size )
        {
            msg_Warn( p_demux, "incomplete file" );
            goto error;
        }
    }

    p_sys->i_ssnd_start = p_sys->i_ssnd_pos + 16 + p_sys->i_ssnd_offset;
    p_sys->i_ssnd_end   = p_sys->i_ssnd_start + p_sys->i_ssnd_size;

    p_sys->i_ssnd_fsize = p_sys->fmt.audio.i_channels *
                          ((p_sys->fmt.audio.i_bitspersample + 7) / 8);

    if( p_sys->i_ssnd_fsize <= 0 )
    {
        msg_Err( p_demux, "invalid audio parameters" );
        goto error;
    }

    if( p_sys->i_ssnd_size <= 0 )
    {
        /* unknown */
        p_sys->i_ssnd_end = 0;
    }

    /* seek into SSND chunk */
    if( stream_Seek( p_demux->s, p_sys->i_ssnd_start ) )
    {
        msg_Err( p_demux, "cannot seek to data chunk" );
        goto error;
    }

    /* */
    p_sys->es = es_out_Add( p_demux->out, &p_sys->fmt );

    return VLC_SUCCESS;

error:
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    free( p_sys );
}


/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t     i_tell = stream_Tell( p_demux->s );

    block_t     *p_block;
    int         i_read;

    if( p_sys->i_ssnd_end > 0 && i_tell >= p_sys->i_ssnd_end )
    {
        /* EOF */
        return 0;
    }

    /* Set PCR */
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_time);

    /* we will read 100ms at once */
    i_read = p_sys->i_ssnd_fsize * ( p_sys->fmt.audio.i_rate / 10 );
    if( p_sys->i_ssnd_end > 0 && p_sys->i_ssnd_end - i_tell < i_read )
    {
        i_read = p_sys->i_ssnd_end - i_tell;
    }
    if( ( p_block = stream_Block( p_demux->s, i_read ) ) == NULL )
    {
        return 0;
    }

    p_block->i_dts =
    p_block->i_pts = p_sys->i_time;

    p_sys->i_time += (int64_t)1000000 *
                     p_block->i_buffer /
                     p_sys->i_ssnd_fsize /
                     p_sys->fmt.audio.i_rate;

    /* */
    es_out_Send( p_demux->out, p_sys->es, p_block );
    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    int64_t *pi64;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
        {
            int64_t i_start = p_sys->i_ssnd_start;
            int64_t i_end   = p_sys->i_ssnd_end > 0 ? p_sys->i_ssnd_end : stream_Size( p_demux->s );
            int64_t i_tell  = stream_Tell( p_demux->s );

            pf = (double*) va_arg( args, double* );

            if( i_start < i_end )
            {
                *pf = (double)(i_tell - i_start)/(double)(i_end - i_start);
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }

        case DEMUX_SET_POSITION:
        {
            int64_t i_start = p_sys->i_ssnd_start;
            int64_t i_end  = p_sys->i_ssnd_end > 0 ? p_sys->i_ssnd_end : stream_Size( p_demux->s );

            f = (double) va_arg( args, double );

            if( i_start < i_end )
            {
                int     i_frame = (f * ( i_end - i_start )) / p_sys->i_ssnd_fsize;
                int64_t i_new   = i_start + i_frame * p_sys->i_ssnd_fsize;

                if( stream_Seek( p_demux->s, i_new ) )
                {
                    return VLC_EGENERIC;
                }
                p_sys->i_time = 1 + (int64_t)1000000 * i_frame / p_sys->fmt.audio.i_rate;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_time;
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
        {
            int64_t i_end  = p_sys->i_ssnd_end > 0 ? p_sys->i_ssnd_end : stream_Size( p_demux->s );

            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_ssnd_start < i_end )
            {
                *pi64 = (int64_t)1000000 * ( i_end - p_sys->i_ssnd_start ) / p_sys->i_ssnd_fsize / p_sys->fmt.audio.i_rate;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }
        case DEMUX_SET_TIME:
        case DEMUX_GET_FPS:
        default:
            return VLC_EGENERIC;
    }
}
