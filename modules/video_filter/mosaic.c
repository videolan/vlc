/*****************************************************************************
 * mosaic.c : Mosaic video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2004-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan dot org>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#include <math.h>
#include <limits.h> /* INT_MAX */

#include <vlc_filter.h>
#include <vlc_image.h>

#include "mosaic.h"

#define BLANK_DELAY INT64_C(1000000)

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  CreateFilter    ( vlc_object_t * );
static void DestroyFilter   ( vlc_object_t * );
static subpicture_t *Filter ( filter_t *, mtime_t );

static int MosaicCallback   ( vlc_object_t *, char const *, vlc_value_t,
                              vlc_value_t, void * );

/*****************************************************************************
 * filter_sys_t : filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    vlc_mutex_t lock;         /* Internal filter lock */

    image_handler_t *p_image;

    int i_position;           /* Mosaic positioning method */
    bool b_ar;          /* Do we keep the aspect ratio ? */
    bool b_keep;        /* Do we keep the original picture format ? */
    int i_width, i_height;    /* Mosaic height and width */
    int i_cols, i_rows;       /* Mosaic rows and cols */
    int i_align;              /* Mosaic alignment in background video */
    int i_xoffset, i_yoffset; /* Top left corner offset */
    int i_borderw, i_borderh; /* Border width/height between miniatures */
    int i_alpha;              /* Subfilter alpha blending */

    char **ppsz_order;        /* List of picture-ids */
    int i_order_length;

    int *pi_x_offsets;        /* List of substreams x offsets */
    int *pi_y_offsets;        /* List of substreams y offsets */
    int i_offsets_length;

    mtime_t i_delay;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ALPHA_TEXT N_("Transparency")
#define ALPHA_LONGTEXT N_( \
        "Transparency of the mosaic foreground pictures. " \
        "0 means transparent, 255 opaque (default)." )

#define HEIGHT_TEXT N_("Height")
#define HEIGHT_LONGTEXT N_( "Total height of the mosaic, in pixels." )
#define WIDTH_TEXT N_("Width")
#define WIDTH_LONGTEXT N_( "Total width of the mosaic, in pixels." )

#define XOFFSET_TEXT N_("Top left corner X coordinate")
#define XOFFSET_LONGTEXT N_( \
        "X Coordinate of the top-left corner of the mosaic.")
#define YOFFSET_TEXT N_("Top left corner Y coordinate")
#define YOFFSET_LONGTEXT N_( \
        "Y Coordinate of the top-left corner of the mosaic.")

#define BORDERW_TEXT N_("Border width")
#define BORDERW_LONGTEXT N_( \
        "Width in pixels of the border between miniatures." )
#define BORDERH_TEXT N_("Border height")
#define BORDERH_LONGTEXT N_( \
        "Height in pixels of the border between miniatures." )

#define ALIGN_TEXT N_("Mosaic alignment" )
#define ALIGN_LONGTEXT N_( \
        "You can enforce the mosaic alignment on the video " \
        "(0=center, 1=left, 2=right, 4=top, 8=bottom, you can " \
        "also use combinations of these values, eg 6 = top-right).")

#define POS_TEXT N_("Positioning method")
#define POS_LONGTEXT N_( \
        "Positioning method for the mosaic. auto: " \
        "automatically choose the best number of rows and columns. " \
        "fixed: use the user-defined number of rows and columns. " \
        "offsets: use the user-defined offsets for each image." )

#define ROWS_TEXT N_("Number of rows")
#define ROWS_LONGTEXT N_( \
        "Number of image rows in the mosaic (only used if " \
        "positioning method is set to \"fixed\")." )

#define COLS_TEXT N_("Number of columns")
#define COLS_LONGTEXT N_( \
        "Number of image columns in the mosaic (only used if " \
        "positioning method is set to \"fixed\"." )

