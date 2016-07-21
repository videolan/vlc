/*****************************************************************************
 * renderer_discoverer.c - libvlc smoke test
 *****************************************************************************
 * Copyright © 2016 VLC authors, and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "test.h"

#include <string.h>

static void
item_event(const libvlc_renderer_item_t *p_item, const char *psz_event)
{
    log("item %s: name: '%s', type: '%s', flags: 0x%X\n", psz_event,
        libvlc_renderer_item_name(p_item), libvlc_renderer_item_type(p_item),
        libvlc_renderer_item_flags(p_item));
}

static void
renderer_discoverer_item_added(const struct libvlc_event_t *p_ev, void *p_data)
{
    (void) p_data;
    item_event(p_ev->u.renderer_discoverer_item_added.item, "added");
}

static void
renderer_discoverer_item_deleted(const struct libvlc_event_t *p_ev, void *p_data)
{
    (void) p_data;
    item_event(p_ev->u.renderer_discoverer_item_deleted.item, "deleted");
}

static void
test_discoverer(libvlc_instance_t *p_vlc, const char *psz_name)
{
    log("creating and starting discoverer %s\n", psz_name);

    libvlc_renderer_discoverer_t *p_rd =
        libvlc_renderer_discoverer_new(p_vlc, psz_name);
    assert(p_rd != NULL);

    libvlc_event_manager_t *p_evm = libvlc_renderer_discoverer_event_manager(p_rd);
    assert(p_evm);

    int i_ret;
    i_ret = libvlc_event_attach(p_evm, libvlc_RendererDiscovererItemAdded,
                                renderer_discoverer_item_added, NULL);
    assert(i_ret == 0);
    i_ret = libvlc_event_attach(p_evm, libvlc_RendererDiscovererItemDeleted,
                                renderer_discoverer_item_deleted, NULL);
    assert(i_ret == 0);

    if (libvlc_renderer_discoverer_start(p_rd) == -1)
    {
        log("warn: could not start md (not critical)\n");
    }
    else
    {
        log("Press any keys to stop\n");
        getchar();
        libvlc_renderer_discoverer_stop(p_rd);
    }

    libvlc_renderer_discoverer_release(p_rd);
}

int
main(int i_argc, char *ppsz_argv[])
{
    test_init();

    char *psz_test_name = i_argc > 1 ? ppsz_argv[1] : NULL;

    libvlc_instance_t *p_vlc = libvlc_new(test_defaults_nargs,
                                          test_defaults_args);
    assert(p_vlc != NULL);

    if (psz_test_name != NULL)
    {
        /* Test a specific service discovery from command line */
        alarm(0);
        test_discoverer(p_vlc, psz_test_name);
        libvlc_release(p_vlc);
        return 0;
    }

    log("== getting the list of renderer_discoverer  ==\n");

    libvlc_rd_description_t **pp_services;
    ssize_t i_count =
        libvlc_renderer_discoverer_list_get(p_vlc, &pp_services);
    if (i_count <= 0)
    {
        log("warn: no discoverers (not critical)\n");
        goto end;
    }
    assert(pp_services != NULL);

    for (unsigned int i = 0; i < i_count; ++i)
    {
        libvlc_rd_description_t *p_service = pp_services[i];

        log("= discoverer: name: '%s', longname: '%s' =\n",
            p_service->psz_name, p_service->psz_longname);
    }

    libvlc_renderer_discoverer_list_release(pp_services, i_count);

end:
    libvlc_release(p_vlc);

    return 0;
}
