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
#include "dialogs/dialogs_provider.hpp"
#include "maininterface/mainctx.hpp"
#include "util/color_scheme_model.hpp"
#include "util/proxycolumnmodel.hpp"
#include "medialibrary/mlrecentmediamodel.hpp"

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

#include <QStyleFactory>
#include <QScreen>
#include <QDir>

#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QWidget>
#include <QHBoxLayout>
#include <QDialog>
#include <QFileDialog>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QRegularExpression>

#include <cassert>
#include <math.h>
#ifdef _WIN32
#include <wrl/client.h>
#endif

#define ICON_HEIGHT 48
#define ICON_WIDTH 48

#ifdef _WIN32
# include <vlc_charset.h>
# include <shobjidl.h>
#endif
#include <vlc_modules.h>

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

    /* Note: For hysterical raisins, this is sorted by decreasing priority
     * order (then alphabetical order). */
    if (!strcmp(aout, "pulse"))
        return -1;

#ifdef __linux__
    if (!strcmp(aout, "alsa") && module_exists("alsa"))
        return cbrtf(config_GetFloat("alsa-gain")) * 100.f + .5f;
#endif
#ifdef _WIN32
    if (!strcmp(aout, "mmdevice"))
        return config_GetFloat("mmdevice-volume") * 100.f + .5f;
#endif
#ifdef __APPLE__
    if (!strcmp(aout, "auhal") && module_exists("auhal"))
        return (config_GetFloat("auhal-volume") * 100.f + .5f)
                 / AOUT_VOLUME_DEFAULT;
#endif

    if (!strcmp(aout, "jack"))
        return cbrtf(config_GetFloat("jack-gain")) * 100.f + 0.5f;

#ifdef __OS2__
    if (!strcmp(aout, "kai"))
        return cbrtf(config_GetFloat("kai-gain")) * 100.f + .5f;
#endif
#ifdef _WIN32
    if (!strcmp(aout, "waveout"))
        return config_GetFloat("waveout-volume") * 100.f + .5f;
#endif

    return -1;
}

static const QString styleSettingsKey = QStringLiteral("MainWindow/QtStyle");

class PropertyResetter
{
public:
    PropertyResetter(QWidget *control, const char * property)
        : m_control {control}
        , m_property {property}
        , m_initialValue {m_control->property(property)}
    {
    }

    void reset()
    {
        bool success = m_control->setProperty(m_property.data(), m_initialValue);
        vlc_assert(success);
    }

private:
    QWidget *m_control;
    const QByteArray m_property;
    const QVariant m_initialValue;
};

/*********************************************************************
 * The List of categories
 *********************************************************************/
