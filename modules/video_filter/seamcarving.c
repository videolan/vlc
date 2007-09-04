/*****************************************************************************
 * seamcarving.c: "Seam Carving for Content-Aware Image Resizing"
 * Based on paper by Shai Avidan and Ariel Shamir.
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

#include <vlc/vlc.h>
#include <vlc_sout.h>
#include <vlc_vout.h>

#include "vlc_filter.h"

#include <assert.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );
static int CropCallback( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t,
                         void * );

static void FilterSeamCarving( filter_t *, picture_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define FILTER_PREFIX "seamcarving-"

vlc_module_begin();
    set_description( _("Seam Carving video filter") );
    set_shortname( _( "Seam Carvinf" ));
    set_capability( "video filter2", 0 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );

    set_callbacks( Create, Destroy );
vlc_module_end();

static const char *ppsz_filter_options[] = {
    NULL
};

struct filter_sys_t
{
    int *p_energy;
    int *p_grad;

    int i_crop;
};

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

    p_filter->pf_video_filter = Filter;
    p_filter->p_sys->p_energy = NULL;
    p_filter->p_sys->p_grad = NULL;
    p_filter->p_sys->i_crop = 0;

    config_ChainParse( p_filter, FILTER_PREFIX, ppsz_filter_options,
                   p_filter->p_cfg );

    var_Create( p_filter, "crop", VLC_VAR_INTEGER|VLC_VAR_ISCOMMAND );
    var_AddCallback( p_filter, "crop", CropCallback, p_filter->p_sys );

    return VLC_SUCCESS;
}

static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    free( p_filter->p_sys->p_energy );
    free( p_filter->p_sys->p_grad );

    free( p_filter->p_sys );
}

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

    FilterSeamCarving( p_filter, p_pic, p_outpic );

    p_outpic->date = p_pic->date;
    p_outpic->b_force = p_pic->b_force;
    p_outpic->i_nb_fields = p_pic->i_nb_fields;
    p_outpic->b_progressive = p_pic->b_progressive;
    p_outpic->b_top_field_first = p_pic->b_top_field_first;

    if( p_pic->pf_release )
        p_pic->pf_release( p_pic );

    return p_outpic;
}

static inline int min3( int a, int b, int c );
static inline int min( int a, int b );
static int RemoveVerticalSeam( filter_t *p_filter, picture_t *p_inpic, picture_t *p_outpic, int i_src_visible );

//#define DRAW_GRADIENT
//#define DRAW_ENERGY
//#define DRAW_SEAM

static void FilterSeamCarving( filter_t *p_filter, picture_t *p_inpic,
                                                picture_t *p_outpic )
{
    const int i_src_pitch = p_inpic->p[Y_PLANE].i_pitch;
    const int i_num_lines = p_inpic->p[Y_PLANE].i_visible_lines;

    int i_src_visible = p_inpic->p[Y_PLANE].i_visible_pitch;

    if( !p_filter->p_sys->p_energy )
        p_filter->p_sys->p_energy = (int*)malloc(i_src_pitch * i_num_lines * sizeof(int));
    if( !p_filter->p_sys->p_grad )
        p_filter->p_sys->p_grad = (int*)malloc(i_src_pitch * i_num_lines * sizeof(int));

//#if defined( DRAW_GRADIENT ) || defined( DRAW_ENERGY ) || defined( DRAW_SEAM )
    p_filter->p_libvlc->pf_memcpy( p_outpic->p[Y_PLANE].p_pixels,
                                   p_inpic->p[Y_PLANE].p_pixels,
        p_outpic->p[Y_PLANE].i_lines * p_outpic->p[Y_PLANE].i_pitch );
//#else
//    p_filter->p_libvlc->pf_memset( p_outpix, 0x80,
//        p_outpic->p[Y_PLANE].i_lines * p_outpic->p[Y_PLANE].i_pitch );
//#endif
    p_filter->p_libvlc->pf_memset( p_outpic->p[U_PLANE].p_pixels, 0x80,
        p_outpic->p[U_PLANE].i_lines * p_outpic->p[U_PLANE].i_pitch );
    p_filter->p_libvlc->pf_memset( p_outpic->p[V_PLANE].p_pixels, 0x80,
        p_outpic->p[V_PLANE].i_lines * p_outpic->p[V_PLANE].i_pitch );

#if defined( DRAW_GRADIENT ) || defined( DRAW_ENERGY ) || defined( DRAW_SEAM )
    i_src_visible = RemoveVerticalSeam( p_filter, p_outpic, p_outpic, i_src_visible );
#else
    static int j = 1;
    static int k = 1;
    int i;
    if( p_filter->p_sys->i_crop != 0 )
        j = p_filter->p_sys->i_crop;
    for( i = 0; i < j; i++ )
    i_src_visible = RemoveVerticalSeam( p_filter, p_outpic, p_outpic, i_src_visible );
    int y;
    for( y = 0; y < p_outpic->p[Y_PLANE].i_lines; y++ )
        p_filter->p_libvlc->pf_memset( p_outpic->p[Y_PLANE].p_pixels + y*p_outpic->p[Y_PLANE].i_pitch + i_src_visible, 0x00, p_outpic->p[Y_PLANE].i_pitch - i_src_visible );
    j += k;
    if( j == 100 ) k = -1;
    if( j == 1 ) k = 1;
#endif
}

static int ComputeGradient( filter_t *p_filter, picture_t *p_inpic, int i_src_visible )
{
    int x, y;
    const int i_src_pitch = p_inpic->p[Y_PLANE].i_pitch;
    const int i_num_lines = p_inpic->p[Y_PLANE].i_visible_lines;

    const uint8_t *p_inpix = p_inpic->p[Y_PLANE].p_pixels;
    int *p_grad = p_filter->p_sys->p_grad;

    for( y = 1; y < i_num_lines - 1;
         y++, p_grad += i_src_pitch )
    {
        /* Compute line y's gradient */
