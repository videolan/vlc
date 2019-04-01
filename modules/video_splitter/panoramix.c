/*****************************************************************************
 * panoramix.c : Wall panoramic video with edge blending plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2002, 2003 VideoLAN
 *
 * Authors: Cedric Cocquebert <cedric.cocquebert@supelec.fr>
 *          based on Samuel Hocevar <sam@zoy.org>
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
#include <math.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_video_splitter.h>
#include <vlc_vout_window.h>

#define OVERLAP

#ifdef OVERLAP
/* OS CODE DEPENDENT to get display dimensions */
#   ifdef _WIN32
#       include <windows.h>
#   else
#       include <xcb/xcb.h>
#       include <xcb/randr.h>
#   endif
#endif

#define ROW_MAX (15)
#define COL_MAX (15)

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define COLS_TEXT N_("Number of columns")
#define COLS_LONGTEXT N_("Select the number of horizontal video windows in " \
    "which to split the video")

#define ROWS_TEXT N_("Number of rows")
#define ROWS_LONGTEXT N_("Select the number of vertical video windows in " \
    "which to split the video")

#define ACTIVE_TEXT N_("Active windows")
#define ACTIVE_LONGTEXT N_("Comma-separated list of active windows, " \
    "defaults to all")

#define CFG_PREFIX "panoramix-"

#define PANORAMIX_HELP N_("Split the video in multiple windows to " \
    "display on a wall of screens")

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_description( N_("Panoramix: wall with overlap video filter") )
    set_shortname( N_("Panoramix" ))
    set_help(PANORAMIX_HELP)
    set_capability( "video splitter", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_SPLITTER )

    add_integer( CFG_PREFIX "cols", -1, COLS_TEXT, COLS_LONGTEXT, true )
    change_integer_range( -1, COL_MAX )
    add_integer( CFG_PREFIX "rows", -1, ROWS_TEXT, ROWS_LONGTEXT, true )
    change_integer_range( -1, ROW_MAX )

#ifdef OVERLAP
#define LENGTH_TEXT N_("length of the overlapping area (in %)")
#define LENGTH_LONGTEXT N_("Select in percent the length of the blended zone")
    add_integer_with_range( CFG_PREFIX "bz-length", 100, 0, 100, LENGTH_TEXT, LENGTH_LONGTEXT, true )

#define HEIGHT_TEXT N_("height of the overlapping area (in %)")
#define HEIGHT_LONGTEXT N_("Select in percent the height of the blended zone (case of 2x2 wall)")
    add_integer_with_range( CFG_PREFIX "bz-height", 100, 0, 100, HEIGHT_TEXT, HEIGHT_LONGTEXT, true )

#define ATTENUATION_TEXT N_("Attenuation")
#define ATTENUATION_LONGTEXT N_("Check this option if you want attenuate blended zone by this plug-in (if option is unchecked, attenuate is made by opengl)")
    add_bool( CFG_PREFIX "attenuate", true, ATTENUATION_TEXT, ATTENUATION_LONGTEXT, false )

#define BEGIN_TEXT N_("Attenuation, begin (in %)")
#define BEGIN_LONGTEXT N_("Select in percent the Lagrange coefficient of the beginning blended zone")
    add_integer_with_range( CFG_PREFIX "bz-begin", 0, 0, 100, BEGIN_TEXT, BEGIN_LONGTEXT, true )

#define MIDDLE_TEXT N_("Attenuation, middle (in %)")
#define MIDDLE_LONGTEXT N_("Select in percent the Lagrange coefficient of the middle of blended zone")
    add_integer_with_range( CFG_PREFIX "bz-middle", 50, 0, 100, MIDDLE_TEXT, MIDDLE_LONGTEXT, false )

#define END_TEXT N_("Attenuation, end (in %)")
#define END_LONGTEXT N_("Select in percent the Lagrange coefficient of the end of blended zone")
    add_integer_with_range( CFG_PREFIX "bz-end", 100, 0, 100, END_TEXT, END_LONGTEXT, true )

#define MIDDLE_POS_TEXT N_("middle position (in %)")
#define MIDDLE_POS_LONGTEXT N_("Select in percent (50 is center) the position of the middle point (Lagrange) of blended zone")
    add_integer_with_range( CFG_PREFIX "bz-middle-pos", 50, 1, 99, MIDDLE_POS_TEXT, MIDDLE_POS_LONGTEXT, false )
#define RGAMMA_TEXT N_("Gamma (Red) correction")
#define RGAMMA_LONGTEXT N_("Select the gamma for the correction of blended zone (Red or Y component)")
    add_float_with_range( CFG_PREFIX "bz-gamma-red", 1, 0, 5, RGAMMA_TEXT, RGAMMA_LONGTEXT, true )

#define GGAMMA_TEXT N_("Gamma (Green) correction")
#define GGAMMA_LONGTEXT N_("Select the gamma for the correction of blended zone (Green or U component)")
    add_float_with_range( CFG_PREFIX "bz-gamma-green", 1, 0, 5, GGAMMA_TEXT, GGAMMA_LONGTEXT, true )

#define BGAMMA_TEXT N_("Gamma (Blue) correction")
#define BGAMMA_LONGTEXT N_("Select the gamma for the correction of blended zone (Blue or V component)")
    add_float_with_range( CFG_PREFIX "bz-gamma-blue", 1, 0, 5, BGAMMA_TEXT, BGAMMA_LONGTEXT, true )

