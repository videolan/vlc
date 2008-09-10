/*****************************************************************************
 * open.cpp : Panels for the open dialogs
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 * Copyright (C) 2007 Société des arts technologiques
 * Copyright (C) 2007 Savoir-faire Linux
 *
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

#include "qt4.hpp"
#include "components/open_panels.hpp"
#include "dialogs/open.hpp"
#include "dialogs_provider.hpp"

#include <QFileDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QStackedLayout>
#include <QListView>
#include <QCompleter>
#include <QDirModel>
#include <QScrollArea>
#include <QUrl>

#define I_DEVICE_TOOLTIP N_("Select the device or the VIDEO_TS directory")

/**************************************************************************
 * Open Files and subtitles                                               *
 **************************************************************************/
FileOpenPanel::FileOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    /* Classic UI Setup */
    ui.setupUi( this );

    /** BEGIN QFileDialog tweaking **/
    /* Use a QFileDialog and customize it because we don't want to
       rewrite it all. Be careful to your eyes cause there are a few hacks.
       Be very careful and test correctly when you modify this. */

    /* Set Filters for file selection */
    QString fileTypes = "";
    ADD_FILTER_MEDIA( fileTypes );
    ADD_FILTER_VIDEO( fileTypes );
    ADD_FILTER_AUDIO( fileTypes );
    ADD_FILTER_PLAYLIST( fileTypes );
    ADD_FILTER_ALL( fileTypes );
    fileTypes.replace( QString(";*"), QString(" *"));

    // Make this QFileDialog a child of tempWidget from the ui.
    dialogBox = new FileOpenBox( ui.tempWidget, NULL,
            qfu( p_intf->p_sys->psz_filepath ), fileTypes );

    dialogBox->setFileMode( QFileDialog::ExistingFiles );
    dialogBox->setAcceptMode( QFileDialog::AcceptOpen );
#if HAS_QT43
    dialogBox->restoreState(
            getSettings()->value( "file-dialog-state" ).toByteArray() );
#endif

    /* We don't want to see a grip in the middle of the window, do we? */
    dialogBox->setSizeGripEnabled( false );

    /* Add a tooltip */
    dialogBox->setToolTip( qtr( "Select one or multiple files" ) );
    dialogBox->setMinimumHeight( 250 );

    // But hide the two OK/Cancel buttons. Enable them for debug.
    QDialogButtonBox *fileDialogAcceptBox =
                      dialogBox->findChildren<QDialogButtonBox*>()[0];
    fileDialogAcceptBox->hide();

    /* Ugly hacks to get the good Widget */
    //This lineEdit is the normal line in the fileDialog.
#if HAS_QT43
    lineFileEdit = dialogBox->findChildren<QLineEdit*>()[0];
#else
    lineFileEdit = dialogBox->findChildren<QLineEdit*>()[1];
#endif
    /* Make a list of QLabel inside the QFileDialog to access the good ones */
    QList<QLabel *> listLabel = dialogBox->findChildren<QLabel*>();

    /* Hide the FileNames one. Enable it for debug */
    listLabel[1]->setText( qtr( "File names:" ) );
    /* Change the text that was uncool in the usual box */
    listLabel[2]->setText( qtr( "Filter:" ) );

    dialogBox->layout()->setMargin( 0 );
    dialogBox->layout()->setSizeConstraint( QLayout::SetNoConstraint );

    /** END of QFileDialog tweaking **/

    // Add the DialogBox to the layout
    ui.gridLayout->addWidget( dialogBox, 0, 0, 1, 3 );

    //TODO later: fill the fileCompleteList with previous items played.
    QCompleter *fileCompleter = new QCompleter( fileCompleteList, this );
    fileCompleter->setModel( new QDirModel( fileCompleter ) );
    lineFileEdit->setCompleter( fileCompleter );

    // Hide the subtitles control by default.
    ui.subFrame->hide();

    /* Build the subs size combo box */
    setfillVLCConfigCombo( "freetype-rel-fontsize" , p_intf,
                            ui.sizeSubComboBox );

    /* Build the subs align combo box */
    setfillVLCConfigCombo( "subsdec-align", p_intf, ui.alignSubComboBox );

    /* Connects  */
    BUTTONACT( ui.subBrowseButton, browseFileSub() );
    BUTTONACT( ui.subCheckBox, toggleSubtitleFrame());

    CONNECT( lineFileEdit, textChanged( QString ), this, updateMRL() );
    CONNECT( ui.subInput, textChanged( QString ), this, updateMRL() );
    CONNECT( ui.alignSubComboBox, currentIndexChanged( int ), this, updateMRL() );
    CONNECT( ui.sizeSubComboBox, currentIndexChanged( int ), this, updateMRL() );
}

FileOpenPanel::~FileOpenPanel()
{
#if HAS_QT43
    getSettings()->setValue( "file-dialog-state", dialogBox->saveState() );
#endif
}

/* Show a fileBrowser to select a subtitle */
void FileOpenPanel::browseFileSub()
{
    // TODO Handle selection of more than one subtitles file
    QStringList files = THEDP->showSimpleOpen( qtr("Open subtitles file"),
                            EXT_FILTER_SUBTITLE,
                            dialogBox->directory().absolutePath() );
    if( files.isEmpty() ) return;
    ui.subInput->setText( files.join(" ") );
    updateMRL();
}

/* Update the current MRL */
void FileOpenPanel::updateMRL()
{
    QString mrl = "";
    foreach( QString file, dialogBox->selectedFiles() ) {
         mrl += "\"" + file + "\" ";
    }

    if( ui.subCheckBox->isChecked() ) {
        mrl.append( " :sub-file=\"" + ui.subInput->text() + "\"" );
        int align = ui.alignSubComboBox->itemData(
                    ui.alignSubComboBox->currentIndex() ).toInt();
        mrl.append( " :subsdec-align=" + QString().setNum( align ) );
        int size = ui.sizeSubComboBox->itemData(
                   ui.sizeSubComboBox->currentIndex() ).toInt();
        mrl.append( " :freetype-rel-fontsize=" + QString().setNum( size ) );
    }

    emit mrlUpdated( mrl );
    emit methodChanged( "file-caching" );
}

