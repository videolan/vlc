/*****************************************************************************
 * mirror.c : Mirror video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Branko Kokanovic <branko.kokanovic@gmail.com>
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_atomic.h>
#include <vlc_filter.h>
#include "filter_picture.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create      ( vlc_object_t * );
static void Destroy     ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );
static void VerticalMirror( picture_t *, picture_t *, int plane, bool );
static void HorizontalMirror( picture_t *, picture_t *, int, bool );
static void PlanarVerticalMirror( picture_t *, picture_t *, int plane, bool );
static void YUV422VerticalMirror( picture_t *, picture_t *, int plane, bool, bool );
static void RV24VerticalMirror( picture_t *, picture_t *, int plane, bool );
static void RV32VerticalMirror( picture_t *, picture_t *, int plane, bool );

static void YUV422Mirror2Pixels( uint8_t *, uint8_t *, bool );

static const char *const ppsz_filter_options[] = {
    "split", "direction", NULL
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ORIENTATION_TEXT N_("Mirror orientation")
#define ORIENTATION_LONGTEXT N_("Defines orientation of the mirror splitting. \
    Can be vertical or horizontal" )
static const int pi_orientation_values[] = { 0, 1 };
static const char *const ppsz_orientation_descriptions[] = {
  N_("Vertical"), N_("Horizontal") };

#define DIRECTION_TEXT N_("Direction")
#define DIRECTION_LONGTEXT N_("Direction of the mirroring" )
static const int pi_direction_values[] = { 0, 1 };
static const char *const ppsz_direction_descriptions[] = {
  N_("Left to right/Top to bottom"), N_("Right to left/Bottom to top") };

#define CFG_PREFIX "mirror-"

vlc_module_begin ()
    set_description( N_("Mirror video filter") )
    set_shortname( N_("Mirror video" ))
    set_help( N_("Splits video in two same parts, like in a mirror") )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter2", 0 )
    add_integer( CFG_PREFIX "split", 0, ORIENTATION_TEXT,
                ORIENTATION_LONGTEXT, false )
        change_integer_list( pi_orientation_values,
                            ppsz_orientation_descriptions )
    add_integer( CFG_PREFIX "direction", 0, DIRECTION_TEXT,
                DIRECTION_LONGTEXT, false )
        change_integer_list( pi_direction_values, ppsz_direction_descriptions )
    set_callbacks( Create, Destroy )
vlc_module_end ()

/*****************************************************************************
 * callback prototypes
 *****************************************************************************/
static int FilterCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * filter_sys_t: adjust filter method descriptor
 *****************************************************************************/
struct filter_sys_t
{
    atomic_int i_split;
    atomic_int i_direction;
};

/*****************************************************************************
 * Create: allocates Mirror video thread output method
 *****************************************************************************
 * This function allocates and initializes a Mirror vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    switch( p_filter->fmt_in.video.i_chroma )
    {
        CASE_PLANAR_YUV_SQUARE
            break;
        CASE_PACKED_YUV_422
            break;
        case VLC_CODEC_RGB24:
        case VLC_CODEC_RGB32:
            break;

        default:
            msg_Err( p_filter, "Unsupported input chroma (%4.4s)",
                     (char*)&(p_filter->fmt_in.video.i_chroma) );
            return VLC_EGENERIC;
    }

    if( p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma )
    {
        msg_Err( p_filter, "Input and output chromas don't match" );
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );
    atomic_init( &p_sys->i_split,
                 var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "split" ) );
    atomic_init( &p_sys->i_direction,
                 var_CreateGetIntegerCommand( p_filter,
                                                    CFG_PREFIX "direction" ) );

    var_AddCallback( p_filter, CFG_PREFIX "split", FilterCallback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "direction", FilterCallback, p_sys );

    p_filter->pf_video_filter = Filter;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy Mirror video thread output method
 *****************************************************************************
 * Terminate an output method created by MirrorCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    var_DelCallback( p_filter, CFG_PREFIX "split", FilterCallback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "direction", FilterCallback, p_sys );
    free( p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Mirror image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    bool b_vertical_split, b_left_to_right;
    int i_index;

    if( !p_pic ) return NULL;

    filter_sys_t *p_sys = p_filter->p_sys;
    b_vertical_split = !atomic_load( &p_sys->i_split );
    b_left_to_right = !atomic_load( &p_sys->i_direction );

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        picture_Release( p_pic );
        return NULL;
    }

    for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
    {
        if ( b_vertical_split )
            VerticalMirror( p_pic, p_outpic, i_index, b_left_to_right );
        else
            HorizontalMirror( p_pic, p_outpic, i_index, b_left_to_right );
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}

/*****************************************************************************
 * VerticalMirror: Mirrors vertically image
 *****************************************************************************
 * This function is a simple delegate to concrete function for vertical
 * mirroring depending on the input format.
 *****************************************************************************/
