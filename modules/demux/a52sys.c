/*****************************************************************************
 * a52.c : Raw a52 Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: a52sys.c,v 1.8 2003/11/13 12:28:34 fenrir Exp $
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
    set_description( _("A52 demuxer" ) );
    set_capability( "demux", 100 );
    set_callbacks( Open, Close );
    add_shortcut( "a52" );
vlc_module_end();

/* TODO:
 *
 */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Demux       ( input_thread_t * );

struct demux_sys_t
{
    mtime_t         i_time;

    es_out_id_t     *p_es;
};

static inline int HeaderCheck( const uint8_t * p )
{
    if( (p[0] != 0x0b) || (p[1] != 0x77) || /* syncword */
        (p[5] >= 0x60) || /* bsid >= 12 */
        (p[4] & 63) >= 38  || /* frmsizecod */
        ( (p[4] & 0xc0) != 0 && (p[4] & 0xc0) != 0x40 && (p[4] & 0xc0) != 0x80 ) )
    {
        return VLC_FALSE;
    }
    return VLC_TRUE;
}

static int HeaderInfo( const uint8_t * p,
                       int *pi_channels,
                       int *pi_sample_rate,
                       int *pi_frame_size );

/*****************************************************************************
 * Open: initializes A52 demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys;
    int            b_forced = VLC_FALSE;

    uint8_t        *p_peek;

    module_t       *p_id3;

    es_format_t    fmt;


    if( p_input->psz_demux && !strncmp( p_input->psz_demux, "a52", 3 ) )
    {
        b_forced = VLC_TRUE;
    }
    if( p_input->psz_name )
    {
        int  i_len = strlen( p_input->psz_name );

        if( i_len > 4 && !strcasecmp( &p_input->psz_name[i_len - 4], ".a52" ) )
        {
            b_forced = VLC_TRUE;
        }
    }

    /* skip possible id3 header */
    p_id3 = module_Need( p_input, "id3", NULL );
    if ( p_id3 )
    {
        module_Unneed( p_input, p_id3 );
    }

    /* see if it could be 52 */
    if( !b_forced )
    {
        if( input_Peek( p_input, &p_peek, 6 ) < 6 )
        {
            msg_Err( p_input, "cannot peek" );
            return VLC_EGENERIC;
        }
        if( !HeaderCheck( p_peek ) )
        {
            msg_Warn( p_input, "A52 module discarded" );
            return VLC_EGENERIC;
        }
    }

    p_input->pf_demux = Demux;
    p_input->pf_demux_control = demux_vaControlDefault;

    p_input->p_demux_data = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->i_time = 0;

    if( stream_Peek( p_input->s, &p_peek, 6 ) < 6 )
    {
        msg_Err( p_input, "cannot peek" );
        goto error;
    }

    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 'a', '5', '2', ' ' ) );
    if( HeaderCheck( p_peek ) )
    {
        int i_frame_size;

        HeaderInfo( p_peek, &fmt.audio.i_channels, &fmt.audio.i_samplerate, &i_frame_size );

        msg_Dbg( p_input, "a52 channels=%d sample_rate=%d",
                 fmt.audio.i_channels, fmt.audio.i_samplerate );
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        goto error;
    }
    p_input->stream.i_mux_rate = 0 / 50;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    p_sys->p_es = es_out_Add( p_input->p_es_out, &fmt );
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
static int Demux( input_thread_t * p_input )
{
    demux_sys_t  *p_sys = p_input->p_demux_data;
    pes_packet_t *p_pes;

    int i_channels, i_sample_rate, i_frame_size;

    uint8_t      *p_peek;

    if( stream_Peek( p_input->s, &p_peek, 6 ) < 6 )
    {
        msg_Warn( p_input, "cannot peek" );
        return 0;
    }

    if( !HeaderCheck( p_peek ) )
    {
        /* we need to resynch */
        vlc_bool_t  b_ok = VLC_FALSE;
        int         i_skip = 0;
        int         i_peek;

        i_peek = stream_Peek( p_input->s, &p_peek, 8096 );
        if( i_peek < 8 )
        {
            msg_Warn( p_input, "cannot peek" );
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

        msg_Warn( p_input, "garbage=%d bytes", i_skip );
        stream_Read( p_input->s, NULL, i_skip );
        return 1;
    }

    HeaderInfo( p_peek, &i_channels, &i_sample_rate, &i_frame_size );

    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_sys->i_time * 9 / 100 );

    if( ( p_pes = stream_PesPacket( p_input->s, i_frame_size ) ) == NULL )
    {
        msg_Warn( p_input, "cannot read data" );
        return 0;
    }

    p_pes->i_dts =
    p_pes->i_pts = input_ClockGetTS( p_input,
                                     p_input->stream.p_selected_program,
                                     p_sys->i_time * 9 / 100 );

    es_out_Send( p_input->p_es_out, p_sys->p_es, p_pes );

    p_sys->i_time += (mtime_t)1000000 *
                     (mtime_t)1536 /
                     (mtime_t)i_sample_rate;
    return( 1 );
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    demux_sys_t    *p_sys = p_input->p_demux_data;

    free( p_sys );
}



/*****************************************************************************
 * SyncInfo: parse A/52 sync info
 *****************************************************************************
 * This code is borrowed from liba52 by Aaron Holtzman & Michel Lespinasse,
 *****************************************************************************/
static int HeaderInfo( const uint8_t * p,
                        int *pi_channels,
                        int *pi_sample_rate,
                        int *pi_frame_size )
{
    static const uint8_t halfrate[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3 };
    static const int rate[] = { 32,  40,  48,  56,  64,  80,  96, 112,
                                128, 160, 192, 224, 256, 320, 384, 448,
                                512, 576, 640 };
    static const uint8_t lfeon[8] = { 0x10, 0x10, 0x04, 0x04,
                                      0x04, 0x01, 0x04, 0x01 };
    int frmsizecod;
    int bitrate;
    int half;
    int acmod;

    if ((p[0] != 0x0b) || (p[1] != 0x77))        /* syncword */
        return VLC_FALSE;

    if (p[5] >= 0x60)                /* bsid >= 12 */
        return VLC_FALSE;
    half = halfrate[p[5] >> 3];

    /* acmod, dsurmod and lfeon */
    acmod = p[6] >> 5;
    if ( (p[6] & 0xf8) == 0x50 )
    {
        /* Dolby surround = stereo + Dolby */
        *pi_channels = 2;
    }
    else
    {
        static const int acmod_to_channels[8] =
        {
            2 /* dual mono */, 1 /* mono */, 2 /* stereo */,
            3 /* 3F */, 3 /* 2f1R */,
            4 /* 3F1R */, 4, /* 2F2R */
            5 /* 3F2R */
        };

        *pi_channels = acmod_to_channels[acmod];
    }

    if ( p[6] & lfeon[acmod] ) (*pi_channels)++;    /* LFE */

    frmsizecod = p[4] & 63;
    if (frmsizecod >= 38)
        return VLC_FALSE;
    bitrate = rate[frmsizecod >> 1];

    switch (p[4] & 0xc0) {
    case 0:
        *pi_sample_rate = 48000 >> half;
        *pi_frame_size = 4 * bitrate;
        return VLC_TRUE;
    case 0x40:
        *pi_sample_rate = 44100 >> half;
        *pi_frame_size = 2 * (320 * bitrate / 147 + (frmsizecod & 1));
        return VLC_TRUE;
    case 0x80:
        *pi_sample_rate = 32000 >> half;
        *pi_frame_size =6 * bitrate;
        return VLC_TRUE;
    default:
        return VLC_FALSE;
    }
}

