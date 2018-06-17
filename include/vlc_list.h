/******************************************************************************
 * vlc_list.h
 ******************************************************************************
 * Copyright © 2018 Rémi Denis-Courmont
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

#ifndef VLC_LIST_H
# define VLC_LIST_H 1

# include <stdalign.h>
# include <stdbool.h>

/**
 * \defgroup list Linked lists
 * \ingroup cext
 * @{
 * \file
 * This provides convenience helpers for linked lists.
 */

/**
 * Doubly-linked list node.
 *
 * This data structure provides a doubly-linked list node.
 * It must be embedded in each member of a list as a node proper.
 * It also serves as a list head in the object containing the list.
 */
struct vlc_list
{
    struct vlc_list *prev;
    struct vlc_list *next;
};

/**
 * Static initializer for a list head.
 */
#define VLC_LIST_INITIALIZER(h) { h, h }

/**
 * Initializes an empty list head.
 */
static inline void vlc_list_init(struct vlc_list *restrict head)
{
    head->prev = head;
    head->next = head;
}

/**
 * Inserts an element in a list.
 *
 * \param node Node pointer of the element to insert [OUT].
 * \param prev Node pointer of the previous element.
 * \param next Node pointer of the next element.
 */
static inline void vlc_list_add_between(struct vlc_list *restrict node,
                                        struct vlc_list *prev,
                                        struct vlc_list *next)
{
    node->prev = prev;
    node->next = next;
    prev->next = node;
    next->prev = node;
}

/**
 * Inserts an element after another.
 *
 * \param node Node pointer of the element to insert [OUT].
 * \param prev Node pointer of the previous element.
 */
static inline void vlc_list_add_after(struct vlc_list *restrict node,
                                      struct vlc_list *prev)
{
    vlc_list_add_between(node, prev, prev->next);
}

/**
 * Inserts an element before another.
 *
 * \param node Node pointer of the element to insert [OUT].
 * \param prev Node pointer of the next element.
 */
static inline void vlc_list_add_before(struct vlc_list *restrict node,
                                       struct vlc_list *next)
{
    vlc_list_add_between(node, next->prev, next);
}

/**
 * Appends an element into a list.
 *
 * \param node Node pointer of the element to append to the list [OUT].
 * \param head Head pointer of the list to append the element to.
 */
static inline void vlc_list_append(struct vlc_list *restrict node,
                                   struct vlc_list *head)
{
    vlc_list_add_before(node, head);
}

/**
 * Prepends an element into a list.
 *
 * \param node Node pointer of the element to prepend to the list [OUT].
 * \param head Head pointer of the list to prepend the element to.
 */
static inline void vlc_list_prepend(struct vlc_list *restrict node,
                                    struct vlc_list *head)
{
    vlc_list_add_after(node, head);
}

/**
 * Removes an element from a list.
 *
 * \param node Node of the element to remove from a list.
 * \warning The element must be inside a list.
 * Otherwise the behaviour is undefined.
 */
static inline void vlc_list_remove(struct vlc_list *restrict node)
{
    struct vlc_list *prev = node->prev;
    struct vlc_list *next = node->next;

    prev->next = next;
    next->prev = prev;
}

/**
 * Replaces an element with another one.
 *
 * \param origin Node pointer of the element to remove from the list [IN].
 * \param substitute Node pointer of the replacement [OUT].
 */
static inline void vlc_list_replace(const struct vlc_list *original,
                                    struct vlc_list *restrict substitute)
{
    vlc_list_add_between(substitute, original->prev, original->next);
}

/**
 * Checks if a list is empty.
 *
 * \param head Head of the list to be checked [IN].
 *
 * \retval false The list is not empty.
 * \retval true The list is empty.
 *
 * \note Obviously the list must have been initialized.
 * Otherwise, the behaviour is undefined.
 */
static inline bool vlc_list_is_empty(const struct vlc_list *head)
{
    return head->next == head;
}

/**
 * Checks if an element is first in a list.
 *
 * \param node List node of the element [IN].
 * \param head Head of the list to be checked [IN].
 *
 * \retval false The element is not first (or is in another list).
 * \retval true The element is first.
 */
static inline bool vlc_list_is_first(const struct vlc_list *node,
                                     const struct vlc_list *head)
{
    return node->prev == head;
}

/**
 * Checks if an element is last in a list.
 *
 * \param node List node of the element [IN].
 * \param head Head of the list to be checked [IN].
 *
 * \retval false The element is not last (or is in another list).
 * \retval true The element is last.
 */
