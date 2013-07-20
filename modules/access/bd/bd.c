/*****************************************************************************
 * bd.c: BluRay Disc support (uncrypted)
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <limits.h>
#include <sys/stat.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_demux.h>
#include <vlc_fs.h>
#include <vlc_bits.h>
#include <assert.h>

#include "mpls.h"
#include "clpi.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("BD") )
    set_description( N_("Blu-ray Disc Input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    set_capability( "access_demux", 60 )
    add_shortcut( "bd", "file" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Documentation
 * I have used:
 *  - http://www.stebbins.biz/source/bdtools.tgz
 *  - hdcookbook java code
 *  - BDInfo source code
 *****************************************************************************/

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct demux_sys_t
{
    char *psz_base;
    bool b_shortname;

    /* */
    int       i_mpls;
    bd_mpls_t **pp_mpls;

    /* */
    int       i_clpi;
    bd_clpi_t **pp_clpi;

    /* */
    int             i_title;
    input_title_t   **pp_title;

    /* */
    es_out_t        *p_out;

    /* Current state */
    const bd_clpi_t *p_clpi;
    int             i_clpi_ep;
    stream_t        *p_parser;
    stream_t        *p_m2ts;
    int             i_play_item;
    int             i_packet;
    int             i_packet_start;
    int             i_packet_stop;
    int             i_packet_headers;
    int64_t         i_atc_initial;
    int64_t         i_atc_current;
    int64_t         i_atc_wrap;
    int64_t         i_atc_last;
};

static int Control( demux_t *, int, va_list );
static int Demux( demux_t * );

static char *FindPathBase( const char *, bool *pb_shortname );

static int LoadPlaylist( demux_t * );
static int LoadClip( demux_t * );

static void ReorderPlaylist( demux_t * );

static void InitTitles( demux_t * );
static int  SetTitle( demux_t *, int );
static int  SetChapter( demux_t *, int );
static int64_t GetTime( demux_t * );
static double  GetPosition( demux_t * );
static int     SetTime( demux_t *, int64_t );
static int     SetPosition( demux_t *, double );

static int SetPlayItem( demux_t *p_demux, int i_mpls, int i_play_item );
static void ClosePlayItem( demux_t * );

/* */
static int64_t GetClpiPacket( demux_t *p_demux, int *pi_ep, const bd_mpls_clpi_t *p_mpls_clpi, int64_t i_time /* in 45kHz */ );

static es_out_t *EsOutNew( demux_t *p_demux );

//#define BD_DEBUG

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    if( p_demux->psz_file == NULL )
        return VLC_EGENERIC;
    if( *p_demux->psz_access &&
        strcmp( p_demux->psz_access, "bd" ) &&
        strcmp( p_demux->psz_access, "file" ) )
        return VLC_EGENERIC;

    /* */
    bool b_shortname;
    char *psz_base = FindPathBase( p_demux->psz_file, &b_shortname );
    if( !psz_base )
        return VLC_EGENERIC;

    msg_Dbg( p_demux, "Using path '%s'", psz_base );

    /* Fill p_demux field */
    p_demux->p_sys = p_sys = malloc( sizeof(*p_sys) );
    if( !p_sys )
        return VLC_EGENERIC;
    p_sys->psz_base = psz_base;
    p_sys->b_shortname = b_shortname;
    TAB_INIT( p_sys->i_mpls, p_sys->pp_mpls );
    TAB_INIT( p_sys->i_clpi, p_sys->pp_clpi );
    TAB_INIT( p_sys->i_title, p_sys->pp_title );
    p_demux->info.i_title = -1;
    p_sys->p_clpi = NULL;
    p_sys->i_clpi_ep = -1;
    p_sys->p_parser = NULL;
    p_sys->p_m2ts = NULL;
    p_sys->i_play_item = -1;
    p_sys->i_packet = -1;
    p_sys->i_packet_start = -1;
    p_sys->i_packet_stop = -1;
    p_sys->i_packet_headers = -1;
    p_sys->p_out = EsOutNew( p_demux );
    if( !p_sys->p_out )
        goto error;

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;

    /* Load all clip/playlist files */
    LoadClip( p_demux );
    LoadPlaylist( p_demux );

    /* Reorder playlist to have the most significant first
     * (as we don't have menu support, no idea how to find the main title */
    ReorderPlaylist( p_demux );

    /* Setup variables (for TS demuxer) */
    var_Create( p_demux, "ts-es-id-pid", VLC_VAR_BOOL );
    var_SetBool( p_demux, "ts-es-id-pid", true );

    /* */
    InitTitles( p_demux );
    if( SetTitle( p_demux, 0 ) )
        goto error;

    return VLC_SUCCESS;

