/*****************************************************************************
 * x264.c: h264 video encoder
 *****************************************************************************
 * Copyright (C) 2004-2010 the VideoLAN team
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Ilkka Ollakka <ileoo (at)videolan org>
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

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_codec.h>
#include <vlc_charset.h>
#include <vlc_cpu.h>
#include <math.h>

#if defined(PTW32_STATIC_LIB) && defined(HAVE_PTHREAD_H)
#include <pthread.h>
#endif
#ifdef MODULE_NAME_IS_x262
#include <x262.h>
#else
#include <x264.h>
#endif

#include <assert.h>

#ifdef MODULE_NAME_IS_x26410b
#define SOUT_CFG_PREFIX "sout-x26410b-"
#endif
#ifdef MODULE_NAME_IS_x262
#define SOUT_CFG_PREFIX "sout-x262-"
#endif
#ifdef MODULE_NAME_IS_x264
#define SOUT_CFG_PREFIX "sout-x264-"
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );
static void x264_log( void *, int i_level, const char *psz, va_list );

/* Frame-type options */

#define KEYINT_TEXT N_("Maximum GOP size")
#define KEYINT_LONGTEXT N_( "Sets maximum interval between IDR-frames. " \
    "Larger values save bits, thus improving quality for a given bitrate at " \
    "the cost of seeking precision. Use -1 for infinite." )

#define MIN_KEYINT_TEXT N_("Minimum GOP size")
#define MIN_KEYINT_LONGTEXT N_( "Sets minimum interval between IDR-frames. " \
    "In H.264, I-frames do not necessarily bound a closed GOP because it is " \
    "allowable for a P-frame to be predicted from more frames than just the " \
    "one frame before it (also see reference frame option). Therefore, " \
    "I-frames are not necessarily seekable. IDR-frames restrict subsequent " \
    "P-frames from referring to any frame prior to the IDR-frame. \n" \
    "If scenecuts appear within this interval, they are still encoded as " \
    "I-frames, but do not start a new GOP." )

#define OPENGOP_TEXT N_("Use recovery points to close GOPs")
#define OPENGOP_LONGTEXT N_("use open GOP, for bluray compatibility use also bluray-compat option")

#define BLURAY_TEXT N_("Enable compatibility hacks for Blu-ray support")
#define BLURAY_LONGTEXT N_("Enable hacks for Blu-ray support, this doesn't enforce every aspect of Blu-ray compatibility\n" \
    "e.g. resolution, framerate, level" )

#define SCENE_TEXT N_("Extra I-frames aggressivity" )
#define SCENE_LONGTEXT N_( "Scene-cut detection. Controls how " \
    "aggressively to insert extra I-frames. With small values of " \
    "scenecut, the codec often has " \
    "to force an I-frame when it would exceed keyint. " \
    "Good values of scenecut may find a better location for the " \
    "I-frame. Large values use more I-frames " \
    "than necessary, thus wasting bits. -1 disables scene-cut detection, so " \
    "I-frames are inserted only every other keyint frames, which probably " \
    "leads to ugly encoding artifacts. Range 1 to 100." )


#define BFRAMES_TEXT N_("B-frames between I and P")
#define BFRAMES_LONGTEXT N_( "Number of consecutive B-frames between I and " \
    "P-frames. Range 1 to 16." )

#define B_ADAPT_TEXT N_("Adaptive B-frame decision")
#define B_ADAPT_LONGTEXT N_( "Force the specified number of " \
    "consecutive B-frames to be used, except possibly before an I-frame. " \
    "Range 0 to 2." )

#define B_BIAS_TEXT N_("Influence (bias) B-frames usage")
#define B_BIAS_LONGTEXT N_( "Bias the choice to use B-frames. Positive values " \
    "cause more B-frames, negative values cause less B-frames." )


#define BPYRAMID_TEXT N_("Keep some B-frames as references")
#define BPYRAMID_LONGTEXT N_( "Allows B-frames to be used as references for " \
    "predicting other frames. Keeps the middle of 2+ consecutive B-frames " \
    "as a reference, and reorders frame appropriately.\n" \
    " - none: Disabled\n" \
    " - strict: Strictly hierarchical pyramid\n" \
    " - normal: Non-strict (not Blu-ray compatible)\n"\
    )

#define FULLRANGE_TEXT N_("Use fullrange instead of TV colorrange")
#define FULLRANGE_LONGTEXT N_("TV-range is usually used colorrange, defining this to true " \
                              "will enable libx264 to use full colorrange on encoding")

#define CABAC_TEXT N_("CABAC")
#define CABAC_LONGTEXT N_( "CABAC (Context-Adaptive Binary Arithmetic "\
    "Coding). Slightly slows down encoding and decoding, but should save " \
    "10 to 15% bitrate." )

#define REF_TEXT N_("Number of reference frames")
#define REF_LONGTEXT N_( "Number of previous frames used as predictors. " \
    "This is effective in Anime, but seems to make little difference in " \
    "live-action source material. Some decoders are unable to deal with " \
    "large frameref values. Range 1 to 16." )

#define NF_TEXT N_("Skip loop filter")
#define NF_LONGTEXT N_( "Deactivate the deblocking loop filter (decreases quality).")

#define FILTER_TEXT N_("Loop filter AlphaC0 and Beta parameters alpha:beta")
#define FILTER_LONGTEXT N_( "Loop filter AlphaC0 and Beta parameters. " \
    "Range -6 to 6 for both alpha and beta parameters. -6 means light " \
    "filter, 6 means strong.")

#define LEVEL_TEXT N_("H.264 level")
#define LEVEL_LONGTEXT N_( "Specify H.264 level (as defined by Annex A " \
    "of the standard). Levels are not enforced; it's up to the user to select " \
    "a level compatible with the rest of the encoding options. Range 1 to 5.1 " \
    "(10 to 51 is also allowed). Set to 0 for letting x264 set level.")

#define PROFILE_TEXT N_("H.264 profile")
#define PROFILE_LONGTEXT N_("Specify H.264 profile which limits are enforced over " \
        "other settings" )

/* In order to play an interlaced output stream encoded by x264, a decoder needs
   mbaff support. r570 is using the 'mb' part and not 'aff' yet; so it's really
   'pure-interlaced' mode */
#define INTERLACED_TEXT N_("Interlaced mode")
#define INTERLACED_LONGTEXT N_( "Pure-interlaced mode.")

#define FRAMEPACKING_TEXT N_("Frame packing")
#define FRAMEPACKING_LONGTEXT N_( "For stereoscopic videos define frame arrangement:\n" \
    " 0: checkerboard - pixels are alternatively from L and R\n" \
    " 1: column alternation - L and R are interlaced by column\n" \
    " 2: row alternation - L and R are interlaced by row\n" \
    " 3: side by side - L is on the left, R on the right\n" \
    " 4: top bottom - L is on top, R on bottom\n" \
    " 5: frame alternation - one view per frame" )

#define INTRAREFRESH_TEXT N_("Use Periodic Intra Refresh")
#define INTRAREFRESH_LONGTEXT N_("Use Periodic Intra Refresh instead of IDR frames")

#define MBTREE_TEXT N_("Use mb-tree ratecontrol")
#define MBTREE_LONGTEXT N_("You can disable use of Macroblock-tree on ratecontrol")

#define SLICE_COUNT N_("Force number of slices per frame")
#define SLICE_COUNT_LONGTEXT N_("Force rectangular slices and is overridden by other slicing options")

#define SLICE_MAX_SIZE N_("Limit the size of each slice in bytes")
#define SLICE_MAX_SIZE_LONGTEXT N_("Sets a maximum slice size in bytes, Includes NAL overhead in size")

#define SLICE_MAX_MBS N_("Limit the size of each slice in macroblocks")
#define SLICE_MAX_MBS_LONGTEXT N_("Sets a maximum number of macroblocks per slice")
/* Ratecontrol */

#define QP_TEXT N_("Set QP")
#define QP_LONGTEXT N_( "This selects the quantizer to use. " \
    "Lower values result in better fidelity, but higher bitrates. 26 is a " \
    "good default value. Range 0 (lossless) to 51." )

#define CRF_TEXT N_("Quality-based VBR")
#define CRF_LONGTEXT N_( "1-pass Quality-based VBR. Range 0 to 51." )

#define QPMIN_TEXT N_("Min QP")
#define QPMIN_LONGTEXT N_( "Minimum quantizer parameter. 15 to 35 seems to " \
    "be a useful range." )

#define QPMAX_TEXT N_("Max QP")
#define QPMAX_LONGTEXT N_( "Maximum quantizer parameter." )

#define QPSTEP_TEXT N_("Max QP step")
#define QPSTEP_LONGTEXT N_( "Max QP step between frames.")

#define RATETOL_TEXT N_("Average bitrate tolerance")
#define RATETOL_LONGTEXT N_( "Allowed variance in average " \
    "bitrate (in kbits/s).")

