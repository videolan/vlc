/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef ADDONS_MODEL_HPP
#define ADDONS_MODEL_HPP

#include "util/base_model.hpp"
#include "maininterface/mainctx.hpp"

class AddonsModelPrivate;
class AddonsModel : public BaseModel
{
    Q_OBJECT

public:

    Q_PROPERTY(MainCtx* ctx READ getCtx WRITE setCtx NOTIFY ctxChanged FINAL)
    //filter list by to match addonTypeFilter, set to NONE to disable filtering
    Q_PROPERTY(Type typeFilter READ getTypeFilter WRITE setTypeFilter NOTIFY typeFilterChanged FINAL)
    Q_PROPERTY(State stateFilter READ getStateFilter WRITE setStateFilter NOTIFY stateFilterChanged FINAL)

    Q_PROPERTY(int maxScore READ getMaxScore CONSTANT FINAL)

    enum class State // equivalent to addon_state_t
    {
        STATE_NOTINSTALLED = 0,
        STATE_INSTALLING,
        STATE_INSTALLED,
        STATE_UNINSTALLING,
        STATE_NONE //not defined in addon_state_t
    };
    Q_ENUM(State)

    enum Role : int {
        NAME = Qt::UserRole + 1,
        AUTHOR,
        SUMMARY,
        DESCRIPTION,
        DOWNLOADS,
        SCORE,
        STATE,
        TYPE,
        ARTWORK,
        LINK,
        FILENAME,
        ADDON_VERSION, //VERSION conflicts with config.h define
        UUID,
        DOWNLOAD_COUNT,
        BROKEN,
        MANAGEABLE,
        UPDATABLE,
    };
    Q_ENUM(Role)

    enum class Type // equivalent as addon_type_t
    {
        TYPE_UNKNOWN = 0,
        TYPE_EXTENSION,
        TYPE_PLAYLIST_PARSER,
        TYPE_SERVICE_DISCOVERY,
        TYPE_SKIN2,
        TYPE_PLUGIN,
        TYPE_INTERFACE,
        TYPE_META,
        TYPE_OTHER,
        TYPE_NONE //not defined in addon_type_t
    };
    Q_ENUM(Type)


    explicit AddonsModel(QObject* parent = nullptr);

public: //QAbstractListModel override
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant &value, int role) override;
    Qt::ItemFlags flags( const QModelIndex &index ) const override;
    QHash<int, QByteArray> roleNames() const override;

public: //invokable functions
    Q_INVOKABLE void installService(int idx);
    Q_INVOKABLE void removeService(int idx);

    Q_INVOKABLE void loadFromDefaultRepository();
    Q_INVOKABLE void loadFromExternalRepository(QUrl uri);

    Q_INVOKABLE static QString getLabelForType(Type type);
    Q_INVOKABLE static QColor getColorForType(Type type);
    Q_INVOKABLE static QString getIconForType(Type type);

    static int getMaxScore();

public: // properties
    void setCtx(MainCtx* ctx);
    MainCtx* getCtx() const;

    State getStateFilter() const;
    void setStateFilter(State state);

    Type getTypeFilter() const;
    void setTypeFilter(Type type);

signals:
    void ctxChanged();
    void stateFilterChanged();
    void typeFilterChanged();

private:
    Q_DECLARE_PRIVATE(AddonsModel);
};

#endif // ADDONS_MODEL_HPP
