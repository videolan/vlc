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

#ifdef HAVE_SETLOCALE
# include <locale.h>
#endif

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

static void version (void)
{
    puts ("LibVLC plugins cache generation version "VERSION);
}

static void usage (const char *path)
{
    printf ("Usage: %s <path>\n"
            "Generate the LibVLC plugins cache "
            "for the specified plugins directory.\n", path);
}

/* Explicit HACK */
extern void LocaleFree (const char *);
extern char *FromLocale (const char *);

int main (int argc, char *argv[])
{
    static const struct option opts[] =
    {
        { "help",       no_argument,       NULL, 'h' },
        { "version",    no_argument,       NULL, 'V' },
        { NULL,         no_argument,       NULL, '\0'}
    };

#ifdef HAVE_SETLOCALE
    setlocale (LC_CTYPE, ""); /* needed by FromLocale() */
#endif

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

    for (int i = optind; i < argc; i++)
    {
        /* Note that FromLocale() can be used before libvlc is initialized */
        const char *path = FromLocale (argv[i]);
        char *arg;

        if (asprintf (&arg, "--plugin-path=%s", path) == -1)
            abort ();

        const char *const vlc_argv[] = {
            "--ignore-config",
            "--quiet",
            "--no-media-library",
            arg,
            NULL,
        };
        size_t vlc_argc = sizeof (vlc_argv) / sizeof (vlc_argv[0]) - 1;

        libvlc_exception_t ex;
        libvlc_exception_init (&ex);

        libvlc_instance_t *vlc = libvlc_new (vlc_argc, vlc_argv, &ex);
        if (vlc != NULL)
            libvlc_release (vlc);
        free (arg);
        if (vlc == NULL)
            fprintf (stderr, "No plugins in %s\n", path);
        LocaleFree (path);
        if (vlc == NULL)
            return 1;
    }

    return 0;
}
