/*****************************************************************************
 * mpga.c : MPEG-I/II Audio input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: mpga.c,v 1.1 2003/08/01 00:37:06 fenrir Exp $
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

#include <ninput.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin();
    set_description( _("MPEG-I/II Audio demuxer" ) );
    set_capability( "demux", 100 );
    set_callbacks( Open, Close );
    add_shortcut( "mpga" );
    add_shortcut( "mp3" );
vlc_module_end();

/* TODO:
 * - mpeg 2.5
 * - free bitrate
 */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Demux       ( input_thread_t * );

struct demux_sys_t
{
    stream_t        *s;
    mtime_t         i_time;

    int             i_bitrate_avg;  /* extracted from Xing header */
    es_descriptor_t *p_es;
};


static inline uint32_t GetDWBE( uint8_t *p )
{
    return( ( p[0] << 24 )|( p[1] << 16 )|( p[2] <<  8 )|( p[3] ) );
}

static int HeaderCheck( uint32_t h )
{
    if( ((( h >> 20 )&0x0FFF) != 0x0FFF )  /* header sync */
        || (((h >> 17)&0x03) == 0 )  /* valid layer ?*/
        || (((h >> 12)&0x0F) == 0x0F )
        || (((h >> 12)&0x0F) == 0x00 ) /* valid bitrate ? */
        || (((h >> 10) & 0x03) == 0x03 ) /* valide sampling freq ? */
        || ((h & 0x03) == 0x02 )) /* valid emphasis ? */
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
#define MPGA_SAMPLE_RATE(h) mpga_sample_rate[MPGA_VERSION(h)][((h)>>10)&0x03]
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

#if 0
static int CheckPS( input_thread_t *p_input )
{
    uint8_t  *p_peek;
    int i_startcode = 0;
    int i_size = input_Peek( p_input, &p_peek, 8196 );

    while( i_size > 4 )
    {
        if( ( p_peek[0] == 0 ) && ( p_peek[1] == 0 ) &&
            ( p_peek[2] == 1 ) && ( p_peek[3] >= 0xb9 ) &&
            ++i_startcode >= 3 )
        {
            return 1;
        }
        p_peek++;
        i_size--;
    }

    return 0;
}
#endif

/*****************************************************************************
 * Open: initializes demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys;
    vlc_bool_t     b_forced = VLC_FALSE;
    vlc_bool_t     b_extention = VLC_FALSE;

    uint32_t       header;

    uint8_t        *p_peek;

    module_t       *p_id3;


    if( p_input->psz_demux &&
        ( !strncmp( p_input->psz_demux, "mpga", 4 ) ||
          !strncmp( p_input->psz_demux, "mp3", 3 ) ) )
    {
        b_forced = VLC_TRUE;
    }
    if( p_input->psz_name )
    {
        int  i_len = strlen( p_input->psz_name );

        if( i_len > 4 && !strcasecmp( &p_input->psz_name[i_len - 4], ".mp3" ) )
        {
            b_extention = VLC_TRUE;
        }
    }

    /* skip possible id3 header */
    p_id3 = module_Need( p_input, "id3", NULL );
    if ( p_id3 )
    {
        module_Unneed( p_input, p_id3 );
    }

    if( input_Peek( p_input, &p_peek, 4 ) < 4 )
    {
        msg_Err( p_input, "cannot peek" );
        return VLC_EGENERIC;
    }

    if( !HeaderCheck( header = GetDWBE( p_peek ) ) )
    {
        vlc_bool_t b_ok = VLC_FALSE;
        int i_peek;

        if( !b_forced && !b_extention )
        {
            msg_Warn( p_input, "mpga module discarded" );
            return VLC_EGENERIC;
        }

        i_peek = input_Peek( p_input, &p_peek, 8096 );

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
            msg_Warn( p_input, "mpga module discarded" );
            return VLC_EGENERIC;
        }
    }

    p_input->pf_demux = Demux;

    p_input->p_demux_data = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->i_time = 0;
    p_sys->i_bitrate_avg = 0;

    if( ( p_sys->s = stream_OpenInput( p_input ) ) == NULL )
    {
        msg_Err( p_input, "cannot create stream" );
        goto error;
    }

    if( HeaderCheck( header ) )
    {
        int     i_xing;
        uint8_t *p_xing;

        input_info_category_t * p_cat;
        static char* mpga_mode[4] =
        {
            "stereo", "joint stereo", "dual channel", "mono"
        };

        p_sys->i_bitrate_avg = MPGA_BITRATE( header ) * 1000;
        if( ( i_xing = stream_Peek( p_sys->s, &p_xing, 1024 ) ) >= 21 )
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
                if( i_flags&0x08 && i_skip + 4 <= i_xing )   /* XING_VBR */
                {
                    p_sys->i_bitrate_avg = GetDWBE( &p_xing[i_skip] );
                    msg_Dbg( p_input, "xing vbr value present (%d)", p_sys->i_bitrate_avg );
                }
                else if( i_frames > 0 && i_bytes > 0 )
                {
                    p_sys->i_bitrate_avg = (int64_t)i_bytes *
                                           (int64_t)8 *
                                           (int64_t)MPGA_SAMPLE_RATE( header ) /
                                           (int64_t)i_frames /
                                           (int64_t)mpga_frame_samples( header );
                    msg_Dbg( p_input, "xing frames&bytes value present (%db/s)", p_sys->i_bitrate_avg );
                }
            }
        }

        msg_Dbg( p_input, "version=%d layer=%d channels=%d samplerate=%d",
                 MPGA_VERSION( header) + 1,
                 MPGA_LAYER( header ) + 1,
                 MPGA_CHANNELS( header ),
                 MPGA_SAMPLE_RATE( header ) );

        vlc_mutex_lock( &p_input->stream.stream_lock );

        p_cat = input_InfoCategory( p_input, _("MPEG") );
        input_AddInfo( p_cat, _("Input Type"), "Audio MPEG-%d",
                       MPGA_VERSION( header) + 1 );
        input_AddInfo( p_cat, _("Layer"), "%d",
                       MPGA_LAYER( header ) + 1 );
        input_AddInfo( p_cat, _("Mode"),
                       mpga_mode[MPGA_MODE( header )] );
        input_AddInfo( p_cat, _("Sample Rate"), "%dHz",
                       MPGA_SAMPLE_RATE( header ) );
        input_AddInfo( p_cat, _("Average Bitrate"), "%dKb/s",
                       p_sys->i_bitrate_avg / 1000 );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        goto error;
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot add program" );
        goto error;
    }
    p_input->stream.pp_programs[0]->b_is_ok = 0;
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];

    p_input->stream.i_mux_rate = p_sys->i_bitrate_avg / 8 / 50;

    p_sys->p_es = input_AddES( p_input,
                               p_input->stream.p_selected_program,
                               1 , AUDIO_ES, NULL, 0 );

    p_sys->p_es->i_stream_id = 1;
    p_sys->p_es->i_fourcc = VLC_FOURCC( 'm', 'p', 'g', 'a' );
    input_SelectES( p_input, p_sys->p_es );

    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return VLC_SUCCESS;

