/*****************************************************************************
 * extract.c : Extract RGB components
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea .t videolan d@t org>
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
#include <vlc_vout.h>

#include "vlc_filter.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create      ( vlc_object_t * );
static void Destroy     ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

#define COMPONENT_TEXT N_("RGB component to extract")
#define COMPONENT_LONGTEXT N_("RGB component to extract. 0 for Red, 1 for Green and 2 for Blue.")
#define FILTER_PREFIX "extract-"

static int pi_component_values[] = { 0, 1, 2 };
static const char *ppsz_component_descriptions[] = { "Red", "Green", "Blue" };

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Extract RGB component video filter") );
    set_shortname( _("Extract" ));
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );
    set_capability( "video filter2", 0 );
    add_shortcut( "extract" );

    add_integer( FILTER_PREFIX "component", 0, NULL,
                 COMPONENT_TEXT, COMPONENT_LONGTEXT, VLC_FALSE );
        change_integer_list( pi_component_values, ppsz_component_descriptions, 0 );

    set_callbacks( Create, Destroy );
vlc_module_end();

static const char *ppsz_filter_options[] = {
    "component", NULL
};

enum { RED, GREEN, BLUE };
struct filter_sys_t
{
    int i_color;
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

    config_ChainParse( p_filter, FILTER_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    var_Create( p_filter, FILTER_PREFIX "component",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    p_filter->p_sys->i_color = var_GetInteger( p_filter,
                                               FILTER_PREFIX "component" );

    p_filter->pf_video_filter = Filter;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    free( p_filter->p_sys );
}

/*****************************************************************************
 * Render
 *****************************************************************************/
static void get_red_from_yuv420( picture_t *, picture_t *, int, int, int );
static void get_green_from_yuv420( picture_t *, picture_t *, int, int, int );
static void get_blue_from_yuv420( picture_t *, picture_t *, int, int, int );
static void get_red_from_yuv422( picture_t *, picture_t *, int, int, int );
static void get_green_from_yuv422( picture_t *, picture_t *, int, int, int );
static void get_blue_from_yuv422( picture_t *, picture_t *, int, int, int );

static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;

    if( !p_pic ) return NULL;

    p_outpic = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        if( p_pic->pf_release )
            p_pic->pf_release( p_pic );
        return NULL;
    }

    switch( p_pic->format.i_chroma )
    {
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','Y','U','V'):
        case VLC_FOURCC('J','4','2','0'):
        case VLC_FOURCC('Y','V','1','2'):
            switch( p_filter->p_sys->i_color )
            {
                case RED:
                    get_red_from_yuv420( p_pic, p_outpic,
                                         Y_PLANE, U_PLANE, V_PLANE );
                    break;
                case GREEN:
                    get_green_from_yuv420( p_pic, p_outpic,
                                           Y_PLANE, U_PLANE, V_PLANE );
                    break;
                case BLUE:
                    get_blue_from_yuv420( p_pic, p_outpic,
                                          Y_PLANE, U_PLANE, V_PLANE );
                    break;
            }
            break;

        case VLC_FOURCC('I','4','2','2'):
        case VLC_FOURCC('J','4','2','2'):
            switch( p_filter->p_sys->i_color )
            {
                case RED:
                    get_red_from_yuv422( p_pic, p_outpic,
                                         Y_PLANE, U_PLANE, V_PLANE );
                    break;
                case GREEN:
                    get_green_from_yuv422( p_pic, p_outpic,
                                           Y_PLANE, U_PLANE, V_PLANE );
                    break;
                case BLUE:
                    get_blue_from_yuv422( p_pic, p_outpic,
                                          Y_PLANE, U_PLANE, V_PLANE );
                    break;
            }
            break;

        default:
            msg_Warn( p_filter, "Unsupported input chroma (%4s)",
                      (char*)&(p_pic->format.i_chroma) );
            if( p_pic->pf_release )
                p_pic->pf_release( p_pic );
            return NULL;
    }

    p_outpic->date = p_pic->date;
    p_outpic->b_force = p_pic->b_force;
    p_outpic->i_nb_fields = p_pic->i_nb_fields;
    p_outpic->b_progressive = p_pic->b_progressive;
    p_outpic->b_top_field_first = p_pic->b_top_field_first;

    if( p_pic->pf_release )
        p_pic->pf_release( p_pic );

    return p_outpic;
}

