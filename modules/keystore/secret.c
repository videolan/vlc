/*****************************************************************************
 * secret.c: libsecret keystore module
 *****************************************************************************
 * Copyright © 2015-2016 VLC authors, VideoLAN and VideoLabs
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_keystore.h>
#include <vlc_interrupt.h>

#include <assert.h>

#include <libsecret/secret.h>
#include <gio/gdbusnamewatching.h>

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname(N_("libsecret keystore"))
    set_description(N_("Secrets are stored via libsecret"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_capability("keystore", 100)
    set_callbacks(Open, Close)
    /* Since we can't destroy gdbus_shared_thread_func */
    cannot_unload_broken_library()
vlc_module_end ()

static const char *const ppsz_keys[] = {
    "protocol",
    "user",
    "server",
    "path",
    "port",
    "realm",
    "authtype",
};
static_assert(sizeof(ppsz_keys)/sizeof(*ppsz_keys) == KEY_MAX, "key mismatch");

static int
str2key(const char *psz_key)
{
    for (unsigned int i = 0; i < KEY_MAX; ++i)
    {
        if (strcmp(ppsz_keys[i], psz_key) == 0)
            return i;
    }
    return -1;
}

static void cancellable_interrupted(void *p_data)
{
    GCancellable *p_canc = p_data;
    g_cancellable_cancel(p_canc);
}

static GCancellable *cancellable_register()
{
    GCancellable *p_canc = g_cancellable_new();
    if (!p_canc)
        return NULL;
    vlc_interrupt_register(cancellable_interrupted, p_canc);
    return p_canc;
}

static void cancellable_unregister(GCancellable *p_canc)
{
    if (p_canc != NULL)
    {
        vlc_interrupt_unregister();
        g_object_unref(p_canc);
    }
}

static GHashTable *
values_to_ghashtable(const char *const ppsz_values[KEY_MAX])
{
    GHashTable *p_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                               NULL, NULL);
    if (!p_hash)
        return NULL;
    for (unsigned int i = 0; i < KEY_MAX; ++i)
    {
        if (ppsz_values[i])
            g_hash_table_insert(p_hash, (gpointer) ppsz_keys[i],
                                (gpointer) ppsz_values[i]);
    }
    return p_hash;
}

static void
ghash_to_value(gpointer key, gpointer value, gpointer user_data)
{
    const char **ppsz_values = user_data;

    const char *psz_key = key;
    int i_key = str2key(psz_key);
    if (i_key == -1 || i_key >= KEY_MAX)
        return;

    ppsz_values[i_key] = strdup((const char *)value);
}

static int
ghashtable_to_values(GHashTable *g_hash, const char *ppsz_values[KEY_MAX])
{
    g_hash_table_foreach(g_hash, ghash_to_value, ppsz_values);
    return VLC_SUCCESS;
}

static void
ghashtable_insert_vlc_id(GHashTable *g_hash)
{
    g_hash_table_insert(g_hash, (gpointer) ".created_by",
                        (gpointer) VLC_KEYSTORE_NAME);
}

static int
Store(vlc_keystore *p_keystore, const char *const ppsz_values[KEY_MAX],
      const uint8_t *p_secret, size_t i_secret_len, const char *psz_label)
{
    SecretService *p_ss = (SecretService *) p_keystore->p_sys;
    GHashTable *p_hash = values_to_ghashtable(ppsz_values);
    if (!p_hash)
        return VLC_EGENERIC;
    ghashtable_insert_vlc_id(p_hash);

    SecretValue *p_sv = secret_value_new((const gchar *)p_secret, i_secret_len,
                                         "text/plain");
    if (!p_sv)
    {
        g_hash_table_unref(p_hash);
        return VLC_EGENERIC;
    }

    GCancellable *p_canc = cancellable_register();
    gboolean b_ret = secret_service_store_sync(p_ss, NULL, p_hash,
                                               SECRET_COLLECTION_DEFAULT,
                                               psz_label, p_sv, p_canc, NULL);
    cancellable_unregister(p_canc);

    secret_value_unref(p_sv);
    g_hash_table_unref(p_hash);
    return b_ret ? VLC_SUCCESS : VLC_EGENERIC;
}

static GList*
items_search(SecretService *p_ss, const char *const ppsz_values[KEY_MAX],
             bool b_safe)
{
    GHashTable *p_hash = values_to_ghashtable(ppsz_values);
    if (!p_hash)
        return 0;

    /* If true, do not allow to remove non VLC entries */
    if (b_safe)
        ghashtable_insert_vlc_id(p_hash);

    GCancellable *p_canc = cancellable_register();
    GList *p_list = secret_service_search_sync(p_ss, NULL, p_hash,
                                               SECRET_SEARCH_ALL
                                               | SECRET_SEARCH_UNLOCK
                                               | SECRET_SEARCH_LOAD_SECRETS,
                                               p_canc, NULL);
    cancellable_unregister(p_canc);
    g_hash_table_unref(p_hash);
    return p_list;
}

