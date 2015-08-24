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

#define MC_API_ERROR (-1)
#define MC_API_INFO_TRYAGAIN (-11)
#define MC_API_INFO_OUTPUT_FORMAT_CHANGED (-12)
#define MC_API_INFO_OUTPUT_BUFFERS_CHANGED (-13)

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
        union
        {
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
            } video;
            struct
            {
                int channel_count;
                int channel_mask;
                int sample_rate;
            } audio;
        } conf;
    } u;
};

union mc_api_args
{
    struct
    {
        AWindowHandler *p_awh;
        int i_width;
        int i_height;
        int i_angle;
    } video;
    struct
    {
        int i_sample_rate;
        int i_channel_count;
    } audio;
};

struct mc_api
{
    vlc_object_t *p_obj;

    mc_api_sys *p_sys;

    bool b_started;
    bool b_video;
    bool b_direct_rendering;
    bool b_support_interlaced;

    void (*clean)(mc_api *);
    int (*start)(mc_api *, const char *psz_name, const char *psz_mime,
                 union mc_api_args *p_args);
    int (*stop)(mc_api *);
    int (*flush)(mc_api *);

    /* The Dequeue functions return:
     * - The index of the input or output buffer if >= 0,
     * - MC_API_INFO_TRYAGAIN if no buffers where dequeued during i_timeout,
     * - MC_API_INFO_OUTPUT_FORMAT_CHANGED if output format changed
     * - MC_API_INFO_OUTPUT_BUFFERS_CHANGED if buffers changed
     * - MC_API_ERROR in case of error. */
    int (*dequeue_in)(mc_api *, mtime_t i_timeout);
    int (*dequeue_out)(mc_api *, mtime_t i_timeout);

    /* i_index is the index returned by dequeue_in and should be >= 0
     * Returns 0 if buffer is successfully queued, or MC_API_ERROR */
    int (*queue_in)(mc_api *, int i_index, const void *p_buf, size_t i_size,
                    mtime_t i_ts, bool b_config);

    /* i_index is the index returned by dequeue_out and should be >= 0,
     * MC_API_INFO_OUTPUT_FORMAT_CHANGED, or MC_API_INFO_OUTPUT_BUFFERS_CHANGED.
     * Returns 1 if p_out if valid, 0 if p_out is unchanged or MC_API_ERROR */
    int (*get_out)(mc_api *, int i_index, mc_api_out *p_out);

    /* i_index is the index returned by dequeue_out and should be >= 0 */
    int (*release_out)(mc_api *, int i_index, bool b_render);
};

#endif
