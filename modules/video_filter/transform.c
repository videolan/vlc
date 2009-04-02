/*****************************************************************************
 * transform.c : transform image module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <vlc_vout.h>

#include "filter_common.h"
#include "filter_picture.h"

#define TRANSFORM_MODE_HFLIP   1
#define TRANSFORM_MODE_VFLIP   2
#define TRANSFORM_MODE_90      3
#define TRANSFORM_MODE_180     4
#define TRANSFORM_MODE_270     5

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static void FilterPlanar( vout_thread_t *, const picture_t *, picture_t * );
static void FilterI422( vout_thread_t *, const picture_t *, picture_t * );
static void FilterYUYV( vout_thread_t *, const picture_t *, picture_t * );

static int  MouseEvent( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define TYPE_TEXT N_("Transform type")
#define TYPE_LONGTEXT N_("One of '90', '180', '270', 'hflip' and 'vflip'")

static const char *const type_list[] = { "90", "180", "270", "hflip", "vflip" };
static const char *const type_list_text[] = { N_("Rotate by 90 degrees"),
  N_("Rotate by 180 degrees"), N_("Rotate by 270 degrees"),
  N_("Flip horizontally"), N_("Flip vertically") };

#define CFG_PREFIX "transform-"

vlc_module_begin ()
    set_description( N_("Video transformation filter") )
    set_shortname( N_("Transformation"))
    set_capability( "video filter", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_string( CFG_PREFIX "type", "90", NULL,
                          TYPE_TEXT, TYPE_LONGTEXT, false)
        change_string_list( type_list, type_list_text, 0)

    add_shortcut( "transform" )
    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "type", NULL
};

/*****************************************************************************
 * vout_sys_t: Transform video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Transform specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    int i_mode;
    bool b_rotation;
    vout_thread_t *p_vout;

    void (*pf_filter)( vout_thread_t *, const picture_t *, picture_t * );
};

/*****************************************************************************
 * Control: control facility for the vout (forwards to child vout)
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    return vout_vaControl( p_vout->p_sys->p_vout, i_query, args );
}

/*****************************************************************************
 * Create: allocates Transform video thread output method
 *****************************************************************************
 * This function allocates and initializes a Transform vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    char *psz_method;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return VLC_ENOMEM;

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;
    p_vout->pf_control = Control;

    config_ChainParse( p_vout, CFG_PREFIX, ppsz_filter_options,
                           p_vout->p_cfg );

    /* Look what method was requested */
    psz_method = var_CreateGetNonEmptyStringCommand( p_vout, "transform-type" );

    switch( p_vout->fmt_in.i_chroma )
    {
        CASE_PLANAR_YUV_SQUARE
        case VLC_FOURCC('G','R','E','Y'):
            p_vout->p_sys->pf_filter = FilterPlanar;
            break;

        case VLC_FOURCC('I','4','2','2'):
        case VLC_FOURCC('J','4','2','2'):
            p_vout->p_sys->pf_filter = FilterI422;
            break;

        CASE_PACKED_YUV_422
            p_vout->p_sys->pf_filter = FilterYUYV;
            break;

        default:
            msg_Err( p_vout, "Unsupported chroma" );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
    }

    if( psz_method == NULL )
    {
        msg_Err( p_vout, "configuration variable %s empty", "transform-type" );
        msg_Err( p_vout, "no valid transform mode provided, using '90'" );
        p_vout->p_sys->i_mode = TRANSFORM_MODE_90;
        p_vout->p_sys->b_rotation = 1;
    }
    else
    {
        if( !strcmp( psz_method, "hflip" ) )
        {
            p_vout->p_sys->i_mode = TRANSFORM_MODE_HFLIP;
            p_vout->p_sys->b_rotation = 0;
        }
        else if( !strcmp( psz_method, "vflip" ) )
        {
            p_vout->p_sys->i_mode = TRANSFORM_MODE_VFLIP;
            p_vout->p_sys->b_rotation = 0;
        }
        else if( !strcmp( psz_method, "90" ) )
        {
            p_vout->p_sys->i_mode = TRANSFORM_MODE_90;
            p_vout->p_sys->b_rotation = 1;
        }
        else if( !strcmp( psz_method, "180" ) )
        {
            p_vout->p_sys->i_mode = TRANSFORM_MODE_180;
            p_vout->p_sys->b_rotation = 0;
        }
        else if( !strcmp( psz_method, "270" ) )
        {
            p_vout->p_sys->i_mode = TRANSFORM_MODE_270;
            p_vout->p_sys->b_rotation = 1;
        }
        else
        {
            msg_Err( p_vout, "no valid transform mode provided, using '90'" );
            p_vout->p_sys->i_mode = TRANSFORM_MODE_90;
            p_vout->p_sys->b_rotation = 1;
        }

        free( psz_method );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize Transform video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    video_format_t fmt;

    I_OUTPUTPICTURES = 0;
    memset( &fmt, 0, sizeof(video_format_t) );

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;
    p_vout->fmt_out = p_vout->fmt_in;
    fmt = p_vout->fmt_out;

    /* Try to open the real video output */
    msg_Dbg( p_vout, "spawning the real video output" );

    if( p_vout->p_sys->b_rotation )
    {
        fmt.i_width = p_vout->fmt_out.i_height;
        fmt.i_visible_width = p_vout->fmt_out.i_visible_height;
        fmt.i_x_offset = p_vout->fmt_out.i_y_offset;

        fmt.i_height = p_vout->fmt_out.i_width;
        fmt.i_visible_height = p_vout->fmt_out.i_visible_width;
        fmt.i_y_offset = p_vout->fmt_out.i_x_offset;

        fmt.i_aspect = VOUT_ASPECT_FACTOR *
            (uint64_t)VOUT_ASPECT_FACTOR / fmt.i_aspect;

        fmt.i_sar_num = p_vout->fmt_out.i_sar_den;
        fmt.i_sar_den = p_vout->fmt_out.i_sar_num;
    }

    p_vout->p_sys->p_vout = vout_Create( p_vout, &fmt );

    /* Everything failed */
    if( p_vout->p_sys->p_vout == NULL )
    {
        msg_Err( p_vout, "cannot open vout, aborting" );
        return VLC_EGENERIC;
    }

    vout_filter_AllocateDirectBuffers( p_vout, VOUT_MAX_PICTURES );

    vout_filter_AddChild( p_vout, p_vout->p_sys->p_vout, MouseEvent );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate Transform video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    vout_filter_DelChild( p_vout, p_sys->p_vout, MouseEvent );
    vout_CloseAndRelease( p_sys->p_vout );

    vout_filter_ReleaseDirectBuffers( p_vout );
}

