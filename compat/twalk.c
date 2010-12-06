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

#define _SEARCH_PRIVATE

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/cdefs.h>
#include <assert.h>
#include <stdlib.h>

static void trecurse __P((const node_t *,
    void  (*action)(const void *, VISIT, int), int level));

/* Walk the nodes of a tree */
static void
trecurse(root, action, level)
	const node_t *root;	/* Root of the tree to be walked */
	void (*action) __P((const void *, VISIT, int));
	int level;
{
	assert(root != NULL);
	assert(action != NULL);

	if (root->llink == NULL && root->rlink == NULL)
		(*action)(root, leaf, level);
	else {
		(*action)(root, preorder, level);
		if (root->llink != NULL)
			trecurse(root->llink, action, level + 1);
		(*action)(root, postorder, level);
		if (root->rlink != NULL)
			trecurse(root->rlink, action, level + 1);
		(*action)(root, endorder, level);
	}
}

/* Walk the nodes of a tree */
void
twalk(vroot, action)
	const void *vroot;	/* Root of the tree to be walked */
	void (*action) __P((const void *, VISIT, int));
{
	if (vroot != NULL && action != NULL)
		trecurse(vroot, action, 0);
}
