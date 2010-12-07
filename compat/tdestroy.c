/*****************************************************************************
 * tdestroy.c : either implement tdestroy based on existing t* functions
 *              or implement every t* fuctions (including tdestroy)
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/** search.h is present so only tdestroy has to be implemented based on the
    existing functions */
#ifdef HAVE_SEARCH_H

/*****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

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

void tdestroy (void *root, void (*freenode) (void *))
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
         smallest.node = tab[i];
         if (tdelete (smallest.node, &root, cmp_smallest) == NULL)
             abort ();
    }
    vlc_mutex_unlock (&smallest.lock);
    assert (root == NULL);

    /* Destroy the nodes */
    for (size_t i = 0; i < count; i++)
         freenode ((void *)(tab[i]));
    free (tab);
}

/** search.h is not present, so every t* function has to be implemented */
#else // HAVE_SEARCH_H

#include <assert.h>
#include <stdlib.h>

typedef struct node {
    char         *key;
    struct node  *llink, *rlink;
} node_t;

/*	$NetBSD: tdelete.c,v 1.4 2006/03/19 01:12:08 christos Exp $	*/

/*
 * Tree search generalized from Knuth (6.2.2) Algorithm T just like
 * the AT&T man page says.
 *
 * The node_t structure is for internal use only, lint doesn't grok it.
 *
 * Written by reading the System V Interface Definition, not the code.
 *
 * Totally public domain.
 */

/* delete node with given key */
void *
tdelete(vkey, vrootp, compar)
	const void *vkey;	/* key to be deleted */
	void      **vrootp;	/* address of the root of tree */
	int       (*compar) (const void *, const void *);
{
	node_t **rootp = (node_t **)vrootp;
	node_t *p, *q, *r;
	int  cmp;

	assert(vkey != NULL);
	assert(compar != NULL);

	if (rootp == NULL || (p = *rootp) == NULL)
		return NULL;

	while ((cmp = (*compar)(vkey, (*rootp)->key)) != 0) {
		p = *rootp;
		rootp = (cmp < 0) ?
		    &(*rootp)->llink :		/* follow llink branch */
		    &(*rootp)->rlink;		/* follow rlink branch */
		if (*rootp == NULL)
			return NULL;		/* key not found */
	}
	r = (*rootp)->rlink;			/* D1: */
	if ((q = (*rootp)->llink) == NULL)	/* Left NULL? */
		q = r;
	else if (r != NULL) {			/* Right link is NULL? */
		if (r->llink == NULL) {		/* D2: Find successor */
			r->llink = q;
			q = r;
		} else {			/* D3: Find NULL link */
			for (q = r->llink; q->llink != NULL; q = r->llink)
				r = q;
			r->llink = q->rlink;
			q->llink = (*rootp)->llink;
			q->rlink = (*rootp)->rlink;
		}
	}
	if (p != *rootp)
		free(*rootp);			/* D4: Free node */
	*rootp = q;				/* link parent to new node */
	return p;
}


/*	$NetBSD: tdestroy.c,v 1.2 1999/09/16 11:45:37 lukem Exp $	*/

/*
 * Tree search generalized from Knuth (6.2.2) Algorithm T just like
 * the AT&T man page says.
 *
 * The node_t structure is for internal use only, lint doesn't grok it.
 *
 * Written by reading the System V Interface Definition, not the code.
 *
 * Totally public domain.
 */

/* Walk the nodes of a tree */
static void
tdestroy_recurse(node_t* root, void (*free_action)(void *))
{
  if (root->llink != NULL)
    tdestroy_recurse(root->llink, free_action);
  if (root->rlink != NULL)
    tdestroy_recurse(root->rlink, free_action);

  (*free_action) ((void *) root->key);
  free(root);
}

void
tdestroy(vrootp, freefct)
       void *vrootp;
       void (*freefct)(void *);
{
  node_t *root = (node_t *) vrootp;

  if (root != NULL)
    tdestroy_recurse(root, freefct);
}


/*	$NetBSD: tfind.c,v 1.5 2005/03/23 08:16:53 kleink Exp $	*/

