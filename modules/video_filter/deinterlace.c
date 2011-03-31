/*****************************************************************************
 * deinterlace.c : deinterlacer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2011 the VideoLAN team
 * $Id$
 *
 * Author: Sam Hocevar <sam@zoy.org>
 *         Juha Jeronen <juha.jeronen@jyu.fi> (Phosphor and IVTC modes)
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
#include <stdint.h> /* int_fast32_t */

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
#define DEINTERLACE_IVTC     10

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
    "yadif", "yadif2x", "phosphor", "ivtc" };
static const char *const mode_list_text[] = {
    N_("Discard"), N_("Blend"), N_("Mean"), N_("Bob"), N_("Linear"), "X",
    "Yadif", "Yadif (2x)", N_("Phosphor"), N_("Film NTSC (IVTC)") };

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
static int  RenderIVTC    ( filter_t *, picture_t *, picture_t * );

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

/**
 * Inverse telecine subsystem state.
 * @see RenderIVTC()
 */
#define IVTC_NUM_FIELD_PAIRS 7
#define IVTC_DETECTION_HISTORY_SIZE 3
#define IVTC_LATEST (IVTC_DETECTION_HISTORY_SIZE-1)
typedef struct
{
    int i_mode; /**< Detecting, hard TC, or soft TC. @see ivtc_mode */
    int i_old_mode; /**< @see IVTCSoftTelecineDetect() */

    int i_cadence_pos; /**< Cadence counter, 0..4. Runs when locked on. */
    int i_tfd; /**< TFF or BFF telecine. Detected from the video. */

    /** Raw low-level detector output.
     *
     *  @see IVTCLowLevelDetect()
     */
    int pi_scores[IVTC_NUM_FIELD_PAIRS]; /**< Interlace scores. */
    int pi_motion[IVTC_DETECTION_HISTORY_SIZE]; /**< 8x8 blocks with motion. */
    int pi_top_rep[IVTC_DETECTION_HISTORY_SIZE]; /**< Hard top field repeat. */
    int pi_bot_rep[IVTC_DETECTION_HISTORY_SIZE]; /**< Hard bot field repeat. */

    /** Interlace scores of outgoing frames, used for judging IVTC output
     *  (detecting cadence breaks).
     *
     *  @see IVTCOutputOrDropFrame()
     */
    int pi_final_scores[IVTC_DETECTION_HISTORY_SIZE];

    /** Cadence position detection history (in ivtc_cadence_pos format).
     *  Contains the detected cadence position and a corresponding
     *  reliability flag for each algorithm.
     *
     *  s = scores, interlace scores based algorithm, original to this filter.
     *  v = vektor, hard field repeat based algorithm, inspired by
     *              the TVTime/Xine IVTC filter by Billy Biggs (Vektor).
     *
     *  Each algorithm may also keep internal, opaque data.
     *
     *  @see ivtc_cadence_pos
     *  @see IVTCCadenceDetectAlgoScores()
     *  @see IVTCCadenceDetectAlgoVektor()
     */
    int  pi_s_cadence_pos[IVTC_DETECTION_HISTORY_SIZE];
    bool pb_s_reliable[IVTC_DETECTION_HISTORY_SIZE];
    int  pi_v_raw[IVTC_DETECTION_HISTORY_SIZE]; /**< "vektor" algo internal */
    int  pi_v_cadence_pos[IVTC_DETECTION_HISTORY_SIZE];
    bool pb_v_reliable[IVTC_DETECTION_HISTORY_SIZE];

    /** Final result, chosen by IVTCCadenceDetectFinalize() from the results
     *  given by the different detection algorithms.
     *
     *  @see IVTCCadenceDetectFinalize()
     */
    int pi_cadence_pos_history[IVTC_DETECTION_HISTORY_SIZE];

    /**
     *  Set by cadence analyzer. Whether the sequence of last
     *  IVTC_DETECTION_HISTORY_SIZE detected positions, stored in
     *  pi_cadence_pos_history, looks like a valid telecine.
     *
     *  @see IVTCCadenceAnalyze()
     */
    bool b_sequence_valid;

    /**
     *  Set by cadence analyzer. True if detected position = "dea".
     *  The three entries of this are used for detecting three progressive
     *  stencil positions in a row, i.e. five progressive frames in a row;
     *  this triggers exit from hard IVTC.
     *
     *  @see IVTCCadenceAnalyze()
     */
    bool pb_all_progressives[IVTC_DETECTION_HISTORY_SIZE];
} ivtc_sys_t;

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
    ivtc_sys_t ivtc;
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
    else if( !strcmp( psz_method, "ivtc" ) )
    {
        p_sys->i_mode = DEINTERLACE_IVTC;
        p_sys->b_double_rate = false;
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
        case DEINTERLACE_IVTC:
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
 * Inverse telecine (IVTC) filter (a.k.a. "film mode", "3:2 reverse pulldown")
 *****************************************************************************/

/**
 * @fn RenderIVTC
 * Deinterlace filter. Performs inverse telecine.
 *
 * Also known as "film mode" or "3:2 reverse pulldown" in some equipment.
 *
 * This filter attempts to reconstruct the original film frames from an
 * NTSC telecined signal. It is intended for 24fps progressive material
 * that was telecined to NTSC 60i. For example, most NTSC anime DVDs
 * are like this.
 *
 * @param p_filter The filter instance.
 * @param[in] p_src Input frame.
 * @param[out] p_dst Output frame. Must be allocated by caller.
 * @return VLC error code (int).
 * @retval VLC_SUCCESS A film frame was reconstructed to p_dst.
 * @retval VLC_EGENERIC Frame dropped as part of normal IVTC operation.
 * @see Deinterlace()
 * @see ComposeFrame()
 * @see CalculateInterlaceScore()
 * @see EstimateNumBlocksWithMotion()
 *
 *   Overall explanation:
 *
 *   This filter attempts to do in realtime what Transcode's
 *   ivtc->decimate->32detect chain does offline. Additionally, it removes
 *   soft telecine. It is an original design, based on some ideas from
 *   Transcode, some from TVTime, and some original.
 *
 *   If the input material is pure NTSC telecined film, inverse telecine
 *   (also known as "film mode") will (ideally) exactly recover the original
 *   (progressive film frames. The output will run at 4/5 of the original
 *   (framerate with no loss of information. Interlacing artifacts are removed,
 *   and motion becomes as smooth as it was on the original film.
 *   For soft-telecined material, on the other hand, the progressive frames
 *   alredy exist, so only the timings are changed such that the output
 *   becomes smooth 24fps (or would, if the output device had an infinite
 *   framerate).
 *
 *   Put in simple terms, this filter is targeted for NTSC movies and
 *   especially anime. Virtually all 1990s and early 2000s anime is
 *   hard-telecined. Because the source material is like that,
 *   IVTC is needed for also virtually all official R1 (US) anime DVDs.
 *
 *   Note that some anime from the turn of the century (e.g. Silent Mobius
 *   and Sol Bianca) is a hybrid of telecined film and true interlaced
 *   computer-generated effects and camera pans. In this case, applying IVTC
 *   will effectively attempt to reconstruct the frames based on the film
 *   component, but even if this is successful, the framerate reduction will
 *   cause the computer-generated effects to stutter. This is mathematically
 *   unavoidable. Instead of IVTC, a framerate doubling deinterlacer is
 *   recommended for such material. Try "Phosphor", "Bob", or "Linear".
 *
 *   Fortunately, 30fps true progressive anime is on the rise (e.g. ARIA,
 *   Black Lagoon, Galaxy Angel, Ghost in the Shell: Solid State Society,
 *   Mai Otome, Last Exile, and Rocket Girls). This type requires no
 *   deinterlacer at all.
 *
 *   Another recent trend is using 24fps computer-generated effects and
 *   telecining them along with the cels (e.g. Kiddy Grade, Str.A.In. and
 *   The Third: The Girl with the Blue Eye). For this group, IVTC is the
 *   correct way to deinterlace, and works properly.
 *
 *   Soft telecined anime, while rare, also exists. Stellvia of the Universe
 *   and Angel Links are examples of this. Stellvia constantly alternates
 *   between soft and hard telecine - pure CGI sequences are soft-telecined,
 *   while sequences incorporating cel animation are hard-telecined.
 *   This makes it very hard for the cadence detector to lock on,
 *   and indeed Stellvia gives some trouble for the filter.
 *
 *   To finish the list of different material types, Azumanga Daioh deserves
 *   a special mention. The OP and ED sequences are both 30fps progressive,
 *   while the episodes themselves are hard-telecined. This filter should
 *   mostly work correctly with such material, too. (The beginning of the OP
 *   shows some artifacts, but otherwise both the OP and ED are indeed
 *   rendered progressive. The technical reason is that the filter has been
 *   designed to aggressively reconstruct film frames, which helps in many
 *   cases with hard-telecined material. In very rare cases, this approach may
 *   go wrong, regardless of whether the input is telecined or progressive.)
 *
 *   Finally, note also that IVTC is the only correct way to deinterlace NTSC
 *   telecined material. Simply applying an interpolating deinterlacing filter
 *   (with no framerate doubling) is harmful for two reasons. First, even if
 *   (the filter does not damage already progressive frames, it will lose half
 *   (of the available vertical resolution of those frames that are judged
 *   interlaced. Some algorithms combining data from multiple frames may be
 *   able to counter this to an extent, effectively performing something akin
 *   to the frame reconstruction part of IVTC. A more serious problem is that
 *   any motion will stutter, because (even in the ideal case) one out of
 *   every four film frames will be shown twice, while the other three will
 *   be shown only once. Duplicate removal and framerate reduction - which are
 *   part of IVTC - are also needed to properly play back telecined material
 *   on progressive displays at a non-doubled framerate.
 *
 *   So, try this filter on your NTSC anime DVDs. It just might help.
 *
 *
 *   Technical details:
 *
 *
 *   First, NTSC hard telecine in a nutshell:
 *
 *   Film is commonly captured at 24 fps. The framerate must be raised from
 *   24 fps to 59.94 fields per second, This starts by pretending that the
 *   original framerate is 23.976 fps. When authoring, the audio can be
 *   slowed down by 0.1% to match. Now 59.94 = 5/4 * (2*23.976), which gives
 *   a nice ratio made out of small integers.
 *
 *   Thus, each group of four film frames must become five frames in the NTSC
 *   video stream. One cannot simply repeat one frame of every four, because
 *   this would result in jerky motion. To slightly soften the jerkiness,
 *   the extra frame is split into two extra fields, inserted at different
 *   times. The content of the extra fields is (in classical telecine)
 *   duplicated as-is from existing fields.
 *
 *   The field duplication technique is called "3:2 pulldown". The pattern
 *   is called the cadence. The output from 3:2 pulldown looks like this
 *   (if the telecine is TFF, top field first):
 *
 *   a  b  c  d  e     Telecined frame (actual frames stored on DVD)
 *   T1 T1 T2 T3 T4    *T*op field content
 *   B1 B2 B3 B3 B4    *B*ottom field content
 *
 *   Numbers 1-4 denote the original film frames. E.g. T1 = top field of
 *   original film frame 1. The field Tb, and one of either Bc or Bd, are
 *   the extra fields inserted in the telecine. With exact duplication, it
 *   of course doesn't matter whether Bc or Bd is the extra field, but
 *   with "full field blended" material (see below) this will affect how to
 *   correctly wxtract film frame 3.
 *
 *   See the following web pages for illustrations and discussion:
 *   http://neuron2.net/LVG/telecining1.html
 *   http://arbor.ee.ntu.edu.tw/~jackeikuo/dvd2avi/ivtc/
 *
 *   Note that film frame 2 has been stored "half and half" into two telecined
 *   frames (b and c). Note also that telecine produces a sequence of
 *   3 progressive frames (d, e and a) followed by 2 interlaced frames
 *   (b and c).
 *
 *   The output may also look like this (BFF telecine, bottom field first):
 *
 *   a' b' c' d' e'
 *   T1 T2 T3 T3 T4
 *   B1 B1 B2 B3 B4
 *
 *   Now field Bb', and one of either Tc' or Td', are the extra fields.
 *   Again, film frame 2 is stored "half and half" (into b' and c').
 *
 *   Whether the pattern is like abcde or a'b'c'd'e', depends on the telecine
 *   field dominance (TFF or BFF). This must match the video field dominance,
 *   but is conceptually different. Importantly, there is no temporal
 *   difference between those fields that came from the same film frame.
 *   Also, see the section on soft telecine below.
 *
 *   In a hard telecine, the TFD and VFD must match for field renderers
 *   (e.g. traditional DVD player + CRT TV) to work correctly; this should be
 *   fairly obvious by considering the above telecine patterns and how a
 *   field renderer displays the material (one field at a time, dominant
 *   field first).
 *
 *   Note that the VFD may, *correctly*, flip mid-stream, if soft field repeats
 *   (repeat_pict) have been used. They are commonly used in soft telecine
 *   (see below), but also occasional lone field repeats exist in some streams,
 *   e.g., Sol Bianca.
 *
 *   See e.g.
 *   http://www.cambridgeimaging.co.uk/downloads/Telecine%20field%20dominance.pdf
 *   for discussion. The document discusses mostly PAL, but includes some notes
 *   on NTSC, too.
 *
 *   The reason for the words "classical telecine" above, when field
 *   duplication was first mentioned, is that there exists a
 *   "full field blended" version, where the added fields are not exact
 *   "duplicates, but are blends of the original film frames. This is rare
 *   in NTSC, but some material like this reportedly exists. See
 *   http://www.animemusicvideos.org/guides/avtech/videogetb2a.html
 *   In these cases, the additional fields are a (probably 50%) blend of the
 *   frames between which they have been inserted. Which one of the two
 *   possibilites is the extra field then becomes important.
 *   This filter does NOT support "full field blended" material.
 *
 *   To summarize, the 3:2 pulldown sequence produces a group of ten fields
 *   out of every four film frames. Only eight of these fields are unique.
 *   To remove the telecine, the duplicate fields must be removed, and the
 *   original progressive frames restored. Additionally, the presentation
 *   timestamps (PTS) must be adjusted, and one frame out of five (containing
 *   no new information) dropped. The duration of each frame in the output
 *   becomes 5/4 of that in the input, i.e. 25% longer.
 *
 *   Theoretically, this whole mess could be avoided by soft telecining, if the
 *   original material is pure 24fps progressive. By using the stream flags
 *   correctly, the original progressive frames can be stored on the DVD.
 *   In such cases, the DVD player will apply "soft" 3:2 pulldown. See the
 *   following section.
 *
 *   Also, the mess with cadence detection for hard telecine (see below) could
 *   be avoided by using the progressive frame flag and a five-frame future
 *   buffer, but no one ever sets the flag correctly for hard-telecined
 *   streams. All frames are marked as interlaced, regardless of their cadence
 *   position. This is evil, but sort-of-understandable, given that video
 *   editors often come with "progressive" and "interlaced" editing modes,
 *   but no separate "telecined" mode that could correctly handle this
 *   information.
 *
 *   In practice, most material with its origins in Asia (including virtually
 *   all official US (R1) anime DVDs) is hard-telecined. Combined with the
 *   turn-of-the-century practice of rendering true interlaced effects
 *   on top of the hard-telecined stream, we have what can only be described
 *   as a monstrosity. Fortunately, recent material is much more consistent,
 *   even though still almost always hard-telecined.
 *
 *   Finally, note that telecined video is often edited directly in interlaced
 *   form, disregarding safe cut positions as pertains to the telecine sequence
 *   (there are only two: between "d" and "e", or between "e" and the
 *   (next "a"). Thus, the telecine sequence will in practice jump erratically
 *   at cuts [**]. An aggressive detection strategy is needed to cope with
 *   this.
 *
 *   [**] http://users.softlab.ece.ntua.gr/~ttsiod/ivtc.html
 *
 *
 *   Note about chroma formats: 4:2:0 is very common at least on anime DVDs.
 *   In the interlaced frames in a hard telecine, the chroma alternates
 *   every chroma line, even if the chroma format is 4:2:0! This means that
 *   if the interlaced picture is viewed as-is, the luma alternates every line,
 *   while the chroma alternates only every two lines of the picture.
 *
 *   That is, an interlaced frame from a 4:2:0 telecine looks like this
 *   (numbers indicate which frame the data comes from):
 *
 *   luma  stored 4:2:0 chroma  displayed chroma
 *   1111  1111                 1111
 *   2222                       1111
 *   1111  2222                 2222
 *   2222                       2222
 *   ...   ...                  ...
 *
 *   The deinterlace filter sees the stored 4:2:0 chroma.
 *   The "displayed chroma" is only generated later in the filter chain
 *   (probably when YUV is converted to the display format, if the display
 *   does not accept YUV 4:2:0 directly).
 *
 *
 *   Next, how NTSC soft telecine works:
 *
 *   a  b  c  d     Frame index (actual frames stored on DVD)
 *   T1 T2 T3 T4    *T*op field content
 *   B1 B2 B3 B4    *B*ottom field content
 *
 *   Here the progressive frames are stored as-is. The catch is in the stream
 *   flags. For hard telecine, which was explained above, we have
 *   VFD = constant and nb_fields = 2, just like in a true progressive or
 *   true interlaced stream. Soft telecine, on the other hand, looks like this:
 *
 *   a  b  c  d
 *   3  2  3  2     nb_fields
 *   T  B  B  T     *Video* field dominance (for TFF telecine)
 *   B  T  T  B     *Video* field dominance (for BFF telecine)
 *
 *   Now the video field dominance flipflops every two frames!
 *
 *   Note that nb_fields = 3 means the frame duration will be 1.5x that of a
 *   normal frame. Often, soft-telecined frames are correctly flagged as
 *   progressive.
 *
 *   Here the telecining is expected to be done by the player, utilizing the
 *   soft field repeat (repeat_pict) feature. This is indeed what a field
 *   renderer (traditional interlaced equipment, or a framerate doubler)
 *   should do with such a stream.
 *
 *   In the IVTC filter, our job is to even out the frame durations, but
 *   disregard video field dominance and just pass the progressive pictures
 *   through as-is.
 *
 *   Fortunately, for soft telecine to work at all, the stream flags must be
 *   set correctly. Thus this type can be detected reliably by reading
 *   nb_fields from three consecutive frames:
 *
 *   Let P = previous, C = current, N = next. If the frame to be rendered is C,
 *   there are only three relevant nb_fields flag patterns for the three-frame
 *   stencil concerning soft telecine:
 *
 *   P C N   What is happening:
 *   2 3 2   Entering soft telecine at frame C, or running inside it already.
 *   3 2 3   Running inside soft telecine.
 *   3 2 2   Exiting soft telecine at frame C. C is the last frame that should
 *           be handled as soft-telecined. (If we do timing adjustments to the
 *           "3"s only, we can already exit soft telecine mode  when we see
 *           this pattern.)
 *
 *   Note that the same stream may alternate between soft and hard telecine,
 *   but these cannot occur at the same time. The start and end of the
 *   soft-telecined parts can be read off the stream flags, and the rest of
 *   the stream can be handed to the hard IVTC part of the filter for analysis.
 *
 *   Finally, note also that a stream may also request a lone field repeat
 *   (a sudden "3" surrounded by "2"s). Fortunately, these can be handled as
 *   (a two-frame soft telecine, as they match the first and third
 *   flag patterns above.
 *
 *   Combinations with several "3"s in a row are not valid for soft or hard
 *   telecine, so if they occur, the frames can be passed through as-is.
 *
 *
 *   Cadence detection for hard telecine:
 *
 *   Consider viewing the TFF and BFF hard telecine sequences through a
 *   three-frame stencil. Again, let P = previous, C = current, N = next.
 *   A brief analysis leads to the following cadence tables.
 *
 *   PCN                 = stencil position (Previous Current Next),
 *   Dups.               = duplicate fields,
 *   Best field pairs... = combinations of fields which correctly reproduce
 *                         the original progressive frames,
 *   *                   = see timestamp considerations below for why
 *                         this particular arrangement.
 *
 *   For TFF:
 *
 *   PCN   Dups.     Best field pairs for progressive (correct, theoretical)
 *   abc   TP = TC   TPBP = frame 1, TCBP = frame 1, TNBC = frame 2
 *   bcd   BC = BN   TCBP = frame 2, TNBC = frame 3, TNBN = frame 3
 *   cde   BP = BC   TCBP = frame 3, TCBC = frame 3, TNBN = frame 4
 *   dea   none      TPBP = frame 3, TCBC = frame 4, TNBN = frame 1
 *   eab   TC = TN   TPBP = frame 4, TCBC = frame 1, TNBC = frame 1
 *
 *   (table cont'd)
 *   PCN   Progressive output*
 *   abc   frame 2 = TNBC (compose TN+BC)
 *   bcd   frame 3 = TNBN (copy N)
 *   cde   frame 4 = TNBN (copy N)
 *   dea   (drop)
 *   eab   frame 1 = TCBC (copy C), or TNBC (compose TN+BC)
 *
 *   On the rows "dea" and "eab", frame 1 refers to a frame from the next
 *   group of 4. "Compose TN+BC" means to construct a frame using the
 *   top field of N, and the bottom field of C. See ComposeFrame().
 *
 *   For BFF, swap all B and T, and rearrange the symbol pairs to again
 *   read "TxBx". We have:
 *
 *   PCN   Dups.     Best field pairs for progressive (correct, theoretical)
 *   abc   BP = BC   TPBP = frame 1, TPBC = frame 1, TCBN = frame 2
 *   bcd   TC = TN   TPBC = frame 2, TCBN = frame 3, TNBN = frame 3
 *   cde   TP = TC   TPBC = frame 3, TCBC = frame 3, TNBN = frame 4
 *   dea   none      TPBP = frame 3, TCBC = frame 4, TNBN = frame 1
 *   eab   BC = BN   TPBP = frame 4, TCBC = frame 1, TCBN = frame 1
 *
 *   (table cont'd)
 *   PCN   Progressive output*
 *   abc   frame 2 = TCBN (compose TC+BN)
 *   bcd   frame 3 = TNBN (copy N)
 *   cde   frame 4 = TNBN (copy N)
 *   dea   (drop)
 *   eab   frame 1 = TCBC (copy C), or TCBN (compose TC+BN)
 *
 *   From these cadence tables we can extract two strategies for
 *   cadence detection. We use both.
 *
 *   Strategy 1: duplicated fields.
 *
 *   Consider that each stencil position has a unique duplicate field
 *   condition. In one unique position, "dea", there is no match; in all
 *   other positions, exactly one. By conservatively filtering the
 *   possibilities based on detected hard field repeats (identical fields
 *   in successive input frames), it is possible to gradually lock on
 *   to the cadence. This kind of strategy is used by Vektor's classic
 *   IVTC filter from TVTime (although there are some implementation
 *   differences when compared to ours).
 *
 *   "Conservative" here means that we do not rule anything out, but start at
 *   each stencil position by suggesting the position "dea", and then only add
 *   to the list of possibilities based on field repeats that are detected at
 *   the present stencil position. This estimate is then filtered by ANDing
 *   against a shifted (time-advanced) version of the estimate from the
 *   previous stencil position. Once the detected position becomes unique,
 *   the filter locks on. If the new detection is inconsistent with the
 *   previous one, the detector resets itself and starts from scratch.
 *
 *   The strategy is very reliable, as it only requires running (fuzzy)
 *   duplicate field detection against the input. It is very good at staying
 *   locked on once it acquires the cadence, and it does so correctly very
 *   often. These are indeed characteristics that can be observed in the
 *   behaviour of Vektor's classic filter.
 *
 *   Note especially that 8fps/12fps animation, common in anime, will cause
 *   spurious hard-repeated fields. The conservative nature of the method
 *   makes it very good at dealing with this - any spurious repeats will only
 *   slow down the lock-on, not completely confuse it. It should also be good
 *   at detecting the presence of a telecine, as neither true interlaced nor
 *   true progressive material should contain any hard field repeats.
 *   (This, however, has not been tested yet.)
 *
 *   The disadvantages are that at times the method may lock on slowly,
 *   because the detection must be filtered against the history until
 *   a unique solution is found. Resets, if they happen, will also
 *   slow down the lock-on.
 *
 *   The hard duplicate detection required by this strategy can be made
 *   data-adaptive in several ways. TVTime uses a running average of motion
 *   scores for its history buffer. We utilize a different, original approach.
 *   It is rare, if not nonexistent, that only one field changes between
 *   two valid frames. Thus, if one field changes "much more" than the other
 *   in fieldwise motion detection, the less changed one is probably a
 *   duplicate. Importantly, this works with telecined input, too - the field
 *   that changes "much" may be part of another film frame, while the "less"
 *   changed one is actually a duplicate from the previous film frame.
 *   If both fields change "about as much", then no hard field repeat
 *   is detected.
 *
 *
 *   Strategy 2: progressive/interlaced field combinations.
 *
 *   We can also form a second strategy, which is not as reliable in practice,
 *   but which locks on faster. This is original to this filter.
 *
 *   Consider all possible field pairs from two successive frames: TCBC, TCBN,
 *   TNBC, TNBN. After one frame, these become TPBP, TPBC, TCBP, TCBC.
 *   These eight pairs (seven unique, disregarding the duplicate TCBC)
 *   are the exhaustive list of possible field pairs from two successive
 *   frames in the three-frame PCN stencil.
 *
 *   The field pairs can be used for cadence position detection. The above
 *   tables list triplets of field pair combinations for each cadence position,
 *   which should produce progressive frames. All the given triplets are unique
 *   in each table alone, although the one at "dea" is indistinguishable from
 *   the case of pure progressive material. It is also the only one which is
 *   not unique across both tables.
 *
 *   Thus, all sequences of two neighboring triplets are unique across both
 *   tables. (For "neighboring", each table is considered to wrap around from
 *   "eab" back to "abc", i.e. from the last row back to the first row.)
 *   Furthermore, each sequence of three neighboring triplets is redundantly
 *   unique (i.e. is unique, and reduces the chance of false positives).
 *
 *   The important idea is: *all other* field pair combinations should produce
 *   frames that look interlaced. This includes those combinations present in
 *   the "wrong" (i.e. not current position) rows of the table (insofar as
 *   those combinations are not also present in the "correct" row; by the
 *   uniqueness property, *every* "wrong" row will always contain at least one
 *   combination that differs from those in the "correct" row).
 *
 *   As for how we use these observations, we generate the artificial frames
 *   TCBC, TCBN, TNBC and TNBN (virtually; no data is actually moved).
 *   Two of these are just the frames C and N, which already exist; the two
 *   others correspond to composing the given field pairs. We then compute
 *   the interlace score for each of these frames. The interlace scores
 *   of what are now TPBP, TPBC and TCBP, also needed, were computed by
 *   this same mechanism during the previous input frame. These can be slided
 *   in history and reused.
 *
 *   We then check, using the computed interlace scores, and taking into
 *   account the video field dominance information (to only check valid
 *   combinations), which field combination triplet given in the tables
 *   produces the smallest sum of interlace scores. Unless we are at
 *   PCN = "dea" (which could also be pure progressive!), this immediately
 *   gives us the most likely current cadence position. Combined with a
 *   two-step history, the sequence of three most likely positions found this
 *   way always allows us to make a more or less reliable detection. (That is,
 *   when a reliable detection is possible; note that if the video has no
 *   motion at all, every detection will report the position "dea". In anime,
 *   still shots are common. Thus we must augment this with a full-frame motion
 *   detection that switches the detector off if no motion was detected.)
 *
 *   The detection seems to need four full-frame interlace analyses per frame.
 *   Actually, three are enough, because the previous N is the new C, so we can
 *   slide the already computed result. Also during initialization, we only
 *   need to compute TNBN on the first frame; this has become TPBP when the
 *   third frame is reached. Similarly, we compute TNBN, TNBC and TCBN during
 *   the second frame (just before the filter starts), and these get slided
 *   into TCBC, TCBP and TPBC when the third frame is reached. At that point,
 *   initialization is complete.
 *
 *   Because we only compare interlace scores against each other, no threshold
 *   is needed in the cadence detector. Thus it, trivially, adapts to the
 *   material automatically.
 *
 *   The weakness of this approach is that any comb metric detects incorrectly
 *   every now and then. Especially slow vertical camera pans often get treated
 *   wrong, because the messed-up field combination looks less interlaced
 *   according to the comb metric (especially in anime) than the correct one
 *   (which contains, correctly, one-pixel thick cartoon outlines, parts of
 *   which often perfectly horizontal).
 *
 *   The advantage is that this strategy catches horizontal camera pans
 *   immediately and reliably, while the other strategy may still be trying
 *   to lock on.
 *
 *
 *   Frame reconstruction:
 *
 *   We utilize a hybrid approach. If a valid cadence is locked on, we use the
 *   operation table to decide what to do. This handles those cases correctly,
 *   which would be difficult for the interlace detector alone (e.g. vertical
 *   camera pans). Note that the operations that must be performed for IVTC
 *   include timestamp mangling and frame dropping, which can only be done
 *   reliably on a valid cadence.
 *
 *   When the cadence fails (we detect this from a sudden upward jump in the
 *   interlace scores of the constructed frames), we reset the "TVTime"
 *   detector strategy and fall back to an emergency frame composer, where we
 *   use ideas from Transcode's IVTC.
 *
 *   In the emergency mode, we simply output the least interlaced frame out of
 *   the combinations TNBN, TNBC and TCBN (where only one of the last two is
 *   tested, based on the stream TFF/BFF information). In this mode, we do not
 *   touch the timestamps, and just pass all five frames from each group right
 *   through. This introduces some stutter, but in practice it is often not
 *   noticeable. This is because the kind of material that is likely to trip up
 *   the cadence detector usually includes irregular 8fps/12fps motion. With
 *   true 24fps motion, the cadence quickly locks on, and stays locked on.
 *
 *   Once the cadence locks on again, we resume normal operation based on
 *   the operation table.
 *
 *
 *   Timestamp mangling:
 *
 *   To make five into four we need to extend frame durations by 25%.
 *   Consider the following diagram (times given in 90kHz ticks, rounded to
 *   integers; this is just for illustration):
 *
 *   NTSC input (29.97 fps)
 *   a       b       c       d        e        a (from next group) ...
 *   0    3003    6006    9009    12012    15015
 *   0      3754      7508       11261     15015
 *   1         2         3           4         1 (from next group) ...
 *   Film output (23.976 fps)
 *
 *   Three of the film frames have length 3754, and one has 3753
 *   (it is 1/90000 sec shorter). This rounding was chosen so that the lengths
 *   (of the group of four sum to the original 15015.
 *
 *   From the diagram we get these deltas for presentation timestamp adjustment
 *   (in 90 kHz ticks, for illustration):
 *   (1-a)   (2-b)  (3-c)   (4-d)   (skip)   (1-a) ...
 *       0   +751   +1502   +2252   (skip)       0 ...
 *
 *   In fractions of (p_next->date - p_cur->date), regardless of actual
 *   time unit, the deltas are:
 *   (1-a)   (2-b)  (3-c)   (4-d)   (skip)   (1-a) ...
 *       0   +0.25  +0.50   +0.75   (skip)       0 ...
 *
 *   This is what we actually use. In our implementation, the values are stored
 *   multiplied by 4, as integers.
 *
 *   The "current" frame should be displayed at [original time + delta].
 *   E.g., when "current" = b (i.e. PCN = abc), start displaying film frame 2
 *   at time [original time of b + 751 ticks]. So, when we catch the cadence,
 *   we will start mangling the timestamps according to the cadence position
 *   of the "current" frame, using the deltas given above. This will cause
 *   a one-time jerk, most noticeable if the cadence happens to catch at
 *   position "d". (Alternatively, upon lock-on, we could wait until we are
 *   at "a" before switching on IVTC, but this makes the maximal delay
 *   [max. detection + max. wait] = 3 + 4 = 7 input frames, which comes to
 *   [7/30 ~ 0.23 seconds instead of the 3/30 = 0.10 seconds from purely
 *   the detection. I prefer the one-time jerk, which also happens to be
 *   simpler to implement.)
 *
 *   It is clear that "e" is a safe choice for the dropped frame. This can be
 *   seen from the timings and the cadence tables. First, consider the timings.
 *   If we have only one future frame, "e" is the only one whose PTS, comparing
 *   to the film frames, allows dropping it safely. To see this, consider which
 *   film frame needs to be rendered as each new input frame arrives. Secondly,
 *   consider the cadence tables. It is ok to drop "e", because the same
 *   film frame "1" is available also at the next PCN position "eab".
 *   (As a side note, it is interesting that Vektor's filter drops "b".
 *   See the TVTime sources.)
 *
 *   When the filter falls out of film mode, the timestamps of the incoming
 *   frames are left untouched. Thus, the output from this filter has a
 *   variable framerate: 4/5 of the input framerate when IVTC is active
 *   (whether hard or soft), and the same framerate as input when it is not
 *   (or when in emergency mode).
 *
 *
 *   For other open-source IVTC codes, which may be a useful source for ideas,
 *   see the following:
 *
 *   The classic filter by Billy Biggs (Vektor). Written in 2001-2003 for
 *   TVTime, and adapted into Xine later. In xine-lib 1.1.19, it is at
 *   src/post/deinterlace/pulldown.*. Also needed are tvtime.*, and speedy.*.
 *
 *   Transcode's ivtc->decimate->32detect chain by Thanassis Tsiodras.
 *   Written in 2002, added in Transcode 0.6.12. This probably has something
 *   to do with the same chain in MPlayer, considering that MPlayer acquired
 *   an IVTC filter around the same time. In Transcode 1.1.5, the IVTC part is
 *   at filter/filter_ivtc.c. Transcode 1.1.5 sources can be downloaded from
 *   http://developer.berlios.de/project/showfiles.php?group_id=10094
 */

/**
 * Helper function: estimates "how much interlaced" the given field pair is.
 *
 * It is allowed that p_pic_top == p_pic_bottom.
 *
 * If p_pic_top != p_pic_bot (fields come from different pictures), you can use
 * ComposeFrame() to actually construct the picture if needed.
 *
 * Number of planes, and number of lines in each plane, in p_pic_top and
 * p_pic_bot must match. If the visible pitches differ, only the compatible
 * (smaller) part will be tested.
 *
 * Luma and chroma planes are tested in the same way. This is correct for
 * telecined input, where in the interlaced frames also chroma alternates
 * every chroma line, even if the chroma format is 4:2:0!
 *
 * This is just a raw detector that produces a score. The overall score
 * indicating a progressive or indicated frame may vary wildly, depending on
 * the material, especially in anime. The scores should be compared to
 * each other locally (in the temporal sense) to make meaningful decisions
 * about progressive or interlaced frames.
 *
 * @param p_pic_top Picture to take the top field from.
 * @param p_pic_bot Picture to take the bottom field from.
 * @return Interlace score, >= 0. Higher values mean more interlaced.
 * @retval -1 Error: incompatible input pictures.
 * @see RenderIVTC()
 * @see ComposeFrame()
 */
static int CalculateInterlaceScore( const picture_t* p_pic_top,
                                    const picture_t* p_pic_bot )
{
    /*
        We use the comb metric from the IVTC filter of Transcode 1.1.5.
        This was found to work better for the particular purpose of IVTC
        than RenderX()'s comb metric.

        Note that we *must not* subsample at all in order to catch interlacing
        in telecined frames with localized motion (e.g. anime with characters
        talking, where only mouths move and everything else stays still.)
    */

    assert( p_pic_top != NULL );
    assert( p_pic_bot != NULL );

    if( p_pic_top->i_planes != p_pic_bot->i_planes )
        return -1;

    unsigned u_cpu = vlc_CPU();

    /* Amount of bits must be known for MMX, thus int32_t.
       Doesn't hurt the C implementation. */
    int32_t i_score = 0;

#ifdef CAN_COMPILE_MMXEXT
    if( u_cpu & CPU_CAPABILITY_MMXEXT )
        pxor_r2r( mm7, mm7 ); /* we will keep score in mm7 */
#endif

    for( int i_plane = 0 ; i_plane < p_pic_top->i_planes ; ++i_plane )
    {
        /* Sanity check */
        if( p_pic_top->p[i_plane].i_visible_lines !=
            p_pic_bot->p[i_plane].i_visible_lines )
            return -1;

        const int i_lasty = p_pic_top->p[i_plane].i_visible_lines-1;
        const int w = FFMIN( p_pic_top->p[i_plane].i_visible_pitch,
                             p_pic_bot->p[i_plane].i_visible_pitch );
        const int wm8 = w % 8;   /* remainder */
        const int w8  = w - wm8; /* part of width that is divisible by 8 */

        /* Current line / neighbouring lines picture pointers */
        const picture_t *cur = p_pic_bot;
        const picture_t *ngh = p_pic_top;
        int wc = cur->p[i_plane].i_pitch;
        int wn = ngh->p[i_plane].i_pitch;

        /* Transcode 1.1.5 only checks every other line. Checking every line
           works better for anime, which may contain horizontal,
           one pixel thick cartoon outlines.
        */
        for( int y = 1; y < i_lasty; ++y )
        {
            uint8_t *p_c = &cur->p[i_plane].p_pixels[y*wc];     /* this line */
            uint8_t *p_p = &ngh->p[i_plane].p_pixels[(y-1)*wn]; /* prev line */
            uint8_t *p_n = &ngh->p[i_plane].p_pixels[(y+1)*wn]; /* next line */

/* Threshold (value from Transcode 1.1.5) */
#define T 100
#ifdef CAN_COMPILE_MMXEXT
            /* Easy-to-read C version further below.

               Assumptions: 0 < T < 127
                            # of pixels < (2^32)/255
               Note: calculates score * 255
            */
            if( u_cpu & CPU_CAPABILITY_MMXEXT )
            {
                static const mmx_t b0   = { .uq = 0x0000000000000000ULL };
                static const mmx_t b128 = { .uq = 0x8080808080808080ULL };
                static const mmx_t bT   = { .ub = { T, T, T, T, T, T, T, T } };

                for( int x = 0; x < w8; x += 8 )
                {
                    movq_m2r( *((int64_t*)p_c), mm0 );
                    movq_m2r( *((int64_t*)p_p), mm1 );
                    movq_m2r( *((int64_t*)p_n), mm2 );

                    psubb_m2r( b128, mm0 );
                    psubb_m2r( b128, mm1 );
                    psubb_m2r( b128, mm2 );

                    psubsb_r2r( mm0, mm1 );
                    psubsb_r2r( mm0, mm2 );

                    pxor_r2r( mm3, mm3 );
                    pxor_r2r( mm4, mm4 );
                    pxor_r2r( mm5, mm5 );
                    pxor_r2r( mm6, mm6 );

                    punpcklbw_r2r( mm1, mm3 );
                    punpcklbw_r2r( mm2, mm4 );
                    punpckhbw_r2r( mm1, mm5 );
                    punpckhbw_r2r( mm2, mm6 );

                    pmulhw_r2r( mm3, mm4 );
                    pmulhw_r2r( mm5, mm6 );

                    packsswb_r2r(mm4, mm6);
                    pcmpgtb_m2r( bT, mm6 );
                    psadbw_m2r( b0, mm6 );
                    paddd_r2r( mm6, mm7 );

                    p_c += 8;
                    p_p += 8;
                    p_n += 8;
                }
                /* Handle the width remainder if any. */
                if( wm8 )
                {
                    for( int x = 0; x < wm8; ++x )
                    {
                        int_fast32_t C = *p_c;
                        int_fast32_t P = *p_p;
                        int_fast32_t N = *p_n;

                        int_fast32_t comb = (P - C) * (N - C);
                        if( comb > T )
                            ++i_score;

                        ++p_c;
                        ++p_p;
                        ++p_n;
                    }
                }
            }
            else
            {
#endif
                for( int x = 0; x < w; ++x )
                {
                    /* Worst case: need 17 bits for "comb". */
                    int_fast32_t C = *p_c;
                    int_fast32_t P = *p_p;
                    int_fast32_t N = *p_n;

                    /* Comments in Transcode's filter_ivtc.c attribute this
                       combing metric to Gunnar Thalin.

                        The idea is that if the picture is interlaced, both
                        expressions will have the same sign, and this comes
                        up positive. The value T = 100 has been chosen such
                        that a pixel difference of 10 (on average) will
                        trigger the detector.
                    */
                    int_fast32_t comb = (P - C) * (N - C);
                    if( comb > T )
                        ++i_score;

                    ++p_c;
                    ++p_p;
                    ++p_n;
                }
#ifdef CAN_COMPILE_MMXEXT
            }
#endif

            /* Now the other field - swap current and neighbour pictures */
            const picture_t *tmp = cur;
            cur = ngh;
            ngh = tmp;
            int tmp_pitch = wc;
            wc = wn;
            wn = tmp_pitch;
        }
    }

#ifdef CAN_COMPILE_MMXEXT
    if( u_cpu & CPU_CAPABILITY_MMXEXT )
    {
        movd_r2m( mm7, i_score );
        emms();
        i_score /= 255;
    }
#endif

    return i_score;
}
#undef T

/**
 * Internal helper function for EstimateNumBlocksWithMotion():
 * estimates whether there is motion in the given 8x8 block on one plane
 * between two images. The block as a whole and its fields are evaluated
 * separately, and use different motion thresholds.
 *
 * This is a low-level function only used by EstimateNumBlocksWithMotion().
 * There is no need to call this function manually.
 *
 * For interpretation of pi_top and pi_bot, it is assumed that the block
 * starts on an even-numbered line (belonging to the top field).
 *
 * The b_mmx parameter avoids the need to call vlc_CPU() separately
 * for each block.
 *
 * @param[in] p_pix_p Base pointer to the block in previous picture
 * @param[in] p_pix_c Base pointer to the same block in current picture
 * @param i_pitch_prev i_pitch of previous picture
 * @param i_pitch_curr i_pitch of current picture
 * @param b_mmx (vlc_CPU() & CPU_CAPABILITY_MMXEXT) or false.
 * @param[out] pi_top 1 if top field of the block had motion, 0 if no
 * @param[out] pi_bot 1 if bottom field of the block had motion, 0 if no
 * @return 1 if the block had motion, 0 if no
 * @see EstimateNumBlocksWithMotion()
 */
static inline int TestForMotionInBlock( uint8_t *p_pix_p, uint8_t *p_pix_c,
                                        int i_pitch_prev, int i_pitch_curr,
                                        bool b_mmx,
                                        int* pi_top, int* pi_bot )
{
/* Pixel luma/chroma difference threshold to detect motion. */
#define T 10

    int32_t i_motion = 0;
    int32_t i_top_motion = 0;
    int32_t i_bot_motion = 0;

/* See below for the C version to see more quickly what this does. */
#ifdef CAN_COMPILE_MMXEXT
    if( b_mmx )
    {
        static const mmx_t bT   = { .ub = { T, T, T, T, T, T, T, T } };
        pxor_r2r( mm6, mm6 ); /* zero, used in psadbw */
        movq_m2r( bT,  mm5 );

        pxor_r2r( mm3, mm3 ); /* score (top field) */
        pxor_r2r( mm4, mm4 ); /* score (bottom field) */
        for( int y = 0; y < 8; y+=2 )
        {
            /* top field */
            movq_m2r( *((uint64_t*)p_pix_c), mm0 );
            movq_m2r( *((uint64_t*)p_pix_p), mm1 );
            movq_r2r( mm0, mm2 );
            psubusb_r2r( mm1, mm2 );
            psubusb_r2r( mm0, mm1 );

            pcmpgtb_r2r( mm5, mm2 );
            pcmpgtb_r2r( mm5, mm1 );
            psadbw_r2r(  mm6, mm2 );
            psadbw_r2r(  mm6, mm1 );

            paddd_r2r( mm2, mm1 );
            paddd_r2r( mm1, mm3 ); /* add to top field score */

            p_pix_c += i_pitch_curr;
            p_pix_p += i_pitch_prev;

            /* bottom field - handling identical to top field, except... */
            movq_m2r( *((uint64_t*)p_pix_c), mm0 );
            movq_m2r( *((uint64_t*)p_pix_p), mm1 );
            movq_r2r( mm0, mm2 );
            psubusb_r2r( mm1, mm2 );
            psubusb_r2r( mm0, mm1 );

            pcmpgtb_r2r( mm5, mm2 );
            pcmpgtb_r2r( mm5, mm1 );
            psadbw_r2r(  mm6, mm2 );
            psadbw_r2r(  mm6, mm1 );

            paddd_r2r( mm2, mm1 );
            paddd_r2r( mm1, mm4 ); /* ...here we add to bottom field score */

            p_pix_c += i_pitch_curr;
            p_pix_p += i_pitch_prev;
        }
        movq_r2r(  mm3, mm7 ); /* score (total) */
        paddd_r2r( mm4, mm7 );
        movd_r2m( mm3, i_top_motion );
        movd_r2m( mm4, i_bot_motion );
        movd_r2m( mm7, i_motion );

        /* The loop counts actual score * 255. */
        i_top_motion /= 255;
        i_bot_motion /= 255;
        i_motion     /= 255;

        emms();
    }
    else
#endif
    {
        for( int y = 0; y < 8; ++y )
        {
            uint8_t *pc = p_pix_c;
            uint8_t *pp = p_pix_p;
            int score = 0;
            for( int x = 0; x < 8; ++x )
            {
                int_fast16_t C = abs((*pc) - (*pp));
                if( C > T )
                    ++score;

                ++pc;
                ++pp;
            }

            i_motion += score;
            if( y % 2 == 0 )
                i_top_motion += score;
            else
                i_bot_motion += score;

            p_pix_c += i_pitch_curr;
            p_pix_p += i_pitch_prev;
        }
    }

    /* Field motion thresholds.

       Empirical value - works better in practice than the "4" that
       would be consistent with the full-block threshold.

       Especially the opening scene of The Third ep. 1 (just after the OP)
       works better with this. It also fixes some talking scenes in
       Stellvia ep. 1, where the cadence would otherwise catch on incorrectly,
       leading to more interlacing artifacts than by just using the emergency
       mode frame composer.
    */
    (*pi_top) = ( i_top_motion >= 8 );
    (*pi_bot) = ( i_bot_motion >= 8 );

    /* Full-block threshold = (8*8)/8: motion is detected if 1/8 of the block
       changes "enough". */
    return (i_motion >= 8);
}
#undef T

/**
 * Helper function: Estimates the number of 8x8 blocks which have motion
 * between the given pictures. Needed for various detectors in RenderIVTC().
 *
 * Number of planes and visible lines in each plane, in the inputs must match.
 * If the visible pitches do not match, only the compatible (smaller)
 * part will be tested.
 *
 * Note that the return value is NOT simply *pi_top + *pi_bot, because
 * the fields and the full block use different motion thresholds.
 *
 * It is allowed to set pi_top and pi_bot to NULL, if the caller does not want
 * the separate field scores. This does not affect computation speed, and is
 * only provided as a syntactic convenience.
 *
 * Motion in each picture plane (Y, U, V) counts separately.
 * The sum of number of blocks with motion across all planes is returned.
 *
 * @param[in] p_prev Previous picture
 * @param[in] p_curr Current picture
 * @param[out] pi_top Number of 8x8 blocks where top field has motion.
 * @param[out] pi_bot Number of 8x8 blocks where bottom field has motion.
 * @return Number of 8x8 blocks that have motion.
 * @retval -1 Error: incompatible input pictures.
 * @see TestForMotionInBlock()
 * @see RenderIVTC()
 */
static int EstimateNumBlocksWithMotion( const picture_t* p_prev,
                                        const picture_t* p_curr,
                                        int *pi_top, int *pi_bot)
{
    assert( p_prev != NULL );
    assert( p_curr != NULL );

    int i_score_top = 0;
    int i_score_bot = 0;

    if( p_prev->i_planes != p_curr->i_planes )
        return -1;

    /* We must tell our inline helper whether to use MMX acceleration. */
#ifdef CAN_COMPILE_MMXEXT
    bool b_mmx = ( vlc_CPU() & CPU_CAPABILITY_MMXEXT );
#else
    bool b_mmx = false;
#endif

    int i_score = 0;
    for( int i_plane = 0 ; i_plane < p_prev->i_planes ; i_plane++ )
    {
        /* Sanity check */
        if( p_prev->p[i_plane].i_visible_lines !=
            p_curr->p[i_plane].i_visible_lines )
            return -1;

        const int i_pitch_prev = p_prev->p[i_plane].i_pitch;
        const int i_pitch_curr = p_curr->p[i_plane].i_pitch;

        /* Last pixels and lines (which do not make whole blocks) are ignored.
           Shouldn't really matter for our purposes. */
        const int i_mby = p_prev->p[i_plane].i_visible_lines / 8;
        const int w = FFMIN( p_prev->p[i_plane].i_visible_pitch,
                             p_curr->p[i_plane].i_visible_pitch );
        const int i_mbx = w / 8;

        for( int by = 0; by < i_mby; ++by )
        {
            uint8_t *p_pix_p = &p_prev->p[i_plane].p_pixels[i_pitch_prev*8*by];
            uint8_t *p_pix_c = &p_curr->p[i_plane].p_pixels[i_pitch_curr*8*by];

            for( int bx = 0; bx < i_mbx; ++bx )
            {
                int i_top_temp, i_bot_temp;
                i_score += TestForMotionInBlock( p_pix_p, p_pix_c,
                                                 i_pitch_prev, i_pitch_curr,
                                                 b_mmx,
                                                 &i_top_temp, &i_bot_temp );
                i_score_top += i_top_temp;
                i_score_bot += i_bot_temp;

                p_pix_p += 8;
                p_pix_c += 8;
            }
        }
    }

    if( pi_top )
        (*pi_top) = i_score_top;
    if( pi_bot )
        (*pi_bot) = i_score_bot;

    return i_score;
}

/* Fasten your seatbelt - lots of IVTC constants follow... */

/**
 * IVTC filter modes.
 *
 * Hard telecine: burned into video stream.
 * Soft telecine: stream consists of progressive frames;
 *                telecining handled by stream flags.
 *
 * @see ivtc_sys_t
 * @see RenderIVTC()
 */
typedef enum { IVTC_MODE_DETECTING           = 0,
               IVTC_MODE_TELECINED_NTSC_HARD = 1,
               IVTC_MODE_TELECINED_NTSC_SOFT = 2 } ivtc_mode;

/**
 *  Field pair combinations from successive frames in the PCN stencil.
 *  T = top, B = bottom, P = previous, C = current, N = next
 *  These are used as array indices; hence the explicit numbering.
 */
typedef enum { FIELD_PAIR_TPBP = 0, FIELD_PAIR_TPBC = 1,
               FIELD_PAIR_TCBP = 2, FIELD_PAIR_TCBC = 3,
               FIELD_PAIR_TCBN = 4, FIELD_PAIR_TNBC = 5,
               FIELD_PAIR_TNBN = 6 } ivtc_field_pair;

/* Note: only valid ones count for NUM */
#define NUM_CADENCE_POS 9
/**
 * Cadence positions for the PCN stencil (PCN, Previous Current Next).
 *
 * Note that "dea" in both cadence tables and a pure progressive signal
 * are indistinguishable.
 *
 * Used as array indices except the -1.
 *
 * This is a combined raw position containing both i_cadence_pos
 * and telecine field dominance.
 * @see pi_detected_pos_to_cadence_pos
 * @see pi_detected_pos_to_tfd
 */
typedef enum { CADENCE_POS_INVALID     = -1,
               CADENCE_POS_PROGRESSIVE =  0,
               CADENCE_POS_TFF_ABC     =  1,
               CADENCE_POS_TFF_BCD     =  2,
               CADENCE_POS_TFF_CDE     =  3,
               CADENCE_POS_TFF_EAB     =  4,
               CADENCE_POS_BFF_ABC     =  5,
               CADENCE_POS_BFF_BCD     =  6,
               CADENCE_POS_BFF_CDE     =  7,
               CADENCE_POS_BFF_EAB     =  8 } ivtc_cadence_pos;
/* First and one-past-end for TFF-only and BFF-only raw positions. */
#define CADENCE_POS_TFF_FIRST 1
#define CADENCE_POS_TFF_END   5
#define CADENCE_POS_BFF_FIRST 5
#define CADENCE_POS_BFF_END   9

/**
 * For Vektor-like cadence detector algorithm.
 *
 * The bitmask is stored in a word, and its layout is:
 * blank blank BFF_CARRY BFF4 BFF3 BFF2 BFF1 BFF0   (high byte)
 * blank blank TFF_CARRY TFF4 TFF3 TFF2 TFF1 TFF0   (low byte)
 *
 * This allows predicting the next position by left-shifting the previous
 * result by one bit, copying the CARRY bits to the respective zeroth position,
 * and ANDing with 0x1F1F.
 *
 * The table is indexed with a valid ivtc_cadence_pos.
 */
const int pi_detected_pos_to_bitmask[NUM_CADENCE_POS] = { 0x0808, /* prog. */
                                                          0x0001, /* TFF ABC */
                                                          0x0002, /* TFF BCD */
                                                          0x0004, /* TFF CDE */
                                                          0x0010, /* TFF EAB */
                                                          0x0100, /* BFF ABC */
                                                          0x0200, /* BFF BCD */
                                                          0x0400, /* BFF CDE */
                                                          0x1000, /* BFF EAB */
                                                        };
#define VEKTOR_CADENCE_POS_ALL 0x1F1F
#define VEKTOR_CADENCE_POS_TFF 0x00FF
#define VEKTOR_CADENCE_POS_BFF 0xFF00
#define VEKTOR_CADENCE_POS_TFF_HIGH 0x0010
#define VEKTOR_CADENCE_POS_TFF_LOW  0x0001
#define VEKTOR_CADENCE_POS_BFF_HIGH 0x1000
#define VEKTOR_CADENCE_POS_BFF_LOW  0x0100

/* Telecine field dominance */
typedef enum { TFD_INVALID = -1, TFD_TFF = 0, TFD_BFF = 1 } ivtc_tfd;

/**
 * Position detection table.
 *
 * These are the (only) field pair combinations that should give progressive
 * frames.
 *
 * First index: detected pos
 */
static const ivtc_field_pair pi_best_field_pairs[NUM_CADENCE_POS][3] = {
    {FIELD_PAIR_TPBP, FIELD_PAIR_TCBC, FIELD_PAIR_TNBN}, /* prog. */

    {FIELD_PAIR_TPBP, FIELD_PAIR_TCBP, FIELD_PAIR_TNBC}, /* TFF ABC */
    {FIELD_PAIR_TCBP, FIELD_PAIR_TNBC, FIELD_PAIR_TNBN}, /* TFF BCD */
    {FIELD_PAIR_TCBP, FIELD_PAIR_TCBC, FIELD_PAIR_TNBN}, /* TFF CDE */
    {FIELD_PAIR_TPBP, FIELD_PAIR_TCBC, FIELD_PAIR_TNBC}, /* TFF EAB */

    {FIELD_PAIR_TPBP, FIELD_PAIR_TPBC, FIELD_PAIR_TCBN}, /* BFF ABC */
    {FIELD_PAIR_TPBC, FIELD_PAIR_TCBN, FIELD_PAIR_TNBN}, /* BFF BCD */
    {FIELD_PAIR_TPBC, FIELD_PAIR_TCBC, FIELD_PAIR_TNBN}, /* BFF CDE */
    {FIELD_PAIR_TPBP, FIELD_PAIR_TCBC, FIELD_PAIR_TCBN}, /* BFF EAB */
};

/**
 * Alternative position detection table.
 *
 * These field pair combinations should give only interlaced frames.
 *
 * Currently unused. During development it was tested that whether we detect
 * best or worst, the resulting detected cadence positions are identical
 * (neither strategy performs any different from the other).
 */
static const ivtc_field_pair pi_worst_field_pairs[NUM_CADENCE_POS][4] = {
    {FIELD_PAIR_TPBC, FIELD_PAIR_TCBP,
        FIELD_PAIR_TCBN, FIELD_PAIR_TNBC}, /* prog. */

    {FIELD_PAIR_TPBC, FIELD_PAIR_TCBC,
        FIELD_PAIR_TCBN, FIELD_PAIR_TNBN}, /* TFF ABC */
    {FIELD_PAIR_TPBP, FIELD_PAIR_TPBC,
        FIELD_PAIR_TCBC, FIELD_PAIR_TCBN}, /* TFF BCD */
    {FIELD_PAIR_TPBP, FIELD_PAIR_TPBC,
        FIELD_PAIR_TCBN, FIELD_PAIR_TNBC}, /* TFF CDE */
    {FIELD_PAIR_TPBC, FIELD_PAIR_TCBP,
        FIELD_PAIR_TCBN, FIELD_PAIR_TNBN}, /* TFF EAB */

    {FIELD_PAIR_TCBP, FIELD_PAIR_TCBC,
        FIELD_PAIR_TNBC, FIELD_PAIR_TNBN}, /* BFF ABC */
    {FIELD_PAIR_TPBP, FIELD_PAIR_TCBP,
        FIELD_PAIR_TCBC, FIELD_PAIR_TNBC}, /* BFF BCD */
    {FIELD_PAIR_TPBP, FIELD_PAIR_TCBP,
        FIELD_PAIR_TNBC, FIELD_PAIR_TCBN}, /* BFF CDE */
    {FIELD_PAIR_TCBP, FIELD_PAIR_TPBC,
        FIELD_PAIR_TNBC, FIELD_PAIR_TNBN}, /* BFF EAB */
};

/**
 * Table for extracting the i_cadence_pos part of detected cadence position
 * (ivtc_cadence_pos).
 *
 * The counter goes from 0 to 4, where "abc" = 0, "bcd" = 1, ...
 *
 * @see ivtc_cadence_pos
 */
static const int pi_detected_pos_to_cadence_pos[NUM_CADENCE_POS] = {
    3, /* prog. */
    0, /* TFF ABC */
    1, /* TFF BCD */
    2, /* TFF CDE */
    4, /* TFF EAB */
    0, /* BFF ABC */
    1, /* BFF BCD */
    2, /* BFF CDE */
    4, /* BFF EAB */
};

/**
 * Table for extracting the telecine field dominance part of detected
 * cadence position (ivtc_cadence_pos).
 *
 * The position "dea" does not provide TFF/BFF information, because it is
 * indistinguishable from progressive.
 *
 * @see ivtc_cadence_pos
 */
static const int pi_detected_pos_to_tfd[NUM_CADENCE_POS] = {
    TFD_INVALID, /* prog. */
    TFD_TFF, /* TFF ABC */
    TFD_TFF, /* TFF BCD */
    TFD_TFF, /* TFF CDE */
    TFD_TFF, /* TFF EAB */
    TFD_BFF, /* BFF ABC */
    TFD_BFF, /* BFF BCD */
    TFD_BFF, /* BFF CDE */
    TFD_BFF, /* BFF EAB */
};

/* Valid telecine sequences (TFF and BFF). Indices: [TFD][i_cadence_pos] */
/* Currently unused and left here for documentation only.
   There is an easier way - just decode the i_cadence_pos part of the
   detected position using the pi_detected_pos_to_cadence_pos table. */
/*static const int pi_valid_cadences[2][5] = { {CADENCE_POS_TFF_ABC,
                                             CADENCE_POS_TFF_BCD,
                                             CADENCE_POS_TFF_CDE,
                                             CADENCE_POS_PROGRESSIVE,
                                             CADENCE_POS_TFF_EAB},

                                             {CADENCE_POS_BFF_ABC,
                                             CADENCE_POS_BFF_BCD,
                                             CADENCE_POS_BFF_CDE,
                                             CADENCE_POS_PROGRESSIVE,
                                             CADENCE_POS_BFF_EAB},
                                           };
*/

/**
 * Operations needed in film frame reconstruction.
 */
typedef enum { IVTC_OP_DROP_FRAME,
               IVTC_OP_COPY_N,
               IVTC_OP_COPY_C,
               IVTC_OP_COMPOSE_TNBC,
               IVTC_OP_COMPOSE_TCBN } ivtc_op;

/* Note: During hard IVTC, we must avoid COPY_C and do a compose instead.
   If we COPY_C, some subtitles will flicker badly, even if we use the
   cadence-based film frame reconstruction. Try the first scene in
   Kanon (2006) vol. 3 to see the problem.

   COPY_C can be used without problems when it is used consistently
   (not constantly mixed in with COPY_N and compose operations),
   for example in soft IVTC.
*/
/**
 * Operation table for film frame reconstruction depending on cadence position.
 * Indices: [TFD][i_cadence_pos]
 * @see pi_detected_pos_to_tfd
 * @see pi_detected_pos_to_cadence_pos
 */
static const ivtc_op pi_reconstruction_ops[2][5] = { /* TFF */
                                                     {IVTC_OP_COMPOSE_TNBC,
                                                      IVTC_OP_COPY_N,
                                                      IVTC_OP_COPY_N,
                                                      IVTC_OP_DROP_FRAME,
                                                      IVTC_OP_COMPOSE_TNBC},

                                                     /* BFF */
                                                     {IVTC_OP_COMPOSE_TCBN,
                                                      IVTC_OP_COPY_N,
                                                      IVTC_OP_COPY_N,
                                                      IVTC_OP_DROP_FRAME,
                                                      IVTC_OP_COMPOSE_TCBN},
                                                   };

/**
 * Timestamp mangling table.
 *
 * This is used in the 29.97 -> 23.976 fps conversion.
 *
 * Index: i_cadence_pos.
 *
 * Valid values are nonnegative. The -1 corresponds to the dropped frame
 * and is never used, except for a debug assert.
 *
 * The unit of the values is 1/4 of frame duration.
 * See the function documentation of RenderIVTC() for an explanation.
 * @see ivtc_cadence_pos
 * @see pi_detected_pos_to_cadence_pos
 * @see pi_reconstruction_ops
 * @see RenderIVTC()
 */
static const int pi_timestamp_deltas[5] = { 1, 2, 3, -1, 0 };

/**
 * Internal helper function for RenderIVTC(): performs initialization
 * at the start of a new frame.
 *
 * In practice, this slides detector histories.
 *
 * This function should only perform initialization that does NOT require
 * the input frame history buffer.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 */
static inline void IVTCFrameInit( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;

    /* Slide detector histories */
    for( int i = 1; i < IVTC_DETECTION_HISTORY_SIZE; i++ )
    {
        p_ivtc->pi_top_rep[i-1] = p_ivtc->pi_top_rep[i];
        p_ivtc->pi_bot_rep[i-1] = p_ivtc->pi_bot_rep[i];
        p_ivtc->pi_motion[i-1]  = p_ivtc->pi_motion[i];

        p_ivtc->pi_s_cadence_pos[i-1] = p_ivtc->pi_s_cadence_pos[i];
        p_ivtc->pb_s_reliable[i-1]    = p_ivtc->pb_s_reliable[i];
        p_ivtc->pi_v_cadence_pos[i-1] = p_ivtc->pi_v_cadence_pos[i];
        p_ivtc->pi_v_raw[i-1]         = p_ivtc->pi_v_raw[i];
        p_ivtc->pb_v_reliable[i-1]    = p_ivtc->pb_v_reliable[i];

        p_ivtc->pi_cadence_pos_history[i-1]
                                      = p_ivtc->pi_cadence_pos_history[i];

        p_ivtc->pb_all_progressives[i-1] = p_ivtc->pb_all_progressives[i];
    }
    /* The latest position has not been detected yet. */
    p_ivtc->pi_s_cadence_pos[IVTC_LATEST] = CADENCE_POS_INVALID;
    p_ivtc->pb_s_reliable[IVTC_LATEST]    = false;
    p_ivtc->pi_v_cadence_pos[IVTC_LATEST] = CADENCE_POS_INVALID;
    p_ivtc->pi_v_raw[IVTC_LATEST]         = VEKTOR_CADENCE_POS_ALL;
    p_ivtc->pb_v_reliable[IVTC_LATEST]    = false;
    p_ivtc->pi_cadence_pos_history[IVTC_LATEST] = CADENCE_POS_INVALID;
    p_ivtc->pi_top_rep[IVTC_LATEST] =  0;
    p_ivtc->pi_bot_rep[IVTC_LATEST] =  0;
    p_ivtc->pi_motion[IVTC_LATEST]  = -1;
    p_ivtc->pb_all_progressives[IVTC_LATEST] = false;

    /* Slide history of field pair interlace scores */
    p_ivtc->pi_scores[FIELD_PAIR_TPBP] = p_ivtc->pi_scores[FIELD_PAIR_TCBC];
    p_ivtc->pi_scores[FIELD_PAIR_TPBC] = p_ivtc->pi_scores[FIELD_PAIR_TCBN];
    p_ivtc->pi_scores[FIELD_PAIR_TCBP] = p_ivtc->pi_scores[FIELD_PAIR_TNBC];
    p_ivtc->pi_scores[FIELD_PAIR_TCBC] = p_ivtc->pi_scores[FIELD_PAIR_TNBN];
}

/**
 * Internal helper function for RenderIVTC(): computes various raw detector
 * data at the start of a new frame.
 *
 * This function requires the input frame history buffer.
 * IVTCFrameInit() must have been called first.
 * Last two frames must be available in the history buffer.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 * @see IVTCFrameInit()
 */
static inline void IVTCLowLevelDetect( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;
    picture_t *p_curr = p_sys->pp_history[1];
    picture_t *p_next = p_sys->pp_history[2];

    assert( p_next != NULL );
    assert( p_curr != NULL );

    /* Compute interlace scores for TNBN, TNBC and TCBN.
        Note that p_next contains TNBN. */
    p_ivtc->pi_scores[FIELD_PAIR_TNBN] = CalculateInterlaceScore( p_next,
                                                                  p_next );
    p_ivtc->pi_scores[FIELD_PAIR_TNBC] = CalculateInterlaceScore( p_next,
                                                                  p_curr );
    p_ivtc->pi_scores[FIELD_PAIR_TCBN] = CalculateInterlaceScore( p_curr,
                                                                  p_next );

    int i_top = 0, i_bot = 0;
    int i_motion = EstimateNumBlocksWithMotion(p_curr, p_next, &i_top, &i_bot);
    p_ivtc->pi_motion[IVTC_LATEST] = i_motion;

    /* It's very rare if nonexistent that only one field changes between
       frames. Thus, if one field changes "clearly more" than the other,
       we know the less changed one is a likely duplicate.

       Threshold 1/2 is too low for some scenes (e.g. pan of the space junk
       at beginning of The Third ep. 1, right after the OP). Thus, we use 2/3,
       which seems to work.
    */
    p_ivtc->pi_top_rep[IVTC_LATEST] = (i_top <= 2*i_bot/3);
    p_ivtc->pi_bot_rep[IVTC_LATEST] = (i_bot <= 2*i_top/3);
}

/**
 * Internal helper function for RenderIVTC(): using raw detector data,
 * detect cadence position by an interlace scores based algorithm.
 *
 * IVTCFrameInit() and IVTCLowLevelDetect() must have been called first.
 * Last frame must be available in the history buffer.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 * @see IVTCFrameInit()
 * @see IVTCLowLevelDetect()
 * @see IVTCCadenceDetectFinalize()
 */
static inline void IVTCCadenceDetectAlgoScores( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;
    picture_t *p_next = p_sys->pp_history[2];

    assert( p_next != NULL );

    /* Detect likely cadence position according to the tables,
       using the tabulated combinations of all 7 available interlace scores.
    */
    int pi_ivtc_scores[NUM_CADENCE_POS];
    for( int i = 0; i < NUM_CADENCE_POS; i++ )
        pi_ivtc_scores[i] = p_ivtc->pi_scores[ pi_best_field_pairs[i][0] ]
                          + p_ivtc->pi_scores[ pi_best_field_pairs[i][1] ]
                          + p_ivtc->pi_scores[ pi_best_field_pairs[i][2] ];
    /* Find minimum */
    int j = CADENCE_POS_PROGRESSIVE; /* valid regardless of TFD */
    int minscore = pi_ivtc_scores[j];
    /* Note that a TFF (respectively BFF) stream may only have TFF
       (respectively BFF) telecine. Don't bother looking at solutions
       we already know to be wrong. */
    int imin = CADENCE_POS_TFF_FIRST; /* first TFF-only entry */
    int iend = CADENCE_POS_TFF_END;   /* one past last TFF-only entry */
    if( !p_next->b_top_field_first )
    {
        imin = CADENCE_POS_BFF_FIRST; /* first BFF-only entry */
        iend = CADENCE_POS_BFF_END;   /* one past last BFF-only entry */
    }
    for( int i = imin; i < iend; i++ )
    {
        if( pi_ivtc_scores[i] < minscore )
        {
            minscore = pi_ivtc_scores[i];
            j = i;
        }
    }

    /* Now "j" contains the most likely position according to the tables,
       accounting also for video TFF/BFF. */
    p_ivtc->pi_s_cadence_pos[IVTC_LATEST] = j;

    /* Estimate reliability of detector result.

       We do this by checking if the winner is an outlier at least
       to some extent. For anyone better versed in statistics,
       feel free to improve this.
    */

    /* Compute sample mean with the winner included and without.

       Sample mean is defined as mu = sum( x_i, i ) / N ,
       where N is the number of samples.
    */
    int mean = pi_ivtc_scores[CADENCE_POS_PROGRESSIVE];
    int mean_except_min = 0;
    if( j != CADENCE_POS_PROGRESSIVE )
        mean_except_min = pi_ivtc_scores[CADENCE_POS_PROGRESSIVE];
    for( int i = imin; i < iend; i++ )
    {
        mean += pi_ivtc_scores[i];
        if( i != j )
            mean_except_min += pi_ivtc_scores[i];
    }
    /* iend points one past end, but progressive counts as the +1. */
    mean /= (iend - imin + 1);
    mean_except_min /= (iend - imin);

    /* Check how much excluding the winner changes the mean. */
    double mean_ratio = (double)mean_except_min / (double)mean;

    /* Let's pretend that the detected position is a stochastic variable.
        Compute sample variance with the winner included and without.

        var = sum( (x_i - mu)^2, i ) / N ,

        where mu is the sample mean.

        Note that we really need int64_t; the numbers are pretty large.
    */
    int64_t diff = (int64_t)(pi_ivtc_scores[CADENCE_POS_PROGRESSIVE] - mean);
    int64_t var = diff*diff;
    int64_t var_except_min = 0;
    if( j != CADENCE_POS_PROGRESSIVE )
    {
        int64_t diff_exm = (int64_t)(pi_ivtc_scores[CADENCE_POS_PROGRESSIVE]
                                      - mean_except_min);
        var_except_min = diff_exm*diff_exm;
    }
    for( int i = imin; i < iend; i++ )
    {
        diff = (int64_t)(pi_ivtc_scores[i] - mean);
        var += (diff*diff);
        if( i != j )
        {
            int64_t diff_exm = (int64_t)(pi_ivtc_scores[i] - mean_except_min);
            var_except_min += (diff_exm*diff_exm);
        }
    }
    /* iend points one past end, but progressive counts as the +1. */
    var /= (uint64_t)(iend - imin + 1);
    var_except_min /= (uint64_t)(iend - imin);

    /* Extract cadence counter part of detected positions for the
       last two frames.

       Note that for the previous frame, we use the final detected cadence
       position, which was not necessarily produced by this algorithm.
       It is the result that was judged the most reliable.
    */
    int j_curr = p_ivtc->pi_cadence_pos_history[IVTC_LATEST-1];
    int pos_next = pi_detected_pos_to_cadence_pos[j];

    /* Be optimistic when unsure. We bias the detection toward accepting
       the next "correct" position, even if the variance check comes up bad.
    */
    bool b_expected = false;
    if( j_curr != CADENCE_POS_INVALID )
    {
        int pos_curr = pi_detected_pos_to_cadence_pos[j_curr];
        b_expected = (pos_next == (pos_curr + 1) % 5);
    }

    /* Use motion detect result as a final sanity check.
       If no motion, the result from this algorithm cannot be reliable.
    */
    int i_blocks_with_motion = p_ivtc->pi_motion[IVTC_LATEST];

    /* The numbers given here are empirical constants that have been tuned
       through trial and error. The test material used was NTSC anime DVDs.

        Easy-to-detect parts seem to give variance boosts of 40-70%, but
        hard-to-detect parts sometimes only 18%. Anything with a smaller boost
        in variance doesn't seem reliable for catching a new lock-on,

        Additionally, it seems that if the mean changes by less than 0.5%,
        the result is not reliable.

        Note that the numbers given are only valid for the pi_best_field_pairs
        detector strategy.

        For motion detection, the detector seems good enough so that
        we can threshold at zero.
    */
    bool b_result_reliable =
      ( i_blocks_with_motion > 0      &&
        mean_ratio           > 1.005  &&
        ( b_expected || ( (double)var > 1.17*(double)var_except_min ) )
      );
    p_ivtc->pb_s_reliable[IVTC_LATEST] = b_result_reliable;
}

/**
 * Internal helper function for RenderIVTC(): using raw detector data,
 * detect cadence position by a hard field repeat based algorithm.
 *
 * This algorithm is inspired by the classic TVTime/Xine IVTC filter
 * by Billy Biggs (Vektor); hence the name. There are however some
 * differences between this and the TVTime/Xine filter.
 *
 * IVTCFrameInit() and IVTCLowLevelDetect() must have been called first.
 * Last frame must be available in the history buffer.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 * @see IVTCFrameInit()
 * @see IVTCLowLevelDetect()
 * @see IVTCCadenceDetectFinalize()
 */
static inline void IVTCCadenceDetectAlgoVektor( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;

    picture_t *p_next = p_sys->pp_history[2];

    assert( p_next != NULL );

    /* Vektor-like cadence detection algorithm.

       This is based on detecting repeated fields (by motion detection),
       and conservatively estimating what the seen repeats could mean
       for the cadence position.

       Several possibilities are kept open until the sequence gives enough
       information to make a unique detection. When the sequence becomes
       inconsistent (e.g. bad cut), the detector resets itself.

       The main ideas taken from Vektor's algorithm are:
        1) conservatively using information from detected field repeats,
        2) cadence counting the earlier detection results and combining with
           the new detection result, and
        3) the observation that video TFF/BFF uniquely determines TFD.

        The main differences are
        1) different motion detection (see EstimateNumBlocksWithMotion()).
           Vektor's original estimates the average top/bottom field diff
           over the last 3 frames, while ours uses a block-based approach
           for diffing and just compares the field diffs of the "next" frame
           against each other. Both approaches are adaptive, but in a
           different way.
        2) the specific detection logic used is a bit different (see both codes
           for details; the original is in xine-lib, function
           determine_pulldown_offset_short_history_new() in pulldown.c;
           ours is obviously given below). I think this one is a bit simpler.
    */

    bool b_top_rep = p_ivtc->pi_top_rep[IVTC_LATEST];
    bool b_bot_rep = p_ivtc->pi_bot_rep[IVTC_LATEST];
    bool b_old_top_rep = p_ivtc->pi_top_rep[IVTC_LATEST-1];
    bool b_old_bot_rep = p_ivtc->pi_bot_rep[IVTC_LATEST-1];

    /* This is a conservative algorithm: we do not rule out possibilities
       if repeats are *not* seen, but only *add* possibilities based on what
       repeats *are* seen. We will do a raw detection, whose result is then
       filtered against what we already know.

       Progressive requires no repeats, so it is always a possibility.
       Filtering will drop it out if we know that the current position
       cannot be "dea".
    */
    int detected = 0;
    detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_PROGRESSIVE ];

    /* Add in other possibilities depending on field repeats seen during the
       last three input frames (i.e. two transitions between input frames).
       See the "Dups." column in the cadence tables.

       Note that we always add and never explicitly rule anything out.
       This is important. Otherwise full-frame repeats in the original film
       (8fps or 12fps animation is common in anime) - causing spurious
       field repeats - would mess up the detection. Handling that in a more
       sophisticated way would be a nightmare - one would have to keep track
       of full-frame repeats in the *outgoing* frames, too, and take into
       account what would happen in the output if a particular cadence position
       was chosen. Accounting for repeats in input frames only (i.e. limiting
       the detection to the progressive parts of the cadence), this has been
       tried, and found less reliable than the current, simpler strategy
       that just ignores full-frame repeats.

       Note also that we don't have to worry about getting the detection right
       in *all* cases. It's enough if we work reliably, say, 99% of the time,
       and the other 1% of the time just admit that we don't know the cadence
       position. (This mostly happens after a bad cut, when the new scene has
       "difficult" motion characteristics, such as repeated film frames.)

       The alternative, "Transcode" strategy in the frame composer will catch
       any telecined frames that slip through. Although in that case there will
       be duplicates and the output PTSs will be wrong, this is less noticeable
       than getting PTS jumps from an incorrectly locked-on cadence. Note that
       it is mostly anime, and even there mostly low-motion scenes with
       duplicate film frames that trigger the misbehavior - and in such cases
       any slight irregularity in the output timings will go unnoticed,
       as long as we get rid of interlacing artifacts.
    */
    if( b_top_rep )
    {
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_TFF_EAB ];
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_BFF_BCD ];
    }
    if( b_old_top_rep )
    {
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_TFF_ABC ];
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_BFF_CDE ];
    }
    if( b_bot_rep )
    {
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_TFF_BCD ];
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_BFF_EAB ];
    }
    if( b_old_bot_rep )
    {
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_TFF_CDE ];
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_BFF_ABC ];
    }

    /* A TFF stream may only have TFF telecine, and similarly for BFF.
        Discard the possibility we know to be incorrect for this stream.
        (Note that the stream may flipflop between the possibilities
        if it contains soft-telecined sequences or lone field repeats,
        so we must keep detecting this for each incoming frame.)
    */
    bool b_tff = p_next->b_top_field_first;
    if( b_tff )
        detected &= VEKTOR_CADENCE_POS_TFF;
    else
        detected &= VEKTOR_CADENCE_POS_BFF;

    /* Predict possible next positions based on our last detection.
       Begin with a shift and carry. */
    int predicted = p_ivtc->pi_v_raw[IVTC_LATEST-1];
    bool b_wrap_tff = false;
    bool b_wrap_bff = false;
    if( predicted & VEKTOR_CADENCE_POS_TFF_HIGH )
        b_wrap_tff = true;
    if( predicted & VEKTOR_CADENCE_POS_BFF_HIGH )
        b_wrap_bff = true;
    /* bump to next position and keep only valid bits */
    predicted = (predicted << 1) & VEKTOR_CADENCE_POS_ALL;
    /* carry */
    if( b_wrap_tff )
        predicted |= VEKTOR_CADENCE_POS_TFF_LOW;
    if( b_wrap_bff )
        predicted |= VEKTOR_CADENCE_POS_BFF_LOW;

    /* Filter: narrow down possibilities based on previous detection,
       if consistent. If not consistent, reset the detector.
       This works better than just using the latest raw detection. */
    if( (detected & predicted) != 0 )
        detected = detected & predicted;
    else
        detected = VEKTOR_CADENCE_POS_ALL;

    /* We're done. Save result to our internal storage so we can use it
       for prediction at the next frame.

       Note that the outgoing frame check in IVTCReconstructFrame()
       has a veto right, resetting us if it determines that the cadence
       has become broken.
    */
    p_ivtc->pi_v_raw[IVTC_LATEST] = detected;

    /* See if the position has been detected uniquely.
       If so, we have acquired a lock-on. */
    ivtc_cadence_pos exact = CADENCE_POS_INVALID;
    if( detected != 0 )
    {
        for( int i = 0; i < NUM_CADENCE_POS; i++ )
        {
            /* Note that we must use "&" instead of just equality to catch
               the progressive case, and also not to trigger on an incomplete
               detection. */
            if( detected == (detected & pi_detected_pos_to_bitmask[i]) )
            {
                exact = i;
                break;
            }
        }
    }

    /* If the result was unique, now "exact" contains the detected
       cadence position (and otherwise CADENCE_POS_INVALID).

       In practice, if the result from this algorithm is unique,
       it is always reliable.
    */
    p_ivtc->pi_v_cadence_pos[IVTC_LATEST] =  exact;
    p_ivtc->pb_v_reliable[IVTC_LATEST]    = (exact != CADENCE_POS_INVALID);
}

