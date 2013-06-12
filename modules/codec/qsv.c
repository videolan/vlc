/*****************************************************************************
 * qsv.c: mpeg4-part10/mpeg2 video encoder using Intel Media SDK
 *****************************************************************************
 * Copyright (C) 2013 VideoLabs
 * $Id$
 *
 * Authors: Julien 'Lta' BALLET <contact@lta.io>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_picture.h>
#include <vlc_codec.h>

#include <mfx/mfxvideo.h>

#define SOUT_CFG_PREFIX     "sout-qsv-"

/* Default wait on Intel Media SDK SyncOperation. Almost useless when async-depth >= 2 */
#define QSV_SYNCPOINT_WAIT  (420)
/* Encoder input synchronization busy wait loop time */
#define QSV_BUSYWAIT_TIME   (10000)
/* The SDK doesn't have a default bitrate, so here's one. */
#define QSV_BITRATE_DEFAULT (842)

/* Makes x a multiple of 'align'. 'align' must me a power of 2 */
#define QSV_ALIGN(align, x)     (((x)+(align)-1)&~((align)-1))

/*****************************************************************************
 * Modules descriptor
 *****************************************************************************/
static int      Open(vlc_object_t *);
static void     Close(vlc_object_t *);

#define SW_IMPL_TEXT N_("Enable software mode")
#define SW_IMPL_LONGTEXT N_("Allow the use of the Intel Media SDK software " \
     "implementation of the codecs if no QuickSync Video hardware " \
     "acceleration is present on the system.")

#define PROFILE_TEXT N_("Codec Profile")
#define PROFILE_LONGTEXT N_( \
    "Specify the codec profile explicitly. If you don't, the codec will " \
    "determine the correct profile from other sources, such as resolution " \
    "and bitrate. E.g. 'high'")

#define LEVEL_TEXT N_("Codec Level")
#define LEVEL_LONGTEXT N_( \
    "Specify the codec level explicitly. If you don't, the codec will " \
    "determine the correct profile from other sources, such as resolution " \
    "and bitrate. E.g. '4.2' for mpeg4-part10 or 'low' for mpeg2")

#define GOP_SIZE_TEXT N_("Group of Picture size")
#define GOP_SIZE_LONGTEXT N_( \
    "Number of pictures within the current GOP (Group of Pictures); if " \
    "GopPicSize=0, then the GOP size is unspecified. If GopPicSize=1, " \
    "only I-frames are used.")

#define GOP_REF_DIST_TEXT N_("Group of Picture Reference Distance")
#define GOP_REF_DIST_LONGTEXT N_( \
    "Distance between I- or P- key frames; if it is zero, the GOP " \
    "structure is unspecified. Note: If GopRefDist = 1, there are no B- " \
    "frames used. ")

#define TARGET_USAGE_TEXT N_("Target Usage")
#define TARGET_USAGE_LONGTEXT N_("The target usage allow to choose between " \
    "different trade-offs between quality and speed. Allowed values are : " \
    "'speed', 'balanced' and 'quality'.")

#define IDR_INTERVAL_TEXT N_("IDR interval")
#define IDR_INTERVAL_LONGTEXT N_( \
    "For H.264, IdrInterval specifies IDR-frame interval in terms of I- " \
    "frames; if IdrInterval=0, then every I-frame is an IDR-frame. If " \
    "IdrInterval=1, then every other I-frame is an IDR-frame, etc. " \
    "For MPEG2, IdrInterval defines sequence header interval in terms " \
    "of I-frames. If IdrInterval=N, SDK inserts the sequence header " \
    "before every Nth I-frame. If IdrInterval=0 (default), SDK inserts " \
    "the sequence header once at the beginning of the stream. ")

#define RATE_CONTROL_TEXT N_("Rate Control Method")
#define RATE_CONTROL_LONGTEXT N_( \
    "The rate control method to use when encoding. Can be one of " \
    "'crb', 'vbr', 'qp', 'avbr'. 'qp' mode isn't supported for mpeg2")

#define QP_TEXT N_("Quantization parameter")
#define QP_LONGTEXT N_("Quantization parameter for all types of frames. " \
    "This parameters sets qpi, qpp and qpp. It has less precedence than " \
    "the forementionned parameters. Used only if rc_method is 'qp'.")

#define QPI_TEXT N_("Quantization parameter for I-frames")
#define QPI_LONGTEXT N_("Quantization parameter for I-frames. This parameter " \
    "overrides any qp set globally. Used only if rc_method is 'qp'.")

