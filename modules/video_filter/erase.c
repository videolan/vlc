/*****************************************************************************
 * erase.c : logo erase video filter
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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
#include <vlc_image.h>

#include <vlc_filter.h>
#include <vlc_url.h>
#include "filter_picture.h"

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

#define ERASE_HELP N_("Remove zones of the video using a picture as mask")

#define CFG_PREFIX "erase-"

vlc_module_begin ()
    set_description( N_("Erase video filter") )
    set_shortname( N_( "Erase" ))
    set_capability( "video filter2", 0 )
    set_help(ERASE_HELP)
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_loadfile( CFG_PREFIX "mask", NULL,
                  MASK_TEXT, MASK_LONGTEXT, false )
    add_integer( CFG_PREFIX "x", 0, POSX_TEXT, POSX_LONGTEXT, false )
    add_integer( CFG_PREFIX "y", 0, POSY_TEXT, POSY_LONGTEXT, false )

    add_shortcut( "erase" )
    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
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
    picture_t *p_old_mask = p_filter->p_sys->p_mask;
    memset( &fmt_in, 0, sizeof( video_format_t ) );
    memset( &fmt_out, 0, sizeof( video_format_t ) );
    fmt_out.i_chroma = VLC_CODEC_YUVA;
    p_image = image_HandlerCreate( p_filter );
    char *psz_url = vlc_path2uri( psz_filename, NULL );
    p_filter->p_sys->p_mask =
        image_ReadUrl( p_image, psz_url, &fmt_in, &fmt_out );
    free( psz_url );
    if( p_filter->p_sys->p_mask )
    {
        if( p_old_mask )
            picture_Release( p_old_mask );
    }
    else if( p_old_mask )
    {
        p_filter->p_sys->p_mask = p_old_mask;
        msg_Err( p_filter, "Error while loading new mask. Keeping old mask." );
    }
    else
        msg_Err( p_filter, "Error while loading new mask. No mask available." );

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

    switch( p_filter->fmt_in.video.i_chroma )
    {
        case VLC_CODEC_I420:
        case VLC_CODEC_J420:
        case VLC_CODEC_YV12:

        case VLC_CODEC_I422:
        case VLC_CODEC_J422:
            break;

        default:
            msg_Err( p_filter, "Unsupported input chroma (%4.4s)",
                     (char*)&(p_filter->fmt_in.video.i_chroma) );
            return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;
    p_sys = p_filter->p_sys;

    p_filter->pf_video_filter = Filter;

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    psz_filename =
        var_CreateGetNonEmptyStringCommand( p_filter, CFG_PREFIX "mask" );

    if( !psz_filename )
    {
        msg_Err( p_filter, "Missing 'mask' option value." );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->p_mask = NULL;
    LoadMask( p_filter, psz_filename );
    free( psz_filename );

    p_sys->i_x = var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "x" );
    p_sys->i_y = var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "y" );

    vlc_mutex_init( &p_sys->lock );
    var_AddCallback( p_filter, CFG_PREFIX "x", EraseCallback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "y", EraseCallback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "mask", EraseCallback, p_sys );

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
        picture_Release( p_sys->p_mask );

    var_DelCallback( p_filter, CFG_PREFIX "x", EraseCallback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "y", EraseCallback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "mask", EraseCallback, p_sys );
    vlc_mutex_destroy( &p_sys->lock );

    free( p_filter->p_sys );
}

/*****************************************************************************
 * Filter
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    /* If the mask is empty: just copy the image */
    vlc_mutex_lock( &p_sys->lock );
    if( p_sys->p_mask )
        FilterErase( p_filter, p_pic, p_outpic );
    else
        picture_CopyPixels( p_outpic, p_pic );
    vlc_mutex_unlock( &p_sys->lock );

    return CopyInfoAndRelease( p_outpic, p_pic );
}

/*****************************************************************************
 * FilterErase
 *****************************************************************************/
