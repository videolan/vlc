/*****************************************************************************
 * qvlcframe.hpp : A few helpers
 ****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#ifndef _QVLCFRAME_H_
#define _QVLCFRAME_H_

#include <QWidget>
#include <QApplication>
#include <QPlastiqueStyle>
#include <vlc/vlc.h>

class QVLCFrame : public QWidget
{
public:
    QVLCFrame( intf_thread_t *_p_intf ) : QWidget( NULL ), p_intf( _p_intf )
    {
        QStyle *style = qApp->style();
        // Plastique is too dark.
        /// theming ? getting KDE data ? ?
        if( qobject_cast<QPlastiqueStyle *>(style) )
        {
            QPalette plt( palette() );
            plt.setColor( QPalette::Active, QPalette::Highlight, Qt::gray );
            QColor vlg = (Qt::lightGray);
            vlg = vlg.toHsv();
            vlg.setHsv( vlg.hue(), vlg.saturation(), 235  );
            plt.setColor( QPalette::Active, QPalette::Window, vlg );
            plt.setColor( QPalette::Inactive, QPalette::Window, vlg );
            setPalette( plt );
        }
    };
    virtual ~QVLCFrame()   {};

    void toggleVisible()
    {
        if( isVisible() ) hide();
        else show();
    }
protected:
    intf_thread_t *p_intf;
};
#endif
