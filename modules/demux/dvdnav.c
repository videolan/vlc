/*****************************************************************************
 * dvdnav.c: DVD module using the dvdnav library.
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: dvdnav.c,v 1.7 2004/01/19 21:30:43 fenrir Exp $
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "vlc_keys.h"
#include "iso_lang.h"

#include <dvdnav/dvdnav.h>

#include "ps.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for dvdnav streams. This " \
    "value should be set in miliseconds units." )

static int  AccessOpen ( vlc_object_t * );
static void AccessClose( vlc_object_t * );

static int  DemuxOpen ( vlc_object_t * );
static void DemuxClose( vlc_object_t * );

vlc_module_begin();
    set_description( "DVDnav Input" );
    add_category_hint( N_("DVD"), NULL , VLC_TRUE );
    add_integer( "dvdnav-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    set_capability( "access", 0 );
    add_shortcut( "dvdnav" );
    set_callbacks( AccessOpen, AccessClose );

    add_submodule();
        set_description( "DVDnav Input (demux)" );
        set_capability( "demux2", 1 );
        add_shortcut( "dvdnav" );
        set_callbacks( DemuxOpen, DemuxClose );
vlc_module_end();

/* TODO
 *  - use dvdnav_get_next_cache_block/dvdnav_free_cache_block
 *  - all
 *  - once done the new input API remove the pseudo access
 *  - ...
 */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static ssize_t AccessRead( input_thread_t *, byte_t *, size_t ); /* dummy */
static char *ParseCL( vlc_object_t *, char *, vlc_bool_t, int *, int *, int *);

typedef struct
{
    VLC_COMMON_MEMBERS

    demux_t        *p_demux;
    vlc_mutex_t     lock;

    vlc_bool_t      b_moved;
    vlc_bool_t      b_clicked;
    vlc_bool_t      b_key;

    vlc_bool_t      b_still;
    int64_t         i_still_end;

} event_thread_t;

static int EventThread( vlc_object_t * );

struct demux_sys_t
{
    dvdnav_t    *dvdnav;

    /* track */
    ps_track_t  tk[PS_TK_COUNT];

    /* for spu variables */
    input_thread_t *p_input;

    /* event */
    event_thread_t *p_ev;

    /* FIXME */
    uint8_t     alpha[4];
    uint32_t    clut[16];

    /* */
    int i_aspect;

    /* */
    vlc_bool_t  b_es_out_ok;
};

static int DemuxControl( demux_t *, int, va_list );
static int DemuxDemux  ( demux_t * );
static int DemuxBlock  ( demux_t *, uint8_t *pkt, int i_pkt );

enum
{
    AR_SQUARE_PICTURE = 1,                          /* square pixels */
    AR_3_4_PICTURE    = 2,                       /* 3:4 picture (TV) */
    AR_16_9_PICTURE   = 3,             /* 16:9 picture (wide screen) */
    AR_221_1_PICTURE  = 4,                 /* 2.21:1 picture (movie) */
};

static int MenusCallback( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );

static void ESSubtitleUpdate( demux_t * );
static void ButtonUpdate( demux_t * );

/*****************************************************************************
 * Open: see if dvdnav can handle the input and if so force dvdnav demux
 *****************************************************************************
 * For now it has to be like that (ie open dvdnav twice) but will be fixed
 * when a demux can work without an access module.
 *****************************************************************************/
