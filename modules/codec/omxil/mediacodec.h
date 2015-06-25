/*****************************************************************************
 * mediacodec.h: Video decoder module using the Android MediaCodec API
 *****************************************************************************
 * Copyright © 2015 VLC authors and VideoLAN, VideoLabs
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

#ifndef VLC_MEDIACODEC_H
#define VLC_MEDIACODEC_H

#include <vlc_common.h>
#include "../../video_output/android/utils.h"

typedef struct mc_api mc_api;
typedef struct mc_api_sys mc_api_sys;
typedef struct mc_api_out mc_api_out;

typedef int (*pf_MediaCodecApi_init)(mc_api*);

char* MediaCodec_GetName(vlc_object_t *p_obj, const char *psz_mime,
                         size_t h264_profile);
int MediaCodecJni_Init(mc_api*);
int MediaCodecNdk_Init(mc_api*);

struct mc_api_out
{
    enum {
        MC_OUT_TYPE_BUF,
        MC_OUT_TYPE_CONF,
    } type;
    union
    {
        struct
        {
            int i_index;
            mtime_t i_ts;
            const void *p_ptr;
            int i_size;
        } buf;
        struct
        {
            int width, height;
            int stride;
            int slice_height;
            int pixel_format;
            int crop_left;
            int crop_top;
            int crop_right;
            int crop_bottom;
        } conf;
    } u;
};

struct mc_api
{
    vlc_object_t *p_obj;

    mc_api_sys *p_sys;

    bool b_started;
    bool b_direct_rendering;
    bool b_support_interlaced;

    void (*clean)(mc_api *);
    int (*start)(mc_api *, AWindowHandler *p_awh, const char *psz_name,
                 const char *psz_mime, int i_width, int i_height, int i_angle);
    int (*stop)(mc_api *);
    int (*flush)(mc_api *);
    int (*put_in)(mc_api *, const void *p_buf, size_t i_size,
                  mtime_t i_ts, bool b_config, mtime_t i_timeout);
    int (*get_out)(mc_api *, mc_api_out *p_out, mtime_t i_timeout);
    int (*release_out)(mc_api *, int i_index, bool b_render);
};

#endif