/*
 * Tree search generalized from Knuth (6.2.2) Algorithm T just like
 * the AT&T man page says.
 *
 * The node_t structure is for internal use only, lint doesn't grok it.
 *
 * Written by reading the System V Interface Definition, not the code.
 *
 * Totally public domain.
 */

/* find a node, or return 0 */
void *
tfind(vkey, vrootp, compar)
	const void *vkey;		/* key to be found */
	const void **vrootp;		/* address of the tree root */
	int (*compar) (const void *, const void *);
{
	node_t * const *rootp = (node_t * const*)vrootp;

	assert(vkey != NULL);
	assert(compar != NULL);

	if (rootp == NULL)
		return NULL;

	while (*rootp != NULL) {		/* T1: */
		int r;

		if ((r = (*compar)(vkey, (*rootp)->key)) == 0)	/* T2: */
			return *rootp;		/* key found */
		rootp = (r < 0) ?
		    &(*rootp)->llink :		/* T3: follow left branch */
		    &(*rootp)->rlink;		/* T4: follow right branch */
	}
	return NULL;
}


/*	$NetBSD: tsearch.c,v 1.5 2005/11/29 03:12:00 christos Exp $	*/

/*
 * Tree search generalized from Knuth (6.2.2) Algorithm T just like
 * the AT&T man page says.
 *
 * The node_t structure is for internal use only, lint doesn't grok it.
 *
 * Written by reading the System V Interface Definition, not the code.
 *
 * Totally public domain.
 */

/* find or insert datum into search tree */
void *
tsearch(vkey, vrootp, compar)
	const void *vkey;		/* key to be located */
	void **vrootp;			/* address of tree root */
	int (*compar) (const void *, const void *);
{
	node_t *q;
	node_t **rootp = (node_t **)vrootp;

	assert(vkey != NULL);
	assert(compar != NULL);

	if (rootp == NULL)
		return NULL;

	while (*rootp != NULL) {	/* Knuth's T1: */
		int r;

		if ((r = (*compar)(vkey, (*rootp)->key)) == 0)	/* T2: */
			return *rootp;		/* we found it! */

		rootp = (r < 0) ?
		    &(*rootp)->llink :		/* T3: follow left branch */
		    &(*rootp)->rlink;		/* T4: follow right branch */
	}

	q = malloc(sizeof(node_t));		/* T5: key not found */
	if (q != 0) {				/* make new node */
		*rootp = q;			/* link new node to old */
		q->key = (void*)vkey;	/* initialize new node */
		q->llink = q->rlink = NULL;
	}
	return q;
}


/*	$NetBSD: twalk.c,v 1.2 1999/09/16 11:45:37 lukem Exp $	*/

/*
 * Tree search generalized from Knuth (6.2.2) Algorithm T just like
 * the AT&T man page says.
 *
 * The node_t structure is for internal use only, lint doesn't grok it.
 *
 * Written by reading the System V Interface Definition, not the code.
 *
 * Totally public domain.
 */

/* Walk the nodes of a tree */
static void
twalk_recurse(root, action, level)
	const node_t *root;	/* Root of the tree to be walked */
	void (*action) (const void *, VISIT, int);
	int level;
{
	assert(root != NULL);
	assert(action != NULL);

	if (root->llink == NULL && root->rlink == NULL)
		(*action)(root, leaf, level);
	else {
		(*action)(root, preorder, level);
		if (root->llink != NULL)
			twalk_recurse(root->llink, action, level + 1);
		(*action)(root, postorder, level);
		if (root->rlink != NULL)
			twalk_recurse(root->rlink, action, level + 1);
		(*action)(root, endorder, level);
	}
}

/* Walk the nodes of a tree */
void
twalk(vroot, action)
	const void *vroot;	/* Root of the tree to be walked */
	void (*action) (const void *, VISIT, int);
{
	if (vroot != NULL && action != NULL)
		twalk_recurse(vroot, action, 0);
}

#endif // HAVE_SEARCH_H
