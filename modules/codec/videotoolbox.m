/*****************************************************************************
 * videotoolbox.m: Video Toolbox decoder
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

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>

#import <sys/types.h>
#import <sys/sysctl.h>
#import <mach/machine.h>

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>

/* support iOS SDKs < v9.1 */
#ifndef CPUFAMILY_ARM_TWISTER
#define CPUFAMILY_ARM_TWISTER 0x92fb37c8
#endif

#endif

#pragma mark - module descriptor

static int OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);

#if MAC_OS_X_VERSION_MIN_ALLOWED < 1090
const CFStringRef kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder = CFSTR("EnableHardwareAcceleratedVideoDecoder");
const CFStringRef kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder = CFSTR("RequireHardwareAcceleratedVideoDecoder");
#endif

#define VT_REQUIRE_HW_DEC N_("Use Hardware decoders only")
#define VT_TEMPO_DEINTERLACE N_("Deinterlacing")
#define VT_TEMPO_DEINTERLACE_LONG N_("If interlaced content is detected, temporal deinterlacing is enabled at the expense of a pipeline delay.")

vlc_module_begin()
set_category(CAT_INPUT)
set_subcategory(SUBCAT_INPUT_VCODEC)
set_description(N_("VideoToolbox video decoder"))
set_capability("video decoder",800)
set_callbacks(OpenDecoder, CloseDecoder)

add_bool("videotoolbox-temporal-deinterlacing", true, VT_TEMPO_DEINTERLACE, VT_TEMPO_DEINTERLACE_LONG, false)
add_bool("videotoolbox-hw-decoder-only", false, VT_REQUIRE_HW_DEC, VT_REQUIRE_HW_DEC, false)
vlc_module_end()

#pragma mark - local prototypes

static int ESDSCreate(decoder_t *, uint8_t *, uint32_t);
static int avcCFromAnnexBCreate(decoder_t *);
static int ExtradataInfoCreate(decoder_t *, CFStringRef, void *, size_t);
static int HandleVTStatus(decoder_t *, OSStatus);
static int DecodeBlock(decoder_t *, block_t *);
static void Flush(decoder_t *);
static void DecoderCallback(void *, void *, OSStatus, VTDecodeInfoFlags,
                            CVPixelBufferRef, CMTime, CMTime);
static BOOL deviceSupportsAdvancedProfiles();
static BOOL deviceSupportsAdvancedLevels();

typedef struct frame_info_t frame_info_t;

struct frame_info_t
{
    picture_t *p_picture;
    int i_poc;
    int i_foc;
    bool b_forced;
    bool b_flush;
    bool b_field;
    bool b_progressive;
    bool b_top_field_first;
    uint8_t i_num_ts;
    unsigned i_length;
    frame_info_t *p_next;
};

#pragma mark - decoder structure

#define H264_MAX_DPB 16

struct decoder_sys_t
{
    CMVideoCodecType            codec;
    struct                      hxxx_helper hh;

    bool                        b_vt_feed;
    bool                        b_vt_flush;
    VTDecompressionSessionRef   session;
    CMVideoFormatDescriptionRef videoFormatDescription;
    CFMutableDictionaryRef      decoderConfiguration;
    CFMutableDictionaryRef      destinationPixelBufferAttributes;
    CFMutableDictionaryRef      extradataInfo;

    vlc_mutex_t                 lock;
    frame_info_t               *p_pic_reorder;
    uint8_t                     i_pic_reorder;
    uint8_t                     i_pic_reorder_max;
    bool                        b_invalid_pic_reorder_max;
    bool                        b_poc_based_reorder;
    bool                        b_enable_temporal_processing;
    bool                        b_handle_deint;

    bool                        b_format_propagated;
    bool                        b_abort;

    poc_context_t               pocctx;
    date_t                      pts;
};

#pragma mark - start & stop

static void GetSPSPPS(uint8_t i_pps_id, void *priv,
                      const h264_sequence_parameter_set_t **pp_sps,
                      const h264_picture_parameter_set_t **pp_pps)
{
    decoder_sys_t *p_sys = priv;

    *pp_pps = p_sys->hh.h264.pps_list[i_pps_id].h264_pps;
    if(*pp_pps == NULL)
        *pp_sps = NULL;
    else
        *pp_sps = p_sys->hh.h264.sps_list[(*pp_pps)->i_sps_id].h264_sps;
}

struct sei_callback_s
{
    uint8_t i_pic_struct;
    const h264_sequence_parameter_set_t *p_sps;
};

static bool ParseH264SEI(const hxxx_sei_data_t *p_sei_data, void *priv)
{

    if(p_sei_data->i_type == HXXX_SEI_PIC_TIMING)
    {
        struct sei_callback_s *s = priv;
        if(s->p_sps && s->p_sps->vui.b_valid)
        {
            if(s->p_sps->vui.b_hrd_parameters_present_flag)
            {
                bs_read(p_sei_data->p_bs, s->p_sps->vui.i_cpb_removal_delay_length_minus1 + 1);
                bs_read(p_sei_data->p_bs, s->p_sps->vui.i_dpb_output_delay_length_minus1 + 1);
            }

            if(s->p_sps->vui.b_pic_struct_present_flag)
               s->i_pic_struct = bs_read( p_sei_data->p_bs, 4);
        }
        return false;
    }

    return true;
}

