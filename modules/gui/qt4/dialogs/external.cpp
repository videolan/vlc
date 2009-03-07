/*****************************************************************************
 * external.hpp : Dialogs from other LibVLC core and other plugins
 ****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
 * Copyright (C) 2006 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

//#include "qt4.hpp"
#include "external.hpp"
#include "errors.hpp"
#include <vlc_dialog.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>

DialogHandler::DialogHandler (intf_thread_t *intf)
{
    this->intf = intf;

    connect (this, SIGNAL(message(const struct dialog_fatal_t *)),
             this, SLOT(displayMessage(const struct dialog_fatal_t *)),
             Qt::BlockingQueuedConnection);
     var_Create (intf, "dialog-fatal", VLC_VAR_ADDRESS);
     var_AddCallback (intf, "dialog-fatal", MessageCallback, this);

    connect (this, SIGNAL(authentication(struct dialog_login_t *)),
             this, SLOT(requestLogin(struct dialog_login_t *)),
             Qt::BlockingQueuedConnection);
     var_Create (intf, "dialog-login", VLC_VAR_ADDRESS);
     var_AddCallback (intf, "dialog-login", LoginCallback, this);

     dialog_Register (intf);
}

DialogHandler::~DialogHandler (void)
{
    dialog_Unregister (intf);
    var_DelCallback (intf, "dialog-login", LoginCallback, this);
    var_DelCallback (intf, "dialog-fatal", MessageCallback, this);
}

int DialogHandler::MessageCallback (vlc_object_t *obj, const char *var,
                                    vlc_value_t, vlc_value_t value,
                                    void *data)
{
     DialogHandler *self = (DialogHandler *)data;
     const dialog_fatal_t *dialog = (const dialog_fatal_t *)value.p_address;

     emit self->message (dialog);
     return VLC_SUCCESS;
}

void DialogHandler::displayMessage (const struct dialog_fatal_t *dialog)
{
    if (dialog->modal)
        QMessageBox::critical (NULL, qfu(dialog->title), qfu(dialog->message),
                               QMessageBox::Ok);
    else
    if (config_GetInt (intf, "qt-error-dialogs"))
        ErrorsDialog::getInstance (intf)->addError(qfu(dialog->title),
                                                   qfu(dialog->message));
}

int DialogHandler::LoginCallback (vlc_object_t *obj, const char *var,
                                  vlc_value_t, vlc_value_t value, void *data)
{
     DialogHandler *self = (DialogHandler *)data;
     dialog_login_t *dialog = (dialog_login_t *)value.p_address;

     emit self->authentication (dialog);
     return VLC_SUCCESS;
}

void DialogHandler::requestLogin (struct dialog_login_t *data)
{
    QDialog *dialog = new QDialog;
    QLayout *layout = new QVBoxLayout (dialog);

    dialog->setWindowTitle (qfu(data->title));
    layout->setMargin (2);

    /* User name and password fields */
    QWidget *panel = new QWidget (dialog);
    QGridLayout *grid = new QGridLayout;
    grid->addWidget (new QLabel (qfu(data->message)), 0, 0, 1, 2);

    QLineEdit *userLine = new QLineEdit;
    grid->addWidget (new QLabel (qtr("User name")), 1, 0);
    grid->addWidget (userLine, 1, 1);

    QLineEdit *passLine = new QLineEdit;
    passLine->setEchoMode (QLineEdit::Password);
    grid->addWidget (new QLabel (qtr("Password")), 2, 0);
    grid->addWidget (passLine, 2, 1);

    panel->setLayout (grid);
    layout->addWidget (panel);

    /* OK, Cancel buttons */
    QDialogButtonBox *buttonBox;
    buttonBox = new QDialogButtonBox (QDialogButtonBox::Ok
                                       | QDialogButtonBox::Cancel);
    connect (buttonBox, SIGNAL(accepted()), dialog, SLOT(accept()));
    connect (buttonBox, SIGNAL(rejected()), dialog, SLOT(reject()));
    layout->addWidget (buttonBox);

    /* Run the dialog */
    dialog->setLayout (layout);

    if (dialog->exec ())
    {
        *data->username = strdup (qtu(userLine->text ()));
        *data->password = strdup (qtu(passLine->text ()));
    }
    else
        *data->username = *data->password = NULL;

    delete dialog;
}
