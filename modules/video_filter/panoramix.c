/*****************************************************************************
 * panoramix.c : Wall panoramic video with edge blending plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2002, 2003 VideoLAN
 * $Id$
 *
 * Authors: Cedric Cocquebert <cedric.cocquebert@supelec.fr>
 *          based on Samuel Hocevar <sam@zoy.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <assert.h>

#include "filter_common.h"

// add by cedric.cocquebert@supelec.fr
#define OVERLAP        2350
#ifdef OVERLAP
    #include <math.h>
    // OS CODE DEPENDENT to get display dimensions
    #ifdef WIN32
        #include <windows.h>
    #else
        #include <X11/Xlib.h>
    #endif
    #define GAMMA        1
//  #define PACKED_YUV    1
    #define F2(a) ((a)*(a))
    #define F4(a,b,x) ((a)*(F2(x))+((b)*(x)))
    #define ACCURACY 1000
    #define RATIO_MAX 2500
    #define CLIP_01(a) (a < 0.0 ? 0.0 : (a > 1.0 ? 1.0 : a))
//    #define CLIP_0A(a) (a < 0.0 ? 0.0 : (a > ACCURACY ? ACCURACY : a))
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
#ifdef PACKED_YUV
static void RenderPackedYUV   ( vout_thread_t *, picture_t * );
#endif
static void RenderPlanarYUV   ( vout_thread_t *, picture_t * );
static void RenderPackedRGB   ( vout_thread_t *, picture_t * );

static void RemoveAllVout  ( vout_thread_t *p_vout );

static int  MouseEvent( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );
static int  FullscreenEventUp( vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void * );
static int  FullscreenEventDown( vlc_object_t *, char const *,
                                 vlc_value_t, vlc_value_t, void * );

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
#define ACTIVE_LONGTEXT N_("Comma separated list of active windows, " \
    "defaults to all")

#define CFG_PREFIX "panoramix-"

vlc_module_begin ()
    set_description( N_("Panoramix: wall with overlap video filter") )
    set_shortname( N_("Panoramix" ))
    set_capability( "video filter", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_integer( CFG_PREFIX "cols", -1, NULL,
                 COLS_TEXT, COLS_LONGTEXT, true )
    add_integer( CFG_PREFIX "rows", -1, NULL,
                 ROWS_TEXT, ROWS_LONGTEXT, true )

#ifdef OVERLAP
#define OFFSET_X_TEXT N_("Offset X offset (automatic compensation)")
#define OFFSET_X_LONGTEXT N_("Select if you want an automatic offset in horizontal (in case of misalignment due to autoratio control)")
    add_bool( CFG_PREFIX "offset-x", 1, NULL, OFFSET_X_TEXT, OFFSET_X_LONGTEXT, true )

#define LENGTH_TEXT N_("length of the overlapping area (in %)")
#define LENGTH_LONGTEXT N_("Select in percent the length of the blended zone")
    add_integer_with_range( CFG_PREFIX "bz-length", 100, 0, 100, NULL, LENGTH_TEXT, LENGTH_LONGTEXT, true )

#define HEIGHT_TEXT N_("height of the overlapping area (in %)")
#define HEIGHT_LONGTEXT N_("Select in percent the height of the blended zone (case of 2x2 wall)")
    add_integer_with_range( CFG_PREFIX "bz-height", 100, 0, 100, NULL, HEIGHT_TEXT, HEIGHT_LONGTEXT, true )

#define ATTENUATION_TEXT N_("Attenuation")
#define ATTENUATION_LONGTEXT N_("Check this option if you want attenuate blended zone by this plug-in (if option is unchecked, attenuate is made by opengl)")
    add_bool( CFG_PREFIX "attenuate", 1, NULL, ATTENUATION_TEXT, ATTENUATION_LONGTEXT, false )

#define BEGIN_TEXT N_("Attenuation, begin (in %)")
#define BEGIN_LONGTEXT N_("Select in percent the Lagrange coeff of the beginning blended zone")
    add_integer_with_range( CFG_PREFIX "bz-begin", 0, 0, 100, NULL, BEGIN_TEXT, BEGIN_LONGTEXT, true )

#define MIDDLE_TEXT N_("Attenuation, middle (in %)")
#define MIDDLE_LONGTEXT N_("Select in percent the Lagrange coeff of the middle of blended zone")
    add_integer_with_range( CFG_PREFIX "bz-middle", 50, 0, 100, NULL, MIDDLE_TEXT, MIDDLE_LONGTEXT, false )

#define END_TEXT N_("Attenuation, end (in %)")
#define END_LONGTEXT N_("Select in percent the Lagrange coeff of the end of blended zone")
    add_integer_with_range( CFG_PREFIX "bz-end", 100, 0, 100, NULL, END_TEXT, END_LONGTEXT, true )

#define MIDDLE_POS_TEXT N_("middle position (in %)")
#define MIDDLE_POS_LONGTEXT N_("Select in percent (50 is center) the position of the middle point (Lagrange) of blended zone")
    add_integer_with_range( CFG_PREFIX "bz-middle-pos", 50, 1, 99, NULL, MIDDLE_POS_TEXT, MIDDLE_POS_LONGTEXT, false )
#ifdef GAMMA
#define RGAMMA_TEXT N_("Gamma (Red) correction")
#define RGAMMA_LONGTEXT N_("Select the gamma for the correction of blended zone (Red or Y component)")
    add_float_with_range( CFG_PREFIX "bz-gamma-red", 1, 0, 5, NULL, RGAMMA_TEXT, RGAMMA_LONGTEXT, true )

#define GGAMMA_TEXT N_("Gamma (Green) correction")
#define GGAMMA_LONGTEXT N_("Select the gamma for the correction of blended zone (Green or U component)")
    add_float_with_range( CFG_PREFIX "bz-gamma-green", 1, 0, 5, NULL, GGAMMA_TEXT, GGAMMA_LONGTEXT, true )

#define BGAMMA_TEXT N_("Gamma (Blue) correction")
#define BGAMMA_LONGTEXT N_("Select the gamma for the correction of blended zone (Blue or V component)")
    add_float_with_range( CFG_PREFIX "bz-gamma-blue", 1, 0, 5, NULL, BGAMMA_TEXT, BGAMMA_LONGTEXT, true )
#endif
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
    add_integer_with_range( CFG_PREFIX "bz-blackcrush-red", 140, 0, 255, NULL, RGAMMA_BC_TEXT, RGAMMA_BC_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-blackcrush-green", 140, 0, 255, NULL, GGAMMA_BC_TEXT, GGAMMA_BC_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-blackcrush-blue", 140, 0, 255, NULL, BGAMMA_BC_TEXT, BGAMMA_BC_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-whitecrush-red", 200, 0, 255, NULL, RGAMMA_WC_TEXT, RGAMMA_WC_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-whitecrush-green", 200, 0, 255, NULL, GGAMMA_WC_TEXT, GGAMMA_WC_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-whitecrush-blue", 200, 0, 255, NULL, BGAMMA_WC_TEXT, BGAMMA_WC_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-blacklevel-red", 150, 0, 255, NULL, RGAMMA_BL_TEXT, RGAMMA_BL_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-blacklevel-green", 150, 0, 255, NULL, GGAMMA_BL_TEXT, GGAMMA_BL_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-blacklevel-blue", 150, 0, 255, NULL, BGAMMA_BL_TEXT, BGAMMA_BL_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-whitelevel-red", 0, 0, 255, NULL, RGAMMA_WL_TEXT, RGAMMA_WL_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-whitelevel-green", 0, 0, 255, NULL, GGAMMA_WL_TEXT, GGAMMA_WL_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "bz-whitelevel-blue", 0, 0, 255, NULL, BGAMMA_WL_TEXT, BGAMMA_WL_LONGTEXT, true )
#ifndef WIN32
#define XINERAMA_TEXT N_("Xinerama option")
#define XINERAMA_LONGTEXT N_("Uncheck if you have not used xinerama")
    add_bool( CFG_PREFIX "xinerama", 1, NULL, XINERAMA_TEXT, XINERAMA_LONGTEXT, true )
#endif
#endif

    add_string( CFG_PREFIX "active", NULL, NULL, ACTIVE_TEXT, ACTIVE_LONGTEXT, true )

    add_shortcut( "panoramix" )
    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "cols", "rows", "offset-x", "bz-length", "bz-height", "attenuate",
    "bz-begin", "bz-middle", "bz-end", "bz-middle-pos", "bz-gamma-red",
    "bz-gamma-green", "bz-gamma-blue", "bz-blackcrush-red",
    "bz-blackcrush-green", "bz-blackcrush-blue", "bz-whitecrush-red",
    "bz-whitecrush-green", "bz-whitecrush-blue", "bz-blacklevel-red",
    "bz-blacklevel-green", "bz-blacklevel-blue", "bz-whitelevel-red",
    "bz-whitelevel-green", "bz-whitelevel-blue", "xinerama", "active",
    NULL
};

/*****************************************************************************
 * vout_sys_t: Wall video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Wall specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
#ifdef OVERLAP
    bool   b_autocrop;
    bool   b_attenuate;
    unsigned int bz_length, bz_height, bz_begin, bz_middle, bz_end, bz_middle_pos;
    unsigned int i_ratio_max;
    unsigned int i_ratio;
    unsigned int a_0, a_1, a_2;
    bool     b_has_changed;
    int lambda[2][VOUT_MAX_PLANES][500];
    int cstYUV[2][VOUT_MAX_PLANES][500];
    int lambda2[2][VOUT_MAX_PLANES][500];
    int cstYUV2[2][VOUT_MAX_PLANES][500];
    unsigned int i_halfLength;
    unsigned int i_halfHeight;
    int i_offset_x;
    int i_offset_y;
#ifdef GAMMA
    float        f_gamma_red, f_gamma_green, f_gamma_blue;
    float         f_gamma[VOUT_MAX_PLANES];
    uint8_t         LUT[VOUT_MAX_PLANES][ACCURACY + 1][256];
#ifdef PACKED_YUV
    uint8_t         LUT2[VOUT_MAX_PLANES][256][500];
#endif
#endif
#ifndef WIN32
    bool   b_xinerama;
#endif
#endif
    int    i_col;
    int    i_row;
    int    i_vout;
    struct vout_list_t
    {
        bool b_active;
        int i_width;
        int i_height;
        vout_thread_t *p_vout;
    } *pp_vout;
};



/*****************************************************************************
 * Control: control facility for the vout (forwards to child vout)
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    int i_row, i_col, i_vout = 0;

    for( i_row = 0; i_row < p_vout->p_sys->i_row; i_row++ )
    {
        for( i_col = 0; i_col < p_vout->p_sys->i_col; i_col++ )
        {
            vout_vaControl( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                            i_query, args );
            i_vout++;
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Create: allocates Wall video thread output method
 *****************************************************************************
 * This function allocates and initializes a Wall vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    char *psz_method, *psz_tmp, *psz_method_tmp;
    int i_vout;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return VLC_ENOMEM;

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
/* Color Format not supported
// Planar Y, packed UV
case VLC_FOURCC('Y','M','G','A'):
// Packed YUV 4:2:2, U:Y:V:Y, interlaced
case VLC_FOURCC('I','U','Y','V'):    // packed by 2
// Packed YUV 2:1:1, Y:U:Y:V
case VLC_FOURCC('Y','2','1','1'):     // packed by 4
// Packed YUV Reverted
case VLC_FOURCC('c','y','u','v'):    // packed by 2
*/
    switch (p_vout->render.i_chroma)
    {
    // planar YUV
        case VLC_FOURCC('I','4','4','4'):
        case VLC_FOURCC('I','4','2','2'):
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('Y','V','1','2'):
        case VLC_FOURCC('I','Y','U','V'):
        case VLC_FOURCC('I','4','1','1'):
        case VLC_FOURCC('I','4','1','0'):
        case VLC_FOURCC('Y','V','U','9'):
        case VLC_FOURCC('Y','U','V','A'):
            p_vout->pf_render = RenderPlanarYUV;
            break;
    // packed RGB
        case VLC_FOURCC('R','G','B','2'):    // packed by 1
        case VLC_FOURCC('R','V','1','5'):    // packed by 2
        case VLC_FOURCC('R','V','1','6'):    // packed by 2
        case VLC_FOURCC('R','V','2','4'):    // packed by 3
        case VLC_FOURCC('R','V','3','2'):    // packed by 4
            p_vout->pf_render = RenderPackedRGB;
            break;
#ifdef PACKED_YUV
    // packed YUV
        case VLC_FOURCC('Y','U','Y','2'):    // packed by 2
        case VLC_FOURCC('Y','U','N','V'):    // packed by 2
        case VLC_FOURCC('U','Y','V','Y'):    // packed by 2
        case VLC_FOURCC('U','Y','N','V'):    // packed by 2
        case VLC_FOURCC('Y','4','2','2'):    // packed by 2
            p_vout->pf_render = RenderPackedYUV;
            break;
#endif
        default:
            msg_Err( p_vout, "colorspace not supported by plug-in !!!");
            free( p_vout->p_sys );
            return VLC_ENOMEM;
    }
    p_vout->pf_display = NULL;
    p_vout->pf_control = Control;

    config_ChainParse( p_vout, CFG_PREFIX, ppsz_filter_options,
                       p_vout->p_cfg );

    /* Look what method was requested */
    p_vout->p_sys->i_col = var_CreateGetInteger( p_vout, CFG_PREFIX "cols" );
    p_vout->p_sys->i_row = var_CreateGetInteger( p_vout, CFG_PREFIX "rows" );

