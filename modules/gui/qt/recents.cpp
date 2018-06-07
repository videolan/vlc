/*****************************************************************************
 * recents.cpp : Recents MRL (menu)
 *****************************************************************************
 * Copyright © 2008-2014 VideoLAN and VLC authors
 * $Id$
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
 *          Jean-baptiste Kempf <jb@videolan.org>
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

#include "qt.hpp"
#include "recents.hpp"
#include "dialogs_provider.hpp"
#include "menus.hpp"
#include "util/qt_dirs.hpp"

#include <QStringList>
#include <QRegExp>
#include <QSignalMapper>

#ifdef _WIN32
    #include <shlobj.h>
    /* typedef enum  {
        SHARD_PIDL              = 0x00000001,
        SHARD_PATHA             = 0x00000002,
        SHARD_PATHW             = 0x00000003,
        SHARD_APPIDINFO         = 0x00000004,
        SHARD_APPIDINFOIDLIST   = 0x00000005,
        SHARD_LINK              = 0x00000006,
        SHARD_APPIDINFOLINK     = 0x00000007,
        SHARD_SHELLITEM         = 0x00000008
    } SHARD; */
    #define SHARD_PATHW 0x00000003

    #include <vlc_charset.h>
#endif

#include <vlc_input_item.h>

RecentsMRL::RecentsMRL( intf_thread_t *_p_intf ) : p_intf( _p_intf )
{
    recents = QStringList();
    times = QStringList();

    signalMapper = new QSignalMapper( this );
    CONNECT( signalMapper,
            mapped(const QString & ),
            this,
            playMRL( const QString & ) );

    /* Load the filter psz */
    char* psz_tmp = var_InheritString( p_intf, "qt-recentplay-filter" );
    if( psz_tmp && *psz_tmp )
        filter = new QRegExp( psz_tmp, Qt::CaseInsensitive );
    else
        filter = NULL;
    free( psz_tmp );

    load();
    isActive = var_InheritBool( p_intf, "qt-recentplay" );
    if( !isActive ) clear();
}

RecentsMRL::~RecentsMRL()
{
    save();
    delete filter;
}

void RecentsMRL::addRecent( const QString &mrl )
{
    if ( !isActive || ( filter && filter->indexIn( mrl ) >= 0 ) )
        return;

#ifdef _WIN32
    /* Add to the Windows 7 default list in taskbar */
    char* path = vlc_uri2path( qtu( mrl ) );
    if( path )
    {
        wchar_t *wmrl = ToWide( path );
        SHAddToRecentDocs( SHARD_PATHW, wmrl );
        free( wmrl );
        free( path );
    }
#endif

    int i_index = recents.indexOf( mrl );
    if( 0 <= i_index )
    {
        /* move to the front */
        recents.move( i_index, 0 );
        times.move( i_index, 0 );
    }
    else
    {
        recents.prepend( mrl );
        times.prepend( "-1" );
        if( recents.count() > RECENTS_LIST_SIZE ) {
            recents.takeLast();
            times.takeLast();
        }
    }
    VLCMenuBar::updateRecents( p_intf );
    save();
}

void RecentsMRL::clear()
{
    if ( recents.isEmpty() )
        return;

    recents.clear();
    times.clear();
    if( isActive ) VLCMenuBar::updateRecents( p_intf );
    save();
}

QStringList RecentsMRL::recentList()
{
    return recents;
}

void RecentsMRL::load()
{
    /* Load from the settings */
    QStringList list = getSettings()->value( "RecentsMRL/list" ).toStringList();
    QStringList list2 = getSettings()->value( "RecentsMRL/times" ).toStringList();

    /* And filter the regexp on the list */
    for( int i = 0; i < list.count(); ++i )
    {
        if ( !filter || filter->indexIn( list.at(i) ) == -1 ) {
            recents.append( list.at(i) );
            times.append( list2.value(i, "-1" ) );
        }
    }
}

void RecentsMRL::save()
{
    getSettings()->setValue( "RecentsMRL/list", recents );
    getSettings()->setValue( "RecentsMRL/times", times );
}

void RecentsMRL::playMRL( const QString &mrl )
{
    Open::openMRL( p_intf, mrl );
}

int RecentsMRL::time( const QString &mrl )
{
    if( !isActive )
        return -1;

    int i_index = recents.indexOf( mrl );
    if( i_index != -1 )
        return times.value(i_index, "-1").toInt();
    else
        return -1;
}

void RecentsMRL::setTime( const QString &mrl, const int64_t time )
{
    int i_index = recents.indexOf( mrl );
    if( i_index != -1 )
        times[i_index] = QString::number( time / 1000 );
}

int Open::openMRL( intf_thread_t *p_intf,
                    const QString &mrl,
                    bool b_start )
{
    return openMRLwithOptions( p_intf, mrl, NULL, b_start );
}

int Open::openMRLwithOptions( intf_thread_t* p_intf,
                     const QString &mrl,
                     QStringList *options,
                     bool b_start,
                     const char *title)
{
    /* Options */
    const char **ppsz_options = NULL;
    int i_options = 0;

    if( options != NULL && options->count() > 0 )
    {
        ppsz_options = new const char *[options->count()];
        for( int j = 0; j < options->count(); j++ ) {
            QString option = colon_unescape( options->at(j) );
            if( !option.isEmpty() ) {
                ppsz_options[i_options] = strdup(qtu(option));
                i_options++;
            }
        }
    }

    /* Add to playlist */
    int i_ret = playlist_AddExt( THEPL, qtu(mrl), title, b_start,
                  i_options, ppsz_options, VLC_INPUT_OPTION_TRUSTED );

    /* Add to recent items, only if played */
    if( i_ret == VLC_SUCCESS && b_start )
        RecentsMRL::getInstance( p_intf )->addRecent( mrl );

    /* Free options */
    if ( ppsz_options != NULL )
    {
        for ( int i = 0; i < i_options; ++i )
            free( (char*)ppsz_options[i] );
        delete[] ppsz_options;
    }
    return i_ret;
}

