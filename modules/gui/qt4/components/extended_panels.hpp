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

class FilterSliderData : public QObject
{
    Q_OBJECT

public:
    typedef struct
    {
        QString name;
        QString descs;
        QString units;
        float f_min;      // min
        float f_max;      // max
        float f_value;    // value
        float f_resolution; // resolution
    } slider_data_t;
    FilterSliderData( QObject *parent, intf_thread_t *p_intf,
                      QSlider *slider,
                      QLabel *valueLabel, QLabel *nameLabel,
                      const slider_data_t *p_data );
    void setValue( float f );

protected:
    float initialValue();

public slots:
    void onValueChanged( int i );
    void updateText( int i );
    void writeToConfig();

private:
    intf_thread_t *p_intf;
    QSlider *slider;
    QLabel *valueLabel;
    QLabel *nameLabel;
    const slider_data_t *p_data;
};

class AudioFilterControlWidget : public QWidget
{
    Q_OBJECT

public:
    AudioFilterControlWidget( intf_thread_t *, QWidget *, const char *name );
    virtual ~AudioFilterControlWidget();

protected:
    virtual void build();
    QVector<FilterSliderData::slider_data_t> controls;
    QGroupBox *slidersBox;
    intf_thread_t *p_intf;
    QString name; // filter's module name

protected slots:
    void enable();
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

class Compressor: public AudioFilterControlWidget
{
    Q_OBJECT

public:
    Compressor( intf_thread_t *, QWidget * );
};

class Spatializer: public QWidget
{
    Q_OBJECT

public:
    Spatializer( intf_thread_t *, QWidget * );

private:
    typedef struct
    {
        const char *psz_name;
        const char *psz_desc;
    } spat_controls_t;
    static const spat_controls_t spat_controls[NUM_SP_CTRL];
    QSlider *spatCtrl[NUM_SP_CTRL];
    QLabel *ctrl_texts[NUM_SP_CTRL];
    QLabel *ctrl_readout[NUM_SP_CTRL];
    float controlVars[NUM_SP_CTRL];
    float oldControlVars[NUM_SP_CTRL];

    QGroupBox *spatializerBox;

    void delCallbacks( vlc_object_t * );
    void addCallbacks( vlc_object_t * );
    intf_thread_t *p_intf;

private slots:
    void enable();
    void setValues();
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
