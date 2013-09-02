/*****************************************************************************
 * info_panels.cpp : Panels for the information dialogs
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

#define __STDC_FORMAT_MACROS 1
#define __STDC_CONSTANT_MACROS 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"
#include "components/info_panels.hpp"
#include "components/interface_widgets.hpp"
#include "info_widgets.hpp"
#include "dialogs/fingerprintdialog.hpp"
#include "adapters/chromaprint.hpp"

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
#include <QTextEdit>

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
    metaLayout->setVerticalSpacing( 0 );

    QFont smallFont = QApplication::font();
    smallFont.setPointSize( smallFont.pointSize() - 1 );
    smallFont.setBold( true );

    int line = 0; /* Counter for GridLayout */
    p_input = NULL;
    QLabel *label;

#define ADD_META( string, widget, col, colspan ) {                        \
    label = new QLabel( qtr( string ) ); label->setFont( smallFont );     \
    label->setContentsMargins( 3, 2, 0, 0 );                              \
    metaLayout->addWidget( label, line++, col, 1, colspan );              \
    widget = new QLineEdit;                                               \
    metaLayout->addWidget( widget, line, col, 1, colspan );               \
    CONNECT( widget, textEdited( QString ), this, enterEditMode() );      \
}

    /* Title, artist and album*/
    ADD_META( VLC_META_TITLE, title_text, 0, 10 ); line++;
    ADD_META( VLC_META_ARTIST, artist_text, 0, 10 ); line++;
    ADD_META( VLC_META_ALBUM, collection_text, 0, 7 );

    /* Date */
    label = new QLabel( qtr( VLC_META_DATE ) );
    label->setFont( smallFont ); label->setContentsMargins( 3, 2, 0, 0 );
    metaLayout->addWidget( label, line - 1, 7, 1, 2 );

    /* Date (Should be in years) */
    date_text = new QLineEdit;
    date_text->setAlignment( Qt::AlignRight );
    date_text->setInputMask("0000");
    date_text->setMaximumWidth( 140 );
    metaLayout->addWidget( date_text, line, 7, 1, -1 );
    line++;

    /* Genre Name */
    /* TODO List id3genres.h is not includable yet ? */
    ADD_META( VLC_META_GENRE, genre_text, 0, 7 );

    /* Number - on the same line */
    label = new QLabel( qtr( VLC_META_TRACK_NUMBER ) );
    label->setFont( smallFont ); label->setContentsMargins( 3, 2, 0, 0 );
    metaLayout->addWidget( label, line - 1, 7, 1, 3  );

    seqnum_text = new QLineEdit;
    seqnum_text->setMaximumWidth( 64 );
    seqnum_text->setAlignment( Qt::AlignRight );
    metaLayout->addWidget( seqnum_text, line, 7, 1, 1 );

    label = new QLabel( "/" ); label->setFont( smallFont );
    metaLayout->addWidget( label, line, 8, 1, 1 );

    seqtot_text = new QLineEdit;
    seqtot_text->setMaximumWidth( 64 );
    seqtot_text->setAlignment( Qt::AlignRight );
    metaLayout->addWidget( seqtot_text, line, 9, 1, 1 );
    line++;

    /* Rating - on the same line */
    /*
    metaLayout->addWidget( new QLabel( qtr( VLC_META_RATING ) ), line, 4, 1, 2 );
    rating_text = new QSpinBox; setSpinBounds( rating_text );
    metaLayout->addWidget( rating_text, line, 6, 1, 1 );
    */

    /* Now Playing - Useful for live feeds (HTTP, DVB, ETC...) */
    ADD_META( VLC_META_NOW_PLAYING, nowplaying_text, 0, 7 );
    nowplaying_text->setReadOnly( true ); line--;

    /* Language on the same line */
    ADD_META( VLC_META_LANGUAGE, language_text, 7, -1 ); line++;
    ADD_META( VLC_META_PUBLISHER, publisher_text, 0, 7 );

    fingerprintButton = new QPushButton( qtr("&Fingerprint") );
    fingerprintButton->setToolTip( qtr( "Find meta data using audio fingerprinting" ) );
    fingerprintButton->setVisible( false );
    metaLayout->addWidget( fingerprintButton, line, 7 , 3, -1 );
    CONNECT( fingerprintButton, clicked(), this, fingerprint() );

    line++;

    lblURL = new QLabel;
    lblURL->setOpenExternalLinks( true );
    lblURL->setTextFormat( Qt::RichText );
    metaLayout->addWidget( lblURL, line -1, 7, 1, -1 );

    ADD_META( VLC_META_COPYRIGHT, copyright_text, 0,  7 ); line++;

    /* ART_URL */
    art_cover = new CoverArtLabel( this, p_intf );
    metaLayout->addWidget( art_cover, line, 7, 6, 3, Qt::AlignLeft );

    ADD_META( VLC_META_ENCODED_BY, encodedby_text, 0, 7 ); line++;

    label = new QLabel( qtr( N_("Comments") ) ); label->setFont( smallFont );
    label->setContentsMargins( 3, 2, 0, 0 );
    metaLayout->addWidget( label, line++, 0, 1, 7 );
    description_text = new QTextEdit;
    description_text->setAcceptRichText( false );
    metaLayout->addWidget( description_text, line, 0, 1, 7 );
    // CONNECT( description_text, textChanged(), this, enterEditMode() ); //FIXME
    line++;

    /* VLC_META_SETTING: Useless */
    /* ADD_META( TRACKID )  Useless ? */
    /* ADD_URI - Do not show it, done outside */

    metaLayout->setColumnStretch( 1, 20 );
    metaLayout->setColumnMinimumWidth ( 1, 80 );
    metaLayout->setRowStretch( line, 10 );
