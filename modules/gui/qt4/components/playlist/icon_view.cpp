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

#include "assert.h"

#define RECT_SIZE_W         120
#define RECT_SIZE_H         120
#define ART_SIZE_W          110
#define ART_SIZE_H          80
//#define OFFSET              (RECT_SIZE_W-ART_SIZE_W)/2
//#define ITEMS_SPACING       10
#define ART_RADIUS          5

QString AbstractPlViewItemDelegate::getMeta( const QModelIndex & index, int meta ) const
{
    return index.model()->index( index.row(),
                                  PLModel::columnFromMeta( meta ),
                                  index.parent() )
                                .data().toString();
}

void AbstractPlViewItemDelegate::paintPlayingItemBg( QPainter *painter, const QStyleOptionViewItem & option ) const
{
    painter->save();
    painter->setOpacity( 0.5 );
    painter->setBrush( QBrush( Qt::gray ) );
    painter->fillRect( option.rect, option.palette.color( QPalette::Dark ) );
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
            artPix = QPixmap( ":/noart" ).scaled( size, Qt::KeepAspectRatio, Qt::SmoothTransformation );
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

    QApplication::style()->drawPrimitive( QStyle::PE_PanelItemViewItem, &option,
                                          painter );

    painter->save();

    if( index.data( PLModel::IsCurrentRole ).toBool() )
    {
       painter->save();
       painter->setOpacity( 0.2 );
       painter->setBrush( QBrush( Qt::gray ) );
       painter->drawRoundedRect( option.rect.adjusted( 0, 0, -1, -1 ), ART_RADIUS, ART_RADIUS );
       painter->restore();
    }

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

    QFont font;
    font.setPointSize( 7 );
    font.setBold( index.data( Qt::FontRole ).value<QFont>().bold() );

    // Draw title
    font.setItalic( true );
    painter->setFont( font );

    QFontMetrics fm = painter->fontMetrics();
    QRect textRect = option.rect.adjusted( 1, ART_SIZE_H + 10, 0, -1 );
    textRect.setHeight( fm.height() + 1 );

    painter->drawText( textRect,
                      fm.elidedText( title, Qt::ElideRight, textRect.width() ),
                      QTextOption( Qt::AlignCenter ) );

    // Draw artist
    painter->setPen( painter->pen().color().lighter( 150 ) );
    font.setItalic( false );
    painter->setFont( font );
    fm = painter->fontMetrics();

    textRect = textRect.adjusted( 0, textRect.height(),
                                    0, textRect.height() );
    painter->drawText(  textRect,
                        fm.elidedText( artist, Qt::ElideRight, textRect.width() ),
                        QTextOption( Qt::AlignCenter ) );

    painter->restore();
}

QSize PlIconViewItemDelegate::sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    return QSize( RECT_SIZE_W, RECT_SIZE_H );
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

    QApplication::style()->drawPrimitive( QStyle::PE_PanelItemViewItem, &option, painter );

    if( index.data( PLModel::IsCurrentRole ).toBool() )
        paintPlayingItemBg( painter, option );

    painter->drawPixmap( option.rect.topLeft() + QPoint(3,3)
                         + QPoint( (LISTVIEW_ART_SIZE - artPix.width()) / 2,
                                   (LISTVIEW_ART_SIZE - artPix.height()) / 2 ),
                         artPix );


    int textH = option.fontMetrics.height() + 2;
    int marginY = ( option.rect.height() / 2 ) - textH;

    QRect textRect = option.rect.adjusted( LISTVIEW_ART_SIZE + 10,
                                           marginY,
                                           -10,
                                           marginY * -1 - ( artistAlbum.isEmpty() ? 0 : textH ) );

    painter->save();

    if( option.state & QStyle::State_Selected )
        painter->setPen( option.palette.color( QPalette::HighlightedText ) );

    QTextOption textOpt( Qt::AlignVCenter | Qt::AlignLeft );
    textOpt.setWrapMode( QTextOption::NoWrap );

    QFont f( option.font );
    if( index.data( PLModel::IsCurrentRole ).toBool() ) f.setBold( true );

    f.setItalic( true );
    painter->setFont( f );

    painter->drawText( textRect, title, textOpt );

    f.setItalic( false );
    painter->setFont( f );
    textRect.moveTop( textRect.top() + textH );

    painter->drawText( textRect, artistAlbum, textOpt );

    painter->restore();
}

QSize PlListViewItemDelegate::sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
  return QSize( LISTVIEW_ART_SIZE + 6, LISTVIEW_ART_SIZE + 6 );
}

PlIconView::PlIconView( PLModel *model, QWidget *parent ) : QListView( parent )
{
    setModel( model );
    setViewMode( QListView::IconMode );
    setMovement( QListView::Static );
    setResizeMode( QListView::Adjust );
    setGridSize( QSize( RECT_SIZE_W, RECT_SIZE_H ) );
    setWrapping( true );
    setUniformItemSizes( true );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    setAcceptDrops( true );

    PlIconViewItemDelegate *delegate = new PlIconViewItemDelegate( this );
    setItemDelegate( delegate );
}

PlListView::PlListView( PLModel *model, QWidget *parent ) : QListView( parent )
{
    setModel( model );
    setViewMode( QListView::ListMode );
    setUniformItemSizes( true );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    setAcceptDrops( true );
    setAlternatingRowColors( true );

    PlListViewItemDelegate *delegate = new PlListViewItemDelegate( this );
    setItemDelegate( delegate );
}
