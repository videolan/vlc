/*****************************************************************************
 * projectm.cpp: visualization module based on libprojectM
 *****************************************************************************
 * Copyright © 2009-2011 the VideoLAN team
 * $Id$
 *
 * Authors: Rémi Duraffort <ivoire@videolan.org>
 *          Laurent Aimar
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifndef __STDC_CONSTANT_MACROS
# define __STDC_CONSTANT_MACROS
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_vout_wrapper.h>
#include <vlc_opengl.h>
#include <vlc_filter.h>
#include <vlc_rand.h>

#include <libprojectM/projectM.hpp>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

#define CONFIG_TEXT N_("projectM configuration file")
#define CONFIG_LONGTEXT N_("File that will be used to configure the projectM " \
                           "module.")

#define PRESET_PATH_TXT N_("projectM preset path")
#define PRESET_PATH_LONGTXT N_("Path to the projectM preset directory")

#define TITLE_FONT_TXT N_("Title font")
#define TITLE_FONT_LONGTXT N_("Font used for the titles")

#define MENU_FONT_TXT N_("Font menu")
#define MENU_FONT_LONGTXT N_("Font used for the menus")

#define WIDTH_TEXT N_("Video width")
#define WIDTH_LONGTEXT N_("The width of the video window, in pixels.")

#define HEIGHT_TEXT N_("Video height")
#define HEIGHT_LONGTEXT N_("The height of the video window, in pixels.")

#define MESHX_TEXT N_("Mesh width")
#define MESHX_LONGTEXT N_("The width of the mesh, in pixels.")

#define MESHY_TEXT N_("Mesh height")
#define MESHY_LONGTEXT N_("The height of the mesh, in pixels.")

#define TEXTURE_TEXT N_("Texture size")
#define TEXTURE_LONGTEXT N_("The size of the texture, in pixels.")

#ifdef _WIN32
# define FONT_PATH      "C:\\WINDOWS\\Fonts\\arial.ttf"
# define FONT_PATH_MENU "C:\\WINDOWS\\Fonts\\arial.ttf"
# define PRESET_PATH    NULL
#else
# define FONT_PATH      "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf"
# define FONT_PATH_MENU "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf"
# define PRESET_PATH    "/usr/share/projectM/presets"
#endif

#ifdef DEFAULT_FONT_FILE
#undef FONT_PATH
#define FONT_PATH DEFAULT_FONT_FILE
#endif

#ifdef DEFAULT_MONOSPACE_FONT_FILE
#undef FONT_PATH_MENU
#define FONT_PATH_MENU DEFAULT_MONOSPACE_FONT_FILE
#endif

vlc_module_begin ()
    set_shortname( N_("projectM"))
    set_description( N_("libprojectM effect") )
    set_capability( "visualization", 0 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_VISUAL )
#ifndef HAVE_PROJECTM2
    add_loadfile( "projectm-config", "/usr/share/projectM/config.inp",
                  CONFIG_TEXT, CONFIG_LONGTEXT, true )
#else
    add_directory( "projectm-preset-path", PRESET_PATH,
                  PRESET_PATH_TXT, PRESET_PATH_LONGTXT, true )
    add_loadfile( "projectm-title-font", FONT_PATH,
                  TITLE_FONT_TXT, TITLE_FONT_LONGTXT, true )
    add_loadfile( "projectm-menu-font", FONT_PATH_MENU,
                  MENU_FONT_TXT, MENU_FONT_LONGTXT, true )
#endif
    add_integer( "projectm-width", 800, WIDTH_TEXT, WIDTH_LONGTEXT,
                 false )
    add_integer( "projectm-height", 500, HEIGHT_TEXT, HEIGHT_LONGTEXT,
                 false )
    add_integer( "projectm-meshx", 32, MESHX_TEXT, MESHX_LONGTEXT,
                 false )
    add_integer( "projectm-meshy", 24, MESHY_TEXT, MESHY_LONGTEXT,
                 false )
    add_integer( "projectm-texture-size", 1024, TEXTURE_TEXT, TEXTURE_LONGTEXT,
                 false )
    add_shortcut( "projectm" )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct filter_sys_t
{
    /* */
    vlc_thread_t thread;
    vlc_sem_t    ready;
    bool         b_error;

    /* Opengl */
    vout_thread_t  *p_vout;
    vout_display_t *p_vd;

    /* Window size */
    int i_width;
    int i_height;

    /* audio info */
    int i_channels;

    /* */
    vlc_mutex_t lock;
    bool  b_quit;
    float *p_buffer;
    unsigned i_buffer_size;
    unsigned i_nb_samples;
};


static block_t *DoWork( filter_t *, block_t * );
static void *Thread( void * );

/**
 * Open the module
 * @param p_this: the filter object
 * @return VLC_SUCCESS or vlc error codes
 */
