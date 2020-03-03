/*****************************************************************************
 * bridge.c: bridge stream output module
 *****************************************************************************
 * Copyright (C) 2005-2008 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Antoine Cellerier <dionoea at videolan dot org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ID_TEXT N_("ID")
#define ID_LONGTEXT N_( \
    "Integer identifier for this elementary stream. This will be used to " \
    "\"find\" this stream later." )

#define DEST_TEXT N_( "Destination bridge-in name" )
#define DEST_LONGTEXT N_( \
    "Name of the destination bridge-in. If you do not need more " \
    "than one bridge-in at a time, you can discard this option." )

#define DELAY_TEXT N_("Delay")
#define DELAY_LONGTEXT N_("Pictures coming from the picture video outputs " \
        "will be delayed according to this value (in milliseconds, should be "\
        ">= 100 ms). For high values, you will need to raise caching values." )

#define ID_OFFSET_TEXT N_("ID Offset")
#define ID_OFFSET_LONGTEXT N_("Offset to add to the stream IDs specified in " \
        "bridge_out to obtain the stream IDs bridge_in will register.")

#define NAME_TEXT N_( "Name of current instance" )
#define NAME_LONGTEXT N_( \
    "Name of this bridge-in instance. If you do not need more " \
    "than one bridge-in at a time, you can discard this option." )

#define PLACEHOLDER_TEXT N_( "Fallback to placeholder stream when out of data" )
#define PLACEHOLDER_LONGTEXT N_( \
    "If set to true, the bridge will discard all input elementary streams " \
    "except if it doesn't receive data from another bridge-in. This can " \
    "be used to configure a place holder stream when the real source " \
    "breaks. Source and placeholder streams should have the same format." )

#define PLACEHOLDER_DELAY_TEXT N_( "Placeholder delay" )
#define PLACEHOLDER_DELAY_LONGTEXT N_( \
    "Delay (in ms) before the placeholder kicks in." )

#define PLACEHOLDER_IFRAME_TEXT N_( "Wait for I frame before toggling placeholder" )
#define PLACEHOLDER_IFRAME_LONGTEXT N_( \
    "If enabled, switching between the placeholder and the normal stream " \
    "will only occur on I frames. This will remove artifacts on stream " \
    "switching at the expense of a slightly longer delay, depending on " \
    "the frequence of I frames in the streams." )

static int  OpenOut ( vlc_object_t * );
static void CloseOut( vlc_object_t * );
static int  OpenIn  ( vlc_object_t * );
static void CloseIn ( vlc_object_t * );

#define SOUT_CFG_PREFIX_OUT "sout-bridge-out-"
#define SOUT_CFG_PREFIX_IN "sout-bridge-in-"

vlc_module_begin ()
    set_shortname( N_("Bridge"))
    set_description( N_("Bridge stream output"))
    add_submodule ()
    set_section( N_("Bridge out"), NULL )
    set_capability( "sout output", 50 )
    add_shortcut( "bridge-out" )
    /* Only usable with VLM. No category so not in gui preferences
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_STREAM )*/
    add_integer( SOUT_CFG_PREFIX_OUT "id", 0, ID_TEXT, ID_LONGTEXT,
                 false )
    add_string( SOUT_CFG_PREFIX_OUT "in-name", "default",
                DEST_TEXT, DEST_LONGTEXT, false )
    set_callbacks( OpenOut, CloseOut )

    add_submodule ()
    set_section( N_("Bridge in"), NULL )
    set_capability( "sout filter", 50 )
    add_shortcut( "bridge-in" )
    /*set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_STREAM )*/
    add_integer( SOUT_CFG_PREFIX_IN "delay", 0, DELAY_TEXT,
                 DELAY_LONGTEXT, false )
    add_integer( SOUT_CFG_PREFIX_IN "id-offset", 8192, ID_OFFSET_TEXT,
                 ID_OFFSET_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX_IN "name", "default",
                NAME_TEXT, NAME_LONGTEXT, false )
    add_bool( SOUT_CFG_PREFIX_IN "placeholder", false,
              PLACEHOLDER_TEXT, PLACEHOLDER_LONGTEXT, false )
    add_integer( SOUT_CFG_PREFIX_IN "placeholder-delay", 200,
                 PLACEHOLDER_DELAY_TEXT, PLACEHOLDER_DELAY_LONGTEXT, false )
    add_bool( SOUT_CFG_PREFIX_IN "placeholder-switch-on-iframe", true,
              PLACEHOLDER_IFRAME_TEXT, PLACEHOLDER_IFRAME_LONGTEXT, false )
    set_callbacks( OpenIn, CloseIn )

vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options_out[] = {
    "id", "in-name", NULL
};

static const char *const ppsz_sout_options_in[] = {
    "delay", "id-offset", "name",
    "placeholder", "placeholder-delay", "placeholder-switch-on-iframe",
    NULL
};

static void *AddOut( sout_stream_t *, const es_format_t * );
static void  DelOut( sout_stream_t *, void * );
static int   SendOut( sout_stream_t *, void *, block_t * );

static void *AddIn( sout_stream_t *, const es_format_t * );
static void  DelIn( sout_stream_t *, void * );
static int   SendIn( sout_stream_t *, void *, block_t * );

typedef struct sout_stream_id_sys_t sout_stream_id_sys_t;

typedef struct bridged_es_t
{
    es_format_t fmt;
    block_t *p_block;
    block_t **pp_last;
    bool b_empty;

    /* bridge in part */
    sout_stream_id_sys_t *id;
    vlc_tick_t i_last;
    bool b_changed;
} bridged_es_t;

typedef struct bridge_t
{
    bridged_es_t **pp_es;
    int i_es_num;
} bridge_t;

static vlc_mutex_t lock = VLC_STATIC_MUTEX;

/*
 * Bridge out
 */

typedef struct out_sout_stream_sys_t
{
    bridged_es_t *p_es;
    int i_id;
    bool b_inited;

    char *psz_name;
} out_sout_stream_sys_t;

/*****************************************************************************
 * OpenOut:
 *****************************************************************************/
static int OpenOut( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t *)p_this;
    out_sout_stream_sys_t *p_sys;
    vlc_value_t val;

    config_ChainParse( p_stream, SOUT_CFG_PREFIX_OUT, ppsz_sout_options_out,
                   p_stream->p_cfg );

    p_sys          = malloc( sizeof( out_sout_stream_sys_t ) );
    if( unlikely( !p_sys ) )
        return VLC_ENOMEM;
    p_sys->b_inited = false;

    var_Get( p_stream, SOUT_CFG_PREFIX_OUT "id", &val );
    p_sys->i_id = val.i_int;

    var_Get( p_stream, SOUT_CFG_PREFIX_OUT "in-name", &val );
    if( asprintf( &p_sys->psz_name, "bridge-struct-%s", val.psz_string )<0 )
    {
        free( val.psz_string );
        free( p_sys );
        return VLC_ENOMEM;
    }
    free( val.psz_string );

    p_stream->pf_add    = AddOut;
    p_stream->pf_del    = DelOut;
    p_stream->pf_send   = SendOut;

    p_stream->p_sys     = p_sys;
    p_stream->pace_nocontrol = true;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseOut:
 *****************************************************************************/
static void CloseOut( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    out_sout_stream_sys_t *p_sys = (out_sout_stream_sys_t *)p_stream->p_sys;

    free( p_sys->psz_name );
    free( p_sys );
}

static void *AddOut( sout_stream_t *p_stream, const es_format_t *p_fmt )
{
    vlc_object_t *vlc = VLC_OBJECT(vlc_object_instance(p_stream));
    out_sout_stream_sys_t *p_sys = (out_sout_stream_sys_t *)p_stream->p_sys;
    bridge_t *p_bridge;
    bridged_es_t *p_es;
    int i;

    if ( p_sys->b_inited )
    {
        msg_Err( p_stream, "bridge-out can only handle 1 es at a time." );
        return NULL;
    }
    p_sys->b_inited = true;

    vlc_mutex_lock( &lock );

    p_bridge = var_GetAddress( vlc, p_sys->psz_name );
    if ( p_bridge == NULL )
    {
        p_bridge = xmalloc( sizeof( bridge_t ) );

        var_Create( vlc, p_sys->psz_name, VLC_VAR_ADDRESS );
        var_SetAddress( vlc, p_sys->psz_name, p_bridge );

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
        p_bridge->pp_es = xrealloc( p_bridge->pp_es,
                          (p_bridge->i_es_num + 1) * sizeof(bridged_es_t *) );
        p_bridge->i_es_num++;
        p_bridge->pp_es[i] = xmalloc( sizeof(bridged_es_t) );
    }

    p_sys->p_es = p_es = p_bridge->pp_es[i];

    p_es->fmt = *p_fmt;
    p_es->fmt.i_id = p_sys->i_id;
    p_es->p_block = NULL;
    p_es->pp_last = &p_es->p_block;
    p_es->b_empty = false;

    p_es->id = NULL;
    p_es->i_last = VLC_TICK_INVALID;
    p_es->b_changed = true;

    msg_Dbg( p_stream, "bridging out input codec=%4.4s id=%d pos=%d",
             (char*)&p_es->fmt.i_codec, p_es->fmt.i_id, i );

    vlc_mutex_unlock( &lock );

    return p_sys;
}

