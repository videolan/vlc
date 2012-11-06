/*****************************************************************************
 * ball.c : Augmented reality ball video filter module
 *****************************************************************************
 * Copyright (C) 2000-2009 VLC authors and VideoLAN
 *
 * Author: Adrien Maglo <magsoft@videolan.org>
 *
 * The Canny edge detection algorithm comes from gradient.c which was
 * writen by:
 *         Samuel Hocevar <sam@zoy.org>
 *         Antoine Cellerier <dionoea -at- videolan -dot- org>
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

#include <math.h> /* sin(), cos(), asin() */

#include <vlc_common.h>
#include <vlc_plugin.h>

#include "vlc_filter.h"
#include "filter_picture.h"
#include "vlc_image.h"

enum { RED, GREEN, BLUE, WHITE };

#define COLORS_RGB \
    p_filter->p_sys->colorList[RED].comp1 = 255; p_filter->p_sys->colorList[RED].comp2 = 0;        \
                                p_filter->p_sys->colorList[RED].comp3 = 0;        \
    p_filter->p_sys->colorList[GREEN].comp1 = 0; p_filter->p_sys->colorList[GREEN].comp2 = 255;    \
                               p_filter->p_sys->colorList[GREEN].comp3 = 0;       \
    p_filter->p_sys->colorList[BLUE].comp1 = 0; p_filter->p_sys->colorList[BLUE].comp2 = 0;        \
                               p_filter->p_sys->colorList[BLUE].comp3 = 255;      \
    p_filter->p_sys->colorList[WHITE].comp1 = 255; p_filter->p_sys->colorList[WHITE].comp2 = 255;  \
                                  p_filter->p_sys->colorList[WHITE].comp3 = 255;

#define COLORS_YUV \
    p_filter->p_sys->colorList[RED].comp1 = 82; p_filter->p_sys->colorList[RED].comp2 = 240;        \
                                p_filter->p_sys->colorList[RED].comp3 = 90;        \
    p_filter->p_sys->colorList[GREEN].comp1 = 145; p_filter->p_sys->colorList[GREEN].comp2 = 34;    \
                               p_filter->p_sys->colorList[GREEN].comp3 = 54 ;      \
    p_filter->p_sys->colorList[BLUE].comp1 = 41; p_filter->p_sys->colorList[BLUE].comp2 = 146;      \
                               p_filter->p_sys->colorList[BLUE].comp3 = 240;       \
    p_filter->p_sys->colorList[WHITE].comp1 = 255; p_filter->p_sys->colorList[WHITE].comp2 = 128;   \
                                  p_filter->p_sys->colorList[WHITE].comp3 = 128;


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

static void drawBall( filter_sys_t *p_sys, picture_t *p_outpic );
static void drawPixelRGB24( filter_sys_t *p_sys, picture_t *p_outpic,
                            uint8_t R, uint8_t G, uint8_t B,
                            int x, int y, bool b_skip );
static void drawPixelI420( filter_sys_t *p_sys, picture_t *p_outpic,
                           uint8_t Y, uint8_t U, uint8_t V,
                           int x, int y, bool b_skip );
static void drawPixelPacked( filter_sys_t *p_sys, picture_t *p_outpic,
                             uint8_t Y, uint8_t U, uint8_t V,
                             int x, int y, bool b_skip );

static void FilterBall( filter_t *, picture_t *, picture_t * );
static int ballCallback( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t,
                         void * );
static int getBallColor( vlc_object_t *p_this, char const *psz_newval );


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define BALL_COLOR_TEXT N_("Ball color")

#define EDGE_VISIBLE_TEXT N_("Edge visible")
#define EDGE_VISIBLE_LONGTEXT N_("Set edge visibility.")

#define BALL_SPEED_TEXT N_("Ball speed")
#define BALL_SPEED_LONGTEXT N_("Set ball speed, the displacement value \
                                in number of pixels by frame.")

