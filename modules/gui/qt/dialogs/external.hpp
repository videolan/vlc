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

#ifndef QVLC_DIALOGS_EXTERNAL_H_
#define QVLC_DIALOGS_EXTERNAL_H_ 1

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <QObject>
#include <QDialog>
#include <QMap>
#include <vlc_common.h>
#include <vlc_dialog.h>
#include "adapters/variables.hpp"

struct intf_thread_t;
class QProgressDialog;
class DialogWrapper;

class DialogHandler : public QObject
{
    Q_OBJECT

public:
    DialogHandler (intf_thread_t *, QObject *parent);
    virtual ~DialogHandler();
    void removeDialogId(vlc_dialog_id *p_id);

signals:
    void errorDisplayed(const QString &title, const QString &text);
    void loginDisplayed(vlc_dialog_id *p_id, const QString &title,
                        const QString &text, const QString &defaultUsername,
                        bool b_ask_store);
    void questionDisplayed(vlc_dialog_id *p_id, const QString &title,
                           const QString &text, int i_type,
                           const QString &cancel, const QString &action1,
                           const QString &action2);
    void progressDisplayed(vlc_dialog_id *p_id, const QString &title,
                           const QString &text, bool b_indeterminate,
                           float f_position, const QString &cancel);
    void cancelled(vlc_dialog_id *p_id);
    void progressUpdated(vlc_dialog_id *p_id, float f_value, const QString &text);

private slots:
    void displayError(const QString &title, const QString &text);
    void displayLogin(vlc_dialog_id *p_id, const QString &title,
                      const QString &text, const QString &defaultUsername,
                      bool b_ask_store);
    void displayQuestion(vlc_dialog_id *p_id, const QString &title,
                         const QString &text, int i_type,
                         const QString &cancel, const QString &action1,
                         const QString &action2);
    void displayProgress(vlc_dialog_id *p_id, const QString &title,
                         const QString &text, bool b_indeterminate,
                         float f_position, const QString &cancel);
    void cancel(vlc_dialog_id *p_id);
    void updateProgress(vlc_dialog_id *p_id, float f_value, const QString &text);

private:
    intf_thread_t *p_intf;

    static void displayErrorCb(void *, const char *, const char *);
    static void displayLoginCb(void *, vlc_dialog_id *, const char *,
                               const char *, const char *, bool);
    static void displayQuestionCb(void *, vlc_dialog_id *, const char *,
                                  const char *, vlc_dialog_question_type,
                                  const char *, const char *, const char *);
    static void displayProgressCb(void *, vlc_dialog_id *, const char *,
                                  const char *, bool, float, const char *);
    static void cancelCb(void *, vlc_dialog_id *);
    static void updateProgressCb(void *, vlc_dialog_id *, float, const char *);
};

class DialogWrapper : public QObject
{
    Q_OBJECT

    friend class DialogHandler;
public:
    DialogWrapper(DialogHandler *p_handler, intf_thread_t *p_intf,
                 vlc_dialog_id *p_id, QDialog *p_dialog);
    virtual ~DialogWrapper();
protected slots:
    virtual void finish(int result = QDialog::Rejected);
protected:
    DialogHandler *p_handler;
    intf_thread_t *p_intf;
    vlc_dialog_id *p_id;
    QDialog *p_dialog;
};

class QLineEdit;
class QCheckBox;
class LoginDialogWrapper : public DialogWrapper
{
    Q_OBJECT
public:
    LoginDialogWrapper(DialogHandler *p_handler, intf_thread_t *p_intf,
                       vlc_dialog_id *p_id, QDialog *p_dialog,
                       QLineEdit *userLine, QLineEdit *passLine,
                       QCheckBox *checkbox);
private slots:
    virtual void accept();
private:
    QLineEdit *userLine;
    QLineEdit *passLine;
    QCheckBox *checkbox;
};

class QAbstractButton;
class QMessageBox;
class QuestionDialogWrapper : public DialogWrapper
{
    Q_OBJECT
public:
    QuestionDialogWrapper(DialogHandler *p_handler, intf_thread_t *p_intf,
                          vlc_dialog_id *p_id, QMessageBox *p_box,
                          QAbstractButton *action1, QAbstractButton *action2);
private slots:
    virtual void buttonClicked(QAbstractButton *);
private:
    QAbstractButton *action1;
    QAbstractButton *action2;
};

class ProgressDialogWrapper : public DialogWrapper
{
    Q_OBJECT
public:
    ProgressDialogWrapper(DialogHandler *p_handler, intf_thread_t *p_intf,
                          vlc_dialog_id *p_id, QProgressDialog  *p_progress,
                          bool b_indeterminate);
    void updateProgress(float f_position, const QString &text);
private:
    bool b_indeterminate;
};

#endif
