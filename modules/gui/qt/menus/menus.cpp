/*****************************************************************************
 * menus.cpp : Qt menus
 *****************************************************************************
 * Copyright © 2006-2011 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
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

/** \todo
 * - Remove static currentGroup
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_intf_strings.h>
#include <vlc_aout.h>                             /* audio_output_t */
#include <vlc_player.h>

#include "menus.hpp"

#include "maininterface/main_interface.hpp"                     /* View modifications */
#include "dialogs/dialogs_provider.hpp"                   /* Dialogs display */
#include "player/player_controller.hpp"                      /* Input Management */
#include "playlist/playlist_controller.hpp"
#include "util/recents.hpp"                            /* Recent Items */
#include "dialogs/extensions/extensions_manager.hpp"                 /* Extensions menu */
#include "dialogs/extended/extended_panels.hpp"
#include "util/varchoicemodel.hpp"

#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QActionGroup>
#include <QSignalMapper>
#include <QStatusBar>

using namespace vlc::playlist;

/*
  This file defines the main menus and the pop-up menu (right-click menu)
  and the systray menu (in that order in the file)

  There are 4 menus that have to be rebuilt everytime there are called:
  Audio, Video, Navigation, view
  4 functions are building those menus: AudioMenu, VideoMenu, NavigMenu, View

  */

QMenu *VLCMenuBar::recentsMenu = NULL;
RendererMenu *VLCMenuBar::rendererMenu = NULL;

/**
 * @brief Add static entries to DP in menus
 **/
QAction *addDPStaticEntry( QMenu *menu,
                       const QString& text,
                       const char *icon,
                       const char *member,
                       const char *shortcut = NULL,
                       QAction::MenuRole role = QAction::NoRole
                       )
{
    QAction *action = NULL;
#ifndef __APPLE__ /* We don't set icons in menus in MacOS X */
    if( !EMPTY_STR( icon ) )
    {
        if( !EMPTY_STR( shortcut ) )
            action = menu->addAction( QIcon( icon ), text, THEDP,
                                      member, qtr( shortcut ) );
        else
            action = menu->addAction( QIcon( icon ), text, THEDP, member );
    }
    else
#endif
    {
        if( !EMPTY_STR( shortcut ) )
            action = menu->addAction( text, THEDP, member, qtr( shortcut ) );
        else
            action = menu->addAction( text, THEDP, member );
    }
#ifdef __APPLE__
    action->setMenuRole( role );
#else
    Q_UNUSED( role );
#endif
    return action;
}

/**
 * @brief Add static entries to MIM in menus
 **/
template<typename Fun>
static QAction* addMIMStaticEntry( intf_thread_t *p_intf,
                            QMenu *menu,
                            const QString& text,
                            const char *icon,
                            Fun member )
{
    QAction *action;
#ifndef __APPLE__ /* We don't set icons in menus in MacOS X */
    if( !EMPTY_STR( icon ) )
    {
        action = menu->addAction( text, THEMIM,  member );
        action->setIcon( QIcon( icon ) );
    }
    else
#endif
    {
        action = menu->addAction( text, THEMIM, member );
    }
    return action;
}

template<typename Fun>
static QAction* addMPLStaticEntry( intf_thread_t *p_intf,
                            QMenu *menu,
                            const QString& text,
                            const char *icon,
                            Fun member )
{
    QAction *action;
#ifndef __APPLE__ /* We don't set icons in menus in MacOS X */
    if( !EMPTY_STR( icon ) )
    {
        action = menu->addAction( text, THEMPL,  member );
        action->setIcon( QIcon( icon ) );
    }
    else
#endif
    {
        action = menu->addAction( text, THEMPL, member );
    }
    return action;
}

/*****************************************************************************
 * All normal menus
 * Simple Code
 *****************************************************************************/

// Static menu
static inline void addMenuToMainbar( QMenu *func, QString title, QMenuBar *bar ) {
    func->setTitle( title );
    bar->addMenu( func);
}
/**
 * Main Menu Bar Creation
 **/
void VLCMenuBar::createMenuBar( MainInterface *mi,
                              intf_thread_t *p_intf )
{
    /* QMainWindows->menuBar()
       gives the QProcess::destroyed timeout issue on Cleanlooks style with
       setDesktopAware set to false */
    QMenuBar *bar = mi->menuBar();

    addMenuToMainbar( FileMenu( p_intf, bar, mi ), qtr( "&Media" ), bar );

    /* Dynamic menus, rebuilt before being showed */
    addMenuToMainbar( NavigMenu( p_intf, bar ), qtr( "P&layback" ), bar);
    addMenuToMainbar( AudioMenu( p_intf, bar ), qtr( "&Audio" ), bar );
    addMenuToMainbar( VideoMenu( p_intf, bar ), qtr( "&Video" ), bar );
    addMenuToMainbar( SubtitleMenu( p_intf, bar ), qtr( "Subti&tle" ), bar );

    addMenuToMainbar( ToolsMenu( p_intf, bar ), qtr( "Tool&s" ), bar );

    /* View menu, a bit different */
    addMenuToMainbar( ViewMenu( p_intf, NULL, mi ), qtr( "V&iew" ), bar );

    addMenuToMainbar( HelpMenu( bar ), qtr( "&Help" ), bar );

}

