/*****************************************************************************
 * canvas.c : automatically resize and padd a video to fit in canvas
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
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

#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_vout.h>

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static int  Activate( vlc_object_t * );
static void Destroy( vlc_object_t * );
static picture_t *Filter( filter_t *, picture_t * );
static int alloc_init( filter_t *, void * );

#define WIDTH_TEXT N_( "Image width" )
#define WIDTH_LONGTEXT N_( \
    "Image width" )
#define HEIGHT_TEXT N_( "Image height" )
#define HEIGHT_LONGTEXT N_( \
    "Image height" )
#define ASPECT_TEXT N_( "Aspect ratio" )
#define ASPECT_LONGTEXT N_( \
    "Set aspect (like 4:3) of the video canvas" )
#define PADD_TEXT N_( "Padd video" )
#define PADD_LONGTEXT N_( \
    "If enabled, video will be padded to fit in canvas after scaling. " \
    "Otherwise, video will be cropped to fix in canvas after scaling." )

#define CFG_PREFIX "canvas-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Automatically resize and padd a video") )
    set_capability( "video filter2", 0 )
    set_callbacks( Activate, Destroy )

    add_integer_with_range( CFG_PREFIX "width", 0, 0, INT_MAX, NULL,
                            WIDTH_TEXT, WIDTH_LONGTEXT, false )
    add_integer_with_range( CFG_PREFIX "height", 0, 0, INT_MAX, NULL,
                            HEIGHT_TEXT, HEIGHT_LONGTEXT, false )

    add_string( CFG_PREFIX "aspect", "4:3", NULL,
                ASPECT_TEXT, ASPECT_LONGTEXT, false )

    add_bool( CFG_PREFIX "padd", true, NULL,
              PADD_TEXT, PADD_LONGTEXT, false )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "width", "height", "aspect", "padd", NULL
};

struct filter_sys_t
{
    filter_chain_t *p_chain;
};

/*****************************************************************************
 *
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    unsigned int i_width, i_height;
    es_format_t fmt;
    char psz_croppadd[100];
    int i_padd,i_offset;
    char *psz_aspect, *psz_parser;
    int i_aspect;
    bool b_padd;

    if( !p_filter->b_allow_fmt_out_change )
    {
        msg_Err( p_filter, "Picture format change isn't allowed" );
        return VLC_EGENERIC;
    }

    if( p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma )
    {
        msg_Err( p_filter, "Input and output chromas don't match" );
        return VLC_EGENERIC;
    }

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    i_width = var_CreateGetInteger( p_filter, CFG_PREFIX "width" );
    i_height = var_CreateGetInteger( p_filter, CFG_PREFIX "height" );

    if( i_width == 0 || i_height == 0 )
    {
        msg_Err( p_filter, "Width and height options must be set" );
        return VLC_EGENERIC;
    }

    if( i_width & 1 || i_height & 1 )
    {
        msg_Err( p_filter, "Width and height options must be even integers" );
        return VLC_EGENERIC;
    }

    psz_aspect = var_CreateGetNonEmptyString( p_filter, CFG_PREFIX "aspect" );
    if( !psz_aspect )
    {
        msg_Err( p_filter, "Aspect ratio must be set" );
        return VLC_EGENERIC;
    }
    psz_parser = strchr( psz_aspect, ':' );
    if( psz_parser ) psz_parser++;
    if( psz_parser && atoi( psz_parser ) > 0 )
        i_aspect = atoi( psz_aspect ) * VOUT_ASPECT_FACTOR / atoi( psz_parser );
    else
        i_aspect = atof( psz_aspect ) * VOUT_ASPECT_FACTOR;
    free( psz_aspect );

    if( i_aspect <= 0 )
    {
        msg_Err( p_filter, "Aspect ratio must be strictly positive" );
        return VLC_EGENERIC;
    }

    b_padd = var_CreateGetBool( p_filter, CFG_PREFIX "padd" );

    filter_sys_t *p_sys = (filter_sys_t *)malloc( sizeof( filter_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_filter->p_sys = p_sys;

    p_sys->p_chain = filter_chain_New( p_filter, "video filter2", true,
                                       alloc_init, NULL, p_filter );
    if( !p_sys->p_chain )
    {
        msg_Err( p_filter, "Could not allocate filter chain" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    es_format_Copy( &fmt, &p_filter->fmt_in );

    fmt.video.i_width = i_width;
    fmt.video.i_height = ( p_filter->fmt_in.video.i_height * i_width )
                         / p_filter->fmt_in.video.i_width;
    fmt.video.i_height = ( fmt.video.i_height * i_aspect )
                         / p_filter->fmt_in.video.i_aspect;

    if( b_padd )
    {
        /* Padd */
        if( fmt.video.i_height > i_height )
        {
            fmt.video.i_height = i_height;
            fmt.video.i_width = ( p_filter->fmt_in.video.i_width * i_height )
                                / p_filter->fmt_in.video.i_height;
            fmt.video.i_width = ( fmt.video.i_width * p_filter->fmt_in.video.i_aspect )
                                / i_aspect;
            if( fmt.video.i_width & 1 ) fmt.video.i_width -= 1;

            i_padd = (i_width - fmt.video.i_width) / 2;
            i_offset = (i_padd & 1);
            /* Gruik */
            snprintf( psz_croppadd, 100, "croppadd{paddleft=%d,paddright=%d}",
                      i_padd - i_offset, i_padd + i_offset );
        }
        else
        {
            if( fmt.video.i_height & 1 ) fmt.video.i_height -= 1;
            i_padd = (i_height - fmt.video.i_height ) / 2;
            i_offset = (i_padd & 1);
            /* Gruik */
            snprintf( psz_croppadd, 100, "croppadd{paddtop=%d,paddbottom=%d}",
                      i_padd - i_offset, i_padd + i_offset );
        }
    }
    else
    {
        /* Crop */
        if( fmt.video.i_height < i_height )
        {
            fmt.video.i_height = i_height;
            fmt.video.i_width = ( p_filter->fmt_in.video.i_width * i_height )
                                / p_filter->fmt_in.video.i_height;
            fmt.video.i_width = ( fmt.video.i_width * p_filter->fmt_in.video.i_aspect )
                                / i_aspect;
            if( fmt.video.i_width & 1 ) fmt.video.i_width -= 1;

            i_padd = (fmt.video.i_width - i_width) / 2;
            i_offset =  (i_padd & 1);
            /* Gruik */
            snprintf( psz_croppadd, 100, "croppadd{cropleft=%d,cropright=%d}",
                      i_padd - i_offset, i_padd + i_offset );
        }
        else
        {
            if( fmt.video.i_height & 1 ) fmt.video.i_height -= 1;
            i_padd = (fmt.video.i_height - i_height) / 2;
            i_offset = (i_padd & 1);
            /* Gruik */
            snprintf( psz_croppadd, 100, "croppadd{croptop=%d,cropbottom=%d}",
                      i_padd - i_offset, i_padd + i_offset );
        }
    }

    fmt.video.i_visible_width = fmt.video.i_width;
    fmt.video.i_visible_height = fmt.video.i_height;

    filter_chain_Reset( p_sys->p_chain, &p_filter->fmt_in, &fmt );
    /* Append scaling module */
    filter_chain_AppendFilter( p_sys->p_chain, NULL, NULL, NULL, NULL );
    /* Append padding module */
    filter_chain_AppendFromString( p_sys->p_chain, psz_croppadd );

    fmt = *filter_chain_GetFmtOut( p_sys->p_chain );
    es_format_Copy( &p_filter->fmt_out, &fmt );

    p_filter->fmt_out.video.i_aspect = i_aspect * i_width / i_height;

    if( p_filter->fmt_out.video.i_width != i_width
     || p_filter->fmt_out.video.i_height != i_height )
    {
        msg_Warn( p_filter, "Looks like something went wrong. "
                  "Output size is %dx%d while we asked for %dx%d",
                  p_filter->fmt_out.video.i_width,
                  p_filter->fmt_out.video.i_height,
                  i_width, i_height );
    }

    p_filter->pf_video_filter = Filter;

    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_chain_Delete( p_filter->p_sys->p_chain );
    free( p_filter->p_sys );
}

/*****************************************************************************
 *
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    return filter_chain_VideoFilter( p_filter->p_sys->p_chain, p_pic );
}

/*****************************************************************************
 *
 *****************************************************************************/
static picture_t *video_new( filter_t *p_filter )
{
    return ((filter_t*)p_filter->p_owner)->pf_vout_buffer_new( (filter_t*)p_filter->p_owner );
}

static void video_del( filter_t *p_filter, picture_t *p_pic )
{
    if( ((filter_t*)p_filter->p_owner)->pf_vout_buffer_del )
        ((filter_t*)p_filter->p_owner)->pf_vout_buffer_del( (filter_t*)p_filter->p_owner, p_pic );
}

static int alloc_init( filter_t *p_filter, void *p_data )
{
    p_filter->p_owner = p_data;
    p_filter->pf_vout_buffer_new = video_new;
    p_filter->pf_vout_buffer_del = video_del;
    return VLC_SUCCESS;
}