#define QPP_TEXT N_("Quantization parameter for P-frames")
#define QPP_LONGTEXT N_("Quantization parameter for P-frames. This parameter " \
    "overrides any qp set globally. Used only if rc_method is 'qp'.")

#define QPB_TEXT N_("Quantization parameter for B-frames")
#define QPB_LONGTEXT N_("Quantization parameter for B-frames. This parameter " \
    "overrides any qp set globally. Used only if rc_method is 'qp'.")

#define MAX_BITRATE_TEXT N_("Maximum Bitrate")
#define MAX_BITRATE_LONGTEXT N_("Defines the maximum bitrate in Kpbs " \
    "(1000 bits/s) for VBR rate control method. If not set, this parameter" \
    ". is computed from other sources such as bitrate, profile, level, etc.")

#define ACCURACY_TEXT N_("Accuracy of RateControl")
#define ACCURACY_LONGTEXT N_("Tolerance in percentage of the 'avbr' " \
    " (Average Variable BitRate) method. (e.g. 10 with a bitrate of 800 " \
    " kpbs means the encoder tries not to  go above 880 kpbs and under " \
    " 730 kpbs. The targeted accuracy is only reached after a certained " \
    " convergence period. See the convergence parameter")

#define CONVERGENCE_TEXT N_("Convergence time of 'avbr' RateControl")
#define CONVERGENCE_LONGTEXT N_("Number of 100 frames before the " \
    "'avbr' rate control method reaches the requested bitrate with " \
    "the requested accuracy. See the accuracy parameter. ")

#define NUM_SLICE_TEXT N_("Number of slices per frame")
#define NUM_SLICE_LONGTEXT N_("Number of slices in each video frame; "\
    "each slice contains one or more macro-block rows. If numslice is " \
    "not set, the encoder may choose any slice partitioning allowed " \
    "by the codec standard.")

#define NUM_REF_FRAME_TEXT N_("Number of reference frames")
#define NUM_REF_FRAME_LONGTEXT N_("Number of reference frames")

#define ASYNC_DEPTH_TEXT N_("Number of parallel operations")
#define ASYNC_DEPTH_LONGTEXT N_("Defines the number of parallel " \
     "encoding operations before we synchronise the result. Higher " \
     " may result on better throughput depending on hardware. " \
     "MPEG2 needs at least 1 here.")

static const int const profile_h264_list[] =
      { 0, MFX_PROFILE_AVC_BASELINE, MFX_PROFILE_AVC_MAIN,
      MFX_PROFILE_AVC_EXTENDED, MFX_PROFILE_AVC_HIGH };
static const char *const profile_h264_text[] =
    { "decide", "baseline", "main", "extended", "high" };

static const int const profile_mpeg2_list[] =
    { 0, MFX_PROFILE_MPEG2_SIMPLE, MFX_PROFILE_MPEG2_MAIN,
      MFX_PROFILE_MPEG2_HIGH };
static const char *const profile_mpeg2_text[] =
    { "decide", "simple", "main", "high" };

static const int const level_h264_list[] =
    { 0, 10, 9, 12, 13, 20, 21, 22, 30, 31, 32, 40, 41,   42,   50, 51, 52};
static const char *const level_h264_text[] =
    { "decide", "1", "1.1b", "1.2", "1.3", "2", "2.1", "2.2", "3", "3.1",
      "3.2", "4", "4.1",   "4.2",   "5", "5.1", "5.2" };

static const int const level_mpeg2_list[] =
    { 0, MFX_LEVEL_MPEG2_LOW, MFX_LEVEL_MPEG2_MAIN,
      MFX_LEVEL_MPEG2_HIGH, MFX_LEVEL_MPEG2_HIGH1440 };
static const char *const level_mpeg2_text[] =
    { "decide", "low", "main", "high", "high1440" };

static const int const target_usage_list[] =
    { 0, MFX_TARGETUSAGE_BEST_QUALITY, MFX_TARGETUSAGE_BALANCED,
      MFX_TARGETUSAGE_BEST_SPEED };
static const char *const target_usage_text[] =
    { "decide", "quality", "balanced", "speed" };

static const int const rc_method_list[] =
    { MFX_RATECONTROL_CBR, MFX_RATECONTROL_VBR,
      MFX_RATECONTROL_CQP, MFX_RATECONTROL_AVBR};
static const char *const rc_method_text[] =
    { "cbr", "vbr", "qp", "avbr" };

