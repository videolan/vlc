/*****************************************************************************
 * instance.c: libplacebo instance abstraction
 *****************************************************************************
 * Copyright (C) 2021 Niklas Haas
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
#include <vlc_configuration.h>
#include <vlc_modules.h>
#include <vlc_fs.h>

#include "instance.h"
#include "utils.h"

// 20 MB is plenty of space to cache all shaders plus several 3DLUTs, while
// still taking negligible amounts of time to parse on startup (<5ms)
#define VLC_PL_MAX_CACHE_SIZE (20 << 20)

static int vlc_placebo_start(void *func, bool forced, va_list ap)
{
    int (*activate)(vlc_placebo_t *, const vout_display_cfg_t *) = func;
    vlc_placebo_t *pl = va_arg(ap, vlc_placebo_t *);
    const vout_display_cfg_t *cfg = va_arg(ap, const vout_display_cfg_t *);

    int ret = activate(pl, cfg);
    /* TODO: vlc_objres_clear, which is not in the public API. */
    (void)forced;
    return ret;
}

#if PL_API_VER >= 320
static FILE *vlc_placebo_OpenCache(const char *mode)
{
    char *filename;
    char *cachedir = config_GetUserDir(VLC_CACHE_DIR);
    if (!cachedir)
        return NULL;
    int ret = asprintf(&filename, "%s" DIR_SEP "libplacebo", cachedir);
    free(cachedir);
    if (ret < 0)
        return NULL;
    FILE *file = vlc_fopen(filename, mode);
    free(filename);
    return file;
}
#endif

/**
 * Creates a libplacebo context, and swapchain, tied to a window
 *
 * @param cfg vout display cfg to use as the swapchain source
 * @param name module name for libplacebo GPU provider (or NULL for auto)
 * @return a new context, or NULL on failure
 */
vlc_placebo_t *vlc_placebo_Create(const vout_display_cfg_t *cfg, const char *name)
{
    vlc_object_t *parent = VLC_OBJECT(cfg->window);
    vlc_placebo_t *pl = vlc_object_create(parent, sizeof (*pl));
    if (unlikely(pl == NULL))
        return NULL;

    pl->sys = NULL;
    pl->ops = NULL;
    pl->log = vlc_placebo_CreateLog(VLC_OBJECT(pl));
    if (pl->log == NULL)
        goto delete_pl;

    module_t *module = vlc_module_load(vlc_object_logger(parent), "libplacebo gpu",
                                       name, false,
                                       vlc_placebo_start, pl, cfg);
    if (module == NULL)
        goto delete_log;

#if PL_API_VER >= 320
    pl->cache = pl_cache_create(pl_cache_params(
        .log = pl->log,
        .max_total_size = VLC_PL_MAX_CACHE_SIZE,
    ));
    assert(pl->gpu);
    pl_gpu_set_cache(pl->gpu, pl->cache);
    FILE *cache = vlc_placebo_OpenCache("rb");
    if (cache) {
        pl_cache_load_file(pl->cache, cache);
        fclose(cache);
    }
#endif

    return pl;

delete_log:
    pl_log_destroy(&pl->log);

delete_pl:
    vlc_object_delete(pl);
    return NULL;
}


void vlc_placebo_Release(vlc_placebo_t *pl)
{
    if (pl->ops)
        pl->ops->close(pl);

#if PL_API_VER >= 320
    FILE *cache = vlc_placebo_OpenCache("wb");
    if (cache) {
        pl_cache_save_file(pl->cache, cache);
        fclose(cache);
    }
    pl_cache_destroy(&pl->cache);
#endif

    pl_log_destroy(&pl->log);

    /* TODO: use vlc_objres_clear */
    vlc_object_delete(pl);
}

int vlc_placebo_MakeCurrent(vlc_placebo_t * pl)
{
    if (pl->ops->make_current)
        return pl->ops->make_current(pl);

    return VLC_SUCCESS;
}

void vlc_placebo_ReleaseCurrent(vlc_placebo_t *pl)
{
    if (pl->ops->release_current)
        pl->ops->release_current(pl);
}
