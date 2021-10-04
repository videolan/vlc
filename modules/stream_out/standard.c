/*****************************************************************************
 * standard.c: standard stream output module
 *****************************************************************************
 * Copyright (C) 2003-2011 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#include <vlc_network.h>
#include <vlc_url.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ACCESS_TEXT N_("Output access method")
#define ACCESS_LONGTEXT N_( \
    "Output method to use for the stream." )
#define MUX_TEXT N_("Output muxer")
#define MUX_LONGTEXT N_( \
    "Muxer to use for the stream." )
#define DEST_TEXT N_("Output destination")
#define DEST_LONGTEXT N_( \
    "Destination (URL) to use for the stream. Overrides path and bind parameters" )
#define BIND_TEXT N_("Address to bind to (helper setting for dst)")
#define BIND_LONGTEXT N_( \
  "address:port to bind vlc to listening incoming streams. "\
  "Helper setting for dst, dst=bind+'/'+path. dst-parameter overrides this." )
#define PATH_TEXT N_("Filename for stream (helper setting for dst)")
#define PATH_LONGTEXT N_( \
  "Filename for stream. "\
  "Helper setting for dst, dst=bind+'/'+path. dst-parameter overrides this." )

static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-standard-"

#ifdef ENABLE_SRT
#define SRT_SHORTCUT "srt"
#else
#define SRT_SHORTCUT
#endif

vlc_module_begin ()
    set_shortname( N_("Standard"))
    set_description( N_("Standard stream output") )
    set_capability( "sout output", 50 )
    add_shortcut( "standard", "std", "file", "http", SRT_SHORTCUT )
    set_subcategory( SUBCAT_SOUT_STREAM )

    add_string( SOUT_CFG_PREFIX "access", "", ACCESS_TEXT, ACCESS_LONGTEXT )
    add_string( SOUT_CFG_PREFIX "mux", "", MUX_TEXT, MUX_LONGTEXT )
    add_string( SOUT_CFG_PREFIX "dst", "", DEST_TEXT, DEST_LONGTEXT )
    add_string( SOUT_CFG_PREFIX "bind", "", BIND_TEXT, BIND_LONGTEXT )
    add_string( SOUT_CFG_PREFIX "path", "", PATH_TEXT, PATH_LONGTEXT )
    add_obsolete_bool( SOUT_CFG_PREFIX "sap" ) /* since 4.0.0 */
    add_obsolete_string( SOUT_CFG_PREFIX "name" ) /* since 4.0.0 */
    add_obsolete_string( SOUT_CFG_PREFIX "description" ) /* since 4.0.0 */
    add_obsolete_string( SOUT_CFG_PREFIX "url" ) /* since 4.0.0 */
    add_obsolete_string( SOUT_CFG_PREFIX "email" ) /* since 4.0.0 */
    add_obsolete_string( SOUT_CFG_PREFIX "phone" ) /* since 3.0.0 */

    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "access", "mux", "url", "dst",
    "bind", "path", NULL
};

typedef struct
{
    sout_mux_t           *p_mux;
    session_descriptor_t *p_session;
    bool                  synchronous;
} sout_stream_sys_t;

static void *Add( sout_stream_t *p_stream, const es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    return sout_MuxAddStream( p_sys->p_mux, p_fmt );
}

static void Del( sout_stream_t *p_stream, void *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_MuxDeleteStream( p_sys->p_mux, (sout_input_t*)id );
}

static int Send( sout_stream_t *p_stream, void *id, block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    return sout_MuxSendBuffer( p_sys->p_mux, (sout_input_t*)id, p_buffer );
}

static void Flush( sout_stream_t *p_stream, void *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_MuxFlush( p_sys->p_mux, (sout_input_t*)id );
}

static const char *getMuxFromAlias( const char *psz_alias )
{
    static struct { const char alias[6]; const char mux[32]; } mux_alias[] =
    {
        { "avi", "avi" },
        { "ogg", "ogg" },
        { "ogm", "ogg" },
        { "ogv", "ogg" },
        { "flac","raw" },
        { "mp3", "raw" },
        { "mp4", "mp4" },
        { "mov", "mov" },
        { "moov","mov" },
        { "asf", "asf" },
        { "wma", "asf" },
        { "wmv", "asf" },
        { "trp", "ts" },
        { "ts",  "ts" },
        { "mpg", "ps" },
        { "mpeg","ps" },
        { "ps",  "ps" },
        { "mpeg1","mpeg1" },
        { "wav", "wav" },
        { "flv", "avformat{mux=flv}" },
        { "mkv", "avformat{mux=matroska}"},
        { "webm", "avformat{mux=webm}"},
    };

    if( !psz_alias )
        return NULL;

    for( size_t i = 0; i < sizeof mux_alias / sizeof *mux_alias; i++ )
        if( !strcasecmp( psz_alias, mux_alias[i].alias ) )
            return mux_alias[i].mux;

    return NULL;
}

