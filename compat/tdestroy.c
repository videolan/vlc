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

#define _SEARCH_PRIVATE

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/cdefs.h>
#include <assert.h>
#include <stdlib.h>

/* Walk the nodes of a tree */
static void
trecurse(node_t* root, void (*free_action)(void *))
{
  if (root->llink != NULL)
    trecurse(root->llink, free_action);
  if (root->rlink != NULL)
    trecurse(root->rlink, free_action);

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
    trecurse(root, freefct);
}
