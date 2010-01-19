/*****************************************************************************
 * position.h: Support routines for logo and marquee plugin objects
 *****************************************************************************
 * Copyright (C) 2010 M2X BV
 *
 * Authors: JP Dinger <jpd (at) videolan (dot) org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef POSITION_H
#define POSITION_H

struct posidx_s { const char *n; size_t i; };
static const posidx_s posidx[] = {
    { "center",        0 },
    { "left",          1 },
    { "right",         2 },
    { "top",           4 },
    { "bottom",        8 },
    { "top-left",      5 },
    { "top-right",     6 },
    { "bottom-left",   9 },
    { "bottom-right", 10 },
};
enum { num_posidx = sizeof(posidx)/sizeof(*posidx) };

static inline const char *position_bynumber( size_t i )
{
    for( const posidx_s *h=posidx; h<posidx+num_posidx; ++h )
        if( h->i == i )
            return h->n;
    return "undefined";
}

static inline bool position_byname( const char *n, size_t &i )
{
    for( const posidx_s *h=posidx; h<posidx+num_posidx; ++h )
        if( !strcasecmp( n, h->n ) )
            { i=h->i; return true; }
    return false;
}
#endif
