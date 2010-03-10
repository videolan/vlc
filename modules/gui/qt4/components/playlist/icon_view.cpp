/*****************************************************************************
 * icon_view.cpp : Icon view for the Playlist
 ****************************************************************************
 * Copyright © 2010 the VideoLAN team
 * $Id$
 *
 * Authors:         Jean-Baptiste Kempf <jb@videolan.org>
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

#include "components/playlist/icon_view.hpp"
#include "components/playlist/playlist_model.hpp"
#include "components/playlist/sorting.h"
#include "input_manager.hpp"

#include <QApplication>
#include <QPainter>
#include <QRect>
#include <QStyleOptionViewItem>
#include <QFontMetrics>
#include <QPixmapCache>
#include <QDrag>

#include "assert.h"

#define ART_SIZE_W          110
#define ART_SIZE_H          80
#define ART_RADIUS          5
#define SPACER              5

QString AbstractPlViewItemDelegate::getMeta( const QModelIndex & index, int meta ) const
{
    return index.model()->index( index.row(),
                                  PLModel::columnFromMeta( meta ),
                                  index.parent() )
                                .data().toString();
}

void AbstractPlViewItemDelegate::paintBackground(
    QPainter *painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    /* FIXME: This does not indicate item selection in all QStyles, so for the time being we
       have to draw it ourselves, to ensure visible effect of selection on all platforms */
    /* QApplication::style()->drawPrimitive( QStyle::PE_PanelItemViewItem, &option,
                                            painter ); */

    painter->save();
    QRect r = option.rect.adjusted( 0, 0, -1, -1 );
    if( option.state & QStyle::State_Selected )
    {
        painter->setBrush( option.palette.color( QPalette::Highlight ) );
        painter->setPen( option.palette.color( QPalette::Highlight ).darker( 150 ) );
        painter->drawRect( r );
    }
    else if( index.data( PLModel::IsCurrentRole ).toBool() )
    {
        painter->setBrush( QBrush( Qt::lightGray ) );
        painter->setPen( QColor( Qt::darkGray ) );
        painter->drawRect( r );
    }
    if( option.state & QStyle::State_MouseOver )
    {
        painter->setOpacity( 0.5 );
        painter->setPen( Qt::NoPen );
        painter->setBrush( option.palette.color( QPalette::Highlight ).lighter( 150 ) );
        painter->drawRect( option.rect );
    }
    painter->restore();
}

QPixmap AbstractPlViewItemDelegate::getArtPixmap( const QModelIndex & index, const QSize & size ) const
{
    PLItem *item = static_cast<PLItem*>( index.internalPointer() );
    assert( item );

    QString artUrl = InputManager::decodeArtURL( item->inputItem() );

    if( artUrl.isEmpty() )
    {
        for( int i = 0; i < item->childCount(); i++ )
        {
            artUrl = InputManager::decodeArtURL( item->child( i )->inputItem() );
            if( !artUrl.isEmpty() )
                break;
        }
    }

    QPixmap artPix;

    QString key = artUrl + QString("%1%2").arg(size.width()).arg(size.height());

    if( !QPixmapCache::find( key, artPix ))
    {
        if( artUrl.isEmpty() || !artPix.load( artUrl ) )
        {
            key = QString("noart%1%2").arg(size.width()).arg(size.height());
            if( !QPixmapCache::find( key, artPix ) )
            {
                artPix = QPixmap( ":/noart" ).scaled( size,
                                                      Qt::KeepAspectRatio,
                                                      Qt::SmoothTransformation );
                QPixmapCache::insert( key, artPix );
            }
        }
        else
        {
            artPix = artPix.scaled( size, Qt::KeepAspectRatio, Qt::SmoothTransformation );
            QPixmapCache::insert( key, artPix );
        }
    }

    return artPix;
}

