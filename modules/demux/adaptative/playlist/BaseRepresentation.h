/*
 * Representation.h
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
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

#ifndef BASEREPRESENTATION_H_
#define BASEREPRESENTATION_H_

#include <string>

#include "CommonAttributesElements.h"
#include "SegmentInformation.hpp"

namespace adaptative
{
    namespace playlist
    {
        class BaseAdaptationSet;
        class AbstractPlaylist;
        class BaseSegmentTemplate;

        class BaseRepresentation : public CommonAttributesElements,
                                   public SegmentInformation
        {
            public:
                BaseRepresentation( BaseAdaptationSet *, AbstractPlaylist *playlist );
                virtual ~BaseRepresentation ();

                /*
                 *  @return The bitrate required for this representation
                 *          in bits per seconds.
                 *          Will be a valid value, as the parser refuses Representation
                 *          without bandwidth.
                 */
                uint64_t            getBandwidth            () const;
                void                setBandwidth            ( uint64_t bandwidth );

                AbstractPlaylist*   getPlaylist             () const;

                std::vector<std::string> toString(int = 0) const;

                /* for segment templates */
                virtual std::string contextualize(size_t, const std::string &,
                                                  const BaseSegmentTemplate *) const;

            protected:
                AbstractPlaylist                   *playlist;
                BaseAdaptationSet                  *adaptationSet;
                uint64_t                            bandwidth;
        };
    }
}

#endif /* BASEREPRESENTATION_H_ */