#define VBV_MAXRATE_TEXT N_("Max local bitrate")
#define VBV_MAXRATE_LONGTEXT N_( "Sets a maximum local bitrate (in kbits/s).")

#define VBV_BUFSIZE_TEXT N_("VBV buffer")
#define VBV_BUFSIZE_LONGTEXT N_( "Averaging period for the maximum " \
    "local bitrate (in kbits).")

#define VBV_INIT_TEXT N_("Initial VBV buffer occupancy")
#define VBV_INIT_LONGTEXT N_( "Sets the initial buffer occupancy as a " \
    "fraction of the buffer size. Range 0.0 to 1.0.")

#define AQ_MODE_TEXT N_("How AQ distributes bits")
#define AQ_MODE_LONGTEXT N_("Defines bitdistribution mode for AQ, default 1\n" \
        " - 0: Disabled\n"\
        " - 1: Current x264 default mode\n"\
        " - 2: uses log(var)^2 instead of log(var) and attempts to adapt strength per frame")

#define AQ_STRENGTH_TEXT N_("Strength of AQ")
#define AQ_STRENGTH_LONGTEXT N_("Strength to reduce blocking and blurring in flat\n"\
        "and textured areas, default 1.0 recommended to be between 0..2\n"\
        " - 0.5: weak AQ\n"\
        " - 1.5: strong AQ")

/* IP Ratio < 1 is technically valid but should never improve quality */
#define IPRATIO_TEXT N_("QP factor between I and P")
#define IPRATIO_LONGTEXT N_( "QP factor between I and P. Range 1.0 to 2.0.")

/* PB ratio < 1 is not valid and breaks ratecontrol */
#define PBRATIO_TEXT N_("QP factor between P and B")
#define PBRATIO_LONGTEXT N_( "QP factor between P and B. Range 1.0 to 2.0.")

#define CHROMA_QP_OFFSET_TEXT N_("QP difference between chroma and luma")
#define CHROMA_QP_OFFSET_LONGTEXT N_( "QP difference between chroma and luma.")

#define PASS_TEXT N_("Multipass ratecontrol")
#define PASS_LONGTEXT N_( "Multipass ratecontrol:\n" \
    " - 1: First pass, creates stats file\n" \
    " - 2: Last pass, does not overwrite stats file\n" \
    " - 3: Nth pass, overwrites stats file\n" )

#define QCOMP_TEXT N_("QP curve compression")
#define QCOMP_LONGTEXT N_( "QP curve compression. Range 0.0 (CBR) to 1.0 (QCP).")

#define CPLXBLUR_TEXT N_("Reduce fluctuations in QP")
#define CPLXBLUR_LONGTEXT N_( "This reduces the fluctuations in QP " \
    "before curve compression. Temporally blurs complexity.")

#define QBLUR_TEXT N_("Reduce fluctuations in QP")
#define QBLUR_LONGTEXT N_( "This reduces the fluctuations in QP " \
    "after curve compression. Temporally blurs quants.")

/* Analysis */

#define ANALYSE_TEXT N_("Partitions to consider")
#define ANALYSE_LONGTEXT N_( "Partitions to consider in analyse mode:\n" \
    " - none  : \n" \
    " - fast  : i4x4\n" \
    " - normal: i4x4,p8x8,(i8x8)\n" \
    " - slow  : i4x4,p8x8,(i8x8),b8x8\n" \
    " - all   : i4x4,p8x8,(i8x8),b8x8,p4x4\n" \
    "(p4x4 requires p8x8. i8x8 requires 8x8dct).")

#define DIRECT_PRED_TEXT N_("Direct MV prediction mode")
#define DIRECT_PRED_LONGTEXT DIRECT_PRED_TEXT

#define DIRECT_PRED_SIZE_TEXT N_("Direct prediction size")
#define DIRECT_PRED_SIZE_LONGTEXT N_( "Direct prediction size:\n" \
    " -  0: 4x4\n" \
    " -  1: 8x8\n" \
    " - -1: smallest possible according to level\n" )

#define WEIGHTB_TEXT N_("Weighted prediction for B-frames")
#define WEIGHTB_LONGTEXT N_( "Weighted prediction for B-frames.")

#define WEIGHTP_TEXT N_("Weighted prediction for P-frames")
#define WEIGHTP_LONGTEXT N_("Weighted prediction for P-frames:\n" \
    " - 0: Disabled\n"\
    " - 1: Blind offset\n"\
    " - 2: Smart analysis\n" )

#define ME_TEXT N_("Integer pixel motion estimation method")
#define ME_LONGTEXT N_( "Selects the motion estimation algorithm:\n"\
    " - dia: diamond search, radius 1 (fast)\n" \
    " - hex: hexagonal search, radius 2\n" \
    " - umh: uneven multi-hexagon search (better but slower)\n" \
    " - esa: exhaustive search (extremely slow, primarily for testing)\n" \
    " - tesa: hadamard exhaustive search (extremely slow, primarily for testing)\n" )

#define MERANGE_TEXT N_("Maximum motion vector search range")
#define MERANGE_LONGTEXT N_( "Maximum distance to search for " \
    "motion estimation, measured from predicted position(s). " \
    "Default of 16 is good for most footage, high motion sequences may " \
    "benefit from settings between 24 and 32. Range 0 to 64." )

#define MVRANGE_TEXT N_("Maximum motion vector length")
#define MVRANGE_LONGTEXT N_( "Maximum motion vector length in pixels. " \
    "-1 is automatic, based on level." )

#define MVRANGE_THREAD_TEXT N_("Minimum buffer space between threads")
#define MVRANGE_THREAD_LONGTEXT N_( "Minimum buffer space between threads. " \
    "-1 is automatic, based on number of threads." )

#define PSY_RD_TEXT N_( "Strength of psychovisual optimization, default is \"1.0:0.0\"")
#define PSY_RD_LONGTEXT N_( "First parameter controls if RD is on (subme>=6) or off.\n"\
        "Second parameter controls if Trellis is used on psychovisual optimization, " \
        "default off")

#define SUBME_TEXT N_("Subpixel motion estimation and partition decision " \
    "quality")
#define SUBME_LONGTEXT N_( "This parameter controls quality versus speed " \
    "tradeoffs involved in the motion estimation decision process " \
    "(lower = quicker and higher = better quality). Range 1 to 9." )

#define B_RDO_LONGTEXT N_( "RD based mode decision for B-frames. This " \
    "requires subme 6 (or higher).")

#define MIXED_REFS_TEXT N_("Decide references on a per partition basis")
#define MIXED_REFS_LONGTEXT N_( "Allows each 8x8 or 16x8 partition to " \
    "independently select a reference frame, as opposed to only one ref " \
    "per macroblock." )

#define CHROMA_ME_TEXT N_("Chroma in motion estimation")
#define CHROMA_ME_LONGTEXT N_( "Chroma ME for subpel and mode decision in " \
    "P-frames.")

#define BIME_LONGTEXT N_( "Joint bidirectional motion refinement.")

#define TRANSFORM_8X8DCT_TEXT N_("Adaptive spatial transform size")
#define TRANSFORM_8X8DCT_LONGTEXT N_( \
    "SATD-based decision for 8x8 transform in inter-MBs.")

#define TRELLIS_TEXT N_("Trellis RD quantization" )
#define TRELLIS_LONGTEXT N_( "Trellis RD quantization:\n" \
    " - 0: disabled\n" \
    " - 1: enabled only on the final encode of a MB\n" \
    " - 2: enabled on all mode decisions\n" \
    "This requires CABAC." )

#define FAST_PSKIP_TEXT N_("Early SKIP detection on P-frames")
#define FAST_PSKIP_LONGTEXT N_( "Early SKIP detection on P-frames.")

#define DCT_DECIMATE_TEXT N_("Coefficient thresholding on P-frames")
#define DCT_DECIMATE_LONGTEXT N_( "Coefficient thresholding on P-frames. " \
    "Eliminate dct blocks containing only a small single coefficient.")

#define PSY_TEXT N_("Use Psy-optimizations")
#define PSY_LONGTEXT N_("Use all visual optimizations that can worsen both PSNR and SSIM")

/* Noise reduction 1 is too weak to measure, suggest at least 10 */
#define NR_TEXT N_("Noise reduction")
#define NR_LONGTEXT N_( "Dct-domain noise reduction. Adaptive pseudo-deadzone. " \
    "10 to 1000 seems to be a useful range." )

#define DEADZONE_INTER_TEXT N_("Inter luma quantization deadzone")
#define DEADZONE_INTER_LONGTEXT N_( "Set the size of the inter luma quantization deadzone. " \
    "Range 0 to 32." )

#define DEADZONE_INTRA_TEXT N_("Intra luma quantization deadzone")
#define DEADZONE_INTRA_LONGTEXT N_( "Set the size of the intra luma quantization deadzone. " \
    "Range 0 to 32." )

/* Input/Output */

