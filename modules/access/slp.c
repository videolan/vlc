/*****************************************************************************
 * slp.c: SLP access plugin
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: slp.c,v 1.5 2003/01/21 17:34:03 lool Exp $
 *
 * Authors: Loïc Minier <lool@videolan.org>
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
#include <vlc/vlc.h>
#include <vlc/input.h>

#include <vlc_playlist.h>

#include <slp.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );
static ssize_t Read  ( input_thread_t *, byte_t *, size_t );

static int  Init  ( vlc_object_t * );
static void End   ( vlc_object_t * );
static int  Demux ( input_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define SRVTYPE_TEXT "SLP service type"
#define SRVTYPE_LONGTEXT "The service type string for SLP queries, " \
                         "including the authority string (if any) for the " \
                         "request. May not be empty"
#define SCOPELIST_TEXT "SLP scopes list"
#define SCOPELIST_LONGTEXT "This string is a comma separated list of scope " \
                           "names or empty if you want to use the default "  \
                           "scopes; it is used in all SLP queries"
#define NAMINGAUTHORITY_TEXT "SLP naming authority"
#define NAMINGAUTHORITY_LONGTEXT "This string is a list of naming " \
                                 "authorities to search. Use \"*\" for all " \
                                 "and the empty string for the default of " \
                                 "IANA"
#define FILTER_TEXT "SLP LDAP filter"
#define FILTER_LONGTEXT "This is a query formulated of attribute pattern " \
                        "matching expressions in the form of an LDAPv3 "   \
                        "search filter or empty for all answers"
#define LANG_TEXT "language asked in SLP requests"
#define LANG_LONGTEXT "RFC 1766 Language Tag for the natural language " \
                      "locale of requests, leave empty to use the "     \
                      "default locale"

vlc_module_begin();
    set_description( _("SLP access module") );
    add_category_hint( N_("slp"), NULL );
/*    add_string( "slp-srvtype", "service:vls.services.videolan.org:udpm",
                NULL, SRVTYPE_TEXT, SRVTYPE_LONGTEXT ); */
    add_string( "slp-scopelist", "", NULL, SCOPELIST_TEXT,
                SCOPELIST_LONGTEXT );
    add_string( "slp-namingauthority", "*", NULL, NAMINGAUTHORITY_TEXT,
                NAMINGAUTHORITY_LONGTEXT );
    add_string( "slp-filter", "", NULL, FILTER_TEXT, FILTER_LONGTEXT );
    add_string( "slp-lang", "", NULL, LANG_TEXT, LANG_LONGTEXT );
    add_submodule();
        set_capability( "access", 0 );
        set_callbacks( Open, Close );
    add_submodule();
        add_shortcut( "demux_slp" );
        set_capability( "demux", 0 );
        set_callbacks( Init, End );
vlc_module_end();

/*****************************************************************************
 * SrvUrlCallback: adds an entry to the playlist
 *****************************************************************************/
static SLPBoolean SrvUrlCallback( SLPHandle slph_slp,
                           const char * psz_srvurl,
                           uint16_t i_lifetime,
                           SLPError slpe_errcode,
                           void * p_cookie )
{
    input_thread_t * p_input = (input_thread_t  *)p_cookie;
    playlist_t * p_playlist;
    char psz_item[42] = "udp:@";
    char * psz_s;

    /* our callback was only called to tell us there's nothing more to read */
    if( slpe_errcode == SLP_LAST_CALL )
    {
        return SLP_TRUE;
    }

    /* or there was a problem with getting the data we requested */
    if( (slpe_errcode != SLP_OK) )
    {
        msg_Err( p_input,
                 "SrvUrlCallback got an error %i with URL %s",
                 slpe_errcode,
                 psz_srvurl );
        return SLP_TRUE;
    }

    /* search the returned address after a double-slash */
    psz_s = strstr( psz_srvurl, "//" );
    /* skip the slashes */
    psz_s = &psz_s[2];
    if( psz_s == NULL )
    {
        msg_Err( (input_thread_t *)p_input,
                 "SrvUrlCallback got a NULL string if your libslp" );
        return SLP_TRUE;
    }
    /* add udp:@ in front of the address */
    psz_s = strncat( psz_item,
                     psz_s,
                     sizeof(psz_item) - strlen(psz_item) - 1 );

    /* search the main playlist object */
    p_playlist = vlc_object_find( (input_thread_t *)p_input,
                                  VLC_OBJECT_PLAYLIST,
                                  FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        msg_Warn( (input_thread_t *)p_input,
                  "could not find playlist, not adding entries" );
        return SLP_TRUE;
    }

    playlist_Add( p_playlist, psz_s,
                  PLAYLIST_APPEND,
                  PLAYLIST_END );
    vlc_object_release( (vlc_object_t *)p_playlist );

    msg_Info( (input_thread_t *)p_input,
             "added « %s » (lifetime %i) to playlist",
             psz_srvurl,
             i_lifetime );

    return SLP_TRUE;
}

