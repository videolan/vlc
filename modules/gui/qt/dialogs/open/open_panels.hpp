/*****************************************************************************
 * open_panels.hpp : Panels for the open dialogs
 ****************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
 * Copyright (C) 2007 Société des arts technologiques
 * Copyright (C) 2007 Savoir-faire Linux
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

#ifndef VLC_QT_OPEN_PANELS_HPP_
#define VLC_QT_OPEN_PANELS_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/preferences/preferences_widgets.hpp"

/* Auto-generated from .ui files */
#include "ui_open_file.h"
#include "ui_open_disk.h"
#include "ui_open_net.h"
#include "ui_open_capture.h"

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
class QEvent;

class OpenPanel: public QWidget
{
    Q_OBJECT
public:
    OpenPanel( QWidget *p, intf_thread_t *_p_intf ) : QWidget( p )
    {
        p_intf = _p_intf;
        context = CONTEXT_INTERACTIVE;
    }
    virtual ~OpenPanel() {};
    virtual void clear() = 0;
    virtual void onFocus() {}
    virtual void onAccept() {}

    static const int CONTEXT_INTERACTIVE = 0;
    static const int CONTEXT_BATCH = 1;

    virtual void updateContext(int c) { context = c; }

protected:
    intf_thread_t *p_intf;
    int context;

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
                 const QUrl &directory, const QString &filter ):
                QFileDialog( parent, caption, "", filter ) {
        setDirectoryUrl(directory);
    }
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
    void clear() Q_DECL_OVERRIDE;
    virtual void accept() ;
protected:
    bool eventFilter(QObject *, QEvent *event) Q_DECL_OVERRIDE
    {
        if( event->type() == QEvent::Hide ||
            event->type() == QEvent::HideToParent )
        {
            event->accept();
            return true;
        }
        return false;
    }
    void dropEvent( QDropEvent *) Q_DECL_OVERRIDE;
    void dragEnterEvent( QDragEnterEvent * ) Q_DECL_OVERRIDE;
    void dragMoveEvent( QDragMoveEvent * ) Q_DECL_OVERRIDE;
    void dragLeaveEvent( QDragLeaveEvent * ) Q_DECL_OVERRIDE;
private:
    Ui::OpenFile ui;
    QList<QUrl> urlList;
    QUrl subUrl; //url for subtitle
    FileOpenBox *dialogBox;
    void BuildOldPanel();
public slots:
    void updateMRL() Q_DECL_OVERRIDE;
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
    void clear()  Q_DECL_OVERRIDE;
    void onFocus() Q_DECL_OVERRIDE;
    void onAccept() Q_DECL_OVERRIDE;
private:
    Ui::OpenNetwork ui;
    bool b_recentList;
public slots:
    void updateMRL() Q_DECL_OVERRIDE;
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
    void clear() Q_DECL_OVERRIDE;
    virtual void accept();
#if defined( _WIN32 ) || defined( __OS2__ )
    virtual void onFocus();
#endif
    virtual void updateContext(int) Q_DECL_OVERRIDE;
private:
    Ui::OpenDisk ui;
    char *psz_dvddiscpath, *psz_vcddiscpath, *psz_cddadiscpath;
    DiscType m_discType;
public slots:
    void updateMRL() Q_DECL_OVERRIDE;
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
    void clear() Q_DECL_OVERRIDE;
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
    void updateMRL() Q_DECL_OVERRIDE;
    void initialize();
private slots:
    void updateButtons();
    void enableAdvancedDialog( int );
    void advancedDialog();
};

#endif