/**
 * Media ( File ) Menu
 * Opening, streaming and quit
 **/
QMenu *VLCMenuBar::FileMenu( intf_thread_t *p_intf, QWidget *parent, MainInterface *mi )
{
    QMenu *menu = new QMenu( parent );
    QAction *action;

    addDPStaticEntry( menu, qtr( "Open &File..." ),
        ":/type/file-asym.svg", SLOT( simpleOpenDialog() ), "Ctrl+O" );
    addDPStaticEntry( menu, qtr( "&Open Multiple Files..." ),
        ":/type/file-asym.svg", SLOT( openFileDialog() ), "Ctrl+Shift+O" );
    addDPStaticEntry( menu, qtr( I_OP_OPDIR ),
        ":/type/folder-grey.svg", SLOT( PLOpenDir() ), "Ctrl+F" );
    addDPStaticEntry( menu, qtr( "Open &Disc..." ),
        ":/type/disc.svg", SLOT( openDiscDialog() ), "Ctrl+D" );
    addDPStaticEntry( menu, qtr( "Open &Network Stream..." ),
        ":/type/network.svg", SLOT( openNetDialog() ), "Ctrl+N" );
    addDPStaticEntry( menu, qtr( "Open &Capture Device..." ),
        ":/type/capture-card.svg", SLOT( openCaptureDialog() ), "Ctrl+C" );

    addDPStaticEntry( menu, qtr( "Open &Location from clipboard" ),
                      NULL, SLOT( openUrlDialog() ), "Ctrl+V" );

    if( !recentsMenu && var_InheritBool( p_intf, "qt-recentplay" ) )
        recentsMenu = new QMenu( qtr( "Open &Recent Media" ) );

    if( recentsMenu )
    {
        updateRecents( p_intf );
        menu->addMenu( recentsMenu );
    }
    menu->addSeparator();

    addDPStaticEntry( menu, qtr( I_PL_SAVE ), "", SLOT( savePlayingToPlaylist() ),
        "Ctrl+Y" );

#ifdef ENABLE_SOUT
    addDPStaticEntry( menu, qtr( "Conve&rt / Save..." ), "",
        SLOT( openAndTranscodingDialogs() ), "Ctrl+R" );
    addDPStaticEntry( menu, qtr( "&Stream..." ),
        ":/menu/stream.svg", SLOT( openAndStreamingDialogs() ), "Ctrl+S" );
    menu->addSeparator();
#endif

    action = addMPLStaticEntry( p_intf, menu, qtr( "Quit at the end of playlist" ), "",
                               &PlaylistControllerModel::playAndExitChanged );
    action->setCheckable( true );
    action->setChecked( THEMPL->isPlayAndExit() );

    if( mi && mi->getSysTray() )
    {
        action = menu->addAction( qtr( "Close to systray"), mi,
                                 SLOT( toggleUpdateSystrayMenu() ) );
    }

    addDPStaticEntry( menu, qtr( "&Quit" ) ,
        ":/menu/exit.svg", SLOT( quit() ), "Ctrl+Q" );
    return menu;
}

/**
 * Tools, like Media Information, Preferences or Messages
 **/
QMenu *VLCMenuBar::ToolsMenu( intf_thread_t *p_intf, QMenu *menu )
{
    addDPStaticEntry( menu, qtr( "&Effects and Filters"), ":/menu/settings.svg",
            SLOT( extendedDialog() ), "Ctrl+E" );

    addDPStaticEntry( menu, qtr( "&Track Synchronization"), ":/menu/setting.svgs",
            SLOT( synchroDialog() ), "" );

    addDPStaticEntry( menu, qtr( I_MENU_INFO ) , ":/menu/info.svg",
        SLOT( mediaInfoDialog() ), "Ctrl+I" );
    addDPStaticEntry( menu, qtr( I_MENU_CODECINFO ) ,
        ":/menu/info.svg", SLOT( mediaCodecDialog() ), "Ctrl+J" );

#ifdef ENABLE_VLM
    addDPStaticEntry( menu, qtr( I_MENU_VLM ), "", SLOT( vlmDialog() ),
        "Ctrl+Shift+W" );
#endif

    addDPStaticEntry( menu, qtr( "Program Guide" ), "", SLOT( epgDialog() ),
        "" );

    addDPStaticEntry( menu, qtr( I_MENU_MSG ),
        ":/menu/messages.svg", SLOT( messagesDialog() ), "Ctrl+M" );

    addDPStaticEntry( menu, qtr( "Plu&gins and extensions" ),
        "", SLOT( pluginDialog() ) );
    menu->addSeparator();

    if( !p_intf->p_sys->b_isDialogProvider )
        addDPStaticEntry( menu, qtr( "Customi&ze Interface..." ),
            ":/menu/preferences.svg", SLOT( toolbarDialog() ) );

    addDPStaticEntry( menu, qtr( "&Preferences" ),
        ":/menu/preferences.svg", SLOT( prefsDialog() ), "Ctrl+P", QAction::PreferencesRole );

    return menu;
}

