/*****************************************************************************
 * CommonAttributesElements.cpp: Defines the common attributes and elements
 *                               for some Dash elements.
 *****************************************************************************
 * Copyright © 2011 VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "CommonAttributesElements.h"

#include <vlc_common.h>
#include <vlc_arrays.h>

using namespace adaptive::playlist;

CommonAttributesElements::CommonAttributesElements(CommonAttributesElements *p) :
    width( -1 ),
    height( -1 )
{
    parentCommonAttributes = p;
}

CommonAttributesElements::~CommonAttributesElements()
{
}

const std::string& CommonAttributesElements::getMimeType() const
{
    if(!mimeType.empty() || !parentCommonAttributes)
        return mimeType;
    return parentCommonAttributes->getMimeType();
}

void CommonAttributesElements::setMimeType( const std::string &mimeType )
{
    this->mimeType = mimeType;
}

int     CommonAttributesElements::getWidth                () const
{
    if(width != -1 || !parentCommonAttributes)
        return width;
    return parentCommonAttributes->getWidth();
}

void    CommonAttributesElements::setWidth( int width )
{
    if ( width > 0 )
        this->width = width;
}

int     CommonAttributesElements::getHeight               () const
{
    if(height != -1 || !parentCommonAttributes)
        return height;
    return parentCommonAttributes->getHeight();
}

void    CommonAttributesElements::setHeight( int height )
{
    if ( height > 0 )
        this->height = height;
}

void CommonAttributesElements::setAspectRatio(const AspectRatio &r)
{
    aspectRatio = r;
}

const AspectRatio & CommonAttributesElements::getAspectRatio() const
{
    if(aspectRatio.isValid() || !parentCommonAttributes)
        return aspectRatio;
    return parentCommonAttributes->getAspectRatio();
}

const Rate & CommonAttributesElements::getFrameRate() const
{
    if(frameRate.isValid() || !parentCommonAttributes)
        return frameRate;
    return parentCommonAttributes->getFrameRate();
}

void CommonAttributesElements::setFrameRate(const Rate &r)
{
    frameRate = r;
}

const Rate & CommonAttributesElements::getSampleRate() const
{
    if(sampleRate.isValid() || !parentCommonAttributes)
        return sampleRate;
    return parentCommonAttributes->getSampleRate();
}

void CommonAttributesElements::setSampleRate(const Rate &r)
{
    sampleRate = r;
}
