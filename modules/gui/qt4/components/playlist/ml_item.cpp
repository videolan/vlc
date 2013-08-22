/*****************************************************************************
 * ml_item.cpp: the SQL media library's result item
 *****************************************************************************
 * Copyright (C) 2008-2011 the VideoLAN Team and AUTHORS
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *          Rémi Duraffort <ivoire@videolan.org>
 *          Adrien Maglo <magsoft@videolan.org>
 *          Srikanth Raju <srikiraju#gmail#com>
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
# include <config.h>
#endif

#ifdef SQL_MEDIA_LIBRARY

#include <QDateTime>
#include <QUrl>
#include <QFileInfo>
#include "ml_item.hpp"
#include <assert.h>


/**
 * @brief Compare the attribute 'meta' between medias a and b.
 * @param a
 * @param b
 * @param meta
 * @note If a->meta < b->meta, return -1
 *       If a->meta == b->meta, return 0
 *       If a->meta > b->meta, return +1
 * @note If a->meta == NULL and b->meta != NULL (strings), then b>a
 */
static int compareMeta( ml_media_t *a, ml_media_t *b, ml_select_e meta )
{
    bool i_ret = 0;
#   define scomp(c) i_ret = ((a->c&&b->c&&*a->c&&*b->c) ?\
                     strcasecmp(a->c,b->c) : \
                     (a->c&&*a->c?-1:(b->c&&*b->c?1:0))); break;
#   define icomp(c) i_ret = (a->c<b->c?-1:(a->c==b->c?0:1)); break;
    if ( a == b ) return 0;
    vlc_mutex_lock( &a->lock );
    vlc_mutex_lock( &b->lock );
    switch( meta )
    {
    case ML_ALBUM: scomp( psz_album );
    case ML_ALBUM_ID: icomp( i_album_id );
        //case ML_ARTIST: scomp( psz_artist );
        //case ML_ARTIST_ID: icomp( i_artist_id );
    case ML_COVER: scomp( psz_cover );
    case ML_DURATION: icomp( i_duration );
    case ML_EXTRA: scomp( psz_extra );
    case ML_GENRE: scomp( psz_genre );
    case ML_ID: icomp( i_id );
    case ML_LAST_PLAYED: icomp( i_last_played );
    case ML_ORIGINAL_TITLE: scomp( psz_orig_title );
    case ML_PLAYED_COUNT: icomp( i_played_count );
        // case ML_ROLE:  0;
    case ML_SCORE: icomp( i_score );
    case ML_TITLE: scomp( psz_title );
    case ML_TRACK_NUMBER: icomp( i_track_number );
    case ML_TYPE: icomp( i_type );
    case ML_URI: scomp( psz_uri );
    case ML_VOTE: icomp( i_vote );
    case ML_YEAR: icomp( i_year );
    default:
        break;
    }
#   undef scomp
#   undef icomp
    vlc_mutex_unlock( &a->lock );
    vlc_mutex_unlock( &b->lock );
    return i_ret;
}


MLItem::MLItem( intf_thread_t* _p_intf,
                            ml_media_t *p_media,
                            MLItem *p_parent )
{
    parentItem = p_parent;
    if( p_media )
        ml_gc_incref( p_media );
    media = p_media;
    p_ml = ml_Get( _p_intf );
}

MLItem::~MLItem()
{
    // Free private data
    if( this->media )
        ml_gc_decref( this->media );
    if( !children.isEmpty() )
        clearChildren();
}

AbstractPLItem* MLItem::child( int row ) const
{
    if( row < 0 || row >= childCount() ) return NULL;
    else return children.at( row );
}

input_item_t* MLItem::inputItem()
{
    return ml_CreateInputItem( p_ml,  id( MLMEDIA_ID ) );
}

/**
 * @brief Get a QVariant representing the data on a column
 * @param column
 * @return The QVariant may be formed of a int, QString
 *         Use toString() to print it on the screen, except for pixmaps
 */
