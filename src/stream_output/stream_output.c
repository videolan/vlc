/*****************************************************************************
 * stream_output.c : stream output module
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: stream_output.c,v 1.23 2003/03/31 03:46:11 fenrir Exp $
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
static int          InstanceNewOutput   ( sout_instance_t *, char * );
static int          InstanceMuxNew      ( sout_instance_t *,
                                          char *, char *, char * );

static sout_mux_t * MuxNew              ( sout_instance_t*,
                                          char *, sout_access_out_t * );
static sout_input_t *MuxAddStream       ( sout_mux_t *, sout_packet_format_t * );
static void         MuxDeleteStream     ( sout_mux_t *, sout_input_t * );
static void         MuxDelete           ( sout_mux_t * );

#if 0
typedef struct
{
    /* if muxer doesn't support adding stream at any time then we first wait
     *  for stream then we refuse all stream and start muxing */
    vlc_bool_t  b_add_stream_any_time;
    vlc_bool_t  b_waiting_stream;

    /* we wait one second after first stream added */
    mtime_t     i_add_stream_start;

} sout_instance_sys_mux_t;
#endif

struct sout_instance_sys_t
{
    int i_d_u_m_m_y;
};

/*
 * Generic MRL parser
 *
 */
/* <access>{options}/<way>{options}://<name> */
typedef struct mrl_option_s
{
    struct mrl_option_s *p_next;

    char *psz_name;
    char *psz_value;
} mrl_option_t;

typedef struct
{
    char            *psz_access;
    mrl_option_t    *p_access_options;

    char            *psz_way;
    mrl_option_t    *p_way_options;

    char *psz_name;
} mrl_t;

/* mrl_Parse: parse psz_mrl and fill p_mrl */
static int  mrl_Parse( mrl_t *p_mrl, char *psz_mrl );
/* mrl_Clean: clean p_mrl  after a call to mrl_Parse */
static void mrl_Clean( mrl_t *p_mrl );

/* some macro */
#define TAB_APPEND( count, tab, p )             \
    if( (count) > 0 )                           \
    {                                           \
        (tab) = realloc( (tab), sizeof( void ** ) * ( (count) + 1 ) ); \
    }                                           \
    else                                        \
    {                                           \
        (tab) = malloc( sizeof( void ** ) );    \
    }                                           \
    (void**)(tab)[(count)] = (void*)(p);        \
    (count)++

#define TAB_FIND( count, tab, p, index )        \
    {                                           \
        int _i_;                                \
        (index) = -1;                           \
        for( _i_ = 0; _i_ < (count); _i_++ )    \
        {                                       \
            if((void**)(tab)[_i_]==(void*)(p))  \
            {                                   \
                (index) = _i_;                  \
                break;                          \
            }                                   \
        }                                       \
    }

#define TAB_REMOVE( count, tab, p )             \
    {                                           \
        int i_index;                            \
        TAB_FIND( count, tab, p, i_index );     \
        if( i_index >= 0 )                      \
        {                                       \
            if( count > 1 )                     \
            {                                   \
                memmove( ((void**)tab + i_index),    \
                         ((void**)tab + i_index+1),  \
                         ( (count) - i_index - 1 ) * sizeof( void* ) );\
            }                                   \
            else                                \
            {                                   \
                free( tab );                    \
                (tab) = NULL;                   \
            }                                   \
            (count)--;                          \
        }                                       \
    }

#define FREE( p ) if( p ) { free( p ); (p) = NULL; }

/*****************************************************************************
 * sout_NewInstance: creates a new stream output instance
 *****************************************************************************/