#undef ADD_META

    CONNECT( seqnum_text, textEdited( QString ), this, enterEditMode() );
    CONNECT( seqtot_text, textEdited( QString ), this, enterEditMode() );

    CONNECT( date_text, textEdited( QString ), this, enterEditMode() );
//    CONNECT( THEMIM->getIM(), artChanged( QString ), this, enterEditMode() );
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
    p_input = p_item;

    char *psz_meta;
#define UPDATE_META( meta, widget ) {                                   \
    psz_meta = input_item_Get##meta( p_item );                          \
    widget->setText( !EMPTY_STR( psz_meta ) ? qfu( psz_meta ) : "" );   \
    free( psz_meta ); }

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
    psz_meta = input_item_GetURI( p_item );
    if( !EMPTY_STR( psz_meta ) )
        emit uriSet( qfu( psz_meta ) );
    fingerprintButton->setVisible( Chromaprint::isSupported( QString( psz_meta ) ) );
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
    UPDATE_META( EncodedBy, encodedby_text );

    UPDATE_META( Date, date_text );
    UPDATE_META( TrackNum, seqnum_text );
    UPDATE_META( TrackTotal, seqtot_text );
//    UPDATE_META( Setting, setting_text );
//    UPDATE_META_INT( Rating, rating_text );

    /* URL */
    psz_meta = input_item_GetURL( p_item );
    if( !EMPTY_STR( psz_meta ) )
    {
        QString newURL = qfu(psz_meta);
        if( currentURL != newURL )
        {
            currentURL = newURL;
            lblURL->setText( "<a href='" + currentURL + "'>" +
                             currentURL.remove( QRegExp( ".*://") ) + "</a>" );
        }
    }
    free( psz_meta );
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
    art_cover->setItem( p_item );
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
    input_item_SetTrackTotal(  p_input, qtu( seqtot_text->text() ) );
    input_item_SetDate(  p_input, qtu( date_text->text() ) );
    input_item_SetLanguage(  p_input, qtu( language_text->text() ) );

    input_item_SetCopyright( p_input, qtu( copyright_text->text() ) );
    input_item_SetPublisher( p_input, qtu( publisher_text->text() ) );
    input_item_SetDescription( p_input, qtu( description_text->toPlainText() ) );

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
    seqtot_text->clear();
    description_text->clear();
    date_text->clear();
    language_text->clear();
    nowplaying_text->clear();
    publisher_text->clear();
    encodedby_text->clear();
    art_cover->clear();
    fingerprintButton->setVisible( false );

    setEditMode( false );
    emit uriSet( "" );
}

void MetaPanel::fingerprint()
{
    FingerprintDialog *dialog = new FingerprintDialog( this, p_intf, p_input );
    CONNECT( dialog, metaApplied( input_item_t * ), this, fingerprintUpdate( input_item_t * ) );
    dialog->setAttribute( Qt::WA_DeleteOnClose, true );
    dialog->show();
}

void MetaPanel::fingerprintUpdate( input_item_t *p_item )
{
    update( p_item );
    setEditMode( true );
}

/**
 * Second Panel - Shows the extra metadata in a tree, non editable.
 **/
