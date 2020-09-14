/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
#ifndef QMLMENUWRAPPER_HPP
#define QMLMENUWRAPPER_HPP

#include "qt.hpp"

#include <QObject>
#include <QPoint>

#include "menus.hpp"

class MediaLib;
class MLAlbumModel;
class MLGenreModel;
class MLArtistModel;
class MLAlbumTrackModel;
class MLVideoModel;
class NetworkMediaModel;
class QmlMainContext;

#define SIMPLE_MENU_PROPERTY(type, name, defaultValue) \
    Q_PROPERTY(type name READ get##name WRITE set##name) \
    public: \
    inline void set##name( type data) { m_##name = data; } \
    inline type get##name() const { return m_##name; } \
    private: \
    type m_##name = defaultValue;

//inherit VLCMenuBar so we can access menu creation functions
class QmlGlobalMenu : public VLCMenuBar
{
    Q_OBJECT
    SIMPLE_MENU_PROPERTY(QmlMainContext*, ctx, nullptr)
public:
    explicit QmlGlobalMenu(QObject *parent = nullptr);

public slots:
    void popup( QPoint pos );
};

class BaseMedialibMenu : public QObject
{
    Q_OBJECT
public:
    BaseMedialibMenu(QObject* parent = nullptr);
signals:
    void showMediaInformation(int index);

protected:
    void medialibAudioContextMenu(MediaLib* ml, const QVariantList& mlId, const QPoint& pos, const QVariantMap& options);

    template<typename ModelType>
    void popup(ModelType* model, typename ModelType::Roles role,  const QModelIndexList &selected, const  QPoint& pos, const QVariantMap& options) {
        if (!model)
            return;

        MediaLib* ml= model->ml();
        if (!ml)
            return;

        QVariantList itemIdList;
        for (const QModelIndex& modelIndex : selected)
            itemIdList.push_back(model->data(modelIndex, role));
        medialibAudioContextMenu(ml, itemIdList, pos, options);
    }
};

class AlbumContextMenu : public BaseMedialibMenu {
    Q_OBJECT
    SIMPLE_MENU_PROPERTY(MLAlbumModel*, model, nullptr)
public:
    AlbumContextMenu(QObject* parent = nullptr);
public slots:
    void popup(const QModelIndexList& selected, QPoint pos, QVariantMap options = {});
};


class ArtistContextMenu : public BaseMedialibMenu {
    Q_OBJECT
    SIMPLE_MENU_PROPERTY(MLArtistModel*, model, nullptr)
public:
    ArtistContextMenu(QObject* parent = nullptr);
public slots:
    void popup(const QModelIndexList& selected, QPoint pos, QVariantMap options = {});
};


class GenreContextMenu : public BaseMedialibMenu {
    Q_OBJECT
    SIMPLE_MENU_PROPERTY(MLGenreModel*, model, nullptr)
public:
    GenreContextMenu(QObject* parent = nullptr);
public slots:
    void popup(const QModelIndexList& selected, QPoint pos, QVariantMap options = {});
};


class AlbumTrackContextMenu : public BaseMedialibMenu {
    Q_OBJECT
    SIMPLE_MENU_PROPERTY(MLAlbumTrackModel*, model, nullptr)
public:
    AlbumTrackContextMenu(QObject* parent = nullptr);
public slots:
    void popup(const QModelIndexList& selected, QPoint pos, QVariantMap options = {});
};


class VideoContextMenu : public QObject {
    Q_OBJECT
    SIMPLE_MENU_PROPERTY(MLVideoModel*, model, nullptr)
public:
    VideoContextMenu(QObject* parent = nullptr);
public slots:
    void popup(const QModelIndexList& selected, QPoint pos, QVariantMap options = {} );
signals:
    void showMediaInformation(int index);
};

class NetworkMediaContextMenu : public QObject {
    Q_OBJECT
    SIMPLE_MENU_PROPERTY(NetworkMediaModel*, model, nullptr)
public:
    NetworkMediaContextMenu(QObject* parent = nullptr);
public slots:
    void popup(const QModelIndexList& selected, QPoint pos );
};

#undef SIMPLE_MENU_PROPERTY

#endif // QMLMENUWRAPPER_HPP
