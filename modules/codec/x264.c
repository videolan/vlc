/*****************************************************************************
 * x264.c: h264 video encoder
 *****************************************************************************
 * Copyright (C) 2004-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc/vout.h>
#include <vlc/sout.h>
#include <vlc/decoder.h>

#include <x264.h>

#define SOUT_CFG_PREFIX "sout-x264-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

/* Frame-type options */

#define KEYINT_TEXT N_("Maximum GOP size")
#define KEYINT_LONGTEXT N_( "Sets maximum interval between IDR-frames." \
  "Larger values save bits, thus improving quality for a given bitrate at " \
  "the cost of seeking precision." )

#define MIN_KEYINT_TEXT N_("Minimum GOP size")
#define MIN_KEYINT_LONGTEXT N_( "Sets minimum interval between IDR-frames. " \
    "In H.264, I-frames do not necessarily bound a closed GOP because it is " \
    "allowable for a P-frame to be predicted from more frames than just the " \
    "one frame before it (also see frameref). Therefore, I-frames are not " \
    "necessarily seekable. IDR-Frames restrict subsequent P-frames from " \
    "referring to any frame prior to the IDR-Frame. \n" \
    "If scenecuts appear within this interval, they are still encoded as " \
    "I-frames, but do not start a new GOP." )

#define SCENE_TEXT N_("Extra I-Frames aggressivity" )
#define SCENE_LONGTEXT N_( "Scene-cut detection. Controls how " \
    "aggressively to insert extra I-frames. With small values of " \
    "scenecut, the codec often has " \
    "to force an I-frame when it would exceed keyint. " \
    "Good values of scenecut may find a better location for the " \
    "I-frame. Large values use more I-frames " \
    "than necessary, thus wasting bits. -1 disables scene-cut detection, so " \
    "I-frames are be inserted only every other keyint frames, which probably " \
    "leads to ugly encoding artifacts. (1-100)." )

#define BFRAMES_TEXT N_("B-frames between I and P")
#define BFRAMES_LONGTEXT N_( "Number of consecutive B-frames between I and " \
    "P-frames. (1 to 16)" )

/// \bug [String] extra space
#define B_ADAPT_TEXT N_("Adaptive B-frame decision")
#define B_ADAPT_LONGTEXT N_( "Force the specified number of " \
    "consecutive B-frames to be used, except possibly before an I-frame. " )

/// \bug [String] extra space
#define B_BIAS_TEXT N_("B-frames usage")
#define B_BIAS_LONGTEXT N_( "Bias the choice to use B-frames. Positive values " \
    "cause more B-frames, negative values cause less B-frames. " )

#define BPYRAMID_TEXT N_("Keep some B-frames as references")
#define BPYRAMID_LONGTEXT N_( "Allows B-frames to be used as references for " \
    "predicting other frames. Keeps the middle of 2+ consecutive B-frames " \
    "as a reference, and reorders frame appropriately." )

#define CABAC_TEXT N_("CABAC")
#define CABAC_LONGTEXT N_( "CABAC (Context-Adaptive Binary Arithmetic "\
    "Coding). Slightly slows down encoding and decoding, but should save " \
    "10-15% bitrate." )
/// \bug [String] Missing final dot
#define REF_TEXT N_("Number of reference frames")
#define REF_LONGTEXT N_( "Number of previous frames used as predictors. " \
    "This is effective in Anime, but seems to make little difference in " \
    "live-action source material. Some decoders are unable to deal with " \
    "large frameref values. From 1 to 16" )

#define NF_TEXT N_("Skip loop filter")
#define NF_LONGTEXT N_( "Deactivate the deblocking loop filter (decreases quality).")

#define FILTER_TEXT N_("Loop filter AlphaC0 and Beta parameters alpha:beta")
#define FILTER_LONGTEXT N_( "Loop filter AlphaC0 and Beta parameters. " \
    "Range -6 to 6 for both alpha and beta parameters. -6 means light " \
    "filter, 6 means strong.")

/* Ratecontrol */

#define QP_TEXT N_("Set QP")
#define QP_LONGTEXT N_( "This selects the quantizer to use. " \
    "Lower values result in better fidelity, but higher bitrates. 26 is a " \
    "good default value. From 0 to 51. 0 means lossless" )

/// \bug [String] missing point
#define CRF_TEXT N_("Quality-based VBR")
#define CRF_LONGTEXT N_( "1-pass Quality-based VBR. From 0 to 51" )

#define QPMIN_TEXT N_("Min QP")
#define QPMIN_LONGTEXT N_( "Minimum quantizer, 15/35 seems to be a useful " \
    "range." )

/// \bug [String] typo ? Why doesn't it work in po ?
#define QPMAX_TEXT N_("Max QP")
#define QPMAX_LONGTEXT N_( "Maximum quantizer parameter." )

#define QPSTEP_TEXT N_("Max QP step")
#define QPSTEP_LONGTEXT N_( "Max QP step between frames.")

#define RATETOL_TEXT N_("Average bitrate tolerance")
#define RATETOL_LONGTEXT N_( "Allowed variance in average. " \
    "bitrate (in kbits/s).")

#define VBV_MAXRATE_TEXT N_("Max local bitrate")
#define VBV_MAXRATE_LONGTEXT N_( "Sets a maximum local bitrate in kbits/s.")

#define VBV_BUFSIZE_TEXT N_("VBV buffer")
#define VBV_BUFSIZE_LONGTEXT N_( "Averaging period for the maximum " \
    "local bitrate in kbits.")

#define VBV_INIT_TEXT N_("Initial VBV buffer occupancy")
#define VBV_INIT_LONGTEXT N_( "Sets the initial buffer occupancy as a " \
    "fraction of the buffer size.")

#define IPRATIO_TEXT N_("QP factor between I and P")
#define IPRATIO_LONGTEXT N_( "QP factor between I and P.")