#define RGAMMA_BC_TEXT N_("Black Crush for Red")
#define RGAMMA_BC_LONGTEXT N_("Select the Black Crush of blended zone (Red or Y component)")
#define GGAMMA_BC_TEXT N_("Black Crush for Green")
#define GGAMMA_BC_LONGTEXT N_("Select the Black Crush of blended zone (Green or U component)")
#define BGAMMA_BC_TEXT N_("Black Crush for Blue")
#define BGAMMA_BC_LONGTEXT N_("Select the Black Crush of blended zone (Blue or V component)")

#define RGAMMA_WC_TEXT N_("White Crush for Red")
#define RGAMMA_WC_LONGTEXT N_("Select the White Crush of blended zone (Red or Y component)")
#define GGAMMA_WC_TEXT N_("White Crush for Green")
#define GGAMMA_WC_LONGTEXT N_("Select the White Crush of blended zone (Green or U component)")
#define BGAMMA_WC_TEXT N_("White Crush for Blue")
#define BGAMMA_WC_LONGTEXT N_("Select the White Crush of blended zone (Blue or V component)")

#define RGAMMA_BL_TEXT N_("Black Level for Red")
#define RGAMMA_BL_LONGTEXT N_("Select the Black Level of blended zone (Red or Y component)")
#define GGAMMA_BL_TEXT N_("Black Level for Green")
#define GGAMMA_BL_LONGTEXT N_("Select the Black Level of blended zone (Green or U component)")
#define BGAMMA_BL_TEXT N_("Black Level for Blue")
#define BGAMMA_BL_LONGTEXT N_("Select the Black Level of blended zone (Blue or V component)")

