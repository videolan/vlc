
/*****************************************************************************
 * mpga.c : MPEG-I/II Audio input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2004 VideoLAN
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

#include "vlc_meta.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("MPEG-I/II audio demuxer" ) );
    set_capability( "demux2", 100 );
    set_callbacks( Open, Close );
    add_shortcut( "mpga" );
    add_shortcut( "mp3" );
vlc_module_end();

/* TODO:
 * - free bitrate
 */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

struct demux_sys_t
{
    mtime_t         i_time;
    mtime_t         i_time_offset;

    int             i_bitrate_avg;  /* extracted from Xing header */

    vlc_meta_t      *meta;

    es_out_id_t     *p_es;
};

static int HeaderCheck( uint32_t h )
{
    if( ((( h >> 21 )&0x07FF) != 0x07FF )   /* header sync */
        || (((h >> 17)&0x03) == 0 )         /* valid layer ?*/
        || (((h >> 12)&0x0F) == 0x0F )
        || (((h >> 12)&0x0F) == 0x00 )      /* valid bitrate ? */
        || (((h >> 10) & 0x03) == 0x03 )    /* valide sampling freq ? */
        || ((h & 0x03) == 0x02 ))           /* valid emphasis ? */
    {
        return( VLC_FALSE );
    }
    return( VLC_TRUE );
}

static int mpga_sample_rate[2][4] =
{
    { 44100, 48000, 32000, 0 },
    { 22050, 24000, 16000, 0 }
};

