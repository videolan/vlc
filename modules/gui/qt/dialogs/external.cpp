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

#include "external.hpp"
#include "errors.hpp"

#include <assert.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>

DialogHandler::DialogHandler (intf_thread_t *p_intf, QObject *_parent)
    : QObject( _parent ), p_intf (p_intf)
{
    const vlc_dialog_cbs cbs = {
        displayErrorCb,
        displayLoginCb,
        displayQuestionCb,
        displayProgressCb,
        cancelCb,
        updateProgressCb
    };
    vlc_dialog_provider_set_callbacks(p_intf, &cbs, this);

    CONNECT(this, errorDisplayed(const QString &, const QString &),
            this, displayError(const QString &, const QString &));

    CONNECT(this, loginDisplayed(vlc_dialog_id *, const QString &,
                                 const QString &, const QString &, bool),
            this, displayLogin(vlc_dialog_id *, const QString &, const QString &,
                               const QString &, bool));

    CONNECT(this, questionDisplayed(vlc_dialog_id *, const QString &,
                                    const QString &, int, const QString &,
                                    const QString &, const QString &),
            this, displayQuestion(vlc_dialog_id *, const QString &, const QString &,
                                  int, const QString &, const QString &,
                                  const QString &));

    CONNECT(this, progressDisplayed(vlc_dialog_id *, const QString &, const QString &,
                                    bool, float, const QString &),
            this, displayProgress(vlc_dialog_id *, const QString &, const QString &,
                                  bool, float, const QString &));

    CONNECT(this, cancelled(vlc_dialog_id *), this, cancel(vlc_dialog_id *));

    CONNECT(this, progressUpdated(vlc_dialog_id *, float, const QString &),
            this, updateProgress(vlc_dialog_id *, float, const QString &));
}

DialogHandler::~DialogHandler()
{
    vlc_dialog_provider_set_callbacks(p_intf, NULL, NULL);
}

void
DialogHandler::displayErrorCb(void *p_data, const char *psz_title,
                              const char *psz_text)
{
    DialogHandler *self =  static_cast<DialogHandler *>(p_data);
    const QString title = qfu(psz_title);
    const QString text = qfu(psz_text);

    emit self->errorDisplayed(title, text);
}


void
DialogHandler::displayLoginCb(void *p_data, vlc_dialog_id *p_id,
                              const char *psz_title, const char *psz_text,
                              const char *psz_default_username,
                              bool b_ask_store)
{
    DialogHandler *self =  static_cast<DialogHandler *>(p_data);
    const QString title = qfu(psz_title);
    const QString text = qfu(psz_text);

    const QString defaultUsername =
        psz_default_username != NULL ? qfu(psz_default_username) : QString();

    emit self->loginDisplayed(p_id, title, text, defaultUsername,
                              b_ask_store);
}

void
DialogHandler::displayQuestionCb(void *p_data, vlc_dialog_id *p_id,
                                 const char *psz_title, const char *psz_text,
                                 vlc_dialog_question_type i_type,
                                 const char *psz_cancel, const char *psz_action1,
                                 const char *psz_action2)
{
    DialogHandler *self =  static_cast<DialogHandler *>(p_data);
    const QString title = qfu(psz_title);
    const QString text = qfu(psz_text);

    const QString cancel = qfu(psz_cancel);
    const QString action1 = psz_action1 != NULL ? qfu(psz_action1) : QString();
    const QString action2 = psz_action2 != NULL ? qfu(psz_action2) : QString();

    emit self->questionDisplayed(p_id, title, text, i_type, cancel,
                                 action1, action2);
}

void
DialogHandler::displayProgressCb(void *p_data, vlc_dialog_id *p_id,
                                 const char *psz_title, const char *psz_text,
                                 bool b_indeterminate, float f_position,
                                 const char *psz_cancel)
{
    DialogHandler *self =  static_cast<DialogHandler *>(p_data);
    const QString title = qfu(psz_title);
    const QString text = qfu(psz_text);

    const QString cancel = psz_cancel != NULL ? qfu(psz_cancel) : QString();
    emit self->progressDisplayed(p_id, title, text, b_indeterminate,
                                 f_position, cancel);
}

void DialogHandler::cancelCb(void *p_data, vlc_dialog_id *p_id)
{
    DialogHandler *self = static_cast<DialogHandler *>(p_data);
    emit self->cancelled(p_id);
}