static int Open( vlc_object_t * p_this )
{
    filter_t     *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    p_sys = p_filter->p_sys = (filter_sys_t*)malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* Create the object for the thread */
    vlc_sem_init( &p_sys->ready, 0 );
    p_sys->b_error       = false;
    p_sys->b_quit        = false;
    p_sys->i_width       = var_InheritInteger( p_filter, "projectm-width" );
    p_sys->i_height      = var_InheritInteger( p_filter, "projectm-height" );
    p_sys->i_channels    = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    vlc_mutex_init( &p_sys->lock );
    p_sys->p_buffer      = NULL;
    p_sys->i_buffer_size = 0;
    p_sys->i_nb_samples  = 0;

    /* Create the thread */
    if( vlc_clone( &p_sys->thread, Thread, p_filter, VLC_THREAD_PRIORITY_LOW ) )
        goto error;

    vlc_sem_wait( &p_sys->ready );
    if( p_sys->b_error )
    {
        vlc_join( p_sys->thread, NULL );
        goto error;
    }

    p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    p_filter->pf_audio_filter = DoWork;
    return VLC_SUCCESS;

error:
    vlc_sem_destroy( &p_sys->ready );
    free (p_sys );
    return VLC_EGENERIC;
}


/**
 * Close the module
 * @param p_this: the filter object
 */
static void Close( vlc_object_t *p_this )
{
    filter_t  *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    /* Stop the thread
     * XXX vlc_cleanup_push does not seems to work with C++ so no
     * vlc_cancel()... */
    vlc_mutex_lock( &p_sys->lock );
    p_sys->b_quit = true;
    vlc_mutex_unlock( &p_sys->lock );

    vlc_join( p_sys->thread, NULL );

    /* Free the ressources */
    vlc_sem_destroy( &p_sys->ready );
    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys->p_buffer );
    free( p_sys );
}


/**
 * Do the actual work with the new sample
 * @param p_aout: audio output object
 * @param p_filter: filter object
 * @param p_in_buf: input buffer
 * @param p_out_buf: output buffer
 */
static block_t *DoWork( filter_t *p_filter, block_t *p_in_buf )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    if( p_sys->i_buffer_size > 0 )
    {
        p_sys->i_nb_samples = __MIN( p_sys->i_buffer_size,
                                     p_in_buf->i_nb_samples );

        const float *p_src = (float*)p_in_buf->p_buffer;
        for( unsigned i = 0; i < p_sys->i_nb_samples; i++ )
        {
            float v = 0;
            for( int j = 0; j < p_sys->i_channels; j++ )
                v += p_src[p_sys->i_channels * i + j];
            p_sys->p_buffer[i] = v / p_sys->i_channels;
        }
    }
    vlc_mutex_unlock( &p_sys->lock );

    return p_in_buf;
}

/**
 * Variable callback for the dummy vout
 */
static int VoutCallback( vlc_object_t *p_vout, char const *psz_name,
                         vlc_value_t oldv, vlc_value_t newv, void *p_data )
{
    VLC_UNUSED( p_vout ); VLC_UNUSED( oldv );
    vout_display_t *p_vd = (vout_display_t*)p_data;

    if( !strcmp(psz_name, "fullscreen") )
    {
        vout_SetDisplayFullscreen( p_vd, newv.b_bool );
    }
    return VLC_SUCCESS;
}

/**
 * ProjectM update thread which do the rendering
 * @param p_this: the p_thread object
 */