error:
    Close( VLC_OBJECT(p_demux) );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    /* */
    ClosePlayItem( p_demux );

    /* */
    es_out_Delete( p_sys->p_out );

    /* Titles */
    for( int i = 0; i < p_sys->i_title; i++ )
        vlc_input_title_Delete( p_sys->pp_title[i] );
    TAB_CLEAN( p_sys->i_title, p_sys->pp_title );

    /* CLPI */
    for( int i = 0; i < p_sys->i_clpi; i++ )
    {
        bd_clpi_t *p_clpi = p_sys->pp_clpi[i];

        bd_clpi_Clean( p_clpi );
        free( p_clpi );
    }
    TAB_CLEAN( p_sys->i_clpi, p_sys->pp_clpi );

    /* MPLS */
    for( int i = 0; i < p_sys->i_mpls; i++ )
    {
        bd_mpls_t *p_mpls = p_sys->pp_mpls[i];

        bd_mpls_Clean( p_mpls );
        free( p_mpls );
    }
    TAB_CLEAN( p_sys->i_mpls, p_sys->pp_mpls );

    free( p_sys->psz_base );
    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    switch( i_query )
    {
    case DEMUX_GET_TIME:
    {
        int64_t *pi_time = (int64_t*)va_arg( args, int64_t * );
        *pi_time = GetTime( p_demux );
        return VLC_SUCCESS;;
    }

    case DEMUX_GET_POSITION:
    {
        double *pf_position = (double*)va_arg( args, double * );
        *pf_position = GetPosition( p_demux );
        return VLC_SUCCESS;
    }

    case DEMUX_SET_TIME:
    {
        int64_t i_time = (int64_t)va_arg( args, int64_t );
        return SetTime( p_demux, i_time );
    }
    case DEMUX_SET_POSITION:
    {
        double f_position = (double)va_arg( args, double );
        return SetPosition( p_demux, f_position );
    }

    case DEMUX_GET_LENGTH:
    {
        int64_t *pi_length = (int64_t*)va_arg( args, int64_t * );
        *pi_length = p_sys->pp_title[p_demux->info.i_title]->i_length;
        return VLC_SUCCESS;
    }

    /* Special for access_demux */
    case DEMUX_CAN_PAUSE:
    case DEMUX_CAN_SEEK:
    case DEMUX_CAN_CONTROL_PACE:
    {
        bool *pb_bool = (bool*)va_arg( args, bool * );
        *pb_bool = true;
        return VLC_SUCCESS;
    }

    case DEMUX_SET_PAUSE_STATE:
        return VLC_SUCCESS;

    case DEMUX_GET_TITLE_INFO:
    {
        input_title_t ***ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
        int *pi_int    = (int*)va_arg( args, int* );
        int *pi_title_offset = (int*)va_arg( args, int* );
        int *pi_chapter_offset = (int*)va_arg( args, int* );

        /* */
        *pi_title_offset = 0;
        *pi_chapter_offset = 0;

        /* Duplicate title infos */
        *pi_int = p_sys->i_title;
        *ppp_title = calloc( p_sys->i_title, sizeof(input_title_t *) );
        for( int i = 0; i < p_sys->i_title; i++ )
            (*ppp_title)[i] = vlc_input_title_Duplicate( p_sys->pp_title[i] );

        return VLC_SUCCESS;
    }

    case DEMUX_SET_TITLE:
    {
        int i_title = (int)va_arg( args, int );

        if( SetTitle( p_demux, i_title ) )
            return VLC_EGENERIC;
        return VLC_SUCCESS;
    }
    case DEMUX_SET_SEEKPOINT:
    {
        int i_chapter = (int)va_arg( args, int );

        if( SetChapter( p_demux, i_chapter ) )
            return VLC_EGENERIC;
        return VLC_SUCCESS;
    }

    case DEMUX_GET_PTS_DELAY:
    {
        int64_t *pi_delay = (int64_t*)va_arg( args, int64_t * );

        *pi_delay =
            INT64_C(1000) * var_InheritInteger( p_demux, "disc-caching" );
        return VLC_SUCCESS;
    }

    case DEMUX_GET_META:

    default:
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
#define BD_TS_PACKET_HEADER (4)
#define BD_TS_PACKET_SIZE (192)
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    if( !p_sys->p_m2ts )
        return -1;

    /* */
    if( p_sys->i_packet == p_sys->i_packet_start )
    {
        stream_Seek( p_sys->p_m2ts, 0 );

        block_t *p_block = stream_Block( p_sys->p_m2ts,
                                         p_sys->i_packet_headers * (int64_t)BD_TS_PACKET_SIZE + BD_TS_PACKET_HEADER );
        if( p_block )
        {
            p_block->i_buffer -= BD_TS_PACKET_HEADER;
            p_block->p_buffer += BD_TS_PACKET_HEADER;
            stream_DemuxSend( p_sys->p_parser, p_block );
        }

        stream_Seek( p_sys->p_m2ts, p_sys->i_packet_start * (int64_t)BD_TS_PACKET_SIZE );
    }

    /* */
    const int i_packets = __MIN( 5, p_sys->i_packet_stop - p_sys->i_packet );
    if( i_packets <= 0 )
    {
        const int i_title = p_demux->info.i_title;
        const bd_mpls_t *p_mpls = p_sys->pp_mpls[i_title];

        if( p_sys->i_play_item < p_mpls->i_play_item )
        {
            if( !SetPlayItem( p_demux, i_title, p_sys->i_play_item + 1 ) )
                return 1;
            msg_Warn( p_demux, "Failed to switch to the next play item" );
        }

        /* */
        if( SetTitle( p_demux, i_title + 1 ) )
            return 0; /* EOF */
        return 1;
    }

    /* XXX
     * we ensure that the TS packet start at the begining of the buffer,
     * it ensure proper TS parsing */
    block_t *p_block = block_Alloc( i_packets * BD_TS_PACKET_SIZE + BD_TS_PACKET_HEADER );
    if( !p_block )
        return -1;

    const int i_read = stream_Read( p_sys->p_m2ts, p_block->p_buffer, p_block->i_buffer - BD_TS_PACKET_HEADER );
    if( i_read <= 0 )
    {
        msg_Err( p_demux, "Error reading current title" );
        return -1;
    }

    if( i_read > 4 )
    {
        const int64_t i_atc = GetDWBE( p_block->p_buffer ) & ( (1 << 30) - 1 );

        if( i_atc < p_sys->i_atc_last )
            p_sys->i_atc_wrap += 1 << 30;
        p_sys->i_atc_last = i_atc;

        if( p_sys->i_atc_initial < 0 )
            p_sys->i_atc_initial = i_atc + p_sys->i_atc_wrap;

        p_sys->i_atc_current = i_atc + p_sys->i_atc_wrap;
    }

    p_block->i_buffer = i_read;
    p_block->p_buffer += BD_TS_PACKET_HEADER;
    stream_DemuxSend( p_sys->p_parser, p_block );

    p_sys->i_packet += i_read / BD_TS_PACKET_SIZE;

    /* Update EP */
    if( p_sys->p_clpi->i_ep_map > 0 )
    {
        const int i_old_clpi_ep = p_sys->i_clpi_ep;

        const bd_clpi_ep_map_t *p_ep_map = &p_sys->p_clpi->p_ep_map[0];
        for( ; p_sys->i_clpi_ep+1 < p_ep_map->i_ep; p_sys->i_clpi_ep++ )
        {
            const bd_clpi_ep_t *p_ep = &p_ep_map->p_ep[p_sys->i_clpi_ep+1];

            if( p_ep->i_packet > p_sys->i_packet )
                break;
        }
        if( i_old_clpi_ep != p_sys->i_clpi_ep )
        {
            /* We have changed of EP */
            p_sys->i_atc_initial = p_sys->i_atc_current; /* FIXME not exact */

            /* Update seekpoint */
            const input_title_t *p_title = p_sys->pp_title[p_demux->info.i_title];
            const int64_t i_time = GetTime( p_demux );

            for( ; p_demux->info.i_seekpoint+1 < p_title->i_seekpoint; p_demux->info.i_seekpoint++ )
            {
                const seekpoint_t *p_seekpoint = p_title->seekpoint[p_demux->info.i_seekpoint+1];
                if( p_seekpoint->i_time_offset >  i_time )
                    break;
                p_demux->info.i_update |= INPUT_UPDATE_SEEKPOINT;
            }
        }
    }
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
#define BD_45KHZ INT64_C(45000)
static void InitTitles( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* */
    for( int i = 0; i < p_sys->i_mpls; i++ )
    {
        const bd_mpls_t *p_mpls = p_sys->pp_mpls[i];

        input_title_t *t = vlc_input_title_New();
        if( !t )
            break;

        /* */
        t->i_length = 0;
        for( int j = 0; j < p_mpls->i_play_item; j++ )
        {
            const bd_mpls_play_item_t *p_item = &p_mpls->p_play_item[j];

            t->i_length += ( p_item->i_out_time - p_item->i_in_time ) * CLOCK_FREQ / BD_45KHZ;
        }

#ifdef BD_DEBUG
        {
        char psz_length[MSTRTIME_MAX_SIZE];
        msg_Warn( p_demux, "TITLE[%d] %s", i, secstotimestr( psz_length, t->i_length / CLOCK_FREQ ) );
        }
#endif

        /* Seekpoint */
        for( int j = 0; j < p_mpls->i_mark; j++ )
        {
            bd_mpls_mark_t *p_mark = &p_mpls->p_mark[j];

            if( p_mark->i_type == BD_MPLS_MARK_TYPE_BOOKMARK &&
                p_mark->i_play_item_id >= 0 && p_mark->i_play_item_id < p_mpls->i_play_item )
            {
                seekpoint_t *s = vlc_seekpoint_New();
                if( !s )
                    break;

                for( int k = 0; k <= p_mark->i_play_item_id; k++ )
                {
                    const bd_mpls_play_item_t *p_item = &p_mpls->p_play_item[k];
                    int64_t i_out_time;

                    if( k == p_mark->i_play_item_id )
                        i_out_time = p_mark->i_time;
                    else
                        i_out_time = p_item->i_out_time;
                    s->i_time_offset += ( i_out_time - p_item->i_in_time ) * CLOCK_FREQ / BD_45KHZ;
                }
#ifdef BD_DEBUG
                {
                char psz_time[MSTRTIME_MAX_SIZE];
                msg_Warn( p_demux, "    SEEKPOINT[%d] %s", j, secstotimestr( psz_time, s->i_time_offset / CLOCK_FREQ ) );
                }
#endif
                TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
            }
        }
        if( t->i_seekpoint <= 0 )
        {
            seekpoint_t *s = vlc_seekpoint_New();
            if( s )
                TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
        }

        TAB_APPEND( p_sys->i_title, p_sys->pp_title, t );
    }
}
static int SetTitle( demux_t *p_demux, int i_title )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( i_title < 0 || i_title >= p_sys->i_title )
        return VLC_EGENERIC;


    if( SetPlayItem( p_demux, i_title, 0 ) )
        return VLC_EGENERIC;

    /* */
    p_demux->info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
    p_demux->info.i_title = i_title;
    p_demux->info.i_seekpoint = 0;

    return VLC_SUCCESS;
}
static int SetChapter( demux_t *p_demux, int i_chapter )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const int i_title = p_demux->info.i_title;
    const input_title_t *p_title = p_sys->pp_title[i_title];

    if( i_chapter < 0 || i_chapter > p_title->i_seekpoint )
        return VLC_EGENERIC;

    if( SetTime( p_demux, p_title->seekpoint[i_chapter]->i_time_offset ) )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}
