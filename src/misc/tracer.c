/*****************************************************************************
 * tracer.c: tracing interface
 * This library provides an interface to the traces to be used by other
 * modules. See vlc_config.h for output configuration.
 *****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_tracer.h>
#include "../libvlc.h"

struct vlc_tracer {
    const struct vlc_tracer_operations *ops;
};

/**
 * Module-based message trace.
 */
struct vlc_tracer_module {
    struct vlc_object_t obj;
    struct vlc_tracer tracer;
    void *opaque;
};

void vlc_tracer_TraceWithTs(struct vlc_tracer *tracer, vlc_tick_t ts, ...)
{
    assert(tracer->ops->trace != NULL);
    struct vlc_tracer_module *module =
            container_of(tracer, struct vlc_tracer_module, tracer);

    /* Pass message to the callback */
    va_list entries;
    va_start(entries, ts);
    tracer->ops->trace(module->opaque, ts, entries);
    va_end(entries);
}

static int vlc_tracer_load(void *func, bool forced, va_list ap)
{
    const struct vlc_tracer_operations *(*activate)(vlc_object_t *,
                                                    void **) = func;
    struct vlc_tracer_module *module = va_arg(ap, struct vlc_tracer_module *);

    (void) forced;
    module->tracer.ops = activate(VLC_OBJECT(module), &module->opaque);
    return (module->tracer.ops != NULL) ? VLC_SUCCESS : VLC_EGENERIC;
}

static struct vlc_tracer *vlc_TraceModuleCreate(vlc_object_t *parent)
{
    struct vlc_tracer_module *module;

    module = vlc_custom_create(parent, sizeof (*module), "tracer");
    if (unlikely(module == NULL))
        return NULL;

    char *module_name = var_InheritString(parent, "tracer");
    if (vlc_module_load(VLC_OBJECT(module), "tracer", module_name, false,
                        vlc_tracer_load, module) == NULL) {
        vlc_object_delete(VLC_OBJECT(module));
        free(module_name);
        return NULL;
    }
    free(module_name);

    return &module->tracer;
}

/**
 * Initializes the messages tracing system */
void vlc_tracer_Init(libvlc_int_t *vlc)
{
    struct vlc_tracer *tracer = vlc_TraceModuleCreate(VLC_OBJECT(vlc));
    libvlc_priv_t *vlc_priv = libvlc_priv(vlc);
    vlc_priv->tracer = tracer;
}

void vlc_tracer_Destroy(libvlc_int_t *vlc)
{
    libvlc_priv_t *vlc_priv = libvlc_priv(vlc);

    if (vlc_priv->tracer == NULL)
        return;

    struct vlc_tracer_module *module =
        container_of(vlc_priv->tracer, struct vlc_tracer_module, tracer);

    if (module->tracer.ops->destroy != NULL)
        module->tracer.ops->destroy(module->opaque);

    vlc_object_delete(VLC_OBJECT(module));
}
}
