/*****************************************************************************
 * stream_output.c : stream output module
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: stream_output.c,v 1.12 2003/01/17 15:26:24 fenrir Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Erioc Petit <titer@videolan.org>
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
static int      InitInstance      ( sout_instance_t * );

/*****************************************************************************
 * sout_NewInstance: creates a new stream output instance
 *****************************************************************************/
sout_instance_t * __sout_NewInstance ( vlc_object_t *p_parent,
                                       char * psz_dest )
{
    sout_instance_t * p_sout;

    /* Allocate descriptor */
    p_sout = vlc_object_create( p_parent, VLC_OBJECT_SOUT );
    if( p_sout == NULL )
    {
        msg_Err( p_parent, "out of memory" );
        return NULL;
    }

    p_sout->psz_dest = strdup( psz_dest );

    if ( InitInstance( p_sout ) == -1 )
    {
        vlc_object_destroy( p_sout );
        return NULL;
    }

    vlc_object_attach( p_sout, p_parent );

    return p_sout;
}

/*****************************************************************************
 * InitInstance: opens appropriate modules
 *****************************************************************************/
static int InitInstance( sout_instance_t * p_sout )
{
    /* Parse dest string. Syntax : [[<access>][/<mux>]:][<dest>] */
    /* This code is identical to input.c:InitThread. FIXME : factorize it ? */
    char * psz_parser = p_sout->psz_dest;

    p_sout->psz_access = "";
    p_sout->psz_mux = "";
    p_sout->psz_name = "";
    p_sout->p_access = NULL;
    p_sout->p_mux = NULL;
    p_sout->i_access_preheader = 0;
    p_sout->i_mux_preheader = 0;
    p_sout->i_nb_inputs = 0;
    p_sout->pp_inputs = NULL;
    vlc_mutex_init( p_sout, &p_sout->lock );

    /* Skip the plug-in names */
    while( *psz_parser && *psz_parser != ':' )
    {
        psz_parser++;
    }
#if defined( WIN32 ) || defined( UNDER_CE )
    if( psz_parser - p_sout->psz_dest == 1 )
    {
        msg_Warn( p_sout, "drive letter %c: found in source string",
                          *p_sout->psz_dest ) ;
        psz_parser = "";
    }
#endif

    if( !*psz_parser )
    {
        p_sout->psz_access = p_sout->psz_mux = "";
        p_sout->psz_name = p_sout->psz_dest;
    }
    else
    {
        *psz_parser++ = '\0';

        /* let's skip '//' */
        if( psz_parser[0] == '/' && psz_parser[1] == '/' )
        {
            psz_parser += 2 ;
        } 

        p_sout->psz_name = psz_parser ;

        /* Come back to parse the access and mux plug-ins */
        psz_parser = p_sout->psz_dest;

        if( !*psz_parser )
        {
            /* No access */
            p_sout->psz_access = "";
        }
        else if( *psz_parser == '/' )
        {
            /* No access */
            p_sout->psz_access = "";
            psz_parser++;
        }
        else
        {
            p_sout->psz_access = psz_parser;

            while( *psz_parser && *psz_parser != '/' )
            {
                psz_parser++;
            }

            if( *psz_parser == '/' )
            {
                *psz_parser++ = '\0';
            }
        }

        if( !*psz_parser )
        {
            /* No mux */
            p_sout->psz_mux = "";
        }
        else
        {
            p_sout->psz_mux = psz_parser;
        }
    }

    msg_Dbg( p_sout, "access `%s', mux `%s', name `%s'",
             p_sout->psz_access, p_sout->psz_mux, p_sout->psz_name );


    /* Find and open appropriate access module */
    p_sout->p_access =
        module_Need( p_sout, "sout access", p_sout->psz_access );

    if( p_sout->p_access == NULL )
    {
        msg_Err( p_sout, "no suitable sout access module for `%s/%s://%s'",
                 p_sout->psz_access, p_sout->psz_mux, p_sout->psz_name );
        return -1;
    }

    /* Find and open appropriate mux module */
    p_sout->p_mux =
        module_Need( p_sout, "sout mux", p_sout->psz_mux );

    if( p_sout->p_mux == NULL )
    {
        msg_Err( p_sout, "no suitable mux module for `%s/%s://%s'",
                 p_sout->psz_access, p_sout->psz_mux, p_sout->psz_name );
        module_Unneed( p_sout, p_sout->p_access );
        return -1;
    }

    p_sout->i_nb_inputs = 0;
    p_sout->pp_inputs = NULL;

    return 0;
}