/*****************************************************************************
 * Destroy: destroy Transform video thread output method
 *****************************************************************************
 * Terminate an output method created by TransformCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    free( p_vout->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Transform image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic;

    /* This is a new frame. Get a structure from the video_output. */
    while( ( p_outpic = vout_CreatePicture( p_vout->p_sys->p_vout, 0, 0, 0 ) )
              == NULL )
    {
        if( !vlc_object_alive (p_vout) || p_vout->b_error )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }

    p_outpic->date = p_pic->date;
    vout_LinkPicture( p_vout->p_sys->p_vout, p_outpic );

    p_vout->p_sys->pf_filter( p_vout, p_pic, p_outpic );

    vout_UnlinkPicture( p_vout->p_sys->p_vout, p_outpic );

    vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
}

/**
 * Forward mouse event with proper conversion.
 */
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = p_data;
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    /* Translate the mouse coordinates
     * FIXME missing lock */
    if( !strcmp( psz_var, "mouse-x" ) )
    {
        switch( p_vout->p_sys->i_mode )
        {
        case TRANSFORM_MODE_270:
            newval.i_int = p_vout->p_sys->p_vout->output.i_width
                             - newval.i_int;
        case TRANSFORM_MODE_90:
            psz_var = "mouse-y";
            break;

        case TRANSFORM_MODE_180:
        case TRANSFORM_MODE_HFLIP:
            newval.i_int = p_vout->p_sys->p_vout->output.i_width
                             - newval.i_int;
            break;

        case TRANSFORM_MODE_VFLIP:
        default:
            break;
        }
    }
    else if( !strcmp( psz_var, "mouse-y" ) )
    {
        switch( p_vout->p_sys->i_mode )
        {
        case TRANSFORM_MODE_90:
            newval.i_int = p_vout->p_sys->p_vout->output.i_height
                             - newval.i_int;
        case TRANSFORM_MODE_270:
            psz_var = "mouse-x";
            break;

        case TRANSFORM_MODE_180:
        case TRANSFORM_MODE_VFLIP:
            newval.i_int = p_vout->p_sys->p_vout->output.i_height
                             - newval.i_int;
            break;

        case TRANSFORM_MODE_HFLIP:
        default:
            break;
        }
    }

    return var_Set( p_vout, psz_var, newval );
}

