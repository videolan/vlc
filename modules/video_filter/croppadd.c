/*****************************************************************************
 * croppadd.c: Crop/Padd image filter
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 *
 * Authors: Antoine Cellerier <dionoea @t videolan dot org>
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

#include <limits.h> /* INT_MAX */

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include "filter_picture.h"

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenFilter ( vlc_object_t * );
static void CloseFilter( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

#define CROPTOP_TEXT N_( "Pixels to crop from top" )
#define CROPTOP_LONGTEXT N_( \
    "Number of pixels to crop from the top of the image." )
#define CROPBOTTOM_TEXT N_( "Pixels to crop from bottom" )
#define CROPBOTTOM_LONGTEXT N_( \
    "Number of pixels to crop from the bottom of the image." )
#define CROPLEFT_TEXT N_( "Pixels to crop from left" )
#define CROPLEFT_LONGTEXT N_( \
    "Number of pixels to crop from the left of the image." )
#define CROPRIGHT_TEXT N_( "Pixels to crop from right" )
#define CROPRIGHT_LONGTEXT N_( \
    "Number of pixels to crop from the right of the image." )

#define PADDTOP_TEXT N_( "Pixels to padd to top" )
#define PADDTOP_LONGTEXT N_( \
    "Number of pixels to padd to the top of the image after cropping." )
#define PADDBOTTOM_TEXT N_( "Pixels to padd to bottom" )
#define PADDBOTTOM_LONGTEXT N_( \
    "Number of pixels to padd to the bottom of the image after cropping." )
#define PADDLEFT_TEXT N_( "Pixels to padd to left" )
#define PADDLEFT_LONGTEXT N_( \
    "Number of pixels to padd to the left of the image after cropping." )
#define PADDRIGHT_TEXT N_( "Pixels to padd to right" )
#define PADDRIGHT_LONGTEXT N_( \
    "Number of pixels to padd to the right of the image after cropping." )

#define CFG_PREFIX "croppadd-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname( N_("Croppadd") )
    set_description( N_("Video cropping filter") )
    set_capability( "video filter", 0 )
    set_callbacks( OpenFilter, CloseFilter )

    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER );

    set_section( N_("Crop"), NULL )
        add_integer_with_range( CFG_PREFIX "croptop", 0, 0, INT_MAX,
                                CROPTOP_TEXT, CROPTOP_LONGTEXT, false )
        add_integer_with_range( CFG_PREFIX "cropbottom", 0, 0, INT_MAX,
                                CROPBOTTOM_TEXT, CROPBOTTOM_LONGTEXT, false )
        add_integer_with_range( CFG_PREFIX "cropleft", 0, 0, INT_MAX,
                                CROPLEFT_TEXT, CROPLEFT_LONGTEXT, false )
        add_integer_with_range( CFG_PREFIX "cropright", 0, 0, INT_MAX,
                                CROPRIGHT_TEXT, CROPRIGHT_LONGTEXT, false )

    set_section( N_("Padd"), NULL )
        add_integer_with_range( CFG_PREFIX "paddtop", 0, 0, INT_MAX,
                                PADDTOP_TEXT, PADDTOP_LONGTEXT, false )
        add_integer_with_range( CFG_PREFIX "paddbottom", 0, 0, INT_MAX,
                                PADDBOTTOM_TEXT, PADDBOTTOM_LONGTEXT, false )
        add_integer_with_range( CFG_PREFIX "paddleft", 0, 0, INT_MAX,
                                PADDLEFT_TEXT, PADDLEFT_LONGTEXT, false )
        add_integer_with_range( CFG_PREFIX "paddright", 0, 0, INT_MAX,
                                PADDRIGHT_TEXT, PADDRIGHT_LONGTEXT, false )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "croptop", "cropbottom", "cropleft", "cropright",
    "paddtop", "paddbottom", "paddleft", "paddright",
    NULL
};

typedef struct
{
    int i_croptop;
    int i_cropbottom;
    int i_cropleft;
    int i_cropright;
    int i_paddtop;
    int i_paddbottom;
    int i_paddleft;
    int i_paddright;
} filter_sys_t;

