/*****************************************************************************
 * stream_output.c : stream output module
 *****************************************************************************
 * Copyright (C) 2002-2007 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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

#include <assert.h>

#include <vlc_common.h>

#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>

#include <vlc_sout.h>

#include "stream_output.h"

#include <vlc_meta.h>
#include <vlc_block.h>
#include <vlc_codec.h>
#include <vlc_modules.h>

#include "input/input_interface.h"

#undef DEBUG_BUFFER
/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static char *sout_stream_url_to_chain( bool, const char * );

/*
 * Generic MRL parser
 *
 */

typedef struct
{
    char *psz_access;
    char *psz_way;
    char *psz_name;
} mrl_t;

/* mrl_Parse: parse psz_mrl and fill p_mrl */
static int  mrl_Parse( mrl_t *p_mrl, const char *psz_mrl );
/* mrl_Clean: clean p_mrl  after a call to mrl_Parse */
static void mrl_Clean( mrl_t *p_mrl );

#undef sout_NewInstance

/*****************************************************************************
 * sout_NewInstance: creates a new stream output instance
 *****************************************************************************/
sout_stream_t *sout_NewInstance( vlc_object_t *p_parent, const char *psz_dest )
{
    sout_stream_t *p_sout;
    char *psz_chain;

    assert( psz_dest != NULL );

    if( psz_dest[0] == '#' )
    {
        psz_chain = strdup( &psz_dest[1] );
    }
    else
    {
        psz_chain = sout_stream_url_to_chain(
            var_InheritBool(p_parent, "sout-display"), psz_dest );
    }
    if(!psz_chain)
        return NULL;

    msg_Dbg(p_parent, "creating stream output chain `%s'", psz_chain);

    p_sout = sout_StreamChainNew(p_parent, psz_chain, NULL);
    if (p_sout == NULL)
        msg_Err(p_parent, "failed to create stream output chain `%s'",
                psz_chain);
    free(psz_chain);
    return p_sout;
}

/*****************************************************************************
 * sout_DeleteInstance: delete a previously allocated instance
 *****************************************************************************/
void sout_DeleteInstance( sout_stream_t * p_sout )
{
    sout_StreamChainDelete( p_sout, NULL );
}

/*****************************************************************************
 * Packetizer/Input
 *****************************************************************************/

struct sout_packetizer_input_t
{
    void                *id;
    bool                 b_flushed;
};

sout_packetizer_input_t *sout_InputNew( sout_stream_t *p_sout,
                                        const es_format_t *p_fmt )
{
    sout_packetizer_input_t *p_input;

    /* *** create a packetizer input *** */
    if( !p_fmt->i_codec || !(p_input = malloc(sizeof(sout_packetizer_input_t))) )
        return NULL;

    p_input->b_flushed = false;

    msg_Dbg(p_sout, "adding an output ES for `%4.4s` (%p)",
            (char *)&p_fmt->i_codec, (void *)p_input);

    /* *** add it to the stream chain */
    p_input->id = sout_StreamIdAdd( p_sout, p_fmt );

    if( p_input->id == NULL )
    {
        msg_Warn(p_sout, "failed to add output ES (%p)", (void *)p_input);
        free( p_input );
        p_input = NULL;
    }

    return( p_input );
}

/*****************************************************************************
 *
 *****************************************************************************/
int sout_InputDelete( sout_stream_t *p_sout,
                      sout_packetizer_input_t *p_input )
{

    msg_Dbg(p_sout, "removing an output ES (%p)", (void *)p_input);

    sout_StreamIdDel( p_sout, p_input->id );
    free( p_input );

    return( VLC_SUCCESS);
}

static int sout_InputControlVa( sout_stream_t *p_sout,
                                sout_packetizer_input_t *p_input,
                                int i_query, va_list args )
{
    if( i_query == SOUT_INPUT_SET_SPU_HIGHLIGHT )
    {
        return sout_StreamControl( p_sout, SOUT_STREAM_ID_SPU_HIGHLIGHT,
                                   p_input->id, va_arg(args, void *) );
    }
    return VLC_EGENERIC;
}

int sout_InputControl( sout_stream_t *p_sout,
                       sout_packetizer_input_t *p_input, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = sout_InputControlVa( p_sout, p_input, i_query, args );
    va_end( args );
    return i_result;
}

void sout_InputFlush( sout_stream_t *p_sout,
                      sout_packetizer_input_t *p_input )
{
    sout_StreamFlush( p_sout, p_input->id );
    p_input->b_flushed = true;
}