void DialogHandler::updateProgressCb(void *p_data, vlc_dialog_id *p_id,
                                     float f_value, const char *psz_text)
{
    DialogHandler *self = static_cast<DialogHandler *>(p_data);
    emit self->progressUpdated(p_id, f_value, qfu(psz_text));
}

void DialogHandler::cancel(vlc_dialog_id *p_id)
{
    DialogWrapper *p_wrapper =
        static_cast<DialogWrapper *>(vlc_dialog_id_get_context(p_id));
    if (p_wrapper != NULL)
        p_wrapper->finish(QDialog::Rejected);
}

void DialogHandler::updateProgress(vlc_dialog_id *p_id, float f_value,
                                   const QString &text)
{
    ProgressDialogWrapper *p_wrapper =
        static_cast<ProgressDialogWrapper *>(vlc_dialog_id_get_context(p_id));

    if (p_wrapper != NULL)
        p_wrapper->updateProgress(f_value, text);
}

void DialogHandler::displayError(const QString &title, const QString &text)
{
    ErrorsDialog::getInstance (p_intf)->addError(title, text);
}

void DialogHandler::displayLogin(vlc_dialog_id *p_id, const QString &title,
                                 const QString &text,
                                 const QString &defaultUsername,
                                 bool b_ask_store)
{
    QDialog *dialog = new QDialog();
    QLayout *layout = new QVBoxLayout (dialog);

    dialog->setWindowTitle (title);
    dialog->setWindowRole ("vlc-login");
    dialog->setModal(true);
    layout->setMargin (2);

    /* Username and password fields */
    QWidget *panel = new QWidget (dialog);
    QGridLayout *grid = new QGridLayout;
    grid->addWidget (new QLabel (text), 0, 0, 1, 2);

    QLineEdit *userLine = new QLineEdit;
    if (!defaultUsername.isEmpty())
        userLine->setText(defaultUsername);
    grid->addWidget (new QLabel (qtr("Username")), 1, 0);
    grid->addWidget (userLine, 1, 1);

    QLineEdit *passLine = new QLineEdit;
    passLine->setEchoMode (QLineEdit::Password);
    grid->addWidget (new QLabel (qtr("Password")), 2, 0);
    grid->addWidget (passLine, 2, 1);

    QCheckBox *checkbox = NULL;
    if (b_ask_store)
    {
        checkbox = new QCheckBox;
        checkbox->setChecked (getSettings()->value ("store_password", true).toBool ());
        grid->addWidget (new QLabel (qtr("Store the Password")), 3, 0);
        grid->addWidget (checkbox, 3, 1);
    }

    panel->setLayout (grid);
    layout->addWidget (panel);

    /* focus on passLine if the username is already set */
    if (!defaultUsername.isEmpty())
        passLine->setFocus();

    /* OK, Cancel buttons */
    QDialogButtonBox *buttonBox = new QDialogButtonBox;
    QPushButton *okButton = new QPushButton( "&Ok" );
    QPushButton *cancelButton = new QPushButton( "&Cancel" );
    buttonBox->addButton( okButton, QDialogButtonBox::AcceptRole );
    buttonBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    CONNECT( buttonBox, accepted(), dialog, accept() );
    CONNECT( buttonBox, rejected(), dialog, reject() );
    layout->addWidget (buttonBox);

    dialog->setLayout (layout);
    vlc_dialog_id_set_context(p_id,
        new LoginDialogWrapper(this, p_intf, p_id, dialog, userLine, passLine,
                               checkbox));
    dialog->show();
}

void
DialogHandler::displayQuestion(vlc_dialog_id *p_id, const QString &title,
                               const QString &text, int i_type,
                               const QString &cancel, const QString &action1,
                               const QString &action2)
{
    enum QMessageBox::Icon icon;
    switch (i_type)
    {
        case VLC_DIALOG_QUESTION_WARNING:
            icon = QMessageBox::Warning;
            break;
        case VLC_DIALOG_QUESTION_CRITICAL:
            icon = QMessageBox::Critical;
            break;
        default:
        case VLC_DIALOG_QUESTION_NORMAL:
            icon = action1.isEmpty() && action2.isEmpty() ?
                 QMessageBox::Information : QMessageBox::Question;
            break;
    }
    QMessageBox *box = new QMessageBox (icon, title, text);
    box->addButton ("&" + cancel, QMessageBox::RejectRole);
    box->setModal(true);
    QAbstractButton *action1Button = NULL;
    if (!action1.isEmpty())
        action1Button = box->addButton("&" + action1, QMessageBox::AcceptRole);
    QAbstractButton *action2Button = NULL;
    if (!action2.isEmpty())
        action2Button = box->addButton("&" + action2, QMessageBox::AcceptRole);

    vlc_dialog_id_set_context(p_id,
        new QuestionDialogWrapper(this, p_intf, p_id, box, action1Button,
                                  action2Button));
    box->show();
}