/* Function called by Open Dialog when clicke on Play/Enqueue */
void FileOpenPanel::accept()
{
    //TODO set the completer
    p_intf->p_sys->psz_filepath = qtu( dialogBox->directory().absolutePath() );
}

void FileOpenBox::accept()
{
    OpenDialog::getInstance( NULL, NULL, true )->selectSlots();
}

void FileOpenBox::reject()
{
    OpenDialog::getInstance( NULL, NULL, true )->cancel();
}

/* Function called by Open Dialog when clicked on cancel */
void FileOpenPanel::clear()
{
    lineFileEdit->clear();
    ui.subInput->clear();
}

void FileOpenPanel::toggleSubtitleFrame()
{
    TOGGLEV( ui.subFrame );

    /* Update the MRL */
    updateMRL();
}

/**************************************************************************
 * Open Discs ( DVD, CD, VCD and similar devices )                        *
 **************************************************************************/
DiscOpenPanel::DiscOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    ui.setupUi( this );

    /* Get the default configuration path for the devices */
    psz_dvddiscpath = config_GetPsz( p_intf, "dvd" );
    psz_vcddiscpath = config_GetPsz( p_intf, "vcd" );
    psz_cddadiscpath = config_GetPsz( p_intf, "cd-audio" );

    /* State to avoid overwritting the users changes with the configuration */
    b_firstdvd = true;
    b_firstvcd = true;
    b_firstcdda = true;

    ui.browseDiscButton->setToolTip( qtr( I_DEVICE_TOOLTIP ));
    ui.deviceCombo->setToolTip( I_DEVICE_TOOLTIP );

#if WIN32 /* Disc drives probing for Windows */
    char szDrives[512];
    szDrives[0] = '\0';
    if( GetLogicalDriveStringsA( sizeof( szDrives ) - 1, szDrives ) )
    {
        char *drive = szDrives;
        UINT oldMode = SetErrorMode( SEM_FAILCRITICALERRORS );
        while( *drive )
        {
            if( GetDriveTypeA(drive) == DRIVE_CDROM )
                ui.deviceCombo->addItem( drive );

            /* go to next drive */
            while( *(drive++) );
        }
        SetErrorMode(oldMode);
    }
#else /* Use a Completer under Linux */
    QCompleter *discCompleter = new QCompleter( this );
    discCompleter->setModel( new QDirModel( discCompleter ) );
    ui.deviceCombo->setCompleter( discCompleter );
#endif

    /* CONNECTs */
    BUTTONACT( ui.dvdRadioButton, updateButtons() );
    BUTTONACT( ui.vcdRadioButton, updateButtons() );
    BUTTONACT( ui.audioCDRadioButton, updateButtons() );
    BUTTONACT( ui.dvdsimple, updateButtons() );
    BUTTONACT( ui.browseDiscButton, browseDevice() );
    BUTTON_SET_ACT_I( ui.ejectButton, "", eject, qtr( "Eject the disc" ),
            eject() );

    CONNECT( ui.deviceCombo, editTextChanged( QString ), this, updateMRL());
    CONNECT( ui.titleSpin, valueChanged( int ), this, updateMRL());
    CONNECT( ui.chapterSpin, valueChanged( int ), this, updateMRL());
    CONNECT( ui.audioSpin, valueChanged( int ), this, updateMRL());
    CONNECT( ui.subtitlesSpin, valueChanged( int ), this, updateMRL());

    /* Run once the updateButtons function in order to fill correctly the comboBoxes */
    updateButtons();
}

DiscOpenPanel::~DiscOpenPanel()
{
    free( psz_dvddiscpath );
    free( psz_vcddiscpath );
    free( psz_cddadiscpath );
}

void DiscOpenPanel::clear()
{
    ui.titleSpin->setValue( 0 );
    ui.chapterSpin->setValue( 0 );
    b_firstcdda = true;
    b_firstdvd = true;
    b_firstvcd = true;
}

#ifdef WIN32
    #define setDrive( psz_name ) {\
    int index = ui.deviceCombo->findText( qfu( psz_name ) ); \
    if( index != -1 ) ui.deviceCombo->setCurrentIndex( index );}
#else
    #define setDrive( psz_name ) {\
    ui.deviceCombo->setEditText( qfu( psz_name ) ); }
#endif

/* update the buttons according the type of device */
void DiscOpenPanel::updateButtons()
{
    if ( ui.dvdRadioButton->isChecked() )
    {
        if( b_firstdvd )
        {
            setDrive( psz_dvddiscpath );
            b_firstdvd = false;
        }
        ui.titleLabel->setText( qtr("Title") );
        ui.chapterLabel->show();
        ui.chapterSpin->show();
        ui.diskOptionBox_2->show();
        ui.dvdsimple->setEnabled( true );
    }
    else if ( ui.vcdRadioButton->isChecked() )
    {
        if( b_firstvcd )
        {
            setDrive( psz_vcddiscpath );
            b_firstvcd = false;
        }
        ui.titleLabel->setText( qtr("Entry") );
        ui.chapterLabel->hide();
        ui.chapterSpin->hide();
        ui.diskOptionBox_2->show();
        ui.dvdsimple->setEnabled( false );
    }
    else /* CDDA */
    {
        if( b_firstcdda )
        {
            setDrive( psz_cddadiscpath );
            b_firstcdda = false;
        }
        ui.titleLabel->setText( qtr("Track") );
        ui.chapterLabel->hide();
        ui.chapterSpin->hide();
        ui.diskOptionBox_2->hide();
        ui.dvdsimple->setEnabled( false );
    }

    updateMRL();
}

