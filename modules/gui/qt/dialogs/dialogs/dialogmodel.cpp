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
#include <qt.hpp>

//=================================================================================================
// DialogErrorModel
//=================================================================================================

/* explicit */ DialogErrorModel::DialogErrorModel(QObject * parent) : QAbstractListModel(parent) {}

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

void DialogErrorModel::pushError(const DialogError & error)
{
    int row = m_data.count();

    beginInsertRows(QModelIndex(), row, row);

    m_data.append(error);

    endInsertRows();

    emit countChanged();
}

//-------------------------------------------------------------------------------------------------
// Properties
//-------------------------------------------------------------------------------------------------

int DialogErrorModel::count() const
{
    return m_data.count();
}

//=================================================================================================
// DialogModel
//=================================================================================================

/* explicit */ DialogModel::DialogModel(qt_intf_t * intf, QObject * parent)
    : QObject(parent), m_intf(intf)
{
    m_model = new DialogErrorModel(this);

    const vlc_dialog_cbs cbs =
    {
        onError, onLogin, onQuestion, onProgress, onCancelled, onProgressUpdated
    };

    vlc_dialog_provider_set_callbacks(intf, &cbs, this);
}

//-------------------------------------------------------------------------------------------------
// Interface
//-------------------------------------------------------------------------------------------------

/* Q_INVOKABLE */ void DialogModel::post_login(DialogId dialogId, const QString & username,
                                               const QString & password, bool store)
{
    vlc_dialog_id_post_login(dialogId.m_id, qtu(username), qtu(password), store);
}

/* Q_INVOKABLE */ void DialogModel::post_action1(DialogId dialogId)
{
    vlc_dialog_id_post_action(dialogId.m_id, 1);
}

/* Q_INVOKABLE */ void DialogModel::post_action2(DialogId dialogId)
{
    vlc_dialog_id_post_action(dialogId.m_id, 2);
}

/* Q_INVOKABLE */ void DialogModel::dismiss(DialogId dialogId)
{
    vlc_dialog_id_dismiss(dialogId.m_id);
}

//-------------------------------------------------------------------------------------------------
// Private static functions
//-------------------------------------------------------------------------------------------------

/* static */ void DialogModel::onError(void * p_data,
                                       const char * psz_title, const char * psz_text)
{
    DialogModel * model = static_cast<DialogModel *>(p_data);

    DialogErrorModel::DialogError error { psz_title, psz_text };

    QMetaObject::invokeMethod(model, [model, error = std::move(error)]()
    {
        model->m_model->pushError(error);
    });
}

/* static */ void DialogModel::onLogin(void * p_data, vlc_dialog_id * dialogId,
                                       const char * psz_title, const char * psz_text,
                                       const char * psz_default_username, bool b_ask_store)
{
    DialogModel * model = static_cast<DialogModel *>(p_data);

    emit model->login(dialogId, psz_title, psz_text, psz_default_username, b_ask_store);
}

/* static */ void DialogModel::onQuestion(void * p_data, vlc_dialog_id * dialogId,
                                          const char * psz_title, const char * psz_text,
                                          vlc_dialog_question_type i_type,
                                          const char * psz_cancel, const char * psz_action1,
                                          const char * psz_action2)
{
    DialogModel * model = static_cast<DialogModel *>(p_data);

    emit model->question(dialogId, psz_title, psz_text, static_cast<int>(i_type), psz_cancel,
                         psz_action1, psz_action2);
}

/* static */ void DialogModel::onProgress(void * p_data, vlc_dialog_id * dialogId,
                                          const char * psz_title, const char * psz_text,
                                          bool b_indeterminate, float f_position,
                                          const char * psz_cancel)
{
    DialogModel * model = static_cast<DialogModel *>(p_data);

    emit model->progress(dialogId, psz_title, psz_text, b_indeterminate, f_position, psz_cancel);
}

/* static */ void DialogModel::onProgressUpdated(void * p_data, vlc_dialog_id * dialogId,
                                                 float f_value, const char * psz_text)
{
    DialogModel * model = static_cast<DialogModel *>(p_data);

    emit model->progressUpdated(dialogId, f_value, psz_text);
}

/* static */ void DialogModel::onCancelled(void * p_data, vlc_dialog_id * dialogId)
{
    DialogModel * model = static_cast<DialogModel *>(p_data);

    emit model->cancelled(dialogId);
}

//-------------------------------------------------------------------------------------------------
// Properties
//-------------------------------------------------------------------------------------------------

DialogErrorModel * DialogModel::model() const
{
    return m_model;
}