/*****************************************************************************
 *
 *****************************************************************************/
int sout_InputSendBuffer( sout_stream_t *p_sout,
                          sout_packetizer_input_t *p_input,
                          block_t *p_buffer )
{
    if( p_input->b_flushed )
    {
        p_buffer->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        p_input->b_flushed = false;
    }
    return sout_StreamIdSend( p_sout, p_input->id, p_buffer );
}

#undef sout_AccessOutNew
/*****************************************************************************
 * sout_AccessOutNew: allocate a new access out
 *****************************************************************************/
sout_access_out_t *sout_AccessOutNew( vlc_object_t *p_sout,
                                      const char *psz_access, const char *psz_name )
{
    sout_access_out_t *p_access;
    char              *psz_next;

    p_access = vlc_custom_create( p_sout, sizeof( *p_access ), "access out" );
    if( !p_access )
        return NULL;

    psz_next = config_ChainCreate( &p_access->psz_access, &p_access->p_cfg,
                                   psz_access );
    free( psz_next );
    p_access->psz_path   = strdup( psz_name ? psz_name : "" );
    if( unlikely(p_access->psz_path == NULL) )
        goto error;
    p_access->p_sys      = NULL;
    p_access->pf_seek    = NULL;
    p_access->pf_read    = NULL;
    p_access->pf_write   = NULL;
    p_access->pf_control = NULL;
    p_access->p_module   = NULL;

    p_access->p_module   =
        module_need( p_access, "sout access", p_access->psz_access, true );

    if( !p_access->p_module )
    {
        free( p_access->psz_path );
error:
        free( p_access->psz_access );
        vlc_object_delete(p_access);
        return( NULL );
    }

    return p_access;
}
/*****************************************************************************
 * sout_AccessDelete: delete an access out
 *****************************************************************************/
void sout_AccessOutDelete( sout_access_out_t *p_access )
{
    if( p_access->p_module )
    {
        module_unneed( p_access, p_access->p_module );
    }
    free( p_access->psz_access );

    config_ChainDestroy( p_access->p_cfg );

    free( p_access->psz_path );

    vlc_object_delete(p_access);
}

/*****************************************************************************
 * sout_AccessSeek:
 *****************************************************************************/
int sout_AccessOutSeek( sout_access_out_t *p_access, off_t i_pos )
{
    if (p_access->pf_seek == NULL)
        return VLC_EGENERIC;
    return p_access->pf_seek( p_access, i_pos );
}

/*****************************************************************************
 * sout_AccessRead:
 *****************************************************************************/
ssize_t sout_AccessOutRead( sout_access_out_t *p_access, block_t *p_buffer )
{
    return( p_access->pf_read ?
            p_access->pf_read( p_access, p_buffer ) : VLC_EGENERIC );
}

/*****************************************************************************
 * sout_AccessWrite:
 *****************************************************************************/
ssize_t sout_AccessOutWrite( sout_access_out_t *p_access, block_t *p_buffer )
{
    return p_access->pf_write( p_access, p_buffer );
}

/**
 * sout_AccessOutControl
 */
int sout_AccessOutControl (sout_access_out_t *access, int query, ...)
{
    va_list ap;
    int ret;

    va_start (ap, query);
    if (access->pf_control)
        ret = access->pf_control (access, query, ap);
    else
        ret = VLC_EGENERIC;
    va_end (ap);
    return ret;
}

/*****************************************************************************
 * sout_MuxNew: create a new mux
 *****************************************************************************/
sout_mux_t *sout_MuxNew( sout_access_out_t *p_access, const char *psz_mux )
{
    sout_mux_t *p_mux;
    char       *psz_next;

    p_mux = vlc_custom_create( p_access, sizeof( *p_mux ), "mux" );
    if( p_mux == NULL )
        return NULL;

    psz_next = config_ChainCreate( &p_mux->psz_mux, &p_mux->p_cfg, psz_mux );
    free( psz_next );

    p_mux->p_access     = p_access;
    p_mux->pf_control   = NULL;
    p_mux->pf_addstream = NULL;
    p_mux->pf_delstream = NULL;
    p_mux->pf_mux       = NULL;
    p_mux->i_nb_inputs  = 0;
    p_mux->pp_inputs    = NULL;

    p_mux->p_sys        = NULL;
    p_mux->p_module     = NULL;

    p_mux->b_add_stream_any_time = false;
    p_mux->b_waiting_stream = true;
    p_mux->i_add_stream_start = VLC_TICK_INVALID;

    p_mux->p_module =
        module_need( p_mux, "sout mux", p_mux->psz_mux, true );

    if( p_mux->p_module == NULL )
    {
        FREENULL( p_mux->psz_mux );

        vlc_object_delete(p_mux);
        return NULL;
    }

    /* *** probe mux capacity *** */
    if( p_mux->pf_control )
    {
        int b_answer = false;

        if( sout_MuxControl( p_mux, MUX_CAN_ADD_STREAM_WHILE_MUXING,
                             &b_answer ) )
        {
            b_answer = false;
        }

        if( b_answer )
        {
            msg_Dbg( p_access, "muxer support adding stream at any time" );
            p_mux->b_add_stream_any_time = true;
            p_mux->b_waiting_stream = true;
        }
    }

    return p_mux;
}

