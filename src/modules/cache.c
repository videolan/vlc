/*****************************************************************************
 * cache.c: Plugins cache
 *****************************************************************************
 * Copyright (C) 2001-2007 VLC authors and VideoLAN
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Ethan C. Baldridge <BaldridgeE@cadmus.com>
 *          Hans-Peter Jansen <hpj@urpla.net>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <stdalign.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_block.h>
#include "libvlc.h"

#include <vlc_plugin.h>
#include <errno.h>

#include "config/configuration.h"

#include <vlc_fs.h>

#include "modules/modules.h"


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef HAVE_DYNAMIC_PLUGINS
/* Sub-version number
 * (only used to avoid breakage in dev version when cache structure changes) */
#define CACHE_SUBVERSION_NUM 36

/* Cache filename */
#define CACHE_NAME "plugins.dat"
/* Magic for the cache filename */
#define CACHE_STRING "cache "PACKAGE_NAME" "PACKAGE_VERSION


static int vlc_cache_load_immediate(void *out, block_t *in, size_t size)
{
    if (in->i_buffer < size)
        return -1;

    memcpy(out, in->p_buffer, size);
    in->p_buffer += size;
    in->i_buffer -= size;
    return 0;
}

static int vlc_cache_load_bool(bool *out, block_t *in)
{
    unsigned char b;

    if (vlc_cache_load_immediate(&b, in, 1) || b > 1)
        return -1;

    *out = b;
    return 0;
}

static int vlc_cache_load_array(const void **p, size_t size, size_t n,
                                block_t *file)
{
    if (n == 0)
    {
        *p = NULL;
        return 0;
    }

    if (unlikely(size * n < size))
        return -1;

    size *= n;

    if (file->i_buffer < size)
        return -1;

    *p = file->p_buffer;
    file->p_buffer += size;
    file->i_buffer -= size;
    return 0;
}

static int vlc_cache_load_string(const char **restrict p, block_t *file)
{
    uint16_t size;

    if (vlc_cache_load_immediate(&size, file, sizeof (size)) || size > 16384)
        return -1;

    if (size == 0)
    {
        *p = NULL;
        return 0;
    }

    const char *str = (char *)file->p_buffer;

    if (file->i_buffer < size || str[size - 1] != '\0')
        return -1;

    file->p_buffer += size;
    file->i_buffer -= size;
    *p = str;
    return 0;
}

static int vlc_cache_load_align(size_t align, block_t *file)
{
    assert(align > 0);

    size_t skip = (-(uintptr_t)file->p_buffer) % align;
    if (skip == 0)
        return 0;

    assert(skip < align);

    if (file->i_buffer < skip)
        return -1;

    file->p_buffer += skip;
    file->i_buffer -= skip;
    assert((((uintptr_t)file->p_buffer) % align) == 0);
    return 0;
}

#define LOAD_IMMEDIATE(a) \
    if (vlc_cache_load_immediate(&(a), file, sizeof (a))) \
        goto error
#define LOAD_FLAG(a) \
    do \
    { \
        bool b; \
        if (vlc_cache_load_bool(&b, file)) \
            goto error; \
        (a) = b; \
    } while (0)
#define LOAD_ARRAY(a,n) \
    do \
    { \
        const void *base; \
        if (vlc_cache_load_array(&base, sizeof (*(a)), (n), file)) \
            goto error; \
        (a) = base; \
    } while (0)
#define LOAD_STRING(a) \
    if (vlc_cache_load_string(&(a), file)) \
        goto error
#define LOAD_ALIGNOF(t) \
    if (vlc_cache_load_align(alignof(t), file)) \
        goto error

