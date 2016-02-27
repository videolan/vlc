/*****************************************************************************
 * validators.hpp : Custom Input validators
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

#ifndef VALIDATORS_HPP
#define VALIDATORS_HPP

#include "qt.hpp"

#include <QValidator>

class UrlValidator : public QValidator
{
   Q_OBJECT
public:
   UrlValidator( QObject *parent ) : QValidator( parent ) { }
   QValidator::State validate( QString&, int& ) const Q_DECL_OVERRIDE;
   void fixup ( QString & input ) const Q_DECL_OVERRIDE;
};

#endif // VALIDATORS_HPP
