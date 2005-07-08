/*****************************************************************************
 * splaytree.c:
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          code from projectM http://xmms-projectm.sourceforge.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/



/*
                An implementation of top-down splaying
                    D. Sleator <sleator@cs.cmu.edu>
    	                     March 1992

  "Splay trees", or "self-adjusting search trees" are a simple and
  efficient data structure for storing an ordered set.  The data
  structure consists of a binary tree, without parent pointers, and no
  additional fields.  It allows searching, insertion, deletion,
  deletemin, deletemax, splitting, joining, and many other operations,
  all with amortized logarithmic performance.  Since the trees adapt to
  the sequence of requests, their performance on real access patterns is
  typically even better.  Splay trees are described in a number of texts
  and papers [1,2,3,4,5].

  The code here is adapted from simple top-down splay, at the bottom of
  page 669 of [3].  It can be obtained via anonymous ftp from
  spade.pc.cs.cmu.edu in directory /usr/sleator/public.

  The chief modification here is that the splay operation works even if the
  item being splayed is not in the tree, and even if the tree root of the
  tree is NULL.  So the line:

                              t = splay(i, t);

  causes it to search for item with key i in the tree rooted at t.  If it's
  there, it is splayed to the root.  If it isn't there, then the node put
  at the root is the last one before NULL that would have been reached in a
  normal binary search for i.  (It's a neighbor of i in the tree.)  This
  allows many other operations to be easily implemented, as shown below.

  [1] "Fundamentals of data structures in C", Horowitz, Sahni,
       and Anderson-Freed, Computer Science Press, pp 542-547.

  [2] "Data Structures and Their Algorithms", Lewis and Denenberg,
       Harper Collins, 1991, pp 243-251.
  [3] "Self-adjusting Binary Search Trees" Sleator and Tarjan,
       JACM Volume 32, No 3, July 1985, pp 652-686.
  [4] "Data Structure and Algorithm Analysis", Mark Weiss,
       Benjamin Cummins, 1992, pp 119-130.
  [5] "Data Structures, Algorithms, and Performance", Derick Wood,
       Addison-Wesley, 1993, pp 367-375.

  The following code was written by Daniel Sleator, and is released
  in the public domain. It has been heavily modified by Carmelo Piccione,
  (cep@andrew.cmu.edu), to suit personal needs, 
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "fatal.h"

#include "splaytree_types.h"
#include "splaytree.h"



static inline splaynode_t * splay (void * key, splaynode_t * t, int * match_type, int (*compare)());
static inline int free_splaynode(splaynode_t * splaynode, void (*free_key)());
static inline void splay_traverse_helper (void (*func_ptr)(), splaynode_t * splaynode);
static inline splaynode_t * splay_delete_helper(void * key, splaynode_t * t, int (*compare)(), void (*free_key)());
static inline void splay_find_above_min_helper(void * max_key, void ** closest_key, splaynode_t * root, int (*compare)());
static inline void splay_find_below_max_helper(void * max_key, void ** closest_key, splaynode_t * root, int (*compare)());
static inline splaynode_t * new_splaynode(int type, void * key, void * data);
static inline int splay_insert_node(splaynode_t * splaynode, splaytree_t * splaytree);
static inline int splay_rec_size(splaynode_t * splaynode);

/* Creates a splay tree given a compare key function, copy key function, and free key function.
   Ah yes, the wonders of procedural programming */
inline splaytree_t * create_splaytree(int (*compare)(), void * (*copy_key)(), void (*free_key)()) {

  splaytree_t * splaytree;

  /* Allocate memory for the splaytree struct */
  if ((splaytree = (splaytree_t*)malloc(sizeof(splaytree_t))) == NULL)
    return NULL;

  /* Set struct entries */
  splaytree->root = NULL;
  splaytree->compare = compare;
  splaytree->copy_key = copy_key;
  splaytree->free_key = free_key;
  
  /* Return instantiated splay tree */
  return splaytree;
}