#define BALL_SIZE_TEXT N_("Ball size")
#define BALL_SIZE_LONGTEXT N_("Set ball size giving its radius in number \
                                of pixels")

#define GRAD_THRESH_TEXT N_("Gradient threshold")
#define GRAD_THRESH_LONGTEXT N_("Set gradient threshold for edge computation.")

#define BALL_HELP N_("Augmented reality ball game")

#define FILTER_PREFIX "ball-"

static const char *const mode_list[] = { "red", "green", "blue", "white" };
static const char *const mode_list_text[] = { N_("Red"), N_("Green"),
                                              N_("Blue"), N_("White") };

vlc_module_begin ()
    set_description( N_("Ball video filter") )
    set_shortname( N_( "Ball" ))
    set_help(BALL_HELP)
    set_capability( "video filter2", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_string( FILTER_PREFIX "color", "red",
                BALL_COLOR_TEXT, BALL_COLOR_TEXT, false )
    change_string_list( mode_list, mode_list_text )

    add_integer_with_range( FILTER_PREFIX "speed", 4, 1, 15,
                            BALL_SPEED_TEXT, BALL_SPEED_LONGTEXT, false )

    add_integer_with_range( FILTER_PREFIX "size", 10, 5, 30,
                            BALL_SIZE_TEXT, BALL_SIZE_LONGTEXT, false )

    add_integer_with_range( FILTER_PREFIX "gradient-threshold", 40, 1, 200,
                            GRAD_THRESH_TEXT, GRAD_THRESH_LONGTEXT, false )

    add_bool( FILTER_PREFIX "edge-visible", true,
              EDGE_VISIBLE_TEXT, EDGE_VISIBLE_LONGTEXT, true )

    add_shortcut( "ball" )
    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "color", "speed", "size",
    "gradient-threshold", "edge-visible", NULL
};


/*****************************************************************************
* filter_sys_t: Distort video output method descriptor
*****************************************************************************
* This structure is part of the video output thread descriptor.
* It describes the Distort specific properties of an output thread.
 *****************************************************************************/
struct filter_sys_t
{
    vlc_mutex_t lock;

    int ballColor;

    image_handler_t *p_image;

    /* Ball position */
    int i_ball_x;
    int i_ball_y;

    int i_ballSpeed;

    int i_ballSize;

    bool b_edgeVisible;

    /* Offsets for YUV packed chroma */
    int i_y_offset;
    int i_u_offset;
    int i_v_offset;

    /* Gradient values */
    uint32_t *p_smooth;
    int32_t *p_grad_x;
    int32_t *p_grad_y;

    /* Gradient threshold */
    int i_gradThresh;

    /* Motion vectors */
    float f_lastVect_x;
    float f_lastVect_y;

    float f_newVect_x;
    float f_newVect_y;

    float f_contVect_x;
    float f_contVect_y;

    /* Pointer on drawing function */
    void ( *drawingPixelFunction )( filter_sys_t *, picture_t *,
                                    uint8_t, uint8_t, uint8_t,
                                    int, int, bool );
    struct
    {
        uint8_t comp1;
        uint8_t comp2;
        uint8_t comp3;
    } colorList[4];
};


