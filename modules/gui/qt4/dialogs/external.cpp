/*****************************************************************************
 * external.cpp : Dialogs from other LibVLC core and other plugins
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
#include <QPushButton>
#include <QTimer>

DialogHandler::DialogHandler (intf_thread_t *p_intf, QObject *_parent)
    : QObject( _parent ), intf (p_intf),
      critical (VLC_OBJECT(p_intf), "dialog-critical"),
      login (VLC_OBJECT(p_intf), "dialog-login"),
      question (VLC_OBJECT(p_intf), "dialog-question"),
      progressBar (VLC_OBJECT(p_intf), "dialog-progress-bar")
{
    var_Create (intf, "dialog-error", VLC_VAR_ADDRESS);
    var_AddCallback (intf, "dialog-error", error, this);
    connect (this, SIGNAL(error(const QString &, const QString &)),
             SLOT(displayError(const QString &, const QString &)));

    critical.addCallback(this, SLOT(displayCritical(void *)),
                         Qt::BlockingQueuedConnection);
    login.addCallback(this, SLOT(requestLogin(void *)),
                      Qt::BlockingQueuedConnection);
    question.addCallback(this, SLOT(requestAnswer(void *)),
                         Qt::BlockingQueuedConnection);
    progressBar.addCallback(this, SLOT(startProgressBar(void *)),
                            Qt::BlockingQueuedConnection);

    dialog_Register (intf);
}

DialogHandler::~DialogHandler (void)
{
    dialog_Unregister (intf);

    var_DelCallback (intf, "dialog-error", error, this);
    var_Destroy (intf, "dialog-error");
}

int DialogHandler::error (vlc_object_t *obj, const char *,
                          vlc_value_t, vlc_value_t value, void *data)
{
    const dialog_fatal_t *dialog = (const dialog_fatal_t *)value.p_address;
    DialogHandler *self = static_cast<DialogHandler *>(data);

    if (var_InheritBool (obj, "qt-error-dialogs"))
        emit self->error (qfu(dialog->title), qfu(dialog->message));
    return VLC_SUCCESS;
}

void DialogHandler::displayError (const QString& title, const QString& message)
{
    ErrorsDialog::getInstance (intf)->addError(title, message);
}

void DialogHandler::displayCritical (void *value)
{
    const dialog_fatal_t *dialog = (const dialog_fatal_t *)value;

    QMessageBox::critical (NULL, qfu(dialog->title), qfu(dialog->message),
                           QMessageBox::Ok);
}

void DialogHandler::requestLogin (void *value)
{
    dialog_login_t *data = (dialog_login_t *)value;
    QDialog *dialog = new QDialog;
    QLayout *layout = new QVBoxLayout (dialog);

    dialog->setWindowTitle (qfu(data->title));
    dialog->setWindowRole ("vlc-login");
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
    QDialogButtonBox *buttonBox = new QDialogButtonBox;
    QPushButton *okButton = new QPushButton( "&Ok" );
    QPushButton *cancelButton = new QPushButton( "&Cancel" );
    buttonBox->addButton( okButton, QDialogButtonBox::AcceptRole );
    buttonBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    CONNECT( buttonBox, accepted(), dialog, accept() );
    CONNECT( buttonBox, rejected(), dialog, reject() );
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

void DialogHandler::requestAnswer (void *value)
{
    dialog_question_t *data = (dialog_question_t *)value;

    QMessageBox *box = new QMessageBox (QMessageBox::Question,
                                        qfu(data->title), qfu(data->message));
    QAbstractButton *yes = (data->yes != NULL)
        ? box->addButton ("&" + qfu(data->yes), QMessageBox::YesRole) : NULL;
    QAbstractButton *no = (data->no != NULL)
        ? box->addButton ("&" + qfu(data->no), QMessageBox::NoRole) : NULL;
    if (data->cancel != NULL)
        box->addButton ("&" + qfu(data->cancel), QMessageBox::RejectRole);

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
      handler (parent),
      cancelled (false)
{
    if (data->cancel)
        setWindowModality (Qt::ApplicationModal);
    if (data->title != NULL)
        setWindowTitle (qfu(data->title));

    setWindowRole ("vlc-progress");
    setValue( 0 );

    connect (this, SIGNAL(progressed(int)), SLOT(setValue(int)));
    connect (this, SIGNAL(described(const QString&)),
                   SLOT(setLabelText(const QString&)));
    connect (this, SIGNAL(canceled(void)), SLOT(saveCancel(void)));
    connect (this, SIGNAL(released(void)), SLOT(deleteLater(void)));

    data->pf_update = update;
    data->pf_check = check;
    data->pf_destroy = destroy;
    data->p_sys = this;
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

    emit self->released ();
}

void QVLCProgressDialog::saveCancel (void)
{
    QMutexLocker locker (&cancel_mutex);
    cancelled = true;
}

void DialogHandler::startProgressBar (void *value)
{
    dialog_progress_bar_t *data = (dialog_progress_bar_t *)value;
    QWidget *dlg = new QVLCProgressDialog (this, data);

    QTimer::singleShot( 1500, dlg, SLOT( show() ) );
//    dlg->show ();
}

void DialogHandler::stopProgressBar (QWidget *dlg)
{
    delete dlg;
}
