/*****************************************************************************
 * shout.c
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
 * $Id$
 *
 * Authors: Daniel Fischer <dan at subsignal dot org>
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
 * Some Comments:
 *
 * o this only works for ogg streams, and there's no checking about that yet.
 * o i have lots of problems with audio, but i think they are not caused
 *   by this patch
 * o there's a memleak somewhere, quite huge. i'm unsure if its somewhere
 *   in libshout, in vlc or even in this patch...
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

vlc_module_begin();
    set_description( _("libshout (icecast) output") );
    set_shortname( N_("Shout" ));
    set_capability( "sout access", 50 );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_ACO );
    add_shortcut( "shout" );
    set_callbacks( Open, Close );
vlc_module_end();


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

    char *psz_user, *psz_pass, *psz_host, *psz_mount;
    unsigned int i_port;

    char *parser = strdup( p_access->psz_name );
    char *tmp_port;

    if( !p_access->psz_name )
    {
        msg_Err( p_access,
                 "please specify url=user:password@host:port/mountpoint" );
        return VLC_EGENERIC;
    }

    /* Parse connection data user:pwd@host:port/mountpoint */
    psz_user = parser;
    while( parser[0] && parser[0] != ':' ) parser++;
    if( parser[0] ) { parser[0] = 0; parser++; }
    psz_pass = parser;
    while( parser[0] && parser[0] != '@' ) parser++;
    if( parser[0] ) { parser[0] = 0; parser++; }
    psz_host = parser;
    while( parser[0] && parser[0] != ':' ) parser++;
    if( parser[0] ) { parser[0] = 0; parser++; }
    tmp_port = parser;
    while( parser[0] && parser[0] != '/' ) parser++;
    if( parser[0] ) { parser[0] = 0; parser++; }
    psz_mount = parser;

    i_port = atoi( tmp_port );

    p_sys = p_access->p_sys = malloc( sizeof( sout_access_out_sys_t ) );
    if( !p_sys )
    {
        msg_Err( p_access, "out of memory" );
        free( parser );
        return VLC_ENOMEM;
    }

    p_shout = p_sys->p_shout = shout_new();
    if( !p_shout
         || shout_set_host( p_shout, psz_host ) != SHOUTERR_SUCCESS
         || shout_set_protocol( p_shout, SHOUT_PROTOCOL_HTTP )
             != SHOUTERR_SUCCESS
         || shout_set_port( p_shout, i_port ) != SHOUTERR_SUCCESS
         || shout_set_password( p_shout, psz_pass ) != SHOUTERR_SUCCESS
         || shout_set_mount( p_shout, psz_mount ) != SHOUTERR_SUCCESS
         || shout_set_user( p_shout, psz_user ) != SHOUTERR_SUCCESS
         || shout_set_format( p_shout, SHOUT_FORMAT_OGG ) != SHOUTERR_SUCCESS
  //       || shout_set_nonblocking( p_shout, 1 ) != SHOUTERR_SUCCESS
      )
    {
        msg_Err( p_access, "failed to initialize shout streaming to %s:%i%s",
                 psz_host, i_port, psz_mount );
        free( p_access->p_sys );
        free( parser );
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
        msg_Err( p_access, "failed to open shout stream to %s:%i%s: %s",
                 psz_host, i_port, psz_mount, shout_get_error(p_shout) );
        free( p_access->p_sys );
        free( parser );
        return VLC_EGENERIC;
    }

    p_access->pf_write = Write;
    p_access->pf_read  = Read;
    p_access->pf_seek  = Seek;

    msg_Dbg( p_access, "shout access output opened (%s@%s:%i%s)",
             psz_user, psz_host, i_port, psz_mount );

    /* Update pace control flag */
    if( p_access->psz_access && !strcmp( p_access->psz_access, "stream" ) )
    {
        p_access->p_sout->i_out_pace_nocontrol++;
    }

    /* FIXME: it should be free()d somewhere, but not here.... ? */
    //free( parser );

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