static bool ParseH264NAL(decoder_t *p_dec,
                         const uint8_t *p_buffer, size_t i_buffer,
                         uint8_t i_nal_length_size, frame_info_t *p_info)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    hxxx_iterator_ctx_t itctx;
    hxxx_iterator_init(&itctx, p_buffer, i_buffer, i_nal_length_size);

    const uint8_t *p_nal; size_t i_nal;
    const uint8_t *p_sei_nal = NULL; size_t i_sei_nal = 0;
    while(hxxx_iterate_next(&itctx, &p_nal, &i_nal))
    {
        if(i_nal < 2)
            continue;

        const enum h264_nal_unit_type_e i_nal_type = p_nal[0] & 0x1F;

        if (i_nal_type <= H264_NAL_SLICE_IDR && i_nal_type != H264_NAL_UNKNOWN)
        {
            h264_slice_t slice;
            if(!h264_decode_slice(p_nal, i_nal, GetSPSPPS, p_sys, &slice))
                return false;

            const h264_sequence_parameter_set_t *p_sps;
            const h264_picture_parameter_set_t *p_pps;
            GetSPSPPS(slice.i_pic_parameter_set_id, p_sys, &p_sps, &p_pps);
            if(p_sps)
            {
                if(!p_sys->b_invalid_pic_reorder_max && i_nal_type == H264_NAL_SLICE_IDR)
                {
                    unsigned dummy;
                    uint8_t i_reorder;
                    h264_get_dpb_values(p_sps, &i_reorder, &dummy);
                    vlc_mutex_lock(&p_sys->lock);
                    p_sys->i_pic_reorder_max = i_reorder;
                    vlc_mutex_unlock(&p_sys->lock);
                }

                int bFOC;
                h264_compute_poc(p_sps, &slice, &p_sys->pocctx,
                                 &p_info->i_poc, &p_info->i_foc, &bFOC);

                p_info->b_flush = (slice.type == H264_SLICE_TYPE_I) || slice.has_mmco5;
                p_info->b_field = slice.i_field_pic_flag;
                p_info->b_progressive = !p_sps->mb_adaptive_frame_field_flag &&
                                        !slice.i_field_pic_flag;

                struct sei_callback_s sei;
                sei.p_sps = p_sps;
                sei.i_pic_struct = UINT8_MAX;

                if(p_sei_nal)
                    HxxxParseSEI(p_sei_nal, i_sei_nal, 1, ParseH264SEI, &sei);

                p_info->i_num_ts = h264_get_num_ts(p_sps, &slice, sei.i_pic_struct,
                                                   p_info->i_foc, bFOC);

                if(!p_info->b_progressive)
                    p_info->b_top_field_first = (sei.i_pic_struct % 2 == 1);

                /* Set frame rate for timings in case of missing rate */
                if( (!p_dec->fmt_in.video.i_frame_rate_base ||
                     !p_dec->fmt_in.video.i_frame_rate) &&
                    p_sps->vui.i_time_scale && p_sps->vui.i_num_units_in_tick )
                {
                    date_Change( &p_sys->pts, p_sps->vui.i_time_scale,
                                              p_sps->vui.i_num_units_in_tick );
                }
            }

            return true; /* No need to parse further NAL */
        }
        else if(i_nal_type == H264_NAL_SEI)
        {
            p_sei_nal = p_nal;
            i_sei_nal = i_nal;
        }
    }

    return false;
}

static void InsertIntoDPB(decoder_sys_t *p_sys, frame_info_t *p_info)
{
    frame_info_t **pp_lead_in = &p_sys->p_pic_reorder;

    for( ;; pp_lead_in = & ((*pp_lead_in)->p_next))
    {
        bool b_insert;
        if(*pp_lead_in == NULL)
            b_insert = true;
        else if(p_sys->b_poc_based_reorder)
            b_insert = ((*pp_lead_in)->i_foc > p_info->i_foc);
        else
            b_insert = ((*pp_lead_in)->p_picture->date >= p_info->p_picture->date);

        if(b_insert)
        {
            p_info->p_next = *pp_lead_in;
            *pp_lead_in = p_info;
            p_sys->i_pic_reorder += (p_info->b_field) ? 1 : 2;
            break;
        }
    }
#if 0
    for(frame_info_t *p_in=p_sys->p_pic_reorder; p_in; p_in = p_in->p_next)
        printf(" %d", p_in->i_foc);
    printf("\n");
#endif
}

static picture_t * RemoveOneFrameFromDPB(decoder_sys_t *p_sys)
{
    frame_info_t *p_info = p_sys->p_pic_reorder;
    if(p_info == NULL)
        return NULL;

    const int i_framepoc = p_info->i_poc;

    picture_t *p_ret = NULL;
    picture_t **pp_ret_last = &p_ret;
    bool b_dequeue;

    do
    {
        picture_t *p_field = p_info->p_picture;

        /* Compute time if missing */
        if (p_field->date == VLC_TS_INVALID)
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
        else b_dequeue = false;

    } while(b_dequeue);

    return p_ret;
}