/*****************************************************************************
 * sout_MuxDelete:
 *****************************************************************************/
void sout_MuxDelete( sout_mux_t *p_mux )
{
    if( p_mux->p_module )
    {
        module_unneed( p_mux, p_mux->p_module );
    }
    free( p_mux->psz_mux );

    config_ChainDestroy( p_mux->p_cfg );

    vlc_object_delete(p_mux);
}

/*****************************************************************************
 * sout_MuxAddStream:
 *****************************************************************************/
sout_input_t *sout_MuxAddStream( sout_mux_t *p_mux, const es_format_t *p_fmt )
{
    sout_input_t *p_input;

    if( !p_mux->b_add_stream_any_time && !p_mux->b_waiting_stream )
    {
        msg_Err( p_mux, "cannot add a new stream (unsupported while muxing "
                        "to this format). You can try increasing sout-mux-caching value" );
        return NULL;
    }

    msg_Dbg( p_mux, "adding a new input" );

    /* create a new sout input */
    p_input = malloc( sizeof( sout_input_t ) );
    if( !p_input )
        return NULL;

    // FIXME: remove either fmt or p_fmt...
    es_format_Copy( &p_input->fmt, p_fmt );
    p_input->p_fmt = &p_input->fmt;

    p_input->p_fifo = block_FifoNew();
    p_input->p_sys  = NULL;

    TAB_APPEND( p_mux->i_nb_inputs, p_mux->pp_inputs, p_input );
    if( p_mux->pf_addstream( p_mux, p_input ) < 0 )
    {
        msg_Err( p_mux, "cannot add this stream" );
        TAB_REMOVE( p_mux->i_nb_inputs, p_mux->pp_inputs, p_input );
        block_FifoRelease( p_input->p_fifo );
        es_format_Clean( &p_input->fmt );
        free( p_input );
        return NULL;
    }

    return p_input;
}

/*****************************************************************************
 * sout_MuxDeleteStream:
 *****************************************************************************/
void sout_MuxDeleteStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    int i_index;

    if( p_mux->b_waiting_stream
     && block_FifoCount( p_input->p_fifo ) > 0 )
    {
        /* We stop waiting, and call the muxer for taking care of the data
         * before we remove this es */
        p_mux->b_waiting_stream = false;
        p_mux->pf_mux( p_mux );
    }

    TAB_FIND( p_mux->i_nb_inputs, p_mux->pp_inputs, p_input, i_index );
    if( i_index >= 0 )
    {
        /* remove the entry */
        TAB_ERASE( p_mux->i_nb_inputs, p_mux->pp_inputs, i_index );

        p_mux->pf_delstream( p_mux, p_input );

        if( p_mux->i_nb_inputs == 0 )
        {
            msg_Warn( p_mux, "no more input streams for this mux" );
        }

        block_FifoRelease( p_input->p_fifo );
        es_format_Clean( &p_input->fmt );
        free( p_input );
    }
}

/*****************************************************************************
 * sout_MuxSendBuffer:
 *****************************************************************************/
int sout_MuxSendBuffer( sout_mux_t *p_mux, sout_input_t *p_input,
                         block_t *p_buffer )
{
    vlc_tick_t i_dts = p_buffer->i_dts;
    block_FifoPut( p_input->p_fifo, p_buffer );

    if( i_dts == VLC_TICK_INVALID )
        i_dts = p_buffer->i_pts;

    if( p_mux->b_waiting_stream && i_dts != VLC_TICK_INVALID )
    {
        const vlc_tick_t i_caching = VLC_TICK_FROM_MS(var_InheritInteger( p_mux, "sout-mux-caching" ));

        if( p_mux->i_add_stream_start == VLC_TICK_INVALID )
            p_mux->i_add_stream_start = i_dts;

        /* Wait until we have enough data before muxing */
        if( llabs( i_dts - p_mux->i_add_stream_start ) < i_caching )
            return VLC_SUCCESS;
        p_mux->b_waiting_stream = false;
    }
    return p_mux->pf_mux( p_mux );
}

