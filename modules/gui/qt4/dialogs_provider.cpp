/*****************************************************************************
 * dialogs_provider.cpp : Dialog Provider
 *****************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
 * $Id$
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

#include <vlc_intf_strings.h>

#include "qt4.hpp"
#include "dialogs_provider.hpp"
#include "input_manager.hpp" /* Load Subtitles */
#include "menus.hpp"
#include "recents.hpp"
#include "util/qt_dirs.hpp"

/* The dialogs */
#include "dialogs/playlist.hpp"
#include "dialogs/bookmarks.hpp"
#include "dialogs/preferences.hpp"
#include "dialogs/mediainfo.hpp"
#include "dialogs/messages.hpp"
#include "dialogs/extended.hpp"
#include "dialogs/vlm.hpp"
#include "dialogs/sout.hpp"
#include "dialogs/convert.hpp"
#include "dialogs/open.hpp"
#include "dialogs/openurl.hpp"
#include "dialogs/help.hpp"
#include "dialogs/gototime.hpp"
#include "dialogs/podcast_configuration.hpp"
#include "dialogs/toolbar.hpp"
#include "dialogs/plugins.hpp"
#include "dialogs/external.hpp"
#include "dialogs/errors.hpp"
#include "dialogs/epg.hpp"

#include <QEvent>
#include <QApplication>
#include <QSignalMapper>
#include <QFileDialog>


DialogsProvider* DialogsProvider::instance = NULL;

DialogsProvider::DialogsProvider( intf_thread_t *_p_intf ) :
                                  QObject( NULL ), p_intf( _p_intf )
{
    b_isDying = false;

    /* Various signal mappers for the menus */
    menusMapper = new QSignalMapper();
    CONNECT( menusMapper, mapped(QObject *), this, menuAction( QObject *) );

    menusUpdateMapper = new QSignalMapper();
    CONNECT( menusUpdateMapper, mapped(QObject *),
             this, menuUpdateAction( QObject *) );

    SDMapper = new QSignalMapper();
    CONNECT( SDMapper, mapped (QString), this, SDMenuAction( QString ) );

    new DialogHandler (p_intf, this );
}

DialogsProvider::~DialogsProvider()
{
    PlaylistDialog::killInstance();
    MediaInfoDialog::killInstance();
    MessagesDialog::killInstance();
    ExtendedDialog::killInstance();
    BookmarksDialog::killInstance();
    HelpDialog::killInstance();
#ifdef UPDATE_CHECK
    UpdateDialog::killInstance();
#endif

    delete menusMapper;
    delete menusUpdateMapper;
    delete SDMapper;
}

void DialogsProvider::quit()
{
    b_isDying = true;
    libvlc_Quit( p_intf->p_libvlc );
}

void DialogsProvider::customEvent( QEvent *event )
{
    if( event->type() == (int)DialogEvent_Type )
    {
        DialogEvent *de = static_cast<DialogEvent*>(event);
        switch( de->i_dialog )
        {
        case INTF_DIALOG_FILE_SIMPLE:
        case INTF_DIALOG_FILE:
            openDialog(); break;
        case INTF_DIALOG_FILE_GENERIC:
            openFileGenericDialog( de->p_arg ); break;
        case INTF_DIALOG_DISC:
            openDiscDialog(); break;
        case INTF_DIALOG_NET:
            openNetDialog(); break;
        case INTF_DIALOG_SAT:
        case INTF_DIALOG_CAPTURE:
            openCaptureDialog(); break;
        case INTF_DIALOG_DIRECTORY:
            PLAppendDir(); break;
        case INTF_DIALOG_PLAYLIST:
            playlistDialog(); break;
        case INTF_DIALOG_MESSAGES:
            messagesDialog(); break;
        case INTF_DIALOG_FILEINFO:
           mediaInfoDialog(); break;
        case INTF_DIALOG_PREFS:
           prefsDialog(); break;
        case INTF_DIALOG_BOOKMARKS:
           bookmarksDialog(); break;
        case INTF_DIALOG_EXTENDED:
           extendedDialog(); break;
#ifdef ENABLE_VLM
        case INTF_DIALOG_VLM:
           vlmDialog(); break;
#endif
        case INTF_DIALOG_POPUPMENU:
           QVLCMenu::PopupMenu( p_intf, (de->i_arg != 0) ); break;
        case INTF_DIALOG_AUDIOPOPUPMENU:
           QVLCMenu::AudioPopupMenu( p_intf ); break;
        case INTF_DIALOG_VIDEOPOPUPMENU:
           QVLCMenu::VideoPopupMenu( p_intf ); break;
        case INTF_DIALOG_MISCPOPUPMENU:
           QVLCMenu::MiscPopupMenu( p_intf ); break;
        case INTF_DIALOG_WIZARD:
        case INTF_DIALOG_STREAMWIZARD:
            openAndStreamingDialogs(); break;
#ifdef UPDATE_CHECK
        case INTF_DIALOG_UPDATEVLC:
            updateDialog(); break;
#endif
        case INTF_DIALOG_EXIT:
            quit(); break;
        default:
           msg_Warn( p_intf, "unimplemented dialog" );
        }
    }
}

