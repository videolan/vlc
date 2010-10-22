/*****************************************************************************
 * playlist.cpp : Custom widgets for the playlist
 ****************************************************************************
 * Copyright © 2007-2010 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#include "components/playlist/standardpanel.hpp"
#include "components/playlist/selector.hpp"
#include "components/playlist/playlist.hpp"
#include "components/playlist/playlist_model.hpp"

#include "input_manager.hpp" /* art signal */
#include "main_interface.hpp" /* DropEvent TODO remove this*/

#include <QGroupBox>

#include <iostream>
/**********************************************************************
 * Playlist Widget. The embedded playlist
 **********************************************************************/

PlaylistWidget::PlaylistWidget( intf_thread_t *_p_i, QWidget *_par )
               : QSplitter( _par ), p_intf ( _p_i )
{
    setContentsMargins( 3, 3, 3, 3 );

    /* We use a QSplitter for the left part*/
    leftSplitter = new QSplitter( Qt::Vertical, this );

    /* Source Selector */
    selector = new PLSelector( this, p_intf );

    leftSplitter->addWidget( selector);

    /* Create a Container for the Art Label
       in order to have a beautiful resizing for the selector above it */
    QWidget *artContainer = new QWidget;
    QHBoxLayout *artContLay = new QHBoxLayout( artContainer );
    artContLay->setMargin( 0 );
    artContLay->setSpacing( 0 );
    artContainer->setMaximumHeight( 128 );

    /* Art label */
    art = new ArtLabel( artContainer, p_intf );
    art->setToolTip( qtr( "Double click to get media information" ) );

    CONNECT( THEMIM->getIM(), artChanged( QString ),
             art, showArtUpdate( const QString& ) );

    artContLay->addWidget( art, 1 );

    leftSplitter->addWidget( artContainer );

    /* Initialisation of the playlist */
    playlist_t * p_playlist = THEPL;
    PL_LOCK;
    playlist_item_t *p_root = THEPL->p_playing;
    PL_UNLOCK;

    rightPanel = new StandardPLPanel( this, p_intf, THEPL, p_root, selector );

    /* Connect the activation of the selector to a redefining of the PL */
    DCONNECT( selector, activated( playlist_item_t * ),
              rightPanel, setRoot( playlist_item_t * ) );

    rightPanel->setRoot( p_root );

    /* Add the two sides of the QSplitter */
    addWidget( leftSplitter );
    addWidget( rightPanel );

    QList<int> sizeList;
    sizeList << 180 << 420 ;
    setSizes( sizeList );
    //setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Expanding );
    setStretchFactor( 0, 0 );
    setStretchFactor( 1, 3 );
    leftSplitter->setMaximumWidth( 250 );
    setCollapsible( 1, false );

    /* In case we want to keep the splitter information */
    // components shall never write there setting to a fixed location, may infer
    // with other uses of the same component...
    getSettings()->beginGroup("Playlist");
    restoreState( getSettings()->value("splitterSizes").toByteArray());
    leftSplitter->restoreState( getSettings()->value("leftSplitterGeometry").toByteArray() );
    getSettings()->endGroup();

    setAcceptDrops( true );
    setWindowTitle( qtr( "Playlist" ) );
    setWindowRole( "vlc-playlist" );
    setWindowIcon( QApplication::windowIcon() );
}

PlaylistWidget::~PlaylistWidget()
{
    getSettings()->beginGroup("Playlist");
    getSettings()->setValue( "splitterSizes", saveState() );
    getSettings()->setValue( "leftSplitterGeometry", leftSplitter->saveState() );
    getSettings()->endGroup();
    msg_Dbg( p_intf, "Playlist Destroyed" );
}

void PlaylistWidget::dropEvent( QDropEvent *event )
{
    if( p_intf->p_sys->p_mi )
        p_intf->p_sys->p_mi->dropEventPlay( event, false );
}
void PlaylistWidget::dragEnterEvent( QDragEnterEvent *event )
{
    event->acceptProposedAction();
}

void PlaylistWidget::closeEvent( QCloseEvent *event )
{
    if( THEDP->isDying() )
    {
        p_intf->p_sys->p_mi->playlistVisible = true;
        event->accept();
    }
    else
    {
        p_intf->p_sys->p_mi->playlistVisible = false;
        hide();
        event->ignore();
    }
}

void PlaylistWidget::forceHide()
{
    leftSplitter->hide();
    rightPanel->hide();
    updateGeometry();
}

void PlaylistWidget::forceShow()
{
    leftSplitter->show();
    rightPanel->show();
    updateGeometry();
}


#include <QSignalMapper>
#include <QMenu>
#include <QPainter>
LocationBar::LocationBar( PLModel *m )
{
    model = m;
    mapper = new QSignalMapper( this );
    CONNECT( mapper, mapped( int ), this, invoke( int ) );

    btnMore = new LocationButton( "...", false, true, this );
    menuMore = new QMenu( this );
    btnMore->setMenu( menuMore );
}

