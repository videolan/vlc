/*****************************************************************************
 * deinterlace.c : deinterlacer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2011 the VideoLAN team
 * $Id$
 *
 * Author: Sam Hocevar <sam@zoy.org>
 *         Juha Jeronen <juha.jeronen@jyu.fi> (Phosphor mode)
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

#include <assert.h>

#ifdef HAVE_ALTIVEC_H
#   include <altivec.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_cpu.h>

#ifdef CAN_COMPILE_MMXEXT
#   include "mmx.h"
#endif

#define DEINTERLACE_DISCARD  1
#define DEINTERLACE_MEAN     2
#define DEINTERLACE_BLEND    3
#define DEINTERLACE_BOB      4
#define DEINTERLACE_LINEAR   5
#define DEINTERLACE_X        6
#define DEINTERLACE_YADIF    7
#define DEINTERLACE_YADIF2X  8
#define DEINTERLACE_PHOSPHOR 9

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define MODE_TEXT N_("Deinterlace mode")
#define MODE_LONGTEXT N_("Deinterlace method to use for local playback.")

#define SOUT_MODE_TEXT N_("Streaming deinterlace mode")
#define SOUT_MODE_LONGTEXT N_("Deinterlace method to use for streaming.")

#define FILTER_CFG_PREFIX "sout-deinterlace-"

static const char *const mode_list[] = {
    "discard", "blend", "mean", "bob", "linear", "x",
    "yadif", "yadif2x", "phosphor" };
static const char *const mode_list_text[] = {
    N_("Discard"), N_("Blend"), N_("Mean"), N_("Bob"), N_("Linear"), "X",
    "Yadif", "Yadif (2x)", N_("Phosphor") };

/* Tooltips drop linefeeds (at least in the Qt GUI);
   thus the space before each set of consecutive \n. */
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

/* These numbers, and phosphor_chroma_list[], should be in the same order
   as phosphor_chroma_list_text[]. The value 0 is reserved, because
   var_GetInteger() returns 0 in case of error. */
typedef enum { PC_LATEST = 1, PC_ALTLINE   = 2,
               PC_BLEND  = 3, PC_UPCONVERT = 4 } phosphor_chroma_t;
static const int phosphor_chroma_list[] = { PC_LATEST, PC_ALTLINE,
                                            PC_BLEND,  PC_UPCONVERT };
static const char *const phosphor_chroma_list_text[] = { N_("Latest"),
                                                         N_("AltLine"),
                                                         N_("Blend"),
                                                         N_("Upconvert") };

/* Same here. Same order as in phosphor_dimmer_list_text[],
   and the value 0 is reserved for config error. */
static const int phosphor_dimmer_list[] = { 1, 2, 3, 4 };
static const char *const phosphor_dimmer_list_text[] = { N_("Off"),
                                                         N_("Low"),
                                                         N_("Medium"),
                                                         N_("High") };

vlc_module_begin ()
    set_description( N_("Deinterlacing video filter") )
    set_shortname( N_("Deinterlace" ))
    set_capability( "video filter2", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_string( FILTER_CFG_PREFIX "mode", "blend", SOUT_MODE_TEXT,
                SOUT_MODE_LONGTEXT, false )
        change_string_list( mode_list, mode_list_text, 0 )
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
 * Local protypes
 *****************************************************************************/
static void RenderDiscard ( filter_t *, picture_t *, picture_t *, int );
static void RenderBob     ( filter_t *, picture_t *, picture_t *, int );
static void RenderMean    ( filter_t *, picture_t *, picture_t * );
static void RenderBlend   ( filter_t *, picture_t *, picture_t * );
static void RenderLinear  ( filter_t *, picture_t *, picture_t *, int );
static void RenderX       ( picture_t *, picture_t * );
static int  RenderYadif   ( filter_t *, picture_t *, picture_t *, int, int );
static int  RenderPhosphor( filter_t *, picture_t *, picture_t *, int, int );

static void MergeGeneric ( void *, const void *, const void *, size_t );
#if defined(CAN_COMPILE_C_ALTIVEC)
static void MergeAltivec ( void *, const void *, const void *, size_t );
#endif
#if defined(CAN_COMPILE_MMXEXT)
static void MergeMMXEXT  ( void *, const void *, const void *, size_t );
#endif
#if defined(CAN_COMPILE_3DNOW)
static void Merge3DNow   ( void *, const void *, const void *, size_t );
#endif
#if defined(CAN_COMPILE_SSE)
static void MergeSSE2    ( void *, const void *, const void *, size_t );
#endif
#if defined(CAN_COMPILE_MMXEXT) || defined(CAN_COMPILE_SSE)
static void EndMMX       ( void );
#endif
#if defined(CAN_COMPILE_3DNOW)
static void End3DNow     ( void );
#endif
#if defined __ARM_NEON__
static void MergeNEON (void *, const void *, const void *, size_t);
#endif

/* Converts a full-frame plane_t to a field plane_t */
static void FieldFromPlane( plane_t *p_dst, const plane_t *p_src,
                            int i_field );

/* Composes a frame from the given field pair */
typedef enum { CC_ALTLINE, CC_UPCONVERT, CC_SOURCE_TOP, CC_SOURCE_BOTTOM,
               CC_MERGE } compose_chroma_t;
static void ComposeFrame( filter_t *, picture_t *, picture_t *, picture_t *,
                          compose_chroma_t );

static const char *const ppsz_filter_options[] = {
    "mode", "phosphor-chroma", "phosphor-dimmer",
    NULL
};

/* Used for framerate doublers */
#define METADATA_SIZE (3)
typedef struct {
    mtime_t pi_date[METADATA_SIZE];
    int     pi_nb_fields[METADATA_SIZE];
    bool    pb_top_field_first[METADATA_SIZE];
} metadata_history_t;

/* Algorithm-specific state */
typedef struct
{
    phosphor_chroma_t i_chroma_for_420;
    int i_dimmer_strength;
} phosphor_sys_t;

/* Top-level subsystem state */
#define HISTORY_SIZE (3)
#define CUSTOM_PTS -1
struct filter_sys_t
{
    int  i_mode;              /* Deinterlace mode */
    bool b_double_rate;       /* Shall we double the framerate? */
    bool b_half_height;       /* Shall be divide the height by 2 */
    bool b_use_frame_history; /* Does the algorithm need the input frame history buffer? */

    void (*pf_merge) ( void *, const void *, const void *, size_t );
    void (*pf_end_merge) ( void );

    /* Metadata history (PTS, nb_fields, TFF). Used for framerate doublers. */
    metadata_history_t meta;

    /* Output frame timing / framerate doubler control (see below) */
    int i_frame_offset;

    /* Input frame history buffer for algorithms that perform temporal filtering. */
    picture_t *pp_history[HISTORY_SIZE];

    /* Algorithm-specific substructures */
    phosphor_sys_t phosphor;
};

/*  NOTE on i_frame_offset:

    This value indicates the offset between input and output frames in the currently active deinterlace algorithm.
    See the rationale below for why this is needed and how it is used.

    Valid range: 0 <= i_frame_offset < METADATA_SIZE, or i_frame_offset = CUSTOM_PTS.
                 The special value CUSTOM_PTS is only allowed if b_double_rate is false.

                 If CUSTOM_PTS is used, the algorithm must compute the outgoing PTSs itself,
                 and additionally, read the TFF/BFF information itself (if it needs it)
                 from the incoming frames.

    Meaning of values:
    0 = output frame corresponds to the current input frame
        (no frame offset; default if not set),
    1 = output frame corresponds to the previous input frame
        (e.g. Yadif and Yadif2x work like this),
    ...

    If necessary, i_frame_offset should be updated by the active deinterlace algorithm
    to indicate the correct delay for the *next* input frame. It does not matter at which i_order
    the algorithm updates this information, but the new value will only take effect upon the
    next call to Deinterlace() (i.e. at the next incoming frame).

    The first-ever frame that arrives to the filter after Open() is always handled as having
    i_frame_offset = 0. For the second and all subsequent frames, each algorithm is responsible
    for setting the offset correctly. (The default is 0, so if that is correct, there's no need
    to do anything.)

    This solution guarantees that i_frame_offset:
      1) is up to date at the start of each frame,
      2) does not change (as far as Deinterlace() is concerned) during a frame, and
      3) does not need a special API for setting the value at the start of each input frame,
         before the algorithm starts rendering the (first) output frame for that input frame.

    The deinterlace algorithm is allowed to behave differently for different input frames.
    This is especially important for startup, when full history (as defined by each algorithm)
    is not yet available. During the first-ever input frame, it is clear that it is the
    only possible source for information, so i_frame_offset = 0 is necessarily correct.
    After that, what to do is up to each algorithm.

    Having the correct offset at the start of each input frame is critically important in order to:
      1) Allocate the correct number of output frames for framerate doublers, and to
      2) Pass correct TFF/BFF information to the algorithm.

    These points are important for proper soft field repeat support. This feature is used in some
    streams originating from film. In soft NTSC telecine, the number of fields alternates as 3,2,3,2,...
    and the video field dominance flips every two frames (after every "3"). Also, some streams
    request an occasional field repeat (nb_fields = 3), after which the video field dominance flips.
    To render such streams correctly, the nb_fields and TFF/BFF information must be taken from
    the specific input frame that the algorithm intends to render.

    Additionally, the output PTS is automatically computed by Deinterlace() from i_frame_offset and i_order.

    It is possible to use the special value CUSTOM_PTS to indicate that the algorithm computes
    the output PTSs itself. In this case, Deinterlace() will pass them through. This special value
    is not valid for framerate doublers, as by definition they are field renderers, so they need to
    use the original field timings to work correctly. Basically, this special value is only intended
    for algorithms that need to perform nontrivial framerate conversions (such as IVTC).
*/


/*****************************************************************************
 * SetFilterMethod: setup the deinterlace method to use.
 *****************************************************************************/
static void SetFilterMethod( filter_t *p_filter, const char *psz_method, vlc_fourcc_t i_chroma )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( !psz_method )
        psz_method = "";

    if( !strcmp( psz_method, "mean" ) )
    {
        p_sys->i_mode = DEINTERLACE_MEAN;
        p_sys->b_double_rate = false;
        p_sys->b_half_height = true;
        p_sys->b_use_frame_history = false;
    }
    else if( !strcmp( psz_method, "bob" )
             || !strcmp( psz_method, "progressive-scan" ) )
    {
        p_sys->i_mode = DEINTERLACE_BOB;
        p_sys->b_double_rate = true;
        p_sys->b_half_height = false;
        p_sys->b_use_frame_history = false;
    }
    else if( !strcmp( psz_method, "linear" ) )
    {
        p_sys->i_mode = DEINTERLACE_LINEAR;
        p_sys->b_double_rate = true;
        p_sys->b_half_height = false;
        p_sys->b_use_frame_history = false;
    }
    else if( !strcmp( psz_method, "x" ) )
    {
        p_sys->i_mode = DEINTERLACE_X;
        p_sys->b_double_rate = false;
        p_sys->b_half_height = false;
        p_sys->b_use_frame_history = false;
    }
    else if( !strcmp( psz_method, "yadif" ) )
    {
        p_sys->i_mode = DEINTERLACE_YADIF;
        p_sys->b_double_rate = false;
        p_sys->b_half_height = false;
        p_sys->b_use_frame_history = true;
    }
    else if( !strcmp( psz_method, "yadif2x" ) )
    {
        p_sys->i_mode = DEINTERLACE_YADIF2X;
        p_sys->b_double_rate = true;
        p_sys->b_half_height = false;
        p_sys->b_use_frame_history = true;
    }
    else if( !strcmp( psz_method, "phosphor" ) )
    {
        p_sys->i_mode = DEINTERLACE_PHOSPHOR;
        p_sys->b_double_rate = true;
        p_sys->b_half_height = false;
        p_sys->b_use_frame_history = true;
    }
    else if( !strcmp( psz_method, "discard" ) )
    {
        const bool b_i422 = i_chroma == VLC_CODEC_I422 ||
                            i_chroma == VLC_CODEC_J422;

        p_sys->i_mode = DEINTERLACE_DISCARD;
        p_sys->b_double_rate = false;
        p_sys->b_half_height = !b_i422;
        p_sys->b_use_frame_history = false;
    }
    else
    {
        if( strcmp( psz_method, "blend" ) )
            msg_Err( p_filter,
                     "no valid deinterlace mode provided, using \"blend\"" );

        p_sys->i_mode = DEINTERLACE_BLEND;
        p_sys->b_double_rate = false;
        p_sys->b_half_height = false;
        p_sys->b_use_frame_history = false;
    }

    p_sys->i_frame_offset = 0; /* reset to default when method changes */

    msg_Dbg( p_filter, "using %s deinterlace method", psz_method );
}

