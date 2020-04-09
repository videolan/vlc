/*****************************************************************************
 * simple_preferences.cpp : "Simple preferences"
 ****************************************************************************
 * Copyright (C) 2006-2010 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "simple_preferences.hpp"
#include "preferences_widgets.hpp"

#include <vlc_config_cat.h>
#include <vlc_configuration.h>
#include <vlc_aout.h>

#include <QString>
#include <QFont>
#include <QToolButton>
#include <QButtonGroup>
#include <QSignalMapper>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QHeaderView>

#include <QStyleFactory>
#include <QSettings>
#include <QScreen>
#include <QtAlgorithms>
#include <QDir>

#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QWidget>
#include <QHBoxLayout>
#include <QDialog>
#include <QBoxLayout>

#include <assert.h>
#include <math.h>

#define ICON_HEIGHT 48
#define ICON_WIDTH 48

#ifdef _WIN32
# include <vlc_charset.h>
# include <shobjidl.h>
#endif
#include <vlc_modules.h>
#include <vlc_arrays.h>

static struct {
    const char iso[6];
    const char name[34];

} const language_map[] = {
    { "auto",  N_("Auto") },
    { "en",    "American English" },
    { "ar",    "عربي" },
    { "bn",    "বাংলা" },
    { "pt_BR", "Português Brasileiro" },
    { "en_GB", "British English" },
    { "el",    "Νέα Ελληνικά" },
    { "bg",    "български език" },
    { "ca",    "Català" },
    { "zh_TW", "正體中文" },
    { "cs",    "Čeština" },
    { "cy",    "Cymraeg" },
    { "da",    "Dansk" },
    { "nl",    "Nederlands" },
    { "fi",    "Suomi" },
    { "et",    "eesti keel" },
    { "eu",    "Euskara" },
    { "fr",    "Français" },
    { "ga",    "Gaeilge" },
    { "gd",    "Gàidhlig" },
    { "gl",    "Galego" },
    { "ka",    "ქართული" },
    { "de",    "Deutsch" },
    { "he",    "עברית" },
    { "hr",    "hrvatski" },
    { "hu",    "Magyar" },
    { "hy",    "հայերեն" },
    { "is",    "íslenska" },
    { "id",    "Bahasa Indonesia" },
    { "it",    "Italiano" },
    { "ja",    "日本語" },
    { "ko",    "한국어" },
    { "lt",    "lietuvių" },
    { "mn",    "Монгол хэл" },
    { "ms",    "Melayu" },
    { "nb",    "Bokmål" },
    { "nn",    "Nynorsk" },
    { "kk",    "Қазақ тілі" },
    { "km",    "ភាសាខ្មែរ" },
    { "ne",    "नेपाली" },
    { "oc",    "Occitan" },
    { "fa",    "فارسی" },
    { "pl",    "Polski" },
    { "pt_PT", "Português" },
    { "pa",    "ਪੰਜਾਬੀ" },
    { "ro",    "Română" },
    { "ru",    "Русский" },
    { "zh_CN", "简体中文" },
    { "si",    "සිංහල" },
    { "sr",    "српски" },
    { "sk",    "Slovensky" },
    { "sl",    "slovenščina" },
    { "ckb",   "کوردیی سۆرانی" },
    { "es",    "Español" },
    { "sv",    "Svenska" },
    { "te",    "తెలుగు" },
    { "tr",    "Türkçe" },
    { "uk",    "украї́нська мо́ва" },
    { "vi",    "tiếng Việt" },
    { "wa",    "Walon" }
};

static int getDefaultAudioVolume(const char *aout)
{
    if (!strcmp(aout, "") || !strcmp(aout, "any"))
#ifdef _WIN32
        /* All Windows aouts, that can be selected automatically, handle volume
         * saving. In case of automatic mode, we'll save the last volume for
         * every modules. Therefore, all volumes variable we be the same and we
         * can use the first one (mmdevice). */
        return config_GetFloat("mmdevice-volume") * 100.f + .5f;
#else
        return -1;
#endif
    else
    /* Note: For hysterical raisins, this is sorted by decreasing priority
     * order (then alphabetical order). */
    if (!strcmp(aout, "pulse"))
        return -1;
    else
#ifdef __linux__
    if (!strcmp(aout, "alsa") && module_exists("alsa"))
        return cbrtf(config_GetFloat("alsa-gain")) * 100.f + .5f;
    else
#endif
#ifdef _WIN32
    if (!strcmp(aout, "mmdevice"))
        return config_GetFloat("mmdevice-volume") * 100.f + .5f;
    else
#endif
#ifdef __APPLE__
    if (!strcmp(aout, "auhal") && module_exists("auhal"))
        return (config_GetFloat("auhal-volume") * 100.f + .5f)
                 / AOUT_VOLUME_DEFAULT;
    else
#endif
#ifdef _WIN32
    if (!strcmp(aout, "directsound") && module_exists("directsound"))
        return config_GetFloat("directx-volume") * 100.f + .5f;
    else
#endif
    if (!strcmp(aout, "jack"))
        return cbrtf(config_GetFloat("jack-gain")) * 100.f + 0.5f;
    else
#ifdef __OS2__
    if (!strcmp(aout, "kai"))
        return cbrtf(config_GetFloat("kai-gain")) * 100.f + .5f;
    else
#endif
#ifdef _WIN32
    if (!strcmp(aout, "waveout"))
        return config_GetFloat("waveout-volume") * 100.f + .5f;
    else
#endif
        return -1;
}

/*********************************************************************
 * The List of categories
 *********************************************************************/
SPrefsCatList::SPrefsCatList( intf_thread_t *_p_intf, QWidget *_parent ) :
                                  QWidget( _parent ), p_intf( _p_intf )
{
    QHBoxLayout *layout = new QHBoxLayout();

    /* Use autoExclusive buttons and a mapper as QButtonGroup can't
       set focus (keys) when it manages the buttons's exclusivity.
       See QT bugs 131 & 816 and QAbstractButton's source code. */
    QSignalMapper *mapper = new QSignalMapper( layout );
    CONNECT( mapper, mapped(int), this, switchPanel(int) );

    QPixmap scaled;
#if HAS_QT56
    qreal dpr = devicePixelRatioF();
#else
    qreal dpr = devicePixelRatio();
#endif

#define ADD_CATEGORY( button, label, ltooltip, icon, numb )                 \
    QToolButton * button = new QToolButton( this );                         \
    /* Scale icon to non native size outside of toolbutton to avoid widget size */\
    /* computation using native size */\
    scaled = QPixmap( ":/prefsmenu/" #icon ".png" )\
             .scaledToHeight( ICON_HEIGHT * dpr, Qt::SmoothTransformation );\
    scaled.setDevicePixelRatio( dpr ); \
    button->setIcon( scaled );                \
    button->setText( label );                                               \
    button->setToolTip( ltooltip );                                         \
    button->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );              \
    button->setIconSize( QSize( ICON_WIDTH, ICON_HEIGHT ) );          \
    button->setMinimumWidth( 40 + ICON_WIDTH );\
    button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum); \
    button->setAutoRaise( true );                                           \
    button->setCheckable( true );                                           \
    button->setAutoExclusive( true );                                       \
    CONNECT( button, clicked(), mapper, map() );                            \
    mapper->setMapping( button, numb );                                     \
    layout->addWidget( button );

    ADD_CATEGORY( SPrefsInterface, qtr("Interface"), qtr("Interface Settings"),
                  cone_interface_64, 0 );
    ADD_CATEGORY( SPrefsAudio, qtr("Audio"), qtr("Audio Settings"),
                  cone_audio_64, 1 );
    ADD_CATEGORY( SPrefsVideo, qtr("Video"), qtr("Video Settings"),
                  cone_video_64, 2 );
    ADD_CATEGORY( SPrefsSubtitles, qtr(SUBPIC_TITLE), qtr("Subtitle & On Screen Display Settings"),
                  cone_subtitles_64, 3 );
    ADD_CATEGORY( SPrefsInputAndCodecs, qtr(INPUT_TITLE), qtr("Input & Codecs Settings"),
                  cone_input_64, 4 );
    ADD_CATEGORY( SPrefsHotkeys, qtr("Hotkeys"), qtr("Configure Hotkeys"),
                  cone_hotkeys_64, 5 );