error:
    if( p_sys->s )
    {
        stream_Release( p_sys->s );
    }
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

    uint32_t     header;
    uint8_t      *p_peek;

    if( stream_Peek( p_sys->s, &p_peek, 4 ) < 4 )
    {
        msg_Warn( p_input, "cannot peek" );
        return 0;
    }

    if( !HeaderCheck( header = GetDWBE( p_peek ) ) )
    {
        /* we need to resynch */
        vlc_bool_t  b_ok = VLC_FALSE;
        int         i_skip = 0;
        int         i_peek;

        i_peek = stream_Peek( p_sys->s, &p_peek, 8096 );
        if( i_peek < 4 )
        {
            msg_Warn( p_input, "cannot peek" );
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

        msg_Warn( p_input, "garbage=%d bytes", i_skip );
        stream_Read( p_sys->s, NULL, i_skip );
        return 1;
    }

    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_sys->i_time * 9 / 100 );

    if( ( p_pes = stream_PesPacket( p_sys->s, mpga_frame_size( header ) ) ) == NULL )
    {
        msg_Warn( p_input, "cannot read data" );
        return 0;
    }

    p_pes->i_dts =
    p_pes->i_pts = input_ClockGetTS( p_input,
                                     p_input->stream.p_selected_program,
                                     p_sys->i_time * 9 / 100 );

    if( !p_sys->p_es->p_decoder_fifo )
    {
        msg_Err( p_input, "no audio decoder" );
        input_DeletePES( p_input->p_method_data, p_pes );
        return( -1 );
    }

    input_DecodePES( p_sys->p_es->p_decoder_fifo, p_pes );
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
    input_thread_t *p_input = (input_thread_t*)p_this;
    demux_sys_t    *p_sys = p_input->p_demux_data;

    if( p_sys->s )
    {
        stream_Release( p_sys->s );
    }
    free( p_sys );
}

