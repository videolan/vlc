/*****************************************************************************
 * infopanels.cpp : Panels for the information dialogs
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
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
    UPDATE( input_bitrate_text, "%6.0f kb/s", (float)(p_item->p_stats->f_input_bitrate * 8000 ));
    UPDATE( demuxed_text, "%8.0f kB", (float)(p_item->p_stats->i_demux_read_bytes)/1000 );
    UPDATE( stream_bitrate_text, "%6.0f kb/s", (float)(p_item->p_stats->f_demux_bitrate * 8000 ));

    /* Video */
    UPDATE( vdecoded_text, "%5i", p_item->p_stats->i_decoded_video );
    UPDATE( vdisplayed_text, "%5i", p_item->p_stats->i_displayed_pictures );
    UPDATE( vlost_frames, "%5i", p_item->p_stats->i_lost_pictures );

    /* Sout */
    UPDATE( sent_text, "%5i", p_item->p_stats->i_sent_packets );
    UPDATE( sent_bytes_text, "%8.0f kB",
            (float)(p_item->p_stats->i_sent_bytes)/1000 );
    UPDATE( send_bitrate_text, "%6.0f kb/s",
            (float)(p_item->p_stats->f_send_bitrate*8)*1000 );

    /* Audio*/
    UPDATE( adecoded_text, "%5i", p_item->p_stats->i_decoded_audio );
    UPDATE( aplayed_text, "%5i", p_item->p_stats->i_played_abuffers );
    UPDATE( alost_text, "%5i", p_item->p_stats->i_lost_abuffers );

    vlc_mutex_unlock(& p_item->p_stats->lock );
}
