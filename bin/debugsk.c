/*****************************************************************************
 * debugsk.m: SK debug development main executable for VLC media player
 *****************************************************************************
 * Copyright (C) 2020 Videolabs
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
 *          Alexandre Janniaux <ajanni@videolabs.io>
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

#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_variables.h>
#include <vlc_plugin.h>

#include <stdlib.h>
#include <locale.h>
#include <signal.h>
#include <string.h>

int main(int argc, char **argv)
{
    /* The so-called POSIX-compliant MacOS X reportedly processes SIGPIPE even
     * if it is blocked in all thread.
     * Note: this is NOT an excuse for not protecting against SIGPIPE. If
     * LibVLC runs outside of VLC, we cannot rely on this code snippet. */
    signal(SIGPIPE, SIG_IGN);
    /* Restore SIGCHLD in case our parent process ignores it. */
    signal(SIGCHLD, SIG_DFL);


#ifdef TOP_BUILDDIR
    setenv("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 1);
    setenv("VLC_DATA_PATH", TOP_SRCDIR"/share", 1);
    setenv("VLC_LIB_PATH", TOP_BUILDDIR"/modules", 1);
#else
#error no plugins
#endif


    const char * const args[] =
    {
        "--vout=dummy",
        "-vvv",
    };

    /* Initialize libVLC */
    libvlc_instance_t *libvlc = libvlc_new(sizeof args / sizeof args[0], args);
    if (libvlc == NULL)
        return -1;

    /* Start glue interface, see code below */
    //libvlc_add_intf(_libvlc, "ios_interface,none");

    /* Start parsing arguments and eventual playback */
    libvlc_media_player_t *player = libvlc_media_player_new(libvlc);
    if (player == NULL)
        return -1;

    libvlc_media_t *media = libvlc_media_new_location("mock://video_track_count=1;video_width=800;video_height=600;length=10000000000");
    if (media == NULL)
        return -1;
    libvlc_media_player_set_media(player, media);
    libvlc_media_player_play(player);

    //for (int j=0; j<5; ++j)
    //{
    //    sleep(1);
    //    libvlc_media_player_enable_avstat(player, false);
    //    sleep(1);
    //    libvlc_media_player_enable_avstat(player, true);
    //}

    
    libvlc_media_player_enable_avstat(player, false);
    for (int j=0; j<5; ++j)
    {
        sleep(1);
        libvlc_media_player_enable_clock_recovery(player, false);
        sleep(1);
        libvlc_media_player_enable_clock_recovery(player, true);
    }


    return 0;
}