static void FilterPlanar( vout_thread_t *p_vout,
                          const picture_t *p_pic, picture_t *p_outpic )
{
    int i_index;
    switch( p_vout->p_sys->i_mode )
    {
        case TRANSFORM_MODE_90:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                int i_pitch = p_pic->p[i_index].i_pitch;

                uint8_t *p_in = p_pic->p[i_index].p_pixels;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;
                uint8_t *p_out_end = p_out +
                    p_outpic->p[i_index].i_visible_lines *
                    p_outpic->p[i_index].i_pitch;

                for( ; p_out < p_out_end ; )
                {
                    uint8_t *p_line_end;

                    p_out_end -= p_outpic->p[i_index].i_pitch
                                  - p_outpic->p[i_index].i_visible_pitch;
                    p_line_end = p_in + p_pic->p[i_index].i_visible_lines *
                        i_pitch;

                    for( ; p_in < p_line_end ; )
                    {
                        p_line_end -= i_pitch;
                        *(--p_out_end) = *p_line_end;
                    }

                    p_in++;
                }
            }
            break;

        case TRANSFORM_MODE_180:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                uint8_t *p_in = p_pic->p[i_index].p_pixels;
                uint8_t *p_in_end = p_in + p_pic->p[i_index].i_visible_lines
                                            * p_pic->p[i_index].i_pitch;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;

                for( ; p_in < p_in_end ; )
                {
                    uint8_t *p_line_start = p_in_end
                                             - p_pic->p[i_index].i_pitch;
                    p_in_end -= p_pic->p[i_index].i_pitch
                                 - p_pic->p[i_index].i_visible_pitch;

                    for( ; p_line_start < p_in_end ; )
                    {
                        *p_out++ = *(--p_in_end);
                    }

                    p_out += p_outpic->p[i_index].i_pitch
                              - p_outpic->p[i_index].i_visible_pitch;
                }
            }
            break;

        case TRANSFORM_MODE_270:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                int i_pitch = p_pic->p[i_index].i_pitch;

                uint8_t *p_in = p_pic->p[i_index].p_pixels;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;
                uint8_t *p_out_end = p_out +
                    p_outpic->p[i_index].i_visible_lines *
                    p_outpic->p[i_index].i_pitch;

                for( ; p_out < p_out_end ; )
                {
                    uint8_t *p_in_end;

                    p_in_end = p_in + p_pic->p[i_index].i_visible_lines *
                        i_pitch;

                    for( ; p_in < p_in_end ; )
                    {
                        p_in_end -= i_pitch;
                        *p_out++ = *p_in_end;
                    }

                    p_out += p_outpic->p[i_index].i_pitch
                              - p_outpic->p[i_index].i_visible_pitch;
                    p_in++;
                }
            }
            break;

        case TRANSFORM_MODE_HFLIP:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                uint8_t *p_in = p_pic->p[i_index].p_pixels;
                uint8_t *p_in_end = p_in + p_pic->p[i_index].i_visible_lines
                                            * p_pic->p[i_index].i_pitch;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;

                for( ; p_in < p_in_end ; )
                {
                    p_in_end -= p_pic->p[i_index].i_pitch;
                    vlc_memcpy( p_out, p_in_end,
                                p_pic->p[i_index].i_visible_pitch );
                    p_out += p_pic->p[i_index].i_pitch;
                }
            }
            break;

        case TRANSFORM_MODE_VFLIP:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                uint8_t *p_in = p_pic->p[i_index].p_pixels;
                uint8_t *p_in_end = p_in + p_pic->p[i_index].i_visible_lines
                                         * p_pic->p[i_index].i_pitch;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;

                for( ; p_in < p_in_end ; )
                {
                    uint8_t *p_line_end = p_in
                                        + p_pic->p[i_index].i_visible_pitch;

                    for( ; p_in < p_line_end ; )
                    {
                        *p_out++ = *(--p_line_end);
                    }

                    p_in += p_pic->p[i_index].i_pitch;
                }
            }
            break;

        default:
            break;
    }
}

