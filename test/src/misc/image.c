/*****************************************************************************
 * image.c: test for the image_handler code from vlc_image.h
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
#define MODULE_NAME test_misc_image
#define MODULE_STRING "test_misc_image"
#undef VLC_DYNAMIC_PLUGIN
const char vlc_module_name[] = MODULE_STRING;

#include "../../libvlc/test.h"

#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_image.h>
#include <vlc_picture.h>
#include <vlc_plugin.h>
#include <vlc_block.h>
#include <vlc_codec.h>
#include <vlc_filter.h>

#include <limits.h>

static atomic_bool encoder_opened = false;

static int OpenIntf(vlc_object_t *root)
{
    image_handler_t *ih = image_HandlerCreate(root);
    assert(ih != NULL);

    video_format_t fmt_in;
    video_format_Init(&fmt_in, VLC_CODEC_RGBA);
    fmt_in.i_width = fmt_in.i_visible_width = 800;
    fmt_in.i_height = fmt_in.i_visible_height = 600;

    video_format_t fmt_out;
    video_format_Init(&fmt_out, VLC_CODEC_PNG);
    fmt_out.i_width = fmt_out.i_visible_width = 800;
    fmt_out.i_height = fmt_out.i_visible_height = 600;

    picture_t *picture = picture_NewFromFormat(&fmt_in);
    assert(picture != NULL);

    block_t *block;

    block = image_Write(ih, picture, &fmt_in, &fmt_out);
    assert(block != NULL);
    block_Release(block);
    picture_Release(picture);
    assert(atomic_load(&encoder_opened));
    atomic_store(&encoder_opened, false);

    picture = picture_NewFromFormat(&fmt_in);
    fmt_out.i_width = fmt_out.i_visible_width = 400;
    fmt_out.i_height = fmt_out.i_visible_height = 300;
    block = image_Write(ih, picture, &fmt_in, &fmt_out);
    assert(block != NULL);
    block_Release(block);
    picture_Release(picture);
    assert(atomic_load(&encoder_opened));

    image_HandlerDelete(ih);

    return VLC_SUCCESS;
}

static block_t * EncodeVideo(encoder_t *encoder, picture_t *pic)
{
    (void)encoder; (void)pic;

    /* Dummy encoder */
    return block_Alloc(1);
}

static int OpenEncoder(vlc_object_t *obj)
{
    encoder_t *encoder = (encoder_t*)obj;
    static const struct vlc_encoder_operations ops =
    {
        .encode_video = EncodeVideo
    };
    encoder->ops = &ops;
    atomic_store(&encoder_opened, true);
    return VLC_SUCCESS;
}

static picture_t *ConvertVideo(filter_t *filter, picture_t *pic)
{
    picture_Release(pic);
    return picture_NewFromFormat(&filter->fmt_out.video);
}

static int OpenConverter(vlc_object_t *obj)
{
    filter_t *filter = (filter_t*)obj;

    static const struct vlc_filter_operations ops =
    {
        .filter_video = ConvertVideo,
    };
    filter->ops = &ops;
    return VLC_SUCCESS;
}

/** Inject the mocked modules as a static plugin: **/
vlc_module_begin()
    set_callback(OpenEncoder)
    set_capability("video encoder", INT_MAX)

    add_submodule()
        set_callback(OpenConverter)
        set_capability("video converter", INT_MAX)

    add_submodule()
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
