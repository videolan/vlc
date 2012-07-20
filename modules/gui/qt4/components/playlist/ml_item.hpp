/*****************************************************************************
 * ml_item.hpp: the media library's result item
 *****************************************************************************
 * Copyright (C) 2008-2011 the VideoLAN Team and AUTHORS
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *          Rémi Duraffort <ivoire@videolan.org>
 *          Adrien Maglo <magsoft@videolan.org>
 *          Srikanth Raju <srikiraju#gmail#com>
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

#ifndef _MEDIA_LIBRARY_MLITEM_H
#define _MEDIA_LIBRARY_MLITEM_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef MEDIA_LIBRARY

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_media_library.h>

#include "playlist_item.hpp"
#include "ml_model.hpp"
#include "qt4.hpp"

class MLModel;

class MLItem : public AbstractPLItem
{
    friend class MLModel;

public:
    MLItem( const MLModel *p_model, intf_thread_t *_p_intf,
            ml_media_t *p_media, MLItem *p_parent );
    virtual ~MLItem();
    bool operator<( MLItem* item );

private:
    /* AbstractPLItem */
    int id() const;
    input_item_t *inputItem();
    AbstractPLItem* child( int row ) const;

    /* Local */
    void delChild( int row );
    QVariant data( int column ) const;
    bool setData( ml_select_e meta, const QVariant &data );
    // Media structure connections
    ml_media_t* getMedia() const;
    QUrl getUri() const;

    ml_media_t* media;
    intf_thread_t* p_intf;
    const MLModel *model;
    media_library_t* p_ml;
};

#endif
#endif
