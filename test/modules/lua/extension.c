/*****************************************************************************
 * extension.c: test for the lua extension module
 *****************************************************************************
 * Copyright (C) 2023 Videolabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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

/* Define a builtin module for mocked parts */
#define MODULE_NAME test_lua_extension
#define MODULE_STRING "test_lua_extension"
#undef __PLUGIN__
const char vlc_module_name[] = MODULE_STRING;

#include "../../libvlc/test.h"

#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_player.h>
#include <vlc_extensions.h>
#include <vlc_input_item.h>

#include <limits.h>

static int exitcode = 0;

static int OnLuaEventTriggered(vlc_object_t *obj, const char *name,
        vlc_value_t oldv, vlc_value_t newv, void *opaque)
{
    (void)obj; (void)name; (void)oldv; (void)newv;
    vlc_sem_t *sem = opaque;
    vlc_sem_post(sem);
    return VLC_SUCCESS;
}

static int OpenIntf(vlc_object_t *root)
{
    vlc_object_t *libvlc = (vlc_object_t*)vlc_object_instance(root);
    var_Create(libvlc, "test-lua-activate", VLC_VAR_STRING | VLC_VAR_ISCOMMAND);
    var_Create(libvlc, "test-lua-deactivate", VLC_VAR_STRING | VLC_VAR_ISCOMMAND);
    var_Create(libvlc, "test-lua-input-changed", VLC_VAR_STRING | VLC_VAR_ISCOMMAND);

    intf_thread_t *intf = (intf_thread_t*)root;
    extensions_manager_t *mgr =
        vlc_object_create(root, sizeof *mgr);
    assert(mgr);

    setenv("XDG_DATA_HOME", LUA_EXTENSION_DIR, 1);
    setenv("VLC_DATA_PATH", LUA_EXTENSION_DIR, 1);
    setenv("VLC_LIB_PATH", LUA_EXTENSION_DIR, 1);

    vlc_playlist_t *playlist = vlc_intf_GetMainPlaylist(intf);
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);
    mgr->player = player;
    mgr->p_module = module_need(mgr, "extension", "lua", true);

    if (mgr->p_module == NULL)
    {
        exitcode = 77;
        goto end;
    }

    vlc_sem_t sem_input, sem_activate, sem_deactivate;
    vlc_sem_init(&sem_input, 0);
    vlc_sem_init(&sem_activate, 0);
    vlc_sem_init(&sem_deactivate, 0);

    var_AddCallback(libvlc, "test-lua-activate", OnLuaEventTriggered, &sem_activate);
    var_AddCallback(libvlc, "test-lua-deactivate", OnLuaEventTriggered, &sem_deactivate);
    var_AddCallback(libvlc, "test-lua-input-changed", OnLuaEventTriggered, &sem_input);

    /* Check that the extension from the test is correctly probed. */
    assert(mgr->extensions.i_size == 1);
    extension_Activate(mgr, mgr->extensions.p_elems[0]);
    vlc_sem_wait(&sem_activate);

    vlc_player_Lock(player);
    input_item_t *item = input_item_New(
            "mock://length=100000000000000000", // TODO: make it infinite
            "lua_test_sample");
    vlc_player_SetCurrentMedia(player, item);
    vlc_player_Start(player);
    vlc_player_Unlock(player);

    vlc_sem_wait(&sem_input);

    extension_Deactivate(mgr, mgr->extensions.p_elems[0]);
    vlc_sem_wait(&sem_deactivate);

    var_DelCallback(libvlc, "test-lua-activate", OnLuaEventTriggered, &sem_activate);
    var_DelCallback(libvlc, "test-lua-deactivate", OnLuaEventTriggered, &sem_deactivate);
    var_DelCallback(libvlc, "test-lua-input-changed", OnLuaEventTriggered, &sem_input);

    module_unneed(mgr, mgr->p_module);
    input_item_Release(item);
end:
    vlc_object_delete(mgr);
    return VLC_SUCCESS;
}

/** Inject the mocked modules as a static plugin: **/
vlc_module_begin()
    set_callback(OpenIntf)
    set_capability("interface", 0)
vlc_module_end()

/* Helper typedef for vlc_static_modules */
typedef int (*vlc_plugin_cb)(vlc_set_cb, void*);

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};


int main()
{
    test_init();

    const char * const args[] = {
        "-vvv", "--vout=dummy", "--aout=dummy", "--text-renderer=dummy",
        "--no-auto-preparse",
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(args), args);

    libvlc_add_intf(vlc, MODULE_STRING);
    libvlc_playlist_play(vlc);

    libvlc_release(vlc);
    return 0;
}
