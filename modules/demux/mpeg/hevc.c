/*****************************************************************************
 * hevc.c : HEVC Video demuxer
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Denis Charmet <typx@videolan.org>
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
#include <vlc_mtime.h>

#include "../packetizer/hevc_nal.h"
#include "../packetizer/hxxx_nal.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define FPS_TEXT N_("Frames per Second")
#define FPS_LONGTEXT N_("Desired frame rate for the stream.")


vlc_module_begin ()
    set_shortname( "HEVC")
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("HEVC/H.265 video demuxer" ) )
    set_capability( "demux", 0 )
    add_float( "hevc-force-fps", 0.0, FPS_TEXT, FPS_LONGTEXT, true )
    set_callbacks( Open, Close )
    add_shortcut( "hevc" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int Demux( demux_t * );
static int Control( demux_t *, int, va_list );
static int32_t getFPS( demux_t *, uint8_t, block_t * );

struct demux_sys_t
{
    es_out_id_t *p_es;

    date_t      dts;
    unsigned    frame_rate_num;
    unsigned    frame_rate_den;
    decoder_t *p_packetizer;

    /* Only for probing fps */
    hevc_video_parameter_set_t    *rgp_vps[HEVC_VPS_MAX];
};
#define HEVC_BLOCK_SIZE 2048

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

    if( p_peek[0] != 0x00 || p_peek[1] != 0x00 ||
        p_peek[2] != 0x00 || p_peek[3] != 0x01 ||
        (p_peek[4]&0xFE) != 0x40 ) /* VPS & forbidden zero bit*/
    {
        if( !p_demux->b_force )
        {
            msg_Warn( p_demux, "hevc module discarded (no startcode)" );
            return VLC_EGENERIC;
        }

        msg_Err( p_demux, "this doesn't look like a HEVC ES stream, "
                 "continuing anyway" );
    }

    p_demux->p_sys     = p_sys = malloc( sizeof( demux_sys_t ) );

    if( !p_demux->p_sys )
        return VLC_ENOMEM;

    p_sys->p_es        = NULL;
    p_sys->frame_rate_num = p_sys->frame_rate_den = 0;
    float f_force_fps = var_CreateGetFloat( p_demux, "hevc-force-fps" );
    if( f_force_fps != 0.0f )
    {
        if ( f_force_fps < 0.001f ) f_force_fps = 0.001f;
        p_sys->frame_rate_den = 1000;
        p_sys->frame_rate_num = 1000 * f_force_fps;
        date_Init( &p_sys->dts, p_sys->frame_rate_num, p_sys->frame_rate_den );
        msg_Dbg( p_demux, "using %.2f fps", (double) f_force_fps );
    }
    else
        date_Init( &p_sys->dts, 25000, 1000 ); /* Will be overwritten */
    date_Set( &p_sys->dts, VLC_TS_0 );
    memset(p_sys->rgp_vps, 0, sizeof(p_sys->rgp_vps[0]) * HEVC_VPS_MAX);

    /* Load the hevc packetizer */
    es_format_Init( &fmt, VIDEO_ES, VLC_CODEC_HEVC );
    p_sys->p_packetizer = demux_PacketizerNew( p_demux, &fmt, "hevc" );
    if( !p_sys->p_packetizer )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->p_es = es_out_Add( p_demux->out, &p_sys->p_packetizer->fmt_out);
    if( !p_sys->p_es )
    {
        demux_PacketizerDestroy( p_sys->p_packetizer );
        free( p_sys );
        return VLC_ENOMEM;
    }
    p_demux->pf_demux  = Demux;
    p_demux->pf_control= Control;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->p_es )
        es_out_Del( p_demux->out, p_sys->p_es );

    demux_PacketizerDestroy( p_sys->p_packetizer );

    for( unsigned i=0; i<HEVC_VPS_MAX; i++ )
    {
        if( p_sys->rgp_vps[i] )
            hevc_rbsp_release_vps( p_sys->rgp_vps[i] );
    }

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

    if( ( p_block_in = stream_Block( p_demux->s, HEVC_BLOCK_SIZE ) ) == NULL )
    {
        return 0;
    }

    p_block_in->i_dts = VLC_TS_INVALID;
    p_block_in->i_pts = VLC_TS_INVALID;

    while( (p_block_out = p_sys->p_packetizer->pf_packetize( p_sys->p_packetizer, &p_block_in )) )
    {
        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;
            p_block_out->p_next = NULL;

            p_block_out->i_dts = date_Get( &p_sys->dts );
            p_block_out->i_pts = VLC_TS_INVALID;

            uint8_t nal_type = (p_block_out->p_buffer[4] & 0x7E) >> 1;
            uint8_t nal_layer = hevc_getNALLayer(&p_block_out->p_buffer[4]);

            /*Get fps from vps if available and not already forced*/
            if( p_sys->frame_rate_den == 0 &&
               (nal_type == HEVC_NAL_VPS || nal_type == HEVC_NAL_SPS) )
            {
                if( getFPS( p_demux, nal_type, p_block_out) )
                {
                    msg_Err(p_demux,"getFPS failed");
                    return 0;
                }
                else if( p_sys->frame_rate_den )
                {
                    date_Init( &p_sys->dts, p_sys->frame_rate_num, p_sys->frame_rate_den );
                    date_Set( &p_sys->dts, VLC_TS_0 );
                }
            }

            /* Update DTS only on VCL NAL*/
            if( nal_type < HEVC_NAL_VPS && p_sys->frame_rate_den &&
                nal_layer == 0 )  /* Only on base layer */
            {
                es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block_out->i_dts );
                date_Increment( &p_sys->dts, 1 );
            }

            es_out_Send( p_demux->out, p_sys->p_es, p_block_out );

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
    return demux_vaControlHelper( p_demux->s,
                                  0, -1,
                                  0, 1, i_query, args );
}

