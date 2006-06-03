/*****************************************************************************
 * preferences_widgets.hpp : Widgets for preferences panels
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
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

#ifndef _INFOPANELS_H_
#define _INFOPANELS_H_
#include <vlc/vlc.h>
#include <QWidget>
#include "ui/input_stats.h"

class QSpinBox;
class QLineEdit;
class QString;
class QComboBox;
class QCheckBox;

class ConfigControl : public QWidget
{
    Q_OBJECT;
public:
    ConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    virtual ~ConfigControl();
    QString getName() { return _name; }
    bool isAdvanced() { return _advanced; }

    static ConfigControl * createControl( vlc_object_t*,
                                         module_config_t*,QWidget* );
protected:
    vlc_object_t *p_this;
    QString _name;
    bool _advanced;
signals:
    void Updated();
};

/*******************************************************
 * Integer-based controls
 *******************************************************/
class VIntConfigControl : public ConfigControl
{
public:
    VIntConfigControl( vlc_object_t *a, module_config_t *b, QWidget *c ) :
            ConfigControl(a,b,c) {};
    virtual ~VIntConfigControl() {};
    virtual int getValue() = 0;
};

#if 0
class IntegerConfigControl : public VIntConfigControl
{
public:
    IntegerConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    virtual ~IntegerConfigControl();
    virtual int getValue();
private:
    QSpinBox *spin;
};

class BoolConfigControl : public VIntConfigControl
{
public:
    IntConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    virtual ~IntConfigControl();
    virtual int getValue();
private:
    wxCheckBox *checkbox;
};
#endif

/*******************************************************
 * Float-based controls
 *******************************************************/
class VFloatConfigControl : public ConfigControl
{
public:
    VFloatConfigControl( vlc_object_t *a, module_config_t *b, QWidget *c ) :
                ConfigControl(a,b,c) {};
    virtual ~VFloatConfigControl() {};
    virtual float getValue() = 0;
};

#if 0
class FloatConfigControl : public VFloatConfigControl
{
public:
    FloatConfigControl( vlc_object_t *a, module_config_t *b, QWidget *c ) :
                ConfigControl(a,b,c) {};
    virtual ~FloatConfigControl() {};
    virtual float getValue();
private:
    QDoubleSpinBox *spin;
};
#endif

/*******************************************************
 * String-based controls
 *******************************************************/
class VStringConfigControl : public ConfigControl
{
public:
    VStringConfigControl( vlc_object_t *a, module_config_t *b, QWidget *c ) : 
                ConfigControl(a,b,c) {};
    virtual ~VStringConfigControl() {};
    virtual QString getValue() = 0;
};

class StringConfigControl : public VStringConfigControl
{
public:
    StringConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                         bool pwd );
    virtual ~StringConfigControl();
    virtual QString getValue();
private:
    QLineEdit *text;
};

class ModuleConfigControl : public VStringConfigControl
{
public:
    ModuleConfigControl( vlc_object_t *, module_config_t *, QWidget *, bool
                         bycat );
    virtual ~ModuleConfigControl();
    virtual QString getValue();
private:
    QComboBox *combo;
};
#if 0
struct ModuleCheckBox {
    QCheckBox *checkbox;
    QString module;
};

class ModuleListConfigControl : public ConfigControl
{
public:
    StringConfigControl( vlc_object_t *, module_config_t *, QWidget *, bool
                         bycat );
    virtual ~StringConfigControl();
    virtual QString getValue();
private:
    std::vector<ModuleCheckBox> checkboxes;
    QLineEdit *text;
private slot:
    void OnUpdate();
};
#endif

#endif
