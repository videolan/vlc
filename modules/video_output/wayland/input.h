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

#include <stdint.h>

struct vout_window_t;
struct wl_registry;
struct wl_surface;
struct wl_list;

int seat_create(struct vout_window_t *wnd, struct wl_registry *,
                uint32_t name, uint32_t version, struct wl_list *list);
int seat_destroy_one(struct wl_list *list, uint32_t name);
void seat_destroy_all(struct wl_list *list);

int seat_next_timeout(const struct wl_list *list);
void seat_timeout(struct wl_list *list);

struct wl_surface *window_get_cursor(vout_window_t *wnd, int32_t *hotspot_x,
                                     int32_t *hotspot_y);