static void GetOutputFormat( filter_t *p_filter,
                             video_format_t *p_dst, const video_format_t *p_src )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    *p_dst = *p_src;

    if( p_sys->b_half_height )
    {
        p_dst->i_height /= 2;
        p_dst->i_visible_height /= 2;
        p_dst->i_y_offset /= 2;
        p_dst->i_sar_den *= 2;
    }

    if( p_src->i_chroma == VLC_CODEC_I422 ||
        p_src->i_chroma == VLC_CODEC_J422 )
    {
        switch( p_sys->i_mode )
        {
        case DEINTERLACE_MEAN:
        case DEINTERLACE_LINEAR:
        case DEINTERLACE_X:
        case DEINTERLACE_YADIF:
        case DEINTERLACE_YADIF2X:
        case DEINTERLACE_PHOSPHOR:
            p_dst->i_chroma = p_src->i_chroma;
            break;
        default:
            p_dst->i_chroma = p_src->i_chroma == VLC_CODEC_I422 ? VLC_CODEC_I420 :
                                                                  VLC_CODEC_J420;
            break;
        }
    }
    else if( p_sys->i_mode == DEINTERLACE_PHOSPHOR  &&
             p_sys->phosphor.i_chroma_for_420 == PC_UPCONVERT )
    {
        p_dst->i_chroma = p_src->i_chroma == VLC_CODEC_J420 ? VLC_CODEC_J422 :
                                                              VLC_CODEC_I422;
    }
}

static bool IsChromaSupported( vlc_fourcc_t i_chroma )
{
    return i_chroma == VLC_CODEC_I420 ||
           i_chroma == VLC_CODEC_J420 ||
           i_chroma == VLC_CODEC_YV12 ||
           i_chroma == VLC_CODEC_I422 ||
           i_chroma == VLC_CODEC_J422;
}

/*****************************************************************************
 * RenderDiscard: only keep TOP or BOTTOM field, discard the other.
 *****************************************************************************/
static void RenderDiscard( filter_t *p_filter,
                           picture_t *p_outpic, picture_t *p_pic, int i_field )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;
        int i_increment;

        p_in = p_pic->p[i_plane].p_pixels
                   + i_field * p_pic->p[i_plane].i_pitch;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_visible_lines;

        switch( p_filter->fmt_in.video.i_chroma )
        {
        case VLC_CODEC_I420:
        case VLC_CODEC_J420:
        case VLC_CODEC_YV12:

            for( ; p_out < p_out_end ; )
            {
                vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                p_out += p_outpic->p[i_plane].i_pitch;
                p_in += 2 * p_pic->p[i_plane].i_pitch;
            }
            break;

        case VLC_CODEC_I422:
        case VLC_CODEC_J422:

            i_increment = 2 * p_pic->p[i_plane].i_pitch;

            if( i_plane == Y_PLANE )
            {
                for( ; p_out < p_out_end ; )
                {
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                    p_out += p_outpic->p[i_plane].i_pitch;
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                    p_out += p_outpic->p[i_plane].i_pitch;
                    p_in += i_increment;
                }
            }
            else
            {
                for( ; p_out < p_out_end ; )
                {
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                    p_out += p_outpic->p[i_plane].i_pitch;
                    p_in += i_increment;
                }
            }
            break;

        default:
            break;
        }
    }
}

/*****************************************************************************
 * RenderBob: renders a BOB picture - simple copy
 *****************************************************************************/
static void RenderBob( filter_t *p_filter,
                       picture_t *p_outpic, picture_t *p_pic, int i_field )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;
        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_visible_lines;

        switch( p_filter->fmt_in.video.i_chroma )
        {
            case VLC_CODEC_I420:
            case VLC_CODEC_J420:
            case VLC_CODEC_YV12:
                /* For BOTTOM field we need to add the first line */
                if( i_field == 1 )
                {
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                    p_in += p_pic->p[i_plane].i_pitch;
                    p_out += p_outpic->p[i_plane].i_pitch;
                }

                p_out_end -= 2 * p_outpic->p[i_plane].i_pitch;

                for( ; p_out < p_out_end ; )
                {
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                    p_out += p_outpic->p[i_plane].i_pitch;

                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                    p_in += 2 * p_pic->p[i_plane].i_pitch;
                    p_out += p_outpic->p[i_plane].i_pitch;
                }

                vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                /* For TOP field we need to add the last line */
                if( i_field == 0 )
                {
                    p_in += p_pic->p[i_plane].i_pitch;
                    p_out += p_outpic->p[i_plane].i_pitch;
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                }
                break;

            case VLC_CODEC_I422:
            case VLC_CODEC_J422:
                /* For BOTTOM field we need to add the first line */
                if( i_field == 1 )
                {
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                    p_in += p_pic->p[i_plane].i_pitch;
                    p_out += p_outpic->p[i_plane].i_pitch;
                }

                p_out_end -= 2 * p_outpic->p[i_plane].i_pitch;

                if( i_plane == Y_PLANE )
                {
                    for( ; p_out < p_out_end ; )
                    {
                        vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                        p_out += p_outpic->p[i_plane].i_pitch;

                        vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                        p_in += 2 * p_pic->p[i_plane].i_pitch;
                        p_out += p_outpic->p[i_plane].i_pitch;
                    }
                }
                else
                {
                    for( ; p_out < p_out_end ; )
                    {
                        vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                        p_out += p_outpic->p[i_plane].i_pitch;
                        p_in += 2 * p_pic->p[i_plane].i_pitch;
                    }
                }

                vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                /* For TOP field we need to add the last line */
                if( i_field == 0 )
                {
                    p_in += p_pic->p[i_plane].i_pitch;
                    p_out += p_outpic->p[i_plane].i_pitch;
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                }
                break;
        }
    }
}

#define Merge p_filter->p_sys->pf_merge
#define EndMerge if(p_filter->p_sys->pf_end_merge) p_filter->p_sys->pf_end_merge

/*****************************************************************************
 * RenderLinear: BOB with linear interpolation
 *****************************************************************************/
static void RenderLinear( filter_t *p_filter,
                          picture_t *p_outpic, picture_t *p_pic, int i_field )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;
        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_visible_lines;

        /* For BOTTOM field we need to add the first line */
        if( i_field == 1 )
        {
            vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
            p_in += p_pic->p[i_plane].i_pitch;
            p_out += p_outpic->p[i_plane].i_pitch;
        }

        p_out_end -= 2 * p_outpic->p[i_plane].i_pitch;

        for( ; p_out < p_out_end ; )
        {
            vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

            p_out += p_outpic->p[i_plane].i_pitch;

            Merge( p_out, p_in, p_in + 2 * p_pic->p[i_plane].i_pitch,
                   p_pic->p[i_plane].i_pitch );

            p_in += 2 * p_pic->p[i_plane].i_pitch;
            p_out += p_outpic->p[i_plane].i_pitch;
        }

        vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

        /* For TOP field we need to add the last line */
        if( i_field == 0 )
        {
            p_in += p_pic->p[i_plane].i_pitch;
            p_out += p_outpic->p[i_plane].i_pitch;
            vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
        }
    }
    EndMerge();
}

static void RenderMean( filter_t *p_filter,
                        picture_t *p_outpic, picture_t *p_pic )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_visible_lines;

        /* All lines: mean value */
        for( ; p_out < p_out_end ; )
        {
            Merge( p_out, p_in, p_in + p_pic->p[i_plane].i_pitch,
                   p_pic->p[i_plane].i_pitch );

            p_out += p_outpic->p[i_plane].i_pitch;
            p_in += 2 * p_pic->p[i_plane].i_pitch;
        }
    }
    EndMerge();
}

static void RenderBlend( filter_t *p_filter,
                         picture_t *p_outpic, picture_t *p_pic )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_visible_lines;

        switch( p_filter->fmt_in.video.i_chroma )
        {
            case VLC_CODEC_I420:
            case VLC_CODEC_J420:
            case VLC_CODEC_YV12:
                /* First line: simple copy */
                vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                p_out += p_outpic->p[i_plane].i_pitch;

                /* Remaining lines: mean value */
                for( ; p_out < p_out_end ; )
                {
                    Merge( p_out, p_in, p_in + p_pic->p[i_plane].i_pitch,
                           p_pic->p[i_plane].i_pitch );

                    p_out += p_outpic->p[i_plane].i_pitch;
                    p_in += p_pic->p[i_plane].i_pitch;
                }
                break;

            case VLC_CODEC_I422:
            case VLC_CODEC_J422:
                /* First line: simple copy */
                vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                p_out += p_outpic->p[i_plane].i_pitch;

                /* Remaining lines: mean value */
                if( i_plane == Y_PLANE )
                {
                    for( ; p_out < p_out_end ; )
                    {
                        Merge( p_out, p_in, p_in + p_pic->p[i_plane].i_pitch,
                               p_pic->p[i_plane].i_pitch );

                        p_out += p_outpic->p[i_plane].i_pitch;
                        p_in += p_pic->p[i_plane].i_pitch;
                    }
                }

                else
                {
                    for( ; p_out < p_out_end ; )
                    {
                        Merge( p_out, p_in, p_in + p_pic->p[i_plane].i_pitch,
                               p_pic->p[i_plane].i_pitch );

                        p_out += p_outpic->p[i_plane].i_pitch;
                        p_in += 2*p_pic->p[i_plane].i_pitch;
                    }
                }
                break;
        }
    }
    EndMerge();
}

static void MergeGeneric( void *_p_dest, const void *_p_s1,
                          const void *_p_s2, size_t i_bytes )
{
    uint8_t* p_dest = (uint8_t*)_p_dest;
    const uint8_t *p_s1 = (const uint8_t *)_p_s1;
    const uint8_t *p_s2 = (const uint8_t *)_p_s2;
    uint8_t* p_end = p_dest + i_bytes - 8;

    while( p_dest < p_end )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }

    p_end += 8;

    while( p_dest < p_end )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }
}

#if defined(CAN_COMPILE_MMXEXT)
static void MergeMMXEXT( void *_p_dest, const void *_p_s1, const void *_p_s2,
                         size_t i_bytes )
{
    uint8_t* p_dest = (uint8_t*)_p_dest;
    const uint8_t *p_s1 = (const uint8_t *)_p_s1;
    const uint8_t *p_s2 = (const uint8_t *)_p_s2;
    uint8_t* p_end = p_dest + i_bytes - 8;
    while( p_dest < p_end )
    {
        __asm__  __volatile__( "movq %2,%%mm1;"
                               "pavgb %1, %%mm1;"
                               "movq %%mm1, %0" :"=m" (*p_dest):
                                                 "m" (*p_s1),
                                                 "m" (*p_s2) );
        p_dest += 8;
        p_s1 += 8;
        p_s2 += 8;
    }

    p_end += 8;

    while( p_dest < p_end )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }
}
#endif

