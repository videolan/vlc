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
#include "input_models.hpp"

//***************************
//  track list model
//***************************

TrackListModel::TrackListModel(vlc_player_t *player, QObject *parent)
    : QAbstractListModel(parent)
    , m_player(player)
{
}

Qt::ItemFlags TrackListModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsUserCheckable;
}

int TrackListModel::rowCount(const QModelIndex &) const
{
    return m_data.size();
}

QVariant TrackListModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row >= m_data.size())
        return QVariant{};
    if (role == Qt::DisplayRole)
        return m_data[row].m_title;
    else if (role == Qt::CheckStateRole)
        return QVariant::fromValue<bool>(m_data[row].m_selected);
    return QVariant{};
}

bool TrackListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    int row = index.row();
    if (row >= m_data.size())
        return false;
    if ( role != Qt::CheckStateRole )
        return false;
    if (!value.canConvert<bool>())
        return false;
    bool select = value.toBool();
    vlc_player_locker lock{ m_player };

    if (select)
    {
        vlc_es_id_t *es_id = m_data[row].m_id.get();
        const enum es_format_category_e cat = vlc_es_id_GetCat(es_id);
        enum vlc_player_select_policy policy =
            cat == VIDEO_ES ? VLC_PLAYER_SELECT_SIMULTANEOUS
                            : VLC_PLAYER_SELECT_EXCLUSIVE;
        vlc_player_SelectEsId(m_player, es_id, policy);
    }
    else
        vlc_player_UnselectEsId(m_player, m_data[row].m_id.get());
    return true;
}

void TrackListModel::updateTracks(vlc_player_list_action action, const vlc_player_track *track_info)
{
    switch (action) {
    case VLC_PLAYER_LIST_ADDED:
    {
        beginInsertRows({}, m_data.size(), m_data.size());
        m_data.append(Data{ track_info });
        endInsertRows();
        emit countChanged();
        break;
    }
    case VLC_PLAYER_LIST_REMOVED:
    {
        auto it = std::find_if(m_data.begin(), m_data.end(), [&](const Data& t) {
            return t.m_id.get() == track_info->es_id;
        });
        if (it == m_data.end())
            return;

        int pos = std::distance(m_data.begin(), it);
        beginRemoveRows({}, pos, pos);
        m_data.erase(it);
        endRemoveRows();
        emit countChanged();
        break;
    }
    case VLC_PLAYER_LIST_UPDATED:
    {
        int pos = 0;
        bool found = false;
        for (Data& d : m_data)
        {
            if (d.m_id.get() == track_info->es_id)
            {
                d.update(track_info);
                found = true;
                break;
            }
            pos++;
        }
        if (!found)
            return;
        QModelIndex dataIndex = index(pos);
        emit dataChanged(dataIndex, dataIndex, { Qt::DisplayRole, Qt::CheckStateRole });
        break;
    }
    }
}

void TrackListModel::updateTrackSelection(vlc_es_id_t *trackid, bool selected)
{
    if (trackid == NULL)
        return;
    QList<Data>::iterator it = std::find_if(m_data.begin(), m_data.end(), [&](const Data& track) {
        return trackid == track.m_id.get();
    });
    if (it == m_data.end())
        return;
    size_t pos = std::distance(m_data.begin(), it);
    it->m_selected = selected;
    QModelIndex dataIndex = index(pos);
    emit dataChanged(dataIndex, dataIndex, {  Qt::CheckStateRole });
}

void TrackListModel::clear()
{
    beginRemoveRows({}, 0, m_data.size() - 1);
    m_data.clear();
    endRemoveRows();
}

QHash<int, QByteArray> TrackListModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = this->QAbstractListModel::roleNames();
    roleNames[Qt::CheckStateRole] = "checked";
    return roleNames;
}

TrackListModel::Data::Data(const vlc_player_track *track_info)
    : m_title( qfu(track_info->name) )
    , m_id( track_info->es_id, true )
    , m_selected( track_info->selected )
{
}

void TrackListModel::Data::update(const vlc_player_track *track_info)
{
    m_id.reset(track_info->es_id, true);
    m_title = qfu(track_info->name);
    m_selected = track_info->selected;
}


//***************************
//  TitleListModel
//***************************


TitleListModel::TitleListModel(vlc_player_t *player, QObject *parent)
    : QAbstractListModel(parent)
    , m_player(player)
{
}

Qt::ItemFlags TitleListModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsUserCheckable;
}