#define AR_TEXT N_("Keep aspect ratio")
#define AR_LONGTEXT N_( \
        "Keep the original aspect ratio when resizing " \
        "mosaic elements." )
#define KEEP_TEXT N_("Keep original size")
#define KEEP_LONGTEXT N_( \
        "Keep the original size of mosaic elements." )

#define ORDER_TEXT N_("Elements order" )
#define ORDER_LONGTEXT N_( \
        "You can enforce the order of the elements on " \
        "the mosaic. You must give a comma-separated list of picture ID(s)." \
        "These IDs are assigned in the \"mosaic-bridge\" module." )

#define OFFSETS_TEXT N_("Offsets in order" )
#define OFFSETS_LONGTEXT N_( \
        "You can enforce the (x,y) offsets of the elements on the mosaic " \
        "(only used if positioning method is set to \"offsets\"). You " \
        "must give a comma-separated list of coordinates (eg: 10,10,150,10)." )

#define DELAY_TEXT N_("Delay")
#define DELAY_LONGTEXT N_( \
        "Pictures coming from the mosaic elements will be delayed " \
        "according to this value (in milliseconds). For high " \
        "values you will need to raise caching at input.")

enum
{
    position_auto = 0, position_fixed = 1, position_offsets = 2
};
static const int pi_pos_values[] = { 0, 1, 2 };
static const char *const ppsz_pos_descriptions[] =
    { N_("auto"), N_("fixed"), N_("offsets") };

static const int pi_align_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static const char *const ppsz_align_descriptions[] =
     { N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
     N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

#define CFG_PREFIX "mosaic-"

vlc_module_begin ()
    set_description( N_("Mosaic video sub source") )
    set_shortname( N_("Mosaic") )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_SUBPIC)
    set_capability( "sub source", 0 )
    set_callbacks( CreateFilter, DestroyFilter )

    add_integer_with_range( CFG_PREFIX "alpha", 255, 0, 255,
                            ALPHA_TEXT, ALPHA_LONGTEXT, false )

    add_integer( CFG_PREFIX "height", 100,
                 HEIGHT_TEXT, HEIGHT_LONGTEXT, false )
    add_integer( CFG_PREFIX "width", 100,
                 WIDTH_TEXT, WIDTH_LONGTEXT, false )

    add_integer( CFG_PREFIX "align", 5,
                 ALIGN_TEXT, ALIGN_LONGTEXT, true)
        change_integer_list( pi_align_values, ppsz_align_descriptions )

    add_integer( CFG_PREFIX "xoffset", 0,
                 XOFFSET_TEXT, XOFFSET_LONGTEXT, true )
    add_integer( CFG_PREFIX "yoffset", 0,
                 YOFFSET_TEXT, YOFFSET_LONGTEXT, true )

    add_integer( CFG_PREFIX "borderw", 0,
                 BORDERW_TEXT, BORDERW_LONGTEXT, true )
    add_integer( CFG_PREFIX "borderh", 0,
                 BORDERH_TEXT, BORDERH_LONGTEXT, true )

    add_integer( CFG_PREFIX "position", 0,
                 POS_TEXT, POS_LONGTEXT, false )
        change_integer_list( pi_pos_values, ppsz_pos_descriptions )
    add_integer( CFG_PREFIX "rows", 2,
                 ROWS_TEXT, ROWS_LONGTEXT, false )
    add_integer( CFG_PREFIX "cols", 2,
                 COLS_TEXT, COLS_LONGTEXT, false )

    add_bool( CFG_PREFIX "keep-aspect-ratio", false,
              AR_TEXT, AR_LONGTEXT, false )
    add_bool( CFG_PREFIX "keep-picture", false,
              KEEP_TEXT, KEEP_LONGTEXT, false )

    add_string( CFG_PREFIX "order", "",
                ORDER_TEXT, ORDER_LONGTEXT, false )

    add_string( CFG_PREFIX "offsets", "",
                OFFSETS_TEXT, OFFSETS_LONGTEXT, false )

    add_integer( CFG_PREFIX "delay", 0, DELAY_TEXT, DELAY_LONGTEXT,
                 false )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "alpha", "height", "width", "align", "xoffset", "yoffset",
    "borderw", "borderh", "position", "rows", "cols",
    "keep-aspect-ratio", "keep-picture", "order", "offsets",
    "delay", NULL
};