#define GRADx( x ) \
            ( \
              abs( \
                 ( p_inpix[(y-1)*i_src_pitch+x-1] \
                   - p_inpix[(y+1)*i_src_pitch+x-1] ) \
               + ( ( p_inpix[(y-1)*i_src_pitch+x] \
                    - p_inpix[(y+1)*i_src_pitch+x] ) <<1 ) \
               + ( p_inpix[(y-1)*i_src_pitch+x+1] \
                   - p_inpix[(y+1)*i_src_pitch+x+1] ) \
              ) \
            + \
              abs( \
                 ( p_inpix[(y-1)*i_src_pitch+x-1] \
                   - p_inpix[(y-1)*i_src_pitch+x+1] ) \
               + ( ( p_inpix[y*i_src_pitch+x-1] \
                    - p_inpix[y*i_src_pitch+x+1] ) <<1 ) \
               + ( p_inpix[(y+1)*i_src_pitch+x-1] \
                   - p_inpix[(y+1)*i_src_pitch+x+1] ) \
              ) \
            )
        for( x = 1; x < i_src_visible - 1; x++ )
        {
            p_grad[x] = GRADx( x );
#ifdef DRAW_GRADIENT
            p_outpix[y*i_src_pitch+x] = p_grad[x]>>3;
#endif
        }
    }
    return 0;
}