SPrefsCatList::SPrefsCatList( qt_intf_t *_p_intf, QWidget *_parent ) :
                                  QWidget( _parent ), p_intf( _p_intf )
{
    QHBoxLayout *layout = new QHBoxLayout();

    /* Use autoExclusive buttons and a mapper as QButtonGroup can't
       set focus (keys) when it manages the buttons's exclusivity.
       See QT bugs 131 & 816 and QAbstractButton's source code. */
    QSignalMapper *mapper = new QSignalMapper( layout );
    connect( mapper, &QSignalMapper::mappedInt, this, &SPrefsCatList::switchPanel );
    qreal dpr = devicePixelRatioF();

    auto addCategory = [&]( QString label, QString ltooltip, QString icon, int numb) {
        QToolButton * button = new QToolButton( this );
        /* Scale icon to non native size outside of toolbutton to avoid widget size */
        /* computation using native size */
        QPixmap scaled = QPixmap( icon )
              .scaledToHeight( ICON_HEIGHT * dpr, Qt::SmoothTransformation );
        scaled.setDevicePixelRatio( dpr );
        button->setIcon( scaled );
        button->setText( label );
        button->setToolTip( ltooltip );
        button->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
        button->setIconSize( QSize( ICON_WIDTH, ICON_HEIGHT ) );
        button->setMinimumWidth( 40 + ICON_WIDTH );
        button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
        button->setAutoRaise( true );
        button->setCheckable( true );
        button->setAutoExclusive( true );
        connect( button, &QToolButton::clicked, mapper, QOverload<>::of(&QSignalMapper::map) );
        mapper->setMapping( button, numb );
        layout->addWidget( button );
    };

    addCategory( qfut(INTF_TITLE), qfut(INTF_TOOLTIP), ":/prefsmenu/spref_interface.png" , SPrefsInterface );
    addCategory( qfut(AUDIO_TITLE), qfut(AUDIO_TOOLTIP), ":/prefsmenu/spref_audio.png", SPrefsAudio );
    addCategory( qfut(VIDEO_TITLE), qfut(VIDEO_TOOLTIP), ":/prefsmenu/spref_video.png", SPrefsVideo );
    addCategory( qfut(SUBPIC_TITLE), qfut(SUBPIC_TOOLTIP), ":/prefsmenu/spref_subtitles.png", SPrefsSubtitles );
    addCategory( qfut(INPUT_TITLE), qfut(INPUT_TOOLTIP), ":/prefsmenu/spref_input.png", SPrefsInputAndCodecs );
    addCategory( qfut(HOTKEYS_TITLE), qfut(HOTKEYS_TOOLTIP), ":/prefsmenu/spref_hotkeys.png", SPrefsHotkeys );
    if ( vlc_ml_instance_get( p_intf ) != nullptr )
        addCategory( qfut(ML_TITLE), qfut(ML_TOOLTIP), ":/prefsmenu/spref_medialibrary.png", SPrefsMediaLibrary );

    qobject_cast<QToolButton*>(mapper->mapping(SPrefsInterface))->setChecked(true);
    layout->setContentsMargins(0, 0, 0, 0);
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

template<typename ControlType, typename WidgetType>
void SPrefsPanel::configGeneric(const char* option, QLabel* label, WidgetType* control)
{
    module_config_t* p_config =  config_FindConfig( option );
    if( p_config )
    {
        auto configcontrol =  new ControlType(p_config, label, control );
        controls.append( configcontrol );
    }
    else
    {
        control->setEnabled( false );
        if( label )
            label->setEnabled( false );
    }
}

template<typename ControlType, typename WidgetType>
void SPrefsPanel::configGenericNoUi(const char* option, QLabel* label, WidgetType* control)
{
    module_config_t* p_config =  config_FindConfig( option );
    if( p_config )
    {
        auto configcontrol =  new ControlType(p_config, label, control );
        controls.append( configcontrol );
    }
    else
    {
        control->setVisible( false );
        if( label )
            label->setEnabled( false );
    }
}

template<typename ControlType>
void SPrefsPanel::configGenericFile(const char* option, QLabel* label, QLineEdit* control, QPushButton* button)
{
    module_config_t* p_config =  config_FindConfig( option );
    if( p_config )
    {
        auto configcontrol =  new ControlType(p_config, label, control, button );
        controls.append( configcontrol );
    }
    else
    {
        control->setEnabled( false );
        if( label )
            label->setEnabled( false );
        if( button )
            button->setEnabled( false );
    }
}

void SPrefsPanel::configBool(const char* option, QAbstractButton* control)
{
    module_config_t* p_config =  config_FindConfig( option );
    if( p_config )
    {
        auto configcontrol =  new BoolConfigControl(p_config, nullptr, control );
        controls.append( configcontrol );
    }
    else
    {
        control->setEnabled( false );
    }
}

SPrefsPanel::SPrefsPanel( qt_intf_t *_p_intf, QWidget *_parent,
                          int _number ) : QWidget( _parent ), p_intf( _p_intf )
{
    module_config_t *p_config;
    ConfigControl *control;
    number = _number;
    lang = NULL;
    radioGroup = NULL;

    QVBoxLayout *panel_layout = new QVBoxLayout();
    QWidget *panel = new QWidget();
    panel_layout->setContentsMargins(3, 3, 3, 3);

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
        case SPrefsVideo:
        {
            auto& ui = m_videoUI;
            ui.setupUi( panel );
            panel_label->setText( qtr("Video Settings") );

            configBool( "video", ui.enableVideo );
            ui.videoZone->setEnabled( ui.enableVideo->isChecked() );
            connect( ui.enableVideo, &QCheckBox::toggled,
                     ui.videoZone, &QWidget::setEnabled );

            configBool( "fullscreen", ui.fullscreen );
            configBool( "video-deco", ui.windowDecorations );
            configGeneric<StringListConfigControl>("vout", ui.voutLabel, ui.outputModule);

            ui.fullscreenScreenBox->addItem( qtr("Automatic"), -1 );
            int i_screenCount = 0;
            foreach( QScreen* screen, QGuiApplication::screens() )
            {
                ui.fullscreenScreenBox->addItem( screen->name(), i_screenCount );
                i_screenCount++;
            }
            p_config = config_FindConfig( "qt-fullscreen-screennumber" );
            if( p_config )
            {
                int i_defaultScreen = p_config->value.i + 1;
                if ( i_defaultScreen < 0 || i_defaultScreen > ( ui.fullscreenScreenBox->count() - 1 ) )
                    ui.fullscreenScreenBox->setCurrentIndex( 0 );
                else
                    ui.fullscreenScreenBox->setCurrentIndex(p_config->value.i + 1);
            }

#ifdef _WIN32
            configBool( "directx-hw-yuv", ui.hwYUVBox );
#else
            ui.directXBox->setVisible( false );
#endif

#ifdef __OS2__
            configBool( "kva-fixt23", ui.kvaFixT23 );
            configGeneric<StringListConfigControl>( "kva-video-mode", ui.kvaVideoModeLabel,
                            ui.kvaVideoMode );
#else
            ui.kvaBox->setVisible( false );
#endif

            configGeneric<IntegerListConfigControl>( "deinterlace",  ui.deinterLabel, ui.deinterlaceBox );
            configGeneric<StringListConfigControl>( "deinterlace-mode", ui.deinterModeLabel, ui.deinterlaceModeBox );
            configGeneric<StringConfigControl>( "aspect-ratio", ui.arLabel, ui.arLine );

            configGenericFile<DirectoryConfigControl>( "snapshot-path",  ui.dirLabel,
                                 ui.snapshotsDirectory, ui.snapshotsDirectoryBrowse );
            configGeneric<StringConfigControl>( "snapshot-prefix", ui.prefixLabel, ui.snapshotsPrefix );
            configBool( "snapshot-sequential", ui.snapshotsSequentialNumbering );
            configGeneric<StringListConfigControl>( "snapshot-format", ui.arLabel, ui.snapshotsFormat );

            break;
        }

        /******************************
         * AUDIO Panel Implementation *
         ******************************/
        case SPrefsAudio:
        {
            auto& ui = m_audioUI;
            ui.setupUi( panel );
            panel_label->setText( qtr("Audio Settings") );

            configBool( "audio", ui.enableAudio );
            ui.audioZone->setEnabled( ui.enableAudio->isChecked() );
            connect( ui.enableAudio, &QCheckBox::toggled,
                     ui.audioZone, &QWidget::setEnabled );

            /* Build if necessary */
            QGridLayout * outputAudioLayout = qobject_cast<QGridLayout *>(ui.outputAudioBox->layout());

            auto audioControl = [this, outputAudioLayout](QString key, const char* property) {
                QLabel* label = new QLabel( qtr( "Device:" ) );
                label->setMinimumSize(QSize(250, 0));
                outputAudioLayout->addWidget( label, outputAudioLayout->rowCount(), 0, 1, 1 );

                QComboBox* device = new QComboBox;
                label->setBuddy( device );
                device->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred  );
                outputAudioLayout->addWidget( device, outputAudioLayout->rowCount() - 1, 1, 1, -1 );

                audioControlGroups[key] = AudioControlGroup(label, device);
                configGenericNoUi<StringListConfigControl>(property, label, device);
            };

//audioControlFile is only used for oss
#if !defined(_WIN32) && !defined( __OS2__ )
            auto audioControlFile = [this, outputAudioLayout](QString key, const char* property) {
                QLabel* label = new QLabel( qtr( "Device:" ) );
                label->setMinimumSize(QSize(250, 0));
                outputAudioLayout->addWidget( label, outputAudioLayout->rowCount(), 0, 1, 1 );

                QHBoxLayout* hboxLayout = new QHBoxLayout;
                QLineEdit* device = new QLineEdit;
                label->setBuddy( device ); \
                hboxLayout->addWidget( device ); \
                QPushButton * browse = new QPushButton( qtr( "Browse..." ) );
                hboxLayout->addWidget( browse );
                outputAudioLayout->addLayout( hboxLayout, outputAudioLayout->rowCount() - 1, 1, 1, 1, Qt::AlignLeft );

                audioControlGroups[key] = AudioControlGroup(label, device, browse);
                configGenericFile<FileConfigControl>(property, label, device, browse);
            };
#endif

#ifdef _WIN32
            audioControl("directx", "directx-audio-device" );
            audioControl("waveout", "waveout-audio-device" );

#elif defined( __OS2__ )
            audioControl("kai", "kai-audio-device" );
#else
            if( module_exists( "alsa" ) )
                audioControl("alsa", "alsa-audio-device" );

            if( module_exists( "oss" ) )
                audioControlFile("oss", "oss-audio-device" );
#endif

#ifdef _WIN32
            audioControl("mmdevice", "mmdevice-audio-device" );

            configGeneric<IntegerListConfigControl>( "mmdevice-passthrough",
                                                     ui.mmdevicePassthroughLabel, ui.mmdevicePassthroughBox );
#else
            ui.mmdevicePassthroughLabel->setVisible( false );
            ui.mmdevicePassthroughBox->setVisible( false );
#endif

            int i_max_volume = config_GetInt( "qt-max-volume" );

            /* Audio Options */
            ui.volumeValue->setMaximum( i_max_volume );
            ui.defaultVolume->setMaximum( i_max_volume );

            connect( ui.defaultVolume, &QSlider::valueChanged,
                     this, &SPrefsPanel::updateAudioVolume );

            ui.defaultVolume_zone->setEnabled( ui.resetVolumeCheckbox->isChecked() );
            connect( ui.resetVolumeCheckbox, &QCheckBox::toggled,
                     ui.defaultVolume_zone, &QWidget::setEnabled );

            configGeneric<StringConfigControl>( "audio-language" , ui.langLabel, ui.preferredAudioLanguage );

            configBool( "spdif", ui.spdifBox );

            if( !module_exists( "normvol" ) )
                ui.volNormBox->setEnabled( false );
            else
            {
                configGeneric<FloatConfigControl>( "norm-max-level" , nullptr, ui.volNormSpin );
            }
            configGeneric<StringListConfigControl>( "audio-replay-gain-mode", ui.replayLabel,
                            ui.replayCombo );
            configGeneric<StringListConfigControl>( "audio-visual" , ui.visuLabel,
                            ui.visualisation);
            configBool( "audio-time-stretch", ui.autoscaleBox );

            /* Audio Output Specifics */
            configGeneric<StringListConfigControl>( "aout", ui.outputLabel, ui.outputModule );

            connect( ui.outputModule, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this, &SPrefsPanel::updateAudioOptions );

            /* File output exists on all platforms */
            configGenericFile<FileConfigControl>( "audiofile-file",  ui.fileLabel,
                                 ui.fileName, ui.fileBrowseButton );

            updateAudioOptions( ui.outputModule->currentIndex() );

            /* LastFM */
            if( module_exists( "audioscrobbler" ) )
            {
                configGeneric<StringConfigControl>( "lastfm-username", ui.lastfm_user_label,
                        ui.lastfm_user_edit );
                configGeneric<StringConfigControl>( "lastfm-password", ui.lastfm_pass_label,
                        ui.lastfm_pass_edit );

                if( config_ExistIntf( "audioscrobbler" ) )
                    ui.lastfm->setChecked( true );
                else
                    ui.lastfm->setChecked( false );

                ui.lastfm_zone->setVisible( ui.lastfm->isChecked() );

                connect( ui.lastfm, &QCheckBox::toggled, ui.lastfm_zone, &QWidget::setVisible );
                connect( ui.lastfm, &QtCheckboxChanged, this, &SPrefsPanel::lastfm_Changed );
            }
            else
            {
                ui.lastfm->hide();
                ui.lastfm_zone->hide();
            }

            /* Normalizer */
            connect( ui.volNormBox, &QCheckBox::toggled, ui.volNormSpin, &QDoubleSpinBox::setEnabled );

            char* psz = config_GetPsz( "audio-filter" );
            qs_filter = qfu( psz ).split( ':', Qt::SkipEmptyParts );

            free( psz );

            bool b_enabled = ( qs_filter.contains( "normvol" ) );
            ui.volNormBox->setChecked( b_enabled );
            ui.volNormSpin->setEnabled( b_enabled && ui.volNormBox->isEnabled() );

            /* Volume Label */
            updateAudioVolume( ui.defaultVolume->value() ); // First time init

            break;
        }

        /*****************************************
         * INPUT AND CODECS Panel Implementation *
         *****************************************/
        case SPrefsInputAndCodecs:
        {
            auto& ui = m_inputCodecUI;
            ui.setupUi( panel );
            panel_label->setText( qtr("Input & Codecs Settings") );

            /* Disk Devices */
            {
                ui.DVDDeviceComboBox->setToolTip(
                    qtr( "If this property is blank, different values\n"
                         "for DVD, VCD, and CDDA are set.\n"
                         "You can define a unique one or configure them \n"
                         "individually in the advanced preferences." ) );
                bool have_cdda = module_exists( "cdda" );
                char *dvd_discpath = config_GetPsz( "dvd" );
                char *vcd_discpath = config_GetPsz( "vcd" );
                char *cdda_discpath = have_cdda ? config_GetPsz( "cd-audio" ) : nullptr;
                if( dvd_discpath && vcd_discpath && ( !have_cdda || cdda_discpath ) )
                {
                    if( !strcmp( dvd_discpath, vcd_discpath ) &&
                        ( !have_cdda || !strcmp( cdda_discpath, dvd_discpath ) ) )
                    {
                        ui.DVDDeviceComboBox->setEditText( qfu( dvd_discpath ) );
                    }
                }
                free( cdda_discpath );
                free( dvd_discpath );
                free( vcd_discpath );
            }
#if !defined( _WIN32 ) && !defined( __OS2__)
            QStringList DVDDeviceComboBoxStringList = QStringList();
            DVDDeviceComboBoxStringList
                    << "dvd*" << "scd*" << "sr*" << "sg*" << "cd*";
            ui.DVDDeviceComboBox->addItems( QDir( "/dev/" )
                    .entryList( DVDDeviceComboBoxStringList, QDir::System )
                    .replaceInStrings( QRegularExpression(QStringLiteral("^")), "/dev/" )
            );
#endif
            configGeneric<StringConfigControl>( "dvd", ui.DVDLabel,
                            ui.DVDDeviceComboBox->lineEdit() );
            configGenericFile<DirectoryConfigControl>( "input-record-path",  ui.recordLabel,
                                 ui.recordPath, ui.recordBrowse );

            configGeneric<StringConfigControl>( "http-proxy", ui.httpProxyLabel, ui.proxy );
            configGeneric<IntegerConfigControl>( "postproc-q", ui.ppLabel, ui.PostProcLevel );
            configGeneric<IntegerListConfigControl>( "avi-index", ui.aviLabel, ui.AviRepair );

            /* live555 module prefs */
            configBool( "rtsp-tcp", ui.live555TransportRTSP_TCPRadio );
            if ( !module_exists( "live555" ) )
            {
                ui.live555TransportRTSP_TCPRadio->hide();
                ui.live555TransportHTTPRadio->hide();
                ui.live555TransportLabel->hide();
            }
            configGeneric<StringListConfigControl>( "dec-dev", ui.hwAccelLabel, ui.hwAccelModule );
            configBool( "input-fast-seek", ui.fastSeekBox );
            configGeneric<IntegerListConfigControl>( "avcodec-skiploopfilter", ui.filterLabel, ui.loopFilterBox );
            configGeneric<StringListConfigControl>( "sout-x264-tune", ui.x264Label, ui.tuneBox );
            configGeneric<StringListConfigControl>( "sout-x264-preset", ui.x264Label, ui.presetBox );
            configGeneric<StringListConfigControl>( "sout-x264-profile", ui.x264profileLabel, ui.profileBox );
            configGeneric<StringConfigControl>( "sout-x264-level", ui.x264profileLabel, ui.levelBox );
            configBool( "mkv-preload-local-dir", ui.mkvPreloadBox );

            /* Caching */
            /* Add the things to the ComboBox */
            ui.cachingCombo->addItem( qtr("Custom"), QVariant( CachingCustom ) );
            ui.cachingCombo->addItem( qtr("Lowest latency"), QVariant( CachingLowest ) );
            ui.cachingCombo->addItem( qtr("Low latency"), QVariant( CachingLow ) );
            ui.cachingCombo->addItem( qtr("Normal"), QVariant( CachingNormal ) );
            ui.cachingCombo->addItem( qtr("High latency"), QVariant( CachingHigh ) );
            ui.cachingCombo->addItem( qtr("Higher latency"), QVariant( CachingHigher ) );

#define TestCaC( name, factor ) \
    b_cache_equal =  b_cache_equal && \
     ( i_cache * factor == config_GetInt( name ) );
            /* Select the accurate value of the ComboBox */
            bool b_cache_equal = true;
            int i_cache = config_GetInt( "file-caching" );

            TestCaC( "network-caching", 10/3 );
            TestCaC( "disc-caching", 1);
            TestCaC( "live-caching", 1 );
            if( b_cache_equal )
                ui.cachingCombo->setCurrentIndex(
                    ui.cachingCombo->findData( QVariant( i_cache ) ) );
#undef TestCaC

            break;
        }

        /**********************************
         * INTERFACE Panel Implementation *
         **********************************/
        case SPrefsInterface:
        {
            auto& ui = m_interfaceUI;
            ui.setupUi( panel );
            panel_label->setText( qtr("Interface Settings") );

#ifndef _WIN32
            ui.langBox->hide();
#else
            for( size_t i = 0; i < ARRAY_SIZE( language_map ); i++)
                ui.langCombo->addItem( qfu( language_map[i].name ), language_map[i].iso );
            connect( ui.langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SPrefsPanel::langChanged );

            HKEY h_key;
            char *langReg = NULL;
            if( RegOpenKeyEx( HKEY_CURRENT_USER, TEXT("Software\\VideoLAN\\VLC\\"), 0, KEY_READ, &h_key )
                    == ERROR_SUCCESS )
            {
                WCHAR szData[256];
                DWORD len = 256;
                if( RegQueryValueEx( h_key, TEXT("Lang"), NULL, NULL, (LPBYTE) &szData, &len ) == ERROR_SUCCESS ) {
                    langReg = FromWide( szData );
                    ui.langCombo->setCurrentIndex( ui.langCombo->findData(qfu(langReg)) );
                }
            }
            free( langReg);
#endif

//            ui.defaultLabel->setFont( italicFont );
            ui.skinsLabel->setText(
                    qtr( "This is VLC's skinnable interface. You can download other skins at" )
                    + QString( " <a href=\"https://www.videolan.org/vlc/skins.php\">" )
                    + qtr( "VLC skins website" ) + QString( "</a>." ) );
            ui.skinsLabel->setFont( italicFont );

#ifdef _WIN32
            BUTTONACT( ui.assoButton, &SPrefsPanel::assoDialog );
#else
            ui.osGroupBox->hide();
#endif

            if( !module_exists( "skins2" ) )
            {
                ui.skins->hide();
                ui.skinImage->hide();
            }
            else
            {
                /* interface */
                char *psz_intf = config_GetPsz( "intf" );
                if( psz_intf )
                {
                    if( strstr( psz_intf, "skin" ) )
                        ui.skins->setChecked( true );
                }
                free( psz_intf );
            }

#if !defined( _WIN32)
            {
                // Populate styles combobox:
                assert(qApp->property("initialStyle").isValid());
                const QString& initialStyle = qApp->property("initialStyle").toString();
                ui.stylesCombo->addItem( qtr( "System's default (%1)" ).arg( initialStyle ), initialStyle );
                const QStringList& styles = QStyleFactory::keys();
                for ( const auto& i : styles )
                {
                    ui.stylesCombo->addItem( i, i );
                }
                const auto style = getSettings()->value( styleSettingsKey );

                if ( style.isValid() && style.canConvert<QString>() )
                    ui.stylesCombo->setCurrentText( style.toString() );
                ui.stylesCombo->insertSeparator( 1 );
                if ( ui.stylesCombo->currentIndex() < 0 )
                    ui.stylesCombo->setCurrentIndex( 0 ); /* default */
            }

            m_resetters.push_back( std::make_unique<PropertyResetter>( ui.stylesCombo, "currentIndex" ) );

            connect( ui.stylesCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this, &SPrefsPanel::changeStyle );
#else
            ui.stylesCombo->hide();
            ui.stylesLabel->hide();
#endif
            radioGroup = new QButtonGroup(this);
            radioGroup->addButton( ui.modernButton, 0 );
            radioGroup->addButton( ui.classicButton, 1 );
            radioGroup->addButton( ui.skins, 2 );

            configBool( "qt-minimal-view", ui.minimalviewBox );

            /*Update layout radio buttons based on the checkState of the following checkboxes*/
            connect(ui.menuBarCheck, &QtCheckboxChanged, this, &SPrefsPanel::updateLayoutSelection);
            connect(ui.pinVideoControlsCheckbox, &QtCheckboxChanged, this, &SPrefsPanel::updateLayoutSelection);
            connect(ui.titleBarCheckBox, &QtCheckboxChanged, this, &SPrefsPanel::updateLayoutSelection);

            /*Clicking on image will check the corresponding layout radio button*/
            layoutImages = new QButtonGroup( this );
            layoutImages->addButton( ui.modernImage, 0 );
            layoutImages->addButton( ui.classicImage, 1 );
            layoutImages->addButton( ui.skinImage, 2 );

            connect( layoutImages, qOverload<QAbstractButton*>( &QButtonGroup::buttonClicked ), this, &SPrefsPanel::imageLayoutClick );

            /* Set checkboxes depending on the layout selected*/
            connect(radioGroup, &QButtonGroup::idClicked, this, &SPrefsPanel::handleLayoutChange);
            connect(layoutImages, &QButtonGroup::idClicked, this, &SPrefsPanel::handleLayoutChange);

            configBool( "embedded-video", ui.embedVideo );
            configBool( "qt-video-autoresize", ui.resizingBox );
            connect( ui.embedVideo, &QCheckBox::toggled, ui.resizingBox, &QCheckBox::setEnabled );
            ui.resizingBox->setEnabled( ui.embedVideo->isChecked() );

            configBool( "qt-fs-controller", ui.fsController );
            configBool( "qt-system-tray", ui.systrayBox );
            configGeneric<IntegerListConfigControl>( "qt-notification", ui.notificationComboLabel,
                                                      ui.notificationCombo );
            connect( ui.systrayBox, &QCheckBox::toggled, [=]( bool checked ) {
                ui.notificationCombo->setEnabled( checked );
                ui.notificationComboLabel->setEnabled( checked );
            } );
            ui.notificationCombo->setEnabled( ui.systrayBox->isChecked() );

            configBool( "qt-pause-minimized", ui.pauseMinimizedBox );
            configBool( "playlist-tree", ui.treePlaylist );
            configBool( "play-and-pause", ui.playPauseBox );
            configGenericFile<FileConfigControl>( "skins2-last",  ui.skinFileLabel,
                                 ui.fileSkin, ui.skinBrowse );

            configBool( "metadata-network-access", ui.MetadataNetworkAccessMode );
            configBool( "qt-menubar", ui.menuBarCheck );


            configBool( "qt-pin-controls", ui.pinVideoControlsCheckbox );
            m_resetters.push_back(std::make_unique<PropertyResetter>(ui.pinVideoControlsCheckbox, "checked"));
            QObject::connect( ui.pinVideoControlsCheckbox, &QtCheckboxChanged, p_intf->p_mi, &MainCtx::setPinVideoControls );

            ui.colorSchemeComboBox->setModel( p_intf->p_mi->getColorScheme() );
            ui.colorSchemeComboBox->setCurrentText( p_intf->p_mi->getColorScheme()->currentText() );
            m_resetters.push_back(std::make_unique<PropertyResetter>( ui.colorSchemeComboBox, "currentIndex" ));
            QObject::connect( ui.colorSchemeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged)
                              , p_intf->p_mi->getColorScheme(), &ColorSchemeModel::setCurrentIndex );

            const double intfScaleFloatFactor = 100;
            const auto updateIntfUserScaleFactorFromControls =
                    [this, slider = ui.intfScaleFactorSlider, spinBox = ui.intfScaleFactorSpinBox, intfScaleFloatFactor](const int value)
            {
                if (slider->value() != value)
                {
                    QSignalBlocker s( slider );
                    slider->setValue( value );
                }
                if (spinBox->value() != value)
                {
                    QSignalBlocker s( spinBox );
                    spinBox->setValue( value );
                }
                p_intf->p_mi->setIntfUserScaleFactor( value / intfScaleFloatFactor );
            };

            ui.intfScaleFactorSlider->setRange( p_intf->p_mi->getMinIntfUserScaleFactor() * intfScaleFloatFactor
                                                 , p_intf->p_mi->getMaxIntfUserScaleFactor() * intfScaleFloatFactor);
            ui.intfScaleFactorSpinBox->setRange( p_intf->p_mi->getMinIntfUserScaleFactor() * intfScaleFloatFactor
                                                 , p_intf->p_mi->getMaxIntfUserScaleFactor() * intfScaleFloatFactor);

            updateIntfUserScaleFactorFromControls( p_intf->p_mi->getIntfUserScaleFactor() * intfScaleFloatFactor );
            m_resetters.push_back( std::make_unique<PropertyResetter>( ui.intfScaleFactorSlider, "value" ) );

            QObject::connect( ui.intfScaleFactorSlider, QOverload<int>::of(&QSlider::valueChanged)
                              , p_intf->p_mi , updateIntfUserScaleFactorFromControls );
            QObject::connect( ui.intfScaleFactorSpinBox, QOverload<int>::of(&QSpinBox::valueChanged)
                              , p_intf->p_mi , updateIntfUserScaleFactorFromControls );

            DialogsProvider *provider = DialogsProvider::getInstance();

            QObject::connect( ui.toolbarEditor, &QAbstractButton::clicked, provider, &DialogsProvider::showToolbarEditorDialog);

            configBool( "qt-titlebar", ui.titleBarCheckBox );

            /* UPDATE options */
#ifdef UPDATE_CHECK
            configBool( "qt-updates-notif", ui.updatesBox );
            configGeneric<IntegerConfigControl>( "qt-updates-days", nullptr, ui.updatesDays );
            ui.updatesDays->setEnabled( ui.updatesBox->isChecked() );
            connect( ui.updatesBox, &QCheckBox::toggled,
                     ui.updatesDays, &QSpinBox::setEnabled );
#else
            ui.updatesBox->hide();
            ui.updatesDays->hide();
#endif
            /* ONE INSTANCE options */
#if !defined( _WIN32 ) && !defined(__APPLE__) && !defined(__OS2__)
            if( !module_exists( "dbus" ) )
            {
                ui.OneInterfaceMode->hide();
                ui.EnqueueOneInterfaceMode->hide();
                ui.oneInstanceFromFile->hide();
            }
            else
#endif
            {
                configBool( "one-instance", ui.OneInterfaceMode );
                configBool( "playlist-enqueue", ui.EnqueueOneInterfaceMode );
                ui.EnqueueOneInterfaceMode->setEnabled( ui.OneInterfaceMode->isChecked() );
                connect( ui.OneInterfaceMode, &QCheckBox::toggled,
                         ui.EnqueueOneInterfaceMode, &QCheckBox::setEnabled );
                configBool( "one-instance-when-started-from-file", ui.oneInstanceFromFile );
            }

            configGeneric<IntegerListConfigControl>( "qt-auto-raise", ui.autoRaiseLabel, ui.autoRaiseComboBox );

            /* RECENTLY PLAYED options */

            const auto hasMedialibrary = p_intf->p_mi->hasMediaLibrary();

            ui.continuePlaybackLabel->setVisible( hasMedialibrary );
            ui.continuePlaybackComboBox->setVisible( hasMedialibrary );
            ui.saveRecentlyPlayed->setVisible( hasMedialibrary );
            ui.clearRecent->setVisible( hasMedialibrary );
            ui.clearRecentSpacer->changeSize( 0, 0 );

            if ( hasMedialibrary )
            {
                configGeneric<IntegerListConfigControl>( "restore-playback-pos", ui.continuePlaybackLabel, ui.continuePlaybackComboBox );
                configBool( "save-recentplay", ui.saveRecentlyPlayed );

                ui.clearRecentSpacer->changeSize( 1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum );
                MLRecentMediaModel *recentsModel = new MLRecentMediaModel( ui.clearRecent );
                recentsModel->setMl( p_intf->p_mi->getMediaLibrary() );
                connect( ui.clearRecent, &QPushButton::clicked, recentsModel, &MLRecentMediaModel::clearHistory );
            }

            break;
        }

        /**********************************
         * SUBTITLES Panel Implementation *
         **********************************/
        case SPrefsSubtitles:
        {
            auto& ui = m_subtitlesUI;
            ui.setupUi( panel );
            panel_label->setText( qtr("Subtitle & On Screen Display Settings") );

            configBool( "osd", ui.OSDBox);
            configBool( "video-title-show", ui.OSDTitleBox);
            configGeneric<IntegerListConfigControl>( "video-title-position",
                            ui.OSDTitlePosLabel, ui.OSDTitlePos );

            configBool( "spu", ui.spuActiveBox);
            ui.spuZone->setEnabled( ui.spuActiveBox->isChecked() );
            connect( ui.spuActiveBox, &QCheckBox::toggled, ui.spuZone, &QWidget::setEnabled );

            configGeneric<StringListConfigControl>( "subsdec-encoding", ui.encodLabel,
                            ui.encoding );
            configGeneric<StringConfigControl>( "sub-language", ui.subLangLabel,
                            ui.preferredLanguage );

            configGeneric<IntegerListConfigControl>( "freetype-rel-fontsize",
                            ui.fontSizeLabel, ui.fontSize );

            configGeneric<FontConfigControl>( "freetype-font", ui.fontLabel, ui.font );
            configGeneric<ColorConfigControl>( "freetype-color", ui.fontColorLabel, ui.fontColor );
            configGeneric<IntegerListConfigControl>( "freetype-outline-thickness",
                            ui.fontEffectLabel, ui.effect );
            configGeneric<ColorConfigControl>( "freetype-outline-color", ui.outlineColorLabel,
                            ui.outlineColor );

            configGeneric<IntegerConfigControl>( "sub-margin", ui.subsPosLabel, ui.subsPosition );

            if( module_exists( "freetype" ) )
            {
                ui.shadowCheck->setChecked( config_GetInt( "freetype-shadow-opacity" ) > 0 );
                ui.backgroundCheck->setChecked( config_GetInt( "freetype-background-opacity" ) > 0 );
            }
            else
            {
                ui.shadowCheck->setEnabled( false );
                ui.backgroundCheck->setEnabled( false );
            }

            configGeneric<IntegerListConfigControl>( "secondary-sub-alignment",
                            ui.secondarySubsAlignmentLabel, ui.secondarySubsAlignment );
            configGeneric<IntegerConfigControl>( "secondary-sub-margin", ui.secondarySubsPosLabel, ui.secondarySubsPosition );
            break;
        }

        /********************************
         * HOTKEYS Panel Implementation *
         ********************************/
        case SPrefsHotkeys:
        {
            QGridLayout *gLayout = new QGridLayout;
            panel->setLayout( gLayout );
            int line = 0;

            panel_label->setText( qtr( "Configure Hotkeys" ) );
            control = new KeySelectorControl( this );
            control->insertInto( gLayout, line );
            controls.append( control );

            line++;

            p_config = config_FindConfig( "hotkeys-y-wheel-mode" );
            control = new IntegerListConfigControl( p_config, this );
            control->insertInto( gLayout, line );
            controls.append( control );

            line++;

            p_config = config_FindConfig( "hotkeys-x-wheel-mode" );
            control = new IntegerListConfigControl( p_config, this );
            control->insertInto( gLayout, line );
            controls.append( control );

#ifdef _WIN32
            line++;

            p_config = config_FindConfig( "qt-disable-volume-keys" );
            control = new BoolConfigControl( p_config, this );
            control->insertInto( gLayout, line );
            controls.append( control );
#endif
            break;
        }

        /**************************************
         * MEDIA LIBRARY Panel Implementation *
         **************************************/
        case SPrefsMediaLibrary:
        {
            auto& ui = m_medialibUI;
            ui.setupUi( panel );
            panel_label->setText( qtr("Media Library Settings") );

            if ( vlc_ml_instance_get( p_intf ) != NULL )
            {
                auto foldersModel = new MLFoldersModel( this );
                foldersModel->setCtx( p_intf->p_mi );
                ui.entryPoints->setMLFoldersModel( foldersModel );
                mlFoldersEditor = ui.entryPoints;

                auto bannedFoldersModel = new MLBannedFoldersModel( this );
                bannedFoldersModel->setCtx( p_intf->p_mi );
                ui.bannedEntryPoints->setMLFoldersModel( bannedFoldersModel );
                mlBannedFoldersEditor = ui.bannedEntryPoints;

                BUTTONACT( ui.addButton, &SPrefsPanel::MLaddNewFolder );
                BUTTONACT( ui.banButton, &SPrefsPanel::MLBanFolder );

                connect( ui.reloadButton, &QPushButton::clicked
                        , p_intf->p_mi->getMediaLibrary(), &MediaLib::reload);
            }
            else
            {
                ui.mlGroupBox->hide( );
            }

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
}


