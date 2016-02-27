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

#ifndef DECKBUTTONLAYOUT_H
#define DECKBUTTONLAYOUT_H

#include <QLayout>
#include <QPointer>
#include <QAbstractButton>

class QWidget;
class QAbstractButton;

class DeckButtonsLayout : public QLayout
{
    Q_OBJECT

public:
    DeckButtonsLayout( QWidget* parent = 0 );
    virtual ~DeckButtonsLayout();

    virtual QSize sizeHint() const;
    virtual int count() const;

    void setBackwardButton( QAbstractButton* button );
    void setRoundButton( QAbstractButton* button );
    void setForwardButton( QAbstractButton* button );

protected:
    QWidgetItem* backwardItem;
    QWidgetItem* goItem;
    QWidgetItem* forwardItem;
    QPointer<QAbstractButton> backwardButton;
    QPointer<QAbstractButton> RoundButton;
    QPointer<QAbstractButton> forwardButton;

    virtual void setGeometry( const QRect& r );
    virtual void addItem( QLayoutItem* item );
    virtual QLayoutItem* itemAt( int index ) const;
    virtual QLayoutItem* takeAt( int index );
};

#endif // DECKBUTTONLAYOUT_H
