/*****************************************************************************
 * ps.c: Program Stream demux module for VLC.
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
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

#include "ps.h"

/* TODO:
 *  - re-add pre-scanning.
 *  - ...
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static int  OpenAlt( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin();
    set_description( _("PS demuxer") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_capability( "demux2", 1 );
    set_callbacks( Open, Close );
    add_shortcut( "ps" );

    add_submodule();
    set_description( _("PS demuxer") );
    set_capability( "demux2", 9 );
    set_callbacks( OpenAlt, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

struct demux_sys_t
{
    ps_psm_t    psm;
    ps_track_t  tk[PS_TK_COUNT];

    int64_t     i_scr;
    int         i_mux_rate;
};

static int Demux  ( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

static int      ps_pkt_resynch( stream_t *, uint32_t *pi_code );
static block_t *ps_pkt_read   ( stream_t *, uint32_t i_code );

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    uint8_t     *p_peek;

    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 )
    {
        msg_Err( p_demux, "cannot peek" );
        return VLC_EGENERIC;
    }

    if( p_peek[0] != 0 || p_peek[1] != 0 ||
        p_peek[2] != 1 || p_peek[3] < 0xb9 )
    {
        msg_Warn( p_demux, "this does not look like an MPEG PS stream, "
                  "continuing anyway" );
    }

    /* Fill p_demux field */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );

    /* Init p_sys */
    p_sys->i_mux_rate = 0;
    p_sys->i_scr      = -1;

    ps_psm_init( &p_sys->psm );
    ps_track_init( p_sys->tk );

    /* TODO prescanning of ES */

    return VLC_SUCCESS;
}

static int OpenAlt( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    uint8_t *p_peek;

    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 )
    {
        msg_Err( p_demux, "cannot peek" );
        return VLC_EGENERIC;
    }

    if( p_peek[0] != 0 || p_peek[1] != 0 ||
        p_peek[2] != 1 || p_peek[3] < 0xb9 )
    {
        if( !p_demux->b_force ) return VLC_EGENERIC;
    }

    return Open( p_this );
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
        msg_Warn( p_demux, "garbage at input" );
        return 1;
    }

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
            ps_track_t *tk = &p_sys->tk[PS_ID_TO_TK(i_id)];

            if( !tk->b_seen )
            {
                if( !ps_track_fill( tk, &p_sys->psm, i_id ) )
                {
                    tk->es = es_out_Add( p_demux->out, &tk->fmt );
                }
                else
                {
                    msg_Dbg( p_demux, "es id=0x%x format unknown", i_id );
                }
                tk->b_seen = VLC_TRUE;
            }

            /* The popular VCD/SVCD subtitling WinSubMux does not
             * renumber the SCRs when merging subtitles into the PES */
            if( tk->b_seen &&
                ( tk->fmt.i_codec == VLC_FOURCC('o','g','t',' ') ||
                  tk->fmt.i_codec == VLC_FOURCC('c','v','d',' ') ) )
            {
                p_sys->i_scr = -1;
            }

            if( p_sys->i_scr > 0 )
                es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_scr );

            p_sys->i_scr = -1;

            if( tk->b_seen && tk->es &&
                !ps_pkt_parse_pes( p_pkt, tk->i_skip ) )
            {
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
                *pf = (double)stream_Tell( p_demux->s ) / (double)i64;
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = (double) va_arg( args, double );
            i64 = stream_Size( p_demux->s );

            es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

            return stream_Seek( p_demux->s, (int64_t)(i64 * f) );

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
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
            if( p_sys->i_mux_rate > 0 )
            {
                *pi64 = (int64_t)1000000 * ( stream_Size( p_demux->s ) / 50 ) /
                    p_sys->i_mux_rate;
                return VLC_SUCCESS;
            }
            *pi64 = 0;
            return VLC_EGENERIC;

        case DEMUX_SET_TIME:
        case DEMUX_GET_FPS:
        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Divers:
 *****************************************************************************/

/* PSResynch: resynch on a system starcode
 *  It doesn't skip more than 512 bytes
 *  -1 -> error, 0 -> not synch, 1 -> ok
 */
static int ps_pkt_resynch( stream_t *s, uint32_t *pi_code )
{
    uint8_t *p_peek;
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
    uint8_t *p_peek;
    int      i_peek = stream_Peek( s, &p_peek, 14 );
    int      i_size = ps_pkt_size( p_peek, i_peek );

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

    return NULL;
}