// OS dependent code :  Autodetect number of displays in wall
#ifdef WIN32
    if ((p_vout->p_sys->i_col < 0) || (p_vout->p_sys->i_row < 0) )
    {
        int nbMonitors = GetSystemMetrics(SM_CMONITORS);
        if (nbMonitors == 1)
        {
            nbMonitors = 5; // 1 display => 5x1 simulation
            p_vout->p_sys->i_col = nbMonitors;
            p_vout->p_sys->i_row = 1;
        }
        else
        {
            p_vout->p_sys->i_col = GetSystemMetrics( SM_CXVIRTUALSCREEN ) / GetSystemMetrics( SM_CXSCREEN );
            p_vout->p_sys->i_row = GetSystemMetrics( SM_CYVIRTUALSCREEN ) / GetSystemMetrics( SM_CYSCREEN );
            if (p_vout->p_sys->i_col * p_vout->p_sys->i_row != nbMonitors)
            {
                p_vout->p_sys->i_col = nbMonitors;
                p_vout->p_sys->i_row = 1;
            }
        }
        var_SetInteger( p_vout, CFG_PREFIX "cols", p_vout->p_sys->i_col);
        var_SetInteger( p_vout, CFG_PREFIX "rows", p_vout->p_sys->i_row);
    }
#endif

#ifdef OVERLAP
    p_vout->p_sys->i_offset_x = var_CreateGetBool( p_vout, CFG_PREFIX "offset-x" );
    if (p_vout->p_sys->i_col > 2) p_vout->p_sys->i_offset_x = 0; // offset-x is used in case of 2x1 wall & autocrop
    p_vout->p_sys->b_autocrop = !(var_CreateGetInteger( p_vout, "crop-ratio" ) == 0);
    if (!p_vout->p_sys->b_autocrop) p_vout->p_sys->b_autocrop = var_CreateGetInteger( p_vout, "autocrop" );
    p_vout->p_sys->b_attenuate = var_CreateGetBool( p_vout, CFG_PREFIX "attenuate");
    p_vout->p_sys->bz_length = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-length" );
    if (p_vout->p_sys->i_row > 1)
        p_vout->p_sys->bz_height = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-height" );
    else
        p_vout->p_sys->bz_height = 100;
    p_vout->p_sys->bz_begin = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-begin" );
    p_vout->p_sys->bz_middle = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-middle" );
    p_vout->p_sys->bz_end = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-end" );
    p_vout->p_sys->bz_middle_pos = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-middle-pos" );
    double d_p = 100.0 / p_vout->p_sys->bz_middle_pos;
    p_vout->p_sys->i_ratio_max = var_CreateGetInteger( p_vout, "autocrop-ratio-max" ); // in crop module with autocrop ...
    p_vout->p_sys->i_ratio = var_CreateGetInteger( p_vout, "crop-ratio" ); // in crop module with manual ratio ...

    p_vout->p_sys->a_2 = d_p * p_vout->p_sys->bz_begin - (double)(d_p * d_p / (d_p - 1)) * p_vout->p_sys->bz_middle + (double)(d_p / (d_p - 1)) * p_vout->p_sys->bz_end;
    p_vout->p_sys->a_1 = -(d_p + 1) * p_vout->p_sys->bz_begin + (double)(d_p * d_p / (d_p - 1)) * p_vout->p_sys->bz_middle - (double)(1 / (d_p - 1)) * p_vout->p_sys->bz_end;
    p_vout->p_sys->a_0 =  p_vout->p_sys->bz_begin;

#ifdef GAMMA
    p_vout->p_sys->f_gamma_red = var_CreateGetFloat( p_vout, CFG_PREFIX "bz-gamma-red" );
    p_vout->p_sys->f_gamma_green = var_CreateGetFloat( p_vout, CFG_PREFIX "bz-gamma-green" );
    p_vout->p_sys->f_gamma_blue = var_CreateGetFloat( p_vout, CFG_PREFIX "bz-gamma-blue" );
#endif
#ifndef WIN32
    p_vout->p_sys->b_xinerama = var_CreateGetBool( p_vout, CFG_PREFIX "xinerama" );
#endif
#else
    p_vout->p_sys->i_col = __MAX( 1, __MIN( 15, p_vout->p_sys->i_col ) );
    p_vout->p_sys->i_row = __MAX( 1, __MIN( 15, p_vout->p_sys->i_row ) );
#endif

    msg_Dbg( p_vout, "opening a %i x %i wall",
             p_vout->p_sys->i_col, p_vout->p_sys->i_row );

    p_vout->p_sys->pp_vout = calloc( p_vout->p_sys->i_row *
                                     p_vout->p_sys->i_col,
                                     sizeof(struct vout_list_t) );
    if( p_vout->p_sys->pp_vout == NULL )
    {
        free( p_vout->p_sys );
        return VLC_ENOMEM;
    }

    psz_method_tmp =
    psz_method = var_CreateGetNonEmptyString( p_vout, CFG_PREFIX "active" );

    /* If no trailing vout are specified, take them all */
    if( psz_method == NULL )
    {
        for( i_vout = p_vout->p_sys->i_row * p_vout->p_sys->i_col;
             i_vout--; )
        {
            p_vout->p_sys->pp_vout[i_vout].b_active = 1;
        }
    }
    /* If trailing vout are specified, activate only the requested ones */
    else
    {
        for( i_vout = p_vout->p_sys->i_row * p_vout->p_sys->i_col;
             i_vout--; )
        {
            p_vout->p_sys->pp_vout[i_vout].b_active = 0;
        }

        while( *psz_method )
        {
            psz_tmp = psz_method;
            while( *psz_tmp && *psz_tmp != ',' )
            {
                psz_tmp++;
            }

            if( *psz_tmp )
            {
                *psz_tmp = '\0';
                i_vout = atoi( psz_method );
                psz_method = psz_tmp + 1;
            }
            else
            {
                i_vout = atoi( psz_method );
                psz_method = psz_tmp;
            }

            if( i_vout >= 0 &&
                i_vout < p_vout->p_sys->i_row * p_vout->p_sys->i_col )
            {
                p_vout->p_sys->pp_vout[i_vout].b_active = 1;
            }
        }
    }

    free( psz_method_tmp );

    return VLC_SUCCESS;
}


#ifdef OVERLAP
/*****************************************************************************
 * CLIP_0A: clip between 0 and ACCURACY
 *****************************************************************************/
inline static int CLIP_0A( int a )
{
    return (a > ACCURACY) ? ACCURACY : (a < 0) ? 0 : a;
}

#ifdef GAMMA
/*****************************************************************************
 *  Gamma: Gamma correction
 *****************************************************************************/
static double Gamma_Correction(int i_plane, float f_component, float f_BlackCrush[VOUT_MAX_PLANES], float f_WhiteCrush[VOUT_MAX_PLANES], float f_BlackLevel[VOUT_MAX_PLANES], float f_WhiteLevel[VOUT_MAX_PLANES], float f_Gamma[VOUT_MAX_PLANES])
{
    float f_Input;

    f_Input = (f_component * f_BlackLevel[i_plane]) / (f_BlackCrush[i_plane]) + (1.0 - f_BlackLevel[i_plane]);
    if (f_component <= f_BlackCrush[i_plane])
    {
        return pow(f_Input, 1.0 / f_Gamma[i_plane]);
    }
    else if (f_component >= f_WhiteCrush[i_plane])
    {
        f_Input = (f_component * (1.0 - (f_WhiteLevel[i_plane] + 1.0)) + (f_WhiteLevel[i_plane] + 1.0) * f_WhiteCrush[i_plane] - 1.0) / (f_WhiteCrush[i_plane] - 1.0);
        return pow(f_Input, 1.0 / f_Gamma[i_plane]);
    }
    else
    {
        return 1.0;
    }
}

#ifdef PACKED_YUV

/*****************************************************************************
 * F: Function to calculate Gamma correction
 *****************************************************************************/
static uint8_t F(uint8_t i, float gamma)
{
    double input = (double) i / 255.0;

    // return clip(255 * pow(input, 1.0 / gamma));

    if (input < 0.5)
        return clip_uint8((255 * pow(2 * input, gamma)) / 2);
    else
        return clip_uint8(255 * (1 - pow(2 * (1 - input), gamma) / 2));
}
#endif
#endif

/*****************************************************************************
 * AdjustHeight: ajust p_sys->i_height to have same BZ width for any ratio
 *****************************************************************************/