/*****************************************************************************
* Create: allocates Distort video thread output method
*****************************************************************************
* This function allocates and initializes a Distort vout method.
*****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    char *psz_method;

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    switch( p_filter->fmt_in.video.i_chroma )
    {
        case VLC_CODEC_I420:
        case VLC_CODEC_J420:
            p_filter->p_sys->drawingPixelFunction = drawPixelI420;
            COLORS_YUV
            break;
        CASE_PACKED_YUV_422
            p_filter->p_sys->drawingPixelFunction = drawPixelPacked;
            COLORS_YUV
            GetPackedYuvOffsets( p_filter->fmt_in.video.i_chroma,
                                 &p_filter->p_sys->i_y_offset,
                                 &p_filter->p_sys->i_u_offset,
                                 &p_filter->p_sys->i_v_offset );
            break;
        case VLC_CODEC_RGB24:
            p_filter->p_sys->drawingPixelFunction = drawPixelRGB24;
            COLORS_RGB
            break;
        default:
            msg_Err( p_filter, "Unsupported input chroma (%4.4s)",
                     (char*)&(p_filter->fmt_in.video.i_chroma) );
            return VLC_EGENERIC;
    }

    p_filter->p_sys->p_image = image_HandlerCreate( p_filter );
    if( p_filter->p_sys->p_image == NULL )
        return VLC_EGENERIC;

    p_filter->pf_video_filter = Filter;

    config_ChainParse( p_filter, FILTER_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    if( !(psz_method =
        var_CreateGetNonEmptyStringCommand( p_filter,
                                            FILTER_PREFIX "color" ) ) )
    {
        msg_Err( p_filter, "configuration variable "
                 FILTER_PREFIX "color empty" );
        p_filter->p_sys->ballColor = RED;
    }
    else
        p_filter->p_sys->ballColor = getBallColor( p_this, psz_method );

    free( psz_method );

    p_filter->p_sys->i_ballSize =
            var_CreateGetIntegerCommand( p_filter, FILTER_PREFIX "size" );
    p_filter->p_sys->i_ballSpeed =
            var_CreateGetIntegerCommand( p_filter, FILTER_PREFIX "speed" );
    p_filter->p_sys->b_edgeVisible =
            var_CreateGetBoolCommand( p_filter, FILTER_PREFIX "edge-visible" );
    p_filter->p_sys->i_gradThresh =
            var_CreateGetIntegerCommand( p_filter, FILTER_PREFIX "gradient-threshold" );

    vlc_mutex_init( &p_filter->p_sys->lock );

    var_AddCallback( p_filter, FILTER_PREFIX "color",
                     ballCallback, p_filter->p_sys );
    var_AddCallback( p_filter, FILTER_PREFIX "size",
                     ballCallback, p_filter->p_sys );
    var_AddCallback( p_filter, FILTER_PREFIX "speed",
                     ballCallback, p_filter->p_sys );
    var_AddCallback( p_filter, FILTER_PREFIX "edge-visible",
                     ballCallback, p_filter->p_sys );

    p_filter->p_sys->p_smooth = NULL;
    p_filter->p_sys->p_grad_x = NULL;
    p_filter->p_sys->p_grad_y = NULL;

    p_filter->p_sys->i_ball_x = 100;
    p_filter->p_sys->i_ball_y = 100;

    p_filter->p_sys->f_lastVect_x = 0;
    p_filter->p_sys->f_lastVect_y = -1;

    return VLC_SUCCESS;
}


/*****************************************************************************
* Destroy: destroy Distort video thread output method
*****************************************************************************
* Terminate an output method created by DistortCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    var_DelCallback( p_filter, FILTER_PREFIX "color",
                     ballCallback, p_sys );
    var_DelCallback( p_filter, FILTER_PREFIX "size",
                     ballCallback, p_sys );
    var_DelCallback( p_filter, FILTER_PREFIX "speed",
                     ballCallback, p_sys );
    var_DelCallback( p_filter, FILTER_PREFIX "edge-visible",
                     ballCallback, p_sys );

    vlc_mutex_destroy( &p_sys->lock );

    image_HandlerDelete( p_sys->p_image );

    free( p_sys->p_smooth );
    free( p_sys->p_grad_x );
    free( p_sys->p_grad_y );

    free( p_sys );
}


/*****************************************************************************
* Render: displays previously rendered output
*****************************************************************************
* This function send the currently rendered image to Distort image, waits
* until it is displayed and switch the two rendering buffers, preparing next
* frame.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    vlc_mutex_lock( &p_filter->p_sys->lock );
    FilterBall( p_filter, p_pic, p_outpic );
    vlc_mutex_unlock( &p_filter->p_sys->lock );

    return CopyInfoAndRelease( p_outpic, p_pic );
}


/*****************************************************************************
* Drawing functions
*****************************************************************************/

