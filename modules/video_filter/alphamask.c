/*****************************************************************************
 * alphamask.c : Alpha layer mask video filter for vlc
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 *
 * Authors: Antoine Cellerier <dionoea at videolan tod org>
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

#include <vlc_image.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_url.h>

#define ALPHAMASK_HELP N_( \
    "Use an image's alpha channel as a transparency mask." )

#define MASK_TEXT N_("Transparency mask")
#define MASK_LONGTEXT N_( \
    "Alpha blending transparency mask. Uses a png alpha channel.")

#define CFG_PREFIX "alphamask-"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create      ( vlc_object_t * );
static void Destroy     ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );
static void LoadMask( filter_t *, const char * );
static int MaskCallback( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Alpha mask video filter") )
    set_shortname( N_("Alpha mask" ))
    set_help( ALPHAMASK_HELP )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter", 0 )
    add_shortcut( "alphamask", "mask" )
    set_callbacks( Create, Destroy )

    add_loadfile(CFG_PREFIX "mask", NULL, MASK_TEXT, MASK_LONGTEXT)
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "mask", NULL
};

typedef struct
{
    picture_t *p_mask;
    vlc_mutex_t mask_lock;
} filter_sys_t;

static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    char *psz_string;

    if( p_filter->fmt_in.video.i_chroma != VLC_CODEC_YUVA )
    {
        msg_Err( p_filter,
                 "Unsupported input chroma \"%4.4s\". "
                 "Alphamask can only use \"YUVA\".",
                 (char*)&p_filter->fmt_in.video.i_chroma );
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;
    p_sys = p_filter->p_sys;

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    psz_string =
        var_CreateGetStringCommand( p_filter, CFG_PREFIX "mask" );
    if( psz_string && *psz_string )
    {
        LoadMask( p_filter, psz_string );
        if( !p_sys->p_mask )
            msg_Err( p_filter, "Error while loading mask (%s).",
                     psz_string );
    }
    else
       p_sys->p_mask = NULL;
    free( psz_string );

    vlc_mutex_init( &p_sys->mask_lock );
    var_AddCallback( p_filter, CFG_PREFIX "mask", MaskCallback,
                     p_filter );
    p_filter->pf_video_filter = Filter;

    return VLC_SUCCESS;
}

static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    var_DelCallback( p_filter, CFG_PREFIX "mask", MaskCallback,
                     p_filter );

    if( p_sys->p_mask )
        picture_Release( p_sys->p_mask );

    free( p_sys );
}

static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    vlc_mutex_lock( &p_sys->mask_lock );
    plane_t *p_mask = p_sys->p_mask->p+A_PLANE;
    plane_t *p_apic = p_pic->p+A_PLANE;
    if(    p_mask->i_visible_pitch
        != p_apic->i_visible_pitch
        || p_mask->i_visible_lines
        != p_apic->i_visible_lines )
    {
        msg_Err( p_filter,
                  "Mask size (%d x %d) and image size (%d x %d) "
                  "don't match. The mask will not be applied.",
                  p_mask->i_visible_pitch,
                  p_mask->i_visible_lines,
                  p_apic->i_visible_pitch,
                  p_apic->i_visible_lines );
    }
    else
    {
        plane_CopyPixels( p_apic, p_mask );
    }
    vlc_mutex_unlock( &p_sys->mask_lock );
    return p_pic;
}

/* copied from video_filters/erase.c . Gruik ? */
static void LoadMask( filter_t *p_filter, const char *psz_filename )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    image_handler_t *p_image;
    video_format_t fmt_out;
    video_format_Init( &fmt_out, VLC_CODEC_YUVA );
    if( p_sys->p_mask )
        picture_Release( p_sys->p_mask );
    p_image = image_HandlerCreate( p_filter );
    char *psz_url = vlc_path2uri( psz_filename, NULL );
    p_sys->p_mask =
        image_ReadUrl( p_image, psz_url, &fmt_out );
    free( psz_url );
    video_format_Clean( &fmt_out );
    image_HandlerDelete( p_image );
}

/*****************************************************************************
* Callback to update params on the fly
*****************************************************************************/
static int MaskCallback( vlc_object_t *p_this, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval,
                         void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);
    filter_t *p_filter = (filter_t *)p_data;
    filter_sys_t *p_sys = p_filter->p_sys;
    int i_ret = VLC_SUCCESS;

#define VAR_IS( a ) !strcmp( psz_var, CFG_PREFIX a )
    if( VAR_IS( "mask" ) )
    {
        vlc_mutex_lock( &p_sys->mask_lock );
        if( newval.psz_string && *newval.psz_string )
        {
            LoadMask( p_filter, newval.psz_string );
            if( !p_sys->p_mask )
            {
                msg_Err( p_filter, "Error while loading mask (%s).",
                         newval.psz_string );
                i_ret = VLC_EGENERIC;
            }
        }
        else if( p_sys->p_mask )
        {
            picture_Release( p_sys->p_mask );
            p_sys->p_mask = NULL;
        }
        vlc_mutex_unlock( &p_sys->mask_lock );
    }
#undef VAR_IS

    return i_ret;
}
