/*****************************************************************************
 * mosaic.c : Mosaic video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
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
#include <math.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#ifdef HAVE_LIMITS_H
#   include <limits.h> /* INT_MAX */
#endif

#include "vlc_filter.h"
#include "vlc_image.h"

#include "mosaic.h"

#define BLANK_DELAY I64C(1000000)

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  CreateFilter    ( vlc_object_t * );
static void DestroyFilter   ( vlc_object_t * );

static subpicture_t *Filter( filter_t *, mtime_t );

static int MosaicCallback( vlc_object_t *, char const *, vlc_value_t,
                           vlc_value_t, void * );

/*****************************************************************************
 * filter_sys_t : filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    vlc_mutex_t lock;
    vlc_mutex_t *p_lock;

    image_handler_t *p_image;
    picture_t *p_pic;

    int i_position; /* mosaic positioning method */
    vlc_bool_t b_ar; /* do we keep the aspect ratio ? */
    vlc_bool_t b_keep; /* do we keep the original picture format ? */
    int i_width, i_height; /* mosaic height and width */
    int i_cols, i_rows; /* mosaic rows and cols */
    int i_align; /* mosaic alignment in background video */
    int i_xoffset, i_yoffset; /* top left corner offset */
    int i_vborder, i_hborder; /* border width/height between miniatures */
    int i_alpha; /* subfilter alpha blending */

    vlc_bool_t b_bs; /* Bluescreen vars */
    int i_bsu, i_bsv, i_bsut, i_bsvt;

    char **ppsz_order; /* list of picture-id */
    int i_order_length;

    mtime_t i_delay;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ALPHA_TEXT N_("Transparency")
#define ALPHA_LONGTEXT N_("Transparency of the mosaic foreground pictures. " \
        "0 means transparent, 255 opaque (default)." )

#define HEIGHT_TEXT N_("Height")
#define HEIGHT_LONGTEXT N_( "Total height of the mosaic, in pixels." )
#define WIDTH_TEXT N_("Width")
#define WIDTH_LONGTEXT N_( "Total width of the mosaic, in pixels." )

#define XOFFSET_TEXT N_("Top left corner X coordinate")
#define XOFFSET_LONGTEXT N_("X Coordinate of the top-left corner of the mosaic.")
#define YOFFSET_TEXT N_("Top left corner Y coordinate")
#define YOFFSET_LONGTEXT N_("Y Coordinate of the top-left corner of the mosaic.")
#define VBORDER_TEXT N_("Vertical border width")
#define VBORDER_LONGTEXT N_( "Width in pixels of the border than can be "\
    "drawn vertically around the mosaic." )
#define HBORDER_TEXT N_("Horizontal border width")
#define HBORDER_LONGTEXT N_( "Width in pixels of the border than can "\
    "be drawn horizontally around the mosaic." )

#define ALIGN_TEXT N_("Mosaic alignment" )
#define ALIGN_LONGTEXT N_( \
  "You can enforce the mosaic alignment on the video " \
  "(0=center, 1=left, 2=right, 4=top, 8=bottom, you can " \
  "also use combinations of these values, eg 6 = top-right).")

#define POS_TEXT N_("Positioning method")
#define POS_LONGTEXT N_("Positioning method for the mosaic. auto: " \
        "automatically choose the best number of rows and columns. " \
        "fixed: use the user-defined number of rows and columns.")

/// \bug [String] missing closing parenthesis
#define ROWS_TEXT N_("Number of rows")
#define ROWS_LONGTEXT N_("Number of image rows in the mosaic (only used if "\
        "positionning method is set to \"fixed\"." )
#define COLS_TEXT N_("Number of columns")
#define COLS_LONGTEXT N_("Number of image columns in the mosaic (only used if "\
        "positionning method is set to \"fixed\"." )

#define AR_TEXT N_("Keep aspect ratio")
#define AR_LONGTEXT N_("Keep the original aspect ratio when resizing " \
        "mosaic elements." )
#define KEEP_TEXT N_("Keep original size")
#define KEEP_LONGTEXT N_("Keep the original size of mosaic elements." )

#define ORDER_TEXT N_("Elements order" )
#define ORDER_LONGTEXT N_( "You can enforce the order of the elements on " \
        "the mosaic. You must give a comma-separated list of picture ID(s)." \
        "These IDs are assigned in the \"mosaic-bridge\" module." )