#define NON_DETERMINISTIC_TEXT N_("Non-deterministic optimizations when threaded")
#define NON_DETERMINISTIC_LONGTEXT N_( "Slightly improve quality of SMP, " \
    "at the cost of repeatability.")

#define ASM_TEXT N_("CPU optimizations")
#define ASM_LONGTEXT N_( "Use assembler CPU optimizations.")

#define STATS_TEXT N_("Filename for 2 pass stats file")
#define STATS_LONGTEXT N_( "Filename for 2 pass stats file for multi-pass encoding.")

#define PSNR_TEXT N_("PSNR computation")
#define PSNR_LONGTEXT N_( "Compute and print PSNR stats. This has no effect on " \
    "the actual encoding quality." )

#define SSIM_TEXT N_("SSIM computation")
#define SSIM_LONGTEXT N_( "Compute and print SSIM stats. This has no effect on " \
    "the actual encoding quality." )

#define QUIET_TEXT N_("Quiet mode")

#define VERBOSE_TEXT N_("Statistics")
#define VERBOSE_LONGTEXT N_( "Print stats for each frame.")

#define SPS_ID_TEXT N_("SPS and PPS id numbers")
#define SPS_ID_LONGTEXT N_( "Set SPS and PPS id numbers to allow concatenating " \
    "streams with different settings.")

#define AUD_TEXT N_("Access unit delimiters")
#define AUD_LONGTEXT N_( "Generate access unit delimiter NAL units.")

#define LOOKAHEAD_TEXT N_("Framecount to use on frametype lookahead")
#define LOOKAHEAD_LONGTEXT N_("Framecount to use on frametype lookahead. " \
    "Currently default can cause sync-issues on unmuxable output, like rtsp-output without ts-mux" )

#define HRD_TEXT N_("HRD-timing information")
#define TUNE_TEXT N_("Default tune setting used" )
#define PRESET_TEXT N_("Default preset setting used" )

#define X264_OPTIONS_TEXT N_("x264 advanced options")
#define X264_OPTIONS_LONGTEXT N_("x264 advanced options, in the form {opt=val,op2=val2}.")

static const char *const enc_me_list[] =
  { "dia", "hex", "umh", "esa", "tesa" };
static const char *const enc_me_list_text[] =
  { N_("dia"), N_("hex"), N_("umh"), N_("esa"), N_("tesa") };

static const char *const bpyramid_list[] =
  { "none", "strict", "normal" };

static const char *const enc_analyse_list[] =
  { "none", "fast", "normal", "slow", "all" };
static const char *const enc_analyse_list_text[] =
  { N_("None"), N_("Fast"), N_("Normal"), N_("Slow"), N_("All") };

static const char *const direct_pred_list[] =
  { "none", "spatial", "temporal", "auto" };
static const char *const direct_pred_list_text[] =
  { N_("None"), N_("Spatial"), N_("Temporal"), N_("Auto") };

static const int framepacking_list[] =
  { -1, 0, 1, 2, 3, 4, 5, 6 };
static const char *const framepacking_list_text[] =
  { "", N_("checkerboard"), N_("column alternation"), N_("row alternation"), N_("side by side"), N_("top bottom"), N_("frame alternation"), N_("2D") };

vlc_module_begin ()
#ifdef MODULE_NAME_IS_x26410b
    set_description( N_("H.264/MPEG-4 Part 10/AVC encoder (x264 10-bit)"))
    set_capability( "encoder", 0 )
#endif
#ifdef MODULE_NAME_IS_x262
    set_description( N_("H.262/MPEG-2 encoder (x262)"))
    set_capability( "encoder", 0 )
#endif
#ifdef MODULE_NAME_IS_x264
    set_description( N_("H.264/MPEG-4 Part 10/AVC encoder (x264)"))
    set_capability( "encoder", 200 )
#endif
    set_callbacks( Open, Close )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )

/* Frame-type options */

    add_integer( SOUT_CFG_PREFIX "keyint", 250, KEYINT_TEXT,
                 KEYINT_LONGTEXT, false )

    add_integer( SOUT_CFG_PREFIX "min-keyint", 25, MIN_KEYINT_TEXT,
                 MIN_KEYINT_LONGTEXT, true )

    add_bool( SOUT_CFG_PREFIX "opengop", false, OPENGOP_TEXT,
               OPENGOP_LONGTEXT, true )
    add_bool( SOUT_CFG_PREFIX "bluray-compat", false, BLURAY_TEXT,
               BLURAY_LONGTEXT, true )

    add_integer( SOUT_CFG_PREFIX "scenecut", 40, SCENE_TEXT,
                 SCENE_LONGTEXT, true )
        change_integer_range( -1, 100 )

    add_obsolete_bool( SOUT_CFG_PREFIX "pre-scenecut" )

    add_integer( SOUT_CFG_PREFIX "bframes", 3, BFRAMES_TEXT,
                 BFRAMES_LONGTEXT, true )
        change_integer_range( 0, 16 )

    add_integer( SOUT_CFG_PREFIX "b-adapt", 1, B_ADAPT_TEXT,
                 B_ADAPT_LONGTEXT, true )
        change_integer_range( 0, 2 )

    add_integer( SOUT_CFG_PREFIX "b-bias", 0, B_BIAS_TEXT,
                 B_BIAS_LONGTEXT, true )
        change_integer_range( -100, 100 )

    add_string( SOUT_CFG_PREFIX "bpyramid", "normal", BPYRAMID_TEXT,
              BPYRAMID_LONGTEXT, true )
        change_string_list( bpyramid_list, bpyramid_list )

    add_bool( SOUT_CFG_PREFIX "cabac", true, CABAC_TEXT, CABAC_LONGTEXT,
              true )

    add_bool( SOUT_CFG_PREFIX "fullrange", false, FULLRANGE_TEXT, FULLRANGE_LONGTEXT,
              true )

    add_integer( SOUT_CFG_PREFIX "ref", 3, REF_TEXT,
                 REF_LONGTEXT, true )
        change_integer_range( 1, 16 )

    add_bool( SOUT_CFG_PREFIX "nf", false, NF_TEXT,
              NF_LONGTEXT, true )

    add_string( SOUT_CFG_PREFIX "deblock", "0:0", FILTER_TEXT,
                 FILTER_LONGTEXT, true )

    add_string( SOUT_CFG_PREFIX "psy-rd", "1.0:0.0", PSY_RD_TEXT,
                PSY_RD_LONGTEXT, true )

    add_bool( SOUT_CFG_PREFIX "psy", true, PSY_TEXT, PSY_LONGTEXT, true )

    add_string( SOUT_CFG_PREFIX "level", "0", LEVEL_TEXT,
               LEVEL_LONGTEXT, false )

    add_string( SOUT_CFG_PREFIX "profile", "high", PROFILE_TEXT,
               PROFILE_LONGTEXT, false )
        vlc_config_set (VLC_CONFIG_LIST,
            (sizeof(x264_profile_names) / sizeof (char*)) - 1,
            x264_profile_names, x264_profile_names);


    add_bool( SOUT_CFG_PREFIX "interlaced", false, INTERLACED_TEXT, INTERLACED_LONGTEXT,
              true )

    add_integer( SOUT_CFG_PREFIX "frame-packing", -1, FRAMEPACKING_TEXT, FRAMEPACKING_LONGTEXT, true )
        change_integer_list( framepacking_list, framepacking_list_text )
        change_integer_range( -1, 6)

    add_integer( SOUT_CFG_PREFIX "slices", 0, SLICE_COUNT, SLICE_COUNT_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "slice-max-size", 0, SLICE_MAX_SIZE, SLICE_MAX_SIZE_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "slice-max-mbs", 0, SLICE_MAX_MBS, SLICE_MAX_MBS_LONGTEXT, true )

    add_string( SOUT_CFG_PREFIX "hrd", "none", HRD_TEXT, HRD_TEXT, true )
        vlc_config_set (VLC_CONFIG_LIST,
            (sizeof(x264_nal_hrd_names) / sizeof (char*)) - 1,
            x264_nal_hrd_names, x264_nal_hrd_names);


