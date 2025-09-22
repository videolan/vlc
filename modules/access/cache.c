/*****************************************************************************
 * cache.c: access cache helper
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <vlc_threads.h>

#include "cache.h"

#include <assert.h>

#define VLC_ACCESS_CACHE_TTL VLC_TICK_FROM_SEC(5)
#define VLC_ACCESS_CACHE_MAX_ENTRY 5

void
vlc_access_cache_entry_Delete(struct vlc_access_cache_entry *entry)
{
    free(entry->url);
    free(entry->username);

    free(entry);
}

struct vlc_access_cache_entry *
vlc_access_cache_entry_New(void *context, const char *url, const char *username,
                           void (*free_cb)(void *context))
{
    struct vlc_access_cache_entry *entry = malloc(sizeof(*entry));
    if (unlikely(entry == NULL))
        return NULL;

    entry->url = strdup(url);
    entry->username = username ? strdup(username) : NULL;
    if (!entry->url || (entry->username == NULL) != (username == NULL))
    {
        vlc_access_cache_entry_Delete(entry);
        return NULL;
    }

    entry->context = context;
    entry->free_cb = free_cb;

    return entry;
}

#ifdef VLC_ACCESS_CACHE_CAN_REGISTER
static void *
vlc_access_cache_Thread(void *data)
{
    struct vlc_access_cache *cache = data;

    vlc_thread_set_name("vlc-axs-cache");

    vlc_mutex_lock(&cache->lock);
    while (cache->running)
    {
        if (!vlc_list_is_empty(&cache->entries))
        {
            struct vlc_access_cache_entry *entry =
                vlc_list_first_entry_or_null(&cache->entries,
                                             struct vlc_access_cache_entry, node);

            if (entry->timeout == 0 ||
                vlc_cond_timedwait(&cache->cond, &cache->lock, entry->timeout) != 0)
            {
                vlc_list_remove(&entry->node);

                vlc_mutex_unlock(&cache->lock);

                entry->free_cb(entry->context);
                vlc_access_cache_entry_Delete(entry);

                vlc_mutex_lock(&cache->lock);
            }
        }
        else
            vlc_cond_wait(&cache->cond, &cache->lock);
    }
    vlc_mutex_unlock(&cache->lock);

    return NULL;
}

static void
vlc_access_cache_InitOnce(void *data)
{
    struct vlc_access_cache *cache = data;

    vlc_mutex_lock(&cache->lock);

    cache->running = true;
    int ret = vlc_clone(&cache->thread, vlc_access_cache_Thread, cache);
    if (ret != 0)
        cache->running = false;

    vlc_mutex_unlock(&cache->lock);
}

void
vlc_access_cache_Destroy(struct vlc_access_cache *cache)
{
    vlc_mutex_lock(&cache->lock);
    if (cache->running)
    {
        cache->running = false;
        vlc_cond_signal(&cache->cond);
        vlc_mutex_unlock(&cache->lock);
        vlc_join(cache->thread, NULL);
    }
    else
        vlc_mutex_unlock(&cache->lock);

    struct vlc_access_cache_entry *entry;
    vlc_list_foreach(entry, &cache->entries, node)
    {
        entry->free_cb(entry->context);
        vlc_access_cache_entry_Delete(entry);
    }
}
#endif

void
vlc_access_cache_AddEntry(struct vlc_access_cache *cache,
                          struct vlc_access_cache_entry *entry)
{
#ifdef VLC_ACCESS_CACHE_CAN_REGISTER
    vlc_once(&cache->once, vlc_access_cache_InitOnce, cache);
#endif

    vlc_mutex_lock(&cache->lock);

#ifdef VLC_ACCESS_CACHE_CAN_REGISTER
    if (!cache->running)
    {
        vlc_mutex_unlock(&cache->lock);
        entry->free_cb(entry->context);
        vlc_access_cache_entry_Delete(entry);
        return;
    }
#endif

    struct vlc_access_cache_entry *it;
    size_t count = 0;
    vlc_list_foreach(it, &cache->entries, node)
        count++;

    if (count >= VLC_ACCESS_CACHE_MAX_ENTRY)
    {
        /* Too many entries, signal the thread that will delete the first one */
        it = vlc_list_first_entry_or_null(&cache->entries,
                                          struct vlc_access_cache_entry, node);
        it->timeout = 0;
    }

    entry->timeout = vlc_tick_now() + VLC_ACCESS_CACHE_TTL;
    vlc_list_append(&entry->node, &cache->entries);

#ifdef VLC_ACCESS_CACHE_CAN_REGISTER
    vlc_cond_signal(&cache->cond);
#endif
    vlc_mutex_unlock(&cache->lock);
}

struct vlc_access_cache_entry *
vlc_access_cache_GetEntry(struct vlc_access_cache *cache,
                          const char *url, const char *username)
{
#ifdef VLC_ACCESS_CACHE_CAN_REGISTER
    vlc_once(&cache->once, vlc_access_cache_InitOnce, cache);
#endif

    vlc_mutex_lock(&cache->lock);

    struct vlc_access_cache_entry *it;

    vlc_list_foreach(it, &cache->entries, node)
    {

        if (strcmp(url, it->url) == 0
         && (username == NULL) == (it->username == NULL)
         && (username != NULL ? strcmp(username, it->username) == 0 : true))
        {
            vlc_list_remove(&it->node);
#ifdef VLC_ACCESS_CACHE_CAN_REGISTER
            vlc_cond_signal(&cache->cond);
#endif
            vlc_mutex_unlock(&cache->lock);
            return it;
        }
    }

    vlc_mutex_unlock(&cache->lock);

    return NULL;
}
