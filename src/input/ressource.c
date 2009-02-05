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
#include "../video_output/vout_control.h"
#include "input_interface.h"
#include "ressource.h"

struct input_ressource_t
{
    /* This lock is used to serialize request and protect
     * our variables */
    vlc_mutex_t    lock;

    /* */
    input_thread_t *p_input;

    sout_instance_t *p_sout;
    vout_thread_t   *p_vout_free;
    aout_instance_t *p_aout;

    /* This lock is used to protect vout ressources access (for hold)
     * It is a special case because of embed video (possible deadlock
     * between vout window request and vout holds in some(qt4) interface) */
    vlc_mutex_t    lock_vout;
    int             i_vout;
    vout_thread_t   **pp_vout;
};

/* */
static void DestroySout( input_ressource_t *p_ressource )
{
#ifdef ENABLE_SOUT
    if( p_ressource->p_sout )
        sout_DeleteInstance( p_ressource->p_sout );
#endif
    p_ressource->p_sout = NULL;
}
static sout_instance_t *RequestSout( input_ressource_t *p_ressource,
                                     sout_instance_t *p_sout, const char *psz_sout )
{
#ifdef ENABLE_SOUT
    if( !p_sout && !psz_sout )
    {
        if( p_ressource->p_sout )
            msg_Dbg( p_ressource->p_sout, "destroying useless sout" );
        DestroySout( p_ressource );
        return NULL;
    }

    assert( p_ressource->p_input );
    assert( !p_sout || ( !p_ressource->p_sout && !psz_sout ) );

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

        p_sout = p_ressource->p_sout;
        p_ressource->p_sout = NULL;

        return p_sout;
    }
    else
    {
        vlc_object_detach( p_sout );
        p_ressource->p_sout = p_sout;

        return NULL;
    }
#else
    return NULL;
#endif
}