static int vlc_cache_load_config(module_config_t *cfg, block_t *file)
{
    LOAD_IMMEDIATE (cfg->i_type);
    LOAD_IMMEDIATE (cfg->i_short);
    LOAD_FLAG (cfg->b_internal);
    LOAD_FLAG (cfg->b_unsaveable);
    LOAD_FLAG (cfg->b_safe);
    LOAD_FLAG (cfg->b_removed);
    LOAD_STRING (cfg->psz_type);
    LOAD_STRING (cfg->psz_name);
    LOAD_STRING (cfg->psz_text);
    LOAD_STRING (cfg->psz_longtext);
    LOAD_IMMEDIATE (cfg->list_count);

    if (IsConfigStringType (cfg->i_type))
    {
        const char *psz;
        LOAD_STRING(psz);
        cfg->orig.psz = (char *)psz;
        cfg->value.psz = (psz != NULL) ? strdup (cfg->orig.psz) : NULL;

        if (cfg->list_count)
            cfg->list.psz = xmalloc (cfg->list_count * sizeof (char *));
        for (unsigned i = 0; i < cfg->list_count; i++)
        {
            LOAD_STRING (cfg->list.psz[i]);
            if (cfg->list.psz[i] == NULL /* NULL -> empty string */
             && (cfg->list.psz[i] = calloc (1, 1)) == NULL)
                goto error;
        }
    }
    else
    {
        LOAD_IMMEDIATE (cfg->orig);
        LOAD_IMMEDIATE (cfg->min);
        LOAD_IMMEDIATE (cfg->max);
        cfg->value = cfg->orig;

        if (cfg->list_count)
        {
            LOAD_ALIGNOF(*cfg->list.i);
        }

        LOAD_ARRAY(cfg->list.i, cfg->list_count);
    }

    cfg->list_text = xmalloc (cfg->list_count * sizeof (char *));
    for (unsigned i = 0; i < cfg->list_count; i++)
    {
        LOAD_STRING (cfg->list_text[i]);
        if (cfg->list_text[i] == NULL /* NULL -> empty string */
         && (cfg->list_text[i] = calloc (1, 1)) == NULL)
            goto error;
    }

    return 0;
error:
    return -1; /* FIXME: leaks */
}

static int vlc_cache_load_plugin_config(vlc_plugin_t *plugin, block_t *file)
{
    uint16_t lines;

    /* Calculate the structure length */
    LOAD_IMMEDIATE (lines);

    /* Allocate memory */
    if (lines)
    {
        plugin->conf.items = calloc(sizeof (module_config_t), lines);
        if (unlikely(plugin->conf.items == NULL))
        {
            plugin->conf.size = 0;
            return -1;
        }
    }
    else
        plugin->conf.items = NULL;

    plugin->conf.size = lines;

    /* Do the duplication job */
    for (size_t i = 0; i < lines; i++)
    {
        module_config_t *item = plugin->conf.items + i;

        if (vlc_cache_load_config(item, file))
            return -1;

        if (CONFIG_ITEM(item->i_type))
        {
            plugin->conf.count++;
            if (item->i_type == CONFIG_ITEM_BOOL)
                plugin->conf.booleans++;
        }
        item->owner = plugin;
    }

    return 0;
error:
    return -1; /* FIXME: leaks */
}

static int vlc_cache_load_module(vlc_plugin_t *plugin, block_t *file)
{
    module_t *module = vlc_module_create(plugin);
    if (unlikely(module == NULL))
        return -1;

    LOAD_STRING(module->psz_shortname);
    LOAD_STRING(module->psz_longname);
    LOAD_STRING(module->psz_help);

    LOAD_IMMEDIATE(module->i_shortcuts);
    if (module->i_shortcuts > MODULE_SHORTCUT_MAX)
        goto error;
    else
    {
        module->pp_shortcuts =
            xmalloc (sizeof (*module->pp_shortcuts) * module->i_shortcuts);
        for (unsigned j = 0; j < module->i_shortcuts; j++)
            LOAD_STRING(module->pp_shortcuts[j]);
    }

    LOAD_STRING(module->activate_name);
    LOAD_STRING(module->deactivate_name);
    LOAD_STRING(module->psz_capability);
    LOAD_IMMEDIATE(module->i_score);
    return 0;
error:
    return -1;
}

