/*
 * Representationselectors.hpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN and VLC authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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
#ifndef REPRESENTATIONSELECTORS_HPP
#define REPRESENTATIONSELECTORS_HPP

#include <vector>
#include <vlc_common.h>

namespace adaptive
{
    namespace playlist
    {
        class BaseRepresentation;
        class BaseAdaptationSet;
    }

    namespace logic
    {
        using namespace playlist;

        class RepresentationSelector
        {
        public:
            RepresentationSelector(int, int);
             ~RepresentationSelector();
            BaseRepresentation * lowest(BaseAdaptationSet *) const;
            BaseRepresentation * highest(BaseAdaptationSet *) const;
            BaseRepresentation * higher(BaseAdaptationSet *, BaseRepresentation *) const;
            BaseRepresentation * lower(BaseAdaptationSet *, BaseRepresentation *) const;
            BaseRepresentation * select(BaseAdaptationSet *) const;
            BaseRepresentation * select(BaseAdaptationSet *, uint64_t bitrate) const;

        protected:
            int maxwidth;
            int maxheight;
            BaseRepresentation * select(std::vector<BaseRepresentation *>&reps,
                                        uint64_t minbitrate, uint64_t maxbitrate) const;
        };

    }
}

#endif // REPRESENTATIONSELECTORS_HPP