/**
 * Internal helper function for RenderIVTC(): decide the final detected
 * cadence position for the current position of the stencil,
 * using the results of the different cadence detection algorithms.
 *
 * Must be called after all IVTCCadenceDetectAlgo*() functions.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 * @see IVTCCadenceDetectAlgoScores()
 * @see IVTCCadenceDetectAlgoVektor()
 */
static inline void IVTCCadenceDetectFinalize( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;

    /* In practice "vektor" is more reliable than "scores", but it may
       take longer to lock on. Thus, we prefer "vektor" if its reliable bit
       is set, then "scores", and finally just give up.

       For progressive sequences, "vektor" outputs "3, -, 3, -, ...".
       In this case, "scores" fills in the blanks. (This particular task
       could also be done without another cadence detector, by just
       detecting the alternating pattern of "3" and no result.)
    */
    int pos = CADENCE_POS_INVALID;
    if( p_ivtc->pb_v_reliable[IVTC_LATEST] )
        pos = p_ivtc->pi_v_cadence_pos[IVTC_LATEST];
    else if( p_ivtc->pb_s_reliable[IVTC_LATEST] )
        pos = p_ivtc->pi_s_cadence_pos[IVTC_LATEST];
    p_ivtc->pi_cadence_pos_history[IVTC_LATEST] = pos;
}

