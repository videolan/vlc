/*****************************************************************************
 * EPGWidget.h : EPGWidget
 ****************************************************************************
 * Copyright © 2009-2010 VideoLAN
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "EPGWidget.hpp"

#include <QVBoxLayout>
#include <QScrollBar>
#include <QDebug>
#include <QLabel>
#include "qt4.hpp"

EPGWidget::EPGWidget( QWidget *parent ) : QWidget( parent )
{
    m_rulerWidget = new EPGRuler( this );
    m_epgView = new EPGView( this );
    m_channelsWidget = new EPGChannels( this, m_epgView );

    m_channelsWidget->setMinimumWidth( 100 );

    m_epgView->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    setZoom( 1 );

    QGridLayout* layout = new QGridLayout( this );
    layout->addWidget( m_rulerWidget, 0, 1 );
    layout->addWidget( m_channelsWidget, 1, 0 );
    layout->addWidget( m_epgView, 1, 1 );
    layout->setSpacing( 0 );
    setLayout( layout );

    connect( m_epgView, SIGNAL( startTimeChanged(QDateTime) ),
             m_rulerWidget, SLOT( setStartTime(QDateTime) ) );
    connect( m_epgView, SIGNAL( durationChanged(int) ),
             m_rulerWidget, SLOT( setDuration(int) ) );
    connect( m_epgView->horizontalScrollBar(), SIGNAL( valueChanged(int) ),
             m_rulerWidget, SLOT( setOffset(int) ) );
    connect( m_epgView->verticalScrollBar(), SIGNAL( valueChanged(int) ),
             m_channelsWidget, SLOT( setOffset(int) ) );
    connect( m_epgView, SIGNAL( eventFocusedChanged(EPGEvent*)),
             this, SIGNAL(itemSelectionChanged(EPGEvent*)) );
}

void EPGWidget::setZoom( int level )
{
    double scale = (double)level / 20;
    m_epgView->setScale( scale );
    m_rulerWidget->setScale( scale );
}

void EPGWidget::updateEPG( vlc_epg_t **pp_epg, int i_epg )
{
    for ( int i = 0; i < i_epg; ++i )
    {
        vlc_epg_t *p_epg = pp_epg[i];
        QString channelName = qfu( p_epg->psz_name );

        for ( int j = 0; j < p_epg->i_event; ++j )
        {
            vlc_epg_event_t *p_event = p_epg->pp_event[j];
            QString eventName = qfu( p_event->psz_name );
            QDateTime eventStart = QDateTime::fromTime_t( p_event->i_start );

            QList<EPGEvent*> events = m_events.values( channelName );

            EPGEvent *item = new EPGEvent( eventName );
            item->description = qfu( p_event->psz_description );
            item->shortDescription = qfu( p_event->psz_short_description );
            item->start = eventStart;
            item->duration = p_event->i_duration;
            item->channelName = channelName;
            item->current = ( p_epg->p_current == p_event ) ? true : false;

            bool alreadyIn = false;

            for ( int k = 0; k < events.count(); ++k )
            {
                if ( *events.at( k ) == *item )
                {
                    alreadyIn = true;
                    events.at( k )->updated = true;
                    break;
                }
            }

            if ( !alreadyIn )
            {
                m_events.insert( channelName, item );
                m_epgView->addEvent( item );
            }
            else
                delete item;
        }
    }

    // Remove old items
    QMultiMap<QString, EPGEvent*>::iterator i = m_events.begin();
    while ( i != m_events.end() )
    {
        EPGEvent* item = i.value();
        if ( !item->updated )
        {
            m_epgView->delEvent( item );
            delete item;
            --i;
            m_events.erase( i + 1 );
        }
        else
            item->updated = false;

        ++i;
    }

    // Update the global duration and start time.
    m_epgView->updateDuration();
    m_epgView->updateStartTime();
    // Udate the channel list.
    m_channelsWidget->update();
}