static int SetPlayItem( demux_t *p_demux, int i_mpls, int i_play_item )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* FIXME TODO do not reopen everything when avoidable
     * XXX becarefull that then the es_out wrapper need some sort of
     * locking !!! */

    /* */
    const bool b_same_mpls = i_mpls == p_demux->info.i_title;
    //const bool b_same_play_item = b_same_mpls &&
    //                              i_play_item == p_sys->i_play_item;

    /* */
    const bd_mpls_t *p_mpls = p_sys->pp_mpls[i_mpls];

    /* */
    if( i_play_item < 0 || i_play_item >= p_mpls->i_play_item )
        return VLC_EGENERIC;

    const bd_mpls_play_item_t *p_item = &p_mpls->p_play_item[i_play_item];
    const bd_mpls_clpi_t *p_mpls_clpi = &p_item->clpi;

    const bd_clpi_t *p_clpi = NULL;
    for( int i_clpi = 0; i_clpi < p_sys->i_clpi && !p_clpi; i_clpi++ )
    {
        if( p_sys->pp_clpi[i_clpi]->i_id == p_mpls_clpi->i_id )
            p_clpi = p_sys->pp_clpi[i_clpi];
    }

    const bool b_same_clpi = b_same_mpls && p_sys->p_clpi->i_id == p_clpi->i_id;
    stream_t *p_m2ts = NULL;
    if( !b_same_clpi )
    {
        char *psz_m2ts;
        if( asprintf( &psz_m2ts, "%s/STREAM/%05d.%s",
                      p_sys->psz_base, p_mpls_clpi->i_id, p_sys->b_shortname ? "MTS" : "m2ts" ) < 0 )
            return VLC_EGENERIC;

        p_m2ts = stream_UrlNew( p_demux, psz_m2ts );
        if( !p_m2ts )
        {
            msg_Err( p_demux, "Failed to open %s", psz_m2ts );
            free( psz_m2ts );
            return VLC_EGENERIC;
        }
        free( psz_m2ts );
    }

    /* TODO avoid reopenning the parser when unneeded.
     * - b_same_play_item is too strict, we should check the play_items connection.
     * - a way to completely flush the demuxer is also needed !
     */
    //const bool b_same_parser = b_same_play_item && false;
    stream_t *p_parser = stream_DemuxNew( p_demux, "ts", p_sys->p_out );
    if( !p_parser )
    {
        msg_Err( p_demux, "Failed to create TS demuxer" );
        if( p_m2ts )
            stream_Delete( p_m2ts );
        return VLC_EGENERIC;
    }

    /* */
    if( !p_m2ts )
    {
        msg_Dbg( p_demux, "Reusing stream file" );
        p_m2ts = p_sys->p_m2ts;
        p_sys->p_m2ts = NULL;
    }

    /* */
    ClosePlayItem( p_demux );

    /* */
    p_sys->p_clpi = p_clpi;
    p_sys->p_parser = p_parser;
    p_sys->p_m2ts = p_m2ts;
    p_sys->i_play_item = i_play_item;

    p_sys->i_packet_start = GetClpiPacket( p_demux, &p_sys->i_clpi_ep, p_mpls_clpi, p_item->i_in_time );
    if( p_sys->i_packet_start < 0 )
    {
        p_sys->i_packet_start = 0;
        p_sys->i_clpi_ep = 0;
    }

    p_sys->i_packet_stop = GetClpiPacket( p_demux, NULL, p_mpls_clpi, p_item->i_out_time );
    if( p_sys->i_packet_stop < 0 )
        p_sys->i_packet_stop = stream_Size( p_m2ts ) / BD_TS_PACKET_SIZE;
    p_sys->i_packet = p_sys->i_packet_start;

    /* This is a hack to detect the number of packet to send before any data
     * to have the PAT/PMT. I have no idea if it is the right, but seems to work.
     * I used a limits of 10 packets, sufficient if it is really only headers */
    p_sys->i_packet_headers = 0;
    if( p_clpi->i_ep_map > 0 )
    {
        const bd_clpi_ep_map_t *p_ep_map = &p_clpi->p_ep_map[0];
        if( p_ep_map->i_ep > 0 )
            p_sys->i_packet_headers = __MIN( p_ep_map->p_ep[0].i_packet, 10 );
    }

    p_sys->i_atc_initial = -1;
    p_sys->i_atc_current = -1;
    p_sys->i_atc_last    = -1;
    p_sys->i_atc_wrap    = 0;

    return VLC_SUCCESS;
}
static void ClosePlayItem( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->p_m2ts )
        stream_Delete( p_sys->p_m2ts );
    if( p_sys->p_parser )
        stream_Delete( p_sys->p_parser );

    es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
}