void sout_MuxFlush( sout_mux_t *p_mux, sout_input_t *p_input )
{
    VLC_UNUSED(p_mux);
    block_FifoEmpty( p_input->p_fifo );
}

/*****************************************************************************
 * sout_MuxGetStream: find stream to be muxed
 *****************************************************************************/
int sout_MuxGetStream( sout_mux_t *p_mux, unsigned i_blocks, vlc_tick_t *pi_dts )
{
    vlc_tick_t i_dts = 0;
    int     i_stream = -1;

    assert( i_blocks > 0 );

    for( int i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        sout_input_t *p_input = p_mux->pp_inputs[i];
        block_t *p_data;

        if( block_FifoCount( p_input->p_fifo ) < i_blocks )
        {
            if( (!p_mux->b_add_stream_any_time) &&
                (p_input->p_fmt->i_cat != SPU_ES ) )
            {
                return -1;
            }
            /* FIXME: SPU muxing */
            continue;
        }

        p_data = block_FifoShow( p_input->p_fifo );
        if( i_stream < 0 || p_data->i_dts < i_dts )
        {
            i_stream = i;
            i_dts    = p_data->i_dts;
        }
    }

    if( pi_dts ) *pi_dts = i_dts;

    return i_stream;
}


/*****************************************************************************
 *
 *****************************************************************************/
static int mrl_Parse( mrl_t *p_mrl, const char *psz_mrl )
{
    char * psz_dup = strdup( psz_mrl );
    char * psz_parser = psz_dup;
    const char * psz_access;
    const char * psz_way;
    char * psz_name;

    /* *** first parse psz_dest */
    while( *psz_parser && *psz_parser != ':' )
    {
        if( *psz_parser == '{' )
        {
            while( *psz_parser && *psz_parser != '}' )
            {
                psz_parser++;
            }
            if( *psz_parser )
            {
                psz_parser++;
            }
        }
        else
        {
            psz_parser++;
        }
    }
#if defined( _WIN32 ) || defined( __OS2__ )
    if( psz_parser - psz_dup == 1 )
    {
        /* msg_Warn( p_sout, "drive letter %c: found in source string",
                          *psz_dup ) ; */
        *psz_parser = '\0';
    }
#endif

    if( !*psz_parser )
    {
        psz_access = psz_way = "";
        psz_name = psz_dup;
    }
    else
    {
        *psz_parser++ = '\0';

        /* let's skip '//' */
        if( psz_parser[0] == '/' && psz_parser[1] == '/' )
        {
            psz_parser += 2 ;
        }

        psz_name = psz_parser ;

        /* Come back to parse the access and mux plug-ins */
        psz_parser = psz_dup;

        if( !*psz_parser )
        {
            /* No access */
            psz_access = "";
        }
        else if( *psz_parser == '/' )
        {
            /* No access */
            psz_access = "";
            psz_parser++;
        }
        else
        {
            psz_access = psz_parser;

            while( *psz_parser && *psz_parser != '/' )
            {
                if( *psz_parser == '{' )
                {
                    while( *psz_parser && *psz_parser != '}' )
                    {
                        psz_parser++;
                    }
                    if( *psz_parser )
                    {
                        psz_parser++;
                    }
                }
                else
                {
                    psz_parser++;
                }
            }

            if( *psz_parser == '/' )
            {
                *psz_parser++ = '\0';
            }
        }

        if( !*psz_parser )
        {
            /* No mux */
            psz_way = "";
        }
        else
        {
            psz_way = psz_parser;
        }
    }

    p_mrl->psz_access = strdup( psz_access );
    p_mrl->psz_way    = strdup( psz_way );
    p_mrl->psz_name   = strdup( psz_name );

    free( psz_dup );
    return( VLC_SUCCESS );
}


/* mrl_Clean: clean p_mrl  after a call to mrl_Parse */
static void mrl_Clean( mrl_t *p_mrl )
{
    FREENULL( p_mrl->psz_access );
    FREENULL( p_mrl->psz_way );
    FREENULL( p_mrl->psz_name );
}


