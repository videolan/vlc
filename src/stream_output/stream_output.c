/*****************************************************************************
 * stream_output.c : stream output module
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: stream_output.c,v 1.25 2003/04/16 00:12:36 fenrir Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>

#include <vlc/sout.h>
#undef DEBUG_BUFFER
/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define sout_stream_url_to_chain( p, s ) _sout_stream_url_to_chain( VLC_OBJECT(p), s )
static char *_sout_stream_url_to_chain( vlc_object_t *, char * );

/*
 * Generic MRL parser
 *
 */

typedef struct
{
    char            *psz_access;

    char            *psz_way;

    char *psz_name;
} mrl_t;

/* mrl_Parse: parse psz_mrl and fill p_mrl */
static int  mrl_Parse( mrl_t *p_mrl, char *psz_mrl );
/* mrl_Clean: clean p_mrl  after a call to mrl_Parse */
static void mrl_Clean( mrl_t *p_mrl );

#define FREE( p ) if( p ) { free( p ); (p) = NULL; }

/*****************************************************************************
 * sout_NewInstance: creates a new stream output instance
 *****************************************************************************/
sout_instance_t * __sout_NewInstance ( vlc_object_t *p_parent,
                                       char * psz_dest )
{
    sout_instance_t *p_sout;

    /* *** Allocate descriptor *** */
    p_sout = vlc_object_create( p_parent, VLC_OBJECT_SOUT );
    if( p_sout == NULL )
    {
        msg_Err( p_parent, "out of memory" );
        return NULL;
    }

    /* *** init descriptor *** */
    p_sout->psz_sout    = strdup( psz_dest );
    p_sout->i_preheader = 0;
    p_sout->p_sys       = NULL;

    vlc_mutex_init( p_sout, &p_sout->lock );
    if( psz_dest && psz_dest[0] == '#' )
    {
        p_sout->psz_chain = strdup( &psz_dest[1] );
    }
    else
    {
        p_sout->psz_chain = sout_stream_url_to_chain( p_sout, psz_dest );
        msg_Dbg( p_sout, "using sout chain=`%s'", p_sout->psz_chain );
    }

    p_sout->p_stream = sout_stream_new( p_sout, p_sout->psz_chain );

    if( p_sout->p_stream == NULL )
    {
        msg_Err( p_sout, "stream chained failed for `%s'", p_sout->psz_chain );

        FREE( p_sout->psz_sout );
        FREE( p_sout->psz_chain );

        vlc_object_destroy( p_sout );
        return( NULL );
    }

    vlc_object_attach( p_sout, p_parent );

    return p_sout;
}
/*****************************************************************************
 * sout_DeleteInstance: delete a previously allocated instance
 *****************************************************************************/
void sout_DeleteInstance( sout_instance_t * p_sout )
{
    /* Unlink object */
    vlc_object_detach( p_sout );

    /* *** free all string *** */
    FREE( p_sout->psz_sout );
    FREE( p_sout->psz_chain );

    sout_stream_delete( p_sout->p_stream );
    vlc_mutex_destroy( &p_sout->lock );

    /* *** free structure *** */
    vlc_object_destroy( p_sout );
}

/*****************************************************************************
 * Packetizer/Input
 *****************************************************************************/
sout_packetizer_input_t *__sout_InputNew( vlc_object_t  *p_this,
                                          sout_format_t *p_fmt )
{
    sout_instance_t         *p_sout = NULL;
    sout_packetizer_input_t *p_input;

    int             i_try;

    /* search an stream output */
    for( i_try = 0; i_try < 12; i_try++ )
    {
        p_sout = vlc_object_find( p_this, VLC_OBJECT_SOUT, FIND_ANYWHERE );
        if( p_sout )
        {
            break;
        }

        msleep( 100*1000 );
        msg_Dbg( p_this, "waiting for sout" );
    }

    if( !p_sout )
    {
        msg_Err( p_this, "cannot find any stream ouput" );
        return( NULL );
    }

    msg_Dbg( p_sout, "adding a new input" );

    /* *** create a packetizer input *** */
    p_input         = malloc( sizeof( sout_packetizer_input_t ) );
    p_input->p_sout = p_sout;
    p_input->p_fmt  = p_fmt;

    if( p_fmt->i_fourcc == VLC_FOURCC( 'n', 'u', 'l', 'l' ) )
    {
        vlc_object_release( p_sout );
        return p_input;
    }

    /* *** add it to the stream chain */
    vlc_mutex_lock( &p_sout->lock );
    p_input->id = p_sout->p_stream->pf_add( p_sout->p_stream,
                                            p_fmt );
    vlc_mutex_unlock( &p_sout->lock );

    vlc_object_release( p_sout );

    if( p_input->id == NULL )
    {
        free( p_input );
        return( NULL );
    }

    return( p_input );
}


