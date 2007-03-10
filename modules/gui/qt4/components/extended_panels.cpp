/*****************************************************************************
 * extended_panels.cpp : Extended controls panels
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: preferences.cpp 16643 2006-09-13 12:45:46Z zorglub $
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

#include <QLabel>
#include <QVariant>
#include <QString>
#include <QFont>
#include <QGridLayout>
#include <QSignalMapper>

#include "components/extended_panels.hpp"
#include "dialogs/prefs_dialog.hpp"
#include "dialogs_provider.hpp"
#include "qt4.hpp"

#include "../../audio_filter/equalizer_presets.h"
#include <vlc_aout.h>
#include <vlc_intf_strings.h>
#include <vlc_vout.h>

class ConfClickHandler : public QObject
{
public:
    ConfClickHandler( intf_thread_t *_p_intf, ExtVideo *_e ) : QObject (_e) {
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

#define SETUP_VFILTER( widget, conf, tooltip, labtooltip ) \
    ui.widget##Check->setToolTip( tooltip ); \
    if (conf ) {\
        ui.widget##Label->setToolTip( labtooltip ); \
        ui.widget##Label->setPixmap( QPixmap(":/pixmaps/go-next.png") ); \
        ui.widget##Label->installEventFilter(h); \
    } \
    CONNECT( ui.widget##Check, clicked(), this, updateFilters() ); \

ExtVideo::ExtVideo( intf_thread_t *_p_intf, QWidget *_parent ) :
                           QWidget( _parent ) , p_intf( _p_intf )
{
    ConfClickHandler* h = new ConfClickHandler( p_intf, this );

    ui.setupUi( this );

    SETUP_VFILTER( clone, true, qtr(I_CLONE_TIP),
                                qtr("Configure the clone filter") );
    SETUP_VFILTER( magnify, false, qtr(I_MAGNIFY_TIP),
                                qtr("Configure the magnification effect"));
    SETUP_VFILTER( wave, false, qtr(I_WAVE_TIP),
                                qtr("Configure the waves effect"));
    SETUP_VFILTER( ripple, false, qtr(I_RIPPLE_TIP),
                                  qtr("Configure the \"water\" effect"));
    SETUP_VFILTER( invert, false, qtr(I_INVERT_TIP),
                                  qtr("Configure the color inversion effect"));
    SETUP_VFILTER( puzzle, true, qtr(I_PUZZLE_TIP),
                                  qtr("Configure the puzzle effect"));
    SETUP_VFILTER( wall, true, qtr(I_WALL_TIP),
                               qtr("Configure the wall filter") );
    SETUP_VFILTER( gradient, true, qtr(I_GRADIENT_TIP),
                                   qtr("Configure the \"gradient\" effect"));
    SETUP_VFILTER( colorthres, true, qtr(I_COLORTHRES_TIP),
                                   qtr("Configure the color detection effect"));
}

ExtVideo::~ExtVideo()
{
}

static void ChangeVFiltersString( intf_thread_t *p_intf,
                                 char *psz_name, vlc_bool_t b_add )
{
    vout_thread_t *p_vout;
    char *psz_parser, *psz_string;

    psz_string = config_GetPsz( p_intf, "video-filter" );

    if( !psz_string ) psz_string = strdup("");

    psz_parser = strstr( psz_string, psz_name );

    if( b_add )
    {
        if( !psz_parser )
        {
            psz_parser = psz_string;
            asprintf( &psz_string, (*psz_string) ? "%s:%s" : "%s%s",
                            psz_string, psz_name );
            free( psz_parser );
        }
        else
        {
            return;
        }
    }
    else
    {
        if( psz_parser )
        {
            if( *(psz_parser + strlen(psz_name)) == ':' )
            {
                memmove( psz_parser, psz_parser + strlen(psz_name) + 1,
                         strlen(psz_parser + strlen(psz_name) + 1 ) + 1 );
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
    config_PutPsz( p_intf, "video-filter", psz_string );

    /* Try to set on the fly */
    p_vout = (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                              FIND_ANYWHERE );
    if( p_vout )
    {
        var_SetString( p_vout, "video-filter", psz_string );
        vlc_object_release( p_vout );
    }

    free( psz_string );
}

void ExtVideo::updateFilters()
{
    QCheckBox* filter = qobject_cast<QCheckBox*>(sender());
    QString module = filter->objectName().replace("Check", "");

    ChangeVFiltersString( p_intf, qtu(module), filter->isChecked() );
}