/**
 * View Menu
 * Interface modification, load other interfaces, activate Extensions
 * \param current, set to NULL for menu creation, else for menu update
 **/
QMenu *VLCMenuBar::ViewMenu( intf_thread_t *p_intf, QMenu *current, MainInterface *_mi )
{
    QAction *action;
    QMenu *menu;

    MainInterface *mi = _mi ? _mi : p_intf->p_sys->p_mi;
    assert( mi );

    if( !current )
    {
        menu = new QMenu( qtr( "&View" ), mi );
    }
    else
    {
        menu = current;
        //menu->clear();
        //HACK menu->clear() does not delete submenus
        QList<QAction*> actions = menu->actions();
        foreach( QAction *a, actions )
        {
            QMenu *m = a->menu();
            if( a->parent() == menu ) delete a;
            else menu->removeAction( a );
            if( m && m->parent() == menu ) delete m;
        }
    }

    menu->addAction(
#ifndef __APPLE__
            QIcon( ":/menu/playlist_menu.svg" ),
#endif
            qtr( "Play&list" ), mi,
            SLOT( togglePlaylist() ), qtr( "Ctrl+L" ) );

    /* Docked Playlist */
    action = menu->addAction( qtr( "Docked Playlist" ) );
    action->setCheckable( true );
    CONNECT( action, triggered( bool ), mi, dockPlaylist( bool ) );

    menu->addSeparator();

    action = menu->addAction( qtr( "Always on &top" ) );
    action->setCheckable( true );
    action->setChecked( mi->isInterfaceAlwaysOnTop() );
    CONNECT( action, triggered( bool ), mi, setInterfaceAlwaysOnTop( bool ) );

    menu->addSeparator();

    /* Minimal View */
    action = menu->addAction( qtr( "Mi&nimal Interface" ) );
    action->setShortcut( qtr( "Ctrl+H" ) );
    action->setCheckable( true );


    CONNECT( action, triggered( bool ), mi, toggleMinimalView( bool ) );
    CONNECT( mi, minimalViewToggled( bool ), action, setChecked( bool ) );

    /* FullScreen View */
    action = menu->addAction( qtr( "&Fullscreen Interface" ), mi,
            SLOT( toggleInterfaceFullScreen() ), QString( "F11" ) );
    action->setCheckable( true );
    action->setChecked( mi->isInterfaceFullScreen() );
    CONNECT( mi, fullscreenInterfaceToggled( bool ),
             action, setChecked( bool ) );

    /* Advanced Controls */
    action = menu->addAction( qtr( "&Advanced Controls" ), mi,
            SLOT( toggleAdvancedButtons() ) );
    action->setCheckable( true );

    action = menu->addAction( qtr( "Status Bar" ) );
    action->setCheckable( true );
    action->setChecked( mi->statusBar()->isVisible() );
    CONNECT( action, triggered( bool ), mi, setStatusBarVisibility( bool) );
#if 0 /* For Visualisations. Not yet working */
    adv = menu->addAction( qtr( "Visualizations selector" ), mi,
                           SLOT( visual() ) );
    adv->setCheckable( true );
    if( visual_selector_enabled ) adv->setChecked( true );
#endif

    menu->addSeparator();

    InterfacesMenu( p_intf, menu );
    menu->addSeparator();

    /* Extensions */
    ExtensionsMenu( p_intf, menu );

    return menu;
}

/**
 * Interface Sub-Menu, to list extras interface and skins
 **/
QMenu *VLCMenuBar::InterfacesMenu( intf_thread_t *p_intf, QMenu *current )
{
    assert(current);
    VLCVarChoiceModel* model = new VLCVarChoiceModel(VLC_OBJECT(p_intf), "intf-add", current);
    CheckableListMenu* submenu = new CheckableListMenu(qtr("Interfaces"), model, current);
    current->addMenu(submenu);
    return current;
}

/**
 * Extensions menu: populate the current menu with extensions
 **/
void VLCMenuBar::ExtensionsMenu( intf_thread_t *p_intf, QMenu *extMenu )
{
    /* Get ExtensionsManager and load extensions if needed */
    ExtensionsManager *extMgr = ExtensionsManager::getInstance( p_intf );

    if( !var_InheritBool( p_intf, "qt-autoload-extensions")
        && !extMgr->isLoaded() )
    {
        return;
    }

    if( !extMgr->isLoaded() && !extMgr->cannotLoad() )
    {
        extMgr->loadExtensions();
    }

    /* Let the ExtensionsManager build itself the menu */
    extMenu->addSeparator();
    extMgr->menu( extMenu );
}

