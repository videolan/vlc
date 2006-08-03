/*****************************************************************************
 * interaction.hpp : Interaction dialogs
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#ifndef _INTERACTION_H_
#define _INTERACTION_H_

#include <vlc/vlc.h>
#include <vlc_interaction.h>
#undef Q_
#include <ui/inputdialog.h>
#undef Q_
#include <ui/logindialog.h>
#undef Q_
#include <ui/progressdialog.h>
#undef Q_

class InteractionDialog : public QWidget
{
    Q_OBJECT
public:
    InteractionDialog( intf_thread_t *, interaction_dialog_t * );
    virtual ~InteractionDialog();

    void Update();

private:
    QWidget *panel;
    intf_thread_t *p_intf;
    interaction_dialog_t *p_dialog;
    Ui::LoginDialog *uiLogin;
    Ui::InputDialog *uiInput;
    Ui::ProgressDialog *uiProgress;

    QPushButton *defaultButton, *otherButton, *altButton;
    QLabel *description;

    void Finish( int );
private slots:
    void defaultB();
    void altB();
    void otherB();
};

#endif