static int mpga_bitrate[2][3][16] =
{
  {
    { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
    { 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
    { 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0}
  },
  {
    { 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
    { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0},
    { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0}
  }
};


#define MPGA_VERSION( h )   ( 1 - (((h)>>19)&0x01) )
#define MPGA_LAYER( h )     ( 3 - (((h)>>17)&0x03) )
#define MPGA_SAMPLE_RATE(h) \
    ( mpga_sample_rate[MPGA_VERSION(h)][((h)>>10)&0x03] / ( ((h>>20)&0x01) ? 1 : 2) )
#define MPGA_CHANNELS(h)    ( (((h)>>6)&0x03) == 3 ? 1 : 2)
#define MPGA_BITRATE(h)     mpga_bitrate[MPGA_VERSION(h)][MPGA_LAYER(h)][((h)>>12)&0x0f]
#define MPGA_PADDING(h)     ( ((h)>>9)&0x01 )
#define MPGA_MODE(h)        (((h)>> 6)&0x03)

static int mpga_frame_size( uint32_t h )
{
    switch( MPGA_LAYER(h) )
    {
        case 0:
            return ( ( 12000 * MPGA_BITRATE(h) ) / MPGA_SAMPLE_RATE(h) + MPGA_PADDING(h) ) * 4;
        case 1:
            return ( 144000 * MPGA_BITRATE(h) ) / MPGA_SAMPLE_RATE(h) + MPGA_PADDING(h);
        case 2:
            return ( ( MPGA_VERSION(h) ? 72000 : 144000 ) * MPGA_BITRATE(h) ) / MPGA_SAMPLE_RATE(h) + MPGA_PADDING(h);
        default:
            return 0;
    }
}

static int mpga_frame_samples( uint32_t h )
{
    switch( MPGA_LAYER(h) )
    {
        case 0:
            return 384;
        case 1:
            return 1152;
        case 2:
            return MPGA_VERSION(h) ? 576 : 1152;
        default:
            return 0;
    }
}

/*****************************************************************************
 * Open: initializes demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    vlc_bool_t   b_forced = VLC_FALSE;
    vlc_bool_t   b_extention = VLC_FALSE;

    uint32_t     header;
    uint8_t     *p_peek;
    module_t    *p_id3;
    es_format_t   fmt;

    if( !strncmp( p_demux->psz_demux, "mpga", 4 ) ||
        !strncmp( p_demux->psz_demux, "mp3", 3 ) )
    {
        b_forced = VLC_TRUE;
    }
    if( p_demux->psz_path )
    {
        int  i_len = strlen( p_demux->psz_path );
        if( i_len > 4 && !strcasecmp( &p_demux->psz_path[i_len - 4], ".mp3" ) )
        {
            b_extention = VLC_TRUE;
        }
    }

    p_demux->p_sys      = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->i_time = 1;
    p_sys->i_time_offset = 0;
    p_sys->i_bitrate_avg = 0;
    p_sys->meta = NULL;

    /* skip/parse possible id3 header */
    if( ( p_id3 = module_Need( p_demux, "id3", NULL, 0 ) ) )
    {
        p_sys->meta = (vlc_meta_t *)p_demux->p_private;
        p_demux->p_private = NULL;

        module_Unneed( p_demux, p_id3 );
    }

    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 )
    {
        msg_Err( p_demux, "cannot peek" );
        Close( VLC_OBJECT(p_demux ) );
        return VLC_EGENERIC;
    }

    if( !HeaderCheck( header = GetDWBE( p_peek ) ) )
    {
        vlc_bool_t b_ok = VLC_FALSE;
        int i_peek;

        if( !b_forced && !b_extention )
        {
            msg_Warn( p_demux, "mpga module discarded" );
            Close( VLC_OBJECT(p_demux) );
            return VLC_EGENERIC;
        }

        i_peek = stream_Peek( p_demux->s, &p_peek, 8096 );

        while( i_peek > 4 )
        {
            if( HeaderCheck( header = GetDWBE( p_peek ) ) )
            {
                b_ok = VLC_TRUE;
                break;
            }
            p_peek += 4;
            i_peek -= 4;
        }
        if( !b_ok && !b_forced )
        {
            msg_Warn( p_demux, "mpga module discarded" );
            Close( VLC_OBJECT(p_demux) );
            return VLC_EGENERIC;
        }
    }

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;

    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 'm', 'p', 'g', 'a' ) );

    if( HeaderCheck( header ) )
    {
        int     i_xing;
        uint8_t *p_xing;
        char psz_description[50];

        p_sys->i_bitrate_avg = MPGA_BITRATE( header ) * 1000;
        if( ( i_xing = stream_Peek( p_demux->s, &p_xing, 1024 ) ) >= 21 )
        {
            int i_skip;

            if( MPGA_VERSION( header) == 0 )
            {
                i_skip = MPGA_MODE( header ) != 3 ? 36 : 21;
            }
            else
            {
                i_skip = MPGA_MODE( header ) != 3 ? 21 : 13;
            }
            if( i_skip + 8 < i_xing &&
                !strncmp( &p_xing[i_skip], "Xing", 4 ) )
            {
                unsigned int i_flags = GetDWBE( &p_xing[i_skip+4] );
                unsigned int i_bytes = 0, i_frames = 0;

                p_xing += i_skip + 8;
                i_xing -= i_skip + 8;

                i_skip = 0;
                if( i_flags&0x01 && i_skip + 4 <= i_xing )   /* XING_FRAMES */
                {
                    i_frames = GetDWBE( &p_xing[i_skip] );
                    i_skip += 4;
                }
                if( i_flags&0x02 && i_skip + 4 <= i_xing )   /* XING_BYTES */
                {
                    i_bytes = GetDWBE( &p_xing[i_skip] );
                    i_skip += 4;
                }
                if( i_flags&0x04 )   /* XING_TOC */
                {
                    i_skip += 100;
                }
#if 0
// FIXME: doesn't return the right bitrage average, at least with some MP3's
                if( i_flags&0x08 && i_skip + 4 <= i_xing )   /* XING_VBR */
                {
                    p_sys->i_bitrate_avg = GetDWBE( &p_xing[i_skip] );
    fprintf(stderr,"rate2 %d\n", p_sys->i_bitrate_avg);
                    msg_Dbg( p_input, "xing vbr value present (%d)", p_sys->i_bitrate_avg );
                }
                else
#endif
                if( i_frames > 0 && i_bytes > 0 )
                {
                    p_sys->i_bitrate_avg = (int64_t)i_bytes *
                                           (int64_t)8 *
                                           (int64_t)MPGA_SAMPLE_RATE( header ) /
                                           (int64_t)i_frames /
                                           (int64_t)mpga_frame_samples( header );
                    msg_Dbg( p_demux, "xing frames&bytes value present (%db/s)", p_sys->i_bitrate_avg );
                }
            }
        }

        msg_Dbg( p_demux, "version=%d layer=%d channels=%d samplerate=%d",
                 MPGA_VERSION( header ) + 1,
                 MPGA_LAYER( header ) + 1,
                 MPGA_CHANNELS( header ),
                 MPGA_SAMPLE_RATE( header ) );

        fmt.audio.i_channels = MPGA_CHANNELS( header );
        fmt.audio.i_rate = MPGA_SAMPLE_RATE( header );
        fmt.i_bitrate = p_sys->i_bitrate_avg;
        sprintf( psz_description, "MPEG Audio Layer %d, version %d",
                 MPGA_LAYER ( header ) + 1, MPGA_VERSION ( header ) + 1 );
        fmt.psz_description = strdup( psz_description );
    }

    p_sys->p_es = es_out_Add( p_demux->out, &fmt );
    if( fmt.psz_description ) free( fmt.psz_description );

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_frame;

    uint32_t     header;
    uint8_t     *p_peek;

    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 )
    {
        msg_Warn( p_demux, "cannot peek" );
        return 0;
    }

    if( !HeaderCheck( header = GetDWBE( p_peek ) ) )
    {
        /* we need to resynch */
        vlc_bool_t  b_ok = VLC_FALSE;
        int         i_skip = 0;
        int         i_peek;

        i_peek = stream_Peek( p_demux->s, &p_peek, 8096 );
        if( i_peek < 4 )
        {
            msg_Warn( p_demux, "cannot peek" );
            return 0;
        }

        while( i_peek >= 4 )
        {
            if( HeaderCheck( header = GetDWBE( p_peek ) ) )
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

    if( ( p_frame = stream_Block( p_demux->s,
                                  mpga_frame_size( header ) ) ) == NULL )
    {
        msg_Warn( p_demux, "cannot read data" );
        return 0;
    }
    p_frame->i_dts = p_frame->i_pts = p_sys->i_time;

    /* set PCR */
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_time );

    es_out_Send( p_demux->out, p_sys->p_es, p_frame );

    p_sys->i_time += (mtime_t)1000000 *
                     (mtime_t)mpga_frame_samples( header ) /
                     (mtime_t)MPGA_SAMPLE_RATE( header );
    return( 1 );
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    if( p_sys->meta )
    {
        vlc_meta_Delete( p_sys->meta );
    }

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    int64_t *pi64;
    vlc_meta_t **pp_meta;
    int i_ret;

    switch( i_query )
    {
        case DEMUX_GET_META:
            pp_meta = (vlc_meta_t **)va_arg( args, vlc_meta_t** );
            if( p_sys->meta )
            {
                *pp_meta = vlc_meta_Duplicate( p_sys->meta );
            }
            else
            {
                *pp_meta = NULL;
            }
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_time + p_sys->i_time_offset;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
            /* FIXME TODO: implement a high precision seek (with mp3 parsing)
             * needed for multi-input */
        default:
            i_ret = demux2_vaControlHelper( p_demux->s,
                                            0, -1,
                                            p_sys->i_bitrate_avg, 1, i_query,
                                            args );
            if( !i_ret && p_sys->i_bitrate_avg > 0 &&
                ( i_query == DEMUX_SET_POSITION || i_query == DEMUX_SET_TIME ) )
            {
                int64_t i_time = I64C(8000000) * stream_Tell(p_demux->s) / p_sys->i_bitrate_avg;

                /* fix time_offset */
                if( i_time >= 0 )
                    p_sys->i_time_offset = i_time - p_sys->i_time;
            }
            return i_ret;
    }
}