void ExtVideo::gotoConf( QObject* src )
{
#define SHOWCONF(module) \
    if( src->objectName().contains(module) ) \
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

/**********************************************************************
 * Equalizer
 **********************************************************************/

static const QString band_frequencies[] =
{
    "   60Hz  ", " 170 Hz " , " 310 Hz ", " 600 Hz ", "  1 kHz  ",
    "  3 kHz  " , "  6 kHz ", " 12 kHz ", " 14 kHz ", " 16 kHz "
};

Equalizer::Equalizer( intf_thread_t *_p_intf, QWidget *_parent ) :
                            QWidget( _parent ) , p_intf( _p_intf )
{
    QFont smallFont = QApplication::font( static_cast<QWidget*>(0) );
    smallFont.setPointSize( smallFont.pointSize() - 3 );

    ui.setupUi( this );

    ui.preampLabel->setFont( smallFont );
    ui.preampSlider->setMaximum( 400 );
    for( int i = 0 ; i < NB_PRESETS ; i ++ )
    {
        ui.presetsCombo->addItem( qtr( preset_list_text[i] ),
                                  QVariant( i ) );
    }
    CONNECT( ui.presetsCombo, activated( int ), this, setPreset( int ) );

    BUTTONACT( ui.enableCheck, enable() );
    BUTTONACT( ui.eq2PassCheck, set2Pass() );

    CONNECT( ui.preampSlider, valueChanged(int), this, setPreamp() );

    QGridLayout *grid = new QGridLayout( ui.frame );
    grid->setMargin( 0 );
    for( int i = 0 ; i < BANDS ; i++ )
    {
        bands[i] = new QSlider( Qt::Vertical );
        bands[i]->setMaximum( 400 );
        bands[i]->setValue( 200 );
        CONNECT( bands[i], valueChanged(int), this, setBand() );
        band_texts[i] = new QLabel( band_frequencies[i] + "\n0.0dB" );
        band_texts[i]->setFont( smallFont );
        grid->addWidget( bands[i], 0, i );
        grid->addWidget( band_texts[i], 1, i );
    }

    /* Write down initial values */
    aout_instance_t *p_aout = (aout_instance_t *)vlc_object_find(p_intf,
                                    VLC_OBJECT_AOUT, FIND_ANYWHERE);
    char *psz_af = NULL;
    char *psz_bands;
    float f_preamp;
    if( p_aout )
    {
        psz_af = var_GetString( p_aout, "audio-filter" );
        if( var_GetBool( p_aout, "equalizer-2pass" ) )
            ui.eq2PassCheck->setChecked( true );
        psz_bands = var_GetString( p_aout, "equalizer-bands" );
        f_preamp = var_GetFloat( p_aout, "equalizer-preamp" );
        vlc_object_release( p_aout );
    }
    else
    {
        psz_af = config_GetPsz( p_intf, "audio-filter" );
        if( config_GetInt( p_intf, "equalizer-2pass" ) )
            ui.eq2PassCheck->setChecked( true );
        psz_bands = config_GetPsz( p_intf, "equalizer-bands" );
        f_preamp = config_GetFloat( p_intf, "equalizer-preamp" );
    }
    if( psz_af && strstr( psz_af, "equalizer" ) != NULL )
        ui.enableCheck->setChecked( true );
    enable( ui.enableCheck->isChecked() );

    setValues( psz_bands, f_preamp );
}

Equalizer::~Equalizer()
{
}

void Equalizer::enable()
{
    bool en = ui.enableCheck->isChecked();
    aout_EnableFilter( VLC_OBJECT( p_intf ), "equalizer",
                       en ? VLC_TRUE : VLC_FALSE );
    enable( en );
}

void Equalizer::enable( bool en )
{
    ui.eq2PassCheck->setEnabled( en );
    ui.preampLabel->setEnabled( en );
    ui.preampSlider->setEnabled( en  );
    for( int i = 0 ; i< BANDS; i++ )
    {
        bands[i]->setEnabled( en ); band_texts[i]->setEnabled( en );
    }
}

void Equalizer::set2Pass()
{
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                 VLC_OBJECT_AOUT, FIND_ANYWHERE);
    vlc_bool_t b_2p = ui.eq2PassCheck->isChecked();

    if( p_aout == NULL )
        config_PutInt( p_intf, "equalizer-2pass", b_2p );
    else
    {
        var_SetBool( p_aout, "equalizer-2pass", b_2p );
        config_PutInt( p_intf, "equalizer-2pass", b_2p );
        for( int i = 0; i < p_aout->i_nb_inputs; i++ )
        {
            p_aout->pp_inputs[i]->b_restart = VLC_TRUE;
        }
        vlc_object_release( p_aout );
    }
}