/****************************************************************************
 ****************************************************************************
 **
 **
 **
 ****************************************************************************
 ****************************************************************************/

struct sout_stream_private {
    sout_stream_t stream;
    vlc_mutex_t lock;
    module_t *module;
};

#define sout_stream_priv(s) \
        container_of(s, struct sout_stream_private, stream)

static void sout_StreamLock(sout_stream_t *s)
{
    vlc_mutex_lock(&sout_stream_priv(s)->lock);
}

static void sout_StreamUnlock(sout_stream_t *s)
{
    vlc_mutex_unlock(&sout_stream_priv(s)->lock);
}

void *sout_StreamIdAdd(sout_stream_t *s, const es_format_t *fmt)
{
    void *id;

    sout_StreamLock(s);
    id = s->ops->add(s, fmt);
    sout_StreamUnlock(s);
    return id;
}

void sout_StreamIdDel(sout_stream_t *s, void *id)
{
    sout_StreamLock(s);
    s->ops->del(s, id);
    sout_StreamUnlock(s);
}

int sout_StreamIdSend(sout_stream_t *s, void *id, block_t *b)
{
    int val;

    sout_StreamLock(s);
    val = s->ops->send(s, id, b);
    sout_StreamUnlock(s);
    return val;
}

void sout_StreamFlush(sout_stream_t *s, void *id)
{
    if (s->ops->flush != NULL)
    {
        sout_StreamLock(s);
        s->ops->flush(s, id);
        sout_StreamUnlock(s);
    }
}

int sout_StreamControlVa(sout_stream_t *s, int i_query, va_list args)
{
    int val = VLC_EGENERIC;

    if (s->ops->control != NULL)
    {
        sout_StreamLock(s);
        val = s->ops->control(s, i_query, args);
        sout_StreamUnlock(s);
    }
    return val;
}

/* Destroy a "stream_out" module */
static void sout_StreamDelete( sout_stream_t *p_stream )
{
    struct sout_stream_private *priv = sout_stream_priv(p_stream);

    msg_Dbg( p_stream, "destroying chain... (name=%s)", p_stream->psz_name );

    if (priv->module != NULL)
        module_unneed(p_stream, priv->module);

    FREENULL( p_stream->psz_name );

    config_ChainDestroy( p_stream->p_cfg );

    msg_Dbg( p_stream, "destroying chain done" );
    vlc_object_delete(p_stream);
}

/* Destroy a "stream_out" modules chain
 *
 * p_first is the first module to be destroyed in the chain
 * p_last is the last module to be destroyed
 *  if NULL, all modules are destroyed
 *  if not NULL, modules following it must be destroyed separately
 */
void sout_StreamChainDelete(sout_stream_t *p_first, sout_stream_t *end)
{
    while (p_first != end)
    {
        sout_stream_t *p_next = p_first->p_next;

        sout_StreamDelete(p_first);
        p_first = p_next;
    }
}

/* Create a "stream_out" module, which may forward its ES to p_next module */
/*
 * XXX name and p_cfg are used (-> do NOT free them)
 */
static sout_stream_t *sout_StreamNew( vlc_object_t *parent, char *psz_name,
                               config_chain_t *p_cfg, sout_stream_t *p_next)
{
    const char *cap = (p_next != NULL) ? "sout filter" : "sout output";
    struct sout_stream_private *priv;
    sout_stream_t *p_stream;

    assert(psz_name);

    priv = vlc_custom_create(parent, sizeof (*priv), "stream out");
    if (unlikely(priv == NULL))
        return NULL;

    vlc_mutex_init(&priv->lock);
    p_stream = &priv->stream;
    p_stream->psz_name = psz_name;
    p_stream->p_cfg    = p_cfg;
    p_stream->p_next   = p_next;
    p_stream->ops = NULL;
    p_stream->p_sys = NULL;

    msg_Dbg( p_stream, "stream=`%s'", p_stream->psz_name );

    priv->module = module_need(p_stream, cap, p_stream->psz_name, true);

    if (priv->module == NULL)
    {
        /* those must be freed by the caller if creation failed */
        p_stream->psz_name = NULL;
        p_stream->p_cfg = NULL;

        sout_StreamDelete( p_stream );
        return NULL;
    }

    return p_stream;
}

