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

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1090
const CFStringRef kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder = CFSTR("EnableHardwareAcceleratedVideoDecoder");
const CFStringRef kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder = CFSTR("RequireHardwareAcceleratedVideoDecoder");
#endif

#define VT_ZERO_COPY N_("Use zero-copy rendering")
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
add_bool("videotoolbox-zero-copy", false, VT_ZERO_COPY, VT_ZERO_COPY, false)
add_bool("videotoolbox-hw-decoder-only", false, VT_REQUIRE_HW_DEC, VT_REQUIRE_HW_DEC, false)
#else
add_bool("videotoolbox-zero-copy", true, VT_ZERO_COPY, VT_ZERO_COPY, false)
#endif
vlc_module_end()

#pragma mark - local prototypes

static CFDataRef ESDSCreate(decoder_t *, uint8_t *, uint32_t);
static picture_t *DecodeBlock(decoder_t *, block_t **);
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

    bool                        b_started;
    bool                        b_is_avcc;
    VTDecompressionSessionRef   session;
    CMVideoFormatDescriptionRef videoFormatDescription;

    NSMutableArray              *outputTimeStamps;
    NSMutableDictionary         *outputFrames;
    bool                        b_zero_copy;
    bool                        b_enable_temporal_processing;

    bool                        b_format_propagated;
};

#pragma mark - start & stop

static CMVideoCodecType CodecPrecheck(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    size_t i_profile = 0xFFFF, i_level = 0xFFFF;
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

            msg_Dbg(p_dec, "trying to decode MPEG-4 Part 10: profile %zu, level %zu", i_profile, i_level);

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
                    msg_Dbg(p_dec, "unsupported H264 profile %zu", i_profile);
                    return -1;
                }
            }

#if !TARGET_OS_IPHONE
            /* a level higher than 5.2 was not tested, so don't dare to
             * try to decode it*/
            if (i_level > 52) {
                msg_Dbg(p_dec, "unsupported H264 level %zu", i_level);
                return -1;
            }
#else
            /* on SoC A8, 4.2 is the highest specified profile */
            if (i_level > 42) {
                /* on Twister, we can do up to 5.2 */
                if (!deviceSupportsAdvancedLevels() || i_level > 52) {
                    msg_Dbg(p_dec, "unsupported H264 level %zu", i_level);
                    return -1;
                }
            }
#endif

            break;
        case VLC_CODEC_MP4V:
            codec = kCMVideoCodecType_MPEG4Video;
            break;
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