static vlc_plugin_t *vlc_cache_load_plugin(block_t *file)
{
    vlc_plugin_t *plugin = vlc_plugin_create();
    if (unlikely(plugin == NULL))
        return NULL;

    uint32_t modules;
    LOAD_IMMEDIATE(modules);

    for (size_t i = 0; i < modules; i++)
        if (vlc_cache_load_module(plugin, file))
            goto error;

    if (vlc_cache_load_plugin_config(plugin, file))
        goto error;

    LOAD_STRING(plugin->textdomain);

    const char *path;
    LOAD_STRING(path);
    if (path == NULL)
        goto error;

    plugin->path = strdup(path);
    if (unlikely(plugin->path == NULL))
        goto error;

    LOAD_FLAG(plugin->unloadable);
    LOAD_IMMEDIATE(plugin->mtime);
    LOAD_IMMEDIATE(plugin->size);

    if (plugin->textdomain != NULL)
        vlc_bindtextdomain(plugin->textdomain);

    return plugin;

error:
    vlc_plugin_destroy(plugin);
    return NULL;
}

/**
 * Loads a plugins cache file.
 *
 * This function will load the plugin cache if present and valid. This cache
 * will in turn be queried by AllocateAllPlugins() to see if it needs to
 * actually load the dynamically loadable module.
 * This allows us to only fully load plugins when they are actually used.
 */
vlc_plugin_t *vlc_cache_load(vlc_object_t *p_this, const char *dir,
                             block_t **backingp)
{
    char *psz_filename;

    assert( dir != NULL );

    if( asprintf( &psz_filename, "%s"DIR_SEP CACHE_NAME, dir ) == -1 )
        return NULL;

    msg_Dbg( p_this, "loading plugins cache file %s", psz_filename );

    block_t *file = block_FilePath(psz_filename, false);
    if (file == NULL)
        msg_Warn(p_this, "cannot read %s: %s", psz_filename,
                 vlc_strerror_c(errno));
    free(psz_filename);
    if (file == NULL)
        return NULL;

    /* Check the file is a plugins cache */
    char cachestr[sizeof (CACHE_STRING) - 1];

    if (vlc_cache_load_immediate(cachestr, file, sizeof (cachestr))
     || memcmp(cachestr, CACHE_STRING, sizeof (cachestr)))
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache" );
        block_Release(file);
        return NULL;
    }

#ifdef DISTRO_VERSION
    /* Check for distribution specific version */
    char distrostr[sizeof (DISTRO_VERSION) - 1];

    if (vlc_cache_load_immediate(distrostr, file, sizeof (distrostr))
     || memcmp(distrostr, DISTRO_VERSION, sizeof (distrostr)))
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache" );
        block_Release(file);
        return NULL;
    }
#endif

    /* Check sub-version number */
    uint32_t marker;

    if (vlc_cache_load_immediate(&marker, file, sizeof (marker))
     || marker != CACHE_SUBVERSION_NUM)
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache "
                  "(corrupted header)" );
        block_Release(file);
        return NULL;
    }

    /* Check header marker */
    if (vlc_cache_load_immediate(&marker, file, sizeof (marker))
#ifdef DISTRO_VERSION
     || marker != (sizeof (cachestr) + sizeof (distrostr) + sizeof (marker))
#else
     || marker != (sizeof (cachestr) + sizeof (marker))
#endif
        )
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache "
                  "(corrupted header)" );
        block_Release(file);
        return NULL;
    }

    vlc_plugin_t *cache = NULL;

    while (file->i_buffer > 0)
    {
        vlc_plugin_t *plugin = vlc_cache_load_plugin(file);
        if (plugin == NULL)
            goto error;

        if (unlikely(asprintf(&plugin->abspath, "%s" DIR_SEP "%s", dir,
                              plugin->path) == -1))
        {
            plugin->abspath = NULL;
            vlc_plugin_destroy(plugin);
            goto error;
        }

        plugin->next = cache;
        cache = plugin;
    }

    file->p_next = *backingp;
    *backingp = file;
    return cache;

