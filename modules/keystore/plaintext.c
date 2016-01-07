/*****************************************************************************
 * plaintext.c: Insecure keystore
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

#include <stdarg.h>
#include <stdio.h>
#ifdef HAVE_FLOCK
#include <sys/file.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fs.h>
#include <vlc_memory.h>
#include <vlc_keystore.h>
#include <vlc_strings.h>

#include <assert.h>

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname(N_("plaintext keystore (insecure)"))
    set_description(N_("secrets are stored in plaintext without any encryption"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    add_string("keystore-plaintext-file", NULL, NULL, NULL, false )
    set_capability("keystore", 0)
    set_callbacks(Open, Close)
vlc_module_end ()

struct list
{
    vlc_keystore_entry *p_entries;
    unsigned            i_count;
    unsigned            i_max;
};

struct vlc_keystore_sys
{
    char *      psz_file;
    FILE *      p_file;
    int         i_fd;
    struct list list;
    bool        b_error;
};

static struct
{
    vlc_mutex_t         lock;
    unsigned int        i_ref_count;
    vlc_keystore_sys *  p_sys;
} instance = {
    .lock = VLC_STATIC_MUTEX,
    .i_ref_count = 0,
    .p_sys = NULL
};

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

static void
list_free(struct list *p_list)
{
    vlc_keystore_release_entries(p_list->p_entries, p_list->i_count);
    p_list->p_entries = NULL;
    p_list->i_count = 0;
    p_list->i_max = 0;
}

static int
values_copy(const char * ppsz_dst[KEY_MAX], const char *const ppsz_src[KEY_MAX])
{
    for (unsigned int i = 0; i < KEY_MAX; ++i)
    {
        if (ppsz_src[i])
        {
            ppsz_dst[i] = strdup(ppsz_src[i]);
            if (!ppsz_dst[i])
                return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

static int
str_write(FILE *p_file, const char *psz_str)
{
    size_t i_len = strlen(psz_str);
    return fwrite(psz_str, sizeof(char), i_len, p_file) == i_len ? VLC_SUCCESS
                                                                 : VLC_EGENERIC;
}

static int
values_write(FILE *p_file, const char *const ppsz_values[KEY_MAX])
{
    for (unsigned int i = 0; i < KEY_MAX; ++i)
    {
        if (!ppsz_values[i])
            continue;
        if (str_write(p_file, ppsz_keys[i]))
            return VLC_EGENERIC;
        if (str_write(p_file, ":"))
            return VLC_EGENERIC;
        char *psz_b64 = vlc_b64_encode(ppsz_values[i]);
        if (!psz_b64 || str_write(p_file, psz_b64))
        {
            free(psz_b64);
            return VLC_EGENERIC;
        }
        free(psz_b64);
        for (unsigned int j = i + 1; j < KEY_MAX; ++j)
        {
            if (ppsz_values[j])
            {
                if (str_write(p_file, ","))
                    return VLC_EGENERIC;
                break;
            }
        }
    }

    return VLC_SUCCESS;
}

static vlc_keystore_entry *
list_new_entry(struct list *p_list)
{
    if (p_list->i_count + 1 > p_list->i_max)
    {
        p_list->i_max += 10;
        vlc_keystore_entry *p_entries = realloc(p_list->p_entries, p_list->i_max
                                                * sizeof(*p_list->p_entries));
        if (!p_entries)
        {
            list_free(p_list);
            return NULL;
        }
        p_list->p_entries = p_entries;
    }
    vlc_keystore_entry *p_entry = &p_list->p_entries[p_list->i_count];
    p_entry->p_secret = NULL;
    VLC_KEYSTORE_VALUES_INIT(p_entry->ppsz_values);
    p_list->i_count++;

    return p_entry;
}

static vlc_keystore_entry *
list_get_entry(struct list *p_list, const char *const ppsz_values[KEY_MAX],
              unsigned *p_start_index)
{
    for (unsigned int i = p_start_index ? *p_start_index : 0;
         i < p_list->i_count; ++i)
    {
        vlc_keystore_entry *p_entry = &p_list->p_entries[i];
        if (!p_entry->p_secret)
            continue;

        bool b_match = true;
        for (unsigned int j = 0; j < KEY_MAX; ++j)
        {
            const char *psz_value1 = ppsz_values[j];
            const char *psz_value2 = p_entry->ppsz_values[j];

            if (!psz_value1)
                continue;
            if (!psz_value2 || strcmp(psz_value1, psz_value2))
                b_match = false;
        }
        if (b_match)
        {
            if (p_start_index)
                *p_start_index = i + 1;
            return p_entry;
        }
    }
    return NULL;
}

static int
truncate0(int i_fd)
{
#ifndef _WIN32
    return ftruncate(i_fd, 0) == 0 ? VLC_SUCCESS : VLC_EGENERIC;
#else
    return _chsize(i_fd, 0) == 0 ? VLC_SUCCESS : VLC_EGENERIC;
#endif
}

/* a line is "{key1:VALUE1_B64,key2:VALUE2_B64}:PASSWORD_B64" */
static int
list_save(vlc_keystore_sys *p_sys, struct list *p_list)
{
    int i_ret = VLC_EGENERIC;
    rewind(p_sys->p_file);

    if (truncate0(p_sys->i_fd))
        goto end;

    for (unsigned int i = 0; i < p_list->i_count; ++i)
    {
        vlc_keystore_entry *p_entry = &p_list->p_entries[i];
        if (!p_entry->p_secret)
            continue;

        if (str_write(p_sys->p_file, "{"))
            goto end;
        if (values_write(p_sys->p_file,
                         (const char *const *) p_entry->ppsz_values))
            goto end;
        if (str_write(p_sys->p_file, "}:"))
            goto end;
        char *psz_b64 = vlc_b64_encode_binary(p_entry->p_secret,
                                              p_entry->i_secret_len);
        if (!psz_b64 || str_write(p_sys->p_file, psz_b64))
        {
            free(psz_b64);
            goto end;
        }
        free(psz_b64);
        if (i < p_list->i_count - 1 && str_write(p_sys->p_file, "\n"))
            goto end;
    }
    i_ret = VLC_SUCCESS;
end:

    if (i_ret != VLC_SUCCESS)
    {
        p_sys->b_error = true;
        list_free(p_list);
    }
    return i_ret;
}

