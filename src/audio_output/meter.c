/*****************************************************************************
 * meter.c : audio meter
 *****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_aout.h>
#include "aout_internal.h"

struct vlc_audio_meter_plugin
{
    char *name;
    config_chain_t *cfg;
    filter_t *filter;
    vlc_tick_t last_date;

    struct vlc_audio_meter_plugin_owner owner;

    struct vlc_list node;
};

void
(vlc_audio_meter_Init)(struct vlc_audio_meter *meter, vlc_object_t *obj)
{
    vlc_mutex_init(&meter->lock);
    meter->parent = obj;
    meter->fmt = NULL;
    vlc_list_init(&meter->plugins);
}

void
vlc_audio_meter_Destroy(struct vlc_audio_meter *meter)
{
    vlc_audio_meter_plugin *plugin;
    vlc_list_foreach(plugin, &meter->plugins, node)
        vlc_audio_meter_RemovePlugin(meter, plugin);
}

static void
vlc_audio_meter_OnLoudnessChanged(filter_t *filter,
                             const struct vlc_audio_loudness *loudness)
{
    vlc_audio_meter_plugin *plugin = filter->owner.sys;

    if (plugin->owner.cbs->on_loudness != NULL)
        plugin->owner.cbs->on_loudness(plugin->last_date, loudness, plugin->owner.sys);
}

static filter_t *
vlc_audio_meter_CreatePluginFilter(struct vlc_audio_meter *meter, vlc_audio_meter_plugin *plugin)
{
    static const struct filter_audio_callbacks audio_cbs = {
        .meter_loudness = { .on_changed = vlc_audio_meter_OnLoudnessChanged }
    };

    const filter_owner_t owner = {
        .audio = &audio_cbs,
        .sys = plugin,
    };

    return aout_filter_Create(meter->parent, &owner, "audio meter", plugin->name,
                              meter->fmt, meter->fmt, plugin->cfg, true);
}

vlc_audio_meter_plugin *
vlc_audio_meter_AddPlugin(struct vlc_audio_meter *meter, const char *chain,
                          const struct vlc_audio_meter_plugin_owner *owner)
{
    assert(owner != NULL && owner->cbs != NULL);

    vlc_audio_meter_plugin *plugin = malloc(sizeof(*plugin));
    if (plugin == NULL)
        return NULL;
    plugin->owner = *owner;
    plugin->last_date = VLC_TICK_INVALID;
    plugin->name = NULL;
    plugin->cfg = NULL;
    plugin->filter = NULL;

    free(config_ChainCreate(&plugin->name, &plugin->cfg, chain));
    if (plugin->name == NULL)
        goto error;

    if (meter->fmt != NULL)
    {
        plugin->filter = vlc_audio_meter_CreatePluginFilter(meter, plugin);
        if (plugin->filter == NULL)
            goto error;

        assert(plugin->filter->ops->drain_audio == NULL); /* Not supported */
    }

    vlc_mutex_lock(&meter->lock);
    vlc_list_append(&plugin->node, &meter->plugins);
    vlc_mutex_unlock(&meter->lock);

    return plugin;

error:
    free(plugin->name);
    if (plugin->cfg != NULL)
        config_ChainDestroy(plugin->cfg);
    free(plugin);
    return NULL;
}

void
vlc_audio_meter_RemovePlugin(struct vlc_audio_meter *meter, vlc_audio_meter_plugin *plugin)
{
    vlc_mutex_lock(&meter->lock);

    if (plugin->filter != NULL)
    {
        filter_Close(plugin->filter);
        module_unneed(plugin->filter, plugin->filter->p_module);
        vlc_object_delete(plugin->filter);
    }

    if (plugin->cfg != NULL)
        config_ChainDestroy(plugin->cfg);
    free(plugin->name);

    vlc_list_remove(&plugin->node);
    free(plugin);

    vlc_mutex_unlock(&meter->lock);
}

int
vlc_audio_meter_Reset(struct vlc_audio_meter *meter, const audio_sample_format_t *fmt)
{
    int ret = VLC_SUCCESS;

    meter->fmt = fmt;

    vlc_mutex_lock(&meter->lock);

    /* Reload every plugins using the new fmt */
    vlc_audio_meter_plugin *plugin;
    vlc_list_foreach(plugin, &meter->plugins, node)
    {
        if (plugin->filter != NULL)
        {
            filter_Close(plugin->filter);
            module_unneed(plugin->filter, plugin->filter->p_module);
            vlc_object_delete(plugin->filter);
            plugin->filter = NULL;
        }
        plugin->last_date = VLC_TICK_INVALID;

        if (meter->fmt != NULL)
        {
            plugin->filter = vlc_audio_meter_CreatePluginFilter(meter, plugin);
            if (plugin->filter == NULL)
            {
                ret = VLC_EGENERIC;
                break;
            }
        }
    }

    vlc_mutex_unlock(&meter->lock);

    return ret;
}

void
vlc_audio_meter_Process(struct vlc_audio_meter *meter, block_t *block, vlc_tick_t date)
{
    vlc_mutex_lock(&meter->lock);

    vlc_audio_meter_plugin *plugin;
    vlc_list_foreach(plugin, &meter->plugins, node)
    {
        filter_t *filter = plugin->filter;

        if (filter != NULL)
        {
            plugin->last_date = date + block->i_length;

            block_t *same_block = filter->ops->filter_audio(filter, block);
            assert(same_block == block); (void) same_block;
        }
    }

    vlc_mutex_unlock(&meter->lock);
}

void
vlc_audio_meter_Flush(struct vlc_audio_meter *meter)
{
    vlc_mutex_lock(&meter->lock);

    vlc_audio_meter_plugin *plugin;
    vlc_list_foreach(plugin, &meter->plugins, node)
    {
        filter_t *filter = plugin->filter;
        if (filter != NULL)
            filter_Flush(filter);
    }

    vlc_mutex_unlock(&meter->lock);
}
