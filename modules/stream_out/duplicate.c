/*****************************************************************************
 * duplicate.c: duplicate stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 *
 * Author: Laurent Aimar <fenrir@via.ecp.fr>
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
static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Duplicate stream output") )
    set_capability( "sout output", 50 )
    add_shortcut( "duplicate", "dup" )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_STREAM )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static void *Add( sout_stream_t *, const es_format_t * );
static void  Del( sout_stream_t *, void * );
static int   Send( sout_stream_t *, void *, block_t * );

typedef struct
{
    int             i_nb_streams;
    sout_stream_t   **pp_streams;

    int             i_nb_last_streams;
    sout_stream_t   **pp_last_streams;

    int             i_nb_select;
    char            **ppsz_select;
} sout_stream_sys_t;

typedef struct
{
    int                 i_nb_ids;
    void                **pp_ids;
} sout_stream_id_sys_t;

static bool ESSelected( const es_format_t *fmt, char *psz_select );

/*****************************************************************************
 * Control
 *****************************************************************************/
static int Control( sout_stream_t *p_stream, int i_query, va_list args )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /* Fanout controls */
    switch( i_query )
    {
        case SOUT_STREAM_ID_SPU_HIGHLIGHT:
        {
            sout_stream_id_sys_t *id = va_arg(args, void *);
            void *spu_hl = va_arg(args, void *);
            for( int i = 0; i < id->i_nb_ids; i++ )
            {
                if( id->pp_ids[i] )
                    sout_StreamControl( p_sys->pp_streams[i], i_query,
                                        id->pp_ids[i], spu_hl );
            }
            return VLC_SUCCESS;
        }
    }

    return VLC_EGENERIC;
}

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    config_chain_t        *p_cfg;

    msg_Dbg( p_stream, "creating 'duplicate'" );

    p_sys = malloc( sizeof( sout_stream_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    TAB_INIT( p_sys->i_nb_streams, p_sys->pp_streams );
    TAB_INIT( p_sys->i_nb_last_streams, p_sys->pp_last_streams );
    TAB_INIT( p_sys->i_nb_select, p_sys->ppsz_select );

    for( p_cfg = p_stream->p_cfg; p_cfg != NULL; p_cfg = p_cfg->p_next )
    {
        if( !strncmp( p_cfg->psz_name, "dst", strlen( "dst" ) ) )
        {
            sout_stream_t *s, *p_last;

            msg_Dbg( p_stream, " * adding `%s'", p_cfg->psz_value );
            s = sout_StreamChainNew( p_stream->p_sout, p_cfg->psz_value,
                p_stream->p_next, &p_last );

            if( s )
            {
                TAB_APPEND( p_sys->i_nb_streams, p_sys->pp_streams, s );
                TAB_APPEND( p_sys->i_nb_last_streams, p_sys->pp_last_streams,
                    p_last );
                TAB_APPEND( p_sys->i_nb_select,  p_sys->ppsz_select, NULL );
            }
        }
        else if( !strncmp( p_cfg->psz_name, "select", strlen( "select" ) ) )
        {
            char *psz = p_cfg->psz_value;
            if( p_sys->i_nb_select > 0 && psz && *psz )
            {
                char **ppsz_select = &p_sys->ppsz_select[p_sys->i_nb_select - 1];

                if( *ppsz_select )
                {
                    msg_Err( p_stream, " * ignore selection `%s' (it already has `%s')",
                             psz, *ppsz_select );
                }
                else
                {
                    msg_Dbg( p_stream, " * apply selection `%s'", psz );
                    *ppsz_select = strdup( psz );
                }
            }
        }
        else
        {
            msg_Err( p_stream, " * ignore unknown option `%s'", p_cfg->psz_name );
        }
    }

    if( p_sys->i_nb_streams == 0 )
    {
        msg_Err( p_stream, "no destination given" );
        free( p_sys );

        return VLC_EGENERIC;
    }

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;
    p_stream->pf_control = Control;

    p_stream->p_sys     = p_sys;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    msg_Dbg( p_stream, "closing a duplication" );
    for( int i = 0; i < p_sys->i_nb_streams; i++ )
    {
        sout_StreamChainDelete(p_sys->pp_streams[i], p_sys->pp_last_streams[i]);
        free( p_sys->ppsz_select[i] );
    }
    free( p_sys->pp_streams );
    free( p_sys->pp_last_streams );
    free( p_sys->ppsz_select );

    free( p_sys );
}

/*****************************************************************************
 * Add:
 *****************************************************************************/
static void *Add( sout_stream_t *p_stream, const es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_sys_t  *id;
    int i_stream, i_valid_streams = 0;

    id = malloc( sizeof( sout_stream_id_sys_t ) );
    if( !id )
        return NULL;

    TAB_INIT( id->i_nb_ids, id->pp_ids );

    msg_Dbg( p_stream, "duplicated a new stream codec=%4.4s (es=%d group=%d)",
             (char*)&p_fmt->i_codec, p_fmt->i_id, p_fmt->i_group );

    for( i_stream = 0; i_stream < p_sys->i_nb_streams; i_stream++ )
    {
        void *id_new = NULL;

        if( ESSelected( p_fmt, p_sys->ppsz_select[i_stream] ) )
        {
            sout_stream_t *out = p_sys->pp_streams[i_stream];

            id_new = (void*)sout_StreamIdAdd( out, p_fmt );
            if( id_new )
            {
                msg_Dbg( p_stream, "    - added for output %d", i_stream );
                i_valid_streams++;
            }
            else
            {
                msg_Dbg( p_stream, "    - failed for output %d", i_stream );
            }
        }
        else
        {
            msg_Dbg( p_stream, "    - ignored for output %d", i_stream );
        }

        /* Append failed attempts as well to keep track of which pp_id
         * belongs to which duplicated stream */
        TAB_APPEND( id->i_nb_ids, id->pp_ids, id_new );
    }

    if( i_valid_streams <= 0 )
    {
        Del( p_stream, id );
        return NULL;
    }

    return id;
}

/*****************************************************************************
 * Del:
 *****************************************************************************/
static void Del( sout_stream_t *p_stream, void *_id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_sys_t *id = (sout_stream_id_sys_t *)_id;
    int               i_stream;

    for( i_stream = 0; i_stream < p_sys->i_nb_streams; i_stream++ )
    {
        if( id->pp_ids[i_stream] )
        {
            sout_stream_t *out = p_sys->pp_streams[i_stream];
            sout_StreamIdDel( out, id->pp_ids[i_stream] );
        }
    }

    free( id->pp_ids );
    free( id );
}

/*****************************************************************************
 * Send:
 *****************************************************************************/
static int Send( sout_stream_t *p_stream, void *_id, block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_sys_t *id = (sout_stream_id_sys_t *)_id;
    sout_stream_t     *p_dup_stream;
    int               i_stream;

    /* Loop through the linked list of buffers */
    while( p_buffer )
    {
        block_t *p_next = p_buffer->p_next;

        p_buffer->p_next = NULL;

        for( i_stream = 0; i_stream < p_sys->i_nb_streams - 1; i_stream++ )
        {
            p_dup_stream = p_sys->pp_streams[i_stream];

            if( id->pp_ids[i_stream] )
            {
                block_t *p_dup = block_Duplicate( p_buffer );

                if( p_dup )
                    sout_StreamIdSend( p_dup_stream, id->pp_ids[i_stream], p_dup );
            }
        }

        if( i_stream < p_sys->i_nb_streams && id->pp_ids[i_stream] )
        {
            p_dup_stream = p_sys->pp_streams[i_stream];
            sout_StreamIdSend( p_dup_stream, id->pp_ids[i_stream], p_buffer );
        }
        else
        {
            block_Release( p_buffer );
        }

        p_buffer = p_next;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Divers
 *****************************************************************************/
static bool NumInRange( const char *psz_range, int i_num )
{
    int beginRange, endRange;
    int res = sscanf(psz_range, "%d-%d", &beginRange, &endRange);
    if (res == 0)
        return false;
    else if (res == 1)
        return beginRange == i_num;
    return (i_num >= beginRange && i_num <= endRange)
        || (beginRange > endRange && (i_num <= beginRange && i_num >= endRange));
}

static bool ESSelected( const es_format_t *fmt, char *psz_select )
{
    char  *psz_dup;
    char  *psz;

    /* We have tri-state variable : no tested (-1), failed(0), succeed(1) */
    int i_cat = -1;
    int i_es  = -1;
    int i_prgm= -1;

    /* If empty all es are selected */
    if( psz_select == NULL || *psz_select == '\0' )
    {
        return true;
    }
    psz_dup = strdup( psz_select );
    if( !psz_dup )
        return false;
    psz = psz_dup;

    /* If non empty, parse the selection:
     * We have selection[,selection[,..]] where following selection are recognized:
     *      (no(-))audio
     *      (no(-))spu
     *      (no(-))video
     *      (no(-))es=[start]-[end] or es=num
     *      (no(-))prgm=[start]-[end] or prgm=num (program works too)
     *      if a negative test failed we exit directly
     */
    while( psz && *psz )
    {
        char *p;

        /* Skip space */
        while( *psz == ' ' || *psz == '\t' ) psz++;

        /* Search end */
        p = strchr( psz, ',' );
        if( p == psz )
        {
            /* Empty */
            psz = p + 1;
            continue;
        }
        if( p )
        {
            *p++ = '\0';
        }

        if( !strncmp( psz, "no-audio", strlen( "no-audio" ) ) ||
            !strncmp( psz, "noaudio", strlen( "noaudio" ) ) )
        {
            if( i_cat == -1 )
            {
                i_cat = fmt->i_cat != AUDIO_ES ? 1 : 0;
            }
        }
        else if( !strncmp( psz, "no-video", strlen( "no-video" ) ) ||
                 !strncmp( psz, "novideo", strlen( "novideo" ) ) )
        {
            if( i_cat == -1 )
            {
                i_cat = fmt->i_cat != VIDEO_ES ? 1 : 0;
            }
        }
        else if( !strncmp( psz, "no-spu", strlen( "no-spu" ) ) ||
                 !strncmp( psz, "nospu", strlen( "nospu" ) ) )
        {
            if( i_cat == -1 )
            {
                i_cat = fmt->i_cat != SPU_ES ? 1 : 0;
            }
        }
        else if( !strncmp( psz, "audio", strlen( "audio" ) ) )
        {
            if( i_cat == -1 )
            {
                i_cat = fmt->i_cat == AUDIO_ES ? 1 : 0;
            }
        }
        else if( !strncmp( psz, "video", strlen( "video" ) ) )
        {
            if( i_cat == -1 )
            {
                i_cat = fmt->i_cat == VIDEO_ES ? 1 : 0;
            }
        }
        else if( !strncmp( psz, "spu", strlen( "spu" ) ) )
        {
            if( i_cat == -1 )
            {
                i_cat = fmt->i_cat == SPU_ES ? 1 : 0;
            }
        }
        else if( strchr( psz, '=' ) != NULL )
        {
            char *psz_arg = strchr( psz, '=' );
            *psz_arg++ = '\0';

            if( !strcmp( psz, "no-es" ) || !strcmp( psz, "noes" ) )
            {
                if( i_es == -1 )
                {
                    i_es = NumInRange( psz_arg, fmt->i_id ) ? 0 : -1;
                }
            }
            else if( !strcmp( psz, "es" ) )
            {
                if( i_es == -1 )
                {
                    i_es = NumInRange( psz_arg, fmt->i_id) ? 1 : -1;
                }
            }
            else if( !strcmp( psz, "no-prgm" ) || !strcmp( psz, "noprgm" ) ||
                      !strcmp( psz, "no-program" ) || !strcmp( psz, "noprogram" ) )
            {
                if( fmt->i_group >= 0 && i_prgm == -1 )
                {
                    i_prgm = NumInRange( psz_arg, fmt->i_group ) ? 0 : -1;
                }
            }
            else if( !strcmp( psz, "prgm" ) || !strcmp( psz, "program" ) )
            {
                if( fmt->i_group >= 0 && i_prgm == -1 )
                {
                    i_prgm = NumInRange( psz_arg, fmt->i_group ) ? 1 : -1;
                }
            }
        }
        else
        {
            fprintf( stderr, "unknown args (%s)\n", psz );
        }
        /* Next */
        psz = p;
    }

    free( psz_dup );

    if( i_cat == 1 || i_es == 1 || i_prgm == 1 )
    {
        return true;
    }
    return false;
}
