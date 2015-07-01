/*****************************************************************************
 * vda.c: VDA helpers for the libavcodec decoder
 *****************************************************************************
 * Copyright (C) 2012-2015 VLC authors VideoLAN
 *
 * Authors: Sebastien Zwickert <dilaroga@free.fr>
 *          Rémi Denis-Courmont <remi # remlab : net>
 *          Felix Paul Kühne <fkuehne # videolan org>
 *          David Fuhrmann <david.fuhrmann # googlemail com>
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
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_plugin.h>

#include <libavcodec/avcodec.h>

#include "avcodec.h"
#include "va.h"
#include "../../packetizer/h264_nal.h"
#include "../../video_chroma/copy.h"

#include <libavcodec/vda.h>
#include <VideoDecodeAcceleration/VDADecoder.h>

#pragma mark prototypes and definitions

static int Open( vlc_va_t *, AVCodecContext *, enum PixelFormat,
                 const es_format_t *, picture_sys_t * );
static void Close( vlc_va_t * , AVCodecContext *);
static int Setup( vlc_va_t *, AVCodecContext *, vlc_fourcc_t *);
static int Get( vlc_va_t *, picture_t *, uint8_t ** );
static int Extract( vlc_va_t *, picture_t *, uint8_t * );
static void Release( void *opaque, uint8_t *data );

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

vlc_module_begin ()
    set_description( N_("Video Decode Acceleration Framework (VDA)") )
    set_capability( "hw decoder", 0 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_callbacks( Open, Close )
    add_obsolete_integer("avcodec-vda-pix-fmt") /* since 3.0.0 */
vlc_module_end ()

struct vlc_va_sys_t
{
    AVVDAContext *vdactx;
    int i_width;
    int i_height;
};

static int Open(vlc_va_t *va,
                AVCodecContext *avctx,
                enum PixelFormat pix_fmt,
                const es_format_t *fmt,
                picture_sys_t *p_sys)
{
    if( pix_fmt != AV_PIX_FMT_VDA )
        return VLC_EGENERIC;

    (void) fmt;
    (void) p_sys;
    VLC_UNUSED(avctx);

    size_t i_profile = 0xFFFF, i_level = 0xFFFF;
    bool b_ret = false;

    switch (fmt->i_codec) {
        case VLC_CODEC_H264:
            b_ret = h264_get_profile_level(fmt, &i_profile, &i_level, NULL);
            if (!b_ret) {
                msg_Warn( va, "H264 profile and level parsing failed because it didn't arrive yet");
                return VLC_EGENERIC;
            }

            msg_Dbg( va, "trying to decode MPEG-4 Part 10: profile %zu, level %zu", i_profile, i_level);

            switch (i_profile) {
                case PROFILE_H264_BASELINE:
                case PROFILE_H264_MAIN:
                case PROFILE_H264_HIGH:
                    break;

                default:
                {
                    msg_Dbg( va, "unsupported H264 profile %zu", i_profile);
                    return -1;
                }
            }
            break;

        default:
#ifndef NDEBUG
            msg_Err( va, "'%4.4s' is not supported", (char *)&fmt->i_codec);
#endif
            return VLC_EGENERIC;
            break;
    }

    vlc_va_sys_t *sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->vdactx = av_vda_alloc_context();
    sys->vdactx->cv_pix_fmt_type = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    sys->i_width = avctx->width;
    sys->i_height = avctx->height;

    va->sys = sys;
    va->description = (char *)"VDA";
    va->setup = Setup;
    va->get = Get;
    va->release = Release;
    va->extract = Extract;

    return VLC_SUCCESS;
}

static void Close( vlc_va_t *va, AVCodecContext *avctx )
{
    av_vda_default_free(avctx);
    (void) va;
}

static int Setup( vlc_va_t *va, AVCodecContext *avctx, vlc_fourcc_t *pi_chroma )
{

    vlc_va_sys_t *sys = va->sys;

    int i_ret = av_vda_default_init2(avctx, sys->vdactx);

    msg_Dbg(va, "Creating VDA decoder %i", i_ret);

    if (i_ret != 0)
        return VLC_EGENERIC;

    *pi_chroma = VLC_CODEC_I420;

    return VLC_SUCCESS;
}

// Never called
static int Get( vlc_va_t *va, picture_t *p_picture, uint8_t **data )
{
    VLC_UNUSED( va );
    (void) p_picture;
    (void) data;
    return VLC_SUCCESS;
}

// Never called
static void Release( void *opaque, uint8_t *data )
{
    VLC_UNUSED(opaque);
    (void) data;
}

static int Extract( vlc_va_t *va, picture_t *p_picture, uint8_t *data )
{
    vlc_va_sys_t *sys = va->sys;

    CVPixelBufferRef cv_buffer = (CVPixelBufferRef)data;

    if( !cv_buffer )
    {
        msg_Dbg( va, "Frame buffer is empty.");
        return VLC_EGENERIC;
    }
    if (!CVPixelBufferGetDataSize(cv_buffer) > 0)
    {
        msg_Dbg( va, "Empty frame buffer");
        return VLC_EGENERIC;
    }

    copy420YpCbCr8Planar( p_picture, cv_buffer, sys->i_width, sys->i_height );

    return VLC_SUCCESS;
}
