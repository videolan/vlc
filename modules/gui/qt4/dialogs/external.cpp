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
#include <QProgressDialog>
#include <QMutex>

QVLCVariable::QVLCVariable (vlc_object_t *obj, const char *varname, int type)
    : object (obj), name (qfu(varname))
{
    var_Create (object, qtu(name), type);
    var_AddCallback (object, qtu(name), callback, this);
}

QVLCVariable::~QVLCVariable (void)
{
    var_DelCallback (object, qtu(name), callback, this);
    var_Destroy (object, qtu(name));
}

int QVLCVariable::callback (vlc_object_t *object, const char *,
                            vlc_value_t, vlc_value_t cur, void *data)
{
    QVLCVariable *self = (QVLCVariable *)data;

    emit self->pointerChanged (object, cur.p_address);
    return VLC_SUCCESS;
}


DialogHandler::DialogHandler (intf_thread_t *intf, QObject *_parent)
    : intf (intf), QObject( _parent ),
      message (VLC_OBJECT(intf), "dialog-fatal", VLC_VAR_ADDRESS),
      login (VLC_OBJECT(intf), "dialog-login", VLC_VAR_ADDRESS),
      question (VLC_OBJECT(intf), "dialog-question", VLC_VAR_ADDRESS),
      progressBar (VLC_OBJECT(intf), "dialog-progress-bar", VLC_VAR_ADDRESS)
{
    connect (&message, SIGNAL(pointerChanged(vlc_object_t *, void *)),
             SLOT(displayMessage(vlc_object_t *, void *)),
             Qt::BlockingQueuedConnection);
    connect (&login, SIGNAL(pointerChanged(vlc_object_t *, void *)),
             SLOT(requestLogin(vlc_object_t *, void *)),
             Qt::BlockingQueuedConnection);
    connect (&question, SIGNAL(pointerChanged(vlc_object_t *, void *)),
             SLOT(requestAnswer(vlc_object_t *, void *)),
             Qt::BlockingQueuedConnection);
    connect (&progressBar, SIGNAL(pointerChanged(vlc_object_t *, void *)),
             SLOT(startProgressBar(vlc_object_t *, void *)),
             Qt::BlockingQueuedConnection);
    connect (this,
             SIGNAL(progressBarDestroyed(QWidget *)),
             SLOT(stopProgressBar(QWidget *)));

    dialog_Register (intf);
}

DialogHandler::~DialogHandler (void)
{
    dialog_Unregister (intf);
}

void DialogHandler::displayMessage (vlc_object_t *, void *value)
{
     const dialog_fatal_t *dialog = (const dialog_fatal_t *)value;

    if (dialog->modal)
        QMessageBox::critical (NULL, qfu(dialog->title), qfu(dialog->message),
                               QMessageBox::Ok);
    else
    if (config_GetInt (intf, "qt-error-dialogs"))
        ErrorsDialog::getInstance (intf)->addError(qfu(dialog->title),
                                                   qfu(dialog->message));
}

void DialogHandler::requestLogin (vlc_object_t *, void *value)
{
    dialog_login_t *data = (dialog_login_t *)value;
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

void DialogHandler::requestAnswer (vlc_object_t *, void *value)
{
    dialog_question_t *data = (dialog_question_t *)value;

    QMessageBox *box = new QMessageBox (QMessageBox::Question,
                                        qfu(data->title), qfu(data->message));
    QAbstractButton *yes = (data->yes != NULL)
        ? box->addButton ("&" + qfu(data->yes), QMessageBox::YesRole) : NULL;
    QAbstractButton *no = (data->no != NULL)
        ? box->addButton ("&" + qfu(data->no), QMessageBox::NoRole) : NULL;
    QAbstractButton *cancel = (data->cancel != NULL)
        ? box->addButton ("&" + qfu(data->cancel), QMessageBox::RejectRole)
        : NULL;

    box->exec ();

    int answer;
    if (box->clickedButton () == yes)
        answer = 1;
    else
    if (box->clickedButton () == no)
        answer = 2;
    else
        answer = 3;

    delete box;
    data->answer = answer;
}


QVLCProgressDialog::QVLCProgressDialog (DialogHandler *parent,
                                        struct dialog_progress_bar_t *data)
    : QProgressDialog (qfu(data->message),
                       data->cancel ? ("&" + qfu(data->cancel)) : 0, 0, 1000),
      cancelled (false),
      handler (parent)
{
    if (data->title != NULL)
        setWindowTitle (qfu(data->title));
    setMinimumDuration (0);

    connect (this, SIGNAL(progressed(int)), SLOT(setValue(int)));
    connect (this, SIGNAL(described(const QString&)),
                   SLOT(setLabelText(const QString&)));
    connect (this, SIGNAL(canceled(void)), SLOT(saveCancel(void)));

    data->pf_update = update;
    data->pf_check = check;
    data->pf_destroy = destroy;
    data->p_sys = this;
}

QVLCProgressDialog::~QVLCProgressDialog (void)
{
}

void QVLCProgressDialog::update (void *priv, const char *text, float value)
{
    QVLCProgressDialog *self = static_cast<QVLCProgressDialog *>(priv);

    if (text != NULL)
        emit self->described (qfu(text));
    emit self->progressed ((int)(value * 1000.));
}

static QMutex cancel_mutex;

bool QVLCProgressDialog::check (void *priv)
{
    QVLCProgressDialog *self = static_cast<QVLCProgressDialog *>(priv);
    QMutexLocker locker (&cancel_mutex);
    return self->cancelled;
}

void QVLCProgressDialog::destroy (void *priv)
{
    QVLCProgressDialog *self = static_cast<QVLCProgressDialog *>(priv);

    emit self->handler->progressBarDestroyed (self);
}

void QVLCProgressDialog::saveCancel (void)
{
    QMutexLocker locker (&cancel_mutex);
    cancelled = true;
}

void DialogHandler::startProgressBar (vlc_object_t *, void *value)
{
    dialog_progress_bar_t *data = (dialog_progress_bar_t *)value;
    QWidget *dlg = new QVLCProgressDialog (this, data);

    dlg->show ();
}

void DialogHandler::stopProgressBar (QWidget *dlg)
{
    delete dlg;
}