static inline void VolumeEntries( intf_thread_t *p_intf, QMenu *current )
{
    current->addSeparator();

    current->addAction( QIcon( ":/toolbar/volume-high.svg" ), qtr( "&Increase Volume" ), THEMIM, &PlayerController::setVolumeUp );
    current->addAction( QIcon( ":/toolbar/volume-low.svg" ), qtr( "D&ecrease Volume" ), THEMIM, &PlayerController::setVolumeDown );
    current->addAction( QIcon( ":/toolbar/volume-muted.svg" ), qtr( "&Mute" ), THEMIM, &PlayerController::toggleMuted );
}

/**
 * Main Audio Menu
 **/
QMenu *VLCMenuBar::AudioMenu( intf_thread_t *p_intf, QMenu * current )
{
    if( current->isEmpty() )
    {
        current->addMenu(new CheckableListMenu(qtr( "Audio &Track" ), THEMIM->getAudioTracks(), current));

        QAction *audioDeviceAction = new QAction( qtr( "Audio &Device" ), current );
        QMenu *audioDeviceSubmenu = new QMenu( current );
        audioDeviceAction->setMenu( audioDeviceSubmenu );
        current->addAction( audioDeviceAction );
        connect(audioDeviceSubmenu, &QMenu::aboutToShow, [=]() {
            updateAudioDevice( p_intf, audioDeviceSubmenu );
        });

        current->addMenu( new CheckableListMenu(qtr( "&Stereo Mode" ), THEMIM->getAudioStereoMode(), current) );
        current->addSeparator();

        current->addMenu( new CheckableListMenu(qtr( "&Visualizations" ), THEMIM->getAudioVisualizations(), current) );
        VolumeEntries( p_intf, current );
    }

    return current;
}

/* Subtitles */
QMenu *VLCMenuBar::SubtitleMenu( intf_thread_t *p_intf, QMenu *current, bool b_popup )
{
    if( current->isEmpty() || b_popup )
    {
        addDPStaticEntry( current, qtr( "Add &Subtitle File..." ), "",
                SLOT( loadSubtitlesFile() ) );
        current->addMenu(new CheckableListMenu(qtr( "Sub &Track" ), THEMIM->getSubtitleTracks(), current));
        current->addSeparator();
    }
    return current;
}

/**
 * Main Video Menu
 * Subtitles are part of Video.
 **/
QMenu *VLCMenuBar::VideoMenu( intf_thread_t *p_intf, QMenu *current )
{
    if( current->isEmpty() )
    {
        current->addMenu(new CheckableListMenu(qtr( "Video &Track" ), THEMIM->getVideoTracks(), current));

        current->addSeparator();
        /* Surface modifiers */
        current->addAction(new BooleanPropertyAction(qtr( "&Fullscreen"), THEMIM, "fullscreen", current));
        QAction* action = new BooleanPropertyAction(qtr( "Always Fit &Window"), THEMIM, "autoscale", current);
        action->setEnabled( THEMIM->hasVideoOutput() );
        connect(THEMIM, &PlayerController::hasVideoOutputChanged, action, &QAction::setEnabled);
        current->addAction(action);
        current->addAction(new BooleanPropertyAction(qtr( "Set as Wall&paper"), THEMIM, "wallpaperMode", current));

        current->addSeparator();
        /* Size modifiers */
        current->addMenu( new CheckableListMenu(qtr( "&Zoom" ), THEMIM->getZoom(), current) );
        current->addMenu( new CheckableListMenu(qtr( "&Aspect Ratio" ), THEMIM->getAspectRatio(), current) );
        current->addMenu( new CheckableListMenu(qtr( "&Crop" ), THEMIM->getCrop(), current) );

        current->addSeparator();
        /* Rendering modifiers */
        current->addMenu( new CheckableListMenu(qtr( "&Deinterlace" ), THEMIM->getDeinterlace(), current) );
        current->addMenu( new CheckableListMenu(qtr( "&Deinterlace mode" ), THEMIM->getDeinterlaceMode(), current) );

        current->addSeparator();
        /* Other actions */
        QAction* snapshotAction = new QAction(qtr( "Take &Snapshot" ), current);
        connect(snapshotAction, &QAction::triggered, THEMIM, &PlayerController::snapshot);
        current->addAction(snapshotAction);
    }

    return current;
}

/**
 * Navigation Menu
 * For DVD, MP4, MOV and other chapter based format
 **/
