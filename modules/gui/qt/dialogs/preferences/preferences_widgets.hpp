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
#include <cassert>

#include <QTreeWidgetItem>
#include <QLabel>
#include <QDialog>
#include <QSet>
#include <QContextMenuEvent>

class QWidget;
class QTreeWidget;
class QGroupBox;
class QGridLayout;
class QBoxLayout;
class SearchLineEdit;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QFontComboBox;
class QSlider;
class QAbstractButton;

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
    int getType() const { return (p_item) ? p_item->i_type : -1; }
    const char * getName() const { return  p_item->psz_name; }
    void hide() { changeVisibility( false ); }
    void show() { changeVisibility( true ); }
    /* ConfigControl factory */
    static ConfigControl * createControl( module_config_t*, QWidget* );
    static ConfigControl * createControl( module_config_t*, QWidget*,
                                          QGridLayout *, int line = 0 );
    static ConfigControl * createControl( module_config_t*, QWidget*,
                                          QBoxLayout *, int line = 0 );
    /** Inserts control into an existing grid layout */
    virtual void insertInto( QGridLayout*, int row = 0 ) { Q_UNUSED( row ); }
    /** Inserts control into an existing box layout */
    virtual void insertInto( QBoxLayout*, int index = 0 ) { Q_UNUSED( index ); }
    virtual void doApply() = 0;
    virtual void storeValue() = 0;
protected:
    ConfigControl( module_config_t *_p_conf ) : p_item( _p_conf ) {}
    virtual void changeVisibility( bool ) { }
    module_config_t *p_item;
signals:
    void changed();
};

/*******************************************************
 * Integer-based controls
 *******************************************************/
class VIntConfigControl : public ConfigControl
{
    Q_OBJECT
public:
    virtual int getValue() const = 0;
    virtual void doApply() override;
    virtual void storeValue() override;
protected:
    VIntConfigControl( module_config_t *i ) : ConfigControl(i) {}
};

class IntegerConfigControl : public VIntConfigControl
{
    Q_OBJECT
public:
    IntegerConfigControl( module_config_t *, QWidget * );
    IntegerConfigControl( module_config_t *, QLabel*, QSpinBox* );
    void insertInto( QGridLayout*, int row = 0 ) override;
    void insertInto( QBoxLayout*, int index = 0 ) override;
    int getValue() const override;
protected:
    QSpinBox *spin;
    void changeVisibility( bool ) override;
private:
    QLabel *label;
    void finish();
};

class IntegerRangeConfigControl : public IntegerConfigControl
{
    Q_OBJECT
public:
    IntegerRangeConfigControl( module_config_t *, QWidget * );
    IntegerRangeConfigControl( module_config_t *, QLabel*, QSpinBox* );
    IntegerRangeConfigControl( module_config_t *, QLabel*, QSlider* );
private:
    void finish();
};

class IntegerRangeSliderConfigControl : public VIntConfigControl
{
    Q_OBJECT
public:
    IntegerRangeSliderConfigControl( module_config_t *, QLabel *, QSlider * );
    int getValue() const override;
protected:
    QSlider *slider;
    void changeVisibility( bool ) override;
private:
    QLabel *label;
    void finish();
};

class IntegerListConfigControl : public VIntConfigControl
{
Q_OBJECT
public:
    IntegerListConfigControl( module_config_t *, QWidget * );
    IntegerListConfigControl( module_config_t *, QLabel *, QComboBox* );
    void insertInto( QGridLayout*, int row = 0 ) override;
    void insertInto( QBoxLayout*, int index = 0 ) override;
    int getValue() const override;
protected:
    void changeVisibility( bool ) override;
private:
    void finish(module_config_t * );
    QLabel *label;
    QComboBox *combo;
};

class BoolConfigControl : public VIntConfigControl
{
    Q_OBJECT
public:
    BoolConfigControl( module_config_t *, QWidget * );
    BoolConfigControl( module_config_t *, QLabel *, QAbstractButton* );
    void insertInto( QGridLayout*, int row = 0 ) override;
    void insertInto( QBoxLayout*, int index = 0 ) override;
    int getValue() const override;
protected:
    void changeVisibility( bool ) override;
private:
    QAbstractButton *checkbox;
    void finish();
};