static int AdjustHeight( vout_thread_t *p_vout )
{
    bool b_fullscreen = p_vout->b_fullscreen;
    int i_window_width = p_vout->i_window_width;
    int i_window_height = p_vout->i_window_height;
    double d_halfLength = 0;
    double d_halfLength_crop;
    double d_halfLength_calculated;
    int    i_offset = 0;

    // OS DEPENDENT CODE to get display dimensions
    if (b_fullscreen )
    {
#ifdef WIN32
        i_window_width  = GetSystemMetrics(SM_CXSCREEN);
        i_window_height = GetSystemMetrics(SM_CYSCREEN);
#else
        char *psz_display = var_CreateGetNonEmptyString( p_vout,
                                                        "x11-display" );
        Display *p_display = XOpenDisplay( psz_display );
        free( psz_display );
        if (p_vout->p_sys->b_xinerama)
        {
            i_window_width = DisplayWidth(p_display, 0) / p_vout->p_sys->i_col;
            i_window_height = DisplayHeight(p_display, 0) / p_vout->p_sys->i_row;
        }
        else
        {
            i_window_width = DisplayWidth(p_display, 0);
            i_window_height = DisplayHeight(p_display, 0);
        }
        XCloseDisplay( p_display );
#endif
        var_SetInteger( p_vout, "width", i_window_width);
        var_SetInteger( p_vout, "height", i_window_height);
        p_vout->i_window_width = i_window_width;
        p_vout->i_window_height = i_window_height;
    }

    if( p_vout->p_sys->bz_length)
        if ((!p_vout->p_sys->b_autocrop) && (!p_vout->p_sys->i_ratio))
        {
            if ((p_vout->p_sys->i_row > 1) || (p_vout->p_sys->i_col > 1))
            {
                while ((d_halfLength <= 0) || (d_halfLength > p_vout->render.i_width / (2 * p_vout->p_sys->i_col)))
                {
                    if (p_vout->p_sys->bz_length >= 50)
                    {
                        d_halfLength = i_window_width * p_vout->render.i_height / (2 * i_window_height * p_vout->p_sys->i_row) - p_vout->render.i_width / (2 * p_vout->p_sys->i_col);
                    }
                    else
                    {
                        d_halfLength = (p_vout->render.i_width * p_vout->p_sys->bz_length) / (100.0 * p_vout->p_sys->i_col);
                        d_halfLength = __MAX(i_window_width * p_vout->render.i_height / (2 * i_window_height * p_vout->p_sys->i_row) - p_vout->render.i_width / (2 * p_vout->p_sys->i_col), d_halfLength);
                    }
                    if ((d_halfLength <= 0) || (d_halfLength > p_vout->render.i_width / (2 * p_vout->p_sys->i_col)))
                        p_vout->p_sys->i_row--;
                    if (p_vout->p_sys->i_row < 1 )
                    {
                        p_vout->p_sys->i_row = 1;
                        break;
                    }
                }
                p_vout->p_sys->i_halfLength = (d_halfLength + 0.5);
                p_vout->p_sys->bz_length = (p_vout->p_sys->i_halfLength * 100.0 * p_vout->p_sys->i_col) / p_vout->render.i_width;
                var_SetInteger( p_vout, "bz-length", p_vout->p_sys->bz_length);
                var_SetInteger( p_vout, "panoramix-rows", p_vout->p_sys->i_row);
            }
        }
        else
        {
            d_halfLength = ((2 * (double)i_window_width - (double)(p_vout->p_sys->i_ratio_max * i_window_height) / 1000.0 ) * (double)p_vout->p_sys->bz_length) / 200.0;
            d_halfLength_crop = d_halfLength * VOUT_ASPECT_FACTOR * (double)p_vout->output.i_width
                        / (double)i_window_height / (double)p_vout->render.i_aspect;
            p_vout->p_sys->i_halfLength = (d_halfLength_crop + 0.5);
            d_halfLength_calculated = p_vout->p_sys->i_halfLength * (double)i_window_height *
                                (double)p_vout->render.i_aspect  /     VOUT_ASPECT_FACTOR / (double)p_vout->output.i_width;

            if (!p_vout->p_sys->b_attenuate)
            {
                double d_bz_length = (p_vout->p_sys->i_halfLength * p_vout->p_sys->i_col * 100.0) / p_vout->render.i_width;
                // F(2x) != 2F(x) in opengl module
                if (p_vout->p_sys->i_col == 2) d_bz_length = (100.0 * d_bz_length) / (100.0 - d_bz_length) ;
                var_SetInteger( p_vout, "bz-length", (int)(d_bz_length + 0.5));
            }
            i_offset =  (int)d_halfLength - (int)
                        (p_vout->p_sys->i_halfLength * (double)i_window_height *
                        (double)p_vout->render.i_aspect  /     VOUT_ASPECT_FACTOR / (double)p_vout->output.i_width);
        }
    else
        p_vout->p_sys->i_halfLength = 0;

    return i_offset;
}
#endif


/*****************************************************************************
 * Init: initialize Wall video thread output method
 *****************************************************************************/
#define VLC_XCHG( type, a, b ) do { type __tmp = (b); (b) = (a); (a) = __tmp; } while(0)

static int Init( vout_thread_t *p_vout )
{
    int i_index, i_row, i_col;

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;
#ifdef OVERLAP
    p_vout->p_sys->b_has_changed = p_vout->p_sys->b_attenuate;
    int i_video_x = var_GetInteger( p_vout, "video-x");
    int i_video_y = var_GetInteger( p_vout, "video-y");
#ifdef GAMMA
    if (p_vout->p_sys->b_attenuate)
    {
        int i_index2, i_plane;
        int constantYUV[3] = {0,128,128};
        float    f_BlackCrush[VOUT_MAX_PLANES];
        float    f_BlackLevel[VOUT_MAX_PLANES];
        float    f_WhiteCrush[VOUT_MAX_PLANES];
        float    f_WhiteLevel[VOUT_MAX_PLANES];
        p_vout->p_sys->f_gamma[0] = var_CreateGetFloat( p_vout, CFG_PREFIX "bz-gamma-red" );
        p_vout->p_sys->f_gamma[1] = var_CreateGetFloat( p_vout, CFG_PREFIX "bz-gamma-green" );
        p_vout->p_sys->f_gamma[2] = var_CreateGetFloat( p_vout, CFG_PREFIX "bz-gamma-blue" );
        f_BlackCrush[0] = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-blackcrush-red" ) / 255.0;
        f_BlackCrush[1] = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-blackcrush-green" ) / 255.0;
        f_BlackCrush[2] = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-blackcrush-blue" ) / 255.0;
        f_WhiteCrush[0] = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-whitecrush-red" ) / 255.0;
        f_WhiteCrush[1] = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-whitecrush-green" ) / 255.0;
        f_WhiteCrush[2] = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-whitecrush-blue" ) / 255.0;
        f_BlackLevel[0] = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-blacklevel-red" ) / 255.0;
        f_BlackLevel[1] = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-blacklevel-green" ) / 255.0;
        f_BlackLevel[2] = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-blacklevel-blue" ) / 255.0;
        f_WhiteLevel[0] = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-whitelevel-red" ) / 255.0;
        f_WhiteLevel[1] = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-whitelevel-green" ) / 255.0;
        f_WhiteLevel[2] = var_CreateGetInteger( p_vout, CFG_PREFIX "bz-whitelevel-blue" ) / 255.0;
        for( int i = 3; i < VOUT_MAX_PLANES; i++ )
        {
            /* Initialize unsupported planes */
            f_BlackCrush[i] = 140.0/255.0;
            f_WhiteCrush[i] = 200.0/255.0;
            f_BlackLevel[i] = 150.0/255.0;
            f_WhiteLevel[i] = 0.0/255.0;
            p_vout->p_sys->f_gamma[i] = 1.0;
        }

        switch (p_vout->render.i_chroma)
        {
        // planar YVU
            case VLC_FOURCC('Y','V','1','2'):
            case VLC_FOURCC('Y','V','U','9'):
        // packed UYV
            case VLC_FOURCC('U','Y','V','Y'):    // packed by 2
            case VLC_FOURCC('U','Y','N','V'):    // packed by 2
            case VLC_FOURCC('Y','4','2','2'):    // packed by 2
    //        case VLC_FOURCC('c','y','u','v'):    // packed by 2
                VLC_XCHG( float, p_vout->p_sys->f_gamma[1], p_vout->p_sys->f_gamma[2] );
                VLC_XCHG( float, f_BlackCrush[1], f_BlackCrush[2] );
                VLC_XCHG( float, f_WhiteCrush[1], f_WhiteCrush[2] );
                VLC_XCHG( float, f_BlackLevel[1], f_BlackLevel[2] );
                VLC_XCHG( float, f_WhiteLevel[1], f_WhiteLevel[2] );
        // planar YUV
            case VLC_FOURCC('I','4','4','4'):
            case VLC_FOURCC('I','4','2','2'):
            case VLC_FOURCC('I','4','2','0'):
            case VLC_FOURCC('I','4','1','1'):
            case VLC_FOURCC('I','4','1','0'):
            case VLC_FOURCC('I','Y','U','V'):
            case VLC_FOURCC('Y','U','V','A'):
        // packed YUV
            case VLC_FOURCC('Y','U','Y','2'):    // packed by 2
            case VLC_FOURCC('Y','U','N','V'):    // packed by 2
                for (i_index = 0; i_index < 256; i_index++)
                    for (i_index2 = 0; i_index2 <= ACCURACY; i_index2++)
                        for (i_plane = 0; i_plane < VOUT_MAX_PLANES; i_plane++)
                        {
                            float f_lut = CLIP_01(1.0 -
                                     ((ACCURACY - (float)i_index2)
                                     * Gamma_Correction(i_plane, (float)i_index / 255.0, f_BlackCrush, f_WhiteCrush, f_BlackLevel, f_WhiteLevel, p_vout->p_sys->f_gamma)
                                     / (ACCURACY - 1)));
                            p_vout->p_sys->LUT[i_plane][i_index2][i_index] = f_lut * i_index + (int)((1.0 - f_lut) * (float)constantYUV[i_plane]);
                        }
                break;
        // packed RGB
            case VLC_FOURCC('R','G','B','2'):    // packed by 1
            case VLC_FOURCC('R','V','1','5'):    // packed by 2
            case VLC_FOURCC('R','V','1','6'):    // packed by 2
            case VLC_FOURCC('R','V','2','4'):    // packed by 3
            case VLC_FOURCC('R','V','3','2'):    // packed by 4
            for (i_index = 0; i_index < 256; i_index++)
                    for (i_index2 = 0; i_index2 <= ACCURACY; i_index2++)
                        for (i_plane = 0; i_plane < VOUT_MAX_PLANES; i_plane++)
                        {
                            float f_lut = CLIP_01(1.0 -
                                     ((ACCURACY - (float)i_index2)
                                     * Gamma_Correction(i_plane, (float)i_index / 255.0, f_BlackCrush, f_WhiteCrush, f_BlackLevel, f_WhiteLevel, p_vout->p_sys->f_gamma)
                                     / (ACCURACY - 1)));
                            p_vout->p_sys->LUT[i_plane][i_index2][i_index] = f_lut * i_index;
                        }
                break;
            default:
                msg_Err( p_vout, "colorspace not supported by plug-in !!!");
                free( p_vout->p_sys );
                return VLC_ENOMEM;
        }
    }
#endif
    if (p_vout->p_sys->i_offset_x)
        p_vout->p_sys->i_offset_x = AdjustHeight(p_vout);
    else
        AdjustHeight(p_vout);
    if (p_vout->p_sys->i_row >= 2)
    {
        p_vout->p_sys->i_halfHeight = (p_vout->p_sys->i_halfLength * p_vout->p_sys->bz_height) / 100;
        p_vout->p_sys->i_halfHeight -= (p_vout->p_sys->i_halfHeight % 2);
    }
#endif

    /* Try to open the real video output */
    msg_Dbg( p_vout, "spawning the real video outputs" );

    /* FIXME: use bresenham instead of those ugly divisions */
    p_vout->p_sys->i_vout = 0;
    for( i_row = 0; i_row < p_vout->p_sys->i_row; i_row++ )
    {
        for( i_col = 0; i_col < p_vout->p_sys->i_col; i_col++, p_vout->p_sys->i_vout++ )
        {
            struct vout_list_t *p_entry = &p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ];
            video_format_t fmt;
            int i_width, i_height;

            /* */
            i_width = ( p_vout->render.i_width / p_vout->p_sys->i_col ) & ~0x1;
            if( i_col + 1 == p_vout->p_sys->i_col )
                i_width = p_vout->render.i_width - i_col * i_width;

#ifdef OVERLAP
            i_width += p_vout->p_sys->i_halfLength;
            if (p_vout->p_sys->i_col > 2 )
                i_width += p_vout->p_sys->i_halfLength;
            i_width &= ~0x1;
#endif

            /* */
            i_height = ( p_vout->render.i_height / p_vout->p_sys->i_row ) & ~0x3;
            if( i_row + 1 == p_vout->p_sys->i_row )
                i_height = p_vout->render.i_height - i_row * i_height;
#ifdef OVERLAP
            if(p_vout->p_sys->i_row >= 2 )
            {
                i_height += p_vout->p_sys->i_halfHeight;
                if( p_vout->p_sys->i_row > 2 )
                    i_height += p_vout->p_sys->i_halfHeight;
            }
            i_height &= ~0x1;
#endif
            p_entry->i_width = i_width;
            p_entry->i_height = i_height;

            if( !p_entry->b_active )
                continue;

            /* */
            memset( &fmt, 0, sizeof(video_format_t) );
            fmt.i_width = fmt.i_visible_width = p_vout->render.i_width;
            fmt.i_height = fmt.i_visible_height = p_vout->render.i_height;
            fmt.i_x_offset = fmt.i_y_offset = 0;
            fmt.i_chroma = p_vout->render.i_chroma;
            fmt.i_aspect = p_vout->render.i_aspect;
            fmt.i_sar_num = p_vout->render.i_aspect * fmt.i_height / fmt.i_width;
            fmt.i_sar_den = VOUT_ASPECT_FACTOR;
            fmt.i_width = fmt.i_visible_width = i_width;
            fmt.i_height = fmt.i_visible_height = i_height;
            fmt.i_aspect = p_vout->render.i_aspect
                              * p_vout->render.i_height / i_height
                              * i_width / p_vout->render.i_width;
#ifdef OVERLAP
            if (p_vout->p_sys->i_offset_x < 0)
            {
                var_SetInteger(p_vout, "video-x", -p_vout->p_sys->i_offset_x);
                p_vout->p_sys->i_offset_x = 0;
            }
#endif
            p_entry->p_vout = vout_Create( p_vout, &fmt);

            if( p_entry->p_vout == NULL )
            {
                msg_Err( p_vout, "failed to get %ix%i vout threads",
                                 p_vout->p_sys->i_col, p_vout->p_sys->i_row );
                RemoveAllVout( p_vout );
                return VLC_EGENERIC;
            }
            vout_filter_SetupChild( p_vout, p_entry->p_vout,
                                    MouseEvent, FullscreenEventUp, FullscreenEventDown, true );

#ifdef OVERLAP
            p_entry->p_vout->i_alignment = 0;
            if (i_col == 0)
                p_entry->p_vout->i_alignment |= VOUT_ALIGN_RIGHT;
            else if (i_col == p_vout->p_sys->i_col -1)
                p_entry->p_vout->i_alignment |= VOUT_ALIGN_LEFT;
            if (p_vout->p_sys->i_row > 1)
            {
                if (i_row == 0)
                    p_entry->p_vout->i_alignment |= VOUT_ALIGN_BOTTOM;
                else if (i_row == p_vout->p_sys->i_row -1)
                    p_entry->p_vout->i_alignment |= VOUT_ALIGN_TOP;
            }
            // i_active : number of active pp_vout
            int i_active = 0;
            for( int i = 0; i <= p_vout->p_sys->i_vout; i++ )
            {
                if( p_vout->p_sys->pp_vout[i].b_active )
                    i_active++;
            }
            var_SetInteger( p_vout, "align", p_entry->p_vout->i_alignment );
            var_SetInteger( p_vout, "video-x", i_video_x + p_vout->p_sys->i_offset_x + (i_active % p_vout->p_sys->i_col) * p_vout->i_window_width);
            var_SetInteger( p_vout, "video-y", i_video_y +                             (i_active / p_vout->p_sys->i_col) * p_vout->i_window_height);
#endif
        }
    }

    vout_filter_AllocateDirectBuffers( p_vout, VOUT_MAX_PICTURES );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate Wall video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    RemoveAllVout( p_vout );

    vout_filter_ReleaseDirectBuffers( p_vout );

