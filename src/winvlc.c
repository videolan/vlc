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

static int parse_cmdline (char *line, char ***argvp)
{
    char **argv = malloc (sizeof (char *));
    int argc = 0;

    while (*line != '\0')
    {
        char quote = 0;

        /* Skips white spaces */
        while (strchr ("\t ", *line))
            line++;
        if (!*line)
            break;

        /* Starts a new parameter */
        argv = realloc (argv, (argc + 2) * sizeof (char *));
        if (*line == '"')
        {
            quote = '"';
            line++;
        }
        argv[argc++] = line;

    more:
        while (*line && !strchr ("\t ", *line))
            line++;

        if (line > argv[argc - 1] && line[-1] == quote)
            /* End of quoted parameter */
            line[-1] = 0;
        else
        if (*line && quote)
        {
            /* Space within a quote */
            line++;
            goto more;
        }
        else
        /* End of unquoted parameter */
        if (*line)
            *line++ = 0;
    }
    argv[argc] = NULL;
    *argvp = argv;
    return argc;
}

#ifdef UNDER_CE
# define wWinMain WinMain
#endif

/*****************************************************************************
 * wWinMain: parse command line, start interface and spawn threads.
 *****************************************************************************/
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow )
{
    char **argv, psz_cmdline[wcslen(lpCmdLine) * 4];
    int argc, ret;

    (void)hInstance; (void)hPrevInstance; (void)nCmdShow;

    WideCharToMultiByte( CP_UTF8, 0, lpCmdLine, -1,
                         psz_cmdline, sizeof (psz_cmdline), NULL, NULL );

    argc = parse_cmdline (psz_cmdline, &argv);

    libvlc_exception_t ex;
    libvlc_exception_init (&ex);

    /* Initialize libvlc */
    libvlc_instance_t *vlc = libvlc_new (argc, (const char **)argv, &ex);
    if (vlc != NULL)
    {
        libvlc_add_intf (vlc, NULL, &ex);
        libvlc_playlist_play (vlc, -1, 0, NULL, NULL);
        libvlc_wait (vlc);
        libvlc_release (vlc);
    }
    free (argv);

    ret = libvlc_exception_raised (&ex);
    libvlc_exception_clear (&ex);
    return ret;
}

#ifndef IF_MINGW_SUPPORTED_UNICODE
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR args, int nCmdShow)
{
    /* This makes little sense, but at least it links properly */
    wchar_t lpCmdLine[strlen(args) * 3];
    MultiByteToWideChar( CP_ACP, 0, args, -1, lpCmdLine, sizeof(lpCmdLine) );
    return wWinMain (hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}
#endif