/* Update the current MRL */
void DiscOpenPanel::updateMRL()
{
    QString mrl = "";

    /* CDDAX and VCDX not implemented. TODO ? */
    /* DVD */
    if( ui.dvdRadioButton->isChecked() ) {
        if( !ui.dvdsimple->isChecked() )
            mrl = "\"dvd://";
        else
            mrl = "\"dvdsimple://";
        mrl += ui.deviceCombo->currentText();
        emit methodChanged( "dvdnav-caching" );

        if ( ui.titleSpin->value() > 0 ) {
            mrl += QString("@%1").arg( ui.titleSpin->value() );
            if ( ui.chapterSpin->value() > 0 ) {
                mrl+= QString(":%1").arg( ui.chapterSpin->value() );
            }
        }

    /* VCD */
    } else if ( ui.vcdRadioButton->isChecked() ) {
        mrl = "\"vcd://" + ui.deviceCombo->currentText();
        emit methodChanged( "vcd-caching" );

        if( ui.titleSpin->value() > 0 ) {
            mrl += QString("@E%1").arg( ui.titleSpin->value() );
        }

    /* CDDA */
    } else {
        mrl = "\"cdda://" + ui.deviceCombo->currentText();
        if( ui.titleSpin->value() > 0 ) {
            QString("@%1").arg( ui.titleSpin->value() );
        }
    }

    mrl += "\"";

    if ( ui.dvdRadioButton->isChecked() || ui.vcdRadioButton->isChecked() )
    {
        if ( ui.audioSpin->value() >= 0 ) {
            mrl += " :audio-track=" +
                QString("%1").arg( ui.audioSpin->value() );
        }
        if ( ui.subtitlesSpin->value() >= 0 ) {
            mrl += " :sub-track=" +
                QString("%1").arg( ui.subtitlesSpin->value() );
        }
    }
    emit mrlUpdated( mrl );
}

void DiscOpenPanel::browseDevice()
{
    QString dir = QFileDialog::getExistingDirectory( 0,
            qtr( I_DEVICE_TOOLTIP ) );
    if (!dir.isEmpty()) {
        ui.deviceCombo->setEditText( dir );
    }
    updateMRL();
}

void DiscOpenPanel::eject()
{
    intf_Eject( p_intf, qtu( ui.deviceCombo->currentText() ) );
}

void DiscOpenPanel::accept()
{}

/**************************************************************************
 * Open Network streams and URL pages                                     *
 **************************************************************************/
NetOpenPanel::NetOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    ui.setupUi( this );

    /* CONNECTs */
    CONNECT( ui.protocolCombo, activated( int ),
             this, updateProtocol( int ) );
    CONNECT( ui.portSpin, valueChanged( int ), this, updateMRL() );
    CONNECT( ui.addressText, textChanged( QString ), this, updateMRL());
    CONNECT( ui.timeShift, clicked(), this, updateMRL());

    ui.protocolCombo->addItem( "" );
    ui.protocolCombo->addItem("HTTP", QVariant("http"));
    ui.protocolCombo->addItem("HTTPS", QVariant("https"));
    ui.protocolCombo->addItem("MMS", QVariant("mms"));
    ui.protocolCombo->addItem("FTP", QVariant("ftp"));
    ui.protocolCombo->addItem("RTSP", QVariant("rtsp"));
    ui.protocolCombo->addItem("RTP", QVariant("rtp"));
    ui.protocolCombo->addItem("UDP", QVariant("udp"));
    ui.protocolCombo->addItem("RTMP", QVariant("rtmp"));

    updateProtocol( ui.protocolCombo->currentIndex() );
}

NetOpenPanel::~NetOpenPanel()
{}

void NetOpenPanel::clear()
{}

/* update the widgets according the type of protocol */
void NetOpenPanel::updateProtocol( int idx_proto ) {
    QString addr = ui.addressText->text();
    QString proto = ui.protocolCombo->itemData( idx_proto ).toString();

    ui.timeShift->setEnabled( idx_proto == UDP_PROTO );
    ui.portSpin->setEnabled( idx_proto == UDP_PROTO ||
                             idx_proto == RTP_PROTO );

    if( idx_proto == NO_PROTO ) return;

    /* If we already have a protocol in the address, replace it */
    if( addr.contains( "://"))
    {
        if( idx_proto != UDP_PROTO && idx_proto != RTP_PROTO )
            addr.replace( QRegExp("^.*://@*"), proto + "://");
        else
             addr.replace( QRegExp("^.*://"), proto + "://@");
        ui.addressText->setText( addr );
    }
    updateMRL();
}

void NetOpenPanel::updateMRL() {
    QString mrl = "";
    QString addr = ui.addressText->text();
    addr = QUrl::toPercentEncoding( addr, ":/?#@!$&'()*+,;=" );
    int idx_proto = ui.protocolCombo->currentIndex();

    if( addr.contains( "://"))
    {
        /* Match the correct item in the comboBox */
        ui.protocolCombo->setCurrentIndex(
                ui.protocolCombo->findData( addr.section( ':', 0, 0 ) ) );

        if( idx_proto != UDP_PROTO || idx_proto != RTP_PROTO )
            mrl = addr;
    }
    else
    {
        switch( idx_proto ) {
        case HTTP_PROTO:
            mrl = "http://" + addr;
            emit methodChanged("http-caching");
            break;
        case HTTPS_PROTO:
            mrl = "https://" + addr;
            emit methodChanged("http-caching");
            break;
        case MMS_PROTO:
            mrl = "mms://" + addr;
            emit methodChanged("mms-caching");
            break;
        case FTP_PROTO:
            mrl = "ftp://" + addr;
            emit methodChanged("ftp-caching");
            break;
        case RTSP_PROTO:
            mrl = "rtsp://" + addr;
            emit methodChanged("rtsp-caching");
            break;
        case UDP_PROTO:
            mrl = "udp://@";
            /* Add [] to IPv6 */
            if ( addr.contains(':') && !addr.contains('[') )
            {
                mrl += "[" + addr + "]";
            }
            else mrl += addr;
            mrl += QString(":%1").arg( ui.portSpin->value() );
            emit methodChanged("udp-caching");
            break;
        case RTP_PROTO:
            mrl = "rtp://@";
            if ( addr.contains(':') && !addr.contains('[') )
                mrl += "[" + addr + "]"; /* Add [] to IPv6 */
            else
                mrl += addr;
            mrl += QString(":%1").arg( ui.portSpin->value() );
            emit methodChanged("rtp-caching");
            break;
        case RTMP_PROTO:
            mrl = "rtmp://" + addr;
            emit methodChanged("rtmp-caching");
            break;
        }
    }

    // Encode the boring stuffs

    if( ui.timeShift->isEnabled() && ui.timeShift->isChecked() ) {
        mrl += " :access-filter=timeshift";
    }
    emit mrlUpdated( mrl );
}