#define RGAMMA_WL_TEXT N_("White Level for Red")
#define RGAMMA_WL_LONGTEXT N_("Select the White Level of blended zone (Red or Y component)")
#define GGAMMA_WL_TEXT N_("White Level for Green")
#define GGAMMA_WL_LONGTEXT N_("Select the White Level of blended zone (Green or U component)")
#define BGAMMA_WL_TEXT N_("White Level for Blue")
#define BGAMMA_WL_LONGTEXT N_("Select the White Level of blended zone (Blue or V component)")
    add_integer_with_range( CFG_PREFIX "bz-blackcrush-red", 140, 0, 255, RGAMMA_BC_TEXT, RGAMMA_BC_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-blackcrush-green", 140, 0, 255, GGAMMA_BC_TEXT, GGAMMA_BC_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-blackcrush-blue", 140, 0, 255, BGAMMA_BC_TEXT, BGAMMA_BC_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-whitecrush-red", 200, 0, 255, RGAMMA_WC_TEXT, RGAMMA_WC_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-whitecrush-green", 200, 0, 255, GGAMMA_WC_TEXT, GGAMMA_WC_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-whitecrush-blue", 200, 0, 255, BGAMMA_WC_TEXT, BGAMMA_WC_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-blacklevel-red", 150, 0, 255, RGAMMA_BL_TEXT, RGAMMA_BL_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-blacklevel-green", 150, 0, 255, GGAMMA_BL_TEXT, GGAMMA_BL_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-blacklevel-blue", 150, 0, 255, BGAMMA_BL_TEXT, BGAMMA_BL_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-whitelevel-red", 0, 0, 255, RGAMMA_WL_TEXT, RGAMMA_WL_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-whitelevel-green", 0, 0, 255, GGAMMA_WL_TEXT, GGAMMA_WL_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-whitelevel-blue", 0, 0, 255, BGAMMA_WL_TEXT, BGAMMA_WL_LONGTEXT, true )
#ifndef _WIN32
    add_obsolete_bool( CFG_PREFIX "xinerama" );
#endif
    add_obsolete_bool( CFG_PREFIX "offset-x" )
#endif

    add_string( CFG_PREFIX "active", NULL, ACTIVE_TEXT, ACTIVE_LONGTEXT, true )

    add_shortcut( "panoramix" )
    set_callbacks( Open, Close )
vlc_module_end()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *const ppsz_filter_options[] = {
    "cols", "rows", "bz-length", "bz-height", "attenuate",
    "bz-begin", "bz-middle", "bz-end", "bz-middle-pos", "bz-gamma-red",
    "bz-gamma-green", "bz-gamma-blue", "bz-blackcrush-red",
    "bz-blackcrush-green", "bz-blackcrush-blue", "bz-whitecrush-red",
    "bz-whitecrush-green", "bz-whitecrush-blue", "bz-blacklevel-red",
    "bz-blacklevel-green", "bz-blacklevel-blue", "bz-whitelevel-red",
    "bz-whitelevel-green", "bz-whitelevel-blue", "active",
    NULL
};

#define ACCURACY 1000

/* */
static inline int clip_accuracy( int a )
{
    return (a > ACCURACY) ? ACCURACY : (a < 0) ? 0 : a;
}
static inline float clip_unit( float f )
{
    return f < 0.0 ? 0.0 : ( f > 1.0 ? 1.0 : f );
}

/* */
typedef struct
{
    float f_black_crush;
    float f_black_level;

    float f_white_crush;
    float f_white_level;

    float f_gamma;
} panoramix_gamma_t;

typedef struct
{
    struct
    {
        int i_left;
        int i_right;
        int i_top;
        int i_bottom;
    } black;
    struct
    {
        int i_left;
        int i_right;
        int i_top;
        int i_bottom;
    } attenuate;
} panoramix_filter_t;

typedef struct
{
    bool b_active;
    int  i_output;

    /* Output size */
    int i_width;
    int i_height;

    /* Source position and size */
    int  i_src_x;
    int  i_src_y;
    int  i_src_width;
    int  i_src_height;

    /* Filter configuration to use to create the output */
    panoramix_filter_t filter;

} panoramix_output_t;

typedef struct
{
    vlc_fourcc_t i_chroma;

    const int pi_div_w[VOUT_MAX_PLANES];
    const int pi_div_h[VOUT_MAX_PLANES];

    const int pi_black[VOUT_MAX_PLANES];

    bool b_planar;

} panoramix_chroma_t;

typedef struct
{
    const panoramix_chroma_t *p_chroma;

    /* */
    bool   b_attenuate;
    unsigned int bz_length, bz_height;
    unsigned int bz_begin, bz_middle, bz_end;
    unsigned int bz_middle_pos;
    unsigned int a_0, a_1, a_2;

    int lambdav[VOUT_MAX_PLANES][2][ACCURACY/2]; /* [plane][position][?] */
    int lambdah[VOUT_MAX_PLANES][2][ACCURACY/2];

    unsigned int i_overlap_w2;  /* Half overlap width */
    unsigned int i_overlap_h2;  /* Half overlap height */
    uint8_t      p_lut[VOUT_MAX_PLANES][ACCURACY + 1][256];

    /* */
    int i_col;
    int i_row;
    panoramix_output_t pp_output[COL_MAX][ROW_MAX]; /* [x][y] */
} video_splitter_sys_t;

/* */
static int Filter( video_splitter_t *, picture_t *pp_dst[], picture_t * );

static int Mouse( video_splitter_t *, int, vout_window_mouse_event_t * );

/* */
static int Configuration( panoramix_output_t pp_output[ROW_MAX][COL_MAX],
                          int i_col, int i_row,
                          int i_src_width, int i_src_height,
                          int i_half_w, int i_half_h,
                          bool b_attenuate,
                          const bool *pb_active );
static double GammaFactor( const panoramix_gamma_t *, float f_value );

static void FilterPlanar( uint8_t *p_out, int i_out_pitch,
                          const uint8_t *p_in, int i_in_pitch,
                          int i_copy_pitch,
                          int i_copy_lines,
                          int i_pixel_black,
                          const panoramix_filter_t *,
                          uint8_t p_lut[ACCURACY + 1][256],
                          int lambdav[2][ACCURACY/2],
                          int lambdah[2][ACCURACY/2] );

/* */
static const panoramix_chroma_t p_chroma_array[] = {
    /* Planar chroma */
    { VLC_CODEC_I410, { 1, 4, 4, }, { 1, 1, 1, }, { 0, 128, 128 }, true },
    { VLC_CODEC_I411, { 1, 4, 4, }, { 1, 4, 4, }, { 0, 128, 128 }, true },
    { VLC_CODEC_YV12, { 1, 2, 2, }, { 1, 2, 2, }, { 0, 128, 128 }, true },
    { VLC_CODEC_I420, { 1, 2, 2, }, { 1, 2, 2, }, { 0, 128, 128 }, true },
    { VLC_CODEC_J420, { 1, 2, 2, }, { 1, 2, 2, }, { 0, 128, 128 }, true },
    { VLC_CODEC_I422, { 1, 2, 2, }, { 1, 1, 1, }, { 0, 128, 128 }, true },
    { VLC_CODEC_J422, { 1, 2, 2, }, { 1, 1, 1, }, { 0, 128, 128 }, true },
    { VLC_CODEC_I440, { 1, 1, 1, }, { 1, 2, 2, }, { 0, 128, 128 }, true },
    { VLC_CODEC_J440, { 1, 1, 1, }, { 1, 2, 2, }, { 0, 128, 128 }, true },
    { VLC_CODEC_I444, { 1, 1, 1, }, { 1, 1, 1, }, { 0, 128, 128 }, true },
    /* TODO packed chroma (yuv/rgb) ? */

    { 0, {0, }, { 0, }, { 0, 0, 0 }, false }
};

#ifndef _WIN32
/* Get the number of outputs */
static unsigned CountMonitors( vlc_object_t *obj )
{
    char *psz_display = var_InheritString( obj, "x11-display" );
    int snum;
    xcb_connection_t *conn = xcb_connect( psz_display, &snum );
    free( psz_display );
    if( xcb_connection_has_error( conn ) )
        return 0;

    const xcb_setup_t *setup = xcb_get_setup( conn );
    xcb_screen_t *scr = NULL;
    for( xcb_screen_iterator_t i = xcb_setup_roots_iterator( setup );
         i.rem > 0; xcb_screen_next( &i ) )
    {
         if( snum == 0 )
         {
             scr = i.data;
             break;
         }
         snum--;
    }

    unsigned n = 0;
    if( scr == NULL )
        goto error;

    xcb_randr_query_version_reply_t *v =
        xcb_randr_query_version_reply( conn,
            xcb_randr_query_version( conn, 1, 2 ), NULL );
    if( v == NULL )
        goto error;
    msg_Dbg( obj, "using X RandR extension v%"PRIu32".%"PRIu32,
             v->major_version, v->minor_version );
    free( v );

    xcb_randr_get_screen_resources_reply_t *r =
        xcb_randr_get_screen_resources_reply( conn,
            xcb_randr_get_screen_resources( conn, scr->root ), NULL );
    if( r == NULL )
        goto error;

    const xcb_randr_output_t *outputs =
        xcb_randr_get_screen_resources_outputs( r );
    for( unsigned i = 0; i < r->num_outputs; i++ )
    {
        xcb_randr_get_output_info_reply_t *output =
            xcb_randr_get_output_info_reply( conn,
                xcb_randr_get_output_info( conn, outputs[i], 0 ), NULL );
        if( output == NULL )
            continue;
        /* FIXME: do not count cloned outputs multiple times */
        /* XXX: what to do with UNKNOWN state connections? */
        n += output->connection == XCB_RANDR_CONNECTION_CONNECTED;
        free( output );
    }
    free( r );
    msg_Dbg( obj, "X randr has %u outputs", n );

error:
    xcb_disconnect( conn );
    return n;
}
#endif

/*****************************************************************************
 * Open: allocates Wall video thread output method
 *****************************************************************************
 * This function allocates and initializes a Wall vout method.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    video_splitter_t *p_splitter = (video_splitter_t*)p_this;
    video_splitter_sys_t *p_sys;

    const panoramix_chroma_t *p_chroma;
    for( int i = 0; ; i++ )
    {
        vlc_fourcc_t i_chroma = p_chroma_array[i].i_chroma;
        if( !i_chroma )
        {
            msg_Err( p_splitter, "colorspace not supported by plug-in !" );
            return VLC_EGENERIC;
        }
        if( i_chroma == p_splitter->fmt.i_chroma )
        {
            p_chroma = &p_chroma_array[i];
            break;
        }
    }

    /* Allocate structure */
    p_splitter->p_sys = p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* */
    p_sys->p_chroma = p_chroma;

    /* */
    config_ChainParse( p_splitter, CFG_PREFIX, ppsz_filter_options,
                       p_splitter->p_cfg );

    /* */
    p_sys->i_col = var_InheritInteger( p_splitter, CFG_PREFIX "cols" );
    p_sys->i_row = var_InheritInteger( p_splitter, CFG_PREFIX "rows" );

    /* Autodetect number of displays */
    if( p_sys->i_col < 0 || p_sys->i_row < 0 )
    {
#ifdef _WIN32
        const int i_monitor_count = GetSystemMetrics(SM_CMONITORS);
        if( i_monitor_count > 1 )
        {
            p_sys->i_col = GetSystemMetrics( SM_CXVIRTUALSCREEN ) / GetSystemMetrics( SM_CXSCREEN );
            p_sys->i_row = GetSystemMetrics( SM_CYVIRTUALSCREEN ) / GetSystemMetrics( SM_CYSCREEN );
            if( p_sys->i_col * p_sys->i_row != i_monitor_count )
            {
                p_sys->i_col = i_monitor_count;
                p_sys->i_row = 1;
            }
        }
#else
        const unsigned i_monitors = CountMonitors( p_this );
        if( i_monitors > 1 ) /* Find closest to square */
            for( unsigned w = 1; (i_monitors / w) >= w ; w++ )
            {
                if( i_monitors % w )
                    continue;
                p_sys->i_row = w;
                p_sys->i_col = i_monitors / w;
            }
#endif
        /* By default do 2x1 */
        if( p_sys->i_row < 0 )
            p_sys->i_row = 1;
        if( p_sys->i_col < 0 )
            p_sys->i_col = 2;
        var_SetInteger( p_splitter, CFG_PREFIX "cols", p_sys->i_col);
        var_SetInteger( p_splitter, CFG_PREFIX "rows", p_sys->i_row);
    }

    /* */
    p_sys->b_attenuate = var_InheritBool( p_splitter, CFG_PREFIX "attenuate");
    p_sys->bz_length = var_InheritInteger( p_splitter, CFG_PREFIX "bz-length" );
    p_sys->bz_height = var_InheritInteger( p_splitter, CFG_PREFIX "bz-height" );
    p_sys->bz_begin = var_InheritInteger( p_splitter, CFG_PREFIX "bz-begin" );
    p_sys->bz_middle = var_InheritInteger( p_splitter, CFG_PREFIX "bz-middle" );
    p_sys->bz_end = var_InheritInteger( p_splitter, CFG_PREFIX "bz-end" );
    p_sys->bz_middle_pos = var_InheritInteger( p_splitter, CFG_PREFIX "bz-middle-pos" );
    double d_p = 100.0 / p_sys->bz_middle_pos;

    p_sys->a_2 = d_p * p_sys->bz_begin - (double)(d_p * d_p / (d_p - 1)) * p_sys->bz_middle + (double)(d_p / (d_p - 1)) * p_sys->bz_end;
    p_sys->a_1 = -(d_p + 1) * p_sys->bz_begin + (double)(d_p * d_p / (d_p - 1)) * p_sys->bz_middle - (double)(1 / (d_p - 1)) * p_sys->bz_end;
    p_sys->a_0 =  p_sys->bz_begin;

    /* */
    p_sys->i_col = VLC_CLIP( p_sys->i_col, 1, COL_MAX );
    p_sys->i_row = VLC_CLIP( p_sys->i_row, 1, ROW_MAX );
    msg_Dbg( p_splitter, "opening a %i x %i wall",
             p_sys->i_col, p_sys->i_row );

    if( p_sys->bz_length > 0 && ( p_sys->i_row > 1 || p_sys->i_col > 1 ) )
    {
        const int i_overlap_w2_max = p_splitter->fmt.i_width  / p_sys->i_col / 2;
        const int i_overlap_h2_max = p_splitter->fmt.i_height / p_sys->i_row / 2;
        const int i_overlap2_max = __MIN( i_overlap_w2_max, i_overlap_h2_max );

        if( p_sys->i_col > 1 )
            p_sys->i_overlap_w2 = i_overlap2_max * p_sys->bz_length / 100;
        else
            p_sys->i_overlap_w2 = 0;

        if( p_sys->i_row > 1 )
            p_sys->i_overlap_h2 = i_overlap2_max * p_sys->bz_height / 100;
        else
            p_sys->i_overlap_h2 = 0;

        /* */
        int i_div_max_w = 1;
        int i_div_max_h = 1;
        for( int i = 0; i < VOUT_MAX_PLANES; i++ )
        {
            i_div_max_w = __MAX( i_div_max_w, p_chroma->pi_div_w[i] );
            i_div_max_h = __MAX( i_div_max_h, p_chroma->pi_div_h[i] );
        }
        p_sys->i_overlap_w2 = i_div_max_w * (p_sys->i_overlap_w2 / i_div_max_w);
        p_sys->i_overlap_h2 = i_div_max_h * (p_sys->i_overlap_h2 / i_div_max_h);
    }
    else
    {
        p_sys->i_overlap_w2 = 0;
        p_sys->i_overlap_h2 = 0;
    }

    /* Compute attenuate parameters */
    if( p_sys->b_attenuate )
    {
        panoramix_gamma_t p_gamma[VOUT_MAX_PLANES];

        p_gamma[0].f_gamma = var_InheritFloat( p_splitter, CFG_PREFIX "bz-gamma-red" );
        p_gamma[1].f_gamma = var_InheritFloat( p_splitter, CFG_PREFIX "bz-gamma-green" );
        p_gamma[2].f_gamma = var_InheritFloat( p_splitter, CFG_PREFIX "bz-gamma-blue" );

        p_gamma[0].f_black_crush = var_InheritInteger( p_splitter, CFG_PREFIX "bz-blackcrush-red" ) / 255.0;
        p_gamma[1].f_black_crush = var_InheritInteger( p_splitter, CFG_PREFIX "bz-blackcrush-green" ) / 255.0;
        p_gamma[2].f_black_crush = var_InheritInteger( p_splitter, CFG_PREFIX "bz-blackcrush-blue" ) / 255.0;
        p_gamma[0].f_white_crush = var_InheritInteger( p_splitter, CFG_PREFIX "bz-whitecrush-red" ) / 255.0;
        p_gamma[1].f_white_crush = var_InheritInteger( p_splitter, CFG_PREFIX "bz-whitecrush-green" ) / 255.0;
        p_gamma[2].f_white_crush = var_InheritInteger( p_splitter, CFG_PREFIX "bz-whitecrush-blue" ) / 255.0;

        p_gamma[0].f_black_level = var_InheritInteger( p_splitter, CFG_PREFIX "bz-blacklevel-red" ) / 255.0;
        p_gamma[1].f_black_level = var_InheritInteger( p_splitter, CFG_PREFIX "bz-blacklevel-green" ) / 255.0;
        p_gamma[2].f_black_level = var_InheritInteger( p_splitter, CFG_PREFIX "bz-blacklevel-blue" ) / 255.0;
        p_gamma[0].f_white_level = var_InheritInteger( p_splitter, CFG_PREFIX "bz-whitelevel-red" ) / 255.0;
        p_gamma[1].f_white_level = var_InheritInteger( p_splitter, CFG_PREFIX "bz-whitelevel-green" ) / 255.0;
        p_gamma[2].f_white_level = var_InheritInteger( p_splitter, CFG_PREFIX "bz-whitelevel-blue" ) / 255.0;

        for( int i = 3; i < VOUT_MAX_PLANES; i++ )
        {
            /* Initialize unsupported planes */
            p_gamma[i].f_gamma = 1.0;
            p_gamma[i].f_black_crush = 140.0/255.0;
            p_gamma[i].f_white_crush = 200.0/255.0;
            p_gamma[i].f_black_level = 150.0/255.0;
            p_gamma[i].f_white_level = 0.0/255.0;
        }

        if( p_chroma->i_chroma == VLC_CODEC_YV12 )
        {
            /* Exchange U and V */
            panoramix_gamma_t t = p_gamma[1];
            p_gamma[1] = p_gamma[2];
            p_gamma[2] = t;
        }

        for( int i_index = 0; i_index < 256; i_index++ )
        {
            for( int i_index2 = 0; i_index2 <= ACCURACY; i_index2++ )
            {
                for( int i_plane = 0; i_plane < VOUT_MAX_PLANES; i_plane++ )
                {
                    double f_factor = GammaFactor( &p_gamma[i_plane], (float)i_index / 255.0 );

                    float f_lut = clip_unit( 1.0 - ((ACCURACY - (float)i_index2) * f_factor / (ACCURACY - 1)) );

                    p_sys->p_lut[i_plane][i_index2][i_index] = f_lut * i_index + (int)( (1.0 - f_lut) * (float)p_chroma->pi_black[i_plane] );
                }
            }
        }

        for( int i_plane = 0; i_plane < VOUT_MAX_PLANES; i_plane++ )
        {
            if( !p_chroma->pi_div_w[i_plane] || !p_chroma->pi_div_h[i_plane] )
                continue;
            const int i_length_w = (2 * p_sys->i_overlap_w2) / p_chroma->pi_div_w[i_plane];
            const int i_length_h = (2 * p_sys->i_overlap_h2) / p_chroma->pi_div_h[i_plane];

            for( int i_dir = 0; i_dir < 2; i_dir++ )
            {
                const int i_length = i_dir == 0 ? i_length_w : i_length_h;
                const int i_den = i_length * i_length;
                const int a_2 = p_sys->a_2 * (ACCURACY / 100);
                const int a_1 = p_sys->a_1 * i_length * (ACCURACY / 100);
                const int a_0 = p_sys->a_0 * i_den * (ACCURACY / 100);

                for( int i_position = 0; i_position < 2; i_position++ )
                {
                    for( int i_index = 0; i_index < i_length; i_index++ )
                    {
                        const int v = i_position == 1 ? i_index : (i_length - i_index);
                        const int i_lambda = clip_accuracy( ACCURACY - (a_2 * v*v + a_1 * v + a_0) / i_den );

                        if( i_dir == 0 )
                            p_sys->lambdav[i_plane][i_position][i_index] = i_lambda;
                        else
                            p_sys->lambdah[i_plane][i_position][i_index] = i_lambda;
                    }
                }
            }
        }
    }

    /* */
    char *psz_state = var_InheritString( p_splitter, CFG_PREFIX "active" );

    /* */
    bool pb_active[COL_MAX*ROW_MAX];

    for( int i = 0; i < COL_MAX*ROW_MAX; i++ )
        pb_active[i] = psz_state == NULL;

    /* Parse active list if provided */
    char *psz_tmp = psz_state;
    while( psz_tmp && *psz_tmp )
    {
        char *psz_next = strchr( psz_tmp, ',' );
        if( psz_next )
            *psz_next++ = '\0';

        const int i_index = atoi( psz_tmp );
        if( i_index >= 0 && i_index < COL_MAX*ROW_MAX )
            pb_active[i_index] = true;

        psz_tmp = psz_next;
    }
    free( psz_state );

    /* */
    p_splitter->i_output =
        Configuration( p_sys->pp_output,
                       p_sys->i_col, p_sys->i_row,
                       p_splitter->fmt.i_width, p_splitter->fmt.i_height,
                       p_sys->i_overlap_w2, p_sys->i_overlap_h2,
                       p_sys->b_attenuate,
                       pb_active );
    p_splitter->p_output = calloc( p_splitter->i_output,
                                   sizeof(*p_splitter->p_output) );
    if( !p_splitter->p_output )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    for( int y = 0; y < p_sys->i_row; y++ )
    {
        for( int x = 0; x < p_sys->i_col; x++ )
        {
            panoramix_output_t *p_output = &p_sys->pp_output[x][y];
            if( !p_output->b_active )
                continue;

            video_splitter_output_t *p_cfg = &p_splitter->p_output[p_output->i_output];

            /* */
            video_format_Copy( &p_cfg->fmt, &p_splitter->fmt );
            p_cfg->fmt.i_visible_width  =
            p_cfg->fmt.i_width          = p_output->i_width;
            p_cfg->fmt.i_visible_height =
            p_cfg->fmt.i_height         = p_output->i_height;

            p_cfg->psz_module = NULL;
        }
    }


    /* */
    p_splitter->pf_filter = Filter;
    p_splitter->mouse = Mouse;

    return VLC_SUCCESS;
}