void SPrefsPanel::updateAudioOptions( int number )
{

    auto setAudioDeviceVisible = [this](QString key, bool visible) {
        const AudioControlGroup& ctrl = audioControlGroups[key];
        if (ctrl.widget)
            ctrl.widget->setVisible( visible );
        if (ctrl.label)
            ctrl.label->setVisible( visible );
        if (ctrl.button)
            ctrl.button->setVisible( visible );
    };

    QString value = m_audioUI.outputModule->itemData( number ).toString();
#ifdef _WIN32
    /* Since MMDevice is most likely to be used by default, we show MMDevice
     * options by default */
    const bool mmDeviceEnabled = value == "mmdevice" || value == "any";
    m_audioUI.mmdevicePassthroughLabel->setVisible( mmDeviceEnabled );
    m_audioUI.mmdevicePassthroughBox->setVisible( mmDeviceEnabled );
    setAudioDeviceVisible("mmdevice", mmDeviceEnabled);

    setAudioDeviceVisible("waveout", value == "waveout");
#elif defined( __OS2__ )
    setAudioDeviceVisible("kai", value == "kai");
#else
    setAudioDeviceVisible("oss", value == "oss");
    setAudioDeviceVisible("alsa", value == "alsa");

#endif
    m_audioUI.fileControl->setVisible( ( value == "afile" ) );
    m_audioUI.spdifBox->setVisible( ( value == "alsa" || value == "oss" || value == "auhal" ||
                                             value == "waveout" ) );

    int volume = getDefaultAudioVolume(qtu(value));
    bool save = true;

    if (volume >= 0)
        save = config_GetInt("volume-save");

    m_audioUI.resetVolumeCheckbox->setChecked(!save);
    m_audioUI.resetVolumeCheckbox->setEnabled(volume >= 0);

    m_audioUI.defaultVolume->setValue((volume >= 0) ? volume : 100);
    m_audioUI.defaultVolume->setEnabled(volume >= 0);
}