#ifdef OVERLAP
    var_SetInteger( p_vout, "bz-length", p_vout->p_sys->bz_length);
#endif
}

/*****************************************************************************
 * Destroy: destroy Wall video thread output method
 *****************************************************************************
 * Terminate an output method created by WallCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    free( p_vout->p_sys->pp_vout );
    free( p_vout->p_sys );

}

/*****************************************************************************
 * RenderPlanarYUV: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Wall image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void RenderPlanarYUV( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic = NULL;
    int i_col, i_row, i_vout, i_plane;
    int pi_left_skip[VOUT_MAX_PLANES], pi_top_skip[VOUT_MAX_PLANES];
#ifdef OVERLAP
    int TopOffset;
    int constantYUV[3] = {0,128,128};
    int Denom;
    int a_2;
    int a_1;
    int a_0;
    int i_index, i_index2;
#endif

    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
        pi_top_skip[i_plane] = 0;

    for( i_vout = 0, i_row = 0; i_row < p_vout->p_sys->i_row; i_row++ )
    {
        for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
            pi_left_skip[i_plane] = 0;

        for( i_col = 0; i_col < p_vout->p_sys->i_col; i_col++, i_vout++ )
        {
            struct vout_list_t *p_entry = &p_vout->p_sys->pp_vout[ i_vout ];
            if( !p_entry->b_active )
            {
                for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
                {
                    pi_left_skip[i_plane] += p_entry->i_width * p_pic->p[i_plane].i_pitch / p_vout->output.i_width;
                }
                continue;
            }

            while( ( p_outpic = vout_CreatePicture( p_entry->p_vout, 0, 0, 0 )) == NULL )
            {
                if( !vlc_object_alive(p_vout) || p_vout->b_error )
                {
                    vout_DestroyPicture( p_entry->p_vout, p_outpic );
                    return;
                }
                msleep( VOUT_OUTMEM_SLEEP );
            }

            p_outpic->date = p_pic->date;
            vout_LinkPicture( p_entry->p_vout, p_outpic );

            for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
            {
                uint8_t *p_in, *p_in_end, *p_out;
                int i_in_pitch = p_pic->p[i_plane].i_pitch;
                int i_out_pitch = p_outpic->p[i_plane].i_pitch;
                int i_copy_pitch = p_outpic->p[i_plane].i_visible_pitch;
                int i_lines = p_outpic->p[i_plane].i_visible_lines;
                const int i_div = p_entry->i_width / i_copy_pitch;

                const bool b_row_first = i_row == 0;
                const bool b_row_last = i_row + 1 == p_vout->p_sys->i_row;
                const bool b_col_first = i_col == 0;
                const bool b_col_last = i_col + 1 == p_vout->p_sys->i_col;

#ifdef OVERLAP
                if( !b_col_first )
                    pi_left_skip[i_plane] -= (2 * p_vout->p_sys->i_halfLength ) / i_div;

                if( p_vout->p_sys->i_row >= 2 )
                {
                    if( !b_row_first && b_col_first )
                        pi_top_skip[i_plane] -= (2 * p_vout->p_sys->i_halfHeight * p_pic->p[i_plane].i_pitch) / i_div;
                    if( p_vout->p_sys->i_row > 2 && i_row == 1 && b_col_first )
                        pi_top_skip[i_plane] -= (2 * p_vout->p_sys->i_halfHeight * p_pic->p[i_plane].i_pitch) / i_div;
                    if( !p_vout->p_sys->pp_vout[p_vout->p_sys->i_col-1].b_active )
                        pi_top_skip[i_plane] -= (2 * p_vout->p_sys->i_halfHeight * i_row * p_pic->p[i_plane].i_pitch) / i_div;
                }
// i_n : previous inactive pp_vout
                int i_n=0;
                while( (i_col - i_n > 1) && (!p_vout->p_sys->pp_vout[i_row * p_vout->p_sys->i_col + i_col - 1 - i_n].b_active) ) i_n++;
                if( i_col > 1 && i_n )
                    pi_left_skip[i_plane] -= i_n * (2 * p_vout->p_sys->i_halfLength ) / i_div;


                if( p_vout->p_sys->i_row > 2 && ( b_row_first || b_row_last ) )
                    i_lines -= (2 * p_vout->p_sys->i_halfHeight) / i_div;

// 1088 lines bug in a mpeg2 stream of 1080 lines
                if( b_row_last && p_pic->p[i_plane].i_lines == 1088 )
                    i_lines -= 8 / i_div;
#endif
                /* */
                p_in = &p_pic->p[i_plane].p_pixels[ pi_top_skip[i_plane] + pi_left_skip[i_plane] ]; /* Wall proprities */
                p_in_end = &p_in[i_lines * p_pic->p[i_plane].i_pitch];

                p_out = p_outpic->p[i_plane].p_pixels;
#ifdef OVERLAP
                if( p_vout->p_sys->i_row > 2 && b_row_first )
                    p_out += p_outpic->p[i_plane].i_pitch * (2 * p_vout->p_sys->i_halfHeight) / i_div;

                int i_col_mod;
                int length = 2 * p_vout->p_sys->i_halfLength / i_div;

                if( p_vout->p_sys->b_has_changed )
                {
                    Denom = F2(length);
                    a_2 = p_vout->p_sys->a_2 * (ACCURACY / 100);
                    a_1 = p_vout->p_sys->a_1 * length * (ACCURACY / 100);
                    a_0 = p_vout->p_sys->a_0 * Denom * (ACCURACY / 100);
                    for( i_col_mod = 0; i_col_mod < 2; i_col_mod++ )
                    {
                        for( i_index = 0; i_index < length; i_index++ )
                        {
                            p_vout->p_sys->lambda[i_col_mod][i_plane][i_index] = CLIP_0A(!i_col_mod ? ACCURACY - (F4(a_2, a_1, i_index) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,length - i_index) + a_0) / Denom);
                            p_vout->p_sys->cstYUV[i_col_mod][i_plane][i_index] = ((ACCURACY - p_vout->p_sys->lambda[i_col_mod][i_plane][i_index]) * constantYUV[i_plane]) / ACCURACY;
                        }
                    }
                }
