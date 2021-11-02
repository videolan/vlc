/******************************************************************************
 * vlc_vector.h
 ******************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
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
#ifndef VLC_VECTOR_H
#define VLC_VECTOR_H

#include <stdbool.h>
#include <stddef.h>

/**
 * \defgroup vector Vector
 * \ingroup cext
 * @{
 * \file
 * This provides convenience helpers for vectors.
 */

/**
 * Vector struct body.
 *
 * A vector is a dynamic array, managed by the vlc_vector_* helpers.
 *
 * It is generic over the type of its items, so it is implemented as macros.
 *
 * To use a vector, a new type must be defined:
 *
 * \code
 * struct vec_int VLC_VECTOR(int);
 * \endcode
 *
 * The struct may be anonymous:
 *
 * \code
 * struct VLC_VECTOR(const char *) names;
 * \endcode
 *
 * It is convenient to define a typedef to an anonymous structure:
 *
 * \code
 * typedef struct VLC_VECTOR(int) vec_int_t;
 * \endcode
 *
 * Vector size is accessible via `vec.size`, and items are intended to be
 * accessed directly, via `vec.data[i]`.
 *
 * Functions and macros having name ending with '_' are private.
 */
#define VLC_VECTOR(type) \
{ \
    size_t cap; \
    size_t size; \
    type *data; \
}

/**
 * Static initializer for a vector.
 */
#define VLC_VECTOR_INITIALIZER { 0, 0, NULL }

/**
 * Initialize an empty vector.
 */
#define vlc_vector_init(pv) (void) \
( \
    /* cannot be implemented as do-while(0), called from vlc_vector_clear() */ \
    (pv)->cap = 0, \
    (pv)->size = 0, \
    (pv)->data = NULL \
)

/**
 * Destroy a vector.
 *
 * The vector may not be used anymore unless vlc_vector_init() is called.
 */
#define vlc_vector_destroy(pv) \
    free((pv)->data)

/**
 * Clear a vector.
 *
 * Remove all items from the vector.
 */
#define vlc_vector_clear(pv) \
( \
    /* cannot be implemented as do-while(0), called from vlc_vector_resize_() */ \
    vlc_vector_destroy(pv), \
    vlc_vector_init(pv) \
)

/**
 * The minimal allocation size, in number of items.
 *
 * Private.
 */
#define VLC_VECTOR_MINCAP_ ((size_t) 10)

static inline size_t
vlc_vector_min_(size_t a, size_t b)
{
    return a < b ? a : b;
}

static inline size_t
vlc_vector_max_(size_t a, size_t b)
{
    return a > b ? a : b;
}

static inline size_t
vlc_vector_between_(size_t x, size_t min, size_t max)
{
    return vlc_vector_max_(min, vlc_vector_min_(max, x));
}

static inline size_t
vlc_vector_enforce_size_t_(size_t value)
{
    return value;
}

#define VLC_VECTOR_FAILFLAG_ (~(((size_t) -1) >> 1)) /* only the MSB */

/**
 * Realloc data and update vector fields.
 *
 * On reallocation success, return the reallocated array and update the vector
 * capacity and size.
 *
 * On reallocation failure, return `ptr`, keep `*psize` untouched, and set the
 * failflag in `*pcap` to indicate allocation failure (to be consumed by
 * `vlc_vector_test_and_reset_failflag_()`).
 *
 * This weird behavior allows to simultaneously:
 *  - not require compiler extensions like "statement expressions"
 *  - keep the vector data, size and capacity unchanged on reallocation failure
 *  - not require output variables other than vector fields from the caller
 *  - not violate the strict aliasing rules
 *  - report the reallocation status (success or failure)
 *
 * Private.
 *
 * \param ptr the current data to realloc
 * \param count the requested capacity, in number of items
 * \param size the size of one item
 * \param[in,out] pcap a pointer to the `cap` field of the vector
 * \param[in,out] psize a pointer to the `size` field of the vector
 * \return the reallocated array, or `ptr` if reallocation failed
 */