static int RemoveVerticalSeam( filter_t *p_filter, picture_t *p_inpic, picture_t *p_outpic, int i_src_visible )
{
    int x, y;
    const int i_src_pitch = p_inpic->p[Y_PLANE].i_pitch;
    const int i_num_lines = p_inpic->p[Y_PLANE].i_visible_lines;

    const uint8_t *p_inpix = p_inpic->p[Y_PLANE].p_pixels;
    uint8_t *p_outpix = p_outpic->p[Y_PLANE].p_pixels;

    int *p_energy = p_filter->p_sys->p_energy;
    int *p_grad = p_filter->p_sys->p_grad;

    ComputeGradient( p_filter, p_inpic, i_src_visible );

    /** Compute the image's energy (using a sobel gradient as the base energy
     ** function) */
    /* Set the first energy line to 0 */
    memset( p_energy, 0, i_src_pitch*sizeof(int));

    int *p_energy_prev = p_energy;
    for( y = 1; y < i_num_lines - 1;
         y++, p_energy_prev = p_energy, p_energy += i_src_pitch,
         p_grad += i_src_pitch )
    {
        /* Compute line y's minimum energy value for paths ending on
         * each x */
        x = 1;
        p_energy[x] = min( p_energy_prev[x  ]+p_grad[x  ],
                           p_energy_prev[x+1]+p_grad[x+1] );
        for( x = 2; x < i_src_visible - 2; x++ )
        {
            p_energy[x] = min3( p_energy_prev[x-1]+p_grad[x-1],
                                p_energy_prev[x  ]+p_grad[x  ],
                                p_energy_prev[x+1]+p_grad[x+1] );
        }
        p_energy[x] = min( p_energy_prev[x-1]+p_grad[x-1],
                           p_energy_prev[x  ]+p_grad[x  ] );

#ifdef DRAW_ENERGY
        int max = p_energy[1];
        for( x = 1; x < i_src_visible - 1; x++ )
            if( p_energy[x] > max ) max = p_energy[x];
        for( x = 1; x < i_src_visible - 1; x++ )
            p_outpix[y*i_src_pitch+x] = p_energy[x]*0xff/max;
#endif
    }

    /* Find the minimum energy point on the last line */
    y--;
    p_energy -= i_src_pitch;
    p_grad -= i_src_pitch;

    int m = p_energy[1];
    int xmin = 1;
    for( x = 1; x < i_src_visible - 1; x++ )
    {
        if( p_energy[x] < m )
        {
            m = p_energy[x];
            xmin = x;
        }
    }

#ifdef DRAW_SEAM
    p_outpix[y*i_src_pitch+xmin] = 0xff;
    p_outpix[(y+1)*i_src_pitch+xmin] = 0xff;
#else
    memmove( p_outpix+y*i_src_pitch+xmin, p_outpix+y*i_src_pitch+xmin+1, i_src_pitch-(xmin+1) );
    memmove( p_outpix+(y+1)*i_src_pitch+xmin, p_outpix+(y+1)*i_src_pitch+xmin+1, i_src_pitch-(xmin+1) );
#endif

    p_energy -= i_src_pitch;
    for( ; y>1; y--, p_energy -= i_src_pitch, p_grad -= i_src_pitch )
    {
        if( m != p_energy[xmin]+p_grad[xmin] )
        {
            if( xmin > 1 && m == p_energy[xmin-1]+p_grad[xmin-1] )
            {
                xmin--;
            }
            else if( xmin < i_src_visible - 2 && m == p_energy[xmin+1]+p_grad[xmin+1] )
            {
                xmin++;
            }
            else
            {
                printf("Alarm! %d\n" ,y);
                //assert( 0 );
            }
        }
        m = p_energy[xmin];
#ifdef DRAW_SEAM
        p_outpix[y*i_src_pitch+xmin] = 0xff;
#else
        memmove( p_outpix+y*i_src_pitch+xmin, p_outpix+y*i_src_pitch+xmin+1, i_src_pitch-(xmin+1) );
#endif
    }
#ifdef DRAW_SEAM
    p_outpix[y*i_src_pitch+xmin] = 0xff;
#else
    memmove( p_outpix+y*i_src_pitch+xmin, p_outpix+y*i_src_pitch+xmin+1, i_src_pitch-(xmin+1) );
#endif
    y--;
#ifdef DRAW_SEAM
    p_outpix[y*i_src_pitch+xmin] = 0xff;
#else
    memmove( p_outpix+y*i_src_pitch+xmin, p_outpix+y*i_src_pitch+xmin+1, i_src_pitch-(xmin+1) );
#endif

#if defined( DRAW_SEAM )
    return i_src_visible;
#else
    return i_src_visible-1;
#endif
}

static inline int min3( int a, int b, int c )
{
    if( a < b )
    {
        if( a < c ) return a;
        return c;
    }
    if( b < c ) return b;
    return c;
}
static inline int min( int a, int b )
{
    return a < b ? a : b;
}

static int CropCallback( vlc_object_t *p_this, char const *psz_var,
                                vlc_value_t oldval, vlc_value_t newval,
                                void *p_data )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_data;
    p_sys->i_crop = newval.i_int;
    return VLC_SUCCESS;
}
