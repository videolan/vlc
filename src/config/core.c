/*****************************************************************************
 * core.c management of the modules configuration
 *****************************************************************************
 * Copyright (C) 2001-2007 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_actions.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>

#include "vlc_configuration.h"

#include <errno.h>
#include <assert.h>

#include "configuration.h"
#include "modules/modules.h"

vlc_rwlock_t config_lock = VLC_STATIC_RWLOCK;
bool config_dirty = false;

static inline char *strdupnull (const char *src)
{
    return src ? strdup (src) : NULL;
}

int config_GetType(const char *psz_name)
{
    module_config_t *p_config = config_FindConfig(psz_name);

    /* sanity checks */
    if( !p_config )
    {
        return 0;
    }

    switch( CONFIG_CLASS(p_config->i_type) )
    {
        case CONFIG_ITEM_FLOAT:
            return VLC_VAR_FLOAT;
        case CONFIG_ITEM_INTEGER:
            return VLC_VAR_INTEGER;
        case CONFIG_ITEM_BOOL:
            return VLC_VAR_BOOL;
        case CONFIG_ITEM_STRING:
            return VLC_VAR_STRING;
        default:
            return 0;
    }
}

bool config_IsSafe( const char *name )
{
    module_config_t *p_config = config_FindConfig( name );
    return p_config != NULL && p_config->b_safe;
}

int64_t config_GetInt(const char *psz_name)
{
    module_config_t *p_config = config_FindConfig( psz_name );

    /* sanity checks */
    assert(p_config != NULL);
    assert(IsConfigIntegerType(p_config->i_type));

    int64_t val;

    vlc_rwlock_rdlock (&config_lock);
    val = p_config->value.i;
    vlc_rwlock_unlock (&config_lock);
    return val;
}

float config_GetFloat(const char *psz_name)
{
    module_config_t *p_config;

    p_config = config_FindConfig( psz_name );

    /* sanity checks */
    assert(p_config != NULL);
    assert(IsConfigFloatType(p_config->i_type));

    float val;

    vlc_rwlock_rdlock (&config_lock);
    val = p_config->value.f;
    vlc_rwlock_unlock (&config_lock);
    return val;
}

char *config_GetPsz(const char *psz_name)
{
    module_config_t *p_config;

    p_config = config_FindConfig( psz_name );

    /* sanity checks */
    assert(p_config != NULL);
    assert(IsConfigStringType (p_config->i_type));

    /* return a copy of the string */
    vlc_rwlock_rdlock (&config_lock);
    char *psz_value = strdupnull (p_config->value.psz);
    vlc_rwlock_unlock (&config_lock);

    return psz_value;
}

void config_PutPsz(const char *psz_name, const char *psz_value)
{
    module_config_t *p_config = config_FindConfig( psz_name );


    /* sanity checks */
    assert(p_config != NULL);
    assert(IsConfigStringType(p_config->i_type));

    char *str, *oldstr;
    if ((psz_value != NULL) && *psz_value)
        str = strdup (psz_value);
    else
        str = NULL;

    vlc_rwlock_wrlock (&config_lock);
    oldstr = (char *)p_config->value.psz;
    p_config->value.psz = str;
    config_dirty = true;
    vlc_rwlock_unlock (&config_lock);

    free (oldstr);
}

void config_PutInt(const char *psz_name, int64_t i_value )
{
    module_config_t *p_config = config_FindConfig( psz_name );

    /* sanity checks */
    assert(p_config != NULL);
    assert(IsConfigIntegerType(p_config->i_type));

    if (i_value < p_config->min.i)
        i_value = p_config->min.i;
    if (i_value > p_config->max.i)
        i_value = p_config->max.i;

    vlc_rwlock_wrlock (&config_lock);
    p_config->value.i = i_value;
    config_dirty = true;
    vlc_rwlock_unlock (&config_lock);
}