/**
 * Internal helper function for RenderIVTC(): using stream flags,
 * detect soft telecine.
 *
 * This function is different from the other detectors; it may enter or exit
 * IVTC_MODE_TELECINED_NTSC_SOFT, if it detects that soft telecine has just
 * been entered or exited.
 *
 * Upon exit from soft telecine, the filter will resume operation in its
 * previous mode (which it had when soft telecine was entered).
 *
 * Last three frames must be available in the history buffer.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 */
static inline void IVTCSoftTelecineDetect( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;
    picture_t *p_prev = p_sys->pp_history[0];
    picture_t *p_curr = p_sys->pp_history[1];
    picture_t *p_next = p_sys->pp_history[2];

    assert( p_next != NULL );
    assert( p_curr != NULL );
    assert( p_prev != NULL );

    /* Soft telecine can be detected from the flag pattern:
       nb_fields = 3,2,3,2,... and *video* TFF = true, false, false, true
       (TFF telecine) or false, true, true, false (BFF telecine).

       We don't particularly care which field goes first, because we're
       constructing progressive frames, and the video FDs of successive frames
       must in any case match any field repeats in order for field renderers
       (such as traditional DVD player + CRT TV) to work correctly. Thus the
       video TFF/BFF flag provides no additional useful information for us
       on top of checking nb_fields.

       Note that the only thing to *do* to soft telecine in an IVTC filter
       is to even out the outgoing PTS diffs to 2.5 fields each, so that we get
       a steady 24fps output. Thus, we can do this  processing even if it turns
       out that we saw a lone field repeat (which are also sometimes used,
       such as in the Silent Mobius OP and in Sol Bianca). We can be aggressive
       and don't need to care about false positives - as long as we are equally
       aggressive about dropping out of soft telecine mode the moment a "2" is
       followed by another "2" and not a "3" as in soft TC.

       Finally, we conclude that the one-frame future buffer is enough for us
       to make soft TC decisions just in time for rendering the frame in the
       "current" position (the flag patterns below constitute proof of this
       property).

       Soft telecine is relatively rare at least in anime, but it exists;
       e.g. Angel Links OP, Silent Mobius, and Stellvia of the Universe have
       sequences that are soft telecined. Stellvia, especially, alternates
       between soft and hard telecine all the time.
    */

    /* Valid stream flag patterns for soft telecine. There are three: */

    /* Entering soft telecine at frame curr, or running inside it already */
    bool b_soft_telecine_1 = (p_prev->i_nb_fields == 2) &&
                             (p_curr->i_nb_fields == 3) &&
                             (p_next->i_nb_fields == 2);
    /* Running inside soft telecine */
    bool b_soft_telecine_2 = (p_prev->i_nb_fields == 3) &&
                             (p_curr->i_nb_fields == 2) &&
                             (p_next->i_nb_fields == 3);
    /* Exiting soft telecine at frame curr (curr is the last frame
       that should be handled as soft TC) */
    bool b_soft_telecine_3 = (p_prev->i_nb_fields == 3) &&
                             (p_curr->i_nb_fields == 2) &&
                             (p_next->i_nb_fields == 2);

    /* Soft telecine is very clear-cut - the moment we see or do not see
       a valid flag pattern, we can change the filter mode.
    */
    if( b_soft_telecine_1 || b_soft_telecine_2 || b_soft_telecine_3 )
    {
        if( p_ivtc->i_mode != IVTC_MODE_TELECINED_NTSC_SOFT )
        {
            msg_Dbg( p_filter, "IVTC: 3:2 pulldown: NTSC soft telecine "\
                               "detected." );
            p_ivtc->i_old_mode = p_ivtc->i_mode;
        }

        /* Valid flag pattern seen, this frame is soft telecined */
        p_ivtc->i_mode = IVTC_MODE_TELECINED_NTSC_SOFT;

        /* Only used during IVTC'ing hard telecine. */
        p_ivtc->i_cadence_pos = CADENCE_POS_INVALID;
        p_ivtc->i_tfd         = TFD_INVALID;
    }
    /* Note: no flag pattern match now */
    else if( p_ivtc->i_mode == IVTC_MODE_TELECINED_NTSC_SOFT )
    {
        msg_Dbg( p_filter, "IVTC: 3:2 pulldown: NTSC soft telecine ended. "\
                           "Returning to previous mode." );

        /* No longer soft telecined, return filter to the mode it had earlier.
           This is needed to fix cases where we came in from hard telecine, and
           should go back, but can't catch a cadence in time before telecined
           frames slip through. Kickstarting back to hard IVTC fixes the
           problem. This happens a lot in Stellvia.
        */
        p_ivtc->i_mode = p_ivtc->i_old_mode;
        p_ivtc->i_cadence_pos = 0; /* Wild guess. The film frame reconstruction
                                      will start in emergency mode, and this
                                      will be filled in by the detector ASAP.*/
        /* I suppose video field dominance no longer flipflops. */
        p_ivtc->i_tfd = p_next->b_top_field_first;
    }
}