#define DELAY_TEXT N_("Delay")
#define DELAY_LONGTEXT N_("Pictures coming from the mosaic elements " \
        "will be delayed according to this value (in milliseconds). For high " \
        "values you will need to raise caching at input.")

#define BLUESCREEN_TEXT N_("Bluescreen" )
#define BLUESCREEN_LONGTEXT N_( "This effect, also known as \"greenscreen\" "\
   "or \"chroma key\" blends the \"blue parts\" of the foreground images of " \
   "the mosaic on the background (like wheather forecast presenters). You " \
   "can choose the \"key\" color for blending (blue by default)." )

#define BLUESCREENU_TEXT N_("Bluescreen U value")
#define BLUESCREENU_LONGTEXT N_("\"U\" value for the bluescreen key color " \
        "(in YUV values). From 0 to 255. Defaults to 120 for blue." )
#define BLUESCREENV_TEXT N_("Bluescreen V value")
#define BLUESCREENV_LONGTEXT N_("\"V\" value for the bluescreen key color " \
        "(in YUV values). From 0 to 255. Defaults to 90 for blue." )
#define BLUESCREENUTOL_TEXT N_("Bluescreen U tolerance")
#define BLUESCREENUTOL_LONGTEXT N_("Tolerance of the bluescreen blender " \
        "on color variations for the U plane. A value between 10 and 20 " \
        "seems sensible." )
#define BLUESCREENVTOL_TEXT N_("Bluescreen V tolerance")
#define BLUESCREENVTOL_LONGTEXT N_("Tolerance of the bluescreen blender " \
        "on color variations for the V plane. A value between 10 and 20 " \
        "seems sensible." )

static int pi_pos_values[] = { 0, 1 };
static char * ppsz_pos_descriptions[] =
{ N_("auto"), N_("fixed") };

static int pi_align_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static char *ppsz_align_descriptions[] =
     { N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
     N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };


vlc_module_begin();
    set_description( N_("Mosaic video sub filter") );
    set_shortname( N_("Mosaic") );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_SUBPIC);
    set_capability( "sub filter", 0 );
    set_callbacks( CreateFilter, DestroyFilter );

    add_integer( "mosaic-alpha", 255, NULL, ALPHA_TEXT, ALPHA_LONGTEXT, VLC_FALSE );
    add_integer( "mosaic-height", 100, NULL, HEIGHT_TEXT, HEIGHT_LONGTEXT, VLC_FALSE );
    add_integer( "mosaic-width", 100, NULL, WIDTH_TEXT, WIDTH_LONGTEXT, VLC_FALSE );
    add_integer( "mosaic-align", 5, NULL, ALIGN_TEXT, ALIGN_LONGTEXT, VLC_TRUE);
        change_integer_list( pi_align_values, ppsz_align_descriptions, 0 );
    add_integer( "mosaic-xoffset", 0, NULL, XOFFSET_TEXT, XOFFSET_LONGTEXT, VLC_TRUE );
    add_integer( "mosaic-yoffset", 0, NULL, YOFFSET_TEXT, YOFFSET_LONGTEXT, VLC_TRUE );
    add_integer( "mosaic-vborder", 0, NULL, VBORDER_TEXT, VBORDER_LONGTEXT, VLC_TRUE );
    add_integer( "mosaic-hborder", 0, NULL, HBORDER_TEXT, HBORDER_LONGTEXT, VLC_TRUE );

    add_integer( "mosaic-position", 0, NULL, POS_TEXT, POS_LONGTEXT, VLC_FALSE );
        change_integer_list( pi_pos_values, ppsz_pos_descriptions, 0 );
    add_integer( "mosaic-rows", 2, NULL, ROWS_TEXT, ROWS_LONGTEXT, VLC_FALSE );
    add_integer( "mosaic-cols", 2, NULL, COLS_TEXT, COLS_LONGTEXT, VLC_FALSE );
    add_bool( "mosaic-keep-aspect-ratio", 0, NULL, AR_TEXT, AR_LONGTEXT, VLC_FALSE );
    add_bool( "mosaic-keep-picture", 0, NULL, KEEP_TEXT, KEEP_LONGTEXT, VLC_FALSE );
    add_string( "mosaic-order", "", NULL, ORDER_TEXT, ORDER_LONGTEXT, VLC_FALSE );

    add_integer( "mosaic-delay", 0, NULL, DELAY_TEXT, DELAY_LONGTEXT,
                 VLC_FALSE );

    add_bool( "mosaic-bs", 0, NULL, BLUESCREEN_TEXT,
              BLUESCREEN_LONGTEXT, VLC_FALSE );
    add_integer( "mosaic-bsu", 120, NULL, BLUESCREENU_TEXT,
                 BLUESCREENU_LONGTEXT, VLC_FALSE );
    add_integer( "mosaic-bsv", 90, NULL, BLUESCREENV_TEXT,
                 BLUESCREENV_LONGTEXT, VLC_FALSE );
    add_integer( "mosaic-bsut", 17, NULL, BLUESCREENUTOL_TEXT,
                 BLUESCREENUTOL_LONGTEXT, VLC_FALSE );
    add_integer( "mosaic-bsvt", 17, NULL, BLUESCREENVTOL_TEXT,
                 BLUESCREENVTOL_LONGTEXT, VLC_FALSE );

    var_Create( p_module->p_libvlc, "mosaic-lock", VLC_VAR_MUTEX );
