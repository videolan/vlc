/*****************************************************************************
 * core.c management of the modules configuration
 *****************************************************************************
 * Copyright (C) 2001-2007 VLC authors and VideoLAN
 * $Id$
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
#include <vlc_keys.h>
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

#undef config_GetType
/*****************************************************************************
 * config_GetType: get the type of a variable (bool, int, float, string)
 *****************************************************************************
 * This function is used to get the type of a variable from its name.
 * Beware, this is quite slow.
 *****************************************************************************/
int config_GetType( vlc_object_t *p_this, const char *psz_name )
{
    module_config_t *p_config;
    int i_type;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        return 0;
    }

    switch( CONFIG_CLASS(p_config->i_type) )
    {
    case CONFIG_ITEM_FLOAT:
        i_type = VLC_VAR_FLOAT;
        break;

    case CONFIG_ITEM_INTEGER:
        i_type = VLC_VAR_INTEGER;
        break;

    case CONFIG_ITEM_BOOL:
        i_type = VLC_VAR_BOOL;
        break;

    case CONFIG_ITEM_STRING:
        i_type = VLC_VAR_STRING;
        break;

    default:
        i_type = 0;
        break;
    }

    return i_type;
}

bool config_IsSafe( const char *name )
{
    module_config_t *p_config = config_FindConfig( NULL, name );
    return p_config != NULL && p_config->b_safe;
}

#undef config_GetInt
/*****************************************************************************
 * config_GetInt: get the value of an int variable
 *****************************************************************************
 * This function is used to get the value of variables which are internally
 * represented by an integer (CONFIG_ITEM_INTEGER and
 * CONFIG_ITEM_BOOL).
 *****************************************************************************/
int64_t config_GetInt( vlc_object_t *p_this, const char *psz_name )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Err( p_this, "option %s does not exist", psz_name );
        return -1;
    }

    if (!IsConfigIntegerType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to an int", psz_name );
        return -1;
    }

    int64_t val;

    vlc_rwlock_rdlock (&config_lock);
    val = p_config->value.i;
    vlc_rwlock_unlock (&config_lock);
    return val;
}

#undef config_GetFloat
/*****************************************************************************
 * config_GetFloat: get the value of a float variable
 *****************************************************************************
 * This function is used to get the value of variables which are internally
 * represented by a float (CONFIG_ITEM_FLOAT).
 *****************************************************************************/
float config_GetFloat( vlc_object_t *p_this, const char *psz_name )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Err( p_this, "option %s does not exist", psz_name );
        return -1;
    }

    if (!IsConfigFloatType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to a float", psz_name );
        return -1;
    }

    float val;

    vlc_rwlock_rdlock (&config_lock);
    val = p_config->value.f;
    vlc_rwlock_unlock (&config_lock);
    return val;
}

#undef config_GetPsz
/*****************************************************************************
 * config_GetPsz: get the string value of a string variable
 *****************************************************************************
 * This function is used to get the value of variables which are internally
 * represented by a string (CONFIG_ITEM_STRING, CONFIG_ITEM_*FILE,
 * CONFIG_ITEM_DIRECTORY, CONFIG_ITEM_PASSWORD, and CONFIG_ITEM_MODULE).
 *
 * Important note: remember to free() the returned char* because it's a
 *   duplicate of the actual value. It isn't safe to return a pointer to the
 *   actual value as it can be modified at any time.
 *****************************************************************************/
char * config_GetPsz( vlc_object_t *p_this, const char *psz_name )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Err( p_this, "option %s does not exist", psz_name );
        return NULL;
    }

    if (!IsConfigStringType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to a string", psz_name );
        return NULL;
    }

    /* return a copy of the string */
    vlc_rwlock_rdlock (&config_lock);
    char *psz_value = strdupnull (p_config->value.psz);
    vlc_rwlock_unlock (&config_lock);

    return psz_value;
}

#undef config_PutPsz
/*****************************************************************************
 * config_PutPsz: set the string value of a string variable
 *****************************************************************************
 * This function is used to set the value of variables which are internally
 * represented by a string (CONFIG_ITEM_STRING, CONFIG_ITEM_*FILE,
 * CONFIG_ITEM_DIRECTORY, CONFIG_ITEM_PASSWORD, and CONFIG_ITEM_MODULE).
 *****************************************************************************/