#if defined(CAN_COMPILE_3DNOW)
static void Merge3DNow( void *_p_dest, const void *_p_s1, const void *_p_s2,
                        size_t i_bytes )
{
    uint8_t* p_dest = (uint8_t*)_p_dest;
    const uint8_t *p_s1 = (const uint8_t *)_p_s1;
    const uint8_t *p_s2 = (const uint8_t *)_p_s2;
    uint8_t* p_end = p_dest + i_bytes - 8;
    while( p_dest < p_end )
    {
        __asm__  __volatile__( "movq %2,%%mm1;"
                               "pavgusb %1, %%mm1;"
                               "movq %%mm1, %0" :"=m" (*p_dest):
                                                 "m" (*p_s1),
                                                 "m" (*p_s2) );
        p_dest += 8;
        p_s1 += 8;
        p_s2 += 8;
    }

    p_end += 8;

    while( p_dest < p_end )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }
}
#endif

#if defined(CAN_COMPILE_SSE)
static void MergeSSE2( void *_p_dest, const void *_p_s1, const void *_p_s2,
                       size_t i_bytes )
{
    uint8_t* p_dest = (uint8_t*)_p_dest;
    const uint8_t *p_s1 = (const uint8_t *)_p_s1;
    const uint8_t *p_s2 = (const uint8_t *)_p_s2;
    uint8_t* p_end;
    while( (uintptr_t)p_s1 % 16 )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }
    p_end = p_dest + i_bytes - 16;
    while( p_dest < p_end )
    {
        __asm__  __volatile__( "movdqu %2,%%xmm1;"
                               "pavgb %1, %%xmm1;"
                               "movdqu %%xmm1, %0" :"=m" (*p_dest):
                                                 "m" (*p_s1),
                                                 "m" (*p_s2) );
        p_dest += 16;
        p_s1 += 16;
        p_s2 += 16;
    }

    p_end += 16;

    while( p_dest < p_end )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }
}
#endif

#if defined(CAN_COMPILE_MMXEXT) || defined(CAN_COMPILE_SSE)
static void EndMMX( void )
{
    __asm__ __volatile__( "emms" :: );
}
#endif

#if defined(CAN_COMPILE_3DNOW)
static void End3DNow( void )
{
    __asm__ __volatile__( "femms" :: );
}
#endif

#ifdef CAN_COMPILE_C_ALTIVEC
static void MergeAltivec( void *_p_dest, const void *_p_s1,
                          const void *_p_s2, size_t i_bytes )
{
    uint8_t *p_dest = (uint8_t *)_p_dest;
    uint8_t *p_s1   = (uint8_t *)_p_s1;
    uint8_t *p_s2   = (uint8_t *)_p_s2;
    uint8_t *p_end  = p_dest + i_bytes - 15;

    /* Use C until the first 16-bytes aligned destination pixel */
    while( (uintptr_t)p_dest & 0xF )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }

    if( ( (int)p_s1 & 0xF ) | ( (int)p_s2 & 0xF ) )
    {
        /* Unaligned source */
        vector unsigned char s1v, s2v, destv;
        vector unsigned char s1oldv, s2oldv, s1newv, s2newv;
        vector unsigned char perm1v, perm2v;

        perm1v = vec_lvsl( 0, p_s1 );
        perm2v = vec_lvsl( 0, p_s2 );
        s1oldv = vec_ld( 0, p_s1 );
        s2oldv = vec_ld( 0, p_s2 );

        while( p_dest < p_end )
        {
            s1newv = vec_ld( 16, p_s1 );
            s2newv = vec_ld( 16, p_s2 );
            s1v    = vec_perm( s1oldv, s1newv, perm1v );
            s2v    = vec_perm( s2oldv, s2newv, perm2v );
            s1oldv = s1newv;
            s2oldv = s2newv;
            destv  = vec_avg( s1v, s2v );
            vec_st( destv, 0, p_dest );

            p_s1   += 16;
            p_s2   += 16;
            p_dest += 16;
        }
    }
    else
    {
        /* Aligned source */
        vector unsigned char s1v, s2v, destv;

        while( p_dest < p_end )
        {
            s1v   = vec_ld( 0, p_s1 );
            s2v   = vec_ld( 0, p_s2 );
            destv = vec_avg( s1v, s2v );
            vec_st( destv, 0, p_dest );

            p_s1   += 16;
            p_s2   += 16;
            p_dest += 16;
        }
    }

    p_end += 15;

    while( p_dest < p_end )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }
}
#endif

#ifdef __ARM_NEON__
static void MergeNEON (void *restrict out, const void *in1,
                       const void *in2, size_t n)
{
    uint8_t *outp = out;
    const uint8_t *in1p = in1;
    const uint8_t *in2p = in2;
    size_t mis = ((uintptr_t)outp) & 15;

    if (mis)
    {
        MergeGeneric (outp, in1p, in2p, mis);
        outp += mis;
        in1p += mis;
        in2p += mis;
        n -= mis;
    }

    uint8_t *end = outp + (n & ~15);

    if ((((uintptr_t)in1p)|((uintptr_t)in2p)) & 15)
        while (outp < end)
            asm volatile (
                "vld1.u8  {q0-q1}, [%[in1]]!\n"
                "vld1.u8  {q2-q3}, [%[in2]]!\n"
                "vhadd.u8 q4, q0, q2\n"
                "vld1.u8  {q6-q7}, [%[in1]]!\n"
                "vhadd.u8 q5, q1, q3\n"
                "vld1.u8  {q8-q9}, [%[in2]]!\n"
                "vhadd.u8 q10, q6, q8\n"
                "vhadd.u8 q11, q7, q9\n"
                "vst1.u8  {q4-q5}, [%[out],:128]!\n"
                "vst1.u8  {q10-q11}, [%[out],:128]!\n"
                : [out] "+r" (outp), [in1] "+r" (in1p), [in2] "+r" (in2p)
                :
                : "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7",
                  "q8", "q9", "q10", "q11", "memory");
    else
         while (outp < end)
            asm volatile (
                "vld1.u8  {q0-q1}, [%[in1],:128]!\n"
                "vld1.u8  {q2-q3}, [%[in2],:128]!\n"
                "vhadd.u8 q4, q0, q2\n"
                "vld1.u8  {q6-q7}, [%[in1],:128]!\n"
                "vhadd.u8 q5, q1, q3\n"
                "vld1.u8  {q8-q9}, [%[in2],:128]!\n"
                "vhadd.u8 q10, q6, q8\n"
                "vhadd.u8 q11, q7, q9\n"
                "vst1.u8  {q4-q5}, [%[out],:128]!\n"
                "vst1.u8  {q10-q11}, [%[out],:128]!\n"
                : [out] "+r" (outp), [in1] "+r" (in1p), [in2] "+r" (in2p)
                :
                : "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7",
                  "q8", "q9", "q10", "q11", "memory");
    n &= 15;
    if (n)
        MergeGeneric (outp, in1p, in2p, n);
}
#endif

/*****************************************************************************
 * RenderX: This algo works on a 8x8 block basic, it copies the top field
 * and apply a process to recreate the bottom field :
 *  If a 8x8 block is classified as :
 *   - progressive: it applies a small blend (1,6,1)
 *   - interlaced:
 *    * in the MMX version: we do a ME between the 2 fields, if there is a
 *    good match we use MC to recreate the bottom field (with a small
 *    blend (1,6,1) )
 *    * otherwise: it recreates the bottom field by an edge oriented
 *    interpolation.
  *****************************************************************************/

/* XDeint8x8Detect: detect if a 8x8 block is interlaced.
 * XXX: It need to access to 8x10
 * We use more than 8 lines to help with scrolling (text)
 * (and because XDeint8x8Frame use line 9)
 * XXX: smooth/uniform area with noise detection doesn't works well
 * but it's not really a problem because they don't have much details anyway
 */
static inline int ssd( int a ) { return a*a; }
static inline int XDeint8x8DetectC( uint8_t *src, int i_src )
{
    int y, x;
    int ff, fr;
    int fc;

    /* Detect interlacing */
    fc = 0;
    for( y = 0; y < 7; y += 2 )
    {
        ff = fr = 0;
        for( x = 0; x < 8; x++ )
        {
            fr += ssd(src[      x] - src[1*i_src+x]) +
                  ssd(src[i_src+x] - src[2*i_src+x]);
            ff += ssd(src[      x] - src[2*i_src+x]) +
                  ssd(src[i_src+x] - src[3*i_src+x]);
        }
        if( ff < 6*fr/8 && fr > 32 )
            fc++;

        src += 2*i_src;
    }

    return fc < 1 ? false : true;
}
#ifdef CAN_COMPILE_MMXEXT
static inline int XDeint8x8DetectMMXEXT( uint8_t *src, int i_src )
{

    int y, x;
    int32_t ff, fr;
    int fc;

    /* Detect interlacing */
    fc = 0;
    pxor_r2r( mm7, mm7 );
    for( y = 0; y < 9; y += 2 )
    {
        ff = fr = 0;
        pxor_r2r( mm5, mm5 );
        pxor_r2r( mm6, mm6 );
        for( x = 0; x < 8; x+=4 )
        {
            movd_m2r( src[        x], mm0 );
            movd_m2r( src[1*i_src+x], mm1 );
            movd_m2r( src[2*i_src+x], mm2 );
            movd_m2r( src[3*i_src+x], mm3 );

            punpcklbw_r2r( mm7, mm0 );
            punpcklbw_r2r( mm7, mm1 );
            punpcklbw_r2r( mm7, mm2 );
            punpcklbw_r2r( mm7, mm3 );

            movq_r2r( mm0, mm4 );

            psubw_r2r( mm1, mm0 );
            psubw_r2r( mm2, mm4 );

            psubw_r2r( mm1, mm2 );
            psubw_r2r( mm1, mm3 );

            pmaddwd_r2r( mm0, mm0 );
            pmaddwd_r2r( mm4, mm4 );
            pmaddwd_r2r( mm2, mm2 );
            pmaddwd_r2r( mm3, mm3 );
            paddd_r2r( mm0, mm2 );
            paddd_r2r( mm4, mm3 );
            paddd_r2r( mm2, mm5 );
            paddd_r2r( mm3, mm6 );
        }

        movq_r2r( mm5, mm0 );
        psrlq_i2r( 32, mm0 );
        paddd_r2r( mm0, mm5 );
        movd_r2m( mm5, fr );

        movq_r2r( mm6, mm0 );
        psrlq_i2r( 32, mm0 );
        paddd_r2r( mm0, mm6 );
        movd_r2m( mm6, ff );

        if( ff < 6*fr/8 && fr > 32 )
            fc++;

        src += 2*i_src;
    }
    return fc;
}
#endif

static inline void XDeint8x8MergeC( uint8_t *dst, int i_dst,
                                    uint8_t *src1, int i_src1,
                                    uint8_t *src2, int i_src2 )
{
    int y, x;

    /* Progressive */
    for( y = 0; y < 8; y += 2 )
    {
        memcpy( dst, src1, 8 );
        dst  += i_dst;

        for( x = 0; x < 8; x++ )
            dst[x] = (src1[x] + 6*src2[x] + src1[i_src1+x] + 4 ) >> 3;
        dst += i_dst;

        src1 += i_src1;
        src2 += i_src2;
    }
}

