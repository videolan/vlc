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
#include "dialogmodel.hpp"

static void displayErrorCb(void *p_data, const char *psz_title, const char *psz_text)
{
    DialogModel* that = static_cast<DialogModel*>(p_data);
    emit that->errorDisplayed(psz_title, psz_text);
}

static void displayLoginCb(void *p_data, vlc_dialog_id *dialogId,
                        const char *psz_title, const char *psz_text,
                        const char *psz_default_username,
                        bool b_ask_store)
{
    DialogModel* that = static_cast<DialogModel*>(p_data);
    emit that->loginDisplayed( dialogId, psz_title, psz_text, psz_default_username, b_ask_store );
}

static void displayQuestionCb(void *p_data, vlc_dialog_id *dialogId,
                       const char *psz_title, const char *psz_text,
                       vlc_dialog_question_type i_type,
                       const char *psz_cancel, const char *psz_action1,
                       const char *psz_action2)
{
    DialogModel* that = static_cast<DialogModel*>(p_data);
    emit that->questionDisplayed( dialogId, psz_title, psz_text, static_cast<int>(i_type), psz_cancel, psz_action1, psz_action2 );
}

static void displayProgressCb(void *p_data, vlc_dialog_id *dialogId,
                            const char *psz_title, const char *psz_text,
                            bool b_indeterminate, float f_position,
                            const char *psz_cancel)
{
    DialogModel* that = static_cast<DialogModel*>(p_data);
    emit that->progressDisplayed( dialogId, psz_title, psz_text, b_indeterminate, f_position, psz_cancel);
}

static void cancelCb(void *p_data, vlc_dialog_id *dialogId)
{
    DialogModel* that = static_cast<DialogModel*>(p_data);
    emit that->cancelled(dialogId);
}

static void updateProgressCb(void *p_data, vlc_dialog_id *dialogId, float f_value, const char *psz_text)
{
    DialogModel* that = static_cast<DialogModel*>(p_data);
    emit that->progressUpdated( dialogId, f_value, psz_text );
}

const vlc_dialog_cbs cbs = {
    displayErrorCb,
    displayLoginCb,
    displayQuestionCb,
    displayProgressCb,
    cancelCb,
    updateProgressCb
};

DialogModel::DialogModel(QObject *parent)
    : QObject(parent)
{
}

DialogModel::~DialogModel()
{
    if (m_mainCtx)
        vlc_dialog_provider_set_callbacks(m_mainCtx->getIntf(), nullptr, nullptr);
}

void DialogModel::dismiss(DialogId dialogId)
{
    vlc_dialog_id_dismiss(dialogId.m_id);
}

void DialogModel::post_login(DialogId dialogId, const QString& username, const QString& password, bool store)
{
    vlc_dialog_id_post_login(dialogId.m_id, qtu(username), qtu(password), store);
}

void DialogModel::post_action1(DialogId dialogId)
{
    vlc_dialog_id_post_action(dialogId.m_id, 1);
}

void DialogModel::post_action2(DialogId dialogId)
{
    vlc_dialog_id_post_action(dialogId.m_id, 2);
}

void DialogModel::setMainCtx(QmlMainContext* ctx)
{
    if (ctx)
        vlc_dialog_provider_set_callbacks(ctx->getIntf(), &cbs, this);
    else if (m_mainCtx)
        vlc_dialog_provider_set_callbacks(m_mainCtx->getIntf(), nullptr, nullptr);
    m_mainCtx = ctx;
}
