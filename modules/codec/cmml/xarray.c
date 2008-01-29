/*************************************************************************
 * xarray.c: Mutable (dynamically growable) array
 *************************************************************************
 * Copyright (C) 2004 Commonwealth Scientific and Industrial Research
 *                    Organisation (CSIRO) Australia
 * Copyright (C) 2004 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Andre Pang <Andre.Pang@csiro.au>
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
 ************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "xarray.h"

/* local prototypes */
XSTATIC XArray * xarray_New (unsigned int);


#define XARRAY_ASSERT_NOT_NULL(xarray) \
    { \
        if (xarray == NULL) return XARRAY_ENULLPOINTER; \
    }

#define XARRAY_BOUNDS_CHECK(xarray, index) \
    { \
        if (xarray->last_valid_element != -1 && \
                 (int) index > xarray->last_valid_element) \
            return XARRAY_EINDEXTOOLARGE; \
    }

#define XARRAY_GROW_ARRAY(xarray) \
    { \
        xarray->array = (void *) realloc (xarray->array, xarray->size * 2); \
        if (xarray->array == NULL) return XARRAY_ENOMEM; \
    }

XSTATIC XArray * xarray_New (unsigned int initial_size_hint)
{
    XArray *new_xarray = NULL;
    void *inner_array;
    unsigned int initial_size;

    new_xarray = (XArray *) malloc (sizeof(XArray));
    if (new_xarray == NULL) return NULL;

    if (initial_size_hint == 0)
        initial_size = XARRAY_DEFAULT_SIZE;
    else
        initial_size = initial_size_hint;

    inner_array = calloc (initial_size, sizeof(void *));

    new_xarray->last_valid_element = -1;
    new_xarray->size = initial_size;
    new_xarray->last_error = 0;

    if (inner_array == NULL)
    {
        free (new_xarray);
        return NULL;
    }

    new_xarray->array = inner_array;

    /* Make a dummy reference to other functions, so that we don't get
     * warnings about unused functions from the compiler.  Ahem :) */
    while (0)
    {
        void *dummy_reference;

        dummy_reference = xarray_AddObject;
        dummy_reference = xarray_InsertObject;
        dummy_reference = xarray_RemoveLastObject;
        dummy_reference = xarray_RemoveObject;
        dummy_reference = xarray_RemoveObjects;
        dummy_reference = xarray_RemoveObjectsAfter;
        dummy_reference = xarray_ReplaceObject;

        dummy_reference = xarray_ObjectAtIndex;
        dummy_reference = xarray_Count;
    }
 
    return new_xarray;
}

XSTATIC int xarray_ObjectAtIndex (XArray *xarray, unsigned int index,
        void **out_object)
{
    XARRAY_ASSERT_NOT_NULL (xarray);
    XARRAY_BOUNDS_CHECK (xarray, index);

    *out_object = xarray->array[index];

    return XARRAY_SUCCESS;
}

XSTATIC int xarray_AddObject (XArray *xarray, void *object)
{
    XARRAY_ASSERT_NOT_NULL (xarray);

    ++xarray->last_valid_element;
    if (xarray->last_valid_element >= (int) xarray->size)
    {
        XARRAY_GROW_ARRAY (xarray);
    }

    xarray->array[xarray->last_valid_element] = object;

    return XARRAY_SUCCESS;
}

XSTATIC int xarray_InsertObject (XArray *xarray, void *object,
        unsigned int at_index)
{
    XARRAY_ASSERT_NOT_NULL (xarray);
    ++xarray->last_valid_element;
    XARRAY_BOUNDS_CHECK (xarray, at_index);
    if (xarray->last_valid_element >= (int) xarray->size)
    {
        XARRAY_GROW_ARRAY (xarray);
    }

    /* Shift everything from a[i] onward one pointer forward */

    if ((int) at_index < xarray->last_valid_element)
    {
        (void) memmove (&xarray->array[at_index + 1],
                        &xarray->array[at_index],
                        (xarray->last_valid_element - at_index) *
                            sizeof(void *));
    }

    xarray->array[at_index] = object;

    return XARRAY_SUCCESS;
}

XSTATIC int xarray_RemoveLastObject (XArray *xarray)
{
    XARRAY_ASSERT_NOT_NULL (xarray);

    if (xarray->last_valid_element == -1)
        return XARRAY_EEMPTYARRAY;

    xarray->array[xarray->last_valid_element] = NULL;
    --xarray->last_valid_element;

    return XARRAY_SUCCESS;
}

XSTATIC int xarray_RemoveObject (XArray *xarray, unsigned int at_index)
{
    XARRAY_ASSERT_NOT_NULL (xarray);
    XARRAY_BOUNDS_CHECK (xarray, at_index);

    /* Shift everything from a[i] onward one pointer backward */

    if ((int) at_index < xarray->last_valid_element)
    {
        (void) memmove (&xarray->array[at_index],
                        &xarray->array[at_index + 1],
                        (xarray->last_valid_element - at_index) *
                            sizeof(void *));
    }

    xarray->array[xarray->last_valid_element] = NULL;
    --xarray->last_valid_element;

    return XARRAY_SUCCESS;
}

XSTATIC int xarray_RemoveObjects (XArray *xarray, unsigned int at_index,
        int count)
{
    int i;

    XARRAY_ASSERT_NOT_NULL (xarray);
    XARRAY_BOUNDS_CHECK (xarray, at_index);

    if (count == 0) return XARRAY_SUCCESS;

    if ((int) at_index + (count - 1) > xarray->last_valid_element)
        return XARRAY_ECOUNTOUTOFBOUNDS;

    for (i = 0; i < count; i++)
    {
        int e = xarray_RemoveObject (xarray, at_index);
        if (e != XARRAY_SUCCESS) return e;
    }

    return XARRAY_SUCCESS;
}

XSTATIC int xarray_RemoveObjectsAfter (XArray *xarray, unsigned int index)
{
    XARRAY_ASSERT_NOT_NULL (xarray);
    XARRAY_BOUNDS_CHECK (xarray, index);

    index++;

    while ((int) index <= xarray->last_valid_element)
    {
        int e = xarray_RemoveObject (xarray, index);
        if (e != XARRAY_SUCCESS) return e;
    }

    return XARRAY_SUCCESS;
}

XSTATIC int xarray_ReplaceObject (XArray *xarray, unsigned int index,
        void *new_object)
{
    XARRAY_ASSERT_NOT_NULL (xarray);
    XARRAY_BOUNDS_CHECK (xarray, index);

    xarray->array[index] = new_object;

    return XARRAY_SUCCESS;
}

XSTATIC int xarray_Count (XArray *xarray, unsigned int *out_count)
{
    XARRAY_ASSERT_NOT_NULL (xarray);

    *out_count = xarray->last_valid_element + 1;

    return XARRAY_SUCCESS;
}


#undef XARRAY_ASSERT_NOT_NULL
#undef XARRAY_BOUNDS_CHECK
#undef XARRAY_GROW_ARRAY