#endif
                while( p_in < p_in_end )
                {
#ifndef OVERLAP
                    vlc_memcpy( p_out, p_in, i_copy_pitch);
#else
                    if( p_vout->p_sys->i_col > 2 )
                    {
                        const int halfl = length / 2;
                        if( b_col_first)
                            vlc_memcpy( &p_out[halfl], &p_in[0], i_copy_pitch - halfl );
                        else if( b_col_last )
                            vlc_memcpy( &p_out[    0], &p_in[-halfl], i_copy_pitch - halfl );
                        else
                            vlc_memcpy( &p_out[    0], &p_in[-halfl], i_copy_pitch);

                        // black bar
                        if( b_col_first )
                            memset( &p_out[0], constantYUV[i_plane], halfl);
                        else if( b_col_last )
                            memset( &p_out[i_copy_pitch - halfl], constantYUV[i_plane], halfl );
                    }
                    else
                    {
                        vlc_memcpy( p_out , p_in, i_copy_pitch );
                    }

                    if( p_vout->p_sys->b_attenuate )
                    {
                        // vertical blend
                        // first blended zone
                        if( !b_col_first )
                        {
                            uint8_t *p_dst = &p_out[0];
                            for (i_index = 0; i_index < length; i_index++)
                            {
#ifndef GAMMA
                                p_dst[i_index] = (p_vout->p_sys->lambda[1][i_plane][i_index] * p_dst[i_index]) / ACCURACY +
                                                        p_vout->p_sys->cstYUV[1][i_plane][i_index];
#else
                                p_dst[i_index] = p_vout->p_sys->LUT[i_plane][p_vout->p_sys->lambda[1][i_plane][i_index]][p_dst[i_index]];
#endif
                            }
                        }
                        // second blended zone
                        if( !b_col_last )
                        {
                            uint8_t *p_dst = &p_out[i_copy_pitch - length];
                            for (i_index = 0; i_index < length; i_index++)
                            {
#ifndef GAMMA
                                p_dst[i_index] = (p_vout->p_sys->lambda[0][i_plane][i_index] * p_dst[i_index]) / ACCURACY +
                                                        p_vout->p_sys->cstYUV[0][i_plane][i_index];
#else
                               p_dst[i_index] = p_vout->p_sys->LUT[i_plane][p_vout->p_sys->lambda[0][i_plane][i_index]][p_dst[i_index]];
#endif
                            }
                        }
                        // end blended zone
                    }
#endif
                    p_in += i_in_pitch;
                    p_out += i_out_pitch;
                }
#ifdef OVERLAP
       // horizontal blend
        if ( p_vout->p_sys->i_row >= 2 )
        {
           // black bar
           if (( p_vout->p_sys->i_row > 2 ) && (( b_row_first ) || ( b_row_last )))
           {

               int height = 2 * p_vout->p_sys->i_halfHeight / i_div;
               if ( b_row_first )
               {
                    TopOffset = i_lines + (2 * p_vout->p_sys->i_halfHeight) / i_div;
               }
               else
                {
                   TopOffset = height - (2 * p_vout->p_sys->i_halfHeight) / i_div;
                }
                uint8_t *p_dst = p_out - TopOffset * i_out_pitch;
                for (i_index = 0; i_index < height; i_index++)
                   for (i_index2 = 0; i_index2 < i_copy_pitch; i_index2++)
                       p_dst[i_index * i_out_pitch + i_index2] = constantYUV[i_plane];
           }
           if( p_vout->p_sys->b_attenuate )
           {
               length = 2 * p_vout->p_sys->i_halfHeight / (p_vout->p_sys->pp_vout[i_vout].i_width / i_copy_pitch);
               if (p_vout->p_sys->b_has_changed)
               {
                   Denom = F2(length);
                   a_2 = p_vout->p_sys->a_2 * (ACCURACY / 100);
                   a_1 = p_vout->p_sys->a_1 * length * (ACCURACY / 100);
                   a_0 = p_vout->p_sys->a_0 * Denom * (ACCURACY / 100);
                   for(i_col_mod = 0; i_col_mod < 2; i_col_mod++)
                       for (i_index = 0; i_index < length; i_index++)
                       {
                           p_vout->p_sys->lambda2[i_col_mod][i_plane][i_index] = CLIP_0A(!i_col_mod ? ACCURACY - (F4(a_2, a_1, i_index) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,length - i_index) + a_0) / Denom);
                           p_vout->p_sys->cstYUV2[i_col_mod][i_plane][i_index] = ((ACCURACY - p_vout->p_sys->lambda2[i_col_mod][i_plane][i_index]) * constantYUV[i_plane]) / ACCURACY;
                       }
               }
               // first blended zone
               if ( !b_row_first )
               {
                        TopOffset = i_lines;
                        uint8_t *p_dst = p_out - TopOffset * i_out_pitch;

                        for (i_index = 0; i_index < length; i_index++)
                        {
                            for (i_index2 = 0; i_index2 < i_copy_pitch; i_index2++)
                            {
#ifndef GAMMA
                                p_dst[i_index * i_out_pitch + i_index2] = ( p_vout->p_sys->lambda2[1][i_plane][i_index] *
                                             p_dst[i_index * i_out_pitch + i_index2] ) / ACCURACY +
                                             p_vout->p_sys->cstYUV2[1][i_plane][i_index];
#else
                                p_dst[i_index * i_out_pitch + i_index2] = p_vout->p_sys->LUT[i_plane][p_vout->p_sys->lambda2[1][i_plane][i_index]][p_dst[i_index * i_out_pitch + i_index2]];
#endif
                            }
                        }
               }
               // second blended zone
               if ( !b_row_last )
               {
                        TopOffset = length;
                        uint8_t *p_dst = p_out - TopOffset * p_outpic->p[i_plane].i_pitch;

                        for (i_index = 0; i_index < length; i_index++)
                        {
                            for (i_index2 = 0; i_index2 < i_copy_pitch; i_index2++)
                            {
#ifndef GAMMA
                                p_dst[i_index * i_out_pitch + i_index2] = (p_vout->p_sys->lambda2[0][i_plane][i_index] *
                                             p_dst[i_index * i_out_pitch + i_index2]) / ACCURACY +
                                             p_vout->p_sys->cstYUV2[0][i_plane][i_index];
#else

                                p_dst[i_index * i_out_pitch + i_index2] = p_vout->p_sys->LUT[i_plane][p_vout->p_sys->lambda2[0][i_plane][i_index]][p_dst[i_index * i_out_pitch + i_index2]];
#endif
                            }
                        }
               }
           }
        }
       // end blended zone
#endif
                // bug for wall filter : fix by CC
                //            pi_left_skip[i_plane] += i_out_pitch;
                pi_left_skip[i_plane] += i_copy_pitch;
            }

            vout_UnlinkPicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                                p_outpic );
            vout_DisplayPicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                                 p_outpic );
        }

        for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
        {
            pi_top_skip[i_plane] += p_vout->p_sys->pp_vout[ i_vout-1 ].i_height
                                             * p_pic->p[i_plane].i_lines
                                             / p_vout->output.i_height
                                             * p_pic->p[i_plane].i_pitch;
        }
    }
#ifdef OVERLAP
    if (p_vout->p_sys->b_has_changed)
        p_vout->p_sys->b_has_changed = false;
#endif
}


/*****************************************************************************
 * RenderPackedRGB: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Wall image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void RenderPackedRGB( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic = NULL;
    int i_col, i_row, i_vout, i_plane;
    int pi_left_skip[VOUT_MAX_PLANES], pi_top_skip[VOUT_MAX_PLANES];
#ifdef OVERLAP
    int LeftOffset, TopOffset;
    int Denom;
    int a_2;
    int a_1;
    int a_0;
    int i_index, i_index2;
#endif

    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
        pi_top_skip[i_plane] = 0;

    for( i_vout = 0, i_row = 0; i_row < p_vout->p_sys->i_row; i_row++ )
    {
        for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
            pi_left_skip[i_plane] = 0;

        for( i_col = 0; i_col < p_vout->p_sys->i_col; i_col++, i_vout++ )
        {
            if( !p_vout->p_sys->pp_vout[ i_vout ].b_active )
            {
                for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
                {
                    pi_left_skip[i_plane] +=
                        p_vout->p_sys->pp_vout[ i_vout ].i_width * p_pic->p->i_pixel_pitch;
                }
                continue;
            }

            while( ( p_outpic =
                vout_CreatePicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                                    0, 0, 0 )
                   ) == NULL )
            {
                if( !vlc_object_alive (p_vout) || p_vout->b_error )
                {
                    vout_DestroyPicture(
                        p_vout->p_sys->pp_vout[ i_vout ].p_vout, p_outpic );
                    return;
                }

                msleep( VOUT_OUTMEM_SLEEP );
            }

            p_outpic->date = p_pic->date;
            vout_LinkPicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                              p_outpic );

            for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
            {
                uint8_t *p_in, *p_in_end, *p_out;
                int i_in_pitch = p_pic->p[i_plane].i_pitch;
                int i_out_pitch = p_outpic->p[i_plane].i_pitch;
                int i_copy_pitch = p_outpic->p[i_plane].i_visible_pitch;

#ifdef OVERLAP
                if (i_col)
                    pi_left_skip[i_plane] -= (2 * p_vout->p_sys->i_halfLength) * p_pic->p->i_pixel_pitch;
                if( p_vout->p_sys->i_row >= 2 )
                {
                    if( (i_row) && (!i_col))
                        pi_top_skip[i_plane] -= (2 * p_vout->p_sys->i_halfHeight * p_pic->p[i_plane].i_pitch);
                    if( (p_vout->p_sys->i_row > 2) && (i_row == 1) && (!i_col) )
                        pi_top_skip[i_plane] -= (2 * p_vout->p_sys->i_halfHeight * p_pic->p[i_plane].i_pitch);
                    if( !p_vout->p_sys->pp_vout[p_vout->p_sys->i_col-1].b_active )
                        pi_top_skip[i_plane] -= (2 * p_vout->p_sys->i_halfHeight * i_row * p_pic->p[i_plane].i_pitch);
                }
// i_n : previous inactive pp_vout
                int i_n=0;
                while ((!p_vout->p_sys->pp_vout[i_row * p_vout->p_sys->i_col + i_col - 1 - i_n].b_active) && (i_col - i_n > 1)) i_n++;
                if ((i_col > 1) && i_n)
                    pi_left_skip[i_plane] -= i_n*(2 * p_vout->p_sys->i_halfLength ) * p_pic->p->i_pixel_pitch;

                p_in = p_pic->p[i_plane].p_pixels
                /* Wall proprities */
                + pi_top_skip[i_plane] + pi_left_skip[i_plane];

                int i_lines = p_outpic->p[i_plane].i_visible_lines;
// 1088 lines bug in a mpeg2 stream of 1080 lines
                if ((p_vout->p_sys->i_row - 1 == i_row) &&
                    (p_pic->p[i_plane].i_lines == 1088))
                        i_lines -= 8;

                p_in_end = p_in + i_lines * p_pic->p[i_plane].i_pitch;
#else
                p_in = p_pic->p[i_plane].p_pixels
                        + pi_top_skip[i_plane] + pi_left_skip[i_plane];

                p_in_end = p_in + p_outpic->p[i_plane].i_visible_lines
                                        * p_pic->p[i_plane].i_pitch;
#endif //OVERLAP

                p_out = p_outpic->p[i_plane].p_pixels;