static inline void *
vlc_vector_reallocdata_(void *ptr, size_t count, size_t size,
                        size_t *restrict pcap, size_t *restrict psize)
{
    void *n = vlc_reallocarray(ptr, count, size);
    if (!n)
    {
        /* this vector implementation guarantees that the capacity may not
         * exceed SIZE_MAX/2 (to prevent overflows), so we can use the MSB to
         * report allocation failure */
        *pcap |= VLC_VECTOR_FAILFLAG_;
        return ptr;
    }
    *pcap = count;
    *psize = vlc_vector_min_(*psize, count);
    return n;
}

/**
 * Test and reset the fail flag.
 *
 * \retval true if the flag was set
 * \retval false if the flag was not set
 */
static inline bool
vlc_vector_test_and_reset_failflag_(size_t *pcap)
{
    if (*pcap & VLC_VECTOR_FAILFLAG_)
    {
        *pcap &= ~VLC_VECTOR_FAILFLAG_;
        return true;
    }
    return false;
}

/**
 * Realloc the underlying array to `newcap`.
 *
 * Private.
 *
 * \param pv a pointer to the vector
 * \param newcap (size_t) the requested size
 * \retval true if no allocation failed
 * \retval false on allocation failure (the vector is left untouched)
 */
#define vlc_vector_realloc_(pv, newcap) \
( \
    (pv)->data = vlc_vector_reallocdata_((pv)->data, newcap, \
                                         sizeof(*(pv)->data), \
                                         &(pv)->cap, &(pv)->size), \
    !vlc_vector_test_and_reset_failflag_(&(pv)->cap) \
)

/**
 * Resize the vector to `newcap` exactly.
 *
 * If `newcap` is 0, the vector is cleared.
 *
 * Private.
 *
 * \param pv a pointer to the vector
 * \param newcap (size_t) the requested capacity
 * \retval true if no allocation failed
 * \retval false on allocation failure (the vector is left untouched)
 */
#define vlc_vector_resize_(pv, newcap) \
( \
    (pv)->cap == (newcap) /* nothing to do */ || \
    ( \
        (newcap) > 0 ? vlc_vector_realloc_(pv, newcap) \
                     : (vlc_vector_clear(pv), true) \
    ) \
)

static inline size_t
vlc_vector_growsize_(size_t value)
{
    /* integer multiplication by 1.5 */
    return value + (value >> 1);
}

/* SIZE_MAX/2 to fit in ssize_t, and so that cap*1.5 does not overflow. */
#define vlc_vector_max_cap_(pv) (SIZE_MAX / 2 / sizeof(*(pv)->data))

/**
 * Increase the capacity of the vector to at least `mincap`.
 *
 * \param pv a pointer to the vector
 * \param mincap (size_t) the requested capacity
 * \retval true if no allocation failed
 * \retval false on allocation failure (the vector is left untouched)
 */
#define vlc_vector_reserve(pv, mincap) \
    /* avoid to allocate tiny arrays (< VLC_VECTOR_MINCAP_) */ \
    vlc_vector_reserve_internal_(pv, \
                                 vlc_vector_max_(mincap, VLC_VECTOR_MINCAP_))

#define vlc_vector_reserve_internal_(pv, mincap) \
( \
    (mincap) <= (pv)->cap /* nothing to do */ || \
    ( \
        (mincap) <= vlc_vector_max_cap_(pv) /* not too big */ && \
        vlc_vector_realloc_(pv, \
                        /* multiply by 1.5, force between [mincap, maxcap] */ \
                        vlc_vector_between_(vlc_vector_growsize_((pv)->cap), \
                                            mincap, \
                                            vlc_vector_max_cap_(pv))) \
    ) \
)

/**
 * Resize the vector so that its capacity equals its actual size.
 *
 * \param pv a pointer to the vector
 */
