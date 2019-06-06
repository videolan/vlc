/*****************************************************************************
 * intromsg.h
 *****************************************************************************
 * Copyright (C) 1999-2015 VLC authors and VideoLAN
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

static inline void intf_consoleIntroMsg(intf_thread_t *p_intf)
{
    if (getenv( "PWD" ) == NULL) /* detect Cygwin shell or Wine */
    {
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        freopen("CONIN$", "r", stdin);
    }

    msg_rc("VLC media player - %s", VERSION_MESSAGE);
    msg_rc("%s", COPYRIGHT_MESSAGE);
    msg_rc(_("\nWarning: if you cannot access the GUI "
                     "anymore, open a command-line window, go to the "
                     "directory where you installed VLC and run "
                     "\"vlc -I qt\"\n"));
}