QMenu *VLCMenuBar::NavigMenu( intf_thread_t *p_intf, QMenu *menu )
{
    QAction *action;
    QMenu *submenu;

    menu->addMenu( new CheckableListMenu( qtr( "T&itle" ), THEMIM->getTitles(), menu) );
    submenu = new CheckableListMenu( qtr( "&Chapter" ), THEMIM->getChapters(), menu );
    submenu->setTearOffEnabled( true );
    menu->addMenu( submenu );
    menu->addMenu( new CheckableListMenu( qtr("&Program") , THEMIM->getPrograms(), menu) );

    submenu = new QMenu( qtr( I_MENU_BOOKMARK ), menu );
    submenu->setTearOffEnabled( true );
    addDPStaticEntry( submenu, qtr( "&Manage" ), "",
                      SLOT( bookmarksDialog() ), "Ctrl+B" );
    submenu->addSeparator();
    action = menu->addMenu( submenu );
    action->setData( "bookmark" );

    menu->addSeparator();

    if ( !VLCMenuBar::rendererMenu )
        VLCMenuBar::rendererMenu = new RendererMenu( NULL, p_intf );

    menu->addMenu( VLCMenuBar::rendererMenu );
    menu->addSeparator();


    PopupMenuControlEntries( menu, p_intf );

    return RebuildNavigMenu( p_intf, menu );
}

QMenu *VLCMenuBar::RebuildNavigMenu( intf_thread_t *p_intf, QMenu *menu )
{
    QAction* action;

#define ADD_ACTION( title, slot, visibleSignal, visible ) \
    action = menu->addAction( title, THEMIM,  slot ); \
    if (! visible)\
        action->setVisible(false); \
    connect( THEMIM, visibleSignal, action, &QAction::setVisible );

    ADD_ACTION( qtr("Previous Title"), &PlayerController::titlePrev, &PlayerController::hasTitlesChanged, THEMIM->hasTitles() );
    ADD_ACTION( qtr("Next Title"), &PlayerController::titleNext, &PlayerController::hasTitlesChanged, THEMIM->hasTitles() );
    ADD_ACTION( qtr("Previous Chapter"), &PlayerController::chapterPrev, &PlayerController::hasChaptersChanged, THEMIM->hasChapters() );
    ADD_ACTION( qtr("Next Chapter"), &PlayerController::chapterNext, &PlayerController::hasChaptersChanged, THEMIM->hasChapters() );

#undef ADD_ACTION

    PopupMenuPlaylistEntries( menu, p_intf );

    return menu;
}

/**
 * Help/About Menu
**/
QMenu *VLCMenuBar::HelpMenu( QWidget *parent )
{
    QMenu *menu = new QMenu( parent );
    addDPStaticEntry( menu, qtr( "&Help" ) ,
        ":/menu/help.svg", SLOT( helpDialog() ), "F1" );
#ifdef UPDATE_CHECK
    addDPStaticEntry( menu, qtr( "Check for &Updates..." ) , "",
                      SLOT( updateDialog() ) );
#endif
    menu->addSeparator();
    addDPStaticEntry( menu, qtr( I_MENU_ABOUT ), ":/menu/info.svg",
            SLOT( aboutDialog() ), "Shift+F1", QAction::AboutRole );
    return menu;
}

/*****************************************************************************
 * Popup menus - Right Click menus                                           *
 *****************************************************************************/
#define POPUP_BOILERPLATE \
    QMenu* menu;

#define CREATE_POPUP \
    menu = new QMenu(); \
    if( show ) \
        menu->popup( QCursor::pos() ); \

void VLCMenuBar::PopupMenuPlaylistEntries( QMenu *menu, intf_thread_t *p_intf )
{
    QAction *action;
    bool hasInput = THEMIM->hasInput();

    /* Play or Pause action and icon */
    if( !hasInput || THEMIM->getPlayingState() != PlayerController::PLAYING_STATE_PLAYING )
    {
        action = menu->addAction( qtr( "&Play" ), [p_intf]() {
            if ( THEMPL->isEmpty() )
                THEDP->openFileDialog();
            else
                THEMPL->togglePlayPause();
        });
#ifndef __APPLE__ /* No icons in menus in Mac */
        action->setIcon( QIcon( ":/toolbar/play_b.svg" ) );
#endif
    }
    else
    {
        action = addMPLStaticEntry( p_intf, menu, qtr( "Pause" ),
                ":/toolbar/pause_b.svg", &PlaylistControllerModel::togglePlayPause );
    }

    /* Stop */
    action = addMPLStaticEntry( p_intf, menu, qtr( "&Stop" ),
            ":/toolbar/stop_b.svg", &PlaylistControllerModel::stop );
    if( !hasInput )
        action->setEnabled( false );

    /* Next / Previous */
    bool bPlaylistEmpty = THEMPL->isEmpty();
    QAction* previousAction = addMPLStaticEntry( p_intf, menu, qtr( "Pre&vious" ),
            ":/toolbar/previous_b.svg", &PlaylistControllerModel::prev );
    previousAction->setEnabled( !bPlaylistEmpty );
    connect( THEMPL, &PlaylistControllerModel::isEmptyChanged, previousAction, &QAction::setDisabled);

    QAction* nextAction = addMPLStaticEntry( p_intf, menu, qtr( "Ne&xt" ),
            ":/toolbar/next_b.svg", &PlaylistControllerModel::next );
    nextAction->setEnabled( !bPlaylistEmpty );
    connect( THEMPL, &PlaylistControllerModel::isEmptyChanged, nextAction, &QAction::setDisabled);

    action = menu->addAction( qtr( "Record" ), THEMIM, &PlayerController::toggleRecord );
    action->setIcon( QIcon( ":/toolbar/record.svg" ) );
    if( !hasInput )
        action->setEnabled( false );
    menu->addSeparator();
}

