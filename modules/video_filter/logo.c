/*****************************************************************************
 * logo.c : logo video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003-2004 VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Simon Latapie <garf@videolan.org>
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

#include <png.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "vlc_filter.h"
#include "filter_common.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static int  SendEvents( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );
static int MouseEvent ( vlc_object_t *, char const *,
                        vlc_value_t , vlc_value_t , void * );
static int Control    ( vout_thread_t *, int, va_list );

static int  CreateFilter ( vlc_object_t * );
static void DestroyFilter( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FILE_TEXT N_("Logo filename")
#define FILE_LONGTEXT N_("Full path of the PNG file to use.")
#define POSX_TEXT N_("X coordinate of the logo")
#define POSX_LONGTEXT N_("You can move the logo by left-clicking on it." )
#define POSY_TEXT N_("Y coordinate of the logo")
#define POSY_LONGTEXT N_("You can move the logo by left-clicking on it." )
#define TRANS_TEXT N_("Transparency of the logo")
#define TRANS_LONGTEXT N_("You can set the logo transparency value here " \
  "(from 0 for full transparency to 255 for full opacity)." )

vlc_module_begin();
    set_description( _("Logo video filter") );
    set_capability( "video filter", 0 );
    add_shortcut( "logo" );
    set_callbacks( Create, Destroy );

    add_file( "logo-file", NULL, NULL, FILE_TEXT, FILE_LONGTEXT, VLC_FALSE );
    add_integer( "logo-x", 0, NULL, POSX_TEXT, POSX_LONGTEXT, VLC_FALSE );
    add_integer( "logo-y", 0, NULL, POSY_TEXT, POSY_LONGTEXT, VLC_FALSE );
    add_integer_with_range( "logo-transparency", 255, 0, 255, NULL,
        TRANS_TEXT, TRANS_LONGTEXT, VLC_FALSE );

    /* subpicture filter submodule */
    add_submodule();
    set_capability( "sub filter", 0 );
    set_callbacks( CreateFilter, DestroyFilter );
    set_description( _("Logo sub filter") );
    add_shortcut( "logo" );
vlc_module_end();

/*****************************************************************************
 * LoadPNG: loads the PNG logo into memory
 *****************************************************************************/
static picture_t *LoadPNG( vlc_object_t *p_this )
{
    picture_t *p_pic;
    char *psz_filename;
    vlc_value_t val;
    FILE *file;
    int i, j, i_trans;
    vlc_bool_t b_alpha = VLC_TRUE;

    png_uint_32 i_width, i_height;
    int i_color_type, i_interlace_type, i_compression_type, i_filter_type;
    int i_bit_depth;
    png_bytep *p_row_pointers;
    png_structp p_png;
    png_infop p_info, p_end_info;

    var_Create( p_this, "logo-file", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_this, "logo-file", &val );
    psz_filename = val.psz_string;
    if( !psz_filename ) return 0;

    if( !(file = fopen( psz_filename , "rb" )) )
    {
        msg_Err( p_this , "logo file (%s) not found", psz_filename );
        free( psz_filename );
        return 0;
    }
    free( psz_filename );

    p_png = png_create_read_struct( PNG_LIBPNG_VER_STRING, 0, 0, 0 );
    p_info = png_create_info_struct( p_png );
    p_end_info = png_create_info_struct( p_png );
    png_init_io( p_png, file );
    png_read_info( p_png, p_info );
    png_get_IHDR( p_png, p_info, &i_width, &i_height,
                  &i_bit_depth, &i_color_type, &i_interlace_type,
                  &i_compression_type, &i_filter_type);

    if( i_color_type == PNG_COLOR_TYPE_PALETTE )
        png_set_palette_to_rgb( p_png );

    if( i_color_type == PNG_COLOR_TYPE_GRAY ||
        i_color_type == PNG_COLOR_TYPE_GRAY_ALPHA )
          png_set_gray_to_rgb( p_png );

    if( png_get_valid( p_png, p_info, PNG_INFO_tRNS ) )
    {
        png_set_tRNS_to_alpha( p_png );
    }
    else
    {
        b_alpha = VLC_FALSE;
    }

    p_row_pointers = malloc( sizeof(png_bytep) * i_height );
    for( i = 0; i < (int)i_height; i++ )
    {
        p_row_pointers[i] = malloc( 4 * ( i_bit_depth + 7 ) / 8 * i_width );
    }
    png_read_image( p_png, p_row_pointers );
    png_read_end( p_png, p_end_info );

    fclose( file );
    png_destroy_read_struct( &p_png, &p_info, &p_end_info );

    /* Convert to YUVA */
    p_pic = malloc( sizeof(picture_t) );
    if( vout_AllocatePicture( p_this, p_pic, VLC_FOURCC('Y','U','V','A'),
                              i_width, i_height, VOUT_ASPECT_FACTOR ) !=
        VLC_SUCCESS )
    {
        free( p_row_pointers );
        return 0;
    }

    var_Create(p_this, "logo-transparency", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT);
    var_Get( p_this, "logo-transparency", &val );
    i_trans = val.i_int;

    for( j = 0; j < (int)i_height ; j++ )
    {
        uint8_t *p = (uint8_t *)p_row_pointers[j];

        for( i = 0; i < (int)i_width ; i++ )
        {
            int i_offset = i + j * p_pic->p[Y_PLANE].i_pitch;

            p_pic->p[Y_PLANE].p_pixels[i_offset] =
                (p[0] * 257L + p[1] * 504 + p[2] * 98)/1000 + 16;
            p_pic->p[U_PLANE].p_pixels[i_offset] =
                (p[2] * 439L - p[0] * 148 - p[1] * 291)/1000 + 128;
            p_pic->p[V_PLANE].p_pixels[i_offset] =
                (p[0] * 439L - p[1] * 368 - p[2] * 71)/1000 + 128;
            p_pic->p[A_PLANE].p_pixels[i_offset] =
                b_alpha ? (p[3] * i_trans) / 255 : 255;

            p += (b_alpha ? 4 : 3);
        }
    }

    free( p_row_pointers );
    return p_pic;
}