error:
    msg_Warn( p_this, "plugins cache not loaded (corrupted)" );

    /* TODO: cleanup */
    block_Release(file);
    return NULL;
}

#define SAVE_IMMEDIATE( a ) \
    if (fwrite (&(a), sizeof(a), 1, file) != 1) \
        goto error
#define SAVE_FLAG(a) \
    do { \
        char b = (a); \
        SAVE_IMMEDIATE(b); \
    } while (0)

static int CacheSaveString (FILE *file, const char *str)
{
    uint16_t size = (str != NULL) ? (strlen (str) + 1) : 0;

    SAVE_IMMEDIATE (size);
    if (size != 0 && fwrite(str, 1, size, file) != size)
    {
error:
        return -1;
    }
    return 0;
}

#define SAVE_STRING( a ) \
    if (CacheSaveString (file, (a))) \
        goto error

static int CacheSaveAlign(FILE *file, size_t align)
{
    assert(align > 0);

    size_t skip = (-ftell(file)) % align;
    if (skip == 0)
        return 0;

    assert(((ftell(file) + skip) % align) == 0);
    return fseek(file, skip, SEEK_CUR);
}

#define SAVE_ALIGNOF(t) \
    if (CacheSaveAlign(file, alignof (t))) \
        goto error

static int CacheSaveConfig (FILE *file, const module_config_t *cfg)
{
    SAVE_IMMEDIATE (cfg->i_type);
    SAVE_IMMEDIATE (cfg->i_short);
    SAVE_FLAG (cfg->b_internal);
    SAVE_FLAG (cfg->b_unsaveable);
    SAVE_FLAG (cfg->b_safe);
    SAVE_FLAG (cfg->b_removed);
    SAVE_STRING (cfg->psz_type);
    SAVE_STRING (cfg->psz_name);
    SAVE_STRING (cfg->psz_text);
    SAVE_STRING (cfg->psz_longtext);
    SAVE_IMMEDIATE (cfg->list_count);

    if (IsConfigStringType (cfg->i_type))
    {
        SAVE_STRING (cfg->orig.psz);

        for (unsigned i = 0; i < cfg->list_count; i++)
            SAVE_STRING (cfg->list.psz[i]);
    }
    else
    {
        SAVE_IMMEDIATE (cfg->orig);
        SAVE_IMMEDIATE (cfg->min);
        SAVE_IMMEDIATE (cfg->max);

        if (cfg->list_count > 0)
        {
            SAVE_ALIGNOF(*cfg->list.i);
        }

        for (unsigned i = 0; i < cfg->list_count; i++)
             SAVE_IMMEDIATE (cfg->list.i[i]);
    }
    for (unsigned i = 0; i < cfg->list_count; i++)
        SAVE_STRING (cfg->list_text[i]);

    return 0;
error:
    return -1;
}

static int CacheSaveModuleConfig(FILE *file, const vlc_plugin_t *plugin)
{
    uint16_t lines = plugin->conf.size;

    SAVE_IMMEDIATE (lines);

    for (size_t i = 0; i < lines; i++)
        if (CacheSaveConfig(file, plugin->conf.items + i))
           goto error;

    return 0;
error:
    return -1;
}

static int CacheSaveModule(FILE *file, const module_t *module)
{
    SAVE_STRING(module->psz_shortname);
    SAVE_STRING(module->psz_longname);
    SAVE_STRING(module->psz_help);
    SAVE_IMMEDIATE(module->i_shortcuts);

    for (size_t j = 0; j < module->i_shortcuts; j++)
         SAVE_STRING(module->pp_shortcuts[j]);

    SAVE_STRING(module->activate_name);
    SAVE_STRING(module->deactivate_name);
    SAVE_STRING(module->psz_capability);
    SAVE_IMMEDIATE(module->i_score);
    return 0;
error:
    return -1;
}