static int fixAccessMux( sout_stream_t *p_stream, char **ppsz_mux,
                          char **ppsz_access, const char *psz_url )
{
    char *psz_mux = *ppsz_mux;
    char *psz_access = *ppsz_access;
    if( !psz_mux )
    {
        const char *psz_ext = psz_url ? strrchr( psz_url, '.' ) : NULL;
        if( psz_ext )
            psz_ext++; /* use extension */
        const char *psz_mux_byext = getMuxFromAlias( psz_ext );

        if( !psz_access )
        {
            if( !psz_mux_byext )
            {
                msg_Err( p_stream, "no access _and_ no muxer" );
                return 1;
            }

            msg_Warn( p_stream,
                    "no access _and_ no muxer, extension gives file/%s",
                    psz_mux_byext );
            *ppsz_access = strdup("file");
            *ppsz_mux    = strdup(psz_mux_byext);
        }
        else
        {
            if( !strncmp( psz_access, "mmsh", 4 ) )
                *ppsz_mux = strdup("asfh");
            else if( psz_mux_byext )
                *ppsz_mux = strdup(psz_mux_byext);
            else
            {
                msg_Err( p_stream, "no mux specified or found by extension" );
                return 1;
            }
        }
    }
    else if( !psz_access )
    {
        if( !strncmp( psz_mux, "asfh", 4 ) )
            *ppsz_access = strdup("mmsh");
        else /* default file */
            *ppsz_access = strdup("file");
    }
    return 0;
}

static bool exactMatch( const char *psz_target, const char *psz_string,
                        size_t i_len )
{
    if ( strncmp( psz_target, psz_string, i_len ) )
        return false;
    else
        return ( psz_target[i_len] < 'a' || psz_target[i_len] > 'z' );
}

static void checkAccessMux( sout_stream_t *p_stream, char *psz_access,
                            char *psz_mux )
{
    if( exactMatch( psz_access, "mmsh", 4 ) && !exactMatch( psz_mux, "asfh", 4 ) )
        msg_Err( p_stream, "mmsh output is only valid with asfh mux" );
    else if( !exactMatch( psz_access, "file", 4 ) &&
             ( exactMatch( psz_mux, "mov", 3 ) || exactMatch( psz_mux, "mp4", 3 ) ) )
        msg_Err( p_stream, "mov and mp4 mux are only valid with file output" );
}

static int Control(sout_stream_t *stream, int query, va_list args)
{
    sout_stream_sys_t *sys = stream->p_sys;

    switch (query)
    {
        case SOUT_STREAM_IS_SYNCHRONOUS:
            *va_arg(args, bool *) = sys->synchronous;
            break;

        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static const struct sout_stream_operations ops = {
    Add, Del, Send, Control, Flush, NULL,
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t   *p_sys;
    char *psz_mux, *psz_access, *psz_url;
    sout_access_out_t   *p_access;
    int                 ret = VLC_EGENERIC;

    config_ChainParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options,
                   p_stream->p_cfg );

    psz_mux = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "mux" );

    psz_access = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "access" );
    if( !psz_access )
        psz_access = strdup(p_stream->psz_name);

    psz_url = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "dst" );
    if (!psz_url)
    {
        char *psz_bind = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "bind" );
        if( psz_bind )
        {
            char *psz_path = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "path" );
            if( psz_path )
            {
                if( asprintf( &psz_url, "%s/%s", psz_bind, psz_path ) == -1 )
                    psz_url = NULL;
                free(psz_bind);
                free( psz_path );
            }
            else
                psz_url = psz_bind;
        }
    }

    p_sys = p_stream->p_sys = malloc( sizeof( sout_stream_sys_t) );
    if( !p_sys )
    {
        ret = VLC_ENOMEM;
        goto end;
    }
    p_sys->p_session = NULL;

    if( fixAccessMux( p_stream, &psz_mux, &psz_access, psz_url ) )
        goto end;

    checkAccessMux( p_stream, psz_access, psz_mux );

    p_access = sout_AccessOutNew( p_stream, psz_access, psz_url );
    if( p_access == NULL )
    {
        msg_Err( p_stream, "no suitable sout access module for `%s/%s://%s'",
                 psz_access, psz_mux, psz_url );
        goto end;
    }

    p_sys->synchronous = !sout_AccessOutCanControlPace(p_access);
    p_sys->p_mux = sout_MuxNew( p_access, psz_mux );
    if( !p_sys->p_mux )
    {
        const char *psz_mux_guess = getMuxFromAlias( psz_mux );
        if( psz_mux_guess && strcmp( psz_mux_guess, psz_mux ) )
        {
            msg_Dbg( p_stream, "Couldn't open mux `%s', trying `%s' instead",
                psz_mux, psz_mux_guess );
            p_sys->p_mux = sout_MuxNew( p_access, psz_mux_guess );
        }

        if( !p_sys->p_mux )
        {
            msg_Err( p_stream, "no suitable sout mux module for `%s/%s://%s'",
                psz_access, psz_mux, psz_url );

            sout_AccessOutDelete( p_access );
            goto end;
        }
    }

    p_stream->ops = &ops;
    ret = VLC_SUCCESS;
    msg_Dbg( p_this, "using `%s/%s://%s'", psz_access, psz_mux, psz_url );

end:
    if( ret != VLC_SUCCESS )
        free( p_sys );
    free( psz_access );
    free( psz_mux );
    free( psz_url );

    return ret;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys    = p_stream->p_sys;
    sout_access_out_t *p_access = p_sys->p_mux->p_access;

    if( p_sys->p_session != NULL )
        sout_AnnounceUnRegister( p_stream, p_sys->p_session );

    sout_MuxDelete( p_sys->p_mux );
    sout_AccessOutDelete( p_access );

    free( p_sys );
}
