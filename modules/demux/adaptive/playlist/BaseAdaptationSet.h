/*
 * AdaptationSet.h
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

#ifndef BASEADAPTATIONSET_H_
#define BASEADAPTATIONSET_H_

#include <vector>
#include <string>

#include "CommonAttributesElements.h"
#include "SegmentInformation.hpp"
#include "../StreamFormat.hpp"

namespace adaptive
{
    class ID;

    namespace playlist
    {
        class BaseRepresentation;
        class BasePeriod;

        class BaseAdaptationSet : public CommonAttributesElements,
                                  public SegmentInformation
        {
            public:
                BaseAdaptationSet(BasePeriod *);
                virtual ~BaseAdaptationSet();

                virtual StreamFormat            getStreamFormat() const; /*reimpl*/
                std::vector<BaseRepresentation *>&  getRepresentations      ();
                BaseRepresentation *            getRepresentationByID(const ID &);
                void                            setSwitchPolicy(bool value);
                bool                            getBitstreamSwitching() const;
                void                            addRepresentation( BaseRepresentation *rep );
                void                            debug(vlc_object_t *,int = 0) const;
                Property<std::string>           description;

            protected:
                std::vector<BaseRepresentation *>   representations;
                bool                            isBitstreamSwitching;
        };
    }
}

#endif /* BASEADAPTATIONSET_H_ */
