/*****************************************************************************
 * validators.cpp : Custom Input validators
 ****************************************************************************
 * Copyright (C) 2006-2013 the VideoLAN team
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
# include "config.h"
#endif

#include "validators.hpp"

#include <QUrl>

QValidator::State UrlValidator::validate( QString& str, int& ) const
{
    if( str.startsWith( ' ' ) )
        return QValidator::Invalid;

    if ( str.isEmpty() )
        return QValidator::Intermediate;

    QUrl url( str );
    if ( url.scheme().isEmpty() )
        return QValidator::Intermediate;

    return ( url.isValid() ) ? QValidator::Acceptable : QValidator::Intermediate;
}

void UrlValidator::fixup( QString & input ) const
{
    while( input.startsWith( ' ' ) )
        input.chop( 1 );
    QUrl fixed( input, QUrl::TolerantMode );
    input = fixed.toString();
}
