/*****************************************************************************
 * preferences_widgets.hpp : Widgets for preferences panels
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Antoine Cellerier <dionoea@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#include "qt4.hpp"
#include <assert.h>

#include <QWidget>

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QTreeWidget>
#include <QSpinBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QVector>
#include <QDialog>


class QFile;
class QTreeWidget;
class QTreeWidgetItem;
class QGroupBox;
class QGridLayout;

class ConfigControl : public QObject
{
    Q_OBJECT
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
    const char * getName() { return  p_item->psz_name; }
    QWidget *getWidget() { assert( widget ); return widget; }
    bool isAdvanced() { return p_item->b_advanced; }
    virtual void hide() { getWidget()->hide(); };
    virtual void show() { getWidget()->show(); };

    static ConfigControl * createControl( vlc_object_t*,
                                          module_config_t*,QWidget* );
    static ConfigControl * createControl( vlc_object_t*,
                                          module_config_t*,QWidget*,
                                          QGridLayout *, int& );
    void doApply( intf_thread_t *);
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
Q_OBJECT
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
Q_OBJECT
public:
    IntegerConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                          QGridLayout *, int& );
    IntegerConfigControl( vlc_object_t *, module_config_t *,
                          QLabel*, QSpinBox* );
    IntegerConfigControl( vlc_object_t *, module_config_t *,
                          QLabel*, QSlider* );
    virtual ~IntegerConfigControl() {};
    virtual int getValue();
    virtual void show() { spin->show(); if( label ) label->show(); }
    virtual void hide() { spin->hide(); if( label ) label->hide(); }

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
                               QGridLayout *, int& );
    IntegerRangeConfigControl( vlc_object_t *, module_config_t *,
                               QLabel*, QSpinBox* );
private:
    void finish();
};

class IntegerRangeSliderConfigControl : public VIntConfigControl
{
public:
    IntegerRangeSliderConfigControl( vlc_object_t *, module_config_t *,
                                QLabel *, QSlider * );
    virtual ~IntegerRangeSliderConfigControl() {};
    virtual int getValue();
protected:
         QSlider *slider;
private:
         QLabel *label;
         void finish();
};

class IntegerListConfigControl : public VIntConfigControl
{
public:
    IntegerListConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                              bool, QGridLayout*, int& );
    IntegerListConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                              QComboBox*, bool );
    virtual ~IntegerListConfigControl() {};
    virtual int getValue();
    virtual void hide() { combo->hide(); if( label ) label->hide(); }
    virtual void show() { combo->show(); if( label ) label->show(); }
private:
    void finish( bool );
    QLabel *label;
    QComboBox *combo;
};

class BoolConfigControl : public VIntConfigControl
{
public:
    BoolConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                       QGridLayout *, int& );
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
    Q_OBJECT
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
    Q_OBJECT
public:
    FloatConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                        QGridLayout *, int& );
    FloatConfigControl( vlc_object_t *, module_config_t *,
                        QLabel*, QDoubleSpinBox* );
    virtual ~FloatConfigControl() {};
    virtual float getValue();
    virtual void show() { spin->show(); if( label ) label->show(); }
    virtual void hide() { spin->hide(); if( label ) label->hide(); }

protected:
    QDoubleSpinBox *spin;

private:
    QLabel *label;
    void finish();
};

class FloatRangeConfigControl : public FloatConfigControl
{
    Q_OBJECT
public:
    FloatRangeConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                             QGridLayout *, int& );
    FloatRangeConfigControl( vlc_object_t *, module_config_t *,
                             QLabel*, QDoubleSpinBox* );
private:
    void finish();
};

/*******************************************************
 * String-based controls
 *******************************************************/
class VStringConfigControl : public ConfigControl
{
    Q_OBJECT
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
    Q_OBJECT
public:
    StringConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                         QGridLayout *, int&,  bool pwd );
    StringConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                         QLineEdit*,  bool pwd );
    virtual ~StringConfigControl() {};
    virtual QString getValue() { return text->text(); };
    virtual void show() { text->show(); if( label ) label->show(); }
    virtual void hide() { text->hide(); if( label ) label->hide(); }