inline uint8_t crop( int a );
inline uint8_t crop( int a )
{
    if( a < 0 ) return 0;
    if( a > 255 ) return 255;
    else return (uint8_t)a;
}

#define U 128
#define V 128

static void get_red_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_pitch = p_inpic->p[yp].i_pitch;
    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;

    const int i_uv_pitch = p_inpic->p[up].i_pitch;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_pitch;
        y2out = y1out + i_pitch;
        while( y1in < y1end )
        {
/*
19595   0   27473
-11058  0   -15504
32768   0   45941
*/
            *uout++ = crop( (*y1in * -11058 + (*vin - V) * -15504)
                      / 65536 + U );
            *vout++ = crop( (*y1in * 32768 + (*vin - V) * 45941)
                      / 65536 + V );
            *y1out++ = crop( (*y1in++ * 19595 + (*vin - V) * 27473)
                       / 65536 );
            *y1out++ = crop( (*y1in++ * 19595 + (*vin - V) * 27473)
                       / 65536 );
            *y2out++ = crop( (*y2in++ * 19594 + (*vin - V) * 27473)
                       / 65536 );
            *y2out++ = crop( (*y2in++ * 19594 + (*vin++ - V) * 27473)
                       / 65536 );
        }
        y1in  += 2*i_pitch - i_visible_pitch;
        y1out += 2*i_pitch - i_visible_pitch;
        uout  += i_uv_pitch - i_uv_visible_pitch;
        vin   += i_uv_pitch - i_uv_visible_pitch;
        vout  += i_uv_pitch - i_uv_visible_pitch;
    }
}

static void get_green_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_pitch = p_inpic->p[yp].i_pitch;
    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;

    const int i_uv_pitch = p_inpic->p[up].i_pitch;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_pitch;
        y2out = y1out + i_pitch;
        while( y1in < y1end )
        {
/*
38470   -13239  -27473
-21710  7471    15504
-27439  9443    19595
*/
            *uout++ = crop( (*y1in * -21710 + (*uin-U) * 7471 + (*vin-V) * 15504)
                      / 65536 + U );
            *vout++ = crop( (*y1in * -27439 + (*uin-U) * 9443 + (*vin-V) * 19595)
                      / 65536 + V );
            *y1out++ = crop( (*y1in++ * 38470 + (*uin-U) * -13239 + (*vin-V) * -27473)
                       / 65536 );
            *y1out++ = crop( (*y1in++ * 38470 + (*uin-U) * -13239 + (*vin-V) * -27473)
                       / 65536 );
            *y2out++ = crop( (*y2in++ * 38470 + (*uin-U) * -13239 + (*vin-V) * -27473)
                       / 65536 );
            *y2out++ = crop( (*y2in++ * 38470 + (*uin++ - U) * -13239 + (*vin++ -128) * -27473)
                       / 65536 );
        }
        y1in  += 2*i_pitch - i_visible_pitch;
        y1out += 2*i_pitch - i_visible_pitch;
        uin   += i_uv_pitch - i_uv_visible_pitch;
        uout  += i_uv_pitch - i_uv_visible_pitch;
        vin   += i_uv_pitch - i_uv_visible_pitch;
        vout  += i_uv_pitch - i_uv_visible_pitch;
    }
}

