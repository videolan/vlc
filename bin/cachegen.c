/*****************************************************************************
 * cachegen.c: LibVLC plugins cache generator
 *****************************************************************************
 * Copyright (C) 2010 Rémi Denis-Courmont
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

#include <vlc/vlc.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

static void version (void)
{
    puts ("LibVLC plugins cache generation version "VERSION);
}

static void usage (const char *path)
{
    printf (
"Usage: %s <path>\n"
"Generate the LibVLC plugins cache for the specified plugins directory.\n",
            path);
}

int main (int argc, char *argv[])
{
#ifdef HAVE_GETOPT_H
    static const struct option opts[] =
    {
        { "help",       no_argument,       NULL, 'h' },
        { "version",    no_argument,       NULL, 'V' },
        { NULL,         no_argument,       NULL, '\0'}
    };

    int c;

    while ((c = getopt_long (argc, argv, "hV", opts, NULL)) != -1)
        switch (c)
        {
            case 'h':
                usage (argv[0]);
                return 0;
            case 'V':
                version ();
                return 0;
            default:
                usage (argv[0]);
                return 1;
        }
#else
    int optind = 1;
#endif

    for (int i = optind; i < argc; i++)
    {
        const char *path = argv[i];

        if (setenv ("VLC_PLUGIN_PATH", path, 1))
            abort ();

        const char *vlc_argv[4];
        int vlc_argc = 0;

        vlc_argv[vlc_argc++] = "--quiet";
        vlc_argv[vlc_argc++] = "--reset-plugins-cache";
        vlc_argv[vlc_argc++] = "--"; /* end of options */
        vlc_argv[vlc_argc] = NULL;

        libvlc_instance_t *vlc = libvlc_new (vlc_argc, vlc_argv);
        if (vlc == NULL)
            return 1;
        libvlc_release(vlc);
    }

    return 0;
}
