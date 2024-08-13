/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <cassert>
#include "mlaudio.hpp"
#include "mlhelper.hpp"
#include "util/vlctick.hpp"

MLAudio::MLAudio(vlc_medialibrary_t* _ml, const vlc_ml_media_t *_data)
    : MLMedia( _data )
    , m_trackNumber( _data->album_track.i_track_nb )
    , m_discNumber ( _data->album_track.i_disc_nb )
{
    assert( _data );
    assert( _data->i_type == VLC_ML_MEDIA_TYPE_AUDIO );

    if ( _data->album_track.i_album_id != 0 )
    {
        ml_unique_ptr<vlc_ml_album_t> album(vlc_ml_get_album(_ml, _data->album_track.i_album_id));
        if (album)
            m_albumTitle =  album->psz_title;
    }

    if ( _data->album_track.i_artist_id != 0 )
    {
        ml_unique_ptr<vlc_ml_artist_t> artist(vlc_ml_get_artist(_ml, _data->album_track.i_artist_id));
        if (artist)
            m_artist =  artist->psz_name;
    }
}

QString MLAudio::getAlbumTitle() const
{
    return m_albumTitle;
}

QString MLAudio::getArtist() const
{
    return m_artist;
}

unsigned int MLAudio::getTrackNumber() const
{
    return m_trackNumber;
}

unsigned int MLAudio::getDiscNumber() const
{
    return m_discNumber;
}
