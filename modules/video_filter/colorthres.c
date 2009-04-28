/*****************************************************************************
 * colorthres.c: Theshold color based on similarity to reference color
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal <dnumgis@videolan.org>
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

#include <errno.h>
#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_vout.h>

#include "vlc_filter.h"
#include "filter_picture.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define COLOR_TEXT N_("Color")
#define COLOR_LONGTEXT N_("Colors similar to this will be kept, others will be "\
    "grayscaled. This must be an hexadecimal (like HTML colors). The first two "\
    "chars are for red, then green, then blue. #000000 = black, #FF0000 = red,"\
    " #00FF00 = green, #FFFF00 = yellow (red + green), #FFFFFF = white" )
static const int pi_color_values[] = {
  0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x0000FF00, 0x000000FF, 0x0000FFFF };

static const char *const ppsz_color_descriptions[] = {
  N_("Red"), N_("Fuchsia"), N_("Yellow"), N_("Lime"), N_("Blue"), N_("Aqua") };

#define CFG_PREFIX "colorthres-"

vlc_module_begin ()
    set_description( N_("Color threshold filter") )
    set_shortname( N_("Color threshold" ))
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter2", 0 )
    add_integer( CFG_PREFIX "color", 0x00FF0000, NULL, COLOR_TEXT,
                 COLOR_LONGTEXT, false )
        change_integer_list( pi_color_values, ppsz_color_descriptions, NULL )
    add_integer( CFG_PREFIX "saturationthres", 20, NULL,
                 N_("Saturaton threshold"), "", false )
    add_integer( CFG_PREFIX "similaritythres", 15, NULL,
                 N_("Similarity threshold"), "", false )
    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "color", "saturationthes", "similaritythres", NULL
};

/*****************************************************************************
 * filter_sys_t: adjust filter method descriptor
 *****************************************************************************/
struct filter_sys_t
{
};

/*****************************************************************************
 * Create: allocates adjust video thread output method
 *****************************************************************************
 * This function allocates and initializes a adjust vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    switch( p_filter->fmt_in.video.i_chroma )
    {
        CASE_PLANAR_YUV
            break;

        default:
            msg_Err( p_filter, "Unsupported input chroma (%4s)",
                     (char*)&(p_filter->fmt_in.video.i_chroma) );
            return VLC_EGENERIC;
    }

    if( p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma )
    {
        msg_Err( p_filter, "Input and output chromas don't match" );
        return VLC_EGENERIC;
    }

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );
    var_Create( p_filter, CFG_PREFIX "color", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );
    var_Create( p_filter, CFG_PREFIX "similaritythres", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );
    var_Create( p_filter, CFG_PREFIX "saturationthres", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    p_filter->pf_video_filter = Filter;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy adjust video thread output method
 *****************************************************************************
 * Terminate an output method created by adjustCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    free( p_filter->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to adjust modified image,
 * waits until it is displayed and switch the two rendering buffers, preparing
 * next frame.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    uint8_t *p_in_y, *p_in_u, *p_in_v, *p_in_end_u;
    uint8_t *p_out_y, *p_out_u, *p_out_v;
    int i_simthres = var_GetInteger( p_filter, "colorthres-similaritythres" );
    int i_satthres = var_GetInteger( p_filter, "colorthres-saturationthres" );
    int i_color = var_GetInteger( p_filter, "colorthres-color" );

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    p_in_u = p_pic->p[U_PLANE].p_pixels;
    p_in_v = p_pic->p[V_PLANE].p_pixels;
    p_in_y = p_pic->p[Y_PLANE].p_pixels;
    p_in_end_u = p_in_u + p_pic->p[U_PLANE].i_visible_lines
                        * p_pic->p[U_PLANE].i_pitch - 8;

    p_out_y = p_outpic->p[Y_PLANE].p_pixels;
    p_out_u = p_outpic->p[U_PLANE].p_pixels;
    p_out_v = p_outpic->p[V_PLANE].p_pixels;

    /* Create grayscale version of input */
    vlc_memcpy( p_out_y, p_in_y, p_pic->p[Y_PLANE].i_visible_lines
               * p_pic->p[Y_PLANE].i_pitch - 8 );
    vlc_memset( p_out_u, 0x80, p_pic->p[U_PLANE].i_visible_lines
               * p_pic->p[U_PLANE].i_pitch - 8 );
    vlc_memset( p_out_v, 0x80, p_pic->p[U_PLANE].i_visible_lines
               * p_pic->p[U_PLANE].i_pitch - 8 );

    /*
     * Do the U and V planes
     */
    int i_red = ( i_color & 0xFF0000 ) >> 16;
    int i_green = ( i_color & 0xFF00 ) >> 8;
    int i_blue = i_color & 0xFF;
    int i_u = (int8_t)(( -38 * i_red - 74 * i_green +
                     112 * i_blue + 128) >> 8) + 128;
    int i_v = (int8_t)(( 112 * i_red  -  94 * i_green -
                      18 * i_blue + 128) >> 8) + 128;
    int refu = i_u - 0x80;         /*bright red*/
    int refv = i_v - 0x80;
    int reflength = sqrt(refu*refu+refv*refv);

    while( p_in_u < p_in_end_u ) {
        /* Length of color vector */
        int inu = (*p_in_u) - 0x80;
        int inv = (*p_in_v) - 0x80;
        int length = sqrt(inu*inu+inv*inv);

        int diffu = refu * length - inu *reflength;
        int diffv = refv * length - inv *reflength;
        long long int difflen2=diffu*diffu;
        difflen2 +=diffv*diffv;
        long long int thres = length*reflength;
        thres *= thres;
        if( length > i_satthres && (difflen2*i_simthres< thres ) ) {
            *p_out_u = *p_in_u;
            *p_out_v = *p_in_v;
//        fprintf(stderr,"keeping color %d %d\n", length, difflen2);
        }
        p_in_u++;
        p_in_v++;
        p_out_u++;
        p_out_v++;
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}