static void VerticalMirror( picture_t *p_pic, picture_t *p_outpic, int i_plane,
                           bool b_left_to_right )
{
    switch( p_pic->format.i_chroma )
    {
        CASE_PLANAR_YUV_SQUARE
            PlanarVerticalMirror( p_pic, p_outpic, i_plane, b_left_to_right );
            break;
        case VLC_CODEC_YUYV:
        case VLC_CODEC_YVYU:
            YUV422VerticalMirror( p_pic, p_outpic, i_plane, b_left_to_right,
                                 true );
            break;
        case VLC_CODEC_UYVY:
        case VLC_CODEC_CYUV:
            YUV422VerticalMirror( p_pic, p_outpic, i_plane, b_left_to_right,
                                 false );
            break;
        case VLC_CODEC_RGB24:
            RV24VerticalMirror( p_pic, p_outpic, i_plane, b_left_to_right );
            break;
        case VLC_CODEC_RGB32:
            RV32VerticalMirror( p_pic, p_outpic, i_plane, b_left_to_right );
            break;
        default:
            assert( false );
    }
}

/*****************************************************************************
 * PlanarVerticalMirror: Mirrors vertically image byte by byte
 *****************************************************************************
 * This function mirrors image vertically. It iterates for all lines in
 * image and for every line, it mirrors byte for byte if needed.
 * This function works for planar formats only.
 *****************************************************************************/
static void PlanarVerticalMirror( picture_t *p_pic, picture_t *p_outpic,
                                 int i_plane, bool b_left_to_right )
{
    uint8_t *p_in, *p_in_end, *p_line_start, *p_line_end, *p_out;

    p_in = p_pic->p[i_plane].p_pixels;
    p_in_end = p_in + p_pic->p[i_plane].i_visible_lines
        * p_pic->p[i_plane].i_pitch;
    p_out = p_outpic->p[i_plane].p_pixels;

    while( p_in < p_in_end ) {
        p_line_start = p_in;
        p_line_end = p_in + p_pic->p[i_plane].i_visible_pitch;
        while( p_in < p_line_end )
        {
            /* are we in the left part of the line */
            if ( p_in < p_line_start + ( p_line_end - p_line_start ) / 2 )
            {
                if ( b_left_to_right )
                    *p_out = *p_in;
                else
                    *p_out = *( p_line_end - ( p_in - p_line_start ) );
            }
            else
            {
                if ( b_left_to_right )
                    *p_out = *( p_line_start + ( p_line_end - p_in ) );
                else
                    *p_out = *p_in;
            }
            p_in++;
            p_out++;
        }
        p_in += p_pic->p[i_plane].i_pitch - p_pic->p[i_plane].i_visible_pitch;
        p_out += p_outpic->p[i_plane].i_pitch
            - p_outpic->p[i_plane].i_visible_pitch;
    }
}

/*****************************************************************************
 * YUV422VerticalMirror: Mirrors vertically image byte by byte for YUV422 format
 *****************************************************************************
 * This function mirrors image vertically. It iterates for all lines in
 * image and for every line, iterates for 4-byte chucks, properly mirroring
 * them vertically (swapping Y components and keeping Cb and Cr components).
 * This function works only for YUV422 packed formats.
 *****************************************************************************/
static void YUV422VerticalMirror( picture_t *p_pic, picture_t *p_outpic,
                                 int i_plane, bool b_left_to_right,
                                 bool b_y_is_odd )
{
    uint8_t *p_in, *p_in_end, *p_line_start, *p_line_end, *p_out;

    p_in = p_pic->p[i_plane].p_pixels;
    p_in_end = p_in + p_pic->p[i_plane].i_visible_lines
        * p_pic->p[i_plane].i_pitch;
    p_out = p_outpic->p[i_plane].p_pixels;

    while( p_in < p_in_end )
    {
        p_line_start = p_in;
        p_line_end = p_in + p_pic->p[i_plane].i_visible_pitch;
        while( p_in < p_line_end )
        {
            /* are we in the left part of the line */
            if ( p_in < p_line_start + ( p_line_end - p_line_start ) / 2 )
            {
                if ( b_left_to_right )
                {
                    *p_out++ = *p_in++;
                    *p_out++ = *p_in++;
                    *p_out++ = *p_in++;
                    *p_out++ = *p_in++;
                }
                else
                {
                    uint8_t *p_start = p_line_end - ( p_in - p_line_start );
                    YUV422Mirror2Pixels( p_out, p_start, b_y_is_odd );
                    p_in += 4;
                    p_out += 4;
                }
            }
            else
            {
                if ( b_left_to_right )
                {
                    uint8_t *p_start = p_line_end - ( p_in - p_line_start );
                    YUV422Mirror2Pixels( p_out, p_start, b_y_is_odd );
                    p_in += 4;
                    p_out += 4;
                }
                else
                {
                    *p_out++ = *p_in++;
                    *p_out++ = *p_in++;
                    *p_out++ = *p_in++;
                    *p_out++ = *p_in++;
                }
            }
        }
        p_in += p_pic->p[i_plane].i_pitch - p_pic->p[i_plane].i_visible_pitch;
        p_out += p_outpic->p[i_plane].i_pitch
            - p_outpic->p[i_plane].i_visible_pitch;
    }
}

