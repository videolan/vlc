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
 * Module descriptor
 *****************************************************************************/
static int       Activate   ( vlc_object_t * );
static void      Destroy    ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Video filtering using a chain of video filter modules") )
    set_capability( "video filter2", 1 )
    set_callbacks( Activate, Destroy )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static picture_t *Chain         ( filter_t *, picture_t * );
static int BufferAllocationInit ( filter_t *, void * );

static int BuildChromaResize( filter_t * );
static int BuildChromaChain( filter_t *p_filter );

static int CreateChain( filter_chain_t *p_chain, es_format_t *p_fmt_mid );
static void EsFormatMergeSize( es_format_t *p_dst,
                               const es_format_t *p_base,
                               const es_format_t *p_size );

static const vlc_fourcc_t pi_allowed_chromas[] = {
    VLC_FOURCC('I','4','2','0'),
    VLC_FOURCC('I','4','2','2'),
    VLC_FOURCC('R','V','3','2'),
    VLC_FOURCC('R','V','2','4'),
    0
};

struct filter_sys_t
{
    filter_chain_t *p_chain;
};

#define CHAIN_LEVEL_MAX 4

/*****************************************************************************
 * Activate: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    int i_ret;

    const bool b_chroma = p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma;
    const bool b_resize = p_filter->fmt_in.video.i_width  != p_filter->fmt_out.video.i_width ||
                          p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height;

    /* XXX Remove check on b_resize to build chroma chain (untested) */
    if( !b_chroma || !b_resize )
        return VLC_EGENERIC;

    p_sys = p_filter->p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    memset( p_sys, 0, sizeof( *p_sys ) );

    p_sys->p_chain = filter_chain_New( p_filter, "video filter2", false, BufferAllocationInit, NULL, p_filter );
    if( !p_sys->p_chain )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }
    filter_chain_Reset( p_sys->p_chain, &p_filter->fmt_in, &p_filter->fmt_out );

    if( b_chroma && b_resize )
        i_ret = BuildChromaResize( p_filter );
    else if( b_chroma )
        i_ret = BuildChromaChain( p_filter );
    else
        i_ret = VLC_EGENERIC;

    if( i_ret )
    {
        /* Hum ... looks like this really isn't going to work. Too bad. */
        filter_chain_Delete( p_sys->p_chain );
        free( p_sys );
        return VLC_EGENERIC;
    }
    /* */
    p_filter->pf_video_filter = Chain;
    return VLC_SUCCESS;
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

/*****************************************************************************
 * Builders
 *****************************************************************************/
static int BuildChromaResize( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    es_format_t fmt_mid;
    int i_ret;

    /* Lets try resizing and then doing the chroma conversion */
    msg_Dbg( p_filter, "Trying to build resize+chroma" );
    EsFormatMergeSize( &fmt_mid, &p_filter->fmt_in, &p_filter->fmt_out );
    i_ret = CreateChain( p_sys->p_chain, &fmt_mid );
    es_format_Clean( &fmt_mid );
    if( i_ret == VLC_SUCCESS )
        return VLC_SUCCESS;

    /* Lets try it the other way arround (chroma and then resize) */
    msg_Dbg( p_filter, "Trying to build chroma+resize" );
    EsFormatMergeSize( &fmt_mid, &p_filter->fmt_out, &p_filter->fmt_in );
    i_ret = CreateChain( p_sys->p_chain, &fmt_mid );
    es_format_Clean( &fmt_mid );
    if( i_ret == VLC_SUCCESS )
        return VLC_SUCCESS;

    return VLC_EGENERIC;
}

static int BuildChromaChain( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    es_format_t fmt_mid;
    int i_ret;
    int i;

    /* We have to protect ourself against a too high recursion */
    const char *psz_option = MODULE_STRING"-level";
    bool b_first = !var_Type( p_filter, psz_option );

    if( var_Create( p_filter, MODULE_STRING"-level", VLC_VAR_INTEGER | (b_first ? VLC_VAR_DOINHERIT : 0 ) ) )
    {
        msg_Err( p_filter, "Failed to create %s variable", psz_option );
        return VLC_EGENERIC;
    }
    int i_level = var_GetInteger( p_filter, psz_option );
    if( i_level >= CHAIN_LEVEL_MAX )
    {
        msg_Err( p_filter, "Too high level of recursion (%d)", i_level );
        return VLC_EGENERIC;
    }
    var_SetInteger( p_filter, psz_option, i_level + 1 );

    /* Now try chroma format list */
    for( i = 0; pi_allowed_chromas[i]; i++ )
    {
        const vlc_fourcc_t i_chroma = pi_allowed_chromas[i];

        msg_Dbg( p_filter, "Trying to use chroma %4.4s as middle man",
                 (char*)&i_chroma );

        es_format_Copy( &fmt_mid, &p_filter->fmt_in );
        fmt_mid.video.i_chroma = i_chroma;

        i_ret = CreateChain( p_sys->p_chain, &fmt_mid );
        es_format_Clean( &fmt_mid );

        if( i_ret == VLC_SUCCESS )
            return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Buffer management
 *****************************************************************************/
static picture_t *BufferNew( filter_t *p_filter )
{
    filter_t *p_parent = (filter_t*)p_filter->p_owner;

    return p_parent->pf_vout_buffer_new( p_parent );
}
static void BufferDel( filter_t *p_filter, picture_t *p_pic )
{
    filter_t *p_parent = (filter_t*)p_filter->p_owner;

    p_parent->pf_vout_buffer_del( p_parent, p_pic );
}
static int BufferAllocationInit ( filter_t *p_filter, void *p_data )
{
    p_filter->pf_vout_buffer_new = BufferNew;
    p_filter->pf_vout_buffer_del = BufferDel;
    p_filter->p_owner = p_data;
    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
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

static void EsFormatMergeSize( es_format_t *p_dst,
                               const es_format_t *p_base,
                               const es_format_t *p_size )
{
    es_format_Copy( p_dst, p_base );

    p_dst->video.i_width  = p_size->video.i_width;
    p_dst->video.i_height = p_size->video.i_height;

    p_dst->video.i_visible_width  = p_size->video.i_visible_width;
    p_dst->video.i_visible_height = p_size->video.i_visible_height;
}

