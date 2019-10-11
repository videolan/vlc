/*****************************************************************************
 * input_models.hpp
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef INPUT_MODELS_HPP
#define INPUT_MODELS_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include <memory>
#include <vlc_player.h>
#include <vlc_cxx_helpers.hpp>
#include <QAbstractListModel>
#include <QList>

/**
 * @brief The TrackListModel class represent the
 * (audio/video/spu/...)tracks of an input
 *
 * the model expose the track title using the Qt::DisplayRole
 * and the current track using Qt::CheckStateRole
 * we can select a track by setting the Qt::CheckStateRole to true
 *
 * this class expects to be updated by the input manager
 */
class TrackListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ getCount NOTIFY countChanged)


public:
    TrackListModel(vlc_player_t* player, QObject* parent = nullptr);

    virtual Qt::ItemFlags flags(const QModelIndex &) const  override;

    virtual int rowCount(const QModelIndex & = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    void updateTracks(enum vlc_player_list_action action, const vlc_player_track *track_info);

    void updateTrackSelection(vlc_es_id_t *trackid, bool selected);

    void clear();

    QHash<int, QByteArray> roleNames() const override;

    inline int getCount() const { return m_data.size(); }

signals:
    void countChanged();

private:
    vlc_player_t* m_player;
    class Data {
    public:
        Data(const vlc_player_track *track_info);

        Data(const Data& data) = default;
        Data& operator =(const Data& data) = default;

        void update( const vlc_player_track *track_info );

        QString m_title;
        //vlc_es_id_t *m_id = NULL;
        vlc_shared_data_ptr_type(vlc_es_id_t, vlc_es_id_Hold, vlc_es_id_Release) m_id;
        bool m_selected = false;
    };
    QList<Data> m_data;
};

/**
 * @brief The TitleListModel class represent the
 * titles of an input
 *
 * the model expose the title's name using the Qt::DisplayRole
 * and the current title using Qt::CheckStateRole
 * we can select a title by setting the Qt::CheckStateRole to true
 *
 * this class expects to be updated by the input manager
 */
class TitleListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ getCount NOTIFY countChanged)

public:

    TitleListModel(vlc_player_t* player, QObject* parent = nullptr);

    virtual Qt::ItemFlags flags(const QModelIndex &) const  override;

    virtual int rowCount(const QModelIndex & = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    void setCurrent(int current);

    const vlc_player_title* getTitleAt( size_t index ) const;

    void resetTitles(vlc_player_title_list* newTitleList);

    QHash<int, QByteArray> roleNames() const override;

    inline int getCount() const { return m_count; }

signals:
    void countChanged();

private:
    vlc_player_t* m_player;
    typedef vlc_shared_data_ptr_type(vlc_player_title_list, vlc_player_title_list_Hold, vlc_player_title_list_Release) PlayerTitleList;
    PlayerTitleList m_titleList;
    int m_current = -1;
    int m_count = 0;
};

/**
 * @brief The ChapterListModel class represent the
 * chapters of an input
 *
 * the model expose the chapter's name using the Qt::DisplayRole
 * and the current chapter using Qt::CheckStateRole
 * we can select a chapter by setting the Qt::CheckStateRole to true
 *
 * this class expects to be updated by the input manager
 * the parent vlc_player_title isn't hold by this class, it is expected that the property is be valid or null
 */
class ChapterListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ getCount NOTIFY countChanged)

public:
    //user role
    enum ChapterListRoles {
        TimeRole = Qt::UserRole + 1 ,
        PositionRole
    };
public:
    ChapterListModel(vlc_player_t* player, QObject* parent = nullptr);

    virtual Qt::ItemFlags flags(const QModelIndex &) const  override;

    virtual int rowCount(const QModelIndex & = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    QHash<int, QByteArray> roleNames() const override;

    void setCurrent(int current);

    void resetTitle(const vlc_player_title* newTitle);

    inline int getCount() const { return (m_title == nullptr) ?  0 : m_title->chapter_count; }

signals:
    void countChanged();

public slots:
    QString getNameAtPosition(float pos) const;

private:
    vlc_player_t* m_player = nullptr;
    const vlc_player_title* m_title = nullptr;
    int m_current = -1;
};

/**
 * @brief The ProgramListModel class represent the
 * titles of an input
 *
 * the model expose the program's name using the Qt::DisplayRole
 * and the current program using Qt::CheckStateRole
 * we can select a program by setting the Qt::CheckStateRole to true
 *
 * this class expects to be updated by the input manager
 */
class ProgramListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ getCount NOTIFY countChanged)

public:
    ProgramListModel(vlc_player_t* player, QObject* parent = nullptr);

    Qt::ItemFlags flags(const QModelIndex &) const  override;

    int rowCount(const QModelIndex & = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    void updatePrograms(enum vlc_player_list_action action, const struct vlc_player_program* program);

    void updateProgramSelection(int programid, bool selected);

    void clear();

    QHash<int, QByteArray> roleNames() const override;

    inline int getCount() const { return m_data.size(); }

signals:
    void countChanged();

private:
    vlc_player_t* m_player;
    class Data {
    public:
        Data( const struct vlc_player_program* program );

        Data(const Data& data) = default;
        Data& operator =(const Data& data) = default;

        void update( const struct vlc_player_program* program );

        QString m_title;
        int m_id;
        bool m_selected;
        bool m_scrambled;
    };
    QList<Data> m_data;
};

#endif // INPUT_MODELS_HPP
