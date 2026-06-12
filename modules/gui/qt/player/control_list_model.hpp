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
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    explicit ControlListModel(QObject *parent = nullptr);

    enum Roles {
        ID_ROLE = Qt::UserRole
    };

    //changin the enum order or inserting new ID before the end will break user configuration
    //RESERVED_0x__ are available for future use, they can be repurposed
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
        FRAME_NEXT_BUTTON,
        SKIP_BACK_BUTTON,
        SKIP_FW_BUTTON,
        QUIT_BUTTON,
        RANDOM_BUTTON,
        LOOP_BUTTON,
        INFO_BUTTON,
        LANG_BUTTON,
        MENU_BUTTON, // deprecated
        BACK_BUTTON, // deprecated
        CHAPTER_PREVIOUS_BUTTON,
        CHAPTER_NEXT_BUTTON,
        BUTTON_MAX,
        PLAYER_SWITCH_BUTTON,
        ARTWORK_INFO,
        PLAYBACK_SPEED_BUTTON,
        HIGH_RESOLUTION_TIME_WIDGET,
        FRAME_PREV_BUTTON,
        RESERVED_0X1F,

        SPLITTER = 0x20,
        VOLUME,
        TELETEXT_BUTTONS,
        ASPECT_RATIO_COMBOBOX,
        DVD_MENUS_BUTTON,
        REVERSE_BUTTON,
        BOOKMARK_BUTTON,
        RENDERER_BUTTON,
        NAVIGATION_BUTTONS,
        PROGRAM_BUTTON,
        NAVIGATION_BOX,
        RESERVED_0X2B,
        RESERVED_0X2C,
        RESERVED_0X2D,
        RESERVED_0X2E,
        RESERVED_0X2F,
        RESERVED_0X30,
        RESERVED_0X31,
        RESERVED_0X32,
        RESERVED_0X33,
        RESERVED_0X34,
        RESERVED_0X35,
        RESERVED_0X36,
        RESERVED_0X37,
        RESERVED_0X38,
        RESERVED_0X39,
        RESERVED_0X3A,
        RESERVED_0X3B,
        RESERVED_0X3C,
        RESERVED_0X3D,
        RESERVED_0X3E,
        RESERVED_0X3F,

        WIDGET_SPACER = 0x40,
        WIDGET_SPACER_EXTEND,
        WIDGET_MAX,
    };
    Q_ENUM(ControlType)

    static_assert(RESERVED_0X1F == 0x1F, "missaligned");
    static_assert(RESERVED_0X2B == 0x2B, "missaligned");
    static_assert(RESERVED_0X3F == 0x3F, "missaligned");

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // Editable:
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex& index) const override;

    QHash<int, QByteArray> roleNames() const override;

    QVector<int> getControls() const;
    void setControls(const QVector<int>& list);

    bool operator==(const ControlListModel& model) const
    {
        return m_controls == model.m_controls;
    }

    bool operator!=(const ControlListModel& model) const
    {
        return !(operator==(model));
    }

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