int sout_InputDelete( sout_packetizer_input_t *p_input )
{
    sout_instance_t     *p_sout = p_input->p_sout;

    msg_Dbg( p_sout, "removing an input" );

    if( p_input->p_fmt->i_fourcc != VLC_FOURCC( 'n', 'u', 'l', 'l' ) )
    {
        vlc_mutex_lock( &p_sout->lock );
        p_sout->p_stream->pf_del( p_sout->p_stream, p_input->id );
        vlc_mutex_unlock( &p_sout->lock );
    }

    free( p_input );

    return( VLC_SUCCESS);
}


int sout_InputSendBuffer( sout_packetizer_input_t *p_input, sout_buffer_t *p_buffer )
{
    sout_instance_t     *p_sout = p_input->p_sout;

    if( p_input->p_fmt->i_fourcc == VLC_FOURCC( 'n', 'u', 'l', 'l' ) )
    {
        sout_BufferDelete( p_input->p_sout, p_buffer );
        return VLC_SUCCESS;
    }


    return( p_sout->p_stream->pf_send( p_sout->p_stream, p_input->id, p_buffer ) );
}

/*****************************************************************************
 * sout_AccessOutNew: allocate a new access out
 *****************************************************************************/
sout_access_out_t *sout_AccessOutNew( sout_instance_t *p_sout,
                                      char *psz_access, char *psz_name )
{
    sout_access_out_t *p_access;

    if( !( p_access = vlc_object_create( p_sout,
                                         sizeof( sout_access_out_t ) ) ) )
    {
        msg_Err( p_sout, "out of memory" );
        return NULL;
    }
    p_access->psz_access = strdup( psz_access ? psz_access : "" );
    p_access->psz_name   = strdup( psz_name ? psz_name : "" );
    p_access->p_sout     = p_sout;
    p_access->p_sys = NULL;
    p_access->pf_seek    = NULL;
    p_access->pf_write   = NULL;

    p_access->p_module   = module_Need( p_access,
                                        "sout access",
                                        p_access->psz_access );;

    if( !p_access->p_module )
    {
        free( p_access->psz_access );
        free( p_access->psz_name );
        vlc_object_destroy( p_access );
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
        module_Unneed( p_access, p_access->p_module );
    }
    free( p_access->psz_access );
    free( p_access->psz_name );

    vlc_object_destroy( p_access );
}

/*****************************************************************************
 * sout_AccessSeek:
 *****************************************************************************/
int  sout_AccessOutSeek( sout_access_out_t *p_access, off_t i_pos )
{
    return( p_access->pf_seek( p_access, i_pos ) );
}

/*****************************************************************************
 * sout_AccessWrite:
 *****************************************************************************/
int  sout_AccessOutWrite( sout_access_out_t *p_access, sout_buffer_t *p_buffer )
{
    return( p_access->pf_write( p_access, p_buffer ) );
}


/*****************************************************************************
 * MuxNew: allocate a new mux
 *****************************************************************************/