/* Creates a complete "stream_out" modules chain
 *
 *  chain format: module1{option=*:option=*}[:module2{option=*:...}]
 *
 *  The modules are created starting from the last one and linked together
 *
 *  Returns a pointer to the first module.
 */
sout_stream_t *sout_StreamChainNew(vlc_object_t *parent, const char *psz_chain,
                                   sout_stream_t *sink)
{
    if(!psz_chain || !*psz_chain)
    {
        return sink;
    }

    char *psz_parser = strdup(psz_chain);
    if(!psz_parser)
        return NULL;

    vlc_array_t cfg, name;
    vlc_array_init(&cfg);
    vlc_array_init(&name);

    /* parse chain */
    while(psz_parser)
    {
        config_chain_t *p_cfg;
        char *psz_name;
        char *psz_rest_chain = config_ChainCreate( &psz_name, &p_cfg, psz_parser );
        free( psz_parser );
        psz_parser = psz_rest_chain;

        vlc_array_append_or_abort(&cfg, p_cfg);
        vlc_array_append_or_abort(&name, psz_name);
    }

    /* Instantiate modules from back to front of chain */
    sout_stream_t *front = sink;
    size_t i = vlc_array_count(&name);

    while(i--)
    {
        sout_stream_t *prev;

        prev = sout_StreamNew(parent, vlc_array_item_at_index(&name, i),
                              vlc_array_item_at_index(&cfg, i), front);
        if (prev == NULL)
            goto error;

        front = prev;
    }

    vlc_array_clear(&name);
    vlc_array_clear(&cfg);

    return front;

error:

    i++;    /* last module couldn't be created */

    /* Destroy module instances in LIFO order */
    while (front != sink)
    {
        sout_stream_t *next = front->p_next;

        sout_StreamDelete(front);
        front = next;
    }

    /* then destroy all names and config which weren't destroyed by
     * sout_StreamDelete */
    while(i--)
    {
        free(vlc_array_item_at_index(&name, i));
        config_ChainDestroy(vlc_array_item_at_index(&cfg, i));
    }
    vlc_array_clear(&name);
    vlc_array_clear(&cfg);

    return NULL;
}

static char *sout_stream_url_to_chain( bool b_sout_display,
                                       const char *psz_url )
{
    mrl_t       mrl;
    char        *psz_chain;

    mrl_Parse( &mrl, psz_url );

    /* Check if the URLs goes to #rtp - otherwise we'll use #standard */
    static const char rtplist[] = "dccp\0sctp\0tcp\0udplite\0";
    for (const char *a = rtplist; *a; a += strlen (a) + 1)
        if (strcmp (a, mrl.psz_access) == 0)
            goto rtp;

    if (strcmp (mrl.psz_access, "rtp") == 0)
    {
        char *port;
        /* For historical reasons, rtp:// means RTP over UDP */
        strcpy (mrl.psz_access, "udp");
rtp:
        if (mrl.psz_name[0] == '[')
        {
            port = strstr (mrl.psz_name, "]:");
            if (port != NULL)
                port++;
        }
        else
            port = strchr (mrl.psz_name, ':');
        if (port != NULL)
            *port++ = '\0'; /* erase ':' */

        if (asprintf (&psz_chain,
                      "rtp{mux=\"%s\",proto=\"%s\",dst=\"%s%s%s\"}",
                      mrl.psz_way, mrl.psz_access, mrl.psz_name,
                      port ? "\",port=\"" : "", port ? port : "") == -1)
            psz_chain = NULL;
    }
    else
    {
        /* Convert the URL to a basic standard sout chain */
        if (asprintf (&psz_chain,
                      "standard{mux=\"%s\",access=\"%s\",dst=\"%s\"}",
                      mrl.psz_way, mrl.psz_access, mrl.psz_name) == -1)
            psz_chain = NULL;
    }

    /* Duplicate and wrap if sout-display is on */
    if (psz_chain && b_sout_display)
    {
        char *tmp;
        if (asprintf (&tmp, "duplicate{dst=display,dst=%s}", psz_chain) == -1)
            tmp = NULL;
        free (psz_chain);
        psz_chain = tmp;
    }

    mrl_Clean( &mrl );
    return psz_chain;
}

#undef sout_EncoderCreate
encoder_t *sout_EncoderCreate( vlc_object_t *p_this, size_t owner_size )
{
    assert( owner_size >= sizeof(encoder_t) );
    encoder_t *encoder = vlc_custom_create( p_this, owner_size, "encoder" );
    encoder->b_packetized = true;

    return encoder;
}
