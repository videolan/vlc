/*****************************************************************************
 * deinterlace.c : deinterlacer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2011 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Sam Hocevar <sam@zoy.org>
 *         Christophe Massiot <massiot@via.ecp.fr>
 *         Laurent Aimar <fenrir@videolan.org>
 *         Juha Jeronen <juha.jeronen@jyu.fi>
 *         ...and others
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

#include <assert.h>
#include <stdint.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_cpu.h>
#include <vlc_mouse.h>

#include "deinterlace.h"
#include "helpers.h"
#include "merge.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define MODE_TEXT N_("Deinterlace mode")

#define SOUT_MODE_TEXT N_("Streaming deinterlace mode")
#define SOUT_MODE_LONGTEXT N_("Deinterlace method to use for streaming.")

#define FILTER_CFG_PREFIX "sout-deinterlace-"

/* Tooltips drop linefeeds (at least in the Qt GUI);
   thus the space before each set of consecutive \n.

   See phosphor.h for phosphor_chroma_list and phosphor_dimmer_list.
*/
#define PHOSPHOR_CHROMA_TEXT N_("Phosphor chroma mode for 4:2:0 input")
#define PHOSPHOR_CHROMA_LONGTEXT N_("Choose handling for colours in those "\
                                    "output frames that fall across input "\
                                    "frame boundaries. \n"\
                                    "\n"\
                                    "Latest: take chroma from new (bright) "\
                                    "field only. Good for interlaced input, "\
                                    "such as videos from a camcorder. \n"\
                                    "\n"\
                                    "AltLine: take chroma line 1 from top "\
                                    "field, line 2 from bottom field, etc. \n"\
                                    "Default, good for NTSC telecined input "\
                                    "(anime DVDs, etc.). \n"\
                                    "\n"\
                                    "Blend: average input field chromas. "\
                                    "May distort the colours of the new "\
                                    "(bright) field, too. \n"\
                                    "\n"\
                                    "Upconvert: output in 4:2:2 format "\
                                    "(independent chroma for each field). "\
                                    "Best simulation, but requires more CPU "\
                                    "and memory bandwidth.")

#define PHOSPHOR_DIMMER_TEXT N_("Phosphor old field dimmer strength")
#define PHOSPHOR_DIMMER_LONGTEXT N_("This controls the strength of the "\
                                    "darkening filter that simulates CRT TV "\
                                    "phosphor light decay for the old field "\
                                    "in the Phosphor framerate doubler. "\
                                    "Default: Low.")

