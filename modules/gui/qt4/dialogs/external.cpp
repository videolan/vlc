/*****************************************************************************
 * external.hpp : Dialogs from other LibVLC core and other plugins
 ****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
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

#include <QMessageBox>

DialogHandler::DialogHandler (intf_thread_t *intf)
{
    this->intf = intf;

    connect (this, SIGNAL(message(const struct dialog_fatal_t *)),
             this, SLOT(displayMessage(const struct dialog_fatal_t *)),
             Qt::BlockingQueuedConnection);
     var_Create (intf, "dialog-fatal", VLC_VAR_ADDRESS);
     var_AddCallback (intf, "dialog-fatal", MessageCallback, this);

     dialog_Register (intf);
}

DialogHandler::~DialogHandler (void)
{
    dialog_Unregister (intf);
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