#define PBRATIO_TEXT N_("QP factor between P and B")
#define PBRATIO_LONGTEXT N_( "QP factor between P and B.")

#define CHROMA_QP_OFFSET_TEXT N_("QP difference between chroma and luma")
#define CHROMA_QP_OFFSET_LONGTEXT N_( "QP difference between chroma and luma.")

#define QCOMP_TEXT N_("QP curve compression")
#define QCOMP_LONGTEXT N_( "QP curve compression. (0.0=CBR to 1.0=QCP)")

#define CPLXBLUR_TEXT N_("Reduce fluctuations in QP")
#define CPLXBLUR_LONGTEXT N_( "This reduces the fluctuations in QP " \
                "before curve compression. Temporally blurs complexity.")

#define QBLUR_TEXT N_("Reduce fluctuations in QP")
#define QBLUR_LONGTEXT N_( "This reduces the fluctations in QP " \
                        "after curve compression. Temporally blurs quants.")

/* Analysis */

#define ANALYSE_TEXT N_("Partitions to consider")
#define ANALYSE_LONGTEXT N_( "Partitions to consider in analyse mode: \n" \
    " - none  : \n" \
    " - fast  : i4x4\n" \
    " - normal: i4x4,p8x8,(i8x8)\n" \
    " - slow  : i4x4,p8x8,(i8x8),b8x8\n" \
    " - all   : i4x4,p8x8,(i8x8),b8x8,p4x4\n" \
    "(p4x4 requires p8x8. i8x8 requires 8x8dct).")

/// \bug [String] Extra space
#define DIRECT_PRED_TEXT N_("Direct MV prediction mode")
#define DIRECT_PRED_LONGTEXT N_( "Direct MV prediction mode. ")

#define WEIGHTB_TEXT N_("Weighted prediction for B-frames")
#define WEIGHTB_LONGTEXT N_( "Weighted prediction for B-frames.")

#define ME_TEXT N_("Integer pixel motion estimation method")
#define ME_LONGTEXT N_( "Selects the motion estimation algorithm: "\
    " - dia: diamond search, radius 1 (fast)\n" \
    " - hex: hexagonal search, radius 2\n" \
    " - umh: uneven multi-hexagon search (better but slower)\n" \
    " - esa: exhaustive search (extremely slow, primarily for testing)\n" )

#define MERANGE_TEXT N_("Maximum motion vector search range")
#define MERANGE_LONGTEXT N_( "Maximum distance to search for " \
    "motion estimation, measured from predicted position(s). " \
    "Default of 16 is good for most footage, "\
    "high motion sequences may benefit from settings between 24-32. " \
    "From 0 to 64." )

#define SUBME_TEXT N_("Subpixel motion estimation and partition decision " \
    "quality")
#if X264_BUILD >= 46 /* r477 */
#define SUBME_MAX 7
#define SUBME_LONGTEXT N_( "This parameter controls quality versus speed " \
    "tradeoffs involved in the motion estimation decision process " \
    "(lower = quicker and higher = better quality). Range 1 to 7." )
#elif X264_BUILD >= 30 /* r262 */
#define SUBME_MAX 6
#define SUBME_LONGTEXT N_( "This parameter controls quality versus speed " \
    "tradeoffs involved in the motion estimation decision process " \
    "(lower = quicker and higher = better quality). Range 1 to 6." )
#else
#define SUBME_MAX 5
#define SUBME_LONGTEXT N_( "This parameter controls quality versus speed " \
    "tradeoffs involved in the motion estimation decision process " \
    "(lower = quicker and higher = better quality). Range 1 to 5." )
#endif

#define B_RDO_TEXT N_("RD based mode decision for B-frames.")
#define B_RDO_LONGTEXT N_( "RD based mode decision for B-frames. This " \
                        "requires subme 6.")

#define MIXED_REFS_TEXT N_("Decide references on a per partition basis")
#define MIXED_REFS_LONGTEXT N_( "Allows each 8x8 or 16x8 partition to " \
     "independently " \
     "select a reference frame, as opposed to only one ref per macroblock." )

#define CHROMA_ME_TEXT N_("Ignore chroma in motion estimation")
#define CHROMA_ME_LONGTEXT N_( "Chroma ME for subpel and mode decision in " \
                "P-frames.")

#define BIME_TEXT N_("Jointly optimize both MVs in B-frames")
#define BIME_LONGTEXT N_( "Joint bidirectional motion refinement.")

#define TRANSFORM_8X8DCT_TEXT N_("Adaptive spatial transform size")
#define TRANSFORM_8X8DCT_LONGTEXT N_( \
        "SATD-based decision for 8x8 transform in inter-MBs.")

#define TRELLIS_TEXT N_("Trellis RD quantization" )
#define TRELLIS_LONGTEXT N_( "Trellis RD quantization: \n" \
     " - 0: disabled\n" \
     " - 1: enabled only on the final encode of a MB\n" \
     " - 2: enabled on all mode decisions\n" \
     "This requires CABAC." )

#define FAST_PSKIP_TEXT N_("Early SKIP detection on P-frames")
#define FAST_PSKIP_LONGTEXT N_( "Early SKIP detection on P-frames.")

#define DCT_DECIMATE_TEXT N_("Coefficient thresholding on P-frames")
#define DCT_DECIMATE_LONGTEXT N_( "Coefficient thresholding on P-frames." \
    "Eliminate dct blocks containing only a small single coefficient.")

#define NR_TEXT N_("Noise reduction")
#define NR_LONGTEXT N_( "Dct-domain noise reduction. Adaptive pseudo-deadzone.")

/* Input/Output */

#define ASM_TEXT N_("CPU optimizations")
#define ASM_LONGTEXT N_( "Use assembler CPU optimizations.")

#define PSNR_TEXT N_("PSNR calculation")
#define PSNR_LONGTEXT N_( "This has no effect on actual encoding quality, "\
    "it just prevents the stats from being calculated (for speed)." )

