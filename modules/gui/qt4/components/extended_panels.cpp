/*****************************************************************************
 * extended_panels.cpp : Extended controls panels
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Antoine Cellerier <dionoea .t videolan d@t org>
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

#define __STDC_FORMAT_MACROS 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QLabel>
#include <QVariant>
#include <QString>
#include <QFont>
#include <QGridLayout>
#include <QSignalMapper>
#include <QComboBox>
#include <QTimer>
#include <QFileDialog>

#include "components/extended_panels.hpp"
#include "dialogs/preferences.hpp"
#include "qt4.hpp"
#include "input_manager.hpp"
#include "util/qt_dirs.hpp"

#include "../../audio_filter/equalizer_presets.h"
#include <vlc_aout_intf.h>
#include <vlc_intf_strings.h>
#include <vlc_vout.h>
#include <vlc_osd.h>
#include <vlc_modules.h>

#include <vlc_charset.h> /* us_strtod */

static void ChangeVFiltersString( struct intf_thread_t *p_intf, const char *psz_name, bool b_add );

#if 0
class ConfClickHandler : public QObject
{
public:
    ConfClickHandler( intf_thread_t *_p_intf, ExtVideo *_e ) : QObject ( _e ) {
        e = _e; p_intf = _p_intf;
    }
    virtual ~ConfClickHandler() {}
    bool eventFilter( QObject *obj, QEvent *evt )
    {
        if( evt->type() == QEvent::MouseButtonPress )
        {
            e->gotoConf( obj );
            return true;
        }
        return false;
    }
private:
    ExtVideo* e;
    intf_thread_t *p_intf;
};
#endif

const QString ModuleFromWidgetName( QObject *obj )
{
    return obj->objectName().replace( "Enable","" );
}

QString OptionFromWidgetName( QObject *obj )
{
    /* Gruik ? ... nah */
    QString option = obj->objectName().replace( "Slider", "" )
                                      .replace( "Combo" , "" )
                                      .replace( "Dial"  , "" )
                                      .replace( "Check" , "" )
                                      .replace( "Spin"  , "" )
                                      .replace( "Text"  , "" );
    for( char a = 'A'; a <= 'Z'; a++ )
    {
        option = option.replace( QString( a ),
                                 QString( '-' ) + QString( a + 'a' - 'A' ) );
    }
    return option;
}

