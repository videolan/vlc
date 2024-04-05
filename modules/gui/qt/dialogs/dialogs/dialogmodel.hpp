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

class DialogModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(MainCtx* ctx READ getCtx WRITE setCtx NOTIFY ctxChanged FINAL)

public: // Enums
    // NOTE: Is it really useful to have this declared here ?
    enum QuestionType { QUESTION_NORMAL, QUESTION_WARNING, QUESTION_CRITICAL };
    Q_ENUM(QuestionType)

public:
    explicit DialogModel(QObject *parent = nullptr);
    ~DialogModel();

public: // Interface
    Q_INVOKABLE void post_login(DialogId dialogId, const QString & username,
                                const QString & password, bool store = false);

    Q_INVOKABLE void post_action1(DialogId dialogId);
    Q_INVOKABLE void post_action2(DialogId dialogId);

    Q_INVOKABLE void dismiss(DialogId dialogId);

private: // Static functions
    static void onLogin(void * p_data, vlc_dialog_id * dialogId, const char * psz_title,
                        const char * psz_text, const char * psz_default_username,
                        bool b_ask_store);

    static void onQuestion(void * p_data, vlc_dialog_id * dialogId, const char * psz_title,
                           const char * psz_text, vlc_dialog_question_type i_type,
                           const char * psz_cancel, const char * psz_action1,
                           const char * psz_action2);

    static void onProgress(void * p_data, vlc_dialog_id * dialogId, const char * psz_title,
                           const char * psz_text, bool b_indeterminate, float f_position,
                           const char *psz_cancel);

    static void onProgressUpdated(void * p_data, vlc_dialog_id * dialogId, float f_value,
                                  const char * psz_text);

    static void onCancelled(void * p_data, vlc_dialog_id * dialogId);

public:
    MainCtx* getCtx() const;
    void setCtx(MainCtx*);

signals:
    void login(DialogId dialogId, const QString & title,
               const QString & text, const QString & defaultUsername,
               bool b_ask_store);

    void question(DialogId dialogId, const QString & title, const QString & text, int type,
                  const QString & cancel, const QString & action1, const QString & action2);

    void progress(DialogId dialogId, const QString & title, const QString & text,
                  bool b_indeterminate, float f_position, const QString & cancel);

    void progressUpdated(DialogId dialogId, float f_value, const QString & text);

    void cancelled(DialogId dialogId);

    void ctxChanged();

private:
    MainCtx* m_ctx = nullptr;
};

#endif // DIALOGMODEL_HPP
