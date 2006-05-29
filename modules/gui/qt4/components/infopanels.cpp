/*****************************************************************************
 * infopanels.cpp : Panels for the information dialogs
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "components/infopanels.hpp"
#include "qt4.hpp"
#include "ui/input_stats.h"
#include <QWidget>

InputStatsPanel::InputStatsPanel( QWidget *parent, intf_thread_t *_p_intf ) :
                                  QWidget( parent ), p_intf( _p_intf )
{
    ui.setupUi( this );
}

InputStatsPanel::~InputStatsPanel()
{
}

void InputStatsPanel::Update( input_item_t *p_item )
{

    vlc_mutex_lock( &p_item->p_stats->lock );

#define UPDATE( widget,format, calc... ) \
    { QString str; ui.widget->setText( str.sprintf( format, ## calc ) );  }

    UPDATE( read_text, "%8.0f kB", (float)(p_item->p_stats->i_read_bytes)/1000);

    vlc_mutex_unlock(& p_item->p_stats->lock );
}
