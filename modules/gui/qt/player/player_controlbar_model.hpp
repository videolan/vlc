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
#include <array>

class ControlListModel;

class PlayerControlbarModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool dirty READ dirty WRITE setDirty NOTIFY dirtyChanged)

    Q_PROPERTY(ControlListModel* left READ left CONSTANT)
    Q_PROPERTY(ControlListModel* center READ center CONSTANT)
    Q_PROPERTY(ControlListModel* right READ right CONSTANT)

public:
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
