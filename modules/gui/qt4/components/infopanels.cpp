/*****************************************************************************
 * infopanels.cpp : Panels for the information dialogs
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#include <QTreeWidget>
#include <QPushButton>
#include <QHeaderView>
#include <QList>

/************************************************************************
 * Single panels
 ************************************************************************/

InputStatsPanel::InputStatsPanel( QWidget *parent, intf_thread_t *_p_intf ) :
                                  QWidget( parent ), p_intf( _p_intf )
{
    ui.setupUi( this );
}

InputStatsPanel::~InputStatsPanel()
{
}

void InputStatsPanel::update( input_item_t *p_item )
{
    vlc_mutex_lock( &p_item->p_stats->lock );

#define UPDATE( widget,format, calc... ) \
    { QString str; ui.widget->setText( str.sprintf( format, ## calc ) );  }

    UPDATE( read_text, "%8.0f kB", (float)(p_item->p_stats->i_read_bytes)/1000);
    UPDATE( input_bitrate_text, "%6.0f kb/s",
                    (float)(p_item->p_stats->f_input_bitrate * 8000 ));
    UPDATE( demuxed_text, "%8.0f kB",
                    (float)(p_item->p_stats->i_demux_read_bytes)/1000 );
    UPDATE( stream_bitrate_text, "%6.0f kb/s",
                    (float)(p_item->p_stats->f_demux_bitrate * 8000 ));

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

void InputStatsPanel::clear()
{
}

MetaPanel::MetaPanel( QWidget *parent, intf_thread_t *_p_intf ) :
                                    QWidget( parent ), p_intf( _p_intf )
{

}
MetaPanel::~MetaPanel()
{
}
void MetaPanel::update( input_item_t *p_item )
{
}
void MetaPanel::clear()
{
}

char* MetaPanel::getURI()
{
    char *URI;
    return URI;
}

char* MetaPanel::getName()
{
    char *Name;
    return Name;
}


InfoPanel::InfoPanel( QWidget *parent, intf_thread_t *_p_intf ) :
                                      QWidget( parent ), p_intf( _p_intf )
{
//     resize(400, 500);
     QGridLayout *layout = new QGridLayout(this);
     InfoTree = new QTreeWidget(this);
     QList<QTreeWidgetItem *> items;

     layout->addWidget(InfoTree, 0, 0 );
     InfoTree->setColumnCount( 1 );
     InfoTree->header()->hide();
//     InfoTree->resize(400, 400);
}

InfoPanel::~InfoPanel()
{
}

void InfoPanel::update( input_item_t *p_item)
{
    InfoTree->clear();
    QTreeWidgetItem *current_item = NULL;
    QTreeWidgetItem *child_item = NULL;

    for( int i = 0; i< p_item->i_categories ; i++)
    {
        current_item = new QTreeWidgetItem();
        current_item->setText( 0, qfu(p_item->pp_categories[i]->psz_name) );
        InfoTree->addTopLevelItem( current_item );

        for( int j = 0 ; j < p_item->pp_categories[i]->i_infos ; j++ )
        {
            child_item = new QTreeWidgetItem ();
            child_item->setText( 0,
                    qfu(p_item->pp_categories[i]->pp_infos[j]->psz_name)
                    + ": "
                    + qfu(p_item->pp_categories[i]->pp_infos[j]->psz_value));

            current_item->addChild(child_item);
        }
         InfoTree->setItemExpanded( current_item, true);
    }
}

void InfoPanel::clear()
{
    InfoTree->clear();
}

/***************************************************************************
 * Tab widget
 ***************************************************************************/

InfoTab::InfoTab( QWidget *parent,  intf_thread_t *_p_intf, bool _stats ) :
                      QTabWidget( parent ), stats( _stats ), p_intf( _p_intf )
{
//    setGeometry(0, 0, 400, 500);

    MP = new MetaPanel(NULL, p_intf);
    addTab(MP, qtr("&Meta"));
    if( stats )
    {
        ISP = new InputStatsPanel( NULL, p_intf );
        addTab(ISP, qtr("&Stats"));
    }

    IP = new InfoPanel(NULL, p_intf);
    addTab(IP, qtr("&Info"));
}

InfoTab::~InfoTab()
{
}

/* This function should be called approximately twice a second.
 * p_item should be locked
 * Stats will always be updated */
void InfoTab::update( input_item_t *p_item, bool update_info,
                      bool update_meta )
{
    if( update_info )
        IP->update( p_item );
    if( update_meta )
        MP->update( p_item );
    if( stats )
        ISP->update( p_item );
}

void InfoTab::clear()
{
    IP->clear();
    MP->clear();
    if( stats ) ISP->clear();
}
