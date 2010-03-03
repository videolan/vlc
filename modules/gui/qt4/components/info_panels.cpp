/*****************************************************************************
 * infopanels.cpp : Panels for the information dialogs
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Ilkka Ollakka <ileoo@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"
#include "components/info_panels.hpp"
#include "components/interface_widgets.hpp"

#include <assert.h>
#include <vlc_url.h>
#include <vlc_meta.h>

#include <QTreeWidget>
#include <QHeaderView>
#include <QList>
#include <QStringList>
#include <QGridLayout>
#include <QLineEdit>
#include <QLabel>
#include <QSpinBox>

/************************************************************************
 * Single panels
 ************************************************************************/

/**
 * First Panel - Meta Info
 * All the usual MetaData are displayed and can be changed.
 **/
MetaPanel::MetaPanel( QWidget *parent,
                      intf_thread_t *_p_intf )
                      : QWidget( parent ), p_intf( _p_intf )
{
    QGridLayout *metaLayout = new QGridLayout( this );
    metaLayout->setVerticalSpacing( 12 );

    int line = 0; /* Counter for GridLayout */
    p_input = NULL;

#define ADD_META( string, widget ) {                                      \
    metaLayout->addWidget( new QLabel( qtr( string ) + " :" ), line, 0 ); \
    widget = new QLineEdit;                                               \
    metaLayout->addWidget( widget, line, 1, 1, 9 );                       \
    line++;            }

    /* Title, artist and album*/
    ADD_META( VLC_META_TITLE, title_text ); /* OK */
    ADD_META( VLC_META_ARTIST, artist_text ); /* OK */
    ADD_META( VLC_META_ALBUM, collection_text ); /* OK */

    /* Genre Name */
    /* TODO List id3genres.h is not includable yet ? */
    genre_text = new QLineEdit;
    metaLayout->addWidget( new QLabel( qtr( VLC_META_GENRE ) + " :" ), line, 0 );
    metaLayout->addWidget( genre_text, line, 1, 1, 3 );

    /* Number - on the same line */
    metaLayout->addWidget( new QLabel( qtr( VLC_META_TRACK_NUMBER )  + " :" ),
                  line, 5, 1, 2  );
    seqnum_text = new QLineEdit;
    seqnum_text->setInputMask("0000");
    seqnum_text->setAlignment( Qt::AlignRight );
    metaLayout->addWidget( seqnum_text, line, 7, 1, 3 );
    line++;

    /* Date (Should be in years) */
    date_text = new QLineEdit;
    date_text->setInputMask("0000");
    date_text->setAlignment( Qt::AlignRight );
    metaLayout->addWidget( new QLabel( qtr( VLC_META_DATE ) + " :" ), line, 0 );
    metaLayout->addWidget( date_text, line, 1, 1, 3 );

    /* Rating - on the same line */
    /*
    metaLayout->addWidget( new QLabel( qtr( VLC_META_RATING ) + " :" ), line, 4, 1, 2 );
    rating_text = new QSpinBox; setSpinBounds( rating_text );
    metaLayout->addWidget( rating_text, line, 6, 1, 1 );
    */
    /* Language on the same line */
    metaLayout->addWidget( new QLabel( qfu( VLC_META_LANGUAGE ) + " :" ), line, 5, 1, 2 );
    language_text = new QLineEdit;
    language_text->setReadOnly( true );
    metaLayout->addWidget( language_text, line,  7, 1, 3 );
    line++;

    /* ART_URL */
    art_cover = new CoverArtLabel( this, p_intf );
    metaLayout->addWidget( art_cover, line, 8, 4, 2, Qt::AlignRight );

/* Settings is unused */
/*    l->addWidget( new QLabel( qtr( VLC_META_SETTING ) + " :" ), line, 5 );
    setting_text = new QLineEdit;
    l->addWidget( setting_text, line, 6, 1, 4 ); */

