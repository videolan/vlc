/*****************************************************************************
 * Copyright © 2011-2012 VideoLAN
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
#include <QDesktopWidget>

#define TIP_HEIGHT 5

TimeTooltip::TimeTooltip( QWidget *parent ) :
    QWidget( parent ), mInitialized( false )
{
    setWindowFlags( Qt::Window                  |
                    Qt::WindowStaysOnTopHint    |
                    Qt::FramelessWindowHint     |
                    Qt::X11BypassWindowManagerHint );

    // Tell Qt that it doesn't need to erase the background before
    // a paintEvent occurs. This should save some CPU cycles.
    setAttribute( Qt::WA_OpaquePaintEvent );

#if defined( Q_OS_WIN ) || defined( Q_OS_OS2 )
    /*
    - This attribute is required on Windows and OS/2 to avoid focus stealing of other windows.
    - When set on Linux the TimeTooltip appears behind the FSController in fullscreen.
    */
    setAttribute( Qt::WA_ShowWithoutActivating );
#endif

    // Inherit from the system default font size -5
    mFont = QFont( "Verdana", qMax( qApp->font().pointSize() - 5, 7 ) );
    mTipX = -1;

    // By default the widget is unintialized and should not be displayed
    resize( 0, 0 );
}

void TimeTooltip::adjustPosition()
{
    if( mDisplayedText.isEmpty() )
    {
        resize( 0, 0 );
        return;
    }

    // Get the bounding box required to print the text and add some padding
    QFontMetrics metrics( mFont );
    QRect textbox = metrics.boundingRect( mDisplayedText );
    textbox.adjust( -2, -2, 2, 2 );
    textbox.moveTo( 0, 0 );

    // Resize the widget to fit our needs
    QSize size( textbox.width() + 1, textbox.height() + TIP_HEIGHT + 1 );

    // The desired label position is just above the target
    QPoint position( mTarget.x() - size.width() / 2,
        mTarget.y() - size.height() + TIP_HEIGHT / 2 );

    // Keep the tooltip on the same screen if possible
    QRect screen = QApplication::desktop()->screenGeometry( mTarget );
    position.setX( qMax( screen.left(), qMin( position.x(),
        screen.left() + screen.width() - size.width() ) ) );
    position.setY( qMax( screen.top(), qMin( position.y(),
        screen.top() + screen.height() - size.height() ) ) );

    move( position );

    int tipX = mTarget.x() - position.x();
    if( mBox != textbox || mTipX != tipX )
    {
        mBox = textbox;
        mTipX = tipX;

        resize( size );
        buildPath();
        setMask( mMask );
    }
}

void TimeTooltip::buildPath()
{
    // Prepare the painter path for future use so
    // we only have to generate the text at runtime.

    // Draw the text box
    mPainterPath = QPainterPath();
    mPainterPath.addRect( mBox );

    // Draw the tip
    QPolygon polygon;
    polygon << QPoint( qMax( 0, mTipX - 3 ), mBox.height() )
            << QPoint( mTipX, mBox.height() + TIP_HEIGHT )
            << QPoint( qMin( mTipX + 3, mBox.width() ), mBox.height() );
    mPainterPath.addPolygon( polygon );

    // Store the simplified version of the path
    mPainterPath = mPainterPath.simplified();

    // Create the mask used to erase the background
    // Note: this is a binary bitmap (black & white)
    mMask = QBitmap( size() );
    QPainter painter( &mMask );
    painter.fillRect( mMask.rect(), Qt::white );
    painter.setPen( Qt::black );
    painter.setBrush( Qt::black );
    painter.drawPath( mPainterPath );
    painter.end();
}

void TimeTooltip::setTip( const QPoint& target, const QString& time, const QString& text )
{
    mInitialized = true;
    mDisplayedText = time;
    if ( !text.isEmpty() )
        mDisplayedText.append( " - " ).append( text );

    if( mTarget != target || time.length() != mTime.length() || mText != text )
    {
        mTarget = target;
        mTime = time;
        mText = text;
        adjustPosition();
    }

    update();
}

void TimeTooltip::show()
{
    setVisible( true );
#ifdef Q_OS_OS2
    // Bring a tooltip on the top
    // Without this, tooltip does not appear on fullscreen
    // from the second fullscreen state change
    if( mInitialized )
        QWidget::raise();
#endif
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
