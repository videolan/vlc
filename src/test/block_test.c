/*****************************************************************************
 * block_test.c: Test for block_t stuff
 *****************************************************************************
 * Copyright (C) 2008 Rémi Denis-Courmont
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

#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>

#include <vlc_common.h>
#include <vlc_block.h>

static const char text[] =
    "This is a test!\n"
    "This file can be deleted safely!\n";

static void test_block_File (void)
{
    FILE *stream;
    int res;

    stream = fopen ("testfile.txt", "wb+");
    assert (stream != NULL);

    res = fputs (text, stream);
    assert (res != EOF);
    res = fflush (stream);
    assert (res != EOF);

    block_t *block = block_File (fileno (stream));
    fclose (stream);

    assert (block != NULL);
    assert (block->i_buffer == strlen (text));
    assert (!memcmp (block->p_buffer, text, block->i_buffer));
    block_Release (block);

    remove ("testfile.txt");
}

static void test_block (void)
{
    block_t *block = block_Alloc (sizeof (text));
    assert (block != NULL);

    memcpy (block->p_buffer, text, sizeof (text));
    block = block_Realloc (block, 0, sizeof (text));
    assert (block != NULL);
    assert (block->i_buffer == sizeof (text));
    assert (!memcmp (block->p_buffer, text, sizeof (text)));

    block = block_Realloc (block, 200, sizeof (text) + 200);
    assert (block != NULL);
    assert (block->i_buffer == 200 + sizeof (text) + 200);
    assert (!memcmp (block->p_buffer + 200, text, sizeof (text)));

    block = block_Realloc (block, -200, sizeof (text) + 200);
    assert (block != NULL);
    assert (block->i_buffer == sizeof (text));
    assert (!memcmp (block->p_buffer, text, sizeof (text)));
    block_Release (block);

    //block = block_Alloc (SIZE_MAX);
    //assert (block == NULL);
}

int main (void)
{
    test_block_File ();
    test_block ();
    return 0;
}