static void DrainDPB(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    for( ;; )
    {
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
}

static frame_info_t * CreateReorderInfo(decoder_t *p_dec, const block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    frame_info_t *p_info = calloc(1, sizeof(*p_info));
    if (!p_info)
        return NULL;

    if (p_sys->b_poc_based_reorder)
    {
        if(p_sys->codec != kCMVideoCodecType_H264 ||
           !ParseH264NAL(p_dec, p_block->p_buffer, p_block->i_buffer, 4, p_info))
        {
            assert(p_sys->codec == kCMVideoCodecType_H264);
            free(p_info);
            return NULL;
        }
    }
    else
    {
        p_info->i_num_ts = 2;
        p_info->b_progressive = true;
        p_info->b_field = false;
    }

    p_info->i_length = p_block->i_length;

    /* required for still pictures/menus */
    p_info->b_forced = (p_block->i_flags & BLOCK_FLAG_END_OF_SEQUENCE);

    if (date_Get(&p_sys->pts) == VLC_TS_INVALID)
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
            if(p_sys->b_poc_based_reorder && p_sys->p_pic_reorder->i_foc > p_info->i_foc)
            {
                p_sys->b_invalid_pic_reorder_max = true;
                p_sys->i_pic_reorder_max++;
                msg_Info(p_dec, "Raising max DPB to %"PRIu8, p_sys->i_pic_reorder_max);
                break;
            }
            else if (!p_sys->b_poc_based_reorder &&
                     p_info->p_picture->date > VLC_TS_INVALID &&
                     p_sys->p_pic_reorder->p_picture->date > p_info->p_picture->date)
            {
                p_sys->b_invalid_pic_reorder_max = true;
                p_sys->i_pic_reorder_max++;
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
    uint8_t i_profile = 0xFF, i_level = 0xFF;
    bool b_ret = false;
    CMVideoCodecType codec;

    /* check for the codec we can and want to decode */
    switch (p_dec->fmt_in.i_codec) {
        case VLC_CODEC_H264:
            codec = kCMVideoCodecType_H264;

            b_ret = h264_get_profile_level(&p_dec->fmt_in, &i_profile, &i_level, NULL);
            if (!b_ret) {
                msg_Warn(p_dec, "H264 profile and level parsing failed because it didn't arrive yet");
                return kCMVideoCodecType_H264;
            }

            msg_Dbg(p_dec, "trying to decode MPEG-4 Part 10: profile %" PRIx8 ", level %" PRIx8, i_profile, i_level);

            switch (i_profile) {
                case PROFILE_H264_BASELINE:
                case PROFILE_H264_MAIN:
                case PROFILE_H264_HIGH:
                    break;

                case PROFILE_H264_HIGH_10:
                {
                    if (deviceSupportsAdvancedProfiles())
                        break;
                }

                default:
                {
                    msg_Dbg(p_dec, "unsupported H264 profile %" PRIx8, i_profile);
                    return -1;
                }
            }

#if !TARGET_OS_IPHONE
            /* a level higher than 5.2 was not tested, so don't dare to
             * try to decode it*/
            if (i_level > 52) {
                msg_Dbg(p_dec, "unsupported H264 level %" PRIx8, i_level);
                return -1;
            }
#else
            /* on SoC A8, 4.2 is the highest specified profile */
            if (i_level > 42) {
                /* on Twister, we can do up to 5.2 */
                if (!deviceSupportsAdvancedLevels() || i_level > 52) {
                    msg_Dbg(p_dec, "unsupported H264 level %" PRIx8, i_level);
                    return -1;
                }
            }
#endif

            break;
        case VLC_CODEC_MP4V:
        {
            if (p_dec->fmt_in.i_original_fourcc == VLC_FOURCC( 'X','V','I','D' )) {
                msg_Warn(p_dec, "XVID decoding not implemented, fallback on software");
                return -1;
            }

            msg_Dbg(p_dec, "Will decode MP4V with original FourCC '%4.4s'", (char *)&p_dec->fmt_in.i_original_fourcc);
            codec = kCMVideoCodecType_MPEG4Video;

            break;
        }
#if !TARGET_OS_IPHONE
        case VLC_CODEC_H263:
            codec = kCMVideoCodecType_H263;
            break;

            /* there are no DV or ProRes decoders on iOS, so bailout early */
        case VLC_CODEC_PRORES:
            /* the VT decoder can't differenciate between the ProRes flavors, so we do it */
            switch (p_dec->fmt_in.i_original_fourcc) {
                case VLC_FOURCC( 'a','p','4','c' ):
                case VLC_FOURCC( 'a','p','4','h' ):
                    codec = kCMVideoCodecType_AppleProRes4444;
                    break;

                case VLC_FOURCC( 'a','p','c','h' ):
                    codec = kCMVideoCodecType_AppleProRes422HQ;
                    break;

                case VLC_FOURCC( 'a','p','c','s' ):
                    codec = kCMVideoCodecType_AppleProRes422LT;
                    break;

                case VLC_FOURCC( 'a','p','c','o' ):
                    codec = kCMVideoCodecType_AppleProRes422Proxy;
                    break;

                default:
                    codec = kCMVideoCodecType_AppleProRes422;
                    break;
            }
            if (codec != 0)
                break;

        case VLC_CODEC_DV:
            /* the VT decoder can't differenciate between PAL and NTSC, so we need to do it */
            switch (p_dec->fmt_in.i_original_fourcc) {
                case VLC_FOURCC( 'd', 'v', 'c', ' '):
                case VLC_FOURCC( 'd', 'v', ' ', ' '):
                    msg_Dbg(p_dec, "Decoding DV NTSC");
                    codec = kCMVideoCodecType_DVCNTSC;
                    break;

                case VLC_FOURCC( 'd', 'v', 's', 'd'):
                case VLC_FOURCC( 'd', 'v', 'c', 'p'):
                case VLC_FOURCC( 'D', 'V', 'S', 'D'):
                    msg_Dbg(p_dec, "Decoding DV PAL");
                    codec = kCMVideoCodecType_DVCPAL;
                    break;

                default:
                    break;
            }
            if (codec != 0)
                break;
#endif
            /* mpgv / mp2v needs fixing, so disable it for now */
#if 0
        case VLC_CODEC_MPGV:
            codec = kCMVideoCodecType_MPEG1Video;
            break;
        case VLC_CODEC_MP2V:
            codec = kCMVideoCodecType_MPEG2Video;
            break;
#endif

        default:
#ifndef NDEBUG
            msg_Err(p_dec, "'%4.4s' is not supported", (char *)&p_dec->fmt_in.i_codec);
#endif
            return -1;
    }

    return codec;
}

static int StartVideoToolbox(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OSStatus status;

    assert(p_sys->extradataInfo != nil);

    p_sys->decoderConfiguration = cfdict_create(2);
    if (p_sys->decoderConfiguration == NULL)
        return VLC_ENOMEM;

    CFDictionarySetValue(p_sys->decoderConfiguration,
                         kCVImageBufferChromaLocationBottomFieldKey,
                         kCVImageBufferChromaLocation_Left);
    CFDictionarySetValue(p_sys->decoderConfiguration,
                         kCVImageBufferChromaLocationTopFieldKey,
                         kCVImageBufferChromaLocation_Left);

    CFDictionarySetValue(p_sys->decoderConfiguration,
                         kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
                         p_sys->extradataInfo);

    /* pixel aspect ratio */
    CFMutableDictionaryRef pixelaspectratio = cfdict_create(2);

    const unsigned i_video_width = p_dec->fmt_out.video.i_width;
    const unsigned i_video_height = p_dec->fmt_out.video.i_height;
    const unsigned i_sar_num = p_dec->fmt_out.video.i_sar_num;
    const unsigned i_sar_den = p_dec->fmt_out.video.i_sar_den;

    if( p_dec->fmt_in.video.i_frame_rate_base && p_dec->fmt_in.video.i_frame_rate )
    {
        date_Init( &p_sys->pts, p_dec->fmt_in.video.i_frame_rate * 2,
                                p_dec->fmt_in.video.i_frame_rate_base );
    }
    else date_Init( &p_sys->pts, 2 * 30000, 1001 );

    cfdict_set_int32(pixelaspectratio,
                     kCVImageBufferPixelAspectRatioHorizontalSpacingKey,
                     i_sar_num);
    cfdict_set_int32(pixelaspectratio,
                     kCVImageBufferPixelAspectRatioVerticalSpacingKey,
                     i_sar_den);
    CFDictionarySetValue(p_sys->decoderConfiguration,
                         kCVImageBufferPixelAspectRatioKey,
                         pixelaspectratio);
    CFRelease(pixelaspectratio);

    /* enable HW accelerated playback, since this is optional on OS X
     * note that the backend may still fallback on software mode if no
     * suitable hardware is available */
    CFDictionarySetValue(p_sys->decoderConfiguration,
                         kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder,
                         kCFBooleanTrue);

    /* on OS X, we can force VT to fail if no suitable HW decoder is available,
     * preventing the aforementioned SW fallback */
    if (var_InheritInteger(p_dec, "videotoolbox-hw-decoder-only"))
        CFDictionarySetValue(p_sys->decoderConfiguration,
                             kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder,
                             kCFBooleanTrue);

    CFDictionarySetValue(p_sys->decoderConfiguration,
                         kVTDecompressionPropertyKey_FieldMode,
                         kVTDecompressionProperty_FieldMode_DeinterlaceFields);
    CFDictionarySetValue(p_sys->decoderConfiguration,
                         kVTDecompressionPropertyKey_DeinterlaceMode,
                         kVTDecompressionProperty_DeinterlaceMode_Temporal);

    /* create video format description */
    status = CMVideoFormatDescriptionCreate(kCFAllocatorDefault,
                                            p_sys->codec,
                                            i_video_width,
                                            i_video_height,
                                            p_sys->decoderConfiguration,
                                            &p_sys->videoFormatDescription);
    if (status) {
        CFRelease(p_sys->decoderConfiguration);
        msg_Err(p_dec, "video format description creation failed (%i)", (int)status);
        return VLC_EGENERIC;
    }

    /* destination pixel buffer attributes */
    p_sys->destinationPixelBufferAttributes = cfdict_create(2);

#if !TARGET_OS_IPHONE
    CFDictionarySetValue(p_sys->destinationPixelBufferAttributes,
                         kCVPixelBufferIOSurfaceOpenGLTextureCompatibilityKey,
                         kCFBooleanTrue);
#else
    CFDictionarySetValue(p_sys->destinationPixelBufferAttributes,
                         kCVPixelBufferOpenGLESCompatibilityKey,
                         kCFBooleanTrue);
#endif

    cfdict_set_int32(p_sys->destinationPixelBufferAttributes,
                     kCVPixelBufferWidthKey, i_video_width);
    cfdict_set_int32(p_sys->destinationPixelBufferAttributes,
                     kCVPixelBufferHeightKey, i_video_height);
    cfdict_set_int32(p_sys->destinationPixelBufferAttributes,
                     kCVPixelBufferBytesPerRowAlignmentKey,
                     i_video_width * 2);

    /* setup decoder callback record */
    VTDecompressionOutputCallbackRecord decoderCallbackRecord;
    decoderCallbackRecord.decompressionOutputCallback = DecoderCallback;
    decoderCallbackRecord.decompressionOutputRefCon = p_dec;

    /* create decompression session */
    status = VTDecompressionSessionCreate(kCFAllocatorDefault,
                                          p_sys->videoFormatDescription,
                                          p_sys->decoderConfiguration,
                                          p_sys->destinationPixelBufferAttributes,
                                          &decoderCallbackRecord, &p_sys->session);

    if (HandleVTStatus(p_dec, status) != VLC_SUCCESS)
        return VLC_EGENERIC;

    /* Check if the current session supports deinterlacing and temporal
     * processing */
    CFDictionaryRef supportedProps = NULL;
    status = VTSessionCopySupportedPropertyDictionary(p_sys->session,
                                                      &supportedProps);
    p_sys->b_handle_deint = status == noErr &&
        CFDictionaryContainsKey(supportedProps,
                                kVTDecompressionPropertyKey_FieldMode);
    p_sys->b_enable_temporal_processing = status == noErr &&
        CFDictionaryContainsKey(supportedProps,
                                kVTDecompressionProperty_DeinterlaceMode_Temporal);
    if (!p_sys->b_handle_deint)
        msg_Warn(p_dec, "VT decoder doesn't handle deinterlacing");

    if (status == noErr)
        CFRelease(supportedProps);

    return VLC_SUCCESS;
}

static void StopVideoToolbox(decoder_t *p_dec, bool b_reset_format)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->session != nil)
    {
        VTDecompressionSessionInvalidate(p_sys->session);
        CFRelease(p_sys->session);
        p_sys->session = nil;

        if (b_reset_format)
        {
            p_sys->b_format_propagated = false;
            p_dec->fmt_out.i_codec = 0;
        }
        DrainDPB( p_dec );
    }

    if (p_sys->videoFormatDescription != nil) {
        CFRelease(p_sys->videoFormatDescription);
        p_sys->videoFormatDescription = nil;
    }
    if (p_sys->decoderConfiguration != nil) {
        CFRelease(p_sys->decoderConfiguration);
        p_sys->decoderConfiguration = nil;
    }
    if (p_sys->destinationPixelBufferAttributes != nil) {
        CFRelease(p_sys->destinationPixelBufferAttributes);
        p_sys->destinationPixelBufferAttributes = nil;
    }
}

