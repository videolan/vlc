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

#include "dialogmodel.hpp"

// VLC includes
#include <vlc_dialog.h>
#include "qt.hpp"
#include <QThread>

#include "maininterface/mainctx.hpp"

//=================================================================================================
// DialogErrorModel
//=================================================================================================

DialogErrorModel::DialogErrorModel(qt_intf_t * intf, QObject * parent)
    : QAbstractListModel(parent)
    , m_intf(intf)
{
    vlc_dialog_provider_set_error_callback(intf, &DialogErrorModel::onError, this);
}

DialogErrorModel::~DialogErrorModel()
{
    vlc_dialog_provider_set_error_callback(m_intf, nullptr, nullptr);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    assert(qGuiApp);
    qGuiApp->setBadgeNumber(0);
#endif
}

//-------------------------------------------------------------------------------------------------
// QAbstractItemModel implementation
//-------------------------------------------------------------------------------------------------

QVariant DialogErrorModel::data(const QModelIndex & index, int role) const /* override */
{
    int row = index.row();

    if (row < 0 || row >= m_data.count())
        return QVariant();

    switch (role)
    {
        case DIALOG_TITLE:
            return QVariant::fromValue(m_data.at(row).title);
        case DIALOG_TEXT:
            return QVariant::fromValue(m_data.at(row).text);
        default:
            return QVariant();
    }
}

int DialogErrorModel::rowCount(const QModelIndex &) const /* override */
{
    return count();
}

//-------------------------------------------------------------------------------------------------
// QAbstractItemModel reimplementation
//-------------------------------------------------------------------------------------------------

QHash<int, QByteArray> DialogErrorModel::roleNames() const /* override */
{
    return
    {
        { DialogErrorModel::DIALOG_TITLE, "title" },
        { DialogErrorModel::DIALOG_TEXT,  "text"  }
    };
}

//-------------------------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------------------------

void DialogErrorModel::onError(void * p_data,
                               const char * psz_title, const char * psz_text)
{
    auto model = static_cast<DialogErrorModel *>(p_data);

    DialogErrorModel::DialogError error { psz_title, psz_text };

    QMetaObject::invokeMethod(model, [model, error = std::move(error)]()
    {
        model->pushError(error);
    });
}

void DialogErrorModel::pushError(const QString & title, const QString& message)
{
    DialogError error;
    error.title = title;
    error.text = message;
    pushError(error);
}