private:
    void finish();
    QLineEdit *text;
    QLabel *label;
};

class FileConfigControl : public VStringConfigControl
{
    Q_OBJECT;
public:
    FileConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                       QGridLayout *, int&, bool pwd );
    FileConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                       QLineEdit *, QPushButton *, bool pwd );
    virtual ~FileConfigControl() {};
    virtual QString getValue() { return text->text(); };
    virtual void show() { text->show(); if( label ) label->show(); browse->show(); }
    virtual void hide() { text->hide(); if( label ) label->hide(); browse->hide(); }
public slots:
    virtual void updateField();
protected:
    void finish();
    QLineEdit *text;
    QLabel *label;
    QPushButton *browse;
};

class DirectoryConfigControl : public FileConfigControl
{
    Q_OBJECT;
public:
    DirectoryConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                            QGridLayout *, int&, bool pwd );
    DirectoryConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                            QLineEdit *, QPushButton *, bool pwd );
    virtual ~DirectoryConfigControl() {};
public slots:
    virtual void updateField();
};

class FontConfigControl : public FileConfigControl
{
    Q_OBJECT;
public:
    FontConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                       QGridLayout *, int&, bool pwd );
    FontConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                       QLineEdit *, QPushButton *, bool pwd );
    virtual ~FontConfigControl() {};
public slots:
    virtual void updateField();
};

class ModuleConfigControl : public VStringConfigControl
{
public:
    ModuleConfigControl( vlc_object_t *, module_config_t *, QWidget *, bool,
                         QGridLayout*, int& );
    ModuleConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                         QComboBox*, bool );
    virtual ~ModuleConfigControl() {};
    virtual QString getValue();
    virtual void hide() { combo->hide(); if( label ) label->hide(); }
    virtual void show() { combo->show(); if( label ) label->show(); }
private:
    void finish( bool );
    QLabel *label;
    QComboBox *combo;
};

struct checkBoxListItem {
    QCheckBox *checkBox;
    char *psz_module;
};

class ModuleListConfigControl : public VStringConfigControl
{
    Q_OBJECT;
public:
    ModuleListConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                             bool, QGridLayout*, int& );
//    ModuleListConfigControl( vlc_object_t *, module_config_t *, QLabel *,
//                         QComboBox*, bool );
    virtual ~ModuleListConfigControl();
    virtual QString getValue();
    virtual void hide();
    virtual void show();
public slots:
    void onUpdate( int value );
private:
    void finish( bool );
    QVector<checkBoxListItem*> modules;
    QGroupBox *groupBox;
    QLineEdit *text;
};

class StringListConfigControl : public VStringConfigControl
{
public:
    StringListConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                             bool, QGridLayout*, int& );
    StringListConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                             QComboBox*, bool );
    virtual ~StringListConfigControl() {};
    virtual QString getValue();
    virtual void hide() { if( combo ) combo->hide(); if( label ) label->hide(); }
    virtual void show() { if( combo ) combo->show(); if( label ) label->show(); }
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

/**********************************************************************
 * Key selector widget
 **********************************************************************/
class KeyInputDialog : public QDialog
{
public:
    KeyInputDialog( QList<module_config_t *> &, const char * );
    int keyValue;
    bool conflicts;
private:
    void checkForConflicts( int i_vlckey );
    void keyPressEvent( QKeyEvent *);
    void wheelEvent( QWheelEvent *);
    QLabel *selected;
    QLabel *warning;
    const char * keyToChange;
    QList<module_config_t*> values;
};

class KeySelectorControl : public ConfigControl
{
    Q_OBJECT;
public:
    KeySelectorControl( vlc_object_t *, module_config_t *, QWidget *,
                        QGridLayout*, int& );
    virtual int getType() { return 4; }
    virtual ~KeySelectorControl() {};
    virtual void hide() { table->hide(); if( label ) label->hide(); }
    virtual void show() { table->show(); if( label ) label->show(); }
    void doApply();
private:
    void finish();
    QLabel *label;
    QTreeWidget *table;
    QList<module_config_t *> values;
private slots:
    void selectKey( QTreeWidgetItem *);
};

#endif