void PlIconViewItemDelegate::paint( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    QString title = getMeta( index, COLUMN_TITLE );
    QString artist = getMeta( index, COLUMN_ARTIST );

    QPixmap artPix = getArtPixmap( index, QSize( ART_SIZE_W, ART_SIZE_H ) );

    paintBackground( painter, option, index );

    painter->save();

    QRect artRect( option.rect.x() + 5 + ( ART_SIZE_W - artPix.width() ) / 2,
                   option.rect.y() + 5 + ( ART_SIZE_H - artPix.height() ) / 2,
                   artPix.width(), artPix.height() );

    // Draw the drop shadow
    painter->save();
    painter->setOpacity( 0.7 );
    painter->setBrush( QBrush( Qt::darkGray ) );
    painter->setPen( Qt::NoPen );
    painter->drawRoundedRect( artRect.adjusted( 0, 0, 2, 2 ), ART_RADIUS, ART_RADIUS );
    painter->restore();

    // Draw the art pixmap
    QPainterPath artRectPath;
    artRectPath.addRoundedRect( artRect, ART_RADIUS, ART_RADIUS );
    painter->setClipPath( artRectPath );
    painter->drawPixmap( artRect, artPix );
    painter->setClipping( false );

    if( option.state & QStyle::State_Selected )
        painter->setPen( option.palette.color( QPalette::HighlightedText ) );

    QFont font( index.data( Qt::FontRole ).value<QFont>() );
    font.setPointSize( 7 );

    //Draw children indicator
    if( !index.data( PLModel::IsLeafNodeRole ).toBool() )
    {
        painter->setOpacity( 0.75 );
        QRect r( option.rect );
        r.setSize( QSize( 25, 25 ) );
        r.translate( 5, 5 );
        painter->fillRect( r, option.palette.color( QPalette::Mid ) );
        painter->setOpacity( 1.0 );
        QPixmap dirPix( ":/type/node" );
        QRect r2( dirPix.rect() );
        r2.moveCenter( r.center() );
        painter->drawPixmap( r2, dirPix );
    }

    // Draw title
    font.setItalic( true );
    painter->setFont( font );

    QFontMetrics fm = painter->fontMetrics();
    QRect textRect = option.rect.adjusted( 1, ART_SIZE_H + 10, 0, -1 );
    textRect.setHeight( fm.height() );

    painter->drawText( textRect,
                      fm.elidedText( title, Qt::ElideRight, textRect.width() ),
                      QTextOption( Qt::AlignCenter ) );

    // Draw artist
    painter->setPen( painter->pen().color().lighter( 150 ) );
    font.setItalic( false );
    painter->setFont( font );
    fm = painter->fontMetrics();

    textRect.moveTop( textRect.bottom() + 1 );

    painter->drawText(  textRect,
                        fm.elidedText( artist, Qt::ElideRight, textRect.width() ),
                        QTextOption( Qt::AlignCenter ) );

    painter->restore();
}

QSize PlIconViewItemDelegate::sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    QFont f;
    f.setPointSize( 7 );
    f.setBold( true );
    QFontMetrics fm( f );
    int textHeight = fm.height();
    QSize sz ( ART_SIZE_W + 2 * SPACER,
               ART_SIZE_H + 3 * SPACER + 2 * textHeight + 1 );
    return sz;
}


#define LISTVIEW_ART_SIZE 45

