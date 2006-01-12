/*****************************************************************************
 * shout.c: This module forwards vorbis streams to an icecast server
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
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
 * :std{access=shout,mux=ogg,url=localhost:8005}'
 *
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/sout.h>

#include <shout/shout.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-shout-"

#define NAME_TEXT N_("Stream-name")
#define NAME_LONGTEXT N_("The name this stream/channel will get on the icecast server." )

#define DESCRIPTION_TEXT N_("Stream-description")
#define DESCRIPTION_LONGTEXT N_("A description of the stream content. (Information about " \
                         "your channel)." )

#define MP3_TEXT N_("Stream MP3")
#define MP3_LONGTEXT N_("Normally you have to feed the shoutcast module with Ogg streams. " \
                         "This option allows you to feed MP3 streams instead, so you can " \
                         "forward MP3 streams to the icecast server." )

vlc_module_begin();
    set_description( _("libshout (icecast) output") );
    set_shortname( "Shout" );
    set_capability( "sout access", 50 );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_ACO );
    add_shortcut( "shout" );
    add_string( SOUT_CFG_PREFIX "name", "VLC media player - Live stream", NULL,
                NAME_TEXT, NAME_LONGTEXT, VLC_FALSE );
    add_string( SOUT_CFG_PREFIX "description", "Live stream from VLC media player. " \
                "http://www.videolan.org/vlc", NULL,
                DESCRIPTION_TEXT, DESCRIPTION_LONGTEXT, VLC_FALSE );
    add_bool(   SOUT_CFG_PREFIX "mp3", VLC_FALSE, NULL,
                MP3_TEXT, MP3_LONGTEXT, VLC_TRUE );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *ppsz_sout_options[] = {
    "name", "description", "mp3", NULL
};


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int Write( sout_access_out_t *, block_t * );
static int Seek ( sout_access_out_t *, off_t  );
static int Read ( sout_access_out_t *, block_t * );

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
    unsigned int i_port;
    vlc_value_t val;

    char *psz_accessname = NULL;
    char *psz_parser = NULL;
    char *psz_user = NULL;
    char *psz_pass = NULL;
    char *psz_host = NULL;
    char *psz_mount = NULL;
    char *psz_name = NULL;
    char *psz_description = NULL;
    char *tmp_port = NULL;
  
    sout_CfgParse( p_access, SOUT_CFG_PREFIX, ppsz_sout_options, p_access->p_cfg );

    psz_accessname = psz_parser = strdup( p_access->psz_name );

    if( !p_access->psz_name )
    {
        msg_Err( p_access,
                 "please specify url=user:password@host:port/mountpoint" );
        return VLC_EGENERIC;
    }

    /* Parse connection data user:pwd@host:port/mountpoint */
    psz_user = psz_parser;
    while( psz_parser[0] && psz_parser[0] != ':' ) psz_parser++;
    if( psz_parser[0] ) { psz_parser[0] = 0; psz_parser++; }
    psz_pass = psz_parser;
    while( psz_parser[0] && psz_parser[0] != '@' ) psz_parser++;
    if( psz_parser[0] ) { psz_parser[0] = 0; psz_parser++; }
    psz_host = psz_parser;
    while( psz_parser[0] && psz_parser[0] != ':' ) psz_parser++;
    if( psz_parser[0] ) { psz_parser[0] = 0; psz_parser++; }
    tmp_port = psz_parser;
    while( psz_parser[0] && psz_parser[0] != '/' ) psz_parser++;
    if( psz_parser[0] ) { psz_parser[0] = 0; psz_parser++; }
    psz_mount = psz_parser;

    i_port = atoi( tmp_port );

    p_sys = p_access->p_sys = malloc( sizeof( sout_access_out_sys_t ) );
    if( !p_sys )
    {
        msg_Err( p_access, "out of memory" );
        free( psz_accessname );
        return VLC_ENOMEM;
    }

    var_Get( p_access, SOUT_CFG_PREFIX "name", &val );
    if( *val.psz_string )
        psz_name = val.psz_string;
    else
        free( val.psz_string );

    var_Get( p_access, SOUT_CFG_PREFIX "description", &val );
    if( *val.psz_string )
        psz_description = val.psz_string;
    else
        free( val.psz_string );

    p_shout = p_sys->p_shout = shout_new();
    if( !p_shout
         || shout_set_host( p_shout, psz_host ) != SHOUTERR_SUCCESS
         || shout_set_protocol( p_shout, SHOUT_PROTOCOL_HTTP )
             != SHOUTERR_SUCCESS
         || shout_set_port( p_shout, i_port ) != SHOUTERR_SUCCESS
         || shout_set_password( p_shout, psz_pass ) != SHOUTERR_SUCCESS
         || shout_set_mount( p_shout, psz_mount ) != SHOUTERR_SUCCESS
         || shout_set_user( p_shout, psz_user ) != SHOUTERR_SUCCESS
         || shout_set_agent( p_shout, "VLC media player " VERSION ) != SHOUTERR_SUCCESS
         || shout_set_name( p_shout, psz_name ) != SHOUTERR_SUCCESS
         || shout_set_description( p_shout, psz_description ) != SHOUTERR_SUCCESS 
//       || shout_set_nonblocking( p_shout, 1 ) != SHOUTERR_SUCCESS
      )
    {
        msg_Err( p_access, "failed to initialize shout streaming to %s:%i/%s",
                 psz_host, i_port, psz_mount );
        free( p_access->p_sys );
        free( psz_accessname );
        return VLC_EGENERIC;
    }

    if( psz_name ) free( psz_name );
    if( psz_description ) free( psz_description );

    var_Get( p_access, SOUT_CFG_PREFIX "mp3", &val );
    if( val.b_bool == VLC_TRUE )
        i_ret = shout_set_format( p_shout, SHOUT_FORMAT_MP3 );
    else
        i_ret = shout_set_format( p_shout, SHOUT_FORMAT_OGG );

    if( i_ret != SHOUTERR_SUCCESS )
    {
        msg_Err( p_access, "failed to set the shoutcast streaming format" );
        free( p_access->p_sys );
        free( psz_accessname );
        return VLC_EGENERIC;
    }

    i_ret = shout_open( p_shout );
    if( i_ret == SHOUTERR_SUCCESS )
    {
        i_ret = SHOUTERR_CONNECTED;
    }