ExtVideo::ExtVideo( intf_thread_t *_p_intf, QTabWidget *_parent ) :
            QObject( _parent ), p_intf( _p_intf )
{
    ui.setupUi( _parent );

#define SETUP_VFILTER( widget ) \
    { \
        vlc_object_t *p_obj = ( vlc_object_t * ) \
            vlc_object_find_name( p_intf->p_libvlc, \
                                  #widget ); \
        QCheckBox *checkbox = qobject_cast<QCheckBox*>( ui.widget##Enable ); \
        QGroupBox *groupbox = qobject_cast<QGroupBox*>( ui.widget##Enable ); \
        if( p_obj ) \
        { \
            vlc_object_release( p_obj ); \
            if( checkbox ) checkbox->setChecked( true ); \
            else if (groupbox) groupbox->setChecked( true ); \
        } \
        else \
        { \
            if( checkbox ) checkbox->setChecked( false ); \
            else if (groupbox)  groupbox->setChecked( false ); \
        } \
    } \
    CONNECT( ui.widget##Enable, clicked(), this, updateFilters() );


#define SETUP_VFILTER_OPTION( widget, signal ) \
    initComboBoxItems( ui.widget ); \
    setWidgetValue( ui.widget ); \
    CONNECT( ui.widget, signal, this, updateFilterOptions() );

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
    SETUP_VFILTER_OPTION( puzzleBlackSlotCheck, stateChanged( int ) )

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
    SETUP_VFILTER_OPTION( gaussianbluSigmaSlider, valueChanged( int ) )

    SETUP_VFILTER( antiflicker )
    SETUP_VFILTER_OPTION( antiflickerSofteningSizeSlider, valueChanged( int ) )


    if( module_exists( "atmo" ) )
    {
        SETUP_VFILTER( atmo )
        SETUP_VFILTER_OPTION( atmoEdgeweightningSlider, valueChanged( int ) )
        SETUP_VFILTER_OPTION( atmoBrightnessSlider, valueChanged( int ) )
        SETUP_VFILTER_OPTION( atmoDarknesslimitSlider, valueChanged( int ) )
        SETUP_VFILTER_OPTION( atmoMeanlengthSlider, valueChanged( int ) )
        SETUP_VFILTER_OPTION( atmoMeanthresholdSlider, valueChanged( int ) )
        SETUP_VFILTER_OPTION( atmoPercentnewSlider, valueChanged( int ) )
        SETUP_VFILTER_OPTION( atmoFiltermodeCombo, currentIndexChanged( int ) )
        SETUP_VFILTER_OPTION( atmoShowdotsCheck, stateChanged( int ) )
    }
    else
    {
        _parent->removeTab( _parent->indexOf( ui.tab_atmo ) );
    }

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

    vout_thread_t *p_vout = THEMIM->getVout();
    if( p_vout )
    {
        var_SetInteger( p_vout, "crop-top", ui.cropTopPx->value() );
        var_SetInteger( p_vout, "crop-bottom", ui.cropBotPx->value() );
        var_SetInteger( p_vout, "crop-left", ui.cropLeftPx->value() );
        var_SetInteger( p_vout, "crop-right", ui.cropRightPx->value() );
        vlc_object_release( p_vout );
    }
}

void ExtVideo::clean()
{
    ui.cropTopPx->setValue( 0 );
    ui.cropBotPx->setValue( 0 );
    ui.cropLeftPx->setValue( 0 );
    ui.cropRightPx->setValue( 0 );
}

static void ChangeVFiltersString( struct intf_thread_t *p_intf, const char *psz_name, bool b_add )
{
    char *psz_parser, *psz_string;
    const char *psz_filter_type;

    module_t *p_obj = module_find( psz_name );
    if( !p_obj )
    {
        msg_Err( p_intf, "Unable to find filter module \"%s\".", psz_name );
        return;
    }

    if( module_provides( p_obj, "video splitter" ) )
    {
        psz_filter_type = "video-splitter";
    }
    else if( module_provides( p_obj, "video filter2" ) )
    {
        psz_filter_type = "video-filter";
    }
    else if( module_provides( p_obj, "sub source" ) )
    {
        psz_filter_type = "sub-source";
    }
    else if( module_provides( p_obj, "sub filter" ) )
    {
        psz_filter_type = "sub-filter";
    }
    else
    {
        msg_Err( p_intf, "Unknown video filter type." );
        return;
    }

    psz_string = config_GetPsz( p_intf, psz_filter_type );

    if( !psz_string ) psz_string = strdup( "" );

    psz_parser = strstr( psz_string, psz_name );

    if( b_add )
    {
        if( !psz_parser )
        {
            psz_parser = psz_string;
            if( asprintf( &psz_string, ( *psz_string ) ? "%s:%s" : "%s%s",
                            psz_string, psz_name ) == -1 )
            {
                free( psz_parser );
                return;
            }
            free( psz_parser );
        }
        else
        {
            free( psz_string );
            return;
        }
    }
    else
    {
        if( psz_parser )
        {
            if( *( psz_parser + strlen( psz_name ) ) == ':' )
            {
                memmove( psz_parser, psz_parser + strlen( psz_name ) + 1,
                         strlen( psz_parser + strlen( psz_name ) + 1 ) + 1 );
            }
            else
            {
                *psz_parser = '\0';
            }

            /* Remove trailing : : */
            if( strlen( psz_string ) > 0 &&
                *( psz_string + strlen( psz_string ) -1 ) == ':' )
            {
                *( psz_string + strlen( psz_string ) -1 ) = '\0';
            }
        }
        else
        {
            free( psz_string );
            return;
        }
    }
    /* Vout is not kept, so put that in the config */
    config_PutPsz( p_intf, psz_filter_type, psz_string );

    /* Try to set on the fly */
    if( !strcmp( psz_filter_type, "video-splitter" ) )
    {
        playlist_t *p_playlist = pl_Get( p_intf );
        var_SetString( p_playlist, psz_filter_type, psz_string );
    }
    else
    {
        vout_thread_t *p_vout = THEMIM->getVout();
        if( p_vout )
        {
            var_SetString( p_vout, psz_filter_type, psz_string );
            vlc_object_release( p_vout );
        }
    }

    free( psz_string );
}

void ExtVideo::updateFilters()
{
    QString module = ModuleFromWidgetName( sender() );

    QCheckBox *checkbox = qobject_cast<QCheckBox*>( sender() );
    QGroupBox *groupbox = qobject_cast<QGroupBox*>( sender() );

    ChangeVFiltersString( p_intf, qtu( module ),
                          checkbox ? checkbox->isChecked()
                                   : groupbox->isChecked() );
}

void ExtVideo::browseLogo()
{
    QString file = QFileDialog::getOpenFileName( NULL, qtr( "Logo filenames" ),
                   p_intf->p_sys->filepath, "Images (*.png *.jpg);;All (*)" );
    ui.logoFileText->setText( toNativeSeparators( file ) );
}

void ExtVideo::browseEraseFile()
{
    QString file = QFileDialog::getOpenFileName( NULL, qtr( "Image mask" ),
                   p_intf->p_sys->filepath, "Images (*.png *.jpg);;All (*)" );
    ui.eraseMaskText->setText( toNativeSeparators( file ) );
}

void ExtVideo::initComboBoxItems( QObject *widget )
{
    QComboBox *combobox = qobject_cast<QComboBox*>( widget );
    if( !combobox ) return;

    QString option = OptionFromWidgetName( widget );
    module_config_t *p_item = config_FindConfig( VLC_OBJECT( p_intf ),
                                                 qtu( option ) );
    if( p_item )
    {
        int i_type = p_item->i_type;
        for( int i_index = 0; i_index < p_item->i_list; i_index++ )
        {
            if( i_type == CONFIG_ITEM_INTEGER
             || i_type == CONFIG_ITEM_BOOL )
                combobox->addItem( qtr( p_item->ppsz_list_text[i_index] ),
                                   p_item->pi_list[i_index] );
            else if( i_type == CONFIG_ITEM_STRING )
                combobox->addItem( qtr( p_item->ppsz_list_text[i_index] ),
                                   p_item->ppsz_list[i_index] );
        }
    }
    else
    {
        msg_Err( p_intf, "Couldn't find option \"%s\".",
                 qtu( option ) );
    }
}

void ExtVideo::setWidgetValue( QObject *widget )
{
    QString module = ModuleFromWidgetName( widget->parent() );
    //std::cout << "Module name: " << module.toStdString() << std::endl;
    QString option = OptionFromWidgetName( widget );
    //std::cout << "Option name: " << option.toStdString() << std::endl;

    vlc_object_t *p_obj = ( vlc_object_t * )
        vlc_object_find_name( p_intf->p_libvlc, qtu( module ) );
    int i_type;
    vlc_value_t val;

    if( !p_obj )
    {
#if 0
        msg_Dbg( p_intf,
                 "Module instance %s not found, looking in config values.",
                 qtu( module ) );
#endif
        i_type = config_GetType( p_intf, qtu( option ) ) & VLC_VAR_CLASS;
        switch( i_type )
        {
            case VLC_VAR_INTEGER:
            case VLC_VAR_BOOL:
                val.i_int = config_GetInt( p_intf, qtu( option ) );
                break;
            case VLC_VAR_FLOAT:
                val.f_float = config_GetFloat( p_intf, qtu( option ) );
                break;
            case VLC_VAR_STRING:
                val.psz_string = config_GetPsz( p_intf, qtu( option ) );
                break;
        }
    }
    else
    {
        i_type = var_Type( p_obj, qtu( option ) ) & VLC_VAR_CLASS;
        var_Get( p_obj, qtu( option ), &val );
        vlc_object_release( p_obj );
    }

    /* Try to cast to all the widgets we're likely to encounter. Only
     * one of the casts is expected to work. */
    QSlider        *slider        = qobject_cast<QSlider*>       ( widget );
    QCheckBox      *checkbox      = qobject_cast<QCheckBox*>     ( widget );
    QSpinBox       *spinbox       = qobject_cast<QSpinBox*>      ( widget );
    QDoubleSpinBox *doublespinbox = qobject_cast<QDoubleSpinBox*>( widget );
    QDial          *dial          = qobject_cast<QDial*>         ( widget );
    QLineEdit      *lineedit      = qobject_cast<QLineEdit*>     ( widget );
    QComboBox      *combobox      = qobject_cast<QComboBox*>     ( widget );

    if( i_type == VLC_VAR_INTEGER || i_type == VLC_VAR_BOOL )
    {
        if( slider )        slider->setValue( val.i_int );
        else if( checkbox ) checkbox->setCheckState( val.i_int? Qt::Checked
                                                              : Qt::Unchecked );
        else if( spinbox )  spinbox->setValue( val.i_int );
        else if( dial )     dial->setValue( ( 540-val.i_int )%360 );
        else if( lineedit )
        {
            char str[30];
            snprintf( str, sizeof(str), "%06"PRIX64, val.i_int );
            lineedit->setText( str );
        }
        else if( combobox ) combobox->setCurrentIndex(
                            combobox->findData( qlonglong(val.i_int) ) );
        else msg_Warn( p_intf, "Oops %s %s %d", __FILE__, __func__, __LINE__ );
    }
    else if( i_type == VLC_VAR_FLOAT )
    {
        if( slider ) slider->setValue( ( int )( val.f_float*( double )slider->tickInterval() ) ); /* hack alert! */
        else if( doublespinbox ) doublespinbox->setValue( val.f_float );
        else msg_Warn( p_intf, "Oops %s %s %d", __FILE__, __func__, __LINE__ );
    }
    else if( i_type == VLC_VAR_STRING )
    {
        if( lineedit ) lineedit->setText( qfu( val.psz_string ) );
        else if( combobox ) combobox->setCurrentIndex(
                            combobox->findData( qfu( val.psz_string ) ) );
        else msg_Warn( p_intf, "Oops %s %s %d", __FILE__, __func__, __LINE__ );
        free( val.psz_string );
    }
    else
        if( p_obj )
            msg_Err( p_intf,
                     "Module %s's %s variable is of an unsupported type ( %d )",
                     qtu( module ),
                     qtu( option ),
                     i_type );
}

void ExtVideo::updateFilterOptions()
{
    QString module = ModuleFromWidgetName( sender()->parent() );
    //std::cout << "Module name: " << module.toStdString() << std::endl;
    QString option = OptionFromWidgetName( sender() );
    //std::cout << "Option name: " << option.toStdString() << std::endl;

    vlc_object_t *p_obj = ( vlc_object_t * )
        vlc_object_find_name( p_intf->p_libvlc, qtu( module ) );
    int i_type;
    bool b_is_command;
    if( !p_obj )
    {
        msg_Warn( p_intf, "Module %s not found. You'll need to restart the filter to take the change into account.", qtu( module ) );
        i_type = config_GetType( p_intf, qtu( option ) );
        b_is_command = false;
    }
    else
    {
        i_type = var_Type( p_obj, qtu( option ) );
        if( i_type == 0 )
            i_type = config_GetType( p_intf, qtu( option ) );
        b_is_command = ( i_type & VLC_VAR_ISCOMMAND );
    }

    /* Try to cast to all the widgets we're likely to encounter. Only
     * one of the casts is expected to work. */
    QSlider        *slider        = qobject_cast<QSlider*>       ( sender() );
    QCheckBox      *checkbox      = qobject_cast<QCheckBox*>     ( sender() );
    QSpinBox       *spinbox       = qobject_cast<QSpinBox*>      ( sender() );
    QDoubleSpinBox *doublespinbox = qobject_cast<QDoubleSpinBox*>( sender() );
    QDial          *dial          = qobject_cast<QDial*>         ( sender() );
    QLineEdit      *lineedit      = qobject_cast<QLineEdit*>     ( sender() );
    QComboBox      *combobox      = qobject_cast<QComboBox*>     ( sender() );

    i_type &= VLC_VAR_CLASS;
    if( i_type == VLC_VAR_INTEGER || i_type == VLC_VAR_BOOL )
    {
        int i_int = 0;
        if( slider )        i_int = slider->value();
        else if( checkbox ) i_int = checkbox->checkState() == Qt::Checked;
        else if( spinbox )  i_int = spinbox->value();
        else if( dial )     i_int = ( 540-dial->value() )%360;
        else if( lineedit ) i_int = lineedit->text().toInt( NULL,16 );
        else if( combobox ) i_int = combobox->itemData( combobox->currentIndex() ).toInt();
        else msg_Warn( p_intf, "Oops %s %s %d", __FILE__, __func__, __LINE__ );
        config_PutInt( p_intf, qtu( option ), i_int );
        if( b_is_command )
        {
            if( i_type == VLC_VAR_INTEGER )
                var_SetInteger( p_obj, qtu( option ), i_int );
            else
                var_SetBool( p_obj, qtu( option ), i_int );
        }
    }
    else if( i_type == VLC_VAR_FLOAT )
    {
        double f_float = 0;
        if( slider )             f_float = ( double )slider->value()
                                         / ( double )slider->tickInterval(); /* hack alert! */
        else if( doublespinbox ) f_float = doublespinbox->value();
        else if( lineedit ) f_float = lineedit->text().toDouble();
        else msg_Warn( p_intf, "Oops %s %s %d", __FILE__, __func__, __LINE__ );
        config_PutFloat( p_intf, qtu( option ), f_float );
        if( b_is_command )
            var_SetFloat( p_obj, qtu( option ), f_float );
    }
    else if( i_type == VLC_VAR_STRING )
    {
        QString val;
        if( lineedit )
            val = lineedit->text();
        else if( combobox )
            val = combobox->itemData( combobox->currentIndex() ).toString();
        else
            msg_Warn( p_intf, "Oops %s %s %d", __FILE__, __func__, __LINE__ );
        config_PutPsz( p_intf, qtu( option ), qtu( val ) );
        if( b_is_command )
            var_SetString( p_obj, qtu( option ), qtu( val ) );
    }
    else
        msg_Err( p_intf,
                 "Module %s's %s variable is of an unsupported type ( %d )",
                 qtu( module ),
                 qtu( option ),
                 i_type );

    if( !b_is_command )
    {
        msg_Warn( p_intf, "Module %s's %s variable isn't a command. Brute-restarting the filter.",
                 qtu( module ),
                 qtu( option ) );
        ChangeVFiltersString( p_intf, qtu( module ), false );
        ChangeVFiltersString( p_intf, qtu( module ), true );
    }

    if( p_obj ) vlc_object_release( p_obj );
}

#if 0
void ExtVideo::gotoConf( QObject* src )
{
#define SHOWCONF( module ) \
    if( src->objectName().contains( module ) ) \
    { \
        PrefsDialog::getInstance( p_intf )->showModulePrefs( module ); \
        return; \
    }
    SHOWCONF( "clone" );
    SHOWCONF( "magnify" );
    SHOWCONF( "wave" );
    SHOWCONF( "ripple" );
    SHOWCONF( "invert" );
    SHOWCONF( "puzzle" );
    SHOWCONF( "wall" );
    SHOWCONF( "gradient" );
    SHOWCONF( "colorthres" )
}
#endif

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
    vlc_object_t *p_obj = (vlc_object_t*)vlc_object_find_name( pl_Get(p_intf), "v4l2" );
    help->hide();
    if( box )
    {
        layout()->removeWidget( box );
        delete box;
        box = NULL;
    }
    if( p_obj )
    {
        vlc_value_t val, text;
        int i_ret = var_Change( p_obj, "controls", VLC_VAR_GETCHOICES,
                                &val, &text );
        if( i_ret < 0 )
        {
            msg_Err( p_intf, "Oops, v4l2 object doesn't have a 'controls' variable." );
            help->show();
            vlc_object_release( p_obj );
            return;
        }

        box = new QGroupBox( this );
        layout()->addWidget( box );
        QVBoxLayout *layout = new QVBoxLayout( box );
        box->setLayout( layout );

        for( int i = 0; i < val.p_list->i_count; i++ )
        {
            vlc_value_t vartext;
            const char *psz_var = text.p_list->p_values[i].psz_string;

            if( var_Change( p_obj, psz_var, VLC_VAR_GETTEXT, &vartext, NULL ) )
                continue;

            QString name = qtr( vartext.psz_string );
            free( vartext.psz_string );
            msg_Dbg( p_intf, "v4l2 control \"%"PRIx64"\": %s (%s)",
                     val.p_list->p_values[i].i_int, psz_var, qtu( name ) );

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
                        combobox->setObjectName( qtr( psz_var ) );

                        vlc_value_t val2, text2;
                        var_Change( p_obj, psz_var, VLC_VAR_GETCHOICES,
                                    &val2, &text2 );
                        for( int j = 0; j < val2.p_list->i_count; j++ )
                        {
                            combobox->addItem(
                                       text2.p_list->p_values[j].psz_string,
                                       qlonglong( val2.p_list->p_values[j].i_int) );
                            if( i_val == val2.p_list->p_values[j].i_int )
                                combobox->setCurrentIndex( j );
                        }
                        var_FreeList( &val2, &text2 );

                        CONNECT( combobox, currentIndexChanged( int ), this,
                                 ValueChange( int ) );
                        hlayout->addWidget( combobox );
                    }
                    else
                    {
                        QSlider *slider = new QSlider( box );
                        slider->setObjectName( qtr( psz_var ) );
                        slider->setOrientation( Qt::Horizontal );
                        vlc_value_t val2;
                        var_Change( p_obj, psz_var, VLC_VAR_GETMIN,
                                    &val2, NULL );
                        slider->setMinimum( val2.i_int );
                        var_Change( p_obj, psz_var, VLC_VAR_GETMAX,
                                    &val2, NULL );
                        slider->setMaximum( val2.i_int );
                        var_Change( p_obj, psz_var, VLC_VAR_GETSTEP,
                                    &val2, NULL );
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
                    button->setObjectName( qtr( psz_var ) );
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
                        button->setObjectName( qtr( psz_var ) );

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
        }
        var_FreeList( &val, &text );
        vlc_object_release( p_obj );
    }
    else
    {
        msg_Dbg( p_intf, "Couldn't find v4l2 instance" );
        help->show();
        if ( isVisible() )
            QTimer::singleShot( 2000, this, SLOT(Refresh()) );
    }
}

void ExtV4l2::ValueChange( bool value )
{
    ValueChange( (int)value );
}

void ExtV4l2::ValueChange( int value )
{
    QObject *s = sender();
    vlc_object_t *p_obj = (vlc_object_t*)vlc_object_find_name( pl_Get(p_intf), "v4l2" );
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
        vlc_object_release( p_obj );
    }
    else
    {
        msg_Warn( p_intf, "Oops, v4l2 object isn't available anymore" );
        Refresh();
    }
}

/**********************************************************************
 * Equalizer
 **********************************************************************/

static const QString band_frequencies[] =
{
    "  60 Hz  ", " 170 Hz ", " 310 Hz ", " 600 Hz ", "  1 kHz ",
    "  3 kHz  ", "  6 kHz ", " 12 kHz ", " 14 kHz ", " 16 kHz "
};

Equalizer::Equalizer( intf_thread_t *_p_intf, QWidget *_parent ) :
                            QWidget( _parent ) , p_intf( _p_intf )
{
    QFont smallFont = QApplication::font();
    smallFont.setPointSize( smallFont.pointSize() - 3 );

    ui.setupUi( this );
    ui.preampLabel->setFont( smallFont );

    /* Setup of presetsComboBox */
    presetsComboBox = ui.presetsCombo;
    CONNECT( presetsComboBox, activated( int ), this, setCorePreset( int ) );

    /* Add the sliders for the Bands */
    QGridLayout *grid = new QGridLayout( ui.frame );
    grid->setMargin( 0 );
    for( int i = 0 ; i < BANDS ; i++ )
    {
        bands[i] = new QSlider( Qt::Vertical );
        bands[i]->setMaximum( 400 );
        bands[i]->setValue( 200 );
        CONNECT( bands[i], valueChanged( int ), this, setCoreBands() );

        band_texts[i] = new QLabel( band_frequencies[i] + "\n0.0dB" );
        band_texts[i]->setFont( smallFont );

        grid->addWidget( bands[i], 0, i );
        grid->addWidget( band_texts[i], 1, i );
    }

    /* Add the listed presets */
    for( int i = 0 ; i < NB_PRESETS ; i ++ )
    {
        presetsComboBox->addItem( qtr( preset_list_text[i] ),
                                  QVariant( preset_list[i] ) );
    }

    /* Connects */
    BUTTONACT( ui.enableCheck, enable() );
    BUTTONACT( ui.eq2PassCheck, set2Pass() );
    CONNECT( ui.preampSlider, valueChanged( int ), this, setPreamp() );

    /* Do the update from the value of the core */
    updateUIFromCore();
}

/* Write down initial values */
void Equalizer::updateUIFromCore()
{
    char *psz_af, *psz_pres, *psz_bands;
    float f_preamp;
    int i_preset;

    vlc_object_t *p_aout = (vlc_object_t *)THEMIM->getAout();
    if( p_aout )
    {
        psz_af = var_GetNonEmptyString( p_aout, "audio-filter" );
        psz_pres = var_GetString( p_aout, "equalizer-preset" );
        if( var_GetBool( p_aout, "equalizer-2pass" ) )
            ui.eq2PassCheck->setChecked( true );
        f_preamp = var_GetFloat( p_aout, "equalizer-preamp" );
        psz_bands = var_GetNonEmptyString( p_aout, "equalizer-bands" );
        i_preset = presetsComboBox->findData( QVariant( psz_pres ) );
        vlc_object_release( p_aout );
    }
    else
    {
        psz_af = config_GetPsz( p_intf, "audio-filter" );
        psz_pres = config_GetPsz( p_intf, "equalizer-preset" );
        if( config_GetInt( p_intf, "equalizer-2pass" ) )
            ui.eq2PassCheck->setChecked( true );
        f_preamp = config_GetFloat( p_intf, "equalizer-preamp" );
        psz_bands = config_GetPsz( p_intf, "equalizer-bands" );
        i_preset = presetsComboBox->findData( QVariant( psz_pres ) );
    }
    if( psz_af && strstr( psz_af, "equalizer" ) != NULL )
        ui.enableCheck->setChecked( true );
    enable( ui.enableCheck->isChecked() );

    presetsComboBox->setCurrentIndex( i_preset );

    ui.preampSlider->setValue( (int)( ( f_preamp + 20 ) * 10 ) );

    if( psz_bands && strlen( psz_bands ) > 1 )
    {
        char *psz_bands_orig = psz_bands;
        for( int i = 0; i < BANDS; i++ )
        {
            const float f = us_strtod(psz_bands, &psz_bands );
            bands[i]->setValue( (int)( ( f + 20 ) * 10 )  );
            if( psz_bands == NULL || *psz_bands == '\0' ) break;
            psz_bands++;
            if( *psz_bands == '\0' ) break;
        }
        free( psz_bands_orig );
    }
    else free( psz_bands );

    free( psz_af );
    free( psz_pres );
}

/* Functin called when enableButton is toggled */
void Equalizer::enable()
{
    bool en = ui.enableCheck->isChecked();
    aout_EnableFilter( THEPL, "equalizer", en );
//    aout_EnableFilter( THEPL, "upmixer", en );
//     aout_EnableFilter( THEPL, "vsurround", en );
    enable( en );

    if( presetsComboBox->currentIndex() < 0 )
        presetsComboBox->setCurrentIndex( 0 );

}

void Equalizer::enable( bool en )
{
    ui.eq2PassCheck->setEnabled( en );
    presetsComboBox->setEnabled( en );
    ui.presetLabel->setEnabled( en );
    ui.preampLabel->setEnabled( en );
    ui.preampSlider->setEnabled( en  );
    for( int i = 0 ; i< BANDS; i++ )
    {
        bands[i]->setEnabled( en ); band_texts[i]->setEnabled( en );
    }
}

/* Function called when the set2Pass button is activated */
void Equalizer::set2Pass()
{
    vlc_object_t *p_aout= (vlc_object_t *)THEMIM->getAout();
    bool b_2p = ui.eq2PassCheck->isChecked();

    if( p_aout )
    {
        var_SetBool( p_aout, "equalizer-2pass", b_2p );
        vlc_object_release( p_aout );
    }
    config_PutInt( p_intf, "equalizer-2pass", b_2p );
}

/* Function called when the preamp slider is moved */
void Equalizer::setPreamp()
{
    const float f = ( float )(  ui.preampSlider->value() ) /10 - 20;
    vlc_object_t *p_aout = (vlc_object_t *)THEMIM->getAout();

    ui.preampLabel->setText( qtr( "Preamp\n" ) + QString::number( f, 'f', 1 )
                                               + qtr( "dB" ) );
    if( p_aout )
    {
        //delCallbacks( p_aout );
        var_SetFloat( p_aout, "equalizer-preamp", f );
        //addCallbacks( p_aout );
        vlc_object_release( p_aout );
    }
    config_PutFloat( p_intf, "equalizer-preamp", f );
}

void Equalizer::setCoreBands()
{
    /**\todo smoothing */

    QString values;
    for( int i = 0; i < BANDS; i++ )
    {
        const float f_val = (float)( bands[i]->value() ) / 10 - 20;
        QString val = QString("%1").arg( f_val, 5, 'f', 1 );

        band_texts[i]->setText( band_frequencies[i] + "\n" + val + "dB" );
        values += " " + val;
    }

    vlc_object_t *p_aout = (vlc_object_t *)THEMIM->getAout();
    if( p_aout )
    {
        //delCallbacks( p_aout );
        var_SetString( p_aout, "equalizer-bands", qtu( values ) );
        //addCallbacks( p_aout );
        vlc_object_release( p_aout );
    }
}

char * Equalizer::createValuesFromPreset( int i_preset )
{
    QString values;

    /* Create the QString in Qt */
    for( int i = 0 ; i< BANDS ;i++ )
        values += QString( " %1" ).arg( eqz_preset_10b[i_preset].f_amp[i], 5, 'f', 1 );

    /* Convert it to char * */
    return strdup( values.toAscii().constData() );
}

void Equalizer::setCorePreset( int i_preset )
{
    if( i_preset < 0 )
        return;

    /* Update pre-amplification in the UI */
    float f_preamp = eqz_preset_10b[i_preset].f_preamp;
    ui.preampSlider->setValue( (int)( ( f_preamp + 20 ) * 10 ) );
    ui.preampLabel->setText( qtr( "Preamp\n" )
                   + QString::number( f_preamp, 'f', 1 ) + qtr( "dB" ) );

    char *psz_values = createValuesFromPreset( i_preset );
    if( !psz_values ) return ;

    char *p = psz_values;
    for( int i = 0; i < BANDS && *p; i++ )
    {
        const float f = us_strtod( p, &p );

        bands[i]->setValue( (int)( ( f + 20 ) * 10 )  );
        band_texts[i]->setText( band_frequencies[i] + "\n"
                              + QString("%1").arg( f, 5, 'f', 1 ) + "dB" );
        if( *p )
            p++; /* skip separator */
    }

    /* Apply presets to audio output */
    vlc_object_t *p_aout = (vlc_object_t *)THEMIM->getAout();
    if( p_aout )
    {
        var_SetString( p_aout , "equalizer-preset" , preset_list[i_preset] );

        var_SetString( p_aout, "equalizer-bands", psz_values );
        var_SetFloat( p_aout, "equalizer-preamp",
                      eqz_preset_10b[i_preset].f_preamp );
        vlc_object_release( p_aout );
    }
    config_PutPsz( p_intf, "equalizer-bands", psz_values );
    config_PutPsz( p_intf, "equalizer-preset", preset_list[i_preset] );
    config_PutFloat( p_intf, "equalizer-preamp",
                    eqz_preset_10b[i_preset].f_preamp );
    free( psz_values );
}

static int PresetCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED( p_this ); VLC_UNUSED( psz_cmd ); VLC_UNUSED( oldval );

    char *psz_preset = newval.psz_string;
    Equalizer *eq = ( Equalizer * )p_data;
    int i_preset = eq->presetsComboBox->findData( QVariant( psz_preset ) );
    eq->presetsComboBox->setCurrentIndex( i_preset );
    return VLC_SUCCESS;
}

void Equalizer::delCallbacks( vlc_object_t *p_aout )
{
    //var_DelCallback( p_aout, "equalizer-bands", EqzCallback, this );
    //var_DelCallback( p_aout, "equalizer-preamp", EqzCallback, this );
    var_DelCallback( p_aout, "equalizer-preset", PresetCallback, this );
}

void Equalizer::addCallbacks( vlc_object_t *p_aout )
{
    //var_AddCallback( p_aout, "equalizer-bands", EqzCallback, this );
    //var_AddCallback( p_aout, "equalizer-preamp", EqzCallback, this );
    var_AddCallback( p_aout, "equalizer-preset", PresetCallback, this );
}

/**********************************************************************
 * Audio filters
 **********************************************************************/

/**********************************************************************
 * Dynamic range compressor
 **********************************************************************/

typedef struct
{
    const char *psz_name;
    const char *psz_descs;
    const char *psz_units;
    const float f_min;      // min
    const float f_max;      // max
    const float f_value;    // value
    const float f_resolution; // resolution
} comp_controls_t;

static const comp_controls_t comp_controls[] =
{
    { "compressor-rms-peak",    _("RMS/peak"),       "",       0.0f,   1.0f,   0.00f, 0.001f },
    { "compressor-attack",      _("Attack"),       _(" ms"),   1.5f, 400.0f,  25.00f, 0.100f },
    { "compressor-release",     _("Release"),      _(" ms"),   2.0f, 800.0f, 100.00f, 0.100f },
    { "compressor-threshold",   _("Threshold"),    _(" dB"), -30.0f,   0.0f, -11.00f, 0.010f },
    { "compressor-ratio",       _("Ratio"),          ":1",     1.0f,  20.0f,   8.00f, 0.010f },
    { "compressor-knee",        _("Knee\nradius"), _(" dB"),   1.0f,  10.0f,   2.50f, 0.010f },
    { "compressor-makeup-gain", _("Makeup\ngain"), _(" dB"),   0.0f,  24.0f,   7.00f, 0.010f },
};

Compressor::Compressor( intf_thread_t *_p_intf, QWidget *_parent )
           : QWidget( _parent ) , p_intf( _p_intf )
{
    QFont smallFont = QApplication::font();
    smallFont.setPointSize( smallFont.pointSize() - 2 );

    QGridLayout *layout = new QGridLayout( this );

    enableCheck = new QCheckBox( qtr( "Enable dynamic range compressor" ) );
    layout->addWidget( enableCheck, 0, 0, 1, NUM_CP_CTRL );

    for( int i = 0 ; i < NUM_CP_CTRL ; i++ )
    {
        const int i_min = (int)( comp_controls[i].f_min
                               / comp_controls[i].f_resolution );
        const int i_max = (int)( comp_controls[i].f_max
                               / comp_controls[i].f_resolution );
        const int i_val = (int)( comp_controls[i].f_value
                               / comp_controls[i].f_resolution );

        compCtrl[i] = new QSlider( Qt::Vertical );
        compCtrl[i]->setMinimum( i_min );
        compCtrl[i]->setMaximum( i_max );
        compCtrl[i]->setValue(   i_val );

        oldControlVars[i] = comp_controls[i].f_value;

        CONNECT( compCtrl[i], valueChanged( int ), this, setInitValues() );

        ctrl_texts[i] = new QLabel( qtr( comp_controls[i].psz_descs ) + "\n" );
        ctrl_texts[i]->setFont( smallFont );
        ctrl_texts[i]->setAlignment( Qt::AlignHCenter );

        ctrl_readout[i] = new QLabel;
        ctrl_readout[i]->setFont( smallFont );
        ctrl_readout[i]->setAlignment( Qt::AlignHCenter );

        layout->addWidget( compCtrl[i],     1, i, Qt::AlignHCenter );
        layout->addWidget( ctrl_readout[i], 2, i, Qt::AlignHCenter );
        layout->addWidget( ctrl_texts[i],   3, i, Qt::AlignHCenter );
    }

    BUTTONACT( enableCheck, enable() );

    /* Write down initial values */
    vlc_object_t *p_aout = (vlc_object_t *)THEMIM->getAout();
    char *psz_af;

    if( p_aout )
    {
        psz_af = var_GetNonEmptyString( p_aout, "audio-filter" );
        for( int i = 0; i < NUM_CP_CTRL; i++ )
        {
            controlVars[i] = var_GetFloat( p_aout,
                                           comp_controls[i].psz_name );
        }
        vlc_object_release( p_aout );
    }
    else
    {
        psz_af = config_GetPsz( p_intf, "audio-filter" );
        for( int i = 0; i < NUM_CP_CTRL; i++ )
        {
            controlVars[i] = config_GetFloat( p_intf,
                                              comp_controls[i].psz_name );
        }
    }
    if( psz_af && strstr( psz_af, "compressor" ) != NULL )
    {
        enableCheck->setChecked( true );
    }
    free( psz_af );
    enable( enableCheck->isChecked() );
    updateSliders( controlVars );
    setValues();
}

void Compressor::enable()
{
    bool en = enableCheck->isChecked();
    aout_EnableFilter( THEPL, "compressor", en );
    enable( en );
}

void Compressor::enable( bool en )
{
    for( int i = 0 ; i < NUM_CP_CTRL ; i++ )
    {
        compCtrl[i]->setEnabled( en );
        ctrl_texts[i]->setEnabled( en );
        ctrl_readout[i]->setEnabled( en );
    }
}

void Compressor::updateSliders( float * controlVars )
{
    for( int i = 0 ; i < NUM_CP_CTRL ; i++ )
    {
        if( oldControlVars[i] != controlVars[i] )
        {
            compCtrl[i]->setValue(
                    (int)( controlVars[i] / comp_controls[i].f_resolution ) );
        }
    }
}

void Compressor::setInitValues()
{
    setValues();
}

void Compressor::setValues()
{
    vlc_object_t *p_aout = (vlc_object_t *)THEMIM->getAout();

    for( int i = 0 ; i < NUM_CP_CTRL ; i++ )
    {
        float f = (float)( compCtrl[i]->value() ) * ( comp_controls[i].f_resolution );
        ctrl_readout[i]->setText( QString::number( f, 'f', 1 )
                                + qtr( comp_controls[i].psz_units ) );
        if( oldControlVars[i] != f )
        {
            if( p_aout )
            {
                var_SetFloat( p_aout, comp_controls[i].psz_name, f );
            }
            config_PutFloat( p_intf, comp_controls[i].psz_name, f );
            oldControlVars[i] = f;
        }
    }
    if( p_aout )
    {
        vlc_object_release( p_aout );
    }
}

/**********************************************************************
 * Spatializer
 **********************************************************************/
typedef struct
{
    const char *psz_name;
    const char *psz_desc;
} spat_controls_t;

static const spat_controls_t spat_controls[] =
{
    { "spatializer-roomsize", _("Size") },
    { "spatializer-width",    _("Width") },
    { "spatializer-wet",      _("Wet") },
    { "spatializer-dry",      _("Dry") },
    { "spatializer-damp",     _("Damp") },
};

Spatializer::Spatializer( intf_thread_t *_p_intf, QWidget *_parent )
            : QWidget( _parent ) , p_intf( _p_intf )
{
    QFont smallFont = QApplication::font();
    smallFont.setPointSize( smallFont.pointSize() - 2 );

    QGridLayout *layout = new QGridLayout( this );

    enableCheck = new QCheckBox( qtr( "Enable spatializer" ) );
    layout->addWidget( enableCheck, 0, 0, 1, NUM_SP_CTRL );

    for( int i = 0 ; i < NUM_SP_CTRL ; i++ )
    {
        spatCtrl[i] = new QSlider( Qt::Vertical );
        if( i < 2 )
        {
            spatCtrl[i]->setMaximum( 10 );
            spatCtrl[i]->setValue( 2 );
        }
        else
        {
            spatCtrl[i]->setMaximum( 10 );
            spatCtrl[i]->setValue( 0 );
            spatCtrl[i]->setMinimum( -10 );
        }

        oldControlVars[i] = spatCtrl[i]->value();

        CONNECT( spatCtrl[i], valueChanged( int ), this, setInitValues() );

        ctrl_texts[i] = new QLabel( qtr( spat_controls[i].psz_desc ) + "\n" );
        ctrl_texts[i]->setFont( smallFont );

        ctrl_readout[i] = new QLabel;
        ctrl_readout[i]->setFont( smallFont );

        layout->addWidget( spatCtrl[i],     1, i, Qt::AlignHCenter );
        layout->addWidget( ctrl_readout[i], 2, i, Qt::AlignHCenter );
        layout->addWidget( ctrl_texts[i],   3, i, Qt::AlignHCenter );
    }

    BUTTONACT( enableCheck, enable() );

    /* Write down initial values */
    vlc_object_t *p_aout = (vlc_object_t *)THEMIM->getAout();
    char *psz_af;

    if( p_aout )
    {
        psz_af = var_GetNonEmptyString( p_aout, "audio-filter" );
        for( int i = 0; i < NUM_SP_CTRL ; i++ )
        {
            controlVars[i] = var_GetFloat( p_aout, spat_controls[i].psz_name );
        }
        vlc_object_release( p_aout );
    }
    else
    {
        psz_af = config_GetPsz( p_intf, "audio-filter" );
        for( int i = 0; i < NUM_SP_CTRL ; i++ )
        {
            controlVars[i] = config_GetFloat( p_intf, spat_controls[i].psz_name );
        }
    }
    if( psz_af && strstr( psz_af, "spatializer" ) != NULL )
        enableCheck->setChecked( true );
    free( psz_af );
    enable( enableCheck->isChecked() );
    setValues();
}

void Spatializer::enable()
{
    bool en = enableCheck->isChecked();
    aout_EnableFilter( THEPL, "spatializer", en );
    enable( en );
}

void Spatializer::enable( bool en )
{
    for( int i = 0 ; i< NUM_SP_CTRL; i++ )
    {
        spatCtrl[i]->setEnabled( en );
        ctrl_texts[i]->setEnabled( en );
        ctrl_readout[i]->setEnabled( en );
    }
}
void Spatializer::setInitValues()
{
    setValues();
}

void Spatializer::setValues()
{
    vlc_object_t *p_aout = (vlc_object_t *)THEMIM->getAout();

    for( int i = 0 ; i < NUM_SP_CTRL ; i++ )
    {
        float f = (float)(  spatCtrl[i]->value() );
        ctrl_readout[i]->setText( QString::number( f, 'f',  1 ) );
    }
    if( p_aout )
    {
        for( int i = 0 ; i < NUM_SP_CTRL ; i++ )
        {
            if( oldControlVars[i] != spatCtrl[i]->value() )
            {
                var_SetFloat( p_aout, spat_controls[i].psz_name,
                        ( float )spatCtrl[i]->value() );
                config_PutFloat( p_intf, spat_controls[i].psz_name,
                        ( float ) spatCtrl[i]->value() );
                oldControlVars[i] = ( float ) spatCtrl[i]->value();
            }
        }
        vlc_object_release( p_aout );
    }

}
void Spatializer::delCallbacks( vlc_object_t *p_aout )
{
    VLC_UNUSED( p_aout );
    //    var_DelCallback( p_aout, "Spatializer-bands", EqzCallback, this );
    //    var_DelCallback( p_aout, "Spatializer-preamp", EqzCallback, this );
}

void Spatializer::addCallbacks( vlc_object_t *p_aout )
{
    VLC_UNUSED( p_aout );
    //    var_AddCallback( p_aout, "Spatializer-bands", EqzCallback, this );
    //    var_AddCallback( p_aout, "Spatializer-preamp", EqzCallback, this );
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
}

void SyncWidget::setValue( double d )
{
    spinBox.setValue( d );
}

SyncControls::SyncControls( intf_thread_t *_p_intf, QWidget *_parent ) :
                            QWidget( _parent ) , p_intf( _p_intf )
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

    QLabel *subsLabel = new QLabel;
    subsLabel->setText( qtr( "Subtitle track syncronization:" ) );
    subsLayout->addWidget( subsLabel, 0, 0, 1, 1 );

    subsSpin = new SyncWidget( this );
    subsLayout->addWidget( subsSpin, 0, 2, 1, 1 );

    QLabel *subSpeedLabel = new QLabel;
    subSpeedLabel->setText( qtr( "Subtitles speed:" ) );
    subsLayout->addWidget( subSpeedLabel, 1, 0, 1, 1 );

    subSpeedSpin = new QDoubleSpinBox;
    subSpeedSpin->setAlignment( Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter );
    subSpeedSpin->setDecimals( 3 );
    subSpeedSpin->setMinimum( 1 );
    subSpeedSpin->setMaximum( 100 );
    subSpeedSpin->setSingleStep( 0.2 );
    subSpeedSpin->setSuffix( " fps" );
    subSpeedSpin->setButtonSymbols( QDoubleSpinBox::PlusMinus );
    subsLayout->addWidget( subSpeedSpin, 1, 2, 1, 1 );

    QLabel *subDurationLabel = new QLabel;
    subDurationLabel->setText( qtr( "Subtitles duration factor:" ) );
    subsLayout->addWidget( subDurationLabel, 2, 0, 1, 1 );

    subDurationSpin = new QDoubleSpinBox;
    subDurationSpin->setAlignment( Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter );
    subDurationSpin->setDecimals( 3 );
    subDurationSpin->setMinimum( 0 );
    subDurationSpin->setMaximum( 20 );
    subDurationSpin->setSingleStep( 0.2 );
    subDurationSpin->setButtonSymbols( QDoubleSpinBox::PlusMinus );
    subsLayout->addWidget( subDurationSpin, 2, 2, 1, 1 );

    mainLayout->addWidget( subsBox, 2, 0, 2, 5 );

    updateButton = new QToolButton;
    updateButton->setAutoRaise( true );
    mainLayout->addWidget( updateButton, 0, 4, 1, 1 );

    /* Various Connects */
    CONNECT( AVSpin, valueChanged ( double ), this, advanceAudio( double ) ) ;
    CONNECT( subsSpin, valueChanged ( double ), this, advanceSubs( double ) ) ;
    CONNECT( subSpeedSpin, valueChanged ( double ),
             this, adjustSubsSpeed( double ) );
    CONNECT( subDurationSpin, valueChanged ( double ),
             this, adjustSubsDuration( double ) );

    CONNECT( THEMIM->getIM(), synchroChanged(), this, update() );
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
    subSpeedSpin->setValue( 1.0 );
    subsdelayClean();
    b_userAction = true;
}