#define vlc_vector_shrink_to_fit(pv) \
    (void) /* decreasing the size may not fail */ \
    vlc_vector_resize_(pv, (pv)->size)

/**
 * Resize the vector down automatically.
 *
 * Shrink only when necessary (in practice when cap > (size+5)*1.5)
 *
 * \param pv a pointer to the vector
 */
#define vlc_vector_autoshrink(pv) (void) \
( \
    (pv)->cap <= VLC_VECTOR_MINCAP_ /* do not shrink to tiny length */ || \
    (pv)->cap < vlc_vector_growsize_((pv)->size+5) /* no need to shrink */ || \
    vlc_vector_resize_(pv, vlc_vector_max_((pv)->size+5, VLC_VECTOR_MINCAP_)) \
)

#define vlc_vector_check_same_ptr_type_(a, b) \
    (void) ((a) == (b)) /* warn on type mismatch */

/**
 * Push an item at the end of the vector.
 *
 * The amortized complexity is O(1).
 *
 * \param pv a pointer to the vector
 * \param item the item to append
 * \retval true if no allocation failed
 * \retval false on allocation failure (the vector is left untouched)
 */
#define vlc_vector_push(pv, item) \
( \
    vlc_vector_reserve(pv, (pv)->size + 1) && \
    ( \
        (pv)->data[(pv)->size++] = (item), \
        true \
    ) \
)

/**
 * Append `count` items at the end of the vector.
 *
 * \param pv a pointer to the vector
 * \param items the items array to append
 * \param count the number of items in the array
 * \retval true if no allocation failed
 * \retval false on allocation failure (the vector is left untouched)
 */
#define vlc_vector_push_all(pv, items, count) \
    vlc_vector_push_all_internal_(pv, items, vlc_vector_enforce_size_t_(count))

#define vlc_vector_push_all_internal_(pv, items, count) \
( \
    vlc_vector_check_same_ptr_type_((pv)->data, items), \
    vlc_vector_reserve(pv, (pv)->size + (count)) && \
    ( \
        memcpy(&(pv)->data[(pv)->size], items, (count) * sizeof(*(pv)->data)), \
        (pv)->size += (count), \
        true \
    ) \
)

/**
 * Insert an hole of size `count` to the given index.
 *
 * The items in range [index; size-1] will be moved. The items in the hole are
 * left uninitialized.
 *
 * \param pv a pointer to the vector
 * \param index the index where the hole is to be inserted
 * \param count the number of items in the hole
 * \retval true if no allocation failed
 * \retval false on allocation failure (the vector is left untouched)
 */
#define vlc_vector_insert_hole(pv, index, count) \
    vlc_vector_insert_hole_internal_(pv, vlc_vector_enforce_size_t_(index), \
                                     vlc_vector_enforce_size_t_(count))

#define vlc_vector_insert_hole_internal_(pv, index, count) \
( \
    vlc_vector_reserve(pv, (pv)->size + (count)) && \
    ( \
        (index) == (pv)->size || \
        ( \
            memmove(&(pv)->data[(index) + (count)], \
                    &(pv)->data[index], \
                    ((pv)->size - (index)) * sizeof(*(pv)->data)), \
            true \
        ) \
    ) && \
    ( \
        (pv)->size += (count), \
        true \
    ) \
)

/**
 * Insert an item at the given index.
 *
 * The items in range [index; size-1] will be moved.
 *
 * \param pv a pointer to the vector
 * \param index the index where the item is to be inserted
 * \param item the item to append
 * \retval true if no allocation failed
 * \retval false on allocation failure (the vector is left untouched)
 */
#define vlc_vector_insert(pv, index, item) \
( \
    vlc_vector_insert_hole(pv, index, 1) && \
    ( \
        (pv)->data[index] = (item), \
        true \
    ) \
)