static int
list_read(vlc_keystore_sys *p_sys, struct list *p_list)
{
    char *psz_line = NULL;
    size_t i_line_len = 0;
    ssize_t i_read;
    bool b_valid = false;

    while ((i_read = getline(&psz_line, &i_line_len, p_sys->p_file)) != -1)
    {
        char *p = psz_line;
        if (*(p++) != '{')
            goto end;

        vlc_keystore_entry *p_entry = list_new_entry(p_list);
        if (!p_entry)
            goto end;

        bool b_end = false;
        while (*p != '\0' && !b_end)
        {
            int i_key;
            char *p_key, *p_value;
            size_t i_len;

            /* read key */
            i_len = strcspn(p, ":");
            if (!i_len || p[i_len] == '\0')
                goto end;

            p[i_len] = '\0';
            p_key = p;
            i_key = str2key(p_key);
            if (i_key == -1 || i_key >= KEY_MAX)
                goto end;
            p += i_len + 1;

            /* read value */
            i_len = strcspn(p, ",}");
            if (!i_len || p[i_len] == '\0')
                goto end;

            if (p[i_len] == '}')
                b_end = true;

            p[i_len] = '\0';
            p_value = vlc_b64_decode(p); /* BASE 64 */
            if (!p_value)
                goto end;
            p += i_len + 1;

            p_entry->ppsz_values[i_key] = p_value;
        }
        /* read passwd */
        if (*p == '\0' || *p != ':')
            goto end;

        p_entry->i_secret_len = vlc_b64_decode_binary(&p_entry->p_secret, p + 1);
        if (!p_entry->p_secret)
            goto end;
    }

    b_valid = true;

end:
    free(psz_line);
    if (!b_valid)
    {
        p_sys->b_error = true;
        list_free(p_list);
    }
    return VLC_SUCCESS;
}

static int
Store(vlc_keystore *p_keystore, const char *const ppsz_values[KEY_MAX],
      const uint8_t *p_secret, size_t i_secret_len, const char *psz_label)
{
    vlc_mutex_lock(&instance.lock);

    (void) psz_label;
    vlc_keystore_sys *p_sys = p_keystore->p_sys;
    assert(p_sys == instance.p_sys);
    struct list *p_list = &p_sys->list;
    vlc_keystore_entry *p_entry = list_get_entry(p_list, ppsz_values, NULL);

    if (p_entry)
    {
        free(p_entry->p_secret);
        p_entry->p_secret = NULL;
        for (unsigned int i = 0; i < KEY_MAX; ++i)
        {
            free(p_entry->ppsz_values[i]);
            p_entry->ppsz_values[i] = NULL;
        }
    }
    else
    {
        p_entry = list_new_entry(p_list);
        if (!p_entry)
            goto error;
    }
    if (values_copy((const char **)p_entry->ppsz_values, ppsz_values))
        goto error;

    if (vlc_keystore_entry_set_secret(p_entry, p_secret, i_secret_len))
        goto error;

    int i_ret = list_save(p_sys, &p_sys->list);
    vlc_mutex_unlock(&instance.lock);
    return i_ret;

error:
    vlc_mutex_unlock(&instance.lock);
    return VLC_EGENERIC;
}

