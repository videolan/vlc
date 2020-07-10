/*****************************************************************************
 * transcode.c: test for transcoding pipeline
 *****************************************************************************
 * Copyright (C) 2020 VideoLabs
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
# include "config.h"
#endif

/* Define a builtin module for mocked parts */
#define MODULE_NAME test_transcode_mock
#define MODULE_STRING "test_transcode_mock"
#undef __PLUGIN__

#include "../../libvlc/test.h"
#include <vlc_common.h>
#include <vlc_plugin.h>

static int OpenAccess()
{

    return VLC_SUCCESS;
}

static int OpenDecoder()
{

    return VLC_SUCCESS;
}

static int OpenFilter()
{
    return VLC_SUCCESS;
}

static int OpenEncoder()
{

    return VLC_SUCCESS;
}

/* Helper typedef for vlc_static_modules */
typedef int (*vlc_plugin_cb)(vlc_set_cb, void*);

/**
 * Inject the mocked modules as a static plugin:
 *  - access for triggering the correct decoder
 *  - decoder for generating video format and context
 *  - filter for generating video format and context
 *  - encoder to check the previous video format and context
 **/
vlc_module_begin()
    set_callback(OpenAccess)
    set_capability("access", 0)

    add_submodule()
        set_callback(OpenDecoder)
        set_capability("video decoder", 0)

    add_submodule()
        set_callback(OpenFilter)
        set_capability("video filter", 0)

    add_submodule()
        set_callback(OpenEncoder)
        set_capability("encoder", 0)
vlc_module_end()

__attribute__((visibility("default")))
vlc_plugin_cb vlc_static_modules[] = { vlc_entry__test_transcode_mock, NULL };

static void transcode(int argc, char **argv)
{
    libvlc_instance_t *vlc = libvlc_new(argc, argv);

    libvlc_media_t *md = libvlc_media_new_location(vlc, #MODULE_NAME "://");
    assert(md != NULL);

    /* setup stream output */
    libvlc_media_add_option(md, "--sout=#transcode{venc=" #MODULE_NAME "}:dummy");

    libvlc_media_player_t *mp = libvlc_media_player_new(vlc);
    assert(mp != NULL);

    libvlc_media_player_set_media(mp, md);

    // TODO launch, wait events

    // TODO:
    //libvlc_media_player_stop(mp);
    libvlc_media_player_release(mp);
    libvlc_release(vlc);
}

int main( void )
{
    test_init();
    transcode(0, NULL);

    return 0;
}