/*****************************************************************************
 * mosaic_ParseSetOffsets:
 * parse the "--mosaic-offsets x1,y1,x2,y2,x3,y3" parameter
 * and set the corresponding struct filter_sys_t entries.
 *****************************************************************************/
static void mosaic_ParseSetOffsets( vlc_object_t *p_this,
                                      filter_sys_t *p_sys,
                                      char *psz_offsets )
{
    if( *psz_offsets )
    {
        char *psz_end = NULL;
        int i_index = 0;
        do
        {
            i_index++;

            p_sys->pi_x_offsets = xrealloc( p_sys->pi_x_offsets,
                                                   i_index * sizeof(int) );
            p_sys->pi_x_offsets[i_index - 1] = atoi( psz_offsets );
            psz_end = strchr( psz_offsets, ',' );
            psz_offsets = psz_end + 1;

            p_sys->pi_y_offsets = xrealloc( p_sys->pi_y_offsets,
                                                   i_index * sizeof(int) );
            p_sys->pi_y_offsets[i_index - 1] = atoi( psz_offsets );
            psz_end = strchr( psz_offsets, ',' );
            psz_offsets = psz_end + 1;

            msg_Dbg( p_this, CFG_PREFIX "offset: id %d, x=%d, y=%d",
                     i_index, p_sys->pi_x_offsets[i_index - 1],
                              p_sys->pi_y_offsets[i_index - 1]  );

        } while( psz_end );
        p_sys->i_offsets_length = i_index;
    }
}
#define mosaic_ParseSetOffsets( a, b, c ) \
            mosaic_ParseSetOffsets( VLC_OBJECT( a ), b, c )

/*****************************************************************************
 * CreateFiler: allocate mosaic video filter
 *****************************************************************************/
static int CreateFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    char *psz_order, *_psz_order;
    char *psz_offsets;
    int i_index;
    int i_command;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_filter->pf_sub_source = Filter;

    vlc_mutex_init( &p_sys->lock );
    vlc_mutex_lock( &p_sys->lock );

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

