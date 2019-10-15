/*****************************************************************************
 * EPGChannels.cpp: EPGChannels
 ****************************************************************************
 * Copyright © 2009-2010 VideoLAN
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "qt.hpp"

#include <vlc_epg.h>

#include "EPGChannels.hpp"
#include "EPGProgram.hpp"
#include "EPGView.hpp"

#include <QPainter>
#include <QFont>
#include <QPaintEvent>
#include <QtAlgorithms>

EPGChannels::EPGChannels( QWidget *parent, EPGView *m_epgView )
    : QWidget( parent ), m_epgView( m_epgView ), m_offset( 0 )
{
    setContentsMargins( 0, 0, 0, 0 );
}

void EPGChannels::setOffset( int offset )
{
    m_offset = offset;
    update();
}

void EPGChannels::addProgram( const EPGProgram *program )
{
    if ( !programsList.contains( program ) )
    {
        programsList << program;
        std::sort(programsList.begin(), programsList.end(), EPGProgram::lessThan);
        update();
    }
}

void EPGChannels::reset()
{
    programsList.clear();
    update();
}

void EPGChannels::paintEvent( QPaintEvent *event )
{
    Q_UNUSED( event );

    QPainter p( this );

    /* Draw the top and the bottom lines. */
    p.drawLine( 0, 0, width() - 1, 0 );

    unsigned int i=0;
    foreach( const EPGProgram *program, programsList )
    {
        QString text = program->getName();
        /* try to remove the " [Program xxx]" end */
        int i_idx_channel = text.lastIndexOf(" [");
        if (i_idx_channel > 0)
            text = text.left( i_idx_channel );

        p.drawText( 0, - m_offset + ( i++ + 0.5 ) * TRACKS_HEIGHT - 4,
                    width(), height(), Qt::AlignLeft, text );

        int i_width = fontMetrics().horizontalAdvance( text );
        if( width() < i_width )
            setMinimumWidth( i_width );
    }
}