/* Ratecontrol */

    add_integer( SOUT_CFG_PREFIX "qp", -1, QP_TEXT, QP_LONGTEXT,
                 true )
        change_integer_range( -1, 51 ) /* QP 0 -> lossless encoding */

    add_integer( SOUT_CFG_PREFIX "crf", 23, CRF_TEXT,
                 CRF_LONGTEXT, true )
        change_integer_range( 0, 51 )

    add_integer( SOUT_CFG_PREFIX "qpmin", 10, QPMIN_TEXT,
                 QPMIN_LONGTEXT, true )
        change_integer_range( 0, 51 )

    add_integer( SOUT_CFG_PREFIX "qpmax", 51, QPMAX_TEXT,
                 QPMAX_LONGTEXT, true )
        change_integer_range( 0, 51 )

    add_integer( SOUT_CFG_PREFIX "qpstep", 4, QPSTEP_TEXT,
                 QPSTEP_LONGTEXT, true )
        change_integer_range( 0, 51 )

    add_float( SOUT_CFG_PREFIX "ratetol", 1.0, RATETOL_TEXT,
               RATETOL_LONGTEXT, true )
        change_float_range( 0, 100 )

    add_integer( SOUT_CFG_PREFIX "vbv-maxrate", 0, VBV_MAXRATE_TEXT,
                 VBV_MAXRATE_LONGTEXT, true )

    add_integer( SOUT_CFG_PREFIX "vbv-bufsize", 0, VBV_BUFSIZE_TEXT,
                 VBV_BUFSIZE_LONGTEXT, true )

    add_float( SOUT_CFG_PREFIX "vbv-init", 0.9, VBV_INIT_TEXT,
               VBV_INIT_LONGTEXT, true )
        change_float_range( 0, 1 )

    add_float( SOUT_CFG_PREFIX "ipratio", 1.40, IPRATIO_TEXT,
               IPRATIO_LONGTEXT, true )
        change_float_range( 1, 2 )

    add_float( SOUT_CFG_PREFIX "pbratio", 1.30, PBRATIO_TEXT,
               PBRATIO_LONGTEXT, true )
        change_float_range( 1, 2 )

    add_integer( SOUT_CFG_PREFIX "chroma-qp-offset", 0, CHROMA_QP_OFFSET_TEXT,
                 CHROMA_QP_OFFSET_LONGTEXT, true )

    add_integer( SOUT_CFG_PREFIX "pass", 0, PASS_TEXT,
                 PASS_LONGTEXT, false )
        change_integer_range( 0, 3 )

    add_float( SOUT_CFG_PREFIX "qcomp", 0.60, QCOMP_TEXT,
               QCOMP_LONGTEXT, true )
        change_float_range( 0, 1 )

    add_float( SOUT_CFG_PREFIX "cplxblur", 20.0, CPLXBLUR_TEXT,
               CPLXBLUR_LONGTEXT, true )

    add_float( SOUT_CFG_PREFIX "qblur", 0.5, QBLUR_TEXT,
               QBLUR_LONGTEXT, true )

    add_integer( SOUT_CFG_PREFIX "aq-mode", X264_AQ_VARIANCE, AQ_MODE_TEXT,
                 AQ_MODE_LONGTEXT, true )
         change_integer_range( 0, 2 )
    add_float( SOUT_CFG_PREFIX "aq-strength", 1.0, AQ_STRENGTH_TEXT,
               AQ_STRENGTH_LONGTEXT, true )

/* Analysis */

    /* x264 partitions = none (default). set at least "normal" mode. */
    add_string( SOUT_CFG_PREFIX "partitions", "normal", ANALYSE_TEXT,
                ANALYSE_LONGTEXT, true )
        change_string_list( enc_analyse_list, enc_analyse_list_text )

    add_string( SOUT_CFG_PREFIX "direct", "spatial", DIRECT_PRED_TEXT,
                DIRECT_PRED_LONGTEXT, true )
        change_string_list( direct_pred_list, direct_pred_list_text )

    add_integer( SOUT_CFG_PREFIX "direct-8x8", 1, DIRECT_PRED_SIZE_TEXT,
                 DIRECT_PRED_SIZE_LONGTEXT, true )
        change_integer_range( -1, 1 )

    add_bool( SOUT_CFG_PREFIX "weightb", true, WEIGHTB_TEXT,
              WEIGHTB_LONGTEXT, true )

    add_integer( SOUT_CFG_PREFIX "weightp", 2, WEIGHTP_TEXT,
              WEIGHTP_LONGTEXT, true )
        change_integer_range( 0, 2 )

    add_string( SOUT_CFG_PREFIX "me", "hex", ME_TEXT,
                ME_LONGTEXT, true )
        change_string_list( enc_me_list, enc_me_list_text )

    add_integer( SOUT_CFG_PREFIX "merange", 16, MERANGE_TEXT,
                 MERANGE_LONGTEXT, true )
        change_integer_range( 1, 64 )

    add_integer( SOUT_CFG_PREFIX "mvrange", -1, MVRANGE_TEXT,
                 MVRANGE_LONGTEXT, true )

    add_integer( SOUT_CFG_PREFIX "mvrange-thread", -1, MVRANGE_THREAD_TEXT,
                 MVRANGE_THREAD_LONGTEXT, true )

    add_integer( SOUT_CFG_PREFIX "subme", 7, SUBME_TEXT,
                 SUBME_LONGTEXT, true )

    add_obsolete_bool( SOUT_CFG_PREFIX "b-rdo" )

    add_bool( SOUT_CFG_PREFIX "mixed-refs", true, MIXED_REFS_TEXT,
              MIXED_REFS_LONGTEXT, true )

    add_bool( SOUT_CFG_PREFIX "chroma-me", true, CHROMA_ME_TEXT,
              CHROMA_ME_LONGTEXT, true )

    add_obsolete_bool( SOUT_CFG_PREFIX "bime" )

    add_bool( SOUT_CFG_PREFIX "8x8dct", true, TRANSFORM_8X8DCT_TEXT,
              TRANSFORM_8X8DCT_LONGTEXT, true )

    add_integer( SOUT_CFG_PREFIX "trellis", 1, TRELLIS_TEXT,
                 TRELLIS_LONGTEXT, true )
        change_integer_range( 0, 2 )

    add_integer( SOUT_CFG_PREFIX "lookahead", 40, LOOKAHEAD_TEXT,
                 LOOKAHEAD_LONGTEXT, true )
        change_integer_range( 0, 60 )

    add_bool( SOUT_CFG_PREFIX "intra-refresh", false, INTRAREFRESH_TEXT,
              INTRAREFRESH_LONGTEXT, true )

    add_bool( SOUT_CFG_PREFIX "mbtree", true, MBTREE_TEXT, MBTREE_LONGTEXT, true )

    add_bool( SOUT_CFG_PREFIX "fast-pskip", true, FAST_PSKIP_TEXT,
              FAST_PSKIP_LONGTEXT, true )

    add_bool( SOUT_CFG_PREFIX "dct-decimate", true, DCT_DECIMATE_TEXT,
              DCT_DECIMATE_LONGTEXT, true )

    add_integer( SOUT_CFG_PREFIX "nr", 0, NR_TEXT,
                 NR_LONGTEXT, true )
        change_integer_range( 0, 1000 )

    add_integer( SOUT_CFG_PREFIX "deadzone-inter", 21, DEADZONE_INTER_TEXT,
                 DEADZONE_INTRA_LONGTEXT, true )
        change_integer_range( 0, 32 )

    add_integer( SOUT_CFG_PREFIX "deadzone-intra", 11, DEADZONE_INTRA_TEXT,
                 DEADZONE_INTRA_LONGTEXT, true )
        change_integer_range( 0, 32 )

/* Input/Output */

    add_bool( SOUT_CFG_PREFIX "non-deterministic", false, NON_DETERMINISTIC_TEXT,
              NON_DETERMINISTIC_LONGTEXT, true )

    add_bool( SOUT_CFG_PREFIX "asm", true, ASM_TEXT,
              ASM_LONGTEXT, true )

    /* x264 psnr = 1 (default). disable PSNR computation for speed. */
    add_bool( SOUT_CFG_PREFIX "psnr", false, PSNR_TEXT,
              PSNR_LONGTEXT, true )

    /* x264 ssim = 1 (default). disable SSIM computation for speed. */
    add_bool( SOUT_CFG_PREFIX "ssim", false, SSIM_TEXT,
              SSIM_LONGTEXT, true )

    add_bool( SOUT_CFG_PREFIX "quiet", false, QUIET_TEXT,
              QUIET_TEXT, true )

    add_integer( SOUT_CFG_PREFIX "sps-id", 0, SPS_ID_TEXT,
                 SPS_ID_LONGTEXT, true )

    add_bool( SOUT_CFG_PREFIX "aud", false, AUD_TEXT,
              AUD_LONGTEXT, true )

    add_bool( SOUT_CFG_PREFIX "verbose", false, VERBOSE_TEXT,
              VERBOSE_LONGTEXT, true )

    add_string( SOUT_CFG_PREFIX "stats", "x264_2pass.log", STATS_TEXT,
                STATS_LONGTEXT, true )

    add_string( SOUT_CFG_PREFIX "preset", NULL , PRESET_TEXT , PRESET_TEXT, false )
        vlc_config_set (VLC_CONFIG_LIST,
            (sizeof(x264_preset_names) / sizeof (char*)) - 1,
            x264_preset_names, x264_preset_names);
    add_string( SOUT_CFG_PREFIX "tune", NULL , TUNE_TEXT, TUNE_TEXT, false )
        vlc_config_set (VLC_CONFIG_LIST,
            (sizeof(x264_tune_names) / sizeof (char*)) - 1,
            x264_tune_names, x264_tune_names);

    add_string( SOUT_CFG_PREFIX "options", NULL, X264_OPTIONS_TEXT,
                X264_OPTIONS_LONGTEXT, true )

vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "8x8dct", "asm", "aud", "bframes", "bime", "bpyramid",
    "b-adapt", "b-bias", "b-rdo", "cabac", "chroma-me", "chroma-qp-offset",
    "cplxblur", "crf", "dct-decimate", "deadzone-inter", "deadzone-intra",
    "deblock", "direct", "direct-8x8", "fast-pskip",
    "interlaced", "ipratio", "keyint", "level",
    "me", "merange", "min-keyint", "mixed-refs", "mvrange", "mvrange-thread",
    "nf", "non-deterministic", "nr", "partitions", "pass", "pbratio",
    "pre-scenecut", "psnr", "qblur", "qp", "qcomp", "qpstep", "qpmax",
    "qpmin", "quiet", "ratetol", "ref", "scenecut",
    "sps-id", "ssim", "stats", "subme", "trellis",
    "verbose", "vbv-bufsize", "vbv-init", "vbv-maxrate", "weightb", "weightp",
    "aq-mode", "aq-strength", "psy-rd", "psy", "profile", "lookahead", "slices",
    "slice-max-size", "slice-max-mbs", "intra-refresh", "mbtree", "hrd",
    "tune","preset", "opengop", "bluray-compat", "frame-packing", "options",
    "fullrange",
    NULL
};

static block_t *Encode( encoder_t *, picture_t * );

typedef struct
{
    x264_t          *h;
    x264_param_t    param;

    char            *psz_stat_name;
    int             i_sei_size;
    uint32_t         i_colorspace;
    uint8_t         *p_sei;
} encoder_sys_t;

#ifdef PTW32_STATIC_LIB
static vlc_mutex_t pthread_win32_mutex = VLC_STATIC_MUTEX;
static int pthread_win32_count = 0;
#endif

/*****************************************************************************
 * Open: probe the encoder
 *****************************************************************************/
