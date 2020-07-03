/*****************************************************************************
 * videotoolbox.c: Video Toolbox decoder
 *****************************************************************************
 * Copyright © 2014-2015 VideoLabs SAS
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan.org>
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

#pragma mark preamble

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import <vlc_common.h>
#import <vlc_plugin.h>
#import <vlc_codec.h>
#import "hxxx_helper.h"
#import <vlc_bits.h>
#import <vlc_boxes.h>
#import "vt_utils.h"
#import "../packetizer/h264_nal.h"
#import "../packetizer/h264_slice.h"
#import "../packetizer/hxxx_nal.h"
#import "../packetizer/hxxx_sei.h"

#import <VideoToolbox/VideoToolbox.h>
#import <VideoToolbox/VTErrors.h>

#import <CoreFoundation/CoreFoundation.h>
#import <TargetConditionals.h>

#import <sys/types.h>
#import <sys/sysctl.h>
#import <mach/machine.h>

#define VT_ALIGNMENT 16
#define VT_RESTART_MAX 1

#pragma mark - local prototypes

enum vtsession_status
{
    VTSESSION_STATUS_OK,
    VTSESSION_STATUS_RESTART,
    VTSESSION_STATUS_RESTART_CHROMA,
    VTSESSION_STATUS_ABORT,
};

static int ConfigureVout(decoder_t *);
static CFDictionaryRef ESDSExtradataInfoCreate(decoder_t *, uint8_t *, uint32_t);
static CFDictionaryRef ExtradataInfoCreate(CFStringRef, void *, size_t);
static CFMutableDictionaryRef CreateSessionDescriptionFormat(decoder_t *, unsigned, unsigned);
static int HandleVTStatus(decoder_t *, OSStatus, enum vtsession_status *);
static int DecodeBlock(decoder_t *, block_t *);
static void RequestFlush(decoder_t *);
static void Drain(decoder_t *p_dec, bool flush);
static void DecoderCallback(void *, void *, OSStatus, VTDecodeInfoFlags,
                            CVPixelBufferRef, CMTime, CMTime);
static Boolean deviceSupportsHEVC();
static Boolean deviceSupportsAdvancedProfiles();
static Boolean deviceSupportsAdvancedLevels();

typedef struct frame_info_t frame_info_t;

struct frame_info_t
{
    picture_t *p_picture;
    int i_poc;
    int i_foc;
    bool b_flush;
    bool b_eos;
    bool b_keyframe;
    bool b_field;
    bool b_progressive;
    bool b_top_field_first;
    uint8_t i_num_ts;
    unsigned i_length;
    frame_info_t *p_next;
};

#pragma mark - decoder structure

#define H264_MAX_DPB 16
#define VT_MAX_SEI_COUNT 16

typedef struct decoder_sys_t
{
    CMVideoCodecType            codec;
    struct                      hxxx_helper hh;

    /* Codec specific callbacks */
    bool                        (*pf_codec_init)(decoder_t *);
    void                        (*pf_codec_clean)(decoder_t *);
    bool                        (*pf_codec_supported)(decoder_t *);
    bool                        (*pf_late_start)(decoder_t *);
    block_t*                    (*pf_process_block)(decoder_t *,
                                                    block_t *, bool *);
    bool                        (*pf_need_restart)(decoder_t *,
                                                   VTDecompressionSessionRef);
    bool                        (*pf_configure_vout)(decoder_t *);
    CFDictionaryRef             (*pf_copy_extradata)(decoder_t *);
    bool                        (*pf_fill_reorder_info)(decoder_t *, const block_t *,
                                                        frame_info_t *);
    /* !Codec specific callbacks */

    bool                        b_vt_feed;
    bool                        b_vt_flush;
    bool                        b_vt_need_keyframe;
    VTDecompressionSessionRef   session;
    CMVideoFormatDescriptionRef videoFormatDescription;

    vlc_mutex_t                 lock;
    frame_info_t               *p_pic_reorder;
    uint8_t                     i_pic_reorder;
    uint8_t                     i_pic_reorder_max;
    bool                        b_invalid_pic_reorder_max;
    bool                        b_poc_based_reorder;

    bool                        b_format_propagated;

    enum vtsession_status       vtsession_status;
    unsigned                    i_restart_count;

    OSType                      i_cvpx_format;
    bool                        b_cvpx_format_forced;

    h264_poc_context_t          h264_pocctx;
    hevc_poc_ctx_t              hevc_pocctx;
    bool                        b_drop_blocks;
    date_t                      pts;

    vlc_video_context          *vctx;
    struct pic_pacer           *pic_pacer;
} decoder_sys_t;

/* Picture pacer to work-around the VT session allocating too much CVPX buffers
 * that can lead to a OOM. cf. pic_pacer_Wait usage in DecoderCallback() */
struct pic_pacer
{
    vlc_mutex_t lock;
    vlc_cond_t  wait;
    uint8_t     nb_field_out;
    uint8_t     field_reorder_max;
};

static void pic_pacer_UpdateReorderMax(struct pic_pacer *, uint8_t, uint8_t);

#pragma mark - start & stop

/* Codec Specific */

static void HXXXGetBestChroma(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->i_cvpx_format != 0 || p_sys->b_cvpx_format_forced)
        return;

    uint8_t i_chroma_format, i_depth_luma, i_depth_chroma;
    if (hxxx_helper_get_chroma_chroma(&p_sys->hh, &i_chroma_format, &i_depth_luma,
                                      &i_depth_chroma) != VLC_SUCCESS)
        return;

    if (i_chroma_format == 1 /* YUV 4:2:0 */)
    {
        if (i_depth_luma == 8 && i_depth_chroma == 8)
            p_sys->i_cvpx_format = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
#if !TARGET_OS_IPHONE
        /* Not for iOS since there is no 10bits textures with the old iOS
         * openGLES version, and therefore no P010 shaders */
        else if (i_depth_luma == 10 && i_depth_chroma == 10 && deviceSupportsHEVC())
            p_sys->i_cvpx_format = kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
#endif
    }
}

static void GetxPSH264(uint8_t i_pps_id, void *priv,
                      const h264_sequence_parameter_set_t **pp_sps,
                      const h264_picture_parameter_set_t **pp_pps)
{
    decoder_sys_t *p_sys = priv;

    *pp_pps = p_sys->hh.h264.pps_list[i_pps_id].h264_pps;
    if (*pp_pps == NULL)
        *pp_sps = NULL;
    else
        *pp_sps = p_sys->hh.h264.sps_list[(*pp_pps)->i_sps_id].h264_sps;
}

struct sei_callback_h264_s
{
    uint8_t i_pic_struct;
    const h264_sequence_parameter_set_t *p_sps;
};

static bool ParseH264SEI(const hxxx_sei_data_t *p_sei_data, void *priv)
{
    if (p_sei_data->i_type == HXXX_SEI_PIC_TIMING)
    {
        struct sei_callback_h264_s *s = priv;
        if (s->p_sps && s->p_sps->vui.b_valid)
        {
            if (s->p_sps->vui.b_hrd_parameters_present_flag)
            {
                bs_read(p_sei_data->p_bs, s->p_sps->vui.i_cpb_removal_delay_length_minus1 + 1);
                bs_read(p_sei_data->p_bs, s->p_sps->vui.i_dpb_output_delay_length_minus1 + 1);
            }

            if (s->p_sps->vui.b_pic_struct_present_flag)
                s->i_pic_struct = bs_read( p_sei_data->p_bs, 4);
        }
        return false;
    }

    return true;
}

static bool FillReorderInfoH264(decoder_t *p_dec, const block_t *p_block,
                                frame_info_t *p_info)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    hxxx_iterator_ctx_t itctx;
    hxxx_iterator_init(&itctx, p_block->p_buffer, p_block->i_buffer,
                       p_sys->hh.i_nal_length_size);

    const uint8_t *p_nal; size_t i_nal;
    struct
    {
        const uint8_t *p_nal;
        size_t i_nal;
    } sei_array[VT_MAX_SEI_COUNT];
    size_t i_sei_count = 0;
    while (hxxx_iterate_next(&itctx, &p_nal, &i_nal))
    {
        if (i_nal < 2)
            continue;

        const enum h264_nal_unit_type_e i_nal_type = p_nal[0] & 0x1F;

        if (i_nal_type <= H264_NAL_SLICE_IDR && i_nal_type != H264_NAL_UNKNOWN)
        {
            h264_slice_t slice;
            if (!h264_decode_slice(p_nal, i_nal, GetxPSH264, p_sys, &slice))
                return false;

            const h264_sequence_parameter_set_t *p_sps;
            const h264_picture_parameter_set_t *p_pps;
            GetxPSH264(slice.i_pic_parameter_set_id, p_sys, &p_sps, &p_pps);
            if (p_sps)
            {
                int bFOC;
                h264_compute_poc(p_sps, &slice, &p_sys->h264_pocctx,
                                 &p_info->i_poc, &p_info->i_foc, &bFOC);

                p_info->b_keyframe = slice.type == H264_SLICE_TYPE_I;
                p_info->b_flush = (slice.type == H264_SLICE_TYPE_I) || slice.has_mmco5;
                p_info->b_field = slice.i_field_pic_flag;
                p_info->b_progressive = !p_sps->mb_adaptive_frame_field_flag &&
                                        !slice.i_field_pic_flag;

                struct sei_callback_h264_s sei;
                sei.p_sps = p_sps;
                sei.i_pic_struct = UINT8_MAX;

                for (size_t i = 0; i < i_sei_count; i++)
                    HxxxParseSEI(sei_array[i].p_nal, sei_array[i].i_nal, 1,
                                 ParseH264SEI, &sei);

                p_info->i_num_ts = h264_get_num_ts(p_sps, &slice, sei.i_pic_struct,
                                                   p_info->i_foc, bFOC);

                if (!p_info->b_progressive)
                    p_info->b_top_field_first = (sei.i_pic_struct % 2 == 1);

                /* Set frame rate for timings in case of missing rate */
                if ( (!p_dec->fmt_in.video.i_frame_rate_base ||
                      !p_dec->fmt_in.video.i_frame_rate) &&
                     p_sps->vui.i_time_scale && p_sps->vui.i_num_units_in_tick )
                {
                    date_Change( &p_sys->pts, p_sps->vui.i_time_scale,
                                              p_sps->vui.i_num_units_in_tick );
                }

                if(!p_sys->b_invalid_pic_reorder_max && i_nal_type == H264_NAL_SLICE_IDR)
                {
                    unsigned dummy;
                    uint8_t i_reorder;
                    h264_get_dpb_values(p_sps, &i_reorder, &dummy);
                    vlc_mutex_lock(&p_sys->lock);
                    p_sys->i_pic_reorder_max = i_reorder;
                    pic_pacer_UpdateReorderMax(p_sys->pic_pacer,
                                                  p_sys->i_pic_reorder_max,
                                                  p_info->i_num_ts);
                    vlc_mutex_unlock(&p_sys->lock);
                }

            }

            return true; /* No need to parse further NAL */
        }
        else if (i_nal_type == H264_NAL_SEI)
        {
            if (i_sei_count < VT_MAX_SEI_COUNT)
            {
                sei_array[i_sei_count].p_nal = p_nal;
                sei_array[i_sei_count++].i_nal = i_nal;
            }
        }
    }

    return false;
}