static void DelOut( sout_stream_t *p_stream, void *id )
{
    VLC_UNUSED(id);
    out_sout_stream_sys_t *p_sys = (out_sout_stream_sys_t *)p_stream->p_sys;
    bridged_es_t *p_es;

    if ( !p_sys->b_inited )
        return;

    vlc_mutex_lock( &lock );

    p_es = p_sys->p_es;

    p_es->b_empty = true;
    block_ChainRelease( p_es->p_block );
    p_es->p_block = NULL;

    p_es->b_changed = true;
    vlc_mutex_unlock( &lock );

    p_sys->b_inited = false;
}

static int SendOut( sout_stream_t *p_stream, void *id, block_t *p_buffer )
{
    out_sout_stream_sys_t *p_sys = (out_sout_stream_sys_t *)p_stream->p_sys;
    bridged_es_t *p_es;

    if ( (out_sout_stream_sys_t *)id != p_sys )
    {
        block_ChainRelease( p_buffer );
        return VLC_SUCCESS;
    }

    vlc_mutex_lock( &lock );

    p_es = p_sys->p_es;
    *p_es->pp_last = p_buffer;
    while ( p_buffer != NULL )
    {
        p_es->pp_last = &p_buffer->p_next;
        p_buffer = p_buffer->p_next;
    }

    vlc_mutex_unlock( &lock );

    return VLC_SUCCESS;
}


/*
 * Bridge in
 */

typedef struct in_sout_stream_sys_t
{
    int i_id_offset;
    vlc_tick_t i_delay;

    char *psz_name;

    bool b_placeholder;
    bool b_switch_on_iframe;
    int i_state;
    vlc_tick_t i_placeholder_delay;
    sout_stream_id_sys_t *id_video;
    vlc_tick_t i_last_video;
    sout_stream_id_sys_t *id_audio;
    vlc_tick_t i_last_audio;
} in_sout_stream_sys_t;

enum { placeholder_on, placeholder_off };

/*****************************************************************************
 * OpenIn:
 *****************************************************************************/