/* Less used metadata */
#define ADD_META_2( string, widget ) {                                    \
    metaLayout->addWidget( new QLabel( qtr( string ) + " :" ), line, 0 ); \
    widget = new QLineEdit;                                               \
    metaLayout->addWidget( widget, line, 1, 1, 7 );                       \
    line++;            }

    /* Now Playing - Useful for live feeds (HTTP, DVB, ETC...) */
    ADD_META_2( VLC_META_NOW_PLAYING, nowplaying_text );
    nowplaying_text->setReadOnly( true );
    ADD_META_2( VLC_META_PUBLISHER, publisher_text );
    ADD_META_2( VLC_META_COPYRIGHT, copyright_text );
    ADD_META_2( N_("Comments"), description_text );

/* useless metadata */

    //ADD_META_2( VLC_META_ENCODED_BY, encodedby_text );
    /*  ADD_META( TRACKID )  Useless ? */
    /*  ADD_URI - DO not show it, done outside */

    metaLayout->setColumnStretch( 1, 2 );
    metaLayout->setColumnMinimumWidth ( 1, 80 );
    metaLayout->setRowStretch( line, 10 );
#undef ADD_META
#undef ADD_META_2

    CONNECT( title_text, textEdited( QString ), this, enterEditMode() );
    CONNECT( artist_text, textEdited( QString ), this, enterEditMode() );
    CONNECT( collection_text, textEdited( QString ), this, enterEditMode() );
    CONNECT( genre_text, textEdited( QString ), this, enterEditMode() );
    CONNECT( seqnum_text, textEdited( QString ), this, enterEditMode() );

    CONNECT( date_text, textEdited( QString ), this, enterEditMode() );
    CONNECT( description_text, textEdited( QString ), this, enterEditMode() );
/*    CONNECT( rating_text, valueChanged( QString ), this, enterEditMode( QString ) );*/

    /* We are not yet in Edit Mode */
    b_inEditMode = false;
}

/**
 * Update all the MetaData and art on an "item-changed" event
 **/
void MetaPanel::update( input_item_t *p_item )
{
    if( !p_item )
    {
        clear();
        return;
    }

    /* Don't update if you are in edit mode */
    if( b_inEditMode ) return;
    else p_input = p_item;

    char *psz_meta;
#define UPDATE_META( meta, widget ) {               \
    psz_meta = input_item_Get##meta( p_item );      \
    if( !EMPTY_STR( psz_meta ) )                    \
        widget->setText( qfu( psz_meta ) );         \
    else                                            \
        widget->setText( "" ); }                    \
    free( psz_meta );

#define UPDATE_META_INT( meta, widget ) {           \
    psz_meta = input_item_Get##meta( p_item );      \
    if( !EMPTY_STR( psz_meta ) )                    \
        widget->setValue( atoi( psz_meta ) ); }     \
    free( psz_meta );

    /* Name / Title */
    psz_meta = input_item_GetTitleFbName( p_item );
    if( psz_meta )
    {
        title_text->setText( qfu( psz_meta ) );
        free( psz_meta );
    }
    else
        title_text->setText( "" );

    /* URL / URI */
    psz_meta = input_item_GetURL( p_item );
    if( !EMPTY_STR( psz_meta ) )
        emit uriSet( qfu( psz_meta ) );
    else
    {
        free( psz_meta );
        psz_meta = input_item_GetURI( p_item );
        if( !EMPTY_STR( psz_meta ) )
            emit uriSet( qfu( psz_meta ) );
    }
    free( psz_meta );

    /* Other classic though */
    UPDATE_META( Artist, artist_text );
    UPDATE_META( Genre, genre_text );
    UPDATE_META( Copyright, copyright_text );
    UPDATE_META( Album, collection_text );
    UPDATE_META( Description, description_text );
    UPDATE_META( Language, language_text );
    UPDATE_META( NowPlaying, nowplaying_text );
    UPDATE_META( Publisher, publisher_text );
//    UPDATE_META( Setting, setting_text );
//FIXME this is wrong if has Publisher and EncodedBy fields
    UPDATE_META( EncodedBy, publisher_text );

    UPDATE_META( Date, date_text );
    UPDATE_META( TrackNum, seqnum_text );
//    UPDATE_META_INT( Rating, rating_text );

#undef UPDATE_META_INT
#undef UPDATE_META

    // If a artURL is available as a local file, directly display it !

    QString file;
    char *psz_art = input_item_GetArtURL( p_item );
    if( psz_art )
    {
        char *psz = make_path( psz_art );
        free( psz_art );
        file = qfu( psz );
        free( psz );
    }

    art_cover->showArtUpdate( file );

}

