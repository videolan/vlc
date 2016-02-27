/*****************************************************************************
 * Copyright © 2011 VideoLAN
 * $Id$
 *
 * Authors: Filipe Azevedo, aka PasNox
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef BROWSEBUTTON_H
#define BROWSEBUTTON_H

#include "RoundButton.hpp"

class BrowseButton : public RoundButton
{
    Q_OBJECT

public:
    enum Type {
        Backward = 0,
        Forward = 1
    };

    BrowseButton( QWidget* parent = 0, BrowseButton::Type type = BrowseButton::Forward );

    virtual QSize sizeHint() const;

    BrowseButton::Type type() const;

public slots:
    void setType( BrowseButton::Type type );

protected:
    BrowseButton::Type mType;

    virtual void paintEvent( QPaintEvent* event );
};

#endif // BROWSEBUTTON_H