vlc_module_begin ()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_description(N_("Intel QuickSync Video encoder for MPEG4-Part10/MPEG2 (aka H.264/H.262)"))
    set_shortname("qsv")
    set_capability("encoder", 0)
    set_callbacks(Open, Close)

    add_bool(SOUT_CFG_PREFIX "software", false, SW_IMPL_TEXT, SW_IMPL_LONGTEXT, true)

    add_string(SOUT_CFG_PREFIX "h264-profile", "unspecified" , PROFILE_TEXT, PROFILE_LONGTEXT, false)
        change_string_list(profile_h264_text, profile_h264_text)

    add_string(SOUT_CFG_PREFIX "h264-level", "unspecified", LEVEL_TEXT, LEVEL_LONGTEXT, false)
        change_string_list(level_h264_text, level_h264_text)

    add_string(SOUT_CFG_PREFIX "mpeg2-profile", "unspecified", PROFILE_TEXT, PROFILE_LONGTEXT, false)
        change_string_list(profile_mpeg2_text, profile_mpeg2_text)

    add_string(SOUT_CFG_PREFIX "mpeg2-level", "unspecified", LEVEL_TEXT, LEVEL_LONGTEXT, false)
        change_string_list(level_mpeg2_text, level_mpeg2_text)

    add_integer(SOUT_CFG_PREFIX "gop-size", 32, GOP_SIZE_TEXT, GOP_SIZE_LONGTEXT, true)
    add_integer(SOUT_CFG_PREFIX "gop-refdist", 4, GOP_REF_DIST_TEXT, GOP_REF_DIST_LONGTEXT, true)
    add_integer(SOUT_CFG_PREFIX "idr-interval", 0, IDR_INTERVAL_TEXT, IDR_INTERVAL_LONGTEXT, true)

    add_string(SOUT_CFG_PREFIX "target-usage", "quality", TARGET_USAGE_TEXT, TARGET_USAGE_LONGTEXT, false)
        change_string_list(target_usage_text, target_usage_text)

    add_string(SOUT_CFG_PREFIX "rc-method", "vbr", RATE_CONTROL_TEXT, RATE_CONTROL_LONGTEXT, true)
        change_string_list(rc_method_text, rc_method_text)

    add_integer(SOUT_CFG_PREFIX "qp", 0, QP_TEXT, QP_LONGTEXT, true)
        change_integer_range(0, 51)
    add_integer(SOUT_CFG_PREFIX "qpi", 0, QPI_TEXT, QPI_LONGTEXT, true)
        change_integer_range(0, 51)
    add_integer(SOUT_CFG_PREFIX "qpp", 0, QPP_TEXT, QPP_LONGTEXT, true)
        change_integer_range(0, 51)
    add_integer(SOUT_CFG_PREFIX "qpb", 0, QPB_TEXT, QPB_LONGTEXT, true)
        change_integer_range(0, 51)

    add_integer(SOUT_CFG_PREFIX "bitrate-max", 0, MAX_BITRATE_TEXT, MAX_BITRATE_LONGTEXT, true)

    add_integer(SOUT_CFG_PREFIX "accuracy", 0, ACCURACY_TEXT, ACCURACY_LONGTEXT, true)
        change_integer_range(0, 100)

    add_integer(SOUT_CFG_PREFIX "convergence", 0, CONVERGENCE_TEXT, CONVERGENCE_LONGTEXT, true)

    add_integer(SOUT_CFG_PREFIX "num-slice", 0, NUM_SLICE_TEXT, NUM_SLICE_LONGTEXT, true)
    add_integer(SOUT_CFG_PREFIX "num-ref-frame", 0, NUM_REF_FRAME_TEXT, NUM_REF_FRAME_LONGTEXT, true)

    add_integer(SOUT_CFG_PREFIX "async-depth", 4, ASYNC_DEPTH_TEXT, ASYNC_DEPTH_LONGTEXT, true)
        change_integer_range(1, 32)

vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *const sout_options[] = {
    "software", "h264-profile", "h264-level", "mpeg2-profile", "mpeg2-level",
    "gop-size", "gop-refdist", "target-usage", "rc-method", "qp", "qpi", "qpp",
    "qpb", "bitrate-max", "accuracy", "convergence", "num-slice",
    "num-ref-frame", "async-depth",
    NULL
};

