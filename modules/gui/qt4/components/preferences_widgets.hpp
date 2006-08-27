/*****************************************************************************
 * preferences_widgets.hpp : Widgets for preferences panels
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
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
#include <QLineEdit>
#include "ui/input_stats.h"
#include "qt4.hpp"
#include <assert.h>

class QSpinBox;
class QString;
class QComboBox;
class QCheckBox;

class ConfigControl : public QObject
{
    Q_OBJECT;
public:
    ConfigControl( vlc_object_t *_p_this, module_config_t *_p_conf,
                   QWidget *p ) : p_this( _p_this ), p_item( _p_conf )
    {
        widget = new QWidget( p );
    }
    ConfigControl( vlc_object_t *_p_this, module_config_t *_p_conf ) :
                            p_this (_p_this ), p_item( _p_conf )
    {
        widget = NULL;
    }
    virtual ~ConfigControl() {};
    QString getName() { return qfu( p_item->psz_name ); }
    QWidget *getWidget() { assert( widget ); return widget; }
    bool isAdvanced() { return p_item->b_advanced; }

    static ConfigControl * createControl( vlc_object_t*,
                                          module_config_t*,QWidget* );
protected:
    vlc_object_t *p_this;
    module_config_t *p_item;
    QString _name;
    QWidget *widget;
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
    VIntConfigControl( vlc_object_t *a, module_config_t *b ) :
                ConfigControl(a,b) {};
    virtual ~VIntConfigControl() {};
    virtual int getValue() = 0;
};

class IntegerConfigControl : public VIntConfigControl
{
public:
    IntegerConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    IntegerConfigControl( vlc_object_t *, module_config_t *,
                          QLabel*, QSpinBox* );
    virtual ~IntegerConfigControl() {};
    virtual int getValue();
private:
    QSpinBox *spin;
    void finish( QLabel * );
};

#if 0
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
    VFloatConfigControl( vlc_object_t *a, module_config_t *b ) :
                ConfigControl(a,b) {};
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
    VStringConfigControl( vlc_object_t *a, module_config_t *b ) :
                ConfigControl(a,b) {};
    virtual ~VStringConfigControl() {};
    virtual QString getValue() = 0;
};

class StringConfigControl : public VStringConfigControl
{
public:
    StringConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                         bool pwd );
    StringConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                         QLineEdit*,  bool pwd );
    virtual ~StringConfigControl() {};
    virtual QString getValue() { return text->text(); };
private:
    void finish( QLabel * );
    QLineEdit *text;
};

class ModuleConfigControl : public VStringConfigControl
{
public:
    ModuleConfigControl( vlc_object_t *, module_config_t *, QWidget *, bool
                         bycat );
    ModuleConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                         QComboBox*, bool );
    virtual ~ModuleConfigControl() {};
    virtual QString getValue();
private:
    void finish( QLabel *, bool );
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
