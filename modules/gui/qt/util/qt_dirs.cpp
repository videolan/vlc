/*****************************************************************************
 * qt_dirs.cpp : file path helpers
 ****************************************************************************
 * Copyright (C) 2010 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "qt.hpp"
#include "qt_dirs.hpp"
#include <vlc_url.h>

QString toURI( const QString& s )
{
    if( s.contains( qfu("://") ) )
        return s;

    char *psz = vlc_path2uri( qtu(s), NULL );
    if( psz == NULL )
        return qfu("");

    QString uri = qfu( psz );
    free( psz );
    return uri;
}
