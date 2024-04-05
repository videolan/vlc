/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#ifndef PLAYERCONTROLBARMODEL_HPP
#define PLAYERCONTROLBARMODEL_HPP

#include <QObject>
#include <QJSValue>
#include <QMap>

#include <array>

Q_MOC_INCLUDE( "player/control_list_model.hpp" )

class ControlListModel;

class PlayerControlbarModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool dirty READ dirty WRITE setDirty NOTIFY dirtyChanged FINAL)

    Q_PROPERTY(ControlListModel* left READ left CONSTANT FINAL)
    Q_PROPERTY(ControlListModel* center READ center CONSTANT FINAL)
    Q_PROPERTY(ControlListModel* right READ right CONSTANT FINAL)

public:
    // When there is a need to add a new Player, just
    // add its identifier in this enum and set QML buttons layout
    // identifier to it. Such as `property int identifier =
    // PlayerControlbarModel.Videoplayer`.
    // To make it translatable, add a corresponding entry to
    // the static member playerIdentifierDictionary which is
    // initialized in the source file.
    enum PlayerIdentifier {
        Videoplayer = 0,
        Audioplayer,
        Miniplayer
    };
    Q_ENUM(PlayerIdentifier)
    // This enum is iterated through QMetaEnum, and
    // a model out of this enum is generated
    // and used in the configuration editor.
    // Thanks to MOC, adding an entry to this enum
    // is enough for the editor to consider the
    // added entry without any other modification.
    // (except for the translation)

    static const QMap<PlayerIdentifier, const char*> playerIdentifierDictionary;

    static QJSValue getPlaylistIdentifierListModel(class QQmlEngine *engine,
                                                   class QJSEngine *scriptEngine);

    explicit PlayerControlbarModel(QObject *parent = nullptr);
    ~PlayerControlbarModel();

    bool dirty() const;

    std::array<QVector<int>, 3> serializeModels() const;
    void loadModels(const std::array<QVector<int>, 3>& array);

    ControlListModel* left() const;
    ControlListModel* center() const;
    ControlListModel* right() const;

public slots:
    void setDirty(bool dirty);

signals:
    void dirtyChanged(bool dirty);
    void controlListChanged();

private:
    bool m_dirty = false;

    ControlListModel* m_left = nullptr;
    ControlListModel* m_center = nullptr;
    ControlListModel* m_right = nullptr;

private slots:
    void contentChanged();
};

#endif
