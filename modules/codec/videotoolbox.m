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
#import "../packetizer/h264_nal.h"
#import "../packetizer/hxxx_nal.h"
#import "../video_chroma/copy.h"
#import <vlc_bits.h>
#import <vlc_boxes.h>

#import <VideoToolbox/VideoToolbox.h>

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

#if !TARGET_OS_IPHONE
#define VT_REQUIRE_HW_DEC N_("Use Hardware decoders only")
#endif
#define VT_TEMPO_DEINTERLACE N_("Deinterlacing")
#define VT_TEMPO_DEINTERLACE_LONG N_("If interlaced content is detected, temporal deinterlacing is enabled at the expense of a pipeline delay.")

vlc_module_begin()
set_category(CAT_INPUT)
set_subcategory(SUBCAT_INPUT_VCODEC)
set_description(N_("VideoToolbox video decoder"))
set_capability("decoder",800)
set_callbacks(OpenDecoder, CloseDecoder)

add_bool("videotoolbox-temporal-deinterlacing", true, VT_TEMPO_DEINTERLACE, VT_TEMPO_DEINTERLACE_LONG, false)
#if !TARGET_OS_IPHONE
add_bool("videotoolbox-hw-decoder-only", false, VT_REQUIRE_HW_DEC, VT_REQUIRE_HW_DEC, false)
#endif
vlc_module_end()

#pragma mark - local prototypes

static CFDataRef ESDSCreate(decoder_t *, uint8_t *, uint32_t);
static picture_t *DecodeBlock(decoder_t *, block_t **);
static void PicReorder_pushSorted(decoder_t *, picture_t *);
static picture_t *PicReorder_pop(decoder_t *, bool);
static void PicReorder_flush(decoder_t *);
static void Flush(decoder_t *);
static void DecoderCallback(void *, void *, OSStatus, VTDecodeInfoFlags,
                            CVPixelBufferRef, CMTime, CMTime);
void VTDictionarySetInt32(CFMutableDictionaryRef, CFStringRef, int);
static void copy420YpCbCr8Planar(picture_t *, CVPixelBufferRef buffer,
                                 unsigned i_width, unsigned i_height);
static BOOL deviceSupportsAdvancedProfiles();
static BOOL deviceSupportsAdvancedLevels();

struct picture_sys_t {
    CFTypeRef pixelBuffer;
};

#pragma mark - decoder structure

struct decoder_sys_t
{
    CMVideoCodecType            codec;
    uint8_t                     i_nal_length_size;

    bool                        b_vt_feed;
    bool                        b_vt_flush;
    bool                        b_is_avcc;
    VTDecompressionSessionRef   session;
    CMVideoFormatDescriptionRef videoFormatDescription;
    CFMutableDictionaryRef      decoderConfiguration;
    CFMutableDictionaryRef      destinationPixelBufferAttributes;

    vlc_mutex_t                 lock;
    picture_t                   *p_pic_reorder;
    size_t                      i_pic_reorder;
    size_t                      i_pic_reorder_max;
    bool                        b_enable_temporal_processing;

    bool                        b_format_propagated;
    bool                        b_abort;
};

#pragma mark - start & stop

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
        case VLC_CODEC_H263:
            codec = kCMVideoCodecType_H263;
            break;

#if !TARGET_OS_IPHONE
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