/**
 * Internal helper function for RenderIVTC(): using the history of detected
 * cadence positions, analyze the cadence and enter or exit
 * IVTC_MODE_TELECINED_NTSC_HARD when appropriate.
 *
 * This also updates b_sequence_valid.
 *
 * Last three frames must be available in the history buffer.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 */
static void IVTCCadenceAnalyze( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;
    picture_t *p_prev = p_sys->pp_history[0];
    picture_t *p_curr = p_sys->pp_history[1];
    picture_t *p_next = p_sys->pp_history[2];

    assert( p_next != NULL );
    assert( p_curr != NULL );
    assert( p_prev != NULL );

    /* Determine which frames in the buffer qualify for analysis.

       Note that hard telecine always has nb_fields = 2 and
       video TFF = constant (i.e. the stream flags look no different from
       a true interlaced or true progressive stream). Basically, no one ever
       sets the progressive frame flag for the input frames d, e, and a -
       in practice they're all flagged as interlaced.

       A frame may qualify for hard TC analysis if it has no soft field repeat
       (i.e. it cannot be part of a soft telecine). The condition
       nb_fields == 2 must always match.

       Additionally, curr and next must have had motion with respect to the
       previous frame, to ensure that the different field combinations have
       produced unique pictures.

       Alternatively, if there was no motion, but the cadence position was
       reliably detected and it was the expected one, we qualify the frame
       for analysis (mainly, for TFD voting).

       We only proceed with the cadence analysis if all three frames
       in the buffer qualify.
    */

    /* Note that these are the final detected positions
       produced by IVTCCadenceDetectFinalize(). */
    int j_next = p_ivtc->pi_cadence_pos_history[IVTC_LATEST];
    int j_curr = p_ivtc->pi_cadence_pos_history[IVTC_LATEST-1];
    int j_prev = p_ivtc->pi_cadence_pos_history[IVTC_LATEST-2];

    bool b_expected = false;
    if( j_next != CADENCE_POS_INVALID  &&  j_curr != CADENCE_POS_INVALID )
    {
        int pos_next = pi_detected_pos_to_cadence_pos[j_next];
        int pos_curr = pi_detected_pos_to_cadence_pos[j_curr];
        b_expected = (pos_next == (pos_curr + 1) % 5);
    }
    bool b_old_expected  = false;
    if( j_curr != CADENCE_POS_INVALID  &&  j_prev != CADENCE_POS_INVALID )
    {
        int pos_curr = pi_detected_pos_to_cadence_pos[j_curr];
        int pos_prev = pi_detected_pos_to_cadence_pos[j_prev];
        b_old_expected = (pos_curr == (pos_prev + 1) % 5);
    }

    int i_motion     = p_ivtc->pi_motion[IVTC_LATEST];
    int i_old_motion = p_ivtc->pi_motion[IVTC_LATEST-1];

    bool b_prev_valid  = (p_prev->i_nb_fields == 2);
    bool b_curr_valid  = (p_curr->i_nb_fields == 2)  &&
                         (i_old_motion > 0  ||  b_old_expected);
    bool b_next_valid  = (p_next->i_nb_fields == 2)  &&
                         (i_motion > 0      ||  b_expected);
    bool b_no_invalids = (b_prev_valid && b_curr_valid && b_next_valid);

    /* Final sanity check: see that the detection history has been
       completely filled,  i.e. the latest three positions of the stencil
       have given a result from the cadence detector.
    */
    if( b_no_invalids )
    {
        for( int i = 0; i < IVTC_DETECTION_HISTORY_SIZE; ++i )
        {
            const int i_detected_pos = p_ivtc->pi_cadence_pos_history[i];
            if( i_detected_pos == CADENCE_POS_INVALID )
            {
                b_no_invalids = false;
                break;
            }
        }
    }

    /* If still ok, do the analysis. */
    p_ivtc->b_sequence_valid = false; /* needed in frame reconstruction */
    if( b_no_invalids )
    {
        /* Convert the history elements to cadence position and TFD. */
        int pi_tfd[IVTC_DETECTION_HISTORY_SIZE];
        int pi_pos[IVTC_DETECTION_HISTORY_SIZE];
        for( int i = 0; i < IVTC_DETECTION_HISTORY_SIZE; ++i )
        {
            const int i_detected_pos = p_ivtc->pi_cadence_pos_history[i];
            pi_pos[i] = pi_detected_pos_to_cadence_pos[i_detected_pos];
            pi_tfd[i] = pi_detected_pos_to_tfd[i_detected_pos];
        }

        /* See if the sequence is valid. The cadence positions must be
           successive mod 5.  We can't say anything about TFF/BFF yet,
           because the progressive-looking position "dea"  may be there.
           If the sequence otherwise looks valid, we handle that last
           by voting.

           We also test for a progressive signal here, so that we know
           when to exit IVTC_MODE_TELECINED_NTSC_HARD.
        */
        p_ivtc->b_sequence_valid = true;
        bool b_all_progressive = (pi_pos[0] == 3);
        int j = pi_pos[0];
        for( int i = 1; i < IVTC_DETECTION_HISTORY_SIZE; ++i )
        {
            if( pi_pos[i] != (++j % 5) )
                p_ivtc->b_sequence_valid = false;
            if( pi_pos[i] != 3 )
                b_all_progressive = false;
        }
        p_ivtc->pb_all_progressives[IVTC_LATEST] = b_all_progressive;

        if( p_ivtc->b_sequence_valid )
        {
            /* Determine TFF/BFF. */
            int i_vote_invalid = 0;
            int i_vote_tff     = 0;
            int i_vote_bff     = 0;
            for( int i = 0; i < IVTC_DETECTION_HISTORY_SIZE; ++i )
            {
                if( pi_tfd[i] == TFD_INVALID )
                    i_vote_invalid++;
                else if( pi_tfd[i] == TFD_TFF )
                    i_vote_tff++;
                else if( pi_tfd[i] == TFD_BFF )
                    i_vote_bff++;
            }

            /* With three entries, two votes for any one item are enough
               to decide this conclusively. */
            int i_telecine_field_dominance = TFD_INVALID;
            if( i_vote_tff >= 2)
                i_telecine_field_dominance = TFD_TFF;
            else if( i_vote_bff >= 2)
                i_telecine_field_dominance = TFD_BFF;
            /* In all other cases, "invalid" won or no winner.
               This means no NTSC telecine detected. */

            /* Lock on to the cadence if it was valid and TFF/BFF was found.

               Also, aggressively update the cadence counter from the
               lock-on data whenever we can. In practice this has been found
               to be a reliable strategy (if the cadence detectors are
               good enough).
            */
            if( i_telecine_field_dominance == TFD_TFF )
            {
                if( p_ivtc->i_mode != IVTC_MODE_TELECINED_NTSC_HARD )
                    msg_Dbg( p_filter, "IVTC: 3:2 pulldown: NTSC TFF "\
                                       "hard telecine detected." );
                p_ivtc->i_mode        = IVTC_MODE_TELECINED_NTSC_HARD;
                p_ivtc->i_cadence_pos = pi_pos[IVTC_LATEST];
                p_ivtc->i_tfd         = TFD_TFF;
            }
            else if( i_telecine_field_dominance == TFD_BFF )
            {
                if( p_ivtc->i_mode != IVTC_MODE_TELECINED_NTSC_HARD )
                    msg_Dbg( p_filter, "IVTC: 3:2 pulldown: NTSC BFF "\
                                       "hard telecine detected." );
                p_ivtc->i_mode        = IVTC_MODE_TELECINED_NTSC_HARD;
                p_ivtc->i_cadence_pos = pi_pos[IVTC_LATEST];
                p_ivtc->i_tfd         = TFD_BFF;
            }
        }
        /* No telecine... maybe a progressive signal? */
        else if( b_all_progressive )
        {
            /* It seems that in practice, three "3"s in a row can still be
               a fluke rather often. Four or five usually are not.
               This fixes the Stellvia OP. */

            bool b_really_all_progressive = true;
            for( int i = 0; i < IVTC_DETECTION_HISTORY_SIZE ; i++ )
            {
                if( p_ivtc->pb_all_progressives[i] == false )
                {
                    b_really_all_progressive = false;
                    break;
                }
            }

            /* If we still think the signal is progressive... */
            if( b_really_all_progressive )
            {
                /* ...exit film mode immediately. */
                if( p_ivtc->i_mode == IVTC_MODE_TELECINED_NTSC_HARD )
                    msg_Dbg( p_filter, "IVTC: 3:2 pulldown: progressive "\
                                       "signal detected." );
                p_ivtc->i_mode        = IVTC_MODE_DETECTING;
                p_ivtc->i_cadence_pos = CADENCE_POS_INVALID;
                p_ivtc->i_tfd         = TFD_INVALID;
            }
        }
        /* Final missing "else": no valid NTSC telecine sequence detected.

            Either there is no telecine, or the detector - although it produced
            results - had trouble finding it. In this case we do nothing,
            as it's not a good idea to act on unreliable data.
        */
    }
}

