/*****************************************************************************
 * clone.c : Clone video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002, 2003 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

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

static int  SendEvents( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define COUNT_TEXT N_("Number of clones")
#define COUNT_LONGTEXT N_("Select the number of video windows in which to "\
    "clone the video.")

#define VOUTLIST_TEXT N_("List of video output modules")
#define VOUTLIST_LONGTEXT N_("Select the specific video output modules that you want to activate.")

vlc_module_begin();
    set_description( _("Clone video filter") );
    set_capability( "video filter", 0 );
    set_shortname( N_("Clone" ));
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );

    add_integer( "clone-count", 2, NULL, COUNT_TEXT, COUNT_LONGTEXT, VLC_FALSE );
    add_string ( "clone-vout-list", NULL, NULL, VOUTLIST_TEXT, VOUTLIST_LONGTEXT, VLC_FALSE );

    add_shortcut( "clone" );
    set_callbacks( Create, Destroy );
vlc_module_end();

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
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;
    p_vout->pf_control = Control;

    psz_clonelist = config_GetPsz( p_vout, "clone-vout-list" );
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
            msg_Err( p_vout, "out of memory" );
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
        p_vout->p_sys->i_clones = config_GetInt( p_vout, "clone-count" );
        p_vout->p_sys->ppsz_vout_list = NULL;
    }

    p_vout->p_sys->i_clones = __MAX( 1, __MIN( 99, p_vout->p_sys->i_clones ) );

    msg_Dbg( p_vout, "spawning %i clone(s)", p_vout->p_sys->i_clones );

    p_vout->p_sys->pp_vout = malloc( p_vout->p_sys->i_clones *
                                     sizeof(vout_thread_t *) );
    if( p_vout->p_sys->pp_vout == NULL )
    {
        msg_Err( p_vout, "out of memory" );
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
    int   i_index, i_vout;
    picture_t *p_pic;
    char *psz_default_vout;
    video_format_t fmt = {0};

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    fmt.i_width = fmt.i_visible_width = p_vout->render.i_width;
    fmt.i_height = fmt.i_visible_height = p_vout->render.i_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    fmt.i_chroma = p_vout->render.i_chroma;
    fmt.i_aspect = p_vout->render.i_aspect;
    fmt.i_sar_num = p_vout->render.i_aspect * fmt.i_height / fmt.i_width;
    fmt.i_sar_den = VOUT_ASPECT_FACTOR;

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
            if( psz_default_vout ) free( psz_default_vout );
            RemoveAllVout( p_vout );
            return VLC_EGENERIC;
        }

        ADD_CALLBACKS( p_vout->p_sys->pp_vout[ i_vout ], SendEvents );
    }

    if( psz_default_vout ) free( psz_default_vout );
    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    ADD_PARENT_CALLBACKS( SendEventsToChild );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate Clone video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the fake output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        free( PP_OUTPUTPICTURE[ i_index ]->p_data_orig );
    }
}

/*****************************************************************************
 * Destroy: destroy Clone video thread output method
 *****************************************************************************
 * Terminate an output method created by CloneCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    RemoveAllVout( p_vout );

    DEL_PARENT_CALLBACKS( SendEventsToChild );

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
            if( p_vout->b_die || p_vout->b_error )
            {
                vout_DestroyPicture(
                    p_vout->p_sys->pp_vout[ i_vout ], p_outpic );
                return;
            }

            msleep( VOUT_OUTMEM_SLEEP );
        }

        vout_DatePicture( p_vout->p_sys->pp_vout[ i_vout ],
                          p_outpic, p_pic->date );
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
                p_vout->p_vlc->pf_memcpy( p_out, p_in, i_in_pitch
                                     * p_outpic->p[i_plane].i_visible_lines );
            }
            else
            {
                p_in_end = p_in + i_in_pitch *
                    p_outpic->p[i_plane].i_visible_lines;

                while( p_in < p_in_end )
                {
                    p_vout->p_vlc->pf_memcpy( p_out, p_in, i_copy_pitch );
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
    while( p_vout->p_sys->i_clones )
    {
         --p_vout->p_sys->i_clones;
         DEL_CALLBACKS( p_vout->p_sys->pp_vout[p_vout->p_sys->i_clones],
                        SendEvents );
         vlc_object_detach( p_vout->p_sys->pp_vout[p_vout->p_sys->i_clones] );
         vout_Destroy( p_vout->p_sys->pp_vout[p_vout->p_sys->i_clones] );
    }
}

/*****************************************************************************
 * SendEvents: forward mouse and keyboard events to the parent p_vout
 *****************************************************************************/
static int SendEvents( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    var_Set( (vlc_object_t *)p_data, psz_var, newval );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SendEventsToChild: forward events to the child/children vout
 *****************************************************************************/
static int SendEventsToChild( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    int i_vout;

    for( i_vout = 0; i_vout < p_vout->p_sys->i_clones; i_vout++ )
    {
        var_Set( p_vout->p_sys->pp_vout[ i_vout ], psz_var, newval );

        if( !strcmp( psz_var, "fullscreen" ) ) break;
    }

    return VLC_SUCCESS;
}
