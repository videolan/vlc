/*****************************************************************************
 * bridge.c: bridge stream output module
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/sout.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ID_TEXT N_("ID")
#define ID_LONGTEXT N_( \
    "Specify an identifier integer for this elementary stream" )

#define DELAY_TEXT N_("Delay")
#define DELAY_LONGTEXT N_("Pictures coming from the picture video outputs " \
        "will be delayed accordingly (in milliseconds, >= 100 ms). For high " \
        "values you will need to raise file-caching and others.")

#define ID_OFFSET_TEXT N_("ID Offset")
#define ID_OFFSET_LONGTEXT N_("Offset to add to the stream IDs specified in " \
        "bridge_out to obtain the stream IDs bridge_in will register.")

static int  OpenOut ( vlc_object_t * );
static void CloseOut( vlc_object_t * );
static int  OpenIn  ( vlc_object_t * );
static void CloseIn ( vlc_object_t * );

#define SOUT_CFG_PREFIX_OUT "sout-bridge-out-"
#define SOUT_CFG_PREFIX_IN "sout-bridge-in-"

vlc_module_begin();
    set_shortname( _("Bridge"));
    set_description( _("Bridge stream output"));
    add_submodule();
    set_section( N_("Bridge out"), NULL );
    set_capability( "sout stream", 50 );
    add_shortcut( "bridge-out" );
    /* Only usable with VLM. No category so not in gui preferences
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_STREAM );*/
    add_integer( SOUT_CFG_PREFIX_OUT "id", 0, NULL, ID_TEXT, ID_LONGTEXT,
                 VLC_FALSE );
    set_callbacks( OpenOut, CloseOut );

    add_submodule();
    set_section( N_("Bridge in"), NULL );
    set_capability( "sout stream", 50 );
    add_shortcut( "bridge-in" );
    /*set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_STREAM );*/
    add_integer( SOUT_CFG_PREFIX_IN "delay", 0, NULL, DELAY_TEXT,
                 DELAY_LONGTEXT, VLC_FALSE );
    add_integer( SOUT_CFG_PREFIX_IN "id-offset", 8192, NULL, ID_OFFSET_TEXT,
                 ID_OFFSET_LONGTEXT, VLC_FALSE );
    set_callbacks( OpenIn, CloseIn );

    var_Create( p_module->p_libvlc, "bridge-lock", VLC_VAR_MUTEX );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *ppsz_sout_options_out[] = {
    "id", NULL
};

static const char *ppsz_sout_options_in[] = {
    "delay", "id-offset", NULL
};

static sout_stream_id_t *AddOut ( sout_stream_t *, es_format_t * );
static int               DelOut ( sout_stream_t *, sout_stream_id_t * );
static int               SendOut( sout_stream_t *, sout_stream_id_t *, block_t * );

static sout_stream_id_t *AddIn ( sout_stream_t *, es_format_t * );
static int               DelIn ( sout_stream_t *, sout_stream_id_t * );
static int               SendIn( sout_stream_t *, sout_stream_id_t *, block_t * );

typedef struct bridged_es_t
{
    es_format_t fmt;
    block_t *p_block;
    block_t **pp_last;
    vlc_bool_t b_empty;

    /* bridge in part */
    sout_stream_id_t *id;
    mtime_t i_last;
    vlc_bool_t b_changed;
} bridged_es_t;

typedef struct bridge_t
{
    bridged_es_t **pp_es;
    int i_es_num;
} bridge_t;

#define GetBridge(a) __GetBridge( VLC_OBJECT(a) )
static bridge_t *__GetBridge( vlc_object_t *p_object )
{
    libvlc_t *p_libvlc = p_object->p_libvlc;
    bridge_t *p_bridge;
    vlc_value_t val;

    if( var_Get( p_libvlc, "bridge-struct", &val ) != VLC_SUCCESS )
    {
        p_bridge = NULL;
    }
    else
    {
        p_bridge = val.p_address;
    }

    return p_bridge;
}


/*
 * Bridge out
 */

typedef struct out_sout_stream_sys_t
{
    vlc_mutex_t *p_lock;
    bridged_es_t *p_es;
    int i_id;
    vlc_bool_t b_inited;
} out_sout_stream_sys_t;