/*****************************************************************************
 * sout_DeleteInstance: delete a previously allocated instance
 *****************************************************************************/
void sout_DeleteInstance( sout_instance_t * p_sout )
{
    /* Unlink object */
    vlc_object_detach( p_sout );
    if( p_sout->p_mux )
    {
        module_Unneed( p_sout, p_sout->p_mux );
    }
    if( p_sout->p_access )
    {
        module_Unneed( p_sout, p_sout->p_access );
    }

    vlc_mutex_destroy( &p_sout->lock );

    /* Free structure */
    vlc_object_destroy( p_sout );
}


/*****************************************************************************
 *
 *****************************************************************************/
sout_input_t *__sout_InputNew( vlc_object_t *p_this,
                                sout_packet_format_t *p_format )
{
    sout_instance_t *p_sout = NULL;
    sout_input_t    *p_input;
    int             i_try;

    /* search an stream output */
    for( i_try = 0; i_try < 200; i_try++ )
    {
        p_sout = vlc_object_find( p_this, VLC_OBJECT_SOUT, FIND_ANYWHERE );
        if( !p_sout )
        {
            msleep( 100*1000 );
            msg_Dbg( p_this, "waiting for sout" );
        }
        else
        {
            break;
        }
    }

    if( !p_sout )
    {
        msg_Err( p_this, "cannot find any stream ouput" );
        return( NULL );
    }

    msg_Dbg( p_sout, "adding a new input" );

    /* create a new sout input */
    p_input = malloc( sizeof( sout_input_t ) );

    p_input->p_sout = p_sout;
    vlc_mutex_init( p_sout, &p_input->lock );
    memcpy( &p_input->input_format,
            p_format,
            sizeof( sout_packet_format_t ) );
    p_input->p_fifo = sout_FifoCreate( p_sout );
    p_input->p_mux_data = NULL;

    if( p_input->input_format.i_fourcc != VLC_FOURCC( 'n', 'u', 'l', 'l' ) )
    {
        /* add this new one to p_sout */
        vlc_mutex_lock( &p_sout->lock );
        if( p_sout->i_nb_inputs == 0 )
        {
            p_sout->pp_inputs = malloc( sizeof( sout_input_t * ) );
        }
        else
        {
            p_sout->pp_inputs = realloc( p_sout->pp_inputs,
                                        sizeof( sout_input_t * ) *
                                                ( p_sout->i_nb_inputs + 1 ) );
        }
        p_sout->pp_inputs[p_sout->i_nb_inputs] = p_input;
        p_sout->i_nb_inputs++;

        if( p_sout->pf_mux_addstream( p_sout, p_input ) < 0 )
        {
            msg_Err( p_sout, "cannot add this stream" );

            vlc_mutex_unlock( &p_sout->lock );
            sout_InputDelete( p_input );
            vlc_mutex_lock( &p_sout->lock );

            p_input = NULL;
        }
        vlc_mutex_unlock( &p_sout->lock );
    }

    vlc_object_release( p_sout );

    return( p_input );
}


