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

#ifndef AUDIO_DEVICE_MODEL_HPP
#define AUDIO_DEVICE_MODEL_HPP

#include <QAbstractListModel>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include <QObject>
#include <QStringList>
#include <vlc_aout.h>

class AudioDeviceModel : public QAbstractListModel
{
    Q_OBJECT

public:
    AudioDeviceModel(vlc_player_t *player, QObject *parent = nullptr);

    ~AudioDeviceModel();

    virtual Qt::ItemFlags flags(const QModelIndex &) const  override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    void updateCurrent(QString current);

    QHash<int, QByteArray> roleNames() const override;

private:
    int m_inputs = 0;
    char **m_names = nullptr;
    char **m_ids = nullptr;
    QString m_current;
    vlc_player_aout_listener_id* m_player_aout_listener = nullptr;
    audio_output_t* m_aout = nullptr;
    vlc_player_t *m_player;
};

#endif // AUDIO_DEVICE_MODEL_HPP
