/*****************************************************************************
 * preferences_widgets.hpp : Widgets for preferences panels
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
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

#ifndef _PREFERENCESWIDGETS_H_
#define _PREFERENCESWIDGETS_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

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
#include <QDialog>
#include <QFontComboBox>

class QTreeWidget;
class QTreeWidgetItem;
class QGroupBox;
class QGridLayout;
class QDialogButtonBox;
class QVBoxLayout;

/*******************************************************
 * Simple widgets
 *******************************************************/

class InterfacePreviewWidget : public QLabel
{
    Q_OBJECT
public:
    InterfacePreviewWidget( QWidget * );
    enum enum_style {
                 COMPLETE, // aka MPC
                 MINIMAL,  // aka WMP12 minimal
                 SKINS };
public slots:
    void setPreview( enum_style );
    void setNormalPreview( bool b_minimal );
};

/*******************************************************
 * Variable controls
 *******************************************************/

class ConfigControl : public QObject
{
    Q_OBJECT
public:
    ConfigControl( vlc_object_t *_p_this, module_config_t *_p_conf,
                   QWidget *p ) : p_this( _p_this ), p_item( _p_conf )
    {
        widget = NULL;
    }
    ConfigControl( vlc_object_t *_p_this, module_config_t *_p_conf ) :
                            p_this (_p_this ), p_item( _p_conf )
    {
        widget = NULL;
    }
    virtual int getType() const = 0;
    const char * getName() const { return  p_item->psz_name; }
    QWidget *getWidget() const { return widget; }
    bool isAdvanced() const { return p_item->b_advanced; }
    virtual void hide() { if ( widget ) widget->hide(); };
    virtual void show() { if ( widget ) widget->show(); };

    static ConfigControl * createControl( vlc_object_t*,
                                          module_config_t*,QWidget*,
                                          QGridLayout *, int line = 0 );
    virtual void doApply() = 0;
protected:
    vlc_object_t *p_this;
    module_config_t *p_item;
    QString _name;
    QWidget *widget;
#if 0
/* You shouldn't use that now..*/
signals:
    void Updated();
#endif
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
    virtual int getValue() const = 0;
    virtual int getType() const;
    virtual void doApply();
};

class IntegerConfigControl : public VIntConfigControl
{
Q_OBJECT
public:
    IntegerConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                          QGridLayout *, int );
    IntegerConfigControl( vlc_object_t *, module_config_t *,
                          QLabel*, QSpinBox* );
    IntegerConfigControl( vlc_object_t *, module_config_t *,
                          QLabel*, QSlider* );
    virtual int getValue() const;
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
                               QGridLayout *, int );
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
    virtual int getValue() const;
protected:
    QSlider *slider;
private:
    QLabel *label;
    void finish();
};

class IntegerListConfigControl : public VIntConfigControl
{
Q_OBJECT
public:
    IntegerListConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                              bool, QGridLayout*, int );
    IntegerListConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                              QComboBox*, bool );
    virtual int getValue() const;
    virtual void hide() { combo->hide(); if( label ) label->hide(); }
    virtual void show() { combo->show(); if( label ) label->show(); }
private:
    void finish(module_config_t * );
    QLabel *label;
    QComboBox *combo;
private slots:
    void actionRequested( int );

};

class BoolConfigControl : public VIntConfigControl
{
public:
    BoolConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                       QGridLayout *, int );
    BoolConfigControl( vlc_object_t *, module_config_t *,
                       QLabel *, QAbstractButton* );
    virtual int getValue() const;
    virtual void show() { checkbox->show(); }
    virtual void hide() { checkbox->hide(); }
    virtual int getType() const;
private:
    QAbstractButton *checkbox;
    void finish();
};

class ColorConfigControl : public VIntConfigControl
{
Q_OBJECT
public:
    ColorConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                        QGridLayout *, int );
    ColorConfigControl( vlc_object_t *, module_config_t *,
                        QLabel *, QAbstractButton* );
    virtual ~ColorConfigControl() { delete color_px; }
    virtual int getValue() const;
private:
    QLabel *label;
    QAbstractButton *color_but;
    QPixmap *color_px;
    int i_color;
    void finish();
private slots:
    void selectColor();
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
    virtual float getValue() const = 0;
    virtual int getType() const;
    virtual void doApply();
};

