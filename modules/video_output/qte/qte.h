/*****************************************************************************
 * qte.h : QT Embedded plugin for vlc
 *****************************************************************************
 * Copyright (C) 1998-2002 VideoLAN
 * $Id: qte.h,v 1.1 2002/09/04 21:13:33 jpsaman Exp $
 *
 * Authors: Gerald Hansink <gerald.hansink@ordain.nl>
 *          Jean-Paul Saman <jpsaman@wxs.nl>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/


/*****************************************************************************
 * vout_sys_t: video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the specific properties of an video output plugin
 *****************************************************************************/
typedef struct vout_sys_s
{
    /* Internal settings and properties */
    int                 i_width;
    int                 i_height;

    bool                bRunning;
    bool                bOwnsQApp;

    QApplication*       pcQApplication;
    QWidget*            pcVoutWidget;
} vout_sys_t;


/*****************************************************************************
 * picture_sys_t: direct buffer method descriptor
 *****************************************************************************/
typedef struct picture_sys_s
{
    QImage*             pQImage;
} picture_sys_t;


/*****************************************************************************
 * Chroma defines
 *****************************************************************************/
#define QTE_MAX_DIRECTBUFFERS    2

