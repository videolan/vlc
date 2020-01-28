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

#include "audio_device_model.hpp"

extern "C" {

static void on_player_aout_device_changed(audio_output_t *,const char *device, void *data)
{
    AudioDeviceModel* that = static_cast<AudioDeviceModel*>(data);
    QMetaObject::invokeMethod(that, [that, device=QString::fromUtf8(device)](){
        that->updateCurrent(device);
    }, Qt::QueuedConnection, nullptr);
}

}

static const struct vlc_player_aout_cbs player_aout_cbs = {
    nullptr,
    nullptr,
    on_player_aout_device_changed
};

AudioDeviceModel::AudioDeviceModel(vlc_player_t *player, QObject *parent)
    : QAbstractListModel(parent)
    , m_player(player)
{
    {
        vlc_player_locker locker{m_player};
        m_player_aout_listener = vlc_player_aout_AddListener(m_player, &player_aout_cbs, this);
    }

    m_aout = vlc_player_aout_Hold(m_player);
    if (m_aout)
        m_inputs = aout_DevicesList(m_aout, &m_ids, &m_names);
}

AudioDeviceModel::~AudioDeviceModel()
{
    for (int i=0; i<m_inputs; i++)
    {
        free(m_ids[i]);
        free(m_names[i]);
    }

    free(m_ids);
    free(m_names);

    vlc_player_locker locker{m_player};
    vlc_player_aout_RemoveListener(m_player, m_player_aout_listener);

    if (m_aout)
        aout_Release(m_aout);
}

Qt::ItemFlags AudioDeviceModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsUserCheckable;
}

int AudioDeviceModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_inputs > 0 ? m_inputs : 0;
}

QVariant AudioDeviceModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (!index.isValid())
        return QVariant();
    if (row < 0 || row >= m_inputs)
        return QVariant();

    const char *name = m_names[row];
    if (role == Qt::DisplayRole)
        return qfu(name);
    else if (role == Qt::CheckStateRole)
        return QVariant::fromValue<bool>((!m_current.isEmpty() && (m_ids[row] == m_current)) ||
            (m_current.isEmpty() && m_ids[row] && m_ids[row][0] == '\0' )) ;

    return QVariant();
}

bool AudioDeviceModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    int row = index.row();
    if (!index.isValid())
        return false;
    if (row < 0 || row >= m_inputs)
        return false;
    if (role != Qt::CheckStateRole)
        return false;
    if (!value.canConvert<bool>())
        return false;
    bool select = value.toBool();
    if (select)
    {
        aout_DeviceSet(m_aout, m_ids[row]);
    }
    return true;
}

void AudioDeviceModel::updateCurrent(QString current)
{
    if (current == m_current)
        return;
    std::swap(current, m_current);

    int oldIndex = -1;
    int currentIndex = -1;

    for(int i=0; i<m_inputs; i++)
    {
        if(m_ids[i] == m_current){
            currentIndex = i;
        }
        if(m_ids[i] == current){
            oldIndex = i;
        }
    }

    if(oldIndex >= 0)
        emit dataChanged(index(oldIndex), index(oldIndex), { Qt::DisplayRole, Qt::CheckStateRole });
    if(currentIndex >= 0)
        emit dataChanged(index(currentIndex), index(currentIndex), { Qt::DisplayRole, Qt::CheckStateRole });
}

QHash<int, QByteArray> AudioDeviceModel::roleNames() const
{
    return QHash<int, QByteArray>{
        {Qt::DisplayRole, "display"},
        {Qt::CheckStateRole, "checked"}
    };
}
