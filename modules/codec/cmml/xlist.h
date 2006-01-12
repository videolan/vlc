/*****************************************************************************
 * xlist.h : a simple doubly linked list in C (header file)
 *****************************************************************************
 * Copyright (C) 2003-2004 Commonwealth Scientific and Industrial Research
 *                         Organisation (CSIRO) Australia
 * Copyright (C) 2000-2004 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Conrad Parker <Conrad.Parker@csiro.au>
 *          Andre Pang <Andre.Pang@csiro.au>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/


#ifndef __XLIST__
#define __XLIST__

/**
 * A doubly linked list
 */
typedef struct _XList XList;

struct _XList {
  XList * prev;
  XList * next;
  void * data;
};

/**
 * Signature of a cloning function.
 */
typedef void * (*XCloneFunc) (void * data);

/**
 * Signature of a freeing function.
 */
typedef void * (*XFreeFunc) (void * data);

/** Create a new list
 * \return a new list
 */
XList * xlist_new (void);

/**
 * Clone a list using the default clone function
 * \param list the list to clone
 * \returns a newly cloned list
 */
XList * xlist_clone (XList * list);

/**
 * Clone a list using a custom clone function
 * \param list the list to clone
 * \param clone the function to use to clone a list item
 * \returns a newly cloned list
 */
XList * xlist_clone_with (XList * list, XCloneFunc clone);

/**
 * Return the tail element of a list
 * \param list the list
 * \returns the tail element
 */
XList * xlist_tail (XList * list);

/**
 * Prepend a new node to a list containing given data
 * \param list the list
 * \param data the data element of the newly created node
 * \returns the new list head
 */
XList * xlist_prepend (XList * list, void * data);

/**
 * Append a new node to a list containing given data
 * \param list the list
 * \param data the data element of the newly created node
 * \returns the head of the list
 */
XList * xlist_append (XList * list, void * data);

/**
 * Add a new node containing given data before a given node
 * \param list the list
 * \param data the data element of the newly created node
 * \param node the node before which to add the newly created node
 * \returns the head of the list (which may have changed)
 */
XList * xlist_add_before (XList * list, void * data, XList * node);

/**
 * Add a new node containing given data after a given node
 * \param list the list
 * \param data the data element of the newly created node
 * \param node the node after which to add the newly created node
 * \returns the head of the list
 */
XList * xlist_add_after (XList * list, void * data, XList * node);

/**
 * Find the first node containing given data in a list
 * \param list the list
 * \param data the data element to find
 * \returns the first node containing given data, or NULL if it is not found
 */
XList * xlist_find (XList * list, void * data);

/**
 * Remove a node from a list
 * \param list the list
 * \param node the node to remove
 * \returns the head of the list (which may have changed)
 */
XList * xlist_remove (XList * list, XList * node);

/**
 * Query the number of items in a list
 * \param list the list
 * \returns the number of nodes in the list
 */
int xlist_length (XList * list);

/**
 * Query if a list is empty, ie. contains no items
 * \param list the list
 * \returns 1 if the list is empty, 0 otherwise
 */
int xlist_is_empty (XList * list);

/**
 * Query if the list is singleton, ie. contains exactly one item
 * \param list the list
 * \returns 1 if the list is singleton, 0 otherwise
 */
int xlist_is_singleton (XList * list);

/**
 * Free a list, using a given function to free each data element
 * \param list the list
 * \param free_func a function to free each data element
 * \returns NULL on success
 */
XList * xlist_free_with (XList * list, XFreeFunc free_func);

/**
 * Free a list, using anx_free() to free each data element
 * \param list the list
 * \returns NULL on success
 */
XList * xlist_free (XList * list);

#endif /* __XLIST__ */


