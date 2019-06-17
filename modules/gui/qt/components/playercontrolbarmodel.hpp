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

#ifndef CONTROLLERMODEL_H
#define CONTROLLERMODEL_H

#include <QAbstractListModel>
#include <QVector>

#include "qml_main_context.hpp"

class PlayerControlBarModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QmlMainContext* mainCtx READ getMainCtx WRITE setMainCtx NOTIFY ctxChanged)

public:
    explicit PlayerControlBarModel(QObject *_parent = nullptr);
    struct IconToolButton
    {
        int id;
        int size;
    };
    enum{
        ID_ROLE,
        SIZE_ROLE
    };
    enum ButtonType_e
    {

        PLAY_BUTTON,            //0
        STOP_BUTTON,            //1
        OPEN_BUTTON,            //2
        PREV_SLOW_BUTTON,       //3
        NEXT_FAST_BUTTON,       //4
        SLOWER_BUTTON,          //5
        FASTER_BUTTON,          //6
        FULLSCREEN_BUTTON,      //7
        DEFULLSCREEN_BUTTON,    //8
        EXTENDED_BUTTON,        //9
        PLAYLIST_BUTTON,        //10
        SNAPSHOT_BUTTON,        //11
        RECORD_BUTTON,          //12
        ATOB_BUTTON,            //13
        FRAME_BUTTON,           //14
        REVERSE_BUTTON,         //15
        SKIP_BACK_BUTTON,       //16
        SKIP_FW_BUTTON,         //17
        QUIT_BUTTON,            //18
        RANDOM_BUTTON,          //19
        LOOP_BUTTON,            //20
        INFO_BUTTON,            //21
        PREVIOUS_BUTTON,        //22
        NEXT_BUTTON,            //23
        OPEN_SUB_BUTTON,        //24
        FULLWIDTH_BUTTON,       //25
        BUTTON_MAX,             //26

        SPLITTER = 0x20,        //32
        INPUT_SLIDER,           //33
        TIME_LABEL,             //34
        VOLUME,                 //35
        VOLUME_SPECIAL,         //36
        MENU_BUTTONS,           //37
        TELETEXT_BUTTONS,       //38
        ADVANCED_CONTROLLER,    //39
        PLAYBACK_BUTTONS,       //40
        ASPECT_RATIO_COMBOBOX,  //41
        SPEED_LABEL,            //42
        TIME_LABEL_ELAPSED,     //43
        TIME_LABEL_REMAINING,   //44
        SPECIAL_MAX,            //45

        WIDGET_SPACER = 0x40,   //64
        WIDGET_SPACER_EXTEND,   //65
        WIDGET_MAX,             //66
        GOBACK_BUTTON,          //67
        LANG_BUTTON             //68
    };
    Q_ENUM(ButtonType_e)

    enum ButtonSize
    {
        WIDGET_NORMAL = 0x0,
        WIDGET_BIG    = 0x2,
    };
    Q_ENUM(ButtonSize)
    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // Editable:
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex& index) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

    inline QmlMainContext* getMainCtx() const { return m_mainCtx; }
    void setMainCtx(QmlMainContext*);

signals:
    void ctxChanged(QmlMainContext*);

protected:
    intf_thread_t       *p_intf;

private:
    QVector<IconToolButton> mButtons;
    QVector<IconToolButton> buttons() const;

    void parseAndAdd(QString& config);
    void loadConfig();
    bool setButtonAt(int index, const IconToolButton &button);

    QmlMainContext* m_mainCtx;
};

#endif // CONTROLLERMODEL_H