void VLCMenuBar::PopupMenuControlEntries( QMenu *menu, intf_thread_t *p_intf,
                                        bool b_normal )
{
    QAction *action;
    QMenu *rateMenu = new QMenu( qtr( "Sp&eed" ), menu );
    rateMenu->setTearOffEnabled( true );

    if( b_normal )
    {
        /* Faster/Slower */
        action = rateMenu->addAction( qtr( "&Faster" ), THEMIM,
                                  &PlayerController::faster );
#ifndef __APPLE__ /* No icons in menus in Mac */
        action->setIcon( QIcon( ":/toolbar/faster2.svg") );
#endif
    }

    action = rateMenu->addAction( QIcon( ":/toolbar/faster2.svg" ), qtr( "Faster (fine)" ), THEMIM,
                              &PlayerController::littlefaster );

    action = rateMenu->addAction( qtr( "N&ormal Speed" ), THEMIM,
                              &PlayerController::normalRate );

    action = rateMenu->addAction( QIcon( ":/toolbar/slower2.svg" ), qtr( "Slower (fine)" ), THEMIM,
                              &PlayerController::littleslower );

    if( b_normal )
    {
        action = rateMenu->addAction( qtr( "Slo&wer" ), THEMIM,
                                  &PlayerController::slower );
#ifndef __APPLE__ /* No icons in menus in Mac */
        action->setIcon( QIcon( ":/toolbar/slower2.svg") );
#endif
    }

    action = menu->addMenu( rateMenu );

    menu->addSeparator();

    if( !b_normal ) return;

    action = menu->addAction( qtr( "&Jump Forward" ), THEMIM,
             &PlayerController::jumpFwd );
#ifndef __APPLE__ /* No icons in menus in Mac */
    action->setIcon( QIcon( ":/toolbar/skip_fw.svg") );
#endif

    action = menu->addAction( qtr( "Jump Bac&kward" ), THEMIM,
             &PlayerController::jumpBwd );
#ifndef __APPLE__ /* No icons in menus in Mac */
    action->setIcon( QIcon( ":/toolbar/skip_back.svg") );
#endif

    action = menu->addAction( qtr( I_MENU_GOTOTIME ), THEDP, &DialogsProvider::gotoTimeDialog, qtr( "Ctrl+T" ) );

    menu->addSeparator();
}

void VLCMenuBar::PopupMenuStaticEntries( QMenu *menu )
{
    QMenu *openmenu = new QMenu( qtr( "Open Media" ), menu );
    addDPStaticEntry( openmenu, qtr( I_OP_OPF ),
        ":/type/file-asym.svg", SLOT( openFileDialog() ) );
    addDPStaticEntry( openmenu, qtr( I_OP_OPDIR ),
        ":/type/folder-grey.svg", SLOT( PLOpenDir() ) );
    addDPStaticEntry( openmenu, qtr( "Open &Disc..." ),
        ":/type/disc.svg", SLOT( openDiscDialog() ) );
    addDPStaticEntry( openmenu, qtr( "Open &Network..." ),
        ":/type/network.svg", SLOT( openNetDialog() ) );
    addDPStaticEntry( openmenu, qtr( "Open &Capture Device..." ),
        ":/type/capture-card.svg", SLOT( openCaptureDialog() ) );
    menu->addMenu( openmenu );

    menu->addSeparator();
#if 0
    QMenu *helpmenu = HelpMenu( menu );
    helpmenu->setTitle( qtr( "Help" ) );
    menu->addMenu( helpmenu );
#endif

    addDPStaticEntry( menu, qtr( "Quit" ), ":/menu/exit.svg",
                      SLOT( quit() ), "Ctrl+Q", QAction::QuitRole );
}

/* Video Tracks and Subtitles tracks */
QMenu* VLCMenuBar::VideoPopupMenu( intf_thread_t *p_intf, bool show )
{
    QMenu* menu = new QMenu();
    VideoMenu(p_intf, menu);
    if( show )
        menu->popup( QCursor::pos() );
    return menu;
}

/* Audio Tracks */
QMenu* VLCMenuBar::AudioPopupMenu( intf_thread_t *p_intf, bool show )
{
    QMenu* menu = new QMenu();
    AudioMenu(p_intf, menu);
    if( show )
        menu->popup( QCursor::pos() );
    return menu;
}

