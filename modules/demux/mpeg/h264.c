/*****************************************************************************
 * h264.c : H264 Video demuxer
 *****************************************************************************
 * Copyright (C) 2002-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_codec.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define FPS_TEXT N_("Frames per Second")
#define FPS_LONGTEXT N_("Desired frame rate for the H264 stream.")


vlc_module_begin ()
    set_shortname( "H264")
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("H264 video demuxer" ) )
    set_capability( "demux", 0 )
    add_float( "h264-fps", 0.0, FPS_TEXT, FPS_LONGTEXT, true )
    set_callbacks( Open, Close )
    add_shortcut( "h264" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct demux_sys_t
{
    es_out_id_t *p_es;

    date_t      dts;
    unsigned    frame_rate_num;
    unsigned    frame_rate_den;

    decoder_t *p_packetizer;
};

static int Demux( demux_t * );
static int Control( demux_t *, int, va_list );

#define H264_PACKET_SIZE 2048

/*****************************************************************************
 * Open: initializes demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    const uint8_t *p_peek;
    es_format_t fmt;

    if( stream_Peek( p_demux->s, &p_peek, 5 ) < 5 ) return VLC_EGENERIC;

    const uint8_t i_nal_type = p_peek[4] & 0x1F;
    const uint8_t i_ref_idc = p_peek[4] & 0x60;
    if( memcmp( p_peek, "\x00\x00\x00\x01", 4 ) ||
       (p_peek[4] & 0x80) || /* reserved 0 */
        i_nal_type == 0 || i_nal_type > 12 ||
       ( !i_ref_idc && (i_nal_type < 6 || i_nal_type == 7 || i_nal_type == 8) ) ||
       (  i_ref_idc && (i_nal_type == 6 || i_nal_type >= 9) )
      )
    {
        if( !p_demux->b_force )
        {
            msg_Warn( p_demux, "h264 module discarded (no startcode)" );
            return VLC_EGENERIC;
        }

        msg_Err( p_demux, "this doesn't look like a H264 ES stream, "
                 "continuing anyway" );
    }

    p_demux->pf_demux  = Demux;
    p_demux->pf_control= Control;
    p_demux->p_sys     = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->p_es        = NULL;
    p_sys->frame_rate_num = 0;
    p_sys->frame_rate_den = 0;
    float f_fps        = var_CreateGetFloat( p_demux, "h264-fps" );
    if( f_fps )
    {
        if ( f_fps < 0.001f ) f_fps = 0.001f;
        p_sys->frame_rate_den = 1000;
        p_sys->frame_rate_num = 1000 * f_fps;
        date_Init( &p_sys->dts, p_sys->frame_rate_num, p_sys->frame_rate_den );
    }
    else
        date_Init( &p_sys->dts, 25000, 1000 );
    date_Set( &p_sys->dts, VLC_TS_0 );

    /* Load the mpegvideo packetizer */
    es_format_Init( &fmt, VIDEO_ES, VLC_CODEC_H264 );
    p_sys->p_packetizer = demux_PacketizerNew( p_demux, &fmt, "h264" );
    if( !p_sys->p_packetizer )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    demux_PacketizerDestroy( p_sys->p_packetizer );
    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block_in, *p_block_out;

    if( ( p_block_in = stream_Block( p_demux->s, H264_PACKET_SIZE ) ) == NULL )
    {
        return 0;
    }

    while( (p_block_out = p_sys->p_packetizer->pf_packetize( p_sys->p_packetizer, &p_block_in )) )
    {
        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;

            p_block_out->p_next = NULL;

            p_block_in->i_dts = date_Get( &p_sys->dts );
            p_block_in->i_pts = VLC_TS_INVALID;

            if( p_sys->p_es == NULL )
            {
                p_sys->p_packetizer->fmt_out.b_packetized = true;
                p_sys->p_es = es_out_Add( p_demux->out, &p_sys->p_packetizer->fmt_out );
                if( !p_sys->p_es )
                    return VLC_DEMUXER_EOF;
            }

            /* h264 packetizer does merge multiple NAL into AU, but slice flag persists */
            bool frame = p_block_out->i_flags & BLOCK_FLAG_TYPE_MASK;
            es_out_Send( p_demux->out, p_sys->p_es, p_block_out );
            if( frame )
            {
                if( !p_sys->frame_rate_den )
                {
                    /* Use packetizer's one */
                    if( p_sys->p_packetizer->fmt_out.video.i_frame_rate_base )
                    {
                        p_sys->frame_rate_num = p_sys->p_packetizer->fmt_out.video.i_frame_rate;
                        p_sys->frame_rate_den = p_sys->p_packetizer->fmt_out.video.i_frame_rate_base;
                    }
                    else
                    {
                        p_sys->frame_rate_num = 25000;
                        p_sys->frame_rate_den = 1000;
                    }
                    date_Init( &p_sys->dts, p_sys->frame_rate_num, p_sys->frame_rate_den );
                    date_Set( &p_sys->dts, VLC_TS_0 );
                    msg_Dbg( p_demux, "using %.2f fps", (double) p_sys->frame_rate_num / p_sys->frame_rate_den );
                }

                es_out_Control( p_demux->out, ES_OUT_SET_PCR, date_Get( &p_sys->dts ) );
                date_Increment( &p_sys->dts, 1 );
            }

            p_block_out = p_next;
        }
    }
    return 1;
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
        return demux_vaControlHelper( p_demux->s,
                                       0, -1,
                                       0, 1, i_query, args );
}

