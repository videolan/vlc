/*****************************************************************************
 * aac.c : Raw aac Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_description( _("AAC demuxer" ) );
    set_capability( "demux2", 100 );
    set_callbacks( Open, Close );
    add_shortcut( "aac" );
vlc_module_end();

/* TODO:
 * - adif support ?
 *
 */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct demux_sys_t
{
    mtime_t         i_time;

    es_out_id_t     *p_es;
};

static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

static int i_aac_samplerate[16] =
{
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000,
    7350,  0,     0,     0
};

#define AAC_ID( p )          ( ((p)[1]>>3)&0x01 )
#define AAC_SAMPLE_RATE( p ) i_aac_samplerate[((p)[2]>>2)&0x0f]
#define AAC_CHANNELS( p )    ( (((p)[2]&0x01)<<2) | (((p)[3]>>6)&0x03) )
#define AAC_FRAME_SIZE( p )  ( (((p)[3]&0x03) << 11)|( (p)[4] << 3 )|( (((p)[5]) >>5)&0x7 ) )

/* FIXME it's plain wrong */
#define AAC_FRAME_SAMPLES( p )  1024

static inline int HeaderCheck( uint8_t *p )
{
    if( p[0] != 0xff ||
        ( p[1]&0xf6 ) != 0xf0 ||
        AAC_SAMPLE_RATE( p ) == 0 ||
        AAC_CHANNELS( p ) == 0 ||
        AAC_FRAME_SIZE( p ) == 0 )
    {
        return VLC_FALSE;
    }
    return VLC_TRUE;
}

/*****************************************************************************
 * Open: initializes AAC demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    int         b_forced = VLC_FALSE;

    uint8_t     *p_peek;
    module_t    *p_id3;
    es_format_t fmt;

    if( !strncmp( p_demux->psz_demux, "aac", 3 ) )
    {
        b_forced = VLC_TRUE;
    }

    if( p_demux->psz_path )
    {
        int  i_len = strlen( p_demux->psz_path );

        if( i_len > 4 && !strcasecmp( &p_demux->psz_path[i_len - 4], ".aac" ) )
        {
            b_forced = VLC_TRUE;
        }
    }

    if( !b_forced )
    {
        /* I haven't find any sure working aac detection so only forced or
         * extention check
         */
        return VLC_EGENERIC;
    }

    /* skip possible id3 header */
    if( ( p_id3 = module_Need( p_demux, "id3", NULL, 0 ) ) )
    {
        module_Unneed( p_demux, p_id3 );
    }

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );

    p_sys->i_time = 1;

    /* peek the begining (10 is for adts header) */
    if( stream_Peek( p_demux->s, &p_peek, 10 ) < 10 )
    {
        msg_Err( p_demux, "cannot peek" );
        goto error;
    }
    if( !strncmp( p_peek, "ADIF", 4 ) )
    {
        msg_Err( p_demux, "ADIF file. Not yet supported. (Please report)" );
        goto error;
    }

    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 'm', 'p', '4', 'a' ) );
    if( HeaderCheck( p_peek ) )
    {
        fmt.audio.i_channels = AAC_CHANNELS( p_peek );
        fmt.audio.i_rate     = AAC_SAMPLE_RATE( p_peek );

        msg_Dbg( p_demux,
                 "adts header: id=%d channels=%d sample_rate=%d",
                 AAC_ID( p_peek ),
                 AAC_CHANNELS( p_peek ),
                 AAC_SAMPLE_RATE( p_peek ) );
    }

    p_sys->p_es = es_out_Add( p_demux->out, &fmt );
    return VLC_SUCCESS;

error:
    free( p_sys );
    return VLC_EGENERIC;
}


/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block;

    uint8_t     h[8];
    uint8_t     *p_peek;

    if( stream_Peek( p_demux->s, &p_peek, 8 ) < 8 )
    {
        msg_Warn( p_demux, "cannot peek" );
        return 0;
    }

    if( !HeaderCheck( p_peek ) )
    {
        /* we need to resynch */
        vlc_bool_t  b_ok = VLC_FALSE;
        int         i_skip = 0;
        int         i_peek;

        i_peek = stream_Peek( p_demux->s, &p_peek, 8096 );
        if( i_peek < 8 )
        {
            msg_Warn( p_demux, "cannot peek" );
            return 0;
        }

        while( i_peek >= 8 )
        {
            if( HeaderCheck( p_peek ) )
            {
                b_ok = VLC_TRUE;
                break;
            }

            p_peek++;
            i_peek--;
            i_skip++;
        }

        msg_Warn( p_demux, "garbage=%d bytes", i_skip );
        stream_Read( p_demux->s, NULL, i_skip );
        return 1;
    }

    memcpy( h, p_peek, 8 );    /* can't use p_peek after stream_*  */



    /* set PCR */
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_time );

    if( ( p_block = stream_Block( p_demux->s, AAC_FRAME_SIZE( h ) ) ) == NULL )
    {
        msg_Warn( p_demux, "cannot read data" );
        return 0;
    }

    p_block->i_dts = p_block->i_pts = p_sys->i_time;

    es_out_Send( p_demux->out, p_sys->p_es, p_block );

    p_sys->i_time += (mtime_t)1000000 *
                     (mtime_t)AAC_FRAME_SAMPLES( h ) /
                     (mtime_t)AAC_SAMPLE_RATE( h );
    return( 1 );
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    /* demux_sys_t *p_sys  = p_demux->p_sys; */
    /* FIXME calculate the bitrate */
    if( i_query == DEMUX_SET_TIME )
        return VLC_EGENERIC;
    else
        return demux2_vaControlHelper( p_demux->s,
                                       0, -1,
                                       8*0, 1, i_query, args );
}