static void FilterErase( filter_t *p_filter, picture_t *p_inpic,
                                             picture_t *p_outpic )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    const int i_mask_pitch = p_sys->p_mask->A_PITCH;
    const int i_mask_visible_pitch = p_sys->p_mask->p[A_PLANE].i_visible_pitch;
    const int i_mask_visible_lines = p_sys->p_mask->p[A_PLANE].i_visible_lines;

    for( int i_plane = 0; i_plane < p_inpic->i_planes; i_plane++ )
    {
        const int i_pitch = p_outpic->p[i_plane].i_pitch;
        const int i_2pitch = i_pitch<<1;
        const int i_visible_pitch = p_inpic->p[i_plane].i_visible_pitch;
        const int i_visible_lines = p_inpic->p[i_plane].i_visible_lines;

        uint8_t *p_mask = p_sys->p_mask->A_PIXELS;
        int i_x = p_sys->i_x, i_y = p_sys->i_y;

        int x, y;
        int i_height = i_mask_visible_lines;
        int i_width  = i_mask_visible_pitch;

        const bool b_line_factor = ( i_plane /* U_PLANE or V_PLANE */ &&
            !( p_inpic->format.i_chroma == VLC_CODEC_I422
            || p_inpic->format.i_chroma == VLC_CODEC_J422 ) );

        if( i_plane ) /* U_PLANE or V_PLANE */
        {
            i_width  >>= 1;
            i_x      >>= 1;
        }
        if( b_line_factor )
        {
            i_height >>= 1;
            i_y      >>= 1;
        }
        i_height = __MIN( i_visible_lines - i_y, i_height );
        i_width  = __MIN( i_visible_pitch - i_x, i_width  );

        /* Copy original pixel buffer */
        plane_CopyPixels( &p_outpic->p[i_plane], &p_inpic->p[i_plane] );

        /* Horizontal linear interpolation of masked areas */
        uint8_t *p_outpix = p_outpic->p[i_plane].p_pixels + i_y*i_pitch + i_x;
        for( y = 0; y < i_height;
             y++, p_mask += i_mask_pitch, p_outpix += i_pitch )
        {
            uint8_t prev, next = 0;
            int prev_x = -1, next_x = -2;
            int quot = 0;

            /* Find a suitable value for the previous color to use when
             * interpoling a masked pixel's value */
            if( i_x )
            {
                /* There are pixels before current position on the same line.
                 * Use those */
                prev = *(p_outpix-1);
            }
            else if( y || i_y )
            {
                /* This is the first pixel on a line but there other lines
                 * above us. Use the pixel right above */
                prev = *(p_outpix-i_pitch);
            }
            else
            {
                /* We're in the upper left corner. This sucks. We can't use
                 * any previous value, so we'll use a dummy one. In most
                 * cases this dummy value will be fixed later on in the
                 * algorithm */
                prev = 0xff;
            }

            for( x = 0; x < i_width; x++ )
            {
                if( p_mask[i_plane?x<<1:x] > 127 )
                {
                    /* This is a masked pixel */
                    if( next_x <= prev_x )
                    {
                        int x0;
                        /* Look for the next non masked pixel on the same
                         * line (inside the mask's bounding box) */
                        for( x0 = x; x0 < i_width; x0++ )
                        {
                            if( p_mask[i_plane?x0<<1:x0] <= 127 )
                            {
                                /* We found an unmasked pixel. Victory! */
                                next_x = x0;
                                next = p_outpix[x0];
                                break;
                            }
                        }
                        if( next_x <= prev_x )
                        {
                            /* We didn't find an unmasked pixel yet. Try
                             * harder */
                            if( x0 == x ) x0++;
                            if( x0 < i_visible_pitch )
                            {
                                /* If we didn't find a non masked pixel on the
                                 * same line inside the mask's bounding box,
                                 * use the next pixel on the line (except if
                                 * it doesn't exist) */
                                next_x = x0;
                                next = p_outpix[x0];
                            }
                            else
                            {
                                /* The last pixel on the line is masked,
                                 * so we'll use the "prev" value. A better
                                 * approach would be to use unmasked pixels
                                 * at the end of adjacent lines */
                                next_x = x0;
                                next = prev;
                            }
                        }
                        if( !( i_x || y || i_y ) )
                            /* We were unable to find a suitable value for
                             * the previous color (which means that we are
                             * on the first line in the upper left corner)
                             */
                            prev = next;

                        /* Divide only once instead of next_x-prev_x-1 times */
                        quot = ((next-prev)<<16)/(next_x-prev_x);
                    }
                    /* Interpolate new value, and round correctly */
                    p_outpix[x] = prev + (((x-prev_x)*quot+(1<<16))>>16);
                }
                else
                {
                    /* This pixel isn't masked. It's thus suitable as a
                     * previous color for the next interpolation */
                    prev = p_outpix[x];
                    prev_x = x;
                }
            }
        }

        /* Vertical bluring */
        p_mask = p_sys->p_mask->A_PIXELS;
        i_height = b_line_factor ? i_mask_visible_lines>>1
                                 : i_mask_visible_lines;
        /* Make sure that we stop at least 2 lines before the picture's end
         * (since our bluring algorithm uses the 2 next lines) */
        i_height = __MIN( i_visible_lines - i_y - 2, i_height );
        /* Make sure that we start at least 2 lines from the top (since our
         * bluring algorithm uses the 2 previous lines) */
        y = __MAX(i_y,2);
        p_outpix = p_outpic->p[i_plane].p_pixels + (i_y+y)*i_pitch + i_x;
        for( ; y < i_height; y++, p_mask += i_mask_pitch, p_outpix += i_pitch )
        {
            for( x = 0; x < i_width; x++ )
            {
                if( p_mask[i_plane?x<<1:x] > 127 )
                {
                    /* Ugly bluring function */
                    p_outpix[x] =
                        ( (p_outpix[x-i_2pitch]<<1)       /* 2 */
                        + (p_outpix[x-i_pitch ]<<2)       /* 4 */
                        + (p_outpix[x         ]<<2)       /* 4 */
                        + (p_outpix[x+i_pitch ]<<2)       /* 4 */
                        + (p_outpix[x+i_2pitch]<<1) )>>4; /* 2 */
                }
            }
        }
    }
}

static int EraseCallback( vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(oldval);
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