void LocationBar::setIndex( const QModelIndex &index )
{
    qDeleteAll( buttons );
    buttons.clear();
    qDeleteAll( actions );
    actions.clear();

    QModelIndex i = index;
    bool first = true;

    while( true )
    {
        PLItem *item = model->getItem( i );

        char *fb_name = input_item_GetTitleFbName( item->inputItem() );
        QString text = qfu(fb_name);
        free(fb_name);

        QAbstractButton *btn = new LocationButton( text, first, !first, this );
        btn->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
        buttons.append( btn );

        QAction *action = new QAction( text, this );
        actions.append( action );
        CONNECT( btn, clicked(), action, trigger() );

        mapper->setMapping( action, item->id() );
        CONNECT( action, triggered(), mapper, map() );

        first = false;

        if( i.isValid() ) i = i.parent();
        else break;
    }

    QString prefix;
    for( int a = actions.count() - 1; a >= 0 ; a-- )
    {
        actions[a]->setText( prefix + actions[a]->text() );
        prefix += QString("  ");
    }

    if( isVisible() ) layOut( size() );
}

void LocationBar::setRootIndex()
{
    setIndex( QModelIndex() );
}

void LocationBar::invoke( int i_id )
{
    QModelIndex index = model->index( i_id, 0 );
    emit invoked ( index );
}

void LocationBar::layOut( const QSize& size )
{
    menuMore->clear();
    widths.clear();

    int count = buttons.count();
    int totalWidth = 0;
    for( int i = 0; i < count; i++ )
    {
        int w = buttons[i]->sizeHint().width();
        widths.append( w );
        totalWidth += w;
        if( totalWidth > size.width() ) break;
    }

    int x = 0;
    int shown = widths.count();

    if( totalWidth > size.width() && count > 1 )
    {
        QSize sz = btnMore->sizeHint();
        btnMore->setGeometry( 0, 0, sz.width(), size.height() );
        btnMore->show();
        x = sz.width();
        totalWidth += x;
    }
    else
    {
        btnMore->hide();
    }
    for( int i = count - 1; i >= 0; i-- )
    {
        if( totalWidth <= size.width() || i == 0)
        {
            buttons[i]->setGeometry( x, 0, qMin( size.width() - x, widths[i] ), size.height() );
            buttons[i]->show();
            x += widths[i];
            totalWidth -= widths[i];
        }
        else
        {
            menuMore->addAction( actions[i] );
            buttons[i]->hide();
            if( i < shown ) totalWidth -= widths[i];
        }
    }
}

void LocationBar::resizeEvent ( QResizeEvent * event )
{
    layOut( event->size() );
}

QSize LocationBar::sizeHint() const
{
    return btnMore->sizeHint();
}

LocationButton::LocationButton( const QString &text, bool bold,
                                bool arrow, QWidget * parent )
  : b_arrow( arrow ), QPushButton( parent )
{
    QFont font;
    font.setBold( bold );
    setFont( font );
    setText( text );
}

#define PADDING 4

void LocationButton::paintEvent ( QPaintEvent * event )
{
    QStyleOptionButton option;
    option.initFrom( this );
    option.state |= QStyle::State_Enabled;
    QPainter p( this );

    if( underMouse() )
    {
        p.save();
        p.setRenderHint( QPainter::Antialiasing, true );
        QColor c = palette().color( QPalette::Highlight );
        p.setPen( c );
        p.setBrush( c.lighter( 150 ) );
        p.setOpacity( 0.2 );
        p.drawRoundedRect( option.rect.adjusted( 0, 2, 0, -2 ), 5, 5 );
        p.restore();
    }

    QRect r = option.rect.adjusted( PADDING, 0, -PADDING - (b_arrow ? 10 : 0), 0 );

    QString str( text() );
    /* This check is absurd, but either it is not done properly inside elidedText(),
       or boundingRect() is wrong */
    if( r.width() < fontMetrics().boundingRect( text() ).width() )
        str = fontMetrics().elidedText( text(), Qt::ElideRight, r.width() );
    p.drawText( r, Qt::AlignVCenter | Qt::AlignLeft, str );

    if( b_arrow )
    {
        option.rect.setWidth( 10 );
        option.rect.moveRight( rect().right() );
        style()->drawPrimitive( QStyle::PE_IndicatorArrowRight, &option, &p );
    }
}

QSize LocationButton::sizeHint() const
{
    QSize s( fontMetrics().boundingRect( text() ).size() );
    /* Add two pixels to width: font metrics are buggy, if you pass text through elidation
       with exactly the width of its bounding rect, sometimes it still elides */
    s.setWidth( s.width() + ( 2 * PADDING ) + ( b_arrow ? 10 : 0 ) + 2 );
    s.setHeight( s.height() + 2 * PADDING );
    return s;
}

#undef PADDING