static int RestartVideoToolbox(decoder_t *p_dec, bool b_reset_format)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    msg_Dbg(p_dec, "Restarting decoder session");

    if (p_sys->session != nil)
        StopVideoToolbox(p_dec, b_reset_format);

    return StartVideoToolbox(p_dec);
}

#pragma mark - module open and close

static int SetupDecoderExtradata(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    CFMutableDictionaryRef extradata_info = NULL;

    if (p_sys->codec == kCMVideoCodecType_H264)
    {
        hxxx_helper_init(&p_sys->hh, VLC_OBJECT(p_dec),
                         p_dec->fmt_in.i_codec, true);
        int i_ret = hxxx_helper_set_extra(&p_sys->hh, p_dec->fmt_in.p_extra,
                                          p_dec->fmt_in.i_extra);
        if (i_ret != VLC_SUCCESS)
            return i_ret;
        assert(p_sys->hh.pf_process_block != NULL);

        if (p_dec->fmt_in.p_extra)
        {
            int i_ret = ExtradataInfoCreate(p_dec, CFSTR("avcC"),
                                            p_dec->fmt_in.p_extra,
                                            p_dec->fmt_in.i_extra);
            if (i_ret != VLC_SUCCESS)
                return i_ret;
        }
        /* else: AnnexB case, we'll get extradata from first input blocks */
    }
    else if (p_sys->codec == kCMVideoCodecType_MPEG4Video)
    {
        if (!p_dec->fmt_in.i_extra)
            return VLC_EGENERIC;
        int i_ret = ESDSCreate(p_dec, p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra);
        if (i_ret != VLC_SUCCESS)
            return i_ret;
    }
    else
    {
        int i_ret = ExtradataInfoCreate(p_dec, NULL, NULL, 0);
        if (i_ret != VLC_SUCCESS)
            return i_ret;
    }

    return VLC_SUCCESS;
}

