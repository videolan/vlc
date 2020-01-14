/*****************************************************************************
 * player_vout.c: Player vout implementation
 *****************************************************************************
 * Copyright © 2018-2019 VLC authors and VideoLAN
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

#include <limits.h>

#include <vlc_common.h>
#include "player.h"
#include "input/resource.h"

#define vlc_player_vout_SendEvent(player, event, ...) do { \
    vlc_mutex_lock(&player->vout_listeners_lock); \
    vlc_player_vout_listener_id *listener; \
    vlc_list_foreach(listener, &player->vout_listeners, node) \
    { \
        if (listener->cbs->event) \
            listener->cbs->event(__VA_ARGS__, listener->cbs_data); \
    } \
    vlc_mutex_unlock(&player->vout_listeners_lock); \
} while(0)

vout_thread_t *
vlc_player_vout_Hold(vlc_player_t *player)
{
    vout_thread_t *vout = input_resource_HoldVout(player->resource);
    return vout ? vout : input_resource_HoldDummyVout(player->resource);
}

vout_thread_t **
vlc_player_vout_HoldAll(vlc_player_t *player, size_t *count)
{
    vout_thread_t **vouts;
    input_resource_HoldVouts(player->resource, &vouts, count);

    if (*count == 0)
    {
        vouts = vlc_alloc(1, sizeof(*vouts));
        if (vouts)
        {
            *count = 1;
            vouts[0] = input_resource_HoldDummyVout(player->resource);
        }
    }
    return vouts;
}

vlc_player_vout_listener_id *
vlc_player_vout_AddListener(vlc_player_t *player,
                            const struct vlc_player_vout_cbs *cbs,
                            void *cbs_data)
{
    assert(cbs);

    vlc_player_vout_listener_id *listener = malloc(sizeof(*listener));
    if (!listener)
        return NULL;

    listener->cbs = cbs;
    listener->cbs_data = cbs_data;

    vlc_mutex_lock(&player->vout_listeners_lock);
    vlc_list_append(&listener->node, &player->vout_listeners);
    vlc_mutex_unlock(&player->vout_listeners_lock);

    return listener;
}

void
vlc_player_vout_RemoveListener(vlc_player_t *player,
                               vlc_player_vout_listener_id *id)
{
    assert(id);

    vlc_mutex_lock(&player->vout_listeners_lock);
    vlc_list_remove(&id->node);
    vlc_mutex_unlock(&player->vout_listeners_lock);
    free(id);
}

bool
vlc_player_vout_IsFullscreen(vlc_player_t *player)
{
    vout_thread_t *vout = vlc_player_vout_Hold(player);
    bool fs =  var_GetBool(vout, "fullscreen");
    vout_Release(vout);
    return fs;
}

static int
vlc_player_VoutCallback(vlc_object_t *this, const char *var,
                        vlc_value_t oldval, vlc_value_t newval, void *data)
{
    vlc_player_t *player = data;

    if (strcmp(var, "fullscreen") == 0)
    {
        if (oldval.b_bool != newval.b_bool )
            vlc_player_vout_SendEvent(player, on_fullscreen_changed,
                                      (vout_thread_t *)this, newval.b_bool);
    }
    else if (strcmp(var, "video-wallpaper") == 0)
    {
        if (oldval.b_bool != newval.b_bool )
            vlc_player_vout_SendEvent(player, on_wallpaper_mode_changed,
                                      (vout_thread_t *)this, newval.b_bool);
    }
    else
        vlc_assert_unreachable();

    return VLC_SUCCESS;
}

static const char osd_vars[][sizeof("secondary-sub-margin")] = {
    "aspect-ratio", "autoscale", "crop", "crop-bottom",
    "crop-top", "crop-left", "crop-right", "deinterlace",
    "deinterlace-mode", "sub-margin", "secondary-sub-margin", "zoom"
};

void
vlc_player_vout_AddCallbacks(vlc_player_t *player, vout_thread_t *vout)
{
    var_AddCallback(vout, "fullscreen", vlc_player_VoutCallback, player);
    var_AddCallback(vout, "video-wallpaper", vlc_player_VoutCallback, player);

    for (size_t i = 0; i < ARRAY_SIZE(osd_vars); ++i)
        var_AddCallback(vout, osd_vars[i], vlc_player_vout_OSDCallback, player);
}

void
vlc_player_vout_DelCallbacks(vlc_player_t *player, vout_thread_t *vout)
{
    var_DelCallback(vout, "fullscreen", vlc_player_VoutCallback, player);
    var_DelCallback(vout, "video-wallpaper", vlc_player_VoutCallback, player);

    for (size_t i = 0; i < ARRAY_SIZE(osd_vars); ++i)
        var_DelCallback(vout, osd_vars[i], vlc_player_vout_OSDCallback, player);
}

static void
vlc_player_vout_SetVar(vlc_player_t *player, const char *name, int type,
                       vlc_value_t val)
{
    vout_thread_t *vout = vlc_player_vout_Hold(player);
    var_SetChecked(vout, name, type, val);
    vout_Release(vout);
}


static void
vlc_player_vout_TriggerOption(vlc_player_t *player, const char *option)
{
    /* Don't use vlc_player_vout_Hold() since there is nothing to trigger if it
     * returns a dummy vout */
    vout_thread_t *vout = input_resource_HoldVout(player->resource);
    var_TriggerCallback(vout, option);
    vout_Release(vout);
}


void
vlc_player_vout_SetFullscreen(vlc_player_t *player, bool enabled)
{
    vlc_player_vout_SetVar(player, "fullscreen", VLC_VAR_BOOL,
                           (vlc_value_t) { .b_bool = enabled });
    vlc_player_vout_SendEvent(player, on_fullscreen_changed, NULL, enabled);
}

bool
vlc_player_vout_IsWallpaperModeEnabled(vlc_player_t *player)
{
    vout_thread_t *vout = vlc_player_vout_Hold(player);
    bool wm =  var_GetBool(vout, "video-wallpaper");
    vout_Release(vout);
    return wm;
}

void
vlc_player_vout_SetWallpaperModeEnabled(vlc_player_t *player, bool enabled)
{
    vlc_player_vout_SetVar(player, "video-wallpaper", VLC_VAR_BOOL,
                           (vlc_value_t) { .b_bool = enabled });
    vlc_player_vout_SendEvent(player, on_wallpaper_mode_changed, NULL, enabled);
}

void
vlc_player_vout_Snapshot(vlc_player_t *player)
{
    vlc_player_vout_TriggerOption(player, "video-snapshot");
}