/**
 * Terminate a splitter module
 */
static void Close( vlc_object_t *p_this )
{
    video_splitter_t *p_splitter = (video_splitter_t*)p_this;
    video_splitter_sys_t *p_sys = p_splitter->p_sys;

    free( p_splitter->p_output );
    free( p_sys );
}

/**
 * It creates multiples pictures from the source one
 */
static int Filter( video_splitter_t *p_splitter, picture_t *pp_dst[], picture_t *p_src )
{
    video_splitter_sys_t *p_sys = p_splitter->p_sys;

    if( video_splitter_NewPicture( p_splitter, pp_dst ) )
    {
        picture_Release( p_src );
        return VLC_EGENERIC;
    }

    for( int y = 0; y < p_sys->i_row; y++ )
    {
        for( int x = 0; x < p_sys->i_col; x++ )
        {
            const panoramix_output_t *p_output = &p_sys->pp_output[x][y];
            if( !p_output->b_active )
                continue;

            /* */
            picture_t *p_dst = pp_dst[p_output->i_output];

            /* */
            picture_CopyProperties( p_dst, p_src );

            /* */
            for( int i_plane = 0; i_plane < p_src->i_planes; i_plane++ )
            {
                const int i_div_w = p_sys->p_chroma->pi_div_w[i_plane];
                const int i_div_h = p_sys->p_chroma->pi_div_h[i_plane];

                if( !i_div_w || !i_div_h )
                    continue;

                const plane_t *p_srcp = &p_src->p[i_plane];
                const plane_t *p_dstp = &p_dst->p[i_plane];

                /* */
                panoramix_filter_t filter;
                filter.black.i_right  = p_output->filter.black.i_right / i_div_w;
                filter.black.i_left   = p_output->filter.black.i_left / i_div_w;
                filter.black.i_top    = p_output->filter.black.i_top / i_div_h;
                filter.black.i_bottom = p_output->filter.black.i_bottom / i_div_h;

                filter.attenuate.i_right  = p_output->filter.attenuate.i_right / i_div_w;
                filter.attenuate.i_left   = p_output->filter.attenuate.i_left / i_div_w;
                filter.attenuate.i_top    = p_output->filter.attenuate.i_top / i_div_h;
                filter.attenuate.i_bottom = p_output->filter.attenuate.i_bottom / i_div_h;

                /* */
                const int i_x = p_output->i_src_x/i_div_w;
                const int i_y = p_output->i_src_y/i_div_h;

                assert( p_sys->p_chroma->b_planar );
                FilterPlanar( p_dstp->p_pixels, p_dstp->i_pitch,
                              &p_srcp->p_pixels[i_y * p_srcp->i_pitch + i_x * p_srcp->i_pixel_pitch], p_srcp->i_pitch,
                              p_output->i_src_width/i_div_w, p_output->i_src_height/i_div_h,
                              p_sys->p_chroma->pi_black[i_plane],
                              &filter,
                              p_sys->p_lut[i_plane],
                              p_sys->lambdav[i_plane],
                              p_sys->lambdah[i_plane] );
            }
        }
    }

    picture_Release( p_src );
    return VLC_SUCCESS;
}

