/*****************************************************************************
 * image_cvpx.c: export test for image_handler with cvpx sources
 *****************************************************************************
 * Copyright (C) 2023 Videolabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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
# include <config.h>
#endif

/* Define a builtin module for mocked parts */
#define MODULE_NAME test_misc_image_cvpx
#undef VLC_DYNAMIC_PLUGIN

#include "../../libvlc/test.h"
#include "../../../modules/codec/vt_utils.h"

#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_image.h>
#include <vlc_picture.h>
#include <vlc_plugin.h>
#include <vlc_block.h>

#include <limits.h>

const char vlc_module_name[] = MODULE_STRING;

static int OpenIntf(vlc_object_t *root)
{
    image_handler_t *ih = image_HandlerCreate(root);
    assert(ih != NULL);

    video_format_t fmt_in;
    video_format_Init(&fmt_in, VLC_CODEC_CVPX_NV12);
    fmt_in.i_width = fmt_in.i_visible_width = 800;
    fmt_in.i_height = fmt_in.i_visible_height = 600;

    CVPixelBufferPoolRef pool = cvpxpool_create(&fmt_in, 1);
    assert(pool != NULL);

    CVPixelBufferRef buffer = cvpxpool_new_cvpx(pool);
    assert(buffer != NULL);

    picture_t *picture = picture_NewFromFormat(&fmt_in);
    assert(picture != NULL);

    struct vlc_decoder_device *device = NULL;
    static const struct vlc_video_context_operations ops = {
        NULL
    };

    struct vlc_video_context *vctx =vlc_video_context_CreateCVPX(
      device, CVPX_VIDEO_CONTEXT_DEFAULT, 0, &ops);
    int ret = cvpxpic_attach(picture, buffer, vctx,
                             NULL /* TODO: check everything is released */);
    assert(ret == VLC_SUCCESS);

    video_format_t fmt_out;
    video_format_Init(&fmt_out, VLC_CODEC_PNG);
    fmt_out.i_width = fmt_out.i_visible_width = 800;
    fmt_out.i_height = fmt_out.i_visible_height = 600;

    block_t *block;

    block = image_Write(ih, picture, &fmt_in, &fmt_out);
    assert(block != NULL);
    block_Release(block);
    image_HandlerDelete(ih);

    picture_Release(picture);

    return VLC_SUCCESS;
}

/** Inject the mocked modules as a static plugin: **/
vlc_module_begin()
    set_callback(OpenIntf)
    set_capability("interface", 0)
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};


int main()
{
    test_init();

    const char * const args[] = {
        "-vvv", "--vout=dummy", "--aout=dummy", "--text-renderer=dummy",
        "--no-auto-preparse",
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(args), args);

    libvlc_add_intf(vlc, MODULE_STRING);
    libvlc_playlist_play(vlc);

    libvlc_release(vlc);

}

