/*****************************************************************************
 * chain.c : chain multiple video filter modules as a last resort solution
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan dot org>
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
#include <vlc_filter.h>
#include <vlc_vout.h>

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static int  Activate   ( vlc_object_t * );
static void Destroy    ( vlc_object_t * );
static picture_t *Chain( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( N_("Video filtering using a chain of video filter modules") );
    set_capability( "video filter2", 1 );
    set_callbacks( Activate, Destroy );
vlc_module_end();

#define MAX_FILTERS 4

struct filter_sys_t
{
    filter_t       *p_filter1; /* conversion from fmt_in to fmr_mid */
    filter_t       *p_filter2; /* conversion from fmt_mid to fmt_out */
    picture_t      *p_tmp;     /* temporary picture buffer */
    video_format_t  fmt_mid;
};

static const vlc_fourcc_t pi_allowed_chromas[] = {
    VLC_FOURCC('I','4','2','0'),
    VLC_FOURCC('I','4','2','2'),
    VLC_FOURCC('R','V','3','2'),
    VLC_FOURCC('R','V','2','4'),
    0
};

static picture_t *get_pic( filter_t *p_filter )
{
    picture_t *p_pic = (picture_t *)p_filter->p_owner;
    p_filter->p_owner = NULL;
    return p_pic;
}

/* FIXME: this is almost like DeleteFilter in src/misc/image.c */
static void DeleteFilter( filter_t *p_filter )
{
    vlc_object_detach( p_filter );
    if( p_filter->p_module ) module_Unneed( p_filter, p_filter->p_module );
    vlc_object_release( p_filter );
}

/* FIXME: this is almost like CreateFilter in src/misc/image.c */
static filter_t *CreateFilter( vlc_object_t *p_this, video_format_t *fmt_in,
                               video_format_t *fmt_out )
{
    filter_t *p_filter = vlc_object_create( p_this, sizeof(filter_t) );
    vlc_object_attach( p_filter, p_this );

    p_filter->pf_vout_buffer_new = get_pic;

    p_filter->fmt_in = *fmt_in;
    p_filter->fmt_out = *fmt_out;

    p_filter->p_module = module_Need( p_filter, "video filter2", NULL, 0 );

    if( !p_filter->p_module )
    {
        DeleteFilter( p_filter );
        return NULL;
    }

    return p_filter;
}

static int CreateChain( vlc_object_t *p_this, filter_sys_t *p_sys )
{
    p_sys->p_filter1 = CreateFilter( p_this, &p_filter->fmt_in.video,
                                     &p_sys->fmt_mid );
    if( p_sys->p_filter1 )
    {
        p_sys->p_filter2 = CreateFilter( p_this, &p_sys->fmt_mid,
                                         &p_filter->fmt_out.video );
        if( p_sys->p_filter2 )
            return VLC_SUCCESS;
        DeleteFilter( p_sys->p_filter1 );
    }
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Activate: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    static int hack = 0; /* FIXME */

    if( p_filter->fmt_in.video.i_chroma == p_filter->fmt_out.video.i_chroma )
        return VLC_EGENERIC;

    hack++;
    if( hack >= MAX_FILTERS )
    {
        msg_Err( p_this, "Preventing chain filter reccursion (already %d long)",
                 hack );
        return VLC_EGENERIC;
    }

    filter_sys_t *p_sys = (filter_sys_t *)malloc( sizeof( filter_sys_t ) );
    if( !p_sys )
    {
        hack--;
        return VLC_ENOMEM;
    }
    memset( p_sys, 0, sizeof( filter_sys_t ) );
    p_filter->p_sys = p_sys;

    if( p_filter->fmt_in.i_width != p_filter->fmt_out.i_width ||
        p_filter->fmt_in.i_height != p_filter->fmt_out.i_height ||
        p_filter->fmt_in.i_visible_width != p_filter->fmt_out.i_visible_width ||
        p_filter->fmt_in.i_visible_height != p_filter->fmt_out.i_visible_height )
    {
        /* Lets try resizing and then doing the chroma conversion */
        p_sys->fmt_mid = p_filter->fmt_out.video;
        p_sys->fmt_mid.i_chroma = p_filter->fmt_in.video.i_chroma;
        if( CreateChain( p_this, p_sys ) == VLC_SUCCESS )
            return VLC_SUCCESS;

        /* Lets try it the other way arround (chroma and then resize) */
        p_sys->fmt_mid = p_filter->fmt_in.video;
        p_sys->fmt_mid.i_chroma = p_filter->fmt_out.video.i_chroma;
        if( CreateChain( p_this, p_sys ) == VLC_SUCCESS )
            return VLC_SUCCESS;
    }
    else
    {
        /* Lets try doing a chroma chain */
        int i;
        p_sys->fmt_mid = p_filter->fmt_in.video;
        for( i = 0; pi_allowed_chomas[i]; i++ )
        {
            p_sys->fmt_mid.i_chroma = pi_allowed_chromas[i];
            if( CreateChain( p_this, p_sys ) == VLC_SUCCESS )
                return VLC_SUCCESS;
        }
    }

    /* Hum ... looks like this really isn't going to work. Too bad. */
    free( p_sys );
    hack--;
    return VLC_EGENERIC;
}

static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    DeleteFilter( p_filter->p_sys->filter1 );
    DeleteFilter( p_filter->p_sys->filter2 );

    if( p_filter->p_sys->p_tmp )
    {
        free( p_filter->p_sys->p_tmp->p_data_orig );
        free( p_filter->p_sys->p_tmp );
    }

    free( p_filter->p_sys );
}

/*****************************************************************************
 * Chain
 *****************************************************************************/
static picture_t *Chain( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        if( p_pic->pf_release )
            p_pic->pf_release( p_pic );
        return NULL;
    }


    if( !p_sys->p_tmp )
    {
        picture_t *p_tmp = malloc( sizeof( picture_t ) );
        if( !p_tmp )
            return NULL;
        vout_AllocatePicture( VLC_OBJECT( p_vout ), p_tmp,
                              p_sys->fmt_mid.i_chroma,
                              p_sys->fmt_mid.i_width,
                              p_sys->fmt_mid.i_height,
                              p_sys->fmt_mid.i_aspect );
        p_sys->p_tmp = p_tmp;
        p_tmp->pf_release = NULL;
        p_tmp->i_status = RESERVED_PICTURE;
        p_tmp->p_sys = NULL;
    }

    p_sys->p_filter1->p_owner = (filter_owner_sys_t*)p_sys->p_tmp;
    if( !p_sys->p_filter1->pf_video_filter( p_sys->p_filter1, p_pic ) )
    {
        if( p_pic->pf_release )
            p_pic->pf_release( p_pic );
        return NULL;
    }
    if( p_pic->pf_release )
        p_pic->pf_release( p_pic );
    p_sys->p_filter2->p_owner = (filter_owner_sys_t*)p_outpic;
    return p_sys->p_filter2->pf_video_filter( p_sys->p_filter2, p_sys->p_tmp );
}
