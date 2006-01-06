/*****************************************************************************
 * dvdnav.c: DVD module using the dvdnav library.
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#include "vlc_keys.h"
#include "iso_lang.h"

/* FIXME we should find a better way than including that */
#include "../../src/misc/iso-639_def.h"


#include <dvdnav/dvdnav.h>

#include "../demux/ps.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ANGLE_TEXT N_("DVD angle")
#define ANGLE_LONGTEXT N_( \
    "Allows you to select the default DVD angle." )

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for DVDnav streams. This "\
    "value should be set in millisecond units." )
#define MENU_TEXT N_("Start directly in menu")
#define MENU_LONGTEXT N_( \
    "Allows you to start the DVD directly in the main menu. This "\
    "will try to skip all the useless warnings introductions." )

#define LANGUAGE_DEFAULT ("en")

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_shortname( _("DVD with menus") );
    set_description( _("DVDnav Input") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );
    add_integer( "dvdnav-angle", 1, NULL, ANGLE_TEXT,
        ANGLE_LONGTEXT, VLC_FALSE );
    add_integer( "dvdnav-caching", DEFAULT_PTS_DELAY / 1000, NULL,
        CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    add_bool( "dvdnav-menu", VLC_TRUE, NULL,
        MENU_TEXT, MENU_LONGTEXT, VLC_FALSE );
    set_capability( "access_demux", 5 );
    add_shortcut( "dvd" );
    add_shortcut( "dvdnav" );
    set_callbacks( Open, Close );
vlc_module_end();

/* Shall we use libdvdnav's read ahead cache? */
#define DVD_READ_CACHE 1

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
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
    int         i_mux_rate;

    /* for spu variables */
    input_thread_t *p_input;

    /* event */
    event_thread_t *p_ev;

    /* palette for menus */
    uint32_t clut[16];
    uint8_t  palette[4][4];
    vlc_bool_t b_spu_change;

    /* */
    int i_aspect;

    int           i_title;
    input_title_t **title;

    /* lenght of program group chain */
    mtime_t     i_pgc_length;
};

static int Control( demux_t *, int, va_list );
static int Demux( demux_t * );
static int DemuxBlock( demux_t *, uint8_t *, int );

static void DemuxTitles( demux_t * );
static void ESSubtitleUpdate( demux_t * );
static void ButtonUpdate( demux_t *, vlc_bool_t );

static void ESNew( demux_t *, int );
static int ProbeDVD( demux_t *, char * );

static char *DemuxGetLanguageCode( demux_t *p_demux, char *psz_var );

/*****************************************************************************
 * DemuxOpen:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    dvdnav_t    *p_dvdnav;
    int         i_angle;
    char        *psz_name;
    char        *psz_code;
    vlc_value_t val;

    if( !p_demux->psz_path || !*p_demux->psz_path )
    {
        /* Only when selected */
        if( !p_this->b_force ) return VLC_EGENERIC;

        psz_name = var_CreateGetString( p_this, "dvd" );
        if( !psz_name )
        {
            psz_name = strdup("");
        }
    }
    else
        psz_name = strdup( p_demux->psz_path );

#ifdef WIN32
    if( psz_name[0] && psz_name[1] == ':' &&
        psz_name[2] == '\\' && psz_name[3] == '\0' ) psz_name[2] = '\0';