SPrefsPanel::~SPrefsPanel()
{
    if (!m_isApplied)
        clean();

    qDeleteAll( controls ); controls.clear();
    free( lang );
}

/* Checks the layout radio button corresponding the image clicked */
void SPrefsPanel::imageLayoutClick( QAbstractButton* btn )
{
    QAbstractButton* layoutBtn = radioGroup->buttons().at( layoutImages->id( btn ) );
    assert( layoutBtn );
    layoutBtn->setChecked( true );
}

/* Change configurations depending on the layout selected and set check states of radioGroup */
void SPrefsPanel::handleLayoutChange( int id )
{
    auto ui = m_interfaceUI;
    if (id == 0) {
        // Modern layout selected
        ui.styleStackedWidget->setCurrentIndex(0);
        ui.menuBarCheck->setChecked(false);
        ui.titleBarCheckBox->setChecked(false);
        ui.pinVideoControlsCheckbox->setChecked(false);
    }
    else if (id == 1) {
        // Classic layout selected
        ui.styleStackedWidget->setCurrentIndex(0);
        ui.menuBarCheck->setChecked(true);
        ui.titleBarCheckBox->setChecked(true);
        ui.pinVideoControlsCheckbox->setChecked(true);
    }
    else if (id == 2) {
        ui.styleStackedWidget->setCurrentIndex(1);
    }
}

