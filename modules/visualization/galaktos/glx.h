/*****************************************************************************
 * plugin.h:
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
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

#ifndef _GALAKTOS_GLX_H_
#define _GALAKTOS_GLX_H_

#include "plugin.h"

int galaktos_glx_init( galaktos_thread_t *p_thread );
void galaktos_glx_done( galaktos_thread_t *p_thread );
int galaktos_glx_handle_events( galaktos_thread_t *p_thread );
void galaktos_glx_activate_pbuffer( galaktos_thread_t *p_thread );
void galaktos_glx_activate_window( galaktos_thread_t *p_thread );
void galaktos_glx_swap( galaktos_thread_t *p_thread );

/*****************************************************************************
 * mwmhints_t: window manager hints
 *****************************************************************************
 * Fullscreen needs to be able to hide the wm decorations so we provide
 * this structure to make it easier.
 *****************************************************************************/
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5
typedef struct mwmhints_t
{
    uint32_t flags;
    uint32_t functions;
    uint32_t decorations;
    int32_t  input_mode;
    uint32_t status;
} mwmhints_t;

#endif