/**
 * Insert `count` items at the given index.
 *
 * The items in range [index; size-1] will be moved.
 *
 * \param pv a pointer to the vector
 * \param index the index where the items are to be inserted
 * \param items the items array to append
 * \param count the number of items in the array
 * \retval true if no allocation failed
 * \retval false on allocation failure (the vector is left untouched)
 */
#define vlc_vector_insert_all(pv, index, items, count) \
( \
    vlc_vector_check_same_ptr_type_((pv)->data, items), \
    vlc_vector_insert_hole(pv, index, count) && \
    ( \
        memcpy(&(pv)->data[index], items, (count) * sizeof(*(pv)->data)), \
        true \
    ) \
)

/** Reverse a char array in place. */
static inline void
vlc_vector_reverse_array_(char *array, size_t len)
{
    for (size_t i = 0; i < len / 2; ++i)
    {
        char c = array[i];
        array[i] = array[len - i - 1];
        array[len - i - 1] = c;
    }
}

/**
 * Right-rotate a (char) array in place.
 *
 * For example, left-rotating a char array containing {1, 2, 3, 4, 5, 6} with
 * distance 4 will result in {5, 6, 1, 2, 3, 4}.
 *
 * Private.
 */
static inline void
vlc_vector_rotate_array_left_(char *array, size_t len, size_t distance)
{
    vlc_vector_reverse_array_(array, distance);
    vlc_vector_reverse_array_(&array[distance], len - distance);
    vlc_vector_reverse_array_(array, len);
}

/**
 * Right-rotate a (char) array in place.
 *
 * For example, left-rotating a char array containing {1, 2, 3, 4, 5, 6} with
 * distance 2 will result in {5, 6, 1, 2, 3, 4}.
 *
 * Private.
 */
static inline void
vlc_vector_rotate_array_right_(char *array, size_t len, size_t distance)
{
    vlc_vector_rotate_array_left_(array, len, len - distance);
}

/**
 * Move items in a (char) array in place.
 *
 * Move slice [index, count] to target.
 */
static inline void
vlc_vector_move_(char *array, size_t index, size_t count, size_t target)
{
    if (index < target)
        vlc_vector_rotate_array_left_(&array[index], target - index + count,
                                      count);
    else
        vlc_vector_rotate_array_right_(&array[target], index - target + count,
                                       count);
}

/**
 * Move a slice of items to a given target index.
 *
 * The items in range [index; count] will be moved so that the *new* position
 * of the first item is `target`.
 *
 * \param pv a pointer to the vector
 * \param index the index of the first item to move
 * \param count the number of items to move
 * \param target the new index of the moved slice
 */
#define vlc_vector_move_slice(pv, index, count, target) \
    vlc_vector_move_slice_internal_(pv, \
                                    vlc_vector_enforce_size_t_(index), \
                                    vlc_vector_enforce_size_t_(count), \
                                    vlc_vector_enforce_size_t_(target))

#define vlc_vector_move_slice_internal_(pv, index, count, target) \
    vlc_vector_move_((char *) (pv)->data, \
                     (index) * sizeof((pv)->data[0]), \
                     (count) * sizeof((pv)->data[0]), \
                     (target) * sizeof((pv)->data[0]))

/**
 * Move an item to a given target index.
 *
 * The items will be moved so that its *new* position is `target`.
 *
 * \param pv a pointer to the vector
 * \param index the index of the item to move
 * \param target the new index of the moved item
 */
#define vlc_vector_move(pv, index, target) \
    vlc_vector_move_slice(pv, index, 1, target)

/**
 * Remove a slice of items, without shrinking the array.
 *
 * If you have no good reason to use the _noshrink() version, use
 * vlc_vector_remove_slice() instead.
 *
 * The items in range [index+count; size-1] will be moved.
 *
 * \param pv a pointer to the vector
 * \param index the index of the first item to remove
 * \param count the number of items to remove
 */
