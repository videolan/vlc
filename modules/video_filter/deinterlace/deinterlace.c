/*****************************************************************************
 * deinterlace.c : deinterlacer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2011 VLC authors and VideoLAN
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
 * video filter functions
 *****************************************************************************/

/**
 * Top-level filtering method.
 *
 * Open() sets this up as the processing method (pf_video_filter)
 * in the filter structure.
 *
 * Note that there is no guarantee that the returned picture directly
 * corresponds to p_pic. The first few times, the filter may not even
 * return a picture, if it is still filling the history for temporal
 * filtering (although such filters often return *something* also
 * while starting up). It should be assumed that N input pictures map to
 * M output pictures, with no restrictions for N and M (except that there
 * is not much delay).
 *
 * Also, there is no guarantee that the PTS of the frame stays untouched.
 * In fact, framerate doublers automatically compute the proper PTSs for the
 * two output frames for each input frame, and IVTC does a nontrivial
 * framerate conversion (29.97 > 23.976 fps).
 *
 * Yadif has an offset of one frame between input and output, but introduces
 * no delay: the returned frame is the *previous* input frame deinterlaced,
 * complete with its original PTS.
 *
 * Finally, note that returning NULL sometimes can be normal behaviour for some
 * algorithms (e.g. IVTC).
 *
 * Currently:
 *   Most algorithms:        1 -> 1, no offset
 *   All framerate doublers: 1 -> 2, no offset
 *   Yadif:                  1 -> 1, offset of one frame
 *   IVTC:                   1 -> 1 or 0 (depends on whether a drop was needed)
 *                                with an offset of one frame (in most cases)
 *                                and framerate conversion.
 *
 * @param p_filter The filter instance.
 * @param p_pic The latest input picture.
 * @return Deinterlaced picture(s). Linked list of picture_t's or NULL.
 * @see Open()
 * @see filter_t
 * @see filter_sys_t
 */
static picture_t *Deinterlace( filter_t *p_filter, picture_t *p_pic );

/**
 * Reads the configuration, sets up and starts the filter.
 *
 * Possible reasons for returning VLC_EGENERIC:
 *  - Unsupported input chroma. See IsChromaSupported().
 *  - Caller has set p_filter->b_allow_fmt_out_change to false,
 *    but the algorithm chosen in the configuration
 *    wants to convert the output to a format different
 *    from the input. See SetFilterMethod().
 *
 * Open() is atomic: if an error occurs, the state of p_this
 * is left as it was before the call to this function.
 *
 * @param p_this The filter instance as vlc_object_t.
 * @return VLC error code
 * @retval VLC_SUCCESS All ok, filter set up and started.
 * @retval VLC_ENOMEM Memory allocation error, initialization aborted.
 * @retval VLC_EGENERIC Something went wrong, initialization aborted.
 * @see IsChromaSupported()
 * @see SetFilterMethod()
 */
static int Open( vlc_object_t *p_this );

/**
 * Resets the filter state, including resetting all algorithm-specific state
 * and discarding all histories, but does not stop the filter.
 *
 * Open() sets this up as the flush method (pf_flush)
 * in the filter structure.
 *
 * @param p_filter The filter instance.
 * @see Open()
 * @see filter_t
 * @see filter_sys_t
 * @see metadata_history_t
 * @see phosphor_sys_t
 * @see ivtc_sys_t
 */
static void Flush( filter_t *p_filter );

/**
 * Mouse callback for the deinterlace filter.
 *
 * Open() sets this up as the mouse callback method (pf_video_mouse)
 * in the filter structure.
 *
 * Currently, this handles the scaling of the y coordinate for algorithms
 * that halve the output height.
 *
 * @param p_filter The filter instance.
 * @param[out] p_mouse Updated mouse position data.
 * @param[in] p_old Previous mouse position data. Unused in this filter.
 * @param[in] p_new Latest mouse position data.
 * @return VLC error code; currently always VLC_SUCCESS.
 * @retval VLC_SUCCESS All ok.
 * @see Open()
 * @see filter_t
 * @see vlc_mouse_t
 */
static int Mouse( filter_t *p_filter,
                  vlc_mouse_t *p_mouse,
                  const vlc_mouse_t *p_old,
                  const vlc_mouse_t *p_new );

/**
 * Stops and uninitializes the filter, and deallocates memory.
 * @param p_this The filter instance as vlc_object_t.
 */
static void Close( vlc_object_t *p_this );

/*****************************************************************************
 * Extra documentation
 *****************************************************************************/