static int StartVideoToolboxSession(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* setup decoder callback record */
    VTDecompressionOutputCallbackRecord decoderCallbackRecord;
    decoderCallbackRecord.decompressionOutputCallback = DecoderCallback;
    decoderCallbackRecord.decompressionOutputRefCon = p_dec;

    /* create decompression session */
    OSStatus status = VTDecompressionSessionCreate(kCFAllocatorDefault,
                                                   p_sys->videoFormatDescription,
                                                   p_sys->decoderConfiguration,
                                                   p_sys->destinationPixelBufferAttributes,
                                                   &decoderCallbackRecord,
                                                   &p_sys->session);

    /* check if the session is valid */
    if (status) {

        switch (status) {
            case -12470:
                msg_Err(p_dec, "VT is not supported on this hardware");
                break;
            case -12471:
                msg_Err(p_dec, "Video format is not supported by VT");
                break;
            case -12903:
                msg_Err(p_dec, "created session is invalid, could not select and open decoder instance");
                break;
            case -12906:
                msg_Err(p_dec, "could not find decoder");
                break;
            case -12910:
                msg_Err(p_dec, "unsupported data");
                break;
            case -12913:
                msg_Err(p_dec, "VT is not available to sandboxed apps on this OS release or maximum number of decoders reached");
                break;
            case -12917:
                msg_Err(p_dec, "Insufficient source color data");
                break;
            case -12918:
                msg_Err(p_dec, "Could not create color correction data");
                break;
            case -12210:
                msg_Err(p_dec, "Insufficient authorization to create decoder");
                break;
            case -8973:
                msg_Err(p_dec, "Could not select and open decoder instance");
                break;

            default:
                msg_Err(p_dec, "Decompression session creation failed (%i)", (int)status);
                break;
        }
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int StartVideoToolbox(decoder_t *p_dec, block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OSStatus status;

    /* setup the decoder */
    p_sys->decoderConfiguration = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                            2,
                                                            &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(p_sys->decoderConfiguration,
                         kCVImageBufferChromaLocationBottomFieldKey,
                         kCVImageBufferChromaLocation_Left);
    CFDictionarySetValue(p_sys->decoderConfiguration,
                         kCVImageBufferChromaLocationTopFieldKey,
                         kCVImageBufferChromaLocation_Left);

    /* fetch extradata */
    CFMutableDictionaryRef extradata_info = NULL;
    CFDataRef extradata = NULL;

    extradata_info = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                               1,
                                               &kCFTypeDictionaryKeyCallBacks,
                                               &kCFTypeDictionaryValueCallBacks);

    unsigned i_video_width = 0, i_video_visible_width = 0;
    unsigned i_video_height = 0, i_video_visible_height = 0;
    int i_sar_den = 0;
    int i_sar_num = 0;

    if (p_sys->codec == kCMVideoCodecType_H264) {
        /* Do a late opening if there is no extra data and no valid video size */
        if ((p_dec->fmt_in.video.i_width == 0 || p_dec->fmt_in.video.i_height == 0
             || p_dec->fmt_in.i_extra == 0) && p_block == NULL) {
            msg_Dbg(p_dec, "waiting for H264 SPS/PPS, will start late");

            return VLC_SUCCESS;
        }

        size_t i_buf;
        const uint8_t *p_buf = NULL;
        uint8_t *p_alloc_buf = NULL;
        int i_ret = 0;

        /* we need to convert the SPS and PPS units we received from the
         * demuxer's avvC atom so we can process them further */
        p_sys->b_is_avcc = h264_isavcC(p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra);
        if(p_sys->b_is_avcc)
        {
            p_alloc_buf = h264_avcC_to_AnnexB_NAL(p_dec->fmt_in.p_extra,
                                                  p_dec->fmt_in.i_extra,
                                                  &i_buf,
                                                  &p_sys->i_nal_length_size);
            p_buf = p_alloc_buf;
            if(!p_alloc_buf)
            {
                msg_Warn(p_dec, "invalid avc decoder configuration record");
                return VLC_EGENERIC;
            }
        }
        else if(p_block)
        {
            /* we are mid-stream, let's have the h264_get helper see if it
             * can find a NAL unit */
            i_buf = p_block->i_buffer;
            p_buf = p_block->p_buffer;
            p_sys->i_nal_length_size = 4; /* default to 4 bytes */
            i_ret = VLC_SUCCESS;
        }
        else
            return VLC_EGENERIC;

        /* get the SPS and PPS units from the NAL unit which is either
         * part of the demuxer's avvC atom or the mid stream data block */
        const uint8_t *p_sps_nal = NULL, *p_pps_nal = NULL;
        size_t i_sps_nalsize = 0, i_pps_nalsize = 0;
        i_ret = h264_AnnexB_get_spspps(p_buf, i_buf,
                                      &p_sps_nal, &i_sps_nalsize,
                                      &p_pps_nal, &i_pps_nalsize,
                                      NULL, NULL) ? VLC_SUCCESS : VLC_EGENERIC;
        if (i_ret != VLC_SUCCESS || i_sps_nalsize == 0) {
            free(p_alloc_buf);
            msg_Warn(p_dec, "sps pps detection failed");
            return VLC_EGENERIC;
        }
        assert(p_sps_nal);

        /* Decode Sequence Parameter Set */
        h264_sequence_parameter_set_t *p_sps_data;
        if( !( p_sps_data = h264_decode_sps(p_sps_nal, i_sps_nalsize, true) ) )
        {
            free(p_alloc_buf);
            msg_Warn(p_dec, "sps pps parsing failed");
            return VLC_EGENERIC;
        }

        /* this data is more trust-worthy than what we receive
         * from the demuxer, so we will use it to over-write
         * the current values */
        (void)
        h264_get_picture_size( p_sps_data, &i_video_width,
                              &i_video_height,
                              &i_video_visible_width,
                              &i_video_visible_height );
        i_sar_den = p_sps_data->vui.i_sar_den;
        i_sar_num = p_sps_data->vui.i_sar_num;

        uint8_t i_depth;
        unsigned i_delay;
        if (h264_get_dpb_values(p_sps_data, &i_depth, &i_delay) == false)
            i_depth = 4;
        vlc_mutex_lock(&p_sys->lock);
        p_sys->i_pic_reorder_max = i_depth + 1;
        vlc_mutex_unlock(&p_sys->lock);

        h264_release_sps( p_sps_data );

        if(!p_sys->b_is_avcc)
        {
            block_t *p_avcC = h264_NAL_to_avcC( p_sys->i_nal_length_size,
                                                p_sps_nal, i_sps_nalsize,
                                                p_pps_nal, i_pps_nalsize);
            if (!p_avcC) {
                msg_Warn(p_dec, "buffer creation failed");
                return VLC_EGENERIC;
            }

            extradata = CFDataCreate(kCFAllocatorDefault,
                                     p_avcC->p_buffer,
                                     p_avcC->i_buffer);
            block_Release(p_avcC);
        }
        else /* already avcC extradata */
        {
            extradata = CFDataCreate(kCFAllocatorDefault,
                                     p_dec->fmt_in.p_extra,
                                     p_dec->fmt_in.i_extra);
        }

        free(p_alloc_buf);

        if (extradata)
            CFDictionarySetValue(extradata_info, CFSTR("avcC"), extradata);

        CFDictionarySetValue(p_sys->decoderConfiguration,
                             kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
                             extradata_info);

    } else if (p_sys->codec == kCMVideoCodecType_MPEG4Video) {
        extradata = ESDSCreate(p_dec,
                               (uint8_t*)p_dec->fmt_in.p_extra,
                               p_dec->fmt_in.i_extra);

        if (extradata)
            CFDictionarySetValue(extradata_info, CFSTR("esds"), extradata);

        CFDictionarySetValue(p_sys->decoderConfiguration,
                             kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
                             extradata_info);
    } else {
        CFDictionarySetValue(p_sys->decoderConfiguration,
                             kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
                             extradata_info);
    }

    if (extradata)
        CFRelease(extradata);
    CFRelease(extradata_info);

    /* pixel aspect ratio */
    CFMutableDictionaryRef pixelaspectratio = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                        2,
                                                                        &kCFTypeDictionaryKeyCallBacks,
                                                                        &kCFTypeDictionaryValueCallBacks);
    /* fallback on the demuxer if we don't have better info */
    /* FIXME ?: can't we skip temp storage using directly fmt_out */
    if (i_video_width == 0)
        i_video_width = p_dec->fmt_in.video.i_width;
    if (i_video_height == 0)
        i_video_height = p_dec->fmt_in.video.i_height;
    if(!i_video_visible_width)
        i_video_visible_width = p_dec->fmt_in.video.i_visible_width;
    if(!i_video_visible_height)
        i_video_visible_height = p_dec->fmt_in.video.i_visible_height;
    if (i_sar_num == 0)
        i_sar_num = p_dec->fmt_in.video.i_sar_num ? p_dec->fmt_in.video.i_sar_num : 1;
    if (i_sar_den == 0)
        i_sar_den = p_dec->fmt_in.video.i_sar_den ? p_dec->fmt_in.video.i_sar_den : 1;

    VTDictionarySetInt32(pixelaspectratio,
                         kCVImageBufferPixelAspectRatioHorizontalSpacingKey,
                         i_sar_num);
    VTDictionarySetInt32(pixelaspectratio,
                         kCVImageBufferPixelAspectRatioVerticalSpacingKey,
                         i_sar_den);
    CFDictionarySetValue(p_sys->decoderConfiguration,
                         kCVImageBufferPixelAspectRatioKey,
                         pixelaspectratio);
    CFRelease(pixelaspectratio);

#if !TARGET_OS_IPHONE
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
#endif

    p_sys->b_enable_temporal_processing = false;
    if (var_InheritInteger(p_dec, "videotoolbox-temporal-deinterlacing")) {
        if (p_block != NULL) {
            if (p_block->i_flags & BLOCK_FLAG_TOP_FIELD_FIRST ||
                p_block->i_flags & BLOCK_FLAG_BOTTOM_FIELD_FIRST) {
                msg_Dbg(p_dec, "Interlaced content detected, inserting temporal deinterlacer");
                CFDictionarySetValue(p_sys->decoderConfiguration, kVTDecompressionPropertyKey_FieldMode, kVTDecompressionProperty_FieldMode_DeinterlaceFields);
                CFDictionarySetValue(p_sys->decoderConfiguration, kVTDecompressionPropertyKey_DeinterlaceMode, kVTDecompressionProperty_DeinterlaceMode_Temporal);
                p_sys->b_enable_temporal_processing = true;
            }
        }
    }

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
    p_sys->destinationPixelBufferAttributes = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                        2,
                                                                        &kCFTypeDictionaryKeyCallBacks,
                                                                        &kCFTypeDictionaryValueCallBacks);

