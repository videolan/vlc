/*
 * media_list.c - libvlc smoke test
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

#include "test.h"

static void test_media_list (const char ** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md1, *md2, *md3, *md4;
    libvlc_media_list_t *ml;
    int ret;

    log ("Testing media_list\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    ml = libvlc_media_list_new (vlc);
    assert (ml != NULL);

    md1 = libvlc_media_new_path (vlc, "/dev/null");
    assert (md1 != NULL);
    md2 = libvlc_media_new_path (vlc, "/dev/null");
    assert (md2 != NULL);
    md3 = libvlc_media_new_path (vlc, "/dev/null");
    assert (md3 != NULL);

    ret = libvlc_media_list_add_media (ml, md1);
    assert (!ret);
    ret = libvlc_media_list_add_media (ml, md2);
    assert (!ret);

    assert( libvlc_media_list_count (ml) == 2 );
    assert( libvlc_media_list_index_of_item (ml, md1) == 0 );
    assert( libvlc_media_list_index_of_item (ml, md2) == 1 );

    ret = libvlc_media_list_remove_index (ml, 0);  /* removing first item */
    assert (!ret);

    /* test if second item was moved on first place */
    assert( libvlc_media_list_index_of_item (ml, md2) == 0 );
    ret = libvlc_media_list_add_media (ml, md1); /* add 2 items */
    assert (!ret);
    ret = libvlc_media_list_add_media (ml, md1);
    assert (!ret);

    /* there should be 3 pieces */
    assert( libvlc_media_list_count (ml) == 3 );

    ret = libvlc_media_list_insert_media (ml, md3, 2);
    assert (!ret);

    /* there should be 4 pieces */
    assert( libvlc_media_list_count (ml) == 4 );

    /* test inserting on right place */
    assert( libvlc_media_list_index_of_item (ml, md3) == 2 );

    /* test right returning descriptor*/
    assert ( libvlc_media_list_item_at_index (ml, 0) == md2 );

    assert ( libvlc_media_list_item_at_index (ml, 2) == md3 );

    /* test if give errors, when it should */
    /* have 4 items, so index 4 should give error */
    ret = libvlc_media_list_remove_index (ml, 4);
    assert (ret == -1);

    ret = libvlc_media_list_remove_index (ml, 100);
    assert (ret == -1);

    ret = libvlc_media_list_remove_index (ml, -1);
    assert (ret == -1);

    /* getting non valid items */
    libvlc_media_t * p_non_exist =
        libvlc_media_list_item_at_index (ml, 4);
    assert (p_non_exist == NULL);

    p_non_exist = libvlc_media_list_item_at_index (ml, 100);
    assert (p_non_exist == NULL);

    p_non_exist = libvlc_media_list_item_at_index (ml, -1);
    assert (p_non_exist == NULL);

    md4 = libvlc_media_new_path (vlc, "/dev/null");
    assert (md4 != NULL);

    /* try to find non inserted item */
    int i_non_exist = 0;
    i_non_exist = libvlc_media_list_index_of_item (ml, md4);
    assert ( i_non_exist == -1 );

    libvlc_media_release (md1);
    libvlc_media_release (md2);
    libvlc_media_release (md3);
    libvlc_media_release (md4);

    libvlc_media_list_release (ml);

    libvlc_release (vlc);
}

int main (void)
{
    test_init();

    test_media_list (test_defaults_args, test_defaults_nargs);

    return 0;
}
