/*****************************************************************************
 * clone.c : Clone video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002, 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>

#include "filter_common.h"

#define VOUTSEPARATOR ','

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static void RemoveAllVout  ( vout_thread_t *p_vout );

static int  FullscreenEventUp( vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void * );
static int  FullscreenEventDown( vlc_object_t *, char const *,
                                 vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define COUNT_TEXT N_("Number of clones")
#define COUNT_LONGTEXT N_("Number of video windows in which to "\
    "clone the video.")

#define VOUTLIST_TEXT N_("Video output modules")
#define VOUTLIST_LONGTEXT N_("You can use specific video output modules " \
        "for the clones. Use a comma-separated list of modules." )

#define CFG_PREFIX "clone-"

vlc_module_begin ()
    set_description( N_("Clone video filter") )
    set_capability( "video filter", 0 )
    set_shortname( N_("Clone" ))
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_integer( CFG_PREFIX "count", 2, NULL, COUNT_TEXT, COUNT_LONGTEXT, false )
    add_string ( CFG_PREFIX "vout-list", NULL, NULL, VOUTLIST_TEXT, VOUTLIST_LONGTEXT, true )

    add_shortcut( "clone" )
    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "count", "vout-list", NULL
};

/*****************************************************************************
 * vout_sys_t: Clone video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Clone specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    int    i_clones;

    /* list of vout modules to use. "default" will launch a default
     * module. If specified, overrides the setting in i_clones (which it
     * sets to the list length) */
    char **ppsz_vout_list;

    vout_thread_t **pp_vout;
};

/*****************************************************************************
 * Control: control facility for the vout (forwards to child vout)
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    int i_vout;
    for( i_vout = 0; i_vout < p_vout->p_sys->i_clones; i_vout++ )
    {
        vout_vaControl( p_vout->p_sys->pp_vout[ i_vout ], i_query, args );
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Create: allocates Clone video thread output method
 *****************************************************************************
 * This function allocates and initializes a Clone vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    char *psz_clonelist;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return VLC_ENOMEM;

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;
    p_vout->pf_control = Control;

    config_ChainParse( p_vout, CFG_PREFIX, ppsz_filter_options,
                       p_vout->p_cfg );

    psz_clonelist = var_CreateGetNonEmptyString( p_vout,
                                                 CFG_PREFIX "vout-list" );
    if( psz_clonelist )
    {
        int i_dummy;
        char *psz_token;

        /* Count the number of defined vout */
        p_vout->p_sys->i_clones = 1;
        i_dummy = 0;
        while( psz_clonelist[i_dummy] != 0 )
        {
            if( psz_clonelist[i_dummy] == VOUTSEPARATOR )
                p_vout->p_sys->i_clones++;
            i_dummy++;
        }

        p_vout->p_sys->ppsz_vout_list = malloc( p_vout->p_sys->i_clones
                                                * sizeof(char *) );
        if( !p_vout->p_sys->ppsz_vout_list )
        {
            free( psz_clonelist );
            free( p_vout->p_sys );
            return VLC_ENOMEM;
        }

        /* Tokenize the list */
        i_dummy = 0;
        psz_token = psz_clonelist;
        while( psz_token && *psz_token )
        {
           char *psz_module;
           psz_module = psz_token;
           psz_token = strchr( psz_module, VOUTSEPARATOR );
           if( psz_token )
           {
               *psz_token = '\0';
               psz_token++;
           }
           p_vout->p_sys->ppsz_vout_list[i_dummy] = strdup( psz_module );
           i_dummy++;
        }

        free( psz_clonelist );
    }
    else
    {
        /* No list was specified. We will use the default vout, and get
         * the number of clones from clone-count */
        p_vout->p_sys->i_clones =
            var_CreateGetInteger( p_vout, CFG_PREFIX "count" );
        p_vout->p_sys->ppsz_vout_list = NULL;
    }

    p_vout->p_sys->i_clones = __MAX( 1, __MIN( 99, p_vout->p_sys->i_clones ) );

    msg_Dbg( p_vout, "spawning %i clone(s)", p_vout->p_sys->i_clones );

    p_vout->p_sys->pp_vout = malloc( p_vout->p_sys->i_clones *
                                     sizeof(vout_thread_t *) );
    if( p_vout->p_sys->pp_vout == NULL )
    {
        free( p_vout->p_sys );
        return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize Clone video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int   i_vout;
    char *psz_default_vout;
    video_format_t fmt;

    I_OUTPUTPICTURES = 0;
    memset( &fmt, 0, sizeof(video_format_t) );

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;
    p_vout->fmt_out = p_vout->fmt_in;
    fmt = p_vout->fmt_out;

    /* Try to open the real video output */
    msg_Dbg( p_vout, "spawning the real video outputs" );

    /* Save the default vout */
    psz_default_vout = config_GetPsz( p_vout, "vout" );

    for( i_vout = 0; i_vout < p_vout->p_sys->i_clones; i_vout++ )
    {
        if( p_vout->p_sys->ppsz_vout_list == NULL
            || ( !strncmp( p_vout->p_sys->ppsz_vout_list[i_vout],
                           "default", 8 ) ) )
        {
            p_vout->p_sys->pp_vout[i_vout] =
                vout_Create( p_vout, &fmt );
        }
        else
        {
            /* create the appropriate vout instead of the default one */
            config_PutPsz( p_vout, "vout",
                           p_vout->p_sys->ppsz_vout_list[i_vout] );
            p_vout->p_sys->pp_vout[i_vout] =
                vout_Create( p_vout, &fmt );

            /* Reset the default value */
            config_PutPsz( p_vout, "vout", psz_default_vout );
        }

        if( p_vout->p_sys->pp_vout[ i_vout ] == NULL )
        {
            msg_Err( p_vout, "failed to clone %i vout threads",
                     p_vout->p_sys->i_clones );
            p_vout->p_sys->i_clones = i_vout;
            free( psz_default_vout );
            RemoveAllVout( p_vout );
            return VLC_EGENERIC;
        }
        vout_filter_SetupChild( p_vout, p_vout->p_sys->pp_vout[i_vout],
                                NULL, FullscreenEventUp, FullscreenEventDown, true );
    }

    free( psz_default_vout );
    vout_filter_AllocateDirectBuffers( p_vout, VOUT_MAX_PICTURES );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate Clone video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    RemoveAllVout( p_vout );

    vout_filter_ReleaseDirectBuffers( p_vout );
}

