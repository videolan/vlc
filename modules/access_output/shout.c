/*****************************************************************************
 * shout.c: This module forwards vorbis streams to an icecast server
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Daniel Fischer <dan at subsignal dot org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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
 * Some Comments:
 *
 * - this only works for ogg and/or mp3, and we don't check this yet.
 * - MP3 metadata is not passed along, since metadata is only available after
 *   this module is opened.
 *
 * Typical usage:
 *
 * vlc v4l:/dev/video:input=2:norm=pal:size=192x144 \
 * --sout '#transcode{vcodec=theora,vb=300,acodec=vorb,ab=96}\
 * :std{access=shout,mux=ogg,dst=localhost:8005}'
 *
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
#include <vlc_url.h>

#include <shout/shout.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-shout-"

#define NAME_TEXT N_("Stream name")
#define NAME_LONGTEXT N_("Name to give to this stream/channel on the " \
                         "shoutcast/icecast server." )

#define DESCRIPTION_TEXT N_("Stream description")
#define DESCRIPTION_LONGTEXT N_("Description of the stream content or " \
                                "information about your channel." )

#define MP3_TEXT N_("Stream MP3")
#define MP3_LONGTEXT N_("You normally have to feed the shoutcast module " \
                        "with Ogg streams. It is also possible to stream " \
                        "MP3 instead, so you can forward MP3 streams to " \
                        "the shoutcast/icecast server." )

/* To be listed properly as a public stream on the Yellow Pages of shoutcast/icecast
   the genres should match those used on the corresponding sites. Several examples
   are Alternative, Classical, Comedy, Country etc. */

#define GENRE_TEXT N_("Genre description")
#define GENRE_LONGTEXT N_("Genre of the content. " )

#define URL_TEXT N_("URL description")
#define URL_LONGTEXT N_("URL with information about the stream or your channel. " )

/* The shout module only "transmits" data. It does not have direct access to
   "codec level" information. Stream information such as bitrate, samplerate,
   channel numbers and quality (in case of Ogg streaming) need to be set manually */

#define BITRATE_TEXT N_("Bitrate")
#define BITRATE_LONGTEXT N_("Bitrate information of the transcoded stream. " )

#define SAMPLERATE_TEXT N_("Samplerate")
#define SAMPLERATE_LONGTEXT N_("Samplerate information of the transcoded stream. " )

#define CHANNELS_TEXT N_("Number of channels")
#define CHANNELS_LONGTEXT N_("Number of channels information of the transcoded stream. " )

#define QUALITY_TEXT N_("Ogg Vorbis Quality")
#define QUALITY_LONGTEXT N_("Ogg Vorbis Quality information of the transcoded stream. " )

#define PUBLIC_TEXT N_("Stream public")
#define PUBLIC_LONGTEXT N_("Make the server publicly available on the 'Yellow Pages' " \
                           "(directory listing of streams) on the icecast/shoutcast " \
                           "website. Requires the bitrate information specified for " \
                           "shoutcast. Requires Ogg streaming for icecast." )

vlc_module_begin ()
    set_description( N_("IceCAST output") )
    set_shortname( "Shoutcast" )
    set_capability( "sout access", 0 )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_ACO )
    add_shortcut( "shout" )
    add_string( SOUT_CFG_PREFIX "name", "VLC media player - Live stream",
                NAME_TEXT, NAME_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "description", "Live stream from VLC media player",
                DESCRIPTION_TEXT, DESCRIPTION_LONGTEXT, false )
    add_bool(   SOUT_CFG_PREFIX "mp3", false,
                MP3_TEXT, MP3_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "genre", "Alternative",
                GENRE_TEXT, GENRE_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "url", "http://www.videolan.org/vlc",
                URL_TEXT, URL_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "bitrate", "",
                BITRATE_TEXT, BITRATE_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "samplerate", "",
                SAMPLERATE_TEXT, SAMPLERATE_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "channels", "",
                CHANNELS_TEXT, CHANNELS_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "quality", "",
                QUALITY_TEXT, QUALITY_LONGTEXT, false )
    add_bool(   SOUT_CFG_PREFIX "public", false,
                PUBLIC_TEXT, PUBLIC_LONGTEXT, true )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "name", "description", "mp3", "genre", "url", "bitrate", "samplerate",
    "channels", "quality", "public", NULL
};


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static ssize_t Write( sout_access_out_t *, block_t * );
static int Seek ( sout_access_out_t *, off_t  );
static int Control( sout_access_out_t *, int, va_list );

