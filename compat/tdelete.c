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

#define _SEARCH_PRIVATE

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/cdefs.h>
#include <assert.h>
#include <stdlib.h>

/* delete node with given key */
void *
tdelete(vkey, vrootp, compar)
	const void *vkey;	/* key to be deleted */
	void      **vrootp;	/* address of the root of tree */
	int       (*compar) __P((const void *, const void *));
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