static void get_blue_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_pitch = p_inpic->p[yp].i_pitch;
    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;

    const int i_uv_pitch = p_inpic->p[up].i_pitch;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_pitch;
        y2out = y1out + i_pitch;
        while( y1in < y1end )
        {
/*
7471    13239   0
32768   58065   0
-5329   -9443   0
*/
            *uout++ = crop( (*y1in* 32768 + (*uin - U) * 58065 )
                      / 65536 + U );
            *vout++ = crop( (*y1in * -5329 + (*uin - U) * -9443 )
                      / 65536 + V );
            *y1out++ = crop( (*y1in++ * 7471 + (*uin - U) * 13239 )
                       / 65536 );
            *y1out++ = crop( (*y1in++ * 7471 + (*uin - U) * 13239 )
                       / 65536 );
            *y2out++ = crop( (*y2in++ * 7471 + (*uin - U) * 13239 )
                       / 65536 );
            *y2out++ = crop( (*y2in++ * 7471 + (*uin++ - U) * 13239 )
                       / 65536 );
        }
        y1in  += 2*i_pitch - i_visible_pitch;
        y1out += 2*i_pitch - i_visible_pitch;
        uin   += i_uv_pitch - i_uv_visible_pitch;
        uout  += i_uv_pitch - i_uv_visible_pitch;
        vout  += i_uv_pitch - i_uv_visible_pitch;
    }
}

static void get_red_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_pitch = p_inpic->p[yp].i_pitch;
    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;

    const int i_uv_pitch = p_inpic->p[up].i_pitch;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
/*
19595   0   27473
-11058  0   -15504
32768   0   45941
*/
            *uout++ = crop( (*y1in * -11058 + (*vin - V) * -15504)
                      / 65536 + U );
            *vout++ = crop( (*y1in * 32768 + (*vin - V) * 45941)
                      / 65536 + V );
            *y1out++ = crop( (*y1in++ * 19595 + (*vin - V) * 27473)
                       / 65536 );
            *y1out++ = crop( (*y1in++ * 19595 + (*vin++ - V) * 27473)
                       / 65536 );
        }
        y1in  += i_pitch - i_visible_pitch;
        y1out += i_pitch - i_visible_pitch;
        uout  += i_uv_pitch - i_uv_visible_pitch;
        vin   += i_uv_pitch - i_uv_visible_pitch;
        vout  += i_uv_pitch - i_uv_visible_pitch;
    }
}

static void get_green_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                   int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_pitch = p_inpic->p[yp].i_pitch;
    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;

    const int i_uv_pitch = p_inpic->p[up].i_pitch;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
/*
38470   -13239  -27473
-21710  7471    15504
-27439  9443    19595
*/
            *uout++ = crop( (*y1in * -21710 + (*uin-U) * 7471 + (*vin-V) * 15504)
                      / 65536 + U );
            *vout++ = crop( (*y1in * -27439 + (*uin-U) * 9443 + (*vin-V) * 19595)
                      / 65536 + V );
            *y1out++ = crop( (*y1in++ * 38470 + (*uin-U) * -13239 + (*vin-V) * -27473)
                       / 65536 );
            *y1out++ = crop( (*y1in++ * 38470 + (*uin++-U) * -13239 + (*vin++-V) * -27473)
                       / 65536 );
        }
        y1in  += i_pitch - i_visible_pitch;
        y1out += i_pitch - i_visible_pitch;
        uin   += i_uv_pitch - i_uv_visible_pitch;
        uout  += i_uv_pitch - i_uv_visible_pitch;
        vin   += i_uv_pitch - i_uv_visible_pitch;
        vout  += i_uv_pitch - i_uv_visible_pitch;
    }
}

static void get_blue_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_pitch = p_inpic->p[yp].i_pitch;
    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;

    const int i_uv_pitch = p_inpic->p[up].i_pitch;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
/*
7471    13239   0
32768   58065   0
-5329   -9443   0
*/
            *uout++ = crop( (*y1in* 32768 + (*uin - U) * 58065 )
                      / 65536 + U );
            *vout++ = crop( (*y1in * -5329 + (*uin - U) * -9443 )
                      / 65536 + V );
            *y1out++ = crop( (*y1in++ * 7471 + (*uin - U) * 13239 )
                       / 65536 );
            *y1out++ = crop( (*y1in++ * 7471 + (*uin++ - U) * 13239 )
                       / 65536 );
        }
        y1in  += i_pitch - i_visible_pitch;
        y1out += i_pitch - i_visible_pitch;
        uin   += i_uv_pitch - i_uv_visible_pitch;
        uout  += i_uv_pitch - i_uv_visible_pitch;
        vout  += i_uv_pitch - i_uv_visible_pitch;
    }
}