/* Destroys a splay tree */
inline int destroy_splaytree(splaytree_t * splaytree) {

  /* Null argument check */
  if (splaytree == NULL)
    return FAILURE;

  /* Recursively free all splaynodes in tree */
  free_splaynode(splaytree->root, splaytree->free_key);

  /* Free the splaytree struct itself */
  free(splaytree);
  
  /* Done, return success */
  return SUCCESS;

}

/* Recursively free all the splaynodes */
static inline int free_splaynode(splaynode_t * splaynode, void (*free_key)()) {

  /* Ok if this happens, a recursive base case */
  if (splaynode == NULL)
    return SUCCESS;

  /* Free left node */
  free_splaynode(splaynode->left, free_key);
  
  /* Free right node */
  free_splaynode(splaynode->right, free_key);
  
  /* Free this node's key */
  free_key(splaynode->key);
  
  /* Note that the data pointers are not freed here.
     Should be freed with a splay traversal function */
  
  /* Free the splaynode structure itself */
  free(splaynode);
 
  /* Finished, return success */
  return SUCCESS;

}

/* Traverses the entire splay tree with the given function func_ptr */
inline void splay_traverse(void (*func_ptr)(), splaytree_t * splaytree) {

  /* Null argument check */

  if (splaytree == NULL)
    return; 
  if (func_ptr == NULL)
	return;
  
  /* Call recursive helper function */
  splay_traverse_helper(func_ptr, splaytree->root);

  return;
}

/* Helper function to traverse the entire splaytree */
static inline void splay_traverse_helper (void (*func_ptr)(), splaynode_t * splaynode) {  

  /* Normal if this happens, its a base case of recursion */
  if (splaynode == NULL)
    return;

  /* Recursively traverse to the left */
  splay_traverse_helper(func_ptr, splaynode->left);
  
  
  /* Node is a of regular type, so its ok to perform the function on it */
  if (splaynode->type == REGULAR_NODE_TYPE)
  	func_ptr(splaynode->data);
  
  /* Node is of symbolic link type, do nothing */
  else if (splaynode->type == SYMBOLIC_NODE_TYPE)
	;
  
  /* Unknown node type */
  else
    ;
  
  /* Recursively traverse to the right */
  splay_traverse_helper(func_ptr, splaynode->right);

  /* Done */
  return;
}

/* Find the node corresponding to the given key in splaytree, return its data pointer */
inline void * splay_find(void * key, splaytree_t * splaytree) {

  splaynode_t * splaynode;
  int match_type;

  if (key == NULL)
	  return NULL;
  
  if (splaytree == NULL)
	  return NULL;
  
  splaynode = splaytree->root;
  
  /* Bring the targeted splay node to the top of the splaytree */
  splaynode = splay(key, splaynode, &match_type, splaytree->compare);
  splaytree->root = splaynode;
  
  
  /* We only want perfect matches, so return null when match isn't perfect */
  if (match_type == CLOSEST_MATCH) 
    return NULL;

  /* This shouldn't happen because of the match type check, but whatever */
  if (splaytree->root == NULL)
	  return NULL;
  
  /* Node is a regular type, return its data pointer */
  if (splaytree->root->type == REGULAR_NODE_TYPE) /* regular node */
  	return splaytree->root->data;
  
  /* If the node is a symlink, pursue one link */
  if (splaytree->root->type == SYMBOLIC_NODE_TYPE) /* symbolic node */
	return ((splaynode_t*)splaytree->root->data)->data;
    
  
  /* Unknown type */
  return NULL;
}

/* Gets the splaynode that the given key points to */
inline splaynode_t * get_splaynode_of(void * key, splaytree_t * splaytree) {

  splaynode_t * splaynode;
  int match_type;
  
  /* Null argument checks */	
  if (splaytree == NULL)
	  return NULL;
  
  if (key == NULL)
	  return NULL;
  
  splaynode = splaytree->root;

  /* Find the splaynode */
  splaynode = splay(key, splaynode, &match_type, splaytree->compare);
  splaytree->root = splaynode;
 
  /* Only perfect matches are valid */
  if (match_type == CLOSEST_MATCH)
    return NULL;

  /* Return the perfect match splay node */
  return splaynode;
}