#define QUIET_TEXT N_("Quiet mode")
#define QUIET_LONGTEXT N_( "Quiet mode.")

#define VERBOSE_TEXT N_("Statistics")
#define VERBOSE_LONGTEXT N_( "Print stats for each frame.")

#if X264_BUILD >= 24
static char *enc_me_list[] =
  { "", "dia", "hex", "umh", "esa" };
static char *enc_me_list_text[] =
  { N_("default"), N_("dia"), N_("hex"), N_("umh"), N_("esa") };
#endif

static char *enc_analyse_list[] =
  { "", "none", "fast", "normal", "slow", "all" };
static char *enc_analyse_list_text[] =
  { N_("default"), N_("none"), N_("fast"), N_("normal"),
    N_("slow"), N_("all") };

#if X264_BUILD >= 45 /* r457 */
static char *direct_pred_list[] =
  { "", "none", "spatial", "temporal", "auto" };
static char *direct_pred_list_text[] =
  { N_("default"), N_("none"), N_("spatial"), N_("temporal"), N_("auto") };
#else
static char *direct_pred_list[] =
  { "", "none", "spatial", "temporal" };
static char *direct_pred_list_text[] =
  { N_("default"), N_("none"), N_("spatial"), N_("temporal") };
#endif

vlc_module_begin();
    set_description( _("H.264/MPEG4 AVC encoder (using x264 library)"));
    set_capability( "encoder", 200 );
    set_callbacks( Open, Close );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_VCODEC );

/* Frame-type options */

    add_integer( SOUT_CFG_PREFIX "keyint", 250, NULL, KEYINT_TEXT,
                 KEYINT_LONGTEXT, VLC_FALSE );

    add_integer( SOUT_CFG_PREFIX "min-keyint", 25, NULL, MIN_KEYINT_TEXT,
                 MIN_KEYINT_LONGTEXT, VLC_FALSE );
        add_deprecated( SOUT_CFG_PREFIX "keyint-min", VLC_FALSE ); /* Deprecated since 0.8.5 */

    add_integer( SOUT_CFG_PREFIX "scenecut", 40, NULL, SCENE_TEXT,
                 SCENE_LONGTEXT, VLC_FALSE );
        change_integer_range( -1, 100 );

    add_integer( SOUT_CFG_PREFIX "bframes", 0, NULL, BFRAMES_TEXT,
                 BFRAMES_LONGTEXT, VLC_FALSE );
        change_integer_range( 0, 16 );

#if X264_BUILD >= 0x0013 /* r137 */
    add_bool( SOUT_CFG_PREFIX "b-adapt", 1, NULL, B_ADAPT_TEXT,
              B_ADAPT_LONGTEXT, VLC_FALSE );

    add_integer( SOUT_CFG_PREFIX "b-bias", 0, NULL, B_BIAS_TEXT,
                 B_BIAS_LONGTEXT, VLC_FALSE );
        change_integer_range( -100, 100 );
#endif

    add_bool( SOUT_CFG_PREFIX "bpyramid", 0, NULL, BPYRAMID_TEXT,
              BPYRAMID_LONGTEXT, VLC_FALSE );

    add_bool( SOUT_CFG_PREFIX "cabac", 1, NULL, CABAC_TEXT, CABAC_LONGTEXT,
              VLC_FALSE );

    add_integer( SOUT_CFG_PREFIX "ref", 1, NULL, REF_TEXT,
                 REF_LONGTEXT, VLC_FALSE );
        add_deprecated( SOUT_CFG_PREFIX "frameref", VLC_FALSE ); /* Deprecated since 0.8.5 */
        change_integer_range( 1, 16 );

    add_bool( SOUT_CFG_PREFIX "nf", 0, NULL, NF_TEXT,
              NF_LONGTEXT, VLC_FALSE );
        add_deprecated( SOUT_CFG_PREFIX "loopfilter", VLC_FALSE ); /* Deprecated since 0.8.5 */

    add_string( SOUT_CFG_PREFIX "filter", "", NULL, FILTER_TEXT,
                 FILTER_LONGTEXT, VLC_FALSE );

/* Ratecontrol */

    add_integer( SOUT_CFG_PREFIX "qp", 26, NULL, QP_TEXT, QP_LONGTEXT,
                 VLC_FALSE );
        change_integer_range( 0, 51 ); /* QP 0 -> lossless encoding */

#if X264_BUILD >= 37 /* r334 */
    add_integer( SOUT_CFG_PREFIX "crf", 0, NULL, CRF_TEXT,
                 CRF_LONGTEXT, VLC_FALSE );
        change_integer_range( 0, 51 );