void SyncControls::update()
{
    b_userAction = false;

    int64_t i_delay;
    if( THEMIM->getInput() )
    {
        i_delay = var_GetTime( THEMIM->getInput(), "audio-delay" );
        AVSpin->setValue( ( (double)i_delay ) / 1000000 );
        i_delay = var_GetTime( THEMIM->getInput(), "spu-delay" );
        subsSpin->setValue( ( (double)i_delay ) / 1000000 );
        subSpeedSpin->setValue( var_GetFloat( THEMIM->getInput(), "sub-fps" ) );
        subDurationSpin->setValue( var_InheritFloat( p_intf, SUBSDELAY_CFG_FACTOR ) );
    }
    b_userAction = true;
}

void SyncControls::advanceAudio( double f_advance )
{
    if( THEMIM->getInput() && b_userAction )
    {
        int64_t i_delay = f_advance * 1000000;
        var_SetTime( THEMIM->getInput(), "audio-delay", i_delay );
    }
}

void SyncControls::advanceSubs( double f_advance )
{
    if( THEMIM->getInput() && b_userAction )
    {
        int64_t i_delay = f_advance * 1000000;
        var_SetTime( THEMIM->getInput(), "spu-delay", i_delay );
    }
}

void SyncControls::adjustSubsSpeed( double f_fps )
{
    if( THEMIM->getInput() && b_userAction )
    {
        var_SetFloat( THEMIM->getInput(), "sub-fps", f_fps );
    }
}

