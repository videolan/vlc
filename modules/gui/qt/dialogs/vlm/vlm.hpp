/*****************************************************************************
 * vlm.hpp : VLM Management
 ****************************************************************************
 * Copyright ( C ) 2006 the VideoLAN team
 *
 * Authors: Jean-François Massol <jf.massol@gmail.com>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#ifndef QVLC_VLM_DIALOG_H_
#define QVLC_VLM_DIALOG_H_ 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_vlm.h>

/* Auto-generated from .ui files */
#include "ui_vlm.h"

#include "widgets/native/qvlcframe.hpp"
#include "util/singleton.hpp"
#include <QDateTime>

enum{
    QVLM_Broadcast,
    QVLM_Schedule,
};

enum{
    ControlBroadcastPlay,
    ControlBroadcastPause,
    ControlBroadcastStop,
    ControlBroadcastSeek
};

class QComboBox;
class QVBoxLayout;
class QLabel;
class QGridLayout;
class QToolButton;
class QGroupBox;
class QDateTimeEdit;
class QSpinBox;
class VLMAWidget;
class VLMWrapper;


class VLMDialog : public QVLCFrame, public Singleton<VLMDialog>
{
    Q_OBJECT

public:
    void toggleVisible();

private:
    VLMWrapper *vlm;

    VLMDialog( intf_thread_t * );
    virtual ~VLMDialog();

    Ui::Vlm ui;

    QString inputOptions;

    QList<VLMAWidget *> vlmItems;
    int currentIndex;

    QVBoxLayout *vlmItemLayout;
    QWidget *vlmItemWidget;

    QComboBox *mediatype;
    QDateTimeEdit *time, *date, *repeatTime;
    QSpinBox *scherepeatnumber, *repeatDays;
    bool isNameGenuine( const QString& );
    void mediasPopulator();
public slots:
    void removeVLMItem( VLMAWidget * );
    void startModifyVLMItem( VLMAWidget * );
private slots:
    void addVLMItem();
    void clearWidgets();
    void saveModifications();
    void showScheduleWidget( int );
    void selectVLMItem( int );
    void selectInput();
    void selectOutput();
    bool exportVLMConf();
    bool importVLMConf();

    friend class    Singleton<VLMDialog>;
};

class VLMWrapper
{
public:
    VLMWrapper( vlm_t * );
    virtual ~VLMWrapper();

    int GetMedias( vlm_media_t **& );

    void AddBroadcast( const QString&, const QString&,
                       const QString&, const QString&,
                       bool b_enabled = true, bool b_loop = false );
    void EditBroadcast( const QString&, const QString&,
                        const QString&, const QString&,
                        bool b_enabled = true, bool b_loop = false );
    void EditSchedule( const QString&, const QString&,
                       const QString&, const QString&,
                       QDateTime _schetime, QDateTime _schedate,
                       int _scherepeatnumber, int _repeatDays,
                       bool b_enabled = true, const QString& mux = "" );
    void AddSchedule( const QString&, const QString&,
                      const QString&, const QString&,
                      QDateTime _schetime, QDateTime _schedate,
                      int _scherepeatnumber, int _repeatDays,
                      bool b_enabled = true, const QString& mux = "" );

    void ControlBroadcast( const QString&, int, unsigned int seek = 0 );
    void EnableItem( const QString&, bool );

    void SaveConfig( const QString& );
    bool LoadConfig( const QString& );

private:
    vlm_t *p_vlm;
};

class VLMAWidget : public QGroupBox
{
    Q_OBJECT
    friend class VLMDialog;
public:
    VLMAWidget( VLMWrapper *, const QString& name, const QString& input,
                const QString& inputOptions, const QString& output,
                bool _enable, VLMDialog *parent, int _type = QVLM_Broadcast );
    virtual void update() = 0;
protected:
    QLabel *nameLabel;
    QString name;
    QString input;
    QString inputOptions;
    QString output;
    bool b_enabled;
    int type;
    VLMDialog *parent;
    VLMWrapper *vlm;
    QGridLayout *objLayout;
private slots:
    virtual void modify();
    virtual void del();
    virtual void toggleEnabled( bool );
};

class VLMBroadcast : public VLMAWidget
{
    Q_OBJECT
    friend class VLMDialog;
public:
    VLMBroadcast( VLMWrapper *, const QString& name, const QString& input,
                  const QString& inputOptions, const QString& output,
                  bool _enable, bool _loop, VLMDialog *parent );
    void update() Q_DECL_OVERRIDE;
private:
    bool b_looped;
    bool b_playing;
    QToolButton *loopButton, *playButton;
private slots:
    void stop();
    void togglePlayPause();
    void toggleLoop();
};

class VLMSchedule : public VLMAWidget
{
    Q_OBJECT
    friend class VLMDialog;
public:
    VLMSchedule( VLMWrapper *, const QString& name, const QString& input,
                 const QString& inputOptions, const QString& output,
                 QDateTime schetime, QDateTime schedate, int repeatnumber,
                 int repeatdays, bool enabled, VLMDialog *parent );
    void update();
private:
    QDateTime schetime;
    QDateTime schedate;
    int rNumber;
    int rDays;
};

#endif