#ifdef OVERLAP
        if ((p_vout->p_sys->i_row > 2) && (!i_row))
            p_out += (p_outpic->p[i_plane].i_pitch * (2 * p_vout->p_sys->i_halfHeight) * p_pic->p->i_pixel_pitch);

        int length;
        length = 2 * p_vout->p_sys->i_halfLength * p_pic->p->i_pixel_pitch;

        if (p_vout->p_sys->b_has_changed)
        {
            int i_plane_;
            int i_col_mod;
            Denom = F2(length / p_pic->p->i_pixel_pitch);
            a_2 = p_vout->p_sys->a_2 * (ACCURACY / 100);
            a_1 = p_vout->p_sys->a_1 * 2 * p_vout->p_sys->i_halfLength * (ACCURACY / 100);
            a_0 = p_vout->p_sys->a_0 * Denom * (ACCURACY / 100);
            for(i_col_mod = 0; i_col_mod < 2; i_col_mod++)
                for (i_index = 0; i_index < length / p_pic->p->i_pixel_pitch; i_index++)
                    for (i_plane_ =  0; i_plane_ < p_pic->p->i_pixel_pitch; i_plane_++)
                        p_vout->p_sys->lambda[i_col_mod][i_plane_][i_index] = CLIP_0A(!i_col_mod ? ACCURACY - (F4(a_2, a_1, i_index) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length / p_pic->p->i_pixel_pitch) - i_index) + a_0) / Denom);
        }
#endif
            while( p_in < p_in_end )
            {
#ifndef OVERLAP
                vlc_memcpy( p_out, p_in, i_copy_pitch );
#else
                if (p_vout->p_sys->i_col > 2)
                {
                    // vertical blend
                    length /= 2;
                    if (i_col == 0)
                        vlc_memcpy( p_out + length, p_in, i_copy_pitch - length);
                    else if (i_col + 1 == p_vout->p_sys->i_col)
                        vlc_memcpy( p_out, p_in - length, i_copy_pitch - length);
                    else
                        vlc_memcpy( p_out, p_in - length, i_copy_pitch);

                    if ((i_col == 0))
                    // black bar
                    {
                        LeftOffset = 0;
                        p_out += LeftOffset;
                        p_in += LeftOffset;
                        for (i_index = 0; i_index < length; i_index++)
                                *(p_out + i_index) = 0;
                        p_out -= LeftOffset;
                        p_in -= LeftOffset;
                    }
                    else if ((i_col + 1 == p_vout->p_sys->i_col ))
                    // black bar
                        {
                            LeftOffset = i_copy_pitch - length;
                            p_out += LeftOffset;
                            p_in += LeftOffset;
                            for (i_index = 0; i_index < length; i_index++)
                                    *(p_out + i_index) = 0;
                            p_out -= LeftOffset;
                            p_in -= LeftOffset;
                        }
                    length *= 2;
                }
                else
                    vlc_memcpy( p_out, p_in, i_copy_pitch);

// vertical blend
// first blended zone
            if (i_col)
            {
                LeftOffset = 0;
                p_out += LeftOffset;
                for (i_index = 0; i_index < length; i_index++)
#ifndef GAMMA
                    *(p_out + i_index) = (p_vout->p_sys->lambda[1][i_index % p_pic->p->i_pixel_pitch][i_index / p_pic->p->i_pixel_pitch] *
                                 (*(p_out + i_index))) / ACCURACY;
#else
                    *(p_out + i_index) = p_vout->p_sys->LUT[i_index % p_pic->p->i_pixel_pitch][p_vout->p_sys->lambda[1][i_index % p_pic->p->i_pixel_pitch][i_index / p_pic->p->i_pixel_pitch]][*(p_out + i_index)];
#endif
                p_out -= LeftOffset;
            }
// second blended zone
            if (i_col + 1 < p_vout->p_sys->i_col)
            {
                LeftOffset = i_copy_pitch - length;
                p_out +=  LeftOffset;
                for (i_index = 0; i_index < length; i_index++)
#ifndef GAMMA
                    *(p_out + i_index) = (p_vout->p_sys->lambda[0][i_index % p_pic->p->i_pixel_pitch][i_index / p_pic->p->i_pixel_pitch] *
                                 (*(p_out + i_index))) / ACCURACY;
#else
                    *(p_out + i_index) = p_vout->p_sys->LUT[i_index % p_pic->p->i_pixel_pitch][p_vout->p_sys->lambda[0][i_index % p_pic->p->i_pixel_pitch][i_index / p_pic->p->i_pixel_pitch]][*(p_out + i_index)];
#endif
                p_out -= LeftOffset;
            }
// end blended zone
#endif //OVERLAP
                p_in += i_in_pitch;
                p_out += i_out_pitch;
            }
#ifdef OVERLAP
// horizontal blend
        if (!p_vout->p_sys->b_attenuate)
        {
            if ((i_row == 0) && (p_vout->p_sys->i_row > 2))
            // black bar
            {
                    TopOffset = i_lines + (2 * p_vout->p_sys->i_halfHeight);
                    p_out -= TopOffset * p_outpic->p[i_plane].i_pitch;
                    for (i_index = 0; i_index < length; i_index++)
                        for (i_index2 = 0; i_index2 < i_copy_pitch; i_index2++)
                            *(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2) = 0;
                    p_out += TopOffset * p_outpic->p[i_plane].i_pitch;
            }
            else if ((i_row + 1 == p_vout->p_sys->i_row) && (p_vout->p_sys->i_row > 2))
            // black bar
                {
                    TopOffset = length - (2 * p_vout->p_sys->i_halfHeight);
                    p_out -= TopOffset * p_outpic->p[i_plane].i_pitch;
                    for (i_index = 0; i_index < length; i_index++)
                        for (i_index2 = 0; i_index2 < i_copy_pitch; i_index2++)
                            *(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2) = 0;
                    p_out += TopOffset * p_outpic->p[i_plane].i_pitch;
                }
        }
        else
        {
            if (p_vout->p_sys->i_row >= 2)
            {
                length = 2 * p_vout->p_sys->i_halfHeight;
                if (p_vout->p_sys->b_has_changed)
                {
                    int i_plane_;
                    int i_row_mod;
                    Denom = F2(length);
                    a_2 = p_vout->p_sys->a_2 * (ACCURACY / 100);
                    a_1 = p_vout->p_sys->a_1 * length * (ACCURACY / 100);
                    a_0 = p_vout->p_sys->a_0 * Denom * (ACCURACY / 100);
                    for(i_row_mod = 0; i_row_mod < 2; i_row_mod++)
                      for (i_index = 0; i_index < length; i_index++)
                        for (i_plane_ =  0; i_plane_ < p_pic->p->i_pixel_pitch; i_plane_++)
                            p_vout->p_sys->lambda2[i_row_mod][i_plane_][i_index] = CLIP_0A(!i_row_mod ? ACCURACY - (F4(a_2, a_1, i_index) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length) - i_index) + a_0) / Denom);
                }
// first blended zone

            if (i_row)
            {
                TopOffset = i_lines;
                p_out -= TopOffset * p_outpic->p[i_plane].i_pitch;
                for (i_index = 0; i_index < length; i_index++)
                    for (i_index2 = 0; i_index2 < i_copy_pitch; i_index2++)
#ifndef GAMMA
                    *(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2) = (p_vout->p_sys->lambda2[1][i_index2 % p_pic->p->i_pixel_pitch][i_index] *
                                 (*(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2))) / ACCURACY;
#else
                    *(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2) = p_vout->p_sys->LUT[i_index2 % p_pic->p->i_pixel_pitch][p_vout->p_sys->lambda2[1][i_index2 % p_pic->p->i_pixel_pitch][i_index]][*(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2)];
#endif
                p_out += TopOffset * p_outpic->p[i_plane].i_pitch;
            }
            else if (p_vout->p_sys->i_row > 2)
            // black bar
            {
                TopOffset = i_lines + (2 * p_vout->p_sys->i_halfHeight);
                p_out -= TopOffset * p_outpic->p[i_plane].i_pitch;
                for (i_index = 0; i_index < length; i_index++)
                    for (i_index2 = 0; i_index2 < i_copy_pitch; i_index2++)
                        *(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2) = 0;
                p_out += TopOffset * p_outpic->p[i_plane].i_pitch;
            }

// second blended zone

            if (i_row + 1 < p_vout->p_sys->i_row)
            {
                TopOffset = length;
                p_out -= TopOffset * p_outpic->p[i_plane].i_pitch;
                for (i_index = 0; i_index < length; i_index++)
                    for (i_index2 = 0; i_index2 < i_copy_pitch; i_index2++)
#ifndef GAMMA
                    *(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2) = (p_vout->p_sys->lambda2[0][i_index2 % p_pic->p->i_pixel_pitch][i_index] *
                                 (*(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2))) / ACCURACY;
#else
                    *(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2) = p_vout->p_sys->LUT[i_index2 % p_pic->p->i_pixel_pitch][p_vout->p_sys->lambda2[0][i_index2 % p_pic->p->i_pixel_pitch][i_index]][*(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2)];

#endif
                p_out += TopOffset * p_outpic->p[i_plane].i_pitch;
            }
            else if (p_vout->p_sys->i_row > 2)
            // black bar
            {
                TopOffset = length - (2 * p_vout->p_sys->i_halfHeight);
                p_out -= TopOffset * p_outpic->p[i_plane].i_pitch;
                for (i_index = 0; i_index < length; i_index++)
                    for (i_index2 = 0; i_index2 < i_copy_pitch; i_index2++)
                        *(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2) = 0;
                p_out += TopOffset * p_outpic->p[i_plane].i_pitch;
            }
// end blended zone
            }
        }
#endif
// bug for wall filter : fix by CC
//            pi_left_skip[i_plane] += i_out_pitch;
            pi_left_skip[i_plane] += i_copy_pitch;
            }

            vout_UnlinkPicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                                p_outpic );
            vout_DisplayPicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                                 p_outpic );
        }

        for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
        {
            pi_top_skip[i_plane] += p_vout->p_sys->pp_vout[ i_vout-1 ].i_height
                                     * p_pic->p[i_plane].i_lines
                                     / p_vout->output.i_height
                                     * p_pic->p[i_plane].i_pitch;
        }
    }
#ifdef OVERLAP
    if (p_vout->p_sys->b_has_changed) p_vout->p_sys->b_has_changed = false;
#endif
}


