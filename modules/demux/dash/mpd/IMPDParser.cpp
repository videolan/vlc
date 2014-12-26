/*
 * IMPDParser.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2014 VideoLAN Authors
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
#include "IMPDParser.h"
#include "xml/DOMHelper.h"

using namespace dash::mpd;
using namespace dash::xml;

IMPDParser::IMPDParser(Node *root_, MPD *mpd_, stream_t *stream, Representation *rep)
{
    root = root_;
    mpd = mpd_;
    currentRepresentation = rep;
    p_stream = stream;
}

void IMPDParser::setMPDBaseUrl(Node *root)
{
    std::vector<Node *> baseUrls = DOMHelper::getChildElementByTagName(root, "BaseURL");

    for(size_t i = 0; i < baseUrls.size(); i++)
    {
        BaseUrl *url = new BaseUrl(baseUrls.at(i)->getText());
        mpd->addBaseUrl(url);
    }
}

MPD* IMPDParser::getMPD()
{
    return mpd;
}
