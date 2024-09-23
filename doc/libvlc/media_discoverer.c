/*****************************************************************************
 * media_discoverer.c:  libvlc media discoverer API sample usage
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#include <stdio.h>
#include <stdlib.h>

#include <vlc/vlc.h>

static const char *
category_to_string(libvlc_media_discoverer_category_t cat)
{
    switch (cat)
    {
        case libvlc_media_discoverer_devices:   return "devices";
        case libvlc_media_discoverer_lan:       return "lan";
        case libvlc_media_discoverer_podcasts:  return "podcasts";
        case libvlc_media_discoverer_localdirs: return "localdirs";
        default: abort(); /* assert/unreachable */
    }
}

static void
print_usage(const char *name, libvlc_instance_t *libvlc)
{
    /* Print usage and list all available media discoverer plugins */
    fprintf(stderr, "Usage: %s <discoverer_name>\n\n"
            "List of discoverers by category:\n", name);

    static const libvlc_media_discoverer_category_t cats[] = {
        libvlc_media_discoverer_devices,
        libvlc_media_discoverer_lan,
        libvlc_media_discoverer_podcasts,
        libvlc_media_discoverer_localdirs
    };

    for (size_t i = 0; i < sizeof(cats) / sizeof(cats[0]); ++i)
    {
        libvlc_media_discoverer_category_t cat = cats[i];
        libvlc_media_discoverer_description_t **desclist;

        fprintf(stderr, "[%s]:\n", category_to_string(cat));
        size_t count = libvlc_media_discoverer_list_get(libvlc, cat, &desclist);
        for (size_t j = 0; j < count; j++)
        {
            libvlc_media_discoverer_description_t *desc = desclist[j];
            fprintf(stderr, "\t\"%s\" (%s)\n", desc->psz_name, desc->psz_longname);
        }

        libvlc_media_discoverer_list_release(desclist, count);
    }
}

static void
on_media_added(void *opaque, libvlc_media_t *parent, libvlc_media_t *media)
{
    (void) opaque;
    char *parentname = parent == NULL ? NULL
                     : libvlc_media_get_meta(parent, libvlc_meta_Title);
    char *mrl = libvlc_media_get_mrl(media);
    char *name = libvlc_media_get_meta(media, libvlc_meta_Title);
    printf("media_added: \"%s\"\n  parent: \"%s\"\n  url: \"%s\"\n\n",
           name != NULL ? name : "<nil>",
           parentname != NULL ? parentname : "<nil>",
           mrl != NULL ? mrl : "<nil>");
    free(mrl);
    free(name);
    free(parentname);
}

static void
on_media_removed(void *opaque, libvlc_media_t *media)
{
    (void) opaque;
    char *mrl = libvlc_media_get_mrl(media);
    char *name = libvlc_media_get_meta(media, libvlc_meta_Title);
    printf("media_removed: \"%s\"\n  url: \"%s\"\n\n",
           name != NULL ? name : "<nil>",
           mrl != NULL ? mrl : "<nil>");
    free(mrl);
    free(name);
}

int
main(int argc, const char **argv)
{
    static const char* const args[] = {
        "--verbose=1",
    };
    libvlc_instance_t *libvlc = libvlc_new(sizeof args / sizeof *args, args);
    if (libvlc == NULL)
        return EXIT_FAILURE;

    if (argc < 2)
    {
        print_usage(argv[0], libvlc);
        libvlc_release(libvlc);
        return EXIT_FAILURE;
    }

    /* Setup discoverer callbacks */
    static const struct libvlc_media_discoverer_cbs cbs = {
        .version = 0,
        .on_media_added = on_media_added,
        .on_media_removed = on_media_removed,
    };

    libvlc_media_discoverer_t *discoverer =
        libvlc_media_discoverer_new(libvlc, argv[1],
                                    &cbs, NULL);

    int ret = discoverer == NULL ? -1 : libvlc_media_discoverer_start(discoverer);
    if (ret != 0)
    {
        fprintf(stderr, "Error: unknown discoverer: \"%s\"\n", argv[1]);
        print_usage(argv[0], libvlc);

        if (discoverer != NULL)
            libvlc_media_discoverer_destroy(discoverer);
        libvlc_release(libvlc);
        return EXIT_FAILURE;
    }

    printf("Discoverer \"%s\" running, press ENTER to stop\n", argv[1]);
    getchar();

    libvlc_media_discoverer_stop(discoverer);

    libvlc_media_discoverer_destroy(discoverer);
    libvlc_release(libvlc);
    return EXIT_SUCCESS;
}