/****************************************************************************
 * Individual simple dialogs
 ****************************************************************************/
void DialogsProvider::playlistDialog()
{
    PlaylistDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::prefsDialog()
{
    PrefsDialog *p = new PrefsDialog( (QWidget *)p_intf->p_sys->p_mi, p_intf );
    p->toggleVisible();
}

void DialogsProvider::extendedDialog()
{
    if( !ExtendedDialog::getInstance( p_intf )->isVisible() || /* Hidden */
        ExtendedDialog::getInstance( p_intf )->currentTab() != 0 )  /* wrong tab */
        ExtendedDialog::getInstance( p_intf )->showTab( 0 );
    else
        ExtendedDialog::getInstance( p_intf )->hide();
}

void DialogsProvider::synchroDialog()
{
    if( !ExtendedDialog::getInstance( p_intf )->isVisible() || /* Hidden */
        ExtendedDialog::getInstance( p_intf )->currentTab() != 2 )  /* wrong tab */
        ExtendedDialog::getInstance( p_intf )->showTab( 2 );
    else
        ExtendedDialog::getInstance( p_intf )->hide();
}

void DialogsProvider::messagesDialog()
{
    MessagesDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::gotoTimeDialog()
{
    GotoTimeDialog::getInstance( p_intf )->toggleVisible();
}

#ifdef ENABLE_VLM
void DialogsProvider::vlmDialog()
{
    VLMDialog::getInstance( p_intf )->toggleVisible();
}
#endif

void DialogsProvider::helpDialog()
{
    HelpDialog::getInstance( p_intf )->toggleVisible();
}

#ifdef UPDATE_CHECK
void DialogsProvider::updateDialog()
{
    UpdateDialog::getInstance( p_intf )->toggleVisible();
}
#endif

void DialogsProvider::aboutDialog()
{
    AboutDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::mediaInfoDialog()
{
    MediaInfoDialog::getInstance( p_intf )->showTab( 0 );
}

void DialogsProvider::mediaCodecDialog()
{
    MediaInfoDialog::getInstance( p_intf )->showTab( 2 );
}

void DialogsProvider::bookmarksDialog()
{
    BookmarksDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::podcastConfigureDialog()
{
    PodcastConfigDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::toolbarDialog()
{
    ToolbarEditDialog *toolbarEditor = new ToolbarEditDialog( (QWidget *)p_intf->p_sys->p_mi, p_intf );
    if( toolbarEditor->exec() == QDialog::Accepted )
        emit toolBarConfUpdated();
}

void DialogsProvider::pluginDialog()
{
    PluginDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::epgDialog()
{
    EpgDialog::getInstance( p_intf )->toggleVisible();
}

/* Generic open file */
void DialogsProvider::openFileGenericDialog( intf_dialog_args_t *p_arg )
{
    if( p_arg == NULL )
    {
        msg_Warn( p_intf, "openFileGenericDialog() called with NULL arg" );
        return;
    }

    /* Replace the extensions to a Qt format */
    int i = 0;
    QString extensions = qfu( p_arg->psz_extensions );
    while ( ( i = extensions.indexOf( "|", i ) ) != -1 )
    {
        if( ( extensions.count( "|" ) % 2 ) == 0 )
            extensions.replace( i, 1, ");;" );
        else
            extensions.replace( i, 1, "(" );
    }
    extensions.replace( ";*", " *" );
    extensions.append( ")" );

    /* Save */
    if( p_arg->b_save )
    {
        QString file = QFileDialog::getSaveFileName( NULL, p_arg->psz_title,
                                        p_intf->p_sys->filepath, extensions );
        if( !file.isEmpty() )
        {
            p_arg->i_results = 1;
            p_arg->psz_results = (char **)malloc( p_arg->i_results * sizeof( char * ) );
            p_arg->psz_results[0] = strdup( qtu( toNativeSepNoSlash( file ) ) );
        }
        else
            p_arg->i_results = 0;
    }
    else /* non-save mode */
    {
        QStringList files = QFileDialog::getOpenFileNames( NULL,
                p_arg->psz_title, p_intf->p_sys->filepath,
                extensions );
        p_arg->i_results = files.count();
        p_arg->psz_results = (char **)malloc( p_arg->i_results * sizeof( char * ) );
        i = 0;
        foreach( const QString &file, files )
            p_arg->psz_results[i++] = strdup( qtu( toNativeSepNoSlash( file ) ) );
        if(i == 0)
            p_intf->p_sys->filepath = QString::fromAscii("");
        else
            p_intf->p_sys->filepath = qfu( p_arg->psz_results[i-1] );
    }

    /* Callback */
    if( p_arg->pf_callback )
        p_arg->pf_callback( p_arg );

    /* Clean afterwards */
    if( p_arg->psz_results )
    {
        for( i = 0; i < p_arg->i_results; i++ )
            free( p_arg->psz_results[i] );
        free( p_arg->psz_results );
    }
    free( p_arg->psz_title );
    free( p_arg->psz_extensions );
    free( p_arg );
}
/****************************************************************************
 * All the open/add stuff
 * Open Dialog first - Simple Open then
 ****************************************************************************/

void DialogsProvider::openDialog( int i_tab )
{
    OpenDialog::getInstance( p_intf->p_sys->p_mi , p_intf )->showTab( i_tab );
}
void DialogsProvider::openDialog()
{
    openDialog( OPEN_FILE_TAB );
}
void DialogsProvider::openFileDialog()
{
    openDialog( OPEN_FILE_TAB );
}
void DialogsProvider::openDiscDialog()
{
    openDialog( OPEN_DISC_TAB );
}
void DialogsProvider::openNetDialog()
{
    openDialog( OPEN_NETWORK_TAB );
}
void DialogsProvider::openCaptureDialog()
{
    openDialog( OPEN_CAPTURE_TAB );
}

/* Same as the open one, but force the enqueue */
void DialogsProvider::PLAppendDialog( int tab )
{
    OpenDialog::getInstance( p_intf->p_sys->p_mi, p_intf, false,
                             OPEN_AND_ENQUEUE )->showTab( tab );
}

void DialogsProvider::MLAppendDialog( int tab )
{
    OpenDialog::getInstance( p_intf->p_sys->p_mi, p_intf, false,
                            OPEN_AND_ENQUEUE, false, false )
                                    ->showTab( tab );
}

/**
 * Simple open
 ***/
QStringList DialogsProvider::showSimpleOpen( const QString& help,
                                             int filters,
                                             const QString& path )
{
    QString fileTypes = "";
    if( filters & EXT_FILTER_MEDIA ) {
        ADD_FILTER_MEDIA( fileTypes );
    }
    if( filters & EXT_FILTER_VIDEO ) {
        ADD_FILTER_VIDEO( fileTypes );
    }
    if( filters & EXT_FILTER_AUDIO ) {
        ADD_FILTER_AUDIO( fileTypes );
    }
    if( filters & EXT_FILTER_PLAYLIST ) {
        ADD_FILTER_PLAYLIST( fileTypes );
    }
    if( filters & EXT_FILTER_SUBTITLE ) {
        ADD_FILTER_SUBTITLE( fileTypes );
    }
    ADD_FILTER_ALL( fileTypes );
    fileTypes.replace( ";*", " *");

    QStringList files = QFileDialog::getOpenFileNames( NULL,
        help.isEmpty() ? qtr(I_OP_SEL_FILES ) : help,
        path.isEmpty() ? p_intf->p_sys->filepath : path,
        fileTypes );

    if( !files.isEmpty() ) savedirpathFromFile( files.last() );

    return files;
}

/**
 * Open a file,
 * pl helps you to choose from playlist or media library,
 * go to start or enqueue
 **/
void DialogsProvider::addFromSimple( bool pl, bool go)
{
    QStringList files = DialogsProvider::showSimpleOpen();
    int i = 0;
    files.sort();
    foreach( const QString &file, files )
    {
        char* psz_uri = make_URI( qtu( toNativeSeparators(file) ) );
        playlist_Add( THEPL, psz_uri, NULL,
                      go ? ( PLAYLIST_APPEND | ( i ? PLAYLIST_PREPARSE : PLAYLIST_GO ) )
                         : ( PLAYLIST_APPEND | PLAYLIST_PREPARSE ),
                      PLAYLIST_END, pl, pl_Unlocked );
        free( psz_uri );
        RecentsMRL::getInstance( p_intf )->addRecent(
                toNativeSeparators( file ) );
        i++;
    }
}

void DialogsProvider::simpleOpenDialog()
{
    addFromSimple( true, true ); /* Playlist and Go */
}

void DialogsProvider::simplePLAppendDialog()
{
    addFromSimple( true, false );
}

void DialogsProvider::simpleMLAppendDialog()
{
    addFromSimple( false, false );
}

/* Url & Clipboard */
/**
 * Open a MRL.
 * If the clipboard contains URLs, the first is automatically 'preselected'.
 **/
void DialogsProvider::openUrlDialog()
{
    OpenUrlDialog *oud = OpenUrlDialog::getInstance( p_intf );
    if( oud->exec() == QDialog::Accepted )
    {
        QString url = oud->url();
        if( !url.isEmpty() )
        {
            playlist_Add( THEPL, qtu( url ),
                          NULL, !oud->shouldEnqueue() ?
                                  ( PLAYLIST_APPEND | PLAYLIST_GO )
                                : ( PLAYLIST_APPEND | PLAYLIST_PREPARSE ),
                          PLAYLIST_END, true, false );
            RecentsMRL::getInstance( p_intf )->addRecent( url );
        }
    }
}

/* Directory */
/**
 * Open a directory,
 * pl helps you to choose from playlist or media library,
 * go to start or enqueue
 **/
static void openDirectory( intf_thread_t *p_intf, bool pl, bool go )
{
    QString dir = QFileDialog::getExistingDirectory( NULL, qtr("Open Directory"), p_intf->p_sys->filepath );

    if (!dir.isEmpty() )
    {
        QString mrl = (dir.endsWith( "VIDEO_TS", Qt::CaseInsensitive ) ?
                       "dvd://" : "directory://")
                    + toNativeSeparators( dir );
        input_item_t *p_input = input_item_New( THEPL, qtu( mrl ), NULL );

        /* FIXME: playlist_AddInput() can fail */
        playlist_AddInput( THEPL, p_input,
                       go ? ( PLAYLIST_APPEND | PLAYLIST_GO ) : PLAYLIST_APPEND,
                       PLAYLIST_END, pl, pl_Unlocked );
        RecentsMRL::getInstance( p_intf )->addRecent( mrl );
        if( !go )
            input_Read( THEPL, p_input );
        vlc_gc_decref( p_input );
    }
}

void DialogsProvider::PLOpenDir()
{
    openDirectory( p_intf, true, true );
}

void DialogsProvider::PLAppendDir()
{
    openDirectory( p_intf, true, false );
}

void DialogsProvider::MLAppendDir()
{
    openDirectory( p_intf, false , false );
}

/****************
 * Playlist     *
 ****************/
void DialogsProvider::openAPlaylist()
{
    QStringList files = showSimpleOpen( qtr( "Open playlist..." ),
                                        EXT_FILTER_PLAYLIST );
    foreach( const QString &file, files )
    {
        playlist_Import( THEPL, qtu( toNativeSeparators( file ) ) );
    }
}

void DialogsProvider::saveAPlaylist()
{
    static const struct
    {
        char filter[24];
        char module[12];
    } types[] = {
        { N_("XSPF playlist (*.xspf)"), "export-xspf", },
        { N_("M3U8 playlist (*.m3u)"), "export-m3u8", },
        { N_("M3U playlist (*.m3u)"), "export-m3u", },
        { N_("HTML playlist (*.html)"), "export-html", },
    };
    QString filters, selected;

    for( size_t i = 0; i < sizeof (types) / sizeof (types[0]); i++ )
    {
        if( !filters.isEmpty() )
            filters += ";;";
        filters += qfu( vlc_gettext( types[i].filter ) );
    }

    QString file = QFileDialog::getSaveFileName( NULL,
                                  qtr( "Save playlist as..." ),
                                  p_intf->p_sys->filepath, filters, &selected );
    if( file.isEmpty() )
        return;

    for( size_t i = 0; i < sizeof (types) / sizeof (types[0]); i++)
        if( selected == qfu( vlc_gettext( types[i].filter ) ) )
        {
            playlist_Export( THEPL, qtu( toNativeSeparators( file ) ),
                             THEPL->p_local_category, types[i].module );
            break;
        }
}

/****************************************************************************
 * Sout emulation
 ****************************************************************************/

void DialogsProvider::streamingDialog( QWidget *parent,
                                       const QString& mrl,
                                       bool b_transcode_only,
                                       QStringList options )
{
    QString soutoption;

    /* Stream */
    if( !b_transcode_only )
    {
        SoutDialog *s = new SoutDialog( parent, p_intf, mrl );
        if( s->exec() == QDialog::Accepted )
        {
            soutoption = s->getMrl();
            delete s;
        }
        else
        {
            delete s; return;
        }
    } else {
    /* Convert */
        ConvertDialog *s = new ConvertDialog( parent, p_intf, mrl );
        if( s->exec() == QDialog::Accepted )
        {
            soutoption = s->getMrl();
            delete s;
        }
        else
        {
            delete s; return;
        }
    }

    /* Get SoutMRL */
    if( !soutoption.isEmpty() )
    {
        options += soutoption.split( " :");

        /* Create Input */
        input_item_t *p_input;
        p_input = input_item_New( p_intf, qtu( mrl ), _("Streaming") );

        /* Add normal Options */
        for( int j = 0; j < options.size(); j++ )
        {
            QString qs = colon_unescape( options[j] );
            if( !qs.isEmpty() )
            {
                input_item_AddOption( p_input, qtu( qs ),
                        VLC_INPUT_OPTION_TRUSTED );
                msg_Dbg( p_intf, "Adding option: %s", qtu( qs ) );
            }
        }

        /* Switch between enqueuing and starting the item */
        /* FIXME: playlist_AddInput() can fail */
        playlist_AddInput( THEPL, p_input,
                PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END, true, pl_Unlocked );
        vlc_gc_decref( p_input );

        RecentsMRL::getInstance( p_intf )->addRecent( mrl );
    }
}

void DialogsProvider::openAndStreamingDialogs()
{
    OpenDialog::getInstance( p_intf->p_sys->p_mi, p_intf, false, OPEN_AND_STREAM )
                                ->showTab( OPEN_FILE_TAB );
}

void DialogsProvider::openAndTranscodingDialogs()
{
    OpenDialog::getInstance( p_intf->p_sys->p_mi , p_intf, false, OPEN_AND_SAVE )
                                ->showTab( OPEN_FILE_TAB );
}

void DialogsProvider::loadSubtitlesFile()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( !p_input ) return;

    input_item_t *p_item = input_GetItem( p_input );
    if( !p_item ) return;

    char *path = input_item_GetURI( p_item );
    if( !path ) path = strdup( "" );

    char *sep = strrchr( path, DIR_SEP_CHAR );
    if( sep ) *sep = '\0';

    QStringList qsl = showSimpleOpen( qtr( "Open subtitles..." ),
                                      EXT_FILTER_SUBTITLE,
                                      qfu( path ) );
    free( path );
    foreach( const QString &qsFile, qsl )
    {
        if( input_AddSubtitle( p_input, qtu( toNativeSeparators( qsFile ) ),
                    true ) )
            msg_Warn( p_intf, "unable to load subtitles from '%s'",
                      qtu( qsFile ) );
    }
}


/****************************************************************************
 * Menus
 ****************************************************************************/

void DialogsProvider::menuAction( QObject *data )
{
    QVLCMenu::DoAction( data );
}

void DialogsProvider::menuUpdateAction( QObject *data )
{
    MenuFunc *func = qobject_cast<MenuFunc *>(data);
    assert( func );
    func->doFunc( p_intf );
}

void DialogsProvider::SDMenuAction( const QString& data )
{
    char *psz_sd = strdup( qtu( data ) );
    if( !playlist_IsServicesDiscoveryLoaded( THEPL, psz_sd ) )
        playlist_ServicesDiscoveryAdd( THEPL, psz_sd );
    else
        playlist_ServicesDiscoveryRemove( THEPL, psz_sd );
    free( psz_sd );
}

/**
 * Play the MRL contained in the Recently played menu.
 **/
void DialogsProvider::playMRL( const QString &mrl )
{
    char* psz_uri = make_URI( qtu(mrl) );
    playlist_Add( THEPL, psz_uri, NULL,
           PLAYLIST_APPEND | PLAYLIST_GO , PLAYLIST_END, true, false );
    free( psz_uri );

    RecentsMRL::getInstance( p_intf )->addRecent( mrl );
}