static int64_t GetClpiPacket( demux_t *p_demux, int *pi_ep, const bd_mpls_clpi_t *p_mpls_clpi, int64_t i_time /* in 45kHz */ )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    const bd_clpi_t *p_clpi = p_sys->p_clpi;
    assert( p_clpi );

    if( p_clpi->i_ep_map <= 0 )
        return -1;
    const bd_clpi_ep_map_t *p_ep_map = &p_clpi->p_ep_map[0];

    if( p_mpls_clpi->i_stc_id < 0 || p_mpls_clpi->i_stc_id >= p_clpi->i_stc )
        return -1;

    const bd_clpi_stc_t *p_stc = &p_clpi->p_stc[p_mpls_clpi->i_stc_id];
#if 0
    /* Not sure it is right */
    if( i_time < p_stc->i_start || i_time > p_stc->i_end )
        return -1;
#endif

    const int64_t i_packet = p_stc->i_packet;
    int i_ep;
    for( i_ep = 0; i_ep < p_ep_map->i_ep; i_ep++ )
    {
        if( p_ep_map->p_ep[i_ep].i_packet >= i_packet )
            break;
    }
    if( i_ep >= p_ep_map->i_ep )
        return -1;

    for( ; i_ep < p_ep_map->i_ep; i_ep++ )
    {
        const bd_clpi_ep_t *p_ep = &p_ep_map->p_ep[i_ep];
        const bd_clpi_ep_t *p_ep_next = &p_ep_map->p_ep[i_ep+1];

        if( i_ep+1 < p_ep_map->i_ep && p_ep_next->i_pts / 2 > i_time )
            break;
        if( p_ep->i_pts / 2 >= i_time )
            break;
    }
    if( i_ep >= p_ep_map->i_ep )
        return -1;

    /* */
    if( pi_ep )
        *pi_ep = i_ep;
    return p_ep_map->p_ep[i_ep].i_packet;
}

