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

#ifndef MLServicesDiscoveryModel_HPP
#define MLServicesDiscoveryModel_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QAbstractListModel>

#include <vlc_media_library.h>
#include <vlc_media_source.h>
#include <vlc_threads.h>
#include <vlc_addons.h>
#include <vlc_cxx_helpers.hpp>

#include <maininterface/mainctx.hpp>

#include <memory>

class ServicesDiscoveryModel : public QAbstractListModel
{
    Q_OBJECT

public:

    Q_PROPERTY(MainCtx* ctx READ getCtx WRITE setCtx NOTIFY ctxChanged FINAL)
    Q_PROPERTY(bool parsingPending READ getParsingPending NOTIFY parsingPendingChanged FINAL)
    Q_PROPERTY(int count READ getCount NOTIFY countChanged FINAL)

    enum State // equivalent to addon_state_t
    {
        NOTINSTALLED = 0,
        INSTALLING,
        INSTALLED,
        UNINSTALLING
    };
    Q_ENUM(State)

    enum Role {
        SERVICE_NAME = Qt::UserRole + 1,
        SERVICE_AUTHOR,
        SERVICE_SUMMARY,
        SERVICE_DESCRIPTION,
        SERVICE_DOWNLOADS,
        SERVICE_SCORE,
        SERVICE_STATE,
        SERVICE_ARTWORK
    };
    Q_ENUM(Role);

    explicit ServicesDiscoveryModel(QObject* parent = nullptr);
    ~ServicesDiscoveryModel() override;

    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex& parent = {}) const override;

    void setCtx(MainCtx* ctx);

    inline MainCtx* getCtx() const { return m_ctx; }
    inline bool getParsingPending() const { return m_parsingPending; }
    int getCount() const;

    Q_INVOKABLE QMap<QString, QVariant> getDataAt(int idx);
    Q_INVOKABLE void installService(int idx);
    Q_INVOKABLE void removeService(int idx);

signals:
    void parsingPendingChanged();
    void countChanged();
    void ctxChanged();

private:
    using AddonPtr = vlc_shared_data_ptr_type(addon_entry_t,
                                addon_entry_Hold, addon_entry_Release);

    void initializeManager();
    static void addonFoundCallback( addons_manager_t *, struct addon_entry_t * );
    static void addonsDiscoveryEndedCallback( addons_manager_t * );
    static void addonChangedCallback( addons_manager_t *, struct addon_entry_t * );

    void addonFound( AddonPtr addon );
    void addonChanged( AddonPtr addon );
    void discoveryEnded();

    struct Item
    {
        QString name;
        QString summery;
        QString description;
        QString author;
        QUrl sourceUrl;
        QUrl artworkUrl;
        AddonPtr entry;

        Item( AddonPtr addon );
        Item& operator =( AddonPtr addon );
    };

    std::vector<Item> m_items;
    MainCtx* m_ctx = nullptr;
    addons_manager_t* m_manager = nullptr;
    bool m_parsingPending = false;
};

#endif // MLServicesDiscoveryModel_HPP