#endif

    /* Try some simple probing to avoid going through dvdnav_open too often */
    if( ProbeDVD( p_demux, psz_name ) != VLC_SUCCESS )
    {
        free( psz_name );
        return VLC_EGENERIC;
    }

    /* Open dvdnav */
    if( dvdnav_open( &p_dvdnav, psz_name ) != DVDNAV_STATUS_OK )
    {
        msg_Warn( p_demux, "cannot open dvdnav" );
        free( psz_name );
        return VLC_EGENERIC;
    }
    free( psz_name );

    /* Fill p_demux field */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    memset( p_sys, 0, sizeof( demux_sys_t ) );
    p_sys->dvdnav = p_dvdnav;

    ps_track_init( p_sys->tk );
    p_sys->i_aspect = -1;
    p_sys->i_mux_rate = 0;
    p_sys->i_pgc_length = 0;
    p_sys->b_spu_change = VLC_FALSE;

    if( 1 )
    {
        // Hack for libdvdnav CVS.
        // Without it dvdnav_get_number_of_titles() fails.
        // Remove when fixed in libdvdnav CVS.
        uint8_t buffer[DVD_VIDEO_LB_LEN];
        int i_event, i_len;

        if( dvdnav_get_next_block( p_sys->dvdnav, buffer, &i_event, &i_len )
              == DVDNAV_STATUS_ERR )
        {
            msg_Warn( p_demux, "dvdnav_get_next_block failed" );
        }

        dvdnav_sector_search( p_sys->dvdnav, 0, SEEK_SET );
    }

    /* Configure dvdnav */
    if( dvdnav_set_readahead_flag( p_sys->dvdnav, DVD_READ_CACHE ) !=
          DVDNAV_STATUS_OK )
    {
        msg_Warn( p_demux, "cannot set read-a-head flag" );
    }

    if( dvdnav_set_PGC_positioning_flag( p_sys->dvdnav, 1 ) !=
          DVDNAV_STATUS_OK )
    {
        msg_Warn( p_demux, "cannot set PGC positioning flag" );
    }

    /* Set menu language ("en")
     * XXX: maybe it would be better to set it like audio/spu
     * or to create a --menu-language option */
    if( dvdnav_menu_language_select( p_sys->dvdnav,LANGUAGE_DEFAULT ) !=
        DVDNAV_STATUS_OK )
    {
        msg_Warn( p_demux, "can't set menu language to '%s' (%s)",
                  LANGUAGE_DEFAULT, dvdnav_err_to_string( p_sys->dvdnav ) );
    }

    /* Set audio language */
    psz_code = DemuxGetLanguageCode( p_demux, "audio-language" );
    if( dvdnav_audio_language_select( p_sys->dvdnav, psz_code ) !=
        DVDNAV_STATUS_OK )
    {
        msg_Warn( p_demux, "can't set audio language to '%s' (%s)",
                  psz_code, dvdnav_err_to_string( p_sys->dvdnav ) );
        /* We try to fall back to 'en' */
        if( strcmp( psz_code, LANGUAGE_DEFAULT ) )
            dvdnav_audio_language_select( p_sys->dvdnav, LANGUAGE_DEFAULT );
    }
    free( psz_code );

    /* Set spu language */
    psz_code = DemuxGetLanguageCode( p_demux, "sub-language" );
    if( dvdnav_spu_language_select( p_sys->dvdnav, psz_code ) !=
        DVDNAV_STATUS_OK )
    {
        msg_Warn( p_demux, "can't set spu language to '%s' (%s)",
                  psz_code, dvdnav_err_to_string( p_sys->dvdnav ) );
        /* We try to fall back to 'en' */
        if( strcmp( psz_code, LANGUAGE_DEFAULT ) )
            dvdnav_spu_language_select(p_sys->dvdnav, LANGUAGE_DEFAULT );
    }
    free( psz_code );

    DemuxTitles( p_demux );

    var_Create( p_demux, "dvdnav-menu", VLC_VAR_BOOL|VLC_VAR_DOINHERIT );
    var_Get( p_demux, "dvdnav-menu", &val );
    if( val.b_bool )
    {
        msg_Dbg( p_demux, "trying to go to dvd menu" );

        if( dvdnav_title_play( p_sys->dvdnav, 1 ) != DVDNAV_STATUS_OK )
        {
            msg_Err( p_demux, "cannot set title (can't decrypt DVD?)" );
            dvdnav_close( p_sys->dvdnav );
            free( p_sys );
            return VLC_EGENERIC;
        }

        if( dvdnav_menu_call( p_sys->dvdnav, DVD_MENU_Title ) !=
            DVDNAV_STATUS_OK )
        {
            /* Try going to menu root */
            if( dvdnav_menu_call( p_sys->dvdnav, DVD_MENU_Root ) !=
                DVDNAV_STATUS_OK )
                    msg_Warn( p_demux, "cannot go to dvd menu" );
        }
    }

    var_Create( p_demux, "dvdnav-angle", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Get( p_demux, "dvdnav-angle", &val );
    i_angle = val.i_int > 0 ? val.i_int : 1;

    /* Update default_pts to a suitable value for dvdnav access */
    var_Create( p_demux, "dvdnav-caching", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );

    /* FIXME hack hack hack hack FIXME */
    /* Get p_input and create variable */
    p_sys->p_input = vlc_object_find( p_demux, VLC_OBJECT_INPUT, FIND_PARENT );
    var_Create( p_sys->p_input, "x-start", VLC_VAR_INTEGER );
    var_Create( p_sys->p_input, "y-start", VLC_VAR_INTEGER );
    var_Create( p_sys->p_input, "x-end", VLC_VAR_INTEGER );
    var_Create( p_sys->p_input, "y-end", VLC_VAR_INTEGER );
    var_Create( p_sys->p_input, "color", VLC_VAR_ADDRESS );
    var_Create( p_sys->p_input, "menu-palette", VLC_VAR_ADDRESS );
    var_Create( p_sys->p_input, "highlight", VLC_VAR_BOOL );
    var_Create( p_sys->p_input, "highlight-mutex", VLC_VAR_MUTEX );

    /* Now create our event thread catcher */
    p_sys->p_ev = vlc_object_create( p_demux, sizeof( event_thread_t ) );
    p_sys->p_ev->p_demux = p_demux;
    vlc_thread_create( p_sys->p_ev, "dvdnav event thread handler", EventThread,
                       VLC_THREAD_PRIORITY_LOW, VLC_FALSE );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    int i;

    /* stop the event handler */
    p_sys->p_ev->b_die = VLC_TRUE;
    vlc_thread_join( p_sys->p_ev );
    vlc_object_destroy( p_sys->p_ev );

    var_Destroy( p_sys->p_input, "highlight-mutex" );
    var_Destroy( p_sys->p_input, "highlight" );
    var_Destroy( p_sys->p_input, "x-start" );
    var_Destroy( p_sys->p_input, "x-end" );
    var_Destroy( p_sys->p_input, "y-start" );
    var_Destroy( p_sys->p_input, "y-end" );
    var_Destroy( p_sys->p_input, "color" );
    var_Destroy( p_sys->p_input, "menu-palette" );

    vlc_object_release( p_sys->p_input );

    for( i = 0; i < PS_TK_COUNT; i++ )
    {
        ps_track_t *tk = &p_sys->tk[i];
        if( tk->b_seen )
        {
            es_format_Clean( &tk->fmt );
            if( tk->es ) es_out_Del( p_demux->out, tk->es );
        }
    }

    dvdnav_close( p_sys->dvdnav );
    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    vlc_bool_t *pb;
    int64_t *pi64;
    input_title_t ***ppp_title;
    int          *pi_int;
    int i;

    switch( i_query )
    {
        case DEMUX_SET_POSITION:
        case DEMUX_GET_POSITION:
        case DEMUX_GET_TIME:
        case DEMUX_GET_LENGTH:
        {
            uint32_t pos, len;
            if( dvdnav_get_position( p_sys->dvdnav, &pos, &len ) !=
                  DVDNAV_STATUS_OK || len == 0 )
            {
                return VLC_EGENERIC;
            }

            if( i_query == DEMUX_GET_POSITION )
            {
                pf = (double*)va_arg( args, double* );
                *pf = (double)pos / (double)len;
                return VLC_SUCCESS;
            }
            else if( i_query == DEMUX_SET_POSITION )
            {
                f = (double)va_arg( args, double );
                pos = f * len;
                if( dvdnav_sector_search( p_sys->dvdnav, pos, SEEK_SET ) ==
                      DVDNAV_STATUS_OK )
                {
                    return VLC_SUCCESS;
                }
            }
            else if( i_query == DEMUX_GET_TIME )
            {
                pi64 = (int64_t*)va_arg( args, int64_t * );
                if( p_sys->i_pgc_length > 0 )
                {
                    *pi64 = p_sys->i_pgc_length * pos / len;
                    return VLC_SUCCESS;
                }
            }
            else if( i_query == DEMUX_GET_LENGTH )
            {
                pi64 = (int64_t*)va_arg( args, int64_t * );
                if( p_sys->i_pgc_length > 0 )
                {
                    *pi64 = (int64_t)p_sys->i_pgc_length;
                    return VLC_SUCCESS;
                }
            }

            return VLC_EGENERIC;
        }

        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
            /* TODO */
            pb = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
            *pb = VLC_TRUE;
            return VLC_SUCCESS;

        case DEMUX_SET_PAUSE_STATE:
            return VLC_SUCCESS;

        case DEMUX_GET_TITLE_INFO:
            ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
            pi_int    = (int*)va_arg( args, int* );
            *((int*)va_arg( args, int* )) = 0; /* Title offset */
            *((int*)va_arg( args, int* )) = 1; /* Chapter offset */

            /* Duplicate title infos */
            *pi_int = p_sys->i_title;
            *ppp_title = malloc( sizeof( input_title_t ** ) * p_sys->i_title );
            for( i = 0; i < p_sys->i_title; i++ )
            {
                (*ppp_title)[i] = vlc_input_title_Duplicate( p_sys->title[i] );
            }
            return VLC_SUCCESS;

        case DEMUX_SET_TITLE:
            i = (int)va_arg( args, int );
            if( ( i == 0 && dvdnav_menu_call( p_sys->dvdnav, DVD_MENU_Root )
                  != DVDNAV_STATUS_OK ) ||
                ( i != 0 && dvdnav_title_play( p_sys->dvdnav, i )
                  != DVDNAV_STATUS_OK ) )
            {
                msg_Warn( p_demux, "cannot set title/chapter" );
                return VLC_EGENERIC;
            }
            p_demux->info.i_update |=
                INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
            p_demux->info.i_title = i;
            p_demux->info.i_seekpoint = 0;
            return VLC_SUCCESS;

        case DEMUX_SET_SEEKPOINT:
            i = (int)va_arg( args, int );
            if( p_demux->info.i_title == 0 )
            {
                int i_ret;
                /* Special case */
                switch( i )
                {
                case 0:
                    i_ret = dvdnav_menu_call( p_sys->dvdnav, DVD_MENU_Escape );
                    break;
                case 1:
                    i_ret = dvdnav_menu_call( p_sys->dvdnav, DVD_MENU_Root );
                    break;
                case 2:
                    i_ret = dvdnav_menu_call( p_sys->dvdnav, DVD_MENU_Title );
                    break;
                case 3:
                    i_ret = dvdnav_menu_call( p_sys->dvdnav, DVD_MENU_Part );
                    break;
                case 4:
                    i_ret = dvdnav_menu_call( p_sys->dvdnav,
                                              DVD_MENU_Subpicture );
                    break;
                case 5:
                    i_ret = dvdnav_menu_call( p_sys->dvdnav, DVD_MENU_Audio );
                    break;
                case 6:
                    i_ret = dvdnav_menu_call( p_sys->dvdnav, DVD_MENU_Angle );
                    break;
                default:
                    return VLC_EGENERIC;
                }

                if( i_ret != DVDNAV_STATUS_OK )
                    return VLC_EGENERIC;
            }
            else if( dvdnav_part_play( p_sys->dvdnav, p_demux->info.i_title,
                                       i + 1 ) != DVDNAV_STATUS_OK )
            {
                msg_Warn( p_demux, "cannot set title/chapter" );
                return VLC_EGENERIC;
            }
            p_demux->info.i_update |= INPUT_UPDATE_SEEKPOINT;
            p_demux->info.i_seekpoint = i;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = (int64_t)var_GetInteger( p_demux, "dvdnav-caching" ) *1000;
            return VLC_SUCCESS;

        case DEMUX_GET_META:
        {
            const char *title_name = NULL;

            dvdnav_get_title_string(p_sys->dvdnav, &title_name);
            if( (NULL != title_name) && ('\0' != title_name[0]) )
            {
                vlc_meta_t **pp_meta = (vlc_meta_t**)va_arg( args, vlc_meta_t** );
                vlc_meta_t *meta;
                *pp_meta = meta = vlc_meta_New();
                vlc_meta_Add( meta, VLC_META_TITLE, title_name );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    uint8_t buffer[DVD_VIDEO_LB_LEN];
    uint8_t *packet = buffer;
    int i_event;
    int i_len;

#if DVD_READ_CACHE
    if( dvdnav_get_next_cache_block( p_sys->dvdnav, &packet, &i_event, &i_len )
        == DVDNAV_STATUS_ERR )
#else
    if( dvdnav_get_next_block( p_sys->dvdnav, packet, &i_event, &i_len )
        == DVDNAV_STATUS_ERR )
#endif
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

    case DVDNAV_SPU_CLUT_CHANGE:
    {
        int i;

        msg_Dbg( p_demux, "DVDNAV_SPU_CLUT_CHANGE" );
        /* Update color lookup table (16 *uint32_t in packet) */
        memcpy( p_sys->clut, packet, 16 * sizeof( uint32_t ) );

        /* HACK to get the SPU tracks registered in the right order */
        for( i = 0; i < 0x1f; i++ )
        {
            if( dvdnav_spu_stream_to_lang( p_sys->dvdnav, i ) != 0xffff )
                ESNew( p_demux, 0xbd20 + i );
        }
        /* END HACK */
        break;
    }

    case DVDNAV_SPU_STREAM_CHANGE:
    {
        dvdnav_spu_stream_change_event_t *event =
            (dvdnav_spu_stream_change_event_t*)packet;
        int i;

        msg_Dbg( p_demux, "DVDNAV_SPU_STREAM_CHANGE" );
        msg_Dbg( p_demux, "     - physical_wide=%d",
                 event->physical_wide );
        msg_Dbg( p_demux, "     - physical_letterbox=%d",
                 event->physical_letterbox);
        msg_Dbg( p_demux, "     - physical_pan_scan=%d",
                 event->physical_pan_scan );

        ESSubtitleUpdate( p_demux );
        p_sys->b_spu_change = VLC_TRUE;

        /* HACK to get the SPU tracks registered in the right order */
        for( i = 0; i < 0x1f; i++ )
        {
            if( dvdnav_spu_stream_to_lang( p_sys->dvdnav, i ) != 0xffff )
                ESNew( p_demux, 0xbd20 + i );
        }
        /* END HACK */
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
        int32_t i_title = 0;
        int32_t i_part  = 0;
        int i;

        dvdnav_vts_change_event_t *event = (dvdnav_vts_change_event_t*)packet;
        msg_Dbg( p_demux, "DVDNAV_VTS_CHANGE" );
        msg_Dbg( p_demux, "     - vtsN=%d", event->new_vtsN );
        msg_Dbg( p_demux, "     - domain=%d", event->new_domain );

        /* dvdnav_get_video_aspect / dvdnav_get_video_scale_permission */
        /* TODO check if we always have VTS and CELL */
        p_sys->i_aspect = dvdnav_get_video_aspect( p_sys->dvdnav );

        /* reset PCR */
        es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

        for( i = 0; i < PS_TK_COUNT; i++ )
        {
            ps_track_t *tk = &p_sys->tk[i];
            if( tk->b_seen )
            {
                es_format_Clean( &tk->fmt );
                if( tk->es ) es_out_Del( p_demux->out, tk->es );
            }
            tk->b_seen = VLC_FALSE;
        }

        if( dvdnav_current_title_info( p_sys->dvdnav, &i_title,
                                       &i_part ) == DVDNAV_STATUS_OK )
        {
            if( i_title >= 0 && i_title < p_sys->i_title &&
                p_demux->info.i_title != i_title )
            {
                p_demux->info.i_update |= INPUT_UPDATE_TITLE;
                p_demux->info.i_title = i_title;
            }
        }
        break;
    }

    case DVDNAV_CELL_CHANGE:
    {
        int32_t i_title = 0;
        int32_t i_part  = 0;

        dvdnav_cell_change_event_t *event =
            (dvdnav_cell_change_event_t*)packet;
        msg_Dbg( p_demux, "DVDNAV_CELL_CHANGE" );
        msg_Dbg( p_demux, "     - cellN=%d", event->cellN );
        msg_Dbg( p_demux, "     - pgN=%d", event->pgN );
        msg_Dbg( p_demux, "     - cell_length="I64Fd, event->cell_length );
        msg_Dbg( p_demux, "     - pg_length="I64Fd, event->pg_length );
        msg_Dbg( p_demux, "     - pgc_length="I64Fd, event->pgc_length );
        msg_Dbg( p_demux, "     - cell_start="I64Fd, event->cell_start );
        msg_Dbg( p_demux, "     - pg_start="I64Fd, event->pg_start );

        /* Store the lenght in time of the current PGC */
        p_sys->i_pgc_length = event->pgc_length / 90 * 1000;

        /* FIXME is it correct or there is better way to know chapter change */
        if( dvdnav_current_title_info( p_sys->dvdnav, &i_title,
                                       &i_part ) == DVDNAV_STATUS_OK )
        {
            if( i_title >= 0 && i_title < p_sys->i_title &&
                i_part >= 1 && i_part <= p_sys->title[i_title]->i_seekpoint &&
                p_demux->info.i_seekpoint != i_part - 1 )
            {
                p_demux->info.i_update |= INPUT_UPDATE_SEEKPOINT;
                p_demux->info.i_seekpoint = i_part - 1;
            }
        }
        break;
    }

    case DVDNAV_NAV_PACKET:
    {
#ifdef DVDNAV_DEBUG
        msg_Dbg( p_demux, "DVDNAV_NAV_PACKET" );
#endif
        /* A lot of thing to do here :
         *  - handle packet
         *  - fetch pts (for time display)
         *  - ...
         */
        DemuxBlock( p_demux, packet, i_len );
        if( p_sys->b_spu_change ) 
        {
            ButtonUpdate( p_demux, VLC_FALSE );
            p_sys->b_spu_change = VLC_FALSE;
        }
        break;
    }

    case DVDNAV_STOP:   /* EOF */
        msg_Dbg( p_demux, "DVDNAV_STOP" );

#if DVD_READ_CACHE
        dvdnav_free_cache_block( p_sys->dvdnav, packet );
#endif
        return 0;

    case DVDNAV_HIGHLIGHT:
    {
        dvdnav_highlight_event_t *event = (dvdnav_highlight_event_t*)packet;
        msg_Dbg( p_demux, "DVDNAV_HIGHLIGHT" );
        msg_Dbg( p_demux, "     - display=%d", event->display );
        msg_Dbg( p_demux, "     - buttonN=%d", event->buttonN );
        ButtonUpdate( p_demux, VLC_FALSE );
        break;
    }

    case DVDNAV_HOP_CHANNEL:
        msg_Dbg( p_demux, "DVDNAV_HOP_CHANNEL" );
        /* We should try to flush all our internal buffer */
        break;

    case DVDNAV_WAIT:
        msg_Dbg( p_demux, "DVDNAV_WAIT" );

        /* reset PCR */
        es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
        dvdnav_wait_skip( p_sys->dvdnav );
        break;

    default:
        msg_Warn( p_demux, "Unknown event (0x%x)", i_event );
        break;
    }

#if DVD_READ_CACHE
    dvdnav_free_cache_block( p_sys->dvdnav, packet );
#endif

    return 1;
}

/* Get a 2 char code
 * FIXME: partiallyy duplicated from src/input/es_out.c
 */
static char *DemuxGetLanguageCode( demux_t *p_demux, char *psz_var )
{
    const iso639_lang_t *pl;
    char *psz_lang;
    char *p;

    psz_lang = var_CreateGetString( p_demux, psz_var );
    /* XXX: we will use only the first value
     * (and ignore other ones in case of a list) */
    if( ( p = strchr( psz_lang, ',' ) ) ) *p = '\0';

    for( pl = p_languages; pl->psz_iso639_1 != NULL; pl++ )
    {
        if( !strcasecmp( pl->psz_eng_name, psz_lang ) ||
            !strcasecmp( pl->psz_native_name, psz_lang ) ||
            !strcasecmp( pl->psz_iso639_1, psz_lang ) ||
            !strcasecmp( pl->psz_iso639_2T, psz_lang ) ||
            !strcasecmp( pl->psz_iso639_2B, psz_lang ) )
            break;
    }

    free( psz_lang );

    if( pl->psz_iso639_1 != NULL )
        return strdup( pl->psz_iso639_1 );

    return strdup(LANGUAGE_DEFAULT);
}

static void DemuxTitles( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    input_title_t *t;
    seekpoint_t *s;
    int32_t i_titles;
    int i;

    /* Menu */
    t = vlc_input_title_New();
    t->b_menu = VLC_TRUE;
    t->psz_name = strdup( "DVD Menu" );

    s = vlc_seekpoint_New();
    s->psz_name = strdup( "Resume" );
    TAB_APPEND( t->i_seekpoint, t->seekpoint, s );

    s = vlc_seekpoint_New();
    s->psz_name = strdup( "Root" );
    TAB_APPEND( t->i_seekpoint, t->seekpoint, s );

    s = vlc_seekpoint_New();
    s->psz_name = strdup( "Title" );
    TAB_APPEND( t->i_seekpoint, t->seekpoint, s );

    s = vlc_seekpoint_New();
    s->psz_name = strdup( "Chapter" );
    TAB_APPEND( t->i_seekpoint, t->seekpoint, s );

    s = vlc_seekpoint_New();
    s->psz_name = strdup( "Subtitle" );
    TAB_APPEND( t->i_seekpoint, t->seekpoint, s );

    s = vlc_seekpoint_New();
    s->psz_name = strdup( "Audio" );
    TAB_APPEND( t->i_seekpoint, t->seekpoint, s );

    s = vlc_seekpoint_New();
    s->psz_name = strdup( "Angle" );
    TAB_APPEND( t->i_seekpoint, t->seekpoint, s );

    TAB_APPEND( p_sys->i_title, p_sys->title, t );

    /* Find out number of titles/chapters */
    dvdnav_get_number_of_titles( p_sys->dvdnav, &i_titles );
    for( i = 1; i <= i_titles; i++ )
    {
        int32_t i_chapters = 0;
        int j;

        dvdnav_get_number_of_parts( p_sys->dvdnav, i, &i_chapters );

        t = vlc_input_title_New();
        for( j = 0; j < __MAX( i_chapters, 1 ); j++ )
        {
            s = vlc_seekpoint_New();
            TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
        }

        TAB_APPEND( p_sys->i_title, p_sys->title, t );
    }
}

/*****************************************************************************
 * Update functions:
 *****************************************************************************/
static void ButtonUpdate( demux_t *p_demux, vlc_bool_t b_mode )
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

        if( dvdnav_get_current_highlight( p_sys->dvdnav, &i_button )
            != DVDNAV_STATUS_OK )
        {
            msg_Err( p_demux, "dvdnav_get_current_highlight failed" );
            return;
        }

        if( i_button > 0 && i_title ==  0 )
        {
            int i;
            pci_t *pci = dvdnav_get_current_nav_pci( p_sys->dvdnav );

            dvdnav_get_highlight_area( pci, i_button, b_mode, &hl );

            for( i = 0; i < 4; i++ )
            {
                uint32_t i_yuv = p_sys->clut[(hl.palette>>(16+i*4))&0x0f];
                uint8_t i_alpha = (hl.palette>>(i*4))&0x0f;
                i_alpha = i_alpha == 0xf ? 0xff : i_alpha << 4;

                p_sys->palette[i][0] = (i_yuv >> 16) & 0xff;
                p_sys->palette[i][1] = (i_yuv >> 0) & 0xff;
                p_sys->palette[i][2] = (i_yuv >> 8) & 0xff;
                p_sys->palette[i][3] = i_alpha;
            }

            vlc_mutex_lock( p_mutex );
            val.i_int = hl.sx; var_Set( p_sys->p_input, "x-start", val );
            val.i_int = hl.ex; var_Set( p_sys->p_input, "x-end", val );
            val.i_int = hl.sy; var_Set( p_sys->p_input, "y-start", val );
            val.i_int = hl.ey; var_Set( p_sys->p_input, "y-end", val );

            val.p_address = (void *)p_sys->palette;
            var_Set( p_sys->p_input, "menu-palette", val );

            val.b_bool = VLC_TRUE; var_Set( p_sys->p_input, "highlight", val );
            vlc_mutex_unlock( p_mutex );

            msg_Dbg( p_demux, "buttonUpdate %d", i_button );
        }
        else
        {
            msg_Dbg( p_demux, "buttonUpdate not done b=%d t=%d",
                     i_button, i_title );

            /* Show all */
            vlc_mutex_lock( p_mutex );
            val.b_bool = VLC_FALSE;
            var_Set( p_sys->p_input, "highlight", val );
            vlc_mutex_unlock( p_mutex );
        }
    }
}

