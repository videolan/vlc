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

    log ("Testing media_list\n");

    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();

    ml = libvlc_media_list_new (vlc, &ex);
    catch ();

    md1 = libvlc_media_new (vlc, "/dev/null", &ex);
    catch ();
    md2 = libvlc_media_new (vlc, "/dev/null", &ex);
    catch ();
    md3 = libvlc_media_new (vlc, "/dev/null", &ex);
    catch ();

    libvlc_media_list_add_media (ml, md1, &ex);
    catch ();
    libvlc_media_list_add_media (ml, md2, &ex);
    catch ();

    assert( libvlc_media_list_count (ml, &ex) == 2 );
    catch ();

    assert( libvlc_media_list_index_of_item (ml, md1, &ex) == 0 );
    catch ();

    assert( libvlc_media_list_index_of_item (ml, md2, &ex) == 1 );
    catch ();

    libvlc_media_list_remove_index (ml, 0, &ex);  /* removing first item */
    catch ();

    /* test if second item was moved on first place */
    assert( libvlc_media_list_index_of_item (ml, md2, &ex) == 0 );
    catch ();

    libvlc_media_list_add_media (ml, md1, &ex); /* add 2 items */
    catch ();
    libvlc_media_list_add_media (ml, md1, &ex);
    catch ();

    /* there should be 3 pieces */
    assert( libvlc_media_list_count (ml, &ex) == 3 );
    catch ();

    libvlc_media_list_insert_media (ml, md3, 2, &ex);
    catch ();

    /* there should be 4 pieces */
    assert( libvlc_media_list_count (ml, &ex) == 4 );
    catch ();

    /* test inserting on right place */
    assert( libvlc_media_list_index_of_item (ml, md3, &ex) == 2 );
    catch ();

    /* test right returning descriptor*/
    assert ( libvlc_media_list_item_at_index (ml, 0, &ex) == md2 );
    catch ();

    assert ( libvlc_media_list_item_at_index (ml, 2, &ex) == md3 );
    catch ();

    /* test if give exceptions, when it should */
    /* have 4 items, so index 4 should give exception */
    libvlc_media_list_remove_index (ml, 4, &ex);
    assert (have_exception ());

    libvlc_media_list_remove_index (ml, 100, &ex);
    assert (have_exception ());

    libvlc_media_list_remove_index (ml, -1, &ex);
    assert (have_exception ());

    /* getting non valid items */
    libvlc_media_t * p_non_exist =
        libvlc_media_list_item_at_index (ml, 4, &ex);
    assert (have_exception ());

    p_non_exist = libvlc_media_list_item_at_index (ml, 100, &ex);
    assert (have_exception ());

    p_non_exist = libvlc_media_list_item_at_index (ml, -1, &ex);
    assert (have_exception ());

    md4 = libvlc_media_new (vlc, "/dev/null", &ex);
    catch ();

    /* try to find non inserted item */
    int i_non_exist = 0;
    i_non_exist = libvlc_media_list_index_of_item (ml, md4, &ex);
    assert ( i_non_exist == -1 );

    libvlc_media_release (md1);
    libvlc_media_release (md2);
    libvlc_media_release (md3);
    libvlc_media_release (md4);

    libvlc_media_list_release (ml);

    libvlc_release (vlc);
    catch ();
}

int main (void)
{
    test_init();

    test_media_list (test_defaults_args, test_defaults_nargs);

    return 0;
}