#define GET_VAR( name, min, max )                                           \
    i_command = var_CreateGetIntegerCommand( p_filter, CFG_PREFIX #name );  \
    p_sys->i_##name = VLC_CLIP( i_command, min, max );                \
    var_AddCallback( p_filter, CFG_PREFIX #name, MosaicCallback, p_sys );

    GET_VAR( width, 0, INT_MAX );
    GET_VAR( height, 0, INT_MAX );
    GET_VAR( xoffset, 0, INT_MAX );
    GET_VAR( yoffset, 0, INT_MAX );

    GET_VAR( align, 0, 10 );
    if( p_sys->i_align == 3 || p_sys->i_align == 7 )
        p_sys->i_align = 5;

    GET_VAR( borderw, 0, INT_MAX );
    GET_VAR( borderh, 0, INT_MAX );
    GET_VAR( rows, 1, INT_MAX );
    GET_VAR( cols, 1, INT_MAX );
    GET_VAR( alpha, 0, 255 );
    GET_VAR( position, 0, 2 );
    GET_VAR( delay, 100, INT_MAX );
#undef GET_VAR
    p_sys->i_delay *= 1000;

    p_sys->b_ar = var_CreateGetBoolCommand( p_filter,
                                            CFG_PREFIX "keep-aspect-ratio" );
    var_AddCallback( p_filter, CFG_PREFIX "keep-aspect-ratio", MosaicCallback,
                     p_sys );

    p_sys->b_keep = var_CreateGetBoolCommand( p_filter,
                                              CFG_PREFIX "keep-picture" );
    if ( !p_sys->b_keep )
    {
        p_sys->p_image = image_HandlerCreate( p_filter );
    }

    p_sys->i_order_length = 0;
    p_sys->ppsz_order = NULL;
    psz_order = var_CreateGetStringCommand( p_filter, CFG_PREFIX "order" );
    _psz_order = psz_order;
    var_AddCallback( p_filter, CFG_PREFIX "order", MosaicCallback, p_sys );

    if( *psz_order )
    {
        char *psz_end = NULL;
        i_index = 0;
        do
        {
            psz_end = strchr( psz_order, ',' );
            i_index++;
            p_sys->ppsz_order = xrealloc( p_sys->ppsz_order,
                                                 i_index * sizeof(char *) );
            p_sys->ppsz_order[i_index - 1] = strndup( psz_order,
                                           psz_end - psz_order );
            psz_order = psz_end+1;
        } while( psz_end );
        p_sys->i_order_length = i_index;
    }

    free( _psz_order );

    /* Manage specific offsets for substreams */
    psz_offsets = var_CreateGetStringCommand( p_filter, CFG_PREFIX "offsets" );
    p_sys->i_offsets_length = 0;
    p_sys->pi_x_offsets = NULL;
    p_sys->pi_y_offsets = NULL;
    mosaic_ParseSetOffsets( p_filter, p_sys, psz_offsets );
    free( psz_offsets );
    var_AddCallback( p_filter, CFG_PREFIX "offsets", MosaicCallback, p_sys );

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

#define DEL_CB( name ) \
    var_DelCallback( p_filter, CFG_PREFIX #name, MosaicCallback, p_sys )
    DEL_CB( width );
    DEL_CB( height );
    DEL_CB( xoffset );
    DEL_CB( yoffset );

    DEL_CB( align );

    DEL_CB( borderw );
    DEL_CB( borderh );
    DEL_CB( rows );
    DEL_CB( cols );
    DEL_CB( alpha );
    DEL_CB( position );
    DEL_CB( delay );

    DEL_CB( keep-aspect-ratio );
    DEL_CB( order );
#undef DEL_CB

    if( !p_sys->b_keep )
    {
        image_HandlerDelete( p_sys->p_image );
    }

    if( p_sys->i_order_length )
    {
        for( int i_index = 0; i_index < p_sys->i_order_length; i_index++ )
        {
            free( p_sys->ppsz_order[i_index] );
        }
        free( p_sys->ppsz_order );
    }
    if( p_sys->i_offsets_length )
    {
        free( p_sys->pi_x_offsets );
        free( p_sys->pi_y_offsets );
        p_sys->i_offsets_length = 0;
    }

    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys );
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
    p_spu = filter_NewSubpicture( p_filter );
    if( !p_spu )
        return NULL;

    /* Initialize subpicture */
    p_spu->i_channel = 0;
    p_spu->i_start  = date;
    p_spu->i_stop = 0;
    p_spu->b_ephemer = true;
    p_spu->i_alpha = p_sys->i_alpha;
    p_spu->b_absolute = false;

    vlc_mutex_lock( &p_sys->lock );
    vlc_global_lock( VLC_MOSAIC_MUTEX );

    p_bridge = GetBridge( p_filter );
    if ( p_bridge == NULL )
    {
        vlc_global_unlock( VLC_MOSAIC_MUTEX );
        vlc_mutex_unlock( &p_sys->lock );
        return p_spu;
    }

    if ( p_sys->i_position == position_offsets )
    {
        /* If we have either too much or not enough offsets, fall-back
         * to automatic positioning. */
        if ( p_sys->i_offsets_length != p_sys->i_order_length )
        {
            msg_Err( p_filter,
                     "Number of specified offsets (%d) does not match number "
                     "of input substreams in mosaic-order (%d), falling back "
                     "to mosaic-position=0",
                     p_sys->i_offsets_length, p_sys->i_order_length );
            p_sys->i_position = position_auto;
        }
    }

    if ( p_sys->i_position == position_auto )
    {
        int i_numpics = p_sys->i_order_length; /* keep slots and all */
        for ( i_index = 0; i_index < p_bridge->i_es_num; i_index++ )
        {
            bridged_es_t *p_es = p_bridge->pp_es[i_index];
            if ( !p_es->b_empty )
            {
                i_numpics ++;
                if( p_sys->i_order_length && p_es->psz_id != NULL )
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
        p_sys->i_rows = ceil(sqrt( (double)i_numpics ));
        p_sys->i_cols = ( i_numpics % p_sys->i_rows == 0 ?
                            i_numpics / p_sys->i_rows :
                            i_numpics / p_sys->i_rows + 1 );
    }

    col_inner_width  = ( ( p_sys->i_width - ( p_sys->i_cols - 1 )
                       * p_sys->i_borderw ) / p_sys->i_cols );
    row_inner_height = ( ( p_sys->i_height - ( p_sys->i_rows - 1 )
                       * p_sys->i_borderh ) / p_sys->i_rows );

    i_real_index = 0;

    for ( i_index = 0; i_index < p_bridge->i_es_num; i_index++ )
    {
        bridged_es_t *p_es = p_bridge->pp_es[i_index];
        video_format_t fmt_in, fmt_out;
        picture_t *p_converted;

        memset( &fmt_in, 0, sizeof( video_format_t ) );
        memset( &fmt_out, 0, sizeof( video_format_t ) );

        if ( p_es->b_empty )
            continue;

        while ( p_es->p_picture != NULL
                 && p_es->p_picture->date + p_sys->i_delay < date )
        {
            if ( p_es->p_picture->p_next != NULL )
            {
                picture_t *p_next = p_es->p_picture->p_next;
                picture_Release( p_es->p_picture );
                p_es->p_picture = p_next;
            }
            else if ( p_es->p_picture->date + p_sys->i_delay + BLANK_DELAY <
                        date )
            {
                /* Display blank */
                picture_Release( p_es->p_picture );
                p_es->p_picture = NULL;
                p_es->pp_last = &p_es->p_picture;
                break;
            }
            else
            {
                msg_Dbg( p_filter, "too late picture for %s (%"PRId64 ")",
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

            if( fmt_in.i_chroma == VLC_CODEC_YUVA ||
                fmt_in.i_chroma == VLC_CODEC_RGBA )
                fmt_out.i_chroma = VLC_CODEC_YUVA;
            else
                fmt_out.i_chroma = VLC_CODEC_I420;
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
        }
        else
        {
            p_converted = p_es->p_picture;
            fmt_in.i_width = fmt_out.i_width = p_converted->format.i_width;
            fmt_in.i_height = fmt_out.i_height = p_converted->format.i_height;
            fmt_in.i_chroma = fmt_out.i_chroma = p_converted->format.i_chroma;
            fmt_out.i_visible_width = fmt_out.i_width;
            fmt_out.i_visible_height = fmt_out.i_height;
        }

        p_region = subpicture_region_New( &fmt_out );
        /* FIXME the copy is probably not needed anymore */
        if( p_region )
            picture_Copy( p_region->p_picture, p_converted );
        if( !p_sys->b_keep )
            picture_Release( p_converted );

        if( !p_region )
        {
            msg_Err( p_filter, "cannot allocate SPU region" );
            p_filter->pf_sub_buffer_del( p_filter, p_spu );
            vlc_global_unlock( VLC_MOSAIC_MUTEX );
            vlc_mutex_unlock( &p_sys->lock );
            return p_spu;
        }

        if( p_es->i_x >= 0 && p_es->i_y >= 0 )
        {
            p_region->i_x = p_es->i_x;
            p_region->i_y = p_es->i_y;
        }
        else if( p_sys->i_position == position_offsets )
        {
            p_region->i_x = p_sys->pi_x_offsets[i_real_index];
            p_region->i_y = p_sys->pi_y_offsets[i_real_index];
        }
        else
        {
            if( fmt_out.i_width > col_inner_width ||
                p_sys->b_ar || p_sys->b_keep )
            {
                /* we don't have to center the video since it takes the
                whole rectangle area or it's larger than the rectangle */
                p_region->i_x = p_sys->i_xoffset
                            + i_col * ( p_sys->i_width / p_sys->i_cols )
                            + ( i_col * p_sys->i_borderw ) / p_sys->i_cols;
            }
            else
            {
                /* center the video in the dedicated rectangle */
                p_region->i_x = p_sys->i_xoffset
                        + i_col * ( p_sys->i_width / p_sys->i_cols )
                        + ( i_col * p_sys->i_borderw ) / p_sys->i_cols
                        + ( col_inner_width - fmt_out.i_width ) / 2;
            }

            if( fmt_out.i_height > row_inner_height
                || p_sys->b_ar || p_sys->b_keep )
            {
                /* we don't have to center the video since it takes the
                whole rectangle area or it's taller than the rectangle */
                p_region->i_y = p_sys->i_yoffset
                        + i_row * ( p_sys->i_height / p_sys->i_rows )
                        + ( i_row * p_sys->i_borderh ) / p_sys->i_rows;
            }
            else
            {
                /* center the video in the dedicated rectangle */
                p_region->i_y = p_sys->i_yoffset
                        + i_row * ( p_sys->i_height / p_sys->i_rows )
                        + ( i_row * p_sys->i_borderh ) / p_sys->i_rows
                        + ( row_inner_height - fmt_out.i_height ) / 2;
            }
        }
        p_region->i_align = p_sys->i_align;
        p_region->i_alpha = p_es->i_alpha;

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

    vlc_global_unlock( VLC_MOSAIC_MUTEX );
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
    VLC_UNUSED(oldval);
    filter_sys_t *p_sys = (filter_sys_t *) p_data;

#define VAR_IS( a ) !strcmp( psz_var, CFG_PREFIX a )
    if( VAR_IS( "alpha" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing alpha from %d/255 to %d/255",
                         p_sys->i_alpha, (int)newval.i_int);
        p_sys->i_alpha = VLC_CLIP( newval.i_int, 0, 255 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( VAR_IS( "height" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing height from %dpx to %dpx",
                          p_sys->i_height, (int)newval.i_int );
        p_sys->i_height = __MAX( newval.i_int, 0 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( VAR_IS( "width" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing width from %dpx to %dpx",
                         p_sys->i_width, (int)newval.i_int );
        p_sys->i_width = __MAX( newval.i_int, 0 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( VAR_IS( "xoffset" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing x offset from %dpx to %dpx",
                         p_sys->i_xoffset, (int)newval.i_int );
        p_sys->i_xoffset = __MAX( newval.i_int, 0 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( VAR_IS( "yoffset" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing y offset from %dpx to %dpx",
                         p_sys->i_yoffset, (int)newval.i_int );
        p_sys->i_yoffset = __MAX( newval.i_int, 0 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( VAR_IS( "align" ) )
    {
        int i_old = 0, i_new = 0;
        vlc_mutex_lock( &p_sys->lock );
        newval.i_int = VLC_CLIP( newval.i_int, 0, 10 );
        if( newval.i_int == 3 || newval.i_int == 7 )
            newval.i_int = 5;
        while( pi_align_values[i_old] != p_sys->i_align ) i_old++;
        while( pi_align_values[i_new] != newval.i_int ) i_new++;
        msg_Dbg( p_this, "changing alignment from %d (%s) to %d (%s)",
                     p_sys->i_align, ppsz_align_descriptions[i_old],
                     (int)newval.i_int, ppsz_align_descriptions[i_new] );
        p_sys->i_align = newval.i_int;
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( VAR_IS( "borderw" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing border width from %dpx to %dpx",
                         p_sys->i_borderw, (int)newval.i_int );
        p_sys->i_borderw = __MAX( newval.i_int, 0 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( VAR_IS( "borderh" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing border height from %dpx to %dpx",
                         p_sys->i_borderh, (int)newval.i_int );
        p_sys->i_borderh = __MAX( newval.i_int, 0 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( VAR_IS( "position" ) )
    {
        if( newval.i_int > 2 || newval.i_int < 0 )
        {
            msg_Err( p_this,
                     "Position is either 0 (%s), 1 (%s) or 2 (%s)",
                     ppsz_pos_descriptions[0],
                     ppsz_pos_descriptions[1],
                     ppsz_pos_descriptions[2] );
        }
        else
        {
            vlc_mutex_lock( &p_sys->lock );
            msg_Dbg( p_this, "changing position method from %d (%s) to %d (%s)",
                    p_sys->i_position, ppsz_pos_descriptions[p_sys->i_position],
                     (int)newval.i_int, ppsz_pos_descriptions[newval.i_int]);
            p_sys->i_position = newval.i_int;
            vlc_mutex_unlock( &p_sys->lock );
        }
    }
    else if( VAR_IS( "rows" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing number of rows from %d to %d",
                         p_sys->i_rows, (int)newval.i_int );
        p_sys->i_rows = __MAX( newval.i_int, 1 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( VAR_IS( "cols" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "changing number of columns from %d to %d",
                         p_sys->i_cols, (int)newval.i_int );
        p_sys->i_cols = __MAX( newval.i_int, 1 );
        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( VAR_IS( "order" ) )
    {
        char *psz_order;
        int i_index;
        vlc_mutex_lock( &p_sys->lock );
        msg_Dbg( p_this, "Changing mosaic order to %s", newval.psz_string );

        psz_order = newval.psz_string;

        while( p_sys->i_order_length-- )
        {
            free( p_sys->ppsz_order[p_sys->i_order_length] );
        }
        free( p_sys->ppsz_order );
        p_sys->ppsz_order = NULL;

        if( *psz_order )
        {
            char *psz_end = NULL;
            i_index = 0;
            do
            {
                psz_end = strchr( psz_order, ',' );
                i_index++;
                p_sys->ppsz_order = xrealloc( p_sys->ppsz_order,
                                                   i_index * sizeof(char *) );
                p_sys->ppsz_order[i_index - 1] = strndup( psz_order,
                                           psz_end - psz_order );
                psz_order = psz_end+1;
            } while( psz_end );
            p_sys->i_order_length = i_index;
        }

        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( VAR_IS( "offsets" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        msg_Info( p_this, "Changing mosaic-offsets to %s", newval.psz_string );

        if( p_sys->i_offsets_length != 0 )
        {
            p_sys->i_offsets_length = 0;
            free( p_sys->pi_x_offsets );
            free( p_sys->pi_y_offsets );
            p_sys->pi_x_offsets = NULL;
            p_sys->pi_y_offsets = NULL;
        }

        mosaic_ParseSetOffsets( p_this, p_sys, newval.psz_string );

        vlc_mutex_unlock( &p_sys->lock );
    }
    else if( VAR_IS( "keep-aspect-ratio" ) )
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
    else if( VAR_IS( "keep-picture" ) )
    {
        vlc_mutex_lock( &p_sys->lock );
        p_sys->b_keep = newval.b_bool;
        if ( !p_sys->b_keep && !p_sys->p_image )
        {
            p_sys->p_image = image_HandlerCreate( p_this );
        }
        vlc_mutex_unlock( &p_sys->lock );
    }

    return VLC_SUCCESS;
}