static int  Open ( vlc_object_t *p_this )
{
    encoder_t     *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;
    int i_val;
    char *psz_val;
    int i_qmin = 0, i_qmax = 0;
    x264_nal_t    *nal;
    int i, i_nal;
    bool fullrange = false;

#ifdef MODULE_NAME_IS_x262
    if( p_enc->fmt_out.i_codec != VLC_CODEC_MP2V &&
#else
    if( p_enc->fmt_out.i_codec != VLC_CODEC_H264 &&
#endif
        !p_enc->obj.force )
    {
        return VLC_EGENERIC;
    }
    /* X264_POINTVER or X264_VERSION are not available */
#ifdef MODULE_NAME_IS_x262
    msg_Dbg ( p_enc, "version x262 0.%d.X", X264_BUILD );
#else
    msg_Dbg ( p_enc, "version x264 0.%d.X", X264_BUILD );
#endif

    config_ChainParse( p_enc, SOUT_CFG_PREFIX, ppsz_sout_options, p_enc->p_cfg );

    p_enc->fmt_out.i_cat = VIDEO_ES;
#ifdef MODULE_NAME_IS_x262
    p_enc->fmt_out.i_codec = VLC_CODEC_MP2V;
#else
    p_enc->fmt_out.i_codec = VLC_CODEC_H264;
#endif
    p_enc->p_sys = p_sys = vlc_obj_malloc( p_this, sizeof( encoder_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    fullrange = var_GetBool( p_enc, SOUT_CFG_PREFIX "fullrange" );
    fullrange |= p_enc->fmt_in.video.color_range == COLOR_RANGE_FULL;
    p_enc->fmt_in.i_codec = fullrange ? VLC_CODEC_J420 : VLC_CODEC_I420;
    p_sys->i_colorspace = X264_CSP_I420;
    char *psz_profile = var_GetString( p_enc, SOUT_CFG_PREFIX "profile" );
# ifdef MODULE_NAME_IS_x26410b
    const int mask = X264_CSP_HIGH_DEPTH;
# else
    const int mask = 0;
# endif
    if( psz_profile )
    {

        if( !strcmp( psz_profile, "high10" ) )
        {
            p_enc->fmt_in.i_codec = mask ? VLC_CODEC_I420_10L : fullrange ? VLC_CODEC_J420 : VLC_CODEC_I420;
            p_sys->i_colorspace = X264_CSP_I420 | mask;
        }
        else if( !strcmp( psz_profile, "high422" ) )
        {
            p_enc->fmt_in.i_codec = mask ? VLC_CODEC_I422_10L : fullrange ? VLC_CODEC_J422 : VLC_CODEC_I422;
            p_sys->i_colorspace = X264_CSP_I422 | mask;
        }
        else if( !strcmp( psz_profile, "high444" ) )
        {
            p_enc->fmt_in.i_codec = mask ? VLC_CODEC_I444_10L : fullrange ? VLC_CODEC_J444 : VLC_CODEC_I444;
            p_sys->i_colorspace = X264_CSP_I444 | mask;
        }
# ifdef MODULE_NAME_IS_x26410b
        else
        {
            msg_Err( p_enc, "Only high-profiles and 10-bit are supported");
            free( psz_profile );
            return VLC_EGENERIC;
        }
# endif
    }
# ifdef MODULE_NAME_IS_x26410b
    else
    {
        msg_Err( p_enc, "Only high-profiles and 10-bit are supported");
        return VLC_EGENERIC;
    }
# endif
    free( psz_profile );

    p_enc->pf_encode_video = Encode;
    p_enc->pf_encode_audio = NULL;
    p_sys->psz_stat_name = NULL;
    p_sys->i_sei_size = 0;
    p_sys->p_sei = NULL;

    char *psz_preset = var_GetString( p_enc, SOUT_CFG_PREFIX  "preset" );
    char *psz_tune = var_GetString( p_enc, SOUT_CFG_PREFIX  "tune" );
    if( *psz_preset == '\0' )
    {
        free(psz_preset);
        psz_preset = NULL;
    }
#ifdef MODULE_NAME_IS_x262
    p_sys->param.b_mpeg2 = true;
    x264_param_default_mpeg2( &p_sys->param );
    x264_param_default_preset_mpeg2( &p_sys->param, psz_preset, psz_tune );
#else
    x264_param_default( &p_sys->param );
    x264_param_default_preset( &p_sys->param, psz_preset, psz_tune );
# if X264_BUILD > 152
    if( mask )
        p_sys->param.i_bitdepth = 10;
# endif
#endif
    free( psz_preset );
    free( psz_tune );
    p_sys->param.i_csp = p_sys->i_colorspace;
    p_sys->param.i_width  = p_enc->fmt_in.video.i_visible_width;
    p_sys->param.i_height = p_enc->fmt_in.video.i_visible_height;
    p_sys->param.vui.b_fullrange = fullrange;

    switch( p_enc->fmt_in.video.space )
    {
        case COLOR_SPACE_BT601:
            p_sys->param.vui.i_colmatrix = 5; /* bt470bg*/
            break;
        case COLOR_SPACE_BT709:
            p_sys->param.vui.i_colmatrix = 1; /* bt709*/
            break;
        case COLOR_SPACE_BT2020:
            p_sys->param.vui.i_colmatrix = 10; /* bt2020c*/
            break;
        default:
            break;
    }

    switch( p_enc->fmt_in.video.transfer )
    {
        case TRANSFER_FUNC_LINEAR:
            p_sys->param.vui.i_transfer = 7; /* linear*/
            break;
        case TRANSFER_FUNC_SRGB:
        case TRANSFER_FUNC_BT709:
            p_sys->param.vui.i_transfer = 1; /* bt709*/
            break;
        default:
            break;
    }

    switch( p_enc->fmt_in.video.primaries )
    {
        case COLOR_PRIMARIES_BT601_625:
            p_sys->param.vui.i_colorprim = 5; /* BT470BG */
            break;
        case COLOR_PRIMARIES_BT601_525:
            p_sys->param.vui.i_colorprim = 6; /* SMPTE170M */
            break;
        case COLOR_PRIMARIES_BT709:
            p_sys->param.vui.i_colorprim = 1; /* BT.709 */
            break;
        case COLOR_PRIMARIES_BT2020:
            p_sys->param.vui.i_colorprim = 9; /* BT.2020 */
            break;
        default:
            break;
    }


    if( fabs(var_GetFloat( p_enc, SOUT_CFG_PREFIX "qcomp" ) - 0.60) > 0.005 )
       p_sys->param.rc.f_qcompress = var_GetFloat( p_enc, SOUT_CFG_PREFIX "qcomp" );

    /* transcode-default bitrate is 0,
     * set more to ABR if user specifies bitrate */
    if( p_enc->fmt_out.i_bitrate > 0 )
    {
        p_sys->param.rc.i_bitrate = p_enc->fmt_out.i_bitrate / 1000;
        p_sys->param.rc.i_rc_method = X264_RC_ABR;
    }
    else /* Set default to CRF */
    {
        i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "crf" );
        if( i_val > 0 && i_val <= 51 )
        {
            p_sys->param.rc.f_rf_constant = i_val;
            p_sys->param.rc.i_rc_method = X264_RC_CRF;
        }
    }

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "qpstep" );
    if( i_val >= 0 && i_val <= 51 ) p_sys->param.rc.i_qp_step = i_val;

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "qpmin" );
    if( i_val >= 0 && i_val <= 51 )
    {
        i_qmin = i_val;
        p_sys->param.rc.i_qp_min = i_qmin;
    }
    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "qpmax" );
    if( i_val >= 0 && i_val <= 51 )
    {
        i_qmax = i_val;
        p_sys->param.rc.i_qp_max = i_qmax;
    }

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "qp" );
    if( i_val >= 0 && i_val <= 51 )
    {
        if( i_qmin > i_val ) i_qmin = i_val;
        if( i_qmax < i_val ) i_qmax = i_val;

        /* User defined QP-value, so change ratecontrol method */
        p_sys->param.rc.i_rc_method = X264_RC_CQP;
        p_sys->param.rc.i_qp_constant = i_val;
        p_sys->param.rc.i_qp_min = i_qmin;
        p_sys->param.rc.i_qp_max = i_qmax;
    }


    p_sys->param.rc.f_rate_tolerance = var_GetFloat( p_enc,
                            SOUT_CFG_PREFIX "ratetol" );
    p_sys->param.rc.f_vbv_buffer_init = var_GetFloat( p_enc,
                            SOUT_CFG_PREFIX "vbv-init" );
    p_sys->param.rc.i_vbv_buffer_size = var_GetInteger( p_enc,
                            SOUT_CFG_PREFIX "vbv-bufsize" );

    /* max bitrate = average bitrate -> CBR */
    p_sys->param.rc.i_vbv_max_bitrate = var_GetInteger( p_enc, SOUT_CFG_PREFIX "vbv-maxrate" );


    if( !var_GetBool( p_enc, SOUT_CFG_PREFIX "mbtree" ) )
       p_sys->param.rc.b_mb_tree = var_GetBool( p_enc, SOUT_CFG_PREFIX "mbtree" );

    if( !var_GetBool( p_enc, SOUT_CFG_PREFIX "cabac" ) )
        p_sys->param.b_cabac = var_GetBool( p_enc, SOUT_CFG_PREFIX "cabac" );

    /* disable deblocking when nf (no loop filter) is enabled */
    if( var_GetBool( p_enc, SOUT_CFG_PREFIX "nf" ) )
       p_sys->param.b_deblocking_filter = !var_GetBool( p_enc, SOUT_CFG_PREFIX "nf" );

    psz_val = var_GetString( p_enc, SOUT_CFG_PREFIX "deblock" );
    if( psz_val )
    {
        if( atoi( psz_val ) != 0 )
        {
           char *p = strchr( psz_val, ':' );
           p_sys->param.i_deblocking_filter_alphac0 = atoi( psz_val );
           p_sys->param.i_deblocking_filter_beta = p ?
                    atoi( p+1 ) : p_sys->param.i_deblocking_filter_alphac0;
        }
        free( psz_val );
    }

    psz_val = var_GetString( p_enc, SOUT_CFG_PREFIX "psy-rd" );
    if( psz_val )
    {
        if( us_atof( psz_val ) != 1.0 )
        {
           char *p = strchr( psz_val, ':' );
           p_sys->param.analyse.f_psy_rd = us_atof( psz_val );
           p_sys->param.analyse.f_psy_trellis = p ? us_atof( p+1 ) : 0;
        }
        free( psz_val );
    }

    if( !var_GetBool( p_enc, SOUT_CFG_PREFIX "psy" ) )
       p_sys->param.analyse.b_psy = var_GetBool( p_enc, SOUT_CFG_PREFIX "psy" );

    psz_val = var_GetString( p_enc, SOUT_CFG_PREFIX "level" );
    if( psz_val )
    {
        if( us_atof (psz_val) < 6 && us_atof (psz_val) > 0 )
            p_sys->param.i_level_idc = (int) (10 * us_atof (psz_val)
                                              + .5);
        else if( atoi(psz_val) >= 10 && atoi(psz_val) <= 51 )
            p_sys->param.i_level_idc = atoi (psz_val);
        free( psz_val );
    }

    p_sys->param.b_interlaced = var_GetBool( p_enc, SOUT_CFG_PREFIX "interlaced" );
    if( fabs(var_GetFloat( p_enc, SOUT_CFG_PREFIX "ipratio" ) - 1.4) > 0.005 )
       p_sys->param.rc.f_ip_factor = var_GetFloat( p_enc, SOUT_CFG_PREFIX "ipratio" );
    if( fabs(var_GetFloat( p_enc, SOUT_CFG_PREFIX "pbratio" ) - 1.3 ) > 0.005 )
       p_sys->param.rc.f_pb_factor = var_GetFloat( p_enc, SOUT_CFG_PREFIX "pbratio" );
    p_sys->param.rc.f_complexity_blur = var_GetFloat( p_enc, SOUT_CFG_PREFIX "cplxblur" );
    p_sys->param.rc.f_qblur = var_GetFloat( p_enc, SOUT_CFG_PREFIX "qblur" );
    if( var_GetInteger( p_enc, SOUT_CFG_PREFIX "aq-mode" ) != X264_AQ_VARIANCE )
       p_sys->param.rc.i_aq_mode = var_GetInteger( p_enc, SOUT_CFG_PREFIX "aq-mode" );
    if( fabs( var_GetFloat( p_enc, SOUT_CFG_PREFIX "aq-strength" ) - 1.0) > 0.005 )
       p_sys->param.rc.f_aq_strength = var_GetFloat( p_enc, SOUT_CFG_PREFIX "aq-strength" );

    if( var_GetInteger( p_enc, SOUT_CFG_PREFIX "frame-packing" ) > -1 )
       p_sys->param.i_frame_packing = var_GetInteger( p_enc, SOUT_CFG_PREFIX "frame-packing" );

    if( var_GetBool( p_enc, SOUT_CFG_PREFIX "verbose" ) )
        p_sys->param.i_log_level = X264_LOG_DEBUG;

    if( var_GetBool( p_enc, SOUT_CFG_PREFIX "quiet" ) )
        p_sys->param.i_log_level = X264_LOG_NONE;

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "sps-id" );
    if( i_val >= 0 ) p_sys->param.i_sps_id = i_val;

    if( var_GetBool( p_enc, SOUT_CFG_PREFIX "aud" ) )
        p_sys->param.b_aud = true;

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "keyint" );
    if( i_val > 0 && i_val != 250 ) p_sys->param.i_keyint_max = i_val;
    if( i_val == -1 ) p_sys->param.i_keyint_max = X264_KEYINT_MAX_INFINITE;

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "min-keyint" );
    if( i_val > 0 && i_val != 25 ) p_sys->param.i_keyint_min = i_val;

    p_sys->param.b_open_gop = var_GetBool( p_enc, SOUT_CFG_PREFIX "opengop" );
    p_sys->param.b_bluray_compat = var_GetBool( p_enc, SOUT_CFG_PREFIX "bluray-compat" );

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "bframes" );
    if( i_val >= 0 && i_val <= 16 && i_val != 3 )
        p_sys->param.i_bframe = i_val;

    p_sys->param.b_intra_refresh = var_GetBool( p_enc, SOUT_CFG_PREFIX "intra-refresh" );

    psz_val = var_GetString( p_enc, SOUT_CFG_PREFIX "bpyramid" );
    if( !strcmp( psz_val, "normal" ) )
    {
        p_sys->param.i_bframe_pyramid = X264_B_PYRAMID_NORMAL;
    }
    else if ( !strcmp( psz_val, "strict" ) )
    {
        p_sys->param.i_bframe_pyramid = X264_B_PYRAMID_STRICT;
    }
    else if ( !strcmp( psz_val, "none" ) )
    {
        p_sys->param.i_bframe_pyramid = X264_B_PYRAMID_NONE;
    }

    free( psz_val );

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "ref" );
    if( i_val > 0 && i_val <= 15 && i_val != 3 )
        p_sys->param.i_frame_reference = i_val;

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "scenecut" );
    if( i_val >= -1 && i_val <= 100 && i_val != 40 )
        p_sys->param.i_scenecut_threshold = i_val;

    p_sys->param.b_deterministic = var_GetBool( p_enc,
                        SOUT_CFG_PREFIX "non-deterministic" );

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "subme" );
    if( i_val >= 1 && i_val != 7 )
        p_sys->param.analyse.i_subpel_refine = i_val;

    psz_val = var_GetString( p_enc, SOUT_CFG_PREFIX "hrd");
    if( !strcmp( psz_val, "vbr" ) )
        p_sys->param.i_nal_hrd = X264_NAL_HRD_VBR;
    else if( !strcmp( psz_val, "cbr" ) )
        p_sys->param.i_nal_hrd = X264_NAL_HRD_CBR;
    free( psz_val );

    //TODO: psz_val == NULL ?
    psz_val = var_GetString( p_enc, SOUT_CFG_PREFIX "me" );
    if( psz_val && strcmp( psz_val, "hex" ) )
    {
       if( !strcmp( psz_val, "dia" ) )
       {
            p_sys->param.analyse.i_me_method = X264_ME_DIA;
       }
       else if( !strcmp( psz_val, "umh" ) )
       {
            p_sys->param.analyse.i_me_method = X264_ME_UMH;
        }
       else if( !strcmp( psz_val, "esa" ) )
       {
           p_sys->param.analyse.i_me_method = X264_ME_ESA;
       }
       else if( !strcmp( psz_val, "tesa" ) )
       {
           p_sys->param.analyse.i_me_method = X264_ME_TESA;
       }
    }
    free( psz_val );

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "merange" );
    if( i_val >= 0 && i_val <= 64 && i_val != 16 )
        p_sys->param.analyse.i_me_range = i_val;

    p_sys->param.analyse.i_mv_range = var_GetInteger( p_enc,
                                    SOUT_CFG_PREFIX "mvrange" );
    p_sys->param.analyse.i_mv_range_thread = var_GetInteger( p_enc,
                                    SOUT_CFG_PREFIX "mvrange-thread" );

    psz_val = var_GetString( p_enc, SOUT_CFG_PREFIX "direct" );
    if( !strcmp( psz_val, "none" ) )
    {
        p_sys->param.analyse.i_direct_mv_pred = X264_DIRECT_PRED_NONE;
    }
    else if( !strcmp( psz_val, "spatial" ) )
    {
        p_sys->param.analyse.i_direct_mv_pred = X264_DIRECT_PRED_SPATIAL;
    }
    else if( !strcmp( psz_val, "temporal" ) )
    {
        p_sys->param.analyse.i_direct_mv_pred = X264_DIRECT_PRED_TEMPORAL;
    }
    else if( !strcmp( psz_val, "auto" ) )
    {
        p_sys->param.analyse.i_direct_mv_pred = X264_DIRECT_PRED_AUTO;
    }
    free( psz_val );

    p_sys->param.analyse.b_psnr = var_GetBool( p_enc, SOUT_CFG_PREFIX "psnr" );
    p_sys->param.analyse.b_ssim = var_GetBool( p_enc, SOUT_CFG_PREFIX "ssim" );
    if( !var_GetBool( p_enc, SOUT_CFG_PREFIX "weightb" ) )
       p_sys->param.analyse.b_weighted_bipred = var_GetBool( p_enc,
                                    SOUT_CFG_PREFIX "weightb" );
    if( var_GetInteger( p_enc, SOUT_CFG_PREFIX "weightp" ) != 2 )
       p_sys->param.analyse.i_weighted_pred = var_GetInteger( p_enc, SOUT_CFG_PREFIX "weightp" );

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "b-adapt" );
    if( i_val != 1 )
       p_sys->param.i_bframe_adaptive = i_val;

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "b-bias" );
    if( i_val >= -100 && i_val <= 100 && i_val != 0)
        p_sys->param.i_bframe_bias = i_val;

    p_sys->param.analyse.b_chroma_me = var_GetBool( p_enc,
                                    SOUT_CFG_PREFIX "chroma-me" );
    p_sys->param.analyse.i_chroma_qp_offset = var_GetInteger( p_enc,
                                    SOUT_CFG_PREFIX "chroma-qp-offset" );
    if( !var_GetBool( p_enc, SOUT_CFG_PREFIX "mixed-refs" ) )
       p_sys->param.analyse.b_mixed_references = var_GetBool( p_enc,
                                    SOUT_CFG_PREFIX "mixed-refs" );

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "trellis" );
    if( i_val >= 0 && i_val <= 2 && i_val != 1 )
        p_sys->param.analyse.i_trellis = i_val;

    if( !var_GetBool( p_enc, SOUT_CFG_PREFIX "fast-pskip" ) )
       p_sys->param.analyse.b_fast_pskip = var_GetBool( p_enc,
                                    SOUT_CFG_PREFIX "fast-pskip" );

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "nr" );
    if( i_val > 0 && i_val <= 1000 )
        p_sys->param.analyse.i_noise_reduction = i_val;

    if( !var_GetBool( p_enc, SOUT_CFG_PREFIX "dct-decimate" ) )
       p_sys->param.analyse.b_dct_decimate = var_GetBool( p_enc,
                                    SOUT_CFG_PREFIX "dct-decimate" );

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "deadzone-inter" );
    if( i_val >= 0 && i_val <= 32 && i_val != 21 )
        p_sys->param.analyse.i_luma_deadzone[0] = i_val;

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "deadzone-intra" );
    if( i_val >= 0 && i_val <= 32 && i_val != 11)
        p_sys->param.analyse.i_luma_deadzone[1] = i_val;

    if( !var_GetBool( p_enc, SOUT_CFG_PREFIX "asm" ) )
        p_sys->param.cpu = 0;

