/*****************************************************************************
 * open_panels.hpp : Panels for the open dialogs
 ****************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
 * Copyright (C) 2007 Société des arts technologiques
 * Copyright (C) 2007 Savoir-faire Linux
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

#ifndef _OPENPANELS_H_
#define _OPENPANELS_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "components/preferences_widgets.hpp"

#include "ui/open_file.h"
#include "ui/open_disk.h"
#include "ui/open_net.h"
#include "ui/open_capture.h"

#include <QFileDialog>

#include <limits.h>

#define setSpinBoxFreq( spinbox ){ spinbox->setRange ( 0, INT_MAX ); \
    spinbox->setAccelerated( true ); }

enum
{
    V4L2_DEVICE,
    PVR_DEVICE,
    DTV_DEVICE,
    DSHOW_DEVICE,
    SCREEN_DEVICE,
    JACK_DEVICE
};

class QWidget;
class QLineEdit;
class QString;
class QStringListModel;
class QEvent;

class OpenPanel: public QWidget
{
    Q_OBJECT
public:
    OpenPanel( QWidget *p, intf_thread_t *_p_intf ) : QWidget( p )
    {
        p_intf = _p_intf;
    }
    virtual ~OpenPanel() {};
    virtual void clear() = 0;
    virtual void onFocus() {}
    virtual void onAccept() {}
protected:
    intf_thread_t *p_intf;
public slots:
    virtual void updateMRL() = 0;
signals:
    void mrlUpdated( const QStringList&, const QString& );
    void methodChanged( const QString& method );
};

class FileOpenBox: public QFileDialog
{
    Q_OBJECT
public:
    FileOpenBox( QWidget *parent, const QString &caption,
                 const QString &directory, const QString &filter ):
                QFileDialog( parent, caption, directory, filter ) {}
public slots:
    void accept(){}
    void reject(){}
};


class FileOpenPanel: public OpenPanel
{
    Q_OBJECT
public:
    FileOpenPanel( QWidget *, intf_thread_t * );
    virtual ~FileOpenPanel();
    virtual void clear() ;
    virtual void accept() ;
protected:
    bool eventFilter(QObject *, QEvent *event)
    {
        if( event->type() == QEvent::Hide ||
            event->type() == QEvent::HideToParent )
        {
            event->accept();
            return true;
        }
        return false;
    }
    virtual void dropEvent( QDropEvent *);
    virtual void dragEnterEvent( QDragEnterEvent * );
    virtual void dragMoveEvent( QDragMoveEvent * );
    virtual void dragLeaveEvent( QDragLeaveEvent * );
private:
    Ui::OpenFile ui;
    FileOpenBox *dialogBox;
    void BuildOldPanel();
public slots:
    virtual void updateMRL();
private slots:
    void browseFileSub();
    void browseFile();
    void removeFile();
    void updateButtons();
};

class NetOpenPanel: public OpenPanel
{
    Q_OBJECT
public:
    NetOpenPanel( QWidget *, intf_thread_t * );
    virtual ~NetOpenPanel();
    virtual void clear() ;
    virtual void onFocus();
    virtual void onAccept();
private:
    Ui::OpenNetwork ui;
    bool b_recentList;
public slots:
    virtual void updateMRL();
};

class UrlValidator : public QValidator
{
   Q_OBJECT
public:
   UrlValidator( QObject *parent ) : QValidator( parent ) { }
   virtual QValidator::State validate( QString&, int& ) const;
};

class DiscOpenPanel: public OpenPanel
{
    Q_OBJECT
    enum    DiscType
    {
        None,
        Dvd,
        Vcd,
        Cdda,
        BRD
    };
public:
    DiscOpenPanel( QWidget *, intf_thread_t * );
    virtual ~DiscOpenPanel();
    virtual void clear() ;
    virtual void accept() ;
#if defined( _WIN32 ) || defined( __OS2__ )
    virtual void onFocus();
#endif
private:
    Ui::OpenDisk ui;
    char *psz_dvddiscpath, *psz_vcddiscpath, *psz_cddadiscpath;
    DiscType m_discType;
public slots:
    virtual void updateMRL() ;
private slots:
    void browseDevice();
    void updateButtons() ;
    void eject();
};


class CaptureOpenPanel: public OpenPanel
{
    Q_OBJECT
public:
    CaptureOpenPanel( QWidget *, intf_thread_t * );
    virtual ~CaptureOpenPanel();
    virtual void clear() ;
private:
    Ui::OpenCapture ui;
    bool isInitialized;

    QString advMRL;
    QStringList configList;
    QDialog *adv;
#ifdef _WIN32
    StringListConfigControl *vdevDshowW, *adevDshowW;
    QLineEdit *dshowVSizeLine;
#else
    QSpinBox  *pvrFreq;
    QComboBox *v4l2VideoDevice, *v4l2AudioDevice;
    QComboBox *pvrDevice, *pvrAudioDevice;
    QComboBox *v4l2StdBox, *pvrNormBox;
    QSpinBox *jackChannels;
    QCheckBox *jackPace, *jackConnect;
    QLineEdit *jackPortsSelected;
#endif
    QRadioButton *dvbc, *dvbs, *dvbs2, *dvbt, *dvbt2, *atsc, *cqam;
    QLabel *dvbBandLabel, *dvbSrateLabel, *dvbModLabel;
    QComboBox *dvbQamBox, *dvbPskBox, *dvbBandBox;
    QSpinBox *dvbCard, *dvbFreq, *dvbSrate;
    QDoubleSpinBox *screenFPS;

public slots:
    virtual void updateMRL();
    void initialize();
private slots:
    void updateButtons();
    void enableAdvancedDialog( int );
    void advancedDialog();
};

#endif
