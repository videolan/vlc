/*****************************************************************************
 * setid.c: set ID/lang on a stream
 *****************************************************************************
 * Copyright (C) 2009-2011 VideoLAN and AUTHORS
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ID_TEXT N_("ID")
#define ID_LONGTEXT N_( \
    "Specify an identifier integer for this elementary stream" )

#define NEWID_TEXT N_("New ID")
#define NEWID_LONGTEXT N_( \
    "Specify an new identifier integer for this elementary stream" )

#define LANG_TEXT N_("Language")
#define LANG_LONGTEXT N_( \
    "Specify an ISO-639 code (three characters) for this elementary stream" )

static int  OpenId    ( vlc_object_t * );
static int  OpenLang  ( vlc_object_t * );
static void Close     ( vlc_object_t * );

#define SOUT_CFG_PREFIX_ID   "sout-setid-"
#define SOUT_CFG_PREFIX_LANG "sout-setlang-"

vlc_module_begin()
    set_shortname( _("setid"))
    set_description( _("Automatically add/delete input streams"))
    set_capability( "sout stream", 50 )
    add_shortcut( "setid" )
    set_callbacks( OpenId, Close )
    add_integer( SOUT_CFG_PREFIX_ID "id", 0, ID_TEXT, ID_LONGTEXT, false )
    add_integer( SOUT_CFG_PREFIX_ID "new-id", 0, NEWID_TEXT, NEWID_LONGTEXT,
                 false )

    add_submodule ()
    set_section( N_("setlang"), NULL )
    set_shortname( _("setlang"))
    set_description( _("Automatically add/delete input streams"))
    set_capability( "sout stream", 50 )
    add_shortcut( "setlang" );
    set_callbacks( OpenLang, Close )
    add_integer( SOUT_CFG_PREFIX_LANG "id", 0, ID_TEXT, ID_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX_LANG "lang", "eng", LANG_TEXT, LANG_LONGTEXT,
                false );

vlc_module_end()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *ppsz_sout_options_id[] = {
    "id", "new-id", NULL
};

static const char *ppsz_sout_options_lang[] = {
    "id", "lang", NULL
};

static sout_stream_id_t *AddId   ( sout_stream_t *, es_format_t * );
static sout_stream_id_t *AddLang ( sout_stream_t *, es_format_t * );
static int               Del     ( sout_stream_t *, sout_stream_id_t * );
static int               Send    ( sout_stream_t *, sout_stream_id_t *, block_t * );

struct sout_stream_sys_t
{
    sout_stream_t    *p_out;
    int              i_id;
    int              i_new_id;
    char             *psz_language;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int OpenCommon( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;

    p_sys = malloc( sizeof( sout_stream_sys_t ) );
    if( unlikely( !p_sys ) )
        return VLC_ENOMEM;

    if( !p_stream->p_next )
    {
        msg_Err( p_stream, "cannot create chain" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys     = p_sys;

    return VLC_SUCCESS;
}

static int OpenId( vlc_object_t *p_this )
{
    int i_ret = OpenCommon( p_this );
    if( i_ret != VLC_SUCCESS )
        return i_ret;

    sout_stream_t *p_stream = (sout_stream_t*)p_this;

    p_stream->p_sys->i_id = var_GetInteger( p_stream, SOUT_CFG_PREFIX_ID "id" );
    p_stream->p_sys->i_new_id = var_GetInteger( p_stream, SOUT_CFG_PREFIX_ID "new-id" );
    p_stream->p_sys->psz_language = NULL;

    config_ChainParse( p_stream, SOUT_CFG_PREFIX_ID, ppsz_sout_options_id,
                       p_stream->p_cfg );

    p_stream->pf_add = AddId;

    return VLC_SUCCESS;
}

static int OpenLang( vlc_object_t *p_this )
{
    int i_ret = OpenCommon( p_this );
    if( i_ret != VLC_SUCCESS )
        return i_ret;

    sout_stream_t *p_stream = (sout_stream_t*)p_this;

    p_stream->p_sys->i_id = var_GetInteger( p_stream, SOUT_CFG_PREFIX_LANG "id" );
    p_stream->p_sys->i_new_id = -1;
    p_stream->p_sys->psz_language = var_GetString( p_stream, SOUT_CFG_PREFIX_LANG "lang" );

    config_ChainParse( p_stream, SOUT_CFG_PREFIX_LANG, ppsz_sout_options_lang,
                       p_stream->p_cfg );

    p_stream->pf_add = AddLang;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    free( p_sys->psz_language );
    free( p_sys );
}

static sout_stream_id_t * AddId( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    if ( p_fmt->i_id == p_sys->i_id )
    {
        msg_Dbg( p_stream, "turning ID %d to %d",
                 p_sys->i_id, p_sys->i_new_id );
        p_fmt->i_id = p_sys->i_new_id;
    }

    return p_sys->p_out->pf_add( p_sys->p_out, p_fmt );
}

static sout_stream_id_t * AddLang( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    if ( p_fmt->i_id == p_sys->i_id )
    {
        msg_Dbg( p_stream, "turning language %s of ID %d to %s",
                 p_fmt->psz_language ? p_fmt->psz_language : "unk",
                 p_sys->i_id, p_sys->psz_language );
        p_fmt->psz_language = strdup( p_sys->psz_language );
    }

    return p_sys->p_out->pf_add( p_sys->p_out, p_fmt );
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    return p_sys->p_out->pf_del( p_sys->p_out, id );
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    return p_sys->p_out->pf_send( p_sys->p_out, id, p_buffer );
}
