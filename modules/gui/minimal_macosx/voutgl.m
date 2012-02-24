/*****************************************************************************
 * voutgl.m: MacOS X OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "intf.h"
#include "voutgl.h"

int OpenVideoGL  ( vlc_object_t * p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *) p_this;
    int i_drawable_agl;
    int i_drawable_gl;

    if( !CGDisplayUsesOpenGLAcceleration( kCGDirectMainDisplay ) )
    {
        msg_Warn( p_vout, "No OpenGL hardware acceleration found. "
                          "Video display will be slow" );
        return( 1 );
    }

    msg_Dbg( p_vout, "display is Quartz Extreme accelerated" );

    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return( 1 );

    memset( p_vout->p_sys, 0, sizeof( vout_sys_t ) );

    i_drawable_agl = var_GetInteger( p_vout->p_libvlc, "drawable-agl" );
    i_drawable_gl = var_GetInteger( p_vout->p_libvlc, "drawable-gl" );

    /* Are we in the mozilla plugin, which isn't 64bit compatible ? */
#ifndef __x86_64__
    if( i_drawable_agl > 0 )
    {
        p_vout->pf_init             = aglInit;
        p_vout->pf_end              = aglEnd;
        p_vout->pf_manage           = aglManage;
        p_vout->pf_control          = aglControl;
        p_vout->pf_swap             = aglSwap;
        p_vout->pf_lock             = aglLock;
        p_vout->pf_unlock           = aglUnlock;
    }
    else /*if( i_drawable_gl > 0 )*/
    {
        /* Let's use the VLCOpenGLVoutView.m class */
        p_vout->pf_init   = cocoaglvoutviewInit;
        p_vout->pf_end    = cocoaglvoutviewEnd;
        p_vout->pf_manage = cocoaglvoutviewManage;
        p_vout->pf_control= cocoaglvoutviewControl;
        p_vout->pf_swap   = cocoaglvoutviewSwap;
        p_vout->pf_lock   = cocoaglvoutviewLock;
        p_vout->pf_unlock = cocoaglvoutviewUnlock;
    }
#else
    /* Let's use the VLCOpenGLVoutView.m class */
    p_vout->pf_init   = cocoaglvoutviewInit;
    p_vout->pf_end    = cocoaglvoutviewEnd;
    p_vout->pf_manage = cocoaglvoutviewManage;
    p_vout->pf_control= cocoaglvoutviewControl;
    p_vout->pf_swap   = cocoaglvoutviewSwap;
    p_vout->pf_lock   = cocoaglvoutviewLock;
    p_vout->pf_unlock = cocoaglvoutviewUnlock;
#endif
    p_vout->p_sys->b_got_frame = false;

    return VLC_SUCCESS;
}

void CloseVideoGL ( vlc_object_t * p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *) p_this;
    cocoaglvoutviewEnd( p_vout );
    /* Clean up */
    free( p_vout->p_sys );
}
