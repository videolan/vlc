/*****************************************************************************
 * ml_item.cpp: the media library's result item
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

#ifdef MEDIA_LIBRARY

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


MLItem::MLItem( const MLModel *p_model,
                            intf_thread_t* _p_intf,
                            ml_media_t *p_media,
                            MLItem *p_parent )
        : p_intf( _p_intf ), model( p_model )
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

void MLItem::delChild( int row )
{
    if( !childCount() ) return; // assert ?
    AbstractPLItem *item =
            children.takeAt( ( row!=-1 ) ? row : ( children.count()-1 ) );
    assert( item );
    delete item;
}

input_item_t* MLItem::inputItem()
{
    return ml_CreateInputItem( p_ml,  id() );
}

/**
 * @brief Get a QVariant representing the data on a column
 * @param column
 * @return The QVariant may be formed of a int, QString
 *         Use toString() to print it on the screen, except for pixmaps
 */
QVariant MLItem::data( int column ) const
{
    ml_select_e type = model->columnType( column );
    ml_person_t *p_people = NULL, *p_person = NULL;
    QString qsz_return;
#define sreturn(a) if(media->a) return qfu(media->a); break
    switch( type )
    {
        case ML_ALBUM: sreturn( psz_album );
        case ML_ALBUM_ID: return media->i_album_id;
        case ML_ARTIST:
            p_people = ml_GetPersonsFromMedia( p_ml, media, ML_PERSON_ARTIST );
            p_person = p_people;
            while( p_person )
            {
                if( !EMPTY_STR( p_person->psz_name ) )
                {
                    qsz_return.isEmpty() ? qsz_return = qfu( p_person->psz_name ) :
                        qsz_return.append( "," ).append( qfu( p_person->psz_name ) );
                }
                p_person = p_person->p_next;
            }
            ml_FreePeople( p_people );
            return qsz_return;
            break;
        case ML_COVER: sreturn( psz_cover );
        case ML_DURATION:
            return QTime().addSecs( media->i_duration/1000000 ).toString( "HH:mm:ss" );
        case ML_EXTRA: sreturn( psz_extra );
        case ML_GENRE: sreturn( psz_genre );
        case ML_ID: return media->i_id;
        case ML_LAST_PLAYED:
        {
            if( media->i_last_played > 0 )
            {
                QDateTime time( QDate(1970,1,1) );
                return time.addSecs( qint64( media->i_last_played ) );
            }
            else
                return QString();
        }
        case ML_ORIGINAL_TITLE: sreturn( psz_orig_title );
        case ML_PLAYED_COUNT: return media->i_played_count;
        // case ML_ROLE: return qtr( "Role" );
        case ML_SCORE: return media->i_score ? media->i_score : QVariant();
        case ML_TITLE:
        {
            vlc_mutex_lock( &media->lock );
            qsz_return = qfu( media->psz_title );
            vlc_mutex_unlock( &media->lock );
            /* If no title, return filename */
            if( ! qsz_return.isEmpty() )
                return qsz_return;
            else
            {
                vlc_mutex_lock( &media->lock );
                QFileInfo p_file = QFileInfo( qfu( media->psz_uri ) );
                vlc_mutex_unlock( &media->lock );
                return p_file.fileName().isEmpty() ? p_file.absoluteFilePath()
                    : p_file.fileName();
            }
        }
        case ML_TRACK_NUMBER: return media->i_track_number ? media->i_track_number : QVariant();
        case ML_TYPE:
        {
            QString txt;
            if( media->i_type & ML_AUDIO )
                txt = "Audio";
            if( media->i_type & ML_VIDEO )
                txt = "Video";
            if( media->i_type & ML_STREAM )
            {
                if( txt.isEmpty() ) txt = "Stream";
                else txt += " stream";
            }
            if( media->i_type & ML_REMOVABLE )
            {
                if( txt.isEmpty() ) txt = "Removable media";
                else txt += " (removable media)";
            }
            if( media->i_type & ML_NODE )
            {
                if( txt.isEmpty() ) txt = "Playlist";
                else txt += " (Playlist)";
            }
            if( txt.isEmpty() )
                txt = qtr( "Unknown" );
            return txt;
        }
        case ML_URI: sreturn( psz_uri );
        case ML_VOTE: return media->i_vote ? media->i_vote : QVariant();
        case ML_YEAR: return media->i_year ? media->i_year : QVariant();
        default: return QVariant();
    }
#   undef sreturn
    return QVariant();
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
                        return ml_UpdateSimple( p_ml, ML_MEDIA, NULL, id(),
                                ML_PEOPLE, ML_PERSON_ARTIST, qtu( data.toString() ) ) == VLC_SUCCESS;
        case ML_EXTRA: setmeta( psz_extra );
        case ML_GENRE: setmeta( psz_genre );
        case ML_ORIGINAL_TITLE: setmeta( psz_orig_title );
        case ML_TITLE: setmeta( psz_title );
update:
            return ml_UpdateSimple( p_ml, ML_MEDIA, NULL, id(),
                                    meta, qtu( data.toString() ) ) == VLC_SUCCESS;

        /* Modifiable integers */
        case ML_TRACK_NUMBER:
        case ML_YEAR:
            return ml_UpdateSimple( p_ml, ML_MEDIA, NULL, id(),
                                    meta, data.toInt() ) == VLC_SUCCESS;

        // TODO case ML_VOTE:

        /* Non modifiable meta */
        default:
            return false;
    }
#   undef setmeta
}

int MLItem::id() const
{
    return media->i_id;
}

ml_media_t* MLItem::getMedia() const
{
    return media;
}

QUrl MLItem::getUri() const
{
    QString uri;
    vlc_mutex_lock( &media->lock );
    uri = QString( media->psz_uri );
    vlc_mutex_unlock( &media->lock );
    if ( uri.isEmpty() ) return QUrl(); // This should be rootItem

    QUrl url( uri );
    if ( url.scheme().isEmpty() ) url.setScheme( "file" );
    return url;
}

bool MLItem::operator<( MLItem* item )
{
     int ret = compareMeta( getMedia(), item->getMedia(), ML_ALBUM );
     if( ret == -1 )
         return true;
     else return false;
}
#endif
