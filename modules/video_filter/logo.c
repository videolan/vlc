/*****************************************************************************
 * logo.c : logo video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2002, 2003 VideoLAN
 * $Id: logo.c,v 1.2 2003/07/04 19:00:43 titer Exp $
 *
 * Authors: Simon Latapie <garf@videolan.org>
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

static int MouseEvent( vlc_object_t *, char const *,
                       vlc_value_t , vlc_value_t , void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define FILE_TEXT N_("Logo File")
#define FILE_LONGTEXT N_("It must be a PNG in RGBA 8bits (for now)")
#define POSX_TEXT N_("x postion of the logo")
#define POSX_LONGTEXT N_("You can move the logo by left-clicking on it" )
#define POSY_TEXT N_("y position of the logo")
#define POSY_LONGTEXT N_("You can move the logo by left-clicking on it" )
#define TRANS_TEXT N_("transparency of the logo")
#define TRANS_LONGTEXT N_("You can change it by middle-clicking and moving mouse left or right")

vlc_module_begin();
    add_category_hint( N_("logo"), NULL, VLC_FALSE );
    add_file( "logo_file", NULL, NULL, FILE_TEXT, FILE_LONGTEXT, VLC_FALSE );
    add_integer( "logo_x", 0, NULL, POSX_TEXT, POSX_LONGTEXT, VLC_FALSE );
    add_integer( "logo_y", 0, NULL, POSY_TEXT, POSY_LONGTEXT, VLC_FALSE );
    add_float( "logo_transparency", 1, NULL, TRANS_TEXT, TRANS_LONGTEXT, VLC_FALSE );
    set_description( _("logo video filter") );
    set_capability( "video filter", 0 );
    add_shortcut( "logo" );
    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: logo video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Invert specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    vout_thread_t *p_vout;
    png_uint_32 height;
    int bit_depth;
    png_uint_32 width;
    uint8_t * png_image[3];
    uint8_t * png_image_u;
    uint8_t * png_image_v;
    uint8_t * png_image_a[3];
    uint8_t * png_image_a_little;
    int error;
    int posx, posy;
    int trans;
};

/*****************************************************************************
 * Create: allocates logo video thread output method
 *****************************************************************************
 * This function allocates and initializes a Invert vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize logo video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;
    char * filename;
    FILE * fp;
    int color_type;
    int interlace_type;
    int compression_type;
    int filter_type;
    png_structp png_ptr;
    png_bytep * row_pointers;
    png_infop info_ptr;
    unsigned int i;
    unsigned int j;
    int temp;
    int i_size;

    /*  read png file  */
    filename = config_GetPsz( p_vout, "logo_file" );
    fp = fopen( filename , "rb");
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL , NULL , NULL);
    info_ptr = png_create_info_struct(png_ptr);
    
    if (fp == NULL)
    {
        p_vout->p_sys->error=1;
        msg_Err( p_vout , "file not found %s", filename );
        free( filename );
    } else
    {
        free( filename );
        p_vout->p_sys->error=0;
        png_init_io(png_ptr, fp);
        png_read_info(png_ptr, info_ptr);
        png_get_IHDR(png_ptr, info_ptr, &p_vout->p_sys->width, &p_vout->p_sys->height,
                     &p_vout->p_sys->bit_depth, &color_type, &interlace_type,
                     &compression_type, &filter_type);
        row_pointers= malloc( sizeof(png_bytep) * p_vout->p_sys->height );
        for( i = 0; i < p_vout->p_sys->height; i++ )
        {
            row_pointers[i] = malloc( 4 * ( p_vout->p_sys->bit_depth + 7 ) / 8 * p_vout->p_sys->width );
        }
        png_read_image(png_ptr, row_pointers);
        fclose(fp);
        /* finish to read the image in the file. Now We have to convert it YUV */
        /* initialize yuv plans of the image */
        i_size = p_vout->p_sys->height * p_vout->p_sys->width;

        p_vout->p_sys->png_image[0] = malloc( i_size );
        p_vout->p_sys->png_image[1] = malloc( i_size / 4 );
        p_vout->p_sys->png_image[2] = malloc( i_size / 4 );

        p_vout->p_sys->png_image_a[0] = malloc( i_size );
        p_vout->p_sys->png_image_a[1] = malloc( i_size / 4 );
        p_vout->p_sys->png_image_a[2] = p_vout->p_sys->png_image_a[1];

        /* conversion */
        j = 0;
        for( i= 0; i < p_vout->p_sys->height * p_vout->p_sys->width ; i++)
        {
            uint8_t (*p)[4];
            int x;
            int y;
            
            x = i % p_vout->p_sys->width;
            y = i / p_vout->p_sys->width;

            /* FIXME FIXME */
            p = (void*)row_pointers[y];

            p_vout->p_sys->png_image_a[0][i]= p[x][3];        
            p_vout->p_sys->png_image[0][i]= (p[x][0] * 257
                                           + p[x][1] * 504
                                           + p[x][2] * 98)/1000 + 16;

            if( ( x % 2 == 0 ) && ( y % 2 == 0 ) )
            {
                temp = (p[x][2] * 439
                      - p[x][0] * 148
                      - p[x][1] * 291)/1000 + 128;

                temp = (uint8_t)( temp < 0 ? 0 : temp );
                p_vout->p_sys->png_image[1][j] = temp;

                temp = ( p[x][0] * 439
                       - p[x][1] * 368
                       - p[x][2] * 71)/1000 + 128;
                temp = __MAX( __MIN( temp, 255 ), 0 );

                p_vout->p_sys->png_image[2][j] = (uint8_t)( temp < 0 ? 0 : temp );
                p_vout->p_sys->png_image_a[1][j] = p_vout->p_sys->png_image_a[0][i];
                j++;
            }

        }
        /* now we can free row_pointers*/
        free(row_pointers);
    }
    
    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    /* Try to open the real video output */
    msg_Dbg( p_vout, "spawning the real video output" );

    p_vout->p_sys->p_vout = vout_Create( p_vout,
                           p_vout->render.i_width, p_vout->render.i_height,
                           p_vout->render.i_chroma, p_vout->render.i_aspect );

    /* Everything failed */
    if( p_vout->p_sys->p_vout == NULL )
    {
        msg_Err( p_vout, "can't open vout, aborting" );

        return VLC_EGENERIC;
    }

    var_AddCallback( p_vout->p_sys->p_vout, "mouse-x", MouseEvent, p_vout);
    var_AddCallback( p_vout->p_sys->p_vout, "mouse-y", MouseEvent, p_vout);


    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    ADD_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );
    
    p_vout->p_sys->posx = config_GetInt( p_vout, "logo_x" );
    p_vout->p_sys->posy = config_GetInt( p_vout, "logo_y" );
    p_vout->p_sys->trans = (int)(config_GetFloat( p_vout, "logo_transparency" ) * 255);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate logo video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the fake output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        free( PP_OUTPUTPICTURE[ i_index ]->p_data_orig );
    }

    var_DelCallback( p_vout->p_sys->p_vout, "mouse-x", MouseEvent, p_vout);
    var_DelCallback( p_vout->p_sys->p_vout, "mouse-y", MouseEvent, p_vout);

    DEL_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );
    vlc_object_detach( p_vout->p_sys->p_vout );
    vout_Destroy( p_vout->p_sys->p_vout );

    config_PutInt( p_vout, "logo_x", p_vout->p_sys->posx );
    config_PutInt( p_vout, "logo_y", p_vout->p_sys->posy );
    config_PutFloat( p_vout, "logo_transparency", (float)(p_vout->p_sys->trans) / 255.0 );

    if (p_vout->p_sys->error == 0)
    {
        free(p_vout->p_sys->png_image[0]);
        free(p_vout->p_sys->png_image[1]);
        free(p_vout->p_sys->png_image[2]);
        free(p_vout->p_sys->png_image_a[0]);
        free(p_vout->p_sys->png_image_a[1]);
    }
}