#define vlc_vector_remove_slice_noshrink(pv, index, count) \
    vlc_vector_remove_slice_noshrink_internal_(pv, \
            vlc_vector_enforce_size_t_(index), \
            vlc_vector_enforce_size_t_(count))

#define vlc_vector_remove_slice_noshrink_internal_(pv, index, count) \
    do { \
        if ((index) + (count) < (pv)->size) \
            memmove(&(pv)->data[index], \
                    &(pv)->data[(index) + (count)], \
                    ((pv)->size - (index) - (count)) * sizeof(*(pv)->data)); \
        (pv)->size -= (count); \
    } while (0)

/**
 * Remove a slice of items.
 *
 * The items in range [index+count; size-1] will be moved.
 *
 * \param pv a pointer to the vector
 * \param index the index of the first item to remove
 * \param count the number of items to remove
 */
#define vlc_vector_remove_slice(pv, index, count) \
    do { \
        vlc_vector_remove_slice_noshrink(pv, index, count); \
        vlc_vector_autoshrink(pv); \
    } while (0)

/**
 * Remove an item, without shrinking the array.
 *
 * If you have no good reason to use the _noshrink() version, use
 * vlc_vector_remove() instead.
 *
 * The items in range [index+1; size-1] will be moved.
 *
 * \param pv a pointer to the vector
 * \param index the index of item to remove
 */
#define vlc_vector_remove_noshrink(pv, index) \
    vlc_vector_remove_slice_noshrink(pv, index, 1)

/**
 * Remove an item.
 *
 * The items in range [index+1; size-1] will be moved.
 *
 * \param pv a pointer to the vector
 * \param index the index of item to remove
 */
#define vlc_vector_remove(pv, index) \
    do { \
        vlc_vector_remove_noshrink(pv, index); \
        vlc_vector_autoshrink(pv); \
    } while (0)

/**
 * Remove an item.
 *
 * The removed item is replaced by the last item of the vector.
 *
 * This does not preserve ordering, but is O(1). This is useful when the order
 * of items is not meaningful.
 *
 * \param pv a pointer to the vector
 * \param index the index of item to remove
 */
#define vlc_vector_swap_remove(pv, index) \
    do { \
        (pv)->data[index] = (pv)->data[(pv)->size-1]; \
        (pv)->size--; \
    } while(0)

/**
 * Return the index of an item.
 *
 * Iterate over all items to find a given item.
 *
 * Use only for vectors of primitive types or pointers.
 *
 * The result is written to `*(pidx)`:
 *  - the index of the item if it is found;
 *  - -1 if it is not found.
 *
 * \param pv a pointer to the vector
 * \param item the item to find (compared with ==)
 * \param[out] pidx a pointer to the result (ssize_t *)
 */
#define vlc_vector_index_of(pv, item, pidx) \
    do { \
        ssize_t *out = pidx; /* warning/error on type mismatch */ \
        size_t vlc_vector_find_idx_; \
        for (vlc_vector_find_idx_ = 0; \
             vlc_vector_find_idx_ < (pv)->size; \
             ++vlc_vector_find_idx_) \
            if ((pv)->data[vlc_vector_find_idx_] == (item)) \
                break; \
        *out = vlc_vector_find_idx_ == (pv)->size ? -1 : \
            (ssize_t) vlc_vector_find_idx_; \
    } while (0)

/**
 * For-each loop.
 *
 * Use only for vectors of primitive types or pointers (every struct would be
 * copied for a vector of structs).
 *
 * \param[out] item the iteration variable
 * \param[out] pv a pointer to the vector
 */
#define vlc_vector_foreach(item, pv) \
    for (size_t vlc_vector_idx_##item = 0; \
         vlc_vector_idx_##item < (pv)->size && \
             ((item) = (pv)->data[vlc_vector_idx_##item], true); \
         ++vlc_vector_idx_##item)

/** @} */

#endif