#endif

    add_integer( SOUT_CFG_PREFIX "qpmin", 10, NULL, QPMIN_TEXT,
                 QPMIN_LONGTEXT, VLC_FALSE );
        add_deprecated( SOUT_CFG_PREFIX "qp-min", VLC_FALSE ); /* Deprecated since 0.8.5 */
        change_integer_range( 0, 51 );

    add_integer( SOUT_CFG_PREFIX "qpmax", 51, NULL, QPMAX_TEXT,
                 QPMAX_LONGTEXT, VLC_FALSE );
        add_deprecated( SOUT_CFG_PREFIX "qp-max", VLC_FALSE ); /* Deprecated since 0.8.5 */
        change_integer_range( 0, 51 );

    add_integer( SOUT_CFG_PREFIX "qpstep", 4, NULL, QPSTEP_TEXT,
                 QPSTEP_LONGTEXT, VLC_FALSE );
        change_integer_range( 0, 51 );

    add_float( SOUT_CFG_PREFIX "ratetol", 1.0, NULL, RATETOL_TEXT,
               RATETOL_LONGTEXT, VLC_FALSE );
        add_deprecated( SOUT_CFG_PREFIX "tolerance", VLC_FALSE ); /* Deprecated since 0.8.5 */
        change_float_range( 0, 100 );

    add_integer( SOUT_CFG_PREFIX "vbv-maxrate", 0, NULL, VBV_MAXRATE_TEXT,
                 VBV_MAXRATE_LONGTEXT, VLC_FALSE );

    add_integer( SOUT_CFG_PREFIX "vbv-bufsize", 0, NULL, VBV_BUFSIZE_TEXT,
                 VBV_BUFSIZE_LONGTEXT, VLC_FALSE );

    add_float( SOUT_CFG_PREFIX "vbv-init", 0.9, NULL, VBV_INIT_TEXT,
               VBV_INIT_LONGTEXT, VLC_FALSE );
        change_float_range( 0, 1 );

    add_float( SOUT_CFG_PREFIX "ipratio", 1.40, NULL, IPRATIO_TEXT,
               IPRATIO_LONGTEXT, VLC_FALSE );
        change_float_range( 0, 100 );

    add_float( SOUT_CFG_PREFIX "pbratio", 1.30, NULL, PBRATIO_TEXT,
               PBRATIO_LONGTEXT, VLC_FALSE );
        change_float_range( 0, 100 );

#if X264_BUILD >= 23 /* r190 */
    add_integer( SOUT_CFG_PREFIX "chroma-qp-offset", 0, NULL, CHROMA_QP_OFFSET_TEXT,
                 CHROMA_QP_OFFSET_LONGTEXT, VLC_FALSE );
#endif

    add_float( SOUT_CFG_PREFIX "qcomp", 0.60, NULL, QCOMP_TEXT,
               QCOMP_LONGTEXT, VLC_FALSE );
        change_float_range( 0, 1 );

    add_float( SOUT_CFG_PREFIX "cplxblur", 20.0, NULL, CPLXBLUR_TEXT,
               CPLXBLUR_LONGTEXT, VLC_FALSE );

    add_float( SOUT_CFG_PREFIX "qblur", 0.5, NULL, QBLUR_TEXT,
               QBLUR_LONGTEXT, VLC_FALSE );

/* Analysis */

    /* x264 analyse = none (default). set at least "normal" mode. */
    add_string( SOUT_CFG_PREFIX "analyse", "normal", NULL, ANALYSE_TEXT,
                ANALYSE_LONGTEXT, VLC_FALSE );
        change_string_list( enc_analyse_list, enc_analyse_list_text, 0 );

    add_string( SOUT_CFG_PREFIX "direct", "spatial", NULL, DIRECT_PRED_TEXT,
                DIRECT_PRED_LONGTEXT, VLC_FALSE );
        change_string_list( direct_pred_list, direct_pred_list_text, 0 );

#if X264_BUILD >= 0x0012 /* r134 */
    add_bool( SOUT_CFG_PREFIX "weightb", 0, NULL, WEIGHTB_TEXT,
              WEIGHTB_LONGTEXT, VLC_FALSE );
#endif

#if X264_BUILD >= 24 /* r221 */
    add_string( SOUT_CFG_PREFIX "me", "hex", NULL, ME_TEXT,
                ME_LONGTEXT, VLC_FALSE );
        change_string_list( enc_me_list, enc_me_list_text, 0 );

    add_integer( SOUT_CFG_PREFIX "merange", 16, NULL, MERANGE_TEXT,
                 MERANGE_LONGTEXT, VLC_FALSE );
        change_integer_range( 1, 64 );
#endif

    add_integer( SOUT_CFG_PREFIX "subme", 5, NULL, SUBME_TEXT,
                 SUBME_LONGTEXT, VLC_FALSE );
        add_deprecated( SOUT_CFG_PREFIX "subpel", VLC_FALSE ); /* Deprecated since 0.8.5 */
#if X264_BUILD >= 30 /* r262 */
        change_integer_range( 1, 6 );
#else
        change_integer_range( 1, 5 );
#endif

#if X264_BUILD >= 41 /* r368 */
    add_bool( SOUT_CFG_PREFIX "b-rdo", 0, NULL, B_RDO_TEXT,
              B_RDO_LONGTEXT, VLC_FALSE );
#endif

#if X264_BUILD >= 36 /* r318 */
    add_bool( SOUT_CFG_PREFIX "mixed-refs", 0, NULL, MIXED_REFS_TEXT,
              MIXED_REFS_LONGTEXT, VLC_FALSE );
#endif

#if X264_BUILD >= 23 /* r171 */
    add_bool( SOUT_CFG_PREFIX "chroma-me", 1, NULL, CHROMA_ME_TEXT,
              CHROMA_ME_LONGTEXT, VLC_FALSE );
#endif

#if X264_BUILD >= 43 /* r390 */
    add_bool( SOUT_CFG_PREFIX "bime", 0, NULL, BIME_TEXT,
              BIME_LONGTEXT, VLC_FALSE );
#endif

#if X264_BUILD >= 30 /* r251 */
    add_bool( SOUT_CFG_PREFIX "8x8dct", 0, NULL, TRANSFORM_8X8DCT_TEXT,
              TRANSFORM_8X8DCT_LONGTEXT, VLC_FALSE );
#endif

#if X264_BUILD >= 39 /* r360 */
    add_integer( SOUT_CFG_PREFIX "trellis", 0, NULL, TRELLIS_TEXT,
                 TRELLIS_LONGTEXT, VLC_FALSE );
        change_integer_range( 0, 2 );
#endif

#if X264_BUILD >= 42 /* r384 */
    add_bool( SOUT_CFG_PREFIX "fast-pskip", 1, NULL, FAST_PSKIP_TEXT,
              FAST_PSKIP_LONGTEXT, VLC_FALSE );
#endif