// Frame pool for QuickSync video encoder with Intel Media SDK's format frames.
typedef struct qsv_frame_pool_t
{
    mfxFrameInfo          fmt;            // IntelMediaSDK format info.
    mfxFrameSurface1      *frames;        // An allocated array of 'size' frames.
    size_t                size;           // The number of frame in the pool.
} qsv_frame_pool_t;

typedef struct async_task_t
{
    mfxBitstream     bs;                  // Intel's bitstream structure.
    mfxSyncPoint     syncp;               // Async Task Sync Point.
    block_t          *block;              // VLC's block structure to be returned by Encode.
} async_task_t;

struct encoder_sys_t
{
    mfxSession       session;             // Intel Media SDK Session.
    mfxVideoParam    params;              // Encoding parameters.
    mfxIMPL          impl;                // Actual implementation (hw/sw).
    qsv_frame_pool_t frames;              // IntelMediaSDK's frame pool.
    uint64_t         dts_warn_counter;    // DTS warning counter for rate-limiting of msg;
    uint64_t         busy_warn_counter;   // Device Bussy warning counter for rate-limiting of msg;
    uint64_t         async_depth;         // Number of parallel encoding operations.
    uint64_t         first_task;          // The next sync point to be synchronized.
    async_task_t     *tasks;              // The async encoding tasks.
    mtime_t          offset_pts;          // The pts of the first frame, to avoid conversion overflow.
    mtime_t          last_dts;            // The dts of the last frame, to interpolate over buggy dts
};

static block_t *Encode(encoder_t *, picture_t *);


static inline mtime_t qsv_timestamp_to_mtime(int64_t mfx_ts)
{
    return mfx_ts / INT64_C(9) * INT64_C(100);
}

static inline uint64_t qsv_mtime_to_timestamp(mtime_t vlc_ts)
{
    return vlc_ts / UINT64_C(100) * UINT64_C(9);
}

/*
 * Create a new frame pool with 'size' frames in it. Pools cannot be resized.
 */
static int qsv_frame_pool_Init(qsv_frame_pool_t *pool,
                               mfxFrameAllocRequest *request,
                               uint64_t async_depth)
{
    size_t size = request->NumFrameSuggested + async_depth;

    pool->frames = calloc(size, sizeof(mfxFrameSurface1));
    if (unlikely(!pool->frames))
        return VLC_ENOMEM;

    pool->size = size;
    memcpy(&pool->fmt, &request->Info, sizeof(request->Info));

    for (size_t i = 0; i < size; i++) {
        memcpy(&pool->frames[i].Info, &request->Info, sizeof(request->Info));
        pool->frames[i].Data.Pitch = QSV_ALIGN(32, request->Info.Width);
    }

    return VLC_SUCCESS;
}

/*
 * Destroys a pool frame. Only call this function after a MFXClose
 * call since we doesn't check for Locked frames.
 */
static void qsv_frame_pool_Destroy(qsv_frame_pool_t *pool)
{
    for (size_t i = 0; i < pool->size; i++) {
        picture_t *pic = (picture_t *) pool->frames[i].Data.MemId;
        if (pic)
            picture_Release(pic);
    }

    free(pool->frames);
}

/*
 * Finds an unlocked frame, releases the associated picture if
 * necessary associates the new picture with it and return the frame.
 * Returns 0 if there's an error.
 */
static mfxFrameSurface1 *qsv_frame_pool_Get(qsv_frame_pool_t *pool,
                                            picture_t *pic)
{
    for (size_t i = 0; i < pool->size; i++) {
        mfxFrameSurface1 *frame = &pool->frames[i];
        if (frame->Data.Locked)
            continue;
        if (frame->Data.MemId)
            picture_Release((picture_t *)frame->Data.MemId);

        frame->Data.MemId     = pic;
        frame->Data.Y         = pic->p[0].p_pixels;
        frame->Data.U         = pic->p[1].p_pixels;
        frame->Data.V         = pic->p[1].p_pixels + 1;
        frame->Data.TimeStamp = qsv_mtime_to_timestamp(pic->date);

        // Specify picture structure at runtime.
        if (pic->b_progressive)
            frame->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        else if (pic->b_top_field_first)
            frame->Info.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
        else
            frame->Info.PicStruct = MFX_PICSTRUCT_FIELD_BFF;

        picture_Hold(pic);

        return frame;
    }

    return NULL;
}