/*****************************************************************************
 * YUV422Mirror2Pixels: Mirrors 2 consecutive pixels
 *****************************************************************************
 * This function mirrors two consecutive pixels of 4 byte size. Depending of
 * position of Y components (b_y_is_odd is true if Y components are first and
 * third (like in YUYV or YVYU), they are properly swapped, and Cb and Cr
 * component positions are preserved.
 *****************************************************************************/
static void YUV422Mirror2Pixels( uint8_t* p_dst, uint8_t *p_src,
                                bool b_y_is_odd )
{
    if ( b_y_is_odd )
    {
        /* swap Y components */
        *p_dst = *( p_src + 2 );
        *( p_dst + 2 ) = *p_src;
        /* copy Cb and Cr components */
        *( p_dst + 1 ) = *( p_src + 1 );
        *( p_dst + 3 ) = *( p_src + 3 );
    }
    else{
        /* swap Y components */
        *( p_dst + 1 )= *( p_src + 3 );
        *( p_dst + 3 ) = *( p_src + 1);
        /* copy Cb and Cr components */
        *p_dst = *( p_src + 2 );
        *( p_dst + 2 ) = *p_src;
    }
}

/*****************************************************************************
 * RV24VerticalMirror: Mirrors vertically image byte by byte for RV24 format
 *****************************************************************************
 * This function mirrors image vertically. It iterates for all lines in
 * image and for every line, it iterates for 3-byte chunks.
 * This function works only for RV24 formats.
 *****************************************************************************/
static void RV24VerticalMirror( picture_t *p_pic, picture_t *p_outpic,
                                 int i_plane, bool b_left_to_right )
{
    uint8_t *p_in, *p_in_end, *p_line_start, *p_line_end, *p_out;

    p_in = p_pic->p[i_plane].p_pixels;
    p_in_end = p_in + p_pic->p[i_plane].i_visible_lines
        * p_pic->p[i_plane].i_pitch;
    p_out = p_outpic->p[i_plane].p_pixels;

    while( p_in < p_in_end )
    {
        p_line_start = p_in;
        p_line_end = p_in + p_pic->p[i_plane].i_visible_pitch;
        while( p_in < p_line_end )
        {
            /* are we in the left part of the line */
            if ( p_in < p_line_start + ( p_line_end - p_line_start ) / 2 )
            {
                if ( b_left_to_right )
                {
                    *p_out++ = *p_in++;
                    *p_out++ = *p_in++;
                    *p_out++ = *p_in++;
                }
                else
                {
                    uint8_t *p_pixel = p_line_end - ( p_in - p_line_start );
                    p_in += 3;
                    *p_out++ = *p_pixel++;
                    *p_out++ = *p_pixel++;
                    *p_out++ = *p_pixel++;
                }
            }
            else
            {
                if ( b_left_to_right )
                {
                    uint8_t *p_pixel = p_line_end - ( p_in - p_line_start );
                    p_in += 3;
                    *p_out++ = *p_pixel++;
                    *p_out++ = *p_pixel++;
                    *p_out++ = *p_pixel++;
                }
                else
                {
                    *p_out++ = *p_in++;
                    *p_out++ = *p_in++;
                    *p_out++ = *p_in++;
                }
            }
        }
        p_in += p_pic->p[i_plane].i_pitch - p_pic->p[i_plane].i_visible_pitch;
        p_out += p_outpic->p[i_plane].i_pitch
            - p_outpic->p[i_plane].i_visible_pitch;
    }
}

/*****************************************************************************
 * RV32VerticalMirror: Mirrors vertically image byte by byte for RV32 format
 *****************************************************************************
 * This function mirrors image vertically. It iterates for all lines in
 * image and for every line, it iterates for 4-byte chunks as 32-bit pointers.
 * This function works only for RV32 formats.
 *****************************************************************************/