/* Finds the desired node, and changes the tree such that it is the root */
static inline splaynode_t * splay (void * key, splaynode_t * t, int * match_type, int (*compare)()) {
  
/* Simple top down splay, not requiring key to be in the tree t. 
   What it does is described above. */
 
    splaynode_t N, *l, *r, *y;
    *match_type = CLOSEST_MATCH;
  
	if (t == NULL) return t;
    N.left = N.right = NULL;
    l = r = &N;
  
    for (;;) {
	if (compare(key, t->key) < 0) {
	    if (t->left == NULL) break;
	    if (compare(key, t->left->key) < 0) {
		y = t->left;                           /* rotate right */
		t->left = y->right;
		y->right = t;
		t = y;
		if (t->left == NULL) break;
	    }
	    r->left = t;                               /* link right */
	    r = t;
	    t = t->left;
	} else if (compare(key, t->key) > 0) {
	    if (t->right == NULL) break;
	    if (compare(key, t->right->key) > 0) {
		y = t->right;                          /* rotate left */
		t->right = y->left;
		y->left = t;
		t = y;
		if (t->right == NULL) break;
	    }
	    l->right = t;                              /* link left */
	    l = t;
	    t = t->right;
	} else {
	  *match_type = PERFECT_MATCH;
	  break;
	}
    }
    l->right = t->left;                                /* assemble */
    r->left = t->right;
    t->left = N.right;
    t->right = N.left;
    
    return t;

    //return NULL;
}

/* Deletes a splay node from a splay tree. If the node doesn't exist
   then nothing happens */
inline int splay_delete(void * key, splaytree_t * splaytree) {
  
  splaynode_t * splaynode;

  /* Use helper function to delete the node and return the resulting tree */
  if ((splaynode = splay_delete_helper(key, splaytree->root, splaytree->compare, splaytree->free_key)) == NULL)
	  return FAILURE;
  
  /* Set new splaytree root equal to the returned splaynode after deletion */
  splaytree->root = splaynode;
  
  /* Finished, no errors */
  return SUCCESS;
}

/* Deletes a splay node */
static inline splaynode_t * splay_delete_helper(void * key, splaynode_t * splaynode, int (*compare)(), void (*free_key)()) {
	
    splaynode_t * new_root;
    int match_type;
	
	/* Argument check */	
    if (splaynode == NULL) 
		return NULL;
	
    splaynode = splay(key, splaynode, &match_type, compare);
	
	/* If entry wasn't found, quit here */
	if (match_type == CLOSEST_MATCH) 
		return NULL;
	
	/* If the targeted node's left pointer is null, then set the new root
	   equal to the splaynode's right child */
	if (splaynode->left == NULL) {
	    new_root = splaynode->right;
	} 
	
	/* Otherwise, do something I don't currently understand */
	else {
	    new_root = splay(key, splaynode->left, &match_type, compare);
	    new_root->right = splaynode->right;
	}

	/* Set splay nodes children pointers to null */
	splaynode->left = splaynode->right = NULL;
	
	/* Free the splaynode (and only this node since its children are now empty */
	free_splaynode(splaynode, free_key);
	
	/* Return the resulting tree */
	return new_root;
	
}

/* Create a new splay node type */
static inline splaynode_t * new_splaynode(int type, void * key, void * data) {
	splaynode_t * splaynode;	
	/* Argument checks */
	if (data == NULL)
		return NULL;
	
	if (key == NULL)
		return NULL;
	
	/* Creates the new splay node struct */
	if ((splaynode = (splaynode_t*)malloc(sizeof(splaynode_t))) == NULL)
		return NULL;
	
	splaynode->data = data;
	splaynode->type = type;
	splaynode->key = key;
	
	/* Return the new splay node */
	return splaynode;
}