static uint64_t qsv_params_get_value(const char *const *text,
                                     const int const *list,
                                     size_t size, char *sel)
{
    size_t result = 0;

    if (unlikely(!sel))
        return list[0];

    size /= sizeof(list[0]);
    for (size_t i = 0; i < size; i++)
        if (!strcmp(sel, text[i])) {
            result = i;
            break;
        }

    // sel comes from var_InheritString and must be freed.
    free(sel);
    // Returns the found item, or the default/first one if not found.
    return list[result];
}

static int Open(vlc_object_t *this)
{
    encoder_t *enc = (encoder_t *)this;
    encoder_sys_t *sys = NULL;

    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest alloc_request;
    mfxExtCodingOptionSPSPPS headers;
    mfxExtBuffer *extended_params[1] = {(mfxExtBuffer *)&headers};

    uint8_t *p_extra;
    size_t i_extra;
    uint8_t nals[128];

    if (enc->fmt_out.i_codec != VLC_CODEC_H264 &&
        enc->fmt_out.i_codec != VLC_CODEC_MPGV && !enc->b_force)
        return VLC_EGENERIC;

    if (!enc->fmt_in.video.i_height || !enc->fmt_in.video.i_width ||
        !enc->fmt_in.video.i_frame_rate || !enc->fmt_in.video.i_frame_rate_base) {
        msg_Err(enc, "Framerate and picture dimensions must be non-zero");
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    sys = calloc(1, sizeof(encoder_sys_t));
    if (unlikely(!sys))
        return VLC_ENOMEM;

    /* Initialize dispatcher, it will loads the actual SW/HW Implementation */
    sts = MFXInit(MFX_IMPL_AUTO, 0, &sys->session);

    if (sts != MFX_ERR_NONE) {
        msg_Err(enc, "Unable to find an Intel Media SDK implementation.");
        free(sys);
        return VLC_EGENERIC;
    }

    config_ChainParse(enc, SOUT_CFG_PREFIX, sout_options, enc->p_cfg);

    /* Checking if we are on software and are allowing it */
    MFXQueryIMPL(sys->session, &sys->impl);
    if (!var_InheritBool(enc, SOUT_CFG_PREFIX "software") && (sys->impl & MFX_IMPL_SOFTWARE)) {
        msg_Err(enc, "No hardware implementation found and software mode disabled");
        free(sys);
        return VLC_EGENERIC;
    }

    msg_Dbg(enc, "Using Intel QuickSync Video %s implementation",
        sys->impl & MFX_IMPL_HARDWARE ? "hardware" : "software");

    /* Vlc module configuration */
    enc->p_sys                         = sys;
    enc->fmt_in.i_codec                = VLC_CODEC_NV12; // Intel Media SDK requirement
    enc->fmt_in.video.i_bits_per_pixel = 12;

    /* Input picture format description */
    sys->params.mfx.FrameInfo.FrameRateExtN = enc->fmt_in.video.i_frame_rate;
    sys->params.mfx.FrameInfo.FrameRateExtD = enc->fmt_in.video.i_frame_rate_base;
    sys->params.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
    sys->params.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
    sys->params.mfx.FrameInfo.Width         = QSV_ALIGN(16, enc->fmt_in.video.i_width);
    sys->params.mfx.FrameInfo.Height        = QSV_ALIGN(32, enc->fmt_in.video.i_height);
    sys->params.mfx.FrameInfo.CropW         = enc->fmt_in.video.i_width;
    sys->params.mfx.FrameInfo.CropH         = enc->fmt_in.video.i_height;
    sys->params.mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_UNKNOWN;

    /* Parsing options common to all RC methods and codecs */
    sys->params.IOPattern       = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    sys->params.AsyncDepth      = var_InheritInteger(enc, SOUT_CFG_PREFIX "async-depth");
    sys->params.mfx.GopPicSize  = var_InheritInteger(enc, SOUT_CFG_PREFIX "gop-size");
    sys->params.mfx.GopRefDist  = var_InheritInteger(enc, SOUT_CFG_PREFIX "gop-refdist");
    sys->params.mfx.IdrInterval = var_InheritInteger(enc, SOUT_CFG_PREFIX "idr-interval");
    sys->params.mfx.NumSlice    = var_InheritInteger(enc, SOUT_CFG_PREFIX "num-slice");
    sys->params.mfx.NumRefFrame = var_InheritInteger(enc, SOUT_CFG_PREFIX "num-ref-frame");
    sys->params.mfx.TargetUsage = qsv_params_get_value(target_usage_text,
        target_usage_list, sizeof(target_usage_list),
        var_InheritString(enc, SOUT_CFG_PREFIX "target-usage"));

    if (enc->fmt_out.i_codec == VLC_CODEC_H264) {
        sys->params.mfx.CodecId = MFX_CODEC_AVC;
        sys->params.mfx.CodecProfile = qsv_params_get_value(profile_h264_text,
            profile_h264_list, sizeof(profile_h264_list),
            var_InheritString(enc, SOUT_CFG_PREFIX "h264-profile"));
        sys->params.mfx.CodecLevel = qsv_params_get_value(level_h264_text,
            level_h264_list, sizeof(level_h264_list),
            var_InheritString(enc, SOUT_CFG_PREFIX "h264-level"));
        msg_Dbg(enc, "Encoder in H264 mode, with profile %d and level %d",
            sys->params.mfx.CodecProfile, sys->params.mfx.CodecLevel);

    } else {
        sys->params.mfx.CodecId = MFX_CODEC_MPEG2;
        sys->params.mfx.CodecProfile = qsv_params_get_value(profile_mpeg2_text,
            profile_mpeg2_list, sizeof(profile_mpeg2_list),
            var_InheritString(enc, SOUT_CFG_PREFIX "mpeg2-profile"));
        sys->params.mfx.CodecLevel = qsv_params_get_value(level_mpeg2_text,
            level_mpeg2_list, sizeof(level_mpeg2_list),
            var_InheritString(enc, SOUT_CFG_PREFIX "mpeg2-level"));
        msg_Dbg(enc, "Encoder in MPEG2 mode, with profile %d and level %d",
            sys->params.mfx.CodecProfile, sys->params.mfx.CodecLevel);
    }

    sys->params.mfx.RateControlMethod = qsv_params_get_value(rc_method_text,
        rc_method_list, sizeof(rc_method_list),
        var_InheritString(enc, SOUT_CFG_PREFIX "rc-method"));
    msg_Dbg(enc, "Encoder using '%s' Rate Control method",
        var_InheritString(enc, SOUT_CFG_PREFIX "rc-method"));

    if (sys->params.mfx.RateControlMethod == MFX_RATECONTROL_CQP) {
        sys->params.mfx.QPI = sys->params.mfx.QPB = sys->params.mfx.QPP =
            var_InheritInteger(enc, SOUT_CFG_PREFIX "qp");
        sys->params.mfx.QPI = var_InheritInteger(enc, SOUT_CFG_PREFIX "qpi");
        sys->params.mfx.QPB = var_InheritInteger(enc, SOUT_CFG_PREFIX "qpb");
        sys->params.mfx.QPP = var_InheritInteger(enc, SOUT_CFG_PREFIX "qpp");
    } else {
        if (!enc->fmt_out.i_bitrate) {
            msg_Warn(enc, "No bitrate specified, using default %d",
                QSV_BITRATE_DEFAULT);
            sys->params.mfx.TargetKbps = QSV_BITRATE_DEFAULT;
        } else
            sys->params.mfx.TargetKbps = enc->fmt_out.i_bitrate / 1000;

        if (sys->params.mfx.RateControlMethod == MFX_RATECONTROL_AVBR) {
            sys->params.mfx.Accuracy = var_InheritInteger(enc, SOUT_CFG_PREFIX "accuracy");
            sys->params.mfx.Convergence = var_InheritInteger(enc, SOUT_CFG_PREFIX "convergence");
        } else if (sys->params.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
            sys->params.mfx.MaxKbps = var_InheritInteger(enc, SOUT_CFG_PREFIX "bitrate-max");
    }

    /* Initializing MFX_Encoder */
    sts = MFXVideoENCODE_Init(sys->session, &sys->params);
    if (sts == MFX_ERR_NONE)
        msg_Dbg(enc, "Successfuly initialized video encoder");
    else if (sts < MFX_ERR_NONE) {
        msg_Err(enc, "Unable to initialize video encoder error (%d). " \
            " Most likely because of provided encoding parameters", sts);
        goto error;
    } else
        msg_Warn(enc, "Video encoder initialization : %d. The stream might be corrupted/invalid", sts);

    /* Querying PPS/SPS Headers, BufferSizeInKB, ... */
    memset(&headers, 0, sizeof(headers));
    memset(&nals, 0, sizeof(nals));
    headers.Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
    headers.Header.BufferSz = sizeof(headers);
    headers.SPSBufSize      = headers.PPSBufSize = 64;
    headers.SPSBuffer       = nals;
    headers.PPSBuffer       = nals + 64;
    sys->params.ExtParam    = (mfxExtBuffer **)&extended_params;
    sys->params.NumExtParam = 1;

    MFXVideoENCODE_GetVideoParam(sys->session, &sys->params);
    sys->params.NumExtParam = 0;
    sys->params.ExtParam = NULL;

    i_extra = headers.SPSBufSize + headers.PPSBufSize;
    p_extra = malloc(i_extra);

    if (unlikely(!p_extra))
        goto nomem;

    memcpy(p_extra, headers.SPSBuffer, headers.SPSBufSize);
    memcpy(p_extra + headers.SPSBufSize, headers.PPSBuffer, headers.PPSBufSize);
    enc->fmt_out.p_extra = p_extra;
    enc->fmt_out.i_extra = i_extra;

    sys->async_depth = sys->params.AsyncDepth;
    sys->tasks = calloc(sys->async_depth, sizeof(async_task_t));
    if (unlikely(!sys->tasks))
        goto nomem;

    /* Request number of surface needed and creating frame pool */
    if (MFXVideoENCODE_QueryIOSurf(sys->session, &sys->params, &alloc_request)!= MFX_ERR_NONE)
        goto error;
    if (qsv_frame_pool_Init(&sys->frames, &alloc_request, sys->async_depth) != VLC_SUCCESS)
        goto nomem;
    msg_Dbg(enc, "Requested %d surfaces for work", alloc_request.NumFrameSuggested);

    enc->pf_encode_video = Encode;

    return VLC_SUCCESS;

 error:
    Close(this);
    return VLC_EGENERIC;
 nomem:
    Close(this);
    return VLC_ENOMEM;
}

static void Close(vlc_object_t *this)
{
    encoder_t *enc = (encoder_t *)this;
    encoder_sys_t *sys = enc->p_sys;

    MFXVideoENCODE_Close(sys->session);
    MFXClose(sys->session);
    /* if (enc->fmt_out.p_extra) */
    /*     free(enc->fmt_out.p_extra); */
    if (sys->frames.size)
        qsv_frame_pool_Destroy(&sys->frames);
    if (sys->tasks)
        free(sys->tasks);
    free(sys);
}

/*
 * The behavior in the next function comes from x264.c
 */
static void qsv_set_block_flags(block_t *block, uint16_t frame_type)
{
    if ((frame_type & MFX_FRAMETYPE_IDR) || (frame_type & MFX_FRAMETYPE_REF))
        block->i_flags = BLOCK_FLAG_TYPE_I;
    else if ((frame_type & MFX_FRAMETYPE_P) || (frame_type & MFX_FRAMETYPE_I))
        block->i_flags = BLOCK_FLAG_TYPE_P;
    else if (frame_type & MFX_FRAMETYPE_B)
        block->i_flags = BLOCK_FLAG_TYPE_B;
    else
        block->i_flags = BLOCK_FLAG_TYPE_PB;
}

/*
 * Convert the Intel Media SDK's timestamps into VLC's format.
 */
static void qsv_set_block_ts(encoder_t *enc, encoder_sys_t *sys, block_t *block, mfxBitstream *bs)
{
    if (!bs->TimeStamp)
        return;

    block->i_pts = qsv_timestamp_to_mtime(bs->TimeStamp) + sys->offset_pts;
    block->i_dts = qsv_timestamp_to_mtime(bs->DecodeTimeStamp) + sys->offset_pts;

    /* HW encoder (with old driver versions) and some parameters
       combinations doesn't set the DecodeTimeStamp field so we warn
       the user about it */
    if (!bs->DecodeTimeStamp || bs->DecodeTimeStamp > (int64_t)bs->TimeStamp)
        if (sys->dts_warn_counter++ % 16 == 0) // Rate limiting this message.
            msg_Warn(enc, "Encode returning empty DTS or DTS > PTS. Your stream will be invalid. "
                     " Please double-check they weren't any warning at encoder initialization "
                     " and that you have the last version of Intel's drivers installed.");
}


/*
 * The Encode function has 3 encoding phases :
 *   - Feed : We feed the encoder until it stops to return MFX_MORE_DATA_NEEDED
 *     and the async_tasks are all in use (honouring the AsyncDepth)
 *   - Main encoding phase : synchronizing the oldest task each call.
 *   - Empty : pic = 0, we empty the decoder. Synchronizing the remaining tasks.
 */
static block_t *Encode(encoder_t *this, picture_t *pic)
{
    encoder_t     *enc = (encoder_t *)this;
    encoder_sys_t *sys = enc->p_sys;
    async_task_t  *task = NULL;
    block_t       *block = NULL;

    mfxFrameSurface1 *frame = NULL;
    mfxStatus        sts;

    if (pic) {
        /* To avoid qsv -> vlc timestamp conversion overflow, we use timestamp relative
           to the first picture received. That way, vlc will overflow before us.
           (Thanks to funman for the idea) */
        if (!sys->offset_pts) // First frame
            sys->offset_pts = pic->date;
        pic->date -= sys->offset_pts;

        frame = qsv_frame_pool_Get(&sys->frames, pic);
        if (!frame) {
            msg_Warn(enc, "Unable to find an unlocked surface in the pool");
            return NULL;
        }

        /* Finds an available SyncPoint */
        for (size_t i = 0; i < sys->async_depth; i++)
            if ((sys->tasks + (i + sys->first_task) % sys->async_depth)->syncp == 0) {
                task = sys->tasks + (i + sys->first_task) % sys->async_depth;
                break;
            }
    } else
        /* If !pic, we are emptying encoder and tasks, so we force the SyncOperation */
        msg_Dbg(enc, "Emptying encoder");

    /* There is no available task, we need to synchronize */
    if (!task) {
        task = sys->tasks + sys->first_task;

        /* Synchronize and fill block_t. If the SyncOperation fails we leak :-/ (or we can segfault, ur choice) */
        if (MFXVideoCORE_SyncOperation(sys->session, task->syncp, QSV_SYNCPOINT_WAIT) == MFX_ERR_NONE) {
            block = task->block;
            block->i_buffer = task->bs.DataLength;
            block->p_buffer += task->bs.DataOffset;

            qsv_set_block_ts(enc, sys, block, &task->bs);
            qsv_set_block_flags(block, task->bs.FrameType);

            /* msg_Dbg(enc, "block->i_pts = %lld, block->i_dts = %lld", block->i_pts, block->i_dts); */
            /* msg_Dbg(enc, "FrameType = %#.4x, TimeStamp (pts) = %lld, DecodeTimeStamp = %lld", */
            /*         task->bs.FrameType, task->bs.TimeStamp, task->bs.DecodeTimeStamp); */

            /* Copied from x264.c: This isn't really valid for streams with B-frames */
            block->i_length = CLOCK_FREQ *
                enc->fmt_in.video.i_frame_rate_base /
                enc->fmt_in.video.i_frame_rate;

            // Buggy DTS (value comes from experiments)
            if (task->bs.DecodeTimeStamp < -10000)
                block->i_dts = sys->last_dts + block->i_length;
            sys->last_dts = block->i_dts;
        } else // Only happens on buggy drivers
            msg_Err(enc, "SyncOperation failed, outputting garbage data. "
                    "Updating your drivers and/or changing the encoding settings might resolve this");

        /* Reset the task now it has been synchronized and advances first_task pointer */
        task->syncp = 0;
        sys->first_task = (sys->first_task + 1) % sys->async_depth;
    }

    /* Allocate block_t and prepare mfxBitstream for encoder */
    if (!(task->block = block_Alloc(sys->params.mfx.BufferSizeInKB * 1000))) {
        msg_Err(enc, "Unable to allocate block for encoder output");
        return NULL;
    }
    memset(&task->bs, 0, sizeof(task->bs));
    task->bs.MaxLength = sys->params.mfx.BufferSizeInKB * 1000;
    task->bs.Data = task->block->p_buffer;

    for (;;) {
        sts = MFXVideoENCODE_EncodeFrameAsync(sys->session, 0, frame, &task->bs, &task->syncp);
        if (sts != MFX_WRN_DEVICE_BUSY)
            break;
        if (sys->busy_warn_counter++ % 16 == 0)
            msg_Dbg(enc, "Device is busy, let's wait and retry");
        msleep(QSV_BUSYWAIT_TIME);
    }

    // msg_Dbg(enc, "Encode async status: %d, Syncpoint = %tx", sts, (ptrdiff_t)task->syncp);

    if (sts == MFX_ERR_MORE_DATA)
        if (pic)
            msg_Dbg(enc, "Encoder feeding phase, more data is needed.");
        else
            msg_Dbg(enc, "Encoder is empty");
    else if (sts < MFX_ERR_NONE) {
        msg_Err(enc, "Encoder not ready or error (%d), trying a reset...", sts);
        MFXVideoENCODE_Reset(sys->session, &sys->params);
    }

    return block;
}