/**
 * \file
 * Deinterlacer plugin for vlc. Data structures and video filter functions.
 *
 * Note on i_frame_offset:
 *
 * This value indicates the offset between input and output frames in the
 * currently active deinterlace algorithm. See the rationale below for why
 * this is needed and how it is used.
 *
 * Valid range: 0 <= i_frame_offset < METADATA_SIZE, or
 *              i_frame_offset = CUSTOM_PTS.
 *              The special value CUSTOM_PTS is only allowed
 *              if b_double_rate is false.
 *
 *              If CUSTOM_PTS is used, the algorithm must compute the outgoing
 *              PTSs itself, and additionally, read the TFF/BFF information
 *              itself (if it needs it) from the incoming frames.
 *
 * Meaning of values:
 * 0 = output frame corresponds to the current input frame
 *     (no frame offset; default if not set),
 * 1 = output frame corresponds to the previous input frame
 *     (e.g. Yadif and Yadif2x work like this),
 * ...
 *
 * If necessary, i_frame_offset should be updated by the active deinterlace
 * algorithm to indicate the correct delay for the *next* input frame.
 * It does not matter at which i_order the algorithm updates this information,
 * but the new value will only take effect upon the next call to Deinterlace()
 * (i.e. at the next incoming frame).
 *
 * The first-ever frame that arrives to the filter after Open() is always
 * handled as having i_frame_offset = 0. For the second and all subsequent
 * frames, each algorithm is responsible for setting the offset correctly.
 * (The default is 0, so if that is correct, there's no need to do anything.)
 *
 * This solution guarantees that i_frame_offset:
 *   1) is up to date at the start of each frame,
 *   2) does not change (as far as Deinterlace() is concerned) during
 *      a frame, and
 *   3) does not need a special API for setting the value at the start of each
 *      input frame, before the algorithm starts rendering the (first) output
 *      frame for that input frame.
 *
 * The deinterlace algorithm is allowed to behave differently for different
 * input frames. This is especially important for startup, when full history
 * (as defined by each algorithm) is not yet available. During the first-ever
 * input frame, it is clear that it is the only possible source for
 * information, so i_frame_offset = 0 is necessarily correct. After that,
 * what to do is up to each algorithm.
 *
 * Having the correct offset at the start of each input frame is critically
 * important in order to:
 *   1) Allocate the correct number of output frames for framerate doublers,
 *      and to
 *   2) Pass correct TFF/BFF information to the algorithm.
 *
 * These points are important for proper soft field repeat support. This
 * feature is used in some streams (especially NTSC) originating from film.
 * For example, in soft NTSC telecine, the number of fields alternates
 * as 3,2,3,2,... and the video field dominance flips every two frames (after
 * every "3"). Also, some streams request an occasional field repeat
 * (nb_fields = 3), after which the video field dominance flips.
 * To render such streams correctly, the nb_fields and TFF/BFF information
 * must be taken from the specific input frame that the algorithm intends
 * to render.
 *
 * Additionally, the output PTS is automatically computed by Deinterlace()
 * from i_frame_offset and i_order.
 *
 * It is possible to use the special value CUSTOM_PTS to indicate that the
 * algorithm computes the output PTSs itself. In this case, Deinterlace()
 * will pass them through. This special value is not valid for framerate
 * doublers, as by definition they are field renderers, so they need to
 * use the original field timings to work correctly. Basically, this special
 * value is only intended for algorithms that need to perform nontrivial
 * framerate conversions (such as IVTC).
 */


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
            p_sys->context.settings = filter_mode[i].settings;
            p_sys->context.pf_render_ordered = filter_mode[i].pf_render_ordered;
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
    filter_sys_t *p_sys = p_filter->p_sys;
    GetDeinterlacingOutput(&p_sys->context, p_dst, p_src);
}

/*****************************************************************************
 * video filter functions
 *****************************************************************************/

picture_t *AllocPicture( filter_t *filter )
{
    return filter_NewPicture( filter );
}

/* This is the filter function. See Open(). */
picture_t *Deinterlace( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    return DoDeinterlacing( p_filter, &p_sys->context, p_pic );
}

/*****************************************************************************
 * Flush
 *****************************************************************************/

void Flush( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    FlushDeinterlacing(&p_sys->context);

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
    filter_sys_t *p_sys = p_filter->p_sys;
    if( p_sys->context.settings.b_half_height )
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

    InitDeinterlacingContext( &p_sys->context );

    config_ChainParse( p_filter, FILTER_CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );
    char *psz_mode = var_InheritString( p_filter, FILTER_CFG_PREFIX "mode" );
    SetFilterMethod( p_filter, psz_mode, packed );

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
#if defined(CAN_COMPILE_SVE)
    if( vlc_CPU_ARM_SVE() )
        p_sys->pf_merge = pixel_size == 1 ? merge8_arm_sve : merge16_arm_sve;
    else
#endif
#if defined(CAN_COMPILE_ARM64)
    if( vlc_CPU_ARM_NEON() )
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