static void RV32VerticalMirror( picture_t *p_pic, picture_t *p_outpic,
                                 int i_plane, bool b_left_to_right )
{
    uint8_t *p_in, *p_in_end, *p_out;

    p_in = p_pic->p[i_plane].p_pixels;
    p_in_end = p_in + p_pic->p[i_plane].i_visible_lines
        * p_pic->p[i_plane].i_pitch;
    p_out = p_outpic->p[i_plane].p_pixels;

    while( p_in < p_in_end )
    {
        uint32_t *p_in32, *p_out32, *p_line_start32, *p_line_end32;
        p_in32 = (uint32_t*) p_in;
        p_out32 = (uint32_t*) p_out;
        p_line_start32 = p_in32;
        p_line_end32 = (uint32_t*) ( p_in + p_pic->p[i_plane].i_visible_pitch) ;

        while( p_in32 < p_line_end32 )
        {
            /* are we in the left part of the line */
            if ( p_in32 < p_line_start32 + ( p_line_end32 - p_line_start32 ) / 2 )
            {
                if ( b_left_to_right )
                {
                    *p_out32++ = *p_in32++;
                }
                else
                {
                    uint32_t *p_pixel32 = p_line_end32 - ( p_in32 - p_line_start32 );
                    p_in32++;
                    *p_out++ = *p_pixel32;
                }
            }
            else
            {
                if ( b_left_to_right )
                {
                    uint32_t *p_pixel32 = p_line_end32 - ( p_in32 - p_line_start32 );
                    p_in32++;
                    *p_out++ = *p_pixel32;
                }
                else
                {
                    *p_out32++ = *p_in32++;
                }
            }
        }
        p_in = (uint8_t*) p_in32;
        p_out = (uint8_t*) p_out32;
        p_in += p_pic->p[i_plane].i_pitch - p_pic->p[i_plane].i_visible_pitch;
        p_out += p_outpic->p[i_plane].i_pitch
            - p_outpic->p[i_plane].i_visible_pitch;
    }
}

/*****************************************************************************
 * HorizontalMirror: Mirrors horizontally image byte by byte
 *****************************************************************************
 * This function mirrors image horizontally. It iterates for all lines in
 * image and for every line, determines if it should be copied, and if it does,
 * finds opposite line in picture and copies current byte from opposite line.
 * This function works both for planar, packed and RV24 formats.
 *****************************************************************************/
static void HorizontalMirror( picture_t *p_pic, picture_t *p_outpic, int i_plane,
                             bool b_top_to_bottom )
{
    uint8_t *p_in, *p_in_end, *p_line_start, *p_line_end, *p_out;

    int i_curr_line = 0;
    int i_max_lines = p_pic->p[i_plane].i_visible_lines;

    p_in = p_pic->p[i_plane].p_pixels;
    p_in_end = p_in + p_pic->p[i_plane].i_visible_lines
        * p_pic->p[i_plane].i_pitch;
    p_out = p_outpic->p[i_plane].p_pixels;

    while( p_in < p_in_end )
    {
        p_line_start = p_in;
        p_line_end = p_in + p_pic->p[i_plane].i_visible_pitch;
        while( p_in < p_line_end )
        {
            uint8_t *p_inverse_line;
            /* are we in the upper part of the picture */
            if ( i_curr_line < i_max_lines/2 )
            {
                if ( b_top_to_bottom )
                {
                    *p_out = *p_in;
                }
                else
                {
                    /* determines line inverse to current line */
                    p_inverse_line = p_pic->p[i_plane].p_pixels +
                        ( i_max_lines - i_curr_line - 1 ) * p_pic->p[i_plane].i_pitch;
                    *p_out = *( p_inverse_line + ( p_in - p_line_start ) );
                }
            }
            else
            {
                if ( b_top_to_bottom )
                {
                    /* determines line inverse to current line */
                    p_inverse_line = p_pic->p[i_plane].p_pixels +
                        ( i_max_lines - i_curr_line - 1 ) * p_pic->p[i_plane].i_pitch;
                    *p_out = *( p_inverse_line + ( p_in - p_line_start ) );
                }
                else
                {
                    *p_out = *p_in;
                }
            }
            p_in++;
            p_out++;
        }
        i_curr_line++;
        p_in += p_pic->p[i_plane].i_pitch - p_pic->p[i_plane].i_visible_pitch;
        p_out += p_outpic->p[i_plane].i_pitch
            - p_outpic->p[i_plane].i_visible_pitch;
    }
}

static int FilterCallback ( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    (void) p_this; (void)oldval;
    filter_sys_t *p_sys = p_data;

    if( !strcmp( psz_var, CFG_PREFIX "split" ) )
        atomic_store( &p_sys->i_split, newval.i_int );
    else /* CFG_PREFIX "direction" */
        atomic_store( &p_sys->i_direction, newval.i_int );

    return VLC_SUCCESS;
}
