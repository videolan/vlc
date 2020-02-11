/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef ML_FOLDERS_MODEL_HPP
#define ML_FOLDERS_MODEL_HPP

#ifdef HAVE_CONFIG_H

# include "config.h"

#endif

#include "qt.hpp"
#include <QAbstractListModel>
#include <QUrl>
#include <QList>
#include "mlhelper.hpp"

#include <util/qml_main_context.hpp>
#include <vlc_media_library.h>

class MlFoldersModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QmlMainContext* ctx READ getCtx WRITE setCtx NOTIFY ctxChanged)

public:
    MlFoldersModel( QObject * parent = nullptr );

    void setCtx(QmlMainContext* ctx);
    inline QmlMainContext* getCtx() { return m_ctx; }
    void setMl(vlc_medialibrary_t* ml);

    int rowCount( QModelIndex const &parent = {} ) const  override;
    int columnCount (QModelIndex const &parent = {} ) const  override;

    QVariant data( QModelIndex const &index , const int role = Qt::DisplayRole ) const  override;

    Qt::ItemFlags flags ( const QModelIndex & index ) const override;

    QHash<int, QByteArray> roleNames() const override;

    bool setData( const QModelIndex &index , const QVariant &value ,
                 int role ) override;

    static void onMlEvent( void* data , const vlc_ml_event_t* event );
    QVariant headerData( int section , Qt::Orientation orientation , int role ) const override;

    enum Roles
    {
        Banned = Qt::UserRole + 1,
        DisplayUrl
    };
private:
    struct EntryPoint {
        EntryPoint(const vlc_ml_entry_point_t &entryPoint );
        QString  mrl;
        bool banned;
    };

    std::vector<EntryPoint> m_mrls;
    vlc_medialibrary_t *m_ml = nullptr;
    QmlMainContext* m_ctx = nullptr;

    using EventCallbackPtr = std::unique_ptr<vlc_ml_event_callback_t,
    std::function<void( vlc_ml_event_callback_t* )>> ;

    EventCallbackPtr m_ml_event_handle;
signals:
    void ctxChanged();
    void onMLEntryPointModified(QPrivateSignal);

public slots:
    void update();
    void removeAt( int index );
    void add( QUrl mrl );


};

#endif // ML_FOLDERS_MODEL_HPP
