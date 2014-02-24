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
#include <vlc_bits.h>

#include "mpeg_parser_helpers.h"

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
static int32_t getFPS( demux_t *, block_t * );

struct demux_sys_t
{
    mtime_t     i_dts;
    es_out_id_t *p_es;

    float       f_force_fps;
    float       f_fps;
    decoder_t *p_packetizer;
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
    p_sys->i_dts       = VLC_TS_0;
    p_sys->f_force_fps = var_CreateGetFloat( p_demux, "hevc-force-fps" );
    if( p_sys->f_force_fps != 0.0f )
    {
        p_sys->f_fps = ( p_sys->f_force_fps < 0.001f )? 0.001f:
            p_sys->f_force_fps;
        msg_Dbg( p_demux, "using %.2f fps", p_sys->f_fps );
    }
    else
        p_sys->f_fps = 0.0f;

    /* Load the hevc packetizer */
    es_format_Init( &fmt, VIDEO_ES, VLC_CODEC_HEVC );
    p_sys->p_packetizer = demux_PacketizerNew( p_demux, &fmt, "hevc" );
    if( !p_sys->p_packetizer )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->p_packetizer->fmt_out.b_packetized = true;
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

            p_block_out->i_dts = p_sys->i_dts;
            p_block_out->i_pts = VLC_TS_INVALID;

            uint8_t nal_type = p_block_out->p_buffer[4] & 0x7E;

            /*Get fps from vps if available and not already forced*/
            if( p_sys->f_fps == 0.0f && nal_type == 0x40 )
            {
                if( getFPS( p_demux, p_block_out) )
                {
                    msg_Err(p_demux,"getFPS failed");
                    return 0;
                }
            }

            /* Update DTS only on VCL NAL*/
            if( nal_type < 0x40 && p_sys->f_fps )
            {
                es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_dts );
                p_sys->i_dts += (int64_t)((double)1000000.0 / p_sys->f_fps);
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


static uint8_t * CreateDecodedNAL( int *pi_ret,
                              const uint8_t *src, int i_src )
{
    uint8_t *dst = malloc( i_src );
    if( !dst )
        return NULL;

    *pi_ret = nal_decode( src, dst, i_src );
    return dst;
}

static int32_t getFPS( demux_t *p_demux, block_t * p_block )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    bs_t bs;
    uint8_t * p_decoded_nal;
    int i_decoded_nal;

    if( p_block->i_buffer < 5 )
        return -1;

    p_decoded_nal = CreateDecodedNAL(&i_decoded_nal,
                                     p_block->p_buffer+4, p_block->i_buffer-4);

    if( !p_decoded_nal )
        return -1;

    bs_init( &bs, p_decoded_nal, i_decoded_nal );
    bs_skip( &bs, 12 );
    int32_t max_sub_layer_minus1 = bs_read( &bs, 3 );
    bs_skip( &bs, 17 );

    hevc_skip_profile_tiers_level( &bs, max_sub_layer_minus1 );

    int32_t vps_sub_layer_ordering_info_present_flag = bs_read1( &bs );
    int32_t i = vps_sub_layer_ordering_info_present_flag? 0 : max_sub_layer_minus1;
    for( ; i <= max_sub_layer_minus1; i++ )
    {
        read_ue( &bs );
        read_ue( &bs );
        read_ue( &bs );
    }
    uint32_t vps_max_layer_id = bs_read( &bs, 6);
    uint32_t vps_num_layer_sets_minus1 = read_ue( &bs );
    bs_skip( &bs, vps_max_layer_id * vps_num_layer_sets_minus1 );

    if( bs_read1( &bs ))
    {
        uint32_t num_units_in_tick = bs_read( &bs, 32 );
        uint32_t time_scale = bs_read( &bs, 32 );
        if( num_units_in_tick )
        {
            p_sys->f_fps = ( (float) time_scale )/( (float) num_units_in_tick );
            msg_Dbg(p_demux,"Using framerate %f fps from VPS", p_sys->f_fps);
        }
        else
        {
            msg_Err( p_demux, "vps_num_units_in_tick null defaulting to 25 fps");
            p_sys->f_fps = 25.0f;
        }
    }
    else
    {
        msg_Err( p_demux, "No timing info in VPS defaulting to 25 fps");
        p_sys->f_fps = 25.0f;
    }
    free(p_decoded_nal);
    return 0;
}