static void drawBall( filter_sys_t *p_sys, picture_t *p_outpic )
{
    int x = p_sys->i_ball_x;
    int y = p_sys->i_ball_y;
    int size = p_sys->i_ballSize;

    const int i_width = p_outpic->p[0].i_visible_pitch;
    const int i_height = p_outpic->p[0].i_visible_lines;

    for( int j = y - size; j <= y + size; j++ )
    {
        bool b_skip = ( x - size ) % 2;
        for( int i = x - size; i <= x + size; i++ )
        {
            /* Draw the pixel if it is inside the disk
               and check we don't write out the frame. */
            if( ( i - x ) * ( i - x ) + ( j - y ) * ( j - y ) <= size * size
                && i >= 0 && i < i_width
                && j >= 0 && j < i_height )
            {
                ( *p_sys->drawingPixelFunction )( p_sys, p_outpic,
                                    p_sys->colorList[ p_sys->ballColor ].comp1,
                                    p_sys->colorList[ p_sys->ballColor ].comp2,
                                    p_sys->colorList[ p_sys->ballColor ].comp3,
                                    i, j, b_skip );
            }
            b_skip = !b_skip;
        }
    }
}


static void drawPixelRGB24( filter_sys_t *p_sys, picture_t *p_outpic,
                            uint8_t R, uint8_t G, uint8_t B,
                            int x, int y, bool b_skip )
{
    VLC_UNUSED( p_sys );
    VLC_UNUSED( b_skip );
    uint8_t *p_pixel = p_outpic->p[0].p_pixels
                       + p_outpic->p[0].i_pitch
                       * x + 3 * y;
    *p_pixel = B;
    *++p_pixel = G;
    *++p_pixel = R;
}


static void drawPixelI420( filter_sys_t *p_sys, picture_t *p_outpic,
                           uint8_t Y, uint8_t U, uint8_t V,
                           int x, int y, bool b_skip )
{
    VLC_UNUSED( p_sys );
    *( p_outpic->p[0].p_pixels + p_outpic->p[0].i_pitch * y + x ) = Y;
    if( !b_skip )
    {
        *( p_outpic->p[2].p_pixels + p_outpic->p[2].i_pitch
                                     * ( y / 2 ) + x / 2 ) = U;
        *( p_outpic->p[1].p_pixels + p_outpic->p[1].i_pitch
                                     * ( y / 2 ) + x / 2 ) = V;
    }
}


static void drawPixelPacked( filter_sys_t *p_sys, picture_t *p_outpic,
                           uint8_t Y, uint8_t U, uint8_t V,
                           int x, int y, bool b_skip )
{
    uint8_t *p_pixel = p_outpic->p[0].p_pixels
                       + p_outpic->p[0].i_pitch * y + x * 2;
    *( p_pixel + p_sys->i_y_offset ) = Y;
    if( !b_skip )
    {
        *( p_pixel + p_sys->i_u_offset ) = U;
        *( p_pixel + p_sys->i_v_offset ) = V;
    }
}


/*****************************************************************************
* Nomalize vector
*****************************************************************************
* Modify its value to set its norm to 1 and keep its direction.
 *****************************************************************************/
static void NormalizeVector( float *vect_x, float *vect_y )
{
    float norm = sqrt( *vect_x * *vect_x + *vect_y * *vect_y );
    if( *vect_x != 0 || *vect_y != 0 )
    {
        *vect_x /= norm;
        *vect_y /= norm;
    }
}


