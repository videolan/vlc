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

#ifndef CONTROLLISTMODEL_HPP
#define CONTROLLISTMODEL_HPP

#include <QAbstractListModel>
#include <QVector>

class ControlListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    explicit ControlListModel(QObject *parent = nullptr);

    enum Roles {
        ID_ROLE = Qt::UserRole
    };

    enum ControlType
    {
        PLAY_BUTTON,
        STOP_BUTTON,
        OPEN_BUTTON,
        PREVIOUS_BUTTON,
        NEXT_BUTTON,
        SLOWER_BUTTON,
        FASTER_BUTTON,
        FULLSCREEN_BUTTON,
        EXTENDED_BUTTON,
        PLAYLIST_BUTTON,
        SNAPSHOT_BUTTON,
        RECORD_BUTTON,
        ATOB_BUTTON,
        FRAME_BUTTON,
        SKIP_BACK_BUTTON,
        SKIP_FW_BUTTON,
        QUIT_BUTTON,
        RANDOM_BUTTON,
        LOOP_BUTTON,
        INFO_BUTTON,
        LANG_BUTTON,
        MENU_BUTTON,
        BACK_BUTTON,
        CHAPTER_PREVIOUS_BUTTON,
        CHAPTER_NEXT_BUTTON,
        BUTTON_MAX,
        PLAYER_SWITCH_BUTTON,
        ARTWORK_INFO,
        PLAYBACK_SPEED_BUTTON,

        SPLITTER = 0x20,
        VOLUME,
        TELETEXT_BUTTONS,
        ASPECT_RATIO_COMBOBOX,
        SPECIAL_MAX,

        WIDGET_SPACER = 0x40,
        WIDGET_SPACER_EXTEND,
        WIDGET_MAX
    };
    Q_ENUM(ControlType)

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // Editable:
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex& index) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

    QVector<int> getControls() const;
    void setControls(const QVector<int>& list);

signals:
    void countChanged();

private:
    QVector<ControlType> m_controls;
    bool setButtonAt(int index, const ControlType &button);

public slots:
    Q_INVOKABLE void insert(int index, QVariantMap bdata);
    Q_INVOKABLE void move(int src,int dest);
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void clear();
};

#endif // CONTROLLISTMODEL_HPP