#if !TARGET_OS_IPHONE
    CFDictionarySetValue(p_sys->destinationPixelBufferAttributes,
                         kCVPixelBufferIOSurfaceOpenGLTextureCompatibilityKey,
                         kCFBooleanTrue);
#else
    CFDictionarySetValue(p_sys->destinationPixelBufferAttributes,
                         kCVPixelBufferOpenGLESCompatibilityKey,
                         kCFBooleanTrue);
#endif

#if TARGET_OS_IPHONE
    /* FIXME: we should let vt decide its preferred format on IOS too */
    /* full range allows a broader range of colors but is H264 only */
    if (p_dec->fmt_out.i_codec == VLC_CODEC_I420) {
        if (p_sys->codec == kCMVideoCodecType_H264) {
            VTDictionarySetInt32(p_sys->destinationPixelBufferAttributes,
                                 kCVPixelBufferPixelFormatTypeKey,
                                 kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);
        } else {
            VTDictionarySetInt32(p_sys->destinationPixelBufferAttributes,
                                 kCVPixelBufferPixelFormatTypeKey,
                                 kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
        }
    }
#endif

    VTDictionarySetInt32(p_sys->destinationPixelBufferAttributes,
                         kCVPixelBufferWidthKey,
                         i_video_width);
    VTDictionarySetInt32(p_sys->destinationPixelBufferAttributes,
                         kCVPixelBufferHeightKey,
                         i_video_height);
    VTDictionarySetInt32(p_sys->destinationPixelBufferAttributes,
                         kCVPixelBufferBytesPerRowAlignmentKey,
                         i_video_width * 2);

    p_dec->fmt_out.video.i_width = i_video_width;
    p_dec->fmt_out.video.i_height = i_video_height;
    p_dec->fmt_out.video.i_visible_width = i_video_visible_width;
    p_dec->fmt_out.video.i_visible_height = i_video_visible_height;
    p_dec->fmt_out.video.i_sar_den = i_sar_den;
    p_dec->fmt_out.video.i_sar_num = i_sar_num;

    if (StartVideoToolboxSession(p_dec) != VLC_SUCCESS) {
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void StopVideoToolboxSession(decoder_t *p_dec, bool b_reset_format)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    VTDecompressionSessionInvalidate(p_sys->session);
    CFRelease(p_sys->session);
    p_sys->session = nil;

    vlc_mutex_lock(&p_sys->lock);
    if (b_reset_format)
    {
        p_sys->b_format_propagated = false;
        p_dec->fmt_out.i_codec = 0;
    }

    PicReorder_flush(p_dec);
    vlc_mutex_unlock(&p_sys->lock);
}

static void StopVideoToolbox(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->session != nil)
        StopVideoToolboxSession(p_dec, true);

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

static void RestartVideoToolbox(decoder_t *p_dec, bool b_reset_format)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    msg_Dbg(p_dec, "Restarting decoder session");

    if (p_sys->session != nil)
        StopVideoToolboxSession(p_dec, b_reset_format);

    if (StartVideoToolboxSession(p_dec) != VLC_SUCCESS) {
        msg_Warn(p_dec, "Decoder session restart failed");
    }
}