static void FilterI422( vout_thread_t *p_vout,
                        const picture_t *p_pic, picture_t *p_outpic )
{
    int i_index;
    switch( p_vout->p_sys->i_mode )
    {
        case TRANSFORM_MODE_180:
        case TRANSFORM_MODE_HFLIP:
        case TRANSFORM_MODE_VFLIP:
            /* Fall back on the default implementation */
            FilterPlanar( p_vout, p_pic, p_outpic );
            return;

        case TRANSFORM_MODE_90:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                int i_pitch = p_pic->p[i_index].i_pitch;

                uint8_t *p_in = p_pic->p[i_index].p_pixels;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;
                uint8_t *p_out_end = p_out +
                    p_outpic->p[i_index].i_visible_lines *
                    p_outpic->p[i_index].i_pitch;

                if( i_index == 0 )
                {
                    for( ; p_out < p_out_end ; )
                    {
                        uint8_t *p_line_end;

                        p_out_end -= p_outpic->p[i_index].i_pitch
                                      - p_outpic->p[i_index].i_visible_pitch;
                        p_line_end = p_in + p_pic->p[i_index].i_visible_lines *
                            i_pitch;

                        for( ; p_in < p_line_end ; )
                        {
                            p_line_end -= i_pitch;
                            *(--p_out_end) = *p_line_end;
                        }

                        p_in++;
                    }
                }
                else /* i_index == 1 or 2 */
                {
                    for( ; p_out < p_out_end ; )
                    {
                        uint8_t *p_line_end, *p_out_end2;

                        p_out_end -= p_outpic->p[i_index].i_pitch
                                      - p_outpic->p[i_index].i_visible_pitch;
                        p_out_end2 = p_out_end - p_outpic->p[i_index].i_pitch;
                        p_line_end = p_in + p_pic->p[i_index].i_visible_lines *
                            i_pitch;

                        for( ; p_in < p_line_end ; )
                        {
                            uint8_t p1, p2;

                            p_line_end -= i_pitch;
                            p1 = *p_line_end;
                            p_line_end -= i_pitch;
                            p2 = *p_line_end;

                            /* Trick for (x+y)/2 without overflow, based on
                             *   x + y == (x ^ y) + 2 * (x & y) */
                            *(--p_out_end) = (p1 & p2) + ((p1 ^ p2) / 2);
                            *(--p_out_end2) = (p1 & p2) + ((p1 ^ p2) / 2);
                        }

                        p_out_end = p_out_end2;
                        p_in++;
                    }
                }
            }
            break;

        case TRANSFORM_MODE_270:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                int i_pitch = p_pic->p[i_index].i_pitch;

                uint8_t *p_in = p_pic->p[i_index].p_pixels;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;
                uint8_t *p_out_end = p_out +
                    p_outpic->p[i_index].i_visible_lines *
                    p_outpic->p[i_index].i_pitch;

                if( i_index == 0 )
                {
                    for( ; p_out < p_out_end ; )
                    {
                        uint8_t *p_in_end;

                        p_in_end = p_in + p_pic->p[i_index].i_visible_lines *
                            i_pitch;

                        for( ; p_in < p_in_end ; )
                        {
                            p_in_end -= i_pitch;
                            *p_out++ = *p_in_end;
                        }

                        p_out += p_outpic->p[i_index].i_pitch
                                  - p_outpic->p[i_index].i_visible_pitch;
                        p_in++;
                    }
                }
                else /* i_index == 1 or 2 */
                {
                    for( ; p_out < p_out_end ; )
                    {
                        uint8_t *p_in_end, *p_out2;

                        p_in_end = p_in + p_pic->p[i_index].i_visible_lines *
                            i_pitch;
                        p_out2 = p_out + p_outpic->p[i_index].i_pitch;

                        for( ; p_in < p_in_end ; )
                        {
                            uint8_t p1, p2;

                            p_in_end -= i_pitch;
                            p1 = *p_in_end;
                            p_in_end -= i_pitch;
                            p2 = *p_in_end;

                            /* Trick for (x+y)/2 without overflow, based on
                             *   x + y == (x ^ y) + 2 * (x & y) */
                            *p_out++ = (p1 & p2) + ((p1 ^ p2) / 2);
                            *p_out2++ = (p1 & p2) + ((p1 ^ p2) / 2);
                        }

                        p_out2 += p_outpic->p[i_index].i_pitch
                                   - p_outpic->p[i_index].i_visible_pitch;
                        p_out = p_out2;
                        p_in++;
                    }
                }
            }
            break;

        default:
            break;
    }
}

