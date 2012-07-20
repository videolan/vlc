/*****************************************************************************
 * vlc_model.cpp : base for playlist and ml model
 ****************************************************************************
 * Copyright (C) 2010 the VideoLAN team and AUTHORS
 * $Id$
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

VLCModel::VLCModel( intf_thread_t *_p_intf, QObject *parent )
        : QAbstractItemModel( parent ), p_intf(_p_intf)
{
}

VLCModel::~VLCModel()
{
}

QString VLCModel::getMeta( const QModelIndex & index, int meta )
{
    return index.model()->index( index.row(), columnFromMeta( meta ), index.parent() ).
        data().toString();
}

QString VLCModel::getArtUrl( const QModelIndex & index )
{
    return index.model()->index( index.row(),
                    columnFromMeta( COLUMN_COVER ),
                    index.parent() )
           .data().toString();
}

QPixmap VLCModel::getArtPixmap( const QModelIndex & index, const QSize & size )
{
    QString artUrl = VLCModel::getArtUrl( index ) ;

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

int VLCModel::columnCount( const QModelIndex & ) const
{
    return columnFromMeta( COLUMN_END );
}