/**
 * Save the MetaData, triggered by parent->save Button
 **/
void MetaPanel::saveMeta()
{
    if( p_input == NULL )
        return;

    /* now we read the modified meta data */
    input_item_SetTitle(  p_input, qtu( title_text->text() ) );
    input_item_SetArtist( p_input, qtu( artist_text->text() ) );
    input_item_SetAlbum(  p_input, qtu( collection_text->text() ) );
    input_item_SetGenre(  p_input, qtu( genre_text->text() ) );
    input_item_SetTrackNum(  p_input, qtu( seqnum_text->text() ) );
    input_item_SetDate(  p_input, qtu( date_text->text() ) );

    input_item_SetCopyright( p_input, qtu( copyright_text->text() ) );
    input_item_SetPublisher( p_input, qtu( publisher_text->text() ) );
    input_item_SetDescription( p_input, qtu( description_text->text() ) );

    playlist_t *p_playlist = pl_Get( p_intf );
    input_item_WriteMeta( VLC_OBJECT(p_playlist), p_input );

    /* Reset the status of the mode. No need to emit any signal because parent
       is the only caller */
    b_inEditMode = false;
}


bool MetaPanel::isInEditMode()
{
    return b_inEditMode;
}

void MetaPanel::enterEditMode()
{
    msg_Dbg( p_intf, "Entering Edit MetaData Mode" );
    setEditMode( true );
}

void MetaPanel::setEditMode( bool b_editing )
{
    b_inEditMode = b_editing;
    if( b_editing )emit editing();
}

/*
 * Clear all the metadata widgets
 */
void MetaPanel::clear()
{
    title_text->clear();
    artist_text->clear();
    genre_text->clear();
    copyright_text->clear();
    collection_text->clear();
    seqnum_text->clear();
    description_text->clear();
    date_text->clear();
    language_text->clear();
    nowplaying_text->clear();
    publisher_text->clear();

    setEditMode( false );
    emit uriSet( "" );
}

/**
 * Second Panel - Shows the extra metadata in a tree, non editable.
 **/
ExtraMetaPanel::ExtraMetaPanel( QWidget *parent,
                                intf_thread_t *_p_intf )
                                : QWidget( parent ), p_intf( _p_intf )
{
     QGridLayout *layout = new QGridLayout(this);

     QLabel *topLabel = new QLabel( qtr( "Extra metadata and other information"
                 " are shown in this panel.\n" ) );
     topLabel->setWordWrap( true );
     layout->addWidget( topLabel, 0, 0 );

     extraMetaTree = new QTreeWidget( this );
     extraMetaTree->setAlternatingRowColors( true );
     extraMetaTree->setColumnCount( 2 );
     extraMetaTree->resizeColumnToContents( 0 );
     extraMetaTree->header()->hide();
/*     QStringList headerList = ( QStringList() << qtr( "Type" )
 *                                             << qtr( "Value" ) );
 * Useless, add this header if you think it would help the user          **
 */

     layout->addWidget( extraMetaTree, 1, 0 );
}

/**
 * Update the Extra Metadata from p_meta->i_extras
 **/
void ExtraMetaPanel::update( input_item_t *p_item )
{
    if( !p_item )
    {
        clear();
        return;
    }

    QList<QTreeWidgetItem *> items;

    extraMetaTree->clear();

    vlc_mutex_lock( &p_item->lock );
    vlc_meta_t *p_meta = p_item->p_meta;
    if( !p_meta )
    {
        vlc_mutex_unlock( &p_item->lock );
        return;
    }

    char ** ppsz_allkey = vlc_meta_CopyExtraNames( p_meta);

    for( int i = 0; ppsz_allkey[i] ; i++ )
    {
        const char * psz_value = vlc_meta_GetExtra( p_meta, ppsz_allkey[i] );
        QStringList tempItem;
        tempItem.append( qfu( ppsz_allkey[i] ) + " : ");
        tempItem.append( qfu( psz_value ) );
        items.append( new QTreeWidgetItem ( extraMetaTree, tempItem ) );
        free( ppsz_allkey[i] );
    }
    vlc_mutex_unlock( &p_item->lock );
    free( ppsz_allkey );

    extraMetaTree->addTopLevelItems( items );
    extraMetaTree->resizeColumnToContents( 0 );
}

