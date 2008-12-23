/*****************************************************************************
 * ressource.c
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
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

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_aout.h>
#include <vlc_sout.h>
#include "../libvlc.h"
#include "../stream_output/stream_output.h"
#include "../audio_output/aout_internal.h"
#include "input_interface.h"
#include "ressource.h"

struct input_ressource_t
{
    vlc_mutex_t    lock;

    input_thread_t *p_input;

    sout_instance_t *p_sout;

    int             i_vout;
    vout_thread_t   **pp_vout;
    vout_thread_t   *p_vout_free;

    aout_instance_t *p_aout;
};

/* */
static void DestroySout( input_ressource_t *p_ressource )
{
    if( p_ressource->p_sout )
        sout_DeleteInstance( p_ressource->p_sout );
    p_ressource->p_sout = NULL;
}
static sout_instance_t *ExtractSout( input_ressource_t *p_ressource )
{
    sout_instance_t *p_sout = p_ressource->p_sout;

    p_ressource->p_sout = NULL;

    return p_sout;
}
static sout_instance_t *RequestSout( input_ressource_t *p_ressource,
                                     sout_instance_t *p_sout, const char *psz_sout )
{
    assert( p_ressource->p_input );
    assert( !p_sout || ( !p_ressource->p_sout && !psz_sout ) );

    if( !p_sout && !psz_sout )
    {
        if( p_ressource->p_sout )
            msg_Dbg( p_ressource->p_input, "destroying useless sout" );
        DestroySout( p_ressource );
        return NULL;
    }

    /* Check the validity of the sout */
    if( p_ressource->p_sout &&
        strcmp( p_ressource->p_sout->psz_sout, psz_sout ) )
    {
        msg_Dbg( p_ressource->p_input, "destroying unusable sout" );
        DestroySout( p_ressource );
    }

    if( psz_sout )
    {
        if( p_ressource->p_sout )
        {
            /* Reuse it */
            msg_Dbg( p_ressource->p_input, "reusing sout" );
            msg_Dbg( p_ressource->p_input, "you probably want to use gather stream_out" );
            vlc_object_attach( p_ressource->p_sout, p_ressource->p_input );
        }
        else
        {
            /* Create a new one */
            p_ressource->p_sout = sout_NewInstance( p_ressource->p_input, psz_sout );
        }
        return ExtractSout( p_ressource );
    }
    else
    {
        vlc_object_detach( p_sout );
        p_ressource->p_sout = p_sout;

        return NULL;
    }
}

/* */
static void DestroyVout( input_ressource_t *p_ressource )
{
    assert( p_ressource->i_vout == 0 );

    if( p_ressource->p_vout_free )
        vout_CloseAndRelease( p_ressource->p_vout_free );

    p_ressource->p_vout_free = NULL;
}
static vout_thread_t *RequestVout( input_ressource_t *p_ressource,
                                   vout_thread_t *p_vout, video_format_t *p_fmt )
{
    assert( p_ressource->p_input );

    if( !p_vout && !p_fmt )
    {
        if( p_ressource->p_vout_free )
        {
            msg_Err( p_ressource->p_input, "destroying useless vout" );
            vout_CloseAndRelease( p_ressource->p_vout_free );
            p_ressource->p_vout_free = NULL;
        }
        return NULL;
    }

    if( p_fmt )
    {
        /* */
        if( !p_vout && p_ressource->p_vout_free )
        {
            msg_Err( p_ressource->p_input, "trying to reuse free vout" );
            p_vout = p_ressource->p_vout_free;

            p_ressource->p_vout_free = NULL;
        }
        else if( p_vout )
        {
            assert( p_vout != p_ressource->p_vout_free );
            TAB_REMOVE( p_ressource->i_vout, p_ressource->pp_vout, p_vout );
        }

        /* */
        p_vout = vout_Request( p_ressource->p_input, p_vout, p_fmt );
        if( !p_vout )
            return NULL;

        TAB_APPEND( p_ressource->i_vout, p_ressource->pp_vout, p_vout );
        return p_vout;
    }
    else
    {
        assert( p_vout );
        TAB_REMOVE( p_ressource->i_vout, p_ressource->pp_vout, p_vout );
        if( p_ressource->p_vout_free )
        {
            msg_Err( p_ressource->p_input, "detroying vout (already one saved)" );
            vout_CloseAndRelease( p_vout );
        }
        else
        {
            msg_Err( p_ressource->p_input, "saving a free vout" );
            p_ressource->p_vout_free = p_vout;
        }
        return NULL;
    }
}
static vout_thread_t *HoldVout( input_ressource_t *p_ressource )
{
    if( p_ressource->i_vout <= 0 )
        return NULL;

    /* TODO FIXME: p_ressource->pp_vout order is NOT stable */
    vout_thread_t *p_vout = p_ressource->pp_vout[0];

    vlc_object_hold( p_vout );

    return p_vout;
}