struct sout_access_out_sys_t
{
    shout_t *p_shout;
};

/*****************************************************************************
 * Open: open the shout connection
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys;
    shout_t *p_shout;
    long i_ret;
    char *psz_val;

    char *psz_name;
    char *psz_description;
    char *psz_genre;
    char *psz_url;
    vlc_url_t url;

    config_ChainParse( p_access, SOUT_CFG_PREFIX, ppsz_sout_options, p_access->p_cfg );

    if( !p_access->psz_path )
    {
        msg_Err( p_access,
                 "please specify url=user:password@host:port/mountpoint" );
        return VLC_EGENERIC;
    }

    vlc_UrlParse( &url , p_access->psz_path, 0 );
    if( url.i_port <= 0 )
        url.i_port = 8000;

    p_sys = p_access->p_sys = malloc( sizeof( sout_access_out_sys_t ) );
    if( !p_sys )
    {
        vlc_UrlClean( &url );
        return VLC_ENOMEM;
    }

    psz_name = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "name" );
    psz_description = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "description" );
    psz_genre = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "genre" );
    psz_url = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "url" );

    p_shout = p_sys->p_shout = shout_new();
    if( !p_shout
         || shout_set_host( p_shout, url.psz_host ) != SHOUTERR_SUCCESS
         || shout_set_protocol( p_shout, SHOUT_PROTOCOL_ICY ) != SHOUTERR_SUCCESS
         || shout_set_port( p_shout, url.i_port ) != SHOUTERR_SUCCESS
         || shout_set_password( p_shout, url.psz_password ) != SHOUTERR_SUCCESS
         || shout_set_mount( p_shout, url.psz_path ) != SHOUTERR_SUCCESS
         || shout_set_user( p_shout, url.psz_username ) != SHOUTERR_SUCCESS
         || shout_set_agent( p_shout, "VLC media player " VERSION ) != SHOUTERR_SUCCESS
         || shout_set_name( p_shout, psz_name ) != SHOUTERR_SUCCESS
         || shout_set_description( p_shout, psz_description ) != SHOUTERR_SUCCESS
         || shout_set_genre( p_shout, psz_genre ) != SHOUTERR_SUCCESS
         || shout_set_url( p_shout, psz_url ) != SHOUTERR_SUCCESS
         /* || shout_set_nonblocking( p_shout, 1 ) != SHOUTERR_SUCCESS */
      )
    {
        msg_Err( p_access, "failed to initialize shout streaming to %s:%i/%s",
                 url.psz_host, url.i_port, url.psz_path );

        free( psz_name );
        free( psz_description );
        free( psz_genre );
        free( psz_url );
        goto error;
    }

    free( psz_name );
    free( psz_description );
    free( psz_genre );
    free( psz_url );

    i_ret = shout_set_format( p_shout, var_GetBool( p_access, SOUT_CFG_PREFIX "mp3" ) ?
                                       SHOUT_FORMAT_MP3 : SHOUT_FORMAT_OGG );

    if( i_ret != SHOUTERR_SUCCESS )
    {
        msg_Err( p_access, "failed to set the shoutcast streaming format" );
        goto error;
    }

    /* Don't force bitrate to 0 but only use when specified. This will otherwise
       show an empty field on icecast directory listing instead of NA */
    psz_val = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "bitrate" );
    if( psz_val )
    {
        i_ret = shout_set_audio_info( p_shout, SHOUT_AI_BITRATE, psz_val );
        free( psz_val );
        if( i_ret != SHOUTERR_SUCCESS )
        {
            msg_Err( p_access, "failed to set the information about the bitrate" );
            goto error;
        }
    }
    else
    {
        /* Bitrate information is used for icecast/shoutcast servers directory
           listings (sorting, stream info etc.) */
        msg_Warn( p_access, "no bitrate information specified (required for listing " \
                            "the server as public on the shoutcast website)" );
    }

    /* Information about samplerate, channels and quality will not be propagated
       through the YP protocol for icecast to the public directory listing when
       the icecast server is operating in shoutcast compatibility mode */

    psz_val = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "samplerate" );
    if( psz_val )
    {
        i_ret = shout_set_audio_info( p_shout, SHOUT_AI_SAMPLERATE, psz_val );
        free( psz_val );
        if( i_ret != SHOUTERR_SUCCESS )
        {
            msg_Err( p_access, "failed to set the information about the samplerate" );
            goto error;
        }
    }

    psz_val = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "channels" );
    if( psz_val )
    {
        i_ret = shout_set_audio_info( p_shout, SHOUT_AI_CHANNELS, psz_val );
        free( psz_val );
        if( i_ret != SHOUTERR_SUCCESS )
        {
            msg_Err( p_access, "failed to set the information about the number of channels" );
            goto error;
        }
    }

    psz_val = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "quality" );
    if( psz_val )
    {
        i_ret = shout_set_audio_info( p_shout, SHOUT_AI_QUALITY, psz_val );
        free( psz_val );
        if( i_ret != SHOUTERR_SUCCESS )
        {
            msg_Err( p_access, "failed to set the information about Ogg Vorbis quality" );
            goto error;
        }
    }

    if( var_GetBool( p_access, SOUT_CFG_PREFIX "public" ) )
    {
        i_ret = shout_set_public( p_shout, 1 );
        if( i_ret != SHOUTERR_SUCCESS )
        {
            msg_Err( p_access, "failed to set the server status setting to public" );
            goto error;
        }
    }

    /* Connect at startup. Cycle through the possible protocols. */
    i_ret = shout_get_connected( p_shout );
    while ( i_ret != SHOUTERR_CONNECTED )
    {
        /* Shout parameters cannot be changed on an open connection */
        i_ret = shout_close( p_shout );
        if( i_ret == SHOUTERR_SUCCESS )
        {
            i_ret = SHOUTERR_UNCONNECTED;
        }

        /* Re-initialize for Shoutcast using ICY protocol. Not needed for initial connection
           but it is when we are reconnecting after other protocol was tried. */
        i_ret = shout_set_protocol( p_shout, SHOUT_PROTOCOL_ICY );
        if( i_ret != SHOUTERR_SUCCESS )
        {
            msg_Err( p_access, "failed to set the protocol to 'icy'" );
            goto error;
        }
        i_ret = shout_open( p_shout );
        if( i_ret == SHOUTERR_SUCCESS )
        {
            i_ret = SHOUTERR_CONNECTED;
            msg_Dbg( p_access, "connected using 'icy' (shoutcast) protocol" );
        }
        else
        {
            msg_Warn( p_access, "failed to connect using 'icy' (shoutcast) protocol" );

            /* Shout parameters cannot be changed on an open connection */
            i_ret = shout_close( p_shout );
            if( i_ret == SHOUTERR_SUCCESS )
            {
                i_ret = SHOUTERR_UNCONNECTED;
            }

            /* IceCAST using HTTP protocol */
            i_ret = shout_set_protocol( p_shout, SHOUT_PROTOCOL_HTTP );
            if( i_ret != SHOUTERR_SUCCESS )
            {
                msg_Err( p_access, "failed to set the protocol to 'http'" );
                goto error;
            }
            i_ret = shout_open( p_shout );
            if( i_ret == SHOUTERR_SUCCESS )
            {
                i_ret = SHOUTERR_CONNECTED;
                msg_Dbg( p_access, "connected using 'http' (icecast 2.x) protocol" );
            }
            else
                msg_Warn( p_access, "failed to connect using 'http' (icecast 2.x) protocol " );
        }
/*
        for non-blocking, use:
        while( i_ret == SHOUTERR_BUSY )
        {
            sleep( 1 );
            i_ret = shout_get_connected( p_shout );
        }
*/
        if ( i_ret != SHOUTERR_CONNECTED )
        {
            msg_Warn( p_access, "unable to establish connection, retrying..." );
            msleep( 30000000 );
        }
    }

    if( i_ret != SHOUTERR_CONNECTED )
    {
        msg_Err( p_access, "failed to open shout stream to %s:%i/%s: %s",
                 url.psz_host, url.i_port, url.psz_path, shout_get_error(p_shout) );
        goto error;
    }

    p_access->pf_write = Write;
    p_access->pf_seek  = Seek;
    p_access->pf_control = Control;

    msg_Dbg( p_access, "shout access output opened (%s@%s:%i/%s)",
             url.psz_username, url.psz_host, url.i_port, url.psz_path );

    vlc_UrlClean( &url );
    return VLC_SUCCESS;