#undef ADD_CATEGORY

    SPrefsInterface->setChecked( true );
    layout->setMargin( 0 );
    layout->setSpacing( 1 );

    setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Preferred);
    setMinimumWidth( ICON_HEIGHT * 6 + 10 );
    setLayout( layout );
}

void SPrefsCatList::switchPanel( int i )
{
    emit currentItemChanged( i );
}

/*********************************************************************
 * The Panels
 *********************************************************************/
SPrefsPanel::SPrefsPanel( intf_thread_t *_p_intf, QWidget *_parent,
                          int _number ) : QWidget( _parent ), p_intf( _p_intf )
{
    module_config_t *p_config;
    ConfigControl *control;
    number = _number;
    lang = NULL;
    radioGroup = NULL;

#define CONFIG_GENERIC( option, type, label, qcontrol )                   \
            p_config =  config_FindConfig( option );                      \
            if( p_config )                                                \
            {                                                             \
                control =  new type ## ConfigControl( VLC_OBJECT(p_intf), \
                           p_config, label, ui.qcontrol, false );         \
                controls.append( control );                               \
            }                                                             \
            else {                                                        \
                ui.qcontrol->setEnabled( false );                         \
                if( label ) label->setEnabled( false );                   \
            }

#define CONFIG_BOOL( option, qcontrol )                           \
            p_config =  config_FindConfig( option );                      \
            if( p_config )                                                \
            {                                                             \
                control =  new BoolConfigControl( VLC_OBJECT(p_intf),     \
                           p_config, NULL, ui.qcontrol );          \
                controls.append( control );                               \
            }                                                             \
            else { ui.qcontrol->setEnabled( false ); }


#define CONFIG_GENERIC_NO_UI( option, type, label, qcontrol )             \
            p_config =  config_FindConfig( option );                      \
            if( p_config )                                                \
            {                                                             \
                control =  new type ## ConfigControl( VLC_OBJECT(p_intf), \
                           p_config, label, qcontrol, false );            \
                controls.append( control );                               \
            }                                                             \
            else {                                                        \
                QWidget *widget = label;                                  \
                qcontrol->setVisible( false );                            \
                if( widget ) widget->setEnabled( false );                 \
            }


#define CONFIG_GENERIC_NO_BOOL( option, type, label, qcontrol )           \
            p_config =  config_FindConfig( option );                      \
            if( p_config )                                                \
            {                                                             \
                control =  new type ## ConfigControl( VLC_OBJECT(p_intf), \
                           p_config, label, ui.qcontrol );                \
                controls.append( control );                               \
            }

#define CONFIG_GENERIC_FILE( option, type, label, qcontrol, qbutton )     \
            p_config =  config_FindConfig( option );                      \
            if( p_config )                                                \
            {                                                             \
                control =  new type ## ConfigControl( VLC_OBJECT(p_intf), \
                           p_config, label, qcontrol, qbutton );          \
                controls.append( control );                               \
            }