/**
 * Retreive the current time using current EP + ATC delta
 */
static int64_t GetTime( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const int i_mpls = p_demux->info.i_title;

    const bd_mpls_t *p_mpls = p_sys->pp_mpls[i_mpls];
    const bd_mpls_play_item_t *p_item = &p_mpls->p_play_item[p_sys->i_play_item];

    const bd_clpi_t *p_clpi = p_sys->p_clpi;
    if( !p_clpi || p_clpi->i_ep_map <= 0 )
        return 0;

    /* */
    const bd_clpi_ep_map_t *p_ep_map = &p_clpi->p_ep_map[0];
    if( p_sys->i_clpi_ep < 0 || p_sys->i_clpi_ep >= p_ep_map->i_ep )
        return 0;

    const bd_clpi_ep_t *p_ep = &p_ep_map->p_ep[p_sys->i_clpi_ep];
    int64_t i_time = p_ep->i_pts / 2 - p_item->i_in_time +
                     ( p_sys->i_atc_current - p_sys->i_atc_initial ) / 300 / 2;

    for( int j = 0; j < p_sys->i_play_item; j++ )
    {
        const bd_mpls_play_item_t *p_item = &p_mpls->p_play_item[j];
        i_time += ( p_item->i_out_time - p_item->i_in_time );
    }

    return i_time * CLOCK_FREQ / BD_45KHZ;
}

static double GetPosition( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    const int64_t i_time = GetTime( p_demux );
    const input_title_t *p_title = p_sys->pp_title[p_demux->info.i_title];

    if( p_title->i_length <= 0 )
        return 0.0;

    return (double)i_time / p_title->i_length;
}

static int SetTime( demux_t *p_demux, int64_t i_time )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const int i_mpls = p_demux->info.i_title;
    const input_title_t *p_title = p_sys->pp_title[i_mpls];
    const bd_mpls_t *p_mpls = p_sys->pp_mpls[i_mpls];

    /* Find the play item */
    int i_item;
    int64_t i_play_item_time = 0;
    for( i_item = 0; i_item < p_mpls->i_play_item; i_item++ )
    {
        const bd_mpls_play_item_t *p_item = &p_mpls->p_play_item[i_item];
        const int64_t i_duration = ( p_item->i_out_time - p_item->i_in_time ) * CLOCK_FREQ / BD_45KHZ;

        if( i_time >= i_play_item_time && i_time < i_play_item_time + i_duration )
            break;

        i_play_item_time += i_duration;
    }

    if( i_item >= p_mpls->i_play_item )
        return VLC_EGENERIC;

    if( SetPlayItem( p_demux, i_mpls, i_item ) )
        return VLC_EGENERIC;


    /* Find the right entry point */
    if( p_sys->p_clpi->i_ep_map <= 0 )
        goto update;

    const bd_clpi_ep_map_t *p_ep_map = &p_sys->p_clpi->p_ep_map[0];
    if( p_ep_map->i_ep <= 0 )
        goto update;

    int64_t i_next_display_date = -1;
    for( ; p_sys->i_clpi_ep+1 < p_ep_map->i_ep; p_sys->i_clpi_ep++ )
    {
        const bd_clpi_ep_t *p_next = &p_ep_map->p_ep[p_sys->i_clpi_ep+1];
        const int64_t i_next_time = i_play_item_time + ( ( p_next->i_pts / 2 - p_mpls->p_play_item[i_item].i_in_time ) * CLOCK_FREQ / BD_45KHZ );

        if( i_next_time > i_time )
        {
            const bd_clpi_ep_t *p_ep = &p_ep_map->p_ep[p_sys->i_clpi_ep];
            const int64_t i_ep_time = i_play_item_time + ( ( p_ep->i_pts / 2 - p_mpls->p_play_item[i_item].i_in_time ) * CLOCK_FREQ / BD_45KHZ );


            i_next_display_date = p_ep->i_pts * CLOCK_FREQ / 90000 + ( i_time - i_ep_time );
            break;
        }
    }

    const bd_clpi_ep_t *p_ep = &p_ep_map->p_ep[p_sys->i_clpi_ep];
    p_sys->i_packet_start =
    p_sys->i_packet       = p_ep->i_packet;

    if( i_next_display_date >= 0 )
        es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, i_next_display_date );

update:
    /* Update seekpoint */
    for( p_demux->info.i_seekpoint = 0; p_demux->info.i_seekpoint+1 < p_title->i_seekpoint; p_demux->info.i_seekpoint++ )
    {
        const seekpoint_t *p_seekpoint = p_title->seekpoint[p_demux->info.i_seekpoint+1];
        if( p_seekpoint->i_time_offset >  i_time )
            break;
    }
    p_demux->info.i_update |= INPUT_UPDATE_SEEKPOINT;
    return VLC_SUCCESS;
}