#ifdef CAN_COMPILE_MMXEXT
static inline void XDeint8x8MergeMMXEXT( uint8_t *dst, int i_dst,
                                         uint8_t *src1, int i_src1,
                                         uint8_t *src2, int i_src2 )
{
    static const uint64_t m_4 = INT64_C(0x0004000400040004);
    int y, x;

    /* Progressive */
    pxor_r2r( mm7, mm7 );
    for( y = 0; y < 8; y += 2 )
    {
        for( x = 0; x < 8; x +=4 )
        {
            movd_m2r( src1[x], mm0 );
            movd_r2m( mm0, dst[x] );

            movd_m2r( src2[x], mm1 );
            movd_m2r( src1[i_src1+x], mm2 );

            punpcklbw_r2r( mm7, mm0 );
            punpcklbw_r2r( mm7, mm1 );
            punpcklbw_r2r( mm7, mm2 );
            paddw_r2r( mm1, mm1 );
            movq_r2r( mm1, mm3 );
            paddw_r2r( mm3, mm3 );
            paddw_r2r( mm2, mm0 );
            paddw_r2r( mm3, mm1 );
            paddw_m2r( m_4, mm1 );
            paddw_r2r( mm1, mm0 );
            psraw_i2r( 3, mm0 );
            packuswb_r2r( mm7, mm0 );
            movd_r2m( mm0, dst[i_dst+x] );
        }
        dst += 2*i_dst;
        src1 += i_src1;
        src2 += i_src2;
    }
}

#endif

/* For debug */
static inline void XDeint8x8Set( uint8_t *dst, int i_dst, uint8_t v )
{
    int y;
    for( y = 0; y < 8; y++ )
        memset( &dst[y*i_dst], v, 8 );
}

/* XDeint8x8FieldE: Stupid deinterlacing (1,0,1) for block that miss a
 * neighbour
 * (Use 8x9 pixels)
 * TODO: a better one for the inner part.
 */
static inline void XDeint8x8FieldEC( uint8_t *dst, int i_dst,
                                     uint8_t *src, int i_src )
{
    int y, x;

    /* Interlaced */
    for( y = 0; y < 8; y += 2 )
    {
        memcpy( dst, src, 8 );
        dst += i_dst;

        for( x = 0; x < 8; x++ )
            dst[x] = (src[x] + src[2*i_src+x] ) >> 1;
        dst += 1*i_dst;
        src += 2*i_src;
    }
}
#ifdef CAN_COMPILE_MMXEXT
static inline void XDeint8x8FieldEMMXEXT( uint8_t *dst, int i_dst,
                                          uint8_t *src, int i_src )
{
    int y;

    /* Interlaced */
    for( y = 0; y < 8; y += 2 )
    {
        movq_m2r( src[0], mm0 );
        movq_r2m( mm0, dst[0] );
        dst += i_dst;

        movq_m2r( src[2*i_src], mm1 );
        pavgb_r2r( mm1, mm0 );

        movq_r2m( mm0, dst[0] );

        dst += 1*i_dst;
        src += 2*i_src;
    }
}
#endif

/* XDeint8x8Field: Edge oriented interpolation
 * (Need -4 and +5 pixels H, +1 line)
 */
static inline void XDeint8x8FieldC( uint8_t *dst, int i_dst,
                                    uint8_t *src, int i_src )
{
    int y, x;

    /* Interlaced */
    for( y = 0; y < 8; y += 2 )
    {
        memcpy( dst, src, 8 );
        dst += i_dst;

        for( x = 0; x < 8; x++ )
        {
            uint8_t *src2 = &src[2*i_src];
            /* I use 8 pixels just to match the MMX version, but it's overkill
             * 5 would be enough (less isn't good) */
            const int c0 = abs(src[x-4]-src2[x-2]) + abs(src[x-3]-src2[x-1]) +
                           abs(src[x-2]-src2[x+0]) + abs(src[x-1]-src2[x+1]) +
                           abs(src[x+0]-src2[x+2]) + abs(src[x+1]-src2[x+3]) +
                           abs(src[x+2]-src2[x+4]) + abs(src[x+3]-src2[x+5]);

            const int c1 = abs(src[x-3]-src2[x-3]) + abs(src[x-2]-src2[x-2]) +
                           abs(src[x-1]-src2[x-1]) + abs(src[x+0]-src2[x+0]) +
                           abs(src[x+1]-src2[x+1]) + abs(src[x+2]-src2[x+2]) +
                           abs(src[x+3]-src2[x+3]) + abs(src[x+4]-src2[x+4]);

            const int c2 = abs(src[x-2]-src2[x-4]) + abs(src[x-1]-src2[x-3]) +
                           abs(src[x+0]-src2[x-2]) + abs(src[x+1]-src2[x-1]) +
                           abs(src[x+2]-src2[x+0]) + abs(src[x+3]-src2[x+1]) +
                           abs(src[x+4]-src2[x+2]) + abs(src[x+5]-src2[x+3]);

            if( c0 < c1 && c1 <= c2 )
                dst[x] = (src[x-1] + src2[x+1]) >> 1;
            else if( c2 < c1 && c1 <= c0 )
                dst[x] = (src[x+1] + src2[x-1]) >> 1;
            else
                dst[x] = (src[x+0] + src2[x+0]) >> 1;
        }

        dst += 1*i_dst;
        src += 2*i_src;
    }
}
#ifdef CAN_COMPILE_MMXEXT
static inline void XDeint8x8FieldMMXEXT( uint8_t *dst, int i_dst,
                                         uint8_t *src, int i_src )
{
    int y, x;

    /* Interlaced */
    for( y = 0; y < 8; y += 2 )
    {
        memcpy( dst, src, 8 );
        dst += i_dst;

        for( x = 0; x < 8; x++ )
        {
            uint8_t *src2 = &src[2*i_src];
            int32_t c0, c1, c2;

            movq_m2r( src[x-2], mm0 );
            movq_m2r( src[x-3], mm1 );
            movq_m2r( src[x-4], mm2 );

            psadbw_m2r( src2[x-4], mm0 );
            psadbw_m2r( src2[x-3], mm1 );
            psadbw_m2r( src2[x-2], mm2 );

            movd_r2m( mm0, c2 );
            movd_r2m( mm1, c1 );
            movd_r2m( mm2, c0 );

            if( c0 < c1 && c1 <= c2 )
                dst[x] = (src[x-1] + src2[x+1]) >> 1;
            else if( c2 < c1 && c1 <= c0 )
                dst[x] = (src[x+1] + src2[x-1]) >> 1;
            else
                dst[x] = (src[x+0] + src2[x+0]) >> 1;
        }

        dst += 1*i_dst;
        src += 2*i_src;
    }
}
#endif

/* NxN arbitray size (and then only use pixel in the NxN block)
 */
static inline int XDeintNxNDetect( uint8_t *src, int i_src,
                                   int i_height, int i_width )
{
    int y, x;
    int ff, fr;
    int fc;


    /* Detect interlacing */
    /* FIXME way too simple, need to be more like XDeint8x8Detect */
    ff = fr = 0;
    fc = 0;
    for( y = 0; y < i_height - 2; y += 2 )
    {
        const uint8_t *s = &src[y*i_src];
        for( x = 0; x < i_width; x++ )
        {
            fr += ssd(s[      x] - s[1*i_src+x]);
            ff += ssd(s[      x] - s[2*i_src+x]);
        }
        if( ff < fr && fr > i_width / 2 )
            fc++;
    }

    return fc < 2 ? false : true;
}

static inline void XDeintNxNFrame( uint8_t *dst, int i_dst,
                                   uint8_t *src, int i_src,
                                   int i_width, int i_height )
{
    int y, x;

    /* Progressive */
    for( y = 0; y < i_height; y += 2 )
    {
        memcpy( dst, src, i_width );
        dst += i_dst;

        if( y < i_height - 2 )
        {
            for( x = 0; x < i_width; x++ )
                dst[x] = (src[x] + 2*src[1*i_src+x] + src[2*i_src+x] + 2 ) >> 2;
        }
        else
        {
            /* Blend last line */
            for( x = 0; x < i_width; x++ )
                dst[x] = (src[x] + src[1*i_src+x] ) >> 1;
        }
        dst += 1*i_dst;
        src += 2*i_src;
    }
}

static inline void XDeintNxNField( uint8_t *dst, int i_dst,
                                   uint8_t *src, int i_src,
                                   int i_width, int i_height )
{
    int y, x;

    /* Interlaced */
    for( y = 0; y < i_height; y += 2 )
    {
        memcpy( dst, src, i_width );
        dst += i_dst;

        if( y < i_height - 2 )
        {
            for( x = 0; x < i_width; x++ )
                dst[x] = (src[x] + src[2*i_src+x] ) >> 1;
        }
        else
        {
            /* Blend last line */
            for( x = 0; x < i_width; x++ )
                dst[x] = (src[x] + src[i_src+x]) >> 1;
        }
        dst += 1*i_dst;
        src += 2*i_src;
    }
}

static inline void XDeintNxN( uint8_t *dst, int i_dst, uint8_t *src, int i_src,
                              int i_width, int i_height )
{
    if( XDeintNxNDetect( src, i_src, i_width, i_height ) )
        XDeintNxNField( dst, i_dst, src, i_src, i_width, i_height );
    else
        XDeintNxNFrame( dst, i_dst, src, i_src, i_width, i_height );
}


static inline int median( int a, int b, int c )
{
    int min = a, max =a;
    if( b < min )
        min = b;
    else
        max = b;

    if( c < min )
        min = c;
    else if( c > max )
        max = c;

    return a + b + c - min - max;
}


/* XDeintBand8x8:
 */
static inline void XDeintBand8x8C( uint8_t *dst, int i_dst,
                                   uint8_t *src, int i_src,
                                   const int i_mbx, int i_modx )
{
    int x;

    for( x = 0; x < i_mbx; x++ )
    {
        int s;
        if( ( s = XDeint8x8DetectC( src, i_src ) ) )
        {
            if( x == 0 || x == i_mbx - 1 )
                XDeint8x8FieldEC( dst, i_dst, src, i_src );
            else
                XDeint8x8FieldC( dst, i_dst, src, i_src );
        }
        else
        {
            XDeint8x8MergeC( dst, i_dst,
                             &src[0*i_src], 2*i_src,
                             &src[1*i_src], 2*i_src );
        }

        dst += 8;
        src += 8;
    }

    if( i_modx )
        XDeintNxN( dst, i_dst, src, i_src, i_modx, 8 );
}
#ifdef CAN_COMPILE_MMXEXT
static inline void XDeintBand8x8MMXEXT( uint8_t *dst, int i_dst,
                                        uint8_t *src, int i_src,
                                        const int i_mbx, int i_modx )
{
    int x;

    /* Reset current line */
    for( x = 0; x < i_mbx; x++ )
    {
        int s;
        if( ( s = XDeint8x8DetectMMXEXT( src, i_src ) ) )
        {
            if( x == 0 || x == i_mbx - 1 )
                XDeint8x8FieldEMMXEXT( dst, i_dst, src, i_src );
            else
                XDeint8x8FieldMMXEXT( dst, i_dst, src, i_src );
        }
        else
        {
            XDeint8x8MergeMMXEXT( dst, i_dst,
                                  &src[0*i_src], 2*i_src,
                                  &src[1*i_src], 2*i_src );
        }

        dst += 8;
        src += 8;
    }

    if( i_modx )
        XDeintNxN( dst, i_dst, src, i_src, i_modx, 8 );
}
#endif