int TitleListModel::rowCount(const QModelIndex &) const
{
    return m_count;
}

QVariant TitleListModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row >= m_count)
        return QVariant{};
    const vlc_player_title* title = getTitleAt(row);

    if (role == Qt::DisplayRole)
        return qfu(title->name);
    else if (role == Qt::CheckStateRole)
        return QVariant::fromValue<bool>(row == m_current);
    return QVariant{};
}

bool TitleListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    int row = index.row();
    if (row < 0 || row >= m_count)
        return false;
    if ( role != Qt::CheckStateRole )
        return false;
    if (!value.canConvert<bool>())
        return false;
    bool select = value.toBool();
    if (select)
    {
        vlc_player_locker lock{ m_player };
        const vlc_player_title* title = getTitleAt(row);
        if (!title)
            return false;
        vlc_player_SelectTitle(m_player, title);
    }
    return true;
}

void TitleListModel::setCurrent(int current)
{
    if (m_count == 0 || m_current == current)
        return;
    int oldCurrent = m_current;
    m_current = current;

    QModelIndex oldIndex = index(oldCurrent);
    QModelIndex currentIndex = index(current);

    if (oldCurrent >= 0)
        emit dataChanged(oldIndex, oldIndex, { Qt::CheckStateRole });
    if ( current >= 0 )
        emit dataChanged(currentIndex, currentIndex, { Qt::CheckStateRole });
}

const vlc_player_title *TitleListModel::getTitleAt(size_t index) const
{
    return vlc_player_title_list_GetAt(m_titleList.get(), index);
}

void TitleListModel::resetTitles(vlc_player_title_list *newTitleList)
{
    beginResetModel();
    m_titleList.reset(newTitleList, true);
    m_current = -1;
    if (m_titleList)
        m_count = vlc_player_title_list_GetCount(m_titleList.get());
    else
        m_count = 0;
    endResetModel();
    emit countChanged();
}

QHash<int, QByteArray> TitleListModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = this->QAbstractListModel::roleNames();
    roleNames[Qt::CheckStateRole] = "checked";
    return roleNames;
}

//***************************
//  ChapterListModel
//***************************

ChapterListModel::ChapterListModel(vlc_player_t *player, QObject *parent)
    : QAbstractListModel(parent)
    , m_player(player)
{
}

Qt::ItemFlags ChapterListModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsUserCheckable;
}

int ChapterListModel::rowCount(const QModelIndex &) const
{
    return m_title ? m_title->chapter_count : 0;
}

QVariant ChapterListModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (m_title == nullptr || row < 0 || (size_t)row >= m_title->chapter_count)
        return QVariant{};
    const vlc_player_chapter& chapter = m_title->chapters[row];

    if (role == Qt::DisplayRole)
        return qfu(chapter.name);
    else if (role == Qt::CheckStateRole)
        return QVariant::fromValue<bool>(row == m_current);
    else if (role == ChapterListRoles::TimeRole )
        return QVariant::fromValue<vlc_tick_t>(chapter.time);
    else if (role == ChapterListRoles::PositionRole && (m_title->length != 0) )
        return QVariant::fromValue<float>(chapter.time /(float) m_title->length);
    return QVariant{};
}

bool ChapterListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    int row = index.row();
    if (m_title == nullptr || (size_t)row >= m_title->chapter_count)
        return false;
    if ( role != Qt::CheckStateRole )
        return false;
    if (!value.canConvert<bool>())
        return false;
    bool select = value.toBool();
    if (select)
    {
        vlc_player_locker lock{ m_player };
        vlc_player_SelectChapter(m_player, m_title, row);
    }
    return true;
}

QHash<int, QByteArray> ChapterListModel::roleNames() const
{
    return QHash<int, QByteArray>{
        {Qt::DisplayRole, "display"},
        {Qt::CheckStateRole, "checked"},
        {ChapterListRoles::TimeRole, "time"},
        {ChapterListRoles::PositionRole, "position"}
    };
}

void ChapterListModel::setCurrent(int current)
{
    if (m_title == nullptr || m_title->chapter_count == 0 || m_current == current)
        return;
    int oldCurrent = m_current;
    m_current = current;

    QModelIndex oldIndex = index(oldCurrent);
    QModelIndex currentIndex = index(current);

    if (oldCurrent >= 0)
        emit dataChanged(oldIndex, oldIndex, { Qt::CheckStateRole });
    if ( current >= 0 )
        emit dataChanged(currentIndex, currentIndex, { Qt::CheckStateRole });
}

