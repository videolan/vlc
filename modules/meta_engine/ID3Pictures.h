/*****************************************************************************
 * ID3Pictures.h : ID3v2 Pictures definitions
 *****************************************************************************
 * Copyright (C) 2016-2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef ID3PICTURES_H
#define ID3PICTURES_H

enum
{
    ID3_PICTURE_OTHER                = 0x00, // Other
    ID3_PICTURE_FILE_ICON            = 0x01, // 32x32 PNG image that should be used as the file icon
    ID3_PICTURE_OTHER_FILE_ICON      = 0x02, // File icon of a different size or format.
    ID3_PICTURE_FRONT_COVER          = 0x03, // Front cover image of the album.
    ID3_PICTURE_BACK_COVER           = 0x04, // Back cover image of the album
    ID3_PICTURE_LEAFLET_PAGE         = 0x05, // Inside leaflet page of the album
    ID3_PICTURE_MEDIA                = 0x06, // Image from the album itself
    ID3_PICTURE_LEAD_ARTIST          = 0x07, // Picture of the lead artist or soloist
    ID3_PICTURE_ARTIST               = 0x08, // Picture of the artist or performer
    ID3_PICTURE_CONDUCTOR            = 0x09, // Picture of the conductor
    ID3_PICTURE_BAND                 = 0x0A, // Picture of the band or orchestra
    ID3_PICTURE_COMPOSER             = 0x0B, // Picture of the composer
    ID3_PICTURE_LYRICIST             = 0x0C, // Picture of the lyricist or text writer
    ID3_PICTURE_RECORDING_LOCATION   = 0x0D, // Picture of the recording location or studio
    ID3_PICTURE_DURING_RECORDING     = 0x0E, // Picture of the artists during recording
    ID3_PICTURE_DURING_PERFORMANCE   = 0x0F, // Picture of the artists during performance
    ID3_PICTURE_MOVIE_SCREEN_CAPTURE = 0x10, // Picture from a movie or video related to the track
    ID3_PICTURE_COLORED_FISH         = 0x11, // Picture of a large, coloured fish
    ID3_PICTURE_ILLUSTRATION         = 0x12, // Illustration related to the track
    ID3_PICTURE_BAND_LOGO            = 0x13, // Logo of the band or performer
    ID3_PICTURE_PUBLISHER_LOGO       = 0x14, // Logo of the publisher (record company)
    ID3_PICTURE_COUNT
};

/* Preferred type of image
     * The 21 types are defined in id3v2 standard:
     * http://www.id3.org/id3v2.4.0-frames */
static const char ID3v2_cover_scores[] = {
    [ID3_PICTURE_OTHER]                 = 0,
    [ID3_PICTURE_FILE_ICON]             = 1,
    [ID3_PICTURE_OTHER_FILE_ICON]       = 4,
    [ID3_PICTURE_FRONT_COVER]           = 20,
    [ID3_PICTURE_BACK_COVER]            = 19,
    [ID3_PICTURE_LEAFLET_PAGE]          = 13,
    [ID3_PICTURE_MEDIA]                 = 18,
    [ID3_PICTURE_LEAD_ARTIST]           = 17,
    [ID3_PICTURE_ARTIST]                = 16,
    [ID3_PICTURE_CONDUCTOR]             = 14,
    [ID3_PICTURE_BAND]                  = 15,
    [ID3_PICTURE_COMPOSER]              = 9,
    [ID3_PICTURE_LYRICIST]              = 8,
    [ID3_PICTURE_RECORDING_LOCATION]    = 7,
    [ID3_PICTURE_DURING_RECORDING]      = 10,
    [ID3_PICTURE_DURING_PERFORMANCE]    = 11,
    [ID3_PICTURE_MOVIE_SCREEN_CAPTURE]  = 6,
    [ID3_PICTURE_COLORED_FISH]          = 1,
    [ID3_PICTURE_ILLUSTRATION]          = 12,
    [ID3_PICTURE_BAND_LOGO]             = 3,
    [ID3_PICTURE_PUBLISHER_LOGO]        = 2
};

static_assert(ARRAY_SIZE(ID3v2_cover_scores) == ID3_PICTURE_COUNT, "mismatched scoring table size");

#endif
