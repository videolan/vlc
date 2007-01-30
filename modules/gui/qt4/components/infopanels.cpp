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
#include <QGridLayout>


/************************************************************************
 * Single panels
 ************************************************************************/

/* First Panel - Meta Info */

MetaPanel::MetaPanel( QWidget *parent, intf_thread_t *_p_intf ) :
                                    QWidget( parent ), p_intf( _p_intf )
{
    int line = 0;
    QGridLayout *l = new QGridLayout( this );
#define ADD_META( string, widget ) {                            \
    l->addWidget( new QLabel( qfu( string ) ), line, 0 );       \
    widget = new QLabel( "" );                                  \
    l->addWidget( widget, line, 1 );                            \
    line++;            }
    ADD_META( _( "Name" ), name_text );
    ADD_META( _( "URI" ), uri_text );
    ADD_META( VLC_META_ARTIST, artist_text );
    ADD_META( VLC_META_GENRE, genre_text );
    ADD_META( VLC_META_COPYRIGHT, copyright_text );
    ADD_META( VLC_META_COLLECTION, collection_text );
    ADD_META( VLC_META_SEQ_NUM, seqnum_text );
    ADD_META( VLC_META_DESCRIPTION, description_text );
    ADD_META( VLC_META_RATING, rating_text );
    ADD_META( VLC_META_DATE, date_text );
    ADD_META( VLC_META_LANGUAGE, language_text );
    ADD_META( VLC_META_NOW_PLAYING, nowplaying_text );
    ADD_META( VLC_META_PUBLISHER, publisher_text );
    ADD_META( VLC_META_SETTING, setting_text );
}

MetaPanel::~MetaPanel()
{
}

void MetaPanel::update( input_item_t *p_item )
{
#define UPDATE_META( meta, widget ) {               \
    char* psz_meta = p_item->p_meta->psz_##meta;    \
    if( !EMPTY_STR( psz_meta ) )                    \
        widget->setText( qfu( psz_meta ) );         \
    else                                            \
        widget->setText( "" );          }

    if( !EMPTY_STR( p_item->psz_name ) )
        name_text->setText( qfu( p_item->psz_name ) );
    else name_text->setText( "" );
    if( !EMPTY_STR( p_item->psz_uri ) )
        uri_text->setText( qfu( p_item->psz_uri ) );
    else uri_text->setText( "" );
    UPDATE_META( artist, artist_text );
    UPDATE_META( genre, genre_text );
    UPDATE_META( copyright, copyright_text );
    UPDATE_META( album, collection_text );
    UPDATE_META( tracknum, seqnum_text );
    UPDATE_META( description, description_text );
    UPDATE_META( rating, rating_text );
    UPDATE_META( date, date_text );
    UPDATE_META( language, language_text );
    UPDATE_META( nowplaying, nowplaying_text );
    UPDATE_META( publisher, publisher_text );
    UPDATE_META( setting, setting_text );

#undef UPDATE_META
}

void MetaPanel::clear()
{
}

/* Second Panel - Stats */

InputStatsPanel::InputStatsPanel( QWidget *parent, intf_thread_t *_p_intf ) :
                                  QWidget( parent ), p_intf( _p_intf )
{
     QGridLayout *layout = new QGridLayout(this);
     StatsTree = new QTreeWidget(this);
     QList<QTreeWidgetItem *> items;

     layout->addWidget(StatsTree, 0, 0 );
     StatsTree->setColumnCount( 3 );
     StatsTree->header()->hide();

#define CREATE_TREE_ITEM( itemName, itemText, itemValue, unit ) {              \
    itemName =                                                           \
        new QTreeWidgetItem((QStringList () << itemText << itemValue << unit ));  \
    itemName->setTextAlignment( 1 , Qt::AlignRight ) ; }


#define CREATE_CATEGORY( catName, itemText ) {                           \
    CREATE_TREE_ITEM( catName, itemText , "", "" );                      \
    catName->setExpanded( true );                                        \
    StatsTree->addTopLevelItem( catName );    }