/*****************************************************************************
* Gaussian Convolution
*****************************************************************************
*    Gaussian convolution ( sigma == 1.4 )
*
*    |  2  4  5  4  2  |   |  2  4  4  4  2 |
*    |  4  9 12  9  4  |   |  4  8 12  8  4 |
*    |  5 12 15 12  5  | ~ |  4 12 16 12  4 |
*    |  4  9 12  9  4  |   |  4  8 12  8  4 |
*    |  2  4  5  4  2  |   |  2  4  4  4  2 |
 *****************************************************************************/
static void GaussianConvolution( picture_t *p_inpic, uint32_t *p_smooth )
{
    const uint8_t *p_inpix = p_inpic->p[Y_PLANE].p_pixels;
    const int i_src_pitch = p_inpic->p[Y_PLANE].i_pitch;
    const int i_src_visible = p_inpic->p[Y_PLANE].i_visible_pitch;
    const int i_numLines = p_inpic->p[Y_PLANE].i_visible_lines;

    int x,y;
    for( y = 2; y < i_numLines - 2; y++ )
    {
        for( x = 2; x < i_src_visible - 2; x++ )
        {
            p_smooth[y*i_src_visible+x] = (uint32_t)(
                    /* 2 rows up */
                    ( p_inpix[(y-2)*i_src_pitch+x-2] )
                    + ((p_inpix[(y-2)*i_src_pitch+x-1]
                    +   p_inpix[(y-2)*i_src_pitch+x]
                    +   p_inpix[(y-2)*i_src_pitch+x+1])<<1 )
                    + ( p_inpix[(y-2)*i_src_pitch+x+2] )
                    /* 1 row up */
                    + ((p_inpix[(y-1)*i_src_pitch+x-2]
                    + ( p_inpix[(y-1)*i_src_pitch+x-1]<<1 )
                    + ( p_inpix[(y-1)*i_src_pitch+x]*3 )
                    + ( p_inpix[(y-1)*i_src_pitch+x+1]<<1 )
                    +   p_inpix[(y-1)*i_src_pitch+x+2]
                    /* */
                    +   p_inpix[y*i_src_pitch+x-2]
                    + ( p_inpix[y*i_src_pitch+x-1]*3 )
                    + ( p_inpix[y*i_src_pitch+x]<<2 )
                    + ( p_inpix[y*i_src_pitch+x+1]*3 )
                    +   p_inpix[y*i_src_pitch+x+2]
                    /* 1 row down */
                    +   p_inpix[(y+1)*i_src_pitch+x-2]
                    + ( p_inpix[(y+1)*i_src_pitch+x-1]<<1 )
                    + ( p_inpix[(y+1)*i_src_pitch+x]*3 )
                    + ( p_inpix[(y+1)*i_src_pitch+x+1]<<1 )
                    +   p_inpix[(y+1)*i_src_pitch+x+2] )<<1 )
                    /* 2 rows down */
                    + ( p_inpix[(y+2)*i_src_pitch+x-2] )
                    + ((p_inpix[(y+2)*i_src_pitch+x-1]
                    +   p_inpix[(y+2)*i_src_pitch+x]
                    +   p_inpix[(y+2)*i_src_pitch+x+1])<<1 )
                    + ( p_inpix[(y+2)*i_src_pitch+x+2] )
                                                    ) >> 6 /* 115 */;
        }
    }
}


/*****************************************************************************
 * FilterBall: Augmented reality ball video filter
 *****************************************************************************
 * The edge detection part comes from gradient.c video filter module.
 * The Canny edge detection algorithm is used :
 * http://fourier.eng.hmc.edu/e161/lectures/canny/node1.html
 * (well ... the implementation isn't really the canny algorithm ... but some
 * ideas are the same)
 *****************************************************************************/
