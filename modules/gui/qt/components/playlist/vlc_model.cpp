/*****************************************************************************
 * vlc_model.cpp : base for playlist and ml model
 ****************************************************************************
 * Copyright (C) 2010 the VideoLAN team and AUTHORS
 *
 * Authors: Srikanth Raju <srikiraju#gmail#com>
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

#include "vlc_model.hpp"
#include "input_manager.hpp"                            /* THEMIM */
#include "pixmaps/types/type_unknown.xpm"

#include <QImageReader>

VLCModelSubInterface::VLCModelSubInterface()
{
}

VLCModelSubInterface::~VLCModelSubInterface()
{
}

int VLCModelSubInterface::columnFromMeta( int meta_col )
{
    int meta = 1, column = 0;

    while( meta != meta_col && meta != COLUMN_END )
    {
        meta <<= 1;
        column++;
    }

    return column;
}

VLCModel::VLCModel( intf_thread_t *_p_intf, QObject *parent )
    : QAbstractItemModel( parent ), VLCModelSubInterface(), p_intf(_p_intf)
{
    /* Icons initialization */
#define ADD_ICON(type, x) icons[ITEM_TYPE_##type] = QIcon( x )
    ADD_ICON( UNKNOWN , QPixmap( type_unknown_xpm ) );
    ADD_ICON( FILE, ":/type/file.svg" );
    ADD_ICON( DIRECTORY, ":/type/directory.svg" );
    ADD_ICON( DISC, ":/type/disc.svg" );
    ADD_ICON( CARD, ":/type/capture-card.svg" );
    ADD_ICON( STREAM, ":/type/stream.svg" );
    ADD_ICON( PLAYLIST, ":/type/playlist.svg" );
    ADD_ICON( NODE, ":/type/node.svg" );
#undef ADD_ICON
}

VLCModel::~VLCModel()
{

}

QString VLCModel::getMeta( const QModelIndex & index, int meta )
{
    return index.model()->index( index.row(), columnFromMeta( meta ), index.parent() ).
        data().toString();
}

QPixmap VLCModel::getArtPixmap( const QModelIndex & index, const QSize & size )
{
    QString artUrl = index.sibling( index.row(),
                     VLCModel::columnFromMeta(COLUMN_COVER) ).data().toString();
    QPixmap artPix;

    QString key = artUrl + QString("%1%2").arg(size.width()).arg(size.height());

    if( !QPixmapCache::find( key, artPix ))
    {
        if( artUrl.isEmpty() == false )
        {
            QImageReader reader( artUrl );
            reader.setDecideFormatFromContent( true );
            artPix = QPixmap::fromImageReader( &reader ).scaled( size );
            if ( artPix.isNull() == false )
            {
                QPixmapCache::insert( key, artPix );
                return artPix;
            }
        }
        key = QString("noart%1%2").arg(size.width()).arg(size.height());
        if( !QPixmapCache::find( key, artPix ) )
        {
            artPix = QPixmap( ":/noart" ).scaled( size,
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation );
            QPixmapCache::insert( key, artPix );
        }
    }
    return artPix;
}

QVariant VLCModel::headerData( int section, Qt::Orientation orientation,
                              int role ) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    int meta_col = columnToMeta( section );

    if( meta_col == COLUMN_END ) return QVariant();

    return QVariant( qfu( psz_column_title( meta_col ) ) );
}

int VLCModel::columnToMeta( int _column )
{
    int meta = 1, column = 0;

    while( column != _column && meta != COLUMN_END )
    {
        meta <<= 1;
        column++;
    }

    return meta;
}

int VLCModel::metaToColumn( int _meta )
{
    int meta = 1, column = 0;

    while( meta != COLUMN_END )
    {
        if ( meta & _meta )
            break;
        meta <<= 1;
        column++;
    }

    return column;
}

int VLCModel::itemId( const QModelIndex &index ) const
{
    AbstractPLItem *item = getItem( index );
    if ( !item ) return -1;
    return item->id();
}

AbstractPLItem *VLCModel::getItem( const QModelIndex &index ) const
{
    if( index.isValid() )
        return static_cast<AbstractPLItem*>( index.internalPointer() );
    else return NULL;
}

QString VLCModel::getURI( const QModelIndex &index ) const
{
    AbstractPLItem *item = getItem( index );
    if ( !item ) return QString();
    return item->getURI();
}

input_item_t * VLCModel::getInputItem( const QModelIndex &index ) const
{
    AbstractPLItem *item = getItem( index );
    if ( !item ) return NULL;
    return item->inputItem();
}

QString VLCModel::getTitle( const QModelIndex &index ) const
{
    AbstractPLItem *item = getItem( index );
    if ( !item ) return QString();
    return item->getTitle();
}

bool VLCModel::isCurrent( const QModelIndex &index ) const
{
    AbstractPLItem *item = getItem( index );
    if ( !item ) return false;
    return item->inputItem() == THEMIM->currentInputItem();
}

int VLCModel::columnCount( const QModelIndex & ) const
{
    return columnFromMeta( COLUMN_END );
}

void VLCModel::ensureArtRequested( const QModelIndex &index )
{
    if ( index.isValid() && hasChildren( index ) )
    {
        bool b_access = var_InheritBool( THEPL, "metadata-network-access" );
        if ( !b_access ) return;
        int nbnodes = rowCount( index );
        QModelIndex child;
        for( int row = 0 ; row < nbnodes ; row++ )
        {
            child = index.child( row, COLUMN_COVER );
            if ( child.isValid() && child.data().toString().isEmpty() )
                THEMIM->getIM()->requestArtUpdate( getInputItem( child ), false );
        }
    }
}

