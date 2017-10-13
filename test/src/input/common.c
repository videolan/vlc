/*****************************************************************************
 * common.c
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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

#include "../lib/libvlc_internal.h"

#include "common.h"

static inline int getenv_atoi(const char *name)
{
    char *env = getenv(name);
    return env ? atoi(env) : 0;
}

void vlc_run_args_init(struct vlc_run_args *args)
{
    memset(args, 0, sizeof(struct vlc_run_args));
    args->verbose = getenv_atoi("V");
    if (args->verbose >= 10)
        args->verbose = 9;

    args->name = getenv("VLC_TARGET");
    args->test_demux_controls = getenv_atoi("VLC_DEMUX_CONTROLS");
}

libvlc_instance_t *libvlc_create(const struct vlc_run_args *args)
{
#ifdef TOP_BUILDDIR
# ifndef HAVE_STATIC_MODULES
    setenv("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 1);
# endif
    setenv("VLC_DATA_PATH", TOP_SRCDIR"/share", 1);
#endif

    /* Override argc/argv with "--verbose lvl" or "--quiet" depending on the V
     * environment variable */
    const char *argv[2];
    char verbose[2];
    int argc = args->verbose == 0 ? 1 : 2;

    if (args->verbose > 0)
    {
        argv[0] = "--verbose";
        sprintf(verbose, "%u", args->verbose);
        argv[1] = verbose;
    }
    else
        argv[0] = "--quiet";

    libvlc_instance_t *vlc = libvlc_new(argc, argv);
    if (vlc == NULL)
        fprintf(stderr, "Error: cannot initialize LibVLC.\n");

    return vlc;
}