static int AccessOpen( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    dvdnav_t       *dvdnav;
    int            i_title, i_chapter, i_angle;
    char           *psz_name;
    vlc_value_t    val;
    vlc_bool_t     b_force;

    /* We try only if we are forced */
    if( p_input->psz_demux && *p_input->psz_demux &&
        strcmp( p_input->psz_demux, "dvdnav" ) )
    {
        msg_Warn( p_input, "dvdnav access discarded (demuxer forced)" );
        return VLC_EGENERIC;
    }

    b_force = (p_input->psz_access && !strcmp(p_input->psz_access, "dvdnav")) ?
        VLC_TRUE : VLC_FALSE;

    psz_name = ParseCL( VLC_OBJECT(p_input), p_input->psz_name, b_force,
                        &i_title, &i_chapter, &i_angle );
    if( !psz_name )
    {
        return VLC_EGENERIC;
    }

    /* Test dvdnav */
    if( dvdnav_open( &dvdnav, psz_name ) != DVDNAV_STATUS_OK )
    {
        msg_Warn( p_input, "cannot open dvdnav" );
        return VLC_EGENERIC;
    }
    dvdnav_close( dvdnav );
    free( psz_name );

    /* Fill p_input fields */
    p_input->pf_read = AccessRead;
    p_input->pf_set_program = input_SetProgram;
    p_input->pf_set_area = NULL;
    p_input->pf_seek = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = VLC_TRUE;
    p_input->stream.b_seekable = VLC_TRUE;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.p_selected_area->i_size = 1000;
    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    p_input->i_mtu = 0;

    /* force dvdnav plugin */
    p_input->psz_demux = strdup( "dvdnav" );

    /* Update default_pts to a suitable value for udp access */
    var_Create( p_input, "dvdnav-caching", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Get( p_input, "dvdnav-caching", &val );
    p_input->i_pts_delay = val.i_int * 1000;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void AccessClose( vlc_object_t *p_this )
{
}

/*****************************************************************************
 * Read: Should not be called (ie not used by the dvdnav demuxer)
 *****************************************************************************/
static ssize_t AccessRead( input_thread_t *p_input, byte_t *p_buffer,
                           size_t i_len )
{
    memset( p_buffer, 0, i_len );
    return i_len;
}

/*****************************************************************************
 * ParseCL: parse command line
 *****************************************************************************/
static char *ParseCL( vlc_object_t *p_this, char *psz_name, vlc_bool_t b_force,
                      int *i_title, int *i_chapter, int *i_angle )
{
    char *psz_parser, *psz_source, *psz_next;

    psz_source = strdup( psz_name );
    if( psz_source == NULL ) return NULL;

    *i_title = 0;
    *i_chapter = 1;
    *i_angle = 1;

    /* Start with the end, because you could have :
     * dvdnav:/Volumes/my@toto/VIDEO_TS@1,1
     * (yes, this is kludgy). */
    for( psz_parser = psz_source + strlen(psz_source) - 1;
         psz_parser >= psz_source && *psz_parser != '@';
         psz_parser-- );

    if( psz_parser >= psz_source && *psz_parser == '@' )
    {
        /* Found options */
        *psz_parser = '\0';
        ++psz_parser;

        *i_title = (int)strtol( psz_parser, &psz_next, 10 );
        if( *psz_next )
        {
            psz_parser = psz_next + 1;
            *i_chapter = (int)strtol( psz_parser, &psz_next, 10 );
            if( *psz_next )
            {
                *i_angle = (int)strtol( psz_next + 1, NULL, 10 );
            }
        }
    }

    *i_title   = *i_title >= 0 ? *i_title : 0;
    *i_chapter = *i_chapter    ? *i_chapter : 1;
    *i_angle   = *i_angle      ? *i_angle : 1;

    if( !*psz_source )
    {
        free( psz_source );
        if( !b_force )
        {
            return NULL;
        }
        psz_source = config_GetPsz( p_this, "dvd" );
        if( !psz_source ) return NULL;
    }

    msg_Dbg( p_this, "dvdroot=%s title=%d chapter=%d angle=%d",
             psz_source, *i_title, *i_chapter, *i_angle );

    return psz_source;
}

/*****************************************************************************
 * DemuxOpen:
 *****************************************************************************/
static int DemuxOpen( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    vlc_value_t val, text;
    int         i_title, i_titles, i_chapter, i_chapters, i_angle, i;
    char        *psz_name;

    if( strcmp( p_demux->psz_access, "dvdnav" ) ||
        strcmp( p_demux->psz_demux,  "dvdnav" ) )
    {
        msg_Warn( p_demux, "dvdnav module discarded" );
        return VLC_EGENERIC;
    }

    psz_name = ParseCL( VLC_OBJECT(p_demux), p_demux->psz_path, VLC_TRUE,
                        &i_title, &i_chapter, &i_angle );
    if( !psz_name )
    {
        return VLC_EGENERIC;
    }

    /* Init p_sys */
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    memset( p_sys, 0, sizeof( demux_sys_t ) );

    ps_track_init( p_sys->tk );

    p_sys->i_aspect = -1;

    p_sys->b_es_out_ok = VLC_FALSE;

    /* Open dvdnav */
    if( dvdnav_open( &p_sys->dvdnav, psz_name ) != DVDNAV_STATUS_OK )
    {
        msg_Warn( p_demux, "cannot open dvdnav" );
        return VLC_EGENERIC;
    }
    free( psz_name );

    /* Configure dvdnav */
    if( dvdnav_set_readahead_flag( p_sys->dvdnav, 1 ) != DVDNAV_STATUS_OK )
    {
        msg_Warn( p_demux, "cannot set readahead flag" );
    }

    if( dvdnav_set_PGC_positioning_flag( p_sys->dvdnav, 1 ) !=
          DVDNAV_STATUS_OK )
    {
        msg_Warn( p_demux, "cannot set PGC positioning flag" );
    }

    if( dvdnav_menu_language_select( p_sys->dvdnav, "en" ) !=
          DVDNAV_STATUS_OK ||
        dvdnav_audio_language_select( p_sys->dvdnav, "en" ) !=
          DVDNAV_STATUS_OK ||
        dvdnav_spu_language_select( p_sys->dvdnav, "en" ) !=
          DVDNAV_STATUS_OK )
    {
        msg_Warn( p_demux, "something failed while setting en language (%s)",
                  dvdnav_err_to_string( p_sys->dvdnav ) );
    }

    /* Find out number of titles/chapters */
    dvdnav_get_number_of_titles( p_sys->dvdnav, &i_titles );
    for( i = 1; i <= i_titles; i++ )
    {
        i_chapters = 0;
        dvdnav_get_number_of_parts( p_sys->dvdnav, i, &i_chapters );
    }

    if( dvdnav_title_play( p_sys->dvdnav, i_title ) !=
          DVDNAV_STATUS_OK )
    {
        msg_Warn( p_demux, "cannot set title/chapter" );
    }

    /* Get p_input and create variable */
    p_sys->p_input =
        vlc_object_find( p_demux, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    var_Create( p_sys->p_input, "x-start", VLC_VAR_INTEGER );
    var_Create( p_sys->p_input, "y-start", VLC_VAR_INTEGER );
    var_Create( p_sys->p_input, "x-end", VLC_VAR_INTEGER );
    var_Create( p_sys->p_input, "y-end", VLC_VAR_INTEGER );
    var_Create( p_sys->p_input, "color", VLC_VAR_ADDRESS );
    var_Create( p_sys->p_input, "contrast", VLC_VAR_ADDRESS );
    var_Create( p_sys->p_input, "highlight", VLC_VAR_BOOL );
    var_Create( p_sys->p_input, "highlight-mutex", VLC_VAR_MUTEX );

    /* Create a few object variables used for navigation in the interfaces */
    var_Create( p_sys->p_input, "dvd_menus",
                VLC_VAR_INTEGER | VLC_VAR_HASCHOICE | VLC_VAR_ISCOMMAND );
    text.psz_string = _("DVD menus");
    var_Change( p_sys->p_input, "dvd_menus", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_sys->p_input, "dvd_menus", MenusCallback, p_demux );
    val.i_int = DVD_MENU_Escape; text.psz_string = _("Resume");
    var_Change( p_sys->p_input, "dvd_menus", VLC_VAR_ADDCHOICE, &val, &text );
    val.i_int = DVD_MENU_Root; text.psz_string = _("Root");
    var_Change( p_sys->p_input, "dvd_menus", VLC_VAR_ADDCHOICE, &val, &text );
    val.i_int = DVD_MENU_Title; text.psz_string = _("Title");
    var_Change( p_sys->p_input, "dvd_menus", VLC_VAR_ADDCHOICE, &val, &text );
    val.i_int = DVD_MENU_Part; text.psz_string = _("Chapter");
    var_Change( p_sys->p_input, "dvd_menus", VLC_VAR_ADDCHOICE, &val, &text );
    val.i_int = DVD_MENU_Subpicture; text.psz_string = _("Subtitle");
    var_Change( p_sys->p_input, "dvd_menus", VLC_VAR_ADDCHOICE, &val, &text );
    val.i_int = DVD_MENU_Audio; text.psz_string = _("Audio");
    var_Change( p_sys->p_input, "dvd_menus", VLC_VAR_ADDCHOICE, &val, &text );
    val.i_int = DVD_MENU_Angle; text.psz_string = _("Angle");
    var_Change( p_sys->p_input, "dvd_menus", VLC_VAR_ADDCHOICE, &val, &text );

    /* Now create our event thread catcher */
    p_sys->p_ev = vlc_object_create( p_demux, sizeof( event_thread_t ) );
    p_sys->p_ev->p_demux = p_demux;
    vlc_thread_create( p_sys->p_ev, "dvdnav event thread handler", EventThread,
                       VLC_THREAD_PRIORITY_LOW, VLC_FALSE );

    /* fill p_demux field */
    p_demux->pf_control = DemuxControl;
    p_demux->pf_demux = DemuxDemux;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DemuxClose:
 *****************************************************************************/
static void DemuxClose( vlc_object_t *p_this )
{

    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    /* stop the event handler */
    p_sys->p_ev->b_die = VLC_TRUE;
    vlc_thread_join( p_sys->p_ev );

    var_Destroy( p_sys->p_input, "highlight-mutex" );
    var_Destroy( p_sys->p_input, "highlight" );
    var_Destroy( p_sys->p_input, "x-start" );
    var_Destroy( p_sys->p_input, "x-end" );
    var_Destroy( p_sys->p_input, "y-start" );
    var_Destroy( p_sys->p_input, "y-end" );
    var_Destroy( p_sys->p_input, "color" );
    var_Destroy( p_sys->p_input, "contrast" );

    vlc_object_release( p_sys->p_input );

    dvdnav_close( p_sys->dvdnav );
    free( p_sys );
}

/*****************************************************************************
 * DemuxControl:
 *****************************************************************************/
static int DemuxControl( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
        {
            uint32_t pos, len;
            pf = (double*) va_arg( args, double* );
            if( dvdnav_get_position( p_sys->dvdnav, &pos, &len ) ==
                  DVDNAV_STATUS_OK && len > 0 )
            {
                *pf = (double)pos / (double)len;
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;
        }
        case DEMUX_SET_POSITION:
        {
            uint32_t pos, len;
            f = (double) va_arg( args, double );
            if( dvdnav_get_position( p_sys->dvdnav, &pos, &len ) ==
                  DVDNAV_STATUS_OK && len > 0 )
            {
                pos = f * len;
                if( dvdnav_sector_search( p_sys->dvdnav, pos, SEEK_SET ) ==
                      DVDNAV_STATUS_OK )
                {
                    return VLC_SUCCESS;
                }
            }
            return VLC_EGENERIC;
        }

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * DemuxDemux:
 *****************************************************************************/
static int DemuxDemux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    uint8_t packet[DVD_VIDEO_LB_LEN];
    int i_event;
    int i_len;

    if( !p_sys->b_es_out_ok )
    {
        /* We do ourself the selection/unselection
         * Problem: bypass --audio-channel and --spu-channel
         * Solution: call ourself dvdnav_??set_channel -> TODO
         */
        es_out_Control( p_demux->out, ES_OUT_SET_MODE, ES_OUT_MODE_NONE );

        p_sys->b_es_out_ok = VLC_TRUE;
    }

    if( dvdnav_get_next_block( p_sys->dvdnav, packet, &i_event, &i_len ) ==
          DVDNAV_STATUS_ERR )
    {
        msg_Warn( p_demux, "cannot get next block (%s)",
                  dvdnav_err_to_string( p_sys->dvdnav ) );
        return -1;
    }

    switch( i_event )
    {
    case DVDNAV_BLOCK_OK:   /* mpeg block */
        DemuxBlock( p_demux, packet, i_len );
        break;

    case DVDNAV_NOP:    /* Nothing */
        msg_Dbg( p_demux, "DVDNAV_NOP" );
        break;

    case DVDNAV_STILL_FRAME:
    {
        dvdnav_still_event_t *event = (dvdnav_still_event_t*)packet;
        vlc_mutex_lock( &p_sys->p_ev->lock );
        if( !p_sys->p_ev->b_still )
        {
            msg_Dbg( p_demux, "DVDNAV_STILL_FRAME" );
            msg_Dbg( p_demux, "     - length=0x%x", event->length );
            p_sys->p_ev->b_still = VLC_TRUE;
            if( event->length == 0xff )
            {
                p_sys->p_ev->i_still_end = 0;
            }
            else
            {
                p_sys->p_ev->i_still_end = (int64_t)event->length *
                    1000000 + mdate() + p_sys->p_input->i_pts_delay;
            }
        }
        vlc_mutex_unlock( &p_sys->p_ev->lock );
        msleep( 40000 );
        break;
    }
    case DVDNAV_SPU_STREAM_CHANGE:
    {
        dvdnav_spu_stream_change_event_t *event =
            (dvdnav_spu_stream_change_event_t*)packet;
        msg_Dbg( p_demux, "DVDNAV_SPU_STREAM_CHANGE" );
        msg_Dbg( p_demux, "     - physical_wide=%d",
                 event->physical_wide );
        msg_Dbg( p_demux, "     - physical_letterbox=%d",
                 event->physical_letterbox);
        msg_Dbg( p_demux, "     - physical_pan_scan=%d",
                 event->physical_pan_scan );

        ESSubtitleUpdate( p_demux );
        break;
    }
    case DVDNAV_AUDIO_STREAM_CHANGE:
    {
        dvdnav_audio_stream_change_event_t *event =
            (dvdnav_audio_stream_change_event_t*)packet;
        msg_Dbg( p_demux, "DVDNAV_AUDIO_STREAM_CHANGE" );
        msg_Dbg( p_demux, "     - physical=%d", event->physical );
        /* TODO */
        break;
    }
    case DVDNAV_VTS_CHANGE:
    {
        int i;
        dvdnav_vts_change_event_t *event = (dvdnav_vts_change_event_t*)packet;
        msg_Dbg( p_demux, "DVDNAV_VTS_CHANGE" );
        msg_Dbg( p_demux, "     - vtsN=%d", event->new_vtsN );
        msg_Dbg( p_demux, "     - domain=%d", event->new_domain );

        /* dvdnav_get_video_aspect / dvdnav_get_video_scale_permission */
        /* TODO check if we alsways have VTS and CELL */
        p_sys->i_aspect = dvdnav_get_video_aspect( p_sys->dvdnav );

        /* reset PCR */
        es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

        for( i = 0; i < PS_TK_COUNT; i++ )
        {
            ps_track_t *tk = &p_sys->tk[i];
            if( tk->b_seen && tk->es )
            {
                es_out_Del( p_demux->out, tk->es );
            }
            tk->b_seen = VLC_FALSE;
        }
        break;
    }
    case DVDNAV_CELL_CHANGE:
    {
        dvdnav_cell_change_event_t *event =
            (dvdnav_cell_change_event_t*)packet;
        msg_Dbg( p_demux, "DVDNAV_CELL_CHANGE" );
        msg_Dbg( p_demux, "     - cellN=%d", event->cellN );
        msg_Dbg( p_demux, "     - pgN=%d", event->pgN );
        msg_Dbg( p_demux, "     - cell_length=%lld", event->cell_length );
        msg_Dbg( p_demux, "     - pg_length=%lld", event->pg_length );
        msg_Dbg( p_demux, "     - pgc_length=%lld", event->pgc_length );
        msg_Dbg( p_demux, "     - cell_start=%lld", event->cell_start );
        msg_Dbg( p_demux, "     - pg_start=%lld", event->pg_start );
        break;
    }
    case DVDNAV_NAV_PACKET:
    {
        msg_Dbg( p_demux, "DVDNAV_NAV_PACKET" );
        /* A lot of thing to do here :
         *  - handle packet
         *  - fetch pts (for time display)
         *  - ...
         */
        DemuxBlock( p_demux, packet, i_len );
        break;
    }
    case DVDNAV_STOP:   /* EOF */
        msg_Dbg( p_demux, "DVDNAV_STOP" );
        return 0;

    case DVDNAV_HIGHLIGHT:
    {
        dvdnav_highlight_event_t *event = (dvdnav_highlight_event_t*)packet;
        msg_Dbg( p_demux, "DVDNAV_HIGHLIGHT" );
        msg_Dbg( p_demux, "     - display=%d", event->display );
        msg_Dbg( p_demux, "     - buttonN=%d", event->buttonN );

        ButtonUpdate( p_demux );
        break;
    }

    case DVDNAV_SPU_CLUT_CHANGE:
        msg_Dbg( p_demux, "DVDNAV_SPU_CLUT_CHANGE" );
        /* Update color lookup table (16 *uint32_t in packet) */
        memcpy( p_sys->clut, packet, 16 * sizeof( uint32_t ) );
        break;

    case DVDNAV_HOP_CHANNEL:
        msg_Dbg( p_demux, "DVDNAV_HOP_CHANNEL" );
        /* We should try to flush all our internal buffer */
        break;

    case DVDNAV_WAIT:
        msg_Dbg( p_demux, "DVDNAV_WAIT" );

        dvdnav_wait_skip( p_sys->dvdnav );
        break;

    default:
        msg_Warn( p_demux, "Unknown event (0x%x)", i_event );
        break;
    }

    return 1;
}
/*****************************************************************************
 * Update functions:
 *****************************************************************************/
static void ButtonUpdate( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    vlc_value_t val;
    int32_t i_title, i_part;

    dvdnav_current_title_info( p_sys->dvdnav, &i_title, &i_part );

    if( var_Get( p_sys->p_input, "highlight-mutex", &val ) == VLC_SUCCESS )
    {
        vlc_mutex_t *p_mutex = val.p_address;
        dvdnav_highlight_area_t hl;
        int32_t i_button;

        if( dvdnav_get_current_highlight( p_sys->dvdnav, &i_button ) != DVDNAV_STATUS_OK )
        {
            msg_Err( p_demux, "dvdnav_get_current_highlight failed" );
            return;
        }

        if( i_button > 0 && i_title ==  0 )
        {
            pci_t *pci = dvdnav_get_current_nav_pci( p_sys->dvdnav );

            dvdnav_get_highlight_area( pci, i_button, 1, &hl );

            /* I fear it is plain wrong */
            //val.p_address = (void *)&hl.palette;
            p_sys->alpha[0] = hl.palette&0x0f;
            p_sys->alpha[1] = (hl.palette>>4)&0x0f;
            p_sys->alpha[2] = (hl.palette>>8)&0x0f;
            p_sys->alpha[3] = (hl.palette>>12)&0x0f;

            vlc_mutex_lock( p_mutex );
            val.i_int = hl.sx; var_Set( p_sys->p_input, "x-start", val );
            val.i_int = hl.ex; var_Set( p_sys->p_input, "x-end", val );
            val.i_int = hl.sy; var_Set( p_sys->p_input, "y-start", val );
            val.i_int = hl.ey; var_Set( p_sys->p_input, "y-end", val );

            val.p_address = (void *)p_sys->alpha;
            var_Set( p_sys->p_input, "contrast", val );

            val.b_bool = VLC_TRUE; var_Set( p_sys->p_input, "highlight", val );
            vlc_mutex_unlock( p_mutex );

            msg_Dbg( p_demux, "ButtonUpdate %d", i_button );
        }
        else
        {
            msg_Dbg( p_demux, "ButtonUpdate not done b=%d t=%d", i_button, i_title );

            /* Show all */
            vlc_mutex_lock( p_mutex );
            val.b_bool = VLC_FALSE; var_Set( p_sys->p_input, "highlight", val );
            vlc_mutex_unlock( p_mutex );
        }
    }
}

static void ESNew( demux_t *p_demux, int i_id );

static void ESSubtitleUpdate( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int         i_spu = dvdnav_get_active_spu_stream( p_sys->dvdnav );

    ButtonUpdate( p_demux );

    if( i_spu >= 0 && i_spu <= 0x1f )
    {
        ps_track_t *tk = &p_sys->tk[PS_ID_TO_TK(0xbd20 + i_spu)];

        if( !tk->b_seen )
        {
            ESNew( p_demux, 0xbd20 + i_spu);
        }
        /* be sure to unselect it (reset) */
        es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE, tk->es, (vlc_bool_t)VLC_FALSE );

        /* now select it */
        es_out_Control( p_demux->out, ES_OUT_SET_ES, tk->es );
    }
    else
    {
        for( i_spu = 0; i_spu <= 0x1F; i_spu++ )
        {
            ps_track_t *tk = &p_sys->tk[PS_ID_TO_TK(0xbd20 + i_spu)];
            if( tk->b_seen )
            {
                es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE, tk->es, (vlc_bool_t)VLC_FALSE );
            }
        }
    }
}


/*****************************************************************************
 * DemuxBlock: demux a given block
 *****************************************************************************/
static int DemuxBlock( demux_t *p_demux, uint8_t *pkt, int i_pkt )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint8_t     *p = pkt;

    while( p < &pkt[i_pkt] )
    {
        int i_size = ps_pkt_size( p, &pkt[i_pkt] - p );
        block_t *p_pkt;
        if( i_size <= 0 )
        {
            break;
        }

        /* Create a block */
        p_pkt = block_New( p_demux, i_size );
        memcpy( p_pkt->p_buffer, p, i_size);

        /* Parse it and send it */
        switch( 0x100 | p[3] )
        {
        case 0x1b9:
        case 0x1bb:
        case 0x1bc:
            if( p[3] == 0xbc )
            {
                msg_Warn( p_demux, "received a PSM packet" );
            }
            else if( p[3] == 0xbb )
            {
                msg_Warn( p_demux, "received a SYSTEM packet" );
            }
            block_Release( p_pkt );
            break;

        case 0x1ba:
        {
            int64_t i_scr;
            int i_mux_rate;
            if( !ps_pkt_parse_pack( p_pkt, &i_scr, &i_mux_rate ) )
            {
                es_out_Control( p_demux->out, ES_OUT_SET_PCR, i_scr );
            }
            block_Release( p_pkt );
            break;
        }
        default:
        {
            int i_id = ps_pkt_id( p_pkt );
            if( i_id >= 0xc0 )
            {
                ps_track_t *tk = &p_sys->tk[PS_ID_TO_TK(i_id)];

                if( !tk->b_seen )
                {
                    ESNew( p_demux, i_id );
                }
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
        }

        p += i_size;
    }

    return VLC_SUCCESS;
}

static char *LangCode2String( uint16_t i_lang )
{
    const iso639_lang_t *pl;
    char  lang[3];

    if( i_lang == 0xffff )
    {
        return NULL;
    }
    lang[0] = (i_lang >> 8)&0xff;
    lang[1] = (i_lang     )&0xff;
    lang[2] = 0;

    pl =  GetLang_1( lang );
    if( !strcmp( pl->psz_iso639_1, "??" ) )
    {
        return strdup( lang );
    }
    else if( *pl->psz_native_name )
    {
        return strdup( pl->psz_native_name );
    }
    return strdup( pl->psz_eng_name );
}

static void ESNew( demux_t *p_demux, int i_id )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    ps_track_t  *tk = &p_sys->tk[PS_ID_TO_TK(i_id)];
    vlc_bool_t  b_select = VLC_FALSE;

    if( tk->b_seen )
    {
        return;
    }

    if( ps_track_fill( tk, i_id ) )
    {
        msg_Warn( p_demux, "unknown codec for id=0x%x", i_id );
        return;
    }

    /* Add a new ES */
    if( tk->fmt.i_cat == VIDEO_ES )
    {
        if( p_sys->i_aspect >= 0 )
        {
            tk->fmt.video.i_aspect = p_sys->i_aspect;
        }
        b_select = VLC_TRUE;
    }
    else if( tk->fmt.i_cat == AUDIO_ES )
    {
        int i_audio = -1;
        /* find the audio number PLEASE find another way */
        if( (i_id&0xbdf8) == 0xbd88 )       /* dts */
        {
            i_audio = i_id&0x07;
        }
        else if( (i_id&0xbdf0) == 0xbd80 )  /* a52 */
        {
            i_audio = i_id&0xf;
        }
        else if( (i_id&0xbdf0) == 0xbda0 )  /* lpcm */
        {
            i_audio = i_id&0x1f;
        }
        else if( ( i_id&0xe0 ) == 0xc0 )    /* mpga */
        {
            i_audio = i_id&0x1f;
        }
        if( i_audio >= 0 )
        {
            tk->fmt.psz_language =
                LangCode2String( dvdnav_audio_stream_to_lang( p_sys->dvdnav, i_audio ) );
            if( dvdnav_get_active_audio_stream( p_sys->dvdnav ) == i_audio )
            {
                b_select = VLC_TRUE;
            }
        }
    }
    else if( tk->fmt.i_cat == SPU_ES )
    {
        tk->fmt.psz_language =
            LangCode2String( dvdnav_spu_stream_to_lang( p_sys->dvdnav, i_id&0x1f ) );
        /* palette */
        tk->fmt.subs.spu.palette[0] = 0xBeef;
        memcpy( &tk->fmt.subs.spu.palette[1], p_sys->clut, 16 * sizeof( uint32_t ) );
    }

    tk->es = es_out_Add( p_demux->out, &tk->fmt );
    if( b_select )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_ES, tk->es );
    }
    tk->b_seen = VLC_TRUE;
}

/*****************************************************************************
 * Event handler code
 *****************************************************************************/
static int  EventMouse( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );
static int  EventKey  ( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );

static int EventThread( vlc_object_t *p_this )
{
    event_thread_t *p_ev = (event_thread_t*)p_this;
    demux_sys_t    *p_sys = p_ev->p_demux->p_sys;
    vlc_object_t   *p_vout = NULL;

    vlc_mutex_init( p_ev, &p_ev->lock );
    p_ev->b_moved   = VLC_FALSE;
    p_ev->b_clicked = VLC_FALSE;
    p_ev->b_key     = VLC_FALSE;

    p_ev->b_still   = VLC_FALSE;

    /* catch all key event */
    var_AddCallback( p_ev->p_vlc, "key-pressed", EventKey, p_ev );

    /* main loop */
    while( !p_ev->b_die )
    {
        vlc_bool_t b_activated = VLC_FALSE;

        /* KEY part */
        if( p_ev->b_key )
        {
            pci_t *pci = dvdnav_get_current_nav_pci( p_sys->dvdnav );

            vlc_value_t valk;
            struct hotkey *p_hotkeys = p_ev->p_vlc->p_hotkeys;
            int i, i_action = -1;

            vlc_mutex_lock( &p_ev->lock );
            var_Get( p_ev->p_vlc, "key-pressed", &valk );
            for( i = 0; p_hotkeys[i].psz_action != NULL; i++ )
            {
                if( p_hotkeys[i].i_key == valk.i_int )
                {
                    i_action = p_hotkeys[i].i_action;
                }
            }

            switch( i_action )
            {
            case ACTIONID_NAV_LEFT:
                dvdnav_left_button_select( p_sys->dvdnav, pci );
                break;
            case ACTIONID_NAV_RIGHT:
                dvdnav_right_button_select( p_sys->dvdnav, pci );
                break;
            case ACTIONID_NAV_UP:
                dvdnav_upper_button_select( p_sys->dvdnav, pci );
                break;
            case ACTIONID_NAV_DOWN:
                dvdnav_lower_button_select( p_sys->dvdnav, pci );
                break;
            case ACTIONID_NAV_ACTIVATE:
                b_activated = VLC_TRUE;
                dvdnav_button_activate( p_sys->dvdnav, pci );
                break;
            default:
                break;
            }
            p_ev->b_key = VLC_FALSE;
            vlc_mutex_unlock( &p_ev->lock );
        }

        /* VOUT part */
        if( p_vout && ( p_ev->b_moved || p_ev->b_clicked ) )
        {
            pci_t       *pci = dvdnav_get_current_nav_pci( p_sys->dvdnav );
            vlc_value_t valx, valy;

            vlc_mutex_lock( &p_ev->lock );
            var_Get( p_vout, "mouse-x", &valx );
            var_Get( p_vout, "mouse-y", &valy );

            if( p_ev->b_moved )
            {
                dvdnav_mouse_select( p_sys->dvdnav, pci, valx.i_int,
                                     valy.i_int );
            }
            if( p_ev->b_clicked )
            {
                b_activated = VLC_TRUE;
                dvdnav_mouse_activate( p_sys->dvdnav, pci, valx.i_int,
                                       valy.i_int );
            }

            p_ev->b_moved = VLC_FALSE;
            p_ev->b_clicked = VLC_FALSE;
            vlc_mutex_unlock( &p_ev->lock );
        }
        if( p_vout && p_vout->b_die )
        {
            var_DelCallback( p_vout, "mouse-moved", EventMouse, p_ev );
            var_DelCallback( p_vout, "mouse-clicked", EventMouse, p_ev );
            vlc_object_release( p_vout );
            p_vout = NULL;
        }
        if( p_vout == NULL )
        {
            p_vout = vlc_object_find( p_sys->p_input, VLC_OBJECT_VOUT,
                                      FIND_CHILD );
            if( p_vout)
            {
                var_AddCallback( p_vout, "mouse-moved", EventMouse, p_ev );
                var_AddCallback( p_vout, "mouse-clicked", EventMouse, p_ev );
            }
        }

        /* Still part */
        vlc_mutex_lock( &p_ev->lock );
        if( p_ev->b_still )
        {
            if( /* b_activated || // This breaks menus */
                ( p_ev->i_still_end > 0 && p_ev->i_still_end < mdate() ))
            {
                p_ev->b_still = VLC_FALSE;
                dvdnav_still_skip( p_sys->dvdnav );
            }
        }
        vlc_mutex_unlock( &p_ev->lock );

        /* Wait a bit */
        msleep( 10000 );
    }

    /* Release callback */
    if( p_vout )
    {
        var_DelCallback( p_vout, "mouse-moved", EventMouse, p_ev );
        var_DelCallback( p_vout, "mouse-clicked", EventMouse, p_ev );
        vlc_object_release( p_vout );
    }
    var_DelCallback( p_ev->p_vlc, "key-pressed", EventKey, p_ev );

    vlc_mutex_destroy( &p_ev->lock );

    return VLC_SUCCESS;
}

static int EventMouse( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    event_thread_t *p_ev = p_data;
    vlc_mutex_lock( &p_ev->lock );
    if( psz_var[6] == 'c' )
        p_ev->b_clicked = VLC_TRUE;
    else if( psz_var[6] == 'm' )
        p_ev->b_moved = VLC_TRUE;
    vlc_mutex_unlock( &p_ev->lock );

    return VLC_SUCCESS;
}

static int EventKey( vlc_object_t *p_this, char const *psz_var,
                     vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    event_thread_t *p_ev = p_data;
    vlc_mutex_lock( &p_ev->lock );
    p_ev->b_key = VLC_TRUE;
    vlc_mutex_unlock( &p_ev->lock );

    return VLC_SUCCESS;
}

static int MenusCallback( vlc_object_t *p_this, char const *psz_name,
                          vlc_value_t oldval, vlc_value_t newval, void *p_arg )
{
    demux_t *p_demux = (demux_t*)p_arg;

    /* FIXME, not thread safe */
    dvdnav_menu_call( p_demux->p_sys->dvdnav, newval.i_int );

    return VLC_SUCCESS;
}
