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
#include "playlist_item.hpp"

//namespace vlc {
//namespace playlist {

PlaylistItem::PlaylistItem(vlc_playlist_item_t* item)
{
    d = new Data();
    if (item)
    {
        d->item.reset(item);
        sync();
    }
}

bool PlaylistItem::isSelected() const
{
    return d->selected;
}

void PlaylistItem::setSelected(bool selected)
{
    d->selected = selected;
}

QString PlaylistItem::getTitle() const
{
    return d->title;
}

QString PlaylistItem::getArtist() const
{
    return d->artist;
}

QString PlaylistItem::getAlbum() const
{
    return d->album;
}

QUrl PlaylistItem::getArtwork() const
{
    return d->artwork;
}

vlc_tick_t PlaylistItem::getDuration() const
{
    return d->duration;
}

void PlaylistItem::sync() {
    input_item_t *media = vlc_playlist_item_GetMedia(d->item.get());
    vlc_mutex_lock(&media->lock);
    d->title = media->psz_name;
    d->duration = media->i_duration;

    if (media->p_meta) {
        d->artist = vlc_meta_Get(media->p_meta, vlc_meta_Artist);
        d->album = vlc_meta_Get(media->p_meta, vlc_meta_Album);
        d->artwork = vlc_meta_Get(media->p_meta, vlc_meta_ArtworkURL);
    }
    vlc_mutex_unlock(&media->lock);
}

PlaylistItem::operator bool() const
{
    return d;
}


//}
//}