static int StartVideoToolbox(decoder_t *p_dec, block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OSStatus status;

    /* setup the decoder */
    CFMutableDictionaryRef decoderConfiguration = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                            2,
                                                                            &kCFTypeDictionaryKeyCallBacks,
                                                                            &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(decoderConfiguration,
                         kCVImageBufferChromaLocationBottomFieldKey,
                         kCVImageBufferChromaLocation_Left);
    CFDictionarySetValue(decoderConfiguration,
                         kCVImageBufferChromaLocationTopFieldKey,
                         kCVImageBufferChromaLocation_Left);
    p_sys->b_zero_copy = var_InheritBool(p_dec, "videotoolbox-zero-copy");

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
        uint8_t *p_buf = NULL;
        uint8_t *p_alloc_buf = NULL;
        int i_ret = 0;

        if (p_block == NULL) {
            /* we need to convert the SPS and PPS units we received from the
            * demuxer's avvC atom so we can process them further */
            if(h264_isavcC(p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra))
            {
                p_alloc_buf = h264_avcC_to_AnnexB_NAL(p_dec->fmt_in.p_extra,
                                                      p_dec->fmt_in.i_extra,
                                                      &i_buf,
                                                      &p_sys->i_nal_length_size);
                p_buf = p_alloc_buf;
                p_sys->b_is_avcc = !!p_buf;
            }
        } else {
            /* we are mid-stream, let's have the h264_get helper see if it
             * can find a NAL unit */
            i_buf = p_block->i_buffer;
            p_buf = p_block->p_buffer;
            p_sys->i_nal_length_size = 4; /* default to 4 bytes */
            i_ret = VLC_SUCCESS;
        }

        uint8_t *p_sps_ab = NULL, *p_pps_ab = NULL, *p_ext_ab = NULL;
        size_t i_sps_absize = 0, i_pps_absize = 0, i_ext_absize = 0;
        if (!p_buf) {
            msg_Warn(p_dec, "no valid extradata or conversion failed");
            return VLC_EGENERIC;
        }

        /* get the SPS and PPS units from the NAL unit which is either
         * part of the demuxer's avvC atom or the mid stream data block */
        i_ret = h264_get_spspps(p_buf, i_buf,
                                &p_sps_ab, &i_sps_absize,
                                &p_pps_ab, &i_pps_absize,
                                &p_ext_ab, &i_ext_absize);
        if(p_alloc_buf)
            free(p_alloc_buf);
        if (i_ret != VLC_SUCCESS) {
            msg_Warn(p_dec, "sps pps detection failed");
            return VLC_EGENERIC;
        }

        /* Decode Sequence Parameter Set */
        if( p_sps_ab )
        {
            const uint8_t *p_stp_sps_buf = p_sps_ab;
            size_t i_stp_sps_nal = i_sps_absize;
            h264_sequence_parameter_set_t *p_sps_data;
            if( ! hxxx_strip_AnnexB_startcode( &p_stp_sps_buf, &i_stp_sps_nal ) ||
                !( p_sps_data = h264_decode_sps(p_stp_sps_buf, i_stp_sps_nal, true) ) )
            {
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

            h264_release_sps( p_sps_data );
        }
        /* !Decode Sequence Parameter Set */

        if(!p_sys->b_is_avcc)
        {
            block_t *p_avcC = h264_AnnexB_NAL_to_avcC(
                                    p_sys->i_nal_length_size,
                                    p_sps_ab, i_sps_absize,
                                    p_pps_ab, i_pps_absize);
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

        if (extradata)
            CFDictionarySetValue(extradata_info, CFSTR("avcC"), extradata);

        CFDictionarySetValue(decoderConfiguration,
                             kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
                             extradata_info);

    } else if (p_sys->codec == kCMVideoCodecType_MPEG4Video) {
        extradata = ESDSCreate(p_dec,
                               (uint8_t*)p_dec->fmt_in.p_extra,
                               p_dec->fmt_in.i_extra);

        if (extradata)
            CFDictionarySetValue(extradata_info, CFSTR("esds"), extradata);

        CFDictionarySetValue(decoderConfiguration,
                             kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
                             extradata_info);
    } else {
        CFDictionarySetValue(decoderConfiguration,
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
    CFDictionarySetValue(decoderConfiguration,
                         kCVImageBufferPixelAspectRatioKey,
                         pixelaspectratio);
    CFRelease(pixelaspectratio);

#if !TARGET_OS_IPHONE
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
#endif

    p_sys->b_enable_temporal_processing = false;
    if (var_InheritInteger(p_dec, "videotoolbox-temporal-deinterlacing")) {
        if (p_block != NULL) {
            if (p_block->i_flags & BLOCK_FLAG_TOP_FIELD_FIRST ||
                p_block->i_flags & BLOCK_FLAG_BOTTOM_FIELD_FIRST) {
                msg_Dbg(p_dec, "Interlaced content detected, inserting temporal deinterlacer");
                CFDictionarySetValue(decoderConfiguration, kVTDecompressionPropertyKey_FieldMode, kVTDecompressionProperty_FieldMode_DeinterlaceFields);
                CFDictionarySetValue(decoderConfiguration, kVTDecompressionPropertyKey_DeinterlaceMode, kVTDecompressionProperty_DeinterlaceMode_Temporal);
                p_sys->b_enable_temporal_processing = true;
            }
        }
    }

    /* create video format description */
    status = CMVideoFormatDescriptionCreate(kCFAllocatorDefault,
                                            p_sys->codec,
                                            i_video_width,
                                            i_video_height,
                                            decoderConfiguration,
                                            &p_sys->videoFormatDescription);
    if (status) {
        CFRelease(decoderConfiguration);
        msg_Err(p_dec, "video format description creation failed (%i)", status);
        return VLC_EGENERIC;
    }

    /* destination pixel buffer attributes */
    CFMutableDictionaryRef dpba = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                            2,
                                                            &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);

#if !TARGET_OS_IPHONE
    CFDictionarySetValue(dpba,
                         kCVPixelBufferOpenGLCompatibilityKey,
                         kCFBooleanTrue);
#else
    CFDictionarySetValue(dpba,
                         kCVPixelBufferOpenGLESCompatibilityKey,
                         kCFBooleanTrue);
#endif

    /* full range allows a broader range of colors but is H264 only */
    if (p_sys->codec == kCMVideoCodecType_H264) {
        VTDictionarySetInt32(dpba,
                             kCVPixelBufferPixelFormatTypeKey,
                             kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);
    } else {
        VTDictionarySetInt32(dpba,
                             kCVPixelBufferPixelFormatTypeKey,
                             kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
    }
    VTDictionarySetInt32(dpba,
                         kCVPixelBufferWidthKey,
                         i_video_width);
    VTDictionarySetInt32(dpba,
                         kCVPixelBufferHeightKey,
                         i_video_height);
    VTDictionarySetInt32(dpba,
                         kCVPixelBufferBytesPerRowAlignmentKey,
                         i_video_width * 2);

    /* setup decoder callback record */
    VTDecompressionOutputCallbackRecord decoderCallbackRecord;
    decoderCallbackRecord.decompressionOutputCallback = DecoderCallback;
    decoderCallbackRecord.decompressionOutputRefCon = p_dec;

    /* create decompression session */
    status = VTDecompressionSessionCreate(kCFAllocatorDefault,
                                          p_sys->videoFormatDescription,
                                          decoderConfiguration,
                                          dpba,
                                          &decoderCallbackRecord,
                                          &p_sys->session);

    /* release no longer needed storage items */
    CFRelease(dpba);
    CFRelease(decoderConfiguration);

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
                msg_Err(p_dec, "Decompression session creation failed (%i)", status);
                break;
        }
        return VLC_EGENERIC;
    }

    p_dec->fmt_out.video.i_width = i_video_width;
    p_dec->fmt_out.video.i_height = i_video_height;
    p_dec->fmt_out.video.i_visible_width = i_video_visible_width;
    p_dec->fmt_out.video.i_visible_height = i_video_visible_height;
    p_dec->fmt_out.video.i_sar_den = i_sar_den;
    p_dec->fmt_out.video.i_sar_num = i_sar_num;

    if (p_block) {
        /* this is a mid stream change so we need to tell the core about it */
        decoder_UpdateVideoFormat(p_dec);
    }

    /* setup storage */
    p_sys->outputTimeStamps = [[NSMutableArray alloc] init];
    p_sys->outputFrames = [[NSMutableDictionary alloc] init];
    if (!p_sys->outputFrames) {
        msg_Warn(p_dec, "buffer management structure allocation failed");
        return VLC_ENOMEM;
    }

    p_sys->b_started = YES;

    return VLC_SUCCESS;
}

static void StopVideoToolbox(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->b_started) {
        if (p_sys->outputTimeStamps != nil)
            CFRelease(p_sys->outputTimeStamps);
        p_sys->outputTimeStamps = nil;
        if (p_sys->outputFrames != nil)
            CFRelease(p_sys->outputFrames);
        p_sys->outputFrames = nil;

        p_sys->b_started = false;
        if (p_sys->session != nil) {
            VTDecompressionSessionInvalidate(p_sys->session);
            CFRelease(p_sys->session);
            p_sys->session = nil;
        }

        p_sys->b_format_propagated = false;
    }

    if (p_sys->videoFormatDescription != nil) {
        CFRelease(p_sys->videoFormatDescription);
        p_sys->videoFormatDescription = nil;
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
    p_sys->b_started = false;
    p_sys->b_is_avcc = false;
    p_sys->codec = codec;
    p_sys->videoFormatDescription = nil;

    int i_ret = StartVideoToolbox(p_dec, NULL);
    if (i_ret != VLC_SUCCESS) {
        CloseDecoder(p_this);
        return i_ret;
    }

    /* return our proper VLC internal state */
    p_dec->fmt_out.i_cat = p_dec->fmt_in.i_cat;
    p_dec->fmt_out.video = p_dec->fmt_in.video;
    p_dec->fmt_out.audio = p_dec->fmt_in.audio;
    if (p_sys->b_zero_copy) {
        msg_Dbg(p_dec, "zero-copy rendering pipeline enabled");
        p_dec->fmt_out.i_codec = VLC_CODEC_CVPX_OPAQUE;
    } else {
        msg_Dbg(p_dec, "copy rendering pipeline enabled");
        p_dec->fmt_out.i_codec = VLC_CODEC_I420;
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

    if (p_sys->session && p_sys->b_started) {
        VTDecompressionSessionWaitForAsynchronousFrames(p_sys->session);
    }
    StopVideoToolbox(p_dec);

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

    if (p_sys->b_is_avcc) /* FIXME: no change checks done for AVC ? */
        return p_block;

    return (p_block) ? hxxx_AnnexB_to_xVC(p_block, p_sys->i_nal_length_size) : NULL;
}

static CMSampleBufferRef VTSampleBufferCreate(decoder_t *p_dec,
                                              CMFormatDescriptionRef fmt_desc,
                                              void *buffer,
                                              size_t size,
                                              mtime_t i_pts,
                                              mtime_t i_dts,
                                              mtime_t i_length)
{
    OSStatus status;
    CMBlockBufferRef  block_buf = NULL;
    CMSampleBufferRef sample_buf = NULL;

    CMSampleTimingInfo timeInfo;
    CMSampleTimingInfo timeInfoArray[1];

    timeInfo.duration = CMTimeMake(i_length, 1);
    timeInfo.presentationTimeStamp = CMTimeMake(i_pts > 0 ? i_pts : i_dts, CLOCK_FREQ);
    timeInfo.decodeTimeStamp = CMTimeMake(i_dts, CLOCK_FREQ);
    timeInfoArray[0] = timeInfo;

    status = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault,// structureAllocator
                                                buffer,             // memoryBlock
                                                size,               // blockLength
                                                kCFAllocatorNull,   // blockAllocator
                                                NULL,               // customBlockSource
                                                0,                  // offsetToData
                                                size,               // dataLength
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
            msg_Warn(p_dec, "sample buffer creation failure %i", status);
    } else
        msg_Warn(p_dec, "cm block buffer creation failure %i", status);

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

    if (!buffer)
        return;

    CVPixelBufferLockBaseAddress(buffer, 0);

    for (int i = 0; i < 2; i++) {
        pp_plane[i] = CVPixelBufferGetBaseAddressOfPlane(buffer, i);
        pi_pitch[i] = CVPixelBufferGetBytesPerRowOfPlane(buffer, i);
    }

    CopyFromNv12ToI420(p_pic, pp_plane, pi_pitch, i_width, i_height);

    CVPixelBufferUnlockBaseAddress(buffer, 0);
}

#pragma mark - actual decoding

static void Flush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (likely(p_sys->b_started)) {
        @synchronized(p_sys->outputTimeStamps) {
            [p_sys->outputTimeStamps removeAllObjects];
        }
        @synchronized(p_sys->outputFrames) {
            [p_sys->outputFrames removeAllObjects];
        }
    }
}