static block_t *ProcessBlockH264(decoder_t *p_dec, block_t *p_block, bool *pb_config_changed)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    return p_sys->hh.pf_process_block(&p_sys->hh, p_block, pb_config_changed);
}


static bool InitH264(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    h264_poc_context_init(&p_sys->h264_pocctx);
    hxxx_helper_init(&p_sys->hh, VLC_OBJECT(p_dec),
                     p_dec->fmt_in.i_codec, true);
    return hxxx_helper_set_extra(&p_sys->hh, p_dec->fmt_in.p_extra,
                                             p_dec->fmt_in.i_extra) == VLC_SUCCESS;
}

static void CleanH264(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    hxxx_helper_clean(&p_sys->hh);
}

static CFDictionaryRef CopyDecoderExtradataH264(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    CFDictionaryRef extradata = NULL;
    if (p_dec->fmt_in.i_extra && p_sys->hh.b_is_xvcC)
    {
        /* copy DecoderConfiguration */
        extradata = ExtradataInfoCreate(CFSTR("avcC"),
                                        p_dec->fmt_in.p_extra,
                                        p_dec->fmt_in.i_extra);
    }
    else if (p_sys->hh.h264.i_pps_count && p_sys->hh.h264.i_sps_count)
    {
        /* build DecoderConfiguration from gathered */
        block_t *p_avcC = h264_helper_get_avcc_config(&p_sys->hh);
        if (p_avcC)
        {
            extradata = ExtradataInfoCreate(CFSTR("avcC"),
                                            p_avcC->p_buffer,
                                            p_avcC->i_buffer);
            block_Release(p_avcC);
        }
    }
    return extradata;
}

static bool CodecSupportedH264(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    uint8_t i_profile, i_level;
    if (hxxx_helper_get_current_profile_level(&p_sys->hh, &i_profile, &i_level))
        return true;

    switch (i_profile) {
        case PROFILE_H264_BASELINE:
        case PROFILE_H264_MAIN:
        case PROFILE_H264_HIGH:
            break;

        case PROFILE_H264_HIGH_10:
        {
            if (deviceSupportsAdvancedProfiles())
                break;
            else
            {
                msg_Err(p_dec, "current device doesn't support H264 10bits");
                return false;
            }
        }

        default:
        {
            msg_Warn(p_dec, "unknown H264 profile %" PRIx8, i_profile);
            return false;
        }
    }

    /* A level higher than 5.2 was not tested, so don't dare to try to decode
     * it. On SoC A8, 4.2 is the highest specified profile. on Twister, we can
     * do up to 5.2 */
    if (i_level > 52 || (i_level > 42 && !deviceSupportsAdvancedLevels()))
    {
        msg_Err(p_dec, "current device doesn't support this H264 level: %"
                PRIx8, i_level);
        return false;
    }

    HXXXGetBestChroma(p_dec);

    return true;
}

static bool LateStartH264(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    return (p_dec->fmt_in.i_extra == 0 &&
            (!p_sys->hh.h264.i_pps_count || !p_sys->hh.h264.i_sps_count) );
}

static bool ConfigureVoutH264(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_dec->fmt_in.video.primaries == COLOR_PRIMARIES_UNDEF)
    {
        video_color_primaries_t primaries;
        video_transfer_func_t transfer;
        video_color_space_t colorspace;
        video_color_range_t full_range;
        if (hxxx_helper_get_colorimetry(&p_sys->hh,
                                        &primaries,
                                        &transfer,
                                        &colorspace,
                                        &full_range) == VLC_SUCCESS)
        {
            p_dec->fmt_out.video.primaries = primaries;
            p_dec->fmt_out.video.transfer = transfer;
            p_dec->fmt_out.video.space = colorspace;
            p_dec->fmt_out.video.color_range = full_range;
        }
    }

    if (!p_dec->fmt_in.video.i_visible_width || !p_dec->fmt_in.video.i_visible_height)
    {
        unsigned i_width, i_height, i_vis_width, i_vis_height;
        if (VLC_SUCCESS ==
           hxxx_helper_get_current_picture_size(&p_sys->hh,
                                                &i_width, &i_height,
                                                &i_vis_width, &i_vis_height))
        {
            p_dec->fmt_out.video.i_visible_width = i_vis_width;
            p_dec->fmt_out.video.i_width = vlc_align( i_vis_width, VT_ALIGNMENT );
            p_dec->fmt_out.video.i_visible_height = i_vis_height;
            p_dec->fmt_out.video.i_height = vlc_align( i_vis_height, VT_ALIGNMENT );
        }
        else return false;
    }

    if (!p_dec->fmt_in.video.i_sar_num || !p_dec->fmt_in.video.i_sar_den)
    {
        int i_sar_num, i_sar_den;
        if (VLC_SUCCESS ==
            hxxx_helper_get_current_sar(&p_sys->hh, &i_sar_num, &i_sar_den))
        {
            p_dec->fmt_out.video.i_sar_num = i_sar_num;
            p_dec->fmt_out.video.i_sar_den = i_sar_den;
        }
    }

    return true;
}

static bool VideoToolboxNeedsToRestartH264(decoder_t *p_dec,
                                           VTDecompressionSessionRef session)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    const struct hxxx_helper *hh = &p_sys->hh;

    unsigned w, h, vw, vh;
    int sarn, sard;

    if (hxxx_helper_get_current_picture_size(hh, &w, &h, &vw, &vh) != VLC_SUCCESS)
        return true;

    if (hxxx_helper_get_current_sar(hh, &sarn, &sard) != VLC_SUCCESS)
        return true;

    bool b_ret = true;

    CFMutableDictionaryRef decoderConfiguration =
            CreateSessionDescriptionFormat(p_dec, sarn, sard);
    if (decoderConfiguration != NULL)
    {
        CMFormatDescriptionRef newvideoFormatDesc;
        /* create new video format description */
        OSStatus status = CMVideoFormatDescriptionCreate(kCFAllocatorDefault,
                                                         p_sys->codec,
                                                         vw, vh,
                                                         decoderConfiguration,
                                                         &newvideoFormatDesc);
        if (!status)
        {
            b_ret = !VTDecompressionSessionCanAcceptFormatDescription(session,
                                                                      newvideoFormatDesc);
            CFRelease(newvideoFormatDesc);
        }
        CFRelease(decoderConfiguration);
    }

    return b_ret;
}

static bool InitHEVC(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    hevc_poc_cxt_init(&p_sys->hevc_pocctx);
    hxxx_helper_init(&p_sys->hh, VLC_OBJECT(p_dec),
                     p_dec->fmt_in.i_codec, true);
    return hxxx_helper_set_extra(&p_sys->hh, p_dec->fmt_in.p_extra,
                                             p_dec->fmt_in.i_extra) == VLC_SUCCESS;
}

#define CleanHEVC CleanH264

static void GetxPSHEVC(uint8_t i_id, void *priv,
                       hevc_picture_parameter_set_t **pp_pps,
                       hevc_sequence_parameter_set_t **pp_sps,
                       hevc_video_parameter_set_t **pp_vps)
{
    decoder_sys_t *p_sys = priv;

    *pp_pps = p_sys->hh.hevc.pps_list[i_id].hevc_pps;
    if (*pp_pps == NULL)
    {
        *pp_vps = NULL;
        *pp_sps = NULL;
    }
    else
    {
        uint8_t i_sps_id = hevc_get_pps_sps_id(*pp_pps);
        *pp_sps = p_sys->hh.hevc.sps_list[i_sps_id].hevc_sps;
        if (*pp_sps == NULL)
            *pp_vps = NULL;
        else
        {
            uint8_t i_vps_id = hevc_get_sps_vps_id(*pp_sps);
            *pp_vps = p_sys->hh.hevc.vps_list[i_vps_id].hevc_vps;
        }
    }
}

struct hevc_sei_callback_s
{
    hevc_sei_pic_timing_t *p_timing;
    const hevc_sequence_parameter_set_t *p_sps;
};

static bool ParseHEVCSEI(const hxxx_sei_data_t *p_sei_data, void *priv)
{
    if (p_sei_data->i_type == HXXX_SEI_PIC_TIMING)
    {
        struct hevc_sei_callback_s *s = priv;
        if (likely(s->p_sps))
            s->p_timing = hevc_decode_sei_pic_timing(p_sei_data->p_bs, s->p_sps);
        return false;
    }
    return true;
}