/**
 * Clear the ExtraMetaData Tree
 **/
void ExtraMetaPanel::clear()
{
    extraMetaTree->clear();
}

/**
 * Third panel - Stream info
 * Display all codecs and muxers info that we could gather.
 **/
InfoPanel::InfoPanel( QWidget *parent,
                      intf_thread_t *_p_intf )
                      : QWidget( parent ), p_intf( _p_intf )
{
     QGridLayout *layout = new QGridLayout(this);

     QList<QTreeWidgetItem *> items;

     QLabel *topLabel = new QLabel( qtr( "Information about what your media or"
              " stream is made of.\nMuxer, Audio and Video Codecs, Subtitles "
              "are shown." ) );
     topLabel->setWordWrap( true );
     layout->addWidget( topLabel, 0, 0 );

     InfoTree = new QTreeWidget(this);
     InfoTree->setColumnCount( 1 );
     InfoTree->setColumnWidth( 0, 20000 );
     InfoTree->header()->hide();
//     InfoTree->header()->setStretchLastSection(false);
//     InfoTree->header()->setResizeMode(QHeaderView::ResizeToContents);
     layout->addWidget(InfoTree, 1, 0 );
}

/**
 * Update the Codecs information on parent->update()
 **/
void InfoPanel::update( input_item_t *p_item)
{
    if( !p_item )
    {
        clear();
        return;
    }

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

/**
 * Clear the tree
 **/
void InfoPanel::clear()
{
    InfoTree->clear();
}

/**
 * Save all the information to a file
 * Not yet implemented.
 **/
/*
void InfoPanel::saveCodecsInfo()
{}
*/

/**
 * Fourth Panel - Stats
 * Displays the Statistics for reading/streaming/encoding/displaying in a tree
 */
InputStatsPanel::InputStatsPanel( QWidget *parent,
                                  intf_thread_t *_p_intf )
                                  : QWidget( parent ), p_intf( _p_intf )
{
     QGridLayout *layout = new QGridLayout(this);

     QList<QTreeWidgetItem *> items;

     QLabel *topLabel = new QLabel( qtr( "Current"
                 " media / stream " "statistics") );
     topLabel->setWordWrap( true );
     layout->addWidget( topLabel, 0, 0 );

     StatsTree = new QTreeWidget(this);
     StatsTree->setColumnCount( 3 );
     StatsTree->header()->hide();

#define CREATE_TREE_ITEM( itemName, itemText, itemValue, unit ) {              \
    itemName =                                                                 \
      new QTreeWidgetItem((QStringList () << itemText << itemValue << unit )); \
    itemName->setTextAlignment( 1 , Qt::AlignRight ) ; }

#define CREATE_CATEGORY( catName, itemText ) {                           \
    CREATE_TREE_ITEM( catName, itemText , "", "" );                      \
    catName->setExpanded( true );                                        \
    StatsTree->addTopLevelItem( catName );    }

