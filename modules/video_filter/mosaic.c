/*****************************************************************************
* mosaic.c : Mosaic video plugin for vlc
*****************************************************************************
* Copyright (C) 2004-2005 VideoLAN
* $Id: $
*
* Authors: Antoine Cellerier <dionoea@via.ecp.fr>
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
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
*****************************************************************************/

/*****************************************************************************
* Preamble
*****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>
#include <math.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "vlc_filter.h"
#include "vlc_input.h"

#include "vlc_image.h"

/*****************************************************************************
* Local prototypes
*****************************************************************************/

static int  CreateFilter    ( vlc_object_t * );
static void DestroyFilter   ( vlc_object_t * );

static subpicture_t *Filter( filter_t *, mtime_t );

/*****************************************************************************
* filter_sys_t : filter desriptor
*****************************************************************************/

#include "../video_output/picture.h"

struct filter_sys_t
{

    image_handler_t *p_image;
    image_handler_t *p_image2;
    picture_t *p_pic;

    int i_pos; /* mosaic positioning method */
    int i_width, i_height; /* mosaic height and width */
    int i_cols, i_rows; /* mosaic rows and cols */
    int i_xoffset, i_yoffset; /* top left corner offset */
    int i_vborder, i_hborder; /* border width/height between miniatures */
    int i_alpha; /* subfilter alpha blending */

};

/*****************************************************************************
* Module descriptor
*****************************************************************************/

#define ALPHA_TEXT N_("Mosaic alpha blending (0 -> 255)")
#define ALPHA_LONGTEXT N_("Mosaic alpha blending (0 -> 255). default is 255")

#define HEIGHT_TEXT N_("Mosaic height in pixels")
#define WIDTH_TEXT N_("Mosaic width in pixels")
#define XOFFSET_TEXT N_("Mosaic top left corner x coordinate")
#define YOFFSET_TEXT N_("Mosaic top left corner y coordinate")
#define VBORDER_TEXT N_("Mosaic vertical border width in pixels")
#define HBORDER_TEXT N_("Mosaic horizontal border width in pixels")

#define POS_TEXT N_("Mosaic positioning method")
#define ROWS_TEXT N_("Mosaic number of rows")
#define COLS_TEXT N_("Mosaic number of columns")

static int pi_pos_values[] = { 0, 1 };
static char * ppsz_pos_descriptions[] =
{ N_("auto"), N_("fixed") };


vlc_module_begin();
    set_description( _("Mosaic video sub filter") );
    set_shortname( N_("Mosaic") );
    set_capability( "sub filter", 0 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_SUBPIC );
    set_callbacks( CreateFilter, DestroyFilter );

    add_integer( "mosaic-alpha", 255, NULL, ALPHA_TEXT, ALPHA_LONGTEXT, VLC_FALSE );
    add_integer( "mosaic-height", 100, NULL, HEIGHT_TEXT, HEIGHT_TEXT, VLC_FALSE );
    add_integer( "mosaic-width", 100, NULL, WIDTH_TEXT, WIDTH_TEXT, VLC_FALSE );
    add_integer( "mosaic-xoffset", 0, NULL, XOFFSET_TEXT, XOFFSET_TEXT, VLC_FALSE );
    add_integer( "mosaic-yoffset", 0, NULL, YOFFSET_TEXT, YOFFSET_TEXT, VLC_FALSE );
    add_integer( "mosaic-vborder", 0, NULL, VBORDER_TEXT, VBORDER_TEXT, VLC_FALSE );
    add_integer( "mosaic-hborder", 0, NULL, HBORDER_TEXT, HBORDER_TEXT, VLC_FALSE );

    add_integer( "mosaic-position", 0, NULL, POS_TEXT, POS_TEXT, VLC_TRUE );
        change_integer_list( pi_pos_values, ppsz_pos_descriptions, 0 );
    add_integer( "mosaic-rows", 2, NULL, ROWS_TEXT, ROWS_TEXT, VLC_FALSE );
    add_integer( "mosaic-cols", 2, NULL, COLS_TEXT, COLS_TEXT, VLC_FALSE );