void config_PutPsz( vlc_object_t *p_this,
                      const char *psz_name, const char *psz_value )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );


    /* sanity checks */
    if( !p_config )
    {
        msg_Warn( p_this, "option %s does not exist", psz_name );
        return;
    }

    if (!IsConfigStringType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to a string", psz_name );
        return;
    }

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

#undef config_PutInt
/*****************************************************************************
 * config_PutInt: set the integer value of an int variable
 *****************************************************************************
 * This function is used to set the value of variables which are internally
 * represented by an integer (CONFIG_ITEM_INTEGER and
 * CONFIG_ITEM_BOOL).
 *****************************************************************************/
void config_PutInt( vlc_object_t *p_this, const char *psz_name,
                    int64_t i_value )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Warn( p_this, "option %s does not exist", psz_name );
        return;
    }

    if (!IsConfigIntegerType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to an int", psz_name );
        return;
    }

    if (i_value < p_config->min.i)
        i_value = p_config->min.i;
    if (i_value > p_config->max.i)
        i_value = p_config->max.i;

    vlc_rwlock_wrlock (&config_lock);
    p_config->value.i = i_value;
    config_dirty = true;
    vlc_rwlock_unlock (&config_lock);
}

#undef config_PutFloat
/*****************************************************************************
 * config_PutFloat: set the value of a float variable
 *****************************************************************************
 * This function is used to set the value of variables which are internally
 * represented by a float (CONFIG_ITEM_FLOAT).
 *****************************************************************************/
void config_PutFloat( vlc_object_t *p_this,
                      const char *psz_name, float f_value )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Warn( p_this, "option %s does not exist", psz_name );
        return;
    }

    if (!IsConfigFloatType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to a float", psz_name );
        return;
    }

    /* if f_min == f_max == 0, then do not use them */
    if ((p_config->min.f == 0) && (p_config->max.f == 0))
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

/**
 * Determines a list of suggested values for an integer configuration item.
 * \param values pointer to a table of integer values [OUT]
 * \param texts pointer to a table of descriptions strings [OUT]
 * \return number of choices, or -1 on error
 * \note the caller is responsible for calling free() on all descriptions and
 * on both tables. In case of error, both pointers are set to NULL.
 */
ssize_t config_GetIntChoices (vlc_object_t *obj, const char *name,
                             int64_t **restrict values, char ***restrict texts)
{
    *values = NULL;
    *texts = NULL;

    module_config_t *cfg = config_FindConfig (obj, name);
    if (cfg == NULL)
    {
        msg_Warn (obj, "option %s does not exist", name);
        errno = ENOENT;
        return -1;
    }

    size_t count = cfg->list_count;
    if (count == 0)
    {
        if (cfg->list.i_cb == NULL)
            return 0;
        return cfg->list.i_cb(obj, name, values, texts);
    }

    int64_t *vals = xmalloc (sizeof (*vals) * count);
    char **txts = xmalloc (sizeof (*txts) * count);

    for (size_t i = 0; i < count; i++)
    {
        vals[i] = cfg->list.i[i];
        /* FIXME: use module_gettext() instead */
        txts[i] = strdup ((cfg->list_text[i] != NULL)
                                       ? vlc_gettext (cfg->list_text[i]) : "");
        if (unlikely(txts[i] == NULL))
            abort ();
    }

    *values = vals;
    *texts = txts;
    return count;
}


static ssize_t config_ListModules (const char *cap, char ***restrict values,
                                   char ***restrict texts)
{
    module_t **list;
    ssize_t n = module_list_cap (&list, cap);
    if (n <= 0)
    {
        *values = *texts = NULL;
        return n;
    }

    char **vals = xmalloc ((n + 2) * sizeof (*vals));
    char **txts = xmalloc ((n + 2) * sizeof (*txts));

    vals[0] = xstrdup ("any");
    txts[0] = xstrdup (_("Automatic"));

    for (ssize_t i = 0; i < n; i++)
    {
        vals[i + 1] = xstrdup (module_get_object (list[i]));
        txts[i + 1] = xstrdup (module_gettext (list[i],
                               module_get_name (list[i], true)));
    }

    vals[n + 1] = xstrdup ("none");
    txts[n + 1] = xstrdup (_("Disable"));

    *values = vals;
    *texts = txts;
    return n + 2;
}