/*****************************************************************************
 * vout_sys_t: logo video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Invert specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    vout_thread_t *p_vout;

    filter_t *p_blend;
    picture_t *p_pic;

    int i_width, i_height;
    int posx, posy;
};

/*****************************************************************************
 * Create: allocates logo video thread output method
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys;
    vlc_value_t val;

    /* Allocate structure */
    p_sys = p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;
    p_vout->pf_control = Control;

    var_Create( p_this, "logo-x", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, "logo-x", &val );
    p_sys->posx = val.i_int;
    var_Create( p_this, "logo-y", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, "logo-y", &val );
    p_sys->posy = val.i_int;

    p_sys->p_pic = LoadPNG( p_this );
    if( !p_sys->p_pic )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->i_width = p_sys->p_pic->p[Y_PLANE].i_visible_pitch;
    p_sys->i_height = p_sys->p_pic->p[Y_PLANE].i_visible_lines;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize logo video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    picture_t *p_pic;
    int i_index;

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    /* Load the video blending filter */
    p_sys->p_blend = vlc_object_create( p_vout, sizeof(filter_t) );
    vlc_object_attach( p_sys->p_blend, p_vout );
    p_sys->p_blend->fmt_out.video.i_x_offset =
        p_sys->p_blend->fmt_out.video.i_y_offset = 0;
    p_sys->p_blend->fmt_in.video.i_x_offset =
        p_sys->p_blend->fmt_in.video.i_y_offset = 0;
    p_sys->p_blend->fmt_out.video.i_aspect = p_vout->render.i_aspect;
    p_sys->p_blend->fmt_out.video.i_chroma = p_vout->output.i_chroma;
    p_sys->p_blend->fmt_in.video.i_chroma = VLC_FOURCC('Y','U','V','A');
    p_sys->p_blend->fmt_in.video.i_aspect = VOUT_ASPECT_FACTOR;
    p_sys->p_blend->fmt_in.video.i_width =
        p_sys->p_blend->fmt_in.video.i_visible_width =
            p_sys->p_pic->p[Y_PLANE].i_visible_pitch;
    p_sys->p_blend->fmt_in.video.i_height =
        p_sys->p_blend->fmt_in.video.i_visible_height =
            p_sys->p_pic->p[Y_PLANE].i_visible_lines;
    p_sys->p_blend->fmt_out.video.i_width =
        p_sys->p_blend->fmt_out.video.i_visible_width =
           p_vout->output.i_width;
    p_sys->p_blend->fmt_out.video.i_height =
        p_sys->p_blend->fmt_out.video.i_visible_height =
            p_vout->output.i_height;

    p_sys->p_blend->p_module =
        module_Need( p_sys->p_blend, "video blending", 0, 0 );
    if( !p_sys->p_blend->p_module )
    {
        msg_Err( p_vout, "can't open blending filter, aborting" );
        vlc_object_detach( p_sys->p_blend );
        vlc_object_destroy( p_sys->p_blend );
        return VLC_EGENERIC;
    }

    /* Try to open the real video output */
    msg_Dbg( p_vout, "spawning the real video output" );

    p_sys->p_vout =
        vout_Create( p_vout, p_vout->render.i_width, p_vout->render.i_height,
                     p_vout->render.i_chroma, p_vout->render.i_aspect );

    /* Everything failed */
    if( p_sys->p_vout == NULL )
    {
        msg_Err( p_vout, "can't open vout, aborting" );
        return VLC_EGENERIC;
    }

    var_AddCallback( p_sys->p_vout, "mouse-x", MouseEvent, p_vout);
    var_AddCallback( p_sys->p_vout, "mouse-y", MouseEvent, p_vout);

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );
    ADD_CALLBACKS( p_sys->p_vout, SendEvents );
    ADD_PARENT_CALLBACKS( SendEventsToChild );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate logo video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    int i_index;

    /* Free the fake output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        free( PP_OUTPUTPICTURE[ i_index ]->p_data_orig );
    }

    var_DelCallback( p_sys->p_vout, "mouse-x", MouseEvent, p_vout);
    var_DelCallback( p_sys->p_vout, "mouse-y", MouseEvent, p_vout);

    if( p_sys->p_vout )
    {
        DEL_CALLBACKS( p_sys->p_vout, SendEvents );
        vlc_object_detach( p_sys->p_vout );
        vout_Destroy( p_sys->p_vout );
    }

    if( p_sys->p_blend->p_module )
        module_Unneed( p_sys->p_blend, p_sys->p_blend->p_module );
    vlc_object_detach( p_sys->p_blend );
    vlc_object_destroy( p_sys->p_blend );
}

/*****************************************************************************
 * Destroy: destroy logo video thread output method
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys = p_vout->p_sys;

    DEL_PARENT_CALLBACKS( SendEventsToChild );

    if( p_sys->p_pic && p_sys->p_pic->p_data_orig )
        free( p_sys->p_pic->p_data_orig );
    if( p_sys->p_pic ) free( p_sys->p_pic );

    free( p_sys );
}

/*****************************************************************************
 * Render: render the logo onto the video
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    picture_t *p_outpic;

    /* This is a new frame. Get a structure from the video_output. */
    while( !(p_outpic = vout_CreatePicture( p_sys->p_vout, 0, 0, 0 )) )
    {
        if( p_vout->b_die || p_vout->b_error ) return;
        msleep( VOUT_OUTMEM_SLEEP );
    }

    vout_CopyPicture( p_vout, p_outpic, p_pic );
    vout_DatePicture( p_sys->p_vout, p_outpic, p_pic->date );

    p_sys->p_blend->pf_video_blend( p_sys->p_blend, p_outpic, p_outpic,
                                    p_sys->p_pic, p_sys->posx, p_sys->posy );

    vout_DisplayPicture( p_sys->p_vout, p_outpic );
}