/*****************************************************************************
 * OpenOut:
 *****************************************************************************/
static int OpenOut( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t *)p_this;
    out_sout_stream_sys_t *p_sys;
    vlc_value_t val;

    sout_CfgParse( p_stream, SOUT_CFG_PREFIX_OUT, ppsz_sout_options_out,
                   p_stream->p_cfg );

    p_sys          = malloc( sizeof( out_sout_stream_sys_t ) );
    p_sys->b_inited = VLC_FALSE;

    var_Get( p_this->p_libvlc, "bridge-lock", &val );
    p_sys->p_lock = val.p_address;

    var_Get( p_stream, SOUT_CFG_PREFIX_OUT "id", &val );
    p_sys->i_id = val.i_int;

    p_stream->pf_add    = AddOut;
    p_stream->pf_del    = DelOut;
    p_stream->pf_send   = SendOut;

    p_stream->p_sys     = (sout_stream_sys_t *)p_sys;

    p_stream->p_sout->i_out_pace_nocontrol++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseOut:
 *****************************************************************************/
static void CloseOut( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    out_sout_stream_sys_t *p_sys = (out_sout_stream_sys_t *)p_stream->p_sys;

    p_stream->p_sout->i_out_pace_nocontrol--;

    free( p_sys );
}

static sout_stream_id_t * AddOut( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    out_sout_stream_sys_t *p_sys = (out_sout_stream_sys_t *)p_stream->p_sys;
    bridge_t *p_bridge;
    bridged_es_t *p_es;
    int i;

    if ( p_sys->b_inited )
    {
        return NULL;
    }
    p_sys->b_inited = VLC_TRUE;

    vlc_mutex_lock( p_sys->p_lock );

    p_bridge = GetBridge( p_stream );
    if ( p_bridge == NULL )
    {
        libvlc_t *p_libvlc = p_stream->p_libvlc;
        vlc_value_t val;

        p_bridge = malloc( sizeof( bridge_t ) );

        var_Create( p_libvlc, "bridge-struct", VLC_VAR_ADDRESS );
        val.p_address = p_bridge;
        var_Set( p_libvlc, "bridge-struct", val );

        p_bridge->i_es_num = 0;
        p_bridge->pp_es = NULL;
    }

    for ( i = 0; i < p_bridge->i_es_num; i++ )
    {
        if ( p_bridge->pp_es[i]->b_empty && !p_bridge->pp_es[i]->b_changed )
            break;
    }

    if ( i == p_bridge->i_es_num )
    {
        p_bridge->pp_es = realloc( p_bridge->pp_es,
                                   (p_bridge->i_es_num + 1)
                                     * sizeof(bridged_es_t *) );
        p_bridge->i_es_num++;
        p_bridge->pp_es[i] = malloc( sizeof(bridged_es_t) );
    }

    p_sys->p_es = p_es = p_bridge->pp_es[i];

    p_es->fmt = *p_fmt;
    p_es->fmt.i_id = p_sys->i_id;
    p_es->p_block = NULL;
    p_es->pp_last = &p_es->p_block;
    p_es->b_empty = VLC_FALSE;

    p_es->id = NULL;
    p_es->i_last = 0;
    p_es->b_changed = VLC_TRUE;

    msg_Dbg( p_stream, "bridging out input codec=%4.4s id=%d pos=%d",
             (char*)&p_es->fmt.i_codec, p_es->fmt.i_id, i );

    vlc_mutex_unlock( p_sys->p_lock );

    return (sout_stream_id_t *)p_sys;
}

static int DelOut( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    out_sout_stream_sys_t *p_sys = (out_sout_stream_sys_t *)p_stream->p_sys;
    bridged_es_t *p_es;

    if ( !p_sys->b_inited )
    {
        return VLC_SUCCESS;
    }

    vlc_mutex_lock( p_sys->p_lock );

    p_es = p_sys->p_es;

    p_es->b_empty = VLC_TRUE;
    block_ChainRelease( p_es->p_block );
    p_es->p_block = VLC_FALSE;

    p_es->b_changed = VLC_TRUE;
    vlc_mutex_unlock( p_sys->p_lock );

    p_sys->b_inited = VLC_FALSE;

    return VLC_SUCCESS;
}

