/*****************************************************************************
 * test/src/media_source.c
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
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

#include "../../libvlc/test.h"
#include "../lib/libvlc_internal.h"

#include <assert.h>
#include <vlc_common.h>
#include <vlc_media_source.h>
#include <vlc_vector.h>
#include <vlc/vlc.h>

static const char *libvlc_argv[] = {
    "-v",
    "--ignore-config",
    "-Idummy",
    "--no-media-library",
};

static void
test_list(void)
{
    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(libvlc_argv), libvlc_argv);
    assert(vlc);

    vlc_media_source_provider_t *provider =
            vlc_media_source_provider_Get(vlc->p_libvlc_int);
    assert(provider);

    vlc_media_source_meta_list_t *list =
            vlc_media_source_provider_List(provider, 0);
    assert(list);

    size_t count = vlc_media_source_meta_list_Count(list);
    assert(count);
    for (size_t i = 0; i < count; ++i)
    {
        struct vlc_media_source_meta *meta =
                vlc_media_source_meta_list_Get(list, i);
        assert(meta);
        assert(meta->name);
        assert(meta->longname);
        assert(meta->category); /* there is no category 0 */
    }

    vlc_media_source_meta_list_Delete(list);
    libvlc_release(vlc);
}

static void
test_list_filtered_by_category(void)
{
    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(libvlc_argv), libvlc_argv);
    assert(vlc);

    vlc_media_source_provider_t *provider =
            vlc_media_source_provider_Get(vlc->p_libvlc_int);
    assert(provider);

    vlc_media_source_meta_list_t *list =
            vlc_media_source_provider_List(provider, SD_CAT_LAN);
    assert(list);

    size_t count = vlc_media_source_meta_list_Count(list);
    assert(count);
    for (size_t i = 0; i < count; ++i)
    {
        struct vlc_media_source_meta *meta =
                vlc_media_source_meta_list_Get(list, i);
        assert(meta);
        assert(meta->name);
        assert(meta->longname);
        assert(meta->category == SD_CAT_LAN);
    }

    vlc_media_source_meta_list_Delete(list);
    libvlc_release(vlc);
}

int main(void)
{
    test_init();

    test_list();
    test_list_filtered_by_category();
    return 0;
}
