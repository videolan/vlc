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

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static int  Activate   ( vlc_object_t * );
static void Destroy    ( vlc_object_t * );
static picture_t *Chain( filter_t *, picture_t * );
static int AllocInit( filter_t *p_filter, void *p_data );

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
    filter_chain_t *p_chain;
};

static const vlc_fourcc_t pi_allowed_chromas[] = {
    VLC_FOURCC('I','4','2','0'),
    VLC_FOURCC('I','4','2','2'),
    VLC_FOURCC('R','V','3','2'),
    VLC_FOURCC('R','V','2','4'),
    0
};

static int CreateChain( filter_chain_t *p_chain, es_format_t *p_fmt_mid )
{
    filter_t *p_filter1;
    if( !( p_filter1 =
           filter_chain_AppendFilter( p_chain, NULL, NULL, NULL, p_fmt_mid )) )
        return VLC_EGENERIC;
    if( !filter_chain_AppendFilter( p_chain, NULL, NULL, p_fmt_mid, NULL ) )
    {
        filter_chain_DeleteFilter( p_chain, p_filter1 );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int AllocInit( filter_t *p_filter, void *p_data )
{
    /* Not sure about all of this ... it should work */
    p_filter->pf_vout_buffer_new = ((filter_t*)p_data)->pf_vout_buffer_new;
    p_filter->pf_vout_buffer_del = ((filter_t*)p_data)->pf_vout_buffer_del;
    p_filter->p_owner = ((filter_t*)p_data)->p_owner;
    return VLC_SUCCESS;
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
    es_format_t fmt_mid;

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

    p_sys->p_chain = filter_chain_New( p_filter, "video filter2", false, AllocInit, NULL, p_filter );
    if( !p_sys->p_chain )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }
    filter_chain_Reset( p_sys->p_chain, &p_filter->fmt_in, &p_filter->fmt_out );

    if( p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width ||
        p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height ||
        p_filter->fmt_in.video.i_visible_width != p_filter->fmt_out.video.i_visible_width ||
        p_filter->fmt_in.video.i_visible_height != p_filter->fmt_out.video.i_visible_height )
    {
        /* Lets try resizing and then doing the chroma conversion */
        es_format_Copy( &fmt_mid, &p_filter->fmt_out );
        fmt_mid.video.i_chroma = p_filter->fmt_out.video.i_chroma;
        if( CreateChain( p_sys->p_chain, &fmt_mid ) == VLC_SUCCESS )
        {
            es_format_Clean( &fmt_mid );
            p_filter->pf_video_filter = Chain;
            return VLC_SUCCESS;
        }

        /* Lets try it the other way arround (chroma and then resize) */
        es_format_Clean( &fmt_mid );
        es_format_Copy( &fmt_mid, &p_filter->fmt_in );
        fmt_mid.video.i_chroma = p_filter->fmt_out.video.i_chroma;
        if( CreateChain( p_sys->p_chain, &fmt_mid ) == VLC_SUCCESS )
        {
            es_format_Clean( &fmt_mid );
            p_filter->pf_video_filter = Chain;
            return VLC_SUCCESS;
        }
    }
    else
    {
        /* Lets try doing a chroma chain */
        int i;
        es_format_Copy( &fmt_mid, &p_filter->fmt_in );
        for( i = 0; pi_allowed_chromas[i]; i++ )
        {
            fmt_mid.video.i_chroma = pi_allowed_chromas[i];
            if( CreateChain( p_sys->p_chain, &fmt_mid ) == VLC_SUCCESS )
            {
                es_format_Clean( &fmt_mid );
                p_filter->pf_video_filter = Chain;
                return VLC_SUCCESS;
            }
        }
    }

    /* Hum ... looks like this really isn't going to work. Too bad. */
    es_format_Clean( &fmt_mid );
    filter_chain_Delete( p_sys->p_chain );
    free( p_sys );
    hack--;
    return VLC_EGENERIC;
}

static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_chain_Delete( p_filter->p_sys->p_chain );
    free( p_filter->p_sys );
}

/*****************************************************************************
 * Chain
 *****************************************************************************/
static picture_t *Chain( filter_t *p_filter, picture_t *p_pic )
{
    return filter_chain_VideoFilter( p_filter->p_sys->p_chain, p_pic );
}