vlc_module_end();


/*****************************************************************************
* CreateFiler: allocates mosaic video filter
*****************************************************************************/

static int CreateFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }
    p_sys->p_image = image_HandlerCreate( p_filter );
    p_sys->p_image2 = image_HandlerCreate( p_filter );

    p_filter->pf_sub_filter = Filter;
    p_sys->p_pic = NULL;

    p_sys->i_width = __MAX( 0, config_GetInt( p_filter, "mosaic-width" ) );
    p_sys->i_height = __MAX( 0, config_GetInt( p_filter, "mosaic-height" ) );

    p_sys->i_xoffset = __MAX( 0, config_GetInt( p_filter, "mosaic-xoffset" ) );
    p_sys->i_yoffset = __MAX( 0, config_GetInt( p_filter, "mosaic-yoffset" ) );

    p_sys->i_vborder = __MAX( 0, config_GetInt( p_filter, "mosaic-vborder" ) );
    p_sys->i_hborder = __MAX( 0, config_GetInt( p_filter, "mosaic-hborder" ) );

    p_sys->i_rows = __MAX( 1, config_GetInt( p_filter, "mosaic-rows") );
    p_sys->i_cols = __MAX( 1, config_GetInt( p_filter, "mosaic-cols") );

    p_sys->i_alpha = config_GetInt( p_filter, "mosaic-alpha" );
    p_sys->i_alpha = __MIN( 255, __MAX( 0, p_sys->i_alpha ) );

    p_sys->i_pos = config_GetInt( p_filter, "mosaic-position" );
    if( p_sys->i_pos > 1 || p_sys->i_pos < 0 ) p_sys->i_pos = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
* DestroyFilter: destroy mosaic video filter
*****************************************************************************/

static void DestroyFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    image_HandlerDelete( p_sys->p_image );
    image_HandlerDelete( p_sys->p_image2 );

    if( p_sys->p_pic ) p_sys->p_pic->pf_release( p_sys->p_pic );
    free( p_sys );
}

/*****************************************************************************
* Filter
*****************************************************************************/