static bool FillReorderInfoHEVC(decoder_t *p_dec, const block_t *p_block,
                                frame_info_t *p_info)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    hxxx_iterator_ctx_t itctx;
    hxxx_iterator_init(&itctx, p_block->p_buffer, p_block->i_buffer,
                       p_sys->hh.i_nal_length_size);

    const uint8_t *p_nal; size_t i_nal;
    struct
    {
        const uint8_t *p_nal;
        size_t i_nal;
    } sei_array[VT_MAX_SEI_COUNT];
    size_t i_sei_count = 0;

    while(hxxx_iterate_next(&itctx, &p_nal, &i_nal))
    {
        if (i_nal < 2 || hevc_getNALLayer(p_nal) > 0)
            continue;

        const enum hevc_nal_unit_type_e i_nal_type = hevc_getNALType(p_nal);
        if (i_nal_type <= HEVC_NAL_IRAP_VCL23)
        {
            hevc_slice_segment_header_t *p_sli =
                    hevc_decode_slice_header(p_nal, i_nal, true, GetxPSHEVC, p_sys);
            if (!p_sli)
                return false;

            /* XXX: Work-around a VT bug on recent devices (iPhone X, MacBook
             * Pro 2017). The VT session will report a BadDataErr if you send a
             * RASL frame just after a CRA one. Indeed, RASL frames are
             * corrupted if the decoding start at an IRAP frame (IDR/CRA), VT
             * is likely failing to handle this case. */
            if (!p_sys->b_vt_feed && (i_nal_type != HEVC_NAL_IDR_W_RADL &&
                                      i_nal_type != HEVC_NAL_IDR_N_LP))
                p_sys->b_drop_blocks = true;
            else if (p_sys->b_drop_blocks)
            {
                if (i_nal_type == HEVC_NAL_RASL_N || i_nal_type == HEVC_NAL_RASL_R)
                {
                    hevc_rbsp_release_slice_header(p_sli);
                    return false;
                }
                else
                {
                    p_sys->b_drop_blocks = false;
                }
            }

            p_info->b_keyframe = i_nal_type >= HEVC_NAL_BLA_W_LP;
            enum hevc_slice_type_e slice_type;
            if (hevc_get_slice_type(p_sli, &slice_type))
            {
                p_info->b_keyframe |= (slice_type == HEVC_SLICE_TYPE_I);
            }

            hevc_sequence_parameter_set_t *p_sps;
            hevc_picture_parameter_set_t *p_pps;
            hevc_video_parameter_set_t *p_vps;
            GetxPSHEVC(hevc_get_slice_pps_id(p_sli), p_sys, &p_pps, &p_sps, &p_vps);
            if (p_sps)
            {
                struct hevc_sei_callback_s sei;
                sei.p_sps = p_sps;
                sei.p_timing = NULL;

                const int POC = hevc_compute_picture_order_count(p_sps, p_sli,
                                                                 &p_sys->hevc_pocctx);

                for (size_t i=0; i<i_sei_count; i++)
                    HxxxParseSEI(sei_array[i].p_nal, sei_array[i].i_nal,
                                 2, ParseHEVCSEI, &sei);

                p_info->i_poc = POC;
                p_info->i_foc = POC; /* clearly looks wrong :/ */
                p_info->i_num_ts = hevc_get_num_clock_ts(p_sps, sei.p_timing);
                p_info->b_flush = (POC == 0);
                p_info->b_field = (p_info->i_num_ts == 1);
                p_info->b_progressive = hevc_frame_is_progressive(p_sps, sei.p_timing);

                /* Set frame rate for timings in case of missing rate */
                if ( (!p_dec->fmt_in.video.i_frame_rate_base ||
                     !p_dec->fmt_in.video.i_frame_rate) )
                {
                    unsigned num, den;
                    if (hevc_get_frame_rate(p_sps, p_vps, &num, &den))
                        date_Change(&p_sys->pts, num, den);
                }

                if (sei.p_timing)
                    hevc_release_sei_pic_timing(sei.p_timing);

                if (!p_sys->b_invalid_pic_reorder_max && p_vps)
                {
                    vlc_mutex_lock(&p_sys->lock);
                    p_sys->i_pic_reorder_max = hevc_get_max_num_reorder(p_vps);
                    pic_pacer_UpdateReorderMax(p_sys->pic_pacer,
                                                  p_sys->i_pic_reorder_max,
                                                  p_info->i_num_ts);
                    vlc_mutex_unlock(&p_sys->lock);
                }

            }

            hevc_rbsp_release_slice_header(p_sli);
            return true; /* No need to parse further NAL */
        }
        else if (i_nal_type == HEVC_NAL_PREF_SEI)
        {
            if (i_sei_count < VT_MAX_SEI_COUNT)
            {
                sei_array[i_sei_count].p_nal = p_nal;
                sei_array[i_sei_count++].i_nal = i_nal;
            }
        }
    }

    return false;
}

static CFDictionaryRef CopyDecoderExtradataHEVC(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    CFDictionaryRef extradata = NULL;
    if (p_dec->fmt_in.i_extra && p_sys->hh.b_is_xvcC)
    {
        /* copy DecoderConfiguration */
        extradata = ExtradataInfoCreate(CFSTR("hvcC"),
                                        p_dec->fmt_in.p_extra,
                                        p_dec->fmt_in.i_extra);
    }
    else if (p_sys->hh.hevc.i_pps_count &&
             p_sys->hh.hevc.i_sps_count &&
             p_sys->hh.hevc.i_vps_count)
    {
        /* build DecoderConfiguration from gathered */
        block_t *p_hvcC = hevc_helper_get_hvcc_config(&p_sys->hh);
        if (p_hvcC)
        {
            extradata = ExtradataInfoCreate(CFSTR("hvcC"),
                                            p_hvcC->p_buffer,
                                            p_hvcC->i_buffer);
            block_Release(p_hvcC);
        }
    }
    return extradata;
}

static bool LateStartHEVC(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    return (p_dec->fmt_in.i_extra == 0 &&
            (!p_sys->hh.hevc.i_pps_count ||
             !p_sys->hh.hevc.i_sps_count ||
             !p_sys->hh.hevc.i_vps_count) );
}

static bool CodecSupportedHEVC(decoder_t *p_dec)
{
    HXXXGetBestChroma(p_dec);

    return true;
}

#define ConfigureVoutHEVC ConfigureVoutH264
#define ProcessBlockHEVC ProcessBlockH264
#define VideoToolboxNeedsToRestartHEVC VideoToolboxNeedsToRestartH264

static CFDictionaryRef CopyDecoderExtradataMPEG4(decoder_t *p_dec)
{
    if (p_dec->fmt_in.i_extra)
        return ESDSExtradataInfoCreate(p_dec, p_dec->fmt_in.p_extra,
                                              p_dec->fmt_in.i_extra);
    else
        return NULL; /* MPEG4 without esds ? */
}

/* !Codec Specific */

static void InsertIntoDPB(decoder_sys_t *p_sys, frame_info_t *p_info)
{
    frame_info_t **pp_lead_in = &p_sys->p_pic_reorder;

    for ( ;; pp_lead_in = & ((*pp_lead_in)->p_next))
    {
        bool b_insert;
        if (*pp_lead_in == NULL)
            b_insert = true;
        else if (p_sys->b_poc_based_reorder)
            b_insert = ((*pp_lead_in)->i_foc > p_info->i_foc);
        else
            b_insert = ((*pp_lead_in)->p_picture->date >= p_info->p_picture->date);

        if (b_insert)
        {
            p_info->p_next = *pp_lead_in;
            *pp_lead_in = p_info;
            p_sys->i_pic_reorder += (p_info->b_field) ? 1 : 2;
            break;
        }
    }
#if 0
    for (frame_info_t *p_in=p_sys->p_pic_reorder; p_in; p_in = p_in->p_next)
        printf(" %d", p_in->i_foc);
    printf("\n");
#endif
}

static picture_t * RemoveOneFrameFromDPB(decoder_sys_t *p_sys)
{
    frame_info_t *p_info = p_sys->p_pic_reorder;
    if (p_info == NULL)
        return NULL;

    const int i_framepoc = p_info->i_poc;

    picture_t *p_ret = NULL;
    picture_t **pp_ret_last = &p_ret;
    bool b_dequeue;

    do
    {
        picture_t *p_field = p_info->p_picture;

        /* Compute time if missing */
        if (p_field->date == VLC_TICK_INVALID)
            p_field->date = date_Get(&p_sys->pts);
        else
            date_Set(&p_sys->pts, p_field->date);

        /* Set next picture time, in case it is missing */
        if (p_info->i_length)
            date_Set(&p_sys->pts, p_field->date + p_info->i_length);
        else
            date_Increment(&p_sys->pts, p_info->i_num_ts);

        *pp_ret_last = p_field;
        pp_ret_last = &p_field->p_next;

        p_sys->i_pic_reorder -= (p_info->b_field) ? 1 : 2;

        p_sys->p_pic_reorder = p_info->p_next;
        free(p_info);
        p_info = p_sys->p_pic_reorder;

        if (p_info)
        {
            if (p_sys->b_poc_based_reorder)
                b_dequeue = (p_info->i_poc == i_framepoc);
            else
                b_dequeue = (p_field->date == p_info->p_picture->date);
        }
        else
        {
            b_dequeue = false;
        }

    } while(b_dequeue);

    return p_ret;
}

static void DrainDPBLocked(decoder_t *p_dec, bool flush)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    for ( ;; )
    {
        picture_t *p_fields = RemoveOneFrameFromDPB(p_sys);
        if (p_fields == NULL)
            break;
        do
        {
            picture_t *p_next = p_fields->p_next;
            p_fields->p_next = NULL;
            if (flush)
                picture_Release(p_fields);
            else
                decoder_QueueVideo(p_dec, p_fields);
            p_fields = p_next;
        } while(p_fields != NULL);
    }
}

static frame_info_t * CreateReorderInfo(decoder_t *p_dec, const block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    frame_info_t *p_info = calloc(1, sizeof(*p_info));
    if (!p_info)
        return NULL;

    if (p_sys->pf_fill_reorder_info)
    {
        if (!p_sys->pf_fill_reorder_info(p_dec, p_block, p_info))
        {
            free(p_info);
            return NULL;
        }
    }
    else
    {
        p_info->i_num_ts = 2;
        p_info->b_progressive = true;
        p_info->b_field = false;
        p_info->b_keyframe = true;
    }

    p_info->i_length = p_block->i_length;

    /* required for still pictures/menus */
    p_info->b_eos = (p_block->i_flags & BLOCK_FLAG_END_OF_SEQUENCE);

    if (date_Get(&p_sys->pts) == VLC_TICK_INVALID)
        date_Set(&p_sys->pts, p_block->i_dts);

    return p_info;
}