/*****************************************************************************
 * SendEvents: forward mouse and keyboard events to the parent p_vout
 *****************************************************************************/
static int SendEvents( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    var_Set( (vlc_object_t *)p_data, psz_var, newval );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * MouseEvent: callback for mouse events
 *****************************************************************************/
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t*)p_data;
    vout_sys_t *p_sys = p_vout->p_sys;
    vlc_value_t valb;
    int i_delta;

    var_Get( p_vout->p_sys->p_vout, "mouse-button-down", &valb );

    i_delta = newval.i_int - oldval.i_int;

    if( (valb.i_int & 0x1) == 0 )
    {
        return VLC_SUCCESS;
    }

    if( psz_var[6] == 'x' )
    {
        vlc_value_t valy;
        var_Get( p_vout->p_sys->p_vout, "mouse-y", &valy );
        if( newval.i_int >= (int)p_sys->posx &&
            valy.i_int >= (int)p_sys->posy &&
            newval.i_int <= (int)(p_sys->posx + p_sys->i_width) &&
            valy.i_int <= (int)(p_sys->posy + p_sys->i_height) )
        {
            p_sys->posx = __MIN( __MAX( p_sys->posx + i_delta, 0 ),
                          p_vout->output.i_width - p_sys->i_width );
        }
    }
    else if( psz_var[6] == 'y' )
    {
        vlc_value_t valx;
        var_Get( p_vout->p_sys->p_vout, "mouse-x", &valx );
        if( valx.i_int >= (int)p_sys->posx &&
            newval.i_int >= (int)p_sys->posy &&
            valx.i_int <= (int)(p_sys->posx + p_sys->i_width) &&
            newval.i_int <= (int)(p_sys->posy + p_sys->i_height) )
        {
            p_sys->posy = __MIN( __MAX( p_sys->posy + i_delta, 0 ),
                          p_vout->output.i_height - p_sys->i_height );
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control: control facility for the vout (forwards to child vout)
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    return vout_vaControl( p_vout->p_sys->p_vout, i_query, args );
}

/*****************************************************************************
 * SendEventsToChild: forward events to the child/children vout
 *****************************************************************************/
static int SendEventsToChild( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    var_Set( p_vout->p_sys->p_vout, psz_var, newval );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * filter_sys_t: logo filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    picture_t *p_pic;

    int i_width, i_height;
    int posx, posy;

    mtime_t i_last_date;
};

static subpicture_t *Filter( filter_t *, mtime_t );

/*****************************************************************************
 * CreateFilter: allocates logo video filter
 *****************************************************************************/
static int CreateFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    vlc_value_t val;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

    var_Create( p_this, "logo-x", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, "logo-x", &val );
    p_sys->posx = val.i_int;
    var_Create( p_this, "logo-y", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, "logo-y", &val );
    p_sys->posy = val.i_int;

    p_sys->p_pic = LoadPNG( p_this );
    if( !p_sys->p_pic )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->i_width = p_sys->p_pic->p[Y_PLANE].i_visible_pitch;
    p_sys->i_height = p_sys->p_pic->p[Y_PLANE].i_visible_lines;
    p_sys->i_last_date = 0;

    /* Misc init */
    p_filter->pf_sub_filter = Filter;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DestroyFilter: destroy logo video filter
 *****************************************************************************/
static void DestroyFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->p_pic && p_sys->p_pic->p_data_orig )
        free( p_sys->p_pic->p_data_orig );
    if( p_sys->p_pic ) free( p_sys->p_pic );

    free( p_sys );
}