sout_instance_t * __sout_NewInstance ( vlc_object_t *p_parent,
                                       char * psz_dest )
{
    sout_instance_t *p_sout;
    char            *psz_dup, *psz_parser, *psz_pos;

    /* Allocate descriptor */
    p_sout = vlc_object_create( p_parent, VLC_OBJECT_SOUT );
    if( p_sout == NULL )
    {
        msg_Err( p_parent, "out of memory" );
        return NULL;
    }

    p_sout->psz_sout    = NULL;

    p_sout->i_nb_dest   = 0;
    p_sout->ppsz_dest   = NULL;

    p_sout->i_preheader = 0;
    p_sout->i_nb_mux    = 0;
    p_sout->pp_mux      = 0;

    vlc_mutex_init( p_sout, &p_sout->lock );
    p_sout->i_nb_inputs = 0;
    p_sout->pp_inputs   = NULL;

    p_sout->p_sys           = malloc( sizeof( sout_instance_sys_t ) );

    /* now parse psz_sout */
    psz_dup = strdup( psz_dest );
    psz_parser = psz_dup;

    while( ( psz_pos = strchr( psz_parser, '#' ) ) != NULL )
    {
        *psz_pos++ = '\0';

        if( InstanceNewOutput( p_sout, psz_parser ) )
        {
            msg_Err( p_sout, "adding `%s' failed", psz_parser );
        }

        psz_parser = psz_pos;
    }

    if( *psz_parser )
    {
        if( InstanceNewOutput( p_sout, psz_parser ) )
        {
            msg_Err( p_sout, "adding `%s' failed", psz_parser );
        }
    }

    free( psz_dup );

    if( p_sout->i_nb_dest <= 0 )
    {
        msg_Err( p_sout, "all sout failed" );
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
    int i;
    /* Unlink object */
    vlc_object_detach( p_sout );

    /* *** free all string *** */
    FREE( p_sout->psz_sout );

    for( i = 0; i < p_sout->i_nb_dest; i++ )
    {
        FREE( p_sout->ppsz_dest[i] );
    }
    FREE( p_sout->ppsz_dest );

    /* *** there shouldn't be any input ** */
    if( p_sout->i_nb_inputs > 0 )
    {
        msg_Err( p_sout, "i_nb_inputs=%d > 0 !!!!!!", p_sout->i_nb_inputs );
        msg_Err( p_sout, "mmmh I have a bad feeling..." );
    }
    vlc_mutex_destroy( &p_sout->lock );

    /* *** remove all muxer *** */
    for( i = 0; i < p_sout->i_nb_mux; i++ )
    {
        sout_access_out_t *p_access;
#define p_mux p_sout->pp_mux[i]

        p_access = p_mux->p_access;

        MuxDelete( p_mux );
        sout_AccessOutDelete( p_access );
#undef  p_mux
    }
    FREE( p_sout->pp_mux );

#if 0
    for( i = 0; i < p_sout->p_sys->i_nb_mux; i++ )
    {
        FREE( p_sout->p_sys->pp_mux[i] );
    }
    FREE( p_sout->p_sys->pp_mux );
#endif

    /* Free structure */
    vlc_object_destroy( p_sout );
}



/*****************************************************************************
 * InitInstance: opens appropriate modules
 *****************************************************************************/
static int      InstanceNewOutput   (sout_instance_t *p_sout, char *psz_dest )
{
    mrl_t   mrl;
    char * psz_dup;
#if 0
    /* Parse dest string. Syntax : [[<access>][/<mux>]:][<dest>] */
    /* This code is identical to input.c:InitThread. FIXME : factorize it ? */

    char * psz_dup = strdup( psz_dest );
    char * psz_parser = psz_dup;
    char * psz_access = "";
    char * psz_mux = "";
    char * psz_name = "";
    /* *** first parse psz_dest */
    while( *psz_parser && *psz_parser != ':' )
    {
        psz_parser++;
    }
#if defined( WIN32 ) || defined( UNDER_CE )
    if( psz_parser - psz_dup == 1 )
    {
        msg_Warn( p_sout, "drive letter %c: found in source string",
                          *psz_dup ) ;
        psz_parser = "";
    }
#endif

    if( !*psz_parser )
    {
        psz_access = psz_mux = "";
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
            psz_mux = "";
        }
        else
        {
            psz_mux = psz_parser;
        }
    }

    msg_Dbg( p_sout, "access `%s', mux `%s', name `%s'",
             psz_access, psz_mux, psz_name );
#endif

    mrl_Parse( &mrl, psz_dest );
    msg_Dbg( p_sout, "access `%s', mux `%s', name `%s'",
             mrl.psz_access, mrl.psz_way, mrl.psz_name );

    vlc_mutex_lock( &p_sout->lock );
    /* *** create mux *** */

    if( InstanceMuxNew( p_sout, mrl.psz_way, mrl.psz_access, mrl.psz_name ) )
    {
        msg_Err( p_sout, "cannot create sout chain for %s/%s://%s",
                 mrl.psz_access, mrl.psz_way, mrl.psz_name );

        mrl_Clean( &mrl );
        vlc_mutex_unlock( &p_sout->lock );
        return( VLC_EGENERIC );
    }
    mrl_Clean( &mrl );

    /* *** finish all setup *** */
    if( p_sout->psz_sout )
    {
        p_sout->psz_sout =
            realloc( p_sout->psz_sout,
                     strlen( p_sout->psz_sout ) +2+1+ strlen( psz_dest ) );
        strcat( p_sout->psz_sout, "#" );
        strcat( p_sout->psz_sout, psz_dest );
    }
    else
    {
        p_sout->psz_sout = strdup( psz_dest );
    }
    psz_dup = strdup( psz_dest );
    TAB_APPEND( p_sout->i_nb_dest, p_sout->ppsz_dest, psz_dup );
    vlc_mutex_unlock( &p_sout->lock );

    msg_Dbg( p_sout, "complete sout `%s'", p_sout->psz_sout );

    return VLC_SUCCESS;
}