/* Inserts a link into the splay tree */
inline int splay_insert_link(void * alias_key, void * orig_key, splaytree_t * splaytree) {

   splaynode_t * splaynode, * data_node;
   void * key_clone;

   /* Null arguments */	
   if (splaytree == NULL)
		return FAILURE;
   
   if (alias_key == NULL)
	   	return FAILURE;

   if (orig_key == NULL)
	   	return FAILURE;
   
   /* Find the splaynode corresponding to the original key */
   if ((data_node = get_splaynode_of(orig_key, splaytree)) == NULL)
	   return FAILURE;
   
   /* Create a new splay node of symbolic link type */
   if ((splaynode = new_splaynode(SYMBOLIC_NODE_TYPE, (key_clone = splaytree->copy_key(alias_key)), data_node)) == NULL) {
		splaytree->free_key(key_clone);
		return OUTOFMEM_ERROR;
   }

   /* Insert the splaynode into the given splaytree */
   if ((splay_insert_node(splaynode, splaytree)) < 0) {
     splaynode->left=splaynode->right = NULL;
     free_splaynode(splaynode, splaytree->free_key);
     return FAILURE;
   }		
	
   /* Done, return success */
   return SUCCESS;
}	

/* Inserts 'data' into the 'splaytree' paired with the passed 'key' */
inline int splay_insert(void * data, void * key, splaytree_t * splaytree) {

	splaynode_t * splaynode;
	void * key_clone;
	
	/* Null argument checks */
	if (splaytree == NULL) {
	    return FAILURE;
	}
	
	if (key == NULL)
		return FAILURE;
	
	/* Clone the key argument */
	key_clone = splaytree->copy_key(key);

	/* Create a new splaynode (of regular type) */
	if ((splaynode = new_splaynode(REGULAR_NODE_TYPE, key_clone, data)) == NULL) {
		splaytree->free_key(key_clone);
		return OUTOFMEM_ERROR;		
	}
       
	
	/* Inserts the splaynode into the splaytree */
	if (splay_insert_node(splaynode, splaytree) < 0) {
	  splaynode->left=splaynode->right=NULL;
	  free_splaynode(splaynode, splaytree->free_key);
	  return FAILURE;		
	}	
     

	return SUCCESS;
}

/* Helper function to insert splaynodes into the splaytree */
static inline int splay_insert_node(splaynode_t * splaynode, splaytree_t * splaytree) {
  int match_type;
  int cmpval;
  void * key;
  splaynode_t * t;
	
  /* Null argument checks */
  if (splaytree == NULL)
    return FAILURE;

  if (splaynode == NULL)
	return FAILURE;
  
  key = splaynode->key;
  
  t = splaytree->root; 


  /* Root is null, insert splaynode here */
  if (t == NULL) {
	splaynode->left = splaynode->right = NULL;
	splaytree->root = splaynode;
	return SUCCESS;

  }
  
  t = splay(key, t, &match_type, splaytree->compare);
  
  if ((cmpval = splaytree->compare(key,t->key)) < 0) {
	splaynode->left = t->left;
	splaynode->right = t;
	t->left = NULL;
	splaytree->root = splaynode;
	return SUCCESS;

  } 

  else if (cmpval > 0) {
	splaynode->right = t->right;
	splaynode->left = t;
	t->right = NULL; 
	splaytree->root = splaynode;
	return SUCCESS;
   } 
   
   /* Item already exists in tree, don't reinsert */
  else {
    
    return FAILURE;
  }
}

/* Returns the 'maximum' key that is less than the given key in the splaytree */
inline void * splay_find_below_max(void * key, splaytree_t * splaytree) {
	
	void * closest_key;
	
	if (splaytree == NULL)
		return NULL;
	if (splaytree->root == NULL)
		return NULL;
	if (key == NULL)
		return NULL;
	
	closest_key = NULL;
	
	splay_find_below_max_helper(key, &closest_key, splaytree->root, splaytree->compare);

	if (closest_key == NULL) return NULL;
	return splay_find(closest_key, splaytree);
}


