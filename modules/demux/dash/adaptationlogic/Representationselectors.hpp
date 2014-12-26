/*
 * Representationselectors.hpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN authors
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
#include "mpd/Representation.h"
#include "mpd/Period.h"

using namespace dash::mpd;

namespace dash
{
    namespace logic
    {

        class RepresentationSelector
        {
        public:
            RepresentationSelector();
            virtual ~RepresentationSelector() {}
            virtual Representation * select(Period *period, Streams::Type) const;
            virtual Representation * select(Period *period, Streams::Type, uint64_t bitrate) const;
            virtual Representation * select(Period *period, Streams::Type, uint64_t bitrate,
                                            int width, int height) const;
        protected:
            virtual Representation * select(std::vector<Representation *>&reps,
                                            uint64_t minbitrate, uint64_t maxbitrate) const;
        };

    }
}

#endif // REPRESENTATIONSELECTORS_HPP