#if X264_BUILD >= 46 /* r503 */
    add_bool( SOUT_CFG_PREFIX "dct-decimate", 1, NULL, DCT_DECIMATE_TEXT,
              DCT_DECIMATE_LONGTEXT, VLC_FALSE );
#endif

#if X264_BUILD >= 44 /* r398 */
    add_integer( SOUT_CFG_PREFIX "nr", 0, NULL, NR_TEXT,
                 NR_LONGTEXT, VLC_FALSE );
#endif

/* Input/Output */

    add_bool( SOUT_CFG_PREFIX "asm", 1, NULL, ASM_TEXT,
              ASM_LONGTEXT, VLC_FALSE );

    /* x264 psnr = 1 (default). disable PSNR calculation for speed. */
    add_bool( SOUT_CFG_PREFIX "psnr", 0, NULL, PSNR_TEXT,
              PSNR_LONGTEXT, VLC_FALSE );

    add_bool( SOUT_CFG_PREFIX "quiet", 0, NULL, QUIET_TEXT,
              QUIET_LONGTEXT, VLC_FALSE );

#if X264_BUILD >= 0x000e /* r81 */
    add_bool( SOUT_CFG_PREFIX "verbose", 0, NULL, VERBOSE_TEXT,
              VERBOSE_LONGTEXT, VLC_FALSE );
#endif

vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *ppsz_sout_options[] = {
    "8x8dct", "analyse", "asm", "bframes", "bime", "bpyramid", "b-adapt",
    "b-bias", "b-rdo", "cabac", "chroma-me", "chroma-qp-offset", "cplxblur",
    "crf", "dct-decimate", "direct", "filter", "fast-pskip", "frameref",
    "ipratio", "keyint", "keyint-min", "loopfilter", "me", "merange",
    "min-keyint", "mixed-refs", "nf", "nr", "pbratio", "psnr", "qblur",
    "qp", "qcomp", "qpstep", "qpmax", "qpmin", "qp-max", "qp-min", "quiet",
    "ratetol", "ref", "scenecut", "subme", "subpel", "tolerance",
    "trellis", "verbose", "vbv-bufsize", "vbv-init", "vbv-maxrate",
    "weightb", NULL
};

static block_t *Encode( encoder_t *, picture_t * );

struct encoder_sys_t
{
    x264_t          *h;
    x264_param_t    param;

    int             i_buffer;
    uint8_t         *p_buffer;

    mtime_t         i_last_ref_pts;
};

/*****************************************************************************
 * Open: probe the encoder
 *****************************************************************************/
static int  Open ( vlc_object_t *p_this )
{
    encoder_t     *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;
    vlc_value_t    val;
    int i_qmin = 0, i_qmax = 0;

    if( p_enc->fmt_out.i_codec != VLC_FOURCC( 'h', '2', '6', '4' ) &&
        !p_enc->b_force )
    {
        return VLC_EGENERIC;
    }

#if X264_BUILD < 37
    if( p_enc->fmt_in.video.i_width % 16 != 0 ||
        p_enc->fmt_in.video.i_height % 16!= 0 )
    {
        msg_Warn( p_enc, "size is not a multiple of 16 (%ix%i)",
                  p_enc->fmt_in.video.i_width, p_enc->fmt_in.video.i_height );

        if( p_enc->fmt_in.video.i_width < 16 ||
            p_enc->fmt_in.video.i_height < 16 )
        {
            msg_Err( p_enc, "video is too small to be cropped" );
            return VLC_EGENERIC;
        }

        msg_Warn( p_enc, "cropping video to %ix%i",
                  p_enc->fmt_in.video.i_width >> 4 << 4,
                  p_enc->fmt_in.video.i_height >> 4 << 4 );
    }
#endif

    sout_CfgParse( p_enc, SOUT_CFG_PREFIX, ppsz_sout_options, p_enc->p_cfg );

    p_enc->fmt_out.i_codec = VLC_FOURCC( 'h', '2', '6', '4' );
    p_enc->fmt_in.i_codec = VLC_FOURCC('I','4','2','0');

    p_enc->pf_encode_video = Encode;
    p_enc->pf_encode_audio = NULL;
    p_enc->p_sys = p_sys = malloc( sizeof( encoder_sys_t ) );
    p_sys->i_last_ref_pts = 0;

    x264_param_default( &p_sys->param );
    p_sys->param.i_width  = p_enc->fmt_in.video.i_width;
    p_sys->param.i_height = p_enc->fmt_in.video.i_height;
#if X264_BUILD < 37
    p_sys->param.i_width  = p_sys->param.i_width >> 4 << 4;
    p_sys->param.i_height = p_sys->param.i_height >> 4 << 4;
#endif

    /* average bitrate specified by transcode vb */
    p_sys->param.rc.i_bitrate = p_enc->fmt_out.i_bitrate / 1000;

    /* cbr = 1 overrides qp or crf and sets an average bitrate
       but maxrate = average bitrate is needed for "real" CBR */
    if( p_sys->param.rc.i_bitrate > 0 ) p_sys->param.rc.b_cbr = 1;

    var_Get( p_enc, SOUT_CFG_PREFIX "qpstep", &val );
    if( val.i_int >= 0 && val.i_int <= 51 ) p_sys->param.rc.i_qp_step = val.i_int;
    var_Get( p_enc, SOUT_CFG_PREFIX "qpmin", &val );
    if( val.i_int >= 0 && val.i_int <= 51 ) i_qmin = val.i_int;
    var_Get( p_enc, SOUT_CFG_PREFIX "qpmax", &val );
    if( val.i_int >= 0 && val.i_int <= 51 ) i_qmax = val.i_int;

    var_Get( p_enc, SOUT_CFG_PREFIX "qp", &val );
    if( val.i_int >= 0 && val.i_int <= 51 )
    {
        if( i_qmin > val.i_int ) i_qmin = val.i_int;
        if( i_qmax < val.i_int ) i_qmax = val.i_int;

#if X264_BUILD >= 0x000a
        p_sys->param.rc.i_qp_constant = val.i_int;
        p_sys->param.rc.i_qp_min = i_qmin;
        p_sys->param.rc.i_qp_max = i_qmax;
#else
        p_sys->param.i_qp_constant = val.i_int;
#endif
    }

#if X264_BUILD >= 24
    var_Get( p_enc, SOUT_CFG_PREFIX "ratetol", &val );
    p_sys->param.rc.f_rate_tolerance = val.f_float;

    var_Get( p_enc, SOUT_CFG_PREFIX "vbv-init", &val );
    p_sys->param.rc.f_vbv_buffer_init = val.f_float;

    var_Get( p_enc, SOUT_CFG_PREFIX "vbv-bufsize", &val );
    p_sys->param.rc.i_vbv_buffer_size = val.i_int;

    /* x264 vbv-bufsize = 0 (default). if not provided set period
       in seconds for local maximum bitrate (cache/bufsize) based
       on average bitrate. */
    if( !val.i_int )
        p_sys->param.rc.i_vbv_buffer_size = p_sys->param.rc.i_bitrate * 2;

    /* max bitrate = average bitrate -> CBR */
    var_Get( p_enc, SOUT_CFG_PREFIX "vbv-maxrate", &val );
    p_sys->param.rc.i_vbv_max_bitrate = val.i_int;

#else
    p_sys->param.rc.i_rc_buffer_size = p_sys->param.rc.i_bitrate;
    p_sys->param.rc.i_rc_init_buffer = p_sys->param.rc.i_bitrate / 4;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "cabac", &val );
    p_sys->param.b_cabac = val.b_bool;

    /* disable deblocking when nf (no loop filter) is enabled */
    var_Get( p_enc, SOUT_CFG_PREFIX "nf", &val );
    p_sys->param.b_deblocking_filter = !val.b_bool;
    
    var_Get( p_enc, SOUT_CFG_PREFIX "filter", &val );
    if( val.psz_string )
    {
        char *p = strchr( val.psz_string, ':' );
        p_sys->param.i_deblocking_filter_alphac0 = atoi( val.psz_string );
        p_sys->param.i_deblocking_filter_beta = p ? atoi( p+1 ) : p_sys->param.i_deblocking_filter_alphac0;
        free( val.psz_string );
    }

    var_Get( p_enc, SOUT_CFG_PREFIX "ipratio", &val );
    p_sys->param.rc.f_ip_factor = val.f_float;

    var_Get( p_enc, SOUT_CFG_PREFIX "pbratio", &val );
    p_sys->param.rc.f_pb_factor = val.f_float;

    var_Get( p_enc, SOUT_CFG_PREFIX "qcomp", &val );
    p_sys->param.rc.f_qcompress = val.f_float;

    var_Get( p_enc, SOUT_CFG_PREFIX "cplxblur", &val );
    p_sys->param.rc.f_complexity_blur = val.f_float;

    var_Get( p_enc, SOUT_CFG_PREFIX "qblur", &val );
    p_sys->param.rc.f_qblur = val.f_float;

