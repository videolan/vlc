/*****************************************************************************
 * voutagl.h: MacOS X agl OpenGL provider (used by webbrowser.plugin)
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
 *          Damien Fouilleul <damienf at videolan dot org>
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

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

#include <AGL/agl.h>

#include <vlc_common.h>

int  aglInit   ( vout_thread_t * p_vout );
void aglEnd    ( vout_thread_t * p_vout );
int  aglManage ( vout_thread_t * p_vout );
int  aglControl( vout_thread_t *, int, va_list );
void aglSwap   ( vout_thread_t * p_vout );
int  aglLock   ( vout_thread_t * p_vout );
void aglUnlock ( vout_thread_t * p_vout );
