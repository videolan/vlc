/*****************************************************************************
 * filter_event_info.h:
 *****************************************************************************
 * Copyright (C) 2004-2005 VLC authors and VideoLAN
 * $Id: ???
 *
 * Authors: Dugal Harris
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

#ifndef FILTER_EVENT_INFO_H_
#define FILTER_EVENT_INFO_H_

#define VIDEO_FILTER_EVENT_VARIABLE "video-filter-event"

typedef struct video_filter_region_info_t
{
    int i_x; /* x-coordinate of the left-most rectangle corner[s] */
    int i_y; /* y-coordinate of the top-most or bottom-most
                  rectangle corner[s] */
    int i_width; /* width of the rectangle */
    int i_height; /* height of the rectangle */
 
    int i_id;
    int i_type;
 
    char *p_description;
    float *pf_param;
    int i_param_size;
} video_filter_region_info_t;

typedef struct video_filter_event_info_t
{
   video_filter_region_info_t *p_region;
   int i_region_size;
} video_filter_event_info_t;

/*class CTest
{
public:
    CTest(){i=0;};
    void Method(){i=0;};
    int i;
};*/

#endif /*FILTER_EVENT_INFO_H_*/