vlc_module_end();


/*****************************************************************************
 * CreateFiler: allocate mosaic video filter
 *****************************************************************************/
static int CreateFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    libvlc_t *p_libvlc = p_filter->p_libvlc;
    char *psz_order;
    int i_index;
    vlc_value_t val;

    /* The mosaic thread is more important than the decoder threads */
    vlc_thread_set_priority( p_this, VLC_THREAD_PRIORITY_OUTPUT );

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

    p_filter->pf_sub_filter = Filter;
    p_sys->p_pic = NULL;

    vlc_mutex_init( p_filter, &p_sys->lock );
    vlc_mutex_lock( &p_sys->lock );

    var_Get( p_libvlc, "mosaic-lock", &val );
    p_sys->p_lock = val.p_address;

#define GET_VAR( name, min, max )                                           \
    p_sys->i_##name = __MIN( max, __MAX( min,                               \
                var_CreateGetInteger( p_filter, "mosaic-" #name ) ) );      \
    var_Destroy( p_filter, "mosaic-" #name );                               \
    var_Create( p_libvlc, "mosaic-" #name, VLC_VAR_INTEGER );               \
    var_SetInteger( p_libvlc, "mosaic-" #name, p_sys->i_##name );           \
    var_AddCallback( p_libvlc, "mosaic-" #name, MosaicCallback, p_sys );

    GET_VAR( width, 0, INT_MAX );
    GET_VAR( height, 0, INT_MAX );
    GET_VAR( xoffset, 0, INT_MAX );
    GET_VAR( yoffset, 0, INT_MAX );

    p_sys->i_align = __MIN( 10, __MAX( 0,  var_CreateGetInteger( p_filter, "mosaic-align" ) ) );
    if( p_sys->i_align == 3 || p_sys->i_align == 7 )
        p_sys->i_align = 5;
    var_Destroy( p_filter, "mosaic-align" );
    var_Create( p_libvlc, "mosaic-align", VLC_VAR_INTEGER );
    var_SetInteger( p_libvlc, "mosaic-align", p_sys->i_align );
    var_AddCallback( p_libvlc, "mosaic-align", MosaicCallback, p_sys );

    GET_VAR( vborder, 0, INT_MAX );
    GET_VAR( hborder, 0, INT_MAX );
    GET_VAR( rows, 1, INT_MAX );
    GET_VAR( cols, 1, INT_MAX );
    GET_VAR( alpha, 0, 255 );
    GET_VAR( position, 0, 1 );
    GET_VAR( delay, 100, INT_MAX );
    p_sys->i_delay *= 1000;

    p_sys->b_ar = var_CreateGetBool( p_filter, "mosaic-keep-aspect-ratio" );
    var_Destroy( p_filter, "mosaic-keep-aspect-ratio" );
    var_Create( p_libvlc, "mosaic-keep-aspect-ratio", VLC_VAR_INTEGER );
    var_SetBool( p_libvlc, "mosaic-keep-aspect-ratio", p_sys->b_ar );
    var_AddCallback( p_libvlc, "mosaic-keep-aspect-ratio", MosaicCallback,
                     p_sys );

    p_sys->b_keep = var_CreateGetBool( p_filter, "mosaic-keep-picture" );
    if ( !p_sys->b_keep )
    {
        p_sys->p_image = image_HandlerCreate( p_filter );
    }

    p_sys->i_order_length = 0;
    p_sys->ppsz_order = NULL;
    psz_order = var_CreateGetString( p_filter, "mosaic-order" );

    if( psz_order[0] != 0 )
    {
        char *psz_end = NULL;
        i_index = 0;
        do
        {
            psz_end = strchr( psz_order, ',' );
            i_index++;
            p_sys->ppsz_order = realloc( p_sys->ppsz_order,
                                         i_index * sizeof(char *) );
            p_sys->ppsz_order[i_index - 1] = strndup( psz_order,
                                           psz_end - psz_order );
            psz_order = psz_end+1;
        } while( NULL !=  psz_end );
        p_sys->i_order_length = i_index;
    }

    /* Bluescreen specific stuff */
    GET_VAR( bsu, 0x00, 0xff );
    GET_VAR( bsv, 0x00, 0xff );
    GET_VAR( bsut, 0x00, 0xff );
    GET_VAR( bsvt, 0x00, 0xff );
    p_sys->b_bs = var_CreateGetBool( p_filter, "mosaic-bs" );
    var_Destroy( p_filter, "mosaic-bs" );
    var_Create( p_libvlc, "mosaic-bs", VLC_VAR_INTEGER );
    var_SetBool( p_libvlc, "mosaic-bs", p_sys->b_bs );
    var_AddCallback( p_libvlc, "mosaic-bs", MosaicCallback, p_sys );
    if( p_sys->b_bs && p_sys->b_keep )
    {
        msg_Warn( p_filter, "mosaic-keep-picture needs to be disabled for"
                            " bluescreen to work" );
    }

    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DestroyFilter: destroy mosaic video filter
 *****************************************************************************/
static void DestroyFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;
    libvlc_t *p_libvlc = p_filter->p_libvlc;
    int i_index;

    vlc_mutex_lock( &p_sys->lock );

    if( !p_sys->b_keep )
    {
        image_HandlerDelete( p_sys->p_image );
    }

    if( p_sys->i_order_length )
    {
        for( i_index = 0; i_index < p_sys->i_order_length; i_index++ )
        {
            free( p_sys->ppsz_order[i_index] );
        }
        free( p_sys->ppsz_order );
    }

    var_Destroy( p_libvlc, "mosaic-alpha" );
    var_Destroy( p_libvlc, "mosaic-height" );
    var_Destroy( p_libvlc, "mosaic-align" );
    var_Destroy( p_libvlc, "mosaic-width" );
    var_Destroy( p_libvlc, "mosaic-xoffset" );
    var_Destroy( p_libvlc, "mosaic-yoffset" );
    var_Destroy( p_libvlc, "mosaic-vborder" );
    var_Destroy( p_libvlc, "mosaic-hborder" );
    var_Destroy( p_libvlc, "mosaic-position" );
    var_Destroy( p_libvlc, "mosaic-rows" );
    var_Destroy( p_libvlc, "mosaic-cols" );
    var_Destroy( p_libvlc, "mosaic-keep-aspect-ratio" );

    var_Destroy( p_libvlc, "mosaic-bsu" );
    var_Destroy( p_libvlc, "mosaic-bsv" );
    var_Destroy( p_libvlc, "mosaic-bsut" );
    var_Destroy( p_libvlc, "mosaic-bsvt" );
    var_Destroy( p_libvlc, "mosaic-bs" );

    if( p_sys->p_pic ) p_sys->p_pic->pf_release( p_sys->p_pic );
    vlc_mutex_unlock( &p_sys->lock );
    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys );
}

/*****************************************************************************
 * MosaicReleasePicture : Hack to avoid picture duplication
 *****************************************************************************/
static void MosaicReleasePicture( picture_t *p_picture )
{
    picture_t *p_original_pic = (picture_t *)p_picture->p_sys;

    p_original_pic->pf_release( p_original_pic );
}

/*****************************************************************************
 * Filter
 *****************************************************************************/
static subpicture_t *Filter( filter_t *p_filter, mtime_t date )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    bridge_t *p_bridge;

    subpicture_t *p_spu;

    int i_index, i_real_index, i_row, i_col;
    int i_greatest_real_index_used = p_sys->i_order_length - 1;

    unsigned int col_inner_width, row_inner_height;

    subpicture_region_t *p_region;
    subpicture_region_t *p_region_prev = NULL;

    /* Allocate the subpicture internal data. */
    p_spu = p_filter->pf_sub_buffer_new( p_filter );
    if( !p_spu )
    {
        return NULL;
    }

    /* Initialize subpicture */
    p_spu->i_channel = 0;
    p_spu->i_start  = date;
    p_spu->i_stop = 0;
    p_spu->b_ephemer = VLC_TRUE;
    p_spu->i_alpha = p_sys->i_alpha;
    p_spu->i_flags = p_sys->i_align;
    p_spu->b_absolute = VLC_FALSE;

    vlc_mutex_lock( &p_sys->lock );
    vlc_mutex_lock( p_sys->p_lock );

    p_bridge = GetBridge( p_filter );
    if ( p_bridge == NULL )
    {
        vlc_mutex_unlock( p_sys->p_lock );
        vlc_mutex_unlock( &p_sys->lock );
        return p_spu;
    }

    if ( p_sys->i_position == 0 ) /* use automatic positioning */
    {
        int i_numpics = p_sys->i_order_length; /* keep slots and all */
        for ( i_index = 0; i_index < p_bridge->i_es_num; i_index++ )
        {
            bridged_es_t *p_es = p_bridge->pp_es[i_index];
            if ( !p_es->b_empty )
            {
                i_numpics ++;
                if( p_sys->i_order_length && p_es->psz_id != 0 )
                {
                    /* We also want to leave slots for images given in
                     * mosaic-order that are not available in p_vout_picture */
                    int i;
                    for( i = 0; i < p_sys->i_order_length ; i++ )
                    {
                        if( !strcmp( p_sys->ppsz_order[i], p_es->psz_id ) )
                        {
                            i_numpics--;
                            break;
                        }
                    }

                }
            }
        }
        p_sys->i_rows = ((int)ceil(sqrt( (float)i_numpics )));
        p_sys->i_cols = ( i_numpics % p_sys->i_rows == 0 ?
                            i_numpics / p_sys->i_rows :
                            i_numpics / p_sys->i_rows + 1 );
    }

    col_inner_width  = ( ( p_sys->i_width - ( p_sys->i_cols - 1 )
                       * p_sys->i_vborder ) / p_sys->i_cols );
    row_inner_height = ( ( p_sys->i_height - ( p_sys->i_rows - 1 )
                       * p_sys->i_hborder ) / p_sys->i_rows );

    i_real_index = 0;

    for ( i_index = 0; i_index < p_bridge->i_es_num; i_index++ )
    {
        bridged_es_t *p_es = p_bridge->pp_es[i_index];
        video_format_t fmt_in = {0}, fmt_out = {0};
        picture_t *p_converted;

        if ( p_es->b_empty )
            continue;

        while ( p_es->p_picture != NULL
                 && p_es->p_picture->date + p_sys->i_delay < date )
        {
            if ( p_es->p_picture->p_next != NULL )
            {
                picture_t *p_next = p_es->p_picture->p_next;
                p_es->p_picture->pf_release( p_es->p_picture );
                p_es->p_picture = p_next;
            }
            else if ( p_es->p_picture->date + p_sys->i_delay + BLANK_DELAY <
                        date )
            {
                /* Display blank */
                p_es->p_picture->pf_release( p_es->p_picture );
                p_es->p_picture = NULL;
                p_es->pp_last = &p_es->p_picture;
                break;
            }
            else
            {
                msg_Dbg( p_filter, "too late picture for %s (" I64Fd ")",
                         p_es->psz_id,
                         date - p_es->p_picture->date - p_sys->i_delay );
                break;
            }
        }

        if ( p_es->p_picture == NULL )
            continue;

        if ( p_sys->i_order_length == 0 )
        {
            i_real_index++;
        }
        else
        {
            int i;
            for ( i = 0; i <= p_sys->i_order_length; i++ )
            {
                if ( i == p_sys->i_order_length ) break;
                if ( strcmp( p_es->psz_id, p_sys->ppsz_order[i] ) == 0 )
                {
                    i_real_index = i;
                    break;
                }
            }
            if ( i == p_sys->i_order_length )
                i_real_index = ++i_greatest_real_index_used;
        }
        i_row = ( i_real_index / p_sys->i_cols ) % p_sys->i_rows;
        i_col = i_real_index % p_sys->i_cols ;

        if ( !p_sys->b_keep )
        {
            /* Convert the images */
            fmt_in.i_chroma = p_es->p_picture->format.i_chroma;
            fmt_in.i_height = p_es->p_picture->format.i_height;
            fmt_in.i_width = p_es->p_picture->format.i_width;

            fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');
            fmt_out.i_width = col_inner_width;
            fmt_out.i_height = row_inner_height;

            if( p_sys->b_ar ) /* keep aspect ratio */
            {
                if( (float)fmt_out.i_width / (float)fmt_out.i_height
                      > (float)fmt_in.i_width / (float)fmt_in.i_height )
                {
                    fmt_out.i_width = ( fmt_out.i_height * fmt_in.i_width )
                                         / fmt_in.i_height;
                }
                else
                {
                    fmt_out.i_height = ( fmt_out.i_width * fmt_in.i_height )
                                        / fmt_in.i_width;
                }
             }

            fmt_out.i_visible_width = fmt_out.i_width;
            fmt_out.i_visible_height = fmt_out.i_height;

            p_converted = image_Convert( p_sys->p_image, p_es->p_picture,
                                         &fmt_in, &fmt_out );
            if( !p_converted )
            {
                msg_Warn( p_filter,
                           "image resizing and chroma conversion failed" );
                continue;
            }

            /* Bluescreen stuff */
            if( p_sys->b_bs )
            {
                int i,j;
                int i_lines = p_converted->p[ A_PLANE ].i_lines;
                int i_pitch = p_converted->p[ A_PLANE ].i_pitch;
                uint8_t *p_a = p_converted->p[ A_PLANE ].p_pixels;
                uint8_t *p_at = malloc( i_lines * i_pitch * sizeof( uint8_t ) );
                uint8_t *p_u = p_converted->p[ U_PLANE ].p_pixels;
                uint8_t *p_v = p_converted->p[ V_PLANE ].p_pixels;
                uint8_t umin, umax, vmin, vmax;
                umin = p_sys->i_bsu - p_sys->i_bsut >= 0x00 ?
                       p_sys->i_bsu - p_sys->i_bsut : 0x00;
                umax = p_sys->i_bsu + p_sys->i_bsut <= 0xff ?
                       p_sys->i_bsu + p_sys->i_bsut : 0xff;
                vmin = p_sys->i_bsv - p_sys->i_bsvt >= 0x00 ?
                       p_sys->i_bsv - p_sys->i_bsvt : 0x00;
                vmax = p_sys->i_bsv + p_sys->i_bsvt <= 0xff ?
                       p_sys->i_bsv + p_sys->i_bsvt : 0xff;

                for( i = 0; i < i_lines*i_pitch; i++ )
                {
                    if(    p_u[i] < umax
                        && p_u[i] > umin
                        && p_v[i] < vmax
                        && p_v[i] > vmin )
                    {
                        p_at[i] = 0x00;
                    }
                    else
                    {
                        p_at[i] = 0xff;
                    }
                }
                /* Gaussian convolution to make it look cleaner */
                memset( p_a, 0, 2 * i_pitch );
                for( i = 2; i < i_lines - 2; i++ )
                {
                    p_a[i*i_pitch] = 0x00;
                    p_a[i*i_pitch+1] = 0x00;
                    for( j = 2; j < i_pitch - 2; j ++ )
                    {
                        p_a[i*i_pitch+j] = (uint8_t)((
                          /* 2 rows up */
                            ( p_at[(i-2)*i_pitch+j-2]<<1 )
                          + ( p_at[(i-2)*i_pitch+j-1]<<2 )
                          + ( p_at[(i-2)*i_pitch+j]<<2 )
                          + ( p_at[(i-2)*i_pitch+j+1]<<2 )
                          + ( p_at[(i-2)*i_pitch+j+2]<<1 )
                          /* 1 row up */
                          + ( p_at[(i-1)*i_pitch+j-1]<<3 )
                          + ( p_at[(i-1)*i_pitch+j-2]<<2 )
                          + ( p_at[(i-1)*i_pitch+j]*12 )
                          + ( p_at[(i-1)*i_pitch+j+1]<<3 )
                          + ( p_at[(i-1)*i_pitch+j+2]<<2 )
                          /* */
                          + ( p_at[i*i_pitch+j-2]<<2 )
                          + ( p_at[i*i_pitch+j-1]*12 )
                          + ( p_at[i*i_pitch+j]<<4 )
                          + ( p_at[i*i_pitch+j+1]*12 )
                          + ( p_at[i*i_pitch+j+2]<<2 )
                          /* 1 row down */
                          + ( p_at[(i+1)*i_pitch+j-2]<<2 )
                          + ( p_at[(i+1)*i_pitch+j-1]<<3 )
                          + ( p_at[(i+1)*i_pitch+j]*12 )
                          + ( p_at[(i+1)*i_pitch+j+1]<<3 )
                          + ( p_at[(i+1)*i_pitch+j+2]<<2 )
                          /* 2 rows down */
                          + ( p_at[(i+2)*i_pitch+j-2]<<1 )
                          + ( p_at[(i+2)*i_pitch+j-1]<<2 )
                          + ( p_at[(i+2)*i_pitch+j]<<2 )
                          + ( p_at[(i+2)*i_pitch+j+1]<<2 )
                          + ( p_at[(i+2)*i_pitch+j+2]<<1 )
                          )/152);
                          if( p_a[i*i_pitch+j] < 0xbf ) p_a[i*i_pitch+j] = 0x00;
                    }
                }
                free( p_at );
            }
        }
        else
        {
            p_converted = p_es->p_picture;
            p_converted->i_refcount++;
            fmt_in.i_width = fmt_out.i_width = p_converted->format.i_width;
            fmt_in.i_height = fmt_out.i_height = p_converted->format.i_height;
            fmt_in.i_chroma = fmt_out.i_chroma = p_converted->format.i_chroma;
            fmt_out.i_visible_width = fmt_out.i_width;
            fmt_out.i_visible_height = fmt_out.i_height;
        }

        p_region = p_spu->pf_make_region( VLC_OBJECT(p_filter), &fmt_out,
                                          p_converted );
        if( !p_region )
        {
            msg_Err( p_filter, "cannot allocate SPU region" );
            p_filter->pf_sub_buffer_del( p_filter, p_spu );
            vlc_mutex_unlock( &p_sys->lock );
            vlc_mutex_unlock( p_sys->p_lock );
            return p_spu;
        }

        /* HACK ALERT : let's fix the pointers to avoid picture duplication.
         * This is necessary because p_region->picture is not a pointer
         * as it ought to be. */
        if( !p_sys->b_keep )
        {
            free( p_converted );
        }
        else
        {
            /* Keep a pointer to the original picture (and its refcount...). */
            p_region->picture.p_sys = (picture_sys_t *)p_converted;
            p_region->picture.pf_release = MosaicReleasePicture;
        }

        if( fmt_out.i_width > col_inner_width ||
            p_sys->b_ar || p_sys->b_keep )
        {
            /* we don't have to center the video since it takes the
            whole rectangle area or it's larger than the rectangle */
            p_region->i_x = p_sys->i_xoffset
                        + i_col * ( p_sys->i_width / p_sys->i_cols )
                        + ( i_col * p_sys->i_vborder ) / p_sys->i_cols;
        }
        else
        {
            /* center the video in the dedicated rectangle */
            p_region->i_x = p_sys->i_xoffset
                    + i_col * ( p_sys->i_width / p_sys->i_cols )
                    + ( i_col * p_sys->i_vborder ) / p_sys->i_cols
                    + ( col_inner_width - fmt_out.i_width ) / 2;
        }

        if( fmt_out.i_height < row_inner_height
            || p_sys->b_ar || p_sys->b_keep )
        {
            /* we don't have to center the video since it takes the
            whole rectangle area or it's taller than the rectangle */
            p_region->i_y = p_sys->i_yoffset
                    + i_row * ( p_sys->i_height / p_sys->i_rows )
                    + ( i_row * p_sys->i_hborder ) / p_sys->i_rows;
        }
        else
        {
            /* center the video in the dedicated rectangle */
            p_region->i_y = p_sys->i_yoffset
                    + i_row * ( p_sys->i_height / p_sys->i_rows )
                    + ( i_row * p_sys->i_hborder ) / p_sys->i_rows
                    + ( row_inner_height - fmt_out.i_height ) / 2;
        }

        if( p_region_prev == NULL )
        {
            p_spu->p_region = p_region;
        }
        else
        {
            p_region_prev->p_next = p_region;
        }

        p_region_prev = p_region;
    }

    vlc_mutex_unlock( p_sys->p_lock );
    vlc_mutex_unlock( &p_sys->lock );

    return p_spu;
}

/*****************************************************************************
* Callback to update params on the fly
*****************************************************************************/
static int MosaicCallback( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    filter_sys_t *p_sys = (filter_sys_t *) p_data;
    if( !strcmp( psz_var, "mosaic-alpha" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing alpha from %d/255 to %d/255",
                         p_sys->i_alpha, newval.i_int);
        p_sys->i_alpha = __MIN( __MAX( newval.i_int, 0 ), 255 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( !strcmp( psz_var, "mosaic-height" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing height from %dpx to %dpx",
                          p_sys->i_height, newval.i_int );
        p_sys->i_height = __MAX( newval.i_int, 0 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( !strcmp( psz_var, "mosaic-width" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing width from %dpx to %dpx",
                         p_sys->i_width, newval.i_int );
        p_sys->i_width = __MAX( newval.i_int, 0 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( !strcmp( psz_var, "mosaic-xoffset" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing x offset from %dpx to %dpx",
                         p_sys->i_xoffset, newval.i_int );
        p_sys->i_xoffset = __MAX( newval.i_int, 0 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( !strcmp( psz_var, "mosaic-yoffset" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing y offset from %dpx to %dpx",
                         p_sys->i_yoffset, newval.i_int );
        p_sys->i_yoffset = __MAX( newval.i_int, 0 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( !strcmp( psz_var, "mosaic-align" ) )
    {
        int i_old = 0, i_new = 0;
        vlc_mutex_lock( &p_sys->lock );
        newval.i_int = __MIN( __MAX( newval.i_int, 0 ), 10 );
        if( newval.i_int == 3 || newval.i_int == 7 )
            newval.i_int = 5;
        while( pi_align_values[i_old] != p_sys->i_align ) i_old++;
        while( pi_align_values[i_new] != newval.i_int ) i_new++;
        msg_Dbg( p_this, "changing alignment from %d (%s) to %d (%s)",
                     p_sys->i_align, ppsz_align_descriptions[i_old],
                     newval.i_int, ppsz_align_descriptions[i_new] );
        p_sys->i_align = newval.i_int;
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( !strcmp( psz_var, "mosaic-vborder" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing vertical border from %dpx to %dpx",
                         p_sys->i_vborder, newval.i_int );
        p_sys->i_vborder = __MAX( newval.i_int, 0 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( !strcmp( psz_var, "mosaic-hborder" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing horizontal border from %dpx to %dpx",
                         p_sys->i_vborder, newval.i_int );
        p_sys->i_hborder = __MAX( newval.i_int, 0 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( !strcmp( psz_var, "mosaic-position" ) )
    {
        if( newval.i_int > 1 || newval.i_int < 0 )
        {
            msg_Err( p_this, "Position is either 0 (auto) or 1 (fixed)" );
        }
        else
        {
            vlc_mutex_lock( &p_sys->lock );
            msg_Dbg( p_this, "changing position method from %d (%s) to %d (%s)",
                             p_sys->i_position, ppsz_pos_descriptions[p_sys->i_position],
                             newval.i_int, ppsz_pos_descriptions[newval.i_int]);
            p_sys->i_position = newval.i_int;
            vlc_mutex_unlock( &p_sys->lock );
        }
    }
    else if( !strcmp( psz_var, "mosaic-rows" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing number of rows from %d to %d",
                         p_sys->i_rows, newval.i_int );
        p_sys->i_rows = __MAX( newval.i_int, 1 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( !strcmp( psz_var, "mosaic-cols" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing number of columns from %d to %d",
                         p_sys->i_cols, newval.i_int );
        p_sys->i_cols = __MAX( newval.i_int, 1 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( !strcmp( psz_var, "mosaic-keep-aspect-ratio" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        if( newval.i_int )
        {
            msg_Dbg( p_this, "keeping aspect ratio" );
            p_sys->b_ar = 1;
        }
        else
        {
            msg_Dbg( p_this, "won't keep aspect ratio" );
            p_sys->b_ar = 0;
        }
        vlc_mutex_unlock( &p_sys->lock );
    }
    return VLC_SUCCESS;
}