sout_mux_t * sout_MuxNew         ( sout_instance_t *p_sout,
                                   char *psz_mux,
                                   sout_access_out_t *p_access )
{
    sout_mux_t *p_mux;

    p_mux = vlc_object_create( p_sout,
                               sizeof( sout_mux_t ) );
    if( p_mux == NULL )
    {
        msg_Err( p_sout, "out of memory" );
        return NULL;
    }

    p_mux->p_sout       = p_sout;
    p_mux->psz_mux      = strdup( psz_mux);
    p_mux->p_access     = p_access;
    p_mux->i_preheader  = 0;
    p_mux->pf_capacity  = NULL;
    p_mux->pf_addstream = NULL;
    p_mux->pf_delstream = NULL;
    p_mux->pf_mux       = NULL;
    p_mux->i_nb_inputs  = 0;
    p_mux->pp_inputs    = NULL;

    p_mux->p_sys        = NULL;

    p_mux->p_module     = module_Need( p_mux,
                                       "sout mux",
                                       p_mux->psz_mux );
    if( p_mux->p_module == NULL )
    {
        FREE( p_mux->psz_mux );

        vlc_object_destroy( p_mux );
        return NULL;
    }

    /* *** probe mux capacity *** */
    if( p_mux->pf_capacity )
    {
        int b_answer;
        if( p_mux->pf_capacity( p_mux,
                                SOUT_MUX_CAP_GET_ADD_STREAM_ANY_TIME,
                                NULL, (void*)&b_answer ) != SOUT_MUX_CAP_ERR_OK )
        {
            b_answer = VLC_FALSE;
        }
        if( b_answer )
        {
            msg_Dbg( p_sout, "muxer support adding stream at any time" );
            p_mux->b_add_stream_any_time = VLC_TRUE;
            p_mux->b_waiting_stream = VLC_FALSE;
        }
        else
        {
            p_mux->b_add_stream_any_time = VLC_FALSE;
            p_mux->b_waiting_stream = VLC_TRUE;
        }
    }
    else
    {
        p_mux->b_add_stream_any_time = VLC_FALSE;
        p_mux->b_waiting_stream = VLC_TRUE;
    }
    p_mux->i_add_stream_start = -1;

    return p_mux;
}

void sout_MuxDelete              ( sout_mux_t *p_mux )
{
    if( p_mux->p_module )
    {
        module_Unneed( p_mux, p_mux->p_module );
    }
    free( p_mux->psz_mux );

    vlc_object_destroy( p_mux );
}

sout_input_t *sout_MuxAddStream( sout_mux_t *p_mux,
                                 sout_format_t *p_fmt )
{
    sout_input_t *p_input;

    if( !p_mux->b_add_stream_any_time && !p_mux->b_waiting_stream)
    {
        msg_Err( p_mux, "cannot add a new stream (unsuported while muxing for this format)" );
        return NULL;
    }
    if( p_mux->i_add_stream_start < 0 )
    {
        /* we wait for one second */
        p_mux->i_add_stream_start = mdate();
    }

    msg_Dbg( p_mux, "adding a new input" );

    /* create a new sout input */
    p_input = malloc( sizeof( sout_input_t ) );
    p_input->p_sout = p_mux->p_sout;
    p_input->p_fmt  = p_fmt;
    p_input->p_fifo = sout_FifoCreate( p_mux->p_sout );
    p_input->p_sys  = NULL;

    TAB_APPEND( p_mux->i_nb_inputs, p_mux->pp_inputs, p_input );
    if( p_mux->pf_addstream( p_mux, p_input ) < 0 )
    {
            msg_Err( p_mux, "cannot add this stream" );
            sout_MuxDeleteStream( p_mux, p_input );
            return( NULL );
    }

    return( p_input );
}

void sout_MuxDeleteStream     ( sout_mux_t *p_mux,
                                sout_input_t *p_input )
{
    int i_index;

    TAB_FIND( p_mux->i_nb_inputs, p_mux->pp_inputs, p_input, i_index );
    if( i_index >= 0 )
    {
        if( p_mux->pf_delstream( p_mux, p_input ) < 0 )
        {
            msg_Err( p_mux, "cannot del this stream from mux" );
        }

        /* remove the entry */
        TAB_REMOVE( p_mux->i_nb_inputs, p_mux->pp_inputs, p_input );

        if( p_mux->i_nb_inputs == 0 )
        {
            msg_Warn( p_mux, "no more input stream for this mux" );
        }

        sout_FifoDestroy( p_mux->p_sout, p_input->p_fifo );
        free( p_input );
    }
}

void sout_MuxSendBuffer       ( sout_mux_t    *p_mux,
                                sout_input_t  *p_input,
                                sout_buffer_t *p_buffer )
{
    sout_FifoPut( p_input->p_fifo, p_buffer );

    if( p_mux->b_waiting_stream )
    {
        if( p_mux->i_add_stream_start > 0 &&
            p_mux->i_add_stream_start + (mtime_t)1500000 < mdate() )
        {
            /* more than 1.5 second, start muxing */
            p_mux->b_waiting_stream = VLC_FALSE;
        }
        else
        {
            return;
        }
    }
    p_mux->pf_mux( p_mux );
}