/**
 * Internal helper function for RenderIVTC(): render or drop frame,
 * whichever needs to be done. This also sets the output frame PTS.
 *
 * Last two frames must be available in the history buffer.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance. Must be non-NULL.
 * @param[out] p_dst Frame will be rendered here. Must be non-NULL.
 * @return Whether a frame was constructed.
 * @retval true Yes, output frame is in p_dst.
 * @retval false No, this frame was dropped as part of normal IVTC operation.
 * @see RenderIVTC()
 */
static bool IVTCOutputOrDropFrame( filter_t *p_filter, picture_t *p_dst )
{
    assert( p_filter != NULL );
    assert( p_dst != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;
    mtime_t t_final = VLC_TS_INVALID; /* for custom timestamp mangling */

    picture_t *p_curr = p_sys->pp_history[1];
    picture_t *p_next = p_sys->pp_history[2];

    assert( p_next != NULL );
    assert( p_curr != NULL );

    /* Perform IVTC if we're in film mode (either hard or soft telecine).

       Note that we don't necessarily have a lock-on, even if we are in
       IVTC_MODE_TELECINED_NTSC_HARD. We *may* be locked on, or alternatively,
       we have seen a valid cadence some time in the past, but lock-on has
       since been lost, and we have not seen a progressive signal after that.
       The latter case usually results from bad cuts, which interrupt
       the cadence.
    */
    int i_result_score = -1;
    int op;
    if( p_ivtc->i_mode == IVTC_MODE_TELECINED_NTSC_HARD )
    {
        assert( p_ivtc->i_cadence_pos != CADENCE_POS_INVALID );
        assert( p_ivtc->i_tfd != TFD_INVALID );

        /* Decide what to do. The operation table is only enabled
           if the cadence seems reliable. Otherwise we use a backup strategy.
        */
        if( p_ivtc->b_sequence_valid )
        {
            /* Pick correct operation from the operation table. */
            op = pi_reconstruction_ops[p_ivtc->i_tfd][p_ivtc->i_cadence_pos];

            if( op == IVTC_OP_DROP_FRAME )
            {
                /* Bump cadence counter into the next expected position */
                p_ivtc->i_cadence_pos = ++p_ivtc->i_cadence_pos % 5;

                /* Drop frame. We're done. */
                return false;
            }
            /* Frame not dropped */
            else if( p_ivtc->b_sequence_valid )
            {
                if( op == IVTC_OP_COPY_N )
                    i_result_score = p_ivtc->pi_scores[FIELD_PAIR_TNBN];
                else if( op == IVTC_OP_COPY_C )
                    i_result_score = p_ivtc->pi_scores[FIELD_PAIR_TCBC];
                else if( op == IVTC_OP_COMPOSE_TNBC )
                    i_result_score = p_ivtc->pi_scores[FIELD_PAIR_TNBC];
                else if( op == IVTC_OP_COMPOSE_TCBN )
                    i_result_score = p_ivtc->pi_scores[FIELD_PAIR_TCBN];

                /* Sanity check the result */

                /* Compute running mean of outgoing interlace score.
                   See below for history mechanism. */
                int i_avg = 0;
                for( int i = 0; i < IVTC_DETECTION_HISTORY_SIZE; i++)
                    i_avg += p_ivtc->pi_final_scores[i];
                i_avg /= IVTC_DETECTION_HISTORY_SIZE;

                /* Check if the score suddenly became "clearly larger".
                   Also, filter out spurious peaks at the low end. */
                if( i_result_score > 1000  &&  i_result_score > 2*i_avg )
                {
                    /* Sequence wasn't reliable after all; we'll use
                       the Transcode strategy for this frame. */
                    p_ivtc->b_sequence_valid = false;
                    msg_Dbg( p_filter, "Rejected cadence-based frame "\
                                       "construction: interlace score %d "\
                                       "(running average %d)",
                                       i_result_score, i_avg );

                    /* We also reset the detector used in the "vektor"
                       algorithm, as it depends on having a reliable previous
                       position. In practice, we continue using the Transcode
                       strategy until the cadence becomes locked on again.
                       (At that point, b_sequence_valid will become true again,
                       and we continue with this strategy.)
                    */
                    p_ivtc->pi_v_raw[IVTC_LATEST] = VEKTOR_CADENCE_POS_ALL;
                }
            }
        }

        /* Frame not dropped, and the cadence counter seems unreliable.

            Note that this is not an "else" to the previous case. This may
            begin with a valid sequence, and then the above logic decides
            that it wasn't valid after all.
        */
        if( !p_ivtc->b_sequence_valid )
        {
            /* In this case, we must proceed with no cadence information.
                We use a Transcode-like strategy.

                We check which field paired with TN or BN (accounting for
                the field dominance) gives the smallest interlace score,
                and declare that combination the resulting progressive frame.

                This strategy gives good results on average, but often fails
                in talking scenes in anime. Those can be handled more reliably
                with a locked-on cadence produced by the "vektor" algorithm.
            */

            int tnbn = p_ivtc->pi_scores[FIELD_PAIR_TNBN]; /* TFF and BFF */
            int tnbc = p_ivtc->pi_scores[FIELD_PAIR_TNBC]; /* TFF only */
            int tcbn = p_ivtc->pi_scores[FIELD_PAIR_TCBN]; /* BFF only */

            if( p_next->b_top_field_first )
            {
                if( tnbn <= tnbc )
                {
                    op = IVTC_OP_COPY_N;
                    i_result_score = tnbn;
                }
                else
                {
                    op = IVTC_OP_COMPOSE_TNBC;
                    i_result_score = tnbc;
                }
            }
            else
            {
                if( tnbn <= tcbn )
                {
                    op = IVTC_OP_COPY_N;
                    i_result_score = tnbn;
                }
                else
                {
                    op = IVTC_OP_COMPOSE_TCBN;
                    i_result_score = tcbn;
                }
            }
        }

        /* Note that we get to this point only if we didn't drop the frame.
            Mangle the presentation timestamp to convert 29.97 -> 23.976 fps.
        */
        int i_timestamp_delta = pi_timestamp_deltas[p_ivtc->i_cadence_pos];
        if( p_ivtc->b_sequence_valid )
            assert( i_timestamp_delta >= 0 );

        /* "Current" is the frame that is being extracted now. Use its original
           timestamp as the base.

           Note that this way there will be no extra delay compared to the
           raw stream, even though we look one frame into the future.
        */
        if( p_ivtc->b_sequence_valid )
        {
            /* FIXME: use field length as measured by Deinterlace()? */
            t_final = p_curr->date
                    + (p_next->date - p_curr->date)*i_timestamp_delta/4;
        }
        else /* Do not mangle timestamps (or drop frames, either) if cadence
                is not locked on. This causes one of five output frames - if
                all are reconstructed correctly - to be a duplicate, but in
                practice at least with anime (which is the kind of material
                that tends to have this problem) this is less noticeable than
                a sudden jump in the cadence. Especially, a consistently wrong
                lock-on will cause a very visible stutter, which we wish
                to avoid. */
        {
            t_final = p_curr->date;
        }

        /* Bump cadence counter into the next expected position. */
        p_ivtc->i_cadence_pos = ++p_ivtc->i_cadence_pos % 5;
    }
    else if( p_ivtc->i_mode == IVTC_MODE_TELECINED_NTSC_SOFT )
    {
        /* Soft telecine. We have the progressive frames already;
           even out PTS diffs only. */

        /* Pass through the "current" frame. We must choose the frame "current"
           in order to be able to detect soft telecine before we have to output
           the frame. See IVTCSoftTelecineDetect(). Also, this allows
           us to peek at the next timestamp to calculate the duration of
           "current".
        */
        op = IVTC_OP_COPY_C;
        i_result_score = p_ivtc->pi_scores[FIELD_PAIR_TCBC];

        /* Timestamp mangling for soft telecine: bump "threes" forward by
           0.5 field durations. This is more forgiving for the renderer
           than bumping the "twos" back (which would  require to render
           them sooner),
        */
        if( p_curr->i_nb_fields == 3 )
        {
            /* Approximate field duration from the PTS difference.  */
            /* FIXME: use field length as measured by Deinterlace()? */
            mtime_t i_half_field_dur = ( (p_next->date - p_curr->date)/3 ) / 2;
            t_final = p_curr->date + i_half_field_dur;
        }
        else /* Otherwise, use original PTS of the outgoing frame. */
        {
            t_final = p_curr->date;
        }
    }
    else /* Not film mode, timestamp mangling bypassed. */
    {
        op = IVTC_OP_COPY_N;
        i_result_score = p_ivtc->pi_scores[FIELD_PAIR_TNBN];

        /* Preserve original PTS (note that now, in principle,
                                  "next" is the outgoing frame) */
        t_final = p_next->date;
    }

    /* There is only one case where we should drop the frame,
       and it was already handled above. */
    assert( op != IVTC_OP_DROP_FRAME );

    /* Render into p_dst according to the final operation chosen. */
    if( op == IVTC_OP_COPY_N )
        picture_Copy( p_dst, p_next );
    else if( op == IVTC_OP_COPY_C )
        picture_Copy( p_dst, p_curr );
    else if( op == IVTC_OP_COMPOSE_TNBC )
        ComposeFrame( p_filter, p_dst, p_next, p_curr, CC_ALTLINE );
    else if( op == IVTC_OP_COMPOSE_TCBN )
        ComposeFrame( p_filter, p_dst, p_curr, p_next, CC_ALTLINE );

    /* Slide history of outgoing interlace scores. This must be done last,
       and only if the frame was not dropped, so we do it here.

       This is used during the reconstruction to get an idea of what is
       (in the temporally local sense) an acceptable interlace score
       for a correctly reconstructed frame. See above.
    */
    for( int i = 1; i < IVTC_DETECTION_HISTORY_SIZE; i++ )
        p_ivtc->pi_final_scores[i-1] = p_ivtc->pi_final_scores[i];
    p_ivtc->pi_final_scores[IVTC_LATEST] = i_result_score;

    /* Note that picture_Copy() copies the PTS, too. Apply timestamp mangling
       now, if any was needed.
    */
    if( t_final > VLC_TS_INVALID )
        p_dst->date = t_final;

    return true;
}

/* The top-level routine of the IVTC filter.

   See the lengthy comment above for function documentation.
*/
static int RenderIVTC( filter_t *p_filter, picture_t *p_dst, picture_t *p_src )
{
    assert( p_filter != NULL );
    assert( p_src != NULL );
    assert( p_dst != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;

    picture_t *p_prev = p_sys->pp_history[0];
    picture_t *p_curr = p_sys->pp_history[1];
    picture_t *p_next = p_sys->pp_history[2];

    /* If the history mechanism has failed, we have nothing to do. */
    if( !p_next )
        return VLC_EGENERIC;

    /* Slide algorithm-specific histories */
    IVTCFrameInit( p_filter );

    /* Filter if we have all the pictures we need.
       Note that we always have p_next at this point. */
    if( p_prev && p_curr )
    {
        /* Update raw data for motion, field repeats, interlace scores... */
        IVTCLowLevelDetect( p_filter );

        /* Detect soft telecine.

           Enter/exit IVTC_MODE_TELECINED_NTSC_SOFT when needed.
        */
        IVTCSoftTelecineDetect( p_filter );

        /* Detect hard telecine.

           Enter/exit IVTC_MODE_TELECINED_NTSC_HARD when needed.

           If we happen to be running in IVTC_MODE_TELECINED_NTSC_SOFT,
           we nevertheless let the algorithms see for themselves that
           the stream is progressive. This doesn't break anything,
           and this way the full filter state gets updated at each frame.

           See the individual function docs for details.
        */
        IVTCCadenceDetectAlgoScores( p_filter );
        IVTCCadenceDetectAlgoVektor( p_filter );
        IVTCCadenceDetectFinalize( p_filter ); /* pick winner */
        IVTCCadenceAnalyze( p_filter ); /* update filter state */

        /* Now we can... */
        bool b_have_output_frame = IVTCOutputOrDropFrame( p_filter, p_dst );

        /* The next frame will get a custom timestamp, too. */
        p_sys->i_frame_offset = CUSTOM_PTS;

        if( b_have_output_frame )
            return VLC_SUCCESS;
        else
            return VLC_EGENERIC; /* Signal the caller not to expect a frame */
    }
    else if( !p_prev && !p_curr ) /* first frame */
    {
        /* Render the first frame as-is, so that a picture appears immediately.

           We will also do some init for the filter. This score will become
           TPBP by the time the actual filter starts. Note that the sliding of
           final scores only starts when the filter has started (third frame).
        */
        int i_score = CalculateInterlaceScore( p_next, p_next );
        p_ivtc->pi_scores[FIELD_PAIR_TNBN] = i_score;
        p_ivtc->pi_final_scores[0]         = i_score;

        picture_Copy( p_dst, p_next );
        return VLC_SUCCESS;
    }
    else /* second frame */
    {
        /* If the history sliding mechanism works correctly,
           the only remaining possibility is that: */
        assert( p_curr && !p_prev );

        /* We need three frames for the detector to work, so we drop this one.
           We will only do some initialization for the detector here. */

        /* These scores will become TCBC, TCBP and TPBC when the filter starts.
           The score for the current TCBC has already been computed at the
           first frame, and slid into place at the start of this frame
           (by IVTCFrameInit()).
        */
        p_ivtc->pi_scores[FIELD_PAIR_TNBN] =
                                     CalculateInterlaceScore( p_next, p_next );
        p_ivtc->pi_scores[FIELD_PAIR_TNBC] =
                                     CalculateInterlaceScore( p_next, p_curr );
        p_ivtc->pi_scores[FIELD_PAIR_TCBN] =
                                     CalculateInterlaceScore( p_curr, p_next );

        /* TNBN is a wild guess, but doesn't really matter */
        p_ivtc->pi_final_scores[1] = p_ivtc->pi_scores[FIELD_PAIR_TNBN];

        /* At the next frame, the filter starts. The next frame will get
           a custom timestamp. */
        p_sys->i_frame_offset = CUSTOM_PTS;

        /* Not really an error. This is expected, but we must
           signal the caller not to expect an output frame. */
        return VLC_EGENERIC;
    }
}

/**
 * Clears the inverse telecine subsystem state.
 *
 * Used during initialization and uninitialization.
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 * @see Open()
 * @see Flush()
 */
static void IVTCClearState( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc = &p_sys->ivtc;

    p_ivtc->i_cadence_pos = CADENCE_POS_INVALID;
    p_ivtc->i_tfd         = TFD_INVALID;
    p_ivtc->b_sequence_valid = false;
    p_ivtc->i_mode     = IVTC_MODE_DETECTING;
    p_ivtc->i_old_mode = IVTC_MODE_DETECTING;
    for( int i = 0; i < IVTC_NUM_FIELD_PAIRS; i++ )
        p_ivtc->pi_scores[i] = 0;
    for( int i = 0; i < IVTC_DETECTION_HISTORY_SIZE; i++ )
    {
        p_ivtc->pi_cadence_pos_history[i] = CADENCE_POS_INVALID;

        p_ivtc->pi_s_cadence_pos[i] = CADENCE_POS_INVALID;
        p_ivtc->pb_s_reliable[i]    = false;
        p_ivtc->pi_v_cadence_pos[i] = CADENCE_POS_INVALID;
        p_ivtc->pb_v_reliable[i]    = false;

        p_ivtc->pi_v_raw[i]         = VEKTOR_CADENCE_POS_ALL;

        /* the most neutral result considering the "vektor" algorithm */
        p_ivtc->pi_top_rep[i] = 1;
        p_ivtc->pi_bot_rep[i] = 1;
        p_ivtc->pi_motion[i] = -1;

        p_ivtc->pb_all_progressives[i] = false;

        p_ivtc->pi_final_scores[i] = 0;
    }
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

        case DEINTERLACE_IVTC:
            /* Note: RenderIVTC will automatically drop the duplicate frames
                     produced by IVTC. This is part of normal operation. */
            if( RenderIVTC( p_filter, p_dst[0], p_pic ) )
                goto drop;
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

    for( int i = 0; i < DEINTERLACE_DST_SIZE; ++i )
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
    IVTCClearState( p_filter );
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

    IVTCClearState( p_filter );

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