#define START_SPREFS_CAT( name , label )    \
        case SPrefs ## name:                \
        {                                   \
            Ui::SPrefs ## name ui;      \
            ui.setupUi( panel );            \
            panel_label->setText( label );

#define END_SPREFS_CAT      \
            break;          \
        }

    QVBoxLayout *panel_layout = new QVBoxLayout();
    QWidget *panel = new QWidget();
    panel_layout->setMargin( 3 );

    // Title Label
    QLabel *panel_label = new QLabel;
    QFont labelFont = QApplication::font();
    labelFont.setPointSize( labelFont.pointSize() + 6 );
    panel_label->setFont( labelFont );

    // Title <hr>
    QFrame *title_line = new QFrame;
    title_line->setFrameShape(QFrame::HLine);
    title_line->setFrameShadow(QFrame::Sunken);

    QFont italicFont = QApplication::font();
    italicFont.setItalic( true );

    switch( number )
    {
        /******************************
         * VIDEO Panel Implementation *
         ******************************/
        START_SPREFS_CAT( Video , qtr("Video Settings") );
            CONFIG_BOOL( "video", enableVideo );
            ui.videoZone->setEnabled( ui.enableVideo->isChecked() );
            CONNECT( ui.enableVideo, toggled( bool ),
                     ui.videoZone, setEnabled( bool ) );

            CONFIG_BOOL( "fullscreen", fullscreen );
            CONFIG_BOOL( "video-deco", windowDecorations );
            CONFIG_GENERIC( "vout", StringList, ui.voutLabel, outputModule );

            optionWidgets["videoOutCoB"] = ui.outputModule;

            optionWidgets["fullscreenScreenB"] = ui.fullscreenScreenBox;
            ui.fullscreenScreenBox->addItem( qtr("Automatic"), -1 );
            int i_screenCount = 0;
            foreach( QScreen* screen, QGuiApplication::screens() )
            {
                ui.fullscreenScreenBox->addItem( screen->name(), i_screenCount );
                i_screenCount++;
            }
            p_config =  config_FindConfig( "qt-fullscreen-screennumber" );
            if( p_config )
            {
                int i_defaultScreen = p_config->value.i + 1;
                if ( i_defaultScreen < 0 || i_defaultScreen > ( ui.fullscreenScreenBox->count() - 1 ) )
                    ui.fullscreenScreenBox->setCurrentIndex( 0 );
                else
                    ui.fullscreenScreenBox->setCurrentIndex(p_config->value.i + 1);
            }

#ifdef _WIN32
            CONFIG_BOOL( "directx-hw-yuv", hwYUVBox );
#endif

#ifdef __OS2__
            CONFIG_BOOL( "kva-fixt23", kvaFixT23 );
            CONFIG_GENERIC( "kva-video-mode", StringList, ui.kvaVideoModeLabel,
                            kvaVideoMode );
#else
            ui.kvaBox->setVisible( false );
#endif

            CONFIG_GENERIC( "deinterlace", IntegerList, ui.deinterLabel, deinterlaceBox );
            CONFIG_GENERIC( "deinterlace-mode", StringList, ui.deinterModeLabel, deinterlaceModeBox );
            CONFIG_GENERIC( "aspect-ratio", String, ui.arLabel, arLine );

            CONFIG_GENERIC_FILE( "snapshot-path", Directory, ui.dirLabel,
                                 ui.snapshotsDirectory, ui.snapshotsDirectoryBrowse );
            CONFIG_GENERIC( "snapshot-prefix", String, ui.prefixLabel, snapshotsPrefix );
            CONFIG_BOOL( "snapshot-sequential",
                            snapshotsSequentialNumbering );
            CONFIG_GENERIC( "snapshot-format", StringList, ui.arLabel,
                            snapshotsFormat );
         END_SPREFS_CAT;

        /******************************
         * AUDIO Panel Implementation *
         ******************************/
        START_SPREFS_CAT( Audio, qtr("Audio Settings") );

            CONFIG_BOOL( "audio", enableAudio );
            ui.audioZone->setEnabled( ui.enableAudio->isChecked() );
            CONNECT( ui.enableAudio, toggled( bool ),
                     ui.audioZone, setEnabled( bool ) );

#define audioCommon( name ) \
            QLabel * name ## Label = new QLabel( qtr( "Device:" ) ); \
            name ## Label->setMinimumSize(QSize(250, 0)); \
            outputAudioLayout->addWidget( name ## Label, outputAudioLayout->rowCount(), 0, 1, 1 ); \

#define audioControl( name) \
            audioCommon( name ) \
            QComboBox * name ## Device = new QComboBox; \
            name ## Label->setBuddy( name ## Device ); \
            name ## Device->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred  );\
            outputAudioLayout->addWidget( name ## Device, outputAudioLayout->rowCount() - 1, 1, 1, -1 );

#define audioControl2( name) \
            audioCommon( name ) \
            QHBoxLayout * name ## hboxLayout = new QHBoxLayout; \
            QLineEdit * name ## Device = new QLineEdit; \
            name ## Label->setBuddy( name ## Device ); \
            name ## hboxLayout->addWidget( name ## Device ); \
            QPushButton * name ## Browse = new QPushButton( qtr( "Browse..." ) ); \
            name ## hboxLayout->addWidget( name ## Browse ); \
            outputAudioLayout->addLayout( name ## hboxLayout, outputAudioLayout->rowCount() - 1, 1, 1, 1, Qt::AlignLeft );

            /* Build if necessary */
            QGridLayout * outputAudioLayout = qobject_cast<QGridLayout *>(ui.outputAudioBox->layout());
#ifdef _WIN32
            audioControl( DirectX );
            optionWidgets["directxL" ] = DirectXLabel;
            optionWidgets["directxW" ] = DirectXDevice;
            CONFIG_GENERIC_NO_UI( "directx-audio-device", StringList,
                    DirectXLabel, DirectXDevice );

            audioControl( Waveout );
            optionWidgets["waveoutL" ] = WaveoutLabel;
            optionWidgets["waveoutW" ] = WaveoutDevice;
            CONFIG_GENERIC_NO_UI( "waveout-audio-device", StringList,
                    WaveoutLabel, WaveoutDevice );

#elif defined( __OS2__ )
            audioControl( kai );
            optionWidgets["kaiL"] = kaiLabel;
            optionWidgets["kaiW"] = kaiDevice;
            CONFIG_GENERIC_NO_UI( "kai-audio-device", StringList, kaiLabel,
                    kaiDevice );
#else
            if( module_exists( "alsa" ) )
            {
                audioControl( alsa );
                optionWidgets["alsaL"] = alsaLabel;
                optionWidgets["alsaW"] = alsaDevice;
                CONFIG_GENERIC_NO_UI( "alsa-audio-device" , StringList, alsaLabel,
                                alsaDevice );
            }
            if( module_exists( "oss" ) )
            {
                audioControl2( OSS );
                optionWidgets["ossL"] = OSSLabel;
                optionWidgets["ossW"] = OSSDevice;
                optionWidgets["ossB"] = OSSBrowse;
                CONFIG_GENERIC_FILE( "oss-audio-device" , File, OSSLabel, OSSDevice,
                                 OSSBrowse );
            }
#endif

#ifdef _WIN32
            audioControl( MMDevice );
            optionWidgets["mmdeviceL" ] = MMDeviceLabel;
            optionWidgets["mmdeviceW" ] = MMDeviceDevice;
            CONFIG_GENERIC_NO_UI( "mmdevice-audio-device", StringList,
                                  MMDeviceLabel, MMDeviceDevice );

            CONFIG_GENERIC( "mmdevice-passthrough", IntegerList,
                            ui.mmdevicePassthroughLabel, mmdevicePassthroughBox );
            optionWidgets["mmdevicePassthroughL"] = ui.mmdevicePassthroughLabel;
            optionWidgets["mmdevicePassthroughB"] = ui.mmdevicePassthroughBox;
#else
            ui.mmdevicePassthroughLabel->setVisible( false );
            ui.mmdevicePassthroughBox->setVisible( false );
#endif


#undef audioControl2
#undef audioControl
#undef audioCommon

            int i_max_volume = config_GetInt( "qt-max-volume" );

            /* Audio Options */
            ui.volumeValue->setMaximum( i_max_volume );
            ui.defaultVolume->setMaximum( i_max_volume );

            CONNECT( ui.defaultVolume, valueChanged( int ),
                     this, updateAudioVolume( int ) );

            ui.defaultVolume_zone->setEnabled( ui.resetVolumeCheckbox->isChecked() );
            CONNECT( ui.resetVolumeCheckbox, toggled( bool ),
                     ui.defaultVolume_zone, setEnabled( bool ) );

            CONFIG_GENERIC( "audio-language" , String , ui.langLabel,
                            preferredAudioLanguage );

            CONFIG_BOOL( "spdif", spdifBox );

            CONFIG_GENERIC_NO_BOOL( "norm-max-level" , Float, NULL,
                                    volNormSpin );
            CONFIG_GENERIC( "audio-replay-gain-mode", StringList, ui.replayLabel,
                            replayCombo );
            CONFIG_GENERIC( "audio-visual" , StringList, ui.visuLabel,
                            visualisation);
            CONFIG_BOOL( "audio-time-stretch", autoscaleBox );

            /* Audio Output Specifics */
            CONFIG_GENERIC( "aout", StringList, ui.outputLabel, outputModule );

            CONNECT( ui.outputModule, currentIndexChanged( int ),
                     this, updateAudioOptions( int ) );

            /* File output exists on all platforms */
            CONFIG_GENERIC_FILE( "audiofile-file", File, ui.fileLabel,
                                 ui.fileName, ui.fileBrowseButton );

            optionWidgets["fileW"] = ui.fileControl;
            optionWidgets["audioOutCoB"] = ui.outputModule;
            optionWidgets["normalizerChB"] = ui.volNormBox;
            /*Little mofification of ui.volumeValue to compile with Qt < 4.3 */
            ui.volumeValue->setButtonSymbols(QAbstractSpinBox::NoButtons);
            optionWidgets["volLW"] = ui.volumeValue;
            optionWidgets["spdifChB"] = ui.spdifBox;
            optionWidgets["defaultVolume"] = ui.defaultVolume;
            optionWidgets["resetVolumeCheckbox"] = ui.resetVolumeCheckbox;
            updateAudioOptions( ui.outputModule->currentIndex() );

            /* LastFM */
            if( module_exists( "audioscrobbler" ) )
            {
                CONFIG_GENERIC( "lastfm-username", String, ui.lastfm_user_label,
                        lastfm_user_edit );
                CONFIG_GENERIC( "lastfm-password", String, ui.lastfm_pass_label,
                        lastfm_pass_edit );

                if( config_ExistIntf( "audioscrobbler" ) )
                    ui.lastfm->setChecked( true );
                else
                    ui.lastfm->setChecked( false );

                ui.lastfm_zone->setVisible( ui.lastfm->isChecked() );

                CONNECT( ui.lastfm, toggled( bool ),
                         ui.lastfm_zone, setVisible( bool ) );
                CONNECT( ui.lastfm, stateChanged( int ),
                         this, lastfm_Changed( int ) );
            }
            else
            {
                ui.lastfm->hide();
                ui.lastfm_zone->hide();
            }

            /* Normalizer */
            CONNECT( ui.volNormBox, toggled( bool ), ui.volNormSpin,
                     setEnabled( bool ) );

            char* psz = config_GetPsz( "audio-filter" );
            qs_filter = qfu( psz ).split( ':', QString::SkipEmptyParts );
            free( psz );

            bool b_enabled = ( qs_filter.contains( "normvol" ) );
            ui.volNormBox->setChecked( b_enabled );
            ui.volNormSpin->setEnabled( b_enabled );

            /* Volume Label */
            updateAudioVolume( ui.defaultVolume->value() ); // First time init

        END_SPREFS_CAT;

        /* Input and Codecs Panel Implementation */
        START_SPREFS_CAT( InputAndCodecs, qtr("Input & Codecs Settings") );

            /* Disk Devices */
            {
                ui.DVDDeviceComboBox->setToolTip(
                    qtr( "If this property is blank, different values\n"
                         "for DVD, VCD, and CDDA are set.\n"
                         "You can define a unique one or configure them \n"
                         "individually in the advanced preferences." ) );
                char *psz_dvddiscpath = config_GetPsz( "dvd" );
                char *psz_vcddiscpath = config_GetPsz( "vcd" );
                char *psz_cddadiscpath = config_GetPsz( "cd-audio" );
                if( psz_dvddiscpath && psz_vcddiscpath && psz_cddadiscpath )
                if( !strcmp( psz_cddadiscpath, psz_dvddiscpath ) &&
                    !strcmp( psz_dvddiscpath, psz_vcddiscpath ) )
                {
                    ui.DVDDeviceComboBox->setEditText( qfu( psz_dvddiscpath ) );
                }
                free( psz_cddadiscpath );
                free( psz_dvddiscpath );
                free( psz_vcddiscpath );
            }
#ifndef _WIN32
            QStringList DVDDeviceComboBoxStringList = QStringList();
            DVDDeviceComboBoxStringList
                    << "dvd*" << "scd*" << "sr*" << "sg*" << "cd*";
            ui.DVDDeviceComboBox->addItems( QDir( "/dev/" )
                    .entryList( DVDDeviceComboBoxStringList, QDir::System )
                    .replaceInStrings( QRegExp("^"), "/dev/" )
            );
#endif
            CONFIG_GENERIC( "dvd", String, ui.DVDLabel,
                            DVDDeviceComboBox->lineEdit() );
            CONFIG_GENERIC_FILE( "input-record-path", Directory, ui.recordLabel,
                                 ui.recordPath, ui.recordBrowse );

            CONFIG_GENERIC( "http-proxy", String , ui.httpProxyLabel, proxy );
            CONFIG_GENERIC_NO_BOOL( "postproc-q", Integer, ui.ppLabel,
                                    PostProcLevel );
            CONFIG_GENERIC( "avi-index", IntegerList, ui.aviLabel, AviRepair );

            /* live555 module prefs */
            CONFIG_BOOL( "rtsp-tcp",
                                live555TransportRTSP_TCPRadio );
            if ( !module_exists( "live555" ) )
            {
                ui.live555TransportRTSP_TCPRadio->hide();
                ui.live555TransportHTTPRadio->hide();
                ui.live555TransportLabel->hide();
            }
            CONFIG_GENERIC( "dec-dev", StringList, ui.hwAccelLabel, hwAccelModule );
            CONFIG_BOOL( "input-fast-seek", fastSeekBox );
            optionWidgets["inputLE"] = ui.DVDDeviceComboBox;
            optionWidgets["cachingCoB"] = ui.cachingCombo;
            CONFIG_GENERIC( "avcodec-skiploopfilter", IntegerList, ui.filterLabel, loopFilterBox );
            CONFIG_GENERIC( "sout-x264-tune", StringList, ui.x264Label, tuneBox );
            CONFIG_GENERIC( "sout-x264-preset", StringList, ui.x264Label, presetBox );
            CONFIG_GENERIC( "sout-x264-profile", StringList, ui.x264profileLabel, profileBox );
            CONFIG_GENERIC( "sout-x264-level", String, ui.x264profileLabel, levelBox );
            CONFIG_BOOL( "mkv-preload-local-dir", mkvPreloadBox );

            /* Caching */
            /* Add the things to the ComboBox */
            #define addToCachingBox( str, cachingNumber ) \
                ui.cachingCombo->addItem( qtr(str), QVariant( cachingNumber ) );
            addToCachingBox( N_("Custom"), CachingCustom );
            addToCachingBox( N_("Lowest latency"), CachingLowest );
            addToCachingBox( N_("Low latency"), CachingLow );
            addToCachingBox( N_("Normal"), CachingNormal );
            addToCachingBox( N_("High latency"), CachingHigh );
            addToCachingBox( N_("Higher latency"), CachingHigher );
            #undef addToCachingBox

#define TestCaC( name, factor ) \
    b_cache_equal =  b_cache_equal && \
     ( i_cache * factor == config_GetInt( name ) );
            /* Select the accurate value of the ComboBox */
            bool b_cache_equal = true;
            int i_cache = config_GetInt( "file-caching" );

            TestCaC( "network-caching", 10/3 );
            TestCaC( "disc-caching", 1);
            TestCaC( "live-caching", 1 );
            if( b_cache_equal == 1 )
                ui.cachingCombo->setCurrentIndex(
                ui.cachingCombo->findData( QVariant( i_cache ) ) );
#undef TestCaC

        END_SPREFS_CAT;
        /*******************
         * Interface Panel *
         *******************/
        START_SPREFS_CAT( Interface, qtr("Interface Settings") );

#ifndef _WIN32
            ui.langBox->hide();
#else
            for( size_t i = 0; i < ARRAY_SIZE( language_map ); i++)
                ui.langCombo->addItem( qfu( language_map[i].name ), language_map[i].iso );
            CONNECT( ui.langCombo, currentIndexChanged( int ), this, langChanged( int ) );

            HKEY h_key;
            char *langReg = NULL;
            if( RegOpenKeyEx( HKEY_CURRENT_USER, TEXT("Software\\VideoLAN\\VLC\\"), 0, KEY_READ, &h_key )
                    == ERROR_SUCCESS )
            {
                WCHAR szData[256];
                DWORD len = 256;
                if( RegQueryValueEx( h_key, TEXT("Lang"), NULL, NULL, (LPBYTE) &szData, &len ) == ERROR_SUCCESS ) {
                    langReg = FromWide( szData );
                    ui.langCombo->setCurrentIndex( ui.langCombo->findData(langReg) );
                }
            }
            free( langReg);
#endif

//            ui.defaultLabel->setFont( italicFont );
            ui.skinsLabel->setText(
                    qtr( "This is VLC's skinnable interface. You can download other skins at" )
                    + QString( " <a href=\"http://www.videolan.org/vlc/skins.php\">" )
                    + qtr( "VLC skins website" )+ QString( "</a>." ) );
            ui.skinsLabel->setFont( italicFont );

            if ( vlc_ml_instance_get( p_intf ) != NULL )
            {
                mlModel = new MlFoldersModel( this );
                mlModel->setMl( vlc_ml_instance_get( p_intf ) );

                mlTableView = ui.entryPointsTV;

                mlTableView->setModel( mlModel );

                connect( mlModel , &QAbstractItemModel::modelReset , this , &SPrefsPanel::MLdrawControls );

                BUTTONACT( ui.addButton , MLaddNewEntryPoint() );

                MLdrawControls( );

            }else {
                ui.mlGroupBox->hide( );
            }

#ifdef _WIN32
            BUTTONACT( ui.assoButton, assoDialog() );
#else
            ui.osGroupBox->hide();
#endif

            /* interface */
            char *psz_intf = config_GetPsz( "intf" );
            if( psz_intf )
            {
                if( strstr( psz_intf, "skin" ) )
                    ui.skins->setChecked( true );
            } else {
                /* defaults to qt */
                ui.qt->setChecked( true );
            }
            free( psz_intf );

            optionWidgets["skinRB"] = ui.skins;
            optionWidgets["qtRB"] = ui.qt;
#if !defined( _WIN32)
            ui.stylesCombo->addItem( qtr("System's default") );
            ui.stylesCombo->addItems( QStyleFactory::keys() );
            ui.stylesCombo->setCurrentIndex( ui.stylesCombo->findText(
                        getSettings()->value( "MainWindow/QtStyle", "" ).toString() ) );
            ui.stylesCombo->insertSeparator( 1 );
            if ( ui.stylesCombo->currentIndex() < 0 )
                ui.stylesCombo->setCurrentIndex( 0 ); /* default */

            CONNECT( ui.stylesCombo, currentIndexChanged( QString ), this, changeStyle( QString ) );
            optionWidgets["styleCB"] = ui.stylesCombo;
#else
            ui.stylesCombo->hide();
            ui.stylesLabel->hide();
#endif
            radioGroup = new QButtonGroup(this);
            radioGroup->addButton( ui.qt, 0 );
            radioGroup->addButton( ui.skins, 1 );
            CONNECT( radioGroup, buttonClicked( int ),
                     ui.styleStackedWidget, setCurrentIndex( int ) );
            ui.styleStackedWidget->setCurrentIndex( radioGroup->checkedId() );

            CONNECT( ui.minimalviewBox, toggled( bool ),
                     ui.mainPreview, setNormalPreview( bool ) );
            CONFIG_BOOL( "qt-minimal-view", minimalviewBox );
            ui.mainPreview->setNormalPreview( ui.minimalviewBox->isChecked() );
            ui.skinsPreview->setPreview( InterfacePreviewWidget::SKINS );

            CONFIG_BOOL( "embedded-video", embedVideo );
            CONFIG_BOOL( "qt-video-autoresize", resizingBox );
            CONNECT( ui.embedVideo, toggled( bool ), ui.resizingBox, setEnabled( bool ) );
            ui.resizingBox->setEnabled( ui.embedVideo->isChecked() );

            CONFIG_BOOL( "qt-fs-controller", fsController );
            CONFIG_BOOL( "qt-system-tray", systrayBox );
            CONFIG_GENERIC( "qt-notification", IntegerList, ui.notificationComboLabel,
                                                      notificationCombo );
            CONNECT( ui.systrayBox, toggled( bool ), ui.notificationCombo, setEnabled( bool ) );
            CONNECT( ui.systrayBox, toggled( bool ), ui.notificationComboLabel, setEnabled( bool ) );
            ui.notificationCombo->setEnabled( ui.systrayBox->isChecked() );

            CONFIG_BOOL( "qt-pause-minimized", pauseMinimizedBox );
            CONFIG_BOOL( "playlist-tree", treePlaylist );
            CONFIG_BOOL( "play-and-pause", playPauseBox );
            CONFIG_GENERIC_FILE( "skins2-last", File, ui.skinFileLabel,
                                 ui.fileSkin, ui.skinBrowse );

            CONFIG_BOOL( "metadata-network-access", MetadataNetworkAccessMode );

            /* UPDATE options */
#ifdef UPDATE_CHECK
            CONFIG_BOOL( "qt-updates-notif", updatesBox );
            CONFIG_GENERIC_NO_BOOL( "qt-updates-days", Integer, NULL,
                    updatesDays );
            ui.updatesDays->setEnabled( ui.updatesBox->isChecked() );
            CONNECT( ui.updatesBox, toggled( bool ),
                     ui.updatesDays, setEnabled( bool ) );
#else
            ui.updatesBox->hide();
            ui.updatesDays->hide();
#endif
            /* ONE INSTANCE options */
#if !defined( _WIN32 ) && !defined(__APPLE__) && !defined(__OS2__)
            if( !module_exists( "dbus" ) )
                ui.OneInterfaceBox->hide();
            else
#endif
            {
                CONFIG_BOOL( "one-instance", OneInterfaceMode );
                CONFIG_BOOL( "playlist-enqueue", EnqueueOneInterfaceMode );
                ui.EnqueueOneInterfaceMode->setEnabled(
                                                       ui.OneInterfaceMode->isChecked() );
                CONNECT( ui.OneInterfaceMode, toggled( bool ),
                         ui.EnqueueOneInterfaceMode, setEnabled( bool ) );
                CONFIG_BOOL( "one-instance-when-started-from-file", oneInstanceFromFile );
            }

            /* RECENTLY PLAYED options */
            CONNECT( ui.saveRecentlyPlayed, toggled( bool ),
                     ui.recentlyPlayedFilters, setEnabled( bool ) );
            ui.recentlyPlayedFilters->setEnabled( false );
            CONFIG_BOOL( "qt-recentplay", saveRecentlyPlayed );
            CONFIG_GENERIC( "qt-continue", IntegerList, ui.continuePlaybackLabel, continuePlaybackComboBox );
            CONFIG_GENERIC( "qt-recentplay-filter", String, ui.filterLabel,
                    recentlyPlayedFilters );
            CONFIG_GENERIC( "qt-auto-raise", IntegerList, ui.autoRaiseLabel, autoRaiseComboBox );

        END_SPREFS_CAT;

        START_SPREFS_CAT( Subtitles,
                            qtr("Subtitle & On Screen Display Settings") );
            CONFIG_BOOL( "osd", OSDBox);
            CONFIG_BOOL( "video-title-show", OSDTitleBox);
            CONFIG_GENERIC( "video-title-position", IntegerList,
                            ui.OSDTitlePosLabel, OSDTitlePos );

            CONFIG_BOOL( "spu", spuActiveBox);
            ui.spuZone->setEnabled( ui.spuActiveBox->isChecked() );
            CONNECT( ui.spuActiveBox, toggled( bool ),
                     ui.spuZone, setEnabled( bool ) );

            CONFIG_GENERIC( "subsdec-encoding", StringList, ui.encodLabel,
                            encoding );
            CONFIG_GENERIC( "sub-language", String, ui.subLangLabel,
                            preferredLanguage );

            CONFIG_GENERIC( "freetype-rel-fontsize", IntegerList,
                            ui.fontSizeLabel, fontSize );

            CONFIG_GENERIC_NO_BOOL( "freetype-font", Font, ui.fontLabel, font );
            CONFIG_GENERIC_NO_BOOL( "freetype-color", Color, ui.fontColorLabel,
                            fontColor );
            CONFIG_GENERIC( "freetype-outline-thickness", IntegerList,
                            ui.fontEffectLabel, effect );
            CONFIG_GENERIC_NO_BOOL( "freetype-outline-color", Color, ui.outlineColorLabel,
                            outlineColor );

            CONFIG_GENERIC_NO_BOOL( "sub-margin", Integer, ui.subsPosLabel, subsPosition );

            ui.shadowCheck->setChecked( config_GetInt( "freetype-shadow-opacity" ) > 0 );
            ui.backgroundCheck->setChecked( config_GetInt( "freetype-background-opacity" ) > 0 );
            optionWidgets["shadowCB"] = ui.shadowCheck;
            optionWidgets["backgroundCB"] = ui.backgroundCheck;

            CONFIG_GENERIC( "secondary-sub-alignment", IntegerList,
                            ui.secondarySubsAlignmentLabel, secondarySubsAlignment );
            CONFIG_GENERIC_NO_BOOL( "secondary-sub-margin", Integer, ui.secondarySubsPosLabel, secondarySubsPosition );
        END_SPREFS_CAT;

        case SPrefsHotkeys:
        {
            p_config = config_FindConfig( "key-play" );

            QGridLayout *gLayout = new QGridLayout;
            panel->setLayout( gLayout );
            int line = 0;

            panel_label->setText( qtr( "Configure Hotkeys" ) );
            control = new KeySelectorControl( VLC_OBJECT(p_intf), p_config, this );
            control->insertIntoExistingGrid( gLayout, line );
            controls.append( control );

            line++;

            QFrame *sepline = new QFrame;
            sepline->setFrameStyle(QFrame::HLine | QFrame::Sunken);
            gLayout->addWidget( sepline, line, 0, 1, -1 );

            line++;

            p_config = config_FindConfig( "hotkeys-y-wheel-mode" );
            control = new IntegerListConfigControl( VLC_OBJECT(p_intf),
                    p_config, this, false );
            control->insertIntoExistingGrid( gLayout, line );
            controls.append( control );

            line++;

            p_config = config_FindConfig( "hotkeys-x-wheel-mode" );
            control = new IntegerListConfigControl( VLC_OBJECT(p_intf),
                    p_config, this, false );
            control->insertIntoExistingGrid( gLayout, line );
            controls.append( control );

#ifdef _WIN32
            line++;

            p_config = config_FindConfig( "qt-disable-volume-keys" );
            control = new BoolConfigControl( VLC_OBJECT(p_intf), p_config, this );
            control->insertIntoExistingGrid( gLayout, line );
            controls.append( control );
#endif

            break;
        }
    }

    panel_layout->addWidget( panel_label );
    panel_layout->addWidget( title_line );

    QScrollArea *scroller= new QScrollArea;
    scroller->setWidget( panel );
    scroller->setWidgetResizable( true );
    scroller->setFrameStyle( QFrame::NoFrame );
    panel_layout->addWidget( scroller );

    setLayout( panel_layout );

#undef END_SPREFS_CAT
#undef START_SPREFS_CAT
#undef CONFIG_GENERIC_FILE
#undef CONFIG_GENERIC_NO_BOOL
#undef CONFIG_GENERIC_NO_UI
#undef CONFIG_GENERIC
#undef CONFIG_BOOL
}


void SPrefsPanel::updateAudioOptions( int number)
{
    QString value = qobject_cast<QComboBox *>(optionWidgets["audioOutCoB"])
                                            ->itemData( number ).toString();
#ifdef _WIN32
    /* Since MMDevice is most likely to be used by default, we show MMDevice
     * options by default */
    const bool mmDeviceEnabled = value == "mmdevice" || value == "any";
    optionWidgets["mmdevicePassthroughL"]->setVisible( mmDeviceEnabled );
    optionWidgets["mmdevicePassthroughB"]->setVisible( mmDeviceEnabled );
    optionWidgets["mmdeviceW"]->setVisible( mmDeviceEnabled );
    optionWidgets["mmdeviceL"]->setVisible( mmDeviceEnabled );

    optionWidgets["directxW"]->setVisible( ( value == "directsound" ) );
    optionWidgets["directxL"]->setVisible( ( value == "directsound" ) );
    optionWidgets["waveoutW"]->setVisible( ( value == "waveout" ) );
    optionWidgets["waveoutL"]->setVisible( ( value == "waveout" ) );
#elif defined( __OS2__ )
    optionWidgets["kaiL"]->setVisible( ( value == "kai" ) );
    optionWidgets["kaiW"]->setVisible( ( value == "kai" ) );
#else
    /* optionWidgets["ossW] can be NULL */
    if( optionWidgets["ossW"] ) {
        optionWidgets["ossW"]->setVisible( ( value == "oss" ) );
        optionWidgets["ossL"]->setVisible( ( value == "oss" ) );
        optionWidgets["ossB"]->setVisible( ( value == "oss" ) );
    }
    /* optionWidgets["alsaW] can be NULL */
    if( optionWidgets["alsaW"] ) {
        optionWidgets["alsaW"]->setVisible( ( value == "alsa" ) );
        optionWidgets["alsaL"]->setVisible( ( value == "alsa" ) );
    }
#endif
    optionWidgets["fileW"]->setVisible( ( value == "afile" ) );
    optionWidgets["spdifChB"]->setVisible( ( value == "alsa" || value == "oss" || value == "auhal" ||
                                           value == "directsound" || value == "waveout" ) );

    int volume = getDefaultAudioVolume(qtu(value));
    bool save = true;

    if (volume >= 0)
        save = config_GetInt("volume-save");

    QCheckBox *resetVolumeCheckBox =
        qobject_cast<QCheckBox *>(optionWidgets["resetVolumeCheckbox"]);
    resetVolumeCheckBox->setChecked(!save);
    resetVolumeCheckBox->setEnabled(volume >= 0);

    QSlider *defaultVolume =
        qobject_cast<QSlider *>(optionWidgets["defaultVolume"]);
    defaultVolume->setValue((volume >= 0) ? volume : 100);
    defaultVolume->setEnabled(volume >= 0);
}


SPrefsPanel::~SPrefsPanel()
{
    qDeleteAll( controls ); controls.clear();
    free( lang );
}

void SPrefsPanel::updateAudioVolume( int volume )
{
    qobject_cast<QSpinBox *>(optionWidgets["volLW"])
        ->setValue( volume );
}


/* Function called from the main Preferences dialog on each SPrefs Panel */
void SPrefsPanel::apply()
{
    /* Generic save for ever panel */
    QList<ConfigControl *>::const_iterator i;
    for( i = controls.begin() ; i != controls.end() ; ++i )
    {
        ConfigControl *c = qobject_cast<ConfigControl *>(*i);
        c->doApply();
    }

    switch( number )
    {
    case SPrefsInputAndCodecs:
    {
        /* Device default selection */
        QByteArray devicepath =
            qobject_cast<QComboBox *>(optionWidgets["inputLE"])->currentText().toUtf8();
        if( devicepath.size() > 0 )
        {
            config_PutPsz( "dvd", devicepath );
            config_PutPsz( "vcd", devicepath );
            config_PutPsz( "cd-audio", devicepath );
        }

#define CaC( name, factor ) config_PutInt( name, i_comboValue * factor )
        /* Caching */
        QComboBox *cachingCombo = qobject_cast<QComboBox *>(optionWidgets["cachingCoB"]);
        int i_comboValue = cachingCombo->itemData( cachingCombo->currentIndex() ).toInt();
        if( i_comboValue )
        {
            CaC( "file-caching", 1 );
            CaC( "network-caching", 10/3 );
            CaC( "disc-caching", 1 );
            CaC( "live-caching", 1 );
        }
        break;
#undef CaC
    }

    /* Interfaces */
    case SPrefsInterface:
    {
        if( qobject_cast<QRadioButton *>(optionWidgets["skinRB"])->isChecked() )
            config_PutPsz( "intf", "skins2,any" );
        else
        //if( qobject_cast<QRadioButton *>(optionWidgets[qtRB])->isChecked() )
            config_PutPsz( "intf", "" );
        if( qobject_cast<QComboBox *>(optionWidgets["styleCB"]) )
            getSettings()->setValue( "MainWindow/QtStyle",
                qobject_cast<QComboBox *>(optionWidgets["styleCB"])->currentText() );
#ifdef _WIN32
    saveLang();
#endif
        break;
    }

    case SPrefsVideo:
    {
        int i_fullscreenScreen =  qobject_cast<QComboBox *>(optionWidgets["fullscreenScreenB"])->currentData().toInt();
        config_PutInt( "qt-fullscreen-screennumber", i_fullscreenScreen );
        break;
    }

    case SPrefsAudio:
    {
        bool b_checked =
            qobject_cast<QCheckBox *>(optionWidgets["normalizerChB"])->isChecked();
        if( b_checked && !qs_filter.contains( "normvol" ) )
            qs_filter.append( "normvol" );
        if( !b_checked && qs_filter.contains( "normvol" ) )
            qs_filter.removeAll( "normvol" );

        config_PutPsz( "audio-filter", qtu( qs_filter.join( ":" ) ) );

        /* Default volume */
        int i_volume =
            qobject_cast<QSlider *>(optionWidgets["defaultVolume"])->value();
        bool b_reset_volume =
            qobject_cast<QCheckBox *>(optionWidgets["resetVolumeCheckbox"])->isChecked();
        char *psz_aout = config_GetPsz( "aout" );

        float f_gain = powf( i_volume / 100.f, 3 );

#define save_vol_aout( name ) \
            module_exists( name ) && ( !psz_aout || !strcmp( psz_aout, name ) || !strcmp( psz_aout, "any" ) )

        //FIXME this is moot
#if defined( _WIN32 )
        VLC_UNUSED( f_gain );
        if( save_vol_aout( "mmdevice" ) )
            config_PutFloat( "mmdevice-volume", i_volume / 100.f );
        if( save_vol_aout( "directsound" ) )
            config_PutFloat( "directx-volume", i_volume / 100.f );
        if( save_vol_aout( "waveout" ) )
            config_PutFloat( "waveout-volume", i_volume / 100.f );
#elif defined( Q_OS_MAC )
        VLC_UNUSED( f_gain );
        if( save_vol_aout( "auhal" ) )
            config_PutFloat( "auhal-volume", i_volume / 100.f
                    * AOUT_VOLUME_DEFAULT );
#elif defined( __OS2__ )
        if( save_vol_aout( "kai" ) )
            config_PutFloat( "kai-gain",  f_gain );
#else
        if( save_vol_aout( "alsa" ) )
            config_PutFloat( "alsa-gain", f_gain );
        if( save_vol_aout( "jack" ) )
            config_PutFloat( "jack-gain", f_gain );
#endif
#undef save_vol_aout
        free( psz_aout );

        config_PutInt( "volume-save", !b_reset_volume );

        break;
    }
    case SPrefsSubtitles:
    {
        bool b_checked = qobject_cast<QCheckBox *>(optionWidgets["shadowCB"])->isChecked();
        if( b_checked && config_GetInt( "freetype-shadow-opacity" ) == 0 ) {
            config_PutInt( "freetype-shadow-opacity", 128 );
        }
        else if (!b_checked ) {
            config_PutInt( "freetype-shadow-opacity", 0 );
        }

        b_checked = qobject_cast<QCheckBox *>(optionWidgets["backgroundCB"])->isChecked();
        if( b_checked && config_GetInt( "freetype-background-opacity" ) == 0 ) {
            config_PutInt( "freetype-background-opacity", 128 );
        }
        else if (!b_checked ) {
            config_PutInt( "freetype-background-opacity", 0 );
        }

    }
    }
}

void SPrefsPanel::clean()
{}

void SPrefsPanel::lastfm_Changed( int i_state )
{
    if( i_state == Qt::Checked )
        config_AddIntf( "audioscrobbler" );
    else if( i_state == Qt::Unchecked )
        config_RemoveIntf( "audioscrobbler" );
}

void SPrefsPanel::changeStyle( QString s_style )
{
    QApplication::setStyle( s_style );

    /* force refresh on all widgets */
    QWidgetList widgets = QApplication::allWidgets();
    QWidgetList::iterator it = widgets.begin();
    while( it != widgets.end() ) {
        (*it)->update();
        ++it;
    };
}

void SPrefsPanel::langChanged( int i )
{
    free( lang );
    lang = strdup( language_map[i].iso );
}

void SPrefsPanel::configML()
{
#ifdef SQL_MEDIA_LIBRARY
    MLConfDialog *mld = new MLConfDialog( this, p_intf );
    mld->exec();
    delete mld;
#endif
}

#ifdef _WIN32
#include <QDialogButtonBox>
#include "util/registry.hpp"

void SPrefsPanel::cleanLang() {
    QVLCRegistry qvReg( HKEY_CURRENT_USER );
    qvReg.DeleteValue( "Software\\VideoLAN\\VLC\\", "Lang" );
    qvReg.DeleteKey( "Software\\VideoLAN\\", "VLC" );
    qvReg.DeleteKey( "Software\\", "VideoLAN" );
}

void SPrefsPanel::saveLang() {
    if( !lang ) return;

    if( !strncmp( lang, "auto", 4 ) ) {
        cleanLang();
    }
    else
    {
        QVLCRegistry qvReg( HKEY_CURRENT_USER );
        qvReg.WriteRegistry( "Software\\VideoLAN\\VLC\\", "Lang", lang );
    }
}

bool SPrefsPanel::addType( const char * psz_ext, QTreeWidgetItem* current,
                           QTreeWidgetItem* parent, QVLCRegistry *qvReg )
{
    bool b_temp;
    const char* psz_VLC = "VLC";
    current = new QTreeWidgetItem( parent, QStringList( psz_ext ) );

    char* psz_reg = qvReg->ReadRegistry( psz_ext, "", "" );
    if( psz_reg == NULL )
        return false;
    if( strstr( psz_reg, psz_VLC ) )
    {
        current->setCheckState( 0, Qt::Checked );
        b_temp = true;
    }
    else
    {
        current->setCheckState( 0, Qt::Unchecked );
        b_temp = false;
    }
    free( psz_reg );
    listAsso.append( current );
    return b_temp;
}

#if !defined(__IApplicationAssociationRegistrationUI_INTERFACE_DEFINED__)
#define __IApplicationAssociationRegistrationUI_INTERFACE_DEFINED__
    const GUID IID_IApplicationAssociationRegistrationUI = {0x1f76a169,0xf994,0x40ac, {0x8f,0xc8,0x09,0x59,0xe8,0x87,0x47,0x10}};
    extern const GUID CLSID_ApplicationAssociationRegistrationUI;
    interface IApplicationAssociationRegistrationUI : public IUnknown
    {
        virtual HRESULT STDMETHODCALLTYPE LaunchAdvancedAssociationUI(
                LPCWSTR pszAppRegName) = 0;
    };
#endif /* __IApplicationAssociationRegistrationUI_INTERFACE_DEFINED__ */

void SPrefsPanel::assoDialog()
{
    HRESULT hr;

    hr = CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );
    if( SUCCEEDED(hr) )
    {
        void *p;

        hr = CoCreateInstance(CLSID_ApplicationAssociationRegistrationUI,
                              NULL, CLSCTX_INPROC_SERVER,
                              IID_IApplicationAssociationRegistrationUI, &p);
        if( SUCCEEDED(hr) )
        {
            IApplicationAssociationRegistrationUI *p_regui =
                (IApplicationAssociationRegistrationUI *)p;

            hr = p_regui->LaunchAdvancedAssociationUI(L"VLC" );
            p_regui->Release();
        }
        CoUninitialize();
    }

    if( SUCCEEDED(hr) )
        return;

    QDialog *d = new QDialog( this );
    d->setWindowTitle( qtr( "File associations" ) );
    QGridLayout *assoLayout = new QGridLayout( d );

    QTreeWidget *filetypeList = new QTreeWidget;
    assoLayout->addWidget( filetypeList, 0, 0, 1, 4 );
    filetypeList->header()->hide();

    QVLCRegistry qvReg( HKEY_CLASSES_ROOT );

    QTreeWidgetItem *audioType = new QTreeWidgetItem( QStringList( qtr( "Audio Files" ) ) );
    QTreeWidgetItem *videoType = new QTreeWidgetItem( QStringList( qtr( "Video Files" ) ) );
    QTreeWidgetItem *otherType = new QTreeWidgetItem( QStringList( qtr( "Playlist Files" ) ) );

    filetypeList->addTopLevelItem( audioType );
    filetypeList->addTopLevelItem( videoType );
    filetypeList->addTopLevelItem( otherType );

    audioType->setExpanded( true ); audioType->setCheckState( 0, Qt::Unchecked );
    videoType->setExpanded( true ); videoType->setCheckState( 0, Qt::Unchecked );
    otherType->setExpanded( true ); otherType->setCheckState( 0, Qt::Unchecked );

    QTreeWidgetItem *currentItem = NULL;

    int i_temp = 0;
#define aTa( name ) i_temp += addType( name, currentItem, audioType, &qvReg )
#define aTv( name ) i_temp += addType( name, currentItem, videoType, &qvReg )
#define aTo( name ) i_temp += addType( name, currentItem, otherType, &qvReg )

    aTa( ".3ga" ); aTa( ".669" ); aTa( ".a52" ); aTa( ".aac" ); aTa( ".ac3" );
    aTa( ".adt" ); aTa( ".adts" ); aTa( ".aif" ); aTa( ".aifc" ); aTa( ".aiff" );
    aTa( ".au" ); aTa( ".amr" ); aTa( ".aob" ); aTa( ".ape" ); aTa( ".caf" );
    aTa( ".cda" ); aTa( ".dts" ); aTa( ".flac" ); aTa( ".it" ); aTa( ".m4a" );
    aTa( ".m4p" ); aTa( ".mid" ); aTa( ".mka" ); aTa( ".mlp" ); aTa( ".mod" );
    aTa( ".mp1" ); aTa( ".mp2" ); aTa( ".mp3" ); aTa( ".mpc" ); aTa( ".mpga" );
    aTa( ".oga" ); aTa( ".oma" ); aTa( ".opus" ); aTa( ".qcp" ); aTa( ".ra" );
    aTa( ".rmi" ); aTa( ".snd" ); aTa( ".s3m" ); aTa( ".spx" ); aTa( ".tta" );
    aTa( ".voc" ); aTa( ".vqf" ); aTa( ".w64" ); aTa( ".wav" ); aTa( ".wma" );
    aTa( ".wv" ); aTa( ".xa" ); aTa( ".xm" );
    audioType->setCheckState( 0, ( i_temp > 0 ) ?
                              ( ( i_temp == audioType->childCount() ) ?
                               Qt::Checked : Qt::PartiallyChecked )
                            : Qt::Unchecked );

    i_temp = 0;
    aTv( ".3g2" ); aTv( ".3gp" ); aTv( ".3gp2" ); aTv( ".3gpp" ); aTv( ".amv" );
    aTv( ".asf" ); aTv( ".avi" ); aTv( ".bik" ); aTv( ".divx" ); aTv( ".drc" );
    aTv( ".dv" ); aTv( ".f4v" ); aTv( ".flv" ); aTv( ".gvi" ); aTv( ".gxf" );
    aTv( ".m1v" ); aTv( ".m2t" ); aTv( ".m2v" ); aTv( ".m2ts" ); aTv( ".m4v" );
    aTv( ".mkv" ); aTv( ".mov" ); aTv( ".mp2v" ); aTv( ".mp4" ); aTv( ".mp4v" );
    aTv( ".mpa" ); aTv( ".mpe" ); aTv( ".mpeg" ); aTv( ".mpeg1" ); aTv( ".mpeg2" );
    aTv( ".mpeg4" ); aTv( ".mpg" ); aTv( ".mpv2" ); aTv( ".mts" ); aTv( ".mtv" );
    aTv( ".mxf" ); aTv( ".nsv" ); aTv( ".nuv" ); aTv( ".ogg" ); aTv( ".ogm" );
    aTv( ".ogx" ); aTv( ".ogv" ); aTv( ".rec" ); aTv( ".rm" ); aTv( ".rmvb" );
    aTv( ".rpl" ); aTv( ".thp" ); aTv( ".tod" ); aTv( ".ts" ); aTv( ".tts" );
    aTv( ".vob" ); aTv( ".vro" ); aTv( ".webm" ); aTv( ".wmv" ); aTv( ".xesc" );
    videoType->setCheckState( 0, ( i_temp > 0 ) ?
                              ( ( i_temp == videoType->childCount() ) ?
                               Qt::Checked : Qt::PartiallyChecked )
                            : Qt::Unchecked );

    i_temp = 0;
    aTo( ".asx" ); aTo( ".b4s" ); aTo( ".cue" ); aTo( ".ifo" ); aTo( ".m3u" );
    aTo( ".m3u8" ); aTo( ".pls" ); aTo( ".ram" ); aTo( ".sdp" ); aTo( ".vlc" );
    aTo( ".wvx" ); aTo( ".xspf" );
    otherType->setCheckState( 0, ( i_temp > 0 ) ?
                              ( ( i_temp == otherType->childCount() ) ?
                               Qt::Checked : Qt::PartiallyChecked )
                            : Qt::Unchecked );

#undef aTo
#undef aTv
#undef aTa

    CONNECT( filetypeList, itemChanged(QTreeWidgetItem*, int), this, updateCheckBoxes(QTreeWidgetItem*, int) );

    QDialogButtonBox *buttonBox = new QDialogButtonBox( d );
    QPushButton *closeButton = new QPushButton( qtr( "&Apply" ) );
    QPushButton *clearButton = new QPushButton( qtr( "&Cancel" ) );
    buttonBox->addButton( closeButton, QDialogButtonBox::AcceptRole );
    buttonBox->addButton( clearButton, QDialogButtonBox::ActionRole );

    assoLayout->addWidget( buttonBox, 1, 2, 1, 2 );

    CONNECT( closeButton, clicked(), this, saveAsso() );
    CONNECT( clearButton, clicked(), d, reject() );
    d->resize( 300, 400 );
    d->exec();
    listAsso.clear();
}

void SPrefsPanel::updateCheckBoxes(QTreeWidgetItem* item, int column)
{
    if( column != 0 )
        return;

    /* temporarily block signals to avoid signal loops */
    bool b_signalsBlocked = item->treeWidget()->blockSignals(true);

    /* A parent checkbox was changed */
    if( item->parent() == 0 )
    {
        Qt::CheckState checkState = item->checkState(0);
        for( int i = 0; i < item->childCount(); i++ )
        {
            item->child(i)->setCheckState(0, checkState);
        }
    }

    /* A child checkbox was changed */
    else
    {
        bool b_diff = false;
        for( int i = 0; i < item->parent()->childCount(); i++ )
        {
            if( i != item->parent()->indexOfChild(item) && item->checkState(0) != item->parent()->child(i)->checkState(0) )
            {
                b_diff = true;
                break;
            }
        }

        if( b_diff )
            item->parent()->setCheckState(0, Qt::PartiallyChecked);
        else
            item->parent()->setCheckState(0, item->checkState(0));
    }

    /* Stop signal blocking */
    item->treeWidget()->blockSignals(b_signalsBlocked);
}

void addAsso( QVLCRegistry *qvReg, const char *psz_ext )
{
    QString s_path( "VLC" ); s_path += psz_ext;
    QString s_path2 = s_path;

    /* Save a backup if already assigned */
    char *psz_value = qvReg->ReadRegistry( psz_ext, "", ""  );

    if( !EMPTY_STR(psz_value) && strcmp( qtu(s_path), psz_value ) )
        qvReg->WriteRegistry( psz_ext, "VLC.backup", psz_value );
    free( psz_value );

    /* Put a "link" to VLC.EXT as default */
    qvReg->WriteRegistry( psz_ext, "", qtu( s_path ) );

    /* Create the needed Key if they weren't done in the installer */
    if( !qvReg->RegistryKeyExists( qtu( s_path ) ) )
    {
        qvReg->WriteRegistry( psz_ext, "", qtu( s_path ) );
        qvReg->WriteRegistry( qtu( s_path ), "", "Media file" );
        qvReg->WriteRegistry( qtu( s_path.append( "\\shell" ) ), "", "Play" );

        /* Get the installer path */
        QVLCRegistry qvReg2( HKEY_LOCAL_MACHINE );
        QString str_temp = qvReg2.ReadRegistry( "Software\\VideoLAN\\VLC", "", "" );

        if( str_temp.size() )
        {
            qvReg->WriteRegistry( qtu( s_path.append( "\\Play\\command" ) ),
                "", qtu( str_temp.append(" --started-from-file \"%1\"" ) ) );

            qvReg->WriteRegistry( qtu( s_path2.append( "\\DefaultIcon" ) ),
                        "", qtu( str_temp.append(",0") ) );
        }
    }
}

void delAsso( QVLCRegistry *qvReg, const char *psz_ext )
{
    QString s_path( "VLC"); s_path += psz_ext;
    char *psz_value = qvReg->ReadRegistry( psz_ext, "", "" );

    if( psz_value && !strcmp( qtu(s_path), psz_value ) )
    {
        free( psz_value );
        psz_value = qvReg->ReadRegistry( psz_ext, "VLC.backup", "" );
        if( psz_value )
            qvReg->WriteRegistry( psz_ext, "", psz_value );

        qvReg->DeleteValue( psz_ext, "VLC.backup" );
    }
    free( psz_value );
}

void SPrefsPanel::saveAsso()
{
    QVLCRegistry qvReg( HKEY_CLASSES_ROOT );
    for( int i = 0; i < listAsso.size(); i ++ )
    {
        if( listAsso[i]->checkState( 0 ) > 0 )
        {
            addAsso( &qvReg, qtu( listAsso[i]->text( 0 ) ) );
        }
        else
        {
            delAsso( &qvReg, qtu( listAsso[i]->text( 0 ) ) );
        }
    }
    /* Gruik ? Naaah */
    qobject_cast<QDialog *>(listAsso[0]->treeWidget()->parent())->accept();
}

#endif /* _WIN32 */

void SPrefsPanel::MLaddNewEntryPoint( ){
    QUrl newEntryPoints = QFileDialog::getExistingDirectoryUrl( this , qtr("Please choose an entry point folder") ,
                                             QUrl( QDir::homePath( ) ) );

    if(! newEntryPoints.isEmpty() )
        mlModel->add( newEntryPoints );
}

QWidget *SPrefsPanel::MLgenerateWidget( QModelIndex index , MlFoldersModel *mlf , QWidget *parent){
    if ( index.column() == 0 ){

        QWidget *wid = new QWidget( parent );

        QBoxLayout* layout = new QBoxLayout( QBoxLayout::LeftToRight , wid );

        QCheckBox*cb = new QCheckBox( wid );
        cb->setFixedSize( 16 , 16 );

        //cb->setChecked(mlf->data(index, MlFoldersModel::CustomCheckBoxRole).toBool()); //TODO: disable banning till un-banning works
        cb->setEnabled( false );

        layout->addWidget( cb , Qt::AlignCenter );
        wid->setLayout( layout );

        connect( cb , &QPushButton::clicked, [=]( ) {
            mlf->setData( index , cb->isChecked() , MlFoldersModel::Banned);
        } );
        return wid;
    }
    else if ( index.column( ) == 2 ){
        QWidget *wid = new QWidget( parent );

        QBoxLayout* layout = new QBoxLayout( QBoxLayout::LeftToRight , wid );

        QPushButton *pb = new QPushButton( "-" , wid );
        pb->setFixedSize( 16 , 16 );

        layout->addWidget( pb , Qt::AlignCenter );
        wid->setLayout( layout );


        connect( pb , &QPushButton::clicked , [=]() {
            mlf->removeAt(index.row());
        } );

        return wid;
    }

    return nullptr;
}

void SPrefsPanel::MLdrawControls( ) {
  for ( int col = 0 ; col < mlModel->columnCount( ) ; col++ )
    for (int row = 0 ; row < mlModel->rowCount() ; row++ )
      {
    QModelIndex index = mlModel->index ( row , col );
    mlTableView->setIndexWidget ( index, MLgenerateWidget ( index, mlModel,
                               mlTableView ) );
      }

  mlTableView->resizeColumnsToContents( );
  mlTableView->horizontalHeader()->setMinimumSectionSize( 100 );
  mlTableView->horizontalHeader()->setSectionResizeMode( 1 , QHeaderView::Stretch );

  mlTableView->horizontalHeader()->setFixedHeight( 24 );
}