#ifdef PACKED_YUV
// WARNING : NO DEBUGGED
/*****************************************************************************
 * RenderPackedYUV: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Wall image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void RenderPackedYUV( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic = NULL;
    int i_col, i_row, i_vout, i_plane;
    int pi_left_skip[VOUT_MAX_PLANES], pi_top_skip[VOUT_MAX_PLANES];
#ifdef OVERLAP
    int LeftOffset, TopOffset;
    int constantYUV[3] = {0,128,128};
    int Denom;
    int a_2;
    int a_1;
    int a_0;
    int i_index, i_index2;
#endif

    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
        pi_top_skip[i_plane] = 0;

    for( i_vout = 0;, i_row = 0; i_row < p_vout->p_sys->i_row; i_row++ )
    {
        for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
            pi_left_skip[i_plane] = 0;

        for( i_col = 0; i_col < p_vout->p_sys->i_col; i_col++, i_vout++ )
        {
            if( !p_vout->p_sys->pp_vout[ i_vout ].b_active )
            {
                for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
                {
                    pi_left_skip[i_plane] +=
                        p_vout->p_sys->pp_vout[ i_vout ].i_width
                         * p_pic->p[i_plane].i_pitch / p_vout->output.i_width;
                }
                continue;
            }

            while( ( p_outpic =
                vout_CreatePicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                                    0, 0, 0 )
                   ) == NULL )
            {
                if( !vlc_object_alive (p_vout) || p_vout->b_error )
                {
                    vout_DestroyPicture(
                        p_vout->p_sys->pp_vout[ i_vout ].p_vout, p_outpic );
                    return;
                }

                msleep( VOUT_OUTMEM_SLEEP );
            }

            p_outpic->date = p_pic->date;
            vout_LinkPicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                              p_outpic );

            for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
            {
                uint8_t *p_in, *p_in_end, *p_out;
                int i_in_pitch = p_pic->p[i_plane].i_pitch;
                int i_out_pitch = p_outpic->p[i_plane].i_pitch;
                int i_copy_pitch = p_outpic->p[i_plane].i_visible_pitch;
                const int i_div = p_vout->p_sys->pp_vout[i_vout].i_width / i_copy_pitch;

#ifdef OVERLAP
                if (i_col) pi_left_skip[i_plane] -= (2 * p_vout->p_sys->i_halfLength ) / i_div;
                if ((p_vout->p_sys->i_row >= 2) && (i_row) && (!i_col)) pi_top_skip[i_plane] -= (2 * p_vout->p_sys->i_halfHeight * p_pic->p[i_plane].i_pitch) / i_div;
                if ((p_vout->p_sys->i_row > 2) && (i_row == 1) && (!i_col)) pi_top_skip[i_plane] -= (2 * p_vout->p_sys->i_halfHeight * p_pic->p[i_plane].i_pitch) / i_div;
                if( !p_vout->p_sys->pp_vout[p_vout->p_sys->i_col-1].b_active )
                    pi_top_skip[i_plane] -= (2 * p_vout->p_sys->i_halfHeight * i_row * p_pic->p[i_plane].i_pitch) / i_div;
// i_n : previous inactive pp_vout
                int i_n=0;
                while ((!p_vout->p_sys->pp_vout[i_row * p_vout->p_sys->i_col + i_col - 1 - i_n].b_active) && (i_col - i_n > 1)) i_n++;
                if ((i_col > 1) && i_n)
                    pi_left_skip[i_plane] -= i_n*(2 * p_vout->p_sys->i_halfLength ) / i_div;

                p_in = p_pic->p[i_plane].p_pixels
                /* Wall proprities */
                + pi_top_skip[i_plane] + pi_left_skip[i_plane];

                int i_lines = p_outpic->p[i_plane].i_visible_lines;
// 1088 lines bug in a mpeg2 stream of 1080 lines
                if ((p_vout->p_sys->i_row - 1 == i_row) &&
                    (p_pic->p[i_plane].i_lines == 1088))
                        i_lines -= 8;

                p_in_end = p_in + i_lines * p_pic->p[i_plane].i_pitch;
#else
                p_in = p_pic->p[i_plane].p_pixels
                        + pi_top_skip[i_plane] + pi_left_skip[i_plane];

                p_in_end = p_in + p_outpic->p[i_plane].i_visible_lines
                                        * p_pic->p[i_plane].i_pitch;
#endif
                p_out = p_outpic->p[i_plane].p_pixels;
#ifdef OVERLAP
        int length;
        length = 2 * p_vout->p_sys->i_halfLength * p_pic->p->i_pixel_pitch;
        LeftOffset = (i_col ? 0 : i_copy_pitch - length);
        if (p_vout->p_sys->b_has_changed)
        {
#ifdef GAMMA
            int i_plane_;
            for (i_index = 0; i_index < length / p_pic->p->i_pixel_pitch; i_index++)
                for (i_plane_ =  0; i_plane_ < p_pic->p->i_pixel_pitch; i_plane_++)
                    for (i_index2 = 0; i_index2 < 256; i_index2++)
                            p_vout->p_sys->LUT[i_plane_][i_index2][i_index] = F(i_index2, (length / p_pic->p->i_pixel_pitch, i_index, p_vout->p_sys->f_gamma[i_plane_]));
#endif
            switch (p_vout->output.i_chroma)
                {
                    case VLC_FOURCC('Y','U','Y','2'):    // packed by 2
                    case VLC_FOURCC('Y','U','N','V'):    // packed by 2
                        Denom = F2(length / p_pic->p->i_pixel_pitch);
                        a_2 = p_vout->p_sys->a_2 * (ACCURACY / 100);
                        a_1 = p_vout->p_sys->a_1 * 2 * p_vout->p_sys->i_halfLength * (ACCURACY / 100);
                        a_0 = p_vout->p_sys->a_0 * Denom * (ACCURACY / 100);
                        for (i_index = 0; i_index < length / p_pic->p->i_pixel_pitch; i_index+=p_pic->p->i_pixel_pitch)
                        // for each macropixel
                        {
                                // first image pixel
                                p_vout->p_sys->lambda[i_col][0][i_index] = CLIP_0A(!i_col ? ACCURACY - (F4(a_2, a_1, i_index) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length / p_pic->p->i_pixel_pitch) - i_index) + a_0) / Denom);
                                p_vout->p_sys->cstYUV[i_col][0][i_index] = ((ACCURACY - p_vout->p_sys->lambda[i_col][0][i_index]) * constantYUV[0]) / ACCURACY;
                                p_vout->p_sys->lambda[i_col][1][i_index] = CLIP_0A(!i_col ? ACCURACY - (F4(a_2, a_1, i_index) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length / p_pic->p->i_pixel_pitch) - i_index) + a_0) / Denom);
                                p_vout->p_sys->cstYUV[i_col][1][i_index] = ((ACCURACY - p_vout->p_sys->lambda[i_col][1][i_index]) * constantYUV[1]) / ACCURACY;
                                // second image pixel
                                p_vout->p_sys->lambda[i_col][0][i_index + 1] = CLIP_0A(!i_col ? ACCURACY - (F4(a_2, a_1, i_index + 1) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length / p_pic->p->i_pixel_pitch) - (i_index + 1)) + a_0) / Denom);
                                p_vout->p_sys->cstYUV[i_col][0][i_index + 1] = ((ACCURACY - p_vout->p_sys->lambda[i_col][0][i_index]) * constantYUV[0]) / ACCURACY;
                                p_vout->p_sys->lambda[i_col][1][i_index + 1] = p_vout->p_sys->lambda[i_col][1][i_index];
                                p_vout->p_sys->cstYUV[i_col][1][i_index + 1] = p_vout->p_sys->cstYUV[i_col][1][i_index];
                        }
                        break;
                    case VLC_FOURCC('U','Y','V','Y'):    // packed by 2
                    case VLC_FOURCC('U','Y','N','V'):    // packed by 2
                    case VLC_FOURCC('Y','4','2','2'):    // packed by 2
                        Denom = F2(length / p_pic->p->i_pixel_pitch);
                        a_2 = p_vout->p_sys->a_2 * (ACCURACY / 100);
                        a_1 = p_vout->p_sys->a_1 * 2 * p_vout->p_sys->i_halfLength * (ACCURACY / 100);
                        a_0 = p_vout->p_sys->a_0 * Denom * (ACCURACY / 100);
                        for (i_index = 0; i_index < length / p_pic->p->i_pixel_pitch; i_index+=p_pic->p->i_pixel_pitch)
                        // for each macropixel
                        {
                                // first image pixel
                                p_vout->p_sys->lambda[i_col][0][i_index] = CLIP_0A(!i_col ? ACCURACY - (F4(a_2, a_1, i_index) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length / p_pic->p->i_pixel_pitch) - i_index) + a_0) / Denom);
                                p_vout->p_sys->cstYUV[i_col][0][i_index] = ((ACCURACY - p_vout->p_sys->lambda[i_col][0][i_index]) * constantYUV[1]) / ACCURACY;
                                p_vout->p_sys->lambda[i_col][1][i_index] = CLIP_0A(!i_col ? ACCURACY - (F4(a_2, a_1, i_index) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length / p_pic->p->i_pixel_pitch) - i_index) + a_0) / Denom);
                                p_vout->p_sys->cstYUV[i_col][1][i_index] = ((ACCURACY - p_vout->p_sys->lambda[i_col][1][i_index]) * constantYUV[0]) / ACCURACY;
                                // second image pixel
                                p_vout->p_sys->lambda[i_col][0][i_index + 1] = CLIP_0A(!i_col ? ACCURACY - (F4(a_2, a_1, i_index + 1) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length / p_pic->p->i_pixel_pitch) - (i_index + 1)) + a_0) / Denom);
                                p_vout->p_sys->cstYUV[i_col][0][i_index + 1] = ((ACCURACY - p_vout->p_sys->lambda[i_col][0][i_index]) * constantYUV[1]) / ACCURACY;
                                p_vout->p_sys->lambda[i_col][1][i_index + 1] = p_vout->p_sys->lambda[i_col][1][i_index];
                                p_vout->p_sys->cstYUV[i_col][1][i_index + 1] = p_vout->p_sys->cstYUV[i_col][1][i_index];
                        }
                        break;
                    default :
                        break;
                }
        }
#endif
            while( p_in < p_in_end )
            {
#ifndef OVERLAP
                vlc_memcpy( p_out, p_in, i_copy_pitch);
#else
                vlc_memcpy( p_out + i_col * length, p_in + i_col * length, i_copy_pitch - length);
                p_out += LeftOffset;
                p_in += LeftOffset;
#ifndef GAMMA
                for (i_index = 0; i_index < length; i_index++)
                    *(p_out + i_index) = (p_vout->p_sys->lambda[i_col][i_index % p_pic->p->i_pixel_pitch][i_index / p_pic->p->i_pixel_pitch] *
                             (*(p_in + i_index))) / ACCURACY +
                             p_vout->p_sys->cstYUV[i_col][i_index % p_pic->p->i_pixel_pitch][i_index / p_pic->p->i_pixel_pitch];
#else
                for (i_index = 0; i_index < length; i_index++)
                    *(p_out + i_index) = p_vout->p_sys->LUT[i_index % p_pic->p->i_pixel_pitch][(p_vout->p_sys->lambda[i_col][i_index % p_pic->p->i_pixel_pitch][i_index / p_pic->p->i_pixel_pitch] *
                             (*(p_in + i_index))) / ACCURACY +
                             p_vout->p_sys->cstYUV[i_col][i_index % p_pic->p->i_pixel_pitch][i_index / p_pic->p->i_pixel_pitch]][i_index / p_pic->p->i_pixel_pitch];
#endif
                p_out -= LeftOffset;
                p_in -= LeftOffset;
#endif
                p_in += i_in_pitch;
                p_out += i_out_pitch;
            }