static void ESSubtitleUpdate( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int         i_spu = dvdnav_get_active_spu_stream( p_sys->dvdnav );
    int32_t i_title, i_part;

    ButtonUpdate( p_demux, VLC_FALSE );

    dvdnav_current_title_info( p_sys->dvdnav, &i_title, &i_part );
    if( i_title > 0 ) return;

    if( i_spu >= 0 && i_spu <= 0x1f )
    {
        ps_track_t *tk = &p_sys->tk[PS_ID_TO_TK(0xbd20 + i_spu)];

        ESNew( p_demux, 0xbd20 + i_spu );

        /* be sure to unselect it (reset) */
        es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE, tk->es,
                        (vlc_bool_t)VLC_FALSE );

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
                es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE, tk->es,
                                (vlc_bool_t)VLC_FALSE );
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
#ifdef DVDNAV_DEBUG
            if( p[3] == 0xbc )
            {
                msg_Warn( p_demux, "received a PSM packet" );
            }
            else if( p[3] == 0xbb )
            {
                msg_Warn( p_demux, "received a SYSTEM packet" );
            }
#endif
            block_Release( p_pkt );
            break;

        case 0x1ba:
        {
            int64_t i_scr;
            int i_mux_rate;
            if( !ps_pkt_parse_pack( p_pkt, &i_scr, &i_mux_rate ) )
            {
                es_out_Control( p_demux->out, ES_OUT_SET_PCR, i_scr );
                if( i_mux_rate > 0 ) p_sys->i_mux_rate = i_mux_rate;
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

/*****************************************************************************
 * ESNew: register a new elementary stream
 *****************************************************************************/
static void ESNew( demux_t *p_demux, int i_id )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    ps_track_t  *tk = &p_sys->tk[PS_ID_TO_TK(i_id)];
    vlc_bool_t  b_select = VLC_FALSE;

    if( tk->b_seen ) return;

    if( ps_track_fill( tk, 0, i_id ) )
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
            int i_lang = dvdnav_audio_stream_to_lang( p_sys->dvdnav, i_audio );
            if( i_lang != 0xffff )
            {
                tk->fmt.psz_language = malloc( 3 );
                tk->fmt.psz_language[0] = (i_lang >> 8)&0xff;
                tk->fmt.psz_language[1] = (i_lang     )&0xff;
                tk->fmt.psz_language[2] = 0;
            }
            if( dvdnav_get_active_audio_stream( p_sys->dvdnav ) == i_audio )
            {
                b_select = VLC_TRUE;
            }
        }
    }
    else if( tk->fmt.i_cat == SPU_ES )
    {
        int32_t i_title, i_part;
        int i_lang = dvdnav_spu_stream_to_lang( p_sys->dvdnav, i_id&0x1f );
        if( i_lang != 0xffff )
        {
            tk->fmt.psz_language = malloc( 3 );
            tk->fmt.psz_language[0] = (i_lang >> 8)&0xff;
            tk->fmt.psz_language[1] = (i_lang     )&0xff;
            tk->fmt.psz_language[2] = 0;
        }

        /* Palette */
        tk->fmt.subs.spu.palette[0] = 0xBeef;
        memcpy( &tk->fmt.subs.spu.palette[1], p_sys->clut,
                16 * sizeof( uint32_t ) );

        /* We select only when we are not in the menu */
        dvdnav_current_title_info( p_sys->dvdnav, &i_title, &i_part );
        if( i_title > 0 &&
            dvdnav_get_active_spu_stream( p_sys->dvdnav ) == (i_id&0x1f) )
        {
            b_select = VLC_TRUE;
        }
    }

    tk->es = es_out_Add( p_demux->out, &tk->fmt );
    if( b_select )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_ES, tk->es );
    }
    tk->b_seen = VLC_TRUE;

    if( tk->fmt.i_cat == VIDEO_ES ) ButtonUpdate( p_demux, VLC_FALSE );
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
                ButtonUpdate( p_ev->p_demux, VLC_TRUE );
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
            pci_t *pci = dvdnav_get_current_nav_pci( p_sys->dvdnav );
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
                ButtonUpdate( p_ev->p_demux, VLC_TRUE );
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

