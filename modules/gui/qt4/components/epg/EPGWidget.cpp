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

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QLabel>
#include <QStringList>
#include "qt4.hpp"

EPGWidget::EPGWidget( QWidget *parent ) : QWidget( parent )
{
    b_input_type_known = false;
    m_rulerWidget = new EPGRuler( this );
    m_epgView = new EPGView( this );
    m_channelsWidget = new EPGChannels( this, m_epgView );

    m_channelsWidget->setMinimumWidth( 100 );

    m_epgView->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    setZoom( 1 );

    rootWidget = new QStackedWidget( this );

    QWidget *containerWidget = new QWidget( this );
    QGridLayout* layout = new QGridLayout( this );
    layout->addWidget( m_rulerWidget, 0, 1 );
    layout->addWidget( m_channelsWidget, 1, 0 );
    layout->addWidget( m_epgView, 1, 1 );
    layout->setSpacing( 0 );
    containerWidget->setLayout( layout );
    rootWidget->addWidget( containerWidget ); /* index 0 */

    QLabel *noepgLabel = new QLabel( qtr("No EPG Data Available"), this );
    noepgLabel->setAlignment( Qt::AlignCenter );
    rootWidget->addWidget( noepgLabel ); /* index 1 */

    rootWidget->setCurrentIndex( 1 );
    layout = new QGridLayout( this );
    layout->addWidget( rootWidget );
    setLayout( layout );

    CONNECT( m_epgView, startTimeChanged(QDateTime),
             m_rulerWidget, setStartTime(QDateTime) );
    CONNECT( m_epgView, durationChanged(int),
             m_rulerWidget, setDuration(int) );
    CONNECT( m_epgView->horizontalScrollBar(), valueChanged(int),
             m_rulerWidget, setOffset(int) );
    CONNECT( m_epgView->verticalScrollBar(), valueChanged(int),
             m_channelsWidget, setOffset(int) );
    connect( m_epgView, SIGNAL( itemFocused(EPGItem*)),
             this, SIGNAL(itemSelectionChanged(EPGItem*)) );
    CONNECT( m_epgView, channelAdded(QString), m_channelsWidget, addChannel(QString) );
    CONNECT( m_epgView, channelRemoved(QString), m_channelsWidget, removeChannel(QString) );
}

void EPGWidget::reset()
{
    m_epgView->reset();
    m_epgView->updateDuration();
    m_epgView->updateStartTime();
}

void EPGWidget::setZoom( int level )
{
    double scale = (double)level / 20;
    m_epgView->setScale( scale );
    m_rulerWidget->setScale( scale );
}

void EPGWidget::updateEPG( vlc_epg_t **pp_epg, int i_epg, uint8_t i_input_type )
{
    /* flush our EPG data if input type has changed */
    if ( b_input_type_known && i_input_type != i_event_source_type ) m_epgView->reset();
    i_event_source_type = i_input_type;
    b_input_type_known = true;

    m_epgView->cleanup(); /* expire items and flags */
    rootWidget->setCurrentIndex( ( i_epg > 0 ) ? 0 : 1 );

    for ( int i = 0; i < i_epg; ++i )
    {
        vlc_epg_t *p_epg = pp_epg[i];

        /* Read current epg events from libvlc and try to insert them */
        for ( int j = 0; j < p_epg->i_event; ++j )
        {
            vlc_epg_event_t *p_event = p_epg->pp_event[j];
            m_epgView->addEPGEvent( p_event, qfu( p_epg->psz_name ),
                                    ( p_epg->p_current == p_event ) );
        }
    }

    // Update the global duration and start time.
    m_epgView->updateDuration();
    m_epgView->updateStartTime();
}
