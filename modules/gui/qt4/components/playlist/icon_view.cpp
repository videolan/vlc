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
#include "input_manager.hpp"

#include <QApplication>
#include <QPainter>
#include <QRect>
#include <QStyleOptionViewItem>
#include <QFontMetrics>
#include <QPixmapCache>

#include "assert.h"

#define RECT_SIZE_W         100
#define RECT_SIZE_H         105
#define ART_SIZE            64
#define OFFSET              (RECT_SIZE_W-64)/2
#define ITEMS_SPACING       10
#define ART_RADIUS          5


static const QRect drawRect = QRect( 0, 0, RECT_SIZE_W, RECT_SIZE_H );
static const QRect artRect = drawRect.adjusted( OFFSET - 1, 2, - OFFSET, - OFFSET *2 );

void PlListViewItemDelegate::paint( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    PLItem *currentItem = static_cast<PLItem*>( index.internalPointer() );
    assert( currentItem );

    char *meta;

    meta = input_item_GetTitleFbName( currentItem->inputItem() );
    QString title = qfu( meta );
    free( meta );

    meta = input_item_GetArtist( currentItem->inputItem() );
    QString artist = qfu( meta );
    free( meta );

    QString artUrl = InputManager::decodeArtURL( currentItem->inputItem() );

    // look up through all children and use the first picture found
    if( artUrl.isEmpty() )
    {
        int children = currentItem->childCount();
        for( int i = 0; i < children; i++ )
        {
            PLItem *child = currentItem->child( i );
            artUrl = InputManager::decodeArtURL( child->inputItem() );
            if( !artUrl.isEmpty() )
                break;
        }
    }

    /*if( option.state & QStyle::State_Selected )
         painter->fillRect(option.rect, option.palette.highlight());*/
    QApplication::style()->drawPrimitive( QStyle::PE_PanelItemViewItem, &option,
                                          painter );

    // picture where all the rendering happens and which will be cached
    QPixmap pix;

    QString key = title + artist + artUrl
                  + QString( index.data( PLModel::IsCurrentRole ).toBool() );
    if(QPixmapCache::find( key, pix ))
    {
        // cool, we found it in the cache
        painter->drawPixmap( option.rect, pix );
        return;
    }

    // load album art
    QPixmap artPix;
    if( artUrl.isEmpty() || !artPix.load( artUrl ) )
        artPix = QPixmap( ":/noart64" );
    else
        artPix = artPix.scaled( ART_SIZE, ART_SIZE,
                Qt::KeepAspectRatioByExpanding );

    pix = QPixmap( RECT_SIZE_W, RECT_SIZE_H );
    pix.fill( Qt::transparent );

    QPainter *pixpainter = new QPainter( &pix );

    pixpainter->setRenderHints(
            QPainter::Antialiasing | QPainter::SmoothPixmapTransform |
            QPainter::TextAntialiasing );

    if( index.data( PLModel::IsCurrentRole ).toBool() )
    {
       pixpainter->save();
       pixpainter->setOpacity( 0.2 );
       pixpainter->setBrush( QBrush( Qt::gray ) );
       pixpainter->drawRoundedRect( 0, 0, RECT_SIZE_W, RECT_SIZE_H, ART_RADIUS, ART_RADIUS );
       pixpainter->restore();
    }

    // Draw the drop shadow
    pixpainter->save();
    pixpainter->setOpacity( 0.7 );
    pixpainter->setBrush( QBrush( Qt::gray ) );
    pixpainter->drawRoundedRect( artRect.adjusted( 2, 2, 2, 2 ), ART_RADIUS, ART_RADIUS );
    pixpainter->restore();

    // Draw the art pix
    QPainterPath artRectPath;
    artRectPath.addRoundedRect( artRect, ART_RADIUS, ART_RADIUS );
    pixpainter->setClipPath( artRectPath );
    pixpainter->drawPixmap( artRect, artPix );
    pixpainter->setClipping( false );

    QColor text = qApp->palette().text().color();

    // Draw title
    pixpainter->setPen( text );
    QFont font;
    font.setPointSize( 7 );
    font.setItalic(true);
    font.setBold( index.data( Qt::FontRole ).value<QFont>().bold() );
    pixpainter->setFont( font );
    QFontMetrics fm = pixpainter->fontMetrics();
    QRect textRect = drawRect.adjusted( 1, ART_SIZE + 4, 0, -1 );
    textRect.setHeight( fm.height() + 1 );

    pixpainter->drawText( textRect,
                      fm.elidedText( title, Qt::ElideRight, textRect.width() ),
                      QTextOption( Qt::AlignCenter ) );

    // Draw artist
    pixpainter->setPen( text.lighter( 240 ) );
    font.setItalic( false );
    pixpainter->setFont( font );
    fm = pixpainter->fontMetrics();


    textRect = textRect.adjusted( 0, textRect.height(),
                                    0, textRect.height() );
    pixpainter->drawText(  textRect,
                    fm.elidedText( artist, Qt::ElideRight, textRect.width() ),
                    QTextOption( Qt::AlignCenter ) );

    delete pixpainter; // Ensure all paint operations have finished

    // Here real drawing happens
    painter->drawPixmap( option.rect, pix );

    // Cache the rendering
    QPixmapCache::insert( key, pix );
}

QSize PlListViewItemDelegate::sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    return QSize( RECT_SIZE_W, RECT_SIZE_H );
}


PlIconView::PlIconView( PLModel *model, QWidget *parent ) : QListView( parent )
{
    setModel( model );
    setViewMode( QListView::IconMode );
    setMovement( QListView::Static );
    setResizeMode( QListView::Adjust );
    setGridSize( QSize( RECT_SIZE_W, RECT_SIZE_H ) );
    setUniformItemSizes( true );
    setWrapping( true );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    setAcceptDrops( true );

    PlListViewItemDelegate *pl = new PlListViewItemDelegate( this );
    setItemDelegate( pl );
}