#if X264_BUILD >= 0x000e
    var_Get( p_enc, SOUT_CFG_PREFIX "verbose", &val );
    if( val.b_bool ) p_sys->param.i_log_level = X264_LOG_DEBUG;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "quiet", &val );
    if( val.b_bool ) p_sys->param.i_log_level = X264_LOG_NONE;

    var_Get( p_enc, SOUT_CFG_PREFIX "keyint", &val );
#if X264_BUILD >= 0x000e
    if( val.i_int > 0 ) p_sys->param.i_keyint_max = val.i_int;
#else
    if( val.i_int > 0 ) p_sys->param.i_iframe = val.i_int;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "min-keyint", &val );
#if X264_BUILD >= 0x000e
    if( val.i_int > 0 ) p_sys->param.i_keyint_min = val.i_int;
#else
    if( val.i_int > 0 ) p_sys->param.i_idrframe = val.i_int;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "bframes", &val );
    if( val.i_int >= 0 && val.i_int <= 16 )
        p_sys->param.i_bframe = val.i_int;

#if X264_BUILD >= 22
    var_Get( p_enc, SOUT_CFG_PREFIX "bpyramid", &val );
    p_sys->param.b_bframe_pyramid = val.b_bool;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "ref", &val );
    if( val.i_int > 0 && val.i_int <= 15 )
        p_sys->param.i_frame_reference = val.i_int;

    var_Get( p_enc, SOUT_CFG_PREFIX "scenecut", &val );
#if X264_BUILD >= 0x000b
    if( val.i_int >= -1 && val.i_int <= 100 )
        p_sys->param.i_scenecut_threshold = val.i_int;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "subme", &val );
    if( val.i_int >= 1 && val.i_int <= SUBME_MAX )
        p_sys->param.analyse.i_subpel_refine = val.i_int;

#if X264_BUILD >= 24
    var_Get( p_enc, SOUT_CFG_PREFIX "me", &val );
    if( !strcmp( val.psz_string, "dia" ) )
    {
        p_sys->param.analyse.i_me_method = X264_ME_DIA;
    }
    else if( !strcmp( val.psz_string, "hex" ) )
    {
        p_sys->param.analyse.i_me_method = X264_ME_HEX;
    }
    else if( !strcmp( val.psz_string, "umh" ) )
    {
        p_sys->param.analyse.i_me_method = X264_ME_UMH;
    }
    else if( !strcmp( val.psz_string, "esa" ) )
    {
        p_sys->param.analyse.i_me_method = X264_ME_ESA;
    }
    if( val.psz_string ) free( val.psz_string );

    var_Get( p_enc, SOUT_CFG_PREFIX "merange", &val );
    if( val.i_int >= 1 && val.i_int <= 64 )
        p_sys->param.analyse.i_me_range = val.i_int;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "direct", &val );
    if( !strcmp( val.psz_string, "none" ) )
    {
        p_sys->param.analyse.i_direct_mv_pred = X264_DIRECT_PRED_NONE;
    }
    else if( !strcmp( val.psz_string, "spatial" ) )
    {
        p_sys->param.analyse.i_direct_mv_pred = X264_DIRECT_PRED_SPATIAL;
    }
    else if( !strcmp( val.psz_string, "temporal" ) )
    {
        p_sys->param.analyse.i_direct_mv_pred = X264_DIRECT_PRED_TEMPORAL;
    }
