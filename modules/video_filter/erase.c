/*****************************************************************************
 * erase.c : logo erase video filter
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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

#include <math.h>                                            /* sin(), cos() */

#include <vlc/vlc.h>
#include <vlc_sout.h>
#include <vlc_vout.h>
#include "vlc_image.h"

#include "vlc_filter.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );
static void FilterErase( filter_t *, picture_t *, picture_t * );
static int EraseCallback( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define MASK_TEXT N_("Image mask")
#define MASK_LONGTEXT N_("Image mask. Pixels with an alpha value greater than 50% will be erased.")

#define POSX_TEXT N_("X coordinate")
#define POSX_LONGTEXT N_("X coordinate of the mask.")
#define POSY_TEXT N_("Y coordinate")
#define POSY_LONGTEXT N_("Y coordinate of the mask.")

#define CFG_PREFIX "erase-"

vlc_module_begin();
    set_description( _("Erase video filter") );
    set_shortname( _( "Erase" ));
    set_capability( "video filter2", 0 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );

    add_file( CFG_PREFIX "mask", NULL, NULL,
              MASK_TEXT, MASK_LONGTEXT, VLC_FALSE );
    add_integer( CFG_PREFIX "x", 0, NULL, POSX_TEXT, POSX_LONGTEXT, VLC_FALSE );
    add_integer( CFG_PREFIX "y", 0, NULL, POSY_TEXT, POSY_LONGTEXT, VLC_FALSE );

    add_shortcut( "erase" );
    set_callbacks( Create, Destroy );
vlc_module_end();

static const char *ppsz_filter_options[] = {
    "mask", "x", "y", NULL
};

/*****************************************************************************
 * filter_sys_t
 *****************************************************************************/
struct filter_sys_t
{
    int i_x;
    int i_y;
    picture_t *p_mask;
    vlc_mutex_t lock;
};

static void LoadMask( filter_t *p_filter, const char *psz_filename )
{
    image_handler_t *p_image;
    video_format_t fmt_in, fmt_out;
    memset( &fmt_in, 0, sizeof( video_format_t ) );
    memset( &fmt_out, 0, sizeof( video_format_t ) );
    fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');
    if( p_filter->p_sys->p_mask )
        p_filter->p_sys->p_mask->pf_release( p_filter->p_sys->p_mask );
    p_image = image_HandlerCreate( p_filter );
    p_filter->p_sys->p_mask =
        image_ReadUrl( p_image, psz_filename, &fmt_in, &fmt_out );
    image_HandlerDelete( p_image );
}

/*****************************************************************************
 * Create
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    char *psz_filename;

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }
    p_sys = p_filter->p_sys;

    p_filter->pf_video_filter = Filter;

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    psz_filename =
        var_CreateGetNonEmptyStringCommand( p_filter, CFG_PREFIX "mask" );

    if( !psz_filename )
    {
        msg_Err( p_filter, "Missing 'mask' option value." );
        return VLC_EGENERIC;
    }

    p_sys->p_mask = NULL;
    LoadMask( p_filter, psz_filename );
    free( psz_filename );
    p_sys->i_x = var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "x" );
    p_sys->i_y = var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "y" );

    var_AddCallback( p_filter, CFG_PREFIX "x", EraseCallback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "y", EraseCallback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "mask", EraseCallback, p_sys );

    vlc_mutex_init( p_filter, &p_sys->lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;
    if( p_sys->p_mask )
        p_sys->p_mask->pf_release( p_sys->p_mask );

    vlc_mutex_destroy( &p_sys->lock );

    free( p_filter->p_sys );
}

/*****************************************************************************
 * Filter
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;

    if( !p_pic ) return NULL;
    switch( p_pic->format.i_chroma )
    {
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','Y','U','V'):
        case VLC_FOURCC('J','4','2','0'):
        case VLC_FOURCC('Y','V','1','2'):
            break;
        default:
            msg_Warn( p_filter, "Unsupported input chroma (%4s)",
                      (char*)&(p_pic->format.i_chroma) );
            if( p_pic->pf_release )
                p_pic->pf_release( p_pic );
            return NULL;
    }

    p_outpic = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        if( p_pic->pf_release )
            p_pic->pf_release( p_pic );
        return NULL;
    }

    /* Here */
    FilterErase( p_filter, p_pic, p_outpic );

    p_outpic->date = p_pic->date;
    p_outpic->b_force = p_pic->b_force;
    p_outpic->i_nb_fields = p_pic->i_nb_fields;
    p_outpic->b_progressive = p_pic->b_progressive;
    p_outpic->b_top_field_first = p_pic->b_top_field_first;

    if( p_pic->pf_release )
        p_pic->pf_release( p_pic );

    return p_outpic;
}

/*****************************************************************************
 * FilterErase
 *****************************************************************************/