/*****************************************************************************
 * OpenFilter: probe the filter and return score
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys;

    if( !p_filter->b_allow_fmt_out_change )
    {
        msg_Err( p_filter, "Picture format change isn't allowed" );
        return VLC_EGENERIC;
    }

    if( p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma )
    {
        msg_Err( p_filter, "Input and output chromas don't match" );
        /* In fact we don't really care about this since we're allowed
         * to change the output format ... FIXME? */
        return VLC_EGENERIC;
    }

    const vlc_chroma_description_t *p_chroma =
        vlc_fourcc_GetChromaDescription( p_filter->fmt_in.video.i_chroma );
    if( p_chroma == NULL || p_chroma->plane_count == 0 )
    {
        msg_Err( p_filter, "Unknown input chroma %4.4s", p_filter->fmt_in.video.i_chroma?
                     (const char*)&p_filter->fmt_in.video.i_chroma : "xxxx" );
        return VLC_EGENERIC;
    }

    p_filter->p_sys = (filter_sys_t *)malloc( sizeof( filter_sys_t ) );
    if( !p_filter->p_sys ) return VLC_ENOMEM;

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    p_sys = p_filter->p_sys;
#define GET_OPTION( name ) \
    p_sys->i_ ## name = var_CreateGetInteger( p_filter, CFG_PREFIX #name ); \
    if( p_sys->i_ ## name & 1 ) \
        msg_Warn( p_filter, "Using even values for `" #name "' is recommended" );
    GET_OPTION( croptop )
    GET_OPTION( cropbottom )
    GET_OPTION( cropleft )
    GET_OPTION( cropright )
    GET_OPTION( paddtop )
    GET_OPTION( paddbottom )
    GET_OPTION( paddleft )
    GET_OPTION( paddright )

    p_filter->fmt_out.video.i_height =
    p_filter->fmt_out.video.i_visible_height =
        p_filter->fmt_in.video.i_visible_height
        - p_sys->i_croptop - p_sys->i_cropbottom
        + p_sys->i_paddtop + p_sys->i_paddbottom;

    p_filter->fmt_out.video.i_width =
    p_filter->fmt_out.video.i_visible_width =
        p_filter->fmt_in.video.i_visible_width
        - p_sys->i_cropleft - p_sys->i_cropright
        + p_sys->i_paddleft + p_sys->i_paddright;

    p_filter->pf_video_filter = Filter;

    msg_Dbg( p_filter, "Crop: Top: %d, Bottom: %d, Left: %d, Right: %d",
             p_sys->i_croptop, p_sys->i_cropbottom, p_sys->i_cropleft,
             p_sys->i_cropright );
    msg_Dbg( p_filter, "Padd: Top: %d, Bottom: %d, Left: %d, Right: %d",
             p_sys->i_paddtop, p_sys->i_paddbottom, p_sys->i_paddleft,
             p_sys->i_paddright );
    msg_Dbg( p_filter, "%dx%d -> %dx%d",
             p_filter->fmt_in.video.i_width,
             p_filter->fmt_in.video.i_height,
             p_filter->fmt_out.video.i_width,
             p_filter->fmt_out.video.i_height );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseFilter: clean up the filter
 *****************************************************************************/
static void CloseFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    free( p_filter->p_sys );
}

/****************************************************************************
 * Filter: the whole thing
 ****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    picture_t *p_outpic;
    int i_width, i_height, i_xcrop, i_ycrop,
        i_outwidth, i_outheight, i_xpadd, i_ypadd;

    const int p_padd_color[] = { 0x00, 0x80, 0x80, 0xff };

    if( !p_pic ) return NULL;

    /* Request output picture */
    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    for( int i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    /* p_pic and p_outpic have the same chroma/number of planes but that's
     * about it. */
    {
        plane_t *p_plane = p_pic->p+i_plane;
        plane_t *p_outplane = p_outpic->p+i_plane;
        uint8_t *p_in = p_plane->p_pixels;
        uint8_t *p_out = p_outplane->p_pixels;
        int i_pixel_pitch = p_plane->i_pixel_pitch;
        int i_padd_color = i_plane > 3 ? p_padd_color[0]
                                       : p_padd_color[i_plane];

        /* These assignments assume that the first plane always has
         * a width and height equal to the picture's */
        i_width =     ( ( p_filter->fmt_in.video.i_visible_width
                          - p_sys->i_cropleft - p_sys->i_cropright )
                        * p_plane->i_visible_pitch )
                      / p_pic->p->i_visible_pitch;
        i_height =    ( ( p_filter->fmt_in.video.i_visible_height
                          - p_sys->i_croptop - p_sys->i_cropbottom )
                        * p_plane->i_visible_lines )
                      / p_pic->p->i_visible_lines;
        i_xcrop =     ( p_sys->i_cropleft * p_plane->i_visible_pitch)
                      / p_pic->p->i_visible_pitch;
        i_ycrop =     ( p_sys->i_croptop * p_plane->i_visible_lines)
                      / p_pic->p->i_visible_lines;
        i_outwidth =  ( p_filter->fmt_out.video.i_visible_width
                        * p_outplane->i_visible_pitch )
                      / p_outpic->p->i_visible_pitch;
        i_outheight = ( p_filter->fmt_out.video.i_visible_height
                        * p_outplane->i_visible_lines )
                      / p_outpic->p->i_visible_lines;
        i_xpadd =     ( p_sys->i_paddleft * p_outplane->i_visible_pitch )
                      / p_outpic->p->i_visible_pitch;
        i_ypadd =     ( p_sys->i_paddtop * p_outplane->i_visible_lines )
                       / p_outpic->p->i_visible_lines;

        /* Crop the top */
        p_in += i_ycrop * p_plane->i_pitch;

        /* Padd on the top */
        memset( p_out, i_padd_color, i_ypadd * p_outplane->i_pitch );
        p_out += i_ypadd * p_outplane->i_pitch;

        for( int i_line = 0; i_line < i_height; i_line++ )
        {
            uint8_t *p_in_next = p_in + p_plane->i_pitch;
            uint8_t *p_out_next = p_out + p_outplane->i_pitch;

            /* Crop on the left */
            p_in += i_xcrop * i_pixel_pitch;

            /* Padd on the left */
            memset( p_out, i_padd_color, i_xpadd * i_pixel_pitch );
            p_out += i_xpadd * i_pixel_pitch;

            /* Copy the image and crop on the right */
            memcpy( p_out, p_in, i_width * i_pixel_pitch );
            p_out += i_width * i_pixel_pitch;
            p_in += i_width * i_pixel_pitch;

            /* Padd on the right */
            memset( p_out, i_padd_color,
                        ( i_outwidth - i_xpadd - i_width ) * i_pixel_pitch );

            /* Got to begining of the next line */
            p_in = p_in_next;
            p_out = p_out_next;
        }

        /* Padd on the bottom */
        memset( p_out, i_padd_color,
                 ( i_outheight - i_ypadd - i_height ) * p_outplane->i_pitch );
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}
