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
class QBoxLayout;
class SearchLineEdit;

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
    virtual int getType() const = 0;
    const char * getName() const { return  p_item->psz_name; }
    bool isAdvanced() const { return p_item->b_advanced; }
    void hide() { changeVisibility( false ); }
    void show() { changeVisibility( true ); }
    /* ConfigControl factory */
    static ConfigControl * createControl( vlc_object_t*,
                                          module_config_t*,QWidget*,
                                          QGridLayout *, int line = 0 );
    /* Inserts control into another layout block, using a sublayout */
    void insertInto( QBoxLayout * );
    /* Inserts control into an existing grid layout */
    void insertIntoExistingGrid( QGridLayout*, int );
    virtual void doApply() = 0;
protected:
    ConfigControl( vlc_object_t *_p_this, module_config_t *_p_conf ) :
                            p_this (_p_this ), p_item( _p_conf ) {}
    virtual void changeVisibility( bool b ) { Q_UNUSED(b); };
    vlc_object_t *p_this;
    module_config_t *p_item;
    virtual void fillGrid( QGridLayout*, int ) {};
signals:
    void changed();
#if 0
/* You shouldn't use that now..*/
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
    virtual int getValue() const = 0;
    virtual int getType() const;
    virtual void doApply();
protected:
    VIntConfigControl( vlc_object_t *a, module_config_t *b ) :
                ConfigControl(a,b) {};
};

