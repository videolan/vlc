/*****************************************************************************
 * roundimage.hpp: Custom widgets
 ****************************************************************************
 * Copyright (C) 2021 the VideoLAN team
 *
 * Authors: Prince Gupta <guptaprince8832@gmail.com>
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

#ifndef VLC_QT_MLFOLDERSEDITOR_HPP
#define VLC_QT_MLFOLDERSEDITOR_HPP

#include "qt.hpp"

#include <QTableWidget>

#include <memory>

#include <medialibrary/mlfoldersmodel.hpp>

class MLFoldersEditor : public QTableWidget
{
    Q_OBJECT

public:
    MLFoldersEditor( QWidget *parent = nullptr );

    void setMLFoldersModel( MLFoldersBaseModel *foldersModel );
    void add( const QUrl &mrl );

    // call 'commit' to apply changes
    void commit();

private slots:
    void handleOpFailure( int operation, const QUrl &url );
    void resetFolders();

private:
    void newRow(const QUrl &mrl);
    void removeMrlEntry(const QUrl &mrl);

    MLFoldersBaseModel *m_foldersModel = nullptr;

    // new entries to add/remove on 'commit' call
    QVector<QUrl> m_newEntries;
    QVector<QUrl> m_removeEntries;
};

#endif