void Equalizer::setPreamp()
{
    float f= (float)(  ui.preampSlider->value() ) /10 - 20;
    char psz_val[5];
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                       VLC_OBJECT_AOUT, FIND_ANYWHERE);

    sprintf( psz_val, "%.1f", f );
    ui.preampLabel->setText( qtr("Preamp\n") + psz_val + qtr("dB") );
    if( p_aout )
    {
        delCallbacks( p_aout );
        var_SetFloat( p_aout, "equalizer-preamp", f );
        addCallbacks( p_aout );
        vlc_object_release( p_aout );
    }
    config_PutFloat( p_intf, "equalizer-preamp", f );
}

void Equalizer::setBand()
{
    char psz_values[102]; memset( psz_values, 0, 102 );

    /**\todo smoothing */

    for( int i = 0 ; i< BANDS ; i++ )
    {
        char psz_val[5];
        float f_val = (float)(  bands[i]->value() ) / 10 - 20 ;
        sprintf( psz_values, "%s %f", psz_values, f_val );
        sprintf( psz_val, "% 5.1f", f_val );
        band_texts[i]->setText( band_frequencies[i] + "\n" + psz_val + "dB" );
    }
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                          VLC_OBJECT_AOUT, FIND_ANYWHERE);
    if( p_aout )
    {
        delCallbacks( p_aout );
        var_SetString( p_aout, "equalizer-bands", psz_values );
        addCallbacks( p_aout );
        vlc_object_release( p_aout );
    }
}
void Equalizer::setValues( char *psz_bands, float f_preamp )
{
    char *p = psz_bands;
    if ( p )
    {
        for( int i = 0; i < 10; i++ )
        {
            char psz_val[5];
            float f = strtof( p, &p );
            int  i_val= (int)( ( f + 20 ) * 10 );
            bands[i]->setValue(  i_val );
            sprintf( psz_val, "% 5.1f", f );
            band_texts[i]->setText( band_frequencies[i] + "\n" + psz_val +
                                    "dB" );
            if( p == NULL || *p == '\0' ) break;
            p++;
            if( *p == '\0' )  break;
        }
    }
    char psz_val[5];
    int i_val = (int)( ( f_preamp + 20 ) * 10 );
    sprintf( psz_val, "%.1f", f_preamp );
    ui.preampSlider->setValue( i_val );
    ui.preampLabel->setText( qtr("Preamp\n") + psz_val + qtr("dB") );
}

void Equalizer::setPreset( int preset )
{
    aout_instance_t *p_aout= (aout_instance_t *)vlc_object_find(p_intf,
                                                VLC_OBJECT_AOUT, FIND_ANYWHERE);

    char psz_values[102]; memset( psz_values, 0, 102 );
    for( int i = 0 ; i< 10 ;i++ )
        sprintf( psz_values, "%s %.1f", psz_values,
                                        eqz_preset_10b[preset]->f_amp[i] );

    if( p_aout )
    {
        delCallbacks( p_aout );
        var_SetString( p_aout, "equalizer-bands", psz_values );
        var_SetFloat( p_aout, "equalizer-preamp",
                      eqz_preset_10b[preset]->f_preamp );
        addCallbacks( p_aout );
        vlc_object_release( p_aout );
    }
    config_PutPsz( p_intf, "equalizer-bands", psz_values );
    config_PutFloat( p_intf, "equalizer-preamp",
                    eqz_preset_10b[preset]->f_preamp );

    setValues( psz_values, eqz_preset_10b[preset]->f_preamp );
}

void Equalizer::delCallbacks( aout_instance_t *p_aout )
{
//    var_DelCallback( p_aout, "equalizer-bands", EqzCallback, this );
//    var_DelCallback( p_aout, "equalizer-preamp", EqzCallback, this );
}

void Equalizer::addCallbacks( aout_instance_t *p_aout )
{
//    var_AddCallback( p_aout, "equalizer-bands", EqzCallback, this );
//    var_AddCallback( p_aout, "equalizer-preamp", EqzCallback, this );
}


/**********************************************************************
 * Video filters / Adjust
 **********************************************************************/

/**********************************************************************
 * Audio filters
 **********************************************************************/

/**********************************************************************
 * Extended playbak controls
 **********************************************************************/
