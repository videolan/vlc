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

#define ALIGN_16( x ) ( ( ( x ) + 15 ) / 16 * 16 )

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
#define VT_FORCE_CVPX_CHROMA "Force the VT decoder CVPX chroma"
#define VT_FORCE_CVPX_CHROMA_LONG "Values can be 'BGRA', 'y420', '420f', '420v', '2vuy'. \
    By Default, the best chroma is choosen by the VT decoder."

vlc_module_begin()
set_category(CAT_INPUT)
set_subcategory(SUBCAT_INPUT_VCODEC)
set_description(N_("VideoToolbox video decoder"))
set_capability("video decoder",800)
set_callbacks(OpenDecoder, CloseDecoder)

add_bool("videotoolbox-temporal-deinterlacing", true, VT_TEMPO_DEINTERLACE, VT_TEMPO_DEINTERLACE_LONG, false)
add_bool("videotoolbox-hw-decoder-only", false, VT_REQUIRE_HW_DEC, VT_REQUIRE_HW_DEC, false)
add_string("videotoolbox-cvpx-chroma", "", VT_FORCE_CVPX_CHROMA, VT_FORCE_CVPX_CHROMA_LONG, true);
vlc_module_end()

#pragma mark - local prototypes

enum vtsession_status
{
    VTSESSION_STATUS_OK,
    VTSESSION_STATUS_RESTART,
    VTSESSION_STATUS_RESTART_DECAGAIN,
    VTSESSION_STATUS_ABORT,
};

static int SetH264DecoderInfo(decoder_t *, CFMutableDictionaryRef);
static CFMutableDictionaryRef ESDSExtradataInfoCreate(decoder_t *, uint8_t *, uint32_t);
static CFMutableDictionaryRef ExtradataInfoCreate(CFStringRef, void *, size_t);
static CFMutableDictionaryRef H264ExtradataInfoCreate(const struct hxxx_helper *hh);
static int HandleVTStatus(decoder_t *, OSStatus, enum vtsession_status *);
static int DecodeBlock(decoder_t *, block_t *);
static void RequestFlush(decoder_t *);
static void Drain(decoder_t *p_dec, bool flush);
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

    enum vtsession_status       vtsession_status;

    int                         i_forced_cvpx_format;

    h264_poc_context_t          pocctx;
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

