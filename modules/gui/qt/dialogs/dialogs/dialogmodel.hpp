/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

#ifndef DIALOGMODEL_HPP
#define DIALOGMODEL_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

// VLC includes
#include <vlc_common.h>
#include <vlc_dialog.h>

// Qt includes
#include <QAbstractListModel>
#include <QQmlEngine>
#include <QMutex>
#include <QWaitCondition>

#include "qt.hpp"
#include "util/singleton.hpp"

Q_MOC_INCLUDE("maininterface/mainctx.hpp")

class DialogId
{
    Q_GADGET

public:
    DialogId(vlc_dialog_id * id = nullptr) : m_id(id) {}

public: // Operators
    bool operator ==(const DialogId & other) const
    {
        return m_id == other.m_id;
    }

public: // Variables
    vlc_dialog_id * m_id;
};


class DialogErrorModel : public QAbstractListModel, public Singleton<DialogErrorModel>
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(QString notificationText READ notificationText NOTIFY countChanged FINAL)
    Q_PROPERTY(int repeatedMessageCount READ repeatedMessageCount NOTIFY countChanged FINAL)

public: // Enums
    enum DialogRoles
    {
        DIALOG_TITLE = Qt::UserRole + 1,
        DIALOG_TEXT
    };
    Q_ENUM(DialogRoles)

private:
    struct DialogError
    {
        QString title;
        QString text;
    };
    QString lastNotificationText;
    int repeatedNotificationCount = 0;

public:
    explicit DialogErrorModel(qt_intf_t* intf, QObject * parent = nullptr);
    virtual ~DialogErrorModel();

public: // QAbstractItemModel implementation
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;

    int rowCount(const QModelIndex & parent = QModelIndex()) const override;

public: // QAbstractItemModel reimplementation
    QHash<int, QByteArray> roleNames() const override;

public slots:
    ///manually push an error
    void pushError(const QString & title, const QString& message);

private: // Functions
    void pushError(const DialogError & error);

    static void onError(void * p_data, const char * psz_title, const char * psz_text);

signals:
    void modelChanged();

    void countChanged();

public: // Properties
    int count() const;
    QString notificationText() const;
    int repeatedMessageCount() const;
    Q_INVOKABLE void resetRepeatedMessageCount();

private: // Variables
    QList<DialogError> m_data;
    qt_intf_t* m_intf = nullptr;

    friend class Singleton<DialogErrorModel>;
};

/**
 * this class expose vlc_dialog events and allow to reply
 * to use it, instantiate the object, connect the signals then
 * register it in VLCDialogModel
 */
class VLCDialog: public QObject
{
    Q_OBJECT

public:
    enum QuestionType {
        QUESTION_NORMAL = VLC_DIALOG_QUESTION_NORMAL,
        QUESTION_WARNING = VLC_DIALOG_QUESTION_WARNING,
        QUESTION_CRITICAL = VLC_DIALOG_QUESTION_CRITICAL
    };
    Q_ENUM(QuestionType)

signals:
    //dialog user actions
    Q_INVOKABLE void post_login(DialogId dialogId, const QString & username,
                    const QString & password, bool store = false);

    Q_INVOKABLE void post_action1(DialogId dialogId);
    Q_INVOKABLE void post_action2(DialogId dialogId);

    Q_INVOKABLE void dismiss(DialogId dialogId);

    //dialog request

    void login(DialogId dialogId, const QString & title,
               const QString & text, const QString & defaultUsername,
               bool b_ask_store);

    void question(DialogId dialogId, const QString & title, const QString & text, QuestionType type,
                  const QString & cancel, const QString & action1, const QString & action2);

    void progress(DialogId dialogId, const QString & title, const QString & text,
                  bool b_indeterminate, float f_position, const QString & cancel);

    void progressUpdated(DialogId dialogId, float f_value, const QString & text);

    void cancelled(DialogId dialogId);
};

/**
 * This class listen to vlc_dialog_t events and forward them to VLCDialog
 */
class VLCDialogModel : public QObject, public Singleton<VLCDialogModel>
{
    Q_OBJECT

    Q_PROPERTY(VLCDialog* provider READ getProvider WRITE setProvider NOTIFY providerChanged FINAL)

public:
    explicit VLCDialogModel(qt_intf_t* intf, QObject *parent = nullptr);
    ~VLCDialogModel();

public:
    VLCDialog* getProvider() const;
    void setProvider(VLCDialog*);

public:
    //block dialog until m_provider is available the call the callback on it
    void dialogCallback(vlc_dialog_id*, std::function<void(VLCDialog* provider)>);

signals:
    void providerChanged();

private:
    qt_intf_t* m_intf = nullptr;
    QMutex  m_lock;
    //waiting for the provider to be set
    QWaitCondition m_providerWait;

    //during destruction, waiting for dialogCallback to finish
    QWaitCondition m_pendingDialogCond;
    unsigned m_pendingDialog = 0;

    VLCDialog* m_provider = nullptr;
    bool m_shuttingDown = false;

    friend class Singleton<VLCDialogModel>;
};

#endif // DIALOGMODEL_HPP
