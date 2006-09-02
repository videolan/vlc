/*****************************************************************************
 * preferences_widgets.hpp : Widgets for preferences panels
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Antoine Cellerier <dionoea@videolan.org>
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
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include "ui/input_stats.h"
#include "qt4.hpp"
#include <assert.h>

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
    virtual int getType() = 0;
    char * getName() { return  p_item->psz_name; }
    QWidget *getWidget() { assert( widget ); return widget; }
    bool isAdvanced() { return p_item->b_advanced; }
    virtual void hide() { getWidget()->hide(); };
    virtual void show() { getWidget()->show(); };

    static ConfigControl * createControl( vlc_object_t*,
                                          module_config_t*,QWidget* );
    static ConfigControl * createControl( vlc_object_t*,
                                          module_config_t*,QWidget*,
                                          QGridLayout *, int);
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
    virtual int getType() { return 1; }
};

class IntegerConfigControl : public VIntConfigControl
{
public:
    IntegerConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                          QGridLayout *, int );
    IntegerConfigControl( vlc_object_t *, module_config_t *,
                          QLabel*, QSpinBox* );
    virtual ~IntegerConfigControl() {};
    virtual int getValue();
    virtual void show() { spin->show(); label->show(); }
    virtual void hide() { spin->hide(); label->hide(); }

protected:
    QSpinBox *spin;

private:
    QLabel *label;
    void finish();
};

class IntegerRangeConfigControl : public IntegerConfigControl
{
public:
    IntegerRangeConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                               QGridLayout *, int );
    IntegerRangeConfigControl( vlc_object_t *, module_config_t *,
                               QLabel*, QSpinBox* );
private:
    void finish();
};

class IntegerListConfigControl : public VIntConfigControl
{
public:
    IntegerListConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                              bool, QGridLayout*, int );
    IntegerListConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                              QComboBox*, bool );
    virtual ~IntegerListConfigControl() {};
    virtual int getValue();
    virtual void hide() { combo->hide(); label->hide(); }
    virtual void show() { combo->show(); label->show(); }
private:
    void finish( bool );
    QLabel *label;
    QComboBox *combo;
};

class BoolConfigControl : public VIntConfigControl
{
public:
    BoolConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                       QGridLayout *, int );
    BoolConfigControl( vlc_object_t *, module_config_t *,
                       QLabel *, QCheckBox*, bool );
    virtual ~BoolConfigControl() {};
    virtual int getValue();
    virtual void show() { checkbox->show(); }
    virtual void hide() { checkbox->hide(); }
private:
    QCheckBox *checkbox;
    void finish();
};

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
    virtual int getType() { return 2; }
};

class FloatConfigControl : public VFloatConfigControl
{
public:
    FloatConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                        QGridLayout *, int );
    FloatConfigControl( vlc_object_t *, module_config_t *,
                        QLabel*, QDoubleSpinBox* );
    virtual ~FloatConfigControl() {};
    virtual float getValue();
    virtual void show() { spin->show(); label->show(); }
    virtual void hide() { spin->hide(); label->hide(); }

protected:
    QDoubleSpinBox *spin;

private:
    QLabel *label;
    void finish();
};

class FloatRangeConfigControl : public FloatConfigControl
{
public:
    FloatRangeConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                             QGridLayout *, int );
    FloatRangeConfigControl( vlc_object_t *, module_config_t *,
                             QLabel*, QDoubleSpinBox* );
private:
    void finish();
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
    virtual int getType() { return 3; }
};

class StringConfigControl : public VStringConfigControl
{
public:
    StringConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                         QGridLayout *, int,  bool pwd );
    StringConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                         QLineEdit*,  bool pwd );
    virtual ~StringConfigControl() {};
    virtual QString getValue() { return text->text(); };
    virtual void show() { text->show(); label->show(); }
    virtual void hide() { text->hide(); label->hide(); }
private:
    void finish();
    QLineEdit *text;
    QLabel *label;
};

class ModuleConfigControl : public VStringConfigControl
{
public:
    ModuleConfigControl( vlc_object_t *, module_config_t *, QWidget *, bool,
                         QGridLayout*, int );
    ModuleConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                         QComboBox*, bool );
    virtual ~ModuleConfigControl() {};
    virtual QString getValue();
    virtual void hide() { combo->hide(); label->hide(); }
    virtual void show() { combo->show(); label->show(); }
private:
    void finish( bool );
    QLabel *label;
    QComboBox *combo;
};

class StringListConfigControl : public VStringConfigControl
{
public:
    StringListConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                             bool, QGridLayout*, int );
    StringListConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                             QComboBox*, bool );
    virtual ~StringListConfigControl() {};
    virtual QString getValue();
    virtual void hide() { combo->hide(); label->hide(); }
    virtual void show() { combo->show(); label->show(); }
private:
    void finish( bool );
    QLabel *label;
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