#ifndef X264_ANALYSE_BSUB16x16
#   define X264_ANALYSE_BSUB16x16 0
#endif
    psz_val = var_GetString( p_enc, SOUT_CFG_PREFIX "partitions" );
    if( !strcmp( psz_val, "none" ) )
    {
        p_sys->param.analyse.inter = 0;
    }
    else if( !strcmp( psz_val, "fast" ) )
    {
        p_sys->param.analyse.inter = X264_ANALYSE_I4x4;
    }
    else if( !strcmp( psz_val, "normal" ) )
    {
        p_sys->param.analyse.inter =
            X264_ANALYSE_I4x4 |
            X264_ANALYSE_PSUB16x16;
#ifdef X264_ANALYSE_I8x8
        p_sys->param.analyse.inter |= X264_ANALYSE_I8x8;
#endif
    }
    else if( !strcmp( psz_val, "slow" ) )
    {
        p_sys->param.analyse.inter =
            X264_ANALYSE_I4x4 |
            X264_ANALYSE_PSUB16x16 |
            X264_ANALYSE_BSUB16x16;
#ifdef X264_ANALYSE_I8x8
        p_sys->param.analyse.inter |= X264_ANALYSE_I8x8;
#endif
    }
    else if( !strcmp( psz_val, "all" ) )
    {
        p_sys->param.analyse.inter = ~0;
    }
    free( psz_val );

    if( !var_GetBool( p_enc, SOUT_CFG_PREFIX "8x8dct" ) )
       p_sys->param.analyse.b_transform_8x8 = var_GetBool( p_enc,
                                    SOUT_CFG_PREFIX "8x8dct" );

    if( p_enc->fmt_in.video.i_sar_num > 0 &&
        p_enc->fmt_in.video.i_sar_den > 0 )
    {
        unsigned int i_dst_num, i_dst_den;
        vlc_ureduce( &i_dst_num, &i_dst_den,
                     p_enc->fmt_in.video.i_sar_num,
                     p_enc->fmt_in.video.i_sar_den, 0 );
        p_sys->param.vui.i_sar_width = i_dst_num;
        p_sys->param.vui.i_sar_height = i_dst_den;
    }

    p_sys->param.i_timebase_num = 1;
    p_sys->param.i_timebase_den = CLOCK_FREQ;

    if( p_enc->fmt_in.video.i_frame_rate_base > 0 )
    {
        p_sys->param.i_fps_num = p_enc->fmt_in.video.i_frame_rate;
        p_sys->param.i_fps_den = p_enc->fmt_in.video.i_frame_rate_base;
        p_sys->param.b_vfr_input = 0;
    }

    /* Check slice-options */
    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "slices" );
    if( i_val > 0 )
        p_sys->param.i_slice_count = i_val;
    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "slice-max-size" );
    if( i_val > 0 )
        p_sys->param.i_slice_max_size = i_val;
    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "slice-max-mbs" );
    if( i_val > 0 )
        p_sys->param.i_slice_max_mbs = i_val;

    /* Check if user has given some profile (baseline,main,high) to limit
     * settings, and apply those*/
    psz_val = var_GetString( p_enc, SOUT_CFG_PREFIX "profile" );
    if( psz_val && *psz_val )
        x264_param_apply_profile( &p_sys->param, psz_val );
    free( psz_val );

#if defined (__i386__) || defined (__x86_64__)
    if( !vlc_CPU_MMX() )
        p_sys->param.cpu &= ~X264_CPU_MMX;
    if( !vlc_CPU_MMXEXT() )
        p_sys->param.cpu &= ~X264_CPU_MMXEXT;
    if( !vlc_CPU_SSE() )
        p_sys->param.cpu &= ~X264_CPU_SSE;
    if( !vlc_CPU_SSE2() )
        p_sys->param.cpu &= ~X264_CPU_SSE2;
