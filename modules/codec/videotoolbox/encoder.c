/*****************************************************************************
 * videotoolbox/encoder.c: Video Toolbox encoder
 *****************************************************************************
 * Copyright © 2023 VideoLabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
 *          Marvin Scholz <epirat07 at gmail dot com>
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

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import <vlc_common.h>
#import <vlc_plugin.h>
#import <vlc_codec.h>
#import "hxxx_helper.h"
#import <vlc_bits.h>
#import <vlc_threads.h>
#import "vt_utils.h"
#import "../packetizer/h264_nal.h"
#import "../packetizer/h264_slice.h"
#import "../packetizer/hxxx_nal.h"
#import "../packetizer/hxxx_sei.h"

#import <VideoToolbox/VideoToolbox.h>
#import <VideoToolbox/VTErrors.h>

#import <CoreFoundation/CoreFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreMedia/CMTime.h>
#import <TargetConditionals.h>

#import <sys/types.h>
#import <sys/sysctl.h>
#import <mach/machine.h>

#pragma mark Encoder submodule

typedef struct encoder_sys_t
{
    VTCompressionSessionRef session;
    CMTime lastDate;
    bool isDraining;
    vlc_fifo_t *fifo;
    bool header;

    int NALUnitHeaderLengthOut;
} encoder_sys_t;

static block_t *EncodeCallback(encoder_t *enc, picture_t *pic)
{
    encoder_sys_t *sys = enc->p_sys;

    /* If we're draining, we have nothing to push. */
    sys->isDraining |= pic == NULL;
    if (!pic)
        goto return_block;

    picture_Hold(pic);

    CVPixelBufferRef buffer = cvpxpic_get_ref(pic);
    CMTime pts = CMTimeMake(pic->date, CLOCK_FREQ);
    CMTime duration = kCMTimeInvalid;
    VTEncodeInfoFlags infoFlags;

    OSStatus ret = VTCompressionSessionEncodeFrame(sys->session,
        buffer, pts, duration,
        NULL,
        pic,
        &infoFlags);
    /* VTCompressionSessionEncodeFrame can return error if the parameters
     * given above are not correct, like kVTParameterErr if session is NULL
     * so ensure the core is correct but don't overcheck in release. */
    assert(ret == noErr);

return_block:
    /* If we're draining, we wait for the encoder to output every other
     * block and gather them together before returning. */
    if (sys->isDraining)
        VTCompressionSessionCompleteFrames(sys->session, kCMTimeInvalid);

    /* Dequeue all available blocks. */
    vlc_fifo_Lock(sys->fifo);
    block_t *block = vlc_fifo_DequeueAllUnlocked(sys->fifo);
    vlc_fifo_Unlock(sys->fifo);

    if (pic)
        sys->lastDate = CMTimeMake(pic->date, CLOCK_FREQ);

    vlc_fifo_Signal(sys->fifo);

    return block;
}

static vlc_tick_t vlc_CMTime_to_tick(CMTime timestamp)
{
    CMTime scaled = CMTimeConvertScale(
            timestamp, CLOCK_FREQ,
            kCMTimeRoundingMethod_Default);

    return VLC_TICK_0 + scaled.value;
}