static void RenderX( picture_t *p_outpic, picture_t *p_pic )
{
    int i_plane;
    unsigned u_cpu = vlc_CPU();

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        const int i_mby = ( p_outpic->p[i_plane].i_visible_lines + 7 )/8 - 1;
        const int i_mbx = p_outpic->p[i_plane].i_visible_pitch/8;

        const int i_mody = p_outpic->p[i_plane].i_visible_lines - 8*i_mby;
        const int i_modx = p_outpic->p[i_plane].i_visible_pitch - 8*i_mbx;

        const int i_dst = p_outpic->p[i_plane].i_pitch;
        const int i_src = p_pic->p[i_plane].i_pitch;

        int y, x;

        for( y = 0; y < i_mby; y++ )
        {
            uint8_t *dst = &p_outpic->p[i_plane].p_pixels[8*y*i_dst];
            uint8_t *src = &p_pic->p[i_plane].p_pixels[8*y*i_src];

#ifdef CAN_COMPILE_MMXEXT
            if( u_cpu & CPU_CAPABILITY_MMXEXT )
                XDeintBand8x8MMXEXT( dst, i_dst, src, i_src, i_mbx, i_modx );
            else
#endif
                XDeintBand8x8C( dst, i_dst, src, i_src, i_mbx, i_modx );
        }

        /* Last line (C only)*/
        if( i_mody )
        {
            uint8_t *dst = &p_outpic->p[i_plane].p_pixels[8*y*i_dst];
            uint8_t *src = &p_pic->p[i_plane].p_pixels[8*y*i_src];

            for( x = 0; x < i_mbx; x++ )
            {
                XDeintNxN( dst, i_dst, src, i_src, 8, i_mody );

                dst += 8;
                src += 8;
            }

            if( i_modx )
                XDeintNxN( dst, i_dst, src, i_src, i_modx, i_mody );
        }
    }

#ifdef CAN_COMPILE_MMXEXT
    if( u_cpu & CPU_CAPABILITY_MMXEXT )
        emms();
#endif
}

/*****************************************************************************
 * Yadif (Yet Another DeInterlacing Filter).
 *****************************************************************************/
/* */
struct vf_priv_s {
    /*
     * 0: Output 1 frame for each frame.
     * 1: Output 1 frame for each field.
     * 2: Like 0 but skips spatial interlacing check.
     * 3: Like 1 but skips spatial interlacing check.
     *
     * In vlc, only & 0x02 has meaning, as we do the & 0x01 ourself.
     */
    int mode;
};

/* I am unsure it is the right one */
typedef intptr_t x86_reg;

#define FFABS(a) ((a) >= 0 ? (a) : (-(a)))
#define FFMAX(a,b)      __MAX(a,b)
#define FFMAX3(a,b,c)   FFMAX(FFMAX(a,b),c)
#define FFMIN(a,b)      __MIN(a,b)
#define FFMIN3(a,b,c)   FFMIN(FFMIN(a,b),c)

/* yadif.h comes from vf_yadif.c of mplayer project */
#include "yadif.h"

static int RenderYadif( filter_t *p_filter, picture_t *p_dst, picture_t *p_src, int i_order, int i_field )
{
    VLC_UNUSED(p_src);

    filter_sys_t *p_sys = p_filter->p_sys;

    /* */
    assert( i_order >= 0 && i_order <= 2 ); /* 2 = soft field repeat */
    assert( i_field == 0 || i_field == 1 );

    /* As the pitches must match, use ONLY pictures coming from picture_New()! */
    picture_t *p_prev = p_sys->pp_history[0];
    picture_t *p_cur  = p_sys->pp_history[1];
    picture_t *p_next = p_sys->pp_history[2];

    /* Account for soft field repeat.

       The "parity" parameter affects the algorithm like this (from yadif.h):
       uint8_t *prev2= parity ? prev : cur ;
       uint8_t *next2= parity ? cur  : next;

       The original parity expression that was used here is:
       (i_field ^ (i_order == i_field)) & 1

       Truth table:
       i_field = 0, i_order = 0  => 1
       i_field = 1, i_order = 1  => 0
       i_field = 1, i_order = 0  => 1
       i_field = 0, i_order = 1  => 0

       => equivalent with e.g.  (1 - i_order)  or  (i_order + 1) % 2

       Thus, in a normal two-field frame,
             parity 1 = first field  (i_order == 0)
             parity 0 = second field (i_order == 1)

       Now, with three fields, where the third is a copy of the first,
             i_order = 0  =>  parity 1 (as usual)
             i_order = 1  =>  due to the repeat, prev = cur, but also next = cur.
                              Because in such a case there is no motion (otherwise field repeat makes no sense),
                              we don't actually need to invoke Yadif's filter(). Thus, set "parity" to 2,
                              and use this to bypass the filter.
             i_order = 2  =>  parity 0 (as usual)
    */
    int yadif_parity;
    if( p_cur  &&  p_cur->i_nb_fields > 2 )
        yadif_parity = (i_order + 1) % 3; /* 1, *2*, 0; where 2 is a special value meaning "bypass filter". */
    else
        yadif_parity = (i_order + 1) % 2; /* 1, 0 */

    /* Filter if we have all the pictures we need */
    if( p_prev && p_cur && p_next )
    {
        /* */
        void (*filter)(struct vf_priv_s *p, uint8_t *dst, uint8_t *prev, uint8_t *cur, uint8_t *next, int w, int refs, int parity);
#if defined(HAVE_YADIF_SSE2)
        if( vlc_CPU() & CPU_CAPABILITY_SSE2 )
            filter = yadif_filter_line_mmx2;
        else
#endif
            filter = yadif_filter_line_c;

        for( int n = 0; n < p_dst->i_planes; n++ )
        {
            const plane_t *prevp = &p_prev->p[n];
            const plane_t *curp  = &p_cur->p[n];
            const plane_t *nextp = &p_next->p[n];
            plane_t *dstp        = &p_dst->p[n];

            for( int y = 1; y < dstp->i_visible_lines - 1; y++ )
            {
                if( (y % 2) == i_field  ||  yadif_parity == 2 )
                {
                    vlc_memcpy( &dstp->p_pixels[y * dstp->i_pitch],
                                &curp->p_pixels[y * curp->i_pitch], dstp->i_visible_pitch );
                }
                else
                {
                    struct vf_priv_s cfg;
                    /* Spatial checks only when enough data */
                    cfg.mode = (y >= 2 && y < dstp->i_visible_lines - 2) ? 0 : 2;

                    assert( prevp->i_pitch == curp->i_pitch && curp->i_pitch == nextp->i_pitch );
                    filter( &cfg,
                            &dstp->p_pixels[y * dstp->i_pitch],
                            &prevp->p_pixels[y * prevp->i_pitch],
                            &curp->p_pixels[y * curp->i_pitch],
                            &nextp->p_pixels[y * nextp->i_pitch],
                            dstp->i_visible_pitch,
                            curp->i_pitch,
                            yadif_parity );
                }

                /* We duplicate the first and last lines */
                if( y == 1 )
                    vlc_memcpy(&dstp->p_pixels[(y-1) * dstp->i_pitch], &dstp->p_pixels[y * dstp->i_pitch], dstp->i_pitch);
                else if( y == dstp->i_visible_lines - 2 )
                    vlc_memcpy(&dstp->p_pixels[(y+1) * dstp->i_pitch], &dstp->p_pixels[y * dstp->i_pitch], dstp->i_pitch);
            }
        }

        p_sys->i_frame_offset = 1; /* p_curr will be rendered at next frame, too */

        return VLC_SUCCESS;
    }
    else if( !p_prev && !p_cur && p_next )
    {
        /* NOTE: For the first frame, we use the default frame offset
                 as set by Open() or SetFilterMethod(). It is always 0. */

        /* FIXME not good as it does not use i_order/i_field */
        RenderX( p_dst, p_next );
        return VLC_SUCCESS;
    }
    else
    {
        p_sys->i_frame_offset = 1; /* p_curr will be rendered at next frame */

        return VLC_EGENERIC;
    }
}

/*****************************************************************************
* Phosphor - a framerate doubler that simulates gradual light decay of a CRT.
*****************************************************************************/

/**
 * This function converts a normal (full frame) plane_t into a field plane_t.
 *
 * Field plane_t's can be used e.g. for a weaving copy operation from two
 * source frames into one destination frame.
 *
 * The pixels themselves will not be touched; only the metadata is generated.
 * The same pixel data is shared by both the original plane_t and the field
 * plane_t. Note, however, that the bottom field's data starts from the
 * second line, so for the bottom field, the actual pixel pointer value
 * does not exactly match the original plane pixel pointer value. (It points
 * one line further down.)
 *
 * The caller must allocate p_dst (creating a local variable is fine).
 *
 * @param p_dst Field plane_t is written here. Must be non-NULL.
 * @param p_src Original full-frame plane_t. Must be non-NULL.
 * @param i_field Extract which field? 0 = top field, 1 = bottom field.
 * @see plane_CopyPixels()
 * @see ComposeFrame()
 * @see RenderPhosphor()
 */
static void FieldFromPlane( plane_t *p_dst, const plane_t *p_src, int i_field )
{
    assert( p_dst != NULL );
    assert( p_src != NULL );
    assert( i_field == 0  ||  i_field == 1 );

    /* Start with a copy of the metadata, and then update it to refer
       to one field only.

       We utilize the fact that plane_CopyPixels() differentiates between
       visible_pitch and pitch.

       The other field will be defined as the "margin" by doubling the pitch.
       The visible pitch will be left as in the original.
    */
    (*p_dst) = (*p_src);
    p_dst->i_lines /= 2;
    p_dst->i_visible_lines /= 2;
    p_dst->i_pitch *= 2;
    /* For the bottom field, skip the first line in the pixel data. */
    if( i_field == 1 )
        p_dst->p_pixels += p_src->i_pitch;
}

/**
 * Helper function: composes a frame from the given field pair.
 *
 * Caller must manage allocation/deallocation of p_outpic.
 *
 * The inputs are full pictures (frames); only one field
 * will be used from each.
 *
 * Chroma formats of the inputs must match. It is also desirable that the
 * visible pitches of both inputs are the same, so that this will do something
 * sensible. The pitch or visible pitch of the output does not need to match
 * with the input; the compatible (smaller) part of the visible pitch will
 * be filled.
 *
 * The i_output_chroma parameter must always be supplied, but it is only used
 * when the chroma format of the input is detected as 4:2:0. Available modes:
 *   - CC_ALTLINE:       Alternate line copy, like for luma. Chroma line 0
 *                       comes from top field picture, chroma line 1 comes
 *                       from bottom field picture, chroma line 2 from top
 *                       field picture, and so on. This is usually the right
 *                       choice for IVTCing NTSC DVD material, but rarely
 *                       for any other use cases.
 *   - CC_UPCONVERT:     The output will have 4:2:2 chroma. All 4:2:0 chroma
 *                       data from both input fields will be used to generate
 *                       the 4:2:2 chroma data of the output. Each output line
 *                       will thus have independent chroma. This is a good
 *                       choice for most purposes except IVTC, if the machine
 *                       can handle the increased throughput. (Make sure to
 *                       allocate a 4:2:2 output picture first!)
 *                       This mode can also be used for converting a 4:2:0
 *                       frame to 4:2:2 format (by passing the same input
 *                       picture for both input fields).
 *                       Conversions: I420, YV12 --> I422
 *                                    J420       --> J422
 *   - CC_SOURCE_TOP:    Copy chroma of source top field picture.
 *                       Ignore chroma of source bottom field picture.
 *   - CC_SOURCE_BOTTOM: Copy chroma of source bottom field picture.
 *                       Ignore chroma of source top field picture.
 *   - CC_MERGE:         Average the chroma of the input field pictures.
 *                       (Note that this has no effect if the input fields
 *                        come from the same frame.)
 *
 * @param p_outpic Composed picture is written here. Allocated by caller.
 * @param p_inpic_top Picture to extract the top field from.
 * @param p_inpic_bottom Picture to extract the bottom field from.
 * @param i_output_chroma Chroma operation mode for 4:2:0 (see function doc)
 * @see compose_chroma_t
 * @see RenderPhosphor()
 */