static int OpenIn( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    in_sout_stream_sys_t *p_sys;
    vlc_value_t val;

    p_sys          = malloc( sizeof( in_sout_stream_sys_t ) );
    if( unlikely( !p_sys ) )
        return VLC_ENOMEM;

    config_ChainParse( p_stream, SOUT_CFG_PREFIX_IN, ppsz_sout_options_in,
                   p_stream->p_cfg );

    var_Get( p_stream, SOUT_CFG_PREFIX_IN "id-offset", &val );
    p_sys->i_id_offset = val.i_int;

    var_Get( p_stream, SOUT_CFG_PREFIX_IN "delay", &val );
    p_sys->i_delay = VLC_TICK_FROM_MS(val.i_int);

    var_Get( p_stream, SOUT_CFG_PREFIX_IN "name", &val );
    if( asprintf( &p_sys->psz_name, "bridge-struct-%s", val.psz_string )<0 )
    {
        free( val.psz_string );
        free( p_sys );
        return VLC_ENOMEM;
    }
    free( val.psz_string );

    var_Get( p_stream, SOUT_CFG_PREFIX_IN "placeholder", &val );
    p_sys->b_placeholder = val.b_bool;

    var_Get( p_stream, SOUT_CFG_PREFIX_IN "placeholder-switch-on-iframe", &val);
    p_sys->b_switch_on_iframe = val.b_bool;

    p_sys->i_state = placeholder_on;

    var_Get( p_stream, SOUT_CFG_PREFIX_IN "placeholder-delay", &val );
    p_sys->i_placeholder_delay = VLC_TICK_FROM_MS(val.i_int);

    p_sys->i_last_video = VLC_TICK_INVALID;
    p_sys->i_last_audio = VLC_TICK_INVALID;
    p_sys->id_video = NULL;
    p_sys->id_audio = NULL;

    p_stream->pf_add    = AddIn;
    p_stream->pf_del    = DelIn;
    p_stream->pf_send   = SendIn;

    p_stream->p_sys     = p_sys;
    p_stream->pace_nocontrol = true;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseIn:
 *****************************************************************************/
static void CloseIn( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    in_sout_stream_sys_t *p_sys = (in_sout_stream_sys_t *)p_stream->p_sys;

    free( p_sys->psz_name );
    free( p_sys );
}

struct sout_stream_id_sys_t
{
    sout_stream_id_sys_t *id;
    enum es_format_category_e i_cat; /* es category. Used for placeholder option */
};

static void* AddIn( sout_stream_t *p_stream, const es_format_t *p_fmt )
{
    in_sout_stream_sys_t *p_sys = (in_sout_stream_sys_t *)p_stream->p_sys;

    sout_stream_id_sys_t *id = malloc( sizeof( sout_stream_id_sys_t ) );
    if( !id ) return NULL;

    id->id = sout_StreamIdAdd( p_stream->p_next, p_fmt );
    if( !id->id )
    {
        free( id );
        return NULL;
    }

    if( p_sys->b_placeholder )
    {
        id->i_cat = p_fmt->i_cat;
        switch( p_fmt->i_cat )
        {
            case VIDEO_ES:
                if( p_sys->id_video != NULL )
                    msg_Err( p_stream, "We already had a video es!" );
                p_sys->id_video = id->id;
                break;
            case AUDIO_ES:
                if( p_sys->id_audio != NULL )
                    msg_Err( p_stream, "We already had an audio es!" );
                p_sys->id_audio = id->id;
                break;
            default:
                break;
        }
    }

    return id;
}

static void DelIn( sout_stream_t *p_stream, void *_id )
{
    in_sout_stream_sys_t *p_sys = (in_sout_stream_sys_t *)p_stream->p_sys;
    sout_stream_id_sys_t *id = (sout_stream_id_sys_t *)_id;

    if( id == p_sys->id_video ) p_sys->id_video = NULL;
    if( id == p_sys->id_audio ) p_sys->id_audio = NULL;

    sout_StreamIdDel( p_stream->p_next, id->id );
    free( id );
}

static int SendIn( sout_stream_t *p_stream, void *_id, block_t *p_buffer )
{
    vlc_object_t *vlc = VLC_OBJECT(vlc_object_instance(p_stream));
    in_sout_stream_sys_t *p_sys = (in_sout_stream_sys_t *)p_stream->p_sys;
    sout_stream_id_sys_t *id = (sout_stream_id_sys_t *)_id;
    bridge_t *p_bridge;
    bool b_no_es = true;
    int i;
    vlc_tick_t i_date = vlc_tick_now();

    /* First forward the packet for our own ES */
    if( !p_sys->b_placeholder )
        sout_StreamIdSend( p_stream->p_next, id->id, p_buffer );

    /* Then check all bridged streams */
    vlc_mutex_lock( &lock );

    p_bridge = var_GetAddress( vlc, p_sys->psz_name );

    if( p_bridge )
    {
    for ( i = 0; i < p_bridge->i_es_num; i++ )
    {
        if ( !p_bridge->pp_es[i]->b_empty )
            b_no_es = false;

        while ( p_bridge->pp_es[i]->p_block != NULL
                 && (p_bridge->pp_es[i]->p_block->i_dts + p_sys->i_delay
                       < i_date
                      || p_bridge->pp_es[i]->p_block->i_dts + p_sys->i_delay
                          < p_bridge->pp_es[i]->i_last) )
        {
            block_t *p_block = p_bridge->pp_es[i]->p_block;
            msg_Dbg( p_stream, "dropping a packet (%"PRId64 ")",
                     i_date - p_block->i_dts - p_sys->i_delay );
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
                sout_StreamIdDel( p_stream->p_next, p_bridge->pp_es[i]->id );
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
                if( !p_sys->b_placeholder )
                {
                    p_bridge->pp_es[i]->id = sout_StreamIdAdd(
                                p_stream->p_next, &p_bridge->pp_es[i]->fmt );
                    if ( p_bridge->pp_es[i]->id == NULL )
                    {
                        msg_Warn( p_stream, "couldn't create chain for id %d",
                                  p_bridge->pp_es[i]->fmt.i_id );
                    }
                }
                msg_Dbg( p_stream, "bridging in input codec=%4.4s id=%d pos=%d",
                         (char*)&p_bridge->pp_es[i]->fmt.i_codec,
                         p_bridge->pp_es[i]->fmt.i_id, i );
            }
        }
        p_bridge->pp_es[i]->b_changed = false;

        if ( p_bridge->pp_es[i]->b_empty )
            continue;

        if ( p_bridge->pp_es[i]->p_block == NULL )
        {
            if ( p_bridge->pp_es[i]->id != NULL
                  && p_bridge->pp_es[i]->i_last < i_date )
            {
                if( !p_sys->b_placeholder )
                    sout_StreamIdDel( p_stream->p_next, p_bridge->pp_es[i]->id );
                p_bridge->pp_es[i]->fmt.i_id -= p_sys->i_id_offset;
                p_bridge->pp_es[i]->b_changed = true;
                p_bridge->pp_es[i]->id = NULL;
            }
            continue;
        }

        if ( p_bridge->pp_es[i]->id != NULL || p_sys->b_placeholder)
        {
            block_t *p_block = p_bridge->pp_es[i]->p_block;
            while ( p_block != NULL )
            {
                p_bridge->pp_es[i]->i_last = p_block->i_dts;
                p_block->i_pts += p_sys->i_delay;
                p_block->i_dts += p_sys->i_delay;
                p_block = p_block->p_next;
            }
            sout_stream_id_sys_t *newid = NULL;
            if( p_sys->b_placeholder )
            {
                switch( p_bridge->pp_es[i]->fmt.i_cat )
                {
                    case VIDEO_ES:
                        p_sys->i_last_video = i_date;
                        newid = p_sys->id_video;
                        if( !newid )
                            break;
                        if( !p_sys->b_switch_on_iframe ||
                            p_sys->i_state == placeholder_off ||
                            ( p_bridge->pp_es[i]->fmt.i_cat == VIDEO_ES &&
                              p_bridge->pp_es[i]->p_block->i_flags & BLOCK_FLAG_TYPE_I ) )
                        {
                            sout_StreamIdSend( p_stream->p_next,
                                       newid,
                                       p_bridge->pp_es[i]->p_block );
                            p_sys->i_state = placeholder_off;
                        }
                        break;
                    case AUDIO_ES:
                        newid = p_sys->id_audio;
                        if( !newid )
                            break;
                        p_sys->i_last_audio = i_date;
                        /* fall through */
                    default:
                        sout_StreamIdSend( p_stream->p_next,
                                   newid?newid:p_bridge->pp_es[i]->id,
                                   p_bridge->pp_es[i]->p_block );
                        break;
                }
            }
            else /* !b_placeholder */
                sout_StreamIdSend( p_stream->p_next,
                                       p_bridge->pp_es[i]->id,
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
        for ( i = 0; i < p_bridge->i_es_num; i++ )
            free( p_bridge->pp_es[i] );
        free( p_bridge->pp_es );
        free( p_bridge );
        var_Destroy( vlc, p_sys->psz_name );
    }
    }

    if( p_sys->b_placeholder )
    {
        switch( id->i_cat )
        {
            case VIDEO_ES:
                if( ( p_sys->i_last_video + p_sys->i_placeholder_delay < i_date
                    && (  !p_sys->b_switch_on_iframe
                       || p_buffer->i_flags & BLOCK_FLAG_TYPE_I ) )
                  || p_sys->i_state == placeholder_on )
                {
                    sout_StreamIdSend( p_stream->p_next, id->id, p_buffer );
                    p_sys->i_state = placeholder_on;
                }
                else
                    block_Release( p_buffer );
                break;

            case AUDIO_ES:
                if( p_sys->i_last_audio + p_sys->i_placeholder_delay < i_date )
                    sout_StreamIdSend( p_stream->p_next, id->id, p_buffer );
                else
                    block_Release( p_buffer );
                break;

            default:
                block_Release( p_buffer ); /* FIXME: placeholder subs anyone? */
                break;
        }
    }

    vlc_mutex_unlock( &lock );

    return VLC_SUCCESS;
}
