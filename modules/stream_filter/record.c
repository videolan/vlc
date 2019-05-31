/*****************************************************************************
 * record.c
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 *
 * Author: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <assert.h>
#include <vlc_stream.h>
#include <vlc_input_item.h>
#include <vlc_fs.h>


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_STREAM_FILTER )
    set_description( N_("Internal stream record") )
    set_capability( "stream_filter", 0 )
    set_callbacks( Open, Close )
vlc_module_end()

/*****************************************************************************
 *
 *****************************************************************************/
typedef struct
{
    FILE *f;        /* TODO it could be replaced by access_output_t one day */
    bool b_error;
} stream_sys_t;


/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static ssize_t Read( stream_t *, void *p_read, size_t i_read );
static int  Seek   ( stream_t *, uint64_t );
static int  Control( stream_t *, int i_query, va_list );

static int  Start  ( stream_t *, const char *psz_extension );
static int  Stop   ( stream_t * );
static void Write  ( stream_t *, const uint8_t *p_buffer, size_t i_buffer );

/****************************************************************************
 * Open
 ****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    stream_t *s = (stream_t*)p_this;
    stream_sys_t *p_sys;

    if( s->s->pf_readdir != NULL )
        return VLC_EGENERIC;

    /* */
    s->p_sys = p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->f = NULL;

    /* */
    s->pf_read = Read;
    s->pf_seek = Seek;
    s->pf_control = Control;

    return VLC_SUCCESS;
}

/****************************************************************************
 * Close
 ****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    stream_t *s = (stream_t*)p_this;
    stream_sys_t *p_sys = s->p_sys;

    if( p_sys->f )
        Stop( s );

    free( p_sys );
}

/****************************************************************************
 * Stream filters functions
 ****************************************************************************/
static ssize_t Read( stream_t *s, void *p_read, size_t i_read )
{
    stream_sys_t *p_sys = s->p_sys;
    void *p_record = p_read;
    const ssize_t i_record = vlc_stream_Read( s->s, p_record, i_read );

    /* Dump read data */
    if( p_sys->f )
    {
        if( p_record && i_record > 0 )
            Write( s, p_record, i_record );
    }

    return i_record;
}

static int Seek( stream_t *s, uint64_t offset )
{
    return vlc_stream_Seek( s->s, offset );
}

static int Control( stream_t *s, int i_query, va_list args )
{
    if( i_query != STREAM_SET_RECORD_STATE )
        return vlc_stream_vaControl( s->s, i_query, args );

    stream_sys_t *sys = s->p_sys;
    bool b_active = (bool)va_arg( args, int );
    const char *psz_extension = NULL;
    if( b_active )
        psz_extension = va_arg( args, const char* );

    if( !sys->f == !b_active )
        return VLC_SUCCESS;

    if( b_active )
        return Start( s, psz_extension );
    else
        return Stop( s );
}

/****************************************************************************
 * Helpers
 ****************************************************************************/
static int Start( stream_t *s, const char *psz_extension )
{
    stream_sys_t *p_sys = s->p_sys;

    char *psz_file;
    FILE *f;

    /* */
    if( !psz_extension )
        psz_extension = "dat";

    /* Retreive path */
    char *psz_path = var_CreateGetNonEmptyString( s, "input-record-path" );
    if( !psz_path )
        psz_path = config_GetUserDir( VLC_DOWNLOAD_DIR );

    if( !psz_path )
        return VLC_ENOMEM;

    /* Create file name
     * TODO allow prefix configuration */
    psz_file = input_item_CreateFilename( s->p_input_item, psz_path,
                                          INPUT_RECORD_PREFIX, psz_extension );

    free( psz_path );

    if( !psz_file )
        return VLC_ENOMEM;

    f = vlc_fopen( psz_file, "wb" );
    if( !f )
    {
        free( psz_file );
        return VLC_EGENERIC;
    }

    /* signal new record file */
    var_SetString( vlc_object_instance(s), "record-file", psz_file );

    msg_Dbg( s, "Recording into %s", psz_file );
    free( psz_file );

    /* */
    p_sys->f = f;
    p_sys->b_error = false;
    return VLC_SUCCESS;
}
static int Stop( stream_t *s )
{
    stream_sys_t *p_sys = s->p_sys;

    assert( p_sys->f );

    msg_Dbg( s, "Recording completed" );
    fclose( p_sys->f );
    p_sys->f = NULL;
    return VLC_SUCCESS;
}

static void Write( stream_t *s, const uint8_t *p_buffer, size_t i_buffer )
{
    stream_sys_t *p_sys = s->p_sys;

    assert( p_sys->f );

    if( i_buffer > 0 )
    {
        const bool b_previous_error = p_sys->b_error;
        const size_t i_written = fwrite( p_buffer, 1, i_buffer, p_sys->f );

        p_sys->b_error = i_written != i_buffer;

        /* TODO maybe a intf_UserError or something like that ? */
        if( p_sys->b_error && !b_previous_error )
            msg_Err( s, "Failed to record data (begin)" );
        else if( !p_sys->b_error && b_previous_error )
            msg_Err( s, "Failed to record data (end)" );
    }
}