/**
 * It converts mouse events
 */
static int Mouse( video_splitter_t *p_splitter, int i_index,
                  vout_window_mouse_event_t *restrict ev )
{
    video_splitter_sys_t *p_sys = p_splitter->p_sys;

    for( int y = 0; y < p_sys->i_row; y++ )
    {
        for( int x = 0; x < p_sys->i_col; x++ )
        {
            const panoramix_output_t *p_output = &p_sys->pp_output[x][y];
            if( p_output->b_active && p_output->i_output == i_index )
            {
                const int i_x = ev->x - p_output->filter.black.i_left;
                const int i_y = ev->y - p_output->filter.black.i_top;

                if( i_x >= 0 && i_x < p_output->i_width  - p_output->filter.black.i_right &&
                    i_y >= 0 && i_y < p_output->i_height - p_output->filter.black.i_bottom )
                {
                    ev->x = p_output->i_src_x + i_x;
                    ev->y = p_output->i_src_y + i_y;
                    return VLC_SUCCESS;
                }
            }
        }
    }
    return VLC_EGENERIC;
}

/* It return a coefficient between 0.0 and 1.0 to be applied to the given
 * value
 */
static double GammaFactor( const panoramix_gamma_t *g, float f_value )
{
    if( f_value <= g->f_black_crush )
    {
        float f_input = f_value * g->f_black_level / g->f_black_crush + (1.0 - g->f_black_level);

        return pow( f_input, 1.0 / g->f_gamma );
    }
    else if( f_value >= g->f_white_crush )
    {
        float f_input = (f_value * (1.0 - (g->f_white_level + 1.0)) + (g->f_white_level + 1.0) * g->f_white_crush - 1.0) / (g->f_white_crush - 1.0);
        return pow( f_input, 1.0 / g->f_gamma );
    }
    return 1.0;
}