static void FilterBall( filter_t *p_filter, picture_t *p_inpic,
                           picture_t *p_outpic )
{
    int x, y;
    filter_sys_t *p_sys = p_filter->p_sys;

    uint32_t *p_smooth;
    int32_t *p_grad_x;
    int32_t *p_grad_y;

    picture_t *p_converted;
    video_format_t fmt_comp;
    memset( &fmt_comp, 0, sizeof(fmt_comp) );

    switch( p_filter->fmt_in.video.i_chroma )
    {
        case VLC_CODEC_RGB24:
        CASE_PACKED_YUV_422
            fmt_comp.i_width = p_filter->fmt_in.video.i_width;
            fmt_comp.i_height = p_filter->fmt_in.video.i_height;
            fmt_comp.i_chroma = VLC_FOURCC('G','R','E','Y');
            fmt_comp.i_visible_width = fmt_comp.i_width;
            fmt_comp.i_visible_height = fmt_comp.i_height;

            p_converted = image_Convert( p_filter->p_sys->p_image, p_inpic,
                                         &(p_filter->fmt_in.video),
                                         &fmt_comp );
            if( !p_converted )
                return;

            break;

        default:
            p_converted = p_inpic;
            break;
    }

    const int i_numCols = p_converted->p[0].i_visible_pitch;
    const int i_numLines = p_converted->p[0].i_visible_lines;

    if( !p_filter->p_sys->p_smooth )
        p_filter->p_sys->p_smooth =
                (uint32_t *)malloc( i_numLines * i_numCols
                                    * sizeof(uint32_t));
    p_smooth = p_filter->p_sys->p_smooth;

    if( !p_filter->p_sys->p_grad_x )
        p_filter->p_sys->p_grad_x =
                (int32_t *)malloc( i_numLines * i_numCols
                                   * sizeof(int32_t));
    p_grad_x = p_filter->p_sys->p_grad_x;

    if( !p_filter->p_sys->p_grad_y )
        p_filter->p_sys->p_grad_y =
                (int32_t *)malloc( i_numLines * i_numCols
                                   * sizeof(int32_t));
    p_grad_y = p_filter->p_sys->p_grad_y;

    if( !p_smooth || !p_grad_x || !p_grad_y ) return;

    memcpy( p_outpic->p[0].p_pixels, p_inpic->p[0].p_pixels,
                p_outpic->p[0].i_lines * p_outpic->p[0].i_pitch );
    memcpy( p_outpic->p[1].p_pixels, p_inpic->p[1].p_pixels,
                p_outpic->p[1].i_lines * p_outpic->p[1].i_pitch );
    memcpy( p_outpic->p[2].p_pixels, p_inpic->p[2].p_pixels,
                p_outpic->p[2].i_lines * p_outpic->p[2].i_pitch );

    GaussianConvolution( p_converted, p_smooth );

    /* Compute the picture Sobel gradient
       | -1 0 1 |     |  1  2  1 |
       | -2 0 2 | and |  0  0  0 |
       | -1 0 1 |     | -1 -2 -1 | */

    for( y = 1; y < i_numLines - 1; y++ )
    {
        for( x = 1; x < i_numCols - 1; x++ )
        {

            p_grad_x[ y * i_numCols + x ] =
                    ( p_smooth[(y-1)*i_numCols+x-1]
                    - p_smooth[(y+1)*i_numCols+x-1] )
                    + ( ( p_smooth[(y-1)*i_numCols+x]
                    - p_smooth[(y+1)*i_numCols+x] ) <<1 )
                    + ( p_smooth[(y-1)*i_numCols+x+1]
                    - p_smooth[(y+1)*i_numCols+x+1] );
            p_grad_y[ y * i_numCols + x ] =
                    ( p_smooth[(y-1)*i_numCols+x-1]
                    - p_smooth[(y-1)*i_numCols+x+1] )
                    + ( ( p_smooth[y*i_numCols+x-1]
                    - p_smooth[y*i_numCols+x+1] ) <<1 )
                    + ( p_smooth[(y+1)*i_numCols+x-1]
                    - p_smooth[(y+1)*i_numCols+x+1] );
        }
    }

    if( p_sys->b_edgeVisible )
    {
        /* Display the edges. */
        for( y = 1; y < i_numLines - 1; y++ )
        {
            for( x = 1; x < i_numCols - 1; x++ )
            {
                if( abs( p_grad_x[ y * i_numCols + x ] )
                    + abs( p_grad_y[ y * i_numCols + x ] )
                    > p_sys->i_gradThresh )
                {
                    ( *p_sys->drawingPixelFunction )( p_sys, p_outpic,
                                                      p_filter->p_sys->colorList[ WHITE ].comp1,
                                                      p_filter->p_sys->colorList[ WHITE ].comp2,
                                                      p_filter->p_sys->colorList[ WHITE ].comp3,
                                                      x, y, 0 );
                }
            }
        }
    }

    int i_motion;

    float *pf_lastVect_x = &p_sys->f_lastVect_x;
    float *pf_lastVect_y = &p_sys->f_lastVect_y;

    float f_newVect_x = 0;
    float f_newVect_y = 0;
    float f_contVect_x = 0;
    float f_contVect_y = 0;

    int nb_collisions = 0;

    bool bounce = false;

    /* Test collisions for each pixel the ball will cover in its
       motion. */
    for ( i_motion = 0; i_motion <= p_sys->i_ballSpeed && !bounce; i_motion++ )
    {
        /* Compute next ball position */
        x = roundf( (float)p_sys->i_ball_x
                    + *pf_lastVect_x * (float)i_motion );
        y = roundf( (float)p_sys->i_ball_y
                    + *pf_lastVect_y * (float)i_motion );

        for( int i = x - p_sys->i_ballSize; i <= x + p_sys->i_ballSize; i++ )
        {
            for( int j = y - p_sys->i_ballSize;
                 j <= y + p_sys->i_ballSize; j++ )
            {
                /* Test the pixel if it is inside the disk and check we don't
                write out the frame. */
                if( ( i - x ) * ( i - x ) + ( j - y ) * ( j - y )
                    == p_sys->i_ballSize * p_sys->i_ballSize
                    && j <= i_numLines - 1 && x <= i_numCols - 1
                    && j >= 0 && i >= 0 )
                {
                    /* Test firstly the picture limit collisions. */
                    if( i <= 2 )
                    {
                        f_contVect_x = x - i;
                        f_contVect_y = 0;
                        x++;
                        bounce = true;
                        nb_collisions = 1;
                        goto endLoop;
                    }
                    if( j <= 2 )
                    {
                        f_contVect_x = 0;
                        f_contVect_y = y - j;
                        y++;
                        bounce = true;
                        nb_collisions = 1;
                        goto endLoop;
                    }
                    if( j >= i_numLines - 3 )
                    {
                        f_contVect_x = 0;
                        f_contVect_y = y - j;
                        y--;
                        bounce = true;
                        nb_collisions = 1;
                        goto endLoop;
                    }
                    if( i >= i_numCols - 3 )
                    {
                        f_contVect_x = x - i;
                        f_contVect_y = 0;
                        x--;
                        bounce = true;
                        nb_collisions = 1;
                        goto endLoop;
                    }
                    /* Test the collisions with edges. */
                    if( abs( p_grad_x[ j * i_numCols + i ] )
                        + abs( p_grad_y[ j * i_numCols + i ] )
                        > p_sys->i_gradThresh )
                    {
                        f_contVect_x += x - i;
                        f_contVect_y += y - j;
                        nb_collisions++;
                        bounce = true;
                    }
                }
            }
        }
    }

    endLoop:

    if( bounce )
    {
        /* Compute normal vector. */
        f_contVect_x /= nb_collisions;
        f_contVect_y /= nb_collisions;
        NormalizeVector( &f_contVect_x, &f_contVect_y );

        /* Compute the new vector after the bounce. */
        float cosinus = *pf_lastVect_x * f_contVect_x
                        + *pf_lastVect_y * f_contVect_y;
        f_newVect_x = *pf_lastVect_x - 2 * cosinus * f_contVect_x;
        f_newVect_y = *pf_lastVect_y - 2 * cosinus * f_contVect_y;
        NormalizeVector( &f_newVect_x, &f_newVect_y );

        *pf_lastVect_x = f_newVect_x;
        *pf_lastVect_y = f_newVect_y;

        p_sys->i_ball_x = x;
        p_sys->i_ball_y = y;

        /* Test if next pixel is outside the frame limits.
           If it is the case, then the ball is blocked until it can move. */
        x = roundf( (float)x + *pf_lastVect_x );
        y = roundf( (float)y + *pf_lastVect_y );
        if( x - p_sys->i_ballSize < 2
            || x + p_sys->i_ballSize > i_numCols - 3
            || y - p_sys->i_ballSize < 2
            || y + p_sys->i_ballSize > i_numLines - 3 )
        {
            *pf_lastVect_x = 0;
            *pf_lastVect_y = 0;
        }
        else
            /* After a bouce, the first ball motion is always one pixel. */
            i_motion = p_sys->i_ballSpeed - 1;
    }
    else
        i_motion = 0;

    /* Compute next ball position. */
    p_sys->i_ball_x = roundf( (float)p_sys->i_ball_x + *pf_lastVect_x
                              * (float)( p_sys->i_ballSpeed - i_motion ) );
    p_sys->i_ball_y = roundf( p_sys->i_ball_y + *pf_lastVect_y
                              * (float)( p_sys->i_ballSpeed - i_motion ) );

    /* Draw the ball */
    drawBall( p_sys, p_outpic );

    switch( p_filter->fmt_in.video.i_chroma )
    {
        case VLC_CODEC_RGB24:
        CASE_PACKED_YUV_422
            picture_Release( p_converted );
        default:
            break;
    }
}


