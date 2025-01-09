/*****************************************************************************
 * emscripten.c: Web browser power management inhibition
 *****************************************************************************
 * Copyright (c) 2025-2026 Videolabs
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
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
#include <config.h>
#endif

#include <assert.h>
#include <stdint.h>

#include <vlc_common.h>

#include <vlc_inhibit.h>
#include <vlc_plugin.h>

#include <emscripten.h>

struct vlc_inhibit_sys
{
    uint32_t id;
};

static void Inhibit(vlc_inhibit_t *ih, unsigned flags)
{
    vlc_inhibit_sys_t *sys = ih->p_sys;

    if (flags == VLC_INHIBIT_AUDIO)
        return;

    const bool inhibit = (flags == VLC_INHIBIT_VIDEO);
    if (inhibit)
    {
        MAIN_THREAD_EM_ASM({
            const index = $0;
            navigator.wakeLock.request("screen")
                .then(lock => { Module.wakeLocks[index] = lock; })
                .catch(err => {
                    console.error(`WakeLocks are unavailable: ${err.message}`);
                    Module.wakeLocks[index] = undefined;
                });
        }, sys->id);
    }
    else
    {
        MAIN_THREAD_EM_ASM({
            const index = $0;
            const lock = Module.wakeLocks.at(index);
            if (lock !== undefined)
                lock.release().then(() => { Module.wakeLocks[index] = undefined; });
            else
                console.error(`WakeLock ${index} invalid`);
        }, sys->id);
    }
}

static int Open(vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;

    vlc_inhibit_sys_t *sys = malloc(sizeof(*sys));
    if (unlikely(sys == NULL))
        return (-ENOMEM);

    sys->id = MAIN_THREAD_EM_ASM_INT({
        if (Module.wakeLocks === undefined)
            Module.wakeLocks = new Array();

        const id = Module.wakeLocks.length;
        Module.wakeLocks.push(undefined);
        return id;
    });

    ih->inhibit = Inhibit;
    ih->p_sys = sys;

    return 0;
}

static void Close(vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    free(ih->p_sys);
}

vlc_module_begin()
    set_shortname(N_("Emscripten screensaver"))
    set_description(N_("Web browser screen saver inhibition"))
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_capability("inhibit", 10)
    set_callbacks(Open, Close)
vlc_module_end()
