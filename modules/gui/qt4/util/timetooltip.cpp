/*****************************************************************************
 * Copyright © 2011 VideoLAN
 * $Id$
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
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

#include "timetooltip.hpp"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QBitmap>
#include <QFontMetrics>

#define TIP_HEIGHT 5

TimeTooltip::TimeTooltip( QWidget *parent ) :
    QWidget( parent )
{
    setWindowFlags( Qt::Window                  |
                    Qt::WindowStaysOnTopHint    |
                    Qt::FramelessWindowHint     |
                    Qt::X11BypassWindowManagerHint );

    // Tell Qt that it doesn't need to erase the background before
    // a paintEvent occurs. This should save some CPU cycles.
    setAttribute( Qt::WA_OpaquePaintEvent );

#ifdef Q_WS_WIN
    /*
    - This attribute is required on Windows to avoid focus stealing of other windows.
    - When set on Linux the TimeTooltip appears behind the FSController in fullscreen.
    */
    setAttribute( Qt::WA_ShowWithoutActivating );
#endif

    // Inherit from the system default font size -5
    mFont = QFont( "Verdana", qMax( qApp->font().pointSize() - 5, 7 ) );
    mPreviousMetricsWidth = 0;

    // Set default text
    setText( "00:00:00", "" );
}

void TimeTooltip::buildPath()
{
    QFontMetrics metrics( mFont );

    // Get the bounding box required to print the text and add some padding
    QRect textbox = metrics.boundingRect( mDisplayedText ).adjusted( -2, -2, 2, 2 );

    if ( mPreviousMetricsWidth == textbox.width() )
        return; //same width == same path
    else
        mPreviousMetricsWidth = textbox.width();

    mBox = QRect( 0, 0, textbox.width(), textbox.height() );

    // Resize the widget to fit our needs
    resize( mBox.width() + 1, mBox.height() + TIP_HEIGHT + 1 );

    // Prepare the painter path for future use so
    // we only have to generate the text at runtime.

    // Draw the text box
    mPainterPath = QPainterPath();
    mPainterPath.addRect( mBox );

    // Draw the tip
    int center = mBox.width() / 2;
    QPolygon polygon;
    polygon << QPoint( center - 3,   mBox.height() )
            << QPoint( center,       mBox.height() + TIP_HEIGHT )
            << QPoint( center + 3,   mBox.height() );

    mPainterPath.addPolygon( polygon );

    // Store the simplified version of the path
    mPainterPath = mPainterPath.simplified();

    // Create the mask used to erase the background
    // Note: this is a binary bitmap (black & white)
    mMask = QBitmap( size() );
    QPainter painter( &mMask );
    painter.fillRect( mMask.rect(), Qt::white );
    painter.setPen( QColor( 0, 0, 0 ) );
    painter.setBrush( QColor( 0, 0, 0 ) );
    painter.drawPath( mPainterPath );
    painter.end();

    setMask( mMask );
}

void TimeTooltip::setText( const QString& time, const QString& text )
{
    mDisplayedText = time;
    if ( !text.isEmpty() )
        mDisplayedText.append( " - " ).append( text );

    if ( time.length() != mTime.length() || mText != text )
        buildPath();

    mTime = time;
    mText = text;
    update();
}

void TimeTooltip::paintEvent( QPaintEvent * )
{
    QPainter p( this );
    p.setRenderHints( QPainter::HighQualityAntialiasing | QPainter::TextAntialiasing );

    p.setPen( Qt::black );
    p.setBrush( qApp->palette().base() );
    p.drawPath( mPainterPath );

    p.setFont( mFont );
    p.setPen( QPen( qApp->palette().text(), 1 ) );
    p.drawText( mBox, Qt::AlignCenter, mDisplayedText );
}

#undef TIP_HEIGHT