static int CacheSaveBank(FILE *file, vlc_plugin_t *const *cache, size_t n)
{
    uint32_t i_file_size = 0;

    /* Contains version number */
    if (fputs (CACHE_STRING, file) == EOF)
        goto error;
#ifdef DISTRO_VERSION
    /* Allow binary maintaner to pass a string to detect new binary version*/
    if (fputs( DISTRO_VERSION, file ) == EOF)
        goto error;
#endif
    /* Sub-version number (to avoid breakage in the dev version when cache
     * structure changes) */
    i_file_size = CACHE_SUBVERSION_NUM;
    if (fwrite (&i_file_size, sizeof (i_file_size), 1, file) != 1 )
        goto error;

    /* Header marker */
    i_file_size = ftell( file );
    if (fwrite (&i_file_size, sizeof (i_file_size), 1, file) != 1)
        goto error;

    for (size_t i = 0; i < n; i++)
    {
        const vlc_plugin_t *plugin = cache[i];
        uint32_t count = plugin->modules_count;

        SAVE_IMMEDIATE(count);

        for (module_t *module = plugin->module;
             module != NULL;
             module = module->next)
            if (CacheSaveModule(file, module))
                goto error;

        /* Config stuff */
        if (CacheSaveModuleConfig(file, plugin))
            goto error;

        /* Save common info */
        SAVE_STRING(plugin->textdomain);
        SAVE_STRING(plugin->path);
        SAVE_FLAG(plugin->unloadable);
        SAVE_IMMEDIATE(plugin->mtime);
        SAVE_IMMEDIATE(plugin->size);
    }

    if (fflush (file)) /* flush libc buffers */
        goto error;
    return 0; /* success! */

error:
    return -1;
}

/**
 * Saves a module cache to disk, and release cache data from memory.
 */
void CacheSave(vlc_object_t *p_this, const char *dir,
               vlc_plugin_t *const *entries, size_t n)
{
    char *filename = NULL, *tmpname = NULL;

    if (asprintf (&filename, "%s"DIR_SEP CACHE_NAME, dir ) == -1)
        goto out;

    if (asprintf (&tmpname, "%s.%"PRIu32, filename, (uint32_t)getpid ()) == -1)
        goto out;
    msg_Dbg (p_this, "saving plugins cache %s", filename);

    FILE *file = vlc_fopen (tmpname, "wb");
    if (file == NULL)
    {
        if (errno != EACCES && errno != ENOENT)
            msg_Warn (p_this, "cannot create %s: %s", tmpname,
                      vlc_strerror_c(errno));
        goto out;
    }

    if (CacheSaveBank(file, entries, n))
    {
        msg_Warn (p_this, "cannot write %s: %s", tmpname,
                  vlc_strerror_c(errno));
        clearerr (file);
        fclose (file);
        vlc_unlink (tmpname);
        goto out;
    }

#if !defined( _WIN32 ) && !defined( __OS2__ )
    vlc_rename (tmpname, filename); /* atomically replace old cache */
    fclose (file);
#else
    vlc_unlink (filename);
    fclose (file);
    vlc_rename (tmpname, filename);
#endif
out:
    free (filename);
    free (tmpname);
}

/**
 * Looks up a plugin file in a table of cached plugins.
 */
vlc_plugin_t *vlc_cache_lookup(vlc_plugin_t **cache, const char *path)
{
    vlc_plugin_t **pp = cache, *plugin;

    while ((plugin = *pp) != NULL)
    {
        if (plugin->path != NULL && !strcmp(plugin->path, path))
        {
            *pp = plugin->next;
            plugin->next = NULL;
            return plugin;
        }

        pp = &plugin->next;
    }

    return NULL;
}
#endif /* HAVE_DYNAMIC_PLUGINS */