static unsigned int
Find(vlc_keystore *p_keystore, const char *const ppsz_values[KEY_MAX],
     vlc_keystore_entry **pp_entries)
{
    vlc_mutex_lock(&instance.lock);

    vlc_keystore_sys *p_sys = p_keystore->p_sys;
    assert(p_sys == instance.p_sys);
    struct list *p_list = &p_sys->list;
    struct list out_list = { 0 };
    vlc_keystore_entry *p_entry;
    unsigned i_index = 0;

    while ((p_entry = list_get_entry(p_list, ppsz_values, &i_index)))
    {
        vlc_keystore_entry *p_out_entry = list_new_entry(&out_list);

        if (!p_out_entry || values_copy((const char **)p_out_entry->ppsz_values,
                                        (const char *const*)p_entry->ppsz_values))
        {
            list_free(&out_list);
            break;
        }

        if (vlc_keystore_entry_set_secret(p_out_entry, p_entry->p_secret,
                                          p_entry->i_secret_len))
        {
            list_free(&out_list);
            break;
        }
    }

    *pp_entries = out_list.p_entries;

    vlc_mutex_unlock(&instance.lock);
    return out_list.i_count;
}

static unsigned int
Remove(vlc_keystore *p_keystore, const char *const ppsz_values[KEY_MAX])
{
    vlc_mutex_lock(&instance.lock);

    vlc_keystore_sys *p_sys = p_keystore->p_sys;
    assert(p_sys == instance.p_sys);
    struct list *p_list = &p_sys->list;
    vlc_keystore_entry *p_entry;
    unsigned i_index = 0, i_count = 0;

    while ((p_entry = list_get_entry(p_list, ppsz_values, &i_index)))
    {
        free(p_entry->p_secret);
        p_entry->p_secret = NULL;
        i_count++;
    }

    if (list_save(p_sys, &p_sys->list) != VLC_SUCCESS)
    {
        vlc_mutex_unlock(&instance.lock);
        return 0;
    }

    vlc_mutex_unlock(&instance.lock);
    return i_count;
}

static void
CleanUp(vlc_keystore_sys *p_sys)
{
    if (p_sys->p_file)
    {
        if (p_sys->b_error)
        {
            if (truncate0(p_sys->i_fd))
                vlc_unlink(p_sys->psz_file);
        }

#ifdef HAVE_FLOCK
        flock(p_sys->i_fd, LOCK_UN);
#endif
        fclose(p_sys->p_file);
    }

    list_free(&p_sys->list);
    free(p_sys->psz_file);
    free(p_sys);
}

static int
Open(vlc_object_t *p_this)
{
    vlc_keystore *p_keystore = (vlc_keystore *)p_this;
    vlc_keystore_sys *p_sys;

    vlc_mutex_lock(&instance.lock);

    /* The p_sys context is shared and protected between all threads */
    if (instance.i_ref_count == 0)
    {
        char *psz_file = var_InheritString(p_this, "keystore-plaintext-file");
        if (!psz_file)
            return VLC_EGENERIC;

        p_sys = calloc(1, sizeof(vlc_keystore_sys));
        if (!p_sys)
        {
            free(psz_file);
            return VLC_EGENERIC;
        }

        p_sys->psz_file = psz_file;
        p_sys->p_file = vlc_fopen(p_sys->psz_file, "a+");

        if (!p_sys->p_file)
        {
            CleanUp(p_sys);
            return VLC_EGENERIC;
        }
        p_sys->i_fd = fileno(p_sys->p_file);
        if (p_sys->i_fd == -1)
        {
            CleanUp(p_sys);
            return VLC_EGENERIC;
        }

        /* Fail if an other LibVLC process acquired the file lock.
         * If HAVE_FLOCK is not defined, the running OS is most likely Windows
         * and a lock was already acquired when the file was opened. */
#ifdef HAVE_FLOCK
        if (flock(p_sys->i_fd, LOCK_EX|LOCK_NB) != 0)
        {
            CleanUp(p_sys);
            return VLC_EGENERIC;
        }
#endif

        if (list_read(p_sys, &p_sys->list) != VLC_SUCCESS)
        {
            CleanUp(p_sys);
            return VLC_EGENERIC;
        }
        instance.p_sys = p_sys;
    }
    else
        p_sys = instance.p_sys;

    instance.i_ref_count++;
    p_keystore->p_sys = p_sys;

    p_keystore->pf_store = Store;
    p_keystore->pf_find = Find;
    p_keystore->pf_remove = Remove;

    vlc_mutex_unlock(&instance.lock);

    return VLC_SUCCESS;
}

static void
Close(vlc_object_t *p_this)
{
    (void) p_this;

    vlc_mutex_lock(&instance.lock);
    assert(((vlc_keystore *)p_this)->p_sys == instance.p_sys);
    if (--instance.i_ref_count == 0)
    {
        CleanUp(instance.p_sys);
        instance.p_sys = NULL;
    }
    vlc_mutex_unlock(&instance.lock);
}