static int PushBlockUnlocked(encoder_t *enc, CMSampleBufferRef sampleBuffer)
{
    static const uint8_t startcode[] = {0x00, 0x00, 0x00, 0x01};
    encoder_sys_t *sys = enc->p_sys;

    CMTime pts = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    CMTime dts = CMSampleBufferGetDecodeTimeStamp(sampleBuffer);

    vlc_tick_t vlc_pts = vlc_CMTime_to_tick(pts);
    /* Note: dts can be !CMTIME_IS_VALID() when there are B-frames.
     * We need to reorder the frames when it happens, but B-frames are
     * disabled for now. */
    vlc_tick_t vlc_dts = CMTIME_IS_VALID(dts) ?
        vlc_CMTime_to_tick(dts) : vlc_pts;

    CMBlockBufferRef buffer = CMSampleBufferGetDataBuffer(sampleBuffer);

    size_t length = CMBlockBufferGetDataLength(buffer);
    size_t read;
    size_t offset = 0;

    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, 0);
    CFDictionaryRef properties = nil;

    /* Frames are considered to be IDR frames by default:
     * https://developer.apple.com/documentation/coremedia/kcmsampleattachmentkey_notsync */
    Boolean isIDR = TRUE;

    /* We need this attachments array to extract metadata. */
    if (attachments == NULL || CFArrayGetCount(attachments) == 0)
        goto parse_block;

    /* Metadata parsing:
     * We need to check whether the frame is an IDR frame or not
     * in order to know whether we need to inject the SPS/PPS
     * NAL units in the stream. */
    properties = CFArrayGetValueAtIndex(attachments, 0);

    CFBooleanRef isNotSync;
    if (CFDictionaryGetValueIfPresent(properties,
                                      kCMSampleAttachmentKey_NotSync,
                                      (const void**)&isNotSync))
    {
        /* If the attachment signal that it's not a sync frame,
         * it means that it's not an IDR frame. */
        isIDR = !CFBooleanGetValue(isNotSync);
    }

    block_t *header = NULL;
    if (isIDR && !sys->header)
    {
        CMFormatDescriptionRef description =
            CMSampleBufferGetFormatDescription(sampleBuffer);

        const uint8_t *sps, *pps;
        size_t sps_length, pps_length;

        OSStatus status;
        int NALUnitHeaderLengthOut;
        /* The following function will already handle emulation prevention bytes. */
        status = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(description,
                0, &sps, &sps_length, NULL, &NALUnitHeaderLengthOut);
        /* This should only fail here if we gave the wrong values above */
        assert(status == noErr);
        sys->NALUnitHeaderLengthOut = NALUnitHeaderLengthOut;
        status = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(description,
                1, &pps, &pps_length, NULL, NULL);
        /* This should only fail here if we gave the wrong values above */
        assert(status == noErr);

        assert(sps != NULL && pps != NULL);
        assert(sps_length < (1 << 16));
        assert(pps_length < (1 << 16));

        size_t i_extra = sps_length + pps_length + sizeof(startcode) * 2;
        header = block_Alloc(i_extra);
        if (header == NULL){} // TODO

        size_t hdroffset = 0;
        memcpy(&header->p_buffer[hdroffset], startcode, sizeof(startcode));
        hdroffset += sizeof(startcode);

        memcpy(&header->p_buffer[hdroffset], sps, sps_length);
        hdroffset += sps_length;

        memcpy(&header->p_buffer[hdroffset], startcode, sizeof(startcode));
        hdroffset += sizeof(startcode);

        memcpy(&header->p_buffer[hdroffset], pps, pps_length);
        header->i_buffer = i_extra;
        sys->header = true;
    }

parse_block:

    while (offset != length)
    {
        OSStatus status;
        char *cursor_ptr;
        status = CMBlockBufferGetDataPointer(buffer, offset, &read, NULL, &cursor_ptr);
        uint8_t *cursor = (uint8_t *)cursor_ptr;

        /* Did we used an invalid CMBlockBuffer from invalid offset somehow? */
        assert(status == kCMBlockBufferNoErr);

        

        hxxx_iterator_ctx_t hh;
        hxxx_iterator_init(&hh, cursor, read, sys->NALUnitHeaderLengthOut);
        const uint8_t *hh_start;
        size_t hh_size, block_size = 0, block_offset = 0;
        while (hxxx_iterate_next(&hh, &hh_start, &hh_size))
            block_size += sizeof(startcode) + hh_size;

        offset += read;
        block_t *block = block_Alloc(block_size);

        hxxx_iterator_init(&hh, cursor, read, sys->NALUnitHeaderLengthOut);

      /* Extract the avcC block size and copy each NAL to the same block.
       * Multiple NAL can be present in different situation, like I-frame
       * with SEI data. */
        while (hxxx_iterate_next(&hh, &hh_start, &hh_size))
        {
            memcpy(&block->p_buffer[block_offset], startcode, sizeof startcode);
            memcpy(&block->p_buffer[block_offset + sizeof startcode], hh_start, hh_size);
            block_offset += sizeof(startcode) + hh_size;
        }
        block->i_pts = vlc_pts;
        block->i_dts = vlc_dts;
        block->i_flags = isIDR ? BLOCK_FLAG_TYPE_I : 0;
        block->i_flags |= BLOCK_FLAG_AU_END;

        if (header)
        {
            header->p_next = block;
            block = block_ChainGather(header);
            assert(block != NULL);
        }
        vlc_fifo_QueueUnlocked(sys->fifo, block);
    }

    return VLC_SUCCESS;
}