void SPrefsPanel::updateLayoutSelection()
{
    auto ui = m_interfaceUI;
    bool isModern = !ui.menuBarCheck->isChecked()
                    && !ui.titleBarCheckBox->isChecked()
                    && !ui.pinVideoControlsCheckbox->isChecked();

    ui.modernButton->setChecked(isModern);

    bool isClassic = ui.menuBarCheck->isChecked()
                     && ui.titleBarCheckBox->isChecked()
                     && ui.pinVideoControlsCheckbox->isChecked();

    ui.classicButton->setChecked(isClassic);

    if (!isModern && !isClassic) {
        radioGroup->setExclusive(false);
        ui.modernButton->setChecked(false);
        ui.classicButton->setChecked(false);
        radioGroup->setExclusive(true);
    }
}

void SPrefsPanel::updateAudioVolume( int volume )
{
    m_audioUI.volumeValue->setValue( volume );
}


/* Function called from the main Preferences dialog on each SPrefs Panel */
void SPrefsPanel::apply()
{
    m_isApplied = true;

    /* Generic save for ever panel */
    QList<ConfigControl *>::const_iterator i;
    for( i = controls.cbegin() ; i != controls.cend() ; ++i )
    {
        ConfigControl *c = qobject_cast<ConfigControl *>(*i);
        c->doApply();
    }

    switch( number )
    {
    case SPrefsInputAndCodecs:
    {
        /* Device default selection */
        QByteArray devicepath = m_inputCodecUI.DVDDeviceComboBox->currentText().toUtf8();
        if( devicepath.size() > 0 )
        {
            config_PutPsz( "dvd", devicepath.constData() );
            config_PutPsz( "vcd", devicepath.constData() );
            if( module_exists( "cdda" ) )
                config_PutPsz( "cd-audio", devicepath.constData() );
        }

#define CaC( name, factor ) config_PutInt( name, i_comboValue * factor )
        /* Caching */
        QComboBox *cachingCombo = m_inputCodecUI.cachingCombo;
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
        if( m_interfaceUI.skins->isChecked() )
            config_PutPsz( "intf", "skins2,any" );
        else
        //if( m_interfaceUI.qt->isChecked() )
            config_PutPsz( "intf", "" );
#if !defined( _WIN32)
        if ( m_interfaceUI.stylesCombo->currentIndex() > 0 )
            getSettings()->setValue( styleSettingsKey, m_interfaceUI.stylesCombo->currentData().toString() );
        else
            getSettings()->remove( styleSettingsKey );
#endif

#ifdef _WIN32
    saveLang();
#endif
        break;
    }

    case SPrefsVideo:
    {
        int i_fullscreenScreen =   m_videoUI.fullscreenScreenBox->currentData().toInt();
        config_PutInt( "qt-fullscreen-screennumber", i_fullscreenScreen );
        break;
    }

    case SPrefsAudio:
    {
        bool b_checked = m_audioUI.volNormBox->isChecked();
        if( b_checked && !qs_filter.contains( "normvol" ) )
            qs_filter.append( "normvol" );
        if( !b_checked && qs_filter.contains( "normvol" ) )
            qs_filter.removeAll( "normvol" );

        config_PutPsz( "audio-filter", qtu( qs_filter.join( ":" ) ) );

        /* Default volume */
        int i_volume = m_audioUI.defaultVolume->value();
        bool b_reset_volume = m_audioUI.resetVolumeCheckbox->isChecked();
        char *psz_aout = config_GetPsz( "aout" );

        float f_gain = powf( i_volume / 100.f, 3 );

#define save_vol_aout( name ) \
            module_exists( name ) && ( !psz_aout || !strcmp( psz_aout, name ) || !strcmp( psz_aout, "any" ) )

        //FIXME this is moot
#if defined( _WIN32 )
        VLC_UNUSED( f_gain );
        if( save_vol_aout( "mmdevice" ) )
            config_PutFloat( "mmdevice-volume", i_volume / 100.f );
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
        bool b_checked = m_subtitlesUI.shadowCheck->isChecked();
        if( b_checked && config_GetInt( "freetype-shadow-opacity" ) == 0 ) {
            config_PutInt( "freetype-shadow-opacity", 128 );
        }
        else if (!b_checked ) {
            config_PutInt( "freetype-shadow-opacity", 0 );
        }

        b_checked = m_subtitlesUI.backgroundCheck->isChecked();
        if( b_checked && config_GetInt( "freetype-background-opacity" ) == 0 ) {
            config_PutInt( "freetype-background-opacity", 128 );
        }
        else if (!b_checked ) {
            config_PutInt( "freetype-background-opacity", 0 );
        }
        break;
    }

    case SPrefsMediaLibrary:
    {
        mlFoldersEditor->commit();
        mlBannedFoldersEditor->commit();
    }
    }
}

