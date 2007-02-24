/*****************************************************************************
 * simple_preferences.cpp : "Simple preferences"
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: preferences.cpp 16348 2006-08-25 21:10:10Z zorglub $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Antoine Cellerier <dionoea@videolan.org>
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

#include <QListWidget>
#include <QListWidgetItem>
#include <QString>
#include <QFont>

#include "components/simple_preferences.hpp"
#include "components/preferences_widgets.hpp"
#include "qt4.hpp"

#include <vlc_config_cat.h>
#include <assert.h>

#include "pixmaps/advanced_50x50.xpm"
#include "pixmaps/audio_50x50.xpm"
#include "pixmaps/input_and_codecs_50x50.xpm"
#include "pixmaps/interface_50x50.xpm"
//#include "pixmaps/playlist_50x50.xpm"
#include "pixmaps/subtitles_50x50.xpm"
#include "pixmaps/video_50x50.xpm"

#include "ui/sprefs_audio.h"
#include "ui/sprefs_input.h"
#include "ui/sprefs_video.h"
#include "ui/sprefs_subtitles.h"
//#include "ui/sprefs_playlist.h"
#include "ui/sprefs_interface.h"

#define ITEM_HEIGHT 50

/*********************************************************************
 * The List of categories
 *********************************************************************/
SPrefsCatList::SPrefsCatList( intf_thread_t *_p_intf, QWidget *_parent ) :
                                  QListWidget( _parent ), p_intf( _p_intf )
{
    setIconSize( QSize( ITEM_HEIGHT,ITEM_HEIGHT ) );
    setAlternatingRowColors( true );

#define ADD_CATEGORY( id, label, icon )                             \
    addItem( label );                                               \
    item( id )->setIcon( QIcon( QPixmap( icon ) ) );                \
    item( id )->setData( Qt::UserRole, qVariantFromValue( (int)id ) );

    ADD_CATEGORY( SPrefsVideo, qtr("Video"), video_50x50_xpm );
    ADD_CATEGORY( SPrefsAudio, qtr("Audio"), audio_50x50_xpm );
    ADD_CATEGORY( SPrefsInputAndCodecs, qtr("Input and Codecs"),
                  input_and_codecs_50x50_xpm );
//    ADD_CATEGORY( SPrefsPlaylist, qtr("Playlist"), playlist_50x50_xpm );
    ADD_CATEGORY( SPrefsInterface, qtr("Interface"), interface_50x50_xpm );
    ADD_CATEGORY( SPrefsSubtitles, qtr("Subtitles"), subtitles_50x50_xpm );
    ADD_CATEGORY( SPrefsAdvanced, qtr("Advanced"), advanced_50x50_xpm );

    setCurrentRow( SPrefsInterface );
}

/*********************************************************************
 * The Panels
 *********************************************************************/
SPrefsPanel::SPrefsPanel( intf_thread_t *_p_intf, QWidget *_parent,
                          int number ) : QWidget( _parent ), p_intf( _p_intf )
{
    module_config_t *p_config;
    ConfigControl *control;

#define CONFIG_GENERIC( option, type, label, qcontrol )                   \
            p_config =  config_FindConfig( VLC_OBJECT(p_intf), option );  \
            if( p_config )                                                \
            {                                                             \
                control =  new type ## ConfigControl( VLC_OBJECT(p_intf), \
                           p_config, label, ui.qcontrol, false );         \
                controls.append( control );                               \
            }

#define START_SPREFS_CAT( name )    \
        case SPrefs ## name:        \
        {                           \
            Ui::SPrefs ## name ui;  \
            ui.setupUi( this );

#define END_SPREFS_CAT      \
            break;          \
        }


    switch( number )
    {
        START_SPREFS_CAT( Video );

            CONFIG_GENERIC( "video", Bool, NULL, enableVideo );

            CONFIG_GENERIC( "fullscreen", Bool, NULL, fullscreen );
            CONFIG_GENERIC( "overlay", Bool, NULL, overlay );
            CONFIG_GENERIC( "video-on-top", Bool, NULL, alwaysOnTop );
            CONFIG_GENERIC( "video-deco", Bool, NULL, windowDecorations );

            CONFIG_GENERIC( "vout", Module, NULL, outputModule );

            CONFIG_GENERIC( "snapshot-path", String, NULL,
            snapshotsDirectory ); /* FIXME -> use file instead of string */
            CONFIG_GENERIC( "snapshot-prefix", String, NULL, snapshotsPrefix );
            CONFIG_GENERIC( "snapshot-sequential", Bool, NULL,
                            snapshotsSequentialNumbering );
            CONFIG_GENERIC( "snapshot-format", StringList, NULL,
                            snapshotsFormat );

        END_SPREFS_CAT;

        START_SPREFS_CAT( Audio );

            CONFIG_GENERIC( "audio", Bool, NULL, enableAudio );

        END_SPREFS_CAT;

        START_SPREFS_CAT( InputAndCodecs );


        END_SPREFS_CAT;

/*        START_SPREFS_CAT( Playlist );
        END_SPREFS_CAT;*/

        START_SPREFS_CAT( Interface );

            CONFIG_GENERIC( "language", StringList, NULL, language );

        END_SPREFS_CAT;

        START_SPREFS_CAT( Subtitles );

            CONFIG_GENERIC( "subsdec-encoding", StringList, NULL, encoding );
            CONFIG_GENERIC( "sub-language", String, NULL, preferredLanguage );
            CONFIG_GENERIC( "freetype-font", String, NULL, font ); /* FIXME -> use file instead of string */
            CONFIG_GENERIC( "freetype-color", IntegerList, NULL, fontColor );
            CONFIG_GENERIC( "freetype-rel-fontsize", IntegerList, NULL,
                            fontSize );
            CONFIG_GENERIC( "freetype-effect", IntegerList, NULL, effect );

        END_SPREFS_CAT;

        case SPrefsAdvanced: break;
    }
}

void SPrefsPanel::apply()
{
    QList<ConfigControl *>::Iterator i;
    for( i = controls.begin() ; i != controls.end() ; i++ )
    {
        ConfigControl *c = qobject_cast<ConfigControl *>(*i);
        c->doApply( p_intf );
    }
}

void SPrefsPanel::clean()
{}