class ColorConfigControl : public VIntConfigControl
{
    Q_OBJECT
public:
    ColorConfigControl( module_config_t *, QWidget * );
    ColorConfigControl( module_config_t *, QLabel *, QAbstractButton* );
    virtual ~ColorConfigControl() { delete color_px; }
    void insertInto( QGridLayout*, int row = 0 ) override;
    void insertInto( QBoxLayout*, int index = 0 ) override;
    int getValue() const override;
protected:
    void changeVisibility( bool ) override;
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
    void doApply() override;
    void storeValue() override;
protected:
    VFloatConfigControl( module_config_t *i ) : ConfigControl(i) {}
};

class FloatConfigControl : public VFloatConfigControl
{
    Q_OBJECT
public:
    FloatConfigControl( module_config_t *, QWidget * );
    FloatConfigControl( module_config_t *, QLabel*, QDoubleSpinBox* );
    void insertInto( QGridLayout*, int row = 0 ) override;
    void insertInto( QBoxLayout*, int index = 0 ) override;
    float getValue() const override;

protected:
    void changeVisibility( bool ) override;
    QDoubleSpinBox *spin;

private:
    QLabel *label;
    void finish();
};

class FloatRangeConfigControl : public FloatConfigControl
{
    Q_OBJECT
public:
    FloatRangeConfigControl( module_config_t *, QWidget * );
    FloatRangeConfigControl( module_config_t *, QLabel*, QDoubleSpinBox* );
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
    void doApply() override;
    void storeValue() override;
protected:
    VStringConfigControl( module_config_t *i ) : ConfigControl(i) {}
};

class StringConfigControl : public VStringConfigControl
{
    Q_OBJECT
public:
    StringConfigControl( module_config_t *, QWidget * );
    StringConfigControl( module_config_t *, QLabel *, QLineEdit* );
    void insertInto( QGridLayout*, int row = 0 ) override;
    void insertInto( QBoxLayout*, int index = 0 ) override;
    QString getValue() const override;
protected:
    void changeVisibility( bool ) override;
    QLineEdit *text;
private:
    void finish();
    QLabel *label;
};

class PasswordConfigControl : public StringConfigControl
{
    Q_OBJECT
public:
    PasswordConfigControl( module_config_t *, QWidget * );
    PasswordConfigControl( module_config_t *, QLabel *, QLineEdit* );
private:
    void finish();
};

class FileConfigControl : public VStringConfigControl
{
    Q_OBJECT
public:
    FileConfigControl( module_config_t *, QWidget * );
    FileConfigControl( module_config_t *, QLabel *, QLineEdit *, QPushButton * );
    void insertInto( QGridLayout*, int row = 0 ) override;
    void insertInto( QBoxLayout*, int index = 0 ) override;
    QString getValue() const override;
public slots:
    virtual void updateField();
protected:
    void changeVisibility( bool ) override;
    void finish();
    QLineEdit *text;
    QLabel *label;
    QPushButton *browse;
};

class DirectoryConfigControl : public FileConfigControl
{
    Q_OBJECT
public:
    DirectoryConfigControl( module_config_t *, QWidget * );
    DirectoryConfigControl( module_config_t *, QLabel *, QLineEdit *, QPushButton * );
public slots:
    void updateField() override;
};

class FontConfigControl : public VStringConfigControl
{
    Q_OBJECT
public:
    FontConfigControl( module_config_t *, QWidget * );
    FontConfigControl( module_config_t *, QLabel *, QFontComboBox *);
    void insertInto( QGridLayout*, int row = 0 ) override;
    void insertInto( QBoxLayout*, int index = 0 ) override;
    QString getValue() const override;
protected:
    void changeVisibility( bool ) override;
    QLabel *label;
    QFontComboBox *font;
};

class ModuleConfigControl : public VStringConfigControl
{
    Q_OBJECT
public:
    ModuleConfigControl( module_config_t *, QWidget * );
    ModuleConfigControl( module_config_t *, QLabel *, QComboBox* );
    void insertInto( QGridLayout*, int row = 0 ) override;
    void insertInto( QBoxLayout*, int index = 0 ) override;
    QString getValue() const override;
protected:
    void changeVisibility( bool ) override;
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
    ModuleListConfigControl( module_config_t *, QWidget *, bool );
    virtual ~ModuleListConfigControl();
    void insertInto( QGridLayout*, int row = 0 ) override;
    void insertInto( QBoxLayout*, int index = 0 ) override;
    QString getValue() const override;
public slots:
    void onUpdate();
protected:
    void changeVisibility( bool ) override;
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
    StringListConfigControl( module_config_t *, QWidget * );
    StringListConfigControl( module_config_t *, QLabel *, QComboBox* );
    void insertInto( QGridLayout*, int row = 0 ) override;
    void insertInto( QBoxLayout*, int index = 0 ) override;
    QString getValue() const override;
protected:
    void changeVisibility( bool ) override;
    QComboBox *combo;
private:
    void finish(module_config_t * );
    QLabel *label;
private slots:
    void comboIndexChanged( int );
};

