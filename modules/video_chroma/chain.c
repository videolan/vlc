/*****************************************************************************
 * chain.c : chain multiple chroma modules as a last resort solution
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc_vout.h>

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static int  Activate ( vlc_object_t * );
static void Destroy  ( vlc_object_t * );
static void Chain    ( vout_thread_t *, picture_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Chroma conversions using a chain of chroma conversion modules") );
    set_capability( "chroma", 1 );
    set_callbacks( Activate, Destroy );
vlc_module_end();

#define MAX_CHROMAS 2

struct chroma_sys_t
{
    vlc_fourcc_t i_chroma;

    vout_chroma_t chroma1;
    vout_chroma_t chroma2;

    picture_t *p_tmp;
};

static const vlc_fourcc_t pi_allowed_chromas[] = {
    VLC_FOURCC('I','4','2','0'),
    VLC_FOURCC('I','4','2','2'),
    0
};

/*****************************************************************************
 * Activate: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    static int hack = 1;
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    hack++;
    if( hack > MAX_CHROMAS )
    {
        hack--;
        msg_Err( p_this, "Preventing chain chroma reccursion (already %d long)",
                 hack );
        return VLC_EGENERIC;
    }

    chroma_sys_t *p_sys = (chroma_sys_t *)malloc( sizeof( chroma_sys_t ) );
    if( !p_sys )
    {
        hack--;
        return VLC_ENOMEM;
    }
    memset( p_sys, 0, sizeof( chroma_sys_t ) );

    int i;
    vlc_fourcc_t i_output_chroma = p_vout->output.i_chroma;
    vlc_fourcc_t i_render_chroma = p_vout->render.i_chroma;

    for( i = 0; pi_allowed_chromas[i]; i++ )
    {
        msg_Warn( p_vout, "Trying %4s as a chroma chain",
                  (const char *)&pi_allowed_chromas[i] );
        p_vout->output.i_chroma = pi_allowed_chromas[i];
        p_vout->chroma.p_module = module_Need( p_vout, "chroma", NULL, 0 );
        p_vout->output.i_chroma = i_output_chroma;

        if( !p_vout->chroma.p_module )
            continue;

        p_sys->chroma1 = p_vout->chroma;
        memset( &p_vout->chroma, 0, sizeof( vout_chroma_t ) );

        p_vout->render.i_chroma = pi_allowed_chromas[i];
        p_vout->chroma.p_module = module_Need( p_vout, "chroma", NULL, 0 );
        p_vout->render.i_chroma = i_render_chroma;

        if( !p_vout->chroma.p_module )
        {
            p_vout->chroma = p_sys->chroma1;
            module_Unneed( p_vout, p_vout->chroma.p_module );
            continue;
        }

        p_sys->chroma2 = p_vout->chroma;
        memset( &p_vout->chroma, 0, sizeof( vout_chroma_t ) );

        p_sys->i_chroma = pi_allowed_chromas[i];
        p_vout->chroma.pf_convert = Chain;
        p_vout->chroma.p_sys = p_sys;
        hack--;
        printf("Init: p_sys->p_tmp= %p\n", p_sys->p_tmp );
        return VLC_SUCCESS;
    }

    free( p_sys );
    hack--;
    return VLC_EGENERIC;
}

static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_chroma_t chroma = p_vout->chroma;


    p_vout->chroma = chroma.p_sys->chroma1;
    module_Unneed( p_vout, p_vout->chroma.p_module );
    p_vout->chroma = chroma.p_sys->chroma2;
    module_Unneed( p_vout, p_vout->chroma.p_module );
    p_vout->chroma = chroma;

    if( chroma.p_sys->p_tmp )
    {
        free( chroma.p_sys->p_tmp->p_data_orig );
        free( chroma.p_sys->p_tmp );
    }
    free( chroma.p_sys );
    chroma.p_sys = NULL;
}

/*****************************************************************************
 * Chain
 *****************************************************************************/
static void Chain( vout_thread_t *p_vout, picture_t *p_source,
                   picture_t *p_dest )
{
    chroma_sys_t *p_sys = p_vout->chroma.p_sys;

    if( !p_sys->p_tmp )
    {
        picture_t *p_tmp = malloc( sizeof( picture_t ) );
        vout_AllocatePicture( VLC_OBJECT( p_vout ), p_tmp,
                              p_sys->i_chroma,
                              p_source->p_heap->i_width,
                              p_source->p_heap->i_height,
                              p_source->p_heap->i_aspect );
        if( !p_tmp )
            return;
        p_sys->p_tmp = p_tmp;
        p_tmp->pf_release = NULL;
        p_tmp->i_status = RESERVED_PICTURE;
        p_tmp->p_sys = NULL;
    }

    vout_chroma_t chroma = p_vout->chroma;
    p_vout->chroma = p_sys->chroma1;
    p_sys->chroma1.pf_convert( p_vout, p_source, p_sys->p_tmp );
    p_vout->chroma = p_sys->chroma2;
    p_sys->chroma2.pf_convert( p_vout, p_sys->p_tmp, p_dest );
    p_vout->chroma = chroma;
}