/**
 * It creates the panoramix configuration
 */
static int Configuration( panoramix_output_t pp_output[ROW_MAX][COL_MAX],
                          int i_col, int i_row,
                          int i_src_width, int i_src_height,
                          int i_half_w, int i_half_h,
                          bool b_attenuate,
                          const bool *pb_active )
{
#ifdef OVERLAP
    const bool b_overlap = true;
#else
    const bool b_overlap = false;
#endif

    /* */
    int i_output = 0;
    for( int y = 0, i_src_y = 0, i_dst_y = 0; y < i_row; y++ )
    {
        const bool b_row_first = y == 0;
        const bool b_row_last  = y == i_row - 1;

        /* Compute source height */
        int i_win_height = (i_src_height / i_row ) & ~1;
        if( b_row_last )
            i_win_height = i_src_height - y * i_win_height;

        for( int x = 0, i_src_x = 0, i_dst_x = 0; x < i_col; x++ )
        {
            const bool b_col_first = x == 0;
            const bool b_col_last  = x == i_col - 1;

            /* Compute source width */
            int i_win_width  = (i_src_width  / i_col ) & ~1;
            if( b_col_last )
                i_win_width  = i_src_width  - x * i_win_width;

            /* Compute filter configuration */
            panoramix_filter_t cfg;

            memset( &cfg, 0, sizeof(cfg) );
            if( b_overlap && b_attenuate )
            {
                if( i_col > 2 )
                {
                    if( b_col_first )
                        cfg.black.i_left   = i_half_w;
                    if( b_col_last )
                        cfg.black.i_right  = i_half_w;
                }
                if( i_row > 2 )
                {
                    if( b_row_first )
                        cfg.black.i_top    = i_half_h;
                    if( b_row_last )
                        cfg.black.i_bottom = i_half_h;
                }
                if( !b_col_first )
                    cfg.attenuate.i_left   = 2 * i_half_w;
                if( !b_col_last )
                    cfg.attenuate.i_right  = 2 * i_half_w;
                if( !b_row_first )
                    cfg.attenuate.i_top    = 2 * i_half_h;
                if( !b_row_last )
                    cfg.attenuate.i_bottom = 2 * i_half_h;
            }

            /* */
            panoramix_output_t *p_output = &pp_output[x][y];

            /* */
            p_output->i_src_x = i_src_x - cfg.attenuate.i_left/2;
            p_output->i_src_y = i_src_y - cfg.attenuate.i_top/2;
            p_output->i_src_width  = i_win_width  + cfg.attenuate.i_left/2 + cfg.attenuate.i_right/2;
            p_output->i_src_height = i_win_height + cfg.attenuate.i_top/2  + cfg.attenuate.i_bottom/2;

            /* */
            p_output->filter = cfg;

            /* */
            p_output->i_width  = cfg.black.i_left + p_output->i_src_width  + cfg.black.i_right;
            p_output->i_height = cfg.black.i_top  + p_output->i_src_height + cfg.black.i_bottom;

            /* */
            p_output->b_active = pb_active[y * i_col + x] &&
                                 p_output->i_width > 0 &&
                                 p_output->i_height > 0;
            if( p_output->b_active )
                p_output->i_output = i_output++;

            /* */
            i_src_x += i_win_width;
            i_dst_x += p_output->i_width;
        }
        i_src_y += i_win_height;
        i_dst_y += pp_output[0][y].i_height;
    }
    return i_output;
}