static int      InstanceMuxNew      ( sout_instance_t *p_sout,
                                      char *psz_mux, char *psz_access, char *psz_name )
{
    sout_access_out_t *p_access;
    sout_mux_t        *p_mux;

    /* *** find and open appropriate access module *** */
    p_access =
        sout_AccessOutNew( p_sout, psz_access, psz_name );
    if( p_access == NULL )
    {
        msg_Err( p_sout, "no suitable sout access module for `%s/%s://%s'",
                 psz_access, psz_mux, psz_name );
        return( VLC_EGENERIC );
    }

    /* *** find and open appropriate mux module *** */
    p_mux = MuxNew( p_sout, psz_mux, p_access );
    if( p_mux == NULL )
    {
        msg_Err( p_sout, "no suitable sout mux module for `%s/%s://%s'",
                 psz_access, psz_mux, psz_name );

        sout_AccessOutDelete( p_access );
        return( VLC_EGENERIC );
    }

    p_sout->i_preheader = __MAX( p_sout->i_preheader,
                                 p_mux->i_preheader );

    TAB_APPEND( p_sout->i_nb_mux, p_sout->pp_mux, p_mux );


    return VLC_SUCCESS;
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



static sout_input_t *SoutInputCreate( sout_instance_t *p_sout,
                                      sout_packet_format_t *p_format )
{
    sout_input_t *p_input;

    p_input = malloc( sizeof( sout_input_t ) );

    p_input->p_sout = p_sout;
    memcpy( &p_input->input_format,
            p_format,
            sizeof( sout_packet_format_t ) );
    p_input->p_fifo = sout_FifoCreate( p_sout );
    p_input->p_sys = NULL;

    return p_input;
}

static void SoutInputDestroy( sout_instance_t *p_sout,
                              sout_input_t *p_input )
{
    sout_FifoDestroy( p_sout, p_input->p_fifo );
    free( p_input );
}

/*****************************************************************************
 * Mux*: create/destroy/manipulate muxer.
 *  XXX: for now they are private, but I will near export them
 *       to allow muxer creating private muxer (ogg in avi, flexmux in ts/ps)
 *****************************************************************************/

/*****************************************************************************
 * MuxNew: allocate a new mux
 *****************************************************************************/
static sout_mux_t * MuxNew              ( sout_instance_t *p_sout,
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

static void MuxDelete               ( sout_mux_t *p_mux )
{
    if( p_mux->p_module )
    {
        module_Unneed( p_mux, p_mux->p_module );
    }
    free( p_mux->psz_mux );

    vlc_object_destroy( p_mux );
}

static sout_input_t *MuxAddStream   ( sout_mux_t *p_mux,
                                      sout_packet_format_t *p_format )
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
    p_input = SoutInputCreate( p_mux->p_sout, p_format );

    TAB_APPEND( p_mux->i_nb_inputs, p_mux->pp_inputs, p_input );
    if( p_mux->pf_addstream( p_mux, p_input ) < 0 )
    {
            msg_Err( p_mux, "cannot add this stream" );
            MuxDeleteStream( p_mux, p_input );
            return( NULL );
    }

    return( p_input );
}

static void MuxDeleteStream     ( sout_mux_t *p_mux,
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

        SoutInputDestroy( p_mux->p_sout, p_input );
    }
}