sout_fifo_t *sout_FifoCreate( sout_instance_t *p_sout )
{
    sout_fifo_t *p_fifo;

    if( !( p_fifo = malloc( sizeof( sout_fifo_t ) ) ) )
    {
        return( NULL );
    }

    vlc_mutex_init( p_sout, &p_fifo->lock );
    vlc_cond_init ( p_sout, &p_fifo->wait );
    p_fifo->i_depth = 0;
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;

    return( p_fifo );
}

void       sout_FifoFree( sout_instance_t *p_sout, sout_fifo_t *p_fifo )
{
    sout_buffer_t *p_buffer;

    vlc_mutex_lock( &p_fifo->lock );
    p_buffer = p_fifo->p_first;
    while( p_buffer )
    {
        sout_buffer_t *p_next;
        p_next = p_buffer->p_next;
        sout_BufferDelete( p_sout, p_buffer );
        p_buffer = p_next;
    }
    vlc_mutex_unlock( &p_fifo->lock );

    return;
}
void       sout_FifoDestroy( sout_instance_t *p_sout, sout_fifo_t *p_fifo )
{
    sout_FifoFree( p_sout, p_fifo );
    vlc_mutex_destroy( &p_fifo->lock );
    vlc_cond_destroy ( &p_fifo->wait );

    free( p_fifo );
}

void        sout_FifoPut( sout_fifo_t *p_fifo, sout_buffer_t *p_buffer )
{
    vlc_mutex_lock( &p_fifo->lock );

    do
    {
        *p_fifo->pp_last = p_buffer;
        p_fifo->pp_last = &p_buffer->p_next;
        p_fifo->i_depth++;

        p_buffer = p_buffer->p_next;

    } while( p_buffer );

    /* warm there is data in this fifo */
    vlc_cond_signal( &p_fifo->wait );
    vlc_mutex_unlock( &p_fifo->lock );
}

sout_buffer_t *sout_FifoGet( sout_fifo_t *p_fifo )
{
    sout_buffer_t *p_buffer;

    vlc_mutex_lock( &p_fifo->lock );

    if( p_fifo->p_first == NULL )
    {
        vlc_cond_wait( &p_fifo->wait, &p_fifo->lock );
    }

    p_buffer = p_fifo->p_first;

    p_fifo->p_first = p_buffer->p_next;
    p_fifo->i_depth--;

    if( p_fifo->p_first == NULL )
    {
        p_fifo->pp_last = &p_fifo->p_first;
    }

    vlc_mutex_unlock( &p_fifo->lock );

    p_buffer->p_next = NULL;
    return( p_buffer );
}

sout_buffer_t *sout_FifoShow( sout_fifo_t *p_fifo )
{
    sout_buffer_t *p_buffer;

    vlc_mutex_lock( &p_fifo->lock );

    if( p_fifo->p_first == NULL )
    {
        vlc_cond_wait( &p_fifo->wait, &p_fifo->lock );
    }

    p_buffer = p_fifo->p_first;

    vlc_mutex_unlock( &p_fifo->lock );

    return( p_buffer );
}