#if X264_BUILD >= 45 /* r457 */
    else if( !strcmp( val.psz_string, "auto" ) )
    {
        p_sys->param.analyse.i_direct_mv_pred = X264_DIRECT_PRED_AUTO;
    }
#endif
    if( val.psz_string ) free( val.psz_string );

    var_Get( p_enc, SOUT_CFG_PREFIX "psnr", &val );
    p_sys->param.analyse.b_psnr = val.b_bool;

#if X264_BUILD >= 0x0012
    var_Get( p_enc, SOUT_CFG_PREFIX "weightb", &val );
    p_sys->param.analyse.b_weighted_bipred = val.b_bool;
#endif

#if X264_BUILD >= 0x0013
    var_Get( p_enc, SOUT_CFG_PREFIX "b-adapt", &val );
    p_sys->param.b_bframe_adaptive = val.b_bool;

    var_Get( p_enc, SOUT_CFG_PREFIX "b-bias", &val );
    if( val.i_int >= -100 && val.i_int <= 100 )
        p_sys->param.i_bframe_bias = val.i_int;
#endif

#if X264_BUILD >= 23
    var_Get( p_enc, SOUT_CFG_PREFIX "chroma-me", &val );
    p_sys->param.analyse.b_chroma_me = val.b_bool;
    var_Get( p_enc, SOUT_CFG_PREFIX "chroma-qp-offset", &val );
    p_sys->param.analyse.i_chroma_qp_offset = val.i_int;
#endif

#if X264_BUILD >= 36
    var_Get( p_enc, SOUT_CFG_PREFIX "mixed-refs", &val );
    p_sys->param.analyse.b_mixed_references = val.b_bool;
#endif

#if X264_BUILD >= 37
    var_Get( p_enc, SOUT_CFG_PREFIX "crf", &val ); 
    if( val.i_int >= 0 && val.i_int <= 51 ) /* crf != 0 overrides qp */
        p_sys->param.rc.i_rf_constant = val.i_int;
#endif

#if X264_BUILD >= 39
    var_Get( p_enc, SOUT_CFG_PREFIX "trellis", &val );
    if( val.i_int >= 0 && val.i_int <= 2 )
        p_sys->param.analyse.i_trellis = val.i_int;
#endif

#if X264_BUILD >= 41
    var_Get( p_enc, SOUT_CFG_PREFIX "b-rdo", &val );
    p_sys->param.analyse.b_bframe_rdo = val.b_bool;
#endif

#if X264_BUILD >= 42
    var_Get( p_enc, SOUT_CFG_PREFIX "fast-pskip", &val );
    p_sys->param.analyse.b_fast_pskip = val.b_bool;
#endif

#if X264_BUILD >= 43
    var_Get( p_enc, SOUT_CFG_PREFIX "bime", &val );
    p_sys->param.analyse.b_bidir_me = val.b_bool;
#endif

#if X264_BUILD >= 44
    var_Get( p_enc, SOUT_CFG_PREFIX "nr", &val );
    p_sys->param.analyse.i_noise_reduction = val.i_int;
#endif

#if X264_BUILD >= 46
    var_Get( p_enc, SOUT_CFG_PREFIX "dct-decimate", &val );
    p_sys->param.analyse.b_dct_decimate = val.b_bool;
#endif

    var_Get( p_enc, SOUT_CFG_PREFIX "asm", &val );
    if( !val.b_bool ) p_sys->param.cpu = 0;

#ifndef X264_ANALYSE_BSUB16x16
#   define X264_ANALYSE_BSUB16x16 0
#endif
    var_Get( p_enc, SOUT_CFG_PREFIX "analyse", &val );
    if( !strcmp( val.psz_string, "none" ) )
    {
        p_sys->param.analyse.inter = 0;
    }
    else if( !strcmp( val.psz_string, "fast" ) )
    {
        p_sys->param.analyse.inter = X264_ANALYSE_I4x4;
    }
    else if( !strcmp( val.psz_string, "normal" ) )
    {
        p_sys->param.analyse.inter =
            X264_ANALYSE_I4x4 |
            X264_ANALYSE_PSUB16x16;
#ifdef X264_ANALYSE_I8x8
        p_sys->param.analyse.inter |= X264_ANALYSE_I8x8;
#endif
    }
    else if( !strcmp( val.psz_string, "slow" ) )
    {
        p_sys->param.analyse.inter =
            X264_ANALYSE_I4x4 |
            X264_ANALYSE_PSUB16x16 |
            X264_ANALYSE_BSUB16x16;
#ifdef X264_ANALYSE_I8x8
        p_sys->param.analyse.inter |= X264_ANALYSE_I8x8;
#endif
    }
    else if( !strcmp( val.psz_string, "all" ) )
    {
        p_sys->param.analyse.inter =
            X264_ANALYSE_I4x4 |
            X264_ANALYSE_PSUB16x16 |
            X264_ANALYSE_BSUB16x16 |
            X264_ANALYSE_PSUB8x8;
#ifdef X264_ANALYSE_I8x8
        p_sys->param.analyse.inter |= X264_ANALYSE_I8x8;
#endif
    }
    if( val.psz_string ) free( val.psz_string );

#if X264_BUILD >= 30
    var_Get( p_enc, SOUT_CFG_PREFIX "8x8dct", &val );
    p_sys->param.analyse.b_transform_8x8 = val.b_bool;