#pragma mark - module open and close

static int OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;

#if TARGET_OS_IPHONE
    if (unlikely([[UIDevice currentDevice].systemVersion floatValue] < 8.0)) {
        msg_Warn(p_dec, "decoder skipped as OS is too old");
        return VLC_EGENERIC;
    }
#endif

    if (p_dec->fmt_in.i_cat != VIDEO_ES)
        return VLC_EGENERIC;

    /* Fail if this module already failed to decode this ES */
    if (var_Type(p_dec, "videotoolbox-failed") != 0)
        return VLC_EGENERIC;

    /* check quickly if we can digest the offered data */
    CMVideoCodecType codec;
    codec = CodecPrecheck(p_dec);
    if (codec == -1) {
        return VLC_EGENERIC;
    }

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
    p_sys->b_is_avcc = false;
    p_sys->codec = codec;
    p_sys->videoFormatDescription = nil;
    p_sys->decoderConfiguration = nil;
    p_sys->destinationPixelBufferAttributes = nil;
    p_sys->p_pic_reorder = NULL;
    p_sys->i_pic_reorder = 0;
    p_sys->i_pic_reorder_max = 1; /* 1 == no reordering */
    p_sys->b_format_propagated = false;
    p_sys->b_abort = false;
    vlc_mutex_init(&p_sys->lock);

    /* return our proper VLC internal state */
    p_dec->fmt_out.i_cat = p_dec->fmt_in.i_cat;
    p_dec->fmt_out.video = p_dec->fmt_in.video;

    p_dec->fmt_out.i_codec = 0;

    int i_ret = StartVideoToolbox(p_dec, NULL);
    if (i_ret != VLC_SUCCESS) {
        CloseDecoder(p_this);
        return i_ret;
    }

    p_dec->pf_decode_video = DecodeBlock;
    p_dec->pf_flush        = Flush;

    msg_Info(p_dec, "Using Video Toolbox to decode '%4.4s'", (char *)&p_dec->fmt_in.i_codec);

    return VLC_SUCCESS;
}