static void OnDecodedFrame(decoder_t *p_dec, frame_info_t *p_info)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    assert(p_info->p_picture);
    while(p_info->b_flush || p_sys->i_pic_reorder >= (p_sys->i_pic_reorder_max * 2))
    {
        /* First check if DPB sizing was correct before removing one frame */
        if (p_sys->p_pic_reorder && !p_info->b_flush &&
            p_sys->i_pic_reorder_max < H264_MAX_DPB)
        {
            if (p_sys->b_poc_based_reorder && p_sys->p_pic_reorder->i_foc > p_info->i_foc)
            {
                p_sys->b_invalid_pic_reorder_max = true;
                p_sys->i_pic_reorder_max++;
                pic_pacer_UpdateReorderMax(p_sys->pic_pacer,
                                              p_sys->i_pic_reorder_max, p_info->i_num_ts);
                msg_Info(p_dec, "Raising max DPB to %"PRIu8, p_sys->i_pic_reorder_max);
                break;
            }
            else if (!p_sys->b_poc_based_reorder &&
                     p_info->p_picture->date > VLC_TICK_INVALID &&
                     p_sys->p_pic_reorder->p_picture->date > p_info->p_picture->date)
            {
                p_sys->b_invalid_pic_reorder_max = true;
                p_sys->i_pic_reorder_max++;
                pic_pacer_UpdateReorderMax(p_sys->pic_pacer,
                                              p_sys->i_pic_reorder_max, p_info->i_num_ts);
                msg_Info(p_dec, "Raising max DPB to %"PRIu8, p_sys->i_pic_reorder_max);
                break;
            }
        }

        picture_t *p_fields = RemoveOneFrameFromDPB(p_sys);
        if (p_fields == NULL)
            break;
        do
        {
            picture_t *p_next = p_fields->p_next;
            p_fields->p_next = NULL;
            decoder_QueueVideo(p_dec, p_fields);
            p_fields = p_next;
        } while(p_fields != NULL);
    }

    InsertIntoDPB(p_sys, p_info);
}

static CMVideoCodecType CodecPrecheck(decoder_t *p_dec)
{
    /* check for the codec we can and want to decode */
    switch (p_dec->fmt_in.i_codec) {
        case VLC_CODEC_H264:
            return kCMVideoCodecType_H264;

        case VLC_CODEC_HEVC:
            if (!deviceSupportsHEVC())
            {
                msg_Warn(p_dec, "device doesn't support HEVC");
                return 0;
            }
            return kCMVideoCodecType_HEVC;

        case VLC_CODEC_MP4V:
        {
            if (p_dec->fmt_in.i_original_fourcc == VLC_FOURCC( 'X','V','I','D' )) {
                msg_Warn(p_dec, "XVID decoding not implemented, fallback on software");
                return 0;
            }

            msg_Dbg(p_dec, "Will decode MP4V with original FourCC '%4.4s'", (char *)&p_dec->fmt_in.i_original_fourcc);
            return kCMVideoCodecType_MPEG4Video;
        }
#if !TARGET_OS_IPHONE
        case VLC_CODEC_H263:
            return kCMVideoCodecType_H263;

            /* there are no DV or ProRes decoders on iOS, so bailout early */
        case VLC_CODEC_PRORES:
            /* the VT decoder can't differenciate between the ProRes flavors, so we do it */
            switch (p_dec->fmt_in.i_original_fourcc) {
                case VLC_FOURCC( 'a','p','4','c' ):
                case VLC_FOURCC( 'a','p','4','h' ):
                case VLC_FOURCC( 'a','p','4','x' ):
                    return kCMVideoCodecType_AppleProRes4444;

                case VLC_FOURCC( 'a','p','c','h' ):
                    return kCMVideoCodecType_AppleProRes422HQ;

                case VLC_FOURCC( 'a','p','c','s' ):
                    return kCMVideoCodecType_AppleProRes422LT;

                case VLC_FOURCC( 'a','p','c','o' ):
                    return kCMVideoCodecType_AppleProRes422Proxy;

                default:
                    return kCMVideoCodecType_AppleProRes422;
            }

        case VLC_CODEC_DV:
            /* the VT decoder can't differenciate between PAL and NTSC, so we need to do it */
            switch (p_dec->fmt_in.i_original_fourcc) {
                case VLC_FOURCC( 'd', 'v', 'c', ' '):
                case VLC_FOURCC( 'd', 'v', ' ', ' '):
                    msg_Dbg(p_dec, "Decoding DV NTSC");
                    return kCMVideoCodecType_DVCNTSC;

                case VLC_FOURCC( 'd', 'v', 's', 'd'):
                case VLC_FOURCC( 'd', 'v', 'c', 'p'):
                case VLC_FOURCC( 'D', 'V', 'S', 'D'):
                    msg_Dbg(p_dec, "Decoding DV PAL");
                    return kCMVideoCodecType_DVCPAL;
                default:
                    return 0;
            }
#endif
            /* mpgv / mp2v needs fixing, so disable it for now */
#if 0
        case VLC_CODEC_MPGV:
            return kCMVideoCodecType_MPEG1Video;
        case VLC_CODEC_MP2V:
            return kCMVideoCodecType_MPEG2Video;
#endif

        default:
#ifndef NDEBUG
            msg_Err(p_dec, "'%4.4s' is not supported", (char *)&p_dec->fmt_in.i_codec);
#endif
            return -1;
    }

    vlc_assert_unreachable();
}

static CFMutableDictionaryRef CreateSessionDescriptionFormat(decoder_t *p_dec,
                                                             unsigned i_sar_num,
                                                             unsigned i_sar_den)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    CFMutableDictionaryRef decoderConfiguration = cfdict_create(0);
    if (decoderConfiguration == NULL)
        return NULL;

    CFDictionaryRef extradata = p_sys->pf_copy_extradata
                                ? p_sys->pf_copy_extradata(p_dec) : NULL;
    if (extradata)
    {
        /* then decoder will also fail if required, no need to handle it */
        CFDictionarySetValue(decoderConfiguration,
                             kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
                             extradata);
        CFRelease(extradata);
    }

    CFDictionarySetValue(decoderConfiguration,
                         kCVImageBufferChromaLocationBottomFieldKey,
                         kCVImageBufferChromaLocation_Left);
    CFDictionarySetValue(decoderConfiguration,
                         kCVImageBufferChromaLocationTopFieldKey,
                         kCVImageBufferChromaLocation_Left);

    /* pixel aspect ratio */
    if (i_sar_num && i_sar_den)
    {
        CFMutableDictionaryRef pixelaspectratio = cfdict_create(2);
        if (pixelaspectratio == NULL)
        {
            CFRelease(decoderConfiguration);
            return NULL;
        }

        cfdict_set_int32(pixelaspectratio,
                         kCVImageBufferPixelAspectRatioHorizontalSpacingKey,
                         i_sar_num);
        cfdict_set_int32(pixelaspectratio,
                         kCVImageBufferPixelAspectRatioVerticalSpacingKey,
                         i_sar_den);
        CFDictionarySetValue(decoderConfiguration,
                             kCVImageBufferPixelAspectRatioKey,
                             pixelaspectratio);
        CFRelease(pixelaspectratio);
    }

    /* Setup YUV->RGB matrix since VT can output BGRA directly. Don't setup
     * transfer and primaries since the transformation is done via the GL
     * fragment shader. */
    CFStringRef yuvmatrix;
    switch (p_dec->fmt_out.video.space)
    {
        case COLOR_SPACE_BT601:
            yuvmatrix = kCVImageBufferYCbCrMatrix_ITU_R_601_4;
            break;
        case COLOR_SPACE_BT2020:
            yuvmatrix = kCVImageBufferColorPrimaries_ITU_R_2020;
            break;
        case COLOR_SPACE_BT709:
        default:
            yuvmatrix = kCVImageBufferColorPrimaries_ITU_R_709_2;
            break;
    }
    CFDictionarySetValue(decoderConfiguration, kCVImageBufferYCbCrMatrixKey,
                         yuvmatrix);

#if TARGET_OS_OSX
    /* enable HW accelerated playback, since this is optional on OS X
     * note that the backend may still fallback on software mode if no
     * suitable hardware is available */
    CFDictionarySetValue(decoderConfiguration,
                         kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder,
                         kCFBooleanTrue);

    /* on OS X, we can force VT to fail if no suitable HW decoder is available,
     * preventing the aforementioned SW fallback */
    if (var_InheritBool(p_dec, "videotoolbox-hw-decoder-only"))
        CFDictionarySetValue(decoderConfiguration,
                             kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder,
                             kCFBooleanTrue);
#endif

    return decoderConfiguration;
}

static void PtsInit(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_dec->fmt_in.video.i_frame_rate_base && p_dec->fmt_in.video.i_frame_rate)
    {
        date_Init(&p_sys->pts, p_dec->fmt_in.video.i_frame_rate * 2,
                  p_dec->fmt_in.video.i_frame_rate_base);
    }
    else
    {
        date_Init(&p_sys->pts, 2 * 30000, 1001);
    }
}

