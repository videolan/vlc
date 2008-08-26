/*****************************************************************************
 * winvlc.c: the Windows VLC player
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Lots of other people, see the libvlc AUTHORS file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define UNICODE
#include <vlc/vlc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static char *FromWide (const wchar_t *wide)
{
    size_t len;
    len = WideCharToMultiByte (CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);

    char *out = (char *)malloc (len);
    if (out)
        WideCharToMultiByte (CP_UTF8, 0, wide, -1, out, len, NULL, NULL);
    return out;
}


int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
#ifndef UNDER_CE
                    LPSTR lpCmdLine,
#else
                    LPWSTR lpCmdLine,
#endif
                    int nCmdShow )
{
    int argc, ret;
    wchar_t **wargv = CommandLineToArgvW (GetCommandLine (), &argc);
    if (wargv == NULL)
        return 1;

    char *argv[argc + 1];
    for (int i = 0; i < argc; i++)
        argv[i] = FromWide (wargv[i]);
    argv[argc] = NULL;
    LocalFree (wargv);

    libvlc_exception_t ex, dummy;
    libvlc_exception_init (&ex);
    libvlc_exception_init (&dummy);

    /* Initialize libvlc */
    libvlc_instance_t *vlc;
    vlc = libvlc_new (argc - 1, (const char **)argv + 1, &ex);
    if (vlc != NULL)
    {
        libvlc_add_intf (vlc, NULL, &ex);
        libvlc_playlist_play (vlc, -1, 0, NULL, &dummy);
        libvlc_wait (vlc);
        libvlc_release (vlc);
    }

    ret = libvlc_exception_raised (&ex);
    libvlc_exception_clear (&ex);
    libvlc_exception_clear (&dummy);

    for (int i = 0; i < argc; i++)
        free (argv[i]);

    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return ret;
}