static void DrainDPB(decoder_t *p_dec, bool flush)
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

    if (p_sys->b_poc_based_reorder)
    {
        if (p_sys->codec != kCMVideoCodecType_H264 ||
            !ParseH264NAL(p_dec, p_block->p_buffer, p_block->i_buffer,
                          p_sys->hh.i_nal_length_size , p_info))
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
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* check for the codec we can and want to decode */
    switch (p_dec->fmt_in.i_codec) {
        case VLC_CODEC_H264:
            return kCMVideoCodecType_H264;

        case VLC_CODEC_MP4V:
        {
            if (p_dec->fmt_in.i_original_fourcc == VLC_FOURCC( 'X','V','I','D' )) {
                msg_Warn(p_dec, "XVID decoding not implemented, fallback on software");
                return -1;
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
                    return -1;
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
                                                             unsigned i_sar_den,
                                                             CFMutableDictionaryRef extradataInfo)
{
    assert(extradataInfo != nil);

    CFMutableDictionaryRef decoderConfiguration = cfdict_create(2);
    if (decoderConfiguration == NULL)
        return nil;

    CFDictionarySetValue(decoderConfiguration,
                         kCVImageBufferChromaLocationBottomFieldKey,
                         kCVImageBufferChromaLocation_Left);
    CFDictionarySetValue(decoderConfiguration,
                         kCVImageBufferChromaLocationTopFieldKey,
                         kCVImageBufferChromaLocation_Left);

    CFDictionarySetValue(decoderConfiguration,
                         kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
                         extradataInfo);

    /* pixel aspect ratio */
    CFMutableDictionaryRef pixelaspectratio = cfdict_create(2);
    if(pixelaspectratio == NULL)
    {
        CFRelease(decoderConfiguration);
        return nil;
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"

    /* enable HW accelerated playback, since this is optional on OS X
     * note that the backend may still fallback on software mode if no
     * suitable hardware is available */
    CFDictionarySetValue(decoderConfiguration,
                         kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder,
                         kCFBooleanTrue);

    /* on OS X, we can force VT to fail if no suitable HW decoder is available,
     * preventing the aforementioned SW fallback */
    if (var_InheritInteger(p_dec, "videotoolbox-hw-decoder-only"))
        CFDictionarySetValue(decoderConfiguration,
                             kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder,
                             kCFBooleanTrue);

#pragma clang diagnostic pop

    CFDictionarySetValue(decoderConfiguration,
                         kVTDecompressionPropertyKey_FieldMode,
                         kVTDecompressionProperty_FieldMode_DeinterlaceFields);
    CFDictionarySetValue(decoderConfiguration,
                         kVTDecompressionPropertyKey_DeinterlaceMode,
                         kVTDecompressionProperty_DeinterlaceMode_Temporal);

    return decoderConfiguration;
}

static bool VideoToolboxNeedsToRestartH264(decoder_t *p_dec,
                                           VTDecompressionSessionRef session,
                                           const struct hxxx_helper *hh)
{
    unsigned w, h, vw, vh;
    int sarn, sard;

    if (h264_helper_get_current_picture_size(hh, &w, &h, &vw, &vh) != VLC_SUCCESS)
        return true;

    if (h264_helper_get_current_sar(hh, &sarn, &sard) != VLC_SUCCESS)
        return true;

    CFMutableDictionaryRef extradataInfo = H264ExtradataInfoCreate(hh);
    if(extradataInfo == nil)
        return true;

    bool b_ret = true;

    CFMutableDictionaryRef decoderConfiguration =
            CreateSessionDescriptionFormat(p_dec, sarn, sard, extradataInfo);
    if (decoderConfiguration != nil)
    {
        CMFormatDescriptionRef newvideoFormatDesc;
        /* create new video format description */
        OSStatus status = CMVideoFormatDescriptionCreate(kCFAllocatorDefault,
                                                         kCMVideoCodecType_H264,
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
    CFRelease(extradataInfo);

    return b_ret;
}

static int StartVideoToolbox(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* destination pixel buffer attributes */
    CFMutableDictionaryRef destinationPixelBufferAttributes = cfdict_create(2);
    if(destinationPixelBufferAttributes == nil)
        return VLC_EGENERIC;

    CFMutableDictionaryRef decoderConfiguration =
        CreateSessionDescriptionFormat(p_dec,
                                       p_dec->fmt_out.video.i_sar_num,
                                       p_dec->fmt_out.video.i_sar_den,
                                       p_sys->extradataInfo);
    if(decoderConfiguration == nil)
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

    if (p_sys->i_forced_cvpx_format != 0)
    {
        int chroma = htonl(p_sys->i_forced_cvpx_format);
        msg_Warn(p_dec, "forcing CVPX format: %4.4s", (const char *) &chroma);
        cfdict_set_int32(destinationPixelBufferAttributes,
                         kCVPixelBufferPixelFormatTypeKey,
                         p_sys->i_forced_cvpx_format);
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

    if( p_dec->fmt_in.video.i_frame_rate_base && p_dec->fmt_in.video.i_frame_rate )
    {
        date_Init( &p_sys->pts, p_dec->fmt_in.video.i_frame_rate * 2,
                   p_dec->fmt_in.video.i_frame_rate_base );
    }
    else date_Init( &p_sys->pts, 2 * 30000, 1001 );

    return VLC_SUCCESS;
}

static void StopVideoToolbox(decoder_t *p_dec, bool b_reset_format)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->session != nil)
    {
        Drain(p_dec, true);

        VTDecompressionSessionInvalidate(p_sys->session);
        CFRelease(p_sys->session);
        p_sys->session = nil;

        if (b_reset_format)
        {
            p_sys->b_format_propagated = false;
            p_dec->fmt_out.i_codec = 0;
        }
    }

    if (p_sys->videoFormatDescription != nil) {
        CFRelease(p_sys->videoFormatDescription);
        p_sys->videoFormatDescription = nil;
    }
    p_sys->b_vt_feed = false;
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

    assert(p_sys->extradataInfo == nil);
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
            p_sys->extradataInfo = ExtradataInfoCreate(CFSTR("avcC"),
                                            p_dec->fmt_in.p_extra,
                                            p_dec->fmt_in.i_extra);
            if (SetH264DecoderInfo(p_dec, p_sys->extradataInfo) != VLC_SUCCESS)
                return VLC_EGENERIC;
        }
        else
        {
            /* AnnexB case, we'll get extradata from first input blocks */
            return VLC_SUCCESS;
        }
    }
    else if (p_sys->codec == kCMVideoCodecType_MPEG4Video)
    {
        if (!p_dec->fmt_in.i_extra)
            return VLC_EGENERIC;
        p_sys->extradataInfo = ESDSExtradataInfoCreate(p_dec, p_dec->fmt_in.p_extra,
                                                       p_dec->fmt_in.i_extra);
    }
    else
        p_sys->extradataInfo = ExtradataInfoCreate(NULL, NULL, 0);

    return p_sys->extradataInfo != nil ? VLC_SUCCESS : VLC_EGENERIC;
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
    p_sys->extradataInfo = nil;
    p_sys->p_pic_reorder = NULL;
    p_sys->i_pic_reorder = 0;
    p_sys->i_pic_reorder_max = 4;
    p_sys->b_invalid_pic_reorder_max = false;
    p_sys->b_poc_based_reorder = false;
    p_sys->b_format_propagated = false;
    p_sys->vtsession_status = VTSESSION_STATUS_OK;
    p_sys->b_enable_temporal_processing =
        var_InheritBool(p_dec, "videotoolbox-temporal-deinterlacing");

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
        memcpy(&p_sys->i_forced_cvpx_format, cvpx_chroma, 4);
        p_sys->i_forced_cvpx_format = ntohl(p_sys->i_forced_cvpx_format);
        free(cvpx_chroma);
    }
    else
        p_dec->p_sys->i_forced_cvpx_format = 0;

    h264_poc_context_init( &p_sys->pocctx );
    vlc_mutex_init(&p_sys->lock);

    /* return our proper VLC internal state */
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
    p_dec->fmt_out.video.i_width = ALIGN_16( p_dec->fmt_out.video.i_visible_width );
    p_dec->fmt_out.video.i_height = ALIGN_16( p_dec->fmt_out.video.i_visible_height );

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
    p_dec->pf_flush  = RequestFlush;

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
    if (p_sys->extradataInfo)
        CFRelease(p_sys->extradataInfo);

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

static CFMutableDictionaryRef ESDSExtradataInfoCreate(decoder_t *p_dec,
                                                      uint8_t *p_buf,
                                                      uint32_t i_buf_size)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    int full_size = 3 + 5 +13 + 5 + i_buf_size + 3;
    int config_size = 13 + 5 + i_buf_size;
    int padding = 12;

    bo_t bo;
    bool status = bo_init(&bo, 1024);
    if (status != true)
        return nil;

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

    CFMutableDictionaryRef extradataInfo =
        ExtradataInfoCreate(CFSTR("esds"), bo.b->p_buffer, bo.b->i_buffer);
    bo_deinit(&bo);
    return extradataInfo;
}

static CFMutableDictionaryRef H264ExtradataInfoCreate(const struct hxxx_helper *hh)
{
    CFMutableDictionaryRef extradataInfo = nil;
    block_t *p_avcC = h264_helper_get_avcc_config(hh);
    if (p_avcC)
    {
        extradataInfo = ExtradataInfoCreate(CFSTR("avcC"),
                                            p_avcC->p_buffer, p_avcC->i_buffer);
        block_Release(p_avcC);
    }
    return extradataInfo;
}

static bool IsH264ProfileLevelSupported(decoder_t *p_dec, uint8_t i_profile,
                                        uint8_t i_level)
{
    switch (i_profile) {
        case PROFILE_H264_BASELINE:
        case PROFILE_H264_MAIN:
        case PROFILE_H264_HIGH:
            break;

        case PROFILE_H264_HIGH_10:
        {
            if (deviceSupportsAdvancedProfiles())
            {
                /* FIXME: There is no YUV420 10bits chroma. The
                 * decoder seems to output RGBA when decoding 10bits
                 * content, but there is an unknown crash when
                 * displaying such output, so force NV12 for now. */
                if (p_dec->p_sys->i_forced_cvpx_format == 0)
                    p_dec->p_sys->i_forced_cvpx_format =
                        kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
                break;
            }
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

    return true;
}

static int SetH264DecoderInfo(decoder_t *p_dec, CFMutableDictionaryRef extradataInfo)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->hh.h264.i_sps_count == 0 || p_sys->hh.h264.i_pps_count == 0)
        return VLC_EGENERIC;

    uint8_t i_profile, i_level;
    unsigned i_h264_width, i_h264_height, i_video_width, i_video_height;
    int i_sar_num, i_sar_den, i_ret;

    i_ret = h264_helper_get_current_profile_level(&p_sys->hh, &i_profile, &i_level);
    if (i_ret != VLC_SUCCESS)
        return i_ret;
    if (!IsH264ProfileLevelSupported(p_dec, i_profile, i_level))
        return VLC_ENOMOD; /* This error is critical */

    i_ret = h264_helper_get_current_picture_size(&p_sys->hh,
                                                 &i_h264_width, &i_h264_height,
                                                 &i_video_width, &i_video_height);
    if (i_ret != VLC_SUCCESS)
        return i_ret;

    i_ret = h264_helper_get_current_sar(&p_sys->hh, &i_sar_num, &i_sar_den);
    if (i_ret != VLC_SUCCESS)
        return i_ret;

    video_color_primaries_t primaries;
    video_transfer_func_t transfer;
    video_color_space_t colorspace;
    bool full_range;
    if (hxxx_helper_get_colorimetry(&p_sys->hh, &primaries, &transfer,
                                    &colorspace, &full_range) == VLC_SUCCESS
      && primaries != COLOR_PRIMARIES_UNDEF && transfer != TRANSFER_FUNC_UNDEF
      && colorspace != COLOR_SPACE_UNDEF)
    {
        p_dec->fmt_out.video.primaries = primaries;
        p_dec->fmt_out.video.transfer = transfer;
        p_dec->fmt_out.video.space = colorspace;
        p_dec->fmt_out.video.b_color_range_full = full_range;
    }

    p_dec->fmt_out.video.i_visible_width = i_video_width;
    p_dec->fmt_out.video.i_width = ALIGN_16( i_video_width );
    p_dec->fmt_out.video.i_visible_height = i_video_height;
    p_dec->fmt_out.video.i_height = ALIGN_16( i_video_height );
    p_dec->fmt_out.video.i_sar_num = i_sar_num;
    p_dec->fmt_out.video.i_sar_den = i_sar_den;

    if (extradataInfo == nil)
    {
        if (p_sys->extradataInfo != nil)
            CFRelease(p_sys->extradataInfo);
        p_sys->extradataInfo = H264ExtradataInfoCreate(&p_sys->hh);
    }

    return (p_sys->extradataInfo == nil) ? VLC_EGENERIC: VLC_SUCCESS;
}

static CFMutableDictionaryRef ExtradataInfoCreate(CFStringRef name,
                                                  void *p_data, size_t i_data)
{
    CFMutableDictionaryRef extradataInfo = cfdict_create(1);
    if (extradataInfo == nil)
        return nil;

    if (p_data == NULL)
        return nil;

    CFDataRef extradata = CFDataCreate(kCFAllocatorDefault, p_data, i_data);
    if (extradata == nil)
    {
        CFRelease(extradataInfo);
        return nil;
    }
    CFDictionarySetValue(extradataInfo, name, extradata);
    CFRelease(extradata);
    return extradataInfo;
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

static int HandleVTStatus(decoder_t *p_dec, OSStatus status,
                          enum vtsession_status * p_vtsession_status)
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

    if (p_vtsession_status)
    {
        switch (status)
        {
            case -8960 /* codecErr */:
            case kCVReturnInvalidArgument:
            case kVTVideoDecoderMalfunctionErr:
                *p_vtsession_status = VTSESSION_STATUS_ABORT;
                break;
            case -8969 /* codecBadDataErr */:
            case kVTVideoDecoderBadDataErr:
                *p_vtsession_status = VTSESSION_STATUS_RESTART_DECAGAIN;
                break;
            case kVTInvalidSessionErr:
                *p_vtsession_status = VTSESSION_STATUS_RESTART;
                break;
            default:
                *p_vtsession_status = VTSESSION_STATUS_OK;
                break;
        }
    }
    return VLC_EGENERIC;
}

#pragma mark - actual decoding

static void RequestFlush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* There is no Flush in VT api, ask to restart VT from next DecodeBlock if
     * we already feed some input blocks (it's better to not restart here in
     * order to avoid useless restart just before a close). */
    vlc_mutex_lock(&p_sys->lock);
    p_sys->b_vt_flush = p_sys->b_vt_feed;
    vlc_mutex_unlock(&p_sys->lock);
}

static void Drain(decoder_t *p_dec, bool flush)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* draining: return last pictures of the reordered queue */
    if (p_sys->session)
        VTDecompressionSessionWaitForAsynchronousFrames(p_sys->session);

    vlc_mutex_lock(&p_sys->lock);
    DrainDPB(p_dec, flush);
    p_sys->b_vt_flush = false;
    vlc_mutex_unlock(&p_sys->lock);
}

