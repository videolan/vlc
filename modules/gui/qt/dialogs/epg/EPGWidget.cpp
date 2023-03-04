/*****************************************************************************
 * EPGWidget.cpp : EPGWidget
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
#include "player/player_controller.hpp"

#include <QStackedWidget>
#include <QGridLayout>
#include <QScrollBar>
#include <QLabel>
#include <QDateTime>

#include "EPGWidget.hpp"
#include "EPGRuler.hpp"
#include "EPGView.hpp"
#include "EPGChannels.hpp"
#include "EPGItem.hpp"

EPGWidget::EPGWidget( QWidget *parent ) : QWidget( parent )
{
    b_input_type_known = false;
    i_event_source_type = ITEM_TYPE_UNKNOWN;
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
    rootWidget->insertWidget( EPGVIEW_WIDGET, containerWidget );

    QLabel *noepgLabel = new QLabel( qtr("No EPG Data Available"), this );
    noepgLabel->setAlignment( Qt::AlignCenter );
    rootWidget->insertWidget( NOEPG_WIDGET, noepgLabel );

    rootWidget->setCurrentIndex( 1 );
    layout = new QGridLayout( this );
    layout->addWidget( rootWidget );
    setLayout( layout );

    connect( m_epgView, &EPGView::rangeChanged, m_rulerWidget, &EPGRuler::setRange );

    connect( m_epgView->horizontalScrollBar(), &QScrollBar::valueChanged,
             m_rulerWidget, &EPGRuler::setOffset );
    connect( m_epgView->verticalScrollBar(), &QScrollBar::valueChanged,
             m_channelsWidget, &EPGChannels::setOffset );
    connect( m_epgView, &EPGView::itemFocused, this, &EPGWidget::itemSelectionChanged );
    connect( m_epgView, &EPGView::programAdded, m_channelsWidget, &EPGChannels::addProgram );
    connect( m_epgView, &EPGView::programActivated, this, &EPGWidget::activateProgram );
}

void EPGWidget::reset()
{
    m_channelsWidget->reset();
    m_epgView->reset();
    emit itemSelectionChanged( NULL );
}

void EPGWidget::setZoom( int level )
{
    double scale = (double)level / 20;
    m_epgView->setScale( scale );
    m_rulerWidget->setScale( scale );
}

void EPGWidget::updateEPG( input_item_t *p_input_item )
{
    if( !p_input_item ) return;

    /* flush our EPG data if input type has changed */
    if ( b_input_type_known && p_input_item->i_type != i_event_source_type ) m_epgView->reset();
    i_event_source_type = p_input_item->i_type;
    b_input_type_known = true;

    /* Fixme: input could have disappeared */
    vlc_mutex_lock(  & p_input_item->lock );
    m_epgView->updateEPG( p_input_item->pp_epg, p_input_item->i_epg );
    m_epgView->setEpgTime( ( p_input_item->i_epg_time ) ?
                           QDateTime::fromSecsSinceEpoch( p_input_item->i_epg_time ) :
                           QDateTime() );
    vlc_mutex_unlock( & p_input_item->lock );

    /* toggle our widget view */
    rootWidget->setCurrentIndex(
            m_epgView->hasValidData() ? EPGVIEW_WIDGET : NOEPG_WIDGET );

    m_epgView->cleanup();
}

void EPGWidget::activateProgram( int id )
{
    emit programActivated( id );
}