void SPrefsPanel::clean()
{
    for ( auto &resetter : m_resetters )
        resetter->reset();
}

void SPrefsPanel::lastfm_Changed( int i_state )
{
    if( i_state == Qt::Checked )
        config_AddIntf( "audioscrobbler" );
    else if( i_state == Qt::Unchecked )
        config_RemoveIntf( "audioscrobbler" );
}

void SPrefsPanel::changeStyle()
{
    const QString style = m_interfaceUI.stylesCombo->currentData().toString();

    QMetaObject::invokeMethod( qApp, [style]() {
        // Queue this call in order to prevent
        // updating the preferences dialog when
        // it is rejected:
        QApplication::setStyle( style );
    }, Qt::QueuedConnection );
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

void SPrefsPanel::assoDialog()
{
    HRESULT hr;

    hr = CoInitializeEx( NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );
    if( SUCCEEDED(hr) )
    {
        {
            Microsoft::WRL::ComPtr<IApplicationAssociationRegistrationUI> p_regui;

            hr = CoCreateInstance(__uuidof(ApplicationAssociationRegistrationUI),
                                NULL, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&p_regui));
            if( SUCCEEDED(hr) )
                p_regui->LaunchAdvancedAssociationUI(L"VLC" );
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
    aTv( ".3g2" ); aTv( ".3gp" ); aTv( ".3gp2" ); aTv( ".3gpp" ); aTv( ".amrec" );
    aTv( ".amv" ); aTv( ".asf" ); aTv( ".avi" ); aTv( ".bik" ); aTv( ".divx" );
    aTv( ".drc" ); aTv( ".dv" ); aTv( ".f4v" ); aTv( ".flv" ); aTv( ".gvi" );
    aTv( ".gxf" ); aTv( ".m1v" ); aTv( ".m2t" ); aTv( ".m2v" ); aTv( ".m2ts" );
    aTv( ".m4v" ); aTv( ".mkv" ); aTv( ".mov" ); aTv( ".mp2v" ); aTv( ".mp4" );
    aTv( ".mp4v" ); aTv( ".mpa" ); aTv( ".mpe" ); aTv( ".mpeg" ); aTv( ".mpeg1" );
    aTv( ".mpeg2" ); aTv( ".mpeg4" ); aTv( ".mpg" ); aTv( ".mpv2" ); aTv( ".mts" );
    aTv( ".mtv" ); aTv( ".mxf" ); aTv( ".nsv" ); aTv( ".nuv" ); aTv( ".ogg" );
    aTv( ".ogm" ); aTv( ".ogx" ); aTv( ".ogv" ); aTv( ".rec" ); aTv( ".rm" );
    aTv( ".rmvb" ); aTv( ".rpl" ); aTv( ".thp" ); aTv( ".tod" ); aTv( ".ts" );
    aTv( ".tts" ); aTv( ".vob" ); aTv( ".vro" ); aTv( ".webm" ); aTv( ".wmv" );
    aTv( ".xesc" );
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

    connect( filetypeList, &QTreeWidget::itemChanged, this, &SPrefsPanel::updateCheckBoxes );

    QDialogButtonBox *buttonBox = new QDialogButtonBox( d );
    QPushButton *closeButton = new QPushButton( qtr( "&Apply" ) );
    QPushButton *clearButton = new QPushButton( qtr( "&Cancel" ) );
    buttonBox->addButton( closeButton, QDialogButtonBox::AcceptRole );
    buttonBox->addButton( clearButton, QDialogButtonBox::ActionRole );

    assoLayout->addWidget( buttonBox, 1, 2, 1, 2 );

    connect( closeButton, &QPushButton::clicked, [=]() {
        this->saveAsso();
        d->reject();
    } );
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

void SPrefsPanel::MLaddNewFolder() {
    const QUrl homeDirectory = QUrl::fromLocalFile(QDir::homePath());
    QUrl newEntryPoint = QFileDialog::getExistingDirectoryUrl( this
                                                              , qtr("Please choose an entry point folder")
                                                              , homeDirectory );

    if(! newEntryPoint.isEmpty() )
        mlFoldersEditor->add( newEntryPoint );
}

void SPrefsPanel::MLBanFolder( ) {
    const QUrl homeDirectory = QUrl::fromLocalFile(QDir::homePath());
    QUrl newEntryPoint = QFileDialog::getExistingDirectoryUrl( this
                                                              , qtr("Please choose an entry point folder")
                                                              , homeDirectory );

    if(! newEntryPoint.isEmpty() )
        mlBannedFoldersEditor->add( newEntryPoint );
}
