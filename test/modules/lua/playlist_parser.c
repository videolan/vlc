/*****************************************************************************
 * playlist_parser.c: test for the lua playlist parser module
 *****************************************************************************
 * Copyright (C) 2026 VideoLAN
 *
 * Authors: Bipul Lamsal <bipullamsal _at_ gmail.com>
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Define a builtin module for mocked parts */
#define MODULE_NAME test_lua_playlist_parser
#undef VLC_DYNAMIC_PLUGIN

#include "../../libvlc/test.h"

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_input_item.h>
#include <vlc_interface.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>

#include "../lib/libvlc_internal.h"

const char vlc_module_name[] = MODULE_STRING;

static int OpenIntf(vlc_object_t *root) {
  // setup
  const char *test_parsed_value = "VideoLAN";
  const char *sample_test_markup = "<html><title>%s</title></html>\n";
  char sample_test[100];
  snprintf(sample_test, sizeof(sample_test), sample_test_markup,
           test_parsed_value);

  stream_t *p_s = vlc_stream_MemoryNew(root, (uint8_t *)sample_test,
                                       strlen(sample_test), true);
  assert(p_s != NULL);

  p_s->psz_url = strdup("mock://length=100");

  // exercise
  demux_t *p_d = demux_New(root, "luaplaylist", p_s->psz_url, p_s, NULL);

  // verification
  assert(p_d != NULL);

  // next behavior is parse() execution
  input_item_t *p_item = input_item_New(INPUT_ITEM_URI_NOP, NULL);
  input_item_node_t *p_node = input_item_node_Create(p_item);

  int probe_ret = vlc_stream_ReadDir(p_d, p_node);

  assert(probe_ret == 0);
  assert(p_node->i_children == 1);

  char *test_title = p_node->pp_children[0]->p_item->psz_name;
  assert(strcmp(test_title, test_parsed_value) == 0);

  input_item_node_Delete(p_node);

  // cleanup
  // this calls demux_DestroyDemux(p_d) callback unloading the module and
  // deleting the stream as well
  demux_Delete(p_d);
  return VLC_SUCCESS;
}

/** Inject the mocked modules as a static plugin: **/
vlc_module_begin()
    set_callback(OpenIntf)
    set_capability("interface", 0)
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

int main(void) {
  setenv("VLC_USERDATA_PATH", TOP_SRCDIR "/test/modules/", 1);
  test_init();

  const char *const args[] = {
      "-vvv",
      "--vout=dummy",
      "--aout=dummy",
      "--text-renderer=dummy",
      "--no-auto-preparse",
  };

  libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(args), args);

  libvlc_InternalAddIntf(vlc->p_libvlc_int, MODULE_STRING);
  libvlc_InternalPlay(vlc->p_libvlc_int);

  libvlc_release(vlc);
  return 0;
}