static OSStatus PushEachBlockUnlocked(CMSampleBufferRef sampleBuffer,
        CMItemCount index, void *opaque)
{
    encoder_t *encoder = opaque;
    (void)index;

    PushBlockUnlocked(encoder, sampleBuffer);
    return noErr;
}

static void EncoderOutputCallback(void *cookie,
    void *sourceFrameRefCon, OSStatus status,
    VTEncodeInfoFlags infoFlags, CMSampleBufferRef sampleBuffer)
{
    VLC_UNUSED(infoFlags);

    encoder_t *encoder = cookie;
    encoder_sys_t *sys = encoder->p_sys;

    /* Nothing doable here. */
    if (status != noErr)
        return;

    picture_t *pic = sourceFrameRefCon;

    picture_Release(pic);
    vlc_fifo_Lock(sys->fifo);
    CMSampleBufferCallForEachSample(sampleBuffer, PushEachBlockUnlocked, encoder);
    vlc_fifo_Unlock(sys->fifo);
}

static void CloseEncoder(encoder_t *encoder)
{
    encoder_sys_t *p_sys = encoder->p_sys;

    VTCompressionSessionCompleteFrames(p_sys->session, kCMTimeInvalid);
    VTCompressionSessionInvalidate(p_sys->session);

    block_FifoRelease(p_sys->fifo);

    CFRelease(p_sys->session);
    p_sys->session = NULL;
}

static int OpenEncoder(vlc_object_t *obj)
{
    encoder_t *enc = (encoder_t *)obj;
    encoder_sys_t *sys;

    if (enc->fmt_out.i_codec != VLC_CODEC_H264)
        return VLC_EGENERIC;

    sys = vlc_obj_malloc(obj, sizeof *sys);
    if (sys == NULL)
        return VLC_ENOMEM;

    sys->fifo = block_FifoNew();
    if (sys->fifo == NULL)
        return VLC_ENOMEM;
    sys->lastDate = CMTimeMake(0, CLOCK_FREQ);
    sys->header = false;

    OSStatus ret = VTCompressionSessionCreate(NULL,
        enc->fmt_in.video.i_visible_width,
        enc->fmt_in.video.i_visible_height,
        kCMVideoCodecType_H264,
        NULL,
        NULL,
        NULL,
        EncoderOutputCallback,
        enc,
        &sys->session);

    if (ret != noErr)
        goto error_session;

    ret = VTSessionSetProperty(sys->session,
            kVTCompressionPropertyKey_AllowFrameReordering,
            kCFBooleanFalse);

    if (ret != noErr)
        goto error_property;

    static const struct vlc_encoder_operations ops =
    {
        .encode_video = EncodeCallback,
        .close = CloseEncoder,
    };
    enc->p_sys = sys;
    enc->ops = &ops;

    switch (enc->fmt_in.i_codec)
    {
        case VLC_CODEC_CVPX_NV12:
        case VLC_CODEC_CVPX_BGRA:
        case VLC_CODEC_CVPX_UYVY:
        case VLC_CODEC_CVPX_P010:
        case VLC_CODEC_CVPX_I420:
            break;

        default:
            /* Note: QuickTime prefers I420 rather than NV12. */
            enc->fmt_in.i_codec = VLC_CODEC_CVPX_I420;
    }

    video_format_Copy(&enc->fmt_out.video, &enc->fmt_in.video);
    enc->fmt_out.video.i_frame_rate =
        enc->fmt_in.video.i_frame_rate;
    enc->fmt_out.video.i_frame_rate_base =
        enc->fmt_in.video.i_frame_rate_base;
    enc->fmt_out.i_codec = VLC_CODEC_H264;

    msg_Info(enc, "Videotoolbox encoding %4.4s framerate %d/%d size %dx%d",
             (const char *)&enc->fmt_out.i_codec,
             enc->fmt_in.video.i_frame_rate,
             enc->fmt_in.video.i_frame_rate_base,
             enc->fmt_in.video.i_visible_width,
             enc->fmt_in.video.i_visible_height);

    return VLC_SUCCESS;
error_property:
    VTCompressionSessionInvalidate(sys->session);
    CFRelease(sys->session);
error_session:
    block_FifoRelease(sys->fifo);
    return VLC_EGENERIC;
}

#pragma mark - Module descriptor

vlc_module_begin()
    set_section(N_("Encoding") , NULL)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_description(N_("VideoToolbox video encoder"))
    set_capability("video encoder", 1000)
    set_callback(OpenEncoder)
vlc_module_end()