class IntegerConfigControl : public VIntConfigControl
{
Q_OBJECT
public:
    IntegerConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    IntegerConfigControl( vlc_object_t *, module_config_t *,
                          QLabel*, QSpinBox* );
    virtual int getValue() const;
protected:
    QSpinBox *spin;
    virtual void changeVisibility( bool b )
    {
        spin->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    virtual void fillGrid( QGridLayout*, int );
private:
    QLabel *label;
    void finish();
};

class IntegerRangeConfigControl : public IntegerConfigControl
{
    Q_OBJECT
public:
    IntegerRangeConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    IntegerRangeConfigControl( vlc_object_t *, module_config_t *,
                               QLabel*, QSpinBox* );
    IntegerRangeConfigControl( vlc_object_t *, module_config_t *,
                               QLabel*, QSlider* );
private:
    void finish();
};

class IntegerRangeSliderConfigControl : public VIntConfigControl
{
    Q_OBJECT
public:
    IntegerRangeSliderConfigControl( vlc_object_t *, module_config_t *,
                                QLabel *, QSlider * );
    virtual int getValue() const;
protected:
    QSlider *slider;
    virtual void changeVisibility( bool b )
    {
        slider->setVisible( b );
        if ( label ) label->setVisible( b );
    }
private:
    QLabel *label;
    void finish();
};

class IntegerListConfigControl : public VIntConfigControl
{
Q_OBJECT
public:
    IntegerListConfigControl( vlc_object_t *, module_config_t *, QWidget *, bool );
    IntegerListConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                              QComboBox*, bool );
    virtual int getValue() const;
protected:
    virtual void changeVisibility( bool b )
    {
        combo->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    virtual void fillGrid( QGridLayout*, int );
private:
    void finish(module_config_t * );
    QLabel *label;
    QComboBox *combo;
    QList<QPushButton *> buttons;
};

class BoolConfigControl : public VIntConfigControl
{
    Q_OBJECT
public:
    BoolConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    BoolConfigControl( vlc_object_t *, module_config_t *,
                       QLabel *, QAbstractButton* );
    virtual int getValue() const;
    virtual int getType() const;
protected:
    virtual void changeVisibility( bool b )
    {
        checkbox->setVisible( b );
    }
    virtual void fillGrid( QGridLayout*, int );
private:
    QAbstractButton *checkbox;
    void finish();
};

class ColorConfigControl : public VIntConfigControl
{
Q_OBJECT
public:
    ColorConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    ColorConfigControl( vlc_object_t *, module_config_t *,
                        QLabel *, QAbstractButton* );
    virtual ~ColorConfigControl() { delete color_px; }
    virtual int getValue() const;
protected:
    virtual void changeVisibility( bool b )
    {
        color_but->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    virtual void fillGrid( QGridLayout*, int );
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
    virtual float getValue() const = 0;
    virtual int getType() const;
    virtual void doApply();
protected:
    VFloatConfigControl( vlc_object_t *a, module_config_t *b ) :
                ConfigControl(a,b) {};
};

class FloatConfigControl : public VFloatConfigControl
{
    Q_OBJECT
public:
    FloatConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    FloatConfigControl( vlc_object_t *, module_config_t *,
                        QLabel*, QDoubleSpinBox* );
    virtual float getValue() const;

protected:
    virtual void changeVisibility( bool b )
    {
        spin->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    virtual void fillGrid( QGridLayout*, int );
    QDoubleSpinBox *spin;

private:
    QLabel *label;
    void finish();
};

class FloatRangeConfigControl : public FloatConfigControl
{
    Q_OBJECT
public:
    FloatRangeConfigControl( vlc_object_t *, module_config_t *, QWidget * );
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
    virtual QString getValue() const = 0;
    virtual int getType() const;
    virtual void doApply();
protected:
    VStringConfigControl( vlc_object_t *a, module_config_t *b ) :
                ConfigControl(a,b) {};
};

class StringConfigControl : public VStringConfigControl
{
    Q_OBJECT
public:
    StringConfigControl( vlc_object_t *, module_config_t *,
                         QWidget *, bool pwd );
    StringConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                         QLineEdit*,  bool pwd );
    virtual QString getValue() const { return text->text(); };
protected:
    virtual void changeVisibility( bool b )
    {
        text->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    virtual void fillGrid( QGridLayout*, int );
private:
    void finish();
    QLineEdit *text;
    QLabel *label;
};

class FileConfigControl : public VStringConfigControl
{
    Q_OBJECT
public:
    FileConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    FileConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                       QLineEdit *, QPushButton * );
    virtual QString getValue() const { return text->text(); };
public slots:
    virtual void updateField();
protected:
    virtual void changeVisibility( bool b )
    {
        text->setVisible( b );
        browse->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    virtual void fillGrid( QGridLayout*, int );
    void finish();
    QLineEdit *text;
    QLabel *label;
    QPushButton *browse;
};

class DirectoryConfigControl : public FileConfigControl
{
    Q_OBJECT
public:
    DirectoryConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    DirectoryConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                            QLineEdit *, QPushButton * );
public slots:
    virtual void updateField();
};

class FontConfigControl : public VStringConfigControl
{
    Q_OBJECT
public:
    FontConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    FontConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                       QFontComboBox *);
    virtual QString getValue() const { return font->currentFont().family(); }
protected:
    virtual void changeVisibility( bool b )
    {
        font->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    virtual void fillGrid( QGridLayout*, int );
    QLabel *label;
    QFontComboBox *font;
};

class ModuleConfigControl : public VStringConfigControl
{
    Q_OBJECT
public:
    ModuleConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    ModuleConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                         QComboBox* );
    virtual QString getValue() const;
protected:
    virtual void changeVisibility( bool b )
    {
        combo->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    virtual void fillGrid( QGridLayout*, int );
private:
    void finish( );
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
    ModuleListConfigControl( vlc_object_t *, module_config_t *, QWidget *, bool );
//    ModuleListConfigControl( vlc_object_t *, module_config_t *, QLabel *,
//                         QComboBox*, bool );
    virtual ~ModuleListConfigControl();
    virtual QString getValue() const;
public slots:
    void onUpdate();
protected:
    virtual void changeVisibility( bool );
    virtual void fillGrid( QGridLayout*, int );
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
    StringListConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    StringListConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                             QComboBox*, bool );
    virtual QString getValue() const;
protected:
    virtual void changeVisibility( bool b )
    {
        combo->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    virtual void fillGrid( QGridLayout*, int );
    QComboBox *combo;
private:
    void finish(module_config_t * );
    QLabel *label;
    QList<QPushButton *> buttons;
private slots:
    void comboIndexChanged( int );
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
    Q_OBJECT
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
    KeySelectorControl( vlc_object_t *, module_config_t *, QWidget * );
    virtual int getType() const;
    virtual void doApply();

protected:
    virtual bool eventFilter( QObject *, QEvent * );
    virtual void changeVisibility( bool b )
    {
        table->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    virtual void fillGrid( QGridLayout*, int );

private:
    void buildAppHotkeysList( QWidget *rootWidget );
    void finish();
    QLabel *label;
    QLabel *searchLabel;
    SearchLineEdit *actionSearch;
    QComboBox *searchOption;
    QLabel *searchOptionLabel;
    QTreeWidget *table;
    QList<module_config_t *> values;
    QSet<QString> existingkeys;
    enum
    {
        ACTION_COL = 0,
        HOTKEY_COL = 1,
        GLOBAL_HOTKEY_COL = 2,
        ANY_COL = 3 // == count()
    };

private slots:
    void selectKey( QTreeWidgetItem * = NULL, int column = 1 );
    void filter( const QString & );
};

class KeyInputDialog : public QDialog
{
    Q_OBJECT

public:
    KeyInputDialog( QTreeWidget *, const QString&, QWidget *, bool b_global = false );
    int keyValue;
    bool conflicts;
    void setExistingkeysSet( const QSet<QString> *keyset = NULL );

private:
    QTreeWidget *table;
    QLabel *selected, *warning;
    QPushButton *ok, *unset;

    void checkForConflicts( int i_vlckey, const QString &sequence );
    void keyPressEvent( QKeyEvent *);
    void wheelEvent( QWheelEvent *);
    bool b_global;
    const QSet<QString> *existingkeys;

private slots:
    void unsetAction();
};
#endif
