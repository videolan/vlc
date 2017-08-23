/*****************************************************************************
 * entry.c : Callbacks for module entry point
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 * Copyright © 2007-2008 Rémi Denis-Courmont
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
#include <vlc_plugin.h>
#include <vlc_memory.h>
#include <assert.h>
#include <stdarg.h>
#include <limits.h>
#include <float.h>
#ifdef HAVE_SEARCH_H
# include <search.h>
#endif

#include "modules/modules.h"
#include "config/configuration.h"
#include "libvlc.h"

module_t *vlc_module_create(vlc_plugin_t *plugin)
{
    module_t *module = malloc (sizeof (*module));
    if (module == NULL)
        return NULL;

    /* NOTE XXX: For backward compatibility with preferences UIs, the first
     * module must stay first. That defines under which module, the
     * configuration items of the plugin belong. The order of the following
     * entries is irrelevant. */
    module_t *parent = plugin->module;
    if (parent == NULL)
    {
        module->next = NULL;
        plugin->module = module;
    }
    else
    {
        module->next = parent->next;
        parent->next = module;
    }

    plugin->modules_count++;
    module->plugin = plugin;

    module->psz_shortname = NULL;
    module->psz_longname = NULL;
    module->psz_help = NULL;
    module->pp_shortcuts = NULL;
    module->i_shortcuts = 0;
    module->psz_capability = NULL;
    module->i_score = (parent != NULL) ? parent->i_score : 1;
    module->activate_name = NULL;
    module->deactivate_name = NULL;
    module->pf_activate = NULL;
    module->pf_deactivate = NULL;
    return module;
}

/**
 * Destroys a module.
 */
void vlc_module_destroy (module_t *module)
{
    while (module != NULL)
    {
        module_t *next = module->next;

        free(module->pp_shortcuts);
        free(module);
        module = next;
    }
}

vlc_plugin_t *vlc_plugin_create(void)
{
    vlc_plugin_t *plugin = malloc(sizeof (*plugin));
    if (unlikely(plugin == NULL))
        return NULL;

    plugin->modules_count = 0;
    plugin->textdomain = NULL;
    plugin->conf.items = NULL;
    plugin->conf.size = 0;
    plugin->conf.count = 0;
    plugin->conf.booleans = 0;
#ifdef HAVE_DYNAMIC_PLUGINS
    plugin->abspath = NULL;
    atomic_init(&plugin->loaded, false);
    plugin->unloadable = true;
    plugin->handle = NULL;
    plugin->abspath = NULL;
    plugin->path = NULL;
#endif
    plugin->module = NULL;

    return plugin;
}

/**
 * Destroys a plug-in.
 * @warning If the plug-in was dynamically loaded in memory, the library handle
 * and associated memory mappings and linker resources will be leaked.
 */
void vlc_plugin_destroy(vlc_plugin_t *plugin)
{
    assert(plugin != NULL);
#ifdef HAVE_DYNAMIC_PLUGINS
    assert(!plugin->unloadable || !atomic_load(&plugin->loaded));
#endif

    if (plugin->module != NULL)
        vlc_module_destroy(plugin->module);

    config_Free(plugin->conf.items, plugin->conf.size);
#ifdef HAVE_DYNAMIC_PLUGINS
    free(plugin->abspath);
    free(plugin->path);
#endif
    free(plugin);
}

static module_config_t *vlc_config_create(vlc_plugin_t *plugin, int type)
{
    unsigned confsize = plugin->conf.size;
    module_config_t *tab = plugin->conf.items;

    if ((confsize & 0xf) == 0)
    {
        tab = realloc_or_free (tab, (confsize + 17) * sizeof (*tab));
        if (tab == NULL)
            return NULL;

        plugin->conf.items = tab;
    }

    memset (tab + confsize, 0, sizeof (tab[confsize]));
    tab += confsize;
    tab->owner = plugin;

    if (IsConfigIntegerType (type))
    {
        tab->max.i = INT64_MAX;
        tab->min.i = INT64_MIN;
    }
    else if( IsConfigFloatType (type))
    {
        tab->max.f = FLT_MAX;
        tab->min.f = -FLT_MAX;
    }
    tab->i_type = type;

    if (CONFIG_ITEM(type))
    {
        plugin->conf.count++;
        if (type == CONFIG_ITEM_BOOL)
            plugin->conf.booleans++;
    }
    plugin->conf.size++;

    return tab;
}