#define CREATE_AND_ADD_TO_CAT( itemName, itemText, itemValue, catName, unit ){ \
    CREATE_TREE_ITEM( itemName, itemText, itemValue, unit );                   \
    catName->addChild( itemName ); }

    /* Create the main categories */
    CREATE_CATEGORY( audio, qtr("Audio") );
    CREATE_CATEGORY( video, qtr("Video") );
    CREATE_CATEGORY( input, qtr("Input/Read") );
    CREATE_CATEGORY( streaming, qtr("Output/Written/Sent") );

    CREATE_AND_ADD_TO_CAT( read_media_stat, qtr("Media data size"),
                           "0", input , "KiB" );
    CREATE_AND_ADD_TO_CAT( input_bitrate_stat, qtr("Input bitrate"),
                           "0", input, "kb/s" );
    CREATE_AND_ADD_TO_CAT( demuxed_stat, qtr("Demuxed data size"), "0", input, "KiB") ;
    CREATE_AND_ADD_TO_CAT( stream_bitrate_stat, qtr("Content bitrate"),
                           "0", input, "kb/s" );
    CREATE_AND_ADD_TO_CAT( corrupted_stat, qtr("Discarded (corrupted)"),
                           "0", input, "" );
    CREATE_AND_ADD_TO_CAT( discontinuity_stat, qtr("Dropped (discontinued)"),
                           "0", input, "" );

    CREATE_AND_ADD_TO_CAT( vdecoded_stat, qtr("Decoded"),
                           "0", video, qtr("blocks") );
    CREATE_AND_ADD_TO_CAT( vdisplayed_stat, qtr("Displayed"),
                           "0", video, qtr("frames") );
    CREATE_AND_ADD_TO_CAT( vlost_frames_stat, qtr("Lost"),
                           "0", video, qtr("frames") );

    CREATE_AND_ADD_TO_CAT( send_stat, qtr("Sent"), "0", streaming, qtr("packets") );
    CREATE_AND_ADD_TO_CAT( send_bytes_stat, qtr("Sent"),
                           "0", streaming, "KiB" );
    CREATE_AND_ADD_TO_CAT( send_bitrate_stat, qtr("Upstream rate"),
                           "0", streaming, "kb/s" );

    CREATE_AND_ADD_TO_CAT( adecoded_stat, qtr("Decoded"),
                           "0", audio, qtr("blocks") );
    CREATE_AND_ADD_TO_CAT( aplayed_stat, qtr("Played"),
                           "0", audio, qtr("buffers") );
    CREATE_AND_ADD_TO_CAT( alost_stat, qtr("Lost"), "0", audio, qtr("buffers") );

#undef CREATE_AND_ADD_TO_CAT
#undef CREATE_CATEGORY
#undef CREATE_TREE_ITEM

    input->setExpanded( true );
    video->setExpanded( true );
    streaming->setExpanded( true );
    audio->setExpanded( true );

    StatsTree->resizeColumnToContents( 0 );
    StatsTree->setColumnWidth( 1 , 200 );

    layout->addWidget(StatsTree, 1, 0 );
}

/**
 * Update the Statistics
 **/
void InputStatsPanel::update( input_item_t *p_item )
{
    assert( p_item );
    vlc_mutex_lock( &p_item->p_stats->lock );

#define UPDATE( widget, format, calc... ) \
    { QString str; widget->setText( 1 , str.sprintf( format, ## calc ) );  }

    UPDATE( read_media_stat, "%8.0f",
            (float)(p_item->p_stats->i_read_bytes)/1024);
    UPDATE( input_bitrate_stat, "%6.0f",
                    (float)(p_item->p_stats->f_input_bitrate * 8000 ));
    UPDATE( demuxed_stat, "%8.0f",
                    (float)(p_item->p_stats->i_demux_read_bytes)/1024 );
    UPDATE( stream_bitrate_stat, "%6.0f",
                    (float)(p_item->p_stats->f_demux_bitrate * 8000 ));
    UPDATE( corrupted_stat, "%5i", p_item->p_stats->i_demux_corrupted );
    UPDATE( discontinuity_stat, "%5i", p_item->p_stats->i_demux_discontinuity );

    /* Video */
    UPDATE( vdecoded_stat, "%5i", p_item->p_stats->i_decoded_video );
    UPDATE( vdisplayed_stat, "%5i", p_item->p_stats->i_displayed_pictures );
    UPDATE( vlost_frames_stat, "%5i", p_item->p_stats->i_lost_pictures );

    /* Sout */
    UPDATE( send_stat, "%5i", p_item->p_stats->i_sent_packets );
    UPDATE( send_bytes_stat, "%8.0f",
            (float)(p_item->p_stats->i_sent_bytes)/1024 );
    UPDATE( send_bitrate_stat, "%6.0f",
            (float)(p_item->p_stats->f_send_bitrate*8)*1000 );

    /* Audio*/
    UPDATE( adecoded_stat, "%5i", p_item->p_stats->i_decoded_audio );
    UPDATE( aplayed_stat, "%5i", p_item->p_stats->i_played_abuffers );
    UPDATE( alost_stat, "%5i", p_item->p_stats->i_lost_abuffers );

#undef UPDATE

    vlc_mutex_unlock(& p_item->p_stats->lock );
}

void InputStatsPanel::clear()
{
}