/**
 * It filters a video plane
 */
static void FilterPlanar( uint8_t *p_out, int i_out_pitch,
                          const uint8_t *p_in, int i_in_pitch,
                          int i_copy_pitch,
                          int i_copy_lines,
                          int i_pixel_black,
                          const panoramix_filter_t *p_cfg,
                          uint8_t p_lut[ACCURACY + 1][256],
                          int lambdav[2][ACCURACY/2],
                          int lambdah[2][ACCURACY/2] )
{
    /* */
    assert( !p_cfg->black.i_left   || !p_cfg->attenuate.i_left );
    assert( !p_cfg->black.i_right  || !p_cfg->attenuate.i_right );
    assert( !p_cfg->black.i_top    || !p_cfg->attenuate.i_top );
    assert( !p_cfg->black.i_bottom || !p_cfg->attenuate.i_bottom );

    const int i_out_width = p_cfg->black.i_left + i_copy_pitch + p_cfg->black.i_right;

    /* Top black border */
    for( int b = 0; b < p_cfg->black.i_top; b++ )
    {
        memset( p_out, i_pixel_black, i_out_width );
        p_out += i_out_pitch;
    }

    for( int y = 0; y < i_copy_lines; y++ )
    {
        const uint8_t *p_src = p_in;
        uint8_t *p_dst = p_out;

        /* Black border on the left */
        if( p_cfg->black.i_left > 0 )
        {
            memset( p_dst, i_pixel_black, p_cfg->black.i_left );
            p_dst += p_cfg->black.i_left;
        }
        /* Attenuated video on the left */
        for( int i = 0; i < p_cfg->attenuate.i_left; i++ )
            *p_dst++ = p_lut[lambdav[0][i]][*p_src++];

        /* Unmodified video */
        const int i_unmodified_width = i_copy_pitch - p_cfg->attenuate.i_left - p_cfg->attenuate.i_right;
        memcpy( p_dst, p_src, i_unmodified_width );
        p_dst += i_unmodified_width;
        p_src += i_unmodified_width;

        /* Attenuated video on the right */
        for( int i = 0; i < p_cfg->attenuate.i_right; i++ )
            *p_dst++ = p_lut[lambdav[1][i]][*p_src++];
        /* Black border on the right */
        if( p_cfg->black.i_right > 0 )
        {
            memset( p_dst, i_pixel_black, p_cfg->black.i_right );
            p_dst += p_cfg->black.i_right;
        }

        /* Attenuate complete line at top/bottom */
        const bool b_attenuate_top    = y < p_cfg->attenuate.i_top;
        const bool b_attenuate_bottom = y >= i_copy_lines - p_cfg->attenuate.i_bottom;
        if( b_attenuate_top || b_attenuate_bottom )
        {
            const int i_index = b_attenuate_top ? lambdah[0][y] : lambdah[1][y - (i_copy_lines - p_cfg->attenuate.i_bottom)];
            for( int i = 0; i < i_out_width; i++)
                p_out[i] = p_lut[i_index][p_out[i]];
        }

        /* */
        p_in  += i_in_pitch;
        p_out += i_out_pitch;
    }
    /* Bottom black border */
    for( int b = 0; b < p_cfg->black.i_bottom; b++ )
    {
        memset( p_out, i_pixel_black, i_out_width );
        p_out += i_out_pitch;
    }
}