/*
    for non-blocking, use:
    while( i_ret == SHOUTERR_BUSY )
    {
        sleep( 1 );
        i_ret = shout_get_connected( p_shout );
    }
*/
    if( i_ret != SHOUTERR_CONNECTED )
    {
        msg_Err( p_access, "failed to open shout stream to %s:%i/%s: %s",
                 psz_host, i_port, psz_mount, shout_get_error(p_shout) );
        free( p_access->p_sys );
        free( psz_accessname );
        return VLC_EGENERIC;
    }

    p_access->pf_write = Write;
    p_access->pf_read  = Read;
    p_access->pf_seek  = Seek;

    msg_Dbg( p_access, "shout access output opened (%s@%s:%i/%s)",
             psz_user, psz_host, i_port, psz_mount );

    /* Update pace control flag */
    if( p_access->psz_access && !strcmp( p_access->psz_access, "stream" ) )
    {
        p_access->p_sout->i_out_pace_nocontrol++;
    }

    free( psz_accessname );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t *p_access = (sout_access_out_t*)p_this;

    if( p_access->p_sys && p_access->p_sys->p_shout )
    {
        shout_close( p_access->p_sys->p_shout );
        shout_shutdown();
    }
    free( p_access->p_sys );

    /* Update pace control flag */
    if( p_access->psz_access && !strcmp( p_access->psz_access, "stream" ) )
    {
        p_access->p_sout->i_out_pace_nocontrol--;
    }

    msg_Dbg( p_access, "shout access output closed" );
}

/*****************************************************************************
 * Read: standard read -- not supported
 *****************************************************************************/
static int Read( sout_access_out_t *p_access, block_t *p_buffer )
{
    msg_Err( p_access, "cannot read from shout" );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Write: standard write
 *****************************************************************************/
static int Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    size_t i_write = 0;

    shout_sync( p_access->p_sys->p_shout );
    while( p_buffer )
    {
        block_t *p_next = p_buffer->p_next;

        if( shout_send( p_access->p_sys->p_shout,
                        p_buffer->p_buffer, p_buffer->i_buffer )
             == SHOUTERR_SUCCESS )
        {
            i_write += p_buffer->i_buffer;
        }
        else
        {
            msg_Err( p_access, "cannot write to stream: %s",
                     shout_get_error(p_access->p_sys->p_shout) );
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
    msg_Err( p_access, "cannot seek on shout" );
    return VLC_EGENERIC;
}

