/*****************************************************************************
 * player_aout.c: Player aout implementation
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
#include <vlc_decoder.h>
#include "player.h"
#include "input/resource.h"

#define vlc_player_aout_SendEvent(player, event, ...) do { \
    vlc_mutex_lock(&player->aout_listeners_lock); \
    vlc_player_aout_listener_id *listener; \
    vlc_list_foreach(listener, &player->aout_listeners, node) \
    { \
        if (listener->cbs->event) \
            listener->cbs->event(__VA_ARGS__, listener->cbs_data); \
    } \
    vlc_mutex_unlock(&player->aout_listeners_lock); \
} while(0)

audio_output_t *
vlc_player_aout_Hold(vlc_player_t *player)
{
    return input_resource_HoldAout(player->resource);
}

vlc_player_aout_listener_id *
vlc_player_aout_AddListener(vlc_player_t *player,
                            const struct vlc_player_aout_cbs *cbs,
                            void *cbs_data)
{
    assert(cbs);

    vlc_player_aout_listener_id *listener = malloc(sizeof(*listener));
    if (!listener)
        return NULL;

    listener->cbs = cbs;
    listener->cbs_data = cbs_data;

    vlc_mutex_lock(&player->aout_listeners_lock);
    vlc_list_append(&listener->node, &player->aout_listeners);
    vlc_mutex_unlock(&player->aout_listeners_lock);

    return listener;
}

void
vlc_player_aout_RemoveListener(vlc_player_t *player,
                               vlc_player_aout_listener_id *id)
{
    assert(id);

    vlc_mutex_lock(&player->aout_listeners_lock);
    vlc_list_remove(&id->node);
    vlc_mutex_unlock(&player->aout_listeners_lock);
    free(id);
}

static int
vlc_player_AoutCallback(vlc_object_t *this, const char *var,
                        vlc_value_t oldval, vlc_value_t newval, void *data)
{
    vlc_player_t *player = data;

    if (strcmp(var, "volume") == 0)
    {
        if (oldval.f_float != newval.f_float)
        {
            vlc_player_aout_SendEvent(player, on_volume_changed,
                                      (audio_output_t *)this, newval.f_float);
            vlc_player_osd_Volume(player, false);
        }
    }
    else if (strcmp(var, "mute") == 0)
    {
        if (oldval.b_bool != newval.b_bool)
        {
            vlc_player_aout_SendEvent(player, on_mute_changed,
                                      (audio_output_t *)this, newval.b_bool);
            vlc_player_osd_Volume(player, true);
        }
    }
    else if (strcmp(var, "device") == 0)
    {
        const char *old = oldval.psz_string;
        const char *new = newval.psz_string;
        /* support NULL values for string comparison */
        if (old != new && (!old || !new || strcmp(old, new)))
            vlc_player_aout_SendEvent(player, on_device_changed,
                                      (audio_output_t *)this, newval.psz_string);
    }
    else
        vlc_assert_unreachable();

    return VLC_SUCCESS;
    (void) this;
}

float
vlc_player_aout_GetVolume(vlc_player_t *player)
{
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (!aout)
        return -1.f;
    float vol = aout_VolumeGet(aout);
    aout_Release(aout);

    return vol;
}

int
vlc_player_aout_SetVolume(vlc_player_t *player, float volume)
{
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (!aout)
        return -1;
    int ret = aout_VolumeSet(aout, volume);
    aout_Release(aout);

    return ret;
}

int
vlc_player_aout_IncrementVolume(vlc_player_t *player, int steps, float *result)
{
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (!aout)
        return -1;
    int ret = aout_VolumeUpdate(aout, steps, result);
    aout_Release(aout);

    return ret;
}

int
vlc_player_aout_IsMuted(vlc_player_t *player)
{
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (!aout)
        return -1;
    int ret = aout_MuteGet(aout);
    aout_Release(aout);

    return ret;
}

int
vlc_player_aout_Mute(vlc_player_t *player, bool mute)
{
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (!aout)
        return -1;
    int ret = aout_MuteSet (aout, mute);
    aout_Release(aout);

    return ret;
}

int
vlc_player_aout_EnableFilter(vlc_player_t *player, const char *name, bool add)
{
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (!aout)
        return -1;
    aout_EnableFilter(aout, name, add);
    aout_Release(aout);

    return 0;
}


static void
vlc_player_aout_AddCallbacks(vlc_player_t *player)
{
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (!aout)
        return;

    var_AddCallback(aout, "volume", vlc_player_AoutCallback, player);
    var_AddCallback(aout, "mute", vlc_player_AoutCallback, player);
    var_AddCallback(aout, "device", vlc_player_AoutCallback, player);

    aout_Release(aout);
}

static void
vlc_player_aout_DelCallbacks(vlc_player_t *player)
{
    audio_output_t *aout = vlc_player_aout_Hold(player);
    if (!aout)
        return;

    var_DelCallback(aout, "volume", vlc_player_AoutCallback, player);
    var_DelCallback(aout, "mute", vlc_player_AoutCallback, player);
    var_DelCallback(aout, "device", vlc_player_AoutCallback, player);

    aout_Release(aout);
}

audio_output_t *
vlc_player_aout_Init(vlc_player_t *player)
{
    audio_output_t *aout = input_resource_GetAout(player->resource);
    if (aout != NULL)
    {
        vlc_player_aout_AddCallbacks(player);
        input_resource_PutAout(player->resource, aout);
    }
    return aout;
}

void
vlc_player_aout_Deinit(vlc_player_t *player)
{
    vlc_player_aout_DelCallbacks(player);
}

void
vlc_player_aout_Reset(vlc_player_t *player)
{
    vlc_player_aout_Deinit(player);

    input_resource_ResetAout(player->resource);

    vlc_player_aout_Init(player);
}