static int StartVideoToolbox(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Late starts */
    if (p_sys->pf_late_start && p_sys->pf_late_start(p_dec))
    {
        assert(p_sys->session == NULL);
        return VLC_SUCCESS;
    }

    /* Fills fmt_out (from extradata if any) */
    if (ConfigureVout(p_dec) != VLC_SUCCESS)
        return VLC_EGENERIC;

    /* destination pixel buffer attributes */
    CFMutableDictionaryRef destinationPixelBufferAttributes = cfdict_create(0);
    if (destinationPixelBufferAttributes == NULL)
        return VLC_EGENERIC;

    CFMutableDictionaryRef decoderConfiguration =
        CreateSessionDescriptionFormat(p_dec,
                                       p_dec->fmt_out.video.i_sar_num,
                                       p_dec->fmt_out.video.i_sar_den);
    if (decoderConfiguration == NULL)
    {
        CFRelease(destinationPixelBufferAttributes);
        return VLC_EGENERIC;
    }

    /* create video format description */
    OSStatus status = CMVideoFormatDescriptionCreate(
                                            kCFAllocatorDefault,
                                            p_sys->codec,
                                            p_dec->fmt_out.video.i_visible_width,
                                            p_dec->fmt_out.video.i_visible_height,
                                            decoderConfiguration,
                                            &p_sys->videoFormatDescription);
    if (status)
    {
        CFRelease(destinationPixelBufferAttributes);
        CFRelease(decoderConfiguration);
        msg_Err(p_dec, "video format description creation failed (%i)", (int)status);
        return VLC_EGENERIC;
    }

#if !TARGET_OS_IPHONE
    CFDictionarySetValue(destinationPixelBufferAttributes,
                         kCVPixelBufferIOSurfaceOpenGLTextureCompatibilityKey,
                         kCFBooleanTrue);
#else
    CFDictionarySetValue(destinationPixelBufferAttributes,
                         kCVPixelBufferOpenGLESCompatibilityKey,
                         kCFBooleanTrue);
#endif

    cfdict_set_int32(destinationPixelBufferAttributes,
                     kCVPixelBufferWidthKey, p_dec->fmt_out.video.i_visible_width);
    cfdict_set_int32(destinationPixelBufferAttributes,
                     kCVPixelBufferHeightKey, p_dec->fmt_out.video.i_visible_height);

    if (p_sys->i_cvpx_format != 0)
    {
        OSType chroma = htonl(p_sys->i_cvpx_format);
        msg_Warn(p_dec, "forcing output chroma (kCVPixelFormatType): %4.4s",
            (const char *) &chroma);
        cfdict_set_int32(destinationPixelBufferAttributes,
                         kCVPixelBufferPixelFormatTypeKey,
                         p_sys->i_cvpx_format);
    }

    cfdict_set_int32(destinationPixelBufferAttributes,
                     kCVPixelBufferBytesPerRowAlignmentKey, 16);

    /* setup decoder callback record */
    VTDecompressionOutputCallbackRecord decoderCallbackRecord;
    decoderCallbackRecord.decompressionOutputCallback = DecoderCallback;
    decoderCallbackRecord.decompressionOutputRefCon = p_dec;

    /* create decompression session */
    status = VTDecompressionSessionCreate(kCFAllocatorDefault,
                                          p_sys->videoFormatDescription,
                                          decoderConfiguration,
                                          destinationPixelBufferAttributes,
                                          &decoderCallbackRecord, &p_sys->session);
    CFRelease(decoderConfiguration);
    CFRelease(destinationPixelBufferAttributes);

    if (HandleVTStatus(p_dec, status, NULL) != VLC_SUCCESS)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static void StopVideoToolbox(decoder_t *p_dec, bool closing)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->session != NULL)
    {
        Drain(p_dec, true);

        VTDecompressionSessionInvalidate(p_sys->session);
        CFRelease(p_sys->session);
        p_sys->session = NULL;

#if TARGET_OS_IPHONE
        /* In case of 4K 10bits (BGRA), we can easily reach the device max
         * memory when flushing. Indeed, we'll create a new VT session that
         * will reallocate frames while previous frames are still used by the
         * vout (and not released). To work-around this issue, we force a vout
         * change. */
        if (!closing && p_dec->fmt_out.i_codec == VLC_CODEC_CVPX_BGRA
            && p_dec->fmt_out.video.i_width * p_dec->fmt_out.video.i_height >= 8000000)
        {
            const video_format_t orig = p_dec->fmt_out.video;
            p_dec->fmt_out.video.i_width = p_dec->fmt_out.video.i_height =
            p_dec->fmt_out.video.i_visible_width = p_dec->fmt_out.video.i_visible_height = 64;
            (void) decoder_UpdateVideoFormat(p_dec);
            p_dec->fmt_out.video = orig;
        }
#else
        VLC_UNUSED(closing);
#endif

        p_sys->b_format_propagated = false;
        p_dec->fmt_out.i_codec = 0;
    }

    if (p_sys->videoFormatDescription != NULL) {
        CFRelease(p_sys->videoFormatDescription);
        p_sys->videoFormatDescription = NULL;
    }
    p_sys->b_vt_feed = false;
    p_sys->b_drop_blocks = false;
}

#pragma mark - module open and close

static void pic_pacer_Destroy(void *priv)
{
    (void) priv;
}

static void pic_pacer_Init(struct pic_pacer *pic_pacer, uint8_t pic_reorder_max)
{
    vlc_mutex_init(&pic_pacer->lock);
    vlc_cond_init(&pic_pacer->wait);
    pic_pacer->nb_field_out = 0;
    pic_pacer->field_reorder_max = pic_reorder_max * 2;
}

static int
CreateVideoContext(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_decoder_device *dec_dev = decoder_GetDecoderDevice(p_dec);
    if (!dec_dev || dec_dev->type != VLC_DECODER_DEVICE_VIDEOTOOLBOX)
    {
        msg_Warn(p_dec, "Could not find an VIDEOTOOLBOX decoder device");
        return VLC_EGENERIC;
    }

    static const struct vlc_video_context_operations ops =
    {
        pic_pacer_Destroy,
    };
    p_sys->vctx =
        vlc_video_context_CreateCVPX(dec_dev,
                                     CVPX_VIDEO_CONTEXT_VIDEOTOOLBOX,
                                     sizeof(struct pic_pacer), &ops);
    vlc_decoder_device_Release(dec_dev);

    if (!p_sys->vctx)
        return VLC_ENOMEM;

    /* Since pictures can outlive the decoder, the private video context is a
     * good place to place the pic_pacer that need to be valid during the
     * lifetime of all pictures */
    p_sys->pic_pacer =
        vlc_video_context_GetCVPXPrivate(p_sys->vctx,
                                         CVPX_VIDEO_CONTEXT_VIDEOTOOLBOX);
    assert(p_sys->pic_pacer);

    pic_pacer_Init(p_sys->pic_pacer, p_sys->i_pic_reorder_max);

    return VLC_SUCCESS;
}

static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    StopVideoToolbox(p_dec, true);

    if (p_sys->pf_codec_clean)
        p_sys->pf_codec_clean(p_dec);

    vlc_video_context_Release(p_sys->vctx);

    free(p_sys);
}

static int OpenDecoder(vlc_object_t *p_this)
{
    int i_ret;
    decoder_t *p_dec = (decoder_t *)p_this;

    /* Fail if this module already failed to decode this ES */
    if (var_Type(p_dec, "videotoolbox-failed") != 0)
        return VLC_EGENERIC;

    /* check quickly if we can digest the offered data */
    CMVideoCodecType codec;

    codec = CodecPrecheck(p_dec);
    if (codec == 0)
        return VLC_EGENERIC;

    /* now that we see a chance to decode anything, allocate the
     * internals and start the decoding session */
    decoder_sys_t *p_sys;
    p_sys = calloc(1, sizeof(*p_sys));
    if (!p_sys)
        return VLC_ENOMEM;
    p_dec->p_sys = p_sys;
    p_sys->session = NULL;
    p_sys->codec = codec;
    p_sys->videoFormatDescription = NULL;
    p_sys->i_pic_reorder_max = 4;
    p_sys->vtsession_status = VTSESSION_STATUS_OK;
    p_sys->b_cvpx_format_forced = false;

    char *cvpx_chroma = var_InheritString(p_dec, "videotoolbox-cvpx-chroma");
    if (cvpx_chroma != NULL)
    {
        if (strlen(cvpx_chroma) != 4)
        {
            msg_Err(p_dec, "invalid videotoolbox-cvpx-chroma option");
            free(cvpx_chroma);
            free(p_sys);
            return VLC_EGENERIC;
        }
        memcpy(&p_sys->i_cvpx_format, cvpx_chroma, 4);
        p_sys->i_cvpx_format = ntohl(p_sys->i_cvpx_format);
        p_sys->b_cvpx_format_forced = true;
        free(cvpx_chroma);
    }

    p_dec->fmt_out.video = p_dec->fmt_in.video;
    p_dec->fmt_out.video.p_palette = NULL;

    if (!p_dec->fmt_out.video.i_sar_num || !p_dec->fmt_out.video.i_sar_den)
    {
        p_dec->fmt_out.video.i_sar_num = 1;
        p_dec->fmt_out.video.i_sar_den = 1;
    }

    i_ret = CreateVideoContext(p_dec);
    if (i_ret != VLC_SUCCESS)
    {
        free(p_sys);
        return i_ret;
    }

    p_sys->b_vt_need_keyframe = false;

    vlc_mutex_init(&p_sys->lock);

    p_dec->pf_decode = DecodeBlock;
    p_dec->pf_flush  = RequestFlush;

    switch(codec)
    {
        case kCMVideoCodecType_H264:
            p_sys->pf_codec_init = InitH264;
            p_sys->pf_codec_clean = CleanH264;
            p_sys->pf_codec_supported = CodecSupportedH264;
            p_sys->pf_late_start = LateStartH264;
            p_sys->pf_process_block = ProcessBlockH264;
            p_sys->pf_need_restart = VideoToolboxNeedsToRestartH264;
            p_sys->pf_configure_vout = ConfigureVoutH264;
            p_sys->pf_copy_extradata = CopyDecoderExtradataH264;
            p_sys->pf_fill_reorder_info = FillReorderInfoH264;
            p_sys->b_poc_based_reorder = true;
            p_sys->b_vt_need_keyframe = true;
            break;

        case kCMVideoCodecType_HEVC:
            p_sys->pf_codec_init = InitHEVC;
            p_sys->pf_codec_clean = CleanHEVC;
            p_sys->pf_codec_supported = CodecSupportedHEVC;
            p_sys->pf_late_start = LateStartHEVC;
            p_sys->pf_process_block = ProcessBlockHEVC;
            p_sys->pf_need_restart = VideoToolboxNeedsToRestartHEVC;
            p_sys->pf_configure_vout = ConfigureVoutHEVC;
            p_sys->pf_copy_extradata = CopyDecoderExtradataHEVC;
            p_sys->pf_fill_reorder_info = FillReorderInfoHEVC;
            p_sys->b_poc_based_reorder = true;
            p_sys->b_vt_need_keyframe = true;
            break;

        case kCMVideoCodecType_MPEG4Video:
            p_sys->pf_copy_extradata = CopyDecoderExtradataMPEG4;
            break;

        default:
            p_sys->pf_copy_extradata = NULL;
            break;
    }

    if (p_sys->pf_codec_init && !p_sys->pf_codec_init(p_dec))
    {
        CloseDecoder(p_this);
        return VLC_EGENERIC;
    }
    if (p_sys->pf_codec_supported && !p_sys->pf_codec_supported(p_dec))
    {
        CloseDecoder(p_this);
        return VLC_EGENERIC;
    }

    i_ret = StartVideoToolbox(p_dec);
    if (i_ret == VLC_SUCCESS) {
        PtsInit(p_dec);
        msg_Info(p_dec, "Using Video Toolbox to decode '%4.4s'",
                        (char *)&p_dec->fmt_in.i_codec);
    } else {
        CloseDecoder(p_this);
    }

    return i_ret;
}

