/*****************************************************************************
 * ps.c: Program Stream demux module for VLC.
 *****************************************************************************
 * Copyright (C) 2004-2009 VLC authors and VideoLAN
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

#include "ps.h"

/* TODO:
 *  - re-add pre-scanning.
 *  - ...
 */

#define TIME_TEXT N_("Trust MPEG timestamps")
#define TIME_LONGTEXT N_("Normally we use the timestamps of the MPEG files " \
    "to calculate position and duration. However sometimes this might not " \
    "be usable. Disable this option to calculate from the bitrate instead." )

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

struct demux_sys_t
{
    ps_psm_t    psm;
    ps_track_t  tk[PS_TK_COUNT];

    int64_t     i_scr;
    int64_t     i_last_scr;
    int         i_mux_rate;
    int64_t     i_length;
    int         i_time_track;
    int64_t     i_current_pts;

    int         i_aob_mlp_count;

    bool  b_lost_sync;
    bool  b_have_pack;
    bool  b_seekable;
};

static int Demux  ( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

static int      ps_pkt_resynch( stream_t *, uint32_t *pi_code );
static block_t *ps_pkt_read   ( stream_t *, uint32_t i_code );

/*****************************************************************************
 * Open
 *****************************************************************************/
static int OpenCommon( vlc_object_t *p_this, bool b_force )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    const uint8_t *p_peek;

    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 )
    {
        msg_Err( p_demux, "cannot peek" );
        return VLC_EGENERIC;
    }

    if( memcmp( p_peek, "\x00\x00\x01", 3 ) || ( p_peek[3] < 0xb9 ) )
    {
        if( !b_force )
            return VLC_EGENERIC;

        msg_Warn( p_demux, "this does not look like an MPEG PS stream, "
                  "continuing anyway" );
    }

    /* Fill p_demux field */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    if( !p_sys ) return VLC_ENOMEM;

    /* Init p_sys */
    p_sys->i_mux_rate = 0;
    p_sys->i_scr      = -1;
    p_sys->i_last_scr = -1;
    p_sys->i_length   = -1;
    p_sys->i_current_pts = (mtime_t) 0;
    p_sys->i_time_track = -1;
    p_sys->i_aob_mlp_count = 0;

    p_sys->b_lost_sync = false;
    p_sys->b_have_pack = false;
    p_sys->b_seekable  = false;

    stream_Control( p_demux->s, STREAM_CAN_SEEK, &p_sys->b_seekable );

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
    return OpenCommon( p_this, ((demux_t *)p_this)->b_force );
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
        if( tk->b_seen )
        {
            es_format_Clean( &tk->fmt );
            if( tk->es ) es_out_Del( p_demux->out, tk->es );
        }
    }

    ps_psm_destroy( &p_sys->psm );

    free( p_sys );
}

static int Demux2( demux_t *p_demux, bool b_end )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_ret, i_id;
    uint32_t i_code;
    block_t *p_pkt;

    i_ret = ps_pkt_resynch( p_demux->s, &i_code );
    if( i_ret < 0 )
    {
        return 0;
    }
    else if( i_ret == 0 )
    {
        if( !p_sys->b_lost_sync )
            msg_Warn( p_demux, "garbage at input, trying to resync..." );

        p_sys->b_lost_sync = true;
        return 1;
    }

    if( p_sys->b_lost_sync ) msg_Warn( p_demux, "found sync code" );
    p_sys->b_lost_sync = false;

    if( ( p_pkt = ps_pkt_read( p_demux->s, i_code ) ) == NULL )
    {
        return 0;
    }
    if( (i_id = ps_pkt_id( p_pkt )) >= 0xc0 )
    {
        ps_track_t *tk = &p_sys->tk[PS_ID_TO_TK(i_id)];
        if( !ps_pkt_parse_pes( p_pkt, tk->i_skip ) && p_pkt->i_pts > VLC_TS_INVALID )
        {
            if( b_end && p_pkt->i_pts > tk->i_last_pts )
            {
                tk->i_last_pts = p_pkt->i_pts;
            }
            else if ( tk->i_first_pts == -1 )
            {
                tk->i_first_pts = p_pkt->i_pts;
            }
        }
    }
    block_Release( p_pkt );
    return 1;
}