void DialogHandler::displayProgress(vlc_dialog_id *p_id, const QString &title,
                                    const QString &text, bool b_indeterminate,
                                    float f_position, const QString &cancel)
{
    QProgressDialog *progress =
        new QProgressDialog(text, cancel.isEmpty() ? QString() : "&" + cancel,
                            0, b_indeterminate ? 0 : 1000);
    progress->setWindowTitle(title);
    if (cancel.isEmpty())
    {
        /* not cancellable: remove close button */
        progress->setWindowFlags(Qt::Window | Qt::WindowTitleHint |
                                 Qt::CustomizeWindowHint);
    }
    progress->setWindowRole ("vlc-progress");
    progress->setValue(b_indeterminate ? 0 : f_position * 1000);

    vlc_dialog_id_set_context(p_id,
        new ProgressDialogWrapper(this, p_intf, p_id, progress, b_indeterminate));

    progress->show();
}

DialogWrapper::DialogWrapper(DialogHandler *p_handler, intf_thread_t *p_intf,
                             vlc_dialog_id *p_id, QDialog *p_dialog)
    : QObject()
    , p_handler(p_handler)
    , p_intf(p_intf)
    , p_id(p_id)
    , p_dialog(p_dialog)
{
    CONNECT(p_dialog, finished(int), this, finish(int));
}

DialogWrapper::~DialogWrapper()
{
    p_dialog->hide();
    delete p_dialog;
}

void DialogWrapper::finish(int result)
{
    if (result == QDialog::Rejected && p_id != NULL)
    {
        vlc_dialog_id_dismiss(p_id);
        p_id = NULL;
    }
    deleteLater();
}

LoginDialogWrapper::LoginDialogWrapper(DialogHandler *p_handler,
                                       intf_thread_t *p_intf, vlc_dialog_id *p_id,
                                       QDialog *p_dialog, QLineEdit *userLine,
                                       QLineEdit *passLine, QCheckBox *checkbox)
    : DialogWrapper(p_handler, p_intf, p_id, p_dialog)
    , userLine(userLine)
    , passLine(passLine)
    , checkbox(checkbox)
{
    CONNECT(p_dialog, accepted(), this, accept());
}

void LoginDialogWrapper::accept()
{
    if (p_id != NULL)
    {
        vlc_dialog_id_post_login(p_id, qtu(userLine->text ()),
                                 qtu(passLine->text ()),
                                 checkbox != NULL ? checkbox->isChecked () : false);
        p_id = NULL;
        if (checkbox != NULL)
            getSettings()->setValue ("store_password", checkbox->isChecked ());
    }
}

QuestionDialogWrapper::QuestionDialogWrapper(DialogHandler *p_handler,
                                             intf_thread_t *p_intf,
                                             vlc_dialog_id *p_id,
                                             QMessageBox *p_box,
                                             QAbstractButton *action1,
                                             QAbstractButton *action2)
    : DialogWrapper(p_handler, p_intf, p_id, p_box)
    , action1(action1)
    , action2(action2)
{
    CONNECT(p_box, buttonClicked(QAbstractButton *),
            this, buttonClicked(QAbstractButton *));
}

void QuestionDialogWrapper::buttonClicked(QAbstractButton *button)
{
    if (p_id != NULL)
    {
        if (button == action1)
            vlc_dialog_id_post_action(p_id, 1);
        else if (button == action2)
            vlc_dialog_id_post_action(p_id, 2);
        else
            vlc_dialog_id_dismiss(p_id);
        p_id = NULL;
    }
}

ProgressDialogWrapper::ProgressDialogWrapper(DialogHandler *p_handler,
                                             intf_thread_t *p_intf,
                                             vlc_dialog_id *p_id,
                                             QProgressDialog *p_progress,
                                             bool b_indeterminate)
    : DialogWrapper(p_handler, p_intf, p_id, p_progress)
    , b_indeterminate(b_indeterminate)
{
    CONNECT(p_progress, canceled(void), this, finish(void));
}

void ProgressDialogWrapper::updateProgress(float f_position, const QString &text)
{
    QProgressDialog *progress = static_cast<QProgressDialog *>(p_dialog);
    progress->setLabelText(text);
    if (!b_indeterminate)
        progress->setValue(f_position * 1000);
}