#pragma mark - helpers

static Boolean deviceSupportsHEVC()
{
    if (__builtin_available(macOS 10.13, iOS 11.0, tvOS 11.0, *))
        return VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC);
    else
        return false;
}

static Boolean deviceSupportsAdvancedProfiles()
{
#if TARGET_OS_IPHONE
    size_t size;
    cpu_type_t type;

    size = sizeof(type);
    sysctlbyname("hw.cputype", &type, &size, NULL, 0);

    /* Support for H264 profile HIGH 10 was introduced with the first 64bit Apple ARM SoC, the A7 */
    if (type == CPU_TYPE_ARM64)
        return true;

#endif
    return false;
}

static Boolean deviceSupportsAdvancedLevels()
{
#if TARGET_OS_IPHONE
    #ifdef __LP64__
        size_t size;
        int32_t cpufamily;
        size = sizeof(cpufamily);
        sysctlbyname("hw.cpufamily", &cpufamily, &size, NULL, 0);

        /* Proper 4K decoding requires a Twister SoC
         * Everything below will kill the decoder daemon */
        if (cpufamily == CPUFAMILY_ARM_CYCLONE || cpufamily == CPUFAMILY_ARM_TYPHOON) {
            return false;
        }

        return true;
    #else
        /* we need a 64bit SoC for advanced levels */
        return false;
    #endif
#else
    return true;
#endif
}

static inline void bo_add_mp4_tag_descr(bo_t *p_bo, uint8_t tag, uint32_t size)
{
    bo_add_8(p_bo, tag);
    for (int i = 3; i > 0; i--)
        bo_add_8(p_bo, (size >> (7 * i)) | 0x80);
    bo_add_8(p_bo, size & 0x7F);
}

static CFDictionaryRef ESDSExtradataInfoCreate(decoder_t *p_dec,
                                               uint8_t *p_buf,
                                               uint32_t i_buf_size)
{
    VLC_UNUSED(p_dec);
    int full_size = 3 + 5 +13 + 5 + i_buf_size + 3;
    int config_size = 13 + 5 + i_buf_size;

    bo_t bo;
    bool status = bo_init(&bo, 1024);
    if (status != true)
        return NULL;

    bo_add_8(&bo, 0);       // Version
    bo_add_24be(&bo, 0);    // Flags

    // elementary stream description tag
    bo_add_mp4_tag_descr(&bo, 0x03, full_size);
    bo_add_16be(&bo, 0);    // esid
    bo_add_8(&bo, 0);       // stream priority (0-3)

    // decoder configuration description tag
    bo_add_mp4_tag_descr(&bo, 0x04, config_size);
    bo_add_8(&bo, 32);      // object type identification (32 == MPEG4)
    bo_add_8(&bo, 0x11);    // stream type
    bo_add_24be(&bo, 0);    // buffer size
    bo_add_32be(&bo, 0);    // max bitrate
    bo_add_32be(&bo, 0);    // avg bitrate

    // decoder specific description tag
    bo_add_mp4_tag_descr(&bo, 0x05, i_buf_size);
    bo_add_mem(&bo, i_buf_size, p_buf);

    // sync layer configuration description tag
    bo_add_8(&bo, 0x06);    // tag
    bo_add_8(&bo, 0x01);    // length
    bo_add_8(&bo, 0x02);    // no SL

    CFDictionaryRef extradataInfo =
        ExtradataInfoCreate(CFSTR("esds"), bo.b->p_buffer, bo.b->i_buffer);
    bo_deinit(&bo);
    return extradataInfo;
}

static int ConfigureVout(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* return our proper VLC internal state */
    p_dec->fmt_out.i_codec = 0;

    if (p_sys->pf_configure_vout && !p_sys->pf_configure_vout(p_dec))
        return VLC_EGENERIC;

    if (!p_dec->fmt_out.video.i_visible_width || !p_dec->fmt_out.video.i_visible_height)
    {
        p_dec->fmt_out.video.i_visible_width = p_dec->fmt_out.video.i_width;
        p_dec->fmt_out.video.i_visible_height = p_dec->fmt_out.video.i_height;
    }

    p_dec->fmt_out.video.i_width = vlc_align( p_dec->fmt_out.video.i_visible_width, VT_ALIGNMENT );
    p_dec->fmt_out.video.i_height = vlc_align( p_dec->fmt_out.video.i_visible_height, VT_ALIGNMENT );

    return VLC_SUCCESS;
}

static CFDictionaryRef ExtradataInfoCreate(CFStringRef name,
                                                  void *p_data, size_t i_data)
{
    if (p_data == NULL)
        return NULL;

    CFDataRef extradata = CFDataCreate(kCFAllocatorDefault, p_data, i_data);
    if (extradata == NULL)
        return NULL;

    CFDictionaryRef extradataInfo = CFDictionaryCreate(kCFAllocatorDefault,
        &(CFTypeRef){ name },
        &(CFTypeRef){ extradata },
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    CFRelease(extradata);
    return extradataInfo;
}

static CMSampleBufferRef VTSampleBufferCreate(decoder_t *p_dec,
                                              CMFormatDescriptionRef fmt_desc,
                                              block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OSStatus status;
    CMBlockBufferRef  block_buf = NULL;
    CMSampleBufferRef sample_buf = NULL;
    CMTime pts;
    if (!p_sys->b_poc_based_reorder && p_block->i_pts == VLC_TICK_INVALID)
        pts = CMTimeMake(p_block->i_dts, CLOCK_FREQ);
    else
        pts = CMTimeMake(p_block->i_pts, CLOCK_FREQ);

    CMSampleTimingInfo timeInfoArray[1] = { {
        .duration = CMTimeMake(p_block->i_length, 1),
        .presentationTimeStamp = pts,
        .decodeTimeStamp = CMTimeMake(p_block->i_dts, CLOCK_FREQ),
    } };

    status = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault,// structureAllocator
                                                p_block->p_buffer,  // memoryBlock
                                                p_block->i_buffer,  // blockLength
                                                kCFAllocatorNull,   // blockAllocator
                                                NULL,               // customBlockSource
                                                0,                  // offsetToData
                                                p_block->i_buffer,  // dataLength
                                                false,              // flags
                                                &block_buf);

    if (!status) {
        status = CMSampleBufferCreate(kCFAllocatorDefault,  // allocator
                                      block_buf,            // dataBuffer
                                      true,                 // dataReady
                                      0,                    // makeDataReadyCallback
                                      0,                    // makeDataReadyRefcon
                                      fmt_desc,             // formatDescription
                                      1,                    // numSamples
                                      1,                    // numSampleTimingEntries
                                      timeInfoArray,        // sampleTimingArray
                                      0,                    // numSampleSizeEntries
                                      NULL,                 // sampleSizeArray
                                      &sample_buf);
        if (status != noErr)
            msg_Warn(p_dec, "sample buffer creation failure %i", (int)status);
    } else
        msg_Warn(p_dec, "cm block buffer creation failure %i", (int)status);

    if (block_buf != NULL)
        CFRelease(block_buf);
    block_buf = NULL;

    return sample_buf;
}

static int HandleVTStatus(decoder_t *p_dec, OSStatus status,
                          enum vtsession_status * p_vtsession_status)
{
#define VTERRCASE(x) \
    case x: msg_Warn(p_dec, "vt session error: '" #x "'"); break;

    switch (status)
    {
        case noErr:
            return VLC_SUCCESS;

        VTERRCASE(kVTPropertyNotSupportedErr)
        VTERRCASE(kVTPropertyReadOnlyErr)
        VTERRCASE(kVTParameterErr)
        VTERRCASE(kVTInvalidSessionErr)
        VTERRCASE(kVTAllocationFailedErr)
        VTERRCASE(kVTPixelTransferNotSupportedErr)
        VTERRCASE(kVTCouldNotFindVideoDecoderErr)
        VTERRCASE(kVTCouldNotCreateInstanceErr)
        VTERRCASE(kVTCouldNotFindVideoEncoderErr)
        VTERRCASE(kVTVideoDecoderBadDataErr)
        VTERRCASE(kVTVideoDecoderUnsupportedDataFormatErr)
        VTERRCASE(kVTVideoDecoderMalfunctionErr)
        VTERRCASE(kVTVideoEncoderMalfunctionErr)
        VTERRCASE(kVTVideoDecoderNotAvailableNowErr)
        VTERRCASE(kVTImageRotationNotSupportedErr)
        VTERRCASE(kVTVideoEncoderNotAvailableNowErr)
        VTERRCASE(kVTFormatDescriptionChangeNotSupportedErr)
        VTERRCASE(kVTInsufficientSourceColorDataErr)
        VTERRCASE(kVTCouldNotCreateColorCorrectionDataErr)
        VTERRCASE(kVTColorSyncTransformConvertFailedErr)
        VTERRCASE(kVTVideoDecoderAuthorizationErr)
        VTERRCASE(kVTVideoEncoderAuthorizationErr)
        VTERRCASE(kVTColorCorrectionPixelTransferFailedErr)
        VTERRCASE(kVTMultiPassStorageIdentifierMismatchErr)
        VTERRCASE(kVTMultiPassStorageInvalidErr)
        VTERRCASE(kVTFrameSiloInvalidTimeStampErr)
        VTERRCASE(kVTFrameSiloInvalidTimeRangeErr)
        VTERRCASE(kVTCouldNotFindTemporalFilterErr)
        VTERRCASE(kVTPixelTransferNotPermittedErr)
        VTERRCASE(kVTColorCorrectionImageRotationFailedErr)
        default:
            msg_Warn(p_dec, "unknown vt session error (%i)", (int)status);
    }
#undef VTERRCASE

    if (p_vtsession_status)
    {
        switch (status)
        {
            case kVTPixelTransferNotSupportedErr:
            case kVTPixelTransferNotPermittedErr:
                *p_vtsession_status = VTSESSION_STATUS_RESTART_CHROMA;
                break;
            case -8960 /* codecErr */:
            case kVTVideoDecoderMalfunctionErr:
            case kVTInvalidSessionErr:
                *p_vtsession_status = VTSESSION_STATUS_RESTART;
                break;
            case -8969 /* codecBadDataErr */:
            case kVTVideoDecoderBadDataErr:
            default:
                *p_vtsession_status = VTSESSION_STATUS_ABORT;
                break;
        }
    }
    return VLC_EGENERIC;
}

#pragma mark - actual decoding

static void RequestFlush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_mutex_lock(&p_sys->lock);
    p_sys->b_vt_flush = true;
    vlc_mutex_unlock(&p_sys->lock);
}

