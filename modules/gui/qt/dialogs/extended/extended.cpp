/*****************************************************************************
 * extended.cpp : Extended controls - Undocked
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <cassert>

#include "extended.hpp"

#include "maininterface/compositor.hpp" /* Needed for external MI size */
#include "player/player_controller.hpp"

#include <QTabWidget>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QWindow>
#include <vlc_configuration.h>
#include <vlc_modules.h>

ExtendedDialog::ExtendedDialog( qt_intf_t *_p_intf )
               : QVLCDialog( nullptr, _p_intf )
{
#ifdef __APPLE__
    setWindowFlags( Qt::Drawer );
#else
    setWindowFlags( Qt::Tool );
#endif

    setWindowOpacity( var_InheritFloat( p_intf, "qt-opacity" ) );
    setWindowTitle( qtr( "Adjustments and Effects" ) );
    setWindowRole( "vlc-extended" );

    QVBoxLayout *layout = new QVBoxLayout( this );
    layout->setContentsMargins( 0, 2, 0, 1 );
    layout->setSpacing( 3 );

    mainTabW = new QTabWidget( this );

    /* AUDIO effects */
    QWidget *audioWidget = new QWidget;
    QHBoxLayout *audioLayout = new QHBoxLayout( audioWidget );
    QTabWidget *audioTab = new QTabWidget( audioWidget );

    equal = new Equalizer( p_intf, audioTab );
    connect( equal, &AudioFilterControlWidget::configChanged, this, &ExtendedDialog::putAudioConfig );
    audioTab->addTab( equal, qtr( "Equalizer" ) );

    Compressor *compres = new Compressor( p_intf, audioTab );
    connect( compres, &AudioFilterControlWidget::configChanged, this, &ExtendedDialog::putAudioConfig );
    audioTab->addTab( compres, qtr( "Compressor" ) );

    Spatializer *spatial = new Spatializer( p_intf, audioTab );
    connect( spatial, &AudioFilterControlWidget::configChanged, this, &ExtendedDialog::putAudioConfig );
    audioTab->addTab( spatial, qtr( "Spatializer" ) );

    StereoWidener *stereowiden = new StereoWidener( p_intf, audioTab );
    connect( stereowiden, &AudioFilterControlWidget::configChanged, this, &ExtendedDialog::putAudioConfig );
    audioTab->addTab( stereowiden, qtr( "Stereo Widener" ) );

    QWidget *advancedTab = new QWidget;
    QGridLayout *advancedTabLayout = new QGridLayout;

    PitchShifter *pitchshifter = new PitchShifter( p_intf, audioTab );
    connect( pitchshifter, &AudioFilterControlWidget::configChanged, this, &ExtendedDialog::putAudioConfig );

    StereoPanner *stereopanner = new StereoPanner( p_intf, audioTab );
    connect( stereopanner, &AudioFilterControlWidget::configChanged, this, &ExtendedDialog::putAudioConfig );

    advancedTabLayout->setColumnStretch( 2, 10 );
    advancedTabLayout->addWidget( pitchshifter );
    advancedTabLayout->addWidget( stereopanner );

    advancedTab->setLayout( advancedTabLayout );
    audioTab->addTab( advancedTab, qtr( "Advanced" ) );

    audioLayout->addWidget( audioTab );

    mainTabW->insertTab( AUDIO_TAB, audioWidget, qtr( "Audio Effects" ) );

    /* Video Effects */
    QWidget *videoWidget = new QWidget;
    QHBoxLayout *videoLayout = new QHBoxLayout( videoWidget );
    QTabWidget *videoTab = new QTabWidget( videoWidget );

    videoEffect = new ExtVideo( p_intf, videoTab );
    connect( videoEffect, &ExtVideo::configChanged, this, &ExtendedDialog::putVideoConfig );
    videoLayout->addWidget( videoTab );
    videoTab->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Maximum );

    mainTabW->insertTab( VIDEO_TAB, videoWidget, qtr( "Video Effects" ) );

    syncW = new SyncControls( p_intf, mainTabW );
    mainTabW->insertTab( SYNCHRO_TAB, syncW, qtr( "Synchronization" ) );

    if( module_exists( "v4l2" ) )
    {
        ExtV4l2 *v4l2 = new ExtV4l2( p_intf, mainTabW );
        mainTabW->insertTab( V4L2_TAB, v4l2, qtr( "v4l2 controls" ) );
    }

    layout->addWidget( mainTabW );
    connect( mainTabW, &QTabWidget::currentChanged, this, &ExtendedDialog::currentTabChanged );

    /* Bottom buttons */
    QDialogButtonBox *buttonBox = new QDialogButtonBox( Qt::Horizontal, this );

    m_applyButton = new QPushButton( qtr("&Save"), this );
    m_applyButton->setEnabled( false );
    buttonBox->addButton( m_applyButton, QDialogButtonBox::ApplyRole );

    buttonBox->addButton(
        new QPushButton( qtr("&Close"), this ), QDialogButtonBox::RejectRole );
    layout->addWidget( buttonBox );

    connect( buttonBox, &QDialogButtonBox::rejected, this, &ExtendedDialog::close );
    connect( m_applyButton, &QPushButton::clicked, this, &ExtendedDialog::saveConfig );

    /* Restore geometry or move this dialog on the left pane of the MI */
    if( !restoreGeometry( getSettings()->value("EPanel/geometry").toByteArray() ) )
    {
        resize( QSize( 400, 280 ) );

        QWindow *window = p_intf->p_compositor->interfaceMainWindow();
        if( window && window->x() > 50 )
            move( ( window->x() - frameGeometry().width() - 10 ), window->y() );
        else
            move ( 450 , 0 );
    }

    connect( THEMIM, &PlayerController::playingStateChanged, this, &ExtendedDialog::changedItem );
}

