/*****************************************************************************
 * blendbench.c : blending benchmark plugin for vlc
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Søren Bøg <avacore@videolan.org>
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
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_modules.h>

#include <vlc_filter.h>
#include <vlc_image.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Create( vlc_object_t * );
static void Destroy( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define LOOPS_TEXT N_("Number of time to blend")
#define LOOPS_LONGTEXT N_("The number of time the blend will be performed")

#define ALPHA_TEXT N_("Alpha of the blended image")
#define ALPHA_LONGTEXT N_("Alpha with which the blend image is blended")

#define BASE_IMAGE_TEXT N_("Image to be blended onto")
#define BASE_IMAGE_LONGTEXT N_("The image which will be used to blend onto")

#define BASE_CHROMA_TEXT N_("Chroma for the base image")
#define BASE_CHROMA_LONGTEXT N_("Chroma which the base image will be loaded in")

#define BLEND_IMAGE_TEXT N_("Image which will be blended")
#define BLEND_IMAGE_LONGTEXT N_("The image blended onto the base image")

#define BLEND_CHROMA_TEXT N_("Chroma for the blend image")
#define BLEND_CHROMA_LONGTEXT N_("Chroma which the blend image will be loaded" \
                                 " in")

#define CFG_PREFIX "blendbench-"

vlc_module_begin ()
    set_description( N_("Blending benchmark filter") )
    set_shortname( N_("Blendbench" ))
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter2", 0 )

    set_section( N_("Benchmarking"), NULL )
    add_integer( CFG_PREFIX "loops", 1000, LOOPS_TEXT,
              LOOPS_LONGTEXT, false )
    add_integer_with_range( CFG_PREFIX "alpha", 128, 0, 255, ALPHA_TEXT,
              ALPHA_LONGTEXT, false )

    set_section( N_("Base image"), NULL )
    add_loadfile( CFG_PREFIX "base-image", NULL, BASE_IMAGE_TEXT,
                  BASE_IMAGE_LONGTEXT, false )
    add_string( CFG_PREFIX "base-chroma", "I420", BASE_CHROMA_TEXT,
              BASE_CHROMA_LONGTEXT, false )

    set_section( N_("Blend image"), NULL )
    add_loadfile( CFG_PREFIX "blend-image", NULL, BLEND_IMAGE_TEXT,
                  BLEND_IMAGE_LONGTEXT, false )
    add_string( CFG_PREFIX "blend-chroma", "YUVA", BLEND_CHROMA_TEXT,
              BLEND_CHROMA_LONGTEXT, false )

    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "loops", "alpha", "base-image", "base-chroma", "blend-image",
    "blend-chroma", NULL
};

/*****************************************************************************
 * filter_sys_t: filter method descriptor
 *****************************************************************************/
struct filter_sys_t
{
    bool b_done;
    int i_loops, i_alpha;

    picture_t *p_base_image;
    picture_t *p_blend_image;

    vlc_fourcc_t i_base_chroma;
    vlc_fourcc_t i_blend_chroma;
};

static int blendbench_LoadImage( vlc_object_t *p_this, picture_t **pp_pic,
                                 vlc_fourcc_t i_chroma, char *psz_file, const char *psz_name )
{
    image_handler_t *p_image;
    video_format_t fmt_in, fmt_out;

    memset( &fmt_in, 0, sizeof(video_format_t) );
    memset( &fmt_out, 0, sizeof(video_format_t) );

    fmt_out.i_chroma = i_chroma;
    p_image = image_HandlerCreate( p_this );
    *pp_pic = image_ReadUrl( p_image, psz_file, &fmt_in, &fmt_out );
    image_HandlerDelete( p_image );

    if( *pp_pic == NULL )
    {
        msg_Err( p_this, "Unable to load %s image", psz_name );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_this, "%s image has dim %d x %d (Y plane)", psz_name,
             (*pp_pic)->p[Y_PLANE].i_visible_pitch,
             (*pp_pic)->p[Y_PLANE].i_visible_lines );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Create: allocates video thread output method
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    char *psz_temp, *psz_cmd;
    int i_ret;

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    p_sys = p_filter->p_sys;
    p_sys->b_done = false;

    p_filter->pf_video_filter = Filter;

    /* needed to get options passed in transcode using the
     * adjust{name=value} syntax */
    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    p_sys->i_loops = var_CreateGetIntegerCommand( p_filter,
                                                  CFG_PREFIX "loops" );
    p_sys->i_alpha = var_CreateGetIntegerCommand( p_filter,
                                                  CFG_PREFIX "alpha" );

    psz_temp = var_CreateGetStringCommand( p_filter, CFG_PREFIX "base-chroma" );
    p_sys->i_base_chroma = VLC_FOURCC( psz_temp[0], psz_temp[1],
                                       psz_temp[2], psz_temp[3] );
    psz_cmd = var_CreateGetStringCommand( p_filter, CFG_PREFIX "base-image" );
    i_ret = blendbench_LoadImage( p_this, &p_sys->p_base_image,
                                  p_sys->i_base_chroma, psz_cmd, "Base" );
    free( psz_temp );
    free( psz_cmd );
    if( i_ret != VLC_SUCCESS )
    {
        free( p_sys );
        return i_ret;
    }

    psz_temp = var_CreateGetStringCommand( p_filter,
                                           CFG_PREFIX "blend-chroma" );
    p_sys->i_blend_chroma = VLC_FOURCC( psz_temp[0], psz_temp[1],
                                        psz_temp[2], psz_temp[3] );
    psz_cmd = var_CreateGetStringCommand( p_filter, CFG_PREFIX "blend-image" );
    blendbench_LoadImage( p_this, &p_sys->p_blend_image, p_sys->i_blend_chroma,
                          psz_cmd, "Blend" );
    free( psz_temp );
    free( psz_cmd );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy video thread output method
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    picture_Release( p_sys->p_base_image );
    picture_Release( p_sys->p_blend_image );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    filter_t *p_blend;

    if( p_sys->b_done )
        return p_pic;

    p_blend = vlc_object_create( p_filter, sizeof(filter_t) );
    if( !p_blend )
    {
        picture_Release( p_pic );
        return NULL;
    }
    p_blend->fmt_out.video = p_sys->p_base_image->format;
    p_blend->fmt_in.video = p_sys->p_blend_image->format;
    p_blend->p_module = module_need( p_blend, "video blending", NULL, false );
    if( !p_blend->p_module )
    {
        picture_Release( p_pic );
        vlc_object_release( p_blend );
        return NULL;
    }

    mtime_t time = mdate();
    for( int i_iter = 0; i_iter < p_sys->i_loops; ++i_iter )
    {
        p_blend->pf_video_blend( p_blend,
                                 p_sys->p_base_image, p_sys->p_blend_image,
                                 0, 0, p_sys->i_alpha );
    }
    time = mdate() - time;

    msg_Info( p_filter, "Blended %d images in %f sec", p_sys->i_loops,
              time / 1000000.0f );
    msg_Info( p_filter, "Speed is: %f images/second, %f pixels/second",
              (float) p_sys->i_loops / time * 1000000,
              (float) p_sys->i_loops / time * 1000000 *
                  p_sys->p_blend_image->p[Y_PLANE].i_visible_pitch *
                  p_sys->p_blend_image->p[Y_PLANE].i_visible_lines );

    module_unneed( p_blend, p_blend->p_module );

    vlc_object_release( p_blend );

    p_sys->b_done = true;
    return p_pic;
}