static int SetPosition( demux_t *p_demux, double f_position )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const input_title_t *p_title = p_sys->pp_title[p_demux->info.i_title];

    if( p_title->i_length <= 0 )
        return VLC_EGENERIC;

    return SetTime( p_demux, f_position * p_title->i_length );
}

/*****************************************************************************
 * Mpls ordering
 *****************************************************************************/
static int64_t GetMplsUniqueDuration( const bd_mpls_t *p_mpls )
{
    int64_t i_length = 0;

    for( int i = 0; i < p_mpls->i_play_item; i++ )
    {
        const bd_mpls_play_item_t *p_item0 = &p_mpls->p_play_item[i];
        int j;
        for( j = i+1; j < p_mpls->i_play_item; j++ )
        {
            const bd_mpls_play_item_t *p_item1 = &p_mpls->p_play_item[j];
            if( p_item0->clpi.i_id == p_item1->clpi.i_id &&
                p_item0->clpi.i_stc_id == p_item1->clpi.i_stc_id &&
                p_item0->i_in_time == p_item1->i_in_time &&
                p_item0->i_out_time == p_item1->i_out_time )
                break;
        }
        if( j >= p_mpls->i_play_item )
            i_length += p_item0->i_out_time - p_item0->i_in_time;
    }
    return i_length;
}
static int SortMpls( const void *a, const void *b )
{
    const bd_mpls_t * const *pp_mpls_a = a;
    const bd_mpls_t * const *pp_mpls_b = b;

    const int64_t i_length_a = GetMplsUniqueDuration( *pp_mpls_a );
    const int64_t i_length_b = GetMplsUniqueDuration( *pp_mpls_b );

    if( i_length_a == i_length_b )
        return 0;
    return i_length_a < i_length_b ? 1 : -1;
}

static void ReorderPlaylist( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    qsort( p_sys->pp_mpls, p_sys->i_mpls, sizeof(*p_sys->pp_mpls), SortMpls );
}
/*****************************************************************************
 * Helpers:
 *****************************************************************************/