/* Returns the 'minimum' key that is greater than the given key in the splaytree */
inline void * splay_find_above_min(void * key, splaytree_t * splaytree) {
	
	void * closest_key;
	
	if (splaytree == NULL)
		return NULL;
	if (splaytree->root == NULL)
		return NULL;
	if (key == NULL)
		return NULL;
	closest_key = NULL;
	
	splay_find_above_min_helper(key, &closest_key, splaytree->root, splaytree->compare);

	if (closest_key == NULL) { 
		return NULL;
	}
	
	return splay_find(closest_key, splaytree);
}

/* Helper function */
static inline void splay_find_below_max_helper(void * min_key, void ** closest_key, splaynode_t * root, int (*compare)()) {

		/* Empty root, return*/	
		if (root == NULL)
			return;
			
		/* The root key is less than the previously found closest key.
		   Also try to make the key non null if the value is less than the max key */
		
		if ((*closest_key == NULL) || (compare(root->key, *closest_key) < 0)) {
			
			/*  The root key is less than the given max key, so this is the
				smallest change from the given max key */
			if (compare(root->key, min_key) > 0) {
				
				*closest_key = root->key;
				
				/* Look right again in case even a greater key exists that is 
				   still less than the given max key */
				splay_find_below_max_helper(min_key, closest_key, root->left, compare);
			}
			
			/* The root key is greater than the given max key, and greater than 
			   the closest key, so search left */
			else {
				splay_find_below_max_helper(min_key, closest_key, root->right, compare);				
			}	
		}	
		
		/* The root key is less than the found closest key, search right */
		else {
				splay_find_below_max_helper(min_key, closest_key, root->left, compare);				
		}
	
}

/* Helper function */
static inline void splay_find_above_min_helper(void * max_key, void ** closest_key, splaynode_t * root, int (*compare)()) {

		/* Empty root, stop */	
		if (root == NULL)
			return;
			
		/* The root key is greater than the previously found closest key.
		   Also try to make the key non null if the value is less than the min key */
		
		if ((*closest_key == NULL) || (compare(root->key, *closest_key) > 0)) {
			
			/*  The root key is greater than the given min key, so this is the
				smallest change from the given min key */
			if (compare(root->key, max_key) < 0) {
				
				*closest_key = root->key;
				
			   /* Look left again in case even a smaller key exists that is 
				  still greater than the given min key */
				splay_find_above_min_helper(max_key, closest_key, root->right, compare);
			}
			
			/* The root key is less than the given min key, and less than 
			   the closest key, so search right */
			else {
				splay_find_above_min_helper(max_key, closest_key, root->left, compare);				
			}	
		}	
		
		/* The root key is greater than the found closest key, search left */
		else {
				splay_find_above_min_helper(max_key, closest_key, root->right, compare);				
		}
}	

/* Find the minimum entry of the splay tree */
inline void * splay_find_min(splaytree_t * t) {

	splaynode_t * splaynode;
	
	if (t == NULL)
		return NULL;
	if (t->root == NULL)
		return NULL;
	
	splaynode = t->root;
	
	while (splaynode->left != NULL)
		splaynode= splaynode->left;
	
	return splaynode->data;
}


/* Find the maximum entry of the splay tree */
inline void * splay_find_max(splaytree_t * t) {

	splaynode_t * splaynode;
	
	if (t == NULL)
		return NULL;
	if (t->root == NULL)
		return NULL;
	
	splaynode = t->root;
	 
	while (splaynode->right != NULL) {
	  printf("data:%d\n", *(int*)splaynode->key);
		splaynode = splaynode->right;
	}
	return splaynode->data;
}

inline int splay_size(splaytree_t * t) {

	if (t == NULL)
	  return 0;
	if (t->root == NULL)
	  return 0;
	
	return splay_rec_size(t->root);
	 
}

static inline int splay_rec_size(splaynode_t * splaynode) {

  if (!splaynode)
    return 0;

  return 1 + splay_rec_size(splaynode->left) + splay_rec_size(splaynode->right);

}
