/*****************************************************************************
 * filter_picture.h: Common picture functions for filters
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/* FIXME: do all of these really have square pixels? */
#define CASE_PLANAR_YUV_SQUARE              \
        case VLC_FOURCC('I','4','2','0'):   \
        case VLC_FOURCC('I','Y','U','V'):   \
        case VLC_FOURCC('J','4','2','0'):   \
        case VLC_FOURCC('Y','V','1','2'):   \
        case VLC_FOURCC('I','4','1','1'):   \
        case VLC_FOURCC('I','4','1','0'):   \
        case VLC_FOURCC('Y','V','U','9'):   \
        case VLC_FOURCC('I','4','4','4'):   \
        case VLC_FOURCC('J','4','4','4'):   \
        case VLC_FOURCC('Y','U','V','A'):

#define CASE_PLANAR_YUV_NONSQUARE           \
        case VLC_FOURCC('I','4','2','2'):   \
        case VLC_FOURCC('J','4','2','2'):

#define CASE_PLANAR_YUV                     \
        CASE_PLANAR_YUV_SQUARE              \
        CASE_PLANAR_YUV_NONSQUARE           \

#define CASE_PACKED_YUV_422                 \
        case VLC_FOURCC('U','Y','V','Y'):   \
        case VLC_FOURCC('U','Y','N','V'):   \
        case VLC_FOURCC('Y','4','2','2'):   \
        case VLC_FOURCC('c','y','u','v'):   \
        case VLC_FOURCC('Y','U','Y','2'):   \
        case VLC_FOURCC('Y','U','N','V'):   \
        case VLC_FOURCC('Y','V','Y','U'):

static inline int GetPackedYuvOffsets( vlc_fourcc_t i_chroma,
    int *i_y_offset, int *i_u_offset, int *i_v_offset )
{
    switch( i_chroma )
    {
        case VLC_FOURCC('U','Y','V','Y'):
        case VLC_FOURCC('U','Y','N','V'):
        case VLC_FOURCC('Y','4','2','2'):
        case VLC_FOURCC('c','y','u','v'): /* <-- FIXME: reverted, whatever that means */
            /* UYVY */
            *i_y_offset = 1;
            *i_u_offset = 0;
            *i_v_offset = 2;
            return VLC_SUCCESS;
        case VLC_FOURCC('Y','U','Y','2'):
        case VLC_FOURCC('Y','U','N','V'):
            /* YUYV */
            *i_y_offset = 0;
            *i_u_offset = 1;
            *i_v_offset = 3;
            return VLC_SUCCESS;
        case VLC_FOURCC('Y','V','Y','U'):
            /* YVYU */
            *i_y_offset = 0;
            *i_u_offset = 3;
            *i_v_offset = 1;
            return VLC_SUCCESS;
        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
static inline picture_t *CopyInfoAndRelease( picture_t *p_outpic, picture_t *p_inpic )
{
    picture_CopyProperties( p_outpic, p_inpic );

    picture_Release( p_inpic );

    return p_outpic;
}