QVariant MLItem::data( ml_select_e columntype ) const
{
    ml_person_t *p_people = NULL, *p_person = NULL;
    QVariant ret;
    QString temp;

#define sget(a) if(media->a) ret = qfu(media->a);
#define iget(a) if(media->a) ret = QVariant(media->a);

    vlc_mutex_lock( &media->lock );

    switch( columntype )
    {
        case ML_ALBUM:
            sget( psz_album );
            break;
        case ML_ALBUM_ID:
            iget( i_album_id );
            break;
        case ML_ARTIST:
            vlc_mutex_unlock( &media->lock );
            p_people = ml_GetPersonsFromMedia( p_ml, media, ML_PERSON_ARTIST );
            vlc_mutex_lock( &media->lock );
            p_person = p_people;
            while( p_person )
            {
                if( !EMPTY_STR( p_person->psz_name ) )
                {
                    temp.isEmpty() ? temp = qfu( p_person->psz_name ) :
                        temp.append( "," ).append( qfu( p_person->psz_name ) );
                }
                p_person = p_person->p_next;
            }
            ml_FreePeople( p_people );
            ret = temp;
            break;
        case ML_COVER:
            sget( psz_cover );
            break;
        case ML_DURATION:
            if ( media->i_duration )
                ret = QTime().addSecs( media->i_duration/1000000 ).toString( "HH:mm:ss" );
            break;
        case ML_EXTRA:
            sget( psz_extra );
            break;
        case ML_GENRE:
            sget( psz_genre );
            break;
        case ML_ID:
            iget( i_id );
            break;
        case ML_LAST_PLAYED:
            if( media->i_last_played > 0 )
            {
                QDateTime time( QDate(1970,1,1) );
                ret = time.addSecs( qint64( media->i_last_played ) );
            }
            break;
        case ML_ORIGINAL_TITLE:
            sget( psz_orig_title );
            break;
        case ML_PLAYED_COUNT:
            iget( i_played_count );
            break;
        // case ML_ROLE: return qtr( "Role" );
        case ML_SCORE:
            if ( media->i_score ) iget( i_score );
            break;
        case ML_TITLE:
            temp = qfu( media->psz_title );
            /* If no title, return filename */
            if( temp.isEmpty() )
            {
                vlc_mutex_unlock( &media->lock );
                QUrl uri = getURI();
                vlc_mutex_lock( &media->lock );
                if ( uri.scheme() != "file" )
                {
                    ret = QUrl::fromPercentEncoding( uri.toString().toUtf8() );
                }
                else
                {
                    QFileInfo p_file( uri.toLocalFile() );
                    ret = p_file.fileName().isEmpty() ? p_file.absoluteFilePath()
                        : p_file.fileName();
                }
            } else {
                ret = temp;
            }
            break;
        case ML_TRACK_NUMBER:
            if ( media->i_track_number ) iget( i_track_number );
            break;
        case ML_TYPE:
            if( media->i_type & ML_AUDIO )
                temp = "Audio";
            if( media->i_type & ML_VIDEO )
                temp = "Video";
            if( media->i_type & ML_STREAM )
            {
                if( temp.isEmpty() ) temp = "Stream";
                else temp += " stream";
            }
            if( media->i_type & ML_REMOVABLE )
            {
                if( temp.isEmpty() ) temp = "Removable media";
                else temp += " (removable media)";
            }
            if( media->i_type & ML_NODE )
            {
                if( temp.isEmpty() ) temp = "Playlist";
                else temp += " (Playlist)";
            }
            if( temp.isEmpty() )
                temp = qtr( "Unknown" );
            ret = temp;
            break;
        case ML_URI:
            sget( psz_uri );
            break;
        case ML_VOTE:
            if ( media->i_vote ) iget( i_vote );
            break;
        case ML_YEAR:
            if ( media->i_year ) iget( i_year );
            break;
        default:
            break;
    }

    vlc_mutex_unlock( &media->lock );

#undef sget
#undef iget
    return ret;
}

bool MLItem::setData( ml_select_e meta, const QVariant &data )
{
#   define setmeta(a) ml_LockMedia(media); free(media->a);                  \
    media->a = strdup( qtu(data.toString()) ); ml_UnlockMedia( media );     \
    goto update;
    switch( meta )
    {
        /* String values */
        case ML_ALBUM: setmeta( psz_album );
        case ML_ARTIST: ml_DeletePersonTypeFromMedia( media, ML_PERSON_ARTIST );
                        ml_CreateAppendPersonAdv( &media->p_people,
                                ML_PERSON_ARTIST, (char*)qtu(data.toString()), 0 );
                        return ml_UpdateSimple( p_ml, ML_MEDIA, NULL, id( MLMEDIA_ID ),
                                ML_PEOPLE, ML_PERSON_ARTIST, qtu( data.toString() ) ) == VLC_SUCCESS;
        case ML_EXTRA: setmeta( psz_extra );
        case ML_GENRE: setmeta( psz_genre );
        case ML_ORIGINAL_TITLE: setmeta( psz_orig_title );
        case ML_TITLE: setmeta( psz_title );
update:
        Q_ASSERT( qtu( data.toString() ) );
            return ml_UpdateSimple( p_ml, ML_MEDIA, NULL, id( MLMEDIA_ID ),
                                    meta, qtu( data.toString() ) ) == VLC_SUCCESS;

        /* Modifiable integers */
        case ML_TRACK_NUMBER:
        case ML_YEAR:
            return ml_UpdateSimple( p_ml, ML_MEDIA, NULL, id( MLMEDIA_ID ),
                                    meta, data.toInt() ) == VLC_SUCCESS;

        // TODO case ML_VOTE:

        /* Non modifiable meta */
        default:
            return false;
    }
#   undef setmeta
}

int MLItem::id( int type )
{
    switch( type )
    {
    case INPUTITEM_ID:
        return inputItem()->i_id;
    case MLMEDIA_ID:
        return media->i_id;
    default:
    case PLAYLIST_ID:
        assert( NULL );
        return -1;
    }
}

ml_media_t* MLItem::getMedia() const
{
    return media;
}

QUrl MLItem::getURI() const
{
    QString uri;
    vlc_mutex_lock( &media->lock );
    uri = qfu( media->psz_uri );
    vlc_mutex_unlock( &media->lock );
    if ( uri.isEmpty() ) return QUrl(); // This should be rootItem

    QUrl url = QUrl::fromEncoded( uri.toUtf8(), QUrl::TolerantMode );
    if ( url.scheme().isEmpty() ) url.setScheme( "file" );
    return url;
}

QString MLItem::getTitle() const
{
    QString title;
    vlc_mutex_lock( &media->lock );
    title = QString( media->psz_title );
    vlc_mutex_unlock( &media->lock );
    return title;
}

bool MLItem::operator<( MLItem* item )
{
     int ret = compareMeta( getMedia(), item->getMedia(), ML_ALBUM );
     if( ret == -1 )
         return true;
     else return false;
}
#endif