/*****************************************************************************
 * SrvTypeCallback: searchs all servers of a certain type
 *****************************************************************************/
static SLPBoolean SrvTypeCallback( SLPHandle slph_slp,
                           const char * psz_srvurl,
                           SLPError slpe_errcode,
                           void * p_cookie )
{
    input_thread_t * p_input = (input_thread_t  *)p_cookie;
    SLPError slpe_result;
    SLPHandle slph_slp2;

    /* our callback was only called to tell us there's nothing more to read */
    if( slpe_errcode == SLP_LAST_CALL )
    {
        return SLP_TRUE;
    }

    /* or there was a problem with getting the data we requested */
    if( slpe_errcode != SLP_OK )
    {
        msg_Err( p_input,
                 "SrvTypeCallback got an error %i with URL %s",
                 slpe_errcode,
                 psz_srvurl );
        return SLP_TRUE;
    }

    /* get a new handle to the library */
    if( SLPOpen( config_GetPsz( p_input, "slp-lang" ),
                 SLP_FALSE,                              /* synchronous ops */
                 &slph_slp2 ) == SLP_OK )
    {
        /* search for services */
        slpe_result = SLPFindSrvs( slph_slp2,
                                   psz_srvurl,
                                   config_GetPsz( p_input, "slp-scopelist" ),
                                   config_GetPsz( p_input, "slp-filter" ),
                                   SrvUrlCallback,
                                   p_input );

        SLPClose( slph_slp2 );

        if( slpe_result != SLP_OK )
        {
            msg_Err( p_input,
                     "SLPFindSrvs error %i finding servers of type %s",
                     slpe_result,
                     psz_srvurl );
        }
    }

    return SLP_TRUE;
}

/*****************************************************************************
 * Open: initialize library for the access module
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    input_thread_t * p_input = (input_thread_t *)p_this;
    SLPError         slpe_result;
    SLPHandle        slph_slp;
    playlist_t *     p_playlist;

    /* remove the "slp:" entry of the playlist */
    p_playlist = (playlist_t *) vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
                                                 FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Warn( p_input, "hey I can't find the main playlist, I need it" );
        return VLC_FALSE;
    }

    p_playlist->pp_items[p_playlist->i_index]->b_autodeletion = VLC_TRUE;
    vlc_object_release( (vlc_object_t *)p_playlist );

    /* get a new handle to the library */
    if( SLPOpen( config_GetPsz( p_input, "slp-lang" ),
                 SLP_FALSE,                              /* synchronous ops */
                 &slph_slp ) == SLP_OK )
    {
        /* search all service types */
        slpe_result =
            SLPFindSrvTypes( slph_slp,
                             config_GetPsz( p_input, "slp-namingauthority" ),
                             config_GetPsz( p_input, "slp-scopelist" ),
                             SrvTypeCallback,
                             p_input );
        /* we're done, clean up */
        SLPClose( slph_slp );
    }

    if( !p_input->psz_demux || !*p_input->psz_demux )
    {
        p_input->psz_demux = "demux_slp";
    }

    p_input->pf_read = Read;
    p_input->pf_set_program = NULL;
    p_input->pf_set_area = NULL;
    p_input->pf_seek = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = VLC_FALSE;
    p_input->stream.b_seekable = VLC_FALSE;
    p_input->stream.b_connected = VLC_TRUE;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.p_selected_area->i_size = 0;
    p_input->stream.i_method = INPUT_METHOD_SLP;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    p_input->i_mtu = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close access
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    return;
}

/*****************************************************************************
 * Read: should fill but zeroes the buffer
 *****************************************************************************/
static ssize_t Read  ( input_thread_t *p_input, byte_t *p_buffer, size_t s )
{
    memset( p_buffer, 0, s );
    return s;
}

/*****************************************************************************
 * Init: initialize demux
 *****************************************************************************/
static int Init ( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;

    if( p_input->stream.i_method != INPUT_METHOD_SLP )
    {
        return VLC_FALSE;
    }

    p_input->pf_demux  = Demux;
    p_input->pf_rewind = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Demux: should demux but does nothing
 *****************************************************************************/
static int Demux ( input_thread_t * p_input )
{
    return 0;
}

/*****************************************************************************
 * End: end demux
 *****************************************************************************/
static void End ( vlc_object_t *p_this )
{
    return;
}