void SyncControls::adjustSubsDuration( double f_factor )
{
    if( THEMIM->getInput() && b_userAction )
    {
        subsdelaySetFactor( f_factor );
        ChangeVFiltersString( p_intf, "subsdelay", f_factor > 0 );
    }
}

void SyncControls::initSubsDuration()
{
    int i_mode = var_InheritInteger( p_intf, SUBSDELAY_CFG_MODE );

    switch (i_mode)
    {
    default:
    case SUBSDELAY_MODE_ABSOLUTE:
        subDurationSpin->setToolTip( qtr( "Extend subtitles duration by this value.\n"
                                          "Set 0 to disable." ) );
        subDurationSpin->setSuffix( " s" );
        break;
    case SUBSDELAY_MODE_RELATIVE_SOURCE_DELAY:
        subDurationSpin->setToolTip( qtr( "Multiply subtitles duration by this value.\n"
                                          "Set 0 to disable." ) );
        subDurationSpin->setSuffix( "" );
        break;
    case SUBSDELAY_MODE_RELATIVE_SOURCE_CONTENT:
        subDurationSpin->setToolTip( qtr( "Recalculate subtitles duration according\n"
                                          "to their content and this value.\n"
                                          "Set 0 to disable." ) );
        subDurationSpin->setSuffix( "" );
        break;
    }
}

void SyncControls::subsdelayClean()
{
    /* Remove subsdelay filter */
    ChangeVFiltersString( p_intf, "subsdelay", false );
}

void SyncControls::subsdelaySetFactor( double f_factor )
{
    /* Set the factor in the preferences */
    config_PutFloat( p_intf, SUBSDELAY_CFG_FACTOR, f_factor );

    /* Try to find an instance of subsdelay, and set its factor */
    vlc_object_t *p_obj = ( vlc_object_t * ) vlc_object_find_name( p_intf->p_libvlc, "subsdelay" );
    if( p_obj )
    {
        var_SetFloat( p_obj, SUBSDELAY_CFG_FACTOR, f_factor );
        vlc_object_release( p_obj );
    }
}


/**********************************************************************
 * Video filters / Adjust
 **********************************************************************/

/**********************************************************************
 * Extended playbak controls
 **********************************************************************/