/**
 * Determines a list of suggested values for a string configuration item.
 * \param values pointer to a table of value strings [OUT]
 * \param texts pointer to a table of descriptions strings [OUT]
 * \return number of choices, or -1 on error
 * \note the caller is responsible for calling free() on all values, on all
 * descriptions and on both tables.
 * In case of error, both pointers are set to NULL.
 */
ssize_t config_GetPszChoices (vlc_object_t *obj, const char *name,
                              char ***restrict values, char ***restrict texts)
{
    *values = *texts = NULL;

    module_config_t *cfg = config_FindConfig (obj, name);
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
        if (cfg->list.psz_cb == NULL)
            return 0;
        return cfg->list.psz_cb(obj, name, values, texts);
    }

    char **vals = xmalloc (sizeof (*vals) * count);
    char **txts = xmalloc (sizeof (*txts) * count);

    for (size_t i = 0; i < count; i++)
    {
        vals[i] = xstrdup ((cfg->list.psz[i] != NULL) ? cfg->list.psz[i] : "");
        /* FIXME: use module_gettext() instead */
        txts[i] = xstrdup ((cfg->list_text[i] != NULL)
                                       ? vlc_gettext (cfg->list_text[i]) : "");
    }

    *values = vals;
    *texts = txts;
    return count;
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
    size_t nmod, nconf = 0;
    module_t **mlist = module_list_get (&nmod);

    for (size_t i = 0; i < nmod; i++)
         nconf  += mlist[i]->confsize;

    module_config_t **clist = malloc (sizeof (*clist) * nconf);
    if (unlikely(clist == NULL))
    {
        module_list_free (mlist);
        return VLC_ENOMEM;
    }

    nconf = 0;
    for (size_t i = 0; i < nmod; i++)
    {
        module_t *parser = mlist[i];
        module_config_t *item, *end;

        for (item = parser->p_config, end = item + parser->confsize;
             item < end;
             item++)
        {
            if (!CONFIG_ITEM(item->i_type))
                continue; /* ignore hints */
            clist[nconf++] = item;
        }
    }
    module_list_free (mlist);

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

/*****************************************************************************
 * config_FindConfig: find the config structure associated with an option.
 *****************************************************************************
 * FIXME: remove p_this pointer parameter (or use it)
 *****************************************************************************/
module_config_t *config_FindConfig (vlc_object_t *p_this, const char *name)
{
    VLC_UNUSED(p_this);

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
void config_Free (module_config_t *config, size_t confsize)
{
    for (size_t j = 0; j < confsize; j++)
    {
        module_config_t *p_item = config + j;

        free( p_item->psz_type );
        free( p_item->psz_name );
        free( p_item->psz_text );
        free( p_item->psz_longtext );

        if (IsConfigIntegerType (p_item->i_type))
        {
            if (p_item->list_count)
                free (p_item->list.i);
        }
        else
        if (IsConfigStringType (p_item->i_type))
        {
            free (p_item->value.psz);
            free (p_item->orig.psz);
            if (p_item->list_count)
            {
                for (size_t i = 0; i < p_item->list_count; i++)
                    free (p_item->list.psz[i]);
                free (p_item->list.psz);
            }
        }

        for (size_t i = 0; i < p_item->list_count; i++)
                free (p_item->list_text[i]);
        free (p_item->list_text);
    }

    free (config);
}

#undef config_ResetAll
/*****************************************************************************
 * config_ResetAll: reset the configuration data for all the modules.
 *****************************************************************************/
void config_ResetAll( vlc_object_t *p_this )
{
    size_t count;
    module_t **list = module_list_get (&count);

    vlc_rwlock_wrlock (&config_lock);
    for (size_t j = 0; j < count; j++)
    {
        module_t *p_module = list[j];

        for (size_t i = 0; i < p_module->confsize; i++ )
        {
            module_config_t *p_config = p_module->p_config + i;

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

    module_list_free (list);
    VLC_UNUSED(p_this);
}