void DialogErrorModel::pushError(const DialogError & error)
{
    int row = m_data.count();

    beginInsertRows(QModelIndex(), row, row);

    m_data.append(error);

    endInsertRows();

    if (lastNotificationText == error.title)
        repeatedNotificationCount += 1;
    else{
        lastNotificationText = error.title;
        repeatedNotificationCount = 1;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    assert(qGuiApp);
    const int badgeNumber = qGuiApp->property("badgeNumber").toInt() + 1;
    qGuiApp->setBadgeNumber(badgeNumber);
    qGuiApp->setProperty("badgeNumber", badgeNumber);
#endif

    emit countChanged();
}

//-------------------------------------------------------------------------------------------------
// Properties
//-------------------------------------------------------------------------------------------------

int DialogErrorModel::count() const
{
    return m_data.count();
}

QString DialogErrorModel::notificationText() const
{
    return lastNotificationText;
}

int DialogErrorModel::repeatedMessageCount() const
{
    return repeatedNotificationCount;
}

void DialogErrorModel::resetRepeatedMessageCount()
{
   repeatedNotificationCount = 0;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Errors are dismissed, or error dialog is opened.
    assert(qGuiApp);
    qGuiApp->setBadgeNumber(0);
    qGuiApp->setProperty("badgeNumber", 0);
#endif
}

//=================================================================================================
// DialogModel
//=================================================================================================

//static functions
namespace {

void onDialogLogin(void * p_data, vlc_dialog_id * dialogId,
                                       const char * psz_title, const char * psz_text,
                                       const char * psz_default_username, bool b_ask_store)
{
    VLCDialogModel * model = static_cast<VLCDialogModel *>(p_data);
    model->dialogCallback(dialogId, [=](VLCDialog* provider){
        emit provider->login(dialogId, psz_title, psz_text, psz_default_username, b_ask_store);
    });

}

void onDialogQuestion(void * p_data, vlc_dialog_id * dialogId,
                                          const char * psz_title, const char * psz_text,
                                          vlc_dialog_question_type i_type,
                                          const char * psz_cancel, const char * psz_action1,
                                          const char * psz_action2)
{
    auto model = static_cast<VLCDialogModel *>(p_data);
    model->dialogCallback(dialogId, [=](VLCDialog* provider){
        emit provider->question(
            dialogId, psz_title, psz_text,
            static_cast<VLCDialog::QuestionType>(i_type), psz_cancel,
            psz_action1, psz_action2);
    });
}

void onDialogProgress(void * p_data, vlc_dialog_id * dialogId,
                                          const char * psz_title, const char * psz_text,
                                          bool b_indeterminate, float f_position,
                                          const char * psz_cancel)
{
    auto model = static_cast<VLCDialogModel*>(p_data);
    model->dialogCallback(dialogId, [=](VLCDialog* provider){
        emit provider->progress(dialogId, psz_title, psz_text, b_indeterminate, f_position, psz_cancel);
    });
}

void onDialogProgressUpdated(void * p_data, vlc_dialog_id * dialogId,
                                                 float f_value, const char * psz_text)
{
    auto model = static_cast<VLCDialogModel*>(p_data);
    model->dialogCallback(dialogId, [=](VLCDialog* provider){
        emit provider->progressUpdated(dialogId, f_value, psz_text);
    });
}

void onDialogCancelled(void * p_data, vlc_dialog_id * dialogId)
{
    auto model = static_cast<VLCDialogModel*>(p_data);
    model->dialogCallback(dialogId, [=](VLCDialog* provider){
        emit provider->cancelled(dialogId);
    });
}

}

VLCDialogModel::VLCDialogModel(qt_intf_t* intf, QObject* parent)
    : QObject(parent)
    , m_intf(intf)
{
    const vlc_dialog_cbs cbs =
        {
            onDialogLogin, onDialogQuestion, onDialogProgress,
            onDialogCancelled, onDialogProgressUpdated
        };
    vlc_dialog_provider_set_callbacks(intf, &cbs, this);
}

VLCDialogModel::~VLCDialogModel()
{
    m_shuttingDown = true;
    m_providerWait.wakeAll();
    vlc_dialog_provider_set_callbacks(m_intf, nullptr, nullptr);

    //ensure that we are not destroying ourselve before
    {
        QMutexLocker lock(&m_lock);
        while (m_pendingDialog > 0)
            m_pendingDialogCond.wait(&m_lock);
    }
}

VLCDialog* VLCDialogModel::getProvider() const
{
    return m_provider;
}

void VLCDialogModel::setProvider(VLCDialog* provider)
{
    if (m_provider == provider)
        return;
    if (m_provider)
    {
        disconnect(m_provider, nullptr, this, nullptr);
    }
    m_provider = provider;
    if (m_provider)
    {
        connect(m_provider, &VLCDialog::post_login,
                this, [](DialogId dialogId, const QString & username,
                        const QString & password, bool store){
                    vlc_dialog_id_post_login(dialogId.m_id, qtu(username), qtu(password), store);
                });
        connect(m_provider, &VLCDialog::post_action1,
                this, [](DialogId dialogId){
                    vlc_dialog_id_post_action(dialogId.m_id, 1);
                });
        connect(m_provider, &VLCDialog::post_action2,
                this, [](DialogId dialogId){
                    vlc_dialog_id_post_action(dialogId.m_id, 2);
                });
        connect(m_provider, &VLCDialog::dismiss,
                this, [](DialogId dialogId){
                    vlc_dialog_id_dismiss(dialogId.m_id);
                });

    }
    m_providerWait.wakeAll();
    emit providerChanged();
}


void VLCDialogModel::dialogCallback(vlc_dialog_id * dialogId, std::function<void(VLCDialog* provider)> callback)
{
    QMutexLocker lock(&m_lock);
    m_pendingDialog++;
    //dialogs are synchronous calls, if they are spawned from qt thread
    //(which shouldn't happen), we can't wait for m_provider
    if (QThread::currentThread() == this->thread() && m_provider == nullptr)
    {
        vlc_dialog_id_dismiss(dialogId);
        m_pendingDialog--;
        m_pendingDialogCond.wakeOne();
        return;
    }

    while (m_provider == nullptr && !m_shuttingDown)
        m_providerWait.wait(&m_lock);

    if (m_shuttingDown)
    {
        vlc_dialog_id_dismiss(dialogId);
        m_pendingDialog--;
        m_pendingDialogCond.wakeOne();
        return;
    }

    callback(m_provider);

    m_pendingDialog--;
    m_pendingDialogCond.wakeOne();
}