#endif

    if( p_enc->fmt_in.video.i_aspect > 0 )
    {
        int64_t i_num, i_den;
        int i_dst_num, i_dst_den;

        i_num = p_enc->fmt_in.video.i_aspect *
            (int64_t)p_enc->fmt_in.video.i_height;
        i_den = VOUT_ASPECT_FACTOR * p_enc->fmt_in.video.i_width;
        vlc_ureduce( &i_dst_num, &i_dst_den, i_num, i_den, 0 );

        p_sys->param.vui.i_sar_width = i_dst_num;
        p_sys->param.vui.i_sar_height = i_dst_den;
    }
    if( p_enc->fmt_in.video.i_frame_rate_base > 0 )
    {
        p_sys->param.i_fps_num = p_enc->fmt_in.video.i_frame_rate;
        p_sys->param.i_fps_den = p_enc->fmt_in.video.i_frame_rate_base;
    }
    if( !(p_enc->p_libvlc->i_cpu & CPU_CAPABILITY_MMX) )
    {
        p_sys->param.cpu &= ~X264_CPU_MMX;
    }
    if( !(p_enc->p_libvlc->i_cpu & CPU_CAPABILITY_MMXEXT) )
    {
        p_sys->param.cpu &= ~X264_CPU_MMXEXT;
    }
    if( !(p_enc->p_libvlc->i_cpu & CPU_CAPABILITY_SSE) )
    {
        p_sys->param.cpu &= ~X264_CPU_SSE;
    }
    if( !(p_enc->p_libvlc->i_cpu & CPU_CAPABILITY_SSE2) )
    {
        p_sys->param.cpu &= ~X264_CPU_SSE2;
    }

#if X264_BUILD >= 29
    if( p_enc->i_threads >= 1 )
        p_sys->param.i_threads = p_enc->i_threads;
#endif

    /* Open the encoder */
    p_sys->h = x264_encoder_open( &p_sys->param );

    /* alloc mem */
    p_sys->i_buffer = 4 * p_enc->fmt_in.video.i_width *
        p_enc->fmt_in.video.i_height + 1000;
    p_sys->p_buffer = malloc( p_sys->i_buffer );

    /* get the globals headers */
    p_enc->fmt_out.i_extra = 0;
    p_enc->fmt_out.p_extra = NULL;

#if 0
    x264_encoder_headers( p_sys->h, &nal, &i_nal );
    for( i = 0; i < i_nal; i++ )
    {
        int i_size = p_sys->i_buffer;

        x264_nal_encode( p_sys->p_buffer, &i_size, 1, &nal[i] );

        p_enc->fmt_out.p_extra = realloc( p_enc->fmt_out.p_extra, p_enc->fmt_out.i_extra + i_size );

        memcpy( p_enc->fmt_out.p_extra + p_enc->fmt_out.i_extra,
                p_sys->p_buffer, i_size );

        p_enc->fmt_out.i_extra += i_size;
    }
#endif

    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode:
 ****************************************************************************/
static block_t *Encode( encoder_t *p_enc, picture_t *p_pict )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    x264_picture_t pic;
    x264_nal_t *nal;
    block_t *p_block;
    int i_nal, i_out, i;

    /* init pic */
    memset( &pic, 0, sizeof( x264_picture_t ) );
    pic.i_pts = p_pict->date;
    pic.img.i_csp = X264_CSP_I420;
    pic.img.i_plane = p_pict->i_planes;
    for( i = 0; i < p_pict->i_planes; i++ )
    {
        pic.img.plane[i] = p_pict->p[i].p_pixels;
        pic.img.i_stride[i] = p_pict->p[i].i_pitch;
    }

#if X264_BUILD >= 0x0013
    x264_encoder_encode( p_sys->h, &nal, &i_nal, &pic, &pic );
#else
    x264_encoder_encode( p_sys->h, &nal, &i_nal, &pic );
#endif

    if( !i_nal ) return NULL;

    for( i = 0, i_out = 0; i < i_nal; i++ )
    {
        int i_size = p_sys->i_buffer - i_out;
        x264_nal_encode( p_sys->p_buffer + i_out, &i_size, 1, &nal[i] );

        i_out += i_size;
    }

    p_block = block_New( p_enc, i_out );
    memcpy( p_block->p_buffer, p_sys->p_buffer, i_out );

    if( pic.i_type == X264_TYPE_IDR || pic.i_type == X264_TYPE_I )
        p_block->i_flags |= BLOCK_FLAG_TYPE_I;
    else if( pic.i_type == X264_TYPE_P )
        p_block->i_flags |= BLOCK_FLAG_TYPE_P;
    else if( pic.i_type == X264_TYPE_B )
        p_block->i_flags |= BLOCK_FLAG_TYPE_B;

    /* This isn't really valid for streams with B-frames */
    p_block->i_length = I64C(1000000) *
        p_enc->fmt_in.video.i_frame_rate_base /
            p_enc->fmt_in.video.i_frame_rate;

    p_block->i_dts = p_block->i_pts = pic.i_pts;

    if( p_sys->param.i_bframe > 0 )
    {
        if( p_block->i_flags & BLOCK_FLAG_TYPE_B )
        {
            p_block->i_dts = p_block->i_pts;
        }
        else
        {
            if( p_sys->i_last_ref_pts )
            {
                p_block->i_dts = p_sys->i_last_ref_pts;
            }
            else
            {
                /* Let's put something sensible */
                p_block->i_dts = p_block->i_pts;
            }

            p_sys->i_last_ref_pts = p_block->i_pts;
        }
    }

    return p_block;
}

/*****************************************************************************
 * CloseEncoder: x264 encoder destruction
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    encoder_t     *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    x264_encoder_close( p_sys->h );
    free( p_sys->p_buffer );
    free( p_sys );
}