static unsigned int
Find(vlc_keystore *p_keystore, const char *const ppsz_values[KEY_MAX],
     vlc_keystore_entry **pp_entries)
{
    SecretService *p_ss = (SecretService *) p_keystore->p_sys;

    GList *p_list = items_search(p_ss, ppsz_values, false);
    if (!p_list)
        return 0;

    unsigned int i_found_count = g_list_length(p_list);
    unsigned int i_entry_count = 0;
    vlc_keystore_entry *p_entries = calloc(i_found_count,
                                           sizeof(vlc_keystore_entry));
    if (!p_entries)
        goto error;

    for (GList *l = p_list; l != NULL; l = l->next)
    {
        SecretItem *p_item = (SecretItem *) l->data;
        GHashTable *p_attrs = secret_item_get_attributes(p_item);

        vlc_keystore_entry *p_entry = &p_entries[i_entry_count++];
        /* fill ppsz_values */
        if (ghashtable_to_values(p_attrs, (const char **) p_entry->ppsz_values))
        {
            g_hash_table_unref(p_attrs);
            goto error;
        }
        g_hash_table_unref(p_attrs);

        /* fill secret */
        SecretValue *p_secret_value = secret_item_get_secret(p_item);
        gsize i_len;
        const gchar *psz_value = secret_value_get(p_secret_value, &i_len);
        if (i_len > 0)
        {
            if (vlc_keystore_entry_set_secret(p_entry,
                                              (const uint8_t *)psz_value, i_len))
            {
                secret_value_unref(p_secret_value);
                goto error;
            }
        }
        secret_value_unref(p_secret_value);
    }
    g_list_free_full(p_list, g_object_unref);
    *pp_entries = p_entries;
    return i_entry_count;

error:
    g_list_free_full(p_list, g_object_unref);
    if (i_entry_count > 0)
        vlc_keystore_release_entries(p_entries, i_entry_count);
    return 0;
}

static unsigned int
Remove(vlc_keystore *p_keystore, const char *const ppsz_values[KEY_MAX])
{
    SecretService *p_ss = (SecretService *) p_keystore->p_sys;

    GList *p_list = items_search(p_ss, ppsz_values, true);
    if (!p_list)
        return 0;

    unsigned int i_entry_count = 0;
    for (GList *l = p_list; l != NULL; l = l->next)
    {
        SecretItem *p_item = (SecretItem *) l->data;
        secret_item_delete(p_item, NULL, NULL, NULL);
        i_entry_count++;
    }
    g_list_free_full(p_list, g_object_unref);
    return i_entry_count;
}

struct secrets_watch_data
{
    vlc_sem_t sem;
    bool b_running;
};

static void
dbus_appeared_cb(GDBusConnection *connection, const gchar *name,
                const gchar *name_owner, gpointer user_data)
{
    (void) connection; (void) name; (void)name_owner;
    struct secrets_watch_data *p_watch_data = user_data;
    p_watch_data->b_running = true;
    vlc_sem_post(&p_watch_data->sem);
}

static void
dbus_vanished_cb(GDBusConnection *connection, const gchar *name,
                gpointer user_data)
{
    (void) connection; (void) name;
    struct secrets_watch_data *p_watch_data = user_data;
    p_watch_data->b_running = false;
    vlc_sem_post(&p_watch_data->sem);
}

static int
Open(vlc_object_t *p_this)
{
    if (!p_this->obj.force)
    {
        /* First, check if secrets service is running using g_bus_watch_name().
         * Indeed, secret_service_get_sync will spawn a service if it's not
         * running, even on non Gnome environments */
        struct secrets_watch_data watch_data;
        watch_data.b_running = false;
        vlc_sem_init(&watch_data.sem, 0);

        guint i_id = g_bus_watch_name(G_BUS_TYPE_SESSION,
                                      "org.freedesktop.secrets",
                                      G_BUS_NAME_WATCHER_FLAGS_NONE,
                                      dbus_appeared_cb, dbus_vanished_cb,
                                      &watch_data, NULL);

        /* We are guaranteed that one of the callbacks will be invoked after
         * calling g_bus_watch_name */
        vlc_sem_wait_i11e(&watch_data.sem);

        g_bus_unwatch_name(i_id);
        vlc_sem_destroy(&watch_data.sem);

        if (!watch_data.b_running)
            return VLC_EGENERIC;
    }

    GCancellable *p_canc = cancellable_register();
    SecretService *p_ss = secret_service_get_sync(SECRET_SERVICE_NONE,
                                                  p_canc, NULL);
    cancellable_unregister(p_canc);
    if (!p_ss)
        return VLC_EGENERIC;

    vlc_keystore *p_keystore = (vlc_keystore *)p_this;

    p_keystore->p_sys = (vlc_keystore_sys *) p_ss;
    p_keystore->pf_store = Store;
    p_keystore->pf_find = Find;
    p_keystore->pf_remove = Remove;

    return VLC_SUCCESS;
}

static void
Close(vlc_object_t *p_this)
{
    vlc_keystore *p_keystore = (vlc_keystore *)p_this;
    SecretService *p_ss = (SecretService *) p_keystore->p_sys;
    g_object_unref(p_ss);
    secret_service_disconnect();
}