#ifdef OVERLAP
            if (p_vout->p_sys->i_row == 2)
            {
                        length = 2 * p_vout->p_sys->i_halfHeight * p_pic->p->i_pixel_pitch;
                        TopOffset = (i_row ? i_lines : length / p_pic->p->i_pixel_pitch);
                        if (p_vout->p_sys->b_has_changed)
                        {
#ifdef GAMMA
                                int i_plane_;
                                for (i_index = 0; i_index < length / p_pic->p->i_pixel_pitch; i_index++)
                                    for (i_plane_ =  0; i_plane_ < p_pic->p->i_pixel_pitch; i_plane_++)
                                        for (i_index2 = 0; i_index2 < 256; i_index2++)
                                                p_vout->p_sys->LUT2[i_plane_][i_index2][i_index] = F(i_index2, (length / p_pic->p->i_pixel_pitch, i_index, p_vout->p_sys->f_gamma[i_plane_]));
#endif
                                switch (p_vout->output.i_chroma)
                                {
                                    case VLC_FOURCC('Y','U','Y','2'):    // packed by 2
                                    case VLC_FOURCC('Y','U','N','V'):    // packed by 2
                                        Denom = F2(length / p_pic->p->i_pixel_pitch);
                                        a_2 = p_vout->p_sys->a_2 * (ACCURACY / 100);
                                        a_1 = p_vout->p_sys->a_1 * 2 * p_vout->p_sys->i_halfHeight * (ACCURACY / 100);
                                        a_0 = p_vout->p_sys->a_0 * Denom * (ACCURACY / 100);
                                        for (i_index = 0; i_index < length / p_pic->p->i_pixel_pitch; i_index+=p_pic->p->i_pixel_pitch)
                                        // for each macropixel
                                        {
                                                // first image pixel
                                                p_vout->p_sys->lambda2[i_row][0][i_index] = CLIP_0A(!i_row ? ACCURACY - (F4(a_2, a_1, i_index) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length / p_pic->p->i_pixel_pitch) - i_index) + a_0) / Denom);
                                                p_vout->p_sys->cstYUV2[i_row][0][i_index] = ((ACCURACY - p_vout->p_sys->lambda2[i_row][0][i_index]) * constantYUV[0]) / ACCURACY;
                                                p_vout->p_sys->lambda2[i_row][1][i_index] = CLIP_0A(!i_row ? ACCURACY - (F4(a_2, a_1, i_index) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length / p_pic->p->i_pixel_pitch) - i_index) + a_0) / Denom);
                                                p_vout->p_sys->cstYUV2[i_row][1][i_index] = ((ACCURACY - p_vout->p_sys->lambda2[i_row][1][i_index]) * constantYUV[1]) / ACCURACY;
                                                // second image pixel
                                                p_vout->p_sys->lambda2[i_row][0][i_index + 1] = CLIP_0A(!i_row ? ACCURACY - (F4(a_2, a_1, i_index + 1) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length / p_pic->p->i_pixel_pitch) - (i_index + 1)) + a_0) / Denom);
                                                p_vout->p_sys->cstYUV2[i_row][0][i_index + 1] = ((ACCURACY - p_vout->p_sys->lambda2[i_row][0][i_index]) * constantYUV[0]) / ACCURACY;
                                                p_vout->p_sys->lambda2[i_row][1][i_index + 1] = p_vout->p_sys->lambda2[i_row][1][i_index];
                                                p_vout->p_sys->cstYUV2[i_row][1][i_index + 1] = p_vout->p_sys->cstYUV2[i_row][1][i_index];
                                        }
                                        break;
                                    case VLC_FOURCC('U','Y','V','Y'):    // packed by 2
                                    case VLC_FOURCC('U','Y','N','V'):    // packed by 2
                                    case VLC_FOURCC('Y','4','2','2'):    // packed by 2
                                        Denom = F2(length / p_pic->p->i_pixel_pitch);
                                        a_2 = p_vout->p_sys->a_2 * (ACCURACY / 100);
                                        a_1 = p_vout->p_sys->a_1 * 2 * p_vout->p_sys->i_halfHeight * (ACCURACY / 100);
                                        a_0 = p_vout->p_sys->a_0 * Denom * (ACCURACY / 100);
                                        for (i_index = 0; i_index < length / p_pic->p->i_pixel_pitch; i_index+=p_pic->p->i_pixel_pitch)
                                        // for each macropixel
                                        {
                                                // first image pixel
                                                p_vout->p_sys->lambda2[i_row][0][i_index] = CLIP_0A(!i_row ? ACCURACY - (F4(a_2, a_1, i_index) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length / p_pic->p->i_pixel_pitch) - i_index) + a_0) / Denom);
                                                p_vout->p_sys->cstYUV2[i_row][0][i_index] = ((ACCURACY - p_vout->p_sys->lambda2[i_col][0][i_index]) * constantYUV[1]) / ACCURACY;
                                                p_vout->p_sys->lambda2[i_row][1][i_index] = CLIP_0A(!i_row ? ACCURACY - (F4(a_2, a_1, i_index) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length / p_pic->p->i_pixel_pitch) - i_index) + a_0) / Denom);
                                                p_vout->p_sys->cstYUV2[i_row][1][i_index] = ((ACCURACY - p_vout->p_sys->lambda2[i_row][1][i_index]) * constantYUV[0]) / ACCURACY;
                                                // second image pixel
                                                p_vout->p_sys->lambda2[i_row][0][i_index + 1] = CLIP_0A(!i_row ? ACCURACY - (F4(a_2, a_1, i_index + 1) + a_0) / Denom : ACCURACY - (F4(a_2, a_1,(length / p_pic->p->i_pixel_pitch) - (i_index + 1)) + a_0) / Denom);
                                                p_vout->p_sys->cstYUV2[i_row][0][i_index + 1] = ((ACCURACY - p_vout->p_sys->lambda2[i_row][0][i_index]) * constantYUV[1]) / ACCURACY;
                                                p_vout->p_sys->lambda2[i_row][1][i_index + 1] = p_vout->p_sys->lambda2[i_row][1][i_index];
                                                p_vout->p_sys->cstYUV2[i_row][1][i_index + 1] = p_vout->p_sys->cstYUV2[i_row][1][i_index];
                                        }
                                        break;
                                    default :
                                        break;
                                }
                        }
                        p_out -= TopOffset * p_outpic->p[i_plane].i_pitch;
#ifndef GAMMA
                        for (i_index = 0; i_index < length / p_pic->p->i_pixel_pitch; i_index++)
                            for (i_index2 = 0; i_index2 < i_copy_pitch; i_index2++)
                                *(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2) = (p_vout->p_sys->lambda2[i_row][i_index2 % p_pic->p->i_pixel_pitch][i_index] *
                                     (*(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2))) / ACCURACY +
                                     p_vout->p_sys->cstYUV2[i_row][i_index2 % p_pic->p->i_pixel_pitch][i_index];
#else
                        for (i_index = 0; i_index < length / p_pic->p->i_pixel_pitch; i_index++)
                            for (i_index2 = 0; i_index2 < i_copy_pitch; i_index2++)
                                *(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2) = p_vout->p_sys->LUT[i_index % p_pic->p->i_pixel_pitch][(p_vout->p_sys->lambda2[i_row][i_index2 % p_pic->p->i_pixel_pitch][i_index] *
                                     (*(p_out + (i_index * p_outpic->p[i_plane].i_pitch) + i_index2))) / ACCURACY +
                                     p_vout->p_sys->cstYUV2[i_row][i_index2 % p_pic->p->i_pixel_pitch][i_index]][i_index / p_pic->p->i_pixel_pitch];

#endif
                        p_out += TopOffset * p_outpic->p[i_plane].i_pitch;
            }
#endif
// bug for wall filter : fix by CC
//            pi_left_skip[i_plane] += i_out_pitch;
            pi_left_skip[i_plane] += i_copy_pitch;
            }

            vout_UnlinkPicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                                p_outpic );
            vout_DisplayPicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                                 p_outpic );
        }

        for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
        {
            pi_top_skip[i_plane] += p_vout->p_sys->pp_vout[ i_vout-1 ].i_height
                                     * p_pic->p[i_plane].i_lines
                                     / p_vout->output.i_height
                                     * p_pic->p[i_plane].i_pitch;
        }
    }
#ifdef OVERLAP
    if (p_vout->p_sys->b_has_changed) p_vout->p_sys->b_has_changed = false;
#endif
}
#endif


/*****************************************************************************
 * RemoveAllVout: destroy all the child video output threads
 *****************************************************************************/
static void RemoveAllVout( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    for( int i = 0; i < p_vout->p_sys->i_vout; i++ )
    {
        if( p_sys->pp_vout[i].b_active )
        {
            vout_filter_SetupChild( p_vout, p_sys->pp_vout[i].p_vout,
                                    MouseEvent, FullscreenEventUp, FullscreenEventDown, true );
            vout_CloseAndRelease( p_sys->pp_vout[i].p_vout );
            p_sys->pp_vout[i].p_vout = NULL;
        }
    }
}

/*****************************************************************************
 * SendEvents: forward mouse and keyboard events to the parent p_vout
 *****************************************************************************/
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = p_data;
    vout_sys_t *p_sys = p_vout->p_sys;
    VLC_UNUSED(oldval);
    int i_vout;

    /* Find the video output index */
    for( i_vout = 0; i_vout < p_sys->i_vout; i_vout++ )
    {
        if( p_sys->pp_vout[i_vout].b_active &&
            p_this == VLC_OBJECT(p_sys->pp_vout[i_vout].p_vout) )
            break;
    }
    assert( i_vout < p_vout->p_sys->i_vout );

    /* Translate the mouse coordinates */
    if( !strcmp( psz_var, "mouse-x" ) )
    {
#ifdef OVERLAP
        int i_overlap = ((p_sys->i_col > 2) ? 0 : 2 * p_sys->i_halfLength);
           newval.i_int += (p_vout->output.i_width - i_overlap)
#else
           newval.i_int += p_vout->output.i_width
#endif
                         * (i_vout % p_sys->i_col)
                          / p_sys->i_col;
    }
    else if( !strcmp( psz_var, "mouse-y" ) )
    {
#ifdef OVERLAP
        int i_overlap = ((p_sys->i_row > 2) ? 0 : 2 * p_sys->i_halfHeight);
           newval.i_int += (p_vout->output.i_height - i_overlap)
#else
           newval.i_int += p_vout->output.i_height
#endif
//bug fix in Wall plug-in
//                         * (i_vout / p_vout->p_sys->i_row)
                         * (i_vout / p_sys->i_col)
                          / p_sys->i_row;
    }

    return var_Set( p_vout, psz_var, newval );
}

/**
 * Forward fullscreen event to/from the childrens.
 * FIXME pretty much duplicated from wall.c
 */
static bool IsFullscreenActive( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    for( int i = 0; i < p_sys->i_vout; i++ )
    {
        if( p_sys->pp_vout[i].b_active &&
            var_GetBool( p_sys->pp_vout[i].p_vout, "fullscreen" ) )
            return true;
    }
    return false;
}
static int FullscreenEventUp( vlc_object_t *p_this, char const *psz_var,
                              vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = p_data;
    VLC_UNUSED(oldval); VLC_UNUSED(p_this); VLC_UNUSED(psz_var); VLC_UNUSED(newval);

    const bool b_fullscreen = IsFullscreenActive( p_vout );
    if( !var_GetBool( p_vout, "fullscreen" ) != !b_fullscreen )
        return var_SetBool( p_vout, "fullscreen", b_fullscreen );
    return VLC_SUCCESS;
}
static int FullscreenEventDown( vlc_object_t *p_this, char const *psz_var,
                                vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t*)p_this;
    vout_sys_t *p_sys = p_vout->p_sys;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data); VLC_UNUSED(psz_var);

    const bool b_fullscreen = IsFullscreenActive( p_vout );
    if( !b_fullscreen != !newval.b_bool )
    {
        for( int i = 0; i < p_sys->i_vout; i++ )
        {
            if( !p_sys->pp_vout[i].b_active )
                continue;

            vout_thread_t *p_child = p_sys->pp_vout[i].p_vout;
            if( !var_GetBool( p_child, "fullscreen" ) != !newval.b_bool )
            {
                var_SetBool( p_child, "fullscreen", newval.b_bool );
                if( newval.b_bool )
                    return VLC_SUCCESS;
            }
        }
    }
    return VLC_SUCCESS;
}

