/*****************************************************************************
 * customwidgets.cpp: Custom widgets
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "customwidgets.hpp"

#include <QtMath>  // for wheel deadzone calculation
#include <QPainter>
#include <QRect>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QApplication>

#define SPINNER_SIZE 32

QFramelessButton::QFramelessButton( QWidget *parent )
                    : QPushButton( parent )
{
    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Preferred );
}

void QFramelessButton::paintEvent( QPaintEvent * )
{
    QPainter painter( this );
    icon().paint(&painter, QRect( 0, 0, width(), height()));

}

QString QVLCDebugLevelSpinBox::textFromValue( int v ) const
{
    QString const texts[] = {
    /* Note that min level 0 is 'errors' in Qt Ui
       FIXME: fix debug levels accordingly to documentation */
    /*  qtr("infos"),*/
        qtr("errors"),
        qtr("warnings"),
        qtr("debug")
    };
    if ( v < 0 ) v = 0;
    if ( v >= 2 ) v = 2;

    return QString( "%1 (%2)" ).arg( v ).arg( texts[v] );
}

VLCQDial::VLCQDial( QWidget *parent ) : QDial( parent )
{

}

void VLCQDial::paintEvent( QPaintEvent *event )
{
    QDial::paintEvent( event );
    QPainter painter( this );
    painter.setPen( QPen( palette().color( QPalette::WindowText ) ) );
    float radius = 0.5 * 0.707106 * qMin( size().width(), size().height() );
    painter.drawText( QRectF( rect().center().x() + radius,
                              rect().center().y() + radius,
                              size().width(),
                              size().height() ),
                      0, QString::number( value() ), 0 );
    painter.end();
}

/* Animated Icon implementation */
SpinningIcon::SpinningIcon( QWidget *parent ) : QLabel( parent )
{
    QList<QString> frames;
    frames << ":/misc/wait1.svg";
    frames << ":/misc/wait2.svg";
    frames << ":/misc/wait3.svg";
    frames << ":/misc/wait4.svg";
    animator = new PixmapAnimator( this, std::move(frames), SPINNER_SIZE, SPINNER_SIZE );
    connect( animator, &PixmapAnimator::pixmapReady, this, [=]( const QPixmap &pixmap ) {
        this->setPixmap( pixmap );
        this->repaint();
    } );
    setScaledContents( true );
    setFixedSize( 16, 16 );
    animator->setCurrentTime( 0 );
}

QToolButtonExt::QToolButtonExt(QWidget *parent, int ms )
    : QToolButton( parent ),
      shortClick( false ),
      longClick( false )
{
    setAutoRepeat( true );
    /* default to twice the doubleclick delay */
    setAutoRepeatDelay( ( ms > 0 )? ms : 2 * QApplication::doubleClickInterval() );
    setAutoRepeatInterval( 100 );
    connect( this, &QToolButtonExt::released, this, &QToolButtonExt::releasedSlot );
    connect( this, &QToolButtonExt::clicked, this, &QToolButtonExt::clickedSlot );
}

/* table illustrating the different scenarios and the events generated
 * ====================
 *
 *  event     isDown()
 *
 *  released  false   }
 *  clicked   false   }= short click
 *
 *  released  false    = cancelled click (mouse released outside of button area,
 *                                        before long click delay kicks in)
 *
 *  released  true    }
 *  clicked   true    }= long click (multiple of these generated)
 *  released  false    = stop long click (mouse released / moved outside of
 *                                        button area)
 * (clicked   false)   = stop long click (additional event if mouse released
 *                                        inside of button area)
 */

void QToolButtonExt::releasedSlot()
{
    if( isDown() )
    {
        // we are beginning a long click
        longClick = true;
        shortClick = false;
    }
    else
    {
        if( longClick )
        {
            // we are stopping a long click
            longClick = false;
            shortClick = false;
        }
        else
        {
            // we are generating a short click
            longClick = false;
            shortClick = true;
        }
    }
}

void QToolButtonExt::clickedSlot()
{
    if( longClick )
        emit longClicked();
    else if( shortClick )
        emit shortClicked();
}

YesNoCheckBox::YesNoCheckBox( QWidget *parent ) : QCheckBox( parent )
{
    setEnabled( false );
    setStyleSheet("\
                  QCheckBox::indicator:unchecked:hover,\
                  QCheckBox::indicator:unchecked {\
                      image: url(:/menu/clear.svg);\
                  }\
                  QCheckBox::indicator:checked:hover,\
                  QCheckBox::indicator:checked {\
                      image: url(:/menu/valid.svg);\
                  }\
        ");
}