static subpicture_t *Filter( filter_t *p_filter, mtime_t date )
{

    filter_sys_t *p_sys = p_filter->p_sys;
    subpicture_t *p_spu;

    libvlc_t *p_libvlc = p_filter->p_libvlc;
    vlc_value_t val;
    int i_index;

    subpicture_region_t *p_region;
    subpicture_region_t *p_region_prev = NULL;

    struct picture_vout_t *p_picture_vout;

    if( var_Get( p_libvlc, "p_picture_vout", &val ) )
    {
        return NULL;
    }

    /* Allocate the subpicture internal data. */
    p_spu = p_filter->pf_sub_buffer_new( p_filter );
    if( !p_spu )
    {
        return NULL;
    }

    p_picture_vout = val.p_address;

    vlc_mutex_lock( &p_picture_vout->lock );

    if( p_sys->i_pos == 0 ) /* use automatic positioning */
    {
        int i_numpics = 0;
        for( i_index = 0 ;
             i_index < p_picture_vout->i_picture_num ;
             i_index ++ )
        {
            if( p_picture_vout->p_pic[i_index].i_status
                           == PICTURE_VOUT_E_OCCUPIED ) {
                i_numpics ++;
            }
        }
        p_sys->i_rows = ((int)ceil(sqrt( (float)i_numpics )));
        p_sys->i_cols = ( i_numpics%p_sys->i_rows == 0 ?
                            i_numpics/p_sys->i_rows :
                            i_numpics/p_sys->i_rows + 1 );
    }

    for( i_index = 0 ; i_index < p_picture_vout->i_picture_num ; i_index ++ )
    {

        video_format_t fmt_in = {0}, fmt_middle = {0}, fmt_out = {0};

        picture_t *p_converted, *p_middle;

        if(  p_picture_vout->p_pic[i_index].p_picture == NULL )
        {
            break;
        }

        if(  p_picture_vout->p_pic[i_index].i_status
               == PICTURE_VOUT_E_AVAILABLE )
        {
            msg_Dbg( p_filter, "Picture Vout Element is empty");
            break;
        }


        /* Convert the images */
/*        fprintf (stderr, "Input image %ix%i %4.4s\n",
                  p_picture_vout->p_pic[i_index].p_picture->format.i_width,
                  p_picture_vout->p_pic[i_index].p_picture->format.i_height,
                  (char *)&p_picture_vout->p_pic[i_index].p_picture->format.i_chroma );*/

        fmt_in.i_chroma = p_picture_vout->p_pic[i_index].
                                                p_picture->format.i_chroma;
        fmt_in.i_height = p_picture_vout->p_pic[i_index].
                                                p_picture->format.i_height;
        fmt_in.i_width = p_picture_vout->p_pic[i_index].
                                                p_picture->format.i_width;


        fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');
        fmt_out.i_width = fmt_in.i_width *( p_sys->i_width / p_sys->i_cols ) / fmt_in.i_width;
        fmt_out.i_height = fmt_in.i_height*( p_sys->i_height / p_sys->i_rows ) / fmt_in.i_height;
        fmt_out.i_visible_width = fmt_out.i_width;
        fmt_out.i_visible_height = fmt_out.i_height;

        fmt_middle.i_chroma = fmt_in.i_chroma;
        fmt_middle.i_visible_width = fmt_middle.i_width = fmt_out.i_width;
        fmt_middle.i_visible_height = fmt_middle.i_height = fmt_out.i_height;

        p_middle = image_Convert( p_sys->p_image,
            p_picture_vout->p_pic[i_index].p_picture, &fmt_in, &fmt_middle );
        if( !p_middle )
        {
            vlc_mutex_unlock( &p_picture_vout->lock );
            return NULL;
        }

        p_converted = image_Convert( p_sys->p_image2,
                 p_middle, &fmt_middle, &fmt_out );
        if( !p_converted )
        {
            vlc_mutex_unlock( &p_picture_vout->lock );
            return NULL;
        }

/*        fprintf( stderr, "Converted %ix%i %4.4s\n", p_converted->format.i_width, p_converted->format.i_height, (char *)&p_converted->format.i_chroma);*/


        p_region = p_spu->pf_create_region( VLC_OBJECT(p_filter), &fmt_out);
        if( !p_region )
        {
            msg_Err( p_filter, "cannot allocate SPU region" );
            p_filter->pf_sub_buffer_del( p_filter, p_spu );
            vlc_mutex_unlock( &p_picture_vout->lock );
            return NULL;
        }

        p_region->i_x = p_sys->i_xoffset + ( i_index % p_sys->i_cols )
                            * ( p_sys->i_width / p_sys->i_cols + p_sys->i_vborder );
        p_region->i_y = p_sys->i_yoffset
                        + ( ( i_index / p_sys->i_cols ) % p_sys->i_rows )
                            * ( p_sys->i_height / p_sys->i_rows + p_sys->i_hborder );


        if( p_region_prev == NULL ){
            p_spu->p_region = p_region;
        } else {
            p_region_prev->p_next = p_region;
        }

        p_region_prev = p_region;

        vout_CopyPicture( p_filter, &p_region->picture, p_converted );

        p_middle->pf_release( p_middle );
        p_converted->pf_release( p_converted );
    }

    vlc_mutex_unlock( &p_picture_vout->lock );

    /* Initialize subpicture */
    p_spu->i_channel = 0;
    p_spu->i_start  = date;
    p_spu->i_stop = 0;
    p_spu->b_ephemer = VLC_TRUE;
    p_spu->i_alpha = p_sys->i_alpha;

    return p_spu;
}
