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

#ifndef QVLC_TOOLBAREDITOR_DIALOG_H_
#define QVLC_TOOLBAREDITOR_DIALOG_H_ 1

#include "widgets/native/qvlcframe.hpp"                                 /* QVLCDialog */
#include "util/qml_main_context.hpp"
#include "dialogs/sout/profile_selector.hpp"

#include <QAbstractListModel>
#include <QVector>
#include <QQuickWidget>

class ToolbarEditorDialog : public QVLCDialog
{
    Q_OBJECT
public:
    ToolbarEditorDialog( QWidget *, intf_thread_t *);
    virtual ~ToolbarEditorDialog();

public slots:
    Q_INVOKABLE void close();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void deleteCursor();
    Q_INVOKABLE void restoreCursor();

private slots:
    void newProfile();
    void deleteProfile();
    void changeProfile( int );

private:
    QComboBox *profileCombo;
    QQuickWidget *editorView;

signals:
    void updatePlayerModel(QString toolbarName,QString config);
    void saveConfig();
};

#endif