static int32_t getFPS(demux_t *p_demux, uint8_t i_nal_type, block_t *p_block)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    hevc_sequence_parameter_set_t *p_sps = NULL;
    uint8_t i_id;

    if( !hevc_get_xps_id( p_block->p_buffer, p_block->i_buffer, &i_id ) )
        return -1;

    if( p_sys->rgp_vps[i_id] && i_nal_type == HEVC_NAL_VPS )
        return -1;

    const uint8_t *p_nald = p_block->p_buffer;
    size_t i_nald = p_block->i_buffer;
    if( hxxx_strip_AnnexB_startcode( &p_nald, &i_nald ) )
    {
        if( i_nal_type == HEVC_NAL_VPS )
            p_sys->rgp_vps[i_id] = hevc_decode_vps( p_nald, i_nald, true );
        else
            p_sps = hevc_decode_sps( p_nald, i_nald, true );
    }

    if( p_sps )
    {
        if( !hevc_get_frame_rate( p_sps, (const hevc_video_parameter_set_t **) p_sys->rgp_vps,
                                  &p_sys->frame_rate_num, &p_sys->frame_rate_den ) )
        {
            p_sys->frame_rate_num = 25;
            p_sys->frame_rate_den = 1;
            msg_Warn( p_demux, "No timing info in VPS defaulting to 25 fps");
        }
        else
        {
            msg_Dbg( p_demux,"Using framerate %2.f fps from VPS",
                     (float) p_sys->frame_rate_num / (float) p_sys->frame_rate_den );
            for( unsigned i=0; i<HEVC_VPS_MAX; i++ )
            {
                if( p_sys->rgp_vps[i] )
                {
                    hevc_rbsp_release_vps( p_sys->rgp_vps[i] );
                    p_sys->rgp_vps[i] = NULL;
                }
            }
        }
        hevc_rbsp_release_sps( p_sps );
    }

    return 0;
}
