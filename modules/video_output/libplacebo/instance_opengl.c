/**
 * @file instance_opengl.c
 * @brief OpenGL specific libplacebo GPU wrapper
 */
/*****************************************************************************
 * Copyright © 2021 Niklas Haas
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
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_opengl.h>

#include <libplacebo/opengl.h>

#include "instance.h"
#include "utils.h"

static int InitInstance(vlc_placebo_t *pl, const vout_display_cfg_t *cfg);
static void CloseInstance(vlc_placebo_t *pl);
static int MakeCurrent(vlc_placebo_t *pl);
static void ReleaseCurrent(vlc_placebo_t *pl);

#define GL_TEXT N_("OpenGL extension")
#define GLES2_TEXT N_("OpenGL ES 2 extension")
#define PROVIDER_LONGTEXT N_( \
    "Extension through which to use the Open Graphics Library (OpenGL).")

#define ALLOWSW_TEXT "Allow software rasterizers"
#define ALLOWSW_LONGTEXT "If enabled, allow the use of OpenGL contexts detected as software rasterizers (e.g. llvmpipe, swrast)."

#define SWAP_DEPTH_TEXT "Maximum frame latency"
#define SWAP_DEPTH_LONGTEXT "Attempt limiting the maximum frame latency. The true frame latency may be lower than this setting, depending on OpenGL driver internals and the VLC clock settings."

vlc_module_begin()
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("libplacebo gpu", 40)
    set_callback(InitInstance)
#ifdef USE_OPENGL_ES2
# define API VLC_OPENGL_ES2
# define MODULE_VARNAME "pl-gles2"
    set_shortname("libplacebo OpenGL ES2")
    set_description(N_("OpenGL ES2 based GPU instance"))
    add_shortcut("pl_opengles2", "pl_gles2")
    add_module(MODULE_VARNAME, "opengl es2", "any", GLES2_TEXT, PROVIDER_LONGTEXT);
#else // !USE_OPENGL_ES2
# define API VLC_OPENGL
# define MODULE_VARNAME "pl-gl"
    set_shortname("libplacebo OpenGL")
    set_description(N_("OpenGL based GPU instance"))
    add_shortcut("pl_opengl", "pl_gl")
    add_module(MODULE_VARNAME, "opengl", "any", GL_TEXT, PROVIDER_LONGTEXT);
#endif

    set_section("Context settings", NULL)
    add_bool("gl-allow-sw", false, ALLOWSW_TEXT, ALLOWSW_LONGTEXT)
    add_integer_with_range("gl-swap-depth", 0,
            0, 4, SWAP_DEPTH_TEXT, SWAP_DEPTH_LONGTEXT)
vlc_module_end()

struct vlc_placebo_system_t {
    vlc_gl_t *gl;
    pl_opengl opengl;
};

static const struct vlc_placebo_operations instance_opts =
{
    .close = CloseInstance,
    .make_current = MakeCurrent,
    .release_current = ReleaseCurrent,
};

static int InitInstance(vlc_placebo_t *pl, const vout_display_cfg_t *cfg)
{
    vlc_placebo_system_t *sys = pl->sys =
        vlc_obj_calloc(VLC_OBJECT(pl), 1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    bool current = false;

    char *name = var_InheritString(pl, MODULE_VARNAME);
    sys->gl = vlc_gl_Create(cfg, API, name);
    free(name);
    if (!sys->gl || vlc_gl_MakeCurrent(sys->gl) != VLC_SUCCESS)
        goto error;

    current = true;

    // Create OpenGL wrapper
    sys->opengl = pl_opengl_create(pl->log, &(struct pl_opengl_params) {
        .allow_software = var_InheritBool(pl, "gl-allow-sw"),
        .debug = true, // this only sets up the debug report callback
    });
    vlc_gl_ReleaseCurrent (sys->gl);
    if (!sys->opengl)
        goto error;


    // Create swapchain for this surface
    struct pl_opengl_swapchain_params swap_params = {
        .swap_buffers = (void (*)(void *)) vlc_gl_Swap,
        .max_swapchain_depth = var_InheritInteger(pl, "gl-swap-depth"),
        .priv = sys->gl,
    };

    pl->swapchain = pl_opengl_create_swapchain(sys->opengl, &swap_params);
    if (!pl->swapchain)
        goto error;

    vlc_gl_ReleaseCurrent(sys->gl);

    pl->gpu = sys->opengl->gpu;
    pl->ops = &instance_opts;
    return VLC_SUCCESS;

error:
    if (current)
        vlc_gl_ReleaseCurrent(sys->gl);
    CloseInstance(pl);
    return VLC_EGENERIC;
}

static void CloseInstance(vlc_placebo_t *pl)
{
    vlc_placebo_system_t *sys = pl->sys;

    if (sys->gl != NULL) {
        if (vlc_gl_MakeCurrent(sys->gl) == VLC_SUCCESS) {
            pl_swapchain_destroy(&pl->swapchain);
            pl_opengl_destroy(&sys->opengl);
            vlc_gl_ReleaseCurrent(sys->gl);
        }

        vlc_gl_Delete(sys->gl);
    }

    vlc_obj_free(VLC_OBJECT(pl), sys);
    pl->sys = NULL;
}

static int MakeCurrent(vlc_placebo_t *pl)
{
    vlc_placebo_system_t *sys = pl->sys;
    return vlc_gl_MakeCurrent(sys->gl);
}

static void ReleaseCurrent(vlc_placebo_t *pl)
{
    vlc_placebo_system_t *sys = pl->sys;
    vlc_gl_ReleaseCurrent(sys->gl);
}
