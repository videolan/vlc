/*****************************************************************************
 * EPGView.cpp: EPGView
 ****************************************************************************
 * Copyright © 2009-2010 VideoLAN
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "qt.hpp"

#include <vlc_epg.h>

#include "EPGView.hpp"
#include "EPGItem.hpp"

#include <QDateTime>
#include <QMatrix>
#include <QPaintEvent>
#include <QRectF>

EPGGraphicsScene::EPGGraphicsScene( QObject *parent ) : QGraphicsScene( parent )
{}

void EPGGraphicsScene::drawBackground( QPainter *painter, const QRectF &rect)
{
    EPGView *epgView = qobject_cast<EPGView *>(parent());

    if( !epgView->startTime().isValid() )
    {
        QGraphicsScene::drawBackground( painter, rect );
        return;
    }

    /* day change */
    QDateTime rectstarttime = epgView->startTime().addSecs( rect.left() );
    QDateTime nextdaylimit = QDateTime( rectstarttime.date() );
    QRectF area( rect );
    while( area.left() < width() )
    {
        nextdaylimit = nextdaylimit.addDays( 1 );
        area.setRight( epgView->startTime().secsTo( nextdaylimit ) );

        if ( epgView->startTime().date().daysTo( nextdaylimit.date() ) % 2 != 0 )
            painter->fillRect( area, palette().color( QPalette::Base ) );
        else
            painter->fillRect( area, palette().color( QPalette::AlternateBase ) );

        area.setLeft( area.right() + 1 );
    }

    /* channels lines */
    painter->setPen( QPen( QColor( 224, 224, 224 ) ) );
    for( int y = rect.top() + TRACKS_HEIGHT ; y < rect.bottom() ; y += TRACKS_HEIGHT )
       painter->drawLine( QLineF( rect.left(), y, rect.right(), y ) );

    /* current hour line */
    if( epgView->epgTime().isValid() )
    {
        int x = epgView->startTime().secsTo( epgView->epgTime() );
        painter->setPen( QPen( QColor( 255, 192, 192 ) ) );
        painter->drawLine( QLineF( x, rect.top(), x, rect.bottom() ) );
    }
}

EPGView::EPGView( QWidget *parent ) : QGraphicsView( parent )
{
    setContentsMargins( 0, 0, 0, 0 );
    setFrameStyle( QFrame::Box );
    setAlignment( Qt::AlignLeft | Qt::AlignTop );

    m_startTime = QDateTime();
    m_maxTime = m_startTime;
    m_scaleFactor = 1.0;

    EPGGraphicsScene *EPGscene = new EPGGraphicsScene( this );

    setScene( EPGscene );
}

void EPGView::setScale( double scaleFactor )
{
    m_scaleFactor = scaleFactor;
    QTransform matrix;
    matrix.scale( scaleFactor, 1 );
    setTransform( matrix );
}

const QDateTime& EPGView::startTime() const
{
    return m_startTime;
}

QDateTime EPGView::epgTime() const
{
    if( m_startTime.isValid() && m_maxTime.isValid() )
        return m_epgTime;
    return QDateTime();
}

void EPGView::setEpgTime(const QDateTime &time)
{
    m_epgTime = time;
}

bool EPGView::hasValidData() const
{
    return !programs.isEmpty();
}

bool EPGView::updateEPG( const vlc_epg_t * const *pp_epg, size_t i_epg )
{
    m_updtMinTime = QDateTime();

    for ( size_t i = 0; i < i_epg; ++i )
    {
        const vlc_epg_t *p_epg = pp_epg[i];

        EPGProgram *program;

        QHash<uint16_t, EPGProgram*>::iterator it = programs.find( p_epg->i_source_id );
        if( it != programs.end() )
        {
            program = *it;
        }
        else
        {
            program = new EPGProgram( this, p_epg );
            program->setPosition( programs.count() );
            programs.insert( p_epg->i_source_id, program );
            emit programAdded( program );
        }
        program->updateEvents( p_epg->pp_event, p_epg->i_event, p_epg->p_current, &m_updtMinTime );
    }

    if( !m_startTime.isValid() )
        m_startTime = m_updtMinTime;

    return true;
}

void EPGView::reset()
{
    /* clean our items storage and remove them from the scene */
    qDeleteAll(programs.values());
    programs.clear();
    m_startTime = m_maxTime = QDateTime();
}

void EPGView::walkItems( bool b_cleanup )
{
    m_updtMinTime = m_startTime;
    QDateTime maxTime;
    bool b_rangechanged = false;

    foreach( EPGProgram *program, programs )
    {
        /* remove expired items and clear their current flag */
        if( b_cleanup && m_updtMinTime.isValid() )
            program->pruneEvents( m_updtMinTime );

        if( !program->eventsbytime.isEmpty() )
        {
            const EPGItem *last = (program->eventsbytime.end() - 1).value();
            if( !maxTime.isValid() ||
                 last->start().addSecs( last->duration() ) > maxTime )
            {
                maxTime = last->start().addSecs( last->duration() );
            }
        }
    }

    if( m_startTime.isValid() && m_startTime != m_updtMinTime )
        b_rangechanged = m_updtMinTime.isValid();

    if( maxTime.isValid() && m_maxTime != maxTime )
        b_rangechanged |= m_updtMinTime.isValid();

    m_startTime = m_updtMinTime;
    m_maxTime = maxTime;

    if ( b_rangechanged )
    {
        foreach( EPGProgram *program, programs )
            program->updateEventPos();
        emit rangeChanged( m_startTime, m_maxTime );
    }
}

void EPGView::cleanup()
{
    walkItems( true );
}

EPGView::~EPGView()
{
    reset();
}

void EPGView::focusItem( EPGItem *epgItem )
{
    emit itemFocused( epgItem );
}

void EPGView::activateProgram( int id )
{
    emit programActivated( id );
}