sout_buffer_t *sout_BufferNew( sout_instance_t *p_sout, size_t i_size )
{
    sout_buffer_t *p_buffer;
    size_t        i_preheader;

#ifdef DEBUG_BUFFER
    msg_Dbg( p_sout, "allocating an new buffer, size:%d", (uint32_t)i_size );
#endif

    p_buffer = malloc( sizeof( sout_buffer_t ) );
    i_preheader = p_sout->i_preheader;

    if( i_size > 0 )
    {
        p_buffer->p_allocated_buffer = malloc( i_size + i_preheader );
        p_buffer->p_buffer = p_buffer->p_allocated_buffer + i_preheader;
    }
    else
    {
        p_buffer->p_allocated_buffer = NULL;
        p_buffer->p_buffer = NULL;
    }
    p_buffer->i_allocated_size = i_size + i_preheader;
    p_buffer->i_buffer_size = i_size;

    p_buffer->i_size    = i_size;
    p_buffer->i_length  = 0;
    p_buffer->i_dts     = 0;
    p_buffer->i_pts     = 0;
    p_buffer->i_bitrate = 0;
    p_buffer->i_flags   = 0x0000;
    p_buffer->p_next = NULL;

    return( p_buffer );
}
int sout_BufferRealloc( sout_instance_t *p_sout, sout_buffer_t *p_buffer, size_t i_size )
{
    size_t          i_preheader;

#ifdef DEBUG_BUFFER
    msg_Dbg( p_sout,
             "realloc buffer old size:%d new size:%d",
             (uint32_t)p_buffer->i_allocated_size,
             (uint32_t)i_size );
#endif

    i_preheader = p_buffer->p_buffer - p_buffer->p_allocated_buffer;

    if( !( p_buffer->p_allocated_buffer = realloc( p_buffer->p_allocated_buffer, i_size + i_preheader ) ) )
    {
        msg_Err( p_sout, "realloc failed" );
        p_buffer->i_allocated_size = 0;
        p_buffer->i_buffer_size = 0;
        p_buffer->i_size = 0;
        p_buffer->p_buffer = NULL;
        return( -1 );
    }
    p_buffer->p_buffer = p_buffer->p_allocated_buffer + i_preheader;

    p_buffer->i_allocated_size = i_size + i_preheader;
    p_buffer->i_buffer_size = i_size;

    return( 0 );
}

int sout_BufferReallocFromPreHeader( sout_instance_t *p_sout, sout_buffer_t *p_buffer, size_t i_size )
{
    size_t  i_preheader;

    i_preheader = p_buffer->p_buffer - p_buffer->p_allocated_buffer;

    if( i_preheader < i_size )
    {
        return( -1 );
    }

    p_buffer->p_buffer -= i_size;
    p_buffer->i_size += i_size;
    p_buffer->i_buffer_size += i_size;

    return( 0 );
}

int sout_BufferDelete( sout_instance_t *p_sout, sout_buffer_t *p_buffer )
{
#ifdef DEBUG_BUFFER
    msg_Dbg( p_sout, "freeing buffer, size:%d", p_buffer->i_size );
#endif
    if( p_buffer->p_allocated_buffer )
    {
        free( p_buffer->p_allocated_buffer );
    }
    free( p_buffer );
    return( 0 );
}

sout_buffer_t *sout_BufferDuplicate( sout_instance_t *p_sout,
                                     sout_buffer_t *p_buffer )
{
    sout_buffer_t *p_dup;

    p_dup = sout_BufferNew( p_sout, p_buffer->i_size );

    p_dup->i_bitrate= p_buffer->i_bitrate;
    p_dup->i_dts    = p_buffer->i_dts;
    p_dup->i_pts    = p_buffer->i_pts;
    p_dup->i_length = p_buffer->i_length;
    p_dup->i_flags  = p_buffer->i_flags;
    p_sout->p_vlc->pf_memcpy( p_dup->p_buffer, p_buffer->p_buffer, p_buffer->i_size );

    return( p_dup );
}

void sout_BufferChain( sout_buffer_t **pp_chain,
                       sout_buffer_t *p_buffer )
{
    if( *pp_chain == NULL )
    {
        *pp_chain = p_buffer;
    }
    else if( p_buffer != NULL )
    {
        sout_buffer_t *p = *pp_chain;

        while( p->p_next )
        {
            p = p->p_next;
        }

        p->p_next = p_buffer;
    }
}

