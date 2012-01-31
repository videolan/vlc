/**
 * @file tdestroy.c
 * @brief replacement for GNU tdestroy()
 */
/*****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
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

#if defined(HAVE_SEARCH_H) && !defined(HAVE_TDESTROY)

#include <stdlib.h>
#include <assert.h>

#include <vlc_common.h>
#include <search.h>

static struct
{
    const void **tab;
    size_t count;
    vlc_mutex_t lock;
} list = { NULL, 0, VLC_STATIC_MUTEX };

static void list_nodes (const void *node, const VISIT which, const int depth)
{
    (void) depth;

    if (which != postorder && which != leaf)
        return;

    const void **tab = realloc (list.tab, sizeof (*tab) * (list.count + 1));
    if (unlikely(tab == NULL))
        abort ();

    tab[list.count] = *(const void **)node;
    list.tab = tab;
    list.count++;
}

static struct
{
    const void *node;
    vlc_mutex_t lock;
} smallest = { NULL, VLC_STATIC_MUTEX };

static int cmp_smallest (const void *a, const void *b)
{
    if (a == b)
        return 0;
    if (a == smallest.node)
        return -1;
    if (likely(b == smallest.node))
        return +1;
    abort ();
}

void vlc_tdestroy (void *root, void (*freenode) (void *))
{
    const void **tab;
    size_t count;

    assert (freenode != NULL);

    /* Enumerate nodes in order */
    vlc_mutex_lock (&list.lock);
    assert (list.count == 0);
    twalk (root, list_nodes);
    tab = list.tab;
    count = list.count;
    list.tab = NULL;
    list.count = 0;
    vlc_mutex_unlock (&list.lock);

    /* Destroy the tree */
    vlc_mutex_lock (&smallest.lock);
    for (size_t i = 0; i < count; i++)
    {
         void *node  = tab[i];

         smallest.node = node;
         node = tdelete (node, &root, cmp_smallest);
         assert (node != NULL);
    }
    vlc_mutex_unlock (&smallest.lock);
    assert (root == NULL);

    /* Destroy the nodes */
    for (size_t i = 0; i < count; i++)
         freenode ((void *)(tab[i]));
    free (tab);
}

#endif
