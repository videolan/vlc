/*****************************************************************************
 * vlc_internal.h: Header for the Python vlcinternal binding
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Aubert <oaubert at bat710.univ-lyon1.fr>
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
#ifndef _VLCINTERNAL_H
#define _VLCINTERNAL_H 1

/* We need to access some internal features of VLC (for vlc_object) */
/* This is gruik as we are not libvlc at all */
#define __LIBVLC__

#include <Python.h>
#include "structmember.h"

#include <stdio.h>
#include <vlc/vlc.h>
#include <vlc/libvlc.h>
/* Even gruiker ! We access variable_t ! */
#include "../../src/misc/variables.h"

/* Python 2.5 64-bit support compatibility define */
#if PY_VERSION_HEX < 0x02050000 && !defined(PY_SSIZE_T_MIN)
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif

/**********************************************************************
 * VLC Object
 **********************************************************************/
#define VLCSELF ( ( vlcObject* )self )

/**********************************************************************
 * VLCObject Object
 **********************************************************************/
typedef struct
{
    PyObject_HEAD
    vlc_object_t* p_object;
    int b_released;
} vlcObject;

/* Forward declarations */
staticforward PyTypeObject vlcObject_Type;

/* Long long conversion on Mac os X/ppc */
#if defined (__ppc__) || defined(__ppc64__)
#define ntohll(x) ((long long) x >> 64)
#else
#define ntohll(x) (x)
#endif

#endif
