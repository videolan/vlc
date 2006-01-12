/*****************************************************************************
 * xlist.h : a trivial parser for XML-like tags (header file)
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

#ifndef __XTAG_H__
#define __XTAG_H__

typedef void XTag;

XTag * xtag_new_parse (const char * s, int n);

char * xtag_get_name (XTag * xtag);

char * xtag_get_pcdata (XTag * xtag);

char * xtag_get_attribute (XTag * xtag, char * attribute);

XTag * xtag_first_child (XTag * xtag, char * name);

XTag * xtag_next_child (XTag * xtag, char * name);

XTag * xtag_free (XTag * xtag);

int xtag_snprint (char * buf, int n, XTag * xtag);

#endif /* __XTAG_H__ */