static int OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;

#if TARGET_OS_IPHONE
    if (unlikely([[UIDevice currentDevice].systemVersion floatValue] < 8.0)) {
        msg_Warn(p_dec, "decoder skipped as OS is too old");
        return VLC_EGENERIC;
    }
#endif

    /* Fail if this module already failed to decode this ES */
    if (var_Type(p_dec, "videotoolbox-failed") != 0)
        return VLC_EGENERIC;

    /* check quickly if we can digest the offered data */
    CMVideoCodecType codec;
    codec = CodecPrecheck(p_dec);
    if (codec == -1)
        return VLC_EGENERIC;

    /* now that we see a chance to decode anything, allocate the
     * internals and start the decoding session */
    decoder_sys_t *p_sys;
    p_sys = malloc(sizeof(*p_sys));
    if (!p_sys)
        return VLC_ENOMEM;
    p_dec->p_sys = p_sys;
    p_sys->session = nil;
    p_sys->b_vt_feed = false;
    p_sys->b_vt_flush = false;
    p_sys->codec = codec;
    p_sys->videoFormatDescription = nil;
    p_sys->decoderConfiguration = nil;
    p_sys->destinationPixelBufferAttributes = nil;
    p_sys->extradataInfo = nil;
    p_sys->p_pic_reorder = NULL;
    p_sys->i_pic_reorder = 0;
    p_sys->i_pic_reorder_max = 4;
    p_sys->b_invalid_pic_reorder_max = false;
    p_sys->b_poc_based_reorder = false;
    p_sys->b_format_propagated = false;
    p_sys->b_abort = false;
    p_sys->b_enable_temporal_processing =
        var_InheritBool(p_dec, "videotoolbox-temporal-deinterlacing");
    h264_poc_context_init( &p_sys->pocctx );
    vlc_mutex_init(&p_sys->lock);

    /* return our proper VLC internal state */
    p_dec->fmt_out.i_cat = p_dec->fmt_in.i_cat;
    p_dec->fmt_out.video = p_dec->fmt_in.video;
    if (!p_dec->fmt_out.video.i_sar_num || !p_dec->fmt_out.video.i_sar_den)
    {
        p_dec->fmt_out.video.i_sar_num = 1;
        p_dec->fmt_out.video.i_sar_den = 1;
    }
    if (!p_dec->fmt_out.video.i_visible_width
     || !p_dec->fmt_out.video.i_visible_height)
    {
        p_dec->fmt_out.video.i_visible_width = p_dec->fmt_out.video.i_width;
        p_dec->fmt_out.video.i_visible_height = p_dec->fmt_out.video.i_height;
    }
    else
    {
        p_dec->fmt_out.video.i_width = p_dec->fmt_out.video.i_visible_width;
        p_dec->fmt_out.video.i_height = p_dec->fmt_out.video.i_visible_height;
    }

    p_dec->fmt_out.i_codec = 0;

    if( codec == kCMVideoCodecType_H264 )
        p_sys->b_poc_based_reorder = true;

    int i_ret = SetupDecoderExtradata(p_dec);
    if (i_ret != VLC_SUCCESS)
        goto error;

    if (p_sys->extradataInfo != nil)
    {
        i_ret = StartVideoToolbox(p_dec);
        if (i_ret != VLC_SUCCESS)
            goto error;
    } /* else: late opening */

    p_dec->pf_decode = DecodeBlock;
    p_dec->pf_flush  = Flush;

    msg_Info(p_dec, "Using Video Toolbox to decode '%4.4s'", (char *)&p_dec->fmt_in.i_codec);

    return VLC_SUCCESS;

error:
    CloseDecoder(p_this);
    return i_ret;
}

static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    StopVideoToolbox(p_dec, true);

    if (p_sys->codec == kCMVideoCodecType_H264)
        hxxx_helper_clean(&p_sys->hh);

    vlc_mutex_destroy(&p_sys->lock);
    free(p_sys);
}