static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    StopVideoToolbox(p_dec);

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

static CFDataRef ESDSCreate(decoder_t *p_dec, uint8_t *p_buf, uint32_t i_buf_size)
{
    int full_size = 3 + 5 +13 + 5 + i_buf_size + 3;
    int config_size = 13 + 5 + i_buf_size;
    int padding = 12;

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

    CFDataRef data = CFDataCreate(kCFAllocatorDefault,
                                  bo.b->p_buffer,
                                  bo.b->i_buffer);
    bo_deinit(&bo);
    return data;
}

static block_t *H264ProcessBlock(decoder_t *p_dec, block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    assert(p_block);

    if (p_sys->b_is_avcc) /* FIXME: no change checks done for AVC ? */
        return p_block;

    return hxxx_AnnexB_to_xVC(p_block, p_sys->i_nal_length_size);
}

static CMSampleBufferRef VTSampleBufferCreate(decoder_t *p_dec,
                                              CMFormatDescriptionRef fmt_desc,
                                              block_t *p_block)
{
    OSStatus status;
    CMBlockBufferRef  block_buf = NULL;
    CMSampleBufferRef sample_buf = NULL;

    CMSampleTimingInfo timeInfoArray[1] = { {
        .duration = CMTimeMake(p_block->i_length, 1),
        .presentationTimeStamp = CMTimeMake(p_block->i_pts > 0 ? p_block->i_pts : p_block->i_dts, CLOCK_FREQ),
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

void VTDictionarySetInt32(CFMutableDictionaryRef dict, CFStringRef key, int value)
{
    CFNumberRef number;
    number = CFNumberCreate(NULL, kCFNumberSInt32Type, &value);
    CFDictionarySetValue(dict, key, number);
    CFRelease(number);
}

static void copy420YpCbCr8Planar(picture_t *p_pic,
                                 CVPixelBufferRef buffer,
                                 unsigned i_width,
                                 unsigned i_height)
{
    uint8_t *pp_plane[2];
    size_t pi_pitch[2];

    if (!buffer || i_width == 0 || i_height == 0)
        return;

    CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);

    for (int i = 0; i < 2; i++) {
        pp_plane[i] = CVPixelBufferGetBaseAddressOfPlane(buffer, i);
        pi_pitch[i] = CVPixelBufferGetBytesPerRowOfPlane(buffer, i);
    }

    CopyFromNv12ToI420(p_pic, pp_plane, pi_pitch, i_height);

    CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
}

#pragma mark - actual decoding

static void PicReorder_pushSorted(decoder_t *p_dec, picture_t *p_pic)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    assert(p_pic->p_next == NULL);
    p_sys->i_pic_reorder++;
    if (p_sys->p_pic_reorder == NULL)
    {
        p_sys->p_pic_reorder = p_pic;
        return;
    }

    picture_t **pp_last = &p_sys->p_pic_reorder;
    for (picture_t *p_cur = *pp_last; p_cur != NULL;
         pp_last = &p_cur->p_next, p_cur = *pp_last)
    {
        if (p_pic->date < p_cur->date)
        {
            p_pic->p_next = p_cur;
            *pp_last = p_pic;
            return;
        }
    }
    *pp_last = p_pic;
}

static picture_t *PicReorder_pop(decoder_t *p_dec, bool force)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->i_pic_reorder == 0
     || (!force && p_sys->i_pic_reorder < p_sys->i_pic_reorder_max))
        return NULL;

    picture_t *p_pic = p_sys->p_pic_reorder;
    p_sys->p_pic_reorder = p_pic->p_next;
    p_pic->p_next = NULL;
    p_sys->i_pic_reorder--;
    return p_pic;
}