/**************************************************************************
 * Open Capture device ( DVB, PVR, V4L, and similar )                     *
 **************************************************************************/
CaptureOpenPanel::CaptureOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    isInitialized = false;
}

void CaptureOpenPanel::initialize()
{
    if( isInitialized ) return;

    msg_Dbg( p_intf, "Initialization of Capture device panel" );
    isInitialized = true;

    ui.setupUi( this );

    BUTTONACT( ui.advancedButton, advancedDialog() );

    /* Create two stacked layouts in the main comboBoxes */
    QStackedLayout *stackedDevLayout = new QStackedLayout;
    ui.cardBox->setLayout( stackedDevLayout );

    QStackedLayout *stackedPropLayout = new QStackedLayout;
    ui.optionsBox->setLayout( stackedPropLayout );

    /* Creation and connections of the WIdgets in the stacked layout */
#define addModuleAndLayouts( number, name, label )                    \
    QWidget * name ## DevPage = new QWidget( this );                  \
    QWidget * name ## PropPage = new QWidget( this );                 \
    stackedDevLayout->addWidget( name ## DevPage );        \
    stackedPropLayout->addWidget( name ## PropPage );      \
    QGridLayout * name ## DevLayout = new QGridLayout;                \
    QGridLayout * name ## PropLayout = new QGridLayout;               \
    name ## DevPage->setLayout( name ## DevLayout );                  \
    name ## PropPage->setLayout( name ## PropLayout );                \
    ui.deviceCombo->addItem( qtr( label ), QVariant( number ) );

#define CuMRL( widget, slot ) CONNECT( widget , slot , this, updateMRL() );

#ifdef WIN32
    /*********************
     * DirectShow Stuffs *
     *********************/
    if( module_Exists( p_intf, "dshow" ) ){
    addModuleAndLayouts( DSHOW_DEVICE, dshow, "DirectShow" );

    /* dshow Main */
    int line = 0;
    module_config_t *p_config =
        config_FindConfig( VLC_OBJECT(p_intf), "dshow-vdev" );
    vdevDshowW = new StringListConfigControl(
        VLC_OBJECT(p_intf), p_config, this, false, dshowDevLayout, line );
    line++;

    p_config = config_FindConfig( VLC_OBJECT(p_intf), "dshow-adev" );
    adevDshowW = new StringListConfigControl(
        VLC_OBJECT(p_intf), p_config, this, false, dshowDevLayout, line );
    line++;

    /* dshow Properties */
    QLabel *dshowVSizeLabel = new QLabel( qtr( "Video size" ) );
    dshowPropLayout->addWidget( dshowVSizeLabel, 0, 0 );

    dshowVSizeLine = new QLineEdit;
    dshowPropLayout->addWidget( dshowVSizeLine, 0, 1);
    dshowPropLayout->addItem( new QSpacerItem( 20, 20, QSizePolicy::Expanding ),
            1, 0, 3, 1 );

    /* dshow CONNECTs */
    CuMRL( vdevDshowW->combo, currentIndexChanged ( int ) );
    CuMRL( adevDshowW->combo, currentIndexChanged ( int ) );
    CuMRL( dshowVSizeLine, textChanged( QString ) );
    }

    /**************
     * BDA Stuffs *
     **************/
    if( module_Exists( p_intf, "bda" ) ){
    addModuleAndLayouts( BDA_DEVICE, bda, "DVB DirectShow" );

    /* bda Main */
    QLabel *bdaTypeLabel = new QLabel( qtr( "DVB Type:" ) );

    bdas = new QRadioButton( "DVB-S" );
    bdas->setChecked( true );
    bdac = new QRadioButton( "DVB-C" );
    bdat = new QRadioButton( "DVB-T" );

    bdaDevLayout->addWidget( bdaTypeLabel, 0, 0 );
    bdaDevLayout->addWidget( bdas, 0, 1 );
    bdaDevLayout->addWidget( bdac, 0, 2 );
    bdaDevLayout->addWidget( bdat, 0, 3 );

    /* bda Props */
    QLabel *bdaFreqLabel =
                    new QLabel( qtr( "Transponder/multiplex frequency" ) );
    bdaPropLayout->addWidget( bdaFreqLabel, 0, 0 );

    bdaFreq = new QSpinBox;
    bdaFreq->setAlignment( Qt::AlignRight );
    bdaFreq->setSuffix(" kHz");
    bdaFreq->setSingleStep( 1000 );
    setSpinBoxFreq( bdaFreq )
    bdaPropLayout->addWidget( bdaFreq, 0, 1 );

    bdaSrateLabel = new QLabel( qtr( "Transponder symbol rate" ) );
    bdaPropLayout->addWidget( bdaSrateLabel, 1, 0 );

    bdaSrate = new QSpinBox;
    bdaSrate->setAlignment( Qt::AlignRight );
    bdaSrate->setSuffix(" kHz");
    setSpinBoxFreq( bdaSrate );
    bdaPropLayout->addWidget( bdaSrate, 1, 1 );

    bdaBandLabel = new QLabel( qtr( "Bandwidth" ) );
    bdaPropLayout->addWidget( bdaBandLabel, 2, 0 );

    bdaBandBox = new QComboBox;
    setfillVLCConfigCombo( "dvb-bandwidth", p_intf, bdaBandBox );
    bdaPropLayout->addWidget( bdaBandBox, 2, 1 );

    bdaBandLabel->hide();
    bdaBandBox->hide();
    bdaPropLayout->addItem( new QSpacerItem( 20, 20, QSizePolicy::Expanding ),
            2, 0, 2, 1 );

    /* bda CONNECTs */
    CuMRL( bdaFreq, valueChanged ( int ) );
    CuMRL( bdaSrate, valueChanged ( int ) );
    CuMRL( bdaBandBox,  currentIndexChanged ( int ) );
    BUTTONACT( bdas, updateButtons() );
    BUTTONACT( bdat, updateButtons() );
    BUTTONACT( bdac, updateButtons() );
    BUTTONACT( bdas, updateMRL() );
    BUTTONACT( bdat, updateMRL() );
    BUTTONACT( bdac, updateMRL() );
    }

#else /* WIN32 */
    /*******
     * V4L2*
     *******/
    if( module_Exists( p_intf, "v4l2" ) ){
    addModuleAndLayouts( V4L2_DEVICE, v4l2, "Video for Linux 2" );

    /* V4l Main panel */
    QLabel *v4l2VideoDeviceLabel = new QLabel( qtr( "Video device name" ) );
    v4l2DevLayout->addWidget( v4l2VideoDeviceLabel, 0, 0 );

    v4l2VideoDevice = new QLineEdit;
    v4l2DevLayout->addWidget( v4l2VideoDevice, 0, 1 );

    QLabel *v4l2AudioDeviceLabel = new QLabel( qtr( "Audio device name" ) );
    v4l2DevLayout->addWidget( v4l2AudioDeviceLabel, 1, 0 );

    v4l2AudioDevice = new QLineEdit;
    v4l2DevLayout->addWidget( v4l2AudioDevice, 1, 1 );

    /* v4l2 Props panel */
    QLabel *v4l2StdLabel = new QLabel( qtr( "Standard" ) );
    v4l2PropLayout->addWidget( v4l2StdLabel, 0 , 0 );

    v4l2StdBox = new QComboBox;
    setfillVLCConfigCombo( "v4l2-standard", p_intf, v4l2StdBox );
    v4l2PropLayout->addWidget( v4l2StdBox, 0 , 1 );
    v4l2PropLayout->addItem( new QSpacerItem( 20, 20, QSizePolicy::Expanding ),
            1, 0, 3, 1 );

    /* v4l2 CONNECTs */
    CuMRL( v4l2VideoDevice, textChanged( QString ) );
    CuMRL( v4l2AudioDevice, textChanged( QString ) );
    CuMRL( v4l2StdBox,  currentIndexChanged ( int ) );
    }

    /*******
     * V4L *
     *******/
    if( module_Exists( p_intf, "v4l" ) ){
    addModuleAndLayouts( V4L_DEVICE, v4l, "Video for Linux" );

    /* V4l Main panel */
    QLabel *v4lVideoDeviceLabel = new QLabel( qtr( "Video device name" ) );
    v4lDevLayout->addWidget( v4lVideoDeviceLabel, 0, 0 );

    v4lVideoDevice = new QLineEdit;
    v4lDevLayout->addWidget( v4lVideoDevice, 0, 1 );

    QLabel *v4lAudioDeviceLabel = new QLabel( qtr( "Audio device name" ) );
    v4lDevLayout->addWidget( v4lAudioDeviceLabel, 1, 0 );

    v4lAudioDevice = new QLineEdit;
    v4lDevLayout->addWidget( v4lAudioDevice, 1, 1 );

    /* V4l Props panel */
    QLabel *v4lNormLabel = new QLabel( qtr( "Norm" ) );
    v4lPropLayout->addWidget( v4lNormLabel, 0 , 0 );

    v4lNormBox = new QComboBox;
    setfillVLCConfigCombo( "v4l-norm", p_intf, v4lNormBox );
    v4lPropLayout->addWidget( v4lNormBox, 0 , 1 );

    QLabel *v4lFreqLabel = new QLabel( qtr( "Frequency" ) );
    v4lPropLayout->addWidget( v4lFreqLabel, 1 , 0 );

    v4lFreq = new QSpinBox;
    v4lFreq->setAlignment( Qt::AlignRight );
    v4lFreq->setSuffix(" kHz");
    setSpinBoxFreq( v4lFreq );
    v4lPropLayout->addWidget( v4lFreq, 1 , 1 );
    v4lPropLayout->addItem( new QSpacerItem( 20, 20, QSizePolicy::Expanding ),
            2, 0, 2, 1 );

    /* v4l CONNECTs */
    CuMRL( v4lVideoDevice, textChanged( QString ) );
    CuMRL( v4lAudioDevice, textChanged( QString ) );
    CuMRL( v4lFreq, valueChanged ( int ) );
    CuMRL( v4lNormBox,  currentIndexChanged ( int ) );
    }

    /*******
     * JACK *
     *******/
    if( module_Exists( p_intf, "jack" ) ){
    addModuleAndLayouts( JACK_DEVICE, jack, "JACK Audio Connection Kit" );

    /* Jack Main panel */
    /* Channels */
    QLabel *jackChannelsLabel = new QLabel( qtr( "Channels:" ) );
    jackDevLayout->addWidget( jackChannelsLabel, 1, 0 );

    jackChannels = new QSpinBox;
    setSpinBoxFreq( jackChannels );
    jackChannels->setMaximum(255);
    jackChannels->setValue(2);
    jackChannels->setAlignment( Qt::AlignRight );
    jackDevLayout->addWidget( jackChannels, 1, 1 );

    /* Jack Props panel */

    /* Selected ports */
    QLabel *jackPortsLabel = new QLabel( qtr( "Selected ports:" ) );
    jackPropLayout->addWidget( jackPortsLabel, 0 , 0 );

    jackPortsSelected = new QLineEdit( qtr( ".*") );
    jackPortsSelected->setAlignment( Qt::AlignRight );
    jackPropLayout->addWidget( jackPortsSelected, 0, 1 );

    /* Caching */
    QLabel *jackCachingLabel = new QLabel( qtr( "Input caching:" ) );
    jackPropLayout->addWidget( jackCachingLabel, 1 , 0 );
    jackCaching = new QSpinBox;
    setSpinBoxFreq( jackCaching );
    jackCaching->setSuffix( " ms" );
    jackCaching->setValue(1000);
    jackCaching->setAlignment( Qt::AlignRight );
    jackPropLayout->addWidget( jackCaching, 1 , 1 );

    /* Pace */
    jackPace = new QCheckBox(qtr( "Use VLC pace" ));
    jackPropLayout->addWidget( jackPace, 2, 1 );

    /* Auto Connect */
    jackConnect = new QCheckBox( qtr( "Auto connnection" ));
    jackPropLayout->addWidget( jackConnect, 3, 1 );

    /* Jack CONNECTs */
    CuMRL( jackChannels, valueChanged( int ) );
    CuMRL( jackCaching, valueChanged( int ) );
    CuMRL( jackPace, stateChanged( int ) );
    CuMRL( jackConnect, stateChanged( int ) );
    CuMRL( jackPortsSelected, textChanged( QString ) );
    }

    /************
     * PVR      *
     ************/
    if( module_Exists( p_intf, "pvr" ) ){
    addModuleAndLayouts( PVR_DEVICE, pvr, "PVR" );

    /* PVR Main panel */
    QLabel *pvrDeviceLabel = new QLabel( qtr( "Device name" ) );
    pvrDevLayout->addWidget( pvrDeviceLabel, 0, 0 );

    pvrDevice = new QLineEdit;
    pvrDevLayout->addWidget( pvrDevice, 0, 1 );

    QLabel *pvrRadioDeviceLabel = new QLabel( qtr( "Radio device name" ) );
    pvrDevLayout->addWidget( pvrRadioDeviceLabel, 1, 0 );

    pvrRadioDevice = new QLineEdit;
    pvrDevLayout->addWidget( pvrRadioDevice, 1, 1 );

    /* PVR props panel */
    QLabel *pvrNormLabel = new QLabel( qtr( "Norm" ) );
    pvrPropLayout->addWidget( pvrNormLabel, 0, 0 );

    pvrNormBox = new QComboBox;
    setfillVLCConfigCombo( "pvr-norm", p_intf, pvrNormBox );
    pvrPropLayout->addWidget( pvrNormBox, 0, 1 );

    QLabel *pvrFreqLabel = new QLabel( qtr( "Frequency" ) );
    pvrPropLayout->addWidget( pvrFreqLabel, 1, 0 );

    pvrFreq = new QSpinBox;
    pvrFreq->setAlignment( Qt::AlignRight );
    pvrFreq->setSuffix(" kHz");
    setSpinBoxFreq( pvrFreq );
    pvrPropLayout->addWidget( pvrFreq, 1, 1 );

    QLabel *pvrBitrLabel = new QLabel( qtr( "Bitrate" ) );
    pvrPropLayout->addWidget( pvrBitrLabel, 2, 0 );

    pvrBitr = new QSpinBox;
    pvrBitr->setAlignment( Qt::AlignRight );
    pvrBitr->setSuffix(" kHz");
    setSpinBoxFreq( pvrBitr );
    pvrPropLayout->addWidget( pvrBitr, 2, 1 );
    pvrPropLayout->addItem( new QSpacerItem( 20, 20, QSizePolicy::Expanding ),
            3, 0, 1, 1 );

    /* PVR CONNECTs */
    CuMRL( pvrDevice, textChanged( QString ) );
    CuMRL( pvrRadioDevice, textChanged( QString ) );

    CuMRL( pvrFreq, valueChanged ( int ) );
    CuMRL( pvrBitr, valueChanged ( int ) );
    CuMRL( pvrNormBox, currentIndexChanged ( int ) );
    }

    /**************
     * DVB Stuffs *
     **************/
    if( module_Exists( p_intf, "dvb" ) ){
    addModuleAndLayouts( DVB_DEVICE, dvb, "DVB" );

    /* DVB Main */
    QLabel *dvbDeviceLabel = new QLabel( qtr( "Adapter card to tune" ) );
    QLabel *dvbTypeLabel = new QLabel( qtr( "DVB Type:" ) );

    dvbCard = new QSpinBox;
    dvbCard->setAlignment( Qt::AlignRight );
    dvbCard->setPrefix( "/dev/dvb/adapter" );

    dvbDevLayout->addWidget( dvbDeviceLabel, 0, 0 );
    dvbDevLayout->addWidget( dvbCard, 0, 2, 1, 2 );

    dvbs = new QRadioButton( "DVB-S" );
    dvbs->setChecked( true );
    dvbc = new QRadioButton( "DVB-C" );
    dvbt = new QRadioButton( "DVB-T" );

    dvbDevLayout->addWidget( dvbTypeLabel, 1, 0 );
    dvbDevLayout->addWidget( dvbs, 1, 1 );
    dvbDevLayout->addWidget( dvbc, 1, 2 );
    dvbDevLayout->addWidget( dvbt, 1, 3 );

    /* DVB Props panel */
    QLabel *dvbFreqLabel =
                    new QLabel( qtr( "Transponder/multiplex frequency" ) );
    dvbPropLayout->addWidget( dvbFreqLabel, 0, 0 );

    dvbFreq = new QSpinBox;
    dvbFreq->setAlignment( Qt::AlignRight );
    dvbFreq->setSuffix(" kHz");
    setSpinBoxFreq( dvbFreq  );
    dvbPropLayout->addWidget( dvbFreq, 0, 1 );

    QLabel *dvbSrateLabel = new QLabel( qtr( "Transponder symbol rate" ) );
    dvbPropLayout->addWidget( dvbSrateLabel, 1, 0 );

    dvbSrate = new QSpinBox;
    dvbSrate->setAlignment( Qt::AlignRight );
    dvbSrate->setSuffix(" kHz");
    setSpinBoxFreq( dvbSrate );
    dvbPropLayout->addWidget( dvbSrate, 1, 1 );
    dvbPropLayout->addItem( new QSpacerItem( 20, 20, QSizePolicy::Expanding ),
            2, 0, 2, 1 );

    /* DVB CONNECTs */
    CuMRL( dvbCard, valueChanged ( int ) );
    CuMRL( dvbFreq, valueChanged ( int ) );
    CuMRL( dvbSrate, valueChanged ( int ) );

    BUTTONACT( dvbs, updateButtons() );
    BUTTONACT( dvbt, updateButtons() );
    BUTTONACT( dvbc, updateButtons() );
    }

#endif


    /**********
     * Screen *
     **********/
    addModuleAndLayouts( SCREEN_DEVICE, screen, "Desktop" );
    QLabel *screenLabel = new QLabel( "This option will open your own "
            "desktop in order to save or stream it.");
    screenLabel->setWordWrap( true );
    screenDevLayout->addWidget( screenLabel, 0, 0 );

    QLabel *screenFPSLabel = new QLabel(
            qtr( "Desired frame rate for the capture." ) );
    screenPropLayout->addWidget( screenFPSLabel, 0, 0 );

    screenFPS = new QSpinBox;
    screenFPS->setValue( 1 );
    screenFPS->setAlignment( Qt::AlignRight );
    screenPropLayout->addWidget( screenFPS, 0, 1 );

    /* General connects */
    CONNECT( ui.deviceCombo, activated( int ) ,
             stackedDevLayout, setCurrentIndex( int ) );
    CONNECT( ui.deviceCombo, activated( int ),
             stackedPropLayout, setCurrentIndex( int ) );
    CONNECT( ui.deviceCombo, activated( int ), this, updateMRL() );
    CONNECT( ui.deviceCombo, activated( int ), this, updateButtons() );

#undef addModule
}

CaptureOpenPanel::~CaptureOpenPanel()
{
}

void CaptureOpenPanel::clear()
{
    advMRL.clear();
}

void CaptureOpenPanel::updateMRL()
{
    QString mrl = "";
    int i_devicetype = ui.deviceCombo->itemData(
            ui.deviceCombo->currentIndex() ).toInt();
    switch( i_devicetype )
    {
#ifdef WIN32
    case BDA_DEVICE:
        if( bdas->isChecked() ) mrl = "dvb-s://";
        else if(  bdat->isChecked() ) mrl = "dvb-t://";
        else if(  bdac->isChecked() ) mrl = "dvb-c://";
        else return;
        mrl += " :dvb-frequency=" + QString("%1").arg( bdaFreq->value() );
        if( bdas->isChecked() || bdac->isChecked() )
            mrl += " :dvb-srate=" + QString("%1").arg( bdaSrate->value() );
        else
            mrl += " :dvb-bandwidth=" +
                QString("%1").arg( bdaBandBox->itemData(
                    bdaBandBox->currentIndex() ).toInt() );
        break;
    case DSHOW_DEVICE:
        mrl = "dshow://";
        mrl+= " :dshow-vdev=" + QString("\"%1\"").arg( vdevDshowW->getValue() );
        mrl+= " :dshow-adev=" + QString("\"%1\"").arg( adevDshowW->getValue() );
        if( dshowVSizeLine->isModified() )
            mrl += " :dshow-size=" + dshowVSizeLine->text();
        break;
#else
    case V4L_DEVICE:
        mrl = "v4l://";
        mrl += " :v4l-vdev=" + v4lVideoDevice->text();
        mrl += " :v4l-adev=" + v4lAudioDevice->text();
        mrl += " :v4l-norm=" + QString("%1").arg( v4lNormBox->currentIndex() );
        mrl += " :v4l-frequency=" + QString("%1").arg( v4lFreq->value() );
        break;
    case V4L2_DEVICE:
        mrl = "v4l2://";
        mrl += " :v4l2-dev=" + v4l2VideoDevice->text();
        mrl += " :v4l2-adev=" + v4l2AudioDevice->text();
        mrl += " :v4l2-standard=" + QString("%1").arg( v4l2StdBox->currentIndex() );
        break;
    case JACK_DEVICE:
        mrl = "jack://";
        mrl += "channels=" + QString("%1").arg( jackChannels->value() );
        mrl += ":ports=" + jackPortsSelected->text();
        mrl += " --jack-input-caching=" + QString("%1").arg( jackCaching->value() );
        if ( jackPace->isChecked() )
        {
                mrl += " --jack-input-use-vlc-pace";
        }
        if ( jackConnect->isChecked() )
        {
                mrl += " --jack-input-auto-connect";
        }
        break;
    case PVR_DEVICE:
        mrl = "pvr://";
        mrl += " :pvr-device=" + pvrDevice->text();
        mrl += " :pvr-radio-device=" + pvrRadioDevice->text();
        mrl += " :pvr-norm=" + QString("%1").arg( pvrNormBox->currentIndex() );
        if( pvrFreq->value() )
            mrl += " :pvr-frequency=" + QString("%1").arg( pvrFreq->value() );
        if( pvrBitr->value() )
            mrl += " :pvr-bitrate=" + QString("%1").arg( pvrBitr->value() );
        break;
    case DVB_DEVICE:
        mrl = "dvb://";
        mrl += " :dvb-adapter=" + QString("%1").arg( dvbCard->value() );
        mrl += " :dvb-frequency=" + QString("%1").arg( dvbFreq->value() );
        mrl += " :dvb-srate=" + QString("%1").arg( dvbSrate->value() );
        break;
#endif
    case SCREEN_DEVICE:
        mrl = "screen://";
        mrl += " :screen-fps=" + QString("%1").arg( screenFPS->value() );
        updateButtons();
        break;
    }

    if( !advMRL.isEmpty() ) mrl += advMRL;

    emit mrlUpdated( mrl );
}

/**
 * Update the Buttons (show/hide) for the GUI as all device type don't
 * use the same ui. elements.
 **/
void CaptureOpenPanel::updateButtons()
{
    /*  Be sure to display the ui Elements in case they were hidden by
     *  some Device Type (like Screen://) */
    ui.optionsBox->show();
    ui.advancedButton->show();
    /* Get the current Device Number */
    int i_devicetype = ui.deviceCombo->itemData(
                                ui.deviceCombo->currentIndex() ).toInt();
    switch( i_devicetype )
    {
#ifdef WIN32
    case BDA_DEVICE:
        if( bdas->isChecked() || bdac->isChecked() )
        {
            bdaSrate->show();
            bdaSrateLabel->show();
            bdaBandBox->hide();
            bdaBandLabel->hide();
        }
        else
        {
            bdaSrate->hide();
            bdaSrateLabel->hide();
            bdaBandBox->show();
            bdaBandLabel->show();
        }
        break;
#else
    case DVB_DEVICE:
        if( dvbs->isChecked() ) dvbFreq->setSuffix(" kHz");
        if( dvbc->isChecked() || dvbt->isChecked() ) dvbFreq->setSuffix(" Hz");
        break;
#endif
    case SCREEN_DEVICE:
        //ui.optionsBox->hide();
        ui.advancedButton->hide();
        break;
    }

    advMRL.clear();
}

void CaptureOpenPanel::advancedDialog()
{
    /* Get selected device type */
    int i_devicetype = ui.deviceCombo->itemData(
                                ui.deviceCombo->currentIndex() ).toInt();

    /* Get the corresponding module */
    module_t *p_module =
        module_Find( VLC_OBJECT(p_intf), psz_devModule[i_devicetype] );
    if( NULL == p_module ) return;

    /* Init */
    QList<ConfigControl *> controls;

    /* Get the confsize  */
    unsigned int i_confsize;
    module_config_t *p_config;
    p_config = module_GetConfig( p_module, &i_confsize );

    /* New Adv Prop dialog */
    adv = new QDialog( this );
    adv->setWindowTitle( qtr( "Advanced Options" ) );

    /* A main Layout with a Frame */
    QVBoxLayout *mainLayout = new QVBoxLayout( adv );
    QScrollArea *scroll = new QScrollArea;
    mainLayout->addWidget( scroll );

    QFrame *advFrame = new QFrame;
    /* GridLayout inside the Frame */
    QGridLayout *gLayout = new QGridLayout( advFrame );

    scroll->setWidgetResizable( true );
    scroll->setWidget( advFrame );

    /* Create the options inside the FrameLayout */
    for( int n = 0; n < i_confsize; n++ )
    {
        module_config_t *p_item = p_config + n;
        ConfigControl *config = ConfigControl::createControl(
                        VLC_OBJECT( p_intf ), p_item, advFrame, gLayout, n );
        controls.append( config );
    }

    /* Button stuffs */
    QDialogButtonBox *advButtonBox = new QDialogButtonBox( adv );
    QPushButton *closeButton = new QPushButton( qtr( "OK" ) );
    QPushButton *cancelButton = new QPushButton( qtr( "Cancel" ) );

    CONNECT( closeButton, clicked(), adv, accept() );
    CONNECT( cancelButton, clicked(), adv, reject() );

    advButtonBox->addButton( closeButton, QDialogButtonBox::AcceptRole );
    advButtonBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    mainLayout->addWidget( advButtonBox );

    /* Creation of the MRL */
    if( adv->exec() )
    {
        QString tempMRL = "";
        for( int i = 0; i < controls.size(); i++ )
        {
            ConfigControl *control = controls[i];
            if( !control )
            {
                msg_Dbg( p_intf, "This shouldn't happen, please report" );
                continue;
            }

            tempMRL += (i ? " :" : ":");

            if( control->getType() == CONFIG_ITEM_BOOL )
                if( !(qobject_cast<VIntConfigControl *>(control)->getValue() ) )
                    tempMRL += "no-";

            tempMRL += control->getName();

            switch( control->getType() )
            {
                case CONFIG_ITEM_STRING:
                case CONFIG_ITEM_FILE:
                case CONFIG_ITEM_DIRECTORY:
                case CONFIG_ITEM_MODULE:
                    tempMRL += QString("=\"%1\"").arg( qobject_cast<VStringConfigControl *>(control)->getValue() );
                    break;
                case CONFIG_ITEM_INTEGER:
                    tempMRL += QString("=%1").arg( qobject_cast<VIntConfigControl *>(control)->getValue() );
                    break;
                case CONFIG_ITEM_FLOAT:
                    tempMRL += QString("=%1").arg( qobject_cast<VFloatConfigControl *>(control)->getValue() );
                    break;
            }
        }
        advMRL = tempMRL;
        updateMRL();
        msg_Dbg( p_intf, "%s", qtu( advMRL ) );
    }
    delete adv;
    module_PutConfig( p_config );
    module_Put( p_module );
}