static void *Thread( void *p_data )
{
    filter_t  *p_filter = (filter_t*)p_data;
    filter_sys_t *p_sys = p_filter->p_sys;

    video_format_t fmt;
    vlc_gl_t *gl;
    unsigned int i_last_width  = 0;
    unsigned int i_last_height = 0;
    locale_t loc;
    locale_t oldloc;

    projectM *p_projectm;
#ifndef HAVE_PROJECTM2
    char *psz_config;
#else
    char *psz_preset_path;
    char *psz_title_font;
    char *psz_menu_font;
    projectM::Settings settings;
#endif

    vlc_savecancel();

    /* Create the openGL provider */
    p_sys->p_vout =
        (vout_thread_t *)vlc_object_create( p_filter, sizeof(vout_thread_t) );
    if( !p_sys->p_vout )
        goto error;

    /* */
    video_format_Init( &fmt, 0 );
    video_format_Setup( &fmt, VLC_CODEC_RGB32,
                        p_sys->i_width, p_sys->i_height, 0, 1 );
    fmt.i_sar_num = 1;
    fmt.i_sar_den = 1;

    vout_display_state_t state;
    memset( &state, 0, sizeof(state) );
    state.cfg.display.sar.num = 1;
    state.cfg.display.sar.den = 1;
    state.cfg.is_display_filled = true;
    state.cfg.zoom.num = 1;
    state.cfg.zoom.den = 1;
    state.sar.num = 1;
    state.sar.den = 1;

    p_sys->p_vd = vout_NewDisplay( p_sys->p_vout, &fmt, &state, "opengl",
                                   300000, 1000000 );
    if( !p_sys->p_vd )
    {
        vlc_object_release( p_sys->p_vout );
        goto error;
    }
    var_Create( p_sys->p_vout, "fullscreen", VLC_VAR_BOOL );
    var_AddCallback( p_sys->p_vout, "fullscreen", VoutCallback, p_sys->p_vd );

    gl = vout_GetDisplayOpengl( p_sys->p_vd );
    if( !gl )
    {
        var_DelCallback( p_sys->p_vout, "fullscreen", VoutCallback, p_sys->p_vd );
        vout_DeleteDisplay( p_sys->p_vd, NULL );
        vlc_object_release( p_sys->p_vout );
        goto error;
    }

    /* Work-around the projectM locale bug */
    loc = newlocale (LC_NUMERIC_MASK, "C", NULL);
    oldloc = uselocale (loc);

    /* Create the projectM object */
#ifndef HAVE_PROJECTM2
    psz_config = var_InheritString( p_filter, "projectm-config" );
    p_projectm = new projectM( psz_config );
    free( psz_config );
#else
    psz_preset_path = var_InheritString( p_filter, "projectm-preset-path" );
#ifdef _WIN32
    if ( psz_preset_path == NULL )
    {
        char *psz_data_path = config_GetDataDir();
        asprintf( &psz_preset_path, "%s" DIR_SEP "visualization", psz_data_path );
        free( psz_data_path );
    }
#endif

    psz_title_font                = var_InheritString( p_filter, "projectm-title-font" );
    psz_menu_font                 = var_InheritString( p_filter, "projectm-menu-font" );

    settings.meshX                = var_InheritInteger( p_filter, "projectm-meshx" );
    settings.meshY                = var_InheritInteger( p_filter, "projectm-meshy" );
    settings.fps                  = 35;
    settings.textureSize          = var_InheritInteger( p_filter, "projectm-texture-size" );
    settings.windowWidth          = p_sys->i_width;
    settings.windowHeight         = p_sys->i_height;
    settings.presetURL            = psz_preset_path;
    settings.titleFontURL         = psz_title_font;
    settings.menuFontURL          = psz_menu_font;
    settings.smoothPresetDuration = 5;
    settings.presetDuration       = 30;
    settings.beatSensitivity      = 10;
    settings.aspectCorrection     = 1;
    settings.easterEgg            = 1;
    settings.shuffleEnabled       = 1;

    p_projectm = new projectM( settings );

    free( psz_menu_font );
    free( psz_title_font );
    free( psz_preset_path );
#endif /* HAVE_PROJECTM2 */

    p_sys->i_buffer_size = p_projectm->pcm()->maxsamples;
    p_sys->p_buffer = (float*)calloc( p_sys->i_buffer_size,
                                      sizeof( float ) );

    vlc_sem_post( &p_sys->ready );

    /* Choose a preset randomly or projectM will always show the first one */
    if ( p_projectm->getPlaylistSize() > 0 )
        p_projectm->selectPreset( (unsigned)vlc_mrand48() % p_projectm->getPlaylistSize() );

    /* */
    for( ;; )
    {
        const mtime_t i_deadline = mdate() + CLOCK_FREQ / 50; /* 50 fps max */
        /* Manage the events */
        vout_ManageDisplay( p_sys->p_vd, true );
        if( p_sys->p_vd->cfg->display.width  != i_last_width ||
            p_sys->p_vd->cfg->display.height != i_last_height )
        {
            /* FIXME it is not perfect as we will have black bands */
            vout_display_place_t place;
            vout_display_PlacePicture( &place, &p_sys->p_vd->source, p_sys->p_vd->cfg, false );
            p_projectm->projectM_resetGL( place.width, place.height );

            i_last_width  = p_sys->p_vd->cfg->display.width;
            i_last_height = p_sys->p_vd->cfg->display.height;
        }

        /* Render the image and swap the buffers */
        vlc_mutex_lock( &p_sys->lock );
        if( p_sys->i_nb_samples > 0 )
        {
            p_projectm->pcm()->addPCMfloat( p_sys->p_buffer,
                                            p_sys->i_nb_samples );
            p_sys->i_nb_samples = 0;
        }
        if( p_sys->b_quit )
        {
            vlc_mutex_unlock( &p_sys->lock );

            delete p_projectm;
            var_DelCallback( p_sys->p_vout, "fullscreen", VoutCallback, p_sys->p_vd );
            vout_DeleteDisplay( p_sys->p_vd, NULL );
            vlc_object_release( p_sys->p_vout );
            if (loc != (locale_t)0)
            {
                uselocale (oldloc);
                freelocale (loc);
            }
            return NULL;
        }
        vlc_mutex_unlock( &p_sys->lock );

        p_projectm->renderFrame();

        /* */
        mwait( i_deadline );

        if( !vlc_gl_Lock(gl) )
        {
            vlc_gl_Swap( gl );
            vlc_gl_Unlock( gl );
        }
    }
    abort();

error:
    p_sys->b_error = true;
    vlc_sem_post( &p_sys->ready );
    return NULL;
}