static void FilterErase( filter_t *p_filter, picture_t *p_inpic,
                                             picture_t *p_outpic )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    int i_plane;

    const int i_mask_pitch = p_sys->p_mask->A_PITCH;
    const int i_mask_visible_pitch = p_sys->p_mask->p[A_PLANE].i_visible_pitch;
    const int i_mask_visible_lines = p_sys->p_mask->p[A_PLANE].i_visible_lines;

    for( i_plane = 0; i_plane < p_inpic->i_planes; i_plane++ )
    {
        const int i_pitch = p_inpic->p[i_plane].i_pitch;
        const int i_visible_pitch = p_inpic->p[i_plane].i_visible_pitch;
        const int i_lines = p_inpic->p[i_plane].i_lines;
        const int i_visible_lines = p_inpic->p[i_plane].i_visible_lines;

        uint8_t *p_inpix = p_inpic->p[i_plane].p_pixels;
        uint8_t *p_outpix = p_outpic->p[i_plane].p_pixels;
        uint8_t *p_mask = p_sys->p_mask->A_PIXELS;

        int i_x = p_sys->i_x, i_y = p_sys->i_y;
        int x, y;
        int i_height = i_mask_visible_lines;
        int i_width = i_mask_visible_pitch;
        if( i_plane ) /* U_PLANE or V_PLANE */
        {
            i_width       /= 2;
            i_height      /= 2;
            i_x           /= 2;
            i_y           /= 2;
        }
        i_height = __MIN( i_visible_lines - i_y, i_height );
        i_width  = __MIN( i_visible_pitch - i_x, i_width  );

        p_filter->p_libvlc->pf_memcpy( p_outpix, p_inpix, i_pitch * i_lines );

        for( y = 0; y < i_height; y++, p_mask += i_mask_pitch )
        {
            uint8_t prev, next = 0;
            int prev_x = -1, next_x = -2;
            p_outpix = p_outpic->p[i_plane].p_pixels + (i_y+y)*i_pitch + i_x;
            if( i_x )
            {
                prev = *(p_outpix-1);
            }
            else if( y || i_y )
            {
                prev = *(p_outpix-i_pitch);
            }
            else
            {
                prev = 0xff;
            }
            for( x = 0; x < i_width; x++ )
            {
                if( p_mask[i_plane?2*x:x] > 127 )
                {
                    if( next_x <= prev_x )
                    {
                        int x0;
                        for( x0 = x; x0 < i_width; x0++ )
                        {
                            if( p_mask[i_plane?2*x0:x0] <= 127 )
                            {
                                next_x = x0;
                                next = p_outpix[x0];
                                break;
                            }
                        }
                        if( next_x <= prev_x )
                        {
                            if( x0 == x ) x0++;
                            if( x0 >= i_visible_pitch )
                            {
                                next_x = x0;
                                next = prev;
                            }
                            else
                            {
                                next_x = x0;
                                next = p_outpix[x0];
                            }
                        }
                        if( !( i_x || y || i_y ) )
                            prev = next;
                    }
                    /* interpolate new value */
                    p_outpix[x] = prev;// + (x-prev_x)*(next-prev)/(next_x-prev_x);
                }
                else
                {
                    prev = p_outpix[x];
                    prev_x = x;
                }
            }
        }

        /* Vertical bluring */
        p_mask = p_sys->p_mask->A_PIXELS;
        i_height = i_mask_visible_lines / (i_plane?2:1);
        i_height = __MIN( i_visible_lines - i_y - 2, i_height );
        for( y = __MAX(i_y-2,0); y < i_height;
             y++, p_mask += i_mask_pitch )
        {
            p_outpix = p_outpic->p[i_plane].p_pixels + (i_y+y)*i_pitch + i_x;
            for( x = 0; x < i_width; x++ )
            {
                if( p_mask[i_plane?2*x:x] > 127 )
                {
                    p_outpix[x] =
                        ( (p_outpix[x-2*i_pitch]<<1)       /* 2 */
                        + (p_outpix[x-i_pitch]<<2)         /* 4 */
                        + (p_outpix[x]<<2)                 /* 4 */
                        + (p_outpix[x+i_pitch]<<2)         /* 4 */
                        + (p_outpix[x+2*i_pitch]<<1) )>>4; /* 2 */
                }
            }
        }

    }
}

static int EraseCallback( vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_data;

    if( !strcmp( psz_var, CFG_PREFIX "x" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        p_sys->i_x = newval.i_int;
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( !strcmp( psz_var, CFG_PREFIX "y" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        p_sys->i_y = newval.i_int;
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( !strcmp( psz_var, CFG_PREFIX "mask" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        LoadMask( (filter_t*)p_this, newval.psz_string );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else
    {
        msg_Warn( p_this, "Unknown callback command." );
    }
    return VLC_SUCCESS;
}
