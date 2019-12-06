/*****************************************************************************
 * extended_panels.cpp : Extended controls panels
 ****************************************************************************
 * Copyright (C) 2006-2013 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Antoine Cellerier <dionoea .t videolan d@t org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>

#include <algorithm>

#include <QLabel>
#include <QVariant>
#include <QString>
#include <QFont>
#include <QGridLayout>
#include <QComboBox>
#include <QTimer>
#include <QFileDialog>
#include <QGraphicsScene>
#include <QPainter>
#include <QRegExp>
#include <QApplication>
#include <QScreen>

#include "extended_panels.hpp"
#include "dialogs/preferences/preferences.hpp"
#include "qt.hpp"
#include "player/player_controller.hpp"
#include "util/qt_dirs.hpp"
#include "widgets/native/customwidgets.hpp"
#include "dialogs/dialogs_provider.hpp"

#include "../../../../audio_filter/equalizer_presets.h"
#include <vlc_modules.h>
#include <vlc_plugin.h>
#include <vlc_player.h>

static bool filterIsPresent( const QString &filters, const QString &filter )
{
    QStringList list = filters.split( ':', QString::SplitBehavior::SkipEmptyParts );
    foreach( const QString &filterCmp, list )
    {
        if( filterCmp.compare( filter ) == 0 )
            return true;
    }
    return false;
}

static const char* GetVFilterType( struct intf_thread_t *p_intf, const char *psz_name )
{
    module_t *p_obj = module_find( psz_name );
    if( !p_obj )
    {
        msg_Err( p_intf, "Unable to find filter module \"%s\".", psz_name );
        return NULL;
    }

    if( module_provides( p_obj, "video splitter" ) )
        return "video-splitter";
    else if( module_provides( p_obj, "video filter" ) )
        return "video-filter";
    else if( module_provides( p_obj, "sub source" ) )
        return "sub-source";
    else if( module_provides( p_obj, "sub filter" ) )
        return "sub-filter";
    else
    {
        msg_Err( p_intf, "Unknown video filter type." );
        return NULL;
    }
}

static const QString ModuleFromWidgetName( QObject *obj )
{
    return obj->objectName().replace( "Enable","" );
}

static QString OptionFromWidgetName( QObject *obj )
{
    /* Gruik ? ... nah */
    return obj->objectName()
        .remove( QRegExp( "Slider|Combo|Dial|Check|Spin|Text" ) )
        .replace( QRegExp( "([A-Z])" ), "-\\1" )
        .toLower();
}

static inline void setup_vfilter( intf_thread_t *p_intf, const char* psz_name, QWidget *widget )
{
    const char *psz_filter_type = GetVFilterType( p_intf, psz_name );
    if( psz_filter_type == NULL )
        return;

    char *psz_filters = var_InheritString( p_intf, psz_filter_type );
    if( psz_filters == NULL )
        return;

    QCheckBox *checkbox = qobject_cast<QCheckBox*>( widget );
    QGroupBox *groupbox = qobject_cast<QGroupBox*>( widget );
    if( filterIsPresent( qfu(psz_filters), qfu(psz_name) ) )
    {
        if( checkbox ) checkbox->setChecked( true ); \
        else if (groupbox) groupbox->setChecked( true ); \
    }
    else
    {
        if( checkbox ) checkbox->setChecked( false );
        else if (groupbox) groupbox->setChecked( false );
    }
    free( psz_filters );
}