static picture_t *DecodeBlock(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    VTDecodeFrameFlags decoderFlags = 0;
    VTDecodeInfoFlags flagOut;
    OSStatus status;
    int i_ret = 0;

    if (!pp_block)
        return NULL;

    p_block = *pp_block;

    if (likely(p_block != NULL)) {
        if (unlikely(p_block->i_flags&(BLOCK_FLAG_CORRUPTED))) {
            Flush(p_dec);
            block_Release(p_block);
            goto skip;
        }

        /* feed to vt */
        if (likely(p_block->i_buffer)) {
            if (!p_sys->b_started) {
                /* decoding didn't start yet, which is ok for H264, let's see
                 * if we can use this block to get going */
                p_sys->codec = kCMVideoCodecType_H264;
                i_ret = StartVideoToolbox(p_dec, p_block);
            }
            if (i_ret != VLC_SUCCESS || !p_sys->b_started) {
                *pp_block = NULL;
                return NULL;
            }

            if (p_sys->codec == kCMVideoCodecType_H264) {
                p_block = H264ProcessBlock(p_dec, p_block);
                if (!p_block)
                {
                    *pp_block = NULL;
                    return NULL;
                }
            }

            CMSampleBufferRef sampleBuffer;
            sampleBuffer = VTSampleBufferCreate(p_dec,
                                                p_sys->videoFormatDescription,
                                                p_block->p_buffer,
                                                p_block->i_buffer,
                                                p_block->i_pts,
                                                p_block->i_dts,
                                                p_block->i_length);
            if (likely(sampleBuffer)) {
                if (likely(!p_sys->b_enable_temporal_processing))
                    decoderFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
                else
                    decoderFlags = kVTDecodeFrame_EnableAsynchronousDecompression | kVTDecodeFrame_EnableTemporalProcessing;

                status = VTDecompressionSessionDecodeFrame(p_sys->session,
                                                           sampleBuffer,
                                                           decoderFlags,
                                                           NULL, // sourceFrameRefCon
                                                           &flagOut); // infoFlagsOut
                if (status != noErr) {
                    if (status == kCVReturnInvalidSize)
                        msg_Err(p_dec, "decoder failure: invalid block size");
                    else if (status == -666)
                        msg_Err(p_dec, "decoder failure: invalid SPS/PPS");
                    else if (status == -6661) {
                        msg_Err(p_dec, "decoder failure: invalid argument");
                        p_dec->b_error = true;
                    } else if (status == -8969 || status == -12909) {
                        msg_Err(p_dec, "decoder failure: bad data (%i)", status);
                        StopVideoToolbox(p_dec);
                    } else if (status == -8960 || status == -12911) {
                        msg_Err(p_dec, "decoder failure: internal malfunction (%i)", status);
                        StopVideoToolbox(p_dec);
                    } else
                        msg_Dbg(p_dec, "decoding frame failed (%i)", status);
                }

                if (likely(sampleBuffer != nil))
                    CFRelease(sampleBuffer);
                sampleBuffer = nil;
            }
        }

        block_Release(p_block);
    }

skip:

    *pp_block = NULL;

    if (unlikely(!p_sys->b_started))
        return NULL;

    NSUInteger outputFramesCount = [p_sys->outputFrames count];

    if (outputFramesCount > 5) {
        CVPixelBufferRef imageBuffer = NULL;
        id imageBufferObject = nil;
        picture_t *p_pic = NULL;

        NSString *timeStamp;
        @synchronized(p_sys->outputTimeStamps) {
            [p_sys->outputTimeStamps sortUsingComparator:^(id obj1, id obj2) {
                if ([obj1 longLongValue] > [obj2 longLongValue]) {
                    return (NSComparisonResult)NSOrderedDescending;
                }
                if ([obj1 longLongValue] < [obj2 longLongValue]) {
                    return (NSComparisonResult)NSOrderedAscending;
                }
                return (NSComparisonResult)NSOrderedSame;
            }];
            NSMutableArray *timeStamps = p_sys->outputTimeStamps;
            timeStamp = [timeStamps firstObject];
            if (timeStamps.count > 0) {
                [timeStamps removeObjectAtIndex:0];
            }
        }

        @synchronized(p_sys->outputFrames) {
            imageBufferObject = [p_sys->outputFrames objectForKey:timeStamp];
        }
        imageBuffer = (__bridge CVPixelBufferRef)imageBufferObject;

        if (imageBuffer != NULL) {
            if (CVPixelBufferGetDataSize(imageBuffer) > 0) {
                p_pic = decoder_NewPicture(p_dec);

                if (!p_pic)
                    return NULL;

                if (!p_sys->b_zero_copy) {
                    /* ehm, *cough*, memcpy.. */
                    copy420YpCbCr8Planar(p_pic,
                                         imageBuffer,
                                         CVPixelBufferGetWidthOfPlane(imageBuffer, 0),
                                         CVPixelBufferGetHeightOfPlane(imageBuffer, 0));
                } else {
                    /* the structure is allocated by the vout's pool */
                    if (p_pic->p_sys) {
                        /* if we received a recycled picture from the pool
                         * we need release the previous reference first,
                         * otherwise we would leak it */
                        if (p_pic->p_sys->pixelBuffer != nil) {
                            CFRelease(p_pic->p_sys->pixelBuffer);
                            p_pic->p_sys->pixelBuffer = nil;
                        }

                        p_pic->p_sys->pixelBuffer = CFBridgingRetain(imageBufferObject);
                    }
                    /* will be freed by the vout */
                }

                p_pic->date = timeStamp.longLongValue;

                if (imageBufferObject) {
                    @synchronized(p_sys->outputFrames) {
                        [p_sys->outputFrames removeObjectForKey:timeStamp];
                    }
                }
            }
        }
        return p_pic;
    }

    return NULL;
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

    if (unlikely(!p_sys->b_format_propagated)) {
        CFDictionaryRef attachments = CVBufferGetAttachments(imageBuffer, kCVAttachmentMode_ShouldPropagate);
        NSDictionary *attachmentDict = (NSDictionary *)attachments;
#ifndef NDEBUG
        NSLog(@"%@", attachments);
#endif
        if (attachmentDict != nil) {
            if (attachmentDict.count > 0) {
                p_sys->b_format_propagated = true;

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
                decoder_UpdateVideoFormat(p_dec);
            }
        }
    }

    if (status != noErr) {
        msg_Warn(p_dec, "decoding of a frame failed (%i, %u)", status, (unsigned int) infoFlags);
        return;
    }

    if (imageBuffer == nil)
        return;

    if (infoFlags & kVTDecodeInfo_FrameDropped) {
        msg_Dbg(p_dec, "decoder dropped frame");
        if (imageBuffer != nil)
            CFRelease(imageBuffer);
        imageBuffer = nil;
        return;
    }

    NSString *timeStamp = nil;

    if (CMTIME_IS_VALID(pts))
        timeStamp = [[NSNumber numberWithLongLong:pts.value] stringValue];
    else {
        msg_Dbg(p_dec, "invalid timestamp, dropping frame");
        CFRelease(imageBuffer);
        return;
    }

    if (timeStamp) {
        id imageBufferObject = (__bridge id)imageBuffer;
        @synchronized(p_sys->outputTimeStamps) {
            [p_sys->outputTimeStamps addObject:timeStamp];
        }
        @synchronized(p_sys->outputFrames) {
            [p_sys->outputFrames setObject:imageBufferObject forKey:timeStamp];
        }
    }
}
