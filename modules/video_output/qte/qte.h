/*****************************************************************************
 * qte.h : QT Embedded plugin for vlc
 *****************************************************************************
 * Copyright (C) 1998-2002 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

/*****************************************************************************
 * event_thread_t: QT Embedded event thread
 *****************************************************************************/
typedef struct event_thread_t
{
    VLC_COMMON_MEMBERS

    vout_thread_t * p_vout;

} event_thread_t;


/*****************************************************************************
 * vout_sys_t: video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the specific properties of an video output plugin
 *****************************************************************************/
struct vout_sys_t
{
    /* Internal settings and properties */
    int                 i_width;
    int                 i_height;

    bool                bRunning;
    bool                bOwnsQApp;

#ifdef NEED_QTE_MAIN
    module_t *          p_qte_main;
#endif

    QApplication*       p_QApplication;
    QWidget*            p_VideoWidget;

    event_thread_t *    p_event;
};


/*****************************************************************************
 * picture_sys_t: direct buffer method descriptor
 *****************************************************************************/
struct picture_sys_t
{
    QImage*             pQImage;
};


/*****************************************************************************
 * Chroma defines
 *****************************************************************************/
#define QTE_MAX_DIRECTBUFFERS    2