int sout_InputDelete( sout_input_t *p_input )
{
    sout_instance_t     *p_sout = p_input->p_sout;
    int                 i_input;


    msg_Dbg( p_sout, "removing an input" );

    vlc_mutex_lock( &p_sout->lock );

    sout_FifoDestroy( p_sout, p_input->p_fifo );
    vlc_mutex_destroy( &p_input->lock );

    for( i_input = 0; i_input < p_sout->i_nb_inputs; i_input++ )
    {
        if( p_sout->pp_inputs[i_input] == p_input )
        {
            break;
        }
    }
    if( i_input < p_sout->i_nb_inputs )
    {
        if( p_sout->pf_mux_delstream( p_sout, p_input ) < 0 )
        {
            msg_Err( p_sout, "cannot del this stream from mux" );
        }

        /* remove the entry */
        if( p_sout->i_nb_inputs > 1 )
        {
            memmove( &p_sout->pp_inputs[i_input],
                     &p_sout->pp_inputs[i_input+1],
                     (p_sout->i_nb_inputs - i_input - 1) * sizeof( sout_input_t*) );
        }
        else
        {
            free( p_sout->pp_inputs );
        }
        p_sout->i_nb_inputs--;

        if( p_sout->i_nb_inputs == 0 )
        {
            msg_Warn( p_sout, "no more input stream" );
        }
    }
    else if( p_input->input_format.i_fourcc != VLC_FOURCC( 'n', 'u', 'l', 'l' ) )
    {
        msg_Err( p_sout, "cannot find the input to be deleted" );
    }

    free( p_input );

    vlc_mutex_unlock( &p_sout->lock );

    return( 0 );
}


int sout_InputSendBuffer( sout_input_t *p_input, sout_buffer_t *p_buffer )
{
/*    msg_Dbg( p_input->p_sout,
             "send buffer, size:%d", p_buffer->i_size ); */

    if( p_input->input_format.i_fourcc != VLC_FOURCC( 'n', 'u', 'l', 'l' ) )
    {
        sout_FifoPut( p_input->p_fifo, p_buffer );

        vlc_mutex_lock( &p_input->p_sout->lock );
        p_input->p_sout->pf_mux( p_input->p_sout );
        vlc_mutex_unlock( &p_input->p_sout->lock );

    }
    else
    {
        sout_BufferDelete( p_input->p_sout, p_buffer );
    }

    return( 0 );
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
    size_t        i_prehader;

#ifdef DEBUG_BUFFER
    msg_Dbg( p_sout, "allocating an new buffer, size:%d", (uint32_t)i_size );
#endif

    p_buffer = malloc( sizeof( sout_buffer_t ) );
    i_prehader = p_sout->i_access_preheader + p_sout->i_mux_preheader;

    if( i_size > 0 )
    {
        p_buffer->p_allocated_buffer = malloc( i_size + i_prehader );
        p_buffer->p_buffer = p_buffer->p_allocated_buffer + i_prehader;
    }
    else
    {
        p_buffer->p_allocated_buffer = NULL;
        p_buffer->p_buffer = NULL;
    }
    p_buffer->i_allocated_size = i_size + i_prehader;
    p_buffer->i_buffer_size = i_size;

    p_buffer->i_size = i_size;
    p_buffer->i_length = 0;
    p_buffer->i_dts = 0;
    p_buffer->i_pts = 0;
    p_buffer->i_bitrate = 0;
    p_buffer->p_next = NULL;

    return( p_buffer );
}
int sout_BufferRealloc( sout_instance_t *p_sout, sout_buffer_t *p_buffer, size_t i_size )
{
    size_t          i_prehader;

#ifdef DEBUG_BUFFER
    msg_Dbg( p_sout,
             "realloc buffer old size:%d new size:%d",
             (uint32_t)p_buffer->i_allocated_size,
             (uint32_t)i_size );
#endif

    i_prehader = p_buffer->p_buffer - p_buffer->p_allocated_buffer;

    if( !( p_buffer->p_allocated_buffer = realloc( p_buffer->p_allocated_buffer, i_size + i_prehader ) ) )
    {
        msg_Err( p_sout, "realloc failed" );
        p_buffer->i_allocated_size = 0;
        p_buffer->i_buffer_size = 0;
        p_buffer->i_size = 0;
        p_buffer->p_buffer = NULL;
        return( -1 );
    }
    p_buffer->p_buffer = p_buffer->p_allocated_buffer + i_prehader;

    p_buffer->i_allocated_size = i_size + i_prehader;
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
    else
    {
        sout_buffer_t *p = *pp_chain;

        while( p->p_next )
        {
            p = p->p_next;
        }

        p->p_next = p_buffer;
    }
}