static int SendOut( sout_stream_t *p_stream, sout_stream_id_t *id,
                    block_t *p_buffer )
{
    out_sout_stream_sys_t *p_sys = (out_sout_stream_sys_t *)p_stream->p_sys;
    bridged_es_t *p_es;

    if ( (out_sout_stream_sys_t *)id != p_sys )
    {
        block_ChainRelease( p_buffer );
        return VLC_SUCCESS;
    }

    vlc_mutex_lock( p_sys->p_lock );

    p_es = p_sys->p_es;
    *p_es->pp_last = p_buffer;
    while ( p_buffer != NULL )
    {
        p_es->pp_last = &p_buffer->p_next;
        p_buffer = p_buffer->p_next;
    }

    vlc_mutex_unlock( p_sys->p_lock );

    return VLC_SUCCESS;
}


/*
 * Bridge in
 */

typedef struct in_sout_stream_sys_t
{
    sout_stream_t *p_out;
    vlc_mutex_t *p_lock;
    int i_id_offset;
    mtime_t i_delay;
} in_sout_stream_sys_t;

/*****************************************************************************
 * OpenIn:
 *****************************************************************************/
static int OpenIn( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    in_sout_stream_sys_t *p_sys;
    vlc_value_t val;

    p_sys          = malloc( sizeof( in_sout_stream_sys_t ) );

    p_sys->p_out = sout_StreamNew( p_stream->p_sout, p_stream->psz_next );
    if( !p_sys->p_out )
    {
        msg_Err( p_stream, "cannot create chain" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    sout_CfgParse( p_stream, SOUT_CFG_PREFIX_IN, ppsz_sout_options_in,
                   p_stream->p_cfg );

    var_Get( p_this->p_libvlc, "bridge-lock", &val );
    p_sys->p_lock = val.p_address;

    var_Get( p_stream, SOUT_CFG_PREFIX_IN "id-offset", &val );
    p_sys->i_id_offset = val.i_int;

    var_Get( p_stream, SOUT_CFG_PREFIX_IN "delay", &val );
    p_sys->i_delay = (mtime_t)val.i_int * 1000;

    p_stream->pf_add    = AddIn;
    p_stream->pf_del    = DelIn;
    p_stream->pf_send   = SendIn;

    p_stream->p_sys     = (sout_stream_sys_t *)p_sys;

    /* update p_sout->i_out_pace_nocontrol */
    p_stream->p_sout->i_out_pace_nocontrol++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseIn:
 *****************************************************************************/
static void CloseIn( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    in_sout_stream_sys_t *p_sys = (in_sout_stream_sys_t *)p_stream->p_sys;

    sout_StreamDelete( p_sys->p_out );
    p_stream->p_sout->i_out_pace_nocontrol--;

    free( p_sys );
}

static sout_stream_id_t * AddIn( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    in_sout_stream_sys_t *p_sys = (in_sout_stream_sys_t *)p_stream->p_sys;

    return p_sys->p_out->pf_add( p_sys->p_out, p_fmt );
}

static int DelIn( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    in_sout_stream_sys_t *p_sys = (in_sout_stream_sys_t *)p_stream->p_sys;

    return p_sys->p_out->pf_del( p_sys->p_out, id );
}

static int SendIn( sout_stream_t *p_stream, sout_stream_id_t *id,
                   block_t *p_buffer )
{
    in_sout_stream_sys_t *p_sys = (in_sout_stream_sys_t *)p_stream->p_sys;
    bridge_t *p_bridge;
    vlc_bool_t b_no_es = VLC_TRUE;
    int i;

    /* First forward the packet for our own ES */
    p_sys->p_out->pf_send( p_sys->p_out, id, p_buffer );

    /* Then check all bridged streams */
    vlc_mutex_lock( p_sys->p_lock );

    p_bridge = GetBridge( p_stream );
    if ( p_bridge == NULL )
    {
        vlc_mutex_unlock( p_sys->p_lock );
        return VLC_SUCCESS;
    }

    for ( i = 0; i < p_bridge->i_es_num; i++ )
    {
        if ( !p_bridge->pp_es[i]->b_empty )
            b_no_es = VLC_FALSE;

        while ( p_bridge->pp_es[i]->p_block != NULL
                 && (p_bridge->pp_es[i]->p_block->i_dts + p_sys->i_delay
                       < mdate()
                      || p_bridge->pp_es[i]->p_block->i_dts + p_sys->i_delay
                          < p_bridge->pp_es[i]->i_last) )
        {
            block_t *p_block = p_bridge->pp_es[i]->p_block;
            msg_Dbg( p_stream, "dropping a packet (" I64Fd ")",
                     mdate() - p_block->i_dts - p_sys->i_delay );
            p_bridge->pp_es[i]->p_block
                = p_bridge->pp_es[i]->p_block->p_next;
            block_Release( p_block );
        }

        if ( p_bridge->pp_es[i]->p_block == NULL )
        {
            p_bridge->pp_es[i]->pp_last = &p_bridge->pp_es[i]->p_block;
        }

        if ( p_bridge->pp_es[i]->b_changed )
        {
            if ( p_bridge->pp_es[i]->b_empty && p_bridge->pp_es[i]->id != NULL )
            {
                p_sys->p_out->pf_del( p_sys->p_out, p_bridge->pp_es[i]->id );
            }
            else
            {
                /* We need at least two packets to enter the mux. */
                if ( p_bridge->pp_es[i]->p_block == NULL
                      || p_bridge->pp_es[i]->p_block->p_next == NULL )
                {
                    continue;
                }

                p_bridge->pp_es[i]->fmt.i_id += p_sys->i_id_offset;
                p_bridge->pp_es[i]->id = p_sys->p_out->pf_add(
                            p_sys->p_out, &p_bridge->pp_es[i]->fmt );
                if ( p_bridge->pp_es[i]->id == NULL )
                {
                    msg_Warn( p_stream, "couldn't create chain for id %d",
                              p_bridge->pp_es[i]->fmt.i_id );
                }
                msg_Dbg( p_stream, "bridging in input codec=%4.4s id=%d pos=%d",
                         (char*)&p_bridge->pp_es[i]->fmt.i_codec,
                         p_bridge->pp_es[i]->fmt.i_id, i );
            }
        }
        p_bridge->pp_es[i]->b_changed = VLC_FALSE;

        if ( p_bridge->pp_es[i]->b_empty )
            continue;

        if ( p_bridge->pp_es[i]->p_block == NULL )
        {
            if ( p_bridge->pp_es[i]->id != NULL
                  && p_bridge->pp_es[i]->i_last < mdate() )
            {
                p_sys->p_out->pf_del( p_sys->p_out, p_bridge->pp_es[i]->id );
                p_bridge->pp_es[i]->fmt.i_id -= p_sys->i_id_offset;
                p_bridge->pp_es[i]->b_changed = VLC_TRUE;
                p_bridge->pp_es[i]->id = NULL;
            }
            continue;
        }

        if ( p_bridge->pp_es[i]->id != NULL )
        {
            block_t *p_block = p_bridge->pp_es[i]->p_block;
            while ( p_block != NULL )
            {
                p_bridge->pp_es[i]->i_last = p_block->i_dts;
                p_block->i_pts += p_sys->i_delay;
                p_block->i_dts += p_sys->i_delay;
                p_block = p_block->p_next;
            }
            p_sys->p_out->pf_send( p_sys->p_out, p_bridge->pp_es[i]->id,
                                   p_bridge->pp_es[i]->p_block );
        }
        else
        {
            block_ChainRelease( p_bridge->pp_es[i]->p_block );
        }

        p_bridge->pp_es[i]->p_block = NULL;
        p_bridge->pp_es[i]->pp_last = &p_bridge->pp_es[i]->p_block;
    }

    if( b_no_es )
    {
        libvlc_t *p_libvlc = p_stream->p_libvlc;
        for ( i = 0; i < p_bridge->i_es_num; i++ )
            free( p_bridge->pp_es[i] );
        free( p_bridge->pp_es );
        free( p_bridge );
        var_Destroy( p_libvlc, "bridge-struct" );
    }

    vlc_mutex_unlock( p_sys->p_lock );

    return VLC_SUCCESS;
}