static void FilterYUYV( vout_thread_t *p_vout,
                        const picture_t *p_pic, picture_t *p_outpic )
{
    int i_index;
    int i_y_offset, i_u_offset, i_v_offset;
    if( GetPackedYuvOffsets( p_pic->format.i_chroma, &i_y_offset,
                             &i_u_offset, &i_v_offset ) != VLC_SUCCESS )
        return;

    switch( p_vout->p_sys->i_mode )
    {
        case TRANSFORM_MODE_HFLIP:
            /* Fall back on the default implementation */
            FilterPlanar( p_vout, p_pic, p_outpic );
            return;

        case TRANSFORM_MODE_90:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                int i_pitch = p_pic->p[i_index].i_pitch;

                uint8_t *p_in = p_pic->p[i_index].p_pixels;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;
                uint8_t *p_out_end = p_out +
                    p_outpic->p[i_index].i_visible_lines *
                    p_outpic->p[i_index].i_pitch;

                int i_offset  = i_u_offset;
                int i_offset2 = i_v_offset;
                for( ; p_out < p_out_end ; )
                {
                    uint8_t *p_line_end;

                    p_out_end -= p_outpic->p[i_index].i_pitch
                                  - p_outpic->p[i_index].i_visible_pitch;
                    p_line_end = p_in + p_pic->p[i_index].i_visible_lines *
                        i_pitch;

                    for( ; p_in < p_line_end ; )
                    {
                        p_line_end -= i_pitch;
                        p_out_end -= 4;
                        p_out_end[i_y_offset+2] = p_line_end[i_y_offset];
                        p_out_end[i_u_offset] = p_line_end[i_offset];
                        p_line_end -= i_pitch;
                        p_out_end[i_y_offset] = p_line_end[i_y_offset];
                        p_out_end[i_v_offset] = p_line_end[i_offset2];
                    }

                    p_in += 2;

                    {
                        int a = i_offset;
                        i_offset = i_offset2;
                        i_offset2 = a;
                    }
                }
            }
            break;

        case TRANSFORM_MODE_180:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                uint8_t *p_in = p_pic->p[i_index].p_pixels;
                uint8_t *p_in_end = p_in + p_pic->p[i_index].i_visible_lines
                                            * p_pic->p[i_index].i_pitch;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;

                for( ; p_in < p_in_end ; )
                {
                    uint8_t *p_line_start = p_in_end
                                             - p_pic->p[i_index].i_pitch;
                    p_in_end -= p_pic->p[i_index].i_pitch
                                 - p_pic->p[i_index].i_visible_pitch;

                    for( ; p_line_start < p_in_end ; )
                    {
                        p_in_end -= 4;
                        p_out[i_y_offset] = p_in_end[i_y_offset+2];
                        p_out[i_u_offset] = p_in_end[i_u_offset];
                        p_out[i_y_offset+2] = p_in_end[i_y_offset];
                        p_out[i_v_offset] = p_in_end[i_v_offset];
                        p_out += 4;
                    }

                    p_out += p_outpic->p[i_index].i_pitch
                              - p_outpic->p[i_index].i_visible_pitch;
                }
            }
            break;

        case TRANSFORM_MODE_270:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                int i_pitch = p_pic->p[i_index].i_pitch;

                uint8_t *p_in = p_pic->p[i_index].p_pixels;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;
                uint8_t *p_out_end = p_out +
                    p_outpic->p[i_index].i_visible_lines *
                    p_outpic->p[i_index].i_pitch;

                int i_offset  = i_u_offset;
                int i_offset2 = i_v_offset;
                for( ; p_out < p_out_end ; )
                {
                    uint8_t *p_in_end;

                    p_in_end = p_in
                             + p_pic->p[i_index].i_visible_lines * i_pitch;

                    for( ; p_in < p_in_end ; )
                    {
                        p_in_end -= i_pitch;
                        p_out[i_y_offset] = p_in_end[i_y_offset];
                        p_out[i_u_offset] = p_in_end[i_offset];
                        p_in_end -= i_pitch;
                        p_out[i_y_offset+2] = p_in_end[i_y_offset];
                        p_out[i_v_offset] = p_in_end[i_offset2];
                        p_out += 4;
                    }

                    p_out += p_outpic->p[i_index].i_pitch
                           - p_outpic->p[i_index].i_visible_pitch;
                    p_in += 2;

                    {
                        int a = i_offset;
                        i_offset = i_offset2;
                        i_offset2 = a;
                    }
                }
            }
            break;

        case TRANSFORM_MODE_VFLIP:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                uint8_t *p_in = p_pic->p[i_index].p_pixels;
                uint8_t *p_in_end = p_in + p_pic->p[i_index].i_visible_lines
                                         * p_pic->p[i_index].i_pitch;

                uint8_t *p_out = p_outpic->p[i_index].p_pixels;

                for( ; p_in < p_in_end ; )
                {
                    uint8_t *p_line_end = p_in
                                        + p_pic->p[i_index].i_visible_pitch;

                    for( ; p_in < p_line_end ; )
                    {
                        p_line_end -= 4;
                        p_out[i_y_offset] = p_line_end[i_y_offset+2];
                        p_out[i_u_offset] = p_line_end[i_u_offset];
                        p_out[i_y_offset+2] = p_line_end[i_y_offset];
                        p_out[i_v_offset] = p_line_end[i_v_offset];
                        p_out += 4;
                    }

                    p_in += p_pic->p[i_index].i_pitch;
                }
            }
            break;

        default:
            break;
    }
}