ExtraMetaPanel::ExtraMetaPanel( QWidget *parent ) : QWidget( parent )
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
     extraMetaTree->setHeaderHidden( true );
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
InfoPanel::InfoPanel( QWidget *parent ) : QWidget( parent )
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
     InfoTree->header()->hide();
#if QT_VERSION >= 0x050000
     InfoTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
     InfoTree->header()->setResizeMode(QHeaderView::ResizeToContents);
#endif
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
InputStatsPanel::InputStatsPanel( QWidget *parent ): QWidget( parent )
{
     QVBoxLayout *layout = new QVBoxLayout(this);

     QLabel *topLabel = new QLabel( qtr( "Current"
                 " media / stream " "statistics") );
     topLabel->setWordWrap( true );
     layout->addWidget( topLabel, 0, 0 );

     StatsTree = new QTreeWidget(this);
     StatsTree->setColumnCount( 3 );
     StatsTree->setHeaderHidden( true );

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
    input_bitrate_graph = new QTreeWidgetItem();
    input_bitrate_stat->addChild( input_bitrate_graph );
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

    layout->addWidget(StatsTree, 4, 0 );

    statsView = new VLCStatsView( this );
    statsView->setFrameStyle( QFrame::NoFrame );
    statsView->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed );
    input_bitrate_graph->setSizeHint( 1, QSize(0, 100) );
    QString graphlabel =
            QString( "<font style=\"color:#ff8c00\">%1</font><br/>%2" )
            .arg( qtr("Last 60 seconds") )
            .arg( qtr("Overall") );
    StatsTree->setItemWidget( input_bitrate_graph, 0, new QLabel( graphlabel ) );
    StatsTree->setItemWidget( input_bitrate_graph, 1, statsView );
}

void InputStatsPanel::hideEvent( QHideEvent * event )
{
    statsView->reset();
    QWidget::hideEvent( event );
}

/**
 * Update the Statistics
 **/
void InputStatsPanel::update( input_item_t *p_item )
{
    if ( !isVisible() ) return;
    assert( p_item );
    vlc_mutex_lock( &p_item->p_stats->lock );

#define UPDATE_INT( widget, calc... ) \
    { widget->setText( 1, QString::number( (qulonglong)calc ) ); }

#define UPDATE_FLOAT( widget, format, calc... ) \
    { QString str; widget->setText( 1 , str.sprintf( format, ## calc ) );  }

    UPDATE_INT( read_media_stat, (p_item->p_stats->i_read_bytes / 1024 ) );
    UPDATE_FLOAT( input_bitrate_stat,  "%6.0f", (float)(p_item->p_stats->f_input_bitrate *  8000 ));
    UPDATE_INT( demuxed_stat,    (p_item->p_stats->i_demux_read_bytes / 1024 ) );
    UPDATE_FLOAT( stream_bitrate_stat, "%6.0f", (float)(p_item->p_stats->f_demux_bitrate *  8000 ));
    UPDATE_INT( corrupted_stat,      p_item->p_stats->i_demux_corrupted );
    UPDATE_INT( discontinuity_stat,  p_item->p_stats->i_demux_discontinuity );

    statsView->addValue( p_item->p_stats->f_input_bitrate * 8000 );

    /* Video */
    UPDATE_INT( vdecoded_stat,     p_item->p_stats->i_decoded_video );
    UPDATE_INT( vdisplayed_stat,   p_item->p_stats->i_displayed_pictures );
    UPDATE_INT( vlost_frames_stat, p_item->p_stats->i_lost_pictures );

    /* Sout */
    UPDATE_INT( send_stat,        p_item->p_stats->i_sent_packets );
    UPDATE_INT( send_bytes_stat,  (p_item->p_stats->i_sent_bytes)/ 1024 );
    UPDATE_FLOAT( send_bitrate_stat, "%6.0f", (float)(p_item->p_stats->f_send_bitrate * 8000 ) );

    /* Audio*/
    UPDATE_INT( adecoded_stat, p_item->p_stats->i_decoded_audio );
    UPDATE_INT( aplayed_stat,  p_item->p_stats->i_played_abuffers );
    UPDATE_INT( alost_stat,    p_item->p_stats->i_lost_abuffers );

#undef UPDATE_INT
#undef UPDATE_FLOAT

    vlc_mutex_unlock(& p_item->p_stats->lock );
}

void InputStatsPanel::clear()
{
}