static void ComposeFrame( filter_t *p_filter, picture_t *p_outpic,
                          picture_t *p_inpic_top, picture_t *p_inpic_bottom,
                          compose_chroma_t i_output_chroma )
{
    assert( p_filter != NULL );
    assert( p_outpic != NULL );
    assert( p_inpic_top != NULL );
    assert( p_inpic_bottom != NULL );

    /* Valid 4:2:0 chroma handling modes. */
    assert( i_output_chroma == CC_ALTLINE       ||
            i_output_chroma == CC_UPCONVERT     ||
            i_output_chroma == CC_SOURCE_TOP    ||
            i_output_chroma == CC_SOURCE_BOTTOM ||
            i_output_chroma == CC_MERGE );

    const int i_chroma = p_filter->fmt_in.video.i_chroma;
    const bool b_i422 = i_chroma == VLC_CODEC_I422 ||
                        i_chroma == VLC_CODEC_J422;
    const bool b_upconvert_chroma = ( !b_i422  &&
                                      i_output_chroma == CC_UPCONVERT );

    for( int i_plane = 0 ; i_plane < p_inpic_top->i_planes ; i_plane++ )
    {
        bool b_is_chroma_plane = ( i_plane == U_PLANE || i_plane == V_PLANE );

        /* YV12 is YVU, but I422 is YUV. For such input, swap chroma planes
           in output when converting to 4:2:2. */
        int i_out_plane;
        if( b_is_chroma_plane  &&  b_upconvert_chroma  &&
            i_chroma == VLC_CODEC_YV12 )
        {
            if( i_plane == U_PLANE )
                i_out_plane = V_PLANE;
            else /* V_PLANE */
                i_out_plane = U_PLANE;
        }
        else
        {
            i_out_plane = i_plane;
        }

        /* Copy luma or chroma, alternating between input fields. */
        if( !b_is_chroma_plane  ||  b_i422  ||  i_output_chroma == CC_ALTLINE )
        {
            /* Do an alternating line copy. This is always done for luma,
               and for 4:2:2 chroma. It can be requested for 4:2:0 chroma
               using CC_ALTLINE (see function doc).

               Note that when we get here, the number of lines matches
               in input and output.
            */
            plane_t dst_top;
            plane_t dst_bottom;
            plane_t src_top;
            plane_t src_bottom;
            FieldFromPlane( &dst_top,    &p_outpic->p[i_out_plane],   0 );
            FieldFromPlane( &dst_bottom, &p_outpic->p[i_out_plane],   1 );
            FieldFromPlane( &src_top,    &p_inpic_top->p[i_plane],    0 );
            FieldFromPlane( &src_bottom, &p_inpic_bottom->p[i_plane], 1 );

            /* Copy each field from the corresponding source. */
            plane_CopyPixels( &dst_top,    &src_top    );
            plane_CopyPixels( &dst_bottom, &src_bottom );
        }
        else /* Input 4:2:0, on a chroma plane, and not in altline mode. */
        {
            if( i_output_chroma == CC_UPCONVERT )
            {
                /* Upconverting copy - use all data from both input fields.

                   This produces an output picture with independent chroma
                   for each field. It can be used for general input when
                   the two input frames are different.

                   The output is 4:2:2, but the input is 4:2:0. Thus the output
                   has twice the lines of the input, and each full chroma plane
                   in the input corresponds to a field chroma plane in the
                   output.
                */
                plane_t dst_top;
                plane_t dst_bottom;
                FieldFromPlane( &dst_top,    &p_outpic->p[i_out_plane], 0 );
                FieldFromPlane( &dst_bottom, &p_outpic->p[i_out_plane], 1 );

                /* Copy each field from the corresponding source. */
                plane_CopyPixels( &dst_top,    &p_inpic_top->p[i_plane]    );
                plane_CopyPixels( &dst_bottom, &p_inpic_bottom->p[i_plane] );
            }
            else if( i_output_chroma == CC_SOURCE_TOP )
            {
                /* Copy chroma of input top field. Ignore chroma of input
                   bottom field. Input and output are both 4:2:0, so we just
                   copy the whole plane. */
                plane_CopyPixels( &p_outpic->p[i_out_plane],
                                  &p_inpic_top->p[i_plane] );
            }
            else if( i_output_chroma == CC_SOURCE_BOTTOM )
            {
                /* Copy chroma of input bottom field. Ignore chroma of input
                   top field. Input and output are both 4:2:0, so we just
                   copy the whole plane. */
                plane_CopyPixels( &p_outpic->p[i_out_plane],
                                  &p_inpic_bottom->p[i_plane] );
            }
            else /* i_output_chroma == CC_MERGE */
            {
                /* Average the chroma of the input fields.
                   Input and output are both 4:2:0. */
                uint8_t *p_in_top, *p_in_bottom, *p_out_end, *p_out;
                p_in_top    = p_inpic_top->p[i_plane].p_pixels;
                p_in_bottom = p_inpic_bottom->p[i_plane].p_pixels;
                p_out = p_outpic->p[i_out_plane].p_pixels;
                p_out_end = p_out + p_outpic->p[i_out_plane].i_pitch
                                  * p_outpic->p[i_out_plane].i_visible_lines;

                int w = FFMIN3( p_inpic_top->p[i_plane].i_visible_pitch,
                                p_inpic_bottom->p[i_plane].i_visible_pitch,
                                p_outpic->p[i_plane].i_visible_pitch );

                for( ; p_out < p_out_end ; )
                {
                    Merge( p_out, p_in_top, p_in_bottom, w );
                    p_out       += p_outpic->p[i_out_plane].i_pitch;
                    p_in_top    += p_inpic_top->p[i_plane].i_pitch;
                    p_in_bottom += p_inpic_bottom->p[i_plane].i_pitch;
                }
                EndMerge();
            }
        }
    }
}

#undef Merge

/**
 * Helper function: dims (darkens) the given field of the given picture.
 *
 * This is used for simulating CRT light output decay in RenderPhosphor().
 *
 * The strength "1" is recommended. It's a matter of taste,
 * so it's parametrized.
 *
 * Note on chroma formats:
 *   - If input is 4:2:2, all planes are processed.
 *   - If input is 4:2:0, only the luma plane is processed, because both fields
 *     have the same chroma. This will distort colours, especially for high
 *     filter strengths, especially for pixels whose U and/or V values are
 *     far away from the origin (which is at 128 in uint8 format).
 *
 * @param p_dst Input/output picture. Will be modified in-place.
 * @param i_field Darken which field? 0 = top, 1 = bottom.
 * @param i_strength Strength of effect: 1, 2 or 3 (division by 2, 4 or 8).
 * @see RenderPhosphor()
 * @see ComposeFrame()
 */
static void DarkenField( picture_t *p_dst, const int i_field,
                                           const int i_strength )
{
    assert( p_dst != NULL );
    assert( i_field == 0 || i_field == 1 );
    assert( i_strength >= 1 && i_strength <= 3 );

    unsigned u_cpu = vlc_CPU();

    /* Bitwise ANDing with this clears the i_strength highest bits
       of each byte */
#ifdef CAN_COMPILE_MMXEXT
    uint64_t i_strength_u64 = i_strength; /* for MMX version (needs to know
                                             number of bits) */
#endif
    const uint8_t  remove_high_u8 = 0xFF >> i_strength;
    const uint64_t remove_high_u64 = remove_high_u8 *
                                            INT64_C(0x0101010101010101);

    /* Process luma.

       For luma, the operation is just a shift + bitwise AND, so we vectorize
       even in the C version.

       There is an MMX version, too, because it performs about twice faster.
    */
    int i_plane = Y_PLANE;
    uint8_t *p_out, *p_out_end;
    int w = p_dst->p[i_plane].i_visible_pitch;
    p_out = p_dst->p[i_plane].p_pixels;
    p_out_end = p_out + p_dst->p[i_plane].i_pitch
                      * p_dst->p[i_plane].i_visible_lines;

    /* skip first line for bottom field */
    if( i_field == 1 )
        p_out += p_dst->p[i_plane].i_pitch;

    int wm8 = w % 8;   /* remainder */
    int w8  = w - wm8; /* part of width that is divisible by 8 */
    for( ; p_out < p_out_end ; p_out += 2*p_dst->p[i_plane].i_pitch )
    {
        uint64_t *po = (uint64_t *)p_out;
#ifdef CAN_COMPILE_MMXEXT
        if( u_cpu & CPU_CAPABILITY_MMXEXT )
        {
            movq_m2r( i_strength_u64,  mm1 );
            movq_m2r( remove_high_u64, mm2 );
            for( int x = 0 ; x < w8; x += 8 )
            {
                movq_m2r( (*po), mm0 );

                psrlq_r2r( mm1, mm0 );
                pand_r2r(  mm2, mm0 );

                movq_r2m( mm0, (*po++) );
            }
        }
        else
        {
#endif
            for( int x = 0 ; x < w8; x += 8, ++po )
                (*po) = ( ((*po) >> i_strength) & remove_high_u64 );
#ifdef CAN_COMPILE_MMXEXT
        }
#endif
        /* handle the width remainder */
        if( wm8 )
        {
            uint8_t *po_temp = (uint8_t *)po;
            for( int x = 0 ; x < wm8; ++x, ++po_temp )
                (*po_temp) = ( ((*po_temp) >> i_strength) & remove_high_u8 );
        }
    }

    /* Process chroma if the field chromas are independent.

       The origin (black) is at YUV = (0, 128, 128) in the uint8 format.
       The chroma processing is a bit more complicated than luma,
       and needs MMX for vectorization.
    */
    if( p_dst->format.i_chroma == VLC_CODEC_I422  ||
        p_dst->format.i_chroma == VLC_CODEC_J422 )
    {
        for( i_plane = 0 ; i_plane < p_dst->i_planes ; i_plane++ )
        {
            if( i_plane == Y_PLANE )
                continue; /* luma already handled */

            int w = p_dst->p[i_plane].i_visible_pitch;
#ifdef CAN_COMPILE_MMXEXT
            int wm8 = w % 8;   /* remainder */
            int w8  = w - wm8; /* part of width that is divisible by 8 */
#endif
            p_out = p_dst->p[i_plane].p_pixels;
            p_out_end = p_out + p_dst->p[i_plane].i_pitch
                              * p_dst->p[i_plane].i_visible_lines;

            /* skip first line for bottom field */
            if( i_field == 1 )
                p_out += p_dst->p[i_plane].i_pitch;

            for( ; p_out < p_out_end ; p_out += 2*p_dst->p[i_plane].i_pitch )
            {
#ifdef CAN_COMPILE_MMXEXT
                /* See also easy-to-read C version below. */
                if( u_cpu & CPU_CAPABILITY_MMXEXT )
                {
                    static const mmx_t b128 = { .uq = 0x8080808080808080ULL };
                    movq_m2r( b128, mm5 );
                    movq_m2r( i_strength_u64,  mm6 );
                    movq_m2r( remove_high_u64, mm7 );

                    uint64_t *po = (uint64_t *)p_out;
                    for( int x = 0 ; x < w8; x += 8 )
                    {
                        movq_m2r( (*po), mm0 );

                        movq_r2r( mm5, mm2 ); /* 128 */
                        movq_r2r( mm0, mm1 ); /* copy of data */
                        psubusb_r2r( mm2, mm1 ); /* mm1 = max(data - 128, 0) */
                        psubusb_r2r( mm0, mm2 ); /* mm2 = max(128 - data, 0) */

                        /* >> i_strength */
                        psrlq_r2r( mm6, mm1 );
                        psrlq_r2r( mm6, mm2 );
                        pand_r2r(  mm7, mm1 );
                        pand_r2r(  mm7, mm2 );

                        /* collect results from pos./neg. parts */
                        psubb_r2r( mm2, mm1 );
                        paddb_r2r( mm5, mm1 );

                        movq_r2m( mm1, (*po++) );
                    }

                    /* handle the width remainder */
                    if( wm8 )
                    {
                        /* The output is closer to 128 than the input;
                           the result always fits in uint8. */
                        uint8_t *po8 = (uint8_t *)po;
                        for( int x = 0 ; x < wm8; ++x, ++po8 )
                            (*po8) = 128 + ( ((*po8) - 128) /
                                                  (1 << i_strength) );
                    }
                }
                else
                {
#endif
                    /* 4:2:2 chroma handler, C version */
                    uint8_t *po = p_out;
                    for( int x = 0 ; x < w; ++x, ++po )
                        (*po) = 128 + ( ((*po) - 128) / (1 << i_strength) );
#ifdef CAN_COMPILE_MMXEXT
                }
#endif
            } /* for p_out... */
        } /* for i_plane... */
    } /* if b_i422 */

#ifdef CAN_COMPILE_MMXEXT
    if( u_cpu & CPU_CAPABILITY_MMXEXT )
        emms();
#endif
}