#if 0
static void RenderPackedRGB( vout_thread_t *p_vout, picture_t *p_pic )
{
    int length;
    length = 2 * p_sys->i_overlap_w2 * p_pic->p->i_pixel_pitch;

    if (p_sys->b_has_changed)
    {
        int i_plane_;
        int i_col_mod;
        Denom = F2(length / p_pic->p->i_pixel_pitch);
        a_2 = p_sys->a_2 * (ACCURACY / 100);
        a_1 = p_sys->a_1 * 2 * p_sys->i_overlap_w2 * (ACCURACY / 100);
        a_0 = p_sys->a_0 * Denom * (ACCURACY / 100);
        for(i_col_mod = 0; i_col_mod < 2; i_col_mod++)
            for (i_index = 0; i_index < length / p_pic->p->i_pixel_pitch; i_index++)
                for (i_plane_ =  0; i_plane_ < p_pic->p->i_pixel_pitch; i_plane_++)
                    p_sys->lambda[i_col_mod][i_plane_][i_index] = clip_accuracy(!i_col_mod ? ACCURACY - (F4(a_2, a_1, i_index) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length / p_pic->p->i_pixel_pitch) - i_index) + a_0) / Denom);
    }

    length = 2 * p_sys->i_overlap_h2;
    if (p_sys->b_has_changed)
    {
        int i_plane_;
        int i_row_mod;
        Denom = F2(length);
        a_2 = p_sys->a_2 * (ACCURACY / 100);
        a_1 = p_sys->a_1 * length * (ACCURACY / 100);
        a_0 = p_sys->a_0 * Denom * (ACCURACY / 100);
        for(i_row_mod = 0; i_row_mod < 2; i_row_mod++)
          for (i_index = 0; i_index < length; i_index++)
            for (i_plane_ =  0; i_plane_ < p_pic->p->i_pixel_pitch; i_plane_++)
                p_sys->lambda2[i_row_mod][i_plane_][i_index] = clip_accuracy(!i_row_mod ? ACCURACY - (F4(a_2, a_1, i_index) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length) - i_index) + a_0) / Denom);
    }
}
#endif