#endif

    /* BUILD 29 adds support for multi-threaded encoding while BUILD 49 (r543)
       also adds support for threads = 0 for automatically selecting an optimal
       value (cores * 1.5) based on detected CPUs. Default behavior for x264 is
       threads = 1, however VLC usage differs and uses threads = 0 (auto) by
       default unless ofcourse transcode threads is explicitly specified.. */
    p_sys->param.i_threads = p_enc->i_threads;

    psz_val = var_GetString( p_enc, SOUT_CFG_PREFIX "stats" );
    if( psz_val )
    {
        p_sys->param.rc.psz_stat_in  =
        p_sys->param.rc.psz_stat_out =
        p_sys->psz_stat_name         = psz_val;
    }

    i_val = var_GetInteger( p_enc, SOUT_CFG_PREFIX "pass" );
    if( i_val > 0 && i_val <= 3 )
    {
        p_sys->param.rc.b_stat_write = i_val & 1;
        p_sys->param.rc.b_stat_read = i_val & 2;
    }

    p_sys->param.pf_log = x264_log;
    p_sys->param.p_log_private = p_enc;
    /* We need to initialize pthreadw32 before we open the encoder,
       but only once for the whole application. Since pthreadw32
       doesn't keep a refcount, do it ourselves. */
#ifdef PTW32_STATIC_LIB
    vlc_mutex_lock( &pthread_win32_mutex );

    if( pthread_win32_count == 0 )
    {
        msg_Dbg( p_enc, "initializing pthread-win32" );
        if( !pthread_win32_process_attach_np() || !pthread_win32_thread_attach_np() )
        {
            msg_Warn( p_enc, "pthread Win32 Initialization failed" );
            vlc_mutex_unlock( &pthread_win32_mutex );
            return VLC_EGENERIC;
        }
    }

    pthread_win32_count++;
    vlc_mutex_unlock( &pthread_win32_mutex );
#endif

    if( var_GetInteger( p_enc, SOUT_CFG_PREFIX "lookahead" ) != 40 )
    {
       p_sys->param.rc.i_lookahead = var_GetInteger( p_enc, SOUT_CFG_PREFIX "lookahead" );
    }

    /* We don't want repeated headers, we repeat p_extra ourself if needed */
    p_sys->param.b_repeat_headers = 0;

    char *psz_opts = var_InheritString( p_enc, SOUT_CFG_PREFIX "options" );
    if (psz_opts && *psz_opts) {
        config_chain_t *cfg = NULL;
        config_ChainParseOptions(&cfg, psz_opts);
        while (cfg) {
            config_chain_t *next = cfg->p_next;
            char *name  = cfg->psz_name;
            char *value = cfg->psz_value;
            int ret = x264_param_parse(&p_sys->param, name, value);
            if (ret == X264_PARAM_BAD_NAME) {
                msg_Err(p_enc, "Unknown option \"%s\"", name);
            } else if (ret == X264_PARAM_BAD_VALUE) {
                msg_Err(p_enc, "Bad value \"%s\" for option \"%s\"", value, name);
            }
            free(name);
            free(value);
            free(cfg);
            cfg = next;
        }
    }
    free(psz_opts);

    /* Open the encoder */
    p_sys->h = x264_encoder_open( &p_sys->param );

    if( p_sys->h == NULL )
    {
        msg_Err( p_enc, "cannot open x264 encoder" );
        Close( VLC_OBJECT(p_enc) );
        return VLC_EGENERIC;
    }

    /* get the globals headers */
    size_t i_extra = x264_encoder_headers( p_sys->h, &nal, &i_nal );
    uint8_t *p_extra = p_enc->fmt_out.p_extra = malloc( i_extra );
    if( !p_extra )
    {
        Close( VLC_OBJECT(p_enc) );
        return VLC_ENOMEM;
    }

    for( i = 0; i < i_nal; i++ )
    {
        if( nal[i].i_type != NAL_SEI )
        {
            memcpy( p_extra, nal[i].p_payload, nal[i].i_payload );
            p_extra += nal[i].i_payload;
            continue; /* next NAL */
        }

        /* we won't store this NAL in p_extra */
        assert( i_extra >= (size_t)nal[i].i_payload );
        i_extra -= nal[i].i_payload;

        /* Make sure we only have one SEI NAL in the headers */
        assert(p_sys->i_sei_size == 0);
        p_sys->i_sei_size = nal[i].i_payload;

        p_sys->p_sei = malloc( p_sys->i_sei_size );
        if( !p_sys->p_sei )
        {
            free( p_extra );
            Close( VLC_OBJECT(p_enc) );
            return VLC_ENOMEM;
        }
        memcpy( p_sys->p_sei, nal[i].p_payload, nal[i].i_payload );
    }

    p_enc->fmt_out.i_extra = i_extra;

    return VLC_SUCCESS;
}

/****************************************************************************
 * Logging
 ****************************************************************************/
static void x264_log( void *data, int i_level, const char *psz, va_list args)
{
    encoder_t *p_enc = (encoder_t *)data;

    switch( i_level )
    {
        case X264_LOG_ERROR:
            i_level = VLC_MSG_ERR;
            break;
        case X264_LOG_WARNING:
            i_level = VLC_MSG_WARN;
            break;
        case X264_LOG_INFO:
            i_level = VLC_MSG_INFO;
            break;
        case X264_LOG_DEBUG:
        default:
            i_level = VLC_MSG_DBG;
    }

    msg_GenericVa( p_enc, i_level, psz, args );
};

/****************************************************************************
 * Encode:
 ****************************************************************************/
static block_t *Encode( encoder_t *p_enc, picture_t *p_pict )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    x264_picture_t pic;
    x264_nal_t *nal;
    block_t *p_block;
    int i_nal=0, i_out=0, i=0;

    /* init pic */
    x264_picture_init( &pic );
    if( likely(p_pict) ) {
       pic.i_pts = p_pict->date;
       pic.img.i_csp = p_sys->i_colorspace;
       pic.img.i_plane = p_pict->i_planes;
       for( i = 0; i < p_pict->i_planes; i++ )
       {
           pic.img.plane[i] = p_pict->p[i].p_pixels;
           pic.img.i_stride[i] = p_pict->p[i].i_pitch;
       }

       x264_encoder_encode( p_sys->h, &nal, &i_nal, &pic, &pic );
    } else {
       while( x264_encoder_delayed_frames( p_sys->h ) && i_nal == 0 ) {
           x264_encoder_encode( p_sys->h, &nal, &i_nal, NULL, &pic );
       }
    }

    if( !i_nal ) return NULL;


    /* Get size of block we need */
    for( i = 0; i < i_nal; i++ )
        i_out += nal[i].i_payload;

    p_block = block_Alloc( i_out + p_sys->i_sei_size );
    if( !p_block ) return NULL;

    unsigned int i_offset = 0;
    if( unlikely( p_sys->i_sei_size ) )
    {
       /* insert x264 headers SEI nal into the first picture block at the start */
       memcpy( p_block->p_buffer, p_sys->p_sei, p_sys->i_sei_size );
       i_offset = p_sys->i_sei_size;
       p_sys->i_sei_size = 0;
       free( p_sys->p_sei );
       p_sys->p_sei = NULL;
    }
    /* copy encoded data directly to block */
    memcpy( p_block->p_buffer + i_offset, nal[0].p_payload, i_out );

    if( pic.b_keyframe )
        p_block->i_flags |= BLOCK_FLAG_TYPE_I;
    else if( pic.i_type == X264_TYPE_P || pic.i_type == X264_TYPE_I )
        p_block->i_flags |= BLOCK_FLAG_TYPE_P;
    else if( IS_X264_TYPE_B( pic.i_type ) )
        p_block->i_flags |= BLOCK_FLAG_TYPE_B;
    else
        p_block->i_flags |= BLOCK_FLAG_TYPE_PB;

    /* If we happen to have vfr stream, don't set length at all */
    if( !p_sys->param.b_vfr_input )
    {
        /* This isn't really valid for streams with B-frames */
        p_block->i_length = vlc_tick_from_samples(
                    p_enc->fmt_in.video.i_frame_rate_base,
                    p_enc->fmt_in.video.i_frame_rate );
    }

    /* scale pts-values back*/
    p_block->i_pts = pic.i_pts;
    p_block->i_dts = pic.i_dts;

    return p_block;
}

/*****************************************************************************
 * CloseEncoder: x264 encoder destruction
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    encoder_t     *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    free( p_sys->psz_stat_name );
    free( p_sys->p_sei );

    if( p_sys->h )
    {
        msg_Dbg( p_enc, "framecount still in libx264 buffer: %d", x264_encoder_delayed_frames( p_sys->h ) );
        x264_encoder_close( p_sys->h );
    }

#ifdef PTW32_STATIC_LIB
    vlc_mutex_lock( &pthread_win32_mutex );
    pthread_win32_count--;

    if( pthread_win32_count == 0 )
    {
        pthread_win32_thread_detach_np();
        pthread_win32_process_detach_np();
        msg_Dbg( p_enc, "pthread-win32 deinitialized" );
    }

    vlc_mutex_unlock( &pthread_win32_mutex );
#endif
}
