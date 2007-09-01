/*****************************************************************************
 * extended_panels.hpp : Exentended Panels
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: preferences.hpp 16643 2006-09-13 12:45:46Z zorglub $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include <vlc/vlc.h>
#include <vlc_aout.h>

#include "ui/equalizer.h"
#include "ui/video_effects.h"
#include "ui/spatializer.h"

#define BANDS 10
#define NUM_SP_CTRL 5

class QSignalMapper;

class ExtVideo: public QWidget
{
    Q_OBJECT
public:
    ExtVideo( intf_thread_t *, QWidget * );
    virtual ~ExtVideo();
    /*void gotoConf( QObject* );*/
private:
    Ui::ExtVideoWidget ui;
    QSignalMapper* filterMapper;
    intf_thread_t *p_intf;
    void initComboBoxItems( QObject* );
    void setWidgetValue( QObject* );
    void ChangeVFiltersString( char *psz_name, vlc_bool_t b_add );
private slots:
    void updateFilters();
    void updateFilterOptions();
};

class Equalizer: public QWidget
{
    Q_OBJECT
public:
    Equalizer( intf_thread_t *, QWidget * );
    virtual ~Equalizer();

private:
    Ui::EqualizerWidget ui;
    QSlider *bands[BANDS];
    QLabel *band_texts[BANDS];

    void delCallbacks( aout_instance_t * );
    void addCallbacks( aout_instance_t * );
    void setValues( char *, float );

    intf_thread_t *p_intf;
private slots:
    void enable(bool);
    void enable();
    void set2Pass();
    void setPreamp();
    void setBand();
    void setPreset(int);
};

class Spatializer: public QWidget
{
    Q_OBJECT
public:
    Spatializer( intf_thread_t *, QWidget * );
    virtual ~Spatializer();

private:
    Ui::SpatializerWidget ui;
    QSlider *spatCtrl[NUM_SP_CTRL];
    QLabel *ctrl_texts[NUM_SP_CTRL];
    QLabel *ctrl_readout[NUM_SP_CTRL];
    float controlVars[5];
    float oldControlVars[5];

    void delCallbacks( aout_instance_t * );
    void addCallbacks( aout_instance_t * );
    intf_thread_t *p_intf;
private slots:
    void enable(bool);
    void enable();
    void setValues(float *);
    void setInitValues();
};

class ExtendedControls: public QWidget
{
    Q_OBJECT
public:
    ExtendedControls( intf_thread_t *, QWidget * ) {};
    virtual ~ExtendedControls() {};

private:
    intf_thread_t *p_intf;
private slots:
    void slower() {};
    void faster() {};
    void normal() {};
    void snapshot() {};
};

#endif