/*****************************************************************************
 * Destroy: destroy Clone video thread output method
 *****************************************************************************
 * Terminate an output method created by CloneCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    free( p_vout->p_sys->pp_vout );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Clone image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic = NULL;
    int i_vout, i_plane;

    for( i_vout = 0; i_vout < p_vout->p_sys->i_clones; i_vout++ )
    {
        while( ( p_outpic =
            vout_CreatePicture( p_vout->p_sys->pp_vout[ i_vout ], 0, 0, 0 )
               ) == NULL )
        {
            if( !vlc_object_alive (p_vout) || p_vout->b_error )
            {
                vout_DestroyPicture(
                    p_vout->p_sys->pp_vout[ i_vout ], p_outpic );
                return;
            }

            msleep( VOUT_OUTMEM_SLEEP );
        }

        p_outpic->date = p_pic->date;
        vout_LinkPicture( p_vout->p_sys->pp_vout[ i_vout ], p_outpic );

        for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
        {
            uint8_t *p_in, *p_in_end, *p_out;
            int i_in_pitch = p_pic->p[i_plane].i_pitch;
            const int i_out_pitch = p_outpic->p[i_plane].i_pitch;
            const int i_copy_pitch = p_outpic->p[i_plane].i_visible_pitch;

            p_in = p_pic->p[i_plane].p_pixels;
            p_out = p_outpic->p[i_plane].p_pixels;

            if( i_in_pitch == i_copy_pitch
                 && i_out_pitch == i_copy_pitch )
            {
                vlc_memcpy( p_out, p_in, i_in_pitch
                                     * p_outpic->p[i_plane].i_visible_lines );
            }
            else
            {
                p_in_end = p_in + i_in_pitch *
                    p_outpic->p[i_plane].i_visible_lines;

                while( p_in < p_in_end )
                {
                    vlc_memcpy( p_out, p_in, i_copy_pitch );
                    p_in += i_in_pitch;
                    p_out += i_out_pitch;
                }
            }
        }

        vout_UnlinkPicture( p_vout->p_sys->pp_vout[ i_vout ], p_outpic );
        vout_DisplayPicture( p_vout->p_sys->pp_vout[ i_vout ], p_outpic );
    }
}

/*****************************************************************************
 * RemoveAllVout: destroy all the child video output threads
 *****************************************************************************/
static void RemoveAllVout( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    while( p_sys->i_clones )
    {
         p_sys->i_clones--;

         vout_filter_SetupChild( p_vout, p_sys->pp_vout[p_sys->i_clones],
                                 NULL, FullscreenEventUp, FullscreenEventDown, false );
         vout_CloseAndRelease( p_sys->pp_vout[p_sys->i_clones] );
    }
}

/**
 * Forward fullscreen event to/from the childrens.
 * FIXME pretty much duplicated from wall.c
 */
static bool IsFullscreenActive( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    for( int i = 0; i < p_sys->i_clones; i++ )
    {
        if( var_GetBool( p_sys->pp_vout[i], "fullscreen" ) )
            return true;
    }
    return false;
}
static int FullscreenEventUp( vlc_object_t *p_this, char const *psz_var,
                              vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = p_data;
    VLC_UNUSED(oldval); VLC_UNUSED(p_this); VLC_UNUSED(psz_var); VLC_UNUSED(newval);

    const bool b_fullscreen = IsFullscreenActive( p_vout );
    if( !var_GetBool( p_vout, "fullscreen" ) != !b_fullscreen )
        return var_SetBool( p_vout, "fullscreen", b_fullscreen );
    return VLC_SUCCESS;
}
static int FullscreenEventDown( vlc_object_t *p_this, char const *psz_var,
                                vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t*)p_this;
    vout_sys_t *p_sys = p_vout->p_sys;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data); VLC_UNUSED(psz_var);

    const bool b_fullscreen = IsFullscreenActive( p_vout );
    if( !b_fullscreen != !newval.b_bool )
    {
        for( int i = 0; i < p_sys->i_clones; i++ )
        {
            vout_thread_t *p_child = p_sys->pp_vout[i];
            if( !var_GetBool( p_child, "fullscreen" ) != !newval.b_bool )
            {
                var_SetBool( p_child, "fullscreen", newval.b_bool );
                if( newval.b_bool )
                    return VLC_SUCCESS;
            }
        }
    }
    return VLC_SUCCESS;
}