/**
 * Plug-in descriptor callback.
 *
 * This callback populates modules, configuration items and properties of a
 * plug-in from the plug-in descriptor.
 */
static int vlc_plugin_desc_cb(void *ctx, void *tgt, int propid, ...)
{
    vlc_plugin_t *plugin = ctx;
    module_t *module = tgt;
    module_config_t *item = tgt;
    va_list ap;
    int ret = 0;

    va_start (ap, propid);
    switch (propid)
    {
        case VLC_MODULE_CREATE:
        {
            module_t *super = plugin->module;
            module_t *submodule = vlc_module_create(plugin);
            if (unlikely(submodule == NULL))
            {
                ret = -1;
                break;
            }

            *(va_arg (ap, module_t **)) = submodule;
            if (super == NULL)
                break;

            /* Inheritance. Ugly!! */
            submodule->pp_shortcuts = xmalloc (sizeof ( *submodule->pp_shortcuts ));
            submodule->pp_shortcuts[0] = super->pp_shortcuts[0];
            submodule->i_shortcuts = 1; /* object name */

            submodule->psz_shortname = super->psz_shortname;
            submodule->psz_longname = super->psz_longname;
            submodule->psz_capability = super->psz_capability;
            break;
        }

        case VLC_CONFIG_CREATE:
        {
            int type = va_arg (ap, int);
            module_config_t **pp = va_arg (ap, module_config_t **);

            item = vlc_config_create(plugin, type);
            if (unlikely(item == NULL))
            {
                ret = -1;
                break;
            }
            *pp = item;
            break;
        }

        case VLC_MODULE_SHORTCUT:
        {
            unsigned i_shortcuts = va_arg (ap, unsigned);
            unsigned index = module->i_shortcuts;
            /* The cache loader accept only a small number of shortcuts */
            assert(i_shortcuts + index <= MODULE_SHORTCUT_MAX);

            const char *const *tab = va_arg (ap, const char *const *);
            const char **pp = realloc (module->pp_shortcuts,
                                       sizeof (pp[0]) * (index + i_shortcuts));
            if (unlikely(pp == NULL))
            {
                ret = -1;
                break;
            }
            module->pp_shortcuts = pp;
            module->i_shortcuts = index + i_shortcuts;
            pp += index;
            for (unsigned i = 0; i < i_shortcuts; i++)
                pp[i] = tab[i];
            break;
        }

        case VLC_MODULE_CAPABILITY:
            module->psz_capability = va_arg (ap, const char *);
            break;

        case VLC_MODULE_SCORE:
            module->i_score = va_arg (ap, int);
            break;

        case VLC_MODULE_CB_OPEN:
            module->activate_name = va_arg(ap, const char *);
            module->pf_activate = va_arg (ap, void *);
            break;

        case VLC_MODULE_CB_CLOSE:
            module->deactivate_name = va_arg(ap, const char *);
            module->pf_deactivate = va_arg (ap, void *);
            break;

        case VLC_MODULE_NO_UNLOAD:
#ifdef HAVE_DYNAMIC_PLUGINS
            plugin->unloadable = false;
#endif
            break;

        case VLC_MODULE_NAME:
        {
            const char *value = va_arg (ap, const char *);

            assert (module->i_shortcuts == 0);
            module->pp_shortcuts = malloc( sizeof( *module->pp_shortcuts ) );
            module->pp_shortcuts[0] = value;
            module->i_shortcuts = 1;

            assert (module->psz_longname == NULL);
            module->psz_longname = value;
            break;
        }

        case VLC_MODULE_SHORTNAME:
            module->psz_shortname = va_arg (ap, const char *);
            break;

        case VLC_MODULE_DESCRIPTION:
            // TODO: do not set this in VLC_MODULE_NAME
            module->psz_longname = va_arg (ap, const char *);
            break;

        case VLC_MODULE_HELP:
            module->psz_help = va_arg (ap, const char *);
            break;

        case VLC_MODULE_TEXTDOMAIN:
            plugin->textdomain = va_arg(ap, const char *);
            break;

        case VLC_CONFIG_NAME:
        {
            const char *name = va_arg (ap, const char *);

            assert (name != NULL);
            item->psz_name = name;
            break;
        }

        case VLC_CONFIG_VALUE:
        {
            if (IsConfigIntegerType (item->i_type)
             || !CONFIG_ITEM(item->i_type))
            {
                item->orig.i =
                item->value.i = va_arg (ap, int64_t);
            }
            else
            if (IsConfigFloatType (item->i_type))
            {
                item->orig.f =
                item->value.f = va_arg (ap, double);
            }
            else
            if (IsConfigStringType (item->i_type))
            {
                const char *value = va_arg (ap, const char *);
                item->value.psz = value ? strdup (value) : NULL;
                item->orig.psz = (char *)value;
            }
            break;
        }

        case VLC_CONFIG_RANGE:
        {
            if (IsConfigFloatType (item->i_type))
            {
                item->min.f = va_arg (ap, double);
                item->max.f = va_arg (ap, double);
            }
            else
            {
                item->min.i = va_arg (ap, int64_t);
                item->max.i = va_arg (ap, int64_t);
            }
            break;
        }

        case VLC_CONFIG_ADVANCED:
            item->b_advanced = true;
            break;

        case VLC_CONFIG_VOLATILE:
            item->b_unsaveable = true;
            break;

        case VLC_CONFIG_PRIVATE:
            item->b_internal = true;
            break;

        case VLC_CONFIG_REMOVED:
            item->b_removed = true;
            break;

        case VLC_CONFIG_CAPABILITY:
            item->psz_type = va_arg (ap, const char *);
            break;

        case VLC_CONFIG_SHORTCUT:
            item->i_short = va_arg (ap, int);
            break;

        case VLC_CONFIG_SAFE:
            item->b_safe = true;
            break;

        case VLC_CONFIG_DESC:
            item->psz_text = va_arg (ap, const char *);
            item->psz_longtext = va_arg (ap, const char *);
            break;

        case VLC_CONFIG_LIST:
        {
            size_t len = va_arg (ap, size_t);

            assert (item->list_count == 0); /* cannot replace choices */
            assert (item->list.psz_cb == NULL);
            if (len == 0)
                break; /* nothing to do */
            /* Copy values */
            if (IsConfigIntegerType (item->i_type))
                item->list.i = va_arg(ap, const int *);
            else
            if (IsConfigStringType (item->i_type))
            {
                const char *const *src = va_arg (ap, const char *const *);
                const char **dst = xmalloc (sizeof (const char *) * len);

                memcpy(dst, src, sizeof (const char *) * len);
                item->list.psz = dst;
            }
            else
                break;

            /* Copy textual descriptions */
            /* XXX: item->list_text[len + 1] is probably useless. */
            const char *const *text = va_arg (ap, const char *const *);
            const char **dtext = xmalloc (sizeof (const char *) * (len + 1));

            memcpy(dtext, text, sizeof (const char *) * len);
            item->list_text = dtext;
            item->list_count = len;
            break;
        }

        case VLC_CONFIG_LIST_CB:
        {
            void *cb;

            item->list_cb_name = va_arg(ap, const char *);
            cb = va_arg(ap, void *);

            if (IsConfigIntegerType (item->i_type))
               item->list.i_cb = cb;
            else
            if (IsConfigStringType (item->i_type))
               item->list.psz_cb = cb;
            else
                break;
            break;
        }

        default:
            fprintf (stderr, "LibVLC: unknown module property %d\n", propid);
            fprintf (stderr, "LibVLC: too old to use this module?\n");
            ret = -1;
            break;
    }

    va_end (ap);
    return ret;
}

