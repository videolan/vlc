/*****************************************************************************
 * motion_blur.c : motion blur filter for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2002, 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Antoine Cellerier <dionoea &t videolan d.t org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc_sout.h>
#include <vlc_vout.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Local protypes
 *****************************************************************************/
static int  Create       ( vlc_object_t * );
static void Destroy      ( vlc_object_t * );
static picture_t *Filter ( filter_t *, picture_t * );
static void RenderBlur   ( filter_sys_t *, picture_t *, picture_t * );
static void Copy         ( filter_t *, uint8_t **, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FACTOR_TEXT N_("Blur factor (1-127)")
#define FACTOR_LONGTEXT N_("The degree of blurring from 1 to 127.")

#define PERSISTANT_TEXT N_("Keep inifinte memory of past images")
#define PERSISTANT_LONGTEXT N_("Use \"Result = ( new image * ( 128 - factor ) + factor * old result ) / 128\" instead of \"Result = ( new image * ( 128 - factor ) + factor * old image ) / 128\".")

#define FILTER_PREFIX "blur-"

vlc_module_begin();
    set_shortname( _("Motion blur") );
    set_description( _("Motion blur filter") );
    set_capability( "video filter2", 0 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );

    add_integer_with_range( FILTER_PREFIX "factor", 80, 1, 127, NULL,
                            FACTOR_TEXT, FACTOR_LONGTEXT, VLC_FALSE );
    add_bool( FILTER_PREFIX "persistant", 0, NULL, PERSISTANT_TEXT,
              PERSISTANT_LONGTEXT, VLC_FALSE );

    add_shortcut( "blur" );

    set_callbacks( Create, Destroy );
vlc_module_end();

static const char *ppsz_filter_options[] = {
    "factor", "persistant", NULL
};

/*****************************************************************************
 * filter_sys_t
 *****************************************************************************/
struct filter_sys_t
{
    int        i_factor;
    vlc_bool_t b_persistant;

    uint8_t  **pp_planes;
    int        i_planes;
};

/*****************************************************************************
 * Create
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

    p_filter->pf_video_filter = Filter;

    config_ChainParse( p_filter, FILTER_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    var_Create( p_filter, FILTER_PREFIX "factor",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    p_filter->p_sys->i_factor =
        var_GetInteger( p_filter, FILTER_PREFIX "factor" );
    var_Create( p_filter, FILTER_PREFIX "persistant",
                VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    p_filter->p_sys->b_persistant =
        var_GetBool( p_filter, FILTER_PREFIX "persistant" );

    p_filter->p_sys->pp_planes = NULL;
    p_filter->p_sys->i_planes = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    while( p_filter->p_sys->i_planes-- )
        free( p_filter->p_sys->pp_planes[p_filter->p_sys->i_planes] );
    free( p_filter->p_sys->pp_planes );
    free( p_filter->p_sys );
}

#define RELEASE( pic ) \
        if( pic ->pf_release ) \
            pic ->pf_release( pic );

#define INITPIC( dst, src ) \
    dst ->date = src ->date; \
    dst ->b_force = src ->b_force; \
    dst ->i_nb_fields = src ->i_nb_fields; \
    dst ->b_progressive = src->b_progressive; \
    dst ->b_top_field_first = src ->b_top_field_first;

/*****************************************************************************
 * Filter
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t * p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( !p_pic ) return NULL;

    p_outpic = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        RELEASE( p_pic );
        return NULL;
    }
    INITPIC( p_outpic, p_pic );

    if( !p_sys->pp_planes )
    {
        /* initialise our picture buffer */
        int i_plane;
        p_sys->i_planes = p_pic->i_planes;
        p_sys->pp_planes =
            (uint8_t**)malloc( p_sys->i_planes * sizeof( uint8_t * ) );
        for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
        {
            p_sys->pp_planes[i_plane] = (uint8_t*)malloc(
                p_pic->p[i_plane].i_pitch * p_pic->p[i_plane].i_visible_lines );
        }
        Copy( p_filter, p_sys->pp_planes, p_pic );
    }

    /* Get a new picture */
    RenderBlur( p_sys, p_pic, p_outpic );
    Copy( p_filter, p_sys->pp_planes, p_sys->b_persistant ? p_outpic : p_pic );

    RELEASE( p_pic );
    return p_outpic;
}

/*****************************************************************************
 * RenderBlur: renders a blurred picture
 *****************************************************************************/
static void RenderBlur( filter_sys_t *p_sys, picture_t *p_newpic,
                        picture_t *p_outpic )
{
    int i_plane;
    int i_oldfactor = p_sys->i_factor;
    int i_newfactor = 128 - i_oldfactor;
    for( i_plane = 0; i_plane < p_outpic->i_planes; i_plane++ )
    {
        uint8_t *p_old, *p_new, *p_out, *p_out_end, *p_out_line_end;
        const int i_pitch = p_outpic->p[i_plane].i_pitch;
        const int i_visible_pitch = p_outpic->p[i_plane].i_visible_pitch;
        const int i_visible_lines = p_outpic->p[i_plane].i_visible_lines;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_new = p_newpic->p[i_plane].p_pixels;
        p_old = p_sys->pp_planes[i_plane];
        p_out_end = p_out + i_pitch * i_visible_lines;
        while ( p_out < p_out_end )
        {
            p_out_line_end = p_out + i_visible_pitch;

            while ( p_out < p_out_line_end )
            {
                *p_out++ = (((*p_old++) * i_oldfactor) +
                            ((*p_new++) * i_newfactor)) >> 7;
            }

            p_old += i_pitch - i_visible_pitch;
            p_new += i_pitch - i_visible_pitch;
            p_out += i_pitch - i_visible_pitch;
        }
    }
}

static void Copy( filter_t *p_filter, uint8_t **pp_planes, picture_t *p_pic )
{
    int i_plane;
    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        p_filter->p_libvlc->pf_memcpy(
            p_filter->p_sys->pp_planes[i_plane], p_pic->p[i_plane].p_pixels,
            p_pic->p[i_plane].i_pitch * p_pic->p[i_plane].i_visible_lines );
    }
}
