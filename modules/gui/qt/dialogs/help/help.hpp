/*****************************************************************************
 * help.hpp : Help and About dialogs
 ****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
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

#ifndef QVLC_HELP_DIALOG_H_
#define QVLC_HELP_DIALOG_H_ 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"

#include "widgets/native/qvlcframe.hpp"
#include "util/singleton.hpp"

/* Auto-generated from .ui files */
#include "ui_about.h"
#include "ui_update.h"

class QEvent;

class HelpDialog : public QVLCFrame, public Singleton<HelpDialog>
{
    Q_OBJECT
private:
    HelpDialog( intf_thread_t * );
    virtual ~HelpDialog();

public slots:
    void close() Q_DECL_OVERRIDE { toggleVisible(); }

    friend class    Singleton<HelpDialog>;
};

class AboutDialog : public QVLCDialog, public Singleton<AboutDialog>
{
    Q_OBJECT
private:
    AboutDialog( intf_thread_t * );
    Ui::aboutWidget ui;

public slots:
    friend class    Singleton<AboutDialog>;

protected:
    virtual bool eventFilter(QObject *obj, QEvent *event) Q_DECL_OVERRIDE;
    virtual void showEvent ( QShowEvent * ) Q_DECL_OVERRIDE;

private:
    bool b_advanced;

private slots:
    void showLicense();
    void showAuthors();
    void showCredit();
};

#ifdef UPDATE_CHECK

class UpdateDialog : public QVLCFrame, public Singleton<UpdateDialog>
{
    Q_OBJECT
public:
    static const QEvent::Type UDOkEvent;
    static const QEvent::Type UDErrorEvent;
    void updateNotify( bool );

private:
    UpdateDialog( intf_thread_t * );
    virtual ~UpdateDialog();

    Ui::updateWidget ui;
    update_t *p_update;
    void customEvent( QEvent * );
    bool b_checked;

private slots:
    void close() Q_DECL_OVERRIDE { toggleVisible(); }

    void UpdateOrDownload();

    friend class    Singleton<UpdateDialog>;
};
#endif

#endif
