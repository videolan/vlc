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
#include <vlc_aout.h>

#include "ui/equalizer.h"
#include "ui/video_effects.h"
#include "ui/v4l2.h"

#include <QTabWidget>

#define BANDS 10
#define NUM_SP_CTRL 5

class QSignalMapper;

class ExtVideo: public QObject
{
    Q_OBJECT
    friend class ExtendedDialog;
public:
    ExtVideo( intf_thread_t *, QTabWidget * );
    virtual ~ExtVideo();
    /*void gotoConf( QObject* );*/
private:
    Ui::ExtVideoWidget ui;
    QSignalMapper* filterMapper;
    intf_thread_t *p_intf;
    vout_thread_t *p_vout;
    void initComboBoxItems( QObject* );
    void setWidgetValue( QObject* );
    void ChangeVFiltersString( const char *psz_name, bool b_add );
    void clean();
private slots:
    void updateFilters();
    void updateFilterOptions();
    void cropChange();
};

class ExtV4l2 : public QWidget
{
    Q_OBJECT
public:
    ExtV4l2( intf_thread_t *, QWidget * );
    virtual ~ExtV4l2();

    virtual void showEvent( QShowEvent *event );

private:
    intf_thread_t *p_intf;
    Ui::ExtV4l2Widget ui;
    QGroupBox *box;

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
    virtual ~Equalizer();
    QComboBox *presetsComboBox;

    char * createValuesFromPreset( int i_preset );
    void updateUIFromCore();
private:
    Ui::EqualizerWidget ui;
    QSlider *bands[BANDS];
    QLabel *band_texts[BANDS];

    void delCallbacks( aout_instance_t * );
    void addCallbacks( aout_instance_t * );

    intf_thread_t *p_intf;
    void clean();
private slots:
    void enable(bool);
    void enable();
    void set2Pass();
    void setPreamp();
    void setCoreBands();
    void setCorePreset(int);
    void updateUISliderValues( int );
};

class Spatializer: public QWidget
{
    Q_OBJECT
public:
    Spatializer( intf_thread_t *, QWidget * );
    virtual ~Spatializer();

private:
    QSlider *spatCtrl[NUM_SP_CTRL];
    QLabel *ctrl_texts[NUM_SP_CTRL];
    QLabel *ctrl_readout[NUM_SP_CTRL];
    float controlVars[5];
    float oldControlVars[5];

    QCheckBox *enableCheck;

    void delCallbacks( aout_instance_t * );
    void addCallbacks( aout_instance_t * );
    intf_thread_t *p_intf;
private slots:
    void enable(bool);
    void enable();
    void setValues(float *);
    void setInitValues();
};

class SyncControls : public QWidget
{
    Q_OBJECT
    friend class ExtendedDialog;
public:
    SyncControls( intf_thread_t *, QWidget * );
    virtual ~SyncControls() {};
private:
    intf_thread_t *p_intf;
    QDoubleSpinBox *AVSpin;
    QDoubleSpinBox *subsSpin;
    QDoubleSpinBox *subSpeedSpin;

    bool b_userAction;

    void clean();
public slots:
    void update();
private slots:
    void advanceAudio( double );
    void advanceSubs( double );
    void adjustSubsSpeed( double );
};

#endif
