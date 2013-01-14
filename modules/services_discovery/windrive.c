/**
 * @file win_disc.c
 * @brief List of disc drives for VLC media player for Windows
 */
/*****************************************************************************
 * Copyright © 2010 Rémi Denis-Courmont
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
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_services_discovery.h>
#include <vlc_plugin.h>

static int Open (vlc_object_t *);

VLC_SD_PROBE_HELPER("disc", "Discs", SD_CAT_DEVICES)

/*
 * Module descriptor
 */
vlc_module_begin ()
    add_submodule ()
    set_shortname (N_("Discs"))
    set_description (N_("Discs"))
    set_category (CAT_PLAYLIST)
    set_subcategory (SUBCAT_PLAYLIST_SD)
    set_capability ("services_discovery", 0)
    set_callbacks (Open, NULL)
    add_shortcut ("disc")

    VLC_SD_PROBE_SUBMODULE

vlc_module_end ()

/**
 * Probes and initializes.
 */
static int Open (vlc_object_t *obj)
{
    services_discovery_t *sd = (services_discovery_t *)obj;

    LONG drives = GetLogicalDrives ();
    char mrl[12] = "file:///A:/", name[3] = "A:";
    TCHAR path[4] = TEXT("A:\\");

    for (char d = 0; d < 26; d++)
    {
        input_item_t *item;
        char letter = 'A' + d;

        /* Does this drive actually exist? */
        if (!(drives & (1 << d)))
            continue;
        /* Is it a disc drive? */
        path[0] = letter;
        if (GetDriveType (path) != DRIVE_CDROM)
            continue;

        mrl[8] = name[0] = letter;
        item = input_item_NewWithType (mrl, name,
                                       0, NULL, 0, -1, ITEM_TYPE_DISC);
        msg_Dbg (sd, "adding %s (%s)", mrl, name);
        if (item == NULL)
            break;

        services_discovery_AddItem (sd, item, _("Local drives"));
    }
    return VLC_SUCCESS;
}