#define CREATE_AND_ADD_TO_CAT( itemName, itemText, itemValue, catName, unit ) { \
    CREATE_TREE_ITEM( itemName, itemText, itemValue, unit );             \
    catName->addChild( itemName ); }

    CREATE_CATEGORY( input, "Input" );
    CREATE_CATEGORY( video, "Video" );
    CREATE_CATEGORY( streaming, "Streaming" );
    CREATE_CATEGORY( audio, "Audio" );

    CREATE_AND_ADD_TO_CAT( read_media_stat, "Read at media", "0", input , "kB") ;
    CREATE_AND_ADD_TO_CAT( input_bitrate_stat, "Input bitrate", "0", input, "kb/s") ;
    CREATE_AND_ADD_TO_CAT( demuxed_stat, "Demuxed", "0", input, "kB") ;
    CREATE_AND_ADD_TO_CAT( stream_bitrate_stat, "Stream bitrate", "0", input, "kb/s") ;

    CREATE_AND_ADD_TO_CAT( vdecoded_stat, "Decoded blocks", "0", video, "" ) ;
    CREATE_AND_ADD_TO_CAT( vdisplayed_stat, "Displayed frames", "0", video, "") ;
    CREATE_AND_ADD_TO_CAT( vlost_frames_stat, "Lost frames", "0", video, "") ;

    CREATE_AND_ADD_TO_CAT( send_stat, "Sent packets", "0", streaming, "") ;
    CREATE_AND_ADD_TO_CAT( send_bytes_stat, "Sent bytes", "0", streaming, "kB") ;
    CREATE_AND_ADD_TO_CAT( send_bitrate_stat, "Sent bitrates", "0", streaming, "kb/s") ;

    CREATE_AND_ADD_TO_CAT( adecoded_stat, "Decoded blocks", "0", audio, "") ;
    CREATE_AND_ADD_TO_CAT( aplayed_stat, "Played buffers", "0", audio, "") ;
    CREATE_AND_ADD_TO_CAT( alost_stat, "Lost buffers", "0", audio, "") ;

    input->setExpanded( true );
    video->setExpanded( true );
    streaming->setExpanded( true );
    audio->setExpanded( true );

    StatsTree->resizeColumnToContents( 0 );
    StatsTree->setColumnWidth( 1 , 100 );
}

InputStatsPanel::~InputStatsPanel()
{
}

void InputStatsPanel::update( input_item_t *p_item )
{
    vlc_mutex_lock( &p_item->p_stats->lock );

#define UPDATE( widget, format, calc... ) \
    { QString str; widget->setText( 1 , str.sprintf( format, ## calc ) );  }

    UPDATE( read_media_stat, "%8.0f", (float)(p_item->p_stats->i_read_bytes)/1000);
    UPDATE( input_bitrate_stat, "%6.0f",
                    (float)(p_item->p_stats->f_input_bitrate * 8000 ));
    UPDATE( demuxed_stat, "%8.0f",
                    (float)(p_item->p_stats->i_demux_read_bytes)/1000 );
    UPDATE( stream_bitrate_stat, "%6.0f",
                    (float)(p_item->p_stats->f_demux_bitrate * 8000 ));

    /* Video */
    UPDATE( vdecoded_stat, "%5i", p_item->p_stats->i_decoded_video );
    UPDATE( vdisplayed_stat, "%5i", p_item->p_stats->i_displayed_pictures );
    UPDATE( vlost_frames_stat, "%5i", p_item->p_stats->i_lost_pictures );

    /* Sout */
    UPDATE( send_stat, "%5i", p_item->p_stats->i_sent_packets );
    UPDATE( send_bytes_stat, "%8.0f",
            (float)(p_item->p_stats->i_sent_bytes)/1000 );
    UPDATE( send_bitrate_stat, "%6.0f",
            (float)(p_item->p_stats->f_send_bitrate*8)*1000 );

    /* Audio*/
    UPDATE( adecoded_stat, "%5i", p_item->p_stats->i_decoded_audio );
    UPDATE( aplayed_stat, "%5i", p_item->p_stats->i_played_abuffers );
    UPDATE( alost_stat, "%5i", p_item->p_stats->i_lost_abuffers );

    vlc_mutex_unlock(& p_item->p_stats->lock );
}

void InputStatsPanel::clear()
{
}

/* Third panel - Stream info */

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
    addTab(MP, qtr("&General"));
    IP = new InfoPanel(NULL, p_intf);
    addTab(IP, qtr("&Details"));
    if( stats )
    {
        ISP = new InputStatsPanel( NULL, p_intf );
        addTab(ISP, qtr("&Stats"));
    }
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