static void PicReorder_flush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    for (picture_t *p_cur = p_sys->p_pic_reorder, *p_next;
         p_cur != NULL; p_cur = p_next)
    {
        p_next = p_cur->p_next;
        picture_Release(p_cur);
    }
    p_sys->i_pic_reorder = 0;
    p_sys->p_pic_reorder = NULL;
}

static void Flush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* There is no Flush in VT api, ask to restart VT from next DecodeBlock if
     * we already feed some input blocks (it's better to not restart here in
     * order to avoid useless restart just before a close). */
    p_sys->b_vt_flush = p_sys->b_vt_feed;
}

static picture_t *DecodeBlock(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->b_vt_flush) {
        RestartVideoToolbox(p_dec, false);
        p_sys->b_vt_flush = false;
    }

    if (!pp_block)
    {
        /* draining: return last pictures of the reordered queue */
        if (p_sys->session)
            VTDecompressionSessionWaitForAsynchronousFrames(p_sys->session);
        vlc_mutex_lock(&p_sys->lock);
        picture_t *p_pic = PicReorder_pop(p_dec, true);
        vlc_mutex_unlock(&p_sys->lock);
        return p_pic;
    }

    block_t *p_block = *pp_block;
    *pp_block = NULL;

    if (p_block == NULL)
        return NULL; /* no need to be called again, pics are queued asynchronously */

    vlc_mutex_lock(&p_sys->lock);
    if (p_sys->b_abort) { /* abort from output thread (DecoderCallback) */
        vlc_mutex_unlock(&p_sys->lock);
        goto reload;
    }
    vlc_mutex_unlock(&p_sys->lock);

    if (unlikely(p_block->i_flags&(BLOCK_FLAG_CORRUPTED)))
    {
        if (p_sys->b_vt_feed)
            RestartVideoToolbox(p_dec, false);
        goto skip;
    }

    if (!p_sys->session) {
        /* decoding didn't start yet, which is ok for H264, let's see
         * if we can use this block to get going */
        p_sys->codec = kCMVideoCodecType_H264;
        StartVideoToolbox(p_dec, p_block);
    }
    if (!p_sys->session)
        goto skip;

    if (p_block->i_pts == VLC_TS_INVALID && p_block->i_dts != VLC_TS_INVALID &&
        p_sys->i_pic_reorder_max > 1)
    {
        /* VideoToolbox won't reorder output frames and there is no way to know
         * the right order. Abort and use an other decoder. */
        msg_Warn(p_dec, "unable to reorder output frames, abort");
        goto reload;
    }

    if (p_sys->codec == kCMVideoCodecType_H264) {
        p_block = H264ProcessBlock(p_dec, p_block);
        if (!p_block)
            return NULL;
    }

    CMSampleBufferRef sampleBuffer =
        VTSampleBufferCreate(p_dec, p_sys->videoFormatDescription, p_block);
    if (unlikely(!sampleBuffer))
        goto skip;

    VTDecodeInfoFlags flagOut;
    VTDecodeFrameFlags decoderFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
    if (unlikely(p_sys->b_enable_temporal_processing))
        decoderFlags |= kVTDecodeFrame_EnableTemporalProcessing;

    OSStatus status =
        VTDecompressionSessionDecodeFrame(p_sys->session, sampleBuffer,
                                          decoderFlags, NULL, &flagOut);
    CFRelease(sampleBuffer);
    if (status == noErr)
        p_sys->b_vt_feed = true;
    else {
        if (status == kCVReturnInvalidSize)
            msg_Err(p_dec, "decoder failure: invalid block size");
        else if (status == -666)
            msg_Err(p_dec, "decoder failure: invalid SPS/PPS");
        else if (status == -6661) {
            msg_Err(p_dec, "decoder failure: invalid argument");
            goto reload;
        } else if (status == -8969 || status == -12909) {
            msg_Err(p_dec, "decoder failure: bad data (%i)", (int)status);
            StopVideoToolbox(p_dec);
        } else if (status == -8960 || status == -12911) {
            msg_Err(p_dec, "decoder failure: internal malfunction (%i)", (int)status);
            RestartVideoToolbox(p_dec, true);
        } else if (status == -12903) {
            msg_Warn(p_dec, "decoder failure: session invalid");
            RestartVideoToolbox(p_dec, true);
        } else
            msg_Dbg(p_dec, "decoding frame failed (%i)", (int)status);
    }

