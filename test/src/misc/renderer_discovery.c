/*****************************************************************************
 * renderer_discovery.c
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#undef NDEBUG
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_renderer_discovery.h>

static void check_option(config_chain_t **pp_cfg, const char *psz_name,
                         const char *psz_value)
{
    config_chain_t *p_cfg = *pp_cfg;

    assert(p_cfg != NULL);
    assert(strcmp(p_cfg->psz_name, psz_name) == 0);
    if (psz_value != NULL)
    {
        assert(p_cfg->psz_value != NULL);
        assert(strcmp(p_cfg->psz_value, psz_value) == 0);
    }
    else
        assert(p_cfg->psz_value == NULL);
    *pp_cfg = p_cfg->p_next;
}

static void test_renderer_name(const char *psz_name, const char *psz_extra_sout)
{
    vlc_renderer_item_t *p_item = vlc_renderer_item_new(
        "chromecast", psz_name, "chromecast://192.0.2.1:8009", psz_extra_sout,
        "cc_demux", NULL, VLC_RENDERER_CAN_AUDIO | VLC_RENDERER_CAN_VIDEO);
    config_chain_t *p_cfg;
    config_chain_t *p_option;
    char *psz_module;
    char *psz_next;

    assert(p_item != NULL);
    assert(strcmp(vlc_renderer_item_name(p_item),
                  psz_name != NULL ? psz_name : "chromecast (192.0.2.1)") == 0);

    psz_next = config_ChainCreate(&psz_module, &p_cfg,
                                 vlc_renderer_item_sout(p_item));
    assert(psz_module != NULL);
    assert(strcmp(psz_module, "chromecast") == 0);
    assert(psz_next == NULL);

    p_option = p_cfg;
    check_option(&p_option, "ip", "192.0.2.1");
    check_option(&p_option, "port", "8009");
    if (psz_name != NULL)
        check_option(&p_option, "device-name", *psz_name != '\0' ? psz_name : NULL);
    if (psz_extra_sout != NULL && *psz_extra_sout != '\0')
        check_option(&p_option, "no-video", NULL);
    assert(p_option == NULL);

    config_ChainDestroy(p_cfg);
    free(psz_module);
    free(psz_next);
    vlc_renderer_item_release(p_item);
}

int main(void)
{
    const char *const names[] =
    {
        NULL,
        "",
        "Living room (Chromecast)",
        "Alice's room (Chromecast)",
        "Living room, TV (Chromecast)",
        "The \"TV\" (Chromecast)",
        "Living {room}: TV",
        "Living } room",
        "Living \\ room",
        "TV\\",
        "TV\\\"",
        " 	Living room	 ",
        "Télévision (Chromecast)",
    };

    for (size_t i = 0; i < ARRAY_SIZE(names); ++i)
    {
        test_renderer_name(names[i], NULL);
        test_renderer_name(names[i], "");
        test_renderer_name(names[i], "no-video");
    }
    return 0;
}
