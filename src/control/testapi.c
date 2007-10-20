/*
 * testapi.c - libvlc-control smoke test
 *
 * $Id$
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

#include <vlc/mediacontrol.h>
#include <assert.h>

int main (int argc, char *argv[])
{
    mediacontrol_Exception ex;
    mediacontrol_Instance *mc, *mc2;
    libvlc_instance_t *vlc;

    mediacontrol_exception_init (&ex);
    mc = mediacontrol_new (argc, argv, &ex);
    assert (mc);
    assert (!ex.code);
    mediacontrol_exception_cleanup (&ex);

    /* Duplication test */
    vlc = mediacontrol_get_libvlc_instance (mc);
    assert (vlc);
    assert (!ex.code);

    mediacontrol_exception_init (&ex);
    mc2 = mediacontrol_new_from_instance (vlc, &ex);
    assert (mc2);
    assert (!ex.code);
    mediacontrol_exception_cleanup (&ex);

    //mediacontrol_exit (mc2);

    /* Input tests */
    mediacontrol_exception_init (&ex);
    mediacontrol_resume (mc, NULL, &ex);
    assert (ex.code); /* should fail: we have no input */
    mediacontrol_exception_cleanup (&ex);

    mediacontrol_exception_init (&ex);
    mediacontrol_pause (mc, NULL, &ex);
    assert (ex.code); /* should fail: we have no input */
    mediacontrol_exception_cleanup (&ex);

    mediacontrol_exception_init (&ex);
    mediacontrol_stop (mc, NULL, &ex);
    mediacontrol_exception_cleanup (&ex);

    /* Playlist tests */
    mediacontrol_exception_init (&ex);
    mediacontrol_playlist_clear (mc, &ex);
    assert (!ex.code);
    mediacontrol_exception_cleanup (&ex);

    mediacontrol_exception_init (&ex);
    mediacontrol_playlist_add_item (mc, "/dev/null", &ex);
    mediacontrol_exception_cleanup (&ex);

    mediacontrol_exception_init (&ex);
    mediacontrol_playlist_clear (mc, &ex);
    assert (!ex.code);
    mediacontrol_exception_cleanup (&ex);

    mediacontrol_exit (mc);
    return 0;
}