skip:
    block_Release(p_block);
    return NULL;

reload:
    /* Add an empty variable so that videotoolbox won't be loaded again for
     * this ES */
     if (var_Create(p_dec, "videotoolbox-failed", VLC_VAR_VOID) == VLC_SUCCESS)
        decoder_RequestReload(p_dec);
    else
        p_dec->b_error = true;
    block_Release(p_block);
    return NULL;
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
    VLC_UNUSED(sourceFrameRefCon);
    VLC_UNUSED(duration);
    decoder_t *p_dec = (decoder_t *)decompressionOutputRefCon;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (status != noErr) {
        msg_Warn(p_dec, "decoding of a frame failed (%i, %u)", (int)status, (unsigned int) infoFlags);
        return;
    }
    assert(imageBuffer);

    if (unlikely(!p_sys->b_format_propagated)) {
        vlc_mutex_lock(&p_sys->lock);
        p_sys->b_format_propagated =
            UpdateVideoFormat(p_dec, imageBuffer) == VLC_SUCCESS;
        vlc_mutex_unlock(&p_sys->lock);

        if (!p_sys->b_format_propagated)
            return;
        assert(p_dec->fmt_out.i_codec != 0);
    }

    if (infoFlags & kVTDecodeInfo_FrameDropped)
    {
        msg_Dbg(p_dec, "decoder dropped frame");
        return;
    }

    if (!CMTIME_IS_VALID(pts))
        return;

    if (CVPixelBufferGetDataSize(imageBuffer) == 0)
        return;

    picture_t *p_pic = decoder_NewPicture(p_dec);
    if (!p_pic)
        return;
    if (!p_pic->p_sys) {
        vlc_mutex_lock(&p_sys->lock);
        p_dec->p_sys->b_abort = true;
        vlc_mutex_unlock(&p_sys->lock);
        picture_Release(p_pic);
        return;
    }

    /* Can happen if the pic was discarded */
    if (p_pic->p_sys->pixelBuffer != nil)
        CFRelease(p_pic->p_sys->pixelBuffer);

    /* will be freed by the vout */
    p_pic->p_sys->pixelBuffer = CVPixelBufferRetain(imageBuffer);

    p_pic->date = pts.value;
    p_pic->b_progressive = true;

    vlc_mutex_lock(&p_sys->lock);
    PicReorder_pushSorted(p_dec, p_pic);
    p_pic = PicReorder_pop(p_dec, false);
    vlc_mutex_unlock(&p_sys->lock);

    if (p_pic != NULL)
        decoder_QueueVideo(p_dec, p_pic);
    return;
}