static inline bool vlc_list_is_last(const struct vlc_list *node,
                                    const struct vlc_list *head)
{
    return node->next == head;
}

/**
 * List iterator.
 */
struct vlc_list_it
{
    const struct vlc_list *head;
    struct vlc_list *current;
    struct vlc_list *next;
};

static inline
struct vlc_list_it vlc_list_it_start(const struct vlc_list *head)
{
    struct vlc_list *first = head->next;

    return (struct vlc_list_it){ head, first, first->next };
}

static inline bool vlc_list_it_continue(const struct vlc_list_it *restrict it)
{
    return it->current != it->head;
}

static inline void vlc_list_it_next(struct vlc_list_it *restrict it)
{
    struct vlc_list *next = it->next;

    it->current = next;
    it->next = next->next;
}

#define vlc_list_entry_aligned_size(p) \
    ((sizeof (*(p)) + sizeof (max_align_t) - 1) / sizeof (max_align_t))

#define vlc_list_entry_dummy(p) \
    (0 ? (p) : ((void *)(&(max_align_t[vlc_list_entry_aligned_size(p)]){})))

#define vlc_list_offset_p(p, member) \
    ((p) = vlc_list_entry_dummy(p), (char *)(&(p)->member) - (char *)(p))

#define vlc_list_entry_p(node, p, member) \
    (0 ? (p) : (void *)(((char *)(node)) - vlc_list_offset_p(p, member)))

/**
 * List iteration macro.
 *
 * This macro iterates over all elements (excluding the head) of a list,
 * in order from the first to the last.
 *
 * For each iteration, it sets the cursor variable to the current element.
 *
 * \param pos Cursor pointer variable identifier.
 * \param head Head pointer of the list to iterate [IN].
 * \param member Identifier of the member of the data type
 *               serving as list node.
 * \note It it safe to delete the current item while iterating.
 * It is however <b>not</b> safe to delete another item.
 */
#define vlc_list_foreach(pos, head, member) \
    for (struct vlc_list_it vlc_list_it_##pos = vlc_list_it_start(head); \
         vlc_list_it_continue(&(vlc_list_it_##pos)) \
          && ((pos) = vlc_list_entry_p((vlc_list_it_##pos).current, \
                                       pos, member), true); \
         vlc_list_it_next(&(vlc_list_it_##pos)))

/**
 * Converts a list node pointer to an element pointer.
 *
 * \param ptr list node pointer
 * \param type list data element type name
 * \param member list node member within the data element compound type
 */
#define vlc_list_entry(ptr, type, member) container_of(ptr, type, member)

static inline void *vlc_list_first_or_null(const struct vlc_list *head,
                                           size_t offset)
{
    if (vlc_list_is_empty(head))
        return NULL;
    return ((char *)(head->next)) - offset;
}

static inline void *vlc_list_last_or_null(const struct vlc_list *head,
                                          size_t offset)
{
    if (vlc_list_is_empty(head))
        return NULL;
    return ((char *)(head->prev)) - offset;
}

static inline void *vlc_list_prev_or_null(const struct vlc_list *head,
                                          struct vlc_list *node,
                                          size_t offset)
{
    if (vlc_list_is_first(node, head))
        return NULL;
    return ((char *)(node->prev)) - offset;
}

static inline void *vlc_list_next_or_null(const struct vlc_list *head,
                                          struct vlc_list *node,
                                          size_t offset)
{
    if (vlc_list_is_last(node, head))
        return NULL;
    return ((char *)(node->next)) - offset;
}

/**
 * Gets the first element.
 *
 * \param head Head of list whose last element to get [IN].
 *
 * \return the first entry in a list or NULL if empty.
 */
#define vlc_list_first_entry_or_null(head, type, member) \
        ((type *)vlc_list_first_or_null(head, offsetof (type, member)))

/**
 * Gets the last element.
 *
 * \param head Head of list whose last element to get [IN].
 *
 * \return the last entry in a list or NULL if empty.
 */
#define vlc_list_last_entry_or_null(head, type, member) \
        ((type *)vlc_list_last_or_null(head, offsetof (type, member)))

#define vlc_list_prev_entry_or_null(head, entry, type, member) \
        ((type *)vlc_list_prev_or_null(head, &(entry)->member, \
                                       offsetof (type, member)))
#define vlc_list_next_entry_or_null(head, entry, type, member) \
        ((type *)vlc_list_next_or_null(head, &(entry)->member, \
                                       offsetof (type, member)))

/** \todo Merging lists, splitting lists. */

/** @} */

#endif /* VLC_LIST_H */