#pragma mark - helpers

static BOOL deviceSupportsAdvancedProfiles()
{
#if TARGET_IPHONE_SIMULATOR
    return NO;
#endif
#if TARGET_OS_IPHONE
    size_t size;
    cpu_type_t type;

    size = sizeof(type);
    sysctlbyname("hw.cputype", &type, &size, NULL, 0);

    /* Support for H264 profile HIGH 10 was introduced with the first 64bit Apple ARM SoC, the A7 */
    if (type == CPU_TYPE_ARM64)
        return YES;

    return NO;
#else
    return NO;
#endif
}

static BOOL deviceSupportsAdvancedLevels()
{
#if TARGET_IPHONE_SIMULATOR
    return YES;
#endif
#if TARGET_OS_IPHONE
    size_t size;
    int32_t cpufamily;

    size = sizeof(cpufamily);
    sysctlbyname("hw.cpufamily", &cpufamily, &size, NULL, 0);

    /* Proper 4K decoding requires a Twister SoC
     * Everything below will kill the decoder daemon */
    if (cpufamily == CPUFAMILY_ARM_TWISTER) {
        return YES;
    }

    return NO;
#else
    return YES;
#endif
}

static inline void bo_add_mp4_tag_descr(bo_t *p_bo, uint8_t tag, uint32_t size)
{
    bo_add_8(p_bo, tag);
    for (int i = 3; i>0; i--)
        bo_add_8(p_bo, (size>>(7*i)) | 0x80);
    bo_add_8(p_bo, size & 0x7F);
}

static int ESDSCreate(decoder_t *p_dec, uint8_t *p_buf, uint32_t i_buf_size)
{
    int full_size = 3 + 5 +13 + 5 + i_buf_size + 3;
    int config_size = 13 + 5 + i_buf_size;
    int padding = 12;

    bo_t bo;
    bool status = bo_init(&bo, 1024);
    if (status != true)
        return VLC_EGENERIC;

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

    int i_ret = ExtradataInfoCreate(p_dec, CFSTR("esds"), bo.b->p_buffer,
                                    bo.b->i_buffer);
    bo_deinit(&bo);
    return i_ret;
}

static int avcCFromAnnexBCreate(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->hh.h264.i_sps_count == 0 || p_sys->hh.h264.i_pps_count == 0)
        return VLC_EGENERIC;

    unsigned i_h264_width, i_h264_height, i_video_width, i_video_height;
    int i_sar_num, i_sar_den, i_ret;
    i_ret = h264_helper_get_current_picture_size(&p_sys->hh,
                                                 &i_h264_width, &i_h264_height,
                                                 &i_video_width, &i_video_height);
    if (i_ret != VLC_SUCCESS)
        return i_ret;
    i_ret = h264_helper_get_current_sar(&p_sys->hh, &i_sar_num, &i_sar_den);
    if (i_ret != VLC_SUCCESS)
        return i_ret;

    p_dec->fmt_out.video.i_visible_width =
    p_dec->fmt_out.video.i_width = i_video_width;
    p_dec->fmt_out.video.i_visible_height =
    p_dec->fmt_out.video.i_height = i_video_height;
    p_dec->fmt_out.video.i_sar_num = i_sar_num;
    p_dec->fmt_out.video.i_sar_den = i_sar_den;

    block_t *p_avcC = h264_helper_get_avcc_config(&p_sys->hh);
    if (!p_avcC)
        return VLC_EGENERIC;

    i_ret = ExtradataInfoCreate(p_dec, CFSTR("avcC"), p_avcC->p_buffer,
                                p_avcC->i_buffer);
    block_Release(p_avcC);
    return i_ret;
}

static int ExtradataInfoCreate(decoder_t *p_dec, CFStringRef name, void *p_data,
                               size_t i_data)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    p_sys->extradataInfo = cfdict_create(1);
    if (p_sys->extradataInfo == nil)
        return VLC_EGENERIC;

    if (p_data == NULL)
        return VLC_SUCCESS;

    CFDataRef extradata = CFDataCreate(kCFAllocatorDefault, p_data, i_data);
    if (extradata == nil)
    {
        CFRelease(p_sys->extradataInfo);
        p_sys->extradataInfo = nil;
        return VLC_EGENERIC;
    }
    CFDictionarySetValue(p_sys->extradataInfo, name, extradata);
    CFRelease(extradata);
    return VLC_SUCCESS;
}

static CMSampleBufferRef VTSampleBufferCreate(decoder_t *p_dec,
                                              CMFormatDescriptionRef fmt_desc,
                                              block_t *p_block)
{
    OSStatus status;
    CMBlockBufferRef  block_buf = NULL;
    CMSampleBufferRef sample_buf = NULL;
    CMTime pts;
    if(!p_dec->p_sys->b_poc_based_reorder && p_block->i_pts == VLC_TS_INVALID)
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
                                      TRUE,                 // dataReady
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

    if (block_buf != nil)
        CFRelease(block_buf);
    block_buf = nil;

    return sample_buf;
}

static int HandleVTStatus(decoder_t *p_dec, OSStatus status)
{
#define VTERRCASE(x) \
    case x: msg_Err(p_dec, "vt session error: '" #x "'"); break;

    switch (status)
    {
        case 0:
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
        case -12219:
            msg_Err(p_dec, "vt session error: "
                    "'kVTColorCorrectionImageRotationFailedErr'");
            break;
        default:
            msg_Err(p_dec, "unknown vt session error (%i)", (int)status);
    }
#undef VTERRCASE
    return VLC_EGENERIC;
}

#pragma mark - actual decoding

static void Flush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* There is no Flush in VT api, ask to restart VT from next DecodeBlock if
     * we already feed some input blocks (it's better to not restart here in
     * order to avoid useless restart just before a close). */
    p_sys->b_vt_flush = p_sys->b_vt_feed;
}

