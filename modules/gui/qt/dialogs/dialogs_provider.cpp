/*****************************************************************************
 * dialogs_provider.cpp : Dialog Provider
 *****************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
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

#include "qt.hpp"
#include "dialogs_provider.hpp"
#include "player/player_controller.hpp" /* Load Subtitles */
#include "playlist/playlist_controller.hpp"
#include "menus/menus.hpp"
#include "util/recents.hpp"
#include "util/qt_dirs.hpp"
#include "widgets/native/customwidgets.hpp" /* VLCKeyToString() */
#include "maininterface/main_interface.hpp"

/* The dialogs */
#include "dialogs/bookmarks/bookmarks.hpp"
#include "dialogs/preferences/preferences.hpp"
#include "dialogs/mediainfo/mediainfo.hpp"
#include "dialogs/messages/messages.hpp"
#include "dialogs/extended/extended.hpp"
#include "dialogs/vlm/vlm.hpp"
#include "dialogs/sout/sout.hpp"
#include "dialogs/sout/convert.hpp"
#include "dialogs/open/open.hpp"
#include "dialogs/open/openurl.hpp"
#include "dialogs/help/help.hpp"
#include "dialogs/gototime/gototime.hpp"
#include "dialogs/podcast/podcast_configuration.hpp"
#include "dialogs/toolbar/toolbareditor.hpp"
#include "dialogs/plugins/plugins.hpp"
#include "dialogs/epg/epg.hpp"
#include "dialogs/errors/errors.hpp"

#include <QEvent>
#include <QApplication>
#include <QSignalMapper>
#include <QFileDialog>
#include <QUrl>

#define I_OP_DIR_WINTITLE I_DIR_OR_FOLDER( N_("Open Directory"), \
                                           N_("Open Folder") )

DialogsProvider* DialogsProvider::instance = NULL;

DialogsProvider::DialogsProvider( intf_thread_t *_p_intf ) :
                                  QObject( NULL ), p_intf( _p_intf ),
                                  popupMenu( NULL ),
                                  videoPopupMenu( NULL ),
                                  audioPopupMenu( NULL ),
                                  miscPopupMenu( NULL )
{
    b_isDying = false;
}

DialogsProvider::~DialogsProvider()
{
    MediaInfoDialog::killInstance();
    MessagesDialog::killInstance();
    BookmarksDialog::killInstance();
#ifdef ENABLE_VLM
    VLMDialog::killInstance();
#endif
    HelpDialog::killInstance();
#ifdef UPDATE_CHECK
    UpdateDialog::killInstance();
#endif
    PluginDialog::killInstance();
    EpgDialog::killInstance();

    delete popupMenu;
    delete videoPopupMenu;
    delete audioPopupMenu;
    delete miscPopupMenu;

    /* free parentless menus  */
    VLCMenuBar::freeRecentsMenu();
    VLCMenuBar::freeRendererMenu();
}

QString DialogsProvider::getSaveFileName( QWidget *parent,
                                          const QString &caption,
                                          const QUrl &dir,
                                          const QString &filter,
                                          QString *selectedFilter )
{
    const QStringList schemes = QStringList(QStringLiteral("file"));
    return QFileDialog::getSaveFileUrl( parent, caption, dir, filter, selectedFilter, QFileDialog::Options(), schemes).toLocalFile();
}

void DialogsProvider::quit()
{
    b_isDying = true;
    libvlc_Quit( vlc_object_instance(p_intf) );
}