static int DecodeBlock(decoder_t *p_dec, block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->b_vt_flush)
        RestartVideoToolbox(p_dec, false);

    if (p_block == NULL)
    {
        Drain(p_dec, false);
        return VLCDEC_SUCCESS;
    }

    vlc_mutex_lock(&p_sys->lock);

#if TARGET_OS_IPHONE
    if (p_block->i_flags & BLOCK_FLAG_INTERLACED_MASK)
    {
        msg_Warn(p_dec, "VT decoder doesn't handle deinterlacing on iOS, "
                 "aborting...");
        p_sys->vtsession_status = VTSESSION_STATUS_ABORT;
    }
#endif

    if (p_sys->vtsession_status == VTSESSION_STATUS_RESTART)
    {
        msg_Warn(p_dec, "restarting vt session (dec callback failed)");

        vlc_mutex_unlock(&p_sys->lock);
        int ret = RestartVideoToolbox(p_dec, true);
        vlc_mutex_lock(&p_sys->lock);

        p_sys->vtsession_status = ret == VLC_SUCCESS ? VTSESSION_STATUS_OK
                                                     : VTSESSION_STATUS_ABORT;
    }

    if (p_sys->vtsession_status == VTSESSION_STATUS_ABORT)
    {
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
            Drain(p_dec, false);
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

    frame_info_t *p_info = CreateReorderInfo(p_dec, p_block);
    if(unlikely(!p_info))
        goto skip;

    if (b_config_changed && p_info->b_flush)
    {
        assert(p_sys->codec == kCMVideoCodecType_H264);
        if (!p_sys->session ||
            VideoToolboxNeedsToRestartH264(p_dec,p_sys->session, &p_sys->hh))
        {
            if (p_sys->session)
            {
                msg_Dbg(p_dec, "SPS/PPS changed: draining H264 decoder");
                Drain(p_dec, false);
                msg_Dbg(p_dec, "SPS/PPS changed: restarting H264 decoder");
                StopVideoToolbox(p_dec, true);
            }
            /* else decoding didn't start yet, which is ok for H264, let's see
             * if we can use this block to get going */

            int i_ret = SetH264DecoderInfo(p_dec, nil);
            if (i_ret == VLC_SUCCESS)
            {
                msg_Dbg(p_dec, "Got SPS/PPS: late opening of H264 decoder");
                StartVideoToolbox(p_dec);
            }
            else if (i_ret == VLC_ENOMOD)
            {
                /* The current device doesn't handle the h264 profile/level,
                 * abort */
                vlc_mutex_lock(&p_sys->lock);
                p_sys->vtsession_status = VTSESSION_STATUS_ABORT;
                vlc_mutex_unlock(&p_sys->lock);
                goto skip;
            }
        }

        if (!p_sys->session)
        {
            free(p_info);
            goto skip;
        }
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
    if (p_sys->b_enable_temporal_processing
     && (p_block->i_flags & BLOCK_FLAG_INTERLACED_MASK))
        decoderFlags |= kVTDecodeFrame_EnableTemporalProcessing;

    OSStatus status =
        VTDecompressionSessionDecodeFrame(p_sys->session, sampleBuffer,
                                          decoderFlags, p_info, &flagOut);

    enum vtsession_status vtsession_status;
    if (HandleVTStatus(p_dec, status, &vtsession_status) == VLC_SUCCESS)
    {
        p_sys->b_vt_feed = true;
        if( p_block->i_flags & BLOCK_FLAG_END_OF_SEQUENCE )
            Drain( p_dec, false );
    }
    else
    {
        if (vtsession_status == VTSESSION_STATUS_RESTART
         || vtsession_status == VTSESSION_STATUS_RESTART_DECAGAIN)
        {
            int ret = RestartVideoToolbox(p_dec, true);
            if (ret == VLC_SUCCESS
             && vtsession_status == VTSESSION_STATUS_RESTART_DECAGAIN)
            {
                /* Duplicate p_info since it is or will be freed by the
                 * Decoder Callback */
                p_info = CreateReorderInfo(p_dec, p_block);
                if (likely(p_info))
                    status = VTDecompressionSessionDecodeFrame(p_sys->session,
                                    sampleBuffer, decoderFlags, p_info, &flagOut);
                if (status != 0)
                    ret = VLC_EGENERIC;
            }
            if (ret != VLC_SUCCESS) /* restart failed, abort */
                vtsession_status = VTSESSION_STATUS_ABORT;
        }
        vlc_mutex_lock(&p_sys->lock);
        if (vtsession_status == VTSESSION_STATUS_ABORT)
        {
            msg_Err(p_dec, "decoder failure, Abort.");
            /* The decoder module will be reloaded next time since we already
             * modified the input block */
            p_sys->vtsession_status = VTSESSION_STATUS_ABORT;
        }
        else /* reset status set by the decoder callback during restart */
            p_sys->vtsession_status = VTSESSION_STATUS_OK;
        vlc_mutex_unlock(&p_sys->lock);
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


    if (attachmentDict != nil && attachmentDict.count > 0
     && p_dec->fmt_out.video.chroma_location == CHROMA_LOCATION_UNDEF)
    {
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
        }
        if (p_dec->fmt_out.video.chroma_location == CHROMA_LOCATION_UNDEF)
        {
            chromaLocation = attachmentDict[(NSString *)kCVImageBufferChromaLocationBottomFieldKey];
            if (chromaLocation != nil) {
                if ([chromaLocation isEqualToString:(NSString *)kCVImageBufferChromaLocation_BottomLeft])
                    p_dec->fmt_out.video.chroma_location = CHROMA_LOCATION_BOTTOM_LEFT;
                else if ([chromaLocation isEqualToString:(NSString *)kCVImageBufferChromaLocation_Bottom])
                    p_dec->fmt_out.video.chroma_location = CHROMA_LOCATION_BOTTOM_CENTER;
            }
        }
    }

    uint32_t cvfmt = CVPixelBufferGetPixelFormatType(imageBuffer);
    msg_Info(p_dec, "vt cvpx chroma: %4.4s",
             (const char *)&(uint32_t) { htonl(cvfmt) });
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
            p_dec->p_sys->vtsession_status = VTSESSION_STATUS_ABORT;
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

    vlc_mutex_lock(&p_sys->lock);
    if (p_sys->b_vt_flush)
        goto end;

    enum vtsession_status vtsession_status;
    if (HandleVTStatus(p_dec, status, &vtsession_status) != VLC_SUCCESS)
    {
        /* Can't decode again from here */
        if (vtsession_status == VTSESSION_STATUS_RESTART_DECAGAIN)
            vtsession_status = VTSESSION_STATUS_RESTART;

        if (p_sys->vtsession_status != VTSESSION_STATUS_ABORT)
            p_sys->vtsession_status = vtsession_status;
        goto end;
    }
    assert(imageBuffer);
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

    if(likely(p_info))
    {
        picture_t *p_pic = decoder_NewPicture(p_dec);
        if (!p_pic)
            goto end;

        if (cvpxpic_attach(p_pic, imageBuffer) != VLC_SUCCESS)
            goto end;

        p_info->p_picture = p_pic;

        p_pic->date = pts.value;
        p_pic->b_force = p_info->b_forced;
        p_pic->b_progressive = p_sys->b_handle_deint || p_info->b_progressive;
        if(!p_pic->b_progressive)
        {
            p_pic->i_nb_fields = p_info->i_num_ts;
            p_pic->b_top_field_first = p_info->b_top_field_first;
        }

        OnDecodedFrame( p_dec, p_info );
        p_info = NULL;
    }

end:
    free(p_info);
    vlc_mutex_unlock(&p_sys->lock);
    return;
}