void config_PutFloat(const char *psz_name, float f_value)
{
    module_config_t *p_config = config_FindConfig( psz_name );

    /* sanity checks */
    assert(p_config != NULL);
    assert(IsConfigFloatType(p_config->i_type));

    /* if f_min == f_max == 0, then do not use them */
    if ((p_config->min.f == 0.f) && (p_config->max.f == 0.f))
        ;
    else if (f_value < p_config->min.f)
        f_value = p_config->min.f;
    else if (f_value > p_config->max.f)
        f_value = p_config->max.f;

    vlc_rwlock_wrlock (&config_lock);
    p_config->value.f = f_value;
    config_dirty = true;
    vlc_rwlock_unlock (&config_lock);
}

ssize_t config_GetIntChoices(const char *name,
                             int64_t **restrict values, char ***restrict texts)
{
    *values = NULL;
    *texts = NULL;

    module_config_t *cfg = config_FindConfig(name);
    assert(cfg != NULL);

    size_t count = cfg->list_count;
    if (count == 0)
    {
        int (*cb)(const char *, int64_t **, char ***);

        cb = module_Symbol(NULL, cfg->owner, "vlc_entry_cfg_int_enum");
        if (cb == NULL)
            return 0;

        return cb(name, values, texts);
    }

    int64_t *vals = vlc_alloc (count, sizeof (*vals));
    char **txts = vlc_alloc (count, sizeof (*txts));
    if (vals == NULL || txts == NULL)
    {
        errno = ENOMEM;
        goto error;
    }

    for (size_t i = 0; i < count; i++)
    {
        vals[i] = cfg->list.i[i];
        /* FIXME: use module_gettext() instead */
        txts[i] = strdup ((cfg->list_text[i] != NULL)
                                       ? vlc_gettext (cfg->list_text[i]) : "");
        if (unlikely(txts[i] == NULL))
        {
            for (int j = i - 1; j >= 0; --j)
                free(txts[j]);
            errno = ENOMEM;
            goto error;
        }
    }

    *values = vals;
    *texts = txts;
    return count;
error:

    free(vals);
    free(txts);
    return -1;
}


static ssize_t config_ListModules (const char *cap, char ***restrict values,
                                   char ***restrict texts)
{
    module_t *const *list;
    size_t n = module_list_cap(&list, cap);
    char **vals = malloc ((n + 2) * sizeof (*vals));
    char **txts = malloc ((n + 2) * sizeof (*txts));
    if (!vals || !txts)
    {
        free (vals);
        free (txts);
        *values = *texts = NULL;
        return -1;
    }

    size_t i = 0;

    vals[i] = strdup ("any");
    txts[i] = strdup (_("Automatic"));
    if (!vals[i] || !txts[i])
        goto error;

    ++i;
    for (; i <= n; i++)
    {
        vals[i] = strdup (module_get_object (list[i - 1]));
        txts[i] = strdup (module_gettext (list[i - 1],
                               module_get_name (list[i - 1], true)));
        if( !vals[i] || !txts[i])
            goto error;
    }
    vals[i] = strdup ("none");
    txts[i] = strdup (_("Disable"));
    if( !vals[i] || !txts[i])
        goto error;

    *values = vals;
    *texts = txts;
    return i + 1;

error:
    for (size_t j = 0; j <= i; ++j)
    {
        free (vals[j]);
        free (txts[j]);
    }
    free(vals);
    free(txts);
    *values = *texts = NULL;
    return -1;
}

