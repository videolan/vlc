/*****************************************************************************
 * extended_panels.hpp : Exentended Panels
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Antoine Cellerier <dionoea at videolan dot org>
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

#ifndef _EQUALIZER_H_
#define _EQUALIZER_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "ui/equalizer.h"
#include "ui/video_effects.h"

#include <QTabWidget>

#define BANDS 10
#define NUM_CP_CTRL 7
#define NUM_SP_CTRL 5

class QSignalMapper;

class ExtVideo: public QObject
{
    Q_OBJECT
    friend class ExtendedDialog;
public:
    ExtVideo( struct intf_thread_t *, QTabWidget * );
    /*void gotoConf( QObject* );*/
private:
    Ui::ExtVideoWidget ui;
    QSignalMapper* filterMapper;
    intf_thread_t *p_intf;
    void initComboBoxItems( QObject* );
    void setWidgetValue( QObject* );
    void clean();
private slots:
    void updateFilters();
    void updateFilterOptions();
    void cropChange();
    void browseLogo();
    void browseEraseFile();
};

class ExtV4l2 : public QWidget
{
    Q_OBJECT
public:
    ExtV4l2( intf_thread_t *, QWidget * );

    virtual void showEvent( QShowEvent *event );

private:
    intf_thread_t *p_intf;
    QGroupBox *box;
    QLabel *help;

private slots:
    void Refresh( void );
    void ValueChange( int value );
    void ValueChange( bool value );
};

class Equalizer: public QWidget
{
    Q_OBJECT
    friend class ExtendedDialog;
public:
    Equalizer( intf_thread_t *, QWidget * );
    QComboBox *presetsComboBox;

    char * createValuesFromPreset( int i_preset );
    void updateUIFromCore();
    void changeFreqLabels( bool );
private:
    Ui::EqualizerWidget ui;
    QSlider *bands[BANDS];
    QLabel *band_texts[BANDS];
    bool b_vlcBands;

    void delCallbacks( vlc_object_t * );
    void addCallbacks( vlc_object_t * );

    intf_thread_t *p_intf;
    void clean() { enable(); }
private slots:
    void enable(bool);
    void enable();
    void set2Pass();
    void setPreamp();
    void setCoreBands();
    void setCorePreset(int);
};

class Compressor: public QWidget
{
    Q_OBJECT
public:
    Compressor( intf_thread_t *, QWidget * );

private:
    QSlider *compCtrl[NUM_CP_CTRL];
    QLabel *ctrl_texts[NUM_CP_CTRL];
    QLabel *ctrl_readout[NUM_CP_CTRL];
    float controlVars[NUM_CP_CTRL];
    float oldControlVars[NUM_CP_CTRL];

    QCheckBox *enableCheck;

    intf_thread_t *p_intf;

    void delCallbacks( vlc_object_t * );
    void addCallbacks( vlc_object_t * );

    void updateSliders(float *);
    void setValues();

private slots:
    void enable(bool);
    void enable();
    void setInitValues();
};

class Spatializer: public QWidget
{
    Q_OBJECT
public:
    Spatializer( intf_thread_t *, QWidget * );

private:
    QSlider *spatCtrl[NUM_SP_CTRL];
    QLabel *ctrl_texts[NUM_SP_CTRL];
    QLabel *ctrl_readout[NUM_SP_CTRL];
    float controlVars[5];
    float oldControlVars[5];

    QCheckBox *enableCheck;

    void delCallbacks( vlc_object_t * );
    void addCallbacks( vlc_object_t * );
    intf_thread_t *p_intf;

    void setValues();

private slots:
    void enable(bool);
    void enable();
    void setInitValues();
};

class SyncWidget : public QWidget
{
    Q_OBJECT
public:
    SyncWidget( QWidget * );
    void setValue( double d );
signals:
    void valueChanged( double );
private slots:
    void valueChangedHandler( double d );
private:
    QDoubleSpinBox spinBox;
    QLabel spinLabel;
};

class SyncControls : public QWidget
{
    Q_OBJECT
    friend class ExtendedDialog;
public:
    SyncControls( intf_thread_t *, QWidget * );
    virtual ~SyncControls();
private:
    intf_thread_t *p_intf;
    SyncWidget *AVSpin;
    SyncWidget *subsSpin;
    QDoubleSpinBox *subSpeedSpin;
    QDoubleSpinBox *subDurationSpin;

    bool b_userAction;

    void clean();

    void initSubsDuration();
    void subsdelayClean();
    void subsdelaySetFactor( double );
public slots:
    void update();
private slots:
    void advanceAudio( double );
    void advanceSubs( double );
    void adjustSubsSpeed( double );
    void adjustSubsDuration( double );
};

#endif