void PlListViewItemDelegate::paint( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    QModelIndex parent = index.parent();
    QModelIndex i;

    QString title = getMeta( index, COLUMN_TITLE );
    QString duration = getMeta( index, COLUMN_DURATION );
    if( !duration.isEmpty() ) title += QString(" [%1]").arg( duration );

    QString artist = getMeta( index, COLUMN_ARTIST );
    QString album = getMeta( index, COLUMN_ALBUM );
    QString trackNum = getMeta( index, COLUMN_TRACK_NUMBER );
    QString artistAlbum = artist
                          + ( artist.isEmpty() ? QString() : QString( ": " ) )
                          + album
                          + ( album.isEmpty() || trackNum.isEmpty() ?
                              QString() : QString( " [#%1]" ).arg( trackNum ) );

    QPixmap artPix = getArtPixmap( index, QSize( LISTVIEW_ART_SIZE, LISTVIEW_ART_SIZE ) );

    //Draw selection rectangle and current playing item indication
    paintBackground( painter, option, index );

    QRect artRect( artPix.rect() );
    artRect.moveCenter( QPoint( artRect.center().x() + 3,
                                option.rect.center().y() ) );
    //Draw album art
    painter->drawPixmap( artRect, artPix );

    //Start drawing text
    painter->save();

    if( option.state & QStyle::State_Selected )
        painter->setPen( option.palette.color( QPalette::HighlightedText ) );

    QTextOption textOpt( Qt::AlignVCenter | Qt::AlignLeft );
    textOpt.setWrapMode( QTextOption::NoWrap );

    QFont f( index.data( Qt::FontRole ).value<QFont>() );

    //Draw title info
    f.setItalic( true );
    painter->setFont( f );
    QFontMetrics fm( painter->fontMetrics() );

    QRect textRect = option.rect.adjusted( LISTVIEW_ART_SIZE + 10, 0, -10, 0 );
    if( !artistAlbum.isEmpty() )
    {
        textRect.setHeight( fm.height() );
        textRect.moveBottom( option.rect.center().y() - 2 );
    }

    //Draw children indicator
    if( !index.data( PLModel::IsLeafNodeRole ).toBool() )
    {
        QPixmap dirPix = QPixmap( ":/type/node" );
        painter->drawPixmap( QPoint( textRect.x(), textRect.center().y() - dirPix.height() / 2 ),
                             dirPix );
        textRect.setLeft( textRect.x() + dirPix.width() + 5 );
    }

    painter->drawText( textRect,
                       fm.elidedText( title, Qt::ElideRight, textRect.width() ),
                       textOpt );

    // Draw artist and album info
    if( !artistAlbum.isEmpty() )
    {
        f.setItalic( false );
        painter->setFont( f );
        fm = painter->fontMetrics();

        textRect.moveTop( textRect.bottom() + 4 );
        textRect.setLeft( textRect.x() + 20 );

        painter->drawText( textRect,
                           fm.elidedText( artistAlbum, Qt::ElideRight, textRect.width() ),
                           textOpt );
    }

    painter->restore();
}

QSize PlListViewItemDelegate::sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
  QFont f;
  f.setBold( true );
  QFontMetrics fm( f );
  int height = qMax( LISTVIEW_ART_SIZE, 2 * fm.height() + 4 ) + 6;
  return QSize( 0, height );
}

static void plViewStartDrag( QAbstractItemView *view, const Qt::DropActions & supportedActions )
{
    QDrag *drag = new QDrag( view );
    drag->setPixmap( QPixmap( ":/noart64" ) );
    drag->setMimeData( view->model()->mimeData(
        view->selectionModel()->selectedIndexes() ) );
    drag->exec( supportedActions );
}

PlIconView::PlIconView( PLModel *model, QWidget *parent ) : QListView( parent )
{
    PlIconViewItemDelegate *delegate = new PlIconViewItemDelegate( this );

    setModel( model );
    setViewMode( QListView::IconMode );
    setMovement( QListView::Static );
    setResizeMode( QListView::Adjust );
    setGridSize( delegate->sizeHint() );
    setWrapping( true );
    setUniformItemSizes( true );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    setDragEnabled(true);
    /* dropping in QListView::IconMode does not seem to work */
    //setAcceptDrops( true );
    //setDropIndicatorShown(true);

    setItemDelegate( delegate );
}

void PlIconView::startDrag ( Qt::DropActions supportedActions )
{
    plViewStartDrag( this, supportedActions );
}

PlListView::PlListView( PLModel *model, QWidget *parent ) : QListView( parent )
{
    setModel( model );
    setViewMode( QListView::ListMode );
    setUniformItemSizes( true );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    setAlternatingRowColors( true );
    setDragEnabled(true);
    setAcceptDrops( true );
    setDropIndicatorShown(true);

    PlListViewItemDelegate *delegate = new PlListViewItemDelegate( this );
    setItemDelegate( delegate );
}

void PlListView::startDrag ( Qt::DropActions supportedActions )
{
    plViewStartDrag( this, supportedActions );
}

void PlTreeView::startDrag ( Qt::DropActions supportedActions )
{
    plViewStartDrag( this, supportedActions );
}