ExtendedDialog::~ExtendedDialog()
{
    getSettings()->setValue("Epanel/geometry", saveGeometry());
}

void ExtendedDialog::showTab( int i )
{
    mainTabW->setCurrentIndex( i );
    show();
}

int ExtendedDialog::currentTab()
{
    return mainTabW->currentIndex();
}

void ExtendedDialog::changedItem( PlayerController::PlayingState i_status )
{
    if( i_status != PlayerController::PLAYING_STATE_STOPPED ) return;
    syncW->clean();
    videoEffect->clean();
}

void ExtendedDialog::currentTabChanged( int i )
{
    if( i == AUDIO_TAB || i == VIDEO_TAB )
    {
        m_applyButton->setVisible( true );
        m_applyButton->setEnabled( !m_hashConfigs[i].isEmpty() );
        m_applyButton->setFocusPolicy( Qt::StrongFocus );
    }
    else
    {
        m_applyButton->setVisible( false );
        m_applyButton->setFocusPolicy( Qt::NoFocus );
    }
}

void ExtendedDialog::putAudioConfig( const QString& name, const QVariant value )
{
    m_hashConfigs[AUDIO_TAB].insert( name, value );
    m_applyButton->setEnabled( true );
}

void ExtendedDialog::putVideoConfig( const QString& name, const QVariant value )
{
    m_hashConfigs[VIDEO_TAB].insert( name, value );
    m_applyButton->setEnabled( true );
}

void ExtendedDialog::saveConfig()
{
    assert( currentTab() == AUDIO_TAB || currentTab() == VIDEO_TAB );
    QHash<QString, QVariant> *hashConfig = &m_hashConfigs[currentTab()];

    for( QHash<QString, QVariant>::iterator i = hashConfig->begin();
         i != hashConfig->end(); ++i )
    {
        QVariant &value = i.value();
        switch( static_cast<QMetaType::Type>(value.typeId()) )
        {
            case QMetaType::QString:
                config_PutPsz( qtu(i.key()), qtu(value.toString()) );
                break;
            case QMetaType::Int:
            case QMetaType::Bool:
                config_PutInt( qtu(i.key()), value.toInt() ) ;
                break;
            case QMetaType::Double:
            case QMetaType::Float:
                config_PutFloat( qtu(i.key()), value.toFloat() ) ;
                break;
            default:
                vlc_assert_unreachable();
        }
    }
    config_SaveConfigFile( p_intf );
    hashConfig->clear();
    m_applyButton->setEnabled( false );
}