/****************************************************************************
 * Filter: the whole thing
 ****************************************************************************
 * This function outputs subpictures at regular time intervals.
 ****************************************************************************/
static subpicture_t *Filter( filter_t *p_filter, mtime_t date )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    subpicture_t *p_spu;
    subpicture_region_t *p_region;
    video_format_t fmt;

    if( p_sys->i_last_date && p_sys->i_last_date + 5000000 > date ) return 0;

    /* Allocate the subpicture internal data. */
    p_spu = p_filter->pf_sub_buffer_new( p_filter );
    if( !p_spu ) return NULL;

    /* Create new SPU region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('Y','U','V','A');
    fmt.i_aspect = VOUT_ASPECT_FACTOR;
    fmt.i_width = fmt.i_visible_width = p_sys->i_width;
    fmt.i_height = fmt.i_visible_height = p_sys->i_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_region = p_spu->pf_create_region( VLC_OBJECT(p_filter), &fmt );
    if( !p_region )
    {
        msg_Err( p_filter, "cannot allocate SPU region" );
        p_filter->pf_sub_buffer_del( p_filter, p_spu );
        return NULL;
    }

    vout_CopyPicture( p_filter, &p_region->picture, p_sys->p_pic );
    p_region->i_x = 0;
    p_region->i_y = 0;
    p_spu->i_x = p_sys->posx;
    p_spu->i_y = p_sys->posy;
    p_spu->p_region = p_region;

    p_spu->i_start = p_sys->i_last_date = date;
    p_spu->i_stop = 0;
    p_spu->b_ephemer = VLC_TRUE;

    return p_spu;
}