void setfillVLCConfigCombo(const char *configname, QComboBox *combo );

/**********************************************************************
 * Key selector widget
 **********************************************************************/
class KeyTableItem;

class KeySelectorControl : public ConfigControl
{
    Q_OBJECT

public:
    KeySelectorControl( QWidget * );
    void insertInto( QGridLayout*, int row = 0 ) override;
    void doApply() override;
    void storeValue() override {};
    enum ColumnIndex
    {
        ACTION_COL = 0,
        HOTKEY_COL = 1,
        GLOBAL_HOTKEY_COL = 2,
        ANY_COL = 3 // == count()
    };
    static KeyTableItem *find_conflict( QTreeWidget *, QString, KeyTableItem *, enum ColumnIndex );

protected:
    bool eventFilter( QObject *, QEvent * ) override;
#ifndef QT_NO_CONTEXTMENU
    void tableContextMenuEvent( QContextMenuEvent * );
#endif
    void changeVisibility( bool ) override;
    void unset( KeyTableItem *, enum ColumnIndex );
    void unset( QTreeWidgetItem *, int );
    void reset( KeyTableItem *, enum ColumnIndex );
    void reset_all( enum KeySelectorControl::ColumnIndex column );
    /** Reassign key to specified item */
    void reassign_key( KeyTableItem *item, QString keys,
                       enum KeySelectorControl::ColumnIndex column );
    void copy_value( KeyTableItem *, enum KeySelectorControl::ColumnIndex );

private:
    void selectKey( KeyTableItem *, enum ColumnIndex );
    void buildAppHotkeysList( QWidget *rootWidget );
    void finish();
    QLabel *label;
    QLabel *searchLabel;
    SearchLineEdit *actionSearch;
    QComboBox *searchOption;
    QLabel *searchOptionLabel;
    QTreeWidget *table;
    QSet<QString> existingkeys;

private slots:
    void selectKey( QTreeWidgetItem *, int );
    void filter();
};

struct KeyItemAttr
{
    const char *config_name;
    QString default_keys;
    QString keys;
    bool matches_default;
};

class KeyTableItem : public QTreeWidgetItem
{
public:
    KeyTableItem() {}
    const QString &get_keys( enum KeySelectorControl::ColumnIndex );
    QString get_default_keys( enum KeySelectorControl::ColumnIndex );
    void set_keys( QString, enum KeySelectorControl::ColumnIndex );
    void set_keys( const char *keys, enum KeySelectorControl::ColumnIndex column )
    {
        set_keys( (keys) ? qfut( keys ) : qfu( "" ), column );
    }
    bool contains_key( QString, enum KeySelectorControl::ColumnIndex );
    void remove_key( QString, enum KeySelectorControl::ColumnIndex );
    struct KeyItemAttr normal;
    struct KeyItemAttr global;
};

class KeyInputDialog : public QDialog
{
    Q_OBJECT

public:
    KeyInputDialog( QTreeWidget *, KeyTableItem *, enum KeySelectorControl::ColumnIndex );
    bool conflicts;
    QString vlckey, vlckey_tr;
    void setExistingkeysSet( const QSet<QString> *keyset = NULL );

private:
    QTreeWidget *table;
    QLabel *selected, *warning;
    QPushButton *ok, *unset;
    KeyTableItem *keyItem;
    enum KeySelectorControl::ColumnIndex column;

    void checkForConflicts( const QString &sequence );
    void keyPressEvent( QKeyEvent *);
    void wheelEvent( QWheelEvent *);
    const QSet<QString> *existingkeys;

private slots:
    void unsetAction();
};

class KeyConflictDialog : public QDialog
{
    Q_OBJECT

public:
    KeyConflictDialog( QTreeWidget *, KeyTableItem *, enum KeySelectorControl::ColumnIndex );
};
#endif
