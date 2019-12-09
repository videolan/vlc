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
#ifndef DIALOGMODEL_HPP
#define DIALOGMODEL_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_dialog.h>

#include <QObject>

#include "util/qml_main_context.hpp"


class DialogId {
    Q_GADGET
public:
    DialogId(vlc_dialog_id * id = nullptr)
        : m_id(id)
    {}

    bool operator ==(const DialogId& other) const {
        return m_id == other.m_id;
    }
    vlc_dialog_id *m_id;
};

Q_DECLARE_METATYPE(DialogId)

class DialogModel : public QObject
{
    Q_OBJECT

public:
    Q_PROPERTY(QmlMainContext* mainCtx READ getMainCtx WRITE setMainCtx NOTIFY mainCtxChanged)

    enum QuestionType {
        QUESTION_NORMAL,
        QUESTION_WARNING,
        QUESTION_CRITICAL
    };
    Q_ENUM(QuestionType)

public:
    explicit DialogModel(QObject *parent = nullptr);
    ~DialogModel();

    inline QmlMainContext* getMainCtx() const { return m_mainCtx; }
    void setMainCtx(QmlMainContext*);

signals:
    void errorDisplayed(const QString &title, const QString &text);
    void loginDisplayed(DialogId dialogId, const QString &title,
                        const QString &text, const QString &defaultUsername,
                        bool b_ask_store);

    void questionDisplayed(DialogId dialogId, const QString &title,
                           const QString &text, int type,
                           const QString &cancel, const QString &action1,
                           const QString &action2);

    void progressDisplayed(DialogId dialogId, const QString &title,
                           const QString &text, bool b_indeterminate,
                           float f_position, const QString &cancel);

    void cancelled(DialogId dialogId);

    void progressUpdated(DialogId dialogId, float f_value, const QString &text);

    void mainCtxChanged();

public slots:
    void dismiss(DialogId dialogId);
    void post_login(DialogId dialogId, const QString& username, const QString& password, bool store = false);
    void post_action1(DialogId dialogId);
    void post_action2(DialogId dialogId);

private:
    QmlMainContext* m_mainCtx;
};

#endif // DIALOGMODEL_HPP
