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
#include <vlc_interface.h>
#include "../libvlc.h"
#include "rcu.h"

struct vlc_tracer {
    void *opaque;
    const struct vlc_tracer_operations *ops;
};

/**
 * Module-based message trace.
 */
struct vlc_tracer_module {
    struct vlc_object_t obj;
    struct vlc_tracer tracer;

    /* Module parameters */
    void *opaque;
    const struct vlc_tracer_operations *ops;
};

static void vlc_tracer_module_Trace(void *opaque, vlc_tick_t ts,
                                    va_list entries)
{
    struct vlc_tracer_module *module = opaque;
    module->ops->trace(module->opaque, ts, entries);
}

static void vlc_tracer_module_Destroy(void *opaque)
{
    struct vlc_tracer_module *module = opaque;

    if (module->ops->destroy != NULL)
        module->ops->destroy(module->opaque);

    vlc_object_delete(VLC_OBJECT(module));
}

void vlc_tracer_TraceWithTs(struct vlc_tracer *tracer, vlc_tick_t ts, ...)
{
    assert(tracer->ops->trace != NULL);


    /* Pass message to the callback */
    va_list entries;
    va_start(entries, ts);
    tracer->ops->trace(tracer->opaque, ts, entries);
    va_end(entries);
}

static int vlc_tracer_load(void *func, bool forced, va_list ap)
{
    const struct vlc_tracer_operations *(*activate)(vlc_object_t *,
                                                    void **) = func;
    struct vlc_tracer_module *module = va_arg(ap, struct vlc_tracer_module *);

    (void) forced;
    module->ops = activate(VLC_OBJECT(module), &module->opaque);
    return (module->ops != NULL) ? VLC_SUCCESS : VLC_EGENERIC;
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

    static const struct vlc_tracer_operations vlc_tracer_module_ops =
    {
        .trace = vlc_tracer_module_Trace,
        .destroy = vlc_tracer_module_Destroy,
    };
    module->tracer.opaque = module;
    module->tracer.ops = &vlc_tracer_module_ops;

    return &module->tracer;
}

/* No-op tracer */
static void vlc_tracer_discard_Trace(void *opaque, vlc_tick_t ts,
                                     va_list entries)
    { (void)opaque; (void)ts; (void)entries; }

static const struct vlc_tracer_operations discard_tracer_ops =
    { .trace = vlc_tracer_discard_Trace };

static struct vlc_tracer discard_tracer =/* No-op tracer */
    { .ops = &discard_tracer_ops };

/**
 * Switchable tracer.
 *
 * A tracer that can be redirected live.
 */
struct vlc_tracer_switch {
    struct vlc_tracer *_Atomic backend;
    struct vlc_tracer frontend;
    vlc_rwlock_t lock;
};
static const struct vlc_tracer_operations switch_ops;

static void vlc_TraceSwitch(struct vlc_tracer *tracer,
                            struct vlc_tracer *new_tracer)
{
    struct vlc_tracer_switch *traceswitch =
        container_of(tracer, struct vlc_tracer_switch, frontend);
    struct vlc_tracer *old_tracer;

    assert(tracer->ops == &switch_ops);

    if (new_tracer == NULL)
        new_tracer = &discard_tracer;

    old_tracer = atomic_exchange_explicit(&traceswitch->backend, new_tracer,
                                          memory_order_acq_rel);
    vlc_rcu_synchronize();

    if (old_tracer == new_tracer || old_tracer == NULL)
        return;

    if (old_tracer->ops->destroy != NULL)
        old_tracer->ops->destroy(old_tracer->opaque);
}

static void vlc_tracer_switch_vaTrace(void *opaque, vlc_tick_t ts,
                                      va_list entries)
{
    struct vlc_tracer *tracer = opaque;
    assert(tracer->ops->trace == &vlc_tracer_switch_vaTrace);

    struct vlc_tracer_switch *traceswitch =
        container_of(tracer, struct vlc_tracer_switch, frontend);

    vlc_rcu_read_lock();
    struct vlc_tracer *backend =
        atomic_load_explicit(&traceswitch->backend, memory_order_acquire);
    backend->ops->trace(backend->opaque, ts, entries);
    vlc_rcu_read_unlock();
}

static void vlc_tracer_switch_Destroy(void *opaque)
{
    struct vlc_tracer *tracer = opaque;
    assert(tracer->ops->destroy == &vlc_tracer_switch_Destroy);

    vlc_TraceSwitch(tracer, &discard_tracer);

    struct vlc_tracer_switch *traceswitch =
        container_of(tracer, struct vlc_tracer_switch, frontend);
    free(traceswitch);
}

static const struct vlc_tracer_operations switch_ops = {
    vlc_tracer_switch_vaTrace,
    vlc_tracer_switch_Destroy,
};

static struct vlc_tracer *vlc_tracer_switch_Create()
{
    struct vlc_tracer_switch *traceswitch = malloc(sizeof *traceswitch);
    if (unlikely(traceswitch == NULL))
        return NULL;

    vlc_rwlock_init(&traceswitch->lock);
    traceswitch->frontend.ops = &switch_ops;
    traceswitch->frontend.opaque = &traceswitch->frontend;
    atomic_init(&traceswitch->backend, &discard_tracer);
    return &traceswitch->frontend;
}

/**
 * Initializes the messages tracing system */
int vlc_tracer_Init(libvlc_int_t *vlc)
{
    libvlc_priv_t *vlc_priv = libvlc_priv(vlc);

    struct vlc_tracer *tracerswitch = vlc_tracer_switch_Create();
    if (unlikely(tracerswitch == NULL))
        return -1;
    vlc_priv->tracer = tracerswitch;

    /* Pre-allocate the libvlc tracer to avoid allocating it when enabling. */
    vlc_priv->libvlc_tracer = malloc(sizeof *vlc_priv->libvlc_tracer);
    if (vlc_priv->libvlc_tracer == NULL)
        return VLC_EGENERIC;

    struct vlc_tracer *tracer = vlc_TraceModuleCreate(VLC_OBJECT(vlc));
    vlc_TraceSwitch(tracerswitch, tracer);
    return VLC_SUCCESS;
}

void vlc_tracer_Destroy(libvlc_int_t *vlc)
{
    libvlc_priv_t *vlc_priv = libvlc_priv(vlc);

    struct vlc_tracer *tracer = vlc_priv->tracer;
    if (tracer != NULL && tracer->ops->destroy != NULL)
        tracer->ops->destroy(tracer->opaque);

    free(vlc_priv->libvlc_tracer);
}

void vlc_TraceSet(libvlc_int_t *vlc, const struct vlc_tracer_operations *ops,
                  void *opaque)
{
    libvlc_priv_t *vlc_priv = libvlc_priv(vlc);
    struct vlc_tracer *tracer = vlc_priv->tracer;

    vlc_priv->libvlc_tracer->ops = ops;
    vlc_priv->libvlc_tracer->opaque = opaque;
    vlc_TraceSwitch(tracer, vlc_priv->libvlc_tracer);
    atomic_store_explicit(&vlc_priv->tracer_enabled, ops != NULL,
                          memory_order_release);
    vlc_rcu_synchronize();
}
