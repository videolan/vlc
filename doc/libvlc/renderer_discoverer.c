/*****************************************************************************
 * renderer_discoverer.c:  libvlc renderer discoverer API sample usage
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

static void
print_usage(const char *name, libvlc_instance_t *libvlc)
{
    /* Print usage and list all available renderer discoverer plugins */
    fprintf(stderr, "Usage: %s <discoverer_name>\n\n"
            "List of discoverer:\n", name);


    libvlc_rd_description_t **desclist;

    size_t count = libvlc_renderer_discoverer_list_get(libvlc, &desclist);
    for (size_t j = 0; j < count; j++)
    {
        libvlc_rd_description_t *desc = desclist[j];
        fprintf(stderr, "\t\"%s\" (%s)\n", desc->psz_name, desc->psz_longname);
    }

    libvlc_renderer_discoverer_list_release(desclist, count);
}

static void
on_item_added(void *opaque, libvlc_renderer_item_t *item)
{
    (void) opaque;
    printf("item_added: \"%s\"\n  type: \"%s\"\n\n",
           libvlc_renderer_item_name(item),
           libvlc_renderer_item_type(item));
}

static void
on_item_removed(void *opaque, libvlc_renderer_item_t *item)
{
    (void) opaque;
    printf("item_removed: \"%s\"\n\n",
           libvlc_renderer_item_name(item));
}

int
main(int argc, const char **argv)
{
    static const char* const vlcargs[] = {
        "--verbose=1",
    };
    libvlc_instance_t *libvlc = libvlc_new(sizeof vlcargs / sizeof *vlcargs,
                                           vlcargs);
    if (libvlc == NULL)
        return EXIT_FAILURE;

    if (argc < 2)
    {
        print_usage(argv[0], libvlc);
        libvlc_release(libvlc);
        return EXIT_FAILURE;
    }

    /* Setup discoverer callbacks */
    static const struct libvlc_renderer_discoverer_cbs cbs = {
        .version = 0,
        .on_item_added = on_item_added,
        .on_item_removed = on_item_removed,
    };

    libvlc_renderer_discoverer_t *discoverer =
        libvlc_renderer_discoverer_new(libvlc, argv[1],
                                       &cbs, NULL);

    int ret = discoverer == NULL ? -1 : libvlc_renderer_discoverer_start(discoverer);
    if (ret != 0)
    {
        fprintf(stderr, "Error: unknown discoverer: \"%s\"\n", argv[1]);
        print_usage(argv[0], libvlc);

        if (discoverer != NULL)
            libvlc_renderer_discoverer_destroy(discoverer);
        libvlc_release(libvlc);
        return EXIT_FAILURE;
    }

    printf("Discoverer \"%s\" running, press ENTER to stop\n", argv[1]);
    getchar();

    libvlc_renderer_discoverer_stop(discoverer);

    libvlc_renderer_discoverer_destroy(discoverer);
    libvlc_release(libvlc);
    return EXIT_SUCCESS;
}