void ChapterListModel::resetTitle(const vlc_player_title *newTitle)
{
    beginResetModel();
    m_title =newTitle;
    m_current = -1;
    endResetModel();
    emit countChanged();
}

QString ChapterListModel::getNameAtPosition(float pos) const
{
    if(m_title != nullptr){

        vlc_tick_t posTime = pos * m_title->length;
        int prevChapterIndex = 0;

        for(unsigned int i=0;i<m_title->chapter_count;i++){

            vlc_tick_t currentChapterTime = m_title->chapters[i].time;

            if(currentChapterTime > posTime)
                return qfu(m_title->chapters[prevChapterIndex].name);

            else if(i == (m_title->chapter_count - 1))
                return qfu(m_title->chapters[i].name);

            prevChapterIndex = i;
        }
    }
    return QString();
}

//***************************
//  ProgramListModel
//***************************


ProgramListModel::ProgramListModel(vlc_player_t *player, QObject *parent)
    : QAbstractListModel(parent)
    , m_player(player)
{
}

Qt::ItemFlags ProgramListModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsUserCheckable;
}

int ProgramListModel::rowCount(const QModelIndex &) const
{
    return m_data.size();
}

QVariant ProgramListModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row >= m_data.size())
        return QVariant{};
    if (role == Qt::DisplayRole)
        return m_data[row].m_title;
    else if (role == Qt::CheckStateRole)
        return QVariant::fromValue<bool>(m_data[row].m_selected);
    return QVariant{};
}

bool ProgramListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    int row = index.row();
    if (row >= m_data.size())
        return false;
    if ( role != Qt::CheckStateRole )
        return false;
    if (!value.canConvert<bool>())
        return false;
    bool select = value.toBool();
    vlc_player_locker lock{ m_player };

    if (select)
        vlc_player_SelectProgram(m_player, m_data[row].m_id);
    return true;
}

void ProgramListModel::updatePrograms(vlc_player_list_action action, const vlc_player_program *program)
{
    assert(program);
    switch (action) {
    case VLC_PLAYER_LIST_ADDED:
        beginInsertRows({}, m_data.size(), m_data.size());
        m_data.append(Data{ program });
        endInsertRows();
        emit countChanged();
        break;

    case VLC_PLAYER_LIST_REMOVED:
    {
        auto it = std::find_if(m_data.begin(), m_data.end(), [&](const Data& t) {
            return t.m_id == program->group_id;
        });
        if (it == m_data.end())
            return;
        int pos = std::distance(m_data.begin(), it);
        beginRemoveRows({}, pos, pos);
        m_data.erase(it);
        endRemoveRows();
        emit countChanged();
        break;
    }
    case VLC_PLAYER_LIST_UPDATED:
    {
        int pos = 0;
        bool found = false;
        for (Data& d : m_data)
        {
            if (d.m_id == program->group_id)
            {
                d.update(program);
                found = true;
                break;
            }
            pos++;
        }
        if (!found)
            return;
        QModelIndex dataIndex = index(pos);
        emit dataChanged(dataIndex, dataIndex, { Qt::DisplayRole, Qt::CheckStateRole });
        break;
    }
    }
}

void ProgramListModel::updateProgramSelection(int programid, bool selected)
{
    QList<Data>::iterator it = std::find_if(m_data.begin(), m_data.end(), [&](const Data& program) {
        return programid == program.m_id;
    });
    if (it == m_data.end())
        return;
    size_t pos = std::distance(m_data.begin(), it);
    it->m_selected = selected;
    QModelIndex dataIndex = index(pos);
    emit dataChanged(dataIndex, dataIndex, {  Qt::CheckStateRole });
}

void ProgramListModel::clear()
{
    beginRemoveRows({}, 0, m_data.size() - 1);
    m_data.clear();
    endRemoveRows();
}

QHash<int, QByteArray> ProgramListModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = this->QAbstractListModel::roleNames();
    roleNames[Qt::CheckStateRole] = "checked";
    return roleNames;
}

ProgramListModel::Data::Data(const vlc_player_program *program)
    : m_title(qfu(program->name))
    , m_id( program->group_id )
    , m_selected( program->selected )
    , m_scrambled( program->scrambled )
{
    assert(program);
}

void ProgramListModel::Data::update(const vlc_player_program *program)
{
    assert(program);
    m_title = qfu(program->name);
    m_id = program->group_id;
    m_selected = program->selected;
    m_scrambled = program->scrambled;
}