static void Drain(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* draining: return last pictures of the reordered queue */
    if (p_sys->session)
        VTDecompressionSessionWaitForAsynchronousFrames(p_sys->session);

    vlc_mutex_lock(&p_sys->lock);
    DrainDPB( p_dec );
    vlc_mutex_unlock(&p_sys->lock);
}

static int DecodeBlock(decoder_t *p_dec, block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->b_vt_flush) {
        RestartVideoToolbox(p_dec, false);
        p_sys->b_vt_flush = false;
    }

    if (p_block == NULL)
    {
        Drain(p_dec);
        return VLCDEC_SUCCESS;
    }

    vlc_mutex_lock(&p_sys->lock);
    if (p_sys->b_abort) { /* abort from output thread (DecoderCallback) */
        vlc_mutex_unlock(&p_sys->lock);
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
            Drain(p_dec);
            RestartVideoToolbox(p_dec, false);
        }
        goto skip;
    }

    bool b_config_changed = false;
    if (p_sys->codec == kCMVideoCodecType_H264)
    {
        p_block = p_sys->hh.pf_process_block(&p_sys->hh, p_block, &b_config_changed);
        if (!p_block)
            return VLCDEC_SUCCESS;
    }

    if (b_config_changed)
    {
        /* decoding didn't start yet, which is ok for H264, let's see
         * if we can use this block to get going */
        assert(p_sys->codec == kCMVideoCodecType_H264);
        if (p_sys->session)
        {
            msg_Dbg(p_dec, "SPS/PPS changed: draining H264 decoder");
            Drain(p_dec);
            msg_Dbg(p_dec, "SPS/PPS changed: restarting H264 decoder");
            StopVideoToolbox(p_dec, true);
        }

        int i_ret = avcCFromAnnexBCreate(p_dec);
        if (i_ret == VLC_SUCCESS)
        {
            msg_Dbg(p_dec, "Got SPS/PPS: late opening of H264 decoder");
            StartVideoToolbox(p_dec);
        }
        if (!p_sys->session)
            goto skip;
    }

    frame_info_t *p_info = CreateReorderInfo(p_dec, p_block);
    if(unlikely(!p_info))
        goto skip;

    CMSampleBufferRef sampleBuffer =
        VTSampleBufferCreate(p_dec, p_sys->videoFormatDescription, p_block);
    if (unlikely(!sampleBuffer))
    {
        free(p_info);
        goto skip;
    }

    VTDecodeInfoFlags flagOut;
    VTDecodeFrameFlags decoderFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
    if (p_sys->b_enable_temporal_processing
     && (p_block->i_flags & BLOCK_FLAG_INTERLACED_MASK))
        decoderFlags |= kVTDecodeFrame_EnableTemporalProcessing;

    OSStatus status =
        VTDecompressionSessionDecodeFrame(p_sys->session, sampleBuffer,
                                          decoderFlags, p_info, &flagOut);
    if (HandleVTStatus(p_dec, status) == VLC_SUCCESS)
    {
        p_sys->b_vt_feed = true;
        if( p_block->i_flags & BLOCK_FLAG_END_OF_SEQUENCE )
            Drain( p_dec );
    }
    else
    {
        bool b_abort = false;
        switch (status)
        {
            case -8960 /* codecErr */:
            case kCVReturnInvalidArgument:
            case kVTVideoDecoderMalfunctionErr:
                b_abort = true;
                break;
            case -8969 /* codecBadDataErr */:
            case kVTVideoDecoderBadDataErr:
                if (RestartVideoToolbox(p_dec, true) == VLC_SUCCESS)
                {
                    status = VTDecompressionSessionDecodeFrame(p_sys->session,
                                    sampleBuffer, decoderFlags, p_info, &flagOut);

                    if (status != 0)
                    {
                        free( p_info );
                        b_abort = true;
                    }
                }
                break;
            case kVTInvalidSessionErr:
                RestartVideoToolbox(p_dec, true);
                break;
        }
        if (b_abort)
        {
            msg_Err(p_dec, "decoder failure, Abort.");
            /* The decoder module will be reloaded next time since we already
             * modified the input block */
            vlc_mutex_lock(&p_sys->lock);
            p_dec->p_sys->b_abort = true;
            vlc_mutex_unlock(&p_sys->lock);
        }
    }
    CFRelease(sampleBuffer);

skip:
    block_Release(p_block);
    return VLCDEC_SUCCESS;
}