/* Navigation stuff, and general menus ( open ), used only for skins */
QMenu* VLCMenuBar::MiscPopupMenu( intf_thread_t *p_intf, bool show )
{
    QMenu* menu = new QMenu();

    menu->addSeparator();
    PopupMenuPlaylistEntries( menu, p_intf );

    menu->addSeparator();
    PopupMenuControlEntries( menu, p_intf );

    menu->addSeparator();
    PopupMenuStaticEntries( menu );

    if( show )
        menu->popup( QCursor::pos() );
    return menu;
}

/* Main Menu that sticks everything together  */
QMenu* VLCMenuBar::PopupMenu( intf_thread_t *p_intf, bool show )
{
    /* */
    QMenu* menu = new QMenu();
    input_item_t* p_input = THEMIM->getInput();
    QAction *action;
    bool b_isFullscreen = false;
    MainInterface *mi = p_intf->p_sys->p_mi;

    PopupMenuPlaylistEntries( menu, p_intf );
    menu->addSeparator();

    if( p_input )
    {
        QMenu *submenu;
        PlayerController::VoutPtr p_vout = THEMIM->getVout();

        /* Add a fullscreen switch button, since it is the most used function */
        if( p_vout )
        {
            b_isFullscreen = THEMIM->isFullscreen();
            if (b_isFullscreen)
                menu->addAction(new BooleanPropertyAction(qtr( "Leave Fullscreen" ), THEMIM, "fullscreen", menu) );
            else
                menu->addAction(new BooleanPropertyAction(qtr( "&Fullscreen" ), THEMIM, "fullscreen", menu) );

            menu->addSeparator();
        }

        /* Input menu */

        /* Audio menu */
        submenu = new QMenu( menu );
        action = menu->addMenu( AudioMenu( p_intf, submenu ) );
        action->setText( qtr( "&Audio" ) );
        if( action->menu()->isEmpty() )
            action->setEnabled( false );

        /* Video menu */
        submenu = new QMenu( menu );
        action = menu->addMenu( VideoMenu( p_intf, submenu ) );
        action->setText( qtr( "&Video" ) );
        if( action->menu()->isEmpty() )
            action->setEnabled( false );

        /* Subtitles menu */
        submenu = new QMenu( menu );
        action = menu->addMenu( SubtitleMenu( p_intf, submenu, true ) );
        action->setText( qtr( "Subti&tle") );

        /* Playback menu for chapters */
        submenu = new QMenu( menu );
        action = menu->addMenu( NavigMenu( p_intf, submenu ) );
        action->setText( qtr( "&Playback" ) );
        if( action->menu()->isEmpty() )
            action->setEnabled( false );
    }

    menu->addSeparator();

    /* Add some special entries for windowed mode: Interface Menu */
    if( !b_isFullscreen )
    {
        QMenu *submenu = new QMenu( qtr( "Tool&s" ), menu );
        /*QMenu *tools =*/ ToolsMenu( p_intf, submenu );
        submenu->addSeparator();

        if( mi )
        {
            QMenu* viewMenu = ViewMenu( p_intf, NULL, mi );
            viewMenu->setTitle( qtr( "V&iew" ) );
            submenu->addMenu( viewMenu );
        }

        /* In skins interface, append some items */
        if( p_intf->p_sys->b_isDialogProvider )
        {
            vlc_object_t* p_object = vlc_object_parent(p_intf);
            submenu->setTitle( qtr( "Interface" ) );

            /* Open skin dialog box */
            if (var_Type(p_object, "intf-skins-interactive") & VLC_VAR_ISCOMMAND)
            {
                QAction* openSkinAction = new QAction(qtr("Open skin..."));
                openSkinAction->setShortcut( QKeySequence( "Ctrl+Shift+S" ));
                connect(openSkinAction, &QAction::triggered, [=]() {
                    var_TriggerCallback(p_object, "intf-skins-interactive");
                });
                submenu->addAction(openSkinAction);
            }

            VLCVarChoiceModel* skinmodel = new VLCVarChoiceModel(p_object, "intf-skins", submenu);
            CheckableListMenu* skinsubmenu = new CheckableListMenu(qtr("Interface"), skinmodel, submenu);
            submenu->addMenu(skinsubmenu);

            submenu->addSeparator();

            /* list of extensions */
            ExtensionsMenu( p_intf, submenu );
        }

        menu->addMenu( submenu );
    }

    /* Static entries for ending, like open */
    if( p_intf->p_sys->b_isDialogProvider )
    {
        QMenu *openmenu = FileMenu( p_intf, menu );
        openmenu->setTitle( qtr( "Open Media" ) );
        menu->addMenu( openmenu );

        menu->addSeparator();

        QMenu *helpmenu = HelpMenu( menu );
        helpmenu->setTitle( qtr( "Help" ) );
        menu->addMenu( helpmenu );

        addDPStaticEntry( menu, qtr( "Quit" ), ":/menu/exit.svg",
                          SLOT( quit() ), "Ctrl+Q", QAction::QuitRole );
    }
    else
        PopupMenuStaticEntries( menu );

    if( show )
        menu->popup( QCursor::pos() );
    return menu;
}