static void FindLength( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t i_current_pos = -1, i_size = 0, i_end = 0;

    if( !var_CreateGetBool( p_demux, "ps-trust-timestamps" ) )
        return;

    if( p_sys->i_length == -1 ) /* First time */
    {
        p_sys->i_length = 0;
        /* Check beginning */
        int i = 0;
        i_current_pos = stream_Tell( p_demux->s );
        while( vlc_object_alive (p_demux) && i < 40 && Demux2( p_demux, false ) > 0 ) i++;

        /* Check end */
        i_size = stream_Size( p_demux->s );
        i_end = VLC_CLIP( i_size, 0, 200000 );
        stream_Seek( p_demux->s, i_size - i_end );

        i = 0;
        while( vlc_object_alive (p_demux) && i < 40 && Demux2( p_demux, true ) > 0 ) i++;
        if( i_current_pos >= 0 ) stream_Seek( p_demux->s, i_current_pos );
    }

    /* Find the longest track */
    for( int i = 0; i < PS_TK_COUNT; i++ )
    {
        ps_track_t *tk = &p_sys->tk[i];
        if( tk->i_first_pts >= 0 && tk->i_last_pts > 0 &&
            tk->i_last_pts > tk->i_first_pts )
        {
            int64_t i_length = (int64_t)tk->i_last_pts - tk->i_first_pts;
            if( i_length > p_sys->i_length )
            {
                p_sys->i_length = i_length;
                p_sys->i_time_track = i;
                msg_Dbg( p_demux, "we found a length of: %"PRId64, p_sys->i_length );
            }
        }
    }
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_ret, i_id, i_mux_rate;
    uint32_t i_code;
    block_t *p_pkt;

    i_ret = ps_pkt_resynch( p_demux->s, &i_code );
    if( i_ret < 0 )
    {
        return 0;
    }
    else if( i_ret == 0 )
    {
        if( !p_sys->b_lost_sync )
            msg_Warn( p_demux, "garbage at input, trying to resync..." );

        p_sys->b_lost_sync = true;
        return 1;
    }

    if( p_sys->b_lost_sync ) msg_Warn( p_demux, "found sync code" );
    p_sys->b_lost_sync = false;

    if( p_sys->i_length < 0 && p_sys->b_seekable )
        FindLength( p_demux );

    if( ( p_pkt = ps_pkt_read( p_demux->s, i_code ) ) == NULL )
    {
        return 0;
    }

    switch( i_code )
    {
    case 0x1b9:
        block_Release( p_pkt );
        break;

    case 0x1ba:
        if( !ps_pkt_parse_pack( p_pkt, &p_sys->i_scr, &i_mux_rate ) )
        {
            p_sys->i_last_scr = p_sys->i_scr;
            if( !p_sys->b_have_pack ) p_sys->b_have_pack = true;
            /* done later on to work around bad vcd/svcd streams */
            /* es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_scr ); */
            if( i_mux_rate > 0 ) p_sys->i_mux_rate = i_mux_rate;
        }
        block_Release( p_pkt );
        break;

    case 0x1bb:
        if( !ps_pkt_parse_system( p_pkt, &p_sys->psm, p_sys->tk ) )
        {
            int i;
            for( i = 0; i < PS_TK_COUNT; i++ )
            {
                ps_track_t *tk = &p_sys->tk[i];

                if( tk->b_seen && !tk->es && tk->fmt.i_cat != UNKNOWN_ES )
                {
                    tk->es = es_out_Add( p_demux->out, &tk->fmt );
                }
            }
        }
        block_Release( p_pkt );
        break;

    case 0x1bc:
        if( p_sys->psm.i_version == 0xFFFF )
            msg_Dbg( p_demux, "contains a PSM");

        ps_psm_fill( &p_sys->psm, p_pkt, p_sys->tk, p_demux->out );
        block_Release( p_pkt );
        break;

    default:
        if( (i_id = ps_pkt_id( p_pkt )) >= 0xc0 )
        {
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
            ps_track_t *tk = &p_sys->tk[PS_ID_TO_TK(i_id)];

            if( !tk->b_seen )
            {
                if( !ps_track_fill( tk, &p_sys->psm, i_id ) )
                {
                    tk->es = es_out_Add( p_demux->out, &tk->fmt );
                    b_new = true;
                }
                else
                {
                    msg_Dbg( p_demux, "es id=0x%x format unknown", i_id );
                }
                tk->b_seen = true;
            }

            /* The popular VCD/SVCD subtitling WinSubMux does not
             * renumber the SCRs when merging subtitles into the PES */
            if( tk->b_seen &&
                ( tk->fmt.i_codec == VLC_CODEC_OGT ||
                  tk->fmt.i_codec == VLC_CODEC_CVD ) )
            {
                p_sys->i_scr = -1;
                p_sys->i_last_scr = -1;
            }

            if( p_sys->i_scr >= 0 )
                es_out_Control( p_demux->out, ES_OUT_SET_PCR, VLC_TS_0 + p_sys->i_scr );

            p_sys->i_scr = -1;

            if( tk->b_seen && tk->es &&
                !ps_pkt_parse_pes( p_pkt, tk->i_skip ) )
            {
                if( !b_new && !p_sys->b_have_pack &&
                    (tk->fmt.i_cat == AUDIO_ES) &&
                    (p_pkt->i_pts > VLC_TS_INVALID) )
                {
                    /* A hack to sync the A/V on PES files. */
                    msg_Dbg( p_demux, "force SCR: %"PRId64, p_pkt->i_pts );
                    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_pkt->i_pts );
                }
                if( tk->fmt.i_codec == VLC_CODEC_TELETEXT &&
                    p_pkt->i_pts <= VLC_TS_INVALID && p_sys->i_last_scr >= 0 )
                {
                    /* Teletext may have missing PTS (ETSI EN 300 472 Annexe A)
                     * In this case use the last SCR + 40ms */
                    p_pkt->i_pts = VLC_TS_0 + p_sys->i_last_scr + 40000;
                }

                if( (int64_t)p_pkt->i_pts > p_sys->i_current_pts )
                {
                    p_sys->i_current_pts = (int64_t)p_pkt->i_pts;
                }

                es_out_Send( p_demux->out, tk->es, p_pkt );
            }
            else
            {
                block_Release( p_pkt );
            }
        }
        else
        {
            block_Release( p_pkt );
        }
        break;
    }

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    int64_t i64, *pi64;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
            pf = (double*) va_arg( args, double* );
            i64 = stream_Size( p_demux->s );
            if( i64 > 0 )
            {
                double current = stream_Tell( p_demux->s );
                *pf = current / (double)i64;
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = (double) va_arg( args, double );
            i64 = stream_Size( p_demux->s );
            p_sys->i_current_pts = 0;
            p_sys->i_last_scr = -1;

            return stream_Seek( p_demux->s, (int64_t)(i64 * f) );

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_time_track >= 0 && p_sys->i_current_pts > 0 )
            {
                *pi64 = p_sys->i_current_pts - p_sys->tk[p_sys->i_time_track].i_first_pts;
                return VLC_SUCCESS;
            }
            if( p_sys->i_mux_rate > 0 )
            {
                *pi64 = (int64_t)1000000 * ( stream_Tell( p_demux->s ) / 50 ) /
                    p_sys->i_mux_rate;
                return VLC_SUCCESS;
            }
            *pi64 = 0;
            return VLC_EGENERIC;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_length > 0 )
            {
                *pi64 = p_sys->i_length;
                return VLC_SUCCESS;
            }
            else if( p_sys->i_mux_rate > 0 )
            {
                *pi64 = (int64_t)1000000 * ( stream_Size( p_demux->s ) / 50 ) /
                    p_sys->i_mux_rate;
                return VLC_SUCCESS;
            }
            *pi64 = 0;
            return VLC_EGENERIC;

        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            if( p_sys->i_time_track >= 0 && p_sys->i_current_pts > 0 )
            {
                int64_t i_now = p_sys->i_current_pts - p_sys->tk[p_sys->i_time_track].i_first_pts;
                int64_t i_pos = stream_Tell( p_demux->s );

                if( !i_now )
                    return i64 ? VLC_EGENERIC : VLC_SUCCESS;

                p_sys->i_current_pts = 0;
                p_sys->i_last_scr = -1;
                i_pos *= (float)i64 / (float)i_now;
                stream_Seek( p_demux->s, i_pos );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_FPS:
        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Divers:
 *****************************************************************************/

/* PSResynch: resynch on a system startcode
 *  It doesn't skip more than 512 bytes
 *  -1 -> error, 0 -> not synch, 1 -> ok
 */
static int ps_pkt_resynch( stream_t *s, uint32_t *pi_code )
{
    const uint8_t *p_peek;
    int     i_peek;
    int     i_skip;

    if( stream_Peek( s, &p_peek, 4 ) < 4 )
    {
        return -1;
    }
    if( p_peek[0] == 0 && p_peek[1] == 0 && p_peek[2] == 1 &&
        p_peek[3] >= 0xb9 )
    {
        *pi_code = 0x100 | p_peek[3];
        return 1;
    }

    if( ( i_peek = stream_Peek( s, &p_peek, 512 ) ) < 4 )
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
        if( p_peek[0] == 0 && p_peek[1] == 0 && p_peek[2] == 1 &&
            p_peek[3] >= 0xb9 )
        {
            *pi_code = 0x100 | p_peek[3];
            return stream_Read( s, NULL, i_skip ) == i_skip ? 1 : -1;
        }

        p_peek++;
        i_peek--;
        i_skip++;
    }
    return stream_Read( s, NULL, i_skip ) == i_skip ? 0 : -1;
}

static block_t *ps_pkt_read( stream_t *s, uint32_t i_code )
{
    const uint8_t *p_peek;
    int i_peek = stream_Peek( s, &p_peek, 14 );
    if( i_peek < 4 )
        return NULL;

    int i_size = ps_pkt_size( p_peek, i_peek );
    if( i_size <= 6 && p_peek[3] > 0xba )
    {
        /* Special case, search the next start code */
        i_size = 6;
        for( ;; )
        {
            i_peek = stream_Peek( s, &p_peek, i_size + 1024 );
            if( i_peek <= i_size + 4 )
            {
                return NULL;
            }
            while( i_size <= i_peek - 4 )
            {
                if( p_peek[i_size] == 0x00 && p_peek[i_size+1] == 0x00 &&
                    p_peek[i_size+2] == 0x01 && p_peek[i_size+3] >= 0xb9 )
                {
                    return stream_Block( s, i_size );
                }
                i_size++;
            }
        }
    }
    else
    {
        /* Normal case */
        return stream_Block( s, i_size );
    }

    VLC_UNUSED(i_code);
    return NULL;
}