vlc_module_begin ()
    set_description( N_("Deinterlacing video filter") )
    set_shortname( N_("Deinterlace" ))
    set_capability( "video filter", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_string( FILTER_CFG_PREFIX "mode", "blend", SOUT_MODE_TEXT,
                SOUT_MODE_LONGTEXT, false )
        change_string_list( mode_list, mode_list_text )
        change_safe ()
    add_integer( FILTER_CFG_PREFIX "phosphor-chroma", 2, PHOSPHOR_CHROMA_TEXT,
                PHOSPHOR_CHROMA_LONGTEXT, true )
        change_integer_list( phosphor_chroma_list, phosphor_chroma_list_text )
        change_safe ()
    add_integer( FILTER_CFG_PREFIX "phosphor-dimmer", 2, PHOSPHOR_DIMMER_TEXT,
                PHOSPHOR_DIMMER_LONGTEXT, true )
        change_integer_list( phosphor_dimmer_list, phosphor_dimmer_list_text )
        change_safe ()
    add_shortcut( "deinterlace" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local data
 *****************************************************************************/

/**
 * Available config options for the deinterlacer module.
 *
 * Note that also algorithm-specific options must be listed here,
 * and reading logic for them implemented in Open().
 */
static const char *const ppsz_filter_options[] = {
    "mode", "phosphor-chroma", "phosphor-dimmer",
    NULL
};

/*****************************************************************************
 * SetFilterMethod: setup the deinterlace method to use.
 *****************************************************************************/

struct filter_mode_t
{
    const char           *psz_mode;
    union {
    int (*pf_render_ordered)(filter_t *, picture_t *p_dst, picture_t *p_pic,
                             int order, int i_field);
    int (*pf_render_single_pic)(filter_t *, picture_t *p_dst, picture_t *p_pic);
    };
    deinterlace_algo     settings;
    bool                 can_pack;         /**< can handle packed pixel */
    bool                 b_high_bit_depth; /**< can handle high bit depth */
};
static struct filter_mode_t filter_mode [] = {
    { "discard", .pf_render_single_pic = RenderDiscard,
                 { false, false, false, true }, true, true },
    { "bob", .pf_render_ordered = RenderBob,
                 { true, false, false, false }, true, true },
    { "progressive-scan", .pf_render_ordered = RenderBob,
                 { true, false, false, false }, true, true },
    { "linear", .pf_render_ordered = RenderLinear,
                 { true, false, false, false }, true, true },
    { "mean", .pf_render_single_pic = RenderMean,
                 { false, false, false, true }, true, true },
    { "blend", .pf_render_single_pic = RenderBlend,
                 { false, false, false, false }, true, true },
    { "yadif", .pf_render_single_pic = RenderYadifSingle,
                 { false, true, false, false }, false, true },
    { "yadif2x", .pf_render_ordered = RenderYadif,
                 { true, true, false, false }, false, true },
    { "x", .pf_render_single_pic = RenderX,
                 { false, false, false, false }, false, false },
    { "phosphor", .pf_render_ordered = RenderPhosphor,
                 { true, true, false, false }, false, false },
    { "ivtc", .pf_render_single_pic = RenderIVTC,
                 { false, true, true, false }, false, false },
};

/**
 * Setup the deinterlace method to use.
 *
 * FIXME: extract i_chroma from p_filter automatically?
 *
 * @param p_filter The filter instance.
 * @param mode Desired method. See mode_list for available choices.
 * @see mode_list
 */
static void SetFilterMethod( filter_t *p_filter, const char *mode, bool pack )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if ( mode == NULL || !strcmp( mode, "auto" ) )
        mode = "x";

    for ( size_t i = 0; i < ARRAY_SIZE(filter_mode); i++ )
    {
        if( !strcmp( mode, filter_mode[i].psz_mode ) )
        {
            if ( pack && !filter_mode[i].can_pack )
            {
                msg_Err( p_filter, "unknown or incompatible deinterlace mode \"%s\""
                        " for packed format", mode );
                SetFilterMethod( p_filter, "blend", pack );
                return;
            }
            if( p_sys->chroma->pixel_size > 1 && !filter_mode[i].b_high_bit_depth )
            {
                msg_Err( p_filter, "unknown or incompatible deinterlace mode \"%s\""
                        " for high depth format", mode );
                SetFilterMethod( p_filter, "blend", pack );
                return;
            }

            msg_Dbg( p_filter, "using %s deinterlace method", mode );
            p_filter->p_sys->context.settings = filter_mode[i].settings;
            p_filter->p_sys->context.pf_render_ordered = filter_mode[i].pf_render_ordered;
            return;
        }
    }

    msg_Err( p_filter, "unknown deinterlace mode \"%s\"", mode );
}

/**
 * Get the output video format of the chosen deinterlace method
 * for the given input video format.
 *
 * Note that each algorithm is allowed to specify its output format,
 * which may (for some input formats) differ from the input format.
 *
 * @param p_filter The filter instance.
 * @param[out] p_dst Output video format. The structure must be allocated by ca
 * @param[in] p_src Input video format.
 * @see SetFilterMethod()
 */
static void GetOutputFormat( filter_t *p_filter,
                      video_format_t *p_dst, const video_format_t *p_src )
{
    GetDeinterlacingOutput(&p_filter->p_sys->context, p_dst, p_src);
}

/*****************************************************************************
 * video filter functions
 *****************************************************************************/

/* This is the filter function. See Open(). */
picture_t *Deinterlace( filter_t *p_filter, picture_t *p_pic )
{
    return DoDeinterlacing( p_filter, &p_filter->p_sys->context, p_pic );
}

/*****************************************************************************
 * Flush
 *****************************************************************************/

void Flush( filter_t *p_filter )
{
    FlushDeinterlacing(&p_filter->p_sys->context);

    IVTCClearState( p_filter );
}

/*****************************************************************************
 * Mouse event callback
 *****************************************************************************/

int Mouse( filter_t *p_filter,
           vlc_mouse_t *p_mouse,
           const vlc_mouse_t *p_old, const vlc_mouse_t *p_new )
{
    VLC_UNUSED(p_old);
    *p_mouse = *p_new;
    if( p_filter->p_sys->context.settings.b_half_height )
        p_mouse->i_y *= 2;
    return VLC_SUCCESS;
}


/*****************************************************************************
 * Open
 *****************************************************************************/

int Open( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys;

    const vlc_fourcc_t fourcc = p_filter->fmt_in.video.i_chroma;
    const vlc_chroma_description_t *chroma = vlc_fourcc_GetChromaDescription( fourcc );
    if( chroma == NULL || chroma->pixel_size > 2 )
    {
notsupp:
        msg_Dbg( p_filter, "unsupported chroma %4.4s", (char*)&fourcc );
        return VLC_EGENERIC;
    }

    unsigned pixel_size = chroma->pixel_size;
    bool packed = false;
    if( chroma->plane_count != 3 )
    {
        packed = true;
        switch( fourcc )
        {
            case VLC_CODEC_YUYV:
            case VLC_CODEC_UYVY:
            case VLC_CODEC_YVYU:
            case VLC_CODEC_VYUY:
            case VLC_CODEC_NV12:
            case VLC_CODEC_NV21:
                pixel_size = 1;
                break;
            default:
                goto notsupp;
        }
    }
    assert( vlc_fourcc_IsYUV( fourcc ) );

    /* */
    p_sys = p_filter->p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->chroma = chroma;

    config_ChainParse( p_filter, FILTER_CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );
    char *psz_mode = var_InheritString( p_filter, FILTER_CFG_PREFIX "mode" );
    SetFilterMethod( p_filter, psz_mode, packed );

    InitDeinterlacingContext( &p_sys->context );

    IVTCClearState( p_filter );

#if defined(CAN_COMPILE_C_ALTIVEC)
    if( pixel_size == 1 && vlc_CPU_ALTIVEC() )
        p_sys->pf_merge = MergeAltivec;
    else
#endif
#if defined(CAN_COMPILE_SSE2)
    if( vlc_CPU_SSE2() )
    {
        p_sys->pf_merge = pixel_size == 1 ? Merge8BitSSE2 : Merge16BitSSE2;
        p_sys->pf_end_merge = EndMMX;
    }
    else
#endif
#if defined(CAN_COMPILE_MMXEXT)
    if( pixel_size == 1 && vlc_CPU_MMXEXT() )
    {
        p_sys->pf_merge = MergeMMXEXT;
        p_sys->pf_end_merge = EndMMX;
    }
    else
#endif
#if defined(CAN_COMPILE_3DNOW)
    if( pixel_size == 1 && vlc_CPU_3dNOW() )
    {
        p_sys->pf_merge = Merge3DNow;
        p_sys->pf_end_merge = End3DNow;
    }
    else
#endif
#if defined(CAN_COMPILE_ARM)
    if( vlc_CPU_ARM_NEON() )
        p_sys->pf_merge = pixel_size == 1 ? merge8_arm_neon : merge16_arm_neon;
    else
    if( vlc_CPU_ARMv6() )
        p_sys->pf_merge = pixel_size == 1 ? merge8_armv6 : merge16_armv6;
    else
#endif
#if defined(CAN_COMPILE_ARM64)
    if( vlc_CPU_ARM64_NEON() )
        p_sys->pf_merge = pixel_size == 1 ? merge8_arm64_neon : merge16_arm64_neon;
    else
#endif
    {
        p_sys->pf_merge = pixel_size == 1 ? Merge8BitGeneric : Merge16BitGeneric;
#if defined(__i386__) || defined(__x86_64__)
        p_sys->pf_end_merge = NULL;
#endif
    }

    /* */
    video_format_t fmt;
    GetOutputFormat( p_filter, &fmt, &p_filter->fmt_in.video );

    /* */
    if( !strcmp( psz_mode, "phosphor" ) )
    {
        int i_c420 = var_GetInteger( p_filter,
                                     FILTER_CFG_PREFIX "phosphor-chroma" );
        if( i_c420 != PC_LATEST  &&  i_c420 != PC_ALTLINE  &&
            i_c420 != PC_BLEND   && i_c420 != PC_UPCONVERT )
        {
            msg_Dbg( p_filter, "Phosphor 4:2:0 input chroma mode not set"\
                               "or out of range (valid: 1, 2, 3 or 4), "\
                               "using default" );
            i_c420 = PC_ALTLINE;
        }
        msg_Dbg( p_filter, "using Phosphor 4:2:0 input chroma mode %d",
                           i_c420 );
        /* This maps directly to the phosphor_chroma_t enum. */
        p_sys->phosphor.i_chroma_for_420 = i_c420;

        int i_dimmer = var_GetInteger( p_filter,
                                       FILTER_CFG_PREFIX "phosphor-dimmer" );
        if( i_dimmer < 1  ||  i_dimmer > 4 )
        {
            msg_Dbg( p_filter, "Phosphor dimmer strength not set "\
                               "or out of range (valid: 1, 2, 3 or 4), "\
                               "using default" );
            i_dimmer = 2; /* low */
        }
        msg_Dbg( p_filter, "using Phosphor dimmer strength %d", i_dimmer );
        /* The internal value ranges from 0 to 3. */
        p_sys->phosphor.i_dimmer_strength = i_dimmer - 1;

        if( 2 * chroma->p[1].h.num == chroma->p[1].h.den &&
            2 * chroma->p[2].h.num == chroma->p[2].h.den &&
            i_c420 == PC_UPCONVERT )
        {
            fmt.i_chroma = p_filter->fmt_in.video.i_chroma == VLC_CODEC_J420 ?
                        VLC_CODEC_J422 : VLC_CODEC_I422;
        }
    }
    free( psz_mode );

    if( !p_filter->b_allow_fmt_out_change &&
        ( fmt.i_chroma != p_filter->fmt_in.video.i_chroma ||
          fmt.i_height != p_filter->fmt_in.video.i_height ) )
    {
        Close( VLC_OBJECT(p_filter) );
        return VLC_EGENERIC;
    }
    p_filter->fmt_out.video = fmt;
    p_filter->fmt_out.i_codec = fmt.i_chroma;
    p_filter->pf_video_filter = Deinterlace;
    p_filter->pf_flush = Flush;
    p_filter->pf_video_mouse  = Mouse;

    msg_Dbg( p_filter, "deinterlacing" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: clean up the filter
 *****************************************************************************/

void Close( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;

    Flush( p_filter );
    free( p_filter->p_sys );
}
