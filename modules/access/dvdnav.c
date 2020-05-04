/*****************************************************************************
 * dvdnav.c: DVD module using the dvdnav library.
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
 * NOTA BENE: this module requires the linking against a library which is
 * known to require licensing under the GNU General Public License version 2
 * (or later). Therefore, the result of compiling this module will normally
 * be subject to the terms of that later license.
 *****************************************************************************/


/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>     /* close() */

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_demux.h>
#include <vlc_charset.h>
#include <vlc_fs.h>
#include <vlc_mouse.h>
#include <vlc_dialog.h>
#include <vlc_iso_lang.h>

/* FIXME we should find a better way than including that */
#include "../../src/text/iso-639_def.h"


#include <dvdnav/dvdnav.h>
/* Expose without patching headers */
dvdnav_status_t dvdnav_jump_to_sector_by_time(dvdnav_t *, uint64_t, int32_t);

#include "../demux/mpeg/pes.h"
#include "../demux/mpeg/ps.h"
#include "../demux/timestamps_filter.h"

#include "disc_helper.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ANGLE_TEXT N_("DVD angle")
#define ANGLE_LONGTEXT N_( \
     "Default DVD angle." )

#define MENU_TEXT N_("Start directly in menu")
#define MENU_LONGTEXT N_( \
    "Start the DVD directly in the main menu. This "\
    "will try to skip all the useless warning introductions." )

#define LANGUAGE_DEFAULT ("en")

static int  AccessDemuxOpen ( vlc_object_t * );
static void Close( vlc_object_t * );

static int  DemuxOpen ( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("DVD with menus") )
    set_description( N_("DVDnav Input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_integer( "dvdnav-angle", 1, ANGLE_TEXT,
        ANGLE_LONGTEXT, false )
    add_bool( "dvdnav-menu", true,
        MENU_TEXT, MENU_LONGTEXT, false )
    set_capability( "access", 305 )
    add_shortcut( "dvd", "dvdnav", "file" )
    set_callbacks( AccessDemuxOpen, Close )
    add_submodule()
        set_description( N_("DVDnav demuxer") )
        set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_DEMUX )
        set_capability( "demux", 5 )
        set_callbacks( DemuxOpen, Close )
        add_shortcut( "dvd", "iso" )
vlc_module_end ()

/* Shall we use libdvdnav's read ahead cache? */
#ifdef __OS2__
#define DVD_READ_CACHE 0
#else
#define DVD_READ_CACHE 1
#endif

#define BLOCK_FLAG_CELL_DISCONTINUITY (BLOCK_FLAG_PRIVATE_SHIFT << 1)

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct
{
    dvdnav_t    *dvdnav;
    es_out_t    *p_tf_out;

    /* */
    bool        b_reset_pcr;
    bool        b_readahead;

    struct
    {
        bool         b_created;
        bool         b_enabled;
        vlc_mutex_t  lock;
        vlc_timer_t  timer;
    } still;

    /* track */
    ps_track_t  tk[PS_TK_COUNT];
    int         i_mux_rate;

    vlc_mutex_t event_lock;
    es_out_id_t *spu_es;

    /* palette for menus */
    uint32_t clut[16];
    bool b_spu_change;
    struct
    {
        bool b_pending;
        bool b_from_user;
    } highlight;

    /* Aspect ration */
    struct {
        unsigned i_num;
        unsigned i_den;
    } sar;

    /* */
    int           i_title;
    input_title_t **title;
    int           cur_title;
    int           cur_seekpoint;
    unsigned      updates;

    /* length of program group chain */
    vlc_tick_t  i_pgc_length;
    int         i_vobu_index;
    int         i_vobu_flush;

    vlc_mouse_t oldmouse;
} demux_sys_t;

static int Control( demux_t *, int, va_list );
static int Demux( demux_t * );
static int DemuxBlock( demux_t *, const uint8_t *, int );
static void DemuxForceStill( demux_t * );

static void DemuxTitles( demux_t * );
static void ESSubtitleUpdate( demux_t * );
static void ButtonUpdate( demux_t *, bool );

static void ESNew( demux_t *, int );
static int ProbeDVD( const char * );

static char *DemuxGetLanguageCode( demux_t *p_demux, const char *psz_var );

static int ControlInternal( demux_t *, int, ... );

static void StillTimer( void * );

static void EventMouse( const vlc_mouse_t *mouse, void *p_data );

#if DVDNAV_VERSION >= 60100
static void DvdNavLog( void *foo, dvdnav_logger_level_t i, const char *p, va_list z)
{
    msg_GenericVa( (demux_t*)foo, i, p, z );
}
#endif

/*****************************************************************************
 *
 *****************************************************************************/
static const struct
{
    DVDMenuID_t dvdnav_id;
    const char *psz_name;
} menus_id_mapping[] = {
    { DVD_MENU_Escape,     "Resume" },
    { DVD_MENU_Root,       "Root" },
    { DVD_MENU_Title,      "Title" },
    { DVD_MENU_Part,       "Chapter" },
    { DVD_MENU_Subpicture, "Subtitle" },
    { DVD_MENU_Audio,      "Audio" },
    { DVD_MENU_Angle,      "Angle" },
};

