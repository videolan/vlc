/*************************************************************************
 * xarray.h: Mutable (dynamically growable) array (header file)
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

#ifndef __XARRAY_H__
#define __XARRAY_H__

/* define this to 'static' for static linkage */
#define XSTATIC

#define XARRAY_DEFAULT_SIZE 69
#define xarray_malloc malloc

/* Error codes */
enum xarray_errors
{
    XARRAY_SUCCESS, XARRAY_ENULLPOINTER, XARRAY_ENEGATIVEINDEX,
    XARRAY_EINDEXTOOLARGE, XARRAY_ENOMEM, XARRAY_EEMPTYARRAY,
    XARRAY_ECOUNTOUTOFBOUNDS
};


typedef struct
{
    void **array;
    int last_valid_element;
    unsigned int size;
    unsigned int last_error;
}
XArray;

/* Mutable methods */
XSTATIC int      xarray_AddObject (XArray *xarray, void *object);
XSTATIC int      xarray_InsertObject (XArray *xarray, void *object,
                                      unsigned int at_index);
XSTATIC int      xarray_RemoveLastObject (XArray *xarray);
XSTATIC int      xarray_RemoveObject (XArray *xarray, unsigned int at_index);
XSTATIC int      xarray_RemoveObjects (XArray *xarray, unsigned int at_index,
                                       int count);
XSTATIC int      xarray_RemoveObjectsAfter (XArray *xarray, unsigned int index);
XSTATIC int      xarray_ReplaceObject (XArray *xarray, unsigned int index,
                                       void *new_object);

/* Immutable methods */
XSTATIC XArray * xarray_New ();
XSTATIC int      xarray_ObjectAtIndex (XArray *xarray, unsigned int index,
                                       void **out_object);
XSTATIC int      xarray_Count (XArray *xarray, unsigned int *out_count);

#endif /* __XARRAY_H__ */