static void Drain(decoder_t *p_dec, bool flush)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* draining: return last pictures of the reordered queue */
    vlc_mutex_lock(&p_sys->lock);
    p_sys->b_vt_flush = true;
    DrainDPBLocked(p_dec, flush);
    vlc_mutex_unlock(&p_sys->lock);

    if (p_sys->session && p_sys->b_vt_feed)
        VTDecompressionSessionWaitForAsynchronousFrames(p_sys->session);

    vlc_mutex_lock(&p_sys->lock);
    assert(RemoveOneFrameFromDPB(p_sys) == NULL);
    p_sys->b_vt_flush = false;
    p_sys->b_vt_feed = false;
    p_sys->b_drop_blocks = false;
    vlc_mutex_unlock(&p_sys->lock);
}

static int DecodeBlock(decoder_t *p_dec, block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->b_vt_flush)
    {
        Drain(p_dec, true);
        PtsInit(p_dec);
    }

    if (p_block == NULL)
    {
        Drain(p_dec, false);
        return VLCDEC_SUCCESS;
    }

    vlc_mutex_lock(&p_sys->lock);

    if (p_block->i_flags & BLOCK_FLAG_INTERLACED_MASK)
    {
#if TARGET_OS_IPHONE
        msg_Warn(p_dec, "VT decoder doesn't handle deinterlacing on iOS, "
                 "aborting...");
        p_sys->vtsession_status = VTSESSION_STATUS_ABORT;
#else
        if (!p_sys->b_cvpx_format_forced &&
            p_sys->i_cvpx_format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)
        {
            /* In case of interlaced content, force VT to output I420 since our
             * SW deinterlacer handle this chroma natively. This avoids having
             * 2 extra conversions (CVPX->I420 then I420->CVPX). */

            p_sys->i_cvpx_format = kCVPixelFormatType_420YpCbCr8Planar;
            msg_Warn(p_dec, "Interlaced content: forcing VT to output I420");
            if (p_sys->session != NULL && p_sys->vtsession_status == VTSESSION_STATUS_OK)
            {
                msg_Warn(p_dec, "restarting vt session (color changed)");
                vlc_mutex_unlock(&p_sys->lock);

                /* Drain before stopping */
                Drain(p_dec, false);
                StopVideoToolbox(p_dec, false);

                vlc_mutex_lock(&p_sys->lock);
            }
        }
#endif
    }

    if (p_sys->vtsession_status == VTSESSION_STATUS_RESTART ||
        p_sys->vtsession_status == VTSESSION_STATUS_RESTART_CHROMA)
    {
        bool do_restart;
        if (p_sys->vtsession_status == VTSESSION_STATUS_RESTART_CHROMA)
        {
            if (p_sys->i_cvpx_format == 0 && p_sys->b_cvpx_format_forced)
            {
                /* Already tried to fallback to the original chroma, aborting... */
                do_restart = false;
            }
            else
            {
                p_sys->i_cvpx_format = 0;
                p_sys->b_cvpx_format_forced = true;
                do_restart = true;
            }
        }
        else
        {
            do_restart = p_sys->i_restart_count <= VT_RESTART_MAX;
        }

        if (do_restart)
        {
            msg_Warn(p_dec, "restarting vt session (dec callback failed)");
            vlc_mutex_unlock(&p_sys->lock);

            /* Session will be started by Late Start code block */
            StopVideoToolbox(p_dec, false);

            vlc_mutex_lock(&p_sys->lock);
            p_sys->vtsession_status = VTSESSION_STATUS_OK;
        }
        else
        {
            msg_Warn(p_dec, "too many vt failure...");
            p_sys->vtsession_status = VTSESSION_STATUS_ABORT;
        }
    }

    if (p_sys->vtsession_status == VTSESSION_STATUS_ABORT)
    {
        vlc_mutex_unlock(&p_sys->lock);

        msg_Err(p_dec, "decoder failure, Abort.");
        /* Add an empty variable so that videotoolbox won't be loaded again for
         * this ES */
        var_Create(p_dec, "videotoolbox-failed", VLC_VAR_VOID);
        return VLCDEC_RELOAD;
    }
    vlc_mutex_unlock(&p_sys->lock);

    if (unlikely(p_block->i_flags&(BLOCK_FLAG_CORRUPTED)))
    {
        if (p_sys->b_vt_feed)
        {
            Drain(p_dec, false);
            PtsInit(p_dec);
        }
        goto skip;
    }

    bool b_config_changed = false;
    if (p_sys->pf_process_block)
    {
        p_block = p_sys->pf_process_block(p_dec, p_block, &b_config_changed);
        if (!p_block)
            return VLCDEC_SUCCESS;
    }

    frame_info_t *p_info = CreateReorderInfo(p_dec, p_block);
    if (unlikely(!p_info))
        goto skip;

    if (!p_sys->session /* Late Start */||
        (b_config_changed && p_info->b_flush))
    {
        if (p_sys->session &&
            p_sys->pf_need_restart &&
            p_sys->pf_need_restart(p_dec,p_sys->session))
        {
            msg_Dbg(p_dec, "parameters sets changed: draining decoder");
            Drain(p_dec, false);
            msg_Dbg(p_dec, "parameters sets changed: restarting decoder");
            StopVideoToolbox(p_dec, false);
        }

        if (!p_sys->session)
        {
            if ((p_sys->pf_codec_supported && !p_sys->pf_codec_supported(p_dec))
              || StartVideoToolbox(p_dec) != VLC_SUCCESS)
            {
                /* The current device doesn't handle the profile/level, abort */
                vlc_mutex_lock(&p_sys->lock);
                p_sys->vtsession_status = VTSESSION_STATUS_ABORT;
                vlc_mutex_unlock(&p_sys->lock);
            }
        }

        if (!p_sys->session) /* Start Failed */
        {
            free(p_info);
            goto skip;
        }
    }

    if (!p_sys->b_vt_feed && p_sys->b_vt_need_keyframe && !p_info->b_keyframe)
    {
        free(p_info);
        goto skip;
    }

    CMSampleBufferRef sampleBuffer =
        VTSampleBufferCreate(p_dec, p_sys->videoFormatDescription, p_block);
    if (unlikely(!sampleBuffer))
    {
        free(p_info);
        goto skip;
    }

    VTDecodeInfoFlags flagOut;
    VTDecodeFrameFlags decoderFlags = kVTDecodeFrame_EnableAsynchronousDecompression;

    OSStatus status =
        VTDecompressionSessionDecodeFrame(p_sys->session, sampleBuffer,
                                          decoderFlags, p_info, &flagOut);

    enum vtsession_status vtsession_status;
    if (HandleVTStatus(p_dec, status, &vtsession_status) == VLC_SUCCESS)
    {
        p_sys->b_vt_feed = true;
        if (p_block->i_flags & BLOCK_FLAG_END_OF_SEQUENCE)
            Drain( p_dec, false );
    }
    else
    {
        vlc_mutex_lock(&p_sys->lock);
        if (vtsession_status == VTSESSION_STATUS_RESTART)
            p_sys->i_restart_count++;
        p_sys->vtsession_status = vtsession_status;
        /* In case of abort, the decoder module will be reloaded next time
         * since we already modified the input block */
        vlc_mutex_unlock(&p_sys->lock);
    }
    CFRelease(sampleBuffer);

skip:
    block_Release(p_block);
    return VLCDEC_SUCCESS;
}

static int UpdateVideoFormat(decoder_t *p_dec, CVPixelBufferRef imageBuffer)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    CFDictionaryRef attachmentDict =
        CVBufferGetAttachments(imageBuffer, kCVAttachmentMode_ShouldPropagate);

    if (attachmentDict != NULL && CFDictionaryGetCount(attachmentDict) > 0 &&
        p_dec->fmt_out.video.chroma_location == CHROMA_LOCATION_UNDEF)
    {
        CFStringRef chromaLocation =
            CFDictionaryGetValue(attachmentDict, kCVImageBufferChromaLocationTopFieldKey);
        if (chromaLocation != NULL) {
            if (CFEqual(chromaLocation, kCVImageBufferChromaLocation_Left) ||
                CFEqual(chromaLocation, kCVImageBufferChromaLocation_DV420)) {
                p_dec->fmt_out.video.chroma_location = CHROMA_LOCATION_LEFT;
            } else if (CFEqual(chromaLocation, kCVImageBufferChromaLocation_Center)) {
                p_dec->fmt_out.video.chroma_location = CHROMA_LOCATION_CENTER;
            } else if (CFEqual(chromaLocation, kCVImageBufferChromaLocation_TopLeft)) {
                p_dec->fmt_out.video.chroma_location = CHROMA_LOCATION_TOP_LEFT;
            } else if (CFEqual(chromaLocation, kCVImageBufferChromaLocation_Top)) {
                p_dec->fmt_out.video.chroma_location = CHROMA_LOCATION_TOP_CENTER;
            }
        }

        if (p_dec->fmt_out.video.chroma_location == CHROMA_LOCATION_UNDEF) {
            chromaLocation =
                CFDictionaryGetValue(attachmentDict, kCVImageBufferChromaLocationBottomFieldKey);
            if (chromaLocation != NULL) {
                if (CFEqual(chromaLocation, kCVImageBufferChromaLocation_BottomLeft)) {
                    p_dec->fmt_out.video.chroma_location = CHROMA_LOCATION_BOTTOM_LEFT;
                } else if (CFEqual(chromaLocation, kCVImageBufferChromaLocation_Bottom)) {
                    p_dec->fmt_out.video.chroma_location = CHROMA_LOCATION_BOTTOM_CENTER;
                }
            }
        }
    }

    OSType cvfmt = CVPixelBufferGetPixelFormatType(imageBuffer);
    msg_Dbg(p_dec, "output chroma (kCVPixelFormatType): %4.4s",
        (const char *)&(OSType) { htonl(cvfmt) });
    switch (cvfmt)
    {
        case kCVPixelFormatType_422YpCbCr8:
        case 'yuv2':
            p_dec->fmt_out.i_codec = VLC_CODEC_CVPX_UYVY;
            assert(CVPixelBufferIsPlanar(imageBuffer) == false);
            break;
        case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
            p_dec->fmt_out.i_codec = VLC_CODEC_CVPX_NV12;
            assert(CVPixelBufferIsPlanar(imageBuffer) == true);
            break;
        case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
        case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
            p_dec->fmt_out.i_codec = VLC_CODEC_CVPX_P010;
            assert(CVPixelBufferIsPlanar(imageBuffer) == true);
            break;
        case kCVPixelFormatType_420YpCbCr8Planar:
            p_dec->fmt_out.i_codec = VLC_CODEC_CVPX_I420;
            assert(CVPixelBufferIsPlanar(imageBuffer) == true);
            break;
        case kCVPixelFormatType_32BGRA:
            p_dec->fmt_out.i_codec = VLC_CODEC_CVPX_BGRA;
            assert(CVPixelBufferIsPlanar(imageBuffer) == false);
            break;
        default:
            p_sys->vtsession_status = VTSESSION_STATUS_ABORT;
            return -1;
    }

    if (decoder_UpdateVideoOutput(p_dec, p_sys->vctx) != 0)
    {
        p_sys->vtsession_status = VTSESSION_STATUS_ABORT;
        return -1;
    }
    return 0;
}