/**
 * Runs a plug-in descriptor.
 *
 * This loads the plug-in meta-data in memory.
 */
vlc_plugin_t *vlc_plugin_describe(vlc_plugin_cb entry)
{
    vlc_plugin_t *plugin = vlc_plugin_create();
    if (unlikely(plugin == NULL))
        return NULL;

    if (entry(vlc_plugin_desc_cb, plugin) != 0)
    {
        vlc_plugin_destroy(plugin); /* partially initialized plug-in... */
        plugin = NULL;
    }
    return plugin;
}

struct vlc_plugin_symbol
{
    const char *name;
    void *addr;
};

static int vlc_plugin_symbol_compare(const void *a, const void *b)
{
    const struct vlc_plugin_symbol *sa = a , *sb = b;

    return strcmp(sa->name, sb->name);
}

/**
 * Plug-in symbols callback.
 *
 * This callback generates a mapping of plugin symbol names to symbol
 * addresses.
 */
static int vlc_plugin_gpa_cb(void *ctx, void *tgt, int propid, ...)
{
    void **rootp = ctx;
    const char *name;
    void *addr;

    (void) tgt;

    switch (propid)
    {
        case VLC_MODULE_CB_OPEN:
        case VLC_MODULE_CB_CLOSE:
        case VLC_CONFIG_LIST_CB:
        {
            va_list ap;

            va_start(ap, propid);
            name = va_arg(ap, const char *);
            addr = va_arg(ap, void *);
            va_end (ap);
            break;
        }
        default:
            return 0;
    }

    struct vlc_plugin_symbol *sym = malloc(sizeof (*sym));

    sym->name = name;
    sym->addr = addr;

    struct vlc_plugin_symbol **symp = tsearch(sym, rootp,
                                              vlc_plugin_symbol_compare);
    if (unlikely(symp == NULL))
    {   /* Memory error */
        free(sym);
        return -1;
    }

    if (*symp != sym)
    {   /* Duplicate symbol */
        assert((*symp)->addr == sym->addr);
        free(sym);
    }
    return 0;
}