class FloatConfigControl : public VFloatConfigControl
{
    Q_OBJECT
public:
    FloatConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                        QGridLayout *, int );
    FloatConfigControl( vlc_object_t *, module_config_t *,
                        QLabel*, QDoubleSpinBox* );
    virtual float getValue() const;
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
                             QGridLayout *, int );
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
    virtual QString getValue() const = 0;
    virtual int getType() const;
    virtual void doApply();
};

class StringConfigControl : public VStringConfigControl
{
    Q_OBJECT
public:
    StringConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                         QGridLayout *, int,  bool pwd );
    StringConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                         QLineEdit*,  bool pwd );
    virtual QString getValue() const { return text->text(); };
    virtual void show() { text->show(); if( label ) label->show(); }
    virtual void hide() { text->hide(); if( label ) label->hide(); }
private:
    void finish();
    QLineEdit *text;
    QLabel *label;
};

class FileConfigControl : public VStringConfigControl
{
    Q_OBJECT
public:
    FileConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                       QGridLayout *, int );
    FileConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                       QLineEdit *, QPushButton * );
    virtual QString getValue() const { return text->text(); };
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
    Q_OBJECT
public:
    DirectoryConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                            QGridLayout *, int );
    DirectoryConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                            QLineEdit *, QPushButton * );
public slots:
    virtual void updateField();
};

class FontConfigControl : public VStringConfigControl
{
    Q_OBJECT
public:
    FontConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                       QGridLayout *, int);
    FontConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                       QFontComboBox *);
    virtual QString getValue() const { return font->currentFont().family(); }
protected:
    QLabel *label;
    QFontComboBox *font;
};

class ModuleConfigControl : public VStringConfigControl
{
public:
    ModuleConfigControl( vlc_object_t *, module_config_t *, QWidget *, bool,
                         QGridLayout*, int );
    ModuleConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                         QComboBox*, bool );
    virtual QString getValue() const;
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
    Q_OBJECT
    friend class ConfigControl;
public:
    ModuleListConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                             bool, QGridLayout*, int );
//    ModuleListConfigControl( vlc_object_t *, module_config_t *, QLabel *,
//                         QComboBox*, bool );
    virtual ~ModuleListConfigControl();
    virtual QString getValue() const;
    virtual void hide();
    virtual void show();
public slots:
    void onUpdate();
private:
    void finish( bool );
    void checkbox_lists(module_t*);
    void checkbox_lists( QString, QString, const char* );
    QList<checkBoxListItem*> modules;
    QGroupBox *groupBox;
    QLineEdit *text;
};

class StringListConfigControl : public VStringConfigControl
{
    Q_OBJECT
public:
    StringListConfigControl( vlc_object_t *, module_config_t *, QWidget *,
                             QGridLayout*, int );
    StringListConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                             QComboBox*, bool );
    virtual QString getValue() const;
    virtual void hide() { combo->hide(); if( label ) label->hide(); }
    virtual void show() { combo->show(); if( label ) label->show(); }
    QComboBox *combo;
private:
    void finish(module_config_t * );
    QLabel *label;
private slots:
    void actionRequested( int );

};

void setfillVLCConfigCombo(const char *configname, intf_thread_t *p_intf,
                        QComboBox *combo );

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
    QVector<ModuleCheckBox> checkboxes;
    QLineEdit *text;
private slot:
    void OnUpdate();
};
#endif

/**********************************************************************
 * Key selector widget
 **********************************************************************/
class KeySelectorControl : public ConfigControl
{
    Q_OBJECT
public:
    KeySelectorControl( vlc_object_t *, module_config_t *, QWidget *,
                        QGridLayout*, int );
    virtual int getType() const;
    virtual void hide() { table->hide(); if( label ) label->hide(); }
    virtual void show() { table->show(); if( label ) label->show(); }
    virtual void doApply();
protected:
    virtual bool eventFilter( QObject *, QEvent * );
private:
    void finish();
    QLabel *label;
    QTreeWidget *table;
    QList<module_config_t *> values;
private slots:
    void selectKey( QTreeWidgetItem * = NULL, int column = 1 );
    void filter( const QString & );
};

class KeyInputDialog : public QDialog
{
public:
    KeyInputDialog( QTreeWidget *, const QString&, QWidget *, bool b_global = false);
    int keyValue;
    bool conflicts;

private:
    QTreeWidget *table;
    QLabel *selected, *warning;
    QDialogButtonBox *buttonBox;

    void checkForConflicts( int i_vlckey );
    void keyPressEvent( QKeyEvent *);
    void wheelEvent( QWheelEvent *);
    bool b_global;
};
#endif
