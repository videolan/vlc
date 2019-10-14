/*****************************************************************************
 * preferences_widgets.hpp : Widgets for preferences panels
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
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

#ifndef VLC_QT_PREFERENCES_WIDGETS_HPP_
#define VLC_QT_PREFERENCES_WIDGETS_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
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
    virtual void changeVisibility( bool ) { }
    vlc_object_t *p_this;
    module_config_t *p_item;
    virtual void fillGrid( QGridLayout*, int ) {}
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
    virtual int getType() const Q_DECL_OVERRIDE;
    virtual void doApply() Q_DECL_OVERRIDE;
protected:
    VIntConfigControl( vlc_object_t *a, module_config_t *b ) :
                ConfigControl(a,b) {}
};

class IntegerConfigControl : public VIntConfigControl
{
Q_OBJECT
public:
    IntegerConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    IntegerConfigControl( vlc_object_t *, module_config_t *,
                          QLabel*, QSpinBox* );
    int getValue() const Q_DECL_OVERRIDE;
protected:
    QSpinBox *spin;
     void changeVisibility( bool b ) Q_DECL_OVERRIDE
    {
        spin->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    void fillGrid( QGridLayout*, int ) Q_DECL_OVERRIDE;
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
    int getValue() const Q_DECL_OVERRIDE;
protected:
    QSlider *slider;
    void changeVisibility( bool b ) Q_DECL_OVERRIDE
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
    int getValue() const Q_DECL_OVERRIDE;
protected:
    void changeVisibility( bool b ) Q_DECL_OVERRIDE
    {
        combo->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    void fillGrid( QGridLayout*, int ) Q_DECL_OVERRIDE;
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
    int getValue() const Q_DECL_OVERRIDE;
    int getType() const Q_DECL_OVERRIDE;
protected:
    void changeVisibility( bool b ) Q_DECL_OVERRIDE
    {
        checkbox->setVisible( b );
    }
    void fillGrid( QGridLayout*, int ) Q_DECL_OVERRIDE;
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
    int getValue() const Q_DECL_OVERRIDE;
protected:
    void changeVisibility( bool b ) Q_DECL_OVERRIDE
    {
        color_but->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    void fillGrid( QGridLayout*, int ) Q_DECL_OVERRIDE;
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
    int getType() const Q_DECL_OVERRIDE;
    void doApply() Q_DECL_OVERRIDE;
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
    float getValue() const Q_DECL_OVERRIDE;

protected:
    void changeVisibility( bool b ) Q_DECL_OVERRIDE
    {
        spin->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    void fillGrid( QGridLayout*, int ) Q_DECL_OVERRIDE;
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
    int getType() const Q_DECL_OVERRIDE;
    void doApply() Q_DECL_OVERRIDE;
protected:
    VStringConfigControl( vlc_object_t *a, module_config_t *b ) :
                ConfigControl(a,b) {}
};

class StringConfigControl : public VStringConfigControl
{
    Q_OBJECT
public:
    StringConfigControl( vlc_object_t *, module_config_t *,
                         QWidget *, bool pwd );
    StringConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                         QLineEdit*,  bool pwd );
    QString getValue() const Q_DECL_OVERRIDE { return text->text(); };
protected:
    void changeVisibility( bool b ) Q_DECL_OVERRIDE
    {
        text->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    void fillGrid( QGridLayout*, int ) Q_DECL_OVERRIDE;
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
    QString getValue() const Q_DECL_OVERRIDE { return text->text(); };
public slots:
    virtual void updateField();
protected:
    void changeVisibility( bool b ) Q_DECL_OVERRIDE
    {
        text->setVisible( b );
        browse->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    void fillGrid( QGridLayout*, int ) Q_DECL_OVERRIDE;
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
    void updateField() Q_DECL_OVERRIDE;
};

class FontConfigControl : public VStringConfigControl
{
    Q_OBJECT
public:
    FontConfigControl( vlc_object_t *, module_config_t *, QWidget * );
    FontConfigControl( vlc_object_t *, module_config_t *, QLabel *,
                       QFontComboBox *);
    QString getValue() const Q_DECL_OVERRIDE  { return font->currentFont().family(); }
protected:
    void changeVisibility( bool b ) Q_DECL_OVERRIDE
    {
        font->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    void fillGrid( QGridLayout*, int ) Q_DECL_OVERRIDE;
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
    QString getValue() const Q_DECL_OVERRIDE;
protected:
    void changeVisibility( bool b ) Q_DECL_OVERRIDE
    {
        combo->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    void fillGrid( QGridLayout*, int ) Q_DECL_OVERRIDE;
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
    QString getValue() const Q_DECL_OVERRIDE;
public slots:
    void onUpdate();
protected:
    void changeVisibility( bool ) Q_DECL_OVERRIDE;
    void fillGrid( QGridLayout*, int ) Q_DECL_OVERRIDE;
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
    QString getValue() const Q_DECL_OVERRIDE;
protected:
    void changeVisibility( bool b ) Q_DECL_OVERRIDE
    {
        combo->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    void fillGrid( QGridLayout*, int ) Q_DECL_OVERRIDE;
    QComboBox *combo;
private:
    void finish(module_config_t * );
    QLabel *label;
    QList<QPushButton *> buttons;
private slots:
    void comboIndexChanged( int );
};

void setfillVLCConfigCombo(const char *configname, QComboBox *combo );

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
    QString getValue() Q_DECL_OVERRIDE;
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
    int getType() const Q_DECL_OVERRIDE;
    void doApply() Q_DECL_OVERRIDE;

protected:
    bool eventFilter( QObject *, QEvent * ) Q_DECL_OVERRIDE;
    void changeVisibility( bool b ) Q_DECL_OVERRIDE
    {
        table->setVisible( b );
        if ( label ) label->setVisible( b );
    }
    void fillGrid( QGridLayout*, int ) Q_DECL_OVERRIDE;

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