#undef CREATE_POPUP
#undef POPUP_BOILERPLATE
#undef BAR_DADD

/************************************************************************
 * Systray Menu                                                         *
 ************************************************************************/

void VLCMenuBar::updateSystrayMenu( MainInterface *mi,
                                  intf_thread_t *p_intf,
                                  bool b_force_visible )
{
    /* Get the systray menu and clean it */
    QMenu *sysMenu = mi->getSysTrayMenu();
    sysMenu->clear();

#ifndef Q_OS_MAC
    /* Hide / Show VLC and cone */
    if( mi->isVisible() || b_force_visible )
    {
        sysMenu->addAction( QIcon( ":/logo/vlc16.png" ),
                            qtr( "&Hide VLC media player in taskbar" ), mi,
                            SLOT( hideUpdateSystrayMenu() ) );
    }
    else
    {
        sysMenu->addAction( QIcon( ":/logo/vlc16.png" ),
                            qtr( "Sho&w VLC media player" ), mi,
                            SLOT( showUpdateSystrayMenu() ) );
    }
    sysMenu->addSeparator();
#endif

    PopupMenuPlaylistEntries( sysMenu, p_intf );
    PopupMenuControlEntries( sysMenu, p_intf, false );

    VolumeEntries( p_intf, sysMenu );
    sysMenu->addSeparator();
    addDPStaticEntry( sysMenu, qtr( "&Open Media" ),
            ":/type/file-wide.svg", SLOT( openFileDialog() ) );
    addDPStaticEntry( sysMenu, qtr( "&Quit" ) ,
            ":/menu/exit.svg", SLOT( quit() ) );

    /* Set the menu */
    mi->getSysTray()->setContextMenu( sysMenu );
}



/*****************************************************************************
 * Private methods.
 *****************************************************************************/

void VLCMenuBar::updateAudioDevice( intf_thread_t * p_intf, QMenu *current )
{
    char **ids, **names;
    char *selected;

    if( !current )
        return;

    current->clear();
    PlayerController::AoutPtr aout = THEMIM->getAout();
    if (!aout)
        return;

    int i_result = aout_DevicesList( aout.get(), &ids, &names);
    if( i_result < 0 )
        return;

    selected = aout_DeviceGet( aout.get() );

    QActionGroup *actionGroup = new QActionGroup(current);
    QAction *action;

    for( int i = 0; i < i_result; i++ )
    {
        action = new QAction( qfue( names[i] ), actionGroup );
        action->setData( ids[i] );
        action->setCheckable( true );
        if( (selected && !strcmp( ids[i], selected ) ) ||
            (selected == NULL && ids[i] && ids[i][0] == '\0' ) )
            action->setChecked( true );
        actionGroup->addAction( action );
        current->addAction( action );
        connect(action, &QAction::triggered, THEMIM->menusAudioMapper, QOverload<>::of(&QSignalMapper::map));
        THEMIM->menusAudioMapper->setMapping(action, ids[i]);
        free( ids[i] );
        free( names[i] );
    }
    free( ids );
    free( names );
    free( selected );
}

void VLCMenuBar::updateRecents( intf_thread_t *p_intf )
{
    if( recentsMenu )
    {
        QAction* action;
        RecentsMRL* rmrl = RecentsMRL::getInstance( p_intf );
        QStringList l = rmrl->recentList();

        recentsMenu->clear();

        if( !l.count() )
        {
            recentsMenu->setEnabled( false );
        }
        else
        {
            for( int i = 0; i < __MIN( l.count(), 10) ; ++i )
            {
                QString mrl = l.at( i );
                char *psz = vlc_uri_decode_duplicate( qtu( mrl ) );
                QString text = qfu( psz );

                text.replace("&", "&&");
#ifdef _WIN32
# define FILE_SCHEME "file:///"
#else
# define FILE_SCHEME "file://"
#endif
                if ( text.startsWith( FILE_SCHEME ) )
                    text.remove( 0, strlen( FILE_SCHEME ) );
#undef FILE_SCHEME

                free( psz );
                action = recentsMenu->addAction(
                        QString( i < 9 ? "&%1: ": "%1: " ).arg( i + 1 ) +
                            QApplication::fontMetrics().elidedText( text,
                                                          Qt::ElideLeft, 400 ),
                        rmrl->signalMapper, SLOT( map() ),
                        i < 9 ? QString( "Ctrl+%1" ).arg( i + 1 ) : "" );
                rmrl->signalMapper->setMapping( action, l.at( i ) );
            }

            recentsMenu->addSeparator();
            recentsMenu->addAction( qtr("&Clear"), rmrl, SLOT( clear() ) );
            recentsMenu->setEnabled( true );
        }
    }
}