/*****************************************************************************
 * ballCallback
 *****************************************************************************
 * filter parameter modification callback
 *****************************************************************************/
static int ballCallback( vlc_object_t *p_this, char const *psz_var,
                             vlc_value_t oldval, vlc_value_t newval,
                             void *p_data )
{
    VLC_UNUSED(oldval);
    filter_sys_t *p_sys = (filter_sys_t *)p_data;
    msg_Err( p_this, "Test" );

    vlc_mutex_lock( &p_sys->lock );
    if( !strcmp( psz_var, FILTER_PREFIX "color" ) )
    {
        p_sys->ballColor = getBallColor( p_this, newval.psz_string );
    }
    else if( !strcmp( psz_var, FILTER_PREFIX "size" ) )
    {
        p_sys->i_ballSize = newval.i_int;
    }
    else if( !strcmp( psz_var, FILTER_PREFIX "speed" ) )
    {
        p_sys->i_ballSpeed = newval.i_int;
    }
    else if( !strcmp( psz_var, FILTER_PREFIX "edge-visible" ) )
    {
        p_sys->b_edgeVisible = newval.b_bool;
    }
    else if( !strcmp( psz_var, FILTER_PREFIX "gradient-threshold" ) )
    {
        p_sys->i_gradThresh = newval.i_int;
    }
    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}


/*****************************************************************************
 * getBallColor
 *****************************************************************************
 * Get and assign the ball color value
 *****************************************************************************/
static int getBallColor( vlc_object_t *p_this, char const *psz_newval )
{
    int ret;
    if( !strcmp( psz_newval, "red" ) )
        ret = RED;
    else if( !strcmp( psz_newval, "blue" ) )
        ret = BLUE;
    else if( !strcmp( psz_newval, "green" ) )
        ret = GREEN;
    else if( !strcmp( psz_newval, "white" ) )
        ret = WHITE;
    else
    {
        msg_Err( p_this, "no valid ball color provided (%s)", psz_newval );
        ret = RED;
    }
    return ret;
}