static int  mrl_Parse( mrl_t *p_mrl, char *psz_mrl )
{
    char * psz_dup = strdup( psz_mrl );
    char * psz_parser = psz_dup;
    char * psz_access = "";
    char * psz_way = "";
    char * psz_name = "";

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
#if defined( WIN32 ) || defined( UNDER_CE )
    if( psz_parser - psz_dup == 1 )
    {
        /* msg_Warn( p_sout, "drive letter %c: found in source string",
                          *psz_dup ) ; */
        psz_parser = "";
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
    FREE( p_mrl->psz_access );
    FREE( p_mrl->psz_way );
    FREE( p_mrl->psz_name );
}


/****************************************************************************
 ****************************************************************************
 **
 **
 **
 ****************************************************************************
 ****************************************************************************/

/* create a complete chain */
/* chain format:
    module{option=*:option=*}[:module{option=*:...}]
 */

static char *_strndup( char *str, int i_len )
{
    char *p;

    p = malloc( i_len + 1 );
    strncpy( p, str, i_len );
    p[i_len] = '\0';

    return( p );
}

/*
 * parse module{options=str, option="str "}:
 *  return a pointer on the rest
 *  XXX: psz_chain is modified
 */
#define SKIPSPACE( p ) { while( *p && ( *p == ' ' || *p == '\t' ) ) p++; }
/* go accross " " and { } */
static char *_get_chain_end( char *str )
{
    char *p = str;

    SKIPSPACE( p );

    for( ;; )
    {
        if( *p == '{' || *p == '"' || *p == '\'')
        {
            char c;

            if( *p == '{' )
            {
                c = '}';
            }
            else
            {
                c = *p;
            }
            p++;

            for( ;; )
            {
                if( *p == '\0' )
                {
                    return p;
                }

                if( *p == c )
                {
                    p++;
                    return p;
                }
                else if( *p == '{' && c == '}' )
                {
                    p = _get_chain_end( p );
                }
                else
                {
                    p++;
                }
            }
        }
        else if( *p == '\0' || *p == ',' || *p == '}' || *p == ' ' || *p == '\t' )
        {
            return p;
        }
        else
        {
            p++;
        }
    }
}

char * sout_cfg_parser( char **ppsz_name, sout_cfg_t **pp_cfg, char *psz_chain )
{
    sout_cfg_t *p_cfg = NULL;
    char       *p = psz_chain;

    *ppsz_name = NULL;
    *pp_cfg    = NULL;

    SKIPSPACE( p );

    while( *p && *p != '{' && *p != ':' && *p != ' ' && *p != '\t' )
    {
        p++;
    }

    if( p == psz_chain )
    {
        return NULL;
    }

    *ppsz_name = _strndup( psz_chain, p - psz_chain );

    //fprintf( stderr, "name=%s - rest=%s\n", *ppsz_name, p );

    SKIPSPACE( p );

    if( *p == '{' )
    {
        char *psz_name;

        p++;

        for( ;; )
        {
            sout_cfg_t cfg;

            SKIPSPACE( p );

            psz_name = p;

            while( *p && *p != '=' && *p != ',' && *p != '}' && *p != ' ' && *p != '\t' )
            {
                p++;
            }

            //fprintf( stderr, "name=%s - rest=%s\n", psz_name, p );
            if( p == psz_name )
            {
                fprintf( stderr, "invalid options (empty)" );
                break;
            }

            cfg.psz_name = _strndup( psz_name, p - psz_name );

            SKIPSPACE( p );

            if( *p == '=' )
            {
                char *end;

                p++;
#if 0
                SKIPSPACE( p );

                if( *p == '"' )
                {
                    char *end;

                    p++;
                    end = strchr( p, '"' );

                    if( end )
                    {
//                        fprintf( stderr, "##%s -- %s\n", p, end );
                        cfg.psz_value = _strndup( p, end - p );
                        p = end + 1;
                    }
                    else
                    {
                        cfg.psz_value = strdup( p );
                        p += strlen( p );
                    }

                }
                else
                {
                    psz_value = p;
                    while( *p && *p != ',' && *p != '}' && *p != ' ' && *p != '\t' )
                    {
                        p++;
                    }
                    cfg.psz_value = _strndup( psz_value, p - psz_value );
                }
#endif
                end = _get_chain_end( p );
                if( end <= p )
                {
                    cfg.psz_value = NULL;
                }
                else
                {
                    if( *p == '\'' || *p =='"' || *p == '{' )
                    {
                        p++;
                        end--;
                    }
                    if( end <= p )
                    {
                        cfg.psz_value = NULL;
                    }
                    else
                    {
                        cfg.psz_value = _strndup( p, end - p );
                    }
                }

                p = end;
                SKIPSPACE( p );
            }
            else
            {
                cfg.psz_value = NULL;
            }

            cfg.p_next = NULL;
            if( p_cfg )
            {
                p_cfg->p_next = malloc( sizeof( sout_cfg_t ) );
                memcpy( p_cfg->p_next, &cfg, sizeof( sout_cfg_t ) );

                p_cfg = p_cfg->p_next;
            }
            else
            {
                p_cfg = malloc( sizeof( sout_cfg_t ) );
                memcpy( p_cfg, &cfg, sizeof( sout_cfg_t ) );

                *pp_cfg = p_cfg;
            }

            if( *p == ',' )
            {
                p++;
            }

            if( *p == '}' )
            {
                p++;

                break;
            }
        }
    }

    if( *p == ':' )
    {
        return( strdup( p + 1 ) );
    }

    return( NULL );
}





/*
 * XXX name and p_cfg are used (-> do NOT free them)
 */
sout_stream_t *sout_stream_new( sout_instance_t *p_sout,
                                char *psz_chain )
{
    sout_stream_t *p_stream;

    p_stream = vlc_object_create( p_sout, sizeof( sout_stream_t ) );

    if( !p_stream )
    {
        msg_Err( p_sout, "out of memory" );
        return NULL;
    }

    p_stream->p_sout   = p_sout;
    p_stream->p_sys    = NULL;

    p_stream->psz_next = sout_cfg_parser( &p_stream->psz_name, &p_stream->p_cfg, psz_chain);
    msg_Dbg( p_sout, "stream=`%s'", p_stream->psz_name );

    p_stream->p_module =
        module_Need( p_stream, "sout stream", p_stream->psz_name );

    if( !p_stream->p_module )
    {
        /* FIXME */
        vlc_object_destroy( p_stream );
        return NULL;
    }

    return p_stream;
}

void sout_stream_delete( sout_stream_t *p_stream )
{
    sout_cfg_t *p_cfg;

    msg_Dbg( p_stream, "destroying chain... (name=%s)", p_stream->psz_name );
    module_Unneed( p_stream, p_stream->p_module );

    FREE( p_stream->psz_name );
    FREE( p_stream->psz_next );

    p_cfg = p_stream->p_cfg;
    while( p_cfg != NULL )
    {
        sout_cfg_t *p_next;

        p_next = p_cfg->p_next;

        FREE( p_cfg->psz_name );
        FREE( p_cfg->psz_value );
        free( p_cfg );

        p_cfg = p_next;
    }

    msg_Dbg( p_stream, "destroying chain done" );
    vlc_object_destroy( p_stream );
}

static char *_sout_stream_url_to_chain( vlc_object_t *p_this, char *psz_url )
{
    mrl_t       mrl;
    char        *psz_chain, *p;
    char        *psz_vcodec, *psz_acodec;

    mrl_Parse( &mrl, psz_url );
    p = psz_chain = malloc( 500 + strlen( mrl.psz_way ) + strlen( mrl.psz_access ) + strlen( mrl.psz_name ) );

    psz_vcodec = config_GetPsz( p_this, "sout-vcodec" );
    if( psz_vcodec && *psz_vcodec == '\0')
    {
        FREE( psz_vcodec );
    }
    psz_acodec = config_GetPsz( p_this, "sout-acodec" );
    if( psz_acodec && *psz_acodec == '\0' )
    {
        FREE( psz_acodec );
    }
    /* set transcoding */
    if( psz_vcodec || psz_acodec )
    {
        p += sprintf( p, "transcode{" );
        if( psz_vcodec )
        {
            int br;

            p += sprintf( p, "vcodec=%s,", psz_vcodec );

            if( ( br = config_GetInt( p_this, "sout-vbitrate" ) ) > 0 )
            {
                p += sprintf( p, "vb=%d,", br * 1000 );
            }
            free( psz_vcodec );
        }
        if( psz_acodec )
        {
            int br;

            p += sprintf( p, "acodec=%s,", psz_acodec );
            if( ( br = config_GetInt( p_this, "sout-abitrate" ) ) > 0 )
            {
                p += sprintf( p, "ab=%d,", br * 1000 );
            }

            free( psz_acodec );
        }
        p += sprintf( p, "}:" );
    }


    if( config_GetInt( p_this, "sout-display" ) )
    {
        p += sprintf( p, "duplicate{dst=display,dst=std{mux=%s,access=%s,url=\"%s\"}}", mrl.psz_way, mrl.psz_access, mrl.psz_name );
    }
    else
    {
        p += sprintf( p, "std{mux=%s,access=%s,url=\"%s\"}", mrl.psz_way, mrl.psz_access, mrl.psz_name );
    }

    return( psz_chain );
}

