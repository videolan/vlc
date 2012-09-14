/*****************************************************************************
 * ml_configuration.hpp: ML's configuration dialog (folder view)
 *****************************************************************************
 * Copyright (C) 2008-2010 the VideoLAN Team and AUTHORS
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *          Rémi Duraffort <ivoire@videolan.org>
 *          Adrien Maglo <magsoft@videolan.org>
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


#ifndef _MEDIA_LIBRARY_CONFIG_H
#define _MEDIA_LIBRARY_CONFIG_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef MEDIA_LIBRARY

#include <vlc_common.h>
#include <vlc_media_library.h>

#include "util/qvlcframe.hpp"

#include <QDirModel>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>

/** Classes in this header */
class MLDirModel;
class MLConfDialog;

/** *************************************************************************
 * \brief Model representing the filesystem and whose items are checkable
 ****************************************************************************/
class MLDirModel : public QDirModel
{
    Q_OBJECT;

public:
    MLDirModel( const QStringList &nameFilters, QDir::Filters filters,
                QDir::SortFlags sort, QObject *parent = 0 )
                : QDirModel( nameFilters, filters, sort, parent ) {};

    virtual Qt::ItemFlags flags( const QModelIndex &index ) const;
    virtual QVariant data( const QModelIndex &index, int role ) const;
    virtual bool setData( const QModelIndex &index, const QVariant &value,
                          int role = Qt::EditRole );
    int columnCount( const QModelIndex &parent = QModelIndex() ) const;
    void reset( bool, vlc_array_t *);

    QStringList monitoredDirs;

private:
    bool b_recursive;
    QMap<QString, Qt::CheckState> itemCheckState;
    intf_thread_t *p_intf;
    media_library_t* p_ml;

public slots:
    void setRecursivity( bool );
};

/** *************************************************************************
 * \brief Configuration dialog for the media library
 ****************************************************************************/
class MLConfDialog : public QVLCDialog
{
    Q_OBJECT;
public:
    MLConfDialog( QWidget *, intf_thread_t * );

private:
    void init();

    vlc_array_t *p_monitored_dirs;
    media_library_t *p_ml;
    intf_thread_t *p_intf;

    MLDirModel *model;
    QCheckBox *recursivity;
    QCheckBox *synchronous;

    static MLConfDialog *instance;

private slots:
    void save();
    void cancel();
    void reset();
    void close() { save(); };
};

#endif
#endif