error:
    if( p_sys->p_shout )
        shout_free( p_sys->p_shout );
    vlc_UrlClean( &url );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    if( p_sys->p_shout )
    {
        shout_close( p_sys->p_shout );
        shout_free( p_sys->p_shout );
        shout_shutdown();
    }
    free( p_sys );
    msg_Dbg( p_access, "shout access output closed" );
}

static int Control( sout_access_out_t *p_access, int i_query, va_list args )
{
    switch( i_query )
    {
        case ACCESS_OUT_CONTROLS_PACE:
        {
            bool *pb = va_arg( args, bool * );
            *pb = strcmp( p_access->psz_access, "stream" );
            break;
        }

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Write: standard write
 *****************************************************************************/
static ssize_t Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    size_t i_write = 0;

    shout_sync( p_sys->p_shout );
    while( p_buffer )
    {
        block_t *p_next = p_buffer->p_next;

        if( shout_send( p_sys->p_shout, p_buffer->p_buffer, p_buffer->i_buffer )
             == SHOUTERR_SUCCESS )
        {
            i_write += p_buffer->i_buffer;
        }
        else
        {
            msg_Err( p_access, "cannot write to stream: %s",
                     shout_get_error( p_sys->p_shout ) );

            /* The most common cause seems to be a server disconnect, resulting in a
               Socket Error which can only be fixed by closing and reconnecting.
               Since we already began with a working connection, the most feasable
               approach to get out of this error status is a (timed) reconnect approach. */
            shout_close( p_sys->p_shout );
            msg_Warn( p_access, "server unavailable? trying to reconnect..." );
            /* Re-open the connection (protocol params have already been set) and re-sync */
            if( shout_open( p_sys->p_shout ) == SHOUTERR_SUCCESS )
            {
                shout_sync( p_sys->p_shout );
                msg_Warn( p_access, "reconnected to server" );
            }
            else
            {
                msg_Err( p_access, "failed to reconnect to server" );
                block_ChainRelease( p_buffer );
                return VLC_EGENERIC;
            }

        }
        block_Release( p_buffer );

        /* XXX: Unsure if that's the cause for some audio trouble... */

        p_buffer = p_next;
    }

    return i_write;
}

/*****************************************************************************
 * Seek: seek to a specific location -- not supported
 *****************************************************************************/
static int Seek( sout_access_out_t *p_access, off_t i_pos )
{
    VLC_UNUSED(i_pos);
    msg_Err( p_access, "cannot seek on shout" );
    return VLC_EGENERIC;
}