/**
 * Gets the symbols of a plugin.
 *
 * This function generates a list of symbol names and addresses for a given
 * plugin descriptor. The result can be used with vlc_plugin_get_symbol()
 * to resolve a symbol name to an address.
 *
 * The result must be freed with vlc_plugin_free_symbols(). The result is only
 * meaningful until the plugin is unloaded.
 */
static void *vlc_plugin_get_symbols(vlc_plugin_cb entry)
{
    void *root = NULL;

    if (entry(vlc_plugin_gpa_cb, &root))
    {
        tdestroy(root, free);
        return NULL;
    }

    return root;
}

static void vlc_plugin_free_symbols(void *root)
{
    tdestroy(root, free);
}

static int vlc_plugin_get_symbol(void *root, const char *name,
                                 void **restrict addrp)
{
    if (name == NULL)
    {   /* TODO: use this; do not define "NULL" as a name for NULL? */
        *addrp = NULL;
        return 0;
    }

    const struct vlc_plugin_symbol **symp = tfind(&name, &root,
                                                  vlc_plugin_symbol_compare);

    if (symp == NULL)
        return -1;

    *addrp = (*symp)->addr;
    return 0;
}

int vlc_plugin_resolve(vlc_plugin_t *plugin, vlc_plugin_cb entry)
{
    void *syms = vlc_plugin_get_symbols(entry);
    int ret = -1;

    /* Resolve modules activate/deactivate callbacks */
    for (module_t *module = plugin->module;
         module != NULL;
         module = module->next)
    {
        if (vlc_plugin_get_symbol(syms, module->activate_name,
                                  &module->pf_activate)
         || vlc_plugin_get_symbol(syms, module->deactivate_name,
                                  &module->pf_deactivate))
            goto error;
    }

    /* Resolve configuration callbacks */
    for (size_t i = 0; i < plugin->conf.size; i++)
    {
        module_config_t *item = plugin->conf.items + i;
        void *cb;

        if (item->list_cb_name == NULL)
            continue;
        if (vlc_plugin_get_symbol(syms, item->list_cb_name, &cb))
            goto error;

        if (IsConfigIntegerType (item->i_type))
            item->list.i_cb = cb;
        else
        if (IsConfigStringType (item->i_type))
            item->list.psz_cb = cb;
    }

    ret = 0;
error:
    vlc_plugin_free_symbols(syms);
    return ret;
}