static int CheckFileList( const char *psz_base, const char *ppsz_name[] )
{
    for( int i = 0; ppsz_name[i] != NULL ; i++ )
    {
        struct stat s;
        char *psz_tmp;

        if( asprintf( &psz_tmp, "%s/%s", psz_base, ppsz_name[i] ) < 0 )
            return VLC_EGENERIC;

        bool b_ok = vlc_stat( psz_tmp, &s ) == 0 && S_ISREG( s.st_mode );

        free( psz_tmp );
        if( !b_ok )
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
/* */
static char *FindPathBase( const char *psz_path, bool *pb_shortname )
{
    struct stat s;
    char *psz_tmp;

    /* */
    char *psz_base = strdup( psz_path );
    if( !psz_base )
        return NULL;

    /* */
    while( *psz_base && psz_base[strlen(psz_base)-1] == '/' )
        psz_base[strlen(psz_base)-1] = '\0';

    /* */
    if( vlc_stat( psz_base, &s ) || !S_ISDIR( s.st_mode ) )
        goto error;

    /* Check BDMV */
    if( asprintf( &psz_tmp, "%s/BDMV", psz_base ) < 0 )
        goto error;
    if( !vlc_stat( psz_tmp, &s ) && S_ISDIR( s.st_mode ) )
    {
        free( psz_base );
        psz_base = psz_tmp;
    }
    else
    {
        free( psz_tmp );
    }

    /* Check presence of mandatory files */
    static const char *ppsz_name_long[] = {
        "index.bdmv",
        "MovieObject.bdmv",
        NULL
    };
    static const char *ppsz_name_short[] = {
        "INDEX.BDM",
        "MOVIEOBJ.BDM",
        NULL
    };
    *pb_shortname = false;
    if( CheckFileList( psz_base, ppsz_name_long ) )
    {
        if( CheckFileList( psz_base, ppsz_name_short ) )
            goto error;
        *pb_shortname = true;
    }
    return psz_base;

error:
    free( psz_base );
    return NULL;
}

/* */
static block_t *LoadBlock( demux_t *p_demux, const char *psz_name )
{
    stream_t *s = stream_UrlNew( p_demux, psz_name );
    if( !s )
        return NULL;

    const int64_t i_size = stream_Size( s );
    block_t *p_block = NULL;

    if( i_size > 0 && i_size < INT_MAX )
        p_block = stream_Block( s, i_size );

    stream_Delete( s );

    return p_block;
}

/* */
static int FilterMplsLong( const char *psz_name )
{
    return strlen( psz_name ) == strlen( "xxxxx.mpls" ) &&
           !strcmp( &psz_name[5], ".mpls" );
}
static int FilterMplsShort( const char *psz_name )
{
    return strlen( psz_name ) == strlen( "xxxxx.MPL" ) &&
           !strcmp( &psz_name[5], ".MPL" );
}

static void LoadMpls( demux_t *p_demux, const char *psz_name, int i_id )
{
    demux_sys_t *p_sys = p_demux->p_sys;

#if defined(BD_DEBUG)
    msg_Err( p_demux, "Loading %s", psz_name );
#endif

    block_t *p_block = LoadBlock( p_demux, psz_name );
    if( !p_block )
        goto error;

    /* */
    bd_mpls_t *p_mpls = malloc( sizeof(*p_mpls) );
    if( !p_mpls )
        goto error;

    /* */
    bs_t s;
    bs_init( &s, p_block->p_buffer, p_block->i_buffer );

    if( bd_mpls_Parse( p_mpls, &s, i_id ) )
        goto error;

#if defined(BD_DEBUG)
    msg_Err( p_demux, "MPLS: id=%d", p_mpls->i_id );
    msg_Err( p_demux, "MPLS: play_item=%d sub_path=%d",
             p_mpls->i_play_item, p_mpls->i_sub_path );

    for( int i = 0; i < p_mpls->i_play_item; i++ )
    {
        bd_mpls_play_item_t *p_item = &p_mpls->p_play_item[i];

        msg_Err( p_demux, "PLAY_ITEM[%d] connection=%d in=%d out=%d still=%d(%d)",
                 i, p_item->i_connection,
                 (int)p_item->i_in_time, (int)p_item->i_out_time,
                 p_item->i_still, p_item->i_still_time );
        msg_Err( p_demux, "     clpi_default: id=%d stc_id=%d",
                 p_item->clpi.i_id, p_item->clpi.i_stc_id );
        for( int j = 0; j < p_item->i_clpi; j++ )
            msg_Err( p_demux, "     clpi[%d]: id=%d stc_id=%d",
                     j, p_item->p_clpi[j].i_id, p_item->p_clpi[j].i_stc_id );
        for( int j = 0; j < p_item->i_stream; j++ )
            msg_Err( p_demux, "     stream[%d]: type=%d class=%d stream_type=0x%x lang=%s charset=%d",
                     j,
                     p_item->p_stream[j].i_type,
                     p_item->p_stream[j].i_class,
                     p_item->p_stream[j].i_stream_type,
                     p_item->p_stream[j].psz_language,
                     p_item->p_stream[j].i_charset );
    }

    for( int i = 0; i < p_mpls->i_sub_path; i++ )
    {
        bd_mpls_sub_path_t *p_sub = &p_mpls->p_sub_path[i];

        msg_Err( p_demux, "SUB_PATH[%d] type=%d repeat=%d item=%d",
                 i, p_sub->i_type, p_sub->b_repeat, p_sub->i_item );
    }

    for( int i = 0; i < p_mpls->i_mark; i++ )
    {
        bd_mpls_mark_t *p_mark = &p_mpls->p_mark[i];

        msg_Err( p_demux, "M[%d] t=%d play_item_id=%d time=%d entry_es_pid=%d",
                 i, p_mark->i_type, p_mark->i_play_item_id, (int)p_mark->i_time, p_mark->i_entry_es_pid );
    }
#endif

    /* */
    TAB_APPEND( p_sys->i_mpls, p_sys->pp_mpls, p_mpls );

    /* */
    block_Release( p_block );
    return;

error:
    msg_Err( p_demux, "Failed loading %s", psz_name );
    if( p_block )
        block_Release( p_block );
}

/* */
static int FilterClpiLong( const char *psz_name )
{
    return strlen( psz_name ) == strlen( "xxxxx.clpi" ) &&
           !strcmp( &psz_name[5], ".clpi" );
}
static int FilterClpiShort( const char *psz_name )
{
    return strlen( psz_name ) == strlen( "xxxxx.CPI" ) &&
           !strcmp( &psz_name[5], ".CPI" );
}

static void LoadClpi( demux_t *p_demux, const char *psz_name, int i_id )
{
    demux_sys_t *p_sys = p_demux->p_sys;

#if defined(BD_DEBUG)
    msg_Err( p_demux, "Loading %s", psz_name );
#endif

    block_t *p_block = LoadBlock( p_demux, psz_name );
    if( !p_block )
        goto error;

    /* */
    bd_clpi_t *p_clpi = malloc( sizeof(*p_clpi) );
    if( !p_clpi )
        goto error;

    /* */
    bs_t s;
    bs_init( &s, p_block->p_buffer, p_block->i_buffer );

    if( bd_clpi_Parse( p_clpi, &s, i_id ) )
        goto error;

#if defined(BD_DEBUG)
    msg_Err( p_demux, "CLPI: id=%d", p_clpi->i_id );
    msg_Err( p_demux, "CLPI: STC=%d", p_clpi->i_stc );
    for( int i = 0; i < p_clpi->i_stc; i++ )
        msg_Err( p_demux, "   STC[%d] pcr_pid=%d packet=%d start=%d end=%d",
                 i, p_clpi->p_stc[i].i_pcr_pid, (int)p_clpi->p_stc[i].i_packet,
                 (int)p_clpi->p_stc[i].i_start, (int)p_clpi->p_stc[i].i_end );
    msg_Err( p_demux, "CLPI: Stream=%d", p_clpi->i_stream );
    for( int i = 0; i < p_clpi->i_stream; i++ )
        msg_Err( p_demux, "   Stream[%d] pid=%d type=0x%x",
                 i, p_clpi->p_stream[i].i_pid, p_clpi->p_stream[i].i_type );
    msg_Err( p_demux, "CLPI: Ep Map=%d", p_clpi->i_ep_map );
    for( int i = 0; i < p_clpi->i_ep_map; i++ )
    {
        const bd_clpi_ep_map_t *p_ep_map = &p_clpi->p_ep_map[i];
        msg_Err( p_demux, "   Ep Map[%d] pid=%d type=0x%x entry_point=%d",
                 i, p_ep_map->i_pid, p_ep_map->i_type, p_ep_map->i_ep );
        for( int j = 0; j < p_ep_map->i_ep; j++ )
        {
            msg_Err( p_demux, "      Ep[%d] packet=%d pts=%d",
                     j, (int)p_ep_map->p_ep[j].i_packet, (int)p_ep_map->p_ep[j].i_pts );
        }
    }
#endif

    /* */
    TAB_APPEND( p_sys->i_clpi, p_sys->pp_clpi, p_clpi );

    /* */
    block_Release( p_block );
    return;

error:
    msg_Err( p_demux, "Failed loading %s", psz_name );
    if( p_block )
        block_Release( p_block );
}

/* */
static int ScanSort( const char **ppsz_a, const char **ppsz_b )
{
    return strcmp( *ppsz_a, *ppsz_b );
}

static int Load( demux_t *p_demux,
                 const char *psz_dir,
                 int (*pf_filter)( const char * ),
                 void (*pf_load)( demux_t *p_demux, const char *psz_name, int i_id ) )
{
    char *psz_playlist;
    if( asprintf( &psz_playlist, "%s/%s", p_demux->p_sys->psz_base, psz_dir ) < 0 )
        return VLC_EGENERIC;

    char **ppsz_list;

    int i_list = vlc_scandir( psz_playlist, &ppsz_list, pf_filter, ScanSort );

    for( int i = 0; i < i_list; i++ )
    {
        char *psz_file = ppsz_list[i];
        if( !psz_file )
            break;

        char *psz_name;
        if( asprintf( &psz_name, "%s/%s/%s", p_demux->p_sys->psz_base, psz_dir, psz_file ) >= 0)
        {
            const int i_id = strtol( psz_file, NULL, 10 );
            pf_load( p_demux, psz_name, i_id );
            free( psz_name );
        }
        free( psz_file );
    }
    free( ppsz_list );

    free( psz_playlist );
    return VLC_SUCCESS;
}

static int LoadPlaylist( demux_t *p_demux )
{
    return Load( p_demux, "PLAYLIST",
                 p_demux->p_sys->b_shortname ? FilterMplsShort : FilterMplsLong, LoadMpls );
}
static int LoadClip( demux_t *p_demux )
{
    return Load( p_demux, "CLIPINF",
                 p_demux->p_sys->b_shortname ? FilterClpiShort : FilterClpiLong, LoadClpi );
}

/* */
struct es_out_sys_t
{
    demux_t *p_demux;
};

static es_out_id_t *EsOutAdd( es_out_t *p_out, const es_format_t *p_fmt )
{
    demux_t *p_demux = p_out->p_sys->p_demux;
    const bd_mpls_t *p_mpls = p_demux->p_sys->pp_mpls[p_demux->info.i_title];
    const bd_mpls_play_item_t *p_item = &p_mpls->p_play_item[p_demux->p_sys->i_play_item];

    es_format_t fmt;

    es_format_Copy( &fmt, p_fmt );
    fmt.i_priority = -2;

    for( int i = 0; i < p_item->i_stream; i++ )
    {
        const bd_mpls_stream_t *p_stream = &p_item->p_stream[i];
        if( p_stream->i_type != BD_MPLS_STREAM_TYPE_PLAY_ITEM ||
            p_stream->play_item.i_pid != fmt.i_id )
            continue;

        /* TODO improved priority for higher quality stream ?
         * if so, extending stream attributes parsing might be a good idea
         */
        fmt.i_priority = 0;

#if 0
        /* Useless, and beside not sure it is the right thing to do */
        free( fmt.psz_description );
        switch( p_stream->i_class )
        {
        case BD_MPLS_STREAM_CLASS_SECONDARY_AUDIO:
            fmt.psz_description = strdup( "Secondary audio" );
            break;
        default:
            fmt.psz_description = NULL;
            break;
        }
#endif

        //msg_Err( p_demux, "Found ref for stream pid %d", fmt.i_id );
        if( *p_stream->psz_language && ( !fmt.psz_language || *fmt.psz_language == '\0' ) )
        {
            free( fmt.psz_language );
            fmt.psz_language = strdup( p_stream->psz_language );
        }
        switch( p_stream->i_charset )
        {
        /* TODO add all values */
        default:
            break;
        }
        break;
    }
    if( fmt.i_priority < 0 )
        msg_Dbg( p_demux, "Hiding one stream (pid=%d)", fmt.i_id );

    /* */
    es_out_id_t *p_es = es_out_Add( p_demux->out, &fmt );

    es_format_Clean( &fmt );
    return p_es;
}
static int EsOutSend( es_out_t *p_out, es_out_id_t *p_es, block_t *p_block )
{
    return es_out_Send( p_out->p_sys->p_demux->out, p_es, p_block );
}
static void EsOutDel( es_out_t *p_out, es_out_id_t *p_es )
{
    es_out_Del( p_out->p_sys->p_demux->out, p_es );
}
static int EsOutControl( es_out_t *p_out, int i_query, va_list args )
{
    return es_out_vaControl( p_out->p_sys->p_demux->out, i_query, args );
}
static void EsOutDestroy( es_out_t *p_out )
{
    free( p_out->p_sys );
    free( p_out );
}

static es_out_t *EsOutNew( demux_t *p_demux )
{
    es_out_t *p_out = malloc( sizeof(*p_out) );
    es_out_sys_t *p_sys;

    if( !p_out )
        return NULL;

    p_out->pf_add     = EsOutAdd;
    p_out->pf_send    = EsOutSend;
    p_out->pf_del     = EsOutDel;
    p_out->pf_control = EsOutControl;
    p_out->pf_destroy = EsOutDestroy;

    p_out->p_sys = p_sys = malloc( sizeof(*p_sys) );
    if( !p_sys )
    {
        free( p_out );
        return NULL;
    }
    p_sys->p_demux = p_demux;

    return p_out;
}