ssize_t config_GetPszChoices(const char *name,
                             char ***restrict values, char ***restrict texts)
{
    *values = *texts = NULL;

    module_config_t *cfg = config_FindConfig(name);
    if (cfg == NULL)
    {
        errno = ENOENT;
        return -1;
    }

    switch (cfg->i_type)
    {
        case CONFIG_ITEM_MODULE:
            return config_ListModules (cfg->psz_type, values, texts);
        default:
            if (!IsConfigStringType (cfg->i_type))
            {
                errno = EINVAL;
                return -1;
            }
            break;
    }

    size_t count = cfg->list_count;
    if (count == 0)
    {
        int (*cb)(const char *, char ***, char ***);

        cb = module_Symbol(NULL, cfg->owner, "vlc_entry_cfg_str_enum");
        if (cb == NULL)
            return 0;

        return cb(name, values, texts);
    }

    char **vals = malloc (sizeof (*vals) * count);
    char **txts = malloc (sizeof (*txts) * count);
    if (!vals || !txts)
    {
        free (vals);
        free (txts);
        errno = ENOMEM;
        return -1;
    }

    size_t i;
    for (i = 0; i < count; i++)
    {
        vals[i] = strdup ((cfg->list.psz[i] != NULL) ? cfg->list.psz[i] : "");
        /* FIXME: use module_gettext() instead */
        txts[i] = strdup ((cfg->list_text[i] != NULL)
                                       ? vlc_gettext (cfg->list_text[i]) : "");
        if (!vals[i] || !txts[i])
            goto error;
    }

    *values = vals;
    *texts = txts;
    return count;

error:
    for (size_t j = 0; j <= i; ++j)
    {
        free (vals[j]);
        free (txts[j]);
    }
    free(vals);
    free(txts);
    errno = ENOMEM;
    return -1;
}

static int confcmp (const void *a, const void *b)
{
    const module_config_t *const *ca = a, *const *cb = b;

    return strcmp ((*ca)->psz_name, (*cb)->psz_name);
}

static int confnamecmp (const void *key, const void *elem)
{
    const module_config_t *const *conf = elem;

    return strcmp (key, (*conf)->psz_name);
}

static struct
{
    module_config_t **list;
    size_t count;
} config = { NULL, 0 };

/**
 * Index the configuration items by name for faster lookups.
 */
int config_SortConfig (void)
{
    vlc_plugin_t *p;
    size_t nconf = 0;

    for (p = vlc_plugins; p != NULL; p = p->next)
         nconf += p->conf.size;

    module_config_t **clist = vlc_alloc (nconf, sizeof (*clist));
    if (unlikely(clist == NULL))
        return VLC_ENOMEM;

    nconf = 0;
    for (p = vlc_plugins; p != NULL; p = p->next)
    {
        module_config_t *item, *end;

        for (item = p->conf.items, end = item + p->conf.size;
             item < end;
             item++)
        {
            if (!CONFIG_ITEM(item->i_type))
                continue; /* ignore hints */
            clist[nconf++] = item;
        }
    }

    qsort (clist, nconf, sizeof (*clist), confcmp);

    config.list = clist;
    config.count = nconf;
    return VLC_SUCCESS;
}

void config_UnsortConfig (void)
{
    module_config_t **clist;

    clist = config.list;
    config.list = NULL;
    config.count = 0;

    free (clist);
}

module_config_t *config_FindConfig(const char *name)
{
    if (unlikely(name == NULL))
        return NULL;

    module_config_t *const *p;
    p = bsearch (name, config.list, config.count, sizeof (*p), confnamecmp);
    return p ? *p : NULL;
}

/**
 * Destroys an array of configuration items.
 * \param config start of array of items
 * \param confsize number of items in the array
 */
void config_Free (module_config_t *tab, size_t confsize)
{
    for (size_t j = 0; j < confsize; j++)
    {
        module_config_t *p_item = &tab[j];

        if (IsConfigStringType (p_item->i_type))
        {
            free (p_item->value.psz);
            if (p_item->list_count)
                free (p_item->list.psz);
        }

        free (p_item->list_text);
    }

    free (tab);
}

void config_ResetAll(void)
{
    vlc_rwlock_wrlock (&config_lock);
    for (vlc_plugin_t *p = vlc_plugins; p != NULL; p = p->next)
    {
        for (size_t i = 0; i < p->conf.size; i++ )
        {
            module_config_t *p_config = p->conf.items + i;

            if (IsConfigIntegerType (p_config->i_type))
                p_config->value.i = p_config->orig.i;
            else
            if (IsConfigFloatType (p_config->i_type))
                p_config->value.f = p_config->orig.f;
            else
            if (IsConfigStringType (p_config->i_type))
            {
                free ((char *)p_config->value.psz);
                p_config->value.psz =
                        strdupnull (p_config->orig.psz);
            }
        }
    }
    vlc_rwlock_unlock (&config_lock);
}