static int UpdateVideoFormat(decoder_t *p_dec, CVPixelBufferRef imageBuffer)
{
    CFDictionaryRef attachments = CVBufferGetAttachments(imageBuffer, kCVAttachmentMode_ShouldPropagate);
    NSDictionary *attachmentDict = (NSDictionary *)attachments;
#ifndef NDEBUG
    NSLog(@"%@", attachments);
#endif
    if (attachmentDict == nil || attachmentDict.count == 0)
        return -1;

    NSString *colorSpace = attachmentDict[(NSString *)kCVImageBufferYCbCrMatrixKey];
    if (colorSpace != nil) {
        if ([colorSpace isEqualToString:(NSString *)kCVImageBufferYCbCrMatrix_ITU_R_601_4])
            p_dec->fmt_out.video.space = COLOR_SPACE_BT601;
        else if ([colorSpace isEqualToString:(NSString *)kCVImageBufferYCbCrMatrix_ITU_R_709_2])
            p_dec->fmt_out.video.space = COLOR_SPACE_BT709;
        else
            p_dec->fmt_out.video.space = COLOR_SPACE_UNDEF;
    }

    NSString *colorprimary = attachmentDict[(NSString *)kCVImageBufferColorPrimariesKey];
    if (colorprimary != nil) {
        if ([colorprimary isEqualToString:(NSString *)kCVImageBufferColorPrimaries_SMPTE_C] ||
            [colorprimary isEqualToString:(NSString *)kCVImageBufferColorPrimaries_EBU_3213])
            p_dec->fmt_out.video.primaries = COLOR_PRIMARIES_BT601_625;
        else if ([colorprimary isEqualToString:(NSString *)kCVImageBufferColorPrimaries_ITU_R_709_2])
            p_dec->fmt_out.video.primaries = COLOR_PRIMARIES_BT709;
        else if ([colorprimary isEqualToString:(NSString *)kCVImageBufferColorPrimaries_P22])
            p_dec->fmt_out.video.primaries = COLOR_PRIMARIES_DCI_P3;
        else
            p_dec->fmt_out.video.primaries = COLOR_PRIMARIES_UNDEF;
    }

    NSString *transfer = attachmentDict[(NSString *)kCVImageBufferTransferFunctionKey];
    if (transfer != nil) {
        if ([transfer isEqualToString:(NSString *)kCVImageBufferTransferFunction_ITU_R_709_2] ||
            [transfer isEqualToString:(NSString *)kCVImageBufferTransferFunction_SMPTE_240M_1995])
            p_dec->fmt_out.video.transfer = TRANSFER_FUNC_BT709;
        else
            p_dec->fmt_out.video.transfer = TRANSFER_FUNC_UNDEF;
    }

    NSString *chromaLocation = attachmentDict[(NSString *)kCVImageBufferChromaLocationTopFieldKey];
    if (chromaLocation != nil) {
        if ([chromaLocation isEqualToString:(NSString *)kCVImageBufferChromaLocation_Left] ||
            [chromaLocation isEqualToString:(NSString *)kCVImageBufferChromaLocation_DV420])
            p_dec->fmt_out.video.chroma_location = CHROMA_LOCATION_LEFT;
        else if ([chromaLocation isEqualToString:(NSString *)kCVImageBufferChromaLocation_Center])
            p_dec->fmt_out.video.chroma_location = CHROMA_LOCATION_CENTER;
        else if ([chromaLocation isEqualToString:(NSString *)kCVImageBufferChromaLocation_TopLeft])
            p_dec->fmt_out.video.chroma_location = CHROMA_LOCATION_TOP_LEFT;
        else if ([chromaLocation isEqualToString:(NSString *)kCVImageBufferChromaLocation_Top])
            p_dec->fmt_out.video.chroma_location = CHROMA_LOCATION_TOP_CENTER;
        else
            p_dec->fmt_out.video.chroma_location = CHROMA_LOCATION_UNDEF;
    }
    if (p_dec->fmt_out.video.chroma_location == CHROMA_LOCATION_UNDEF) {
        chromaLocation = attachmentDict[(NSString *)kCVImageBufferChromaLocationBottomFieldKey];
        if (chromaLocation != nil) {
            if ([chromaLocation isEqualToString:(NSString *)kCVImageBufferChromaLocation_BottomLeft])
                p_dec->fmt_out.video.chroma_location = CHROMA_LOCATION_BOTTOM_LEFT;
            else if ([chromaLocation isEqualToString:(NSString *)kCVImageBufferChromaLocation_Bottom])
                p_dec->fmt_out.video.chroma_location = CHROMA_LOCATION_BOTTOM_CENTER;
        }
    }

    uint32_t cvfmt = CVPixelBufferGetPixelFormatType(imageBuffer);
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
        case kCVPixelFormatType_420YpCbCr8Planar:
            p_dec->fmt_out.i_codec = VLC_CODEC_CVPX_I420;
            assert(CVPixelBufferIsPlanar(imageBuffer) == true);
            break;
        case kCVPixelFormatType_32BGRA:
            p_dec->fmt_out.i_codec = VLC_CODEC_CVPX_BGRA;
            assert(CVPixelBufferIsPlanar(imageBuffer) == false);
            break;
        default:
            p_dec->p_sys->b_abort = true;
            return -1;
    }
    return decoder_UpdateVideoFormat(p_dec);
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

    if (status != noErr) {
        msg_Warn(p_dec, "decoding of a frame failed (%i, %u)", (int)status, (unsigned int) infoFlags);
        if( status != kVTVideoDecoderBadDataErr && status != -8969 )
            free(p_info);
        return;
    }
    assert(imageBuffer);

    if (unlikely(!p_sys->b_format_propagated)) {
        vlc_mutex_lock(&p_sys->lock);
        p_sys->b_format_propagated =
            UpdateVideoFormat(p_dec, imageBuffer) == VLC_SUCCESS;
        vlc_mutex_unlock(&p_sys->lock);

        if (!p_sys->b_format_propagated)
        {
            free(p_info);
            return;
        }
        assert(p_dec->fmt_out.i_codec != 0);
    }

    if (infoFlags & kVTDecodeInfo_FrameDropped)
    {
        /* We can't trust VT, some decoded frames can be marked as dropped */
        msg_Dbg(p_dec, "decoder dropped frame");
    }

    if (!CMTIME_IS_VALID(pts))
    {
        free(p_info);
        return;
    }

    if (CVPixelBufferGetDataSize(imageBuffer) == 0)
    {
        free(p_info);
        return;
    }

    if(likely(p_info))
    {
        picture_t *p_pic = decoder_NewPicture(p_dec);
        if (!p_pic)
        {
            free(p_info);
            return;
        }

        if (cvpxpic_attach(p_pic, imageBuffer) != VLC_SUCCESS)
        {
            free(p_info);
            return;
        }

        p_info->p_picture = p_pic;

        p_pic->date = pts.value;
        p_pic->b_force = p_info->b_forced;
        p_pic->b_progressive = p_sys->b_handle_deint || p_info->b_progressive;
        if(!p_pic->b_progressive)
        {
            p_pic->i_nb_fields = p_info->i_num_ts;
            p_pic->b_top_field_first = p_info->b_top_field_first;
        }

        vlc_mutex_lock(&p_sys->lock);
        OnDecodedFrame( p_dec, p_info );
        vlc_mutex_unlock(&p_sys->lock);
    }

    return;
}