/*****************************************************************************
 * ProbeDVD: very weak probing that avoids going too often into a dvdnav_open()
 *****************************************************************************/
static int ProbeDVD( demux_t *p_demux, char *psz_name )
{
#ifdef HAVE_SYS_STAT_H
    struct stat stat_info;
    uint8_t pi_anchor[2];
    uint16_t i_tag_id = 0;
    int i_fd, i_ret;

    if( !*psz_name )
    {
        /* Triggers libdvdcss autodetection */
        return VLC_SUCCESS;
    }

    if( stat( psz_name, &stat_info ) || !S_ISREG( stat_info.st_mode ) )
    {
        /* Let dvdnav_open() do the probing */
        return VLC_SUCCESS;
    }

    if( (i_fd = open( psz_name, O_RDONLY )) == -1 )
    {
        /* Let dvdnav_open() do the probing */
        return VLC_SUCCESS;
    }

    /* Try to find the anchor (2 bytes at LBA 256) */
    i_ret = VLC_SUCCESS;
    if( lseek( i_fd, 256 * DVD_VIDEO_LB_LEN, SEEK_SET ) == -1 )
    {
        i_ret = VLC_EGENERIC;
    }

    if( read( i_fd, pi_anchor, 2 ) == 2 )
    {
        i_tag_id = GetWLE(pi_anchor);
        if( i_tag_id != 2 ) i_ret = VLC_EGENERIC; /* Not an anchor */
    }
    else
    {
        i_ret = VLC_EGENERIC;
    }

    close( i_fd );

    return i_ret;
#else

    return VLC_SUCCESS;
#endif
}
