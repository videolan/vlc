/*
 * core.c - libvlc smoke test
 *
 */

/**********************************************************************
 *  Copyright (C) 2007 Rémi Denis-Courmont.                           *
 *  This program is free software; you can redistribute and/or modify *
 *  it under the terms of the GNU General Public License as published *
 *  by the Free Software Foundation; version 2 of the license, or (at *
 *  your option) any later version.                                   *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *  See the GNU General Public License for more details.              *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, you can get it from:             *
 *  http://www.gnu.org/copyleft/gpl.html                              *
 **********************************************************************/

#include "test.h"

#include <string.h>

static void test_core (const char ** argv, int argc)
{
    libvlc_instance_t *vlc;

    test_log ("Testing core\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    libvlc_retain (vlc);
    libvlc_release (vlc);
    libvlc_release (vlc);
}

static void test_moduledescriptionlist (libvlc_module_description_t *list)
{
    libvlc_module_description_t *module = list;
    while ( module ) {
        assert (strlen (module->psz_name) );
        assert (strlen (module->psz_shortname) );
        assert (module->psz_longname == NULL || strlen (module->psz_longname));
        assert (module->psz_help == NULL || strlen (module->psz_help));
        module = module->p_next;
    }    

    libvlc_module_description_list_release (list);
}

static void test_audiovideofilterlists (const char ** argv, int argc)
{
    libvlc_instance_t *vlc;

    test_log ("Testing libvlc_(audio|video)_filter_list_get()\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    test_moduledescriptionlist (libvlc_audio_filter_list_get (vlc));
    test_moduledescriptionlist (libvlc_video_filter_list_get (vlc));

    libvlc_release (vlc);
}

static void test_audio_output (void)
{
    libvlc_instance_t *vlc = libvlc_new (0, NULL);
    assert (vlc != NULL);

    libvlc_audio_output_t *mods = libvlc_audio_output_list_get (vlc);
    assert (mods != NULL);

    puts ("Audio outputs:");
    for (const libvlc_audio_output_t *o = mods; o != NULL; o = o->p_next)
    {
        libvlc_audio_output_device_t *devs;

        printf(" %s: %s\n", o->psz_name, o->psz_description);

        devs = libvlc_audio_output_device_list_get (vlc, o->psz_name);
        if (devs == NULL)
            continue;
        for (const libvlc_audio_output_device_t *d = devs;
             d != NULL;
             d = d->p_next)
             printf("  %s: %s\n", d->psz_device, d->psz_description);

        libvlc_audio_output_device_list_release (devs);
    }
    libvlc_audio_output_list_release (mods);
    libvlc_release (vlc);
}

int main (void)
{
    test_init();

    test_core (test_defaults_args, test_defaults_nargs);
    test_audiovideofilterlists (test_defaults_args, test_defaults_nargs);
    test_audio_output ();

    return 0;
}
