/*****************************************************************************
 * ps.c: Program Stream demux module for VLC.
 *****************************************************************************
 * Copyright (C) 2004-2009 VLC authors and VideoLAN
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

#include "pes.h"
#include "ps.h"

/* TODO:
 *  - re-add pre-scanning.
 *  - ...
 */

#define TIME_TEXT N_("Trust MPEG timestamps")
#define TIME_LONGTEXT N_("Normally we use the timestamps of the MPEG files " \
    "to calculate position and duration. However sometimes this might not " \
    "be usable. Disable this option to calculate from the bitrate instead." )

#define PS_PACKET_PROBE 3
#define CDXA_HEADER_SIZE 44
#define CDXA_SECTOR_SIZE 2352
#define CDXA_SECTOR_HEADER_SIZE 24

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenForce( vlc_object_t * );
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("MPEG-PS demuxer") )
    set_shortname( N_("PS") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_capability( "demux", 1 )
    set_callbacks( OpenForce, Close )
    add_shortcut( "ps" )

    add_bool( "ps-trust-timestamps", true, TIME_TEXT,
                 TIME_LONGTEXT, true )
        change_safe ()

    add_submodule ()
    set_description( N_("MPEG-PS demuxer") )
    set_capability( "demux", 8 )
    set_callbacks( Open, Close )
    add_shortcut( "ps" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct
{
    ps_psm_t    psm;
    ps_track_t  tk[PS_TK_COUNT];

    vlc_tick_t  i_pack_scr; /* current read pack scr value, temp */
    vlc_tick_t  i_first_scr; /* media offset */
    vlc_tick_t  i_scr; /* committed, current position */
    int64_t     i_scr_track_id;
    int         i_mux_rate;
    vlc_tick_t  i_length;
    int         i_time_track_index;
    vlc_tick_t  i_current_pts;
    uint64_t    i_start_byte;
    uint64_t    i_lastpack_byte;

    int         i_aob_mlp_count;

    bool  b_lost_sync;
    bool  b_have_pack;
    bool  b_bad_scr;
    bool  b_seekable;
    enum
    {
        MPEG_PS = 0,
        CDXA_PS,
        PSMF_PS,
    } format;

    int         current_title;
    int         current_seekpoint;
    unsigned    updates;
} demux_sys_t;

static int Demux  ( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

static int      ps_pkt_resynch( stream_t *, int, bool );
static block_t *ps_pkt_read   ( stream_t * );

/*****************************************************************************
 * Open
 *****************************************************************************/
static int OpenCommon( vlc_object_t *p_this, bool b_force )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    const uint8_t *p_peek;
    ssize_t i_peek = 0;
    ssize_t i_offset = 0;
    ssize_t i_skip = 0;
    unsigned i_max_packets = PS_PACKET_PROBE;
    int format = MPEG_PS;
    int i_mux_rate = 0;
    vlc_tick_t i_length = VLC_TICK_INVALID;

    i_peek = vlc_stream_Peek( p_demux->s, &p_peek, 16 );
    if( i_peek < 16 )
        return VLC_EGENERIC;

    if( !memcmp( p_peek, "PSMF", 4 ) &&
        (GetDWBE( &p_peek[4] ) & 0x30303030) == 0x30303030 )
    {
        i_peek = vlc_stream_Peek( p_demux->s, &p_peek, 100 );
        if( i_peek < 100 )
            return VLC_EGENERIC;
        i_skip = i_offset = GetWBE( &p_peek[10] );
        format = PSMF_PS;
        msg_Info( p_demux, "Detected PSMF-PS header");
        i_mux_rate = GetDWBE( &p_peek[96] );
        if( GetDWBE( &p_peek[86] ) > 0 )
            i_length = vlc_tick_from_samples( GetDWBE( &p_peek[92] ), GetDWBE( &p_peek[86] ));
    }
    else if( !memcmp( p_peek, "RIFF", 4 ) && !memcmp( &p_peek[8], "CDXA", 4 ) )
    {
        format = CDXA_PS;
        i_max_packets = 0; /* We can't probe here */
        i_skip = CDXA_HEADER_SIZE;
        msg_Info( p_demux, "Detected CDXA-PS" );
        /* FIXME: have a proper way to decap CD sectors or make an access stream filter */
    }
    else if( b_force )
    {
        msg_Warn( p_demux, "this does not look like an MPEG PS stream, "
                  "continuing anyway" );
        i_max_packets = 0;
    }

    for( unsigned i=0; i<i_max_packets; i++ )
    {
        if( i_peek < i_offset + 16 )
        {
            i_peek = vlc_stream_Peek( p_demux->s, &p_peek, i_offset + 16 );
            if( i_peek < i_offset + 16 )
                return VLC_EGENERIC;
        }

        const uint8_t startcode[3] = { 0x00, 0x00, 0x01 };
        const uint8_t *p_header = &p_peek[i_offset];
        if( memcmp( p_header, startcode, 3 ) ||
           ( (p_header[3] & 0xB0) != 0xB0 &&
            !(p_header[3] >= 0xC0 && p_header[3] <= 0xEF) &&
              p_header[3] != PS_STREAM_ID_EXTENDED &&
              p_header[3] != PS_STREAM_ID_DIRECTORY ) )
            return VLC_EGENERIC;

        ssize_t i_pessize = ps_pkt_size( p_header, 16 );
        if( i_pessize < 5 )
            return VLC_EGENERIC;
        i_offset += i_pessize;
    }

    if( i_skip > 0 && !p_demux->b_preparsing &&
        vlc_stream_Read( p_demux->s, NULL, i_skip ) != i_skip )
        return VLC_EGENERIC;

    /* Fill p_demux field */
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    if( !p_sys ) return VLC_ENOMEM;

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    /* Init p_sys */
    p_sys->i_mux_rate = i_mux_rate;
    p_sys->i_pack_scr  = VLC_TICK_INVALID;
    p_sys->i_first_scr = VLC_TICK_INVALID;
    p_sys->i_scr = VLC_TICK_INVALID;
    p_sys->i_scr_track_id = 0;
    p_sys->i_length   = i_length;
    p_sys->i_current_pts = VLC_TICK_INVALID;
    p_sys->i_time_track_index = -1;
    p_sys->i_aob_mlp_count = 0;
    p_sys->i_start_byte = i_skip;
    p_sys->i_lastpack_byte = i_skip;

    p_sys->b_lost_sync = false;
    p_sys->b_have_pack = false;
    p_sys->b_bad_scr   = false;
    p_sys->b_seekable  = false;
    p_sys->format      = format;
    p_sys->current_title = 0;
    p_sys->current_seekpoint = 0;
    p_sys->updates = 0;

    vlc_stream_Control( p_demux->s, STREAM_CAN_SEEK, &p_sys->b_seekable );

    ps_psm_init( &p_sys->psm );
    ps_track_init( p_sys->tk );

    /* TODO prescanning of ES */

    return VLC_SUCCESS;
}

static int OpenForce( vlc_object_t *p_this )
{
    return OpenCommon( p_this, true );
}

static int Open( vlc_object_t *p_this )
{
    return OpenCommon( p_this, p_this->force );
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    int i;

    for( i = 0; i < PS_TK_COUNT; i++ )
    {
        ps_track_t *tk = &p_sys->tk[i];
        if( tk->b_configured )
        {
            es_format_Clean( &tk->fmt );
            if( tk->es ) es_out_Del( p_demux->out, tk->es );
        }
    }

    ps_psm_destroy( &p_sys->psm );

    free( p_sys );
}

static int Probe( demux_t *p_demux, bool b_end )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_ret, i_id;
    block_t *p_pkt;

    i_ret = ps_pkt_resynch( p_demux->s, p_sys->format, p_sys->b_have_pack );
    if( i_ret < 0 )
    {
        return VLC_DEMUXER_EOF;
    }
    else if( i_ret == 0 )
    {
        if( !p_sys->b_lost_sync )
            msg_Warn( p_demux, "garbage at input, trying to resync..." );

        p_sys->b_lost_sync = true;
        return VLC_DEMUXER_SUCCESS;
    }

    if( p_sys->b_lost_sync ) msg_Warn( p_demux, "found sync code" );
    p_sys->b_lost_sync = false;

    if( ( p_pkt = ps_pkt_read( p_demux->s ) ) == NULL )
    {
        return VLC_DEMUXER_EOF;
    }

    i_id = ps_pkt_id( p_pkt->p_buffer, p_pkt->i_buffer );
    if( i_id >= 0xc0 )
    {
        ps_track_t *tk = &p_sys->tk[ps_id_to_tk(i_id)];
        if( !ps_pkt_parse_pes( VLC_OBJECT(p_demux), p_pkt, tk->i_skip ) &&
             p_pkt->i_pts != VLC_TICK_INVALID )
        {
            if( b_end && (tk->i_last_pts == VLC_TICK_INVALID || p_pkt->i_pts > tk->i_last_pts) )
            {
                tk->i_last_pts = p_pkt->i_pts;
            }
            else if ( tk->i_first_pts == VLC_TICK_INVALID )
            {
                tk->i_first_pts = p_pkt->i_pts;
            }
        }
    }
    else if( i_id == PS_STREAM_ID_PACK_HEADER )
    {
        vlc_tick_t i_scr; int dummy;
        if( !b_end && !ps_pkt_parse_pack( p_pkt->p_buffer, p_pkt->i_buffer,
                                          &i_scr, &dummy ) )
        {
            if( p_sys->i_first_scr == VLC_TICK_INVALID )
                p_sys->i_first_scr = i_scr;
        }
        p_sys->b_have_pack = true;
    }

    block_Release( p_pkt );
    return VLC_DEMUXER_SUCCESS;
}

static bool FindLength( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t i_current_pos = -1, i_size = 0, i_end = 0;

    if( !var_CreateGetBool( p_demux, "ps-trust-timestamps" ) )
        return true;

    if( p_sys->i_length == VLC_TICK_INVALID ) /* First time */
    {
        p_sys->i_length = VLC_TICK_0;
        /* Check beginning */
        int i = 0;
        i_current_pos = vlc_stream_Tell( p_demux->s );
        while( i < 40 && Probe( p_demux, false ) > 0 ) i++;

        /* Check end */
        i_size = stream_Size( p_demux->s );
        i_end = VLC_CLIP( i_size, 0, 200000 );
        if( vlc_stream_Seek( p_demux->s, i_size - i_end ) == VLC_SUCCESS )
        {
            i = 0;
            while( i < 400 && Probe( p_demux, true ) > 0 ) i++;
            if( i_current_pos >= 0 &&
                vlc_stream_Seek( p_demux->s, i_current_pos ) != VLC_SUCCESS )
                    return false;
        }
        else return false;
    }

    /* Find the longest track */
    for( int i = 0; i < PS_TK_COUNT; i++ )
    {
        ps_track_t *tk = &p_sys->tk[i];
        if( tk->i_first_pts != VLC_TICK_INVALID &&
            tk->i_last_pts > tk->i_first_pts )
        {
            vlc_tick_t i_length = tk->i_last_pts - tk->i_first_pts;
            if( i_length > p_sys->i_length )
            {
                p_sys->i_length = i_length;
                p_sys->i_time_track_index = i;
                msg_Dbg( p_demux, "we found a length of: %"PRId64 "s", SEC_FROM_VLC_TICK(p_sys->i_length) );
            }
        }
    }
    return true;
}

static void NotifyDiscontinuity( ps_track_t *p_tk, es_out_t *out )
{
    bool b_selected;
    for( size_t i = 0; i < PS_TK_COUNT; i++ )
    {
        ps_track_t *tk = &p_tk[i];
        if( tk->es &&
                es_out_Control( out, ES_OUT_GET_ES_STATE, tk->es, &b_selected ) == VLC_SUCCESS
                && b_selected )
        {
            tk->i_next_block_flags |= BLOCK_FLAG_DISCONTINUITY;
        }
    }
}

static void CheckPCR( demux_sys_t *p_sys, es_out_t *out, vlc_tick_t i_scr )
{
    if( p_sys->i_scr != VLC_TICK_INVALID &&
        llabs( p_sys->i_scr - i_scr ) > VLC_TICK_FROM_SEC(1) )
        NotifyDiscontinuity( p_sys->tk, out );
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_ret, i_mux_rate;
    block_t *p_pkt;

    i_ret = ps_pkt_resynch( p_demux->s, p_sys->format, p_sys->b_have_pack );
    if( i_ret < 0 )
    {
        return VLC_DEMUXER_EOF;
    }
    else if( i_ret == 0 )
    {
        if( !p_sys->b_lost_sync )
        {
            msg_Warn( p_demux, "garbage at input from %"PRIu64", trying to resync...",
                                vlc_stream_Tell(p_demux->s) );
            NotifyDiscontinuity( p_sys->tk, p_demux->out );
        }

        p_sys->b_lost_sync = true;
        return VLC_DEMUXER_SUCCESS;
    }

    if( p_sys->b_lost_sync ) msg_Warn( p_demux, "found sync code" );
    p_sys->b_lost_sync = false;

    if( p_sys->i_length == VLC_TICK_INVALID && p_sys->b_seekable )
    {
        if( !FindLength( p_demux ) )
            return VLC_DEMUXER_EGENERIC;
    }

    if( ( p_pkt = ps_pkt_read( p_demux->s ) ) == NULL )
    {
        return VLC_DEMUXER_EOF;
    }

    if( p_pkt->i_buffer < 4 )
    {
        block_Release( p_pkt );
        return VLC_DEMUXER_EGENERIC;
    }

    const uint8_t i_stream_id = p_pkt->p_buffer[3];
    switch( i_stream_id )
    {
    case PS_STREAM_ID_END_STREAM:
    case PS_STREAM_ID_PADDING:
        block_Release( p_pkt );
        break;

    case PS_STREAM_ID_PACK_HEADER:
        if( !ps_pkt_parse_pack( p_pkt->p_buffer, p_pkt->i_buffer,
                                &p_sys->i_pack_scr, &i_mux_rate ) )
        {
            if( p_sys->i_first_scr == VLC_TICK_INVALID )
                p_sys->i_first_scr = p_sys->i_pack_scr;
            CheckPCR( p_sys, p_demux->out, p_sys->i_pack_scr );
            p_sys->i_scr = p_sys->i_pack_scr;
            p_sys->i_lastpack_byte = vlc_stream_Tell( p_demux->s );
            if( !p_sys->b_have_pack ) p_sys->b_have_pack = true;
            /* done later on to work around bad vcd/svcd streams */
            /* es_out_SetPCR( p_demux->out, p_sys->i_scr ); */
            if( i_mux_rate > 0 ) p_sys->i_mux_rate = i_mux_rate;
        }
        block_Release( p_pkt );
        break;

    case PS_STREAM_ID_SYSTEM_HEADER:
        if( !ps_pkt_parse_system( p_pkt->p_buffer, p_pkt->i_buffer,
                                  &p_sys->psm, p_sys->tk ) )
        {
            int i;
            for( i = 0; i < PS_TK_COUNT; i++ )
            {
                ps_track_t *tk = &p_sys->tk[i];

                if( !tk->b_configured && tk->fmt.i_cat != UNKNOWN_ES )
                {
                    if( tk->b_seen )
                        tk->es = es_out_Add( p_demux->out, &tk->fmt );
                     /* else create when seeing packet */
                    tk->b_configured = true;
                }
            }
        }
        block_Release( p_pkt );
        break;

    case PS_STREAM_ID_MAP:
        if( p_sys->psm.i_version == 0xFF )
            msg_Dbg( p_demux, "contains a PSM");

        ps_psm_fill( &p_sys->psm,
                     p_pkt->p_buffer, p_pkt->i_buffer,
                     p_sys->tk, p_demux->out );
        block_Release( p_pkt );
        break;

    default:
        /* Reject non video/audio nor PES */
        if( i_stream_id < 0xC0 || i_stream_id > 0xEF )
        {
            block_Release( p_pkt );
            break;
        }
        /* fallthrough */
    case PS_STREAM_ID_PRIVATE_STREAM1:
    case PS_STREAM_ID_EXTENDED:
        {
            int i_id = ps_pkt_id( p_pkt->p_buffer, p_pkt->i_buffer );
            /* Small heuristic to improve MLP detection from AOB */
            if( i_id == 0xa001 &&
                p_sys->i_aob_mlp_count < 500 )
            {
                p_sys->i_aob_mlp_count++;
            }
            else if( i_id == 0xbda1 &&
                     p_sys->i_aob_mlp_count > 0 )
            {
                p_sys->i_aob_mlp_count--;
                i_id = 0xa001;
            }

            bool b_new = false;
            ps_track_t *tk = &p_sys->tk[ps_id_to_tk(i_id)];

            if( !tk->b_configured )
            {
                if( !ps_track_fill( tk, &p_sys->psm, i_id,
                                    p_pkt->p_buffer, p_pkt->i_buffer, false ) )
                {
                    /* No PSM and no probing yet */
                    if( p_sys->format == PSMF_PS )
                    {
                        if( tk->fmt.i_cat == VIDEO_ES )
                            tk->fmt.i_codec = VLC_CODEC_H264;
#if 0
                        if( i_stream_id == PS_STREAM_ID_PRIVATE_STREAM1 )
                        {
                            es_format_Change( &tk->fmt, AUDIO_ES, VLC_CODEC_ATRAC3P );
                            tk->fmt.audio.i_blockalign = 376;
                            tk->fmt.audio.i_channels = 2;
                            tk->fmt.audio.i_rate = 44100;
                        }
#endif
                    }

                    tk->es = es_out_Add( p_demux->out, &tk->fmt );
                    b_new = true;
                    tk->b_configured = true;
                }
                else
                {
                    msg_Dbg( p_demux, "es id=0x%x format unknown", i_id );
                }
            }

            /* Late creation from system header */
            if( !tk->b_seen && tk->b_configured && !tk->es && tk->fmt.i_cat != UNKNOWN_ES )
                tk->es = es_out_Add( p_demux->out, &tk->fmt );

            tk->b_seen = true;

            /* The popular VCD/SVCD subtitling WinSubMux does not
             * renumber the SCRs when merging subtitles into the PES */
            if( tk->b_seen && !p_sys->b_bad_scr &&
                ( tk->fmt.i_codec == VLC_CODEC_OGT ||
                  tk->fmt.i_codec == VLC_CODEC_CVD ) )
            {
                p_sys->b_bad_scr = true;
                p_sys->i_first_scr = VLC_TICK_INVALID;
            }

            if( p_sys->i_pack_scr != VLC_TICK_INVALID && !p_sys->b_bad_scr )
            {
                if( (tk->fmt.i_cat == AUDIO_ES || tk->fmt.i_cat == VIDEO_ES) &&
                    tk->i_first_pts != VLC_TICK_INVALID && tk->i_first_pts - p_sys->i_pack_scr > VLC_TICK_FROM_SEC(2))
                {
                    msg_Warn( p_demux, "Incorrect SCR timing offset by of %"PRId64 "ms, disabling",
                                       MS_FROM_VLC_TICK(tk->i_first_pts - p_sys->i_pack_scr) );
                    p_sys->b_bad_scr = true; /* Disable Offset SCR */
                    p_sys->i_first_scr = VLC_TICK_INVALID;
                }
                else
                    es_out_SetPCR( p_demux->out, p_sys->i_pack_scr );
            }

            if( tk->b_configured && tk->es &&
                !ps_pkt_parse_pes( VLC_OBJECT(p_demux), p_pkt, tk->i_skip ) )
            {
                if( tk->fmt.i_cat == AUDIO_ES || tk->fmt.i_cat == VIDEO_ES )
                {
                    if( !p_sys->b_bad_scr && p_sys->i_pack_scr != VLC_TICK_INVALID && p_pkt->i_pts != VLC_TICK_INVALID &&
                        p_sys->i_pack_scr > p_pkt->i_pts + VLC_TICK_FROM_MS(250) )
                    {
                        msg_Warn( p_demux, "Incorrect SCR timing in advance of %" PRId64 "ms, disabling",
                                           MS_FROM_VLC_TICK(p_sys->i_pack_scr - p_pkt->i_pts) );
                        p_sys->b_bad_scr = true;
                        p_sys->i_first_scr = VLC_TICK_INVALID;
                    }

                    if( (p_sys->b_bad_scr || !p_sys->b_have_pack) && !p_sys->i_scr_track_id )
                    {
                        p_sys->i_scr_track_id = tk->i_id;
                    }
                }

                if( ((!b_new && !p_sys->b_have_pack) || p_sys->b_bad_scr) &&
                    p_sys->i_scr_track_id == tk->i_id &&
                    p_pkt->i_pts != VLC_TICK_INVALID )
                {
                    /* A hack to sync the A/V on PES files. */
                    msg_Dbg( p_demux, "force SCR: %"PRId64, p_pkt->i_pts );
                    CheckPCR( p_sys, p_demux->out, p_pkt->i_pts );
                    p_sys->i_scr = p_pkt->i_pts;
                    if( p_sys->i_first_scr == VLC_TICK_INVALID )
                        p_sys->i_first_scr = p_sys->i_scr;
                    es_out_SetPCR( p_demux->out, p_pkt->i_pts );
                }

                if( tk->fmt.i_codec == VLC_CODEC_TELETEXT &&
                    p_pkt->i_pts == VLC_TICK_INVALID && p_sys->i_scr != VLC_TICK_INVALID )
                {
                    /* Teletext may have missing PTS (ETSI EN 300 472 Annexe A)
                     * In this case use the last SCR + 40ms */
                    p_pkt->i_pts = p_sys->i_scr + VLC_TICK_FROM_MS(40);
                }

                if( p_pkt->i_pts > p_sys->i_current_pts )
                {
                    p_sys->i_current_pts = p_pkt->i_pts;
                }

                if( tk->i_next_block_flags )
                {
                    p_pkt->i_flags = tk->i_next_block_flags;
                    tk->i_next_block_flags = 0;
                }
#if 0
                if( tk->fmt.i_codec == VLC_CODEC_ATRAC3P )
                {
                    p_pkt->p_buffer += 14;
                    p_pkt->i_buffer -= 14;
                }
#endif
                es_out_Send( p_demux->out, tk->es, p_pkt );
            }
            else
            {
                block_Release( p_pkt );
            }

            p_sys->i_pack_scr = VLC_TICK_INVALID;
        }
        break;
    }

    demux_UpdateTitleFromStream( p_demux );
    return VLC_DEMUXER_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    int64_t i64;
    int i_ret;

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            *va_arg( args, bool * ) = p_sys->b_seekable;
            return VLC_SUCCESS;

        case DEMUX_GET_TITLE:
            *va_arg( args, int * ) = p_sys->current_title;
            return VLC_SUCCESS;

        case DEMUX_GET_SEEKPOINT:
            *va_arg( args, int * ) = p_sys->current_seekpoint;
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            pf = va_arg( args, double * );
            i64 = stream_Size( p_demux->s ) - p_sys->i_start_byte;
            if( i64 > 0 )
            {
                double current = vlc_stream_Tell( p_demux->s ) - p_sys->i_start_byte;
                *pf = current / (double)i64;
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = va_arg( args, double );
            i64 = stream_Size( p_demux->s ) - p_sys->i_start_byte;
            p_sys->i_current_pts = VLC_TICK_INVALID;
            p_sys->i_scr = VLC_TICK_INVALID;

            if( p_sys->format == CDXA_PS )
            {
                i64 = (int64_t)(i64  * f); /* Align to sector payload */
                i64 = p_sys->i_start_byte + i64 - (i64 % CDXA_SECTOR_SIZE) + CDXA_SECTOR_HEADER_SIZE;
            }
            else
            {
                i64 = p_sys->i_start_byte + (int64_t)(i64 * f);
            }

            i_ret = vlc_stream_Seek( p_demux->s, i64 );
            if( i_ret == VLC_SUCCESS )
            {
                NotifyDiscontinuity( p_sys->tk, p_demux->out );
                return i_ret;
            }
            break;

        case DEMUX_GET_TIME:
            if( p_sys->i_time_track_index >= 0 && p_sys->i_current_pts != VLC_TICK_INVALID )
            {
                *va_arg( args, vlc_tick_t * ) = p_sys->i_current_pts - p_sys->tk[p_sys->i_time_track_index].i_first_pts;
                return VLC_SUCCESS;
            }
            if( p_sys->i_first_scr != VLC_TICK_INVALID && p_sys->i_scr != VLC_TICK_INVALID )
            {
                vlc_tick_t i_time = p_sys->i_scr - p_sys->i_first_scr;
                /* H.222 2.5.2.2 */
                if( p_sys->i_mux_rate > 0 && p_sys->b_have_pack )
                {
                    uint64_t i_offset = vlc_stream_Tell( p_demux->s ) - p_sys->i_lastpack_byte;
                    i_time += vlc_tick_from_samples(i_offset, p_sys->i_mux_rate * 50);
                }
                *va_arg( args, vlc_tick_t * ) = i_time;
                return VLC_SUCCESS;
            }
            *va_arg( args, vlc_tick_t * ) = 0;
            break;

        case DEMUX_GET_LENGTH:
            if( p_sys->i_length > VLC_TICK_0 )
            {
                *va_arg( args, vlc_tick_t * ) = p_sys->i_length;
                return VLC_SUCCESS;
            }
            else if( p_sys->i_mux_rate > 0 )
            {
                *va_arg( args, vlc_tick_t * ) = vlc_tick_from_samples( stream_Size( p_demux->s ) - p_sys->i_start_byte / 50,
                    p_sys->i_mux_rate );
                return VLC_SUCCESS;
            }
            *va_arg( args, vlc_tick_t * ) = 0;
            break;

        case DEMUX_SET_TIME:
        {
            if( p_sys->i_time_track_index >= 0 && p_sys->i_current_pts != VLC_TICK_INVALID &&
                p_sys->i_length > VLC_TICK_0)
            {
                vlc_tick_t i_time = va_arg( args, vlc_tick_t );
                i_time -= p_sys->tk[p_sys->i_time_track_index].i_first_pts;
                return demux_Control( p_demux, DEMUX_SET_POSITION, (double) i_time / p_sys->i_length );
            }
            break;
        }

        case DEMUX_GET_TITLE_INFO:
        {
            struct input_title_t ***v = va_arg( args, struct input_title_t*** );
            int *c = va_arg( args, int * );

            *va_arg( args, int* ) = 0; /* Title offset */
            *va_arg( args, int* ) = 0; /* Chapter offset */
            return vlc_stream_Control( p_demux->s, STREAM_GET_TITLE_INFO, v,
                                       c );
        }

        case DEMUX_SET_TITLE:
            return vlc_stream_vaControl( p_demux->s, STREAM_SET_TITLE, args );

        case DEMUX_SET_SEEKPOINT:
            return vlc_stream_vaControl( p_demux->s, STREAM_SET_SEEKPOINT,
                                         args );

        case DEMUX_TEST_AND_CLEAR_FLAGS:
        {
            unsigned *restrict flags = va_arg(args, unsigned *);
            *flags &= p_sys->updates;
            p_sys->updates &= ~*flags;
            return VLC_SUCCESS;
        }

        case DEMUX_GET_META:
            return vlc_stream_vaControl( p_demux->s, STREAM_GET_META, args );

        case DEMUX_GET_FPS:
            break;

        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_GET_PTS_DELAY:
            return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );

        default:
            break;

    }
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Divers:
 *****************************************************************************/

/* PSResynch: resynch on a system startcode
 *  It doesn't skip more than 512 bytes
 *  -1 -> error, 0 -> not synch, 1 -> ok
 */
static int ps_pkt_resynch( stream_t *s, int format, bool b_pack )
{
    const uint8_t *p_peek;
    int     i_peek;
    int     i_skip;

    if( vlc_stream_Peek( s, &p_peek, 4 ) < 4 )
    {
        return -1;
    }
    if( p_peek[0] == 0 && p_peek[1] == 0 && p_peek[2] == 1 &&
        p_peek[3] >= PS_STREAM_ID_END_STREAM )
    {
        return 1;
    }

    if( ( i_peek = vlc_stream_Peek( s, &p_peek, 512 ) ) < 4 )
    {
        return -1;
    }
    i_skip = 0;

    for( ;; )
    {
        if( i_peek < 4 )
        {
            break;
        }
        /* Handle mid stream 24 bytes padding+CRC creating emulated sync codes with incorrect
           PES sizes and frelling up to UINT16_MAX bytes followed by 24 bytes CDXA Header */
        if( format == CDXA_PS && i_skip == 0 && i_peek >= 48 )
        {
            const uint8_t cdxasynccode[12] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
                                               0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
            if( !memcmp( &p_peek[24], cdxasynccode, 12 ) )
            {
                i_peek -= 48;
                p_peek += 48;
                i_skip += 48;
                continue;
            }
        }

        if( p_peek[0] == 0 && p_peek[1] == 0 && p_peek[2] == 1 &&
            p_peek[3] >= PS_STREAM_ID_END_STREAM &&
            ( !b_pack || p_peek[3] == PS_STREAM_ID_PACK_HEADER ) )
        {
            return vlc_stream_Read( s, NULL, i_skip ) == i_skip ? 1 : -1;
        }

        p_peek++;
        i_peek--;
        i_skip++;
    }
    return vlc_stream_Read( s, NULL, i_skip ) == i_skip ? 0 : -1;
}

static block_t *ps_pkt_read( stream_t *s )
{
    const uint8_t *p_peek;
    int i_peek = vlc_stream_Peek( s, &p_peek, 14 );
    if( i_peek < 4 )
        return NULL;

    int i_size = ps_pkt_size( p_peek, i_peek );
    if( i_size <= 6 && p_peek[3] > PS_STREAM_ID_PACK_HEADER )
    {
        /* Special case, search the next start code */
        i_size = 6;
        for( ;; )
        {
            i_peek = vlc_stream_Peek( s, &p_peek, i_size + 1024 );
            if( i_peek <= i_size + 4 )
            {
                return NULL;
            }
            while( i_size <= i_peek - 4 )
            {
                if( p_peek[i_size] == 0x00 && p_peek[i_size+1] == 0x00 &&
                    p_peek[i_size+2] == 0x01 && p_peek[i_size+3] >= PS_STREAM_ID_END_STREAM )
                {
                    return vlc_stream_Block( s, i_size );
                }
                i_size++;
            }
        }
    }
    else
    {
        /* Normal case */
        return vlc_stream_Block( s, i_size );
    }

    return NULL;
}