static int MenuIDToSeekpoint( DVDMenuID_t menuid, int *seekpoint )
{
    for( size_t i=0; i<ARRAY_SIZE(menus_id_mapping); i++ )
    {
        if( menus_id_mapping[i].dvdnav_id == menuid )
        {
            *seekpoint = i;
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

static int SeekpointToMenuID( int seekpoint, DVDMenuID_t *id )
{
    if( (size_t) seekpoint >= ARRAY_SIZE(menus_id_mapping) )
        return VLC_EGENERIC;
    *id = menus_id_mapping[seekpoint].dvdnav_id;
    return VLC_SUCCESS;
}

static int CallRootTitleMenu( dvdnav_t *p_dvdnav,
                              int *pi_title, int *pi_seekpoint )
{
    const DVDMenuID_t menuids[2] = { DVD_MENU_Title, DVD_MENU_Root };
    for( int i=0; i<2; i++ )
    {
        if( dvdnav_menu_call( p_dvdnav, menuids[i] )
            == DVDNAV_STATUS_OK )
        {
            *pi_title = 0;
            MenuIDToSeekpoint( menuids[i], pi_seekpoint );
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

/*****************************************************************************
 * CommonOpen:
 *****************************************************************************/
static int CommonOpen( vlc_object_t *p_this,
                       dvdnav_t *p_dvdnav, bool b_readahead )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    int         i_angle;
    char        *psz_code;

    assert( p_dvdnav );

    /* Fill p_demux field */
    DEMUX_INIT_COMMON(); p_sys = p_demux->p_sys;
    p_sys->dvdnav = p_dvdnav;
    p_sys->p_tf_out = timestamps_filter_es_out_New( p_demux->out );
    if( !p_sys->p_tf_out )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    ps_track_init( p_sys->tk );
    p_sys->b_readahead = b_readahead;
    vlc_mouse_Init( &p_sys->oldmouse );

    /* Configure dvdnav */
    if( dvdnav_set_readahead_flag( p_sys->dvdnav, p_sys->b_readahead ) !=
          DVDNAV_STATUS_OK )
    {
        msg_Warn( p_demux, "cannot set read-a-head flag" );
    }

    if( dvdnav_set_PGC_positioning_flag( p_sys->dvdnav, 1 ) !=
          DVDNAV_STATUS_OK )
    {
        msg_Warn( p_demux, "cannot set PGC positioning flag" );
    }

    /* Set menu language */
    psz_code = DemuxGetLanguageCode( p_demux, "menu-language" );
    if( dvdnav_menu_language_select( p_sys->dvdnav, psz_code ) !=
        DVDNAV_STATUS_OK )
    {
        msg_Warn( p_demux, "can't set menu language to '%s' (%s)",
                  psz_code, dvdnav_err_to_string( p_sys->dvdnav ) );
        /* We try to fall back to 'en' */
        if( strcmp( psz_code, LANGUAGE_DEFAULT ) )
            dvdnav_menu_language_select( p_sys->dvdnav, (char*)LANGUAGE_DEFAULT );
    }
    free( psz_code );

    /* Set audio language */
    psz_code = DemuxGetLanguageCode( p_demux, "audio-language" );
    if( dvdnav_audio_language_select( p_sys->dvdnav, psz_code ) !=
        DVDNAV_STATUS_OK )
    {
        msg_Warn( p_demux, "can't set audio language to '%s' (%s)",
                  psz_code, dvdnav_err_to_string( p_sys->dvdnav ) );
        /* We try to fall back to 'en' */
        if( strcmp( psz_code, LANGUAGE_DEFAULT ) )
            dvdnav_audio_language_select( p_sys->dvdnav, (char*)LANGUAGE_DEFAULT );
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
            dvdnav_spu_language_select(p_sys->dvdnav, (char*)LANGUAGE_DEFAULT );
    }
    free( psz_code );

    DemuxTitles( p_demux );

    if( var_CreateGetBool( p_demux, "dvdnav-menu" ) )
    {
        msg_Dbg( p_demux, "trying to go to dvd menu" );

        if( dvdnav_title_play( p_sys->dvdnav, 1 ) != DVDNAV_STATUS_OK )
        {
            msg_Err( p_demux, "cannot set title (can't decrypt DVD?)" );
            vlc_dialog_display_error( p_demux, _("Playback failure"), "%s",
                _("VLC cannot set the DVD's title. It possibly "
                  "cannot decrypt the entire disc.") );
            timestamps_filter_es_out_Delete( p_sys->p_tf_out );
            free( p_sys );
            return VLC_EGENERIC;
        }

        if( CallRootTitleMenu( p_sys->dvdnav, &p_sys->cur_title,
                                              &p_sys->cur_seekpoint ) )
            msg_Warn( p_demux, "cannot go to dvd menu" );
    }

    i_angle = var_CreateGetInteger( p_demux, "dvdnav-angle" );
    if( i_angle <= 0 ) i_angle = 1;

    p_sys->still.b_enabled = false;
    vlc_mutex_init( &p_sys->still.lock );
    vlc_mutex_init( &p_sys->event_lock );
    if( !vlc_timer_create( &p_sys->still.timer, StillTimer, p_sys ) )
        p_sys->still.b_created = true;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * AccessDemuxOpen:
 *****************************************************************************/
static int AccessDemuxOpen ( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    dvdnav_t *p_dvdnav = NULL;
    char *psz_file = NULL;
    const char *psz_path = NULL;
    int i_ret = VLC_EGENERIC;
    bool forced = false;

    if (p_demux->out == NULL)
        return VLC_EGENERIC;

    if( !strncasecmp(p_demux->psz_url, "dvd", 3) )
        forced = true;

    if( !p_demux->psz_filepath || !*p_demux->psz_filepath )
    {
        /* Only when selected */
        if( !forced )
            return VLC_EGENERIC;

        psz_file = var_InheritString( p_this, "dvd" );
    }
    else
        psz_file = strdup( p_demux->psz_filepath );

#if defined( _WIN32 ) || defined( __OS2__ )
    if( psz_file != NULL )
    {
        /* Remove trailing backslash, otherwise dvdnav_open will fail */
        size_t flen = strlen( psz_file );
        if( flen > 0 && psz_file[flen - 1] == '\\' )
            psz_file[flen - 1] = '\0';
    }
    else
        psz_file = strdup("");
#endif

    if( unlikely(psz_file == NULL) )
        return VLC_EGENERIC;

    /* Try some simple probing to avoid going through dvdnav_open too often */
    if( !forced && ProbeDVD( psz_file ) != VLC_SUCCESS )
        goto bailout;

    if( forced && DiscProbeMacOSPermission( p_this, psz_file ) != VLC_SUCCESS )
        goto bailout;

    /* Open dvdnav */
    psz_path = ToLocale( psz_file );
#if DVDNAV_VERSION >= 60100
    dvdnav_logger_cb cbs;
    cbs.pf_log = DvdNavLog;
    if( dvdnav_open2( &p_dvdnav, p_demux, &cbs, psz_path  ) != DVDNAV_STATUS_OK )
#else
    if( dvdnav_open( &p_dvdnav, psz_path  ) != DVDNAV_STATUS_OK )
#endif
    {
        msg_Warn( p_demux, "cannot open DVD (%s)", psz_file);
        goto bailout;
    }

    i_ret = CommonOpen( p_this, p_dvdnav, !!DVD_READ_CACHE );
    if( i_ret != VLC_SUCCESS )
        dvdnav_close( p_dvdnav );

bailout:
    free( psz_file );
    if( psz_path )
        LocaleFree( psz_path );
    return i_ret;
}

/*****************************************************************************
 * StreamProbeDVD: very weak probing that avoids going too often into a dvdnav_open()
 *****************************************************************************/
static int StreamProbeDVD( stream_t *s )
{
    /* first sector should be filled with zeros */
    ssize_t i_peek;
    const uint8_t *p_peek;
    i_peek = vlc_stream_Peek( s, &p_peek, 2048 );
    if( i_peek < 512 ) {
        return VLC_EGENERIC;
    }
    while (i_peek > 0) {
        if (p_peek[ --i_peek ]) {
            return VLC_EGENERIC;
        }
    }

    /* ISO 9660 volume descriptor */
    char iso_dsc[6];
    if( vlc_stream_Seek( s, 0x8000 + 1 ) != VLC_SUCCESS
     || vlc_stream_Read( s, iso_dsc, sizeof (iso_dsc) ) < (int)sizeof (iso_dsc)
     || memcmp( iso_dsc, "CD001\x01", 6 ) )
        return VLC_EGENERIC;

    /* Try to find the anchor (2 bytes at LBA 256) */
    uint16_t anchor;

    if( vlc_stream_Seek( s, 256 * DVD_VIDEO_LB_LEN ) == VLC_SUCCESS
     && vlc_stream_Read( s, &anchor, 2 ) == 2
     && GetWLE( &anchor ) == 2 )
        return VLC_SUCCESS;
    else
        return VLC_EGENERIC;
}

/*****************************************************************************
 * dvdnav stream callbacks
 *****************************************************************************/
static int stream_cb_seek( void *demux, uint64_t pos )
{
    return vlc_stream_Seek( ((demux_t *)demux)->s, pos );
}

static int stream_cb_read( void *demux, void* buffer, int size )
{
    return vlc_stream_Read( ((demux_t *)demux)->s, buffer, size );
}

/*****************************************************************************
 * DemuxOpen:
 *****************************************************************************/
static int DemuxOpen ( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    dvdnav_t *p_dvdnav = NULL;
    bool forced = false, b_seekable = false;

    if( p_demux->psz_name != NULL && !strncmp(p_demux->psz_name, "dvd", 3) )
        forced = true;

    /* StreamProbeDVD need FASTSEEK, but if dvd is forced, we don't probe thus
     * don't need fastseek */
    vlc_stream_Control( p_demux->s, forced ? STREAM_CAN_SEEK : STREAM_CAN_FASTSEEK,
                    &b_seekable );
    if( !b_seekable )
        return VLC_EGENERIC;

    /* Try some simple probing to avoid going through dvdnav_open too often */
    if( !forced && StreamProbeDVD( p_demux->s ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    static dvdnav_stream_cb stream_cb =
    {
        .pf_seek = stream_cb_seek,
        .pf_read = stream_cb_read,
        .pf_readv = NULL,
    };

    /* Open dvdnav with stream callbacks */
#if DVDNAV_VERSION >= 60100
    dvdnav_logger_cb cbs;
    cbs.pf_log = DvdNavLog;
    if( dvdnav_open_stream2( &p_dvdnav, p_demux,
                             &cbs, &stream_cb ) != DVDNAV_STATUS_OK )
#else
    if( dvdnav_open_stream( &p_dvdnav, p_demux,
                            &stream_cb ) != DVDNAV_STATUS_OK )
#endif
    {
        msg_Warn( p_demux, "cannot open DVD with open_stream" );
        return VLC_EGENERIC;
    }

    int i_ret = CommonOpen( p_this, p_dvdnav, false );
    if( i_ret != VLC_SUCCESS )
        dvdnav_close( p_dvdnav );
    return i_ret;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    /* Stop still image handler */
    if( p_sys->still.b_created )
        vlc_timer_destroy( p_sys->still.timer );

    for( int i = 0; i < PS_TK_COUNT; i++ )
    {
        ps_track_t *tk = &p_sys->tk[i];
        if( tk->b_configured )
        {
            es_format_Clean( &tk->fmt );
            if( tk->es ) es_out_Del( p_sys->p_tf_out, tk->es );
        }
    }

    /* Free the array of titles */
    for( int i = 0; i < p_sys->i_title; i++ )
        vlc_input_title_Delete( p_sys->title[i] );
    TAB_CLEAN( p_sys->i_title, p_sys->title );

    timestamps_filter_es_out_Delete( p_sys->p_tf_out );

    dvdnav_close( p_sys->dvdnav );
    free( p_sys );
}


/*****************************************************************************
 * Reset variables on Random Access:
 *****************************************************************************/
static void RandomAccessCleanup(demux_sys_t *p_sys)
{
    p_sys->highlight.b_pending = false;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    input_title_t ***ppp_title;
    int i;

    switch( i_query )
    {
        case DEMUX_SET_POSITION:
        case DEMUX_GET_POSITION:
        case DEMUX_GET_LENGTH:
        {
            uint32_t pos, len;
            if( dvdnav_get_position( p_sys->dvdnav, &pos, &len ) !=
                  DVDNAV_STATUS_OK || len == 0 )
            {
                return VLC_EGENERIC;
            }

            switch( i_query )
            {
            case DEMUX_GET_POSITION:
                *va_arg( args, double* ) = (double)pos / (double)len;
                return VLC_SUCCESS;

            case DEMUX_SET_POSITION:
                pos = va_arg( args, double ) * len;
                if( dvdnav_sector_search( p_sys->dvdnav, pos, SEEK_SET ) ==
                      DVDNAV_STATUS_OK )
                {
                    return VLC_SUCCESS;
                }
                RandomAccessCleanup( p_sys );
                break;

            case DEMUX_GET_LENGTH:
                if( p_sys->i_pgc_length > 0 )
                {
                    *va_arg( args, vlc_tick_t * ) = p_sys->i_pgc_length;
                    return VLC_SUCCESS;
                }
                break;
            }
            return VLC_EGENERIC;
        }

        case DEMUX_GET_TIME:
            if( p_sys->i_pgc_length > 0 )
            {
                *va_arg( args, vlc_tick_t * ) =
                        dvdnav_get_current_time( p_sys->dvdnav ) * 100 / 9;
                return VLC_SUCCESS;
            }
            break;

        case DEMUX_SET_TIME:
        {
            vlc_tick_t i_time = va_arg( args, vlc_tick_t );
            if( dvdnav_jump_to_sector_by_time( p_sys->dvdnav,
                                               i_time * 9 / 100,
                                               SEEK_SET ) == DVDNAV_STATUS_OK )
                return VLC_SUCCESS;
            msg_Err( p_demux, "can't set time to %" PRId64, i_time );
            return VLC_EGENERIC;
        }

        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_CONTROL_PACE:
            /* TODO */
            *va_arg( args, bool * ) = true;
            return VLC_SUCCESS;

        case DEMUX_SET_PAUSE_STATE:
            return VLC_SUCCESS;

        case DEMUX_GET_TITLE_INFO:
            ppp_title = va_arg( args, input_title_t*** );

            /* Duplicate title infos */
            *ppp_title = vlc_alloc( p_sys->i_title, sizeof( input_title_t * ) );
            if( !*ppp_title )
                return VLC_EGENERIC;
            for( i = 0; i < p_sys->i_title; i++ )
            {
                (*ppp_title)[i] = vlc_input_title_Duplicate( p_sys->title[i] );
                if(!(*ppp_title)[i])
                {
                    while( i )
                        free( (*ppp_title)[--i] );
                    free( *ppp_title );
                    return VLC_EGENERIC;
                }
            }
            *va_arg( args, int* ) = p_sys->i_title;
            *va_arg( args, int* ) = 0; /* Title offset */
            *va_arg( args, int* ) = 1; /* Chapter offset */
            return VLC_SUCCESS;

        case DEMUX_SET_TITLE:
            i = va_arg( args, int );
            if( i == 0 && dvdnav_menu_call( p_sys->dvdnav, DVD_MENU_Root )
                  != DVDNAV_STATUS_OK )
            {
                msg_Warn( p_demux, "cannot set title/chapter" );
                return VLC_EGENERIC;
            }

            if( i != 0 )
            {
                dvdnav_still_skip( p_sys->dvdnav );
                if( dvdnav_title_play( p_sys->dvdnav, i ) != DVDNAV_STATUS_OK )
                {
                    msg_Warn( p_demux, "cannot set title/chapter" );
                    return VLC_EGENERIC;
                }
            }

            p_sys->updates |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_title = i;
            if( i != 0 )
                p_sys->cur_seekpoint = 0;
            else
                MenuIDToSeekpoint( DVD_MENU_Root, &p_sys->cur_seekpoint );
            RandomAccessCleanup( p_sys );
            return VLC_SUCCESS;

        case DEMUX_SET_SEEKPOINT:
            i = va_arg( args, int );
            if( p_sys->cur_title == 0 )
            {
                DVDMenuID_t menuid;
                if( SeekpointToMenuID(i, &menuid) ||
                    dvdnav_menu_call(p_sys->dvdnav, menuid) != DVDNAV_STATUS_OK )
                    return VLC_EGENERIC;
            }
            else if( dvdnav_part_play( p_sys->dvdnav, p_sys->cur_title,
                                       i + 1 ) != DVDNAV_STATUS_OK )
            {
                msg_Warn( p_demux, "cannot set title/chapter" );
                return VLC_EGENERIC;
            }
            p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_seekpoint = i;
            RandomAccessCleanup( p_sys );
            return VLC_SUCCESS;

        case DEMUX_TEST_AND_CLEAR_FLAGS:
        {
            unsigned *restrict flags = va_arg(args, unsigned *);
            *flags &= p_sys->updates;
            p_sys->updates &= ~*flags;
            break;
        }

        case DEMUX_GET_TITLE:
            *va_arg( args, int * ) = p_sys->cur_title;
            break;

        case DEMUX_GET_SEEKPOINT:
            *va_arg( args, int * ) = p_sys->cur_seekpoint;
            break;

        case DEMUX_GET_PTS_DELAY:
            *va_arg( args, vlc_tick_t * ) =
                VLC_TICK_FROM_MS( var_InheritInteger( p_demux, "disc-caching" ) );
            return VLC_SUCCESS;

        case DEMUX_GET_META:
        {
            const char *title_name = NULL;

            dvdnav_get_title_string(p_sys->dvdnav, &title_name);
            if( (NULL != title_name) && ('\0' != title_name[0]) && IsUTF8(title_name) )
            {
                vlc_meta_t *p_meta = va_arg( args, vlc_meta_t* );
                vlc_meta_Set( p_meta, vlc_meta_Title, title_name );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }

        case DEMUX_NAV_ACTIVATE:
        {
            pci_t *pci = dvdnav_get_current_nav_pci( p_sys->dvdnav );

            if( dvdnav_button_activate( p_sys->dvdnav, pci ) != DVDNAV_STATUS_OK )
                return VLC_EGENERIC;
            vlc_mutex_lock( &p_sys->event_lock );
            ButtonUpdate( p_demux, true );
            vlc_mutex_unlock( &p_sys->event_lock );
            break;
        }

        case DEMUX_NAV_UP:
        {
            pci_t *pci = dvdnav_get_current_nav_pci( p_sys->dvdnav );

            if( dvdnav_upper_button_select( p_sys->dvdnav, pci ) != DVDNAV_STATUS_OK )
                return VLC_EGENERIC;
            break;
        }

        case DEMUX_NAV_DOWN:
        {
            pci_t *pci = dvdnav_get_current_nav_pci( p_sys->dvdnav );

            if( dvdnav_lower_button_select( p_sys->dvdnav, pci ) != DVDNAV_STATUS_OK )
                return VLC_EGENERIC;
            break;
        }

        case DEMUX_NAV_LEFT:
        {
            pci_t *pci = dvdnav_get_current_nav_pci( p_sys->dvdnav );

            if( dvdnav_left_button_select( p_sys->dvdnav, pci ) != DVDNAV_STATUS_OK )
                return VLC_EGENERIC;
            break;
        }

        case DEMUX_NAV_RIGHT:
        {
            pci_t *pci = dvdnav_get_current_nav_pci( p_sys->dvdnav );

            if( dvdnav_right_button_select( p_sys->dvdnav, pci ) != DVDNAV_STATUS_OK )
                return VLC_EGENERIC;
            break;
        }

        case DEMUX_NAV_MENU:
        {
            if( CallRootTitleMenu( p_sys->dvdnav, &p_sys->cur_title,
                                                  &p_sys->cur_seekpoint ) )
            {
                msg_Warn( p_demux, "cannot select Title/Root menu" );
                return VLC_EGENERIC;
            }
            p_sys->updates |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
            break;
        }

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int ControlInternal( demux_t *p_demux, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = Control( p_demux, i_query, args );
    va_end( args );

    return i_result;
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
    dvdnav_status_t status;

    if( p_sys->b_readahead )
        status = dvdnav_get_next_cache_block( p_sys->dvdnav, &packet, &i_event,
                                              &i_len );
    else
        status = dvdnav_get_next_block( p_sys->dvdnav, packet, &i_event,
                                        &i_len );
    if( status == DVDNAV_STATUS_ERR )
    {
        msg_Warn( p_demux, "cannot get next block (%s)",
                  dvdnav_err_to_string( p_sys->dvdnav ) );
        if( p_sys->cur_title == 0 )
        {
            msg_Dbg( p_demux, "jumping to first title" );
            return ControlInternal( p_demux, DEMUX_SET_TITLE, 1 ) == VLC_SUCCESS ?
                        VLC_DEMUXER_SUCCESS : VLC_DEMUXER_EGENERIC;
        }
        return VLC_DEMUXER_EGENERIC;
    }

    vlc_mutex_lock( &p_sys->event_lock );
    if(p_sys->highlight.b_pending)
        ButtonUpdate(p_demux, p_sys->highlight.b_from_user);
    vlc_mutex_unlock( &p_sys->event_lock );

    switch( i_event )
    {
    case DVDNAV_BLOCK_OK:   /* mpeg block */
        vlc_mutex_lock( &p_sys->still.lock );
        vlc_timer_disarm( p_sys->still.timer );
        p_sys->still.b_enabled = false;
        vlc_mutex_unlock( &p_sys->still.lock );
        if( p_sys->b_reset_pcr )
        {
            es_out_Control( p_sys->p_tf_out, ES_OUT_RESET_PCR );
            p_sys->b_reset_pcr = false;
        }
        DemuxBlock( p_demux, packet, i_len );
        if( p_sys->i_vobu_index > 0 )
        {
            if( p_sys->i_vobu_flush == p_sys->i_vobu_index )
                DemuxForceStill( p_demux );
            p_sys->i_vobu_index++;
        }
        break;

    case DVDNAV_NOP:    /* Nothing */
        msg_Dbg( p_demux, "DVDNAV_NOP" );
        break;

    case DVDNAV_STILL_FRAME:
    {
        dvdnav_still_event_t *event = (dvdnav_still_event_t*)packet;
        bool b_still_init = false;

        vlc_mutex_lock( &p_sys->still.lock );
        if( !p_sys->still.b_enabled )
        {
            msg_Dbg( p_demux, "DVDNAV_STILL_FRAME" );
            msg_Dbg( p_demux, "     - length=0x%x", event->length );
            p_sys->still.b_enabled = true;

            if( event->length != 0xff && p_sys->still.b_created )
            {
                vlc_tick_t delay = vlc_tick_from_sec( event->length );
                vlc_timer_schedule( p_sys->still.timer, false, delay, VLC_TIMER_FIRE_ONCE );
            }

            b_still_init = true;
        }
        vlc_mutex_unlock( &p_sys->still.lock );

        if( b_still_init )
        {
            DemuxForceStill( p_demux );
            p_sys->b_reset_pcr = true;
        }
        vlc_tick_sleep( VLC_TICK_FROM_MS(40) );
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
        p_sys->b_spu_change = true;

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

        dvdnav_vts_change_event_t *event = (dvdnav_vts_change_event_t*)packet;
        msg_Dbg( p_demux, "DVDNAV_VTS_CHANGE" );
        msg_Dbg( p_demux, "     - vtsN=%d", event->new_vtsN );
        msg_Dbg( p_demux, "     - domain=%d", event->new_domain );

        /* reset PCR */
        es_out_Control( p_sys->p_tf_out, ES_OUT_RESET_PCR );

        for( int i = 0; i < PS_TK_COUNT; i++ )
        {
            ps_track_t *tk = &p_sys->tk[i];
            if( tk->b_configured )
            {
                es_format_Clean( &tk->fmt );
                if( tk->es )
                {
                    if( tk->es == p_sys->spu_es )
                    {
                        vlc_mutex_lock( &p_sys->event_lock );
                        p_sys->spu_es = NULL;
                        p_sys->highlight.b_pending = false;
                        vlc_mutex_unlock( &p_sys->event_lock );
                    }
                    es_out_Del( p_sys->p_tf_out, tk->es );
                    tk->es = NULL;
                }
            }
            tk->b_configured = false;
        }

        uint32_t i_width, i_height;
        if( dvdnav_get_video_resolution( p_sys->dvdnav,
                                         &i_width, &i_height ) )
            i_width = i_height = 0;
        switch( dvdnav_get_video_aspect( p_sys->dvdnav ) )
        {
        case 0:
            p_sys->sar.i_num = 4 * i_height;
            p_sys->sar.i_den = 3 * i_width;
            break;
        case 3:
            p_sys->sar.i_num = 16 * i_height;
            p_sys->sar.i_den =  9 * i_width;
            break;
        default:
            p_sys->sar.i_num = 0;
            p_sys->sar.i_den = 0;
            break;
        }

        if( dvdnav_current_title_info( p_sys->dvdnav, &i_title,
                                       &i_part ) == DVDNAV_STATUS_OK )
        {
            if( i_title >= 0 && i_title < p_sys->i_title &&
                p_sys->cur_title != i_title )
            {
                p_sys->updates |= INPUT_UPDATE_TITLE;
                p_sys->cur_title = i_title;
            }
        }
        break;
    }

    case DVDNAV_CELL_CHANGE:
    {
        int32_t i_nav_title = 0;
        int32_t i_nav_part  = 0;

        dvdnav_cell_change_event_t *event =
            (dvdnav_cell_change_event_t*)packet;
        msg_Dbg( p_demux, "DVDNAV_CELL_CHANGE" );
        msg_Dbg( p_demux, "     - cellN=%d", event->cellN );
        msg_Dbg( p_demux, "     - pgN=%d", event->pgN );
        msg_Dbg( p_demux, "     - cell_length=%"PRId64, event->cell_length );
        msg_Dbg( p_demux, "     - pg_length=%"PRId64, event->pg_length );
        msg_Dbg( p_demux, "     - pgc_length=%"PRId64, event->pgc_length );
        msg_Dbg( p_demux, "     - cell_start=%"PRId64, event->cell_start );
        msg_Dbg( p_demux, "     - pg_start=%"PRId64, event->pg_start );

        /* Store the length in time of the current PGC */
        p_sys->i_pgc_length = FROM_SCALE_NZ(event->pgc_length);
        p_sys->i_vobu_index = 0;
        p_sys->i_vobu_flush = 0;

        for( int i=0; i<PS_TK_COUNT; i++ )
            p_sys->tk[i].i_next_block_flags |= BLOCK_FLAG_CELL_DISCONTINUITY;

        /* FIXME is it correct or there is better way to know chapter change */
        if( dvdnav_current_title_info( p_sys->dvdnav, &i_nav_title,
                                       &i_nav_part ) == DVDNAV_STATUS_OK )
        {
            const int i_title = p_sys->cur_title;
            const int i_seekpoint = p_sys->cur_seekpoint;
            if( i_nav_title > 0 && i_nav_title < p_sys->i_title )
            {
                p_sys->cur_title = i_nav_title;
                if( i_nav_part > 0 &&
                    i_nav_part <= p_sys->title[i_nav_title]->i_seekpoint )
                    p_sys->cur_seekpoint = i_nav_part - 1;
                else p_sys->cur_seekpoint = 0;
            }
            else if( i_nav_title == 0 ) /* in menus, i_part == menu id */
            {
                if( MenuIDToSeekpoint( i_nav_part, &p_sys->cur_seekpoint ) )
                    p_sys->cur_seekpoint = 0; /* non standard menu number, can't map back */
                else
                    p_sys->cur_title = 0;
            }
            if( i_title != p_sys->cur_title )
                p_sys->updates |= INPUT_UPDATE_TITLE;
            if( i_seekpoint != p_sys->cur_seekpoint )
                p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
        }
        break;
    }

    case DVDNAV_NAV_PACKET:
    {
        p_sys->i_vobu_index = 1;
        p_sys->i_vobu_flush = 0;

        /* Look if we have need to force a flush (and when) */
        const pci_t *p_pci = dvdnav_get_current_nav_pci( p_sys->dvdnav );
        if( unlikely(!p_pci) )
            break;
        const pci_gi_t *p_pci_gi = &p_pci->pci_gi;
        if( p_pci_gi->vobu_se_e_ptm != 0 && p_pci_gi->vobu_se_e_ptm < p_pci_gi->vobu_e_ptm )
        {
            const dsi_t *p_dsi = dvdnav_get_current_nav_dsi( p_sys->dvdnav );
            if( unlikely(!p_dsi) )
                break;
            const dsi_gi_t *p_dsi_gi = &p_dsi->dsi_gi;
            if( p_dsi_gi->vobu_3rdref_ea != 0 )
                p_sys->i_vobu_flush = p_dsi_gi->vobu_3rdref_ea;
            else if( p_dsi_gi->vobu_2ndref_ea != 0 )
                p_sys->i_vobu_flush = p_dsi_gi->vobu_2ndref_ea;
            else if( p_dsi_gi->vobu_1stref_ea != 0 )
                p_sys->i_vobu_flush = p_dsi_gi->vobu_1stref_ea;
        }

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
            vlc_mutex_lock(&p_sys->event_lock);
            ButtonUpdate( p_demux, false );
            vlc_mutex_unlock(&p_sys->event_lock);
            p_sys->b_spu_change = false;
        }
        break;
    }

    case DVDNAV_STOP:   /* EOF */
        msg_Dbg( p_demux, "DVDNAV_STOP" );

        if( p_sys->b_readahead )
            dvdnav_free_cache_block( p_sys->dvdnav, packet );
        return VLC_DEMUXER_EOF;

    case DVDNAV_HIGHLIGHT:
    {
        dvdnav_highlight_event_t *event = (dvdnav_highlight_event_t*)packet;
        msg_Dbg( p_demux, "DVDNAV_HIGHLIGHT" );
        msg_Dbg( p_demux, "     - display=%d", event->display );
        msg_Dbg( p_demux, "     - buttonN=%d", event->buttonN );
        vlc_mutex_lock(&p_sys->event_lock);
        ButtonUpdate( p_demux, false );
        vlc_mutex_unlock(&p_sys->event_lock);
        break;
    }

    case DVDNAV_HOP_CHANNEL:
        msg_Dbg( p_demux, "DVDNAV_HOP_CHANNEL" );
        p_sys->i_vobu_index = 0;
        p_sys->i_vobu_flush = 0;
        es_out_Control( p_sys->p_tf_out, ES_OUT_RESET_PCR );
        break;

    case DVDNAV_WAIT:
        msg_Dbg( p_demux, "DVDNAV_WAIT" );

        bool b_empty;
        es_out_Control( p_sys->p_tf_out, ES_OUT_GET_EMPTY, &b_empty );
        if( !b_empty )
        {
            vlc_tick_sleep( VLC_TICK_FROM_MS(40) );
        }
        else
        {
            dvdnav_wait_skip( p_sys->dvdnav );
            p_sys->b_reset_pcr = true;
        }
        break;

    default:
        msg_Warn( p_demux, "Unknown event (0x%x)", i_event );
        break;
    }

    if( p_sys->b_readahead )
        dvdnav_free_cache_block( p_sys->dvdnav, packet );

    return VLC_DEMUXER_SUCCESS;
}

/* Get a 2 char code
 * FIXME: partiallyy duplicated from src/input/es_out.c
 */
static char *DemuxGetLanguageCode( demux_t *p_demux, const char *psz_var )
{
    const iso639_lang_t *pl;
    char *psz_lang;
    char *p;

    psz_lang = var_CreateGetString( p_demux, psz_var );
    if( !psz_lang )
        return strdup(LANGUAGE_DEFAULT);

    /* XXX: we will use only the first value
     * (and ignore other ones in case of a list) */
    if( ( p = strchr( psz_lang, ',' ) ) )
        *p = '\0';

    for( pl = p_languages; pl->psz_eng_name != NULL; pl++ )
    {
        if( *psz_lang == '\0' )
            continue;
        if( !strcasecmp( pl->psz_eng_name, psz_lang ) ||
            !strcasecmp( pl->psz_iso639_1, psz_lang ) ||
            !strcasecmp( pl->psz_iso639_2T, psz_lang ) ||
            !strcasecmp( pl->psz_iso639_2B, psz_lang ) )
            break;
    }

    free( psz_lang );

    if( pl->psz_eng_name != NULL )
        return strdup( pl->psz_iso639_1 );

    return strdup(LANGUAGE_DEFAULT);
}

static void DemuxTitles( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    input_title_t *t;
    seekpoint_t *s;
    int32_t i_titles;

    /* Menu */
    t = vlc_input_title_New();
    t->i_flags = INPUT_TITLE_MENU | INPUT_TITLE_INTERACTIVE;
    t->psz_name = strdup( "DVD Menu" );

    for( size_t i=0; i<ARRAY_SIZE(menus_id_mapping); i++ )
    {
        s = vlc_seekpoint_New();
        if(!s)
            break;
        s->psz_name = strdup( menus_id_mapping[i].psz_name );
        TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
    }

    TAB_APPEND( p_sys->i_title, p_sys->title, t );

    /* Find out number of titles/chapters */
    dvdnav_get_number_of_titles( p_sys->dvdnav, &i_titles );

    if( i_titles > 90 )
        msg_Err( p_demux, "This is probably an Arccos Protected DVD. This could take time..." );

    for( int i = 1; i <= i_titles; i++ )
    {
        uint64_t i_title_length;
        uint64_t *p_chapters_time;

        int32_t i_chapters = dvdnav_describe_title_chapters( p_sys->dvdnav, i,
                                                            &p_chapters_time,
                                                            &i_title_length );
        if( i_chapters < 1 )
        {
            i_title_length = 0;
            p_chapters_time = NULL;
        }
        t = vlc_input_title_New();
        t->i_length = FROM_SCALE_NZ(i_title_length);
        for( int j = 0; j < __MAX( i_chapters, 1 ); j++ )
        {
            s = vlc_seekpoint_New();
            if( p_chapters_time )
            {
                if ( j > 0 )
                    s->i_time_offset = FROM_SCALE_NZ(p_chapters_time[j - 1]);
                else
                    s->i_time_offset = 0;
            }
            TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
        }
        free( p_chapters_time );
        TAB_APPEND( p_sys->i_title, p_sys->title, t );
    }
}

/*****************************************************************************
 * Update functions:
 *****************************************************************************/
static void ButtonUpdate( demux_t *p_demux, bool b_mode )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int32_t i_title, i_part;
    int i_ret;

    p_sys->highlight.b_pending = true;
    p_sys->highlight.b_from_user = b_mode;

    if( !p_sys->spu_es )
        return;

    if( dvdnav_current_title_info( p_sys->dvdnav, &i_title, &i_part )
            != DVDNAV_STATUS_OK )
        return;

    dvdnav_highlight_area_t hl;
    int32_t i_button;
    bool    b_button_ok;

    if( dvdnav_get_current_highlight( p_sys->dvdnav, &i_button )
        != DVDNAV_STATUS_OK )
    {
        msg_Err( p_demux, "dvdnav_get_current_highlight failed" );
        return;
    }

    b_button_ok = false;
    if( i_button > 0 && i_title ==  0 )
    {
        pci_t *pci = dvdnav_get_current_nav_pci( p_sys->dvdnav );

        b_button_ok = DVDNAV_STATUS_OK ==
                  dvdnav_get_highlight_area( pci, i_button, b_mode, &hl );
    }

    if( b_button_ok )
    {
        vlc_spu_highlight_t spu_hl = {
            .x_start = hl.sx,
            .x_end = hl.ex,
            .y_start = hl.sy,
            .y_end = hl.ey,
            .palette = {
                .i_entries = 4,
            }
        };

        for( unsigned i = 0; i < 4; i++ )
        {
            uint32_t i_yuv = p_sys->clut[(hl.palette>>(16+i*4))&0x0f];
            uint8_t i_alpha = ( (hl.palette>>(i*4))&0x0f ) * 0xff / 0xf;

            spu_hl.palette.palette[i][0] = (i_yuv >> 16) & 0xff;
            spu_hl.palette.palette[i][1] = (i_yuv >> 0) & 0xff;
            spu_hl.palette.palette[i][2] = (i_yuv >> 8) & 0xff;
            spu_hl.palette.palette[i][3] = i_alpha;
        }

        i_ret = es_out_Control( p_sys->p_tf_out, ES_OUT_SPU_SET_HIGHLIGHT,
                                p_sys->spu_es, &spu_hl );
    }
    else
    {
        i_ret = es_out_Control( p_sys->p_tf_out, ES_OUT_SPU_SET_HIGHLIGHT,
                                p_sys->spu_es, NULL );
    }

    if( i_ret == VLC_SUCCESS )
    {
        msg_Dbg( p_demux, "menu highlight %s button=%d title=%d",
                          b_button_ok ? "set" : "cleared",
                          i_button, i_title );
        p_sys->highlight.b_pending = false;
    }
}

static void ESSubtitleUpdate( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int         i_spu = dvdnav_get_active_spu_stream( p_sys->dvdnav );
    int32_t i_title, i_part;

    vlc_mutex_lock(&p_sys->event_lock);
    ButtonUpdate( p_demux, false );
    vlc_mutex_unlock(&p_sys->event_lock);

    if( dvdnav_current_title_info( p_sys->dvdnav, &i_title, &i_part )
            != DVDNAV_STATUS_OK || i_title > 0 )
        return;

    /* dvdnav_get_active_spu_stream sets (in)visibility flag as 0xF0 */
    if( i_spu >= 0 && i_spu <= 0x1f )
    {
        ps_track_t *tk = &p_sys->tk[ps_id_to_tk(0xbd20 + i_spu)];

        ESNew( p_demux, 0xbd20 + i_spu );

        /* be sure to unselect it (reset) */
        if( tk->es )
        {
            es_out_Control( p_sys->p_tf_out, ES_OUT_SET_ES_STATE, tk->es,
                            (bool)false );

            /* now select it */
            es_out_Control( p_sys->p_tf_out, ES_OUT_SET_ES, tk->es );

            if( tk->fmt.i_cat == SPU_ES )
            {
                vlc_mutex_lock( &p_sys->event_lock );
                p_sys->spu_es = tk->es;
                ButtonUpdate( p_demux, false );
                vlc_mutex_unlock( &p_sys->event_lock );
            }
        }
    }
    else
    {
        for( i_spu = 0; i_spu <= 0x1F; i_spu++ )
        {
            ps_track_t *tk = &p_sys->tk[ps_id_to_tk(0xbd20 + i_spu)];
            if( tk->es )
            {
                es_out_Control( p_sys->p_tf_out, ES_OUT_SET_ES_STATE, tk->es,
                                (bool)false );
            }
        }
    }
}

/*****************************************************************************
 * DemuxBlock: demux a given block
 *****************************************************************************/
static int DemuxBlock( demux_t *p_demux, const uint8_t *p, int len )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    while( len > 0 )
    {
        int i_size = ps_pkt_size( p, len );
        if( i_size <= 0 || i_size > len )
        {
            break;
        }

        /* Create a block */
        block_t *p_pkt = block_Alloc( i_size );
        if( !p_pkt )
            return VLC_EGENERIC;
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
            vlc_tick_t i_scr;
            int i_mux_rate;
            if( !ps_pkt_parse_pack( p_pkt->p_buffer, p_pkt->i_buffer,
                                    &i_scr, &i_mux_rate ) )
            {
                es_out_SetPCR( p_sys->p_tf_out, i_scr );
                if( i_mux_rate > 0 ) p_sys->i_mux_rate = i_mux_rate;
            }
            block_Release( p_pkt );
            break;
        }
        default:
        {
            int i_id = ps_pkt_id( p_pkt->p_buffer, p_pkt->i_buffer );
            if( i_id >= 0xc0 )
            {
                ps_track_t *tk = &p_sys->tk[ps_id_to_tk(i_id)];

                if( !tk->b_configured )
                {
                    ESNew( p_demux, i_id );
                }

                if( tk->es &&
                    !ps_pkt_parse_pes( VLC_OBJECT(p_demux), p_pkt, tk->i_skip ) )
                {
                    int i_next_block_flags = tk->i_next_block_flags;
                    tk->i_next_block_flags = 0;
                    if( i_next_block_flags & BLOCK_FLAG_CELL_DISCONTINUITY )
                    {
                        if( p_pkt->i_dts != VLC_TICK_INVALID )
                        {
                            i_next_block_flags &= ~BLOCK_FLAG_CELL_DISCONTINUITY;
                            i_next_block_flags |= BLOCK_FLAG_DISCONTINUITY;
                        }
                        else tk->i_next_block_flags = BLOCK_FLAG_CELL_DISCONTINUITY;
                    }
                    p_pkt->i_flags |= i_next_block_flags;
                    es_out_Send( p_sys->p_tf_out, tk->es, p_pkt );
                }
                else
                {
                    tk->i_next_block_flags = 0;
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
        len -= i_size;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Force still images to be displayed by sending EOS and stopping buffering.
 *****************************************************************************/
static void DemuxForceStill( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    static const uint8_t buffer[] = {
        0x00, 0x00, 0x01, 0xe0, 0x00, 0x07,
        0x80, 0x00, 0x00,
        0x00, 0x00, 0x01, 0xB7,
    };
    DemuxBlock( p_demux, buffer, sizeof(buffer) );

    bool b_empty;
    es_out_Control( p_sys->p_tf_out, ES_OUT_GET_EMPTY, &b_empty );
}

/*****************************************************************************
 * ESNew: register a new elementary stream
 *****************************************************************************/
static void ESNew( demux_t *p_demux, int i_id )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    ps_track_t  *tk = &p_sys->tk[ps_id_to_tk(i_id)];
    bool  b_select = false;
    int i_lang = 0xffff;

    if( tk->b_configured ) return;

    if( ps_track_fill( tk, 0, i_id, NULL, 0, true ) )
    {
        msg_Warn( p_demux, "unknown codec for id=0x%x", i_id );
        return;
    }

    /* Add a new ES */
    if( tk->fmt.i_cat == VIDEO_ES )
    {
        tk->fmt.video.i_sar_num = p_sys->sar.i_num;
        tk->fmt.video.i_sar_den = p_sys->sar.i_den;
        b_select = true;
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
            i_lang = dvdnav_audio_stream_to_lang( p_sys->dvdnav, i_audio );
            if( dvdnav_get_active_audio_stream( p_sys->dvdnav ) == i_audio )
            {
                b_select = true;
            }
        }
    }
    else if( tk->fmt.i_cat == SPU_ES )
    {
        int32_t i_title, i_part;
        i_lang = dvdnav_spu_stream_to_lang( p_sys->dvdnav, i_id&0x1f );

        /* Palette */
        tk->fmt.subs.spu.palette[0] = SPU_PALETTE_DEFINED;
        memcpy( &tk->fmt.subs.spu.palette[1], p_sys->clut,
                16 * sizeof( uint32_t ) );

        /* We select only when we are not in the menu */
        if( dvdnav_current_title_info( p_sys->dvdnav, &i_title, &i_part ) == DVDNAV_STATUS_OK &&
            i_title > 0 &&
            dvdnav_get_active_spu_stream( p_sys->dvdnav ) == (i_id&0x1f) )
        {
            b_select = true;
        }
    }

    if( i_lang != 0xffff )
    {
        tk->fmt.psz_language = malloc( 3 );
        if( tk->fmt.psz_language )
        {
            tk->fmt.psz_language[0] = (i_lang >> 8)&0xff;
            tk->fmt.psz_language[1] = (i_lang     )&0xff;
            tk->fmt.psz_language[2] = 0;
        }
    }

    tk->fmt.i_id = i_id;
    tk->es = es_out_Add( p_sys->p_tf_out, &tk->fmt );
    if( b_select && tk->es )
    {
        es_out_Control( p_sys->p_tf_out, ES_OUT_SET_ES, tk->es );

        if( tk->fmt.i_cat == VIDEO_ES )
        {
            es_out_Control( p_sys->p_tf_out, ES_OUT_VOUT_SET_MOUSE_EVENT, tk->es,
                            EventMouse, p_demux );
            vlc_mutex_lock( &p_sys->event_lock );
            ButtonUpdate( p_demux, false );
            vlc_mutex_unlock( &p_sys->event_lock );
        }
    }
    tk->b_configured = true;
}

/*****************************************************************************
 * Still image end
 *****************************************************************************/
static void StillTimer( void *p_data )
{
    demux_sys_t    *p_sys = p_data;

    vlc_mutex_lock( &p_sys->still.lock );
    if( likely(p_sys->still.b_enabled) )
    {
        p_sys->still.b_enabled = false;
        dvdnav_still_skip( p_sys->dvdnav );
    }
    vlc_mutex_unlock( &p_sys->still.lock );
}

static void EventMouse( const vlc_mouse_t *newmouse, void *p_data )
{
    demux_t *p_demux = p_data;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( !newmouse )
    {
        vlc_mouse_Init( &p_sys->oldmouse );
        return;
    }

    /* FIXME? PCI usage thread safe? */
    pci_t *pci = dvdnav_get_current_nav_pci( p_sys->dvdnav );

    if( vlc_mouse_HasMoved( &p_sys->oldmouse, newmouse ) )
        dvdnav_mouse_select( p_sys->dvdnav, pci, newmouse->i_x, newmouse->i_y );
    if( vlc_mouse_HasPressed( &p_sys->oldmouse, newmouse, MOUSE_BUTTON_LEFT ) )
    {
        vlc_mutex_lock( &p_sys->event_lock );
        ButtonUpdate( p_demux, true );
        vlc_mutex_unlock( &p_sys->event_lock );
        dvdnav_mouse_activate( p_sys->dvdnav, pci, newmouse->i_x, newmouse->i_y );
    }
    p_sys->oldmouse = *newmouse;
}

/*****************************************************************************
 * ProbeDVD: very weak probing that avoids going too often into a dvdnav_open()
 *****************************************************************************/
static int ProbeDVD( const char *psz_name )
{
    if( !*psz_name )
        /* Triggers libdvdcss autodetection */
        return VLC_SUCCESS;

    int fd = vlc_open( psz_name, O_RDONLY | O_NONBLOCK );
    if( fd == -1 )
#ifdef HAVE_FDOPENDIR
        return VLC_EGENERIC;
#else
        return (errno == ENOENT) ? VLC_EGENERIC : VLC_SUCCESS;
#endif

    int ret = VLC_EGENERIC;
    struct stat stat_info;

    if( fstat( fd, &stat_info ) == -1 )
         goto bailout;
    if( !S_ISREG( stat_info.st_mode ) )
    {
        if( S_ISDIR( stat_info.st_mode ) || S_ISBLK( stat_info.st_mode ) )
            ret = VLC_SUCCESS; /* Let dvdnav_open() do the probing */
        goto bailout;
    }

    /* ISO 9660 volume descriptor */
    char iso_dsc[6];
    if( lseek( fd, 0x8000 + 1, SEEK_SET ) == -1
     || read( fd, iso_dsc, sizeof (iso_dsc) ) < (int)sizeof (iso_dsc)
     || memcmp( iso_dsc, "CD001\x01", 6 ) )
        goto bailout;

    /* Try to find the anchor (2 bytes at LBA 256) */
    uint16_t anchor;

    if( lseek( fd, 256 * DVD_VIDEO_LB_LEN, SEEK_SET ) != -1
     && read( fd, &anchor, 2 ) == 2
     && GetWLE( &anchor ) == 2 )
        ret = VLC_SUCCESS; /* Found a potential anchor */
bailout:
    vlc_close( fd );
    return ret;
}