static void
video_context_OnPicReleased(vlc_video_context *vctx, unsigned nb_fields)
{
    struct pic_pacer *pic_pacer =
        vlc_video_context_GetCVPXPrivate(vctx, CVPX_VIDEO_CONTEXT_VIDEOTOOLBOX);

    vlc_mutex_lock(&pic_pacer->lock);
    assert((int) pic_pacer->nb_field_out - nb_fields >= 0);
    pic_pacer->nb_field_out -= nb_fields;
    vlc_cond_signal(&pic_pacer->wait);
    vlc_mutex_unlock(&pic_pacer->lock);
}

static void
pic_pacer_UpdateReorderMax(struct pic_pacer *pic_pacer, uint8_t pic_reorder_max,
                              uint8_t nb_field)
{
    vlc_mutex_lock(&pic_pacer->lock);

    pic_pacer->field_reorder_max = pic_reorder_max * (nb_field < 2 ? 2 : nb_field);
    vlc_cond_signal(&pic_pacer->wait);

    vlc_mutex_unlock(&pic_pacer->lock);
}

static int pic_pacer_Wait(struct pic_pacer *pic_pacer, const picture_t *pic)
{
    const uint8_t reserved_fields = 2 * (pic->i_nb_fields < 2 ? 2 : pic->i_nb_fields);

    vlc_mutex_lock(&pic_pacer->lock);

    /* Wait 200 ms max. We can't really know what the video output will do with
     * output pictures (will they be rendered immediately ?), so don't wait
     * infinitely. The output will be paced anyway by the vlc_cond_timedwait()
     * call. */
    vlc_tick_t deadline = vlc_tick_now() + VLC_TICK_FROM_MS(200);
    int ret = 0;
    while (ret == 0 && pic_pacer->field_reorder_max != 0
        && pic_pacer->nb_field_out >= pic_pacer->field_reorder_max + reserved_fields)
        ret = vlc_cond_timedwait(&pic_pacer->wait, &pic_pacer->lock, deadline);
    pic_pacer->nb_field_out += pic->i_nb_fields;

    vlc_mutex_unlock(&pic_pacer->lock);

    return ret;
}

static void DecoderCallback(void *decompressionOutputRefCon,
                            void *sourceFrameRefCon,
                            OSStatus status,
                            VTDecodeInfoFlags infoFlags,
                            CVPixelBufferRef imageBuffer,
                            CMTime pts,
                            CMTime duration)
{
    VLC_UNUSED(duration);
    decoder_t *p_dec = (decoder_t *)decompressionOutputRefCon;
    decoder_sys_t *p_sys = p_dec->p_sys;
    frame_info_t *p_info = (frame_info_t *) sourceFrameRefCon;

    vlc_mutex_lock(&p_sys->lock);
    if (p_sys->b_vt_flush)
        goto end;

    enum vtsession_status vtsession_status;
    if (HandleVTStatus(p_dec, status, &vtsession_status) != VLC_SUCCESS)
    {
        if (p_sys->vtsession_status != VTSESSION_STATUS_ABORT)
        {
            p_sys->vtsession_status = vtsession_status;
            if (vtsession_status == VTSESSION_STATUS_RESTART)
                p_sys->i_restart_count++;
        }
        goto end;
    }
    if (unlikely(!imageBuffer))
    {
        msg_Err(p_dec, "critical: null imageBuffer with a valid status");
        p_sys->vtsession_status = VTSESSION_STATUS_ABORT;
        goto end;
    }

    if (p_sys->vtsession_status == VTSESSION_STATUS_ABORT)
        goto end;

    if (unlikely(!p_sys->b_format_propagated)) {
        p_sys->b_format_propagated =
            UpdateVideoFormat(p_dec, imageBuffer) == VLC_SUCCESS;

        if (!p_sys->b_format_propagated)
            goto end;
        assert(p_dec->fmt_out.i_codec != 0);
    }

    if (infoFlags & kVTDecodeInfo_FrameDropped)
    {
        /* We can't trust VT, some decoded frames can be marked as dropped */
        msg_Dbg(p_dec, "decoder dropped frame");
    }

    if (!CMTIME_IS_VALID(pts))
        goto end;

    if (CVPixelBufferGetDataSize(imageBuffer) == 0)
        goto end;

    if (likely(p_info))
    {
        /* Unlock the mutex because decoder_NewPicture() is blocking. Indeed,
         * it can wait indefinitely when the input is paused. */

        vlc_mutex_unlock(&p_sys->lock);

        picture_t *p_pic = decoder_NewPicture(p_dec);
        if (!p_pic)
        {
            vlc_mutex_lock(&p_sys->lock);
            goto end;
        }

        p_info->p_picture = p_pic;

        p_pic->date = pts.value;
        p_pic->b_force = p_info->b_eos;
        p_pic->b_still = p_info->b_eos;
        p_pic->b_progressive = p_info->b_progressive;
        if (!p_pic->b_progressive)
        {
            p_pic->i_nb_fields = p_info->i_num_ts;
            p_pic->b_top_field_first = p_info->b_top_field_first;
        }

        if (cvpxpic_attach(p_pic, imageBuffer, p_sys->vctx,
                           video_context_OnPicReleased) != VLC_SUCCESS)
        {
            vlc_mutex_lock(&p_sys->lock);
            goto end;
        }

        /* VT is not pacing frame allocation. If we are not fast enough to
         * render (release) the output pictures, the VT session can end up
         * allocating way too many frames. This can be problematic for 4K
         * 10bits. To fix this issue, we ensure that we don't have too many
         * output frames allocated by waiting for the vout to release them. */
        if (pic_pacer_Wait(p_sys->pic_pacer, p_pic))
            msg_Warn(p_dec, "pic_pacer_Wait timed out");

        vlc_mutex_lock(&p_sys->lock);

        if (p_sys->b_vt_flush)
        {
            picture_Release(p_pic);
            goto end;
        }

        p_sys->i_restart_count = 0;

        OnDecodedFrame( p_dec, p_info );
        p_info = NULL;
    }

end:
    free(p_info);
    vlc_mutex_unlock(&p_sys->lock);
    return;
}

static int
OpenDecDevice(vlc_decoder_device *device, vout_window_t *window)
{
    VLC_UNUSED(window);
    static const struct vlc_decoder_device_operations ops =
    {
        NULL,
    };
    device->ops = &ops;
    device->type = VLC_DECODER_DEVICE_VIDEOTOOLBOX;

    return VLC_SUCCESS;
}

#pragma mark - Module descriptor

#define VT_REQUIRE_HW_DEC N_("Use Hardware decoders only")
#define VT_FORCE_CVPX_CHROMA "Force the VideoToolbox output chroma"
#define VT_FORCE_CVPX_CHROMA_LONG "Force the VideoToolbox decoder to output \
    CVPixelBuffers in the specified pixel format instead of the default. \
    By Default, the best chroma is choosen by the VideoToolbox decoder."

static const char *const chroma_list_values[] =
    {
        "",
        "BGRA",
        "y420",
        "420f",
        "420v",
        "2vuy",
    };

static const char *const chroma_list_names[] =
    {
        "Auto",
        "BGRA 8-bit",
        "Y'CbCr 8-bit 4:2:0 (Planar)",
        "Y'CbCr 8-bit 4:2:0 (Bi-Planar, Full Range)",
        "Y'CbCr 8-bit 4:2:0 (Bi-Planar)",
        "Y'CbCr 8-bit 4:2:2",
    };

vlc_module_begin()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_description(N_("VideoToolbox video decoder"))
    set_capability("video decoder", 800)
    set_callbacks(OpenDecoder, CloseDecoder)

    add_bool("videotoolbox-hw-decoder-only", true, VT_REQUIRE_HW_DEC, VT_REQUIRE_HW_DEC, false)
    add_string("videotoolbox-cvpx-chroma", "", VT_FORCE_CVPX_CHROMA, VT_FORCE_CVPX_CHROMA_LONG, true);
        change_string_list(chroma_list_values, chroma_list_names)

    /* Deprecated options */
    add_obsolete_bool("videotoolbox-temporal-deinterlacing") // Since 4.0.0
    add_obsolete_bool("videotoolbox") // Since 4.0.0

    add_submodule()
        set_callback_dec_device(OpenDecDevice, 1)
vlc_module_end()