#define SETUP_VFILTER( widget ) \
    setup_vfilter( p_intf, #widget, ui.widget##Enable ); \
    CONNECT( ui.widget##Enable, clicked(), this, updateFilters() );

#define SETUP_VFILTER_OPTION( widget, signal ) \
    initComboBoxItems( ui.widget ); \
    setWidgetValue( ui.widget ); \
    CONNECT( ui.widget, signal, this, updateFilterOptions() );

ExtVideo::ExtVideo( intf_thread_t *_p_intf, QTabWidget *_parent ) :
            QObject( _parent ), p_intf( _p_intf )
{
    ui.setupUi( _parent );

    SETUP_VFILTER( adjust )
    SETUP_VFILTER_OPTION( hueSlider, valueChanged( int ) )
    SETUP_VFILTER_OPTION( contrastSlider, valueChanged( int ) )
    SETUP_VFILTER_OPTION( brightnessSlider, valueChanged( int ) )
    SETUP_VFILTER_OPTION( saturationSlider, valueChanged( int ) )
    SETUP_VFILTER_OPTION( gammaSlider, valueChanged( int ) )
    SETUP_VFILTER_OPTION( brightnessThresholdCheck, stateChanged( int ) )

    SETUP_VFILTER( extract )
    SETUP_VFILTER_OPTION( extractComponentText, textChanged( const QString& ) )

    SETUP_VFILTER( posterize )

    SETUP_VFILTER( colorthres )
    SETUP_VFILTER_OPTION( colorthresColorText, textChanged( const QString& ) )
    SETUP_VFILTER_OPTION( colorthresSaturationthresSlider, valueChanged( int ) )
    SETUP_VFILTER_OPTION( colorthresSimilaritythresSlider, valueChanged( int ) )

    SETUP_VFILTER( sepia )
    SETUP_VFILTER_OPTION( sepiaIntensitySpin, valueChanged( int ) )

    SETUP_VFILTER( invert )

    SETUP_VFILTER( gradient )
    SETUP_VFILTER_OPTION( gradientModeCombo, currentIndexChanged( QString ) )
    SETUP_VFILTER_OPTION( gradientTypeCheck, stateChanged( int ) )
    SETUP_VFILTER_OPTION( gradientCartoonCheck, stateChanged( int ) )

    SETUP_VFILTER( motionblur )
    SETUP_VFILTER_OPTION( blurFactorSlider, valueChanged( int ) )

    SETUP_VFILTER( motiondetect )

    SETUP_VFILTER( psychedelic )

    SETUP_VFILTER( sharpen )
    SETUP_VFILTER_OPTION( sharpenSigmaSlider, valueChanged( int ) )

    SETUP_VFILTER( ripple )

    SETUP_VFILTER( wave )

    SETUP_VFILTER( transform )
    SETUP_VFILTER_OPTION( transformTypeCombo, currentIndexChanged( QString ) )

    SETUP_VFILTER( rotate )
    SETUP_VFILTER_OPTION( rotateAngleDial, valueChanged( int ) )
    ui.rotateAngleDial->setWrapping( true );
    ui.rotateAngleDial->setNotchesVisible( true );

    SETUP_VFILTER( puzzle )
    SETUP_VFILTER_OPTION( puzzleRowsSpin, valueChanged( int ) )
    SETUP_VFILTER_OPTION( puzzleColsSpin, valueChanged( int ) )

    SETUP_VFILTER( magnify )

    SETUP_VFILTER( clone )
    SETUP_VFILTER_OPTION( cloneCountSpin, valueChanged( int ) )

    SETUP_VFILTER( wall )
    SETUP_VFILTER_OPTION( wallRowsSpin, valueChanged( int ) )
    SETUP_VFILTER_OPTION( wallColsSpin, valueChanged( int ) )


    SETUP_VFILTER( erase )
    SETUP_VFILTER_OPTION( eraseMaskText, editingFinished() )
    SETUP_VFILTER_OPTION( eraseYSpin, valueChanged( int ) )
    SETUP_VFILTER_OPTION( eraseXSpin, valueChanged( int ) )
    BUTTONACT( ui.eraseBrowseBtn, browseEraseFile() );

    SETUP_VFILTER( marq )
    SETUP_VFILTER_OPTION( marqMarqueeText, textChanged( const QString& ) )
    SETUP_VFILTER_OPTION( marqPositionCombo, currentIndexChanged( QString ) )

    SETUP_VFILTER( logo )
    SETUP_VFILTER_OPTION( logoFileText, editingFinished() )
    SETUP_VFILTER_OPTION( logoYSpin, valueChanged( int ) )
    SETUP_VFILTER_OPTION( logoXSpin, valueChanged( int ) )
    SETUP_VFILTER_OPTION( logoOpacitySlider, valueChanged( int ) )
    BUTTONACT( ui.logoBrowseBtn, browseLogo() );

    SETUP_VFILTER( gradfun )
    SETUP_VFILTER_OPTION( gradfunRadiusSlider, valueChanged( int ) )

    SETUP_VFILTER( grain )
    SETUP_VFILTER_OPTION( grainVarianceSlider, valueChanged( int ) )

    SETUP_VFILTER( mirror )

    SETUP_VFILTER( gaussianblur )
    SETUP_VFILTER_OPTION( gaussianblurSigmaSlider, valueChanged( int ) )

    SETUP_VFILTER( antiflicker )
    SETUP_VFILTER_OPTION( antiflickerSofteningSizeSlider, valueChanged( int ) )

    SETUP_VFILTER( hqdn3d )
    SETUP_VFILTER_OPTION( hqdn3dLumaSpatSlider, valueChanged( int ) )
    SETUP_VFILTER_OPTION( hqdn3dLumaTempSlider, valueChanged( int ) )
    SETUP_VFILTER_OPTION( hqdn3dChromaSpatSlider, valueChanged( int ) )
    SETUP_VFILTER_OPTION( hqdn3dChromaTempSlider, valueChanged( int ) )


    SETUP_VFILTER( anaglyph )

#undef SETUP_VFILTER
#undef SETUP_VFILTER_OPTION

    CONNECT( ui.cropTopPx, valueChanged( int ), this, cropChange() );
    CONNECT( ui.cropBotPx, valueChanged( int ), this, cropChange() );
    CONNECT( ui.cropLeftPx, valueChanged( int ), this, cropChange() );
    CONNECT( ui.cropRightPx, valueChanged( int ), this, cropChange() );
    CONNECT( ui.leftRightCropSync, toggled ( bool ), this, cropChange() );
    CONNECT( ui.topBotCropSync, toggled ( bool ), this, cropChange() );
    CONNECT( ui.topBotCropSync, toggled( bool ),
             ui.cropBotPx, setDisabled( bool ) );
    CONNECT( ui.leftRightCropSync, toggled( bool ),
             ui.cropRightPx, setDisabled( bool ) );
}

void ExtVideo::cropChange()
{
    if( ui.topBotCropSync->isChecked() )
        ui.cropBotPx->setValue( ui.cropTopPx->value() );
    if( ui.leftRightCropSync->isChecked() )
        ui.cropRightPx->setValue( ui.cropLeftPx->value() );

    PlayerController::VoutPtrList p_vouts = THEMIM->getVouts();
    for( auto p_vout: p_vouts )
    {
        var_SetInteger( p_vout.get(), "crop-top", ui.cropTopPx->value() );
        var_SetInteger( p_vout.get(), "crop-bottom", ui.cropBotPx->value() );
        var_SetInteger( p_vout.get(), "crop-left", ui.cropLeftPx->value() );
        var_SetInteger( p_vout.get(), "crop-right", ui.cropRightPx->value() );
    }
}

void ExtVideo::clean()
{
    ui.cropTopPx->setValue( 0 );
    ui.cropBotPx->setValue( 0 );
    ui.cropLeftPx->setValue( 0 );
    ui.cropRightPx->setValue( 0 );
}

static QString ChangeFiltersString( struct intf_thread_t *p_intf, const char *psz_filter_type, const char *psz_name, bool b_add )
{
    char* psz_chain = var_GetString( p_intf, psz_filter_type );

    QString const chain = QString( psz_chain ? psz_chain : "" );
    QStringList list = chain.split( ':', QString::SplitBehavior::SkipEmptyParts );

    if( b_add && std::find(list.begin(), list.end(), psz_name) == list.end() )
        list << psz_name;
    else if (!b_add)
        list.removeAll( psz_name );

    free( psz_chain );

    return list.join( ":" );
}

static void UpdateVFiltersString( struct intf_thread_t *p_intf,
                                  const char *psz_filter_type, const char *value )
{
    /* Try to set non splitter filters on the fly */
    if( strcmp( psz_filter_type, "video-splitter" ) )
    {
        auto p_vout = THEMIM->getVout();
        if( p_vout )
            var_SetString( p_vout.get(), psz_filter_type, value );
    }
}

void ExtVideo::changeVFiltersString( const char *psz_name, bool b_add )
{
    const char *psz_filter_type = GetVFilterType( p_intf, psz_name );
    if( psz_filter_type == NULL )
        return;

    QString result = ChangeFiltersString( p_intf, psz_filter_type, psz_name, b_add );

    emit configChanged( qfu( psz_filter_type ), result );

    UpdateVFiltersString( p_intf, psz_filter_type, qtu( result ) );
}

void ExtVideo::updateFilters()
{
    QString module = ModuleFromWidgetName( sender() );

    QCheckBox *checkbox = qobject_cast<QCheckBox*>( sender() );
    QGroupBox *groupbox = qobject_cast<QGroupBox*>( sender() );

    changeVFiltersString( qtu( module ),
                          checkbox ? checkbox->isChecked()
                                   : groupbox->isChecked() );
}

#define UPDATE_AND_APPLY_TEXT( widget, file ) \
    CONNECT( ui.widget, textChanged( const QString& ), \
             this, updateFilterOptions() ); \
    ui.widget->setText( toNativeSeparators( file ) ); \
    ui.widget->disconnect( SIGNAL( textChanged( const QString& ) ) );

void ExtVideo::browseLogo()
{
    const QStringList schemes = QStringList(QStringLiteral("file"));
    QString filter = QString( "%1 (*.png *.jpg);;%2 (*)" )
                        .arg( qtr("Image Files") )
                        .arg( TITLE_EXTENSIONS_ALL );
    QString file = QFileDialog::getOpenFileUrl( NULL, qtr( "Logo filenames" ),
                   p_intf->p_sys->filepath, filter,
                   NULL, QFileDialog::Options(), schemes ).toLocalFile();

    UPDATE_AND_APPLY_TEXT( logoFileText, file );
}

void ExtVideo::browseEraseFile()
{
    const QStringList schemes = QStringList(QStringLiteral("file"));
    QString filter = QString( "%1 (*.png *.jpg);;%2 (*)" )
                        .arg( qtr("Image Files") )
                        .arg( TITLE_EXTENSIONS_ALL );
    QString file = QFileDialog::getOpenFileUrl( NULL, qtr( "Image mask" ),
                   p_intf->p_sys->filepath, filter,
                   NULL, QFileDialog::Options(), schemes ).toLocalFile();

    UPDATE_AND_APPLY_TEXT( eraseMaskText, file );
}

#undef UPDATE_AND_APPLY_TEXT

void ExtVideo::initComboBoxItems( QObject *widget )
{
    QComboBox *combobox = qobject_cast<QComboBox*>( widget );
    if( !combobox ) return;

    QString option = OptionFromWidgetName( widget );
    module_config_t *p_item = config_FindConfig( qtu( option ) );
    if( p_item == NULL )
    {
        msg_Err( p_intf, "Couldn't find option \"%s\".", qtu( option ) );
        return;
    }

    if( p_item->i_type == CONFIG_ITEM_INTEGER
     || p_item->i_type == CONFIG_ITEM_BOOL )
    {
        int64_t *values;
        char **texts;
        ssize_t count = config_GetIntChoices( qtu( option ), &values, &texts );
        for( ssize_t i = 0; i < count; i++ )
        {
            combobox->addItem( qtr( texts[i] ), qlonglong(values[i]) );
            free( texts[i] );
        }
        free( texts );
        free( values );
    }
    else if( p_item->i_type == CONFIG_ITEM_STRING )
    {
        char **values;
        char **texts;
        ssize_t count = config_GetPszChoices( qtu( option ), &values, &texts );
        for( ssize_t i = 0; i < count; i++ )
        {
            combobox->addItem( qtr( texts[i] ), qfu(values[i]) );
            free( texts[i] );
            free( values[i] );
        }
        free( texts );
        free( values );
    }
}

void ExtVideo::setWidgetValue( QObject *widget )
{
    QString module = ModuleFromWidgetName( widget->parent() );
    //std::cout << "Module name: " << module.toStdString() << std::endl;
    QString option = OptionFromWidgetName( widget );
    //std::cout << "Option name: " << option.toStdString() << std::endl;

    vlc_value_t val;
    int i_type = config_GetType( qtu( option ) ) & VLC_VAR_CLASS;
    switch( i_type )
    {
        case VLC_VAR_INTEGER:
        case VLC_VAR_BOOL:
        case VLC_VAR_FLOAT:
        case VLC_VAR_STRING:
            break;
        default:
            msg_Err( p_intf,
                     "Module %s's %s variable is of an unsupported type ( %d )",
                     qtu( module ), qtu( option ), i_type );
            return;
    }
    if( var_Create( p_intf, qtu( option ), i_type | VLC_VAR_DOINHERIT ) )
        return;
    if( var_GetChecked( p_intf, qtu( option ), i_type, &val ) )
        return;

    /* Try to cast to all the widgets we're likely to encounter. Only
     * one of the casts is expected to work. */
    QSlider        *slider        = qobject_cast<QSlider*>       ( widget );
    QCheckBox      *checkbox      = qobject_cast<QCheckBox*>     ( widget );
    QSpinBox       *spinbox       = qobject_cast<QSpinBox*>      ( widget );
    QDoubleSpinBox *doublespinbox = qobject_cast<QDoubleSpinBox*>( widget );
    VLCQDial       *dial          = qobject_cast<VLCQDial*>      ( widget );
    QLineEdit      *lineedit      = qobject_cast<QLineEdit*>     ( widget );
    QComboBox      *combobox      = qobject_cast<QComboBox*>     ( widget );

    if( i_type == VLC_VAR_INTEGER || i_type == VLC_VAR_BOOL )
    {
        if( slider )        slider->setValue( val.i_int );
        else if( checkbox ) checkbox->setCheckState( val.i_int? Qt::Checked
                                                              : Qt::Unchecked );
        else if( spinbox )  spinbox->setValue( val.i_int );
        else if( dial )     dial->setValue( (360 - val.i_int) % 360 );
        else if( lineedit )
        {
            char str[30];
            snprintf( str, sizeof(str), "%06" PRIX64, val.i_int );
            lineedit->setText( str );
        }
        else if( combobox ) combobox->setCurrentIndex(
                            combobox->findData( qlonglong(val.i_int) ) );
        else msg_Warn( p_intf, "Could not find the correct Integer widget" );
    }
    else if( i_type == VLC_VAR_FLOAT )
    {
        if( slider ) slider->setValue( ( int )( val.f_float*( double )slider->tickInterval() ) ); /* hack alert! */
        else if( doublespinbox ) doublespinbox->setValue( val.f_float );
        else if( dial ) dial->setValue( (360 - lroundf(val.f_float)) % 360 );
        else msg_Warn( p_intf, "Could not find the correct Float widget" );
    }
    else if( i_type == VLC_VAR_STRING )
    {
        if( lineedit ) lineedit->setText( qfu( val.psz_string ) );
        else if( combobox ) combobox->setCurrentIndex(
                            combobox->findData( qfu( val.psz_string ) ) );
        else msg_Warn( p_intf, "Could not find the correct String widget" );
        free( val.psz_string );
    }
}

void ExtVideo::setFilterOption( const char *psz_module, const char *psz_option,
        int i_int, double f_float, const char *psz_string )
{
    auto p_vout = THEMIM->getVout();
    int i_type = 0;

    if( !p_vout )
        return;

    i_type = var_Type( p_vout.get(), psz_option );
    if( i_type == 0 )
        i_type = config_GetType( psz_option );

    vlc_value_t val;
    i_type &= VLC_VAR_CLASS;
    if( i_type == VLC_VAR_INTEGER || i_type == VLC_VAR_BOOL )
    {
        emit configChanged( qfu( psz_option ), QVariant( i_int ) );
        if( i_type == VLC_VAR_INTEGER )
            val.i_int = i_int;
        else
            val.b_bool = i_int;
    }
    else if( i_type == VLC_VAR_FLOAT )
    {
        emit configChanged( qfu( psz_option ), QVariant( f_float ) );
        val.f_float = f_float;
    }
    else if( i_type == VLC_VAR_STRING )
    {
        if( psz_string == NULL )
            psz_string = "";
        emit configChanged( qfu( psz_option ), QVariant( psz_string ) );
        val.psz_string = (char *) psz_string;
    }
    else
    {
        msg_Err( p_intf,
                 "Module %s's %s variable is of an unsupported type ( %d )",
                 psz_module,
                 psz_option,
                 i_type );
        return;
    }

    var_SetChecked( p_vout.get(), psz_option, i_type, val );
}

void ExtVideo::updateFilterOptions()
{
    QString module = ModuleFromWidgetName( sender()->parent() );
    //msg_Dbg( p_intf, "Module name: %s", qtu( module ) );
    QString option = OptionFromWidgetName( sender() );
    //msg_Dbg( p_intf, "Option name: %s", qtu( option ) );

    /* Try to cast to all the widgets we're likely to encounter. Only
     * one of the casts is expected to work. */
    QSlider        *slider        = qobject_cast<QSlider*>       ( sender() );
    QCheckBox      *checkbox      = qobject_cast<QCheckBox*>     ( sender() );
    QSpinBox       *spinbox       = qobject_cast<QSpinBox*>      ( sender() );
    QDoubleSpinBox *doublespinbox = qobject_cast<QDoubleSpinBox*>( sender() );
    VLCQDial       *dial          = qobject_cast<VLCQDial*>      ( sender() );
    QLineEdit      *lineedit      = qobject_cast<QLineEdit*>     ( sender() );
    QComboBox      *combobox      = qobject_cast<QComboBox*>     ( sender() );

    int i_int = -1;
    double f_float = -1.;
    QString val;

    if( slider ) {
        i_int = slider->value();
        f_float = ( double )slider->value() / ( double )slider->tickInterval(); /* hack alert! */
    }
    else if( checkbox ) i_int = checkbox->checkState() == Qt::Checked;
    else if( spinbox ) i_int = spinbox->value();
    else if( doublespinbox ) f_float = doublespinbox->value();
    else if( dial ) {
        i_int = (360 - dial->value()) % 360;
        f_float = i_int;
    }
    else if( lineedit ) {
        i_int = lineedit->text().toInt( NULL,16 );
        f_float = lineedit->text().toDouble();
        val = lineedit->text();
    }
    else if( combobox ) {
        i_int = combobox->itemData( combobox->currentIndex() ).toInt();
        val = combobox->itemData( combobox->currentIndex() ).toString();
    }

    setFilterOption( qtu( module ), qtu( option ), i_int, f_float, qtu( val ) );
}

/**********************************************************************
 * v4l2 controls
 **********************************************************************/

ExtV4l2::ExtV4l2( intf_thread_t *_p_intf, QWidget *_parent )
    : QWidget( _parent ), p_intf( _p_intf ), box( NULL )
{
    QVBoxLayout *layout = new QVBoxLayout( this );
    help = new QLabel( qtr("No v4l2 instance found.\n"
      "Please check that the device has been opened with VLC and is playing.\n\n"
      "Controls will automatically appear here.")
      , this );
    help->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
    help->setWordWrap( true );
    layout->addWidget( help );
    setLayout( layout );
}

void ExtV4l2::showEvent( QShowEvent *event )
{
    QWidget::showEvent( event );
    Refresh();
}

void ExtV4l2::Refresh( void )
{
    vlc_player_Lock(p_intf->p_sys->p_player);
    vlc_object_t *p_obj = vlc_player_GetV4l2Object(p_intf->p_sys->p_player);
    help->hide();
    if( box )
    {
        layout()->removeWidget( box );
        delete box;
        box = NULL;
    }

    if( p_obj != NULL )
    {
        vlc_value_t *val;
        char **text;
        size_t count;

        int i_ret = var_Change( p_obj, "controls", VLC_VAR_GETCHOICES,
                                &count, &val, &text );
        if( i_ret < 0 )
        {
            msg_Err( p_intf, "Oops, v4l2 object doesn't have a 'controls' variable." );
            help->show();
            return;
        }

        box = new QGroupBox( this );
        layout()->addWidget( box );
        QVBoxLayout *layout = new QVBoxLayout( box );
        box->setLayout( layout );

        for( size_t i = 0; i < count; i++ )
        {
            char *vartext, *psz_var = text[i];
            QString name;

            if( !var_Change( p_obj, psz_var, VLC_VAR_GETTEXT, &vartext ) )
            {
                name = qtr(vartext);
                free(vartext);
            }
            else
                name = qfu(psz_var);

            msg_Dbg( p_intf, "v4l2 control \"%" PRIx64 "\": %s (%s)",
                     val[i].i_int, psz_var, qtu( name ) );

            int i_type = var_Type( p_obj, psz_var );
            switch( i_type & VLC_VAR_TYPE )
            {
                case VLC_VAR_INTEGER:
                {
                    QLabel *label = new QLabel( name, box );
                    QHBoxLayout *hlayout = new QHBoxLayout();
                    hlayout->addWidget( label );
                    int i_val = var_GetInteger( p_obj, psz_var );
                    if( i_type & VLC_VAR_HASCHOICE )
                    {
                        QComboBox *combobox = new QComboBox( box );
                        combobox->setObjectName( qfu( psz_var ) );

                        vlc_value_t *val2;
                        char **text2;
                        size_t count2;

                        var_Change( p_obj, psz_var, VLC_VAR_GETCHOICES,
                                    &count2, &val2, &text2 );
                        for( size_t j = 0; j < count2; j++ )
                        {
                            combobox->addItem( text2[j],
                                       qlonglong( val2[j].i_int) );
                            if( i_val == val2[j].i_int )
                                combobox->setCurrentIndex( j );
                            free(text2[j]);
                        }
                        free(text2);
                        free(val2);

                        CONNECT( combobox, currentIndexChanged( int ), this,
                                 ValueChange( int ) );
                        hlayout->addWidget( combobox );
                    }
                    else
                    {
                        QSlider *slider = new QSlider( box );
                        slider->setObjectName( qfu( psz_var ) );
                        slider->setOrientation( Qt::Horizontal );
                        vlc_value_t val2;
                        var_Change( p_obj, psz_var, VLC_VAR_GETMIN, &val2 );
                        if( val2.i_int < INT_MIN )
                            val2.i_int = INT_MIN; /* FIXME */
                        slider->setMinimum( val2.i_int );
                        var_Change( p_obj, psz_var, VLC_VAR_GETMAX, &val2 );
                        if( val2.i_int > INT_MAX )
                            val2.i_int = INT_MAX; /* FIXME */
                        slider->setMaximum( val2.i_int );
                        if( !var_Change( p_obj, psz_var, VLC_VAR_GETSTEP,
                                         &val2 ) )
                            slider->setSingleStep( val2.i_int );
                        slider->setValue( i_val );
                        CONNECT( slider, valueChanged( int ), this,
                                 ValueChange( int ) );
                        hlayout->addWidget( slider );
                    }
                    layout->addLayout( hlayout );
                    break;
                }
                case VLC_VAR_BOOL:
                {
                    QCheckBox *button = new QCheckBox( name, box );
                    button->setObjectName( qfu( psz_var ) );
                    button->setChecked( var_GetBool( p_obj, psz_var ) );

                    CONNECT( button, clicked( bool ), this,
                             ValueChange( bool ) );
                    layout->addWidget( button );
                    break;
                }
                case VLC_VAR_VOID:
                {
                    if( i_type & VLC_VAR_ISCOMMAND )
                    {
                        QPushButton *button = new QPushButton( name, box );
                        button->setObjectName( qfu( psz_var ) );

                        CONNECT( button, clicked( bool ), this,
                                 ValueChange( bool ) );
                        layout->addWidget( button );
                    }
                    else
                    {
                        QLabel *label = new QLabel( name, box );
                        layout->addWidget( label );
                    }
                    break;
                }
                default:
                    msg_Warn( p_intf, "Unhandled var type for %s", psz_var );
                    break;
            }
            free(psz_var);
        }
        free(text);
        free(val);
    }
    else
    {
        msg_Dbg( p_intf, "Couldn't find v4l2 instance" );
        help->show();
        if ( isVisible() )
            QTimer::singleShot( 2000, this, SLOT(Refresh()) );
    }
    vlc_player_Unlock(p_intf->p_sys->p_player);
}

void ExtV4l2::ValueChange( bool value )
{
    ValueChange( (int)value );
}

void ExtV4l2::ValueChange( int value )
{
    QObject *s = sender();
    vlc_player_Lock(p_intf->p_sys->p_player);
    vlc_object_t *p_obj = vlc_player_GetV4l2Object(p_intf->p_sys->p_player);
    if( p_obj )
    {
        QString var = s->objectName();
        int i_type = var_Type( p_obj, qtu( var ) );
        switch( i_type & VLC_VAR_TYPE )
        {
            case VLC_VAR_INTEGER:
                if( i_type & VLC_VAR_HASCHOICE )
                {
                    QComboBox *combobox = qobject_cast<QComboBox*>( s );
                    value = combobox->itemData( value ).toInt();
                }
                var_SetInteger( p_obj, qtu( var ), value );
                break;
            case VLC_VAR_BOOL:
                var_SetBool( p_obj, qtu( var ), value );
                break;
            case VLC_VAR_VOID:
                var_TriggerCallback( p_obj, qtu( var ) );
                break;
        }
        vlc_player_Unlock(p_intf->p_sys->p_player);
    }
    else
    {
        vlc_player_Unlock(p_intf->p_sys->p_player);
        msg_Warn( p_intf, "Oops, v4l2 object isn't available anymore" );
        Refresh();
    }
}

/**********************************************************************
 * Sliders
 **********************************************************************/

FilterSliderData::FilterSliderData( QObject *parent, QSlider *_slider ) :
    QObject( parent ), slider( _slider )
{
}

FilterSliderData::FilterSliderData( QObject *parent,
                                    intf_thread_t *_p_intf,
                                    QSlider *_slider,
                                    QLabel *_label, QLabel *_nameLabel,
                                    const slider_data_t *_p_data ):
    QObject( parent ), slider( _slider ), valueLabel( _label ),
    nameLabel( _nameLabel ), p_data( _p_data ), p_intf( _p_intf )
{
    slider->setMinimum( p_data->f_min / p_data->f_resolution );
    slider->setMaximum( p_data->f_max / p_data->f_resolution );
    nameLabel->setText( p_data->descs );
    CONNECT( slider, valueChanged( int ), this, updateText( int ) );
    setValue( initialValue() );
    /* In case current == min|max text would not be first updated */
    if ( slider->value() == slider->maximum() ||
         slider->value() == slider->minimum() )
        updateText( slider->value() );
    CONNECT( slider, valueChanged( int ), this, onValueChanged( int ) );
}

void FilterSliderData::setValue( float f )
{
    slider->setValue( f / p_data->f_resolution );
}

void FilterSliderData::updateText( int i )
{
    float f = ((float) i) * p_data->f_resolution * p_data->f_visual_multiplier;
    valueLabel->setText( QString( p_data->units )
                    .prepend( "%1 " )
                    .arg( QString::number( f, 'f', 1 ) ) );
}

float FilterSliderData::initialValue()
{
    PlayerController::AoutPtr p_aout = THEMIM->getAout();
    float f = p_data->f_value;
    if( p_aout )
    {
        if ( var_Type( p_aout.get(), qtu(p_data->name) ) != 0 )
            return var_GetFloat( p_aout.get(), qtu(p_data->name) );
    }

    //* Not found, will try in config */
    if ( ! config_FindConfig( qtu(p_data->name) ) )
        return f;

    f = config_GetFloat( qtu(p_data->name) );
    return f;
}

void FilterSliderData::onValueChanged( int i )
{
    float f = ((float) i) * p_data->f_resolution;
    PlayerController::AoutPtr p_aout = THEMIM->getAout();
    if ( p_aout )
    {
        var_SetFloat( p_aout.get(), qtu(p_data->name), f );
    }
    writeToConfig();
}

void FilterSliderData::writeToConfig()
{
    float f = ((float) slider->value()) * p_data->f_resolution;
    emit configChanged( p_data->name, QVariant( f ) );
}

AudioFilterControlWidget::AudioFilterControlWidget
( intf_thread_t *_p_intf, QWidget *parent, const char *_name ) :
    QWidget( parent ), slidersBox( NULL ), p_intf( _p_intf ), name( _name ),
    i_smallfont(0)
{}

void AudioFilterControlWidget::connectConfigChanged( FilterSliderData *slider )
{
    connect( slider, SIGNAL( configChanged(QString, QVariant) ),
             this, SIGNAL( configChanged(QString, QVariant) ) );
}

void AudioFilterControlWidget::build()
{
    QFont smallFont = QApplication::font();
    smallFont.setPointSize( smallFont.pointSize() + i_smallfont );

    QVBoxLayout *layout = new QVBoxLayout( this );
    slidersBox = new QGroupBox( qtr( "Enable" ) );
    slidersBox->setCheckable( true );
    layout->addWidget( slidersBox );

    QGridLayout *ctrlLayout = new QGridLayout( slidersBox );

    int i = 0;
    foreach( const FilterSliderData::slider_data_t &data, controls )
    {
        QSlider *slider = new QSlider( Qt::Vertical );
        QLabel *valueLabel = new QLabel();
        valueLabel->setFont( smallFont );
        valueLabel->setAlignment( Qt::AlignHCenter );
        QLabel *nameLabel = new QLabel();
        nameLabel->setFont( smallFont );
        nameLabel->setAlignment( Qt::AlignHCenter );
        FilterSliderData *filter =
            new FilterSliderData( this, p_intf,
                                  slider, valueLabel, nameLabel, & data );
        ctrlLayout->addWidget( slider, 0, i, Qt::AlignHCenter );
        ctrlLayout->addWidget( valueLabel, 1, i, Qt::AlignHCenter );
        ctrlLayout->addWidget( nameLabel, 2, i, Qt::AlignHCenter );
        i++;
        sliderDatas << filter;
        connectConfigChanged( filter );
    }

    char *psz_af = var_InheritString( p_intf, "audio-filter" );

    if( psz_af && filterIsPresent( qfu(psz_af), name ) )
        slidersBox->setChecked( true );
    else
        slidersBox->setChecked( false );
    CONNECT( slidersBox, toggled(bool), this, enable(bool) );

    free( psz_af );
}

void AudioFilterControlWidget::enable( bool b_enable )
{
    module_t *p_obj = module_find( qtu(name) );
    if( !p_obj )
    {
        msg_Err( p_intf, "Unable to find filter module \"%s\".", qtu(name) );
        return;
    }

    QString result = ChangeFiltersString( p_intf, "audio-filter", qtu(name),
                                          b_enable );
    emit configChanged( qfu("audio-filter"), result );
    vlc_player_aout_EnableFilter( p_intf->p_sys->p_player, qtu(name), b_enable );
}

/**********************************************************************
 * Equalizer
 **********************************************************************/

EqualizerSliderData::EqualizerSliderData( QObject *parent, intf_thread_t *_p_intf,
                                          QSlider *slider, QLabel *_label,
                                          QLabel *_nameLabel, const slider_data_t *_p_data,
                                          int _index )
    : FilterSliderData( parent, slider ), index( _index )
{
    p_intf = _p_intf;
    valueLabel = _label;
    nameLabel = _nameLabel;
    p_data = _p_data;

    slider->setMinimum( p_data->f_min / p_data->f_resolution );
    slider->setMaximum( p_data->f_max / p_data->f_resolution );
    nameLabel->setText( p_data->descs );
    CONNECT( slider, valueChanged( int ), this, updateText( int ) );
    setValue( initialValue() );
    updateText( slider->value() );
    CONNECT( slider, valueChanged( int ), this, onValueChanged( int ) );
}

QStringList EqualizerSliderData::getBandsFromAout() const
{
    PlayerController::AoutPtr p_aout = THEMIM->getAout();
    QStringList bands;
    if( p_aout )
    {
        if ( var_Type( p_aout.get(), qtu(p_data->name) ) == VLC_VAR_STRING )
        {
            char *psz_bands = var_GetString( p_aout.get(), qtu(p_data->name) );
            if ( psz_bands )
            {
                bands = QString( psz_bands ).split( " ", QString::SkipEmptyParts );
                free( psz_bands );
            }
        }
    }

    if ( bands.count() ) return bands;
    /* Or try config then */

    if ( ! config_FindConfig( qtu(p_data->name) ) )
        return bands;

    char *psz_bands = config_GetPsz( qtu(p_data->name) );
    if ( psz_bands )
    {
        bands = QString( psz_bands ).split( " ", QString::SkipEmptyParts );
        free( psz_bands );
    }

    return bands;
}

float EqualizerSliderData::initialValue()
{
    float f = p_data->f_value;
    QStringList bands = getBandsFromAout();

    if ( bands.count() > index )
        f = QLocale( QLocale::C ).toFloat( bands[ index ] );

    return f;
}

void EqualizerSliderData::onValueChanged( int i )
{
    QStringList bands = getBandsFromAout();
    if ( bands.count() > index )
    {
        float f = ((float) i) * p_data->f_resolution;
        bands[ index ] = QLocale( QLocale::C ).toString( f );
        PlayerController::AoutPtr p_aout = THEMIM->getAout();
        if ( p_aout )
        {
            var_SetString( p_aout.get(), qtu(p_data->name), qtu(bands.join( " " )) );
        }
        writeToConfig();
    }
}

void EqualizerSliderData::writeToConfig()
{
    QStringList bands = getBandsFromAout();
    if ( bands.count() > index )
    {
        float f = (float) slider->value() * p_data->f_resolution;
        bands[ index ] = QLocale( QLocale::C ).toString( f );
        emit configChanged( p_data->name, QVariant( bands.join( " " ) ) );
    }
}

Equalizer::Equalizer( intf_thread_t *p_intf, QWidget *parent )
    : AudioFilterControlWidget( p_intf, parent, "equalizer" )
{
    i_smallfont = -3;
    bool b_vlcBands = var_InheritBool( p_intf, "equalizer-vlcfreqs" );

    const FilterSliderData::slider_data_t vlc_bands[10] =
    {
        { "equalizer-bands", qtr("60 Hz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("170 Hz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("310 Hz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("600 Hz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("1 KHz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("3 KHz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("6 KHz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("12 KHz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("14 KHz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("16 KHz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
    };
    const FilterSliderData::slider_data_t iso_bands[10] =
    {
        { "equalizer-bands", qtr("31 Hz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("63 Hz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("125 Hz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("250 Hz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("500 Hz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("1 KHz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("2 KHz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("4 KHz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("8 KHz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("16 KHz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
    };
    const FilterSliderData::slider_data_t preamp_vals =
        { "equalizer-preamp", qtr("Preamp"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 };

    for( int i=0; i<10 ;i++ ) controls.append( (b_vlcBands) ? vlc_bands[i] : iso_bands[i] );
    preamp_values = preamp_vals;
    build();
}

void Equalizer::build()
{
    QFont smallFont = QApplication::font();
    smallFont.setPointSize( smallFont.pointSize() + i_smallfont );

    Ui::EqualizerWidget ui;
    ui.setupUi( this );

    QGridLayout *ctrlLayout = new QGridLayout( ui.slidersPlaceholder );

    /* set up preamp control */
    ui.preampLabel->setFont( smallFont );
    ui.preampValue->setFont( smallFont );
    preamp = new FilterSliderData( this, p_intf,
        ui.preampSlider, ui.preampValue, ui.preampLabel, & preamp_values );
    connectConfigChanged( preamp );

    /* fix sliders spacing accurately */
    int i_width = qMax( QFontMetrics( smallFont ).horizontalAdvance( "500 Hz" ),
                        QFontMetrics( smallFont ).horizontalAdvance( "-20.0 dB" ) );
    int i = 0;
    foreach( const FilterSliderData::slider_data_t &data, controls )
    {
        QSlider *slider = new QSlider( Qt::Vertical );
        slider->setMinimumWidth( i_width );
        QLabel *valueLabel = new QLabel();
        valueLabel->setFont( smallFont );
        valueLabel->setAlignment( Qt::AlignHCenter );
        QLabel *nameLabel = new QLabel();
        nameLabel->setFont( smallFont );
        nameLabel->setAlignment( Qt::AlignHCenter );
        EqualizerSliderData *filter =
            new EqualizerSliderData( this, p_intf,
                                     slider, valueLabel, nameLabel, & data, i );
        ctrlLayout->addWidget( slider, 0, i, Qt::AlignHCenter );
        ctrlLayout->addWidget( valueLabel, 2, i, Qt::AlignHCenter );
        ctrlLayout->addWidget( nameLabel, 1, i, Qt::AlignHCenter );
        sliderDatas << filter; /* keep track for applying presets */
        i++;
        connectConfigChanged( filter );
    }

    /* Add the listed presets */
    ui.presetsCombo->addItem( "", QVariant() ); /* 1st entry = custom/modified */
    for( i = 0 ; i < NB_PRESETS ; i ++ )
    {
        QGraphicsScene scene;
#if HAS_QT56
        qreal f_ratio = QApplication::primaryScreen()->devicePixelRatio();
        QPixmap icon( 40 * f_ratio, 40 * f_ratio );
#else
        QPixmap icon( 40, 40 );
#endif
        icon.fill( Qt::transparent );
        QPainter painter( &icon );
        for ( int j = 0; j < eqz_preset_10b[i].i_band; j++ )
        {
            float f_value = eqz_preset_10b[i].f_amp[j];
            if ( f_value > 20.0 ) f_value = 20.0;
            if ( f_value < -20.0 ) f_value = -20.0;
            QRectF shape( j, 20.0 - f_value, 1, f_value );
            scene.addRect( shape, QPen(), palette().brush( QPalette::WindowText ) );
        }
        scene.addLine( 0.0, 20.0, eqz_preset_10b[i].i_band, 20.0,
                       palette().color( QPalette::WindowText ) );
        scene.setSceneRect( 0.0, 0.0, eqz_preset_10b[i].i_band , 40.0 );
        scene.render( &painter, icon.rect(), scene.sceneRect(), Qt::IgnoreAspectRatio );
        ui.presetsCombo->addItem( icon, qtr( preset_list_text[i] ),
                                     QVariant( preset_list[i] ) );
    }
    CONNECT( ui.presetsCombo, activated(int), this, setCorePreset(int) );

    /* Set enable checkbox */
    PlayerController::AoutPtr p_aout = THEMIM->getAout();
    char *psz_af;
    if( p_aout )
        psz_af = var_GetNonEmptyString( p_aout.get(), "audio-filter" );
    else
        psz_af = var_InheritString( p_intf, "audio-filter" );

    /* To enable or disable subwidgets */
    /* If that list grows, better iterate over layout's childs */
    CONNECT( ui.enableCheck, toggled(bool), ui.presetsCombo, setEnabled(bool) );
    CONNECT( ui.enableCheck, toggled(bool), ui.presetLabel, setEnabled(bool) );
    CONNECT( ui.enableCheck, toggled(bool), ui.eq2PassCheck, setEnabled(bool) );
    CONNECT( ui.enableCheck, toggled(bool), ui.slidersPlaceholder, setEnabled(bool) );
    CONNECT( ui.enableCheck, toggled(bool), ui.preampSlider, setEnabled(bool) );
    CONNECT( ui.enableCheck, toggled(bool), ui.preampValue, setEnabled(bool) );
    CONNECT( ui.enableCheck, toggled(bool), ui.preampLabel, setEnabled(bool) );

    if( psz_af && filterIsPresent( qfu(psz_af), name ) )
        ui.enableCheck->setChecked( true );
    else
        ui.enableCheck->setChecked( false );

    /* workaround for non emitted toggle() signal */
    ui.enableCheck->toggle(); ui.enableCheck->toggle();

    free( psz_af );
    CONNECT( ui.enableCheck, toggled(bool), this, enable(bool) );

    /* Connect and set 2 Pass checkbox */
    ui.eq2PassCheck->setChecked( var_InheritBool( p_aout.get(), "equalizer-2pass" ) );
    CONNECT( ui.eq2PassCheck, toggled(bool), this, enable2Pass(bool) );
}

void Equalizer::setCorePreset( int i_preset )
{
    if( i_preset < 1 )
        return;

    i_preset--;/* 1st in index was an empty entry */

    preamp->setValue( eqz_preset_10b[i_preset].f_preamp );
    for ( int i=0; i< qMin( eqz_preset_10b[i_preset].i_band,
                            sliderDatas.count() ) ; i++ )
        sliderDatas[i]->setValue( eqz_preset_10b[i_preset].f_amp[i] );

    PlayerController::AoutPtr p_aout = THEMIM->getAout();
    if( p_aout )
    {
        var_SetString( p_aout.get() , "equalizer-preset" , preset_list[i_preset] );
    }
    emit configChanged( qfu( "equalizer-preset" ), QVariant( qfu( preset_list[i_preset] ) ) );
}

/* Function called when the set2Pass button is activated */
void Equalizer::enable2Pass( bool b_enable )
{
    PlayerController::AoutPtr p_aout= THEMIM->getAout();

    if( p_aout )
    {
        var_SetBool( p_aout.get(), "equalizer-2pass", b_enable );
    }
    emit configChanged( qfu( "equalizer-2pass" ), QVariant( b_enable ) );
}

/**********************************************************************
 * Audio filters
 **********************************************************************/

/**********************************************************************
 * Dynamic range compressor
 **********************************************************************/

Compressor::Compressor( intf_thread_t *p_intf, QWidget *parent )
    : AudioFilterControlWidget( p_intf, parent, "compressor" )
{
    i_smallfont = -2;
    const FilterSliderData::slider_data_t a[7] =
    {
        { "compressor-rms-peak",    qtr("RMS/peak"),         "",       0.0f,   1.0f,   0.00f, 0.001f, 1.0 },
        { "compressor-attack",      qtr("Attack"),       qtr("ms"),   1.5f, 400.0f,  25.00f, 0.100f, 1.0 },
        { "compressor-release",     qtr("Release"),      qtr("ms"),   2.0f, 800.0f, 100.00f, 0.100f, 1.0 },
        { "compressor-threshold",   qtr("Threshold"),    qtr("dB"), -30.0f,   0.0f, -11.00f, 0.010f, 1.0 },
        { "compressor-ratio",       qtr("Ratio"),            ":1",     1.0f,  20.0f,   8.00f, 0.010f, 1.0 },
        { "compressor-knee",        qtr("Knee\nradius"), qtr("dB"),   1.0f,  10.0f,   2.50f, 0.010f, 1.0 },
        { "compressor-makeup-gain", qtr("Makeup\ngain"), qtr("dB"),   0.0f,  24.0f,   7.00f, 0.010f, 1.0 },
    };
    for( int i=0; i<7 ;i++ ) controls.append( a[i] );
    build();
}

/**********************************************************************
 * Spatializer
 **********************************************************************/

Spatializer::Spatializer( intf_thread_t *p_intf, QWidget *parent )
    : AudioFilterControlWidget( p_intf, parent, "spatializer" )
{
    i_smallfont = -1;
    const FilterSliderData::slider_data_t a[5] =
    {
        { "spatializer-roomsize",   qtr("Size"),    "", 0.0f, 1.1f, 0.85f, 0.1f, 10.0 },
        { "spatializer-width",      qtr("Width"),   "", 0.0f, 1.0f, 1.0f, 0.1f, 10.0 },
        { "spatializer-wet",        qtr("Wet"),     "", 0.0f, 1.0f, 0.4f, 0.1f, 10.0 },
        { "spatializer-dry",        qtr("Dry"),     "", 0.0f, 1.0f, 0.5f, 0.1f, 10.0 },
        { "spatializer-damp",       qtr("Damp"),    "", 0.0f, 1.0f, 0.5f, 0.1f, 10.0 },
    };
    for( int i=0; i<5 ;i++ ) controls.append( a[i] );
    build();
}

/**********************************************************************
 * Spatializer
 **********************************************************************/

StereoWidener::StereoWidener( intf_thread_t *p_intf, QWidget *parent )
    : AudioFilterControlWidget( p_intf, parent, "stereo_widen" )
{
    i_smallfont = -1;
    const FilterSliderData::slider_data_t a[4] =
    {
        { "stereowiden-delay",     qtr("Delay time"),    "ms", 1.0, 100,  20, 1.0, 1.0 },
        { "stereowiden-feedback",  qtr("Feedback gain"), "%",  0.0, 0.9, 0.3, 0.1, 1.0 },
        { "stereowiden-crossfeed", qtr("Crossfeed"),     "%",  0.0, 0.8, 0.3, 0.1, 1.0 },
        { "stereowiden-dry-mix",   qtr("Dry mix"),       "%",  0.0, 1.0, 0.8, 0.1, 1.0 },
    };
    for( int i=0; i<4 ;i++ ) controls.append( a[i] );
    build();
}

/**********************************************************************
 * Advanced
 **********************************************************************/

PitchShifter::PitchShifter( intf_thread_t *p_intf, QWidget *parent )
    : AudioFilterControlWidget( p_intf, parent, "scaletempo_pitch" )
{
    i_smallfont = -1;
    controls.append( { "pitch-shift", qtr("Adjust pitch"), "semitones",
                        -12.0, 12.0, 0.0, 0.25, 1.0 } );
    build();
}

#include <QToolButton>
#include <QGridLayout>

#define SUBSDELAY_CFG_MODE                     "subsdelay-mode"
#define SUBSDELAY_CFG_FACTOR                   "subsdelay-factor"
#define SUBSDELAY_MODE_ABSOLUTE                0
#define SUBSDELAY_MODE_RELATIVE_SOURCE_DELAY   1
#define SUBSDELAY_MODE_RELATIVE_SOURCE_CONTENT 2

SyncWidget::SyncWidget( QWidget *_parent ) : QWidget( _parent )
{
    QHBoxLayout *layout = new QHBoxLayout;
    spinBox.setAlignment( Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter );
    spinBox.setDecimals( 3 );
    spinBox.setMinimum( -600.0 );
    spinBox.setMaximum( 600.0 );
    spinBox.setSingleStep( 0.1 );
    spinBox.setSuffix( " s" );
    spinBox.setButtonSymbols( QDoubleSpinBox::PlusMinus );
    CONNECT( &spinBox, valueChanged( double ), this, valueChangedHandler( double ) );
    layout->addWidget( &spinBox );
    layout->addWidget( &spinLabel );
    layout->setContentsMargins( 0, 0, 0, 0 );
    setLayout( layout );
}

void SyncWidget::valueChangedHandler( double d )
{
    if ( d < 0 )
        spinLabel.setText( qtr("(Hastened)") );
    else if ( d > 0 )
        spinLabel.setText( qtr("(Delayed)") );
    else
        spinLabel.setText( "" );
    emit valueChanged( d );
}

void SyncWidget::setValue( double d )
{
    spinBox.setValue( d );
}

SyncControls::SyncControls( intf_thread_t *_p_intf, QWidget *_parent )
    : QWidget( _parent )
    , p_intf( _p_intf )
    , m_SubsDelayCfgFactor(p_intf, SUBSDELAY_CFG_FACTOR)
{
    QGroupBox *AVBox, *subsBox;
    QToolButton *updateButton;

    b_userAction = true;

    QGridLayout *mainLayout = new QGridLayout( this );

    /* AV sync */
    AVBox = new QGroupBox( qtr( "Audio/Video" ) );
    QGridLayout *AVLayout = new QGridLayout( AVBox );

    QLabel *AVLabel = new QLabel;
    AVLabel->setText( qtr( "Audio track synchronization:" ) );
    AVLayout->addWidget( AVLabel, 0, 0, 1, 1 );

    AVSpin = new SyncWidget( this );
    AVLayout->addWidget( AVSpin, 0, 2, 1, 1 );
    mainLayout->addWidget( AVBox, 1, 0, 1, 5 );

    /* Subs */
    subsBox = new QGroupBox( qtr( "Subtitles/Video" ) );
    QGridLayout *subsLayout = new QGridLayout( subsBox );

    QGroupBox *subsTrackSyncBox = new QGroupBox( qtr( "Subtitle track synchronization" ) );
    QGridLayout *synchronizationLayout = new QGridLayout( subsTrackSyncBox );

    for( int i = 0; i < 2; i++ )
    {
        QLabel *subsLabel = new QLabel;
        subsLabel->setText( i == 0 ? qtr("Primary subtitle track:") : qtr("Secondary subtitle track:") );
        synchronizationLayout->addWidget( subsLabel, i, 0, 1, 1 );
        SyncWidget **p_subsSpin = i == 0 ? &subsSpin : &secondarySubsSpin;

        *p_subsSpin = new SyncWidget( this );
        synchronizationLayout->addWidget( *p_subsSpin, i, 2, 1, 1 );
    }

    subsLayout->addWidget( subsTrackSyncBox, 0, 0, 2, 5 );

    QLabel *subSpeedLabel = new QLabel;
    subSpeedLabel->setText( qtr( "Subtitle speed:" ) );
    subsLayout->addWidget( subSpeedLabel, 2, 0, 1, 1 );

    subSpeedSpin = new QDoubleSpinBox;
    subSpeedSpin->setAlignment( Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter );
    subSpeedSpin->setDecimals( 3 );
    subSpeedSpin->setMinimum( 1 );
    subSpeedSpin->setMaximum( 100 );
    subSpeedSpin->setSingleStep( 0.2 );
    subSpeedSpin->setSuffix( " fps" );
    subSpeedSpin->setButtonSymbols( QDoubleSpinBox::PlusMinus );
    subsLayout->addWidget( subSpeedSpin, 2, 2, 1, 1 );

    QLabel *subDurationLabel = new QLabel;
    subDurationLabel->setText( qtr( "Subtitle duration factor:" ) );
    subsLayout->addWidget( subDurationLabel, 3, 0, 1, 1 );

    subDurationSpin = new QDoubleSpinBox;
    subDurationSpin->setAlignment( Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter );
    subDurationSpin->setDecimals( 3 );
    subDurationSpin->setMinimum( 0 );
    subDurationSpin->setMaximum( 20 );
    subDurationSpin->setSingleStep( 0.2 );
    subDurationSpin->setButtonSymbols( QDoubleSpinBox::PlusMinus );
    subsLayout->addWidget( subDurationSpin, 3, 2, 1, 1 );

    mainLayout->addWidget( subsBox, 2, 0, 2, 5 );

    updateButton = new QToolButton;
    updateButton->setAutoRaise( true );
    mainLayout->addWidget( updateButton, 0, 4, 1, 1 );

    /* Various Connects */
    connect( AVSpin, &SyncWidget::valueChanged, this, &SyncControls::advanceAudio ) ;
    connect( subsSpin, &SyncWidget::valueChanged, this, &SyncControls::advanceSubs ) ;
    connect( secondarySubsSpin, &SyncWidget::valueChanged, this, &SyncControls::advanceSecondarySubs ) ;
    connect( subSpeedSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SyncControls::adjustSubsSpeed);
    connect( subDurationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SyncControls::adjustSubsDuration);

    connect( THEMIM, &PlayerController::subtitleDelayChanged, [this](vlc_tick_t value) {
        b_userAction = false;
        subsSpin->setValue(secf_from_vlc_tick(value));
        b_userAction = true;
    });
    connect( THEMIM, &PlayerController::secondarySubtitleDelayChanged, [this](vlc_tick_t value) {
        b_userAction = false;
        secondarySubsSpin->setValue(secf_from_vlc_tick(value));
        b_userAction = true;
    });
    connect( THEMIM, &PlayerController::audioDelayChanged, [this](vlc_tick_t value) {
        b_userAction = false;
        AVSpin->setValue(secf_from_vlc_tick(value));
        b_userAction = true;
    });
    connect( THEMIM, &PlayerController::subtitleFPSChanged, subSpeedSpin, &QDoubleSpinBox::setValue );
    connect( &m_SubsDelayCfgFactor, &QVLCFloat::valueChanged, subDurationSpin, &QDoubleSpinBox::setValue);

    BUTTON_SET_ACT_I( updateButton, "", update,
            qtr( "Force update of this dialog's values" ), update() );

    initSubsDuration();

    /* Set it */
    update();
}

SyncControls::~SyncControls()
{
    subsdelayClean();
}

void SyncControls::clean()
{
    b_userAction = false;
    AVSpin->setValue( 0.0 );
    subsSpin->setValue( 0.0 );
    secondarySubsSpin->setValue( 0.0 );
    subSpeedSpin->setValue( 1.0 );
    subsdelayClean();
    b_userAction = true;
}

void SyncControls::update()
{
    b_userAction = false;
    if( THEMIM->getInput() )
    {
        subsSpin->setValue(secf_from_vlc_tick(THEMIM->getSubtitleDelay()));
        secondarySubsSpin->setValue(secf_from_vlc_tick(THEMIM->getSecondarySubtitleDelay()));
        AVSpin->setValue(secf_from_vlc_tick(THEMIM->getAudioDelay()));
        subSpeedSpin->setValue(THEMIM->getSubtitleFPS());
        subDurationSpin->setValue(m_SubsDelayCfgFactor.getValue());
    }
    b_userAction = true;
}

void SyncControls::advanceAudio( double f_advance )
{
    if( THEMIM->getInput() && b_userAction )
    {
        vlc_tick_t i_delay = vlc_tick_from_sec( f_advance );
        THEMIM->setAudioDelay( i_delay );
    }
}

void SyncControls::advanceSubs( double f_advance )
{
    if( THEMIM->getInput() && b_userAction )
    {
        vlc_tick_t i_delay = vlc_tick_from_sec( f_advance );
        THEMIM->setSubtitleDelay( i_delay );
    }
}

void SyncControls::advanceSecondarySubs( double f_advance )
{
    if( THEMIM->getInput() && b_userAction )
    {
        vlc_tick_t i_delay = vlc_tick_from_sec( f_advance );
        THEMIM->setSecondarySubtitleDelay( i_delay );
    }
}

void SyncControls::adjustSubsSpeed( double f_fps )
{
    if( THEMIM->getInput() && b_userAction )
        THEMIM->setSubtitleFPS( f_fps );
}

void SyncControls::adjustSubsDuration( double f_factor )
{
    if( THEMIM->getInput() && b_userAction )
    {
        subsdelaySetFactor( f_factor );
        changeVFiltersString( "subsdelay", f_factor > 0 );
    }
}

void SyncControls::initSubsDuration()
{
    int i_mode = var_InheritInteger( p_intf, SUBSDELAY_CFG_MODE );

    switch (i_mode)
    {
    default:
    case SUBSDELAY_MODE_ABSOLUTE:
        subDurationSpin->setToolTip( qtr( "Extend subtitle duration by this value.\n"
                                          "Set 0 to disable." ) );
        subDurationSpin->setSuffix( " s" );
        break;
    case SUBSDELAY_MODE_RELATIVE_SOURCE_DELAY:
        subDurationSpin->setToolTip( qtr( "Multiply subtitle duration by this value.\n"
                                          "Set 0 to disable." ) );
        subDurationSpin->setSuffix( "" );
        break;
    case SUBSDELAY_MODE_RELATIVE_SOURCE_CONTENT:
        subDurationSpin->setToolTip( qtr( "Recalculate subtitle duration according\n"
                                          "to their content and this value.\n"
                                          "Set 0 to disable." ) );
        subDurationSpin->setSuffix( "" );
        break;
    }
}

void SyncControls::subsdelayClean()
{
    /* Remove subsdelay filter */
    changeVFiltersString( "subsdelay", false );
}

void SyncControls::subsdelaySetFactor( double f_factor )
{
    PlayerController::VoutPtrList p_vouts = THEMIM->getVouts();
    for( auto p_vout: p_vouts )
    {
        var_SetFloat( p_vout.get(), SUBSDELAY_CFG_FACTOR, f_factor );
    }
}

void SyncControls::changeVFiltersString( const char *psz_name, bool b_add )
{
    const char *psz_filter_type = GetVFilterType( p_intf, psz_name );
    if( psz_filter_type == NULL )
        return;

    QString result = ChangeFiltersString( p_intf, psz_filter_type, psz_name, b_add );

    UpdateVFiltersString( p_intf, psz_filter_type, qtu( result ) );
}


/**********************************************************************
 * Video filters / Adjust
 **********************************************************************/

/**********************************************************************
 * Extended playbak controls
 **********************************************************************/