/* */
static void DestroyAout( input_ressource_t *p_ressource )
{
    if( p_ressource->p_aout )
        vlc_object_release( p_ressource->p_aout );
    p_ressource->p_aout = NULL;
}
static aout_instance_t *RequestAout( input_ressource_t *p_ressource, aout_instance_t *p_aout )
{
    assert( p_ressource->p_input );

    if( p_aout )
    {
        msg_Err( p_ressource->p_input, "releasing aout" );
        vlc_object_release( p_aout );
        return NULL;
    }
    else
    {
        if( !p_ressource->p_aout )
        {
            msg_Err( p_ressource->p_input, "creating aout" );
            p_ressource->p_aout = aout_New( p_ressource->p_input );
        }
        else
        {
            msg_Err( p_ressource->p_input, "reusing aout" );
            vlc_object_attach( p_ressource->p_aout, p_ressource->p_input );
        }


        if( !p_ressource->p_aout )
            return NULL;

        vlc_object_hold( p_ressource->p_aout );
        return p_ressource->p_aout;
    }
}

/* */
input_ressource_t *input_ressource_New( void )
{
    input_ressource_t *p_ressource = calloc( 1, sizeof(*p_ressource) );
    if( !p_ressource )
        return NULL;

    vlc_mutex_init( &p_ressource->lock );
    return p_ressource;
}

void input_ressource_Delete( input_ressource_t *p_ressource )
{
    DestroySout( p_ressource );
    DestroyVout( p_ressource );
    DestroyAout( p_ressource );

    vlc_mutex_destroy( &p_ressource->lock );
    free( p_ressource );
}

void input_ressource_SetInput( input_ressource_t *p_ressource, input_thread_t *p_input )
{
    vlc_mutex_lock( &p_ressource->lock );

    if( p_ressource->p_input && !p_input )
    {
        if( p_ressource->p_aout )
            vlc_object_detach( p_ressource->p_aout );

        assert( p_ressource->i_vout == 0 );
        if( p_ressource->p_vout_free )
            vlc_object_detach( p_ressource->p_vout_free );

        if( p_ressource->p_sout )
            vlc_object_detach( p_ressource->p_sout );
    }

    /* */
    p_ressource->p_input = p_input;

    vlc_mutex_unlock( &p_ressource->lock );
}

vout_thread_t *input_ressource_RequestVout( input_ressource_t *p_ressource,
                                            vout_thread_t *p_vout, video_format_t *p_fmt )
{
    vlc_mutex_lock( &p_ressource->lock );
    vout_thread_t *p_ret = RequestVout( p_ressource, p_vout, p_fmt );
    vlc_mutex_unlock( &p_ressource->lock );

    return p_ret;
}
vout_thread_t *input_ressource_HoldVout( input_ressource_t *p_ressource )
{
    vlc_mutex_lock( &p_ressource->lock );
    vout_thread_t *p_ret = HoldVout( p_ressource );
    vlc_mutex_unlock( &p_ressource->lock );

    return p_ret;
}

/* */
aout_instance_t *input_ressource_RequestAout( input_ressource_t *p_ressource, aout_instance_t *p_aout )
{
    vlc_mutex_lock( &p_ressource->lock );
    aout_instance_t *p_ret = RequestAout( p_ressource, p_aout );
    vlc_mutex_unlock( &p_ressource->lock );

    return p_ret;
}

/* */
sout_instance_t *input_ressource_RequestSout( input_ressource_t *p_ressource, sout_instance_t *p_sout, const char *psz_sout )
{
    vlc_mutex_lock( &p_ressource->lock );
    sout_instance_t *p_ret = RequestSout( p_ressource, p_sout, psz_sout );
    vlc_mutex_unlock( &p_ressource->lock );

    return p_ret;
}
sout_instance_t *input_ressource_ExtractSout( input_ressource_t *p_ressource )
{
    vlc_mutex_lock( &p_ressource->lock );
    sout_instance_t *p_ret = ExtractSout( p_ressource );
    vlc_mutex_unlock( &p_ressource->lock );

    return p_ret;
}


