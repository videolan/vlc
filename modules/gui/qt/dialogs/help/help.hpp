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

class HelpDialog : public QVLCFrame
{
    Q_OBJECT
public:
    HelpDialog( qt_intf_t * );
    virtual ~HelpDialog();

public slots:
    void close() override { toggleVisible(); }
};

class AboutDialog : public QVLCDialog
{
    Q_OBJECT
public:
    AboutDialog( qt_intf_t * );

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void showEvent ( QShowEvent * ) override;

private:
    bool b_advanced;
    Ui::aboutWidget ui;

private slots:
    void showLicense();
    void showAuthors();
    void showCredit();
};

#if defined(UPDATE_CHECK)

class UpdateModelPrivate;
class UpdateModel : public QObject
{
    Q_OBJECT

public:
    enum Status {
        Unchecked,
        Checking,
        UpToDate,
        NeedUpdate,
        CheckFailed
    };
    Q_ENUM(Status)

    Q_PROPERTY(Status updateStatus READ updateStatus NOTIFY updateStatusChanged FINAL)
    Q_PROPERTY(int major READ getMajor NOTIFY updateStatusChanged FINAL)
    Q_PROPERTY(int minor READ getMinor NOTIFY updateStatusChanged FINAL)
    Q_PROPERTY(int revision READ getRevision NOTIFY updateStatusChanged FINAL)
    Q_PROPERTY(int extra READ getExtra NOTIFY updateStatusChanged FINAL)
    Q_PROPERTY(QString description READ getDescription NOTIFY updateStatusChanged FINAL)
    Q_PROPERTY(QString url READ getUrl NOTIFY updateStatusChanged FINAL)

public:
    explicit UpdateModel(qt_intf_t * p_intf);
    ~UpdateModel();

    Q_INVOKABLE void checkUpdate();

    Q_INVOKABLE bool download(QString destDir);

    Status updateStatus() const;
    int getMajor() const;
    int getMinor() const;
    int getRevision() const;
    int getExtra() const;
    QString getDescription() const;
    QString getUrl() const;

signals:
    void updateStatusChanged();

private:
    Q_DECLARE_PRIVATE(UpdateModel)
    QScopedPointer<UpdateModelPrivate> d_ptr;
};

class UpdateDialog : public QVLCFrame
{
    Q_OBJECT
public:
    UpdateDialog( qt_intf_t * );
    virtual ~UpdateDialog();

private:
    Ui::updateWidget ui;
    UpdateModel* m_model = nullptr;

private slots:
    void checkOrDownload();
    void updateUI();
    void close() override { toggleVisible(); }
};
#endif

#endif