/**
 * Deinterlace filter. Simulates an interlaced CRT TV (to some extent).
 *
 * The main use case for this filter is anime for which IVTC is not applicable.
 * This is the case, if 24fps telecined material has been mixed with 60fps
 * interlaced effects, such as in Sol Bianca or Silent Mobius. It can also
 * be used for true interlaced video, such as most camcorder recordings.
 *
 * The filter has several modes for handling 4:2:0 chroma for those output
 * frames that fall across input frame temporal boundaries (i.e. fields come
 * from different frames). Upconvert (to 4:2:2) provides the most accurate
 * CRT simulation, but requires more CPU and memory bandwidth than the other
 * modes. The other modes keep the chroma at 4:2:0.
 *
 * About these modes: telecined input (such as NTSC anime DVDs) works better
 * with AltLine, while true interlaced input works better with Latest.
 * Merge is a compromise, which may or may not look acceptable.
 * The mode can be set in the VLC advanced configuration,
 * All settings > Video > Filters > Deinterlace
 *
 * Technically speaking, this is an interlaced field renderer targeted for
 * progressive displays. It works by framerate doubling, and simulating one
 * step of light output decay of the "old" field during the "new" field,
 * until the next new field comes in to replace the "old" one.
 *
 * While playback is running, the simulated light decay gives the picture an
 * appearance of visible "scanlines", much like on a real TV. Only when the
 * video is paused, it is clearly visible that one of the fields is actually
 * brighter than the other.
 *
 * The main differences to the Bob algorithm are:
 *  - in addition to the current field, the previous one (fading out)
 *    is also rendered
 *  - some horizontal lines don't seem to flicker as much
 *  - scanline visual effect (adjustable; the dimmer strength can be set
 *    in the VLC advanced configuration)
 *  - the picture appears 25%, 38% or 44% darker on average (for dimmer
 *    strengths 1, 2 and 3)
 *  - if the input has 4:2:0 chroma, the colours may look messed up in some
 *    output frames. This is a limitation of the 4:2:0 chroma format, and due
 *    to the fact that both fields are present in each output picture. Usually
 *    this doesn't matter in practice, but see the 4:2:0 chroma mode setting
 *    in the configuration if needed (it may help a bit).
 *
 * In addition, when this filter is used on an LCD computer monitor,
 * the main differences to a real CRT TV are:
 *  - Pixel shape and grid layout; CRT TVs were designed for interlaced
 *    field rendering, while LCD monitors weren't.
 *  - No scan flicker even though the display runs (usually) at 60Hz.
 *    (This at least is a good thing.)
 *
 * The output vertical resolution should be large enough for the scaling
 * not to have a too adverse effect on the regular scanline pattern.
 * In practice, NTSC video can be acceptably rendered already at 1024x600
 * if fullscreen even on an LCD. PAL video requires more.
 *
 * Just like Bob, this filter works properly only if the input framerate
 * is stable. Otherwise the scanline effect breaks down and the picture
 * will flicker.
 *
 * Soft field repeat (repeat_pict) is supported. Note that the generated
 * "repeated" output picture is unique because of the simulated light decay.
 * Its "old" field comes from the same input frame as the "new" one, unlike
 * the first output picture of the same frame.
 *
 * As many output frames should be requested for each input frame as is
 * indicated by p_src->i_nb_fields. This is done by calling this function
 * several times, first with i_order = 0, and then with all other parameters
 * the same, but a new p_dst, increasing i_order (1 for second field,
 * and then if i_nb_fields = 3, also i_order = 2 to get the repeated first
 * field), and alternating i_field (starting, at i_order = 0, with the field
 * according to p_src->b_top_field_first). See Deinterlace() for an example.
 *
 * @param p_filter The filter instance. Must be non-NULL.
 * @param p_dst Output frame. Must be allocated by caller.
 * @param p_src Input frame. Must exist.
 * @param i_order Temporal field number: 0 = first, 1 = second, 2 = rep. first.
 * @param i_field Render which field? 0 = top field, 1 = bottom field.
 * @return VLC error code (int).
 * @retval VLC_SUCCESS The requested field was rendered into p_dst.
 * @retval VLC_EGENERIC No pictures in history buffer, cannot render.
 * @see RenderBob()
 * @see RenderLinear()
 * @see Deinterlace()
 */