void DialogsProvider::customEvent( QEvent *event )
{
    if( event->type() == DialogEvent::DialogEvent_Type )
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
            //FIXME
            //playlistDialog(); break;
            break;
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
        case INTF_DIALOG_SENDKEY:
           sendKey( de->i_arg ); break;
#ifdef ENABLE_VLM
        case INTF_DIALOG_VLM:
           vlmDialog(); break;
#endif
        case INTF_DIALOG_POPUPMENU:
        {
           delete popupMenu; popupMenu = NULL;
           bool show = (de->i_arg != 0);
           if( show )
           {
               if( p_intf->p_sys->p_mi )
                   p_intf->p_sys->p_mi->popupMenu( show );
               else
               {
                   //popping a QMenu prevents mouse release events to be received,
                   //this ensures the coherency of the vout mouse state.
                   emit releaseMouseEvents();
                   popupMenu = VLCMenuBar::PopupMenu( p_intf, show );
               }
           }
           break;
        }
        case INTF_DIALOG_AUDIOPOPUPMENU:
        {
           delete audioPopupMenu; audioPopupMenu = NULL;
           bool show = (de->i_arg != 0);
           if( show )
               audioPopupMenu = VLCMenuBar::AudioPopupMenu( p_intf, show );
           break;
        }
        case INTF_DIALOG_VIDEOPOPUPMENU:
        {
           delete videoPopupMenu; videoPopupMenu = NULL;
           bool show = (de->i_arg != 0);
           if( show )
               videoPopupMenu = VLCMenuBar::VideoPopupMenu( p_intf, show );
           break;
        }
        case INTF_DIALOG_MISCPOPUPMENU:
        {
           delete miscPopupMenu; miscPopupMenu = NULL;
           bool show = (de->i_arg != 0);
           if( show )
               miscPopupMenu = VLCMenuBar::MiscPopupMenu( p_intf, show );
           break;
        }
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
const QEvent::Type DialogEvent::DialogEvent_Type =
        (QEvent::Type)QEvent::registerEventType();

void DialogsProvider::prefsDialog()
{
    PrefsDialog *p = new PrefsDialog( (QWidget *)p_intf->p_sys->p_mi, p_intf );
    p->toggleVisible();
}

void DialogsProvider::extendedDialog()
{
    ExtendedDialog *extDialog = ExtendedDialog::getInstance(p_intf );

    if( !extDialog->isVisible() || /* Hidden */
        extDialog->currentTab() != 0 )  /* wrong tab */
        extDialog->showTab( 0 );
    else
        extDialog->hide();
}

void DialogsProvider::synchroDialog()
{
    ExtendedDialog *extDialog = ExtendedDialog::getInstance(p_intf );

    if( !extDialog->isVisible() || /* Hidden */
        extDialog->currentTab() != 2 )  /* wrong tab */
        extDialog->showTab( 2 );
    else
        extDialog->hide();
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
    MediaInfoDialog *dialog = MediaInfoDialog::getInstance( p_intf );
    if( !dialog->isVisible() || dialog->currentTab() != MediaInfoDialog::META_PANEL )
        dialog->showTab( MediaInfoDialog::META_PANEL );
    else
        dialog->hide();
}

void DialogsProvider::mediaCodecDialog()
{
    MediaInfoDialog *dialog = MediaInfoDialog::getInstance( p_intf );
    if( !dialog->isVisible() || dialog->currentTab() != MediaInfoDialog::INFO_PANEL )
        dialog->showTab( MediaInfoDialog::INFO_PANEL );
    else
        dialog->hide();
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
    ToolbarEditorDialog *toolbarEditor = new ToolbarEditorDialog( (QWidget *)p_intf->p_sys->p_mi, p_intf);
    if( toolbarEditor->exec() == QDialog::Accepted )
        emit toolBarConfUpdated();
    delete toolbarEditor;
}

void DialogsProvider::pluginDialog()
{
    PluginDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::epgDialog()
{
    EpgDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::setPopupMenu()
{
    delete popupMenu;
    popupMenu = VLCMenuBar::PopupMenu( p_intf, true );
}

void DialogsProvider::destroyPopupMenu()
{
    delete popupMenu;
    popupMenu = NULL;
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
        QString file = getSaveFileName( NULL,
                                        qfu( p_arg->psz_title ),
                                        p_intf->p_sys->filepath, extensions );
        if( !file.isEmpty() )
        {
            p_arg->i_results = 1;
            p_arg->psz_results = (char **)vlc_alloc( p_arg->i_results, sizeof( char * ) );
            p_arg->psz_results[0] = strdup( qtu( toNativeSepNoSlash( file ) ) );
        }
        else
            p_arg->i_results = 0;
    }
    else /* non-save mode */
    {
        QList<QUrl> urls = QFileDialog::getOpenFileUrls( NULL, qfu( p_arg->psz_title ),
                                       p_intf->p_sys->filepath, extensions );
        p_arg->i_results = urls.count();
        p_arg->psz_results = (char **)vlc_alloc( p_arg->i_results, sizeof( char * ) );
        i = 0;
        foreach( const QUrl &uri, urls )
            p_arg->psz_results[i++] = strdup( uri.toEncoded().constData() );
        if( !urls.isEmpty() )
            p_intf->p_sys->filepath =  urls.last();
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

/**
 * Simple open
 ***/
QStringList DialogsProvider::showSimpleOpen( const QString& help,
                                             int filters,
                                             const QUrl& path )
{
    QString fileTypes = "";
    if( filters & EXT_FILTER_MEDIA ) {
        ADD_EXT_FILTER( fileTypes, EXTENSIONS_MEDIA );
    }
    if( filters & EXT_FILTER_VIDEO ) {
        ADD_EXT_FILTER( fileTypes, EXTENSIONS_VIDEO );
    }
    if( filters & EXT_FILTER_AUDIO ) {
        ADD_EXT_FILTER( fileTypes, EXTENSIONS_AUDIO );
    }
    if( filters & EXT_FILTER_PLAYLIST ) {
        ADD_EXT_FILTER( fileTypes, EXTENSIONS_PLAYLIST );
    }
    if( filters & EXT_FILTER_SUBTITLE ) {
        ADD_EXT_FILTER( fileTypes, EXTENSIONS_SUBTITLE );
    }
    ADD_EXT_FILTER( fileTypes, EXTENSIONS_ALL );
    fileTypes.replace( ";*", " *");
    fileTypes.chop(2); //remove trailling ";;"

    QList<QUrl> urls = QFileDialog::getOpenFileUrls( NULL,
        help.isEmpty() ? qtr(I_OP_SEL_FILES ) : help,
        path.isEmpty() ? p_intf->p_sys->filepath : path,
        fileTypes );

    if( !urls.isEmpty() )
        p_intf->p_sys->filepath = urls.last();

    QStringList res;
    foreach( const QUrl &url, urls )
        res << url.toEncoded();

    return res;
}

void DialogsProvider::simpleOpenDialog()
{
    QStringList urls = DialogsProvider::showSimpleOpen();

    bool first = true;
    urls.sort();
    foreach( const QString &url, urls )
    {
        Open::openMRL( p_intf, url, first );
        first = false;
    }
}

/* Url & Clipboard */
/**
 * Open a MRL.
 * If the clipboard contains URLs, the first is automatically 'preselected'.
 **/
void DialogsProvider::openUrlDialog()
{
    OpenUrlDialog oud( p_intf );
    if( oud.exec() != QDialog::Accepted )
        return;

    QString url = oud.url();
    if( url.isEmpty() )
        return;

    if( !url.contains( qfu( "://" ) ) )
    {
        char *uri = vlc_path2uri( qtu( url ), NULL );
        if( uri == NULL )
            return;
        url = qfu(uri);
        free( uri );
    }

    Open::openMRL( p_intf, qtu(url), !oud.shouldEnqueue() );
}

/* Directory */
/**
 * Open a directory,
 * pl helps you to choose from playlist or media library,
 * go to start or enqueue
 **/
static void openDirectory( intf_thread_t *p_intf, bool go )
{
    QString uri = DialogsProvider::getDirectoryDialog( p_intf );
    if( !uri.isEmpty() )
        Open::openMRL( p_intf, uri, go );
}

QString DialogsProvider::getDirectoryDialog( intf_thread_t *p_intf )
{
    const QStringList schemes = QStringList(QStringLiteral("file"));
    QUrl dirurl = QFileDialog::getExistingDirectoryUrl( NULL,
            qtr( I_OP_DIR_WINTITLE ), p_intf->p_sys->filepath,
            QFileDialog::ShowDirsOnly, schemes );

    if( dirurl.isEmpty() ) return QString();

    p_intf->p_sys->filepath = dirurl;

    QString dir = dirurl.toLocalFile();
    const char *scheme = "directory";
    if( dir.endsWith( DIR_SEP "VIDEO_TS", Qt::CaseInsensitive ) )
        scheme = "dvd";
    else if( dir.endsWith( DIR_SEP "BDMV", Qt::CaseInsensitive ) )
    {
        scheme = "bluray";
        dir.remove( "BDMV" );
    }

    char *uri = vlc_path2uri( qtu( toNativeSeparators( dir ) ), scheme );
    if( unlikely(uri == NULL) )
        return QString();

    dir = qfu( uri );
    free( uri );

    return dir;
}

void DialogsProvider::PLOpenDir()
{
    openDirectory( p_intf, true );
}

void DialogsProvider::PLAppendDir()
{
    openDirectory( p_intf, false );
}

/****************
 * Playlist     *
 ****************/
void DialogsProvider::savePlayingToPlaylist()
{
    static const struct
    {
        char filter_name[14];
        char filter_patterns[5];
        char module[12];
    } types[] = {
        { N_("XSPF playlist"), "xspf", "export-xspf", },
        { N_("M3U playlist"),  "m3u",  "export-m3u", },
        { N_("M3U8 playlist"), "m3u8", "export-m3u8", },
        { N_("HTML playlist"), "html", "export-html", },
    };

    QStringList filters;
    QString ext = getSettings()->value( "last-playlist-ext" ).toString();

    for( size_t i = 0; i < sizeof (types) / sizeof (types[0]); i++ )
    {
        QString tmp = qfu( vlc_gettext( types[i].filter_name ) ) + " (*." + types[i].filter_patterns + ")";
        if( ext == qfu( types[i].filter_patterns ) )
            filters.insert( 0, tmp );
        else
            filters.append( tmp );
    }

    QString selected;
    QString file = getSaveFileName( NULL,
                                    qtr( "Save playlist as..." ),
                                    p_intf->p_sys->filepath, filters.join( ";;" ),
                                    &selected );
    const char *psz_selected_module = NULL;
    const char *psz_last_playlist_ext = NULL;

    if( file.isEmpty() )
        return;

    /* First test if the file extension is set, and different to selected filter */
    for( size_t i = 0; i < sizeof (types) / sizeof (types[0]); i++)
    {
        if ( file.endsWith( QString( "." ) + qfu( types[i].filter_patterns ) ) )
        {
            psz_selected_module = types[i].module;
            psz_last_playlist_ext = types[i].filter_patterns;
            break;
        }
    }

    /* otherwise apply the selected extension */
    if ( !psz_last_playlist_ext )
    {
        for( size_t i = 0; i < sizeof (types) / sizeof (types[0]); i++)
        {
            if ( selected.startsWith( qfu( vlc_gettext( types[i].filter_name ) ) ) )
            {
                psz_selected_module = types[i].module;
                psz_last_playlist_ext = types[i].filter_patterns;
                /* Fix file extension */
                file = file.append( QString( "." ) + qfu( psz_last_playlist_ext ) );
                break;
            }
        }
    }

    if ( psz_selected_module )
    {
        vlc_playlist_Lock( p_intf->p_sys->p_playlist  );
        vlc_playlist_Export( p_intf->p_sys->p_playlist,
                             qtu( toNativeSeparators( file ) ),
                             psz_selected_module );
        vlc_playlist_Unlock( p_intf->p_sys->p_playlist  );
        getSettings()->setValue( "last-playlist-ext", psz_last_playlist_ext );
    }
}

/****************************************************************************
 * Sout emulation
 ****************************************************************************/

void DialogsProvider::streamingDialog( QWidget *parent,
                                       const QStringList& mrls,
                                       bool b_transcode_only,
                                       QStringList options )
{
    QStringList outputMRLs;

    /* Stream */
    // Does streaming multiple files make sense?  I suppose so, just stream one
    // after the other, but not at the moment.
    if( !b_transcode_only )
    {
        SoutDialog *s = new SoutDialog( parent, p_intf, mrls[0] );
        s->setAttribute( Qt::WA_QuitOnClose, false ); // See #4883
        if( s->exec() == QDialog::Accepted )
        {
            outputMRLs.append(s->getChain());
            delete s;
        }
        else
        {
            delete s; return;
        }
    } else {
    /* Convert */
        ConvertDialog *s = new ConvertDialog( parent, p_intf, mrls );
        s->setAttribute( Qt::WA_QuitOnClose, false ); // See #4883
        if( s->exec() == QDialog::Accepted )
        {
            /* Clear the playlist.  This is because we're going to be populating
               it */
            THEMPL->clear();
            outputMRLs = s->getMrls();
            delete s;
        }
        else
        {
            delete s; return;
        }
    }

    /* Get SoutChain(s) */
    if( !outputMRLs.isEmpty() )
    {
        QVector<vlc::playlist::Media> outputMedias;
        std::transform(outputMRLs.cbegin(), outputMRLs.cend(), std::back_inserter(outputMedias), [&](const QString& mrl) {
            QString title = "Converting " + mrl;
            return vlc::playlist::Media(mrl, title, &options);
        });
        THEMPL->append(outputMedias, true);
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
    input_item_t *p_item = THEMIM->getInput();
    if( !p_item ) return;

    char *path = input_item_GetURI( p_item );
    QUrl url;
    if( path )
    {
        url.setUrl( qfu(path) );
        url = url.adjusted(QUrl::RemoveFilename);
        if (url.scheme() != "file")
            url.clear();
        free(path);
    }

    QStringList qsl = showSimpleOpen( qtr( "Open subtitles..." ),
                                      EXT_FILTER_SUBTITLE,
                                      url );

    foreach( const QString &qsUrl, qsl )
    {

        if ( THEMIM->AddAssociatedMedia( SPU_ES, qsUrl, true, true, false ) )
            msg_Warn( p_intf, "unable to load subtitles from '%s'", qtu( qsUrl ) );
    }
}


/****************************************************************************
 * Menus
 ****************************************************************************/

void DialogsProvider::sendKey( int key )
{
     // translate from a vlc keycode into a Qt sequence
     QKeySequence kseq0( VLCKeyToString( key, true ) );

     if( popupMenu == NULL )
     {
         // make sure at least a non visible popupmenu is available
         popupMenu = VLCMenuBar::PopupMenu( p_intf, false );
         if( unlikely( popupMenu == NULL ) )
             return;
     }

     // test against key accelerators from the popupmenu
     QList<QAction*> actions = popupMenu->findChildren<QAction*>();
     for( int i = 0; i < actions.size(); i++ )
     {
         QAction* action = actions.at(i);
         QKeySequence kseq = action->shortcut();
         if( kseq == kseq0 )
         {
             action->trigger();
             return;
         }
     }

     // forward key to vlc core when not a key accelerator
     var_SetInteger( vlc_object_instance(p_intf), "key-pressed", key );
}
