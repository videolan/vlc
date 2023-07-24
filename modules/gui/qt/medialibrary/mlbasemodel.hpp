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

#ifndef MLBASEMODEL_HPP
#define MLBASEMODEL_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <vlc_common.h>


#include <memory>
#include <QAbstractListModel>
#include <QQmlParserStatus>

#include <vlc_media_library.h>
#include "mlqmltypes.hpp"
#include "medialib.hpp"
#include <memory>
#include "mlevent.hpp"
#include "mlqueryparams.hpp"
#include "util/listcacheloader.hpp"

// Fordward declarations
class MLListCache;
class MediaLib;

class MLBaseModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(MLItemId parentId READ parentId WRITE setParentId NOTIFY parentIdChanged
               RESET unsetParentId FINAL)

    Q_PROPERTY(MediaLib * ml READ ml WRITE setMl NOTIFY mlChanged FINAL)

    Q_PROPERTY(QString searchPattern READ searchPattern WRITE setSearchPattern FINAL)

    Q_PROPERTY(Qt::SortOrder sortOrder READ getSortOrder WRITE setSortOrder NOTIFY sortOrderChanged FINAL)

    Q_PROPERTY(QString sortCriteria READ getSortCriteria WRITE setSortCriteria
               NOTIFY sortCriteriaChanged RESET unsetSortCriteria FINAL)

    Q_PROPERTY(unsigned int count READ getCount NOTIFY countChanged FINAL)

    /**
     * @brief loading
     * @return true till no data is available
     */
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged FINAL)

public:
    explicit MLBaseModel(QObject *parent = nullptr);

    virtual ~MLBaseModel();


    virtual QVariant itemRoleData(MLItem *item, int role) const = 0;


    Q_INVOKABLE void sortByColumn(QByteArray name, Qt::SortOrder order);

    Q_INVOKABLE QMap<QString, QVariant> getDataAt(const QModelIndex & index);
    Q_INVOKABLE QMap<QString, QVariant> getDataAt(int idx);

    Q_INVOKABLE void getData(const QModelIndexList &indexes, QJSValue callback);

    QVariant data(const QModelIndex &index, int role) const override final;
    int rowCount(const QModelIndex &parent = {}) const override;

public:
    // properties functions

    MLItemId parentId() const;
    void setParentId(MLItemId parentId);
    void unsetParentId();

    MediaLib* ml() const;
    void setMl(MediaLib* ml);

    const QString& searchPattern() const;
    void setSearchPattern( const QString& pattern );

    Qt::SortOrder getSortOrder() const;
    void setSortOrder(Qt::SortOrder order);
    const QString getSortCriteria() const;
    void setSortCriteria(const QString& criteria);
    void unsetSortCriteria();

    virtual unsigned int getCount() const;

    bool loading() const;


    Q_INVOKABLE void addAndPlay(const QModelIndexList &list, const QStringList &options = {});

signals:
    void parentIdChanged();
    void mlChanged();
    void resetRequested();
    void sortOrderChanged();
    void sortCriteriaChanged();
    void countChanged(unsigned int) const;
    void loadingChanged() const;


protected slots:
    void onResetRequested();
    void onLocalSizeChanged(size_t size);


protected:
    void classBegin() override;
    void componentComplete() override;

    virtual vlc_ml_sorting_criteria_t roleToCriteria(int role) const = 0;
    static QString getFirstSymbol(QString str);
    virtual vlc_ml_sorting_criteria_t nameToCriteria(QByteArray) const {
        return VLC_ML_SORTING_DEFAULT;
    }
    virtual QByteArray criteriaToName(vlc_ml_sorting_criteria_t ) const
    {
        return "";
    }

    void validateCache() const;
    void resetCache();
    void invalidateCache();

    MLItem *item(int signedidx) const;

    // NOTE: This is faster because it only returns items available in cache.
    MLItem *itemCache(int signedidx) const;

    MLItem *findInCache(const MLItemId& id, int *index) const;

    //update and notify changes on an item if this item is in the cache
    void updateItemInCache(const MLItemId& id);

    //delete and notify deletion of an item if this item is in the cache
    //this is only to reflect changes from the ML, it won't alter the database
    void deleteItemInCache(const MLItemId& mlid);

    void moveRangeInCache(int first, int last, int to);
    void deleteRangeInCache(int first, int last);

    virtual void onVlcMlEvent( const MLEvent &event );


    virtual void thumbnailUpdated(const QModelIndex& , MLItem* , const QString& , vlc_ml_thumbnail_status_t )  {}

    /* Data loader for the cache */
    struct BaseLoader : public ListCacheLoader<std::unique_ptr<MLItem>>
    {
        BaseLoader(MLItemId parent, QString searchPattern,
                   vlc_ml_sorting_criteria_t sort, bool sort_desc);
        BaseLoader(const MLBaseModel &model);

        MLQueryParams getParams(size_t index = 0, size_t count = 0) const;

        virtual std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const = 0;

    protected:
        MLItemId m_parent;
        QString m_searchPattern;
        vlc_ml_sorting_criteria_t m_sort;
        bool m_sort_desc;
    };

    virtual std::unique_ptr<BaseLoader> createLoader() const = 0;

private:
    static void onVlcMlEvent( void* data, const vlc_ml_event_t* event );

    void onCacheDataChanged(int first, int last);
    void onCacheBeginInsertRows(int first, int last);
    void onCacheBeginRemoveRows(int first, int last);
    void onCacheBeginMoveRows(int first, int last, int destination);

    using ItemCallback = std::function<void (quint64 requestID
                                            , std::vector<std::unique_ptr<MLItem>> &items)>;

    quint64 loadItems(const QVector<int> &index, ItemCallback cb);

    inline bool cachable() const { return m_mediaLib && !m_qmlInitializing; }

protected:
    MLItemId m_parent;

    MediaLib* m_mediaLib = nullptr;
    QString m_search_pattern;
    vlc_ml_sorting_criteria_t m_sort = VLC_ML_SORTING_DEFAULT;
    bool m_sort_desc = false;

    std::unique_ptr<vlc_ml_event_callback_t,
                    std::function<void(vlc_ml_event_callback_t*)>> m_ml_event_handle;
    bool m_need_reset = false;

    mutable std::unique_ptr<MLListCache> m_cache;

    //loader used to load single items
    std::shared_ptr<BaseLoader> m_itemLoader;

    bool m_qmlInitializing = false;
};

#endif // MLBASEMODEL_HPP