static int RenderPhosphor( filter_t *p_filter,
                           picture_t *p_dst, picture_t *p_src,
                           int i_order, int i_field )
{
    assert( p_filter != NULL );
    assert( p_dst != NULL );
    assert( p_src != NULL );
    assert( i_order >= 0 && i_order <= 2 ); /* 2 = soft field repeat */
    assert( i_field == 0 || i_field == 1 );

    filter_sys_t *p_sys = p_filter->p_sys;

    /* Last two input frames */
    picture_t *p_in  = p_sys->pp_history[HISTORY_SIZE-1];
    picture_t *p_old = p_sys->pp_history[HISTORY_SIZE-2];

    /* Use the same input picture as "old" at the first frame after startup */
    if( !p_old )
        p_old = p_in;

    /* If the history mechanism has failed, we can't do anything. */
    if( !p_in )
        return VLC_EGENERIC;

    assert( p_old != NULL );
    assert( p_in != NULL );

    /* Decide sources for top & bottom fields of output. */
    picture_t *p_in_top    = p_in;
    picture_t *p_in_bottom = p_in;
    /* For the first output field this frame,
       grab "old" field from previous frame. */
    if( i_order == 0 )
    {
        if( i_field == 0 ) /* rendering top field */
            p_in_bottom = p_old;
        else /* i_field == 1, rendering bottom field */
            p_in_top = p_old;
    }

    compose_chroma_t cc;
    switch( p_sys->phosphor.i_chroma_for_420 )
    {
        case PC_BLEND:
            cc = CC_MERGE;
            break;
        case PC_LATEST:
            if( i_field == 0 )
                cc = CC_SOURCE_TOP;
            else /* i_field == 1 */
                cc = CC_SOURCE_BOTTOM;
            break;
        case PC_ALTLINE:
            cc = CC_ALTLINE;
            break;
        case PC_UPCONVERT:
            cc = CC_UPCONVERT;
            break;
        default:
            /* The above are the only possibilities, if there are no bugs. */
            assert(0);
            break;
    }

    ComposeFrame( p_filter, p_dst, p_in_top, p_in_bottom, cc );

    /* Simulate phosphor light output decay for the old field.

       The dimmer can also be switched off in the configuration, but that is
       more of a technical curiosity or an educational toy for advanced users
       than a useful deinterlacer mode (although it does make telecined
       material look slightly better than without any filtering).

       In most use cases the dimmer is used.
    */
    if( p_sys->phosphor.i_dimmer_strength > 0 )
        DarkenField( p_dst, !i_field, p_sys->phosphor.i_dimmer_strength );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * video filter2 functions
 *****************************************************************************/
#define DEINTERLACE_DST_SIZE 3
static picture_t *Deinterlace( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    picture_t *p_dst[DEINTERLACE_DST_SIZE];

    /* Request output picture */
    p_dst[0] = filter_NewPicture( p_filter );
    if( p_dst[0] == NULL )
    {
        picture_Release( p_pic );
        return NULL;
    }
    picture_CopyProperties( p_dst[0], p_pic );

    /* Any unused p_dst pointers must be NULL, because they are used to check how many output frames we have. */
    for( int i = 1; i < DEINTERLACE_DST_SIZE; ++i )
        p_dst[i] = NULL;

    /* Update the input frame history, if the currently active algorithm needs it. */
    if( p_sys->b_use_frame_history )
    {
        /* Duplicate the picture
         * TODO when the vout rework is finished, picture_Hold() might be enough
         * but becarefull, the pitches must match */
        picture_t *p_dup = picture_NewFromFormat( &p_pic->format );
        if( p_dup )
            picture_Copy( p_dup, p_pic );

        /* Slide the history */
        if( p_sys->pp_history[0] )
            picture_Release( p_sys->pp_history[0] );
        for( int i = 1; i < HISTORY_SIZE; i++ )
            p_sys->pp_history[i-1] = p_sys->pp_history[i];
        p_sys->pp_history[HISTORY_SIZE-1] = p_dup;
    }

    /* Slide the metadata history. */
    for( int i = 1; i < METADATA_SIZE; i++ )
    {
        p_sys->meta.pi_date[i-1]            = p_sys->meta.pi_date[i];
        p_sys->meta.pi_nb_fields[i-1]       = p_sys->meta.pi_nb_fields[i];
        p_sys->meta.pb_top_field_first[i-1] = p_sys->meta.pb_top_field_first[i];
    }
    /* The last element corresponds to the current input frame. */
    p_sys->meta.pi_date[METADATA_SIZE-1]            = p_pic->date;
    p_sys->meta.pi_nb_fields[METADATA_SIZE-1]       = p_pic->i_nb_fields;
    p_sys->meta.pb_top_field_first[METADATA_SIZE-1] = p_pic->b_top_field_first;

    /* Remember the frame offset that we should use for this frame.
       The value in p_sys will be updated to reflect the correct value
       for the *next* frame when we call the renderer. */
    int i_frame_offset = p_sys->i_frame_offset;
    int i_meta_idx     = (METADATA_SIZE-1) - i_frame_offset;

    /* These correspond to the current *outgoing* frame. */
    bool b_top_field_first;
    int i_nb_fields;
    if( i_frame_offset != CUSTOM_PTS )
    {
        /* Pick the correct values from the history. */
        b_top_field_first = p_sys->meta.pb_top_field_first[i_meta_idx];
        i_nb_fields       = p_sys->meta.pi_nb_fields[i_meta_idx];
    }
    else
    {
        /* Framerate doublers must not request CUSTOM_PTS, as they need the original field timings,
           and need Deinterlace() to allocate the correct number of output frames. */
        assert( !p_sys->b_double_rate );

        /* NOTE: i_nb_fields is only used for framerate doublers, so it is unused in this case.
                 b_top_field_first is only passed to the algorithm. We assume that algorithms that
                 request CUSTOM_PTS will, if necessary, extract the TFF/BFF information themselves.
        */
        b_top_field_first = p_pic->b_top_field_first; /* this is not guaranteed to be meaningful */
        i_nb_fields       = p_pic->i_nb_fields;       /* unused */
    }

    /* For framerate doublers, determine field duration and allocate output frames. */
    mtime_t i_field_dur = 0;
    int i_double_rate_alloc_end = 0; /* One past last for allocated output frames in p_dst[].
                                        Used only for framerate doublers. Will be inited below.
                                        Declared here because the PTS logic needs the result. */
    if( p_sys->b_double_rate )
    {
        /* Calculate one field duration. */
        int i = 0;
        int iend = METADATA_SIZE-1;
        /* Find oldest valid logged date. Note: the current input frame doesn't count. */
        for( ; i < iend; i++ )
            if( p_sys->meta.pi_date[i] > VLC_TS_INVALID )
                break;
        if( i < iend )
        {
            /* Count how many fields the valid history entries (except the new frame) represent. */
            int i_fields_total = 0;
            for( int j = i ; j < iend; j++ )
                i_fields_total += p_sys->meta.pi_nb_fields[j];
            /* One field took this long. */
            i_field_dur = (p_pic->date - p_sys->meta.pi_date[i]) / i_fields_total;
        }
        /* Note that we default to field duration 0 if it could not be determined.
           This behaves the same as the old code - leaving the extra output frame
           dates the same as p_pic->date if the last cached date was not valid.
        */

        i_double_rate_alloc_end = i_nb_fields;
        if( i_nb_fields > DEINTERLACE_DST_SIZE )
        {
            /* Note that the effective buffer size depends also on the constant private_picture in vout_wrapper.c,
               since that determines the maximum number of output pictures filter_NewPicture() will successfully
               allocate for one input frame.
            */
            msg_Err( p_filter, "Framerate doubler: output buffer too small; fields = %d, buffer size = %d. Dropping the remaining fields.", i_nb_fields, DEINTERLACE_DST_SIZE );
            i_double_rate_alloc_end = DEINTERLACE_DST_SIZE;
        }

        /* Allocate output frames. */
        for( int i = 1; i < i_double_rate_alloc_end ; ++i )
        {
            p_dst[i-1]->p_next =
            p_dst[i]           = filter_NewPicture( p_filter );
            if( p_dst[i] )
            {
                picture_CopyProperties( p_dst[i], p_pic );
            }
            else
            {
                msg_Err( p_filter, "Framerate doubler: could not allocate output frame %d", i+1 );
                i_double_rate_alloc_end = i; /* Inform the PTS logic about the correct end position. */
                break; /* If this happens, the rest of the allocations aren't likely to work, either... */
            }
        }
        /* Now we have allocated *up to* the correct number of frames; normally, exactly the correct number.
           Upon alloc failure, we may have succeeded in allocating *some* output frames, but fewer than
           were desired. In such a case, as many will be rendered as were successfully allocated.

           Note that now p_dst[i] != NULL for 0 <= i < i_double_rate_alloc_end. */
    }
    assert( p_sys->b_double_rate  ||  p_dst[1] == NULL );
    assert( i_nb_fields > 2  ||  p_dst[2] == NULL );

    /* Render */
    switch( p_sys->i_mode )
    {
        case DEINTERLACE_DISCARD:
            RenderDiscard( p_filter, p_dst[0], p_pic, 0 );
            break;

        case DEINTERLACE_BOB:
            RenderBob( p_filter, p_dst[0], p_pic, !b_top_field_first );
            if( p_dst[1] )
                RenderBob( p_filter, p_dst[1], p_pic, b_top_field_first );
            if( p_dst[2] )
                RenderBob( p_filter, p_dst[2], p_pic, !b_top_field_first );
            break;;

        case DEINTERLACE_LINEAR:
            RenderLinear( p_filter, p_dst[0], p_pic, !b_top_field_first );
            if( p_dst[1] )
                RenderLinear( p_filter, p_dst[1], p_pic, b_top_field_first );
            if( p_dst[2] )
                RenderLinear( p_filter, p_dst[2], p_pic, !b_top_field_first );
            break;

        case DEINTERLACE_MEAN:
            RenderMean( p_filter, p_dst[0], p_pic );
            break;

        case DEINTERLACE_BLEND:
            RenderBlend( p_filter, p_dst[0], p_pic );
            break;

        case DEINTERLACE_X:
            RenderX( p_dst[0], p_pic );
            break;

        case DEINTERLACE_YADIF:
            if( RenderYadif( p_filter, p_dst[0], p_pic, 0, 0 ) )
                goto drop;
            break;

        case DEINTERLACE_YADIF2X:
            if( RenderYadif( p_filter, p_dst[0], p_pic, 0, !b_top_field_first ) )
                goto drop;
            if( p_dst[1] )
                RenderYadif( p_filter, p_dst[1], p_pic, 1, b_top_field_first );
            if( p_dst[2] )
                RenderYadif( p_filter, p_dst[2], p_pic, 2, !b_top_field_first );
            break;

        case DEINTERLACE_PHOSPHOR:
            if( RenderPhosphor( p_filter, p_dst[0], p_pic, 0,
                                !b_top_field_first ) )
                goto drop;
            if( p_dst[1] )
                RenderPhosphor( p_filter, p_dst[1], p_pic, 1,
                                b_top_field_first );
            if( p_dst[2] )
                RenderPhosphor( p_filter, p_dst[2], p_pic, 2,
                                !b_top_field_first );
            break;
    }

    /* Set output timestamps, if the algorithm didn't request CUSTOM_PTS for this frame. */
    assert( i_frame_offset <= METADATA_SIZE  ||  i_frame_offset == CUSTOM_PTS );
    if( i_frame_offset != CUSTOM_PTS )
    {
        mtime_t i_base_pts = p_sys->meta.pi_date[i_meta_idx];

        /* Note: in the usual case (i_frame_offset = 0  and  b_double_rate = false),
                 this effectively does nothing. This is needed to correct the timestamp
                 when i_frame_offset > 0. */
        p_dst[0]->date = i_base_pts;

        if( p_sys->b_double_rate )
        {
            /* Processing all actually allocated output frames. */
            for( int i = 1; i < i_double_rate_alloc_end; ++i )
            {
                /* XXX it's not really good especially for the first picture, but
                 * I don't think that delaying by one frame is worth it */
                if( i_base_pts > VLC_TS_INVALID )
                    p_dst[i]->date = i_base_pts + i * i_field_dur;
                else
                    p_dst[i]->date = VLC_TS_INVALID;
            }
        }
    }

    p_dst[0]->b_progressive = true;
    for( int i = 1; i < DEINTERLACE_DST_SIZE; ++i )
    {
        if( p_dst[i] )
        {
            p_dst[i]->b_progressive = true;
            p_dst[i]->i_nb_fields = 2;
        }
    }

    picture_Release( p_pic );
    return p_dst[0];

drop:
    picture_Release( p_dst[0] );
    for( int i = 1; i < DEINTERLACE_DST_SIZE; ++i )
    {
        if( p_dst[i] )
            picture_Release( p_dst[i] );
    }
    picture_Release( p_pic );
    return NULL;
}

static void Flush( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    for( int i = 0; i < METADATA_SIZE; i++ )
    {
        p_sys->meta.pi_date[i] = VLC_TS_INVALID;
        p_sys->meta.pi_nb_fields[i] = 2;
        p_sys->meta.pb_top_field_first[i] = true;
    }
    p_sys->i_frame_offset = 0; /* reset to default value (first frame after flush cannot have offset) */
    for( int i = 0; i < HISTORY_SIZE; i++ )
    {
        if( p_sys->pp_history[i] )
            picture_Release( p_sys->pp_history[i] );
        p_sys->pp_history[i] = NULL;
    }
}

static int Mouse( filter_t *p_filter,
                  vlc_mouse_t *p_mouse, const vlc_mouse_t *p_old, const vlc_mouse_t *p_new )
{
    VLC_UNUSED(p_old);
    *p_mouse = *p_new;
    if( p_filter->p_sys->b_half_height )
        p_mouse->i_y *= 2;
    return VLC_SUCCESS;
}


/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys;

    if( !IsChromaSupported( p_filter->fmt_in.video.i_chroma ) )
        return VLC_EGENERIC;

    /* */
    p_sys = p_filter->p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->i_mode = DEINTERLACE_BLEND;
    p_sys->b_double_rate = false;
    p_sys->b_half_height = true;
    p_sys->b_use_frame_history = false;
    for( int i = 0; i < METADATA_SIZE; i++ )
    {
        p_sys->meta.pi_date[i] = VLC_TS_INVALID;
        p_sys->meta.pi_nb_fields[i] = 2;
        p_sys->meta.pb_top_field_first[i] = true;
    }
    p_sys->i_frame_offset = 0; /* start with default value (first-ever frame cannot have offset) */
    for( int i = 0; i < HISTORY_SIZE; i++ )
        p_sys->pp_history[i] = NULL;

#if defined(CAN_COMPILE_C_ALTIVEC)
    if( vlc_CPU() & CPU_CAPABILITY_ALTIVEC )
    {
        p_sys->pf_merge = MergeAltivec;
        p_sys->pf_end_merge = NULL;
    }
    else
#endif
#if defined(CAN_COMPILE_SSE)
    if( vlc_CPU() & CPU_CAPABILITY_SSE2 )
    {
        p_sys->pf_merge = MergeSSE2;
        p_sys->pf_end_merge = EndMMX;
    }
    else
#endif
#if defined(CAN_COMPILE_MMXEXT)
    if( vlc_CPU() & CPU_CAPABILITY_MMXEXT )
    {
        p_sys->pf_merge = MergeMMXEXT;
        p_sys->pf_end_merge = EndMMX;
    }
    else
#endif
#if defined(CAN_COMPILE_3DNOW)
    if( vlc_CPU() & CPU_CAPABILITY_3DNOW )
    {
        p_sys->pf_merge = Merge3DNow;
        p_sys->pf_end_merge = End3DNow;
    }
    else
#endif
#if defined __ARM_NEON__
    if( vlc_CPU() & CPU_CAPABILITY_NEON )
    {
        p_sys->pf_merge = MergeNEON;
        p_sys->pf_end_merge = NULL;
    }
    else
#endif
    {
        p_sys->pf_merge = MergeGeneric;
        p_sys->pf_end_merge = NULL;
    }

    /* */
    config_ChainParse( p_filter, FILTER_CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    char *psz_mode = var_GetNonEmptyString( p_filter, FILTER_CFG_PREFIX "mode" );
    SetFilterMethod( p_filter, psz_mode, p_filter->fmt_in.video.i_chroma );
    free( psz_mode );

    if( p_sys->i_mode == DEINTERLACE_PHOSPHOR )
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
    }
    else
    {
        p_sys->phosphor.i_chroma_for_420 = PC_ALTLINE;
        p_sys->phosphor.i_dimmer_strength = 1;
    }

    /* */
    video_format_t fmt;
    GetOutputFormat( p_filter, &fmt, &p_filter->fmt_in.video );
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
    p_filter->pf_video_flush  = Flush;
    p_filter->pf_video_mouse  = Mouse;

    msg_Dbg( p_filter, "deinterlacing" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: clean up the filter
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;

    Flush( p_filter );
    free( p_filter->p_sys );
}

