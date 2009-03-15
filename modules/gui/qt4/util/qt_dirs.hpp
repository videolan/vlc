/*****************************************************************************
 * dirs.hpp : String Directory helpers
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 * $Id$
 *
 * Authors:       Jean-Baptiste Kempf <jb@videolan.org>
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

#ifndef _QT_DIR_H_
#define _QT_DIR_H_

#include <QString>
/* Replace separators on Windows because Qt is always using / */
static inline QString toNativeSeparators( QString s )
{
#ifdef WIN32
    for (int i=0; i<(int)s.length(); i++)
    {
        if (s[i] == QLatin1Char('/'))
            s[i] = QLatin1Char('\\');
    }
#endif
    return s;
}

static inline QString removeTrailingSlash( QString s )
{
    if( ( s.length() > 1 ) && ( s[s.length()-1] == QLatin1Char( '/' ) ) )
        s.remove( s.length() - 1, 1 );
    return s;
}

#define toNativeSepNoSlash( a ) toNativeSeparators( removeTrailingSlash( a ) )

static inline QString colon_escape( QString s )
{
    return s.replace( ":", "\\:" );
}
static inline QString colon_unescape( QString s )
{
    return s.replace( "\\:", ":" ).trimmed();
}
#endif