/* */
static void DestroyVout( input_ressource_t *p_ressource )
{
    assert( p_ressource->i_vout == 0 );

    if( p_ressource->p_vout_free )
        vout_CloseAndRelease( p_ressource->p_vout_free );

    p_ressource->p_vout_free = NULL;
}
static void DisplayVoutTitle( input_ressource_t *p_ressource,
                              vout_thread_t *p_vout )
{
    assert( p_ressource->p_input );

    /* TODO display the title only one time for the same input ? */

    input_item_t *p_item = input_GetItem( p_ressource->p_input );

    char *psz_nowplaying = input_item_GetNowPlaying( p_item );
    if( psz_nowplaying && *psz_nowplaying )
    {
        vout_DisplayTitle( p_vout, psz_nowplaying );
    }
    else
    {
        char *psz_artist = input_item_GetArtist( p_item );
        char *psz_name = input_item_GetTitle( p_item );

        if( !psz_name || *psz_name == '\0' )
        {
            free( psz_name );
            psz_name = input_item_GetName( p_item );
        }
        if( psz_artist && *psz_artist )
        {
            char *psz_string;
            if( asprintf( &psz_string, "%s - %s", psz_name, psz_artist ) != -1 )
            {
                vout_DisplayTitle( p_vout, psz_string );
                free( psz_string );
            }
        }
        else if( psz_name )
        {
            vout_DisplayTitle( p_vout, psz_name );
        }
        free( psz_name );
        free( psz_artist );
    }
    free( psz_nowplaying );
}
static vout_thread_t *RequestVout( input_ressource_t *p_ressource,
                                   vout_thread_t *p_vout, video_format_t *p_fmt )
{
    if( !p_vout && !p_fmt )
    {
        if( p_ressource->p_vout_free )
        {
            msg_Dbg( p_ressource->p_vout_free, "destroying useless vout" );
            vout_CloseAndRelease( p_ressource->p_vout_free );
            p_ressource->p_vout_free = NULL;
        }
        return NULL;
    }

    assert( p_ressource->p_input );
    if( p_fmt )
    {
        /* */
        if( !p_vout && p_ressource->p_vout_free )
        {
            msg_Dbg( p_ressource->p_input, "trying to reuse free vout" );
            p_vout = p_ressource->p_vout_free;

            p_ressource->p_vout_free = NULL;
        }
        else if( p_vout )
        {
            assert( p_vout != p_ressource->p_vout_free );

            vlc_mutex_lock( &p_ressource->lock_vout );
            TAB_REMOVE( p_ressource->i_vout, p_ressource->pp_vout, p_vout );
            vlc_mutex_unlock( &p_ressource->lock_vout );
        }

        /* */
        p_vout = vout_Request( p_ressource->p_input, p_vout, p_fmt );
        if( !p_vout )
            return NULL;

        DisplayVoutTitle( p_ressource, p_vout );

        vlc_mutex_lock( &p_ressource->lock_vout );
        TAB_APPEND( p_ressource->i_vout, p_ressource->pp_vout, p_vout );
        vlc_mutex_unlock( &p_ressource->lock_vout );

        return p_vout;
    }
    else
    {
        assert( p_vout );

        vlc_mutex_lock( &p_ressource->lock_vout );
        TAB_REMOVE( p_ressource->i_vout, p_ressource->pp_vout, p_vout );
        const int i_vout_active = p_ressource->i_vout;
        vlc_mutex_unlock( &p_ressource->lock_vout );

        if( p_ressource->p_vout_free || i_vout_active > 0 )
        {
            msg_Dbg( p_ressource->p_input, "detroying vout (already one saved or active)" );
            vout_CloseAndRelease( p_vout );
        }
        else
        {
            msg_Dbg( p_ressource->p_input, "saving a free vout" );
            vout_Flush( p_vout, 1 );
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
    vlc_mutex_lock( &p_ressource->lock_vout );

    vout_thread_t *p_vout = p_ressource->pp_vout[0];

    vlc_object_hold( p_vout );

    vlc_mutex_unlock( &p_ressource->lock_vout );

    return p_vout;
}
static void HoldVouts( input_ressource_t *p_ressource, vout_thread_t ***ppp_vout, int *pi_vout )
{
    vout_thread_t **pp_vout;

    *pi_vout = 0;
    *ppp_vout = NULL;

    vlc_mutex_lock( &p_ressource->lock_vout );

    if( p_ressource->i_vout <= 0 )
        goto exit;

    pp_vout = calloc( p_ressource->i_vout, sizeof(*pp_vout) );
    if( !pp_vout )
        goto exit;

    *ppp_vout = pp_vout;
    *pi_vout = p_ressource->i_vout;

    for( int i = 0; i < p_ressource->i_vout; i++ )
    {
        pp_vout[i] = p_ressource->pp_vout[i];
        vlc_object_hold( pp_vout[i] );
    }

exit:
    vlc_mutex_unlock( &p_ressource->lock_vout );
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
        msg_Dbg( p_ressource->p_input, "releasing aout" );
        vlc_object_release( p_aout );
        return NULL;
    }
    else
    {
        if( !p_ressource->p_aout )
        {
            msg_Dbg( p_ressource->p_input, "creating aout" );
            p_ressource->p_aout = aout_New( p_ressource->p_input );
        }
        else
        {
            msg_Dbg( p_ressource->p_input, "reusing aout" );
        }

        if( !p_ressource->p_aout )
            return NULL;

        vlc_object_detach( p_ressource->p_aout );
        vlc_object_attach( p_ressource->p_aout, p_ressource->p_input );
        vlc_object_hold( p_ressource->p_aout );
        return p_ressource->p_aout;
    }
}
static aout_instance_t *HoldAout( input_ressource_t *p_ressource )
{
    if( !p_ressource->p_aout )
        return NULL;

    aout_instance_t *p_aout = p_ressource->p_aout;

    vlc_object_hold( p_aout );

    return p_aout;
}

/* */
input_ressource_t *input_ressource_New( void )
{
    input_ressource_t *p_ressource = calloc( 1, sizeof(*p_ressource) );
    if( !p_ressource )
        return NULL;

    vlc_mutex_init( &p_ressource->lock );
    vlc_mutex_init( &p_ressource->lock_vout );
    return p_ressource;
}

void input_ressource_Delete( input_ressource_t *p_ressource )
{
    DestroySout( p_ressource );
    DestroyVout( p_ressource );
    DestroyAout( p_ressource );

    vlc_mutex_destroy( &p_ressource->lock_vout );
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
    return HoldVout( p_ressource );
}
void input_ressource_HoldVouts( input_ressource_t *p_ressource, vout_thread_t ***ppp_vout, int *pi_vout )
{
    HoldVouts( p_ressource, ppp_vout, pi_vout );
}
void input_ressource_TerminateVout( input_ressource_t *p_ressource )
{
    input_ressource_RequestVout( p_ressource, NULL, NULL );
}
bool input_ressource_HasVout( input_ressource_t *p_ressource )
{
    vlc_mutex_lock( &p_ressource->lock );
    assert( !p_ressource->p_input );
    const bool b_vout = p_ressource->p_vout_free != NULL;
    vlc_mutex_unlock( &p_ressource->lock );

    return b_vout;
}

/* */
aout_instance_t *input_ressource_RequestAout( input_ressource_t *p_ressource, aout_instance_t *p_aout )
{
    vlc_mutex_lock( &p_ressource->lock );
    aout_instance_t *p_ret = RequestAout( p_ressource, p_aout );
    vlc_mutex_unlock( &p_ressource->lock );

    return p_ret;
}
aout_instance_t *input_ressource_HoldAout( input_ressource_t *p_ressource )
{
    vlc_mutex_lock( &p_ressource->lock );
    aout_instance_t *p_ret = HoldAout( p_ressource );
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
void input_ressource_TerminateSout( input_ressource_t *p_ressource )
{
    input_ressource_RequestSout( p_ressource, NULL, NULL );
}