/*****************************************************************************
 * Destroy: destroy logo video thread output method
 *****************************************************************************
 * Terminate an output method created by InvertCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    free( p_vout->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Invert image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic;
    int i_index;
    int i_pic_width;
    int tr;

    /* This is a new frame. Get a structure from the video_output. */
    while( ( p_outpic = vout_CreatePicture( p_vout->p_sys->p_vout, 0, 0, 0 ) )
              == NULL )
    {
        if( p_vout->b_die || p_vout->b_error )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }

    vout_DatePicture( p_vout->p_sys->p_vout, p_outpic, p_pic->date );
    vout_LinkPicture( p_vout->p_sys->p_vout, p_outpic );

    i_pic_width=p_vout->output.i_width;
    
    tr = p_vout->p_sys->trans;
    
    for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
    {

        memcpy( p_outpic->p[i_index].p_pixels,
                p_pic->p[i_index].p_pixels, 
                p_pic->p[i_index].i_lines * p_pic->p[i_index].i_pitch);

        if (p_vout->p_sys->error == 0)
        {
            unsigned int i;
            unsigned int j;
            uint8_t *p_out, *p_in_a, *p_in;
            int i_delta;
            unsigned int i_max;
            unsigned int j_max;

            if (i_index == 0)
            {
                p_out  = p_outpic->p[i_index].p_pixels + p_vout->p_sys->posy * i_pic_width + p_vout->p_sys->posx;
                i_delta = i_pic_width - p_vout->p_sys->width;
                i_max = p_vout->p_sys->height;
                j_max = p_vout->p_sys->width;
            } else
            {
                p_out  = p_outpic->p[i_index].p_pixels + (p_vout->p_sys->posy / 2)* (i_pic_width / 2) + p_vout->p_sys->posx / 2;
                i_delta = (i_pic_width - p_vout->p_sys->width) / 2;
                i_max = p_vout->p_sys->height / 2;
                j_max = p_vout->p_sys->width / 2;
            }
            
            p_in_a = p_vout->p_sys->png_image_a[i_index];
            p_in   = p_vout->p_sys->png_image[i_index];

            
            for( i = 0; i < i_max ; i++ )
            {
                for( j = 0 ; j < j_max ; j++)
                {
                    *p_out = ( *p_out * ( 65025 - *p_in_a * tr) + *p_in * *p_in_a * tr) >> 16;

                    p_out++;
                    p_in++;
                    p_in_a++;
                }
                p_out += i_delta;
            }
         }

    }

    vout_UnlinkPicture( p_vout->p_sys->p_vout, p_outpic );

    vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
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
 ******************************************************************************/
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t*)p_data;
    vlc_value_t valb;
    int i_delta;

    #define posx p_vout->p_sys->posx
    #define posy p_vout->p_sys->posy
    #define width p_vout->p_sys->width
    #define height p_vout->p_sys->height
    #define trans p_vout->p_sys->trans
        
    var_Get( p_vout->p_sys->p_vout, "mouse-button-down", &valb );

    i_delta = newval.i_int - oldval.i_int;

    if ((valb.i_int & 0x2) == 2 && psz_var[6] == 'x' )
    {
        trans = __MIN( __MAX( trans + i_delta , 0 ) , 255 );
        return VLC_SUCCESS;
    }
    if ((valb.i_int & 0x1) == 0)
    {
        return VLC_SUCCESS;
    }

    if( psz_var[6] == 'x' )
    {
        vlc_value_t valy;
        var_Get( p_vout->p_sys->p_vout, "mouse-y", &valy );
        if ((newval.i_int >= (int)posx) && (valy.i_int >= (int)posy) && (newval.i_int <= (int)(posx + width)) && (valy.i_int <= (int)(posy + height)))
        {
            posx = __MIN( __MAX( posx + i_delta , 0 ) , p_vout->output.i_width - width );
        }
        
    }
    else if( psz_var[6] == 'y' )
    {
        vlc_value_t valx;
        var_Get( p_vout->p_sys->p_vout, "mouse-x", &valx );
        if ((valx.i_int >= (int)posx) && (newval.i_int >= (int)posy) && (valx.i_int <= (int)(posx + width)) && (newval.i_int <= (int)(posy + height)))
        {
            posy = __MIN( __MAX( posy + i_delta , 0 ) , p_vout->output.i_height - height );
        }

    }
    else if( psz_var[6] == 'c' )
    {
        if ((valb.i_int & 0x8) == 1)
        {
            p_vout->p_sys->trans++;
        }
        else if ((valb.i_int & 0x10) == 1)
        {
            p_vout->p_sys->trans--;
        }
    }

    #undef posx
    #undef posy
    #undef width
    #undef height
    #undef trans

    return VLC_SUCCESS;
}
