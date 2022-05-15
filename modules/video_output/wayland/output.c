/**
 * @file input.c
 * @brief Wayland input events for VLC media player
 */
/*****************************************************************************
 * Copyright © 2018 Rémi Denis-Courmont
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
# include <config.h>
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <vlc_common.h>
#include <vlc_window.h>

#include "output.h"

struct output_list
{
    vlc_window_t *owner;
    struct wl_list outputs;
};

struct output_data
{
    vlc_window_t *owner;
    struct wl_output *wl_output;

    uint32_t id;
    uint32_t version;
    char *name;
    char *description;

    struct wl_list node;
};

static void output_done_cb(void *data, struct wl_output *output);

static void output_geometry_cb(void *data, struct wl_output *output,
                               int32_t x, int32_t y, int32_t w, int32_t h,
                               int32_t sp, const char *make, const char *model,
                               int32_t transform)
{
    struct output_data *od = data;
    vlc_window_t *wnd = od->owner;

    msg_Dbg(wnd, "output %"PRIu32" geometry: %"PRId32"x%"PRId32"mm"
            "+%"PRId32"+%"PRId32", subpixel %"PRId32", transform %"PRId32,
            od->id, w, h, x, y, sp, transform);

    if (od->version < WL_OUTPUT_NAME_SINCE_VERSION)
    {
        free(od->name);
        if (unlikely(asprintf(&od->name, "%"PRIu32, od->id) < 0))
            od->name = NULL;
    }

    if (od->version < WL_OUTPUT_DESCRIPTION_SINCE_VERSION)
    {
        free(od->description);
        if (unlikely(asprintf(&od->description, "%s - %s", make, model) < 0))
            od->description = NULL;
    }

    (void) output;
}

static void output_mode_cb(void *data, struct wl_output *output,
                           uint32_t flags, int32_t w, int32_t h, int32_t vr)
{
    struct output_data *od = data;
    vlc_window_t *wnd = od->owner;
    div_t d = div(vr, 1000);

    msg_Dbg(wnd, "output %"PRIu32" mode: 0x%"PRIx32" %"PRId32"x%"PRId32
            ", %d.%03d Hz", od->id, flags, w, h, d.quot, d.rem);

    if (od->version < WL_OUTPUT_DONE_SINCE_VERSION)
        output_done_cb(data, output); /* Ancient display server */

    (void) output;
}

static void output_done_cb(void *data, struct wl_output *output)
{
    struct output_data *od = data;
    vlc_window_t *wnd = od->owner;
    const char *name = od->name;
    const char *description = od->description;

    if (unlikely(description == NULL))
        description = name;
    if (likely(name != NULL))
        vlc_window_ReportOutputDevice(wnd, name, description);

    (void) output;
}

static void output_scale_cb(void *data, struct wl_output *output, int32_t f)
{
    struct output_data *od = data;
    vlc_window_t *wnd = od->owner;

    msg_Dbg(wnd, "output %"PRIu32" scale: %"PRId32, od->id, f);
    (void) output;
}

static void output_name_cb(void *data, struct wl_output *output,
                           const char *name)
{
    struct output_data *od = data;

    free(od->name);
    od->name = strdup(name);
    (void) output;
}

static void output_description_cb(void *data, struct wl_output *output,
                                  const char *description)
{
    struct output_data *od = data;

    free(od->description);
    od->description = strdup(description);
    (void) output;
}


static const struct wl_output_listener wl_output_cbs =
{
    output_geometry_cb,
    output_mode_cb,
    output_done_cb,
    output_scale_cb,
    output_name_cb,
    output_description_cb,
};

struct output_list *output_list_create(vlc_window_t *wnd)
{
    struct output_list *ol = malloc(sizeof (*ol));
    if (unlikely(ol == NULL))
        return NULL;

    ol->owner = wnd;
    wl_list_init(&ol->outputs);
    return ol;
}

struct wl_output *output_create(struct output_list *ol,
                                struct wl_registry *registry,
                                uint32_t id, uint32_t version)
{
    if (unlikely(ol == NULL))
        return NULL;

    struct output_data *od = malloc(sizeof (*od));
    if (unlikely(od == NULL))
        return NULL;

    if (version > 3)
        version = 3;

    struct wl_output *wo = wl_registry_bind(registry, id,
                                            &wl_output_interface, version);
    if (unlikely(wo == NULL))
    {
        free(od);
        return NULL;
    }

    od->wl_output = wo;
    od->owner = ol->owner;
    od->id = id;
    od->version = version;
    od->name = NULL;
    od->description = NULL;

    wl_output_add_listener(wo, &wl_output_cbs, od);
    wl_list_insert(&ol->outputs, &od->node);
    return wo;
}

void output_destroy(struct output_list *ol, struct wl_output *wo)
{
    assert(ol != NULL);
    assert(wo != NULL);

    struct output_data *od = wl_output_get_user_data(wo);

    free(od->description);

    if (od->name != NULL) {
        vlc_window_ReportOutputDevice(ol->owner, od->name, NULL);
        free(od->name);
    }

    wl_list_remove(&od->node);

    if (od->version >= WL_OUTPUT_RELEASE_SINCE_VERSION)
        wl_output_release(od->wl_output);
    else
        wl_output_destroy(od->wl_output);
    free(od);
}

struct wl_output *output_find_by_id(struct output_list *ol, uint32_t id)
{
    if (unlikely(ol == NULL))
        return NULL;

    struct wl_list *list = &ol->outputs;
    struct output_data *od;

    wl_list_for_each(od, list, node)
        if (od->id == id)
            return od->wl_output;

    return NULL;
}

struct wl_output *output_find_by_name(struct output_list *ol, const char *name)
{
    if (unlikely(ol == NULL))
        return NULL;

    struct wl_list *list = &ol->outputs;
    struct output_data *od;

    wl_list_for_each(od, list, node)
        if (strcmp(od->name, name) == 0)
            return od->wl_output;

    return NULL;

}

void output_list_destroy(struct output_list *ol)
{
    if (ol == NULL)
        return;

    struct wl_list *list = &ol->outputs;

    while (!wl_list_empty(list)) {
        struct output_data *od = container_of(list->next, struct output_data,
                                              node);
        output_destroy(ol, od->wl_output);
    }

    free(ol);
}