static void MuxSendBuffer       ( sout_mux_t    *p_mux,
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

/*****************************************************************************
 *
 *****************************************************************************/
sout_packetizer_input_t *__sout_InputNew( vlc_object_t *p_this,
                                          sout_packet_format_t *p_format )
{
    sout_instance_t         *p_sout = NULL;
    sout_packetizer_input_t *p_input;
    int             i_try;
    int             i_mux;
    vlc_bool_t      b_accepted = VLC_FALSE;

    /* search an stream output */
    for( i_try = 0; i_try < 12; i_try++ )
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

    /* *** create a packetizer input *** */
    p_input = malloc( sizeof( sout_packetizer_input_t ) );
    p_input->p_sout         = p_sout;
    p_input->i_nb_inputs    = 0;
    p_input->pp_inputs      = NULL;
    p_input->i_nb_mux       = 0;
    p_input->pp_mux         = NULL;
    memcpy( &p_input->input_format,
            p_format,
            sizeof( sout_packet_format_t ) );

    if( p_format->i_fourcc == VLC_FOURCC( 'n', 'u', 'l', 'l' ) )
    {
        vlc_object_release( p_sout );
        return p_input;
    }

    vlc_mutex_lock( &p_sout->lock );
    /* *** add this input to all muxers *** */
    for( i_mux = 0; i_mux < p_sout->i_nb_mux; i_mux++ )
    {
        sout_input_t *p_mux_input;
#define p_mux p_sout->pp_mux[i_mux]

        p_mux_input = MuxAddStream( p_mux, p_format );
        if( p_mux_input )
        {
            TAB_APPEND( p_input->i_nb_inputs, p_input->pp_inputs, p_mux_input );
            TAB_APPEND( p_input->i_nb_mux,    p_input->pp_mux,    p_mux );

            b_accepted = VLC_TRUE;
        }
#undef  p_mux
    }

    if( !b_accepted )
    {
        /* all muxer refuse this stream, so delete it */
        free( p_input );

        vlc_mutex_unlock( &p_sout->lock );
        vlc_object_release( p_sout );
        return( NULL );
    }

    TAB_APPEND( p_sout->i_nb_inputs, p_sout->pp_inputs, p_input );
    vlc_mutex_unlock( &p_sout->lock );

    vlc_object_release( p_sout );

    return( p_input );
}


int sout_InputDelete( sout_packetizer_input_t *p_input )
{
    sout_instance_t     *p_sout = p_input->p_sout;
    int                 i_input;

    msg_Dbg( p_sout, "removing an input" );

    vlc_mutex_lock( &p_sout->lock );

    /* *** remove this input to all muxers *** */
    for( i_input = 0; i_input < p_input->i_nb_inputs; i_input++ )
    {
        MuxDeleteStream( p_input->pp_mux[i_input], p_input->pp_inputs[i_input] );
    }

    TAB_REMOVE( p_sout->i_nb_inputs, p_sout->pp_inputs, p_input );

    free( p_input->pp_inputs );
    free( p_input->pp_mux );

    free( p_input );

    vlc_mutex_unlock( &p_sout->lock );
    return( 0 );
}


int sout_InputSendBuffer( sout_packetizer_input_t *p_input, sout_buffer_t *p_buffer )
{
//    sout_instance_sys_t *p_sys = p_input->p_sout->p_sys;
/*    msg_Dbg( p_input->p_sout,
             "send buffer, size:%d", p_buffer->i_size ); */

    if( p_input->input_format.i_fourcc != VLC_FOURCC( 'n', 'u', 'l', 'l' ) &&
        p_input->i_nb_inputs > 0 )
    {
        int i;

        vlc_mutex_lock( &p_input->p_sout->lock );
        for( i = 0; i < p_input->i_nb_inputs - 1; i++ )
        {
            sout_buffer_t *p_dup;

            p_dup = sout_BufferDuplicate( p_input->p_sout, p_buffer );

            MuxSendBuffer( p_input->pp_mux[i],
                           p_input->pp_inputs[i],
                           p_dup );
        }
        MuxSendBuffer( p_input->pp_mux[p_input->i_nb_inputs-1],
                       p_input->pp_inputs[p_input->i_nb_inputs-1],
                       p_buffer );

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

#if 0
static int mrl_ParseOptions( mrl_option_t **pp_opt, char *psz_options )
{
    mrl_option_t **pp_last = pp_opt;

    char *psz_parser = strdup( psz_options );

    *pp_last = NULL;

    if( *psz_parser == '=' )
    {
        free( psz_parser );
        return( VLC_EGENERIC );
    }
    if( *psz_parser == '{' )
    {
        free( psz_parser );
    }

    for( ;; )
    {
        char *psz_end;
        mrl_option_t opt;

        /* skip space */
        while( *psz_parser && ( *psz_parser == ' ' || *psz_parser == '\t' || *psz_parser == ';' ) )
        {
            psz_parser++;
        }

        if( ( psz_end = strchr( psz_parser, '=' ) ) != NULL )
        {
            opt.p_next = NULL;

            while( psz_end > psz_parser && ( *psz_end == ' ' ||  *psz_end == '\t' ) )
            {
                psz_end--;
            }

            if( psz_end - psz_parser <= 0 )
            {
                return( VLC_EGENERIC );
            }

            *psz_end = '\0';
            opt.psz_name = strdup( psz_parser );

            psz_parser = psz_end + 1;
            if( ( psz_end = strchr( psz_parser, ';' ) ) == NULL &&
                ( psz_end = strchr( psz_parser, '}' ) ) == NULL )
            {
                psz_end = psz_parser + strlen( psz_parser ) + 1;
            }

            opt.psz_value = strdup( psz_parser );

            fprintf( stderr, "option: name=`%s' value=`%s'\n",
                     opt.psz_name,
                     opt.psz_value );
            psz_parser = psz_end + 1;

            *pp_last = malloc( sizeof( mrl_option_t ) );
            **pp_last = opt;
        }
        else
        {
            break;
        }
    }
}
#endif

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

#if 0
    if( ( psz_parser = strchr( psz_access, '{' ) ) != NULL )
    {
        mrl_ParseOptions( &p_mrl->p_access_options, psz_parser );
        *psz_parser = '\0';
    }

    if( ( psz_parser = strchr( psz_way, '{' ) ) != NULL )
    {
        mrl_ParseOptions( &p_mrl->p_way_options, psz_parser );
        *psz_parser = '\0';
    }
#endif

    p_mrl->p_access_options = NULL;
    p_mrl->p_way_options    = NULL;

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



