/*****************************************************************************
 * DBMHelper.hpp: Decklink SDI Helpers
 *****************************************************************************
 * Copyright © 2014-2016 VideoLAN and VideoLAN Authors
 *             2018-2019 VideoLabs
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
#ifndef DBMHELPER_HPP
#define DBMHELPER_HPP

namespace Decklink
{
    class Helper
    {
        public:
            static IDeckLinkDisplayMode * MatchDisplayMode(vlc_object_t *,
                                                           IDeckLinkOutput *,
                                                           const video_format_t *,
                                                           BMDDisplayMode = bmdModeUnknown);
            static const char *ErrorToString(long i_code);
    };
}

#endif
