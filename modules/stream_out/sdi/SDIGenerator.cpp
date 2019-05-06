/*****************************************************************************
 * SDIGenerator.cpp: SDI Image and sound generators
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_picture.h>
#include <vlc_image.h>

#include "SDIGenerator.hpp"

using namespace sdi;

picture_t * Generator::Picture(vlc_object_t *obj, const char *psz_file,
                               const video_format_t *fmt)
{
    picture_t *p_pic = NULL;
    image_handler_t *img = image_HandlerCreate(obj);
    if (img)
    {
        video_format_t in;
        video_format_Init(&in, 0);
        video_format_Setup(&in, 0,
                           fmt->i_width, fmt->i_height,
                           fmt->i_width, fmt->i_height, 1, 1);

        picture_t *png = image_ReadUrl(img, psz_file, &in);
        if (png)
        {
            video_format_t dummy;
            video_format_Copy(&dummy, fmt);
            p_pic = image_Convert(img, png, &in, &dummy);
            if(!video_format_IsSimilar(&dummy, fmt))
            {
                picture_Release(p_pic);
                p_pic = NULL;
            }
            picture_Release(png);
            video_format_Clean(&dummy);
        }
        image_HandlerDelete(img);
        video_format_Clean(&in);
    }
    return p_pic;
}
