/*****************************************************************************
 * slp.c: SLP access plugin
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id$
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
#include <stdlib.h>                                                /* malloc */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include <vlc_playlist.h>

#include <slp.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );
static ssize_t Read( input_thread_t *, byte_t *, size_t );

static int  Init  ( vlc_object_t * );
static void End   ( vlc_object_t * );
static int  Demux ( input_thread_t * );

int i_group;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#if 0
#define SRVTYPE_TEXT /*N_*/("SLP service type")
#define SRVTYPE_LONGTEXT /*N_*/( \
    "The service type string for SLP queries, including the authority " \
    "string (if any) for the request. May not be empty." )
#endif

#define ATTRIDS_TEXT N_("SLP attribute identifiers")
#define ATTRIDS_LONGTEXT N_( \
    "This string is a comma separated list of attribute identifiers to " \
    "search for a playlist title or empty to use all attributes." )

#define SCOPELIST_TEXT N_("SLP scopes list")
#define SCOPELIST_LONGTEXT N_( \
    "This string is a comma separated list of scope names or empty if you " \
    "want to use the default scopes. It is used in all SLP queries." )

#define NAMINGAUTHORITY_TEXT N_("SLP naming authority")
#define NAMINGAUTHORITY_LONGTEXT N_( \
    "This string is a list of naming authorities to search. " \
    "Use \"*\" for all and the empty string for the default of IANA." )

#define FILTER_TEXT N_("SLP LDAP filter")
#define FILTER_LONGTEXT N_( \
    "This is a query formulated of attribute pattern matching expressions " \
    "in the form of an LDAPv3 search filter or empty for all answers." )

#define LANG_TEXT N_("Language requested in SLP requests")
#define LANG_LONGTEXT N_( \
    "RFC 1766 Language tag for the natural language locale of requests, " \
    "leave empty to use the default locale. It is used in all SLP queries." )

vlc_module_begin();
    set_description( _("SLP input") );

    add_string( "slp-attrids", "", NULL, ATTRIDS_TEXT, ATTRIDS_LONGTEXT,
                VLC_TRUE );
    add_string( "slp-scopelist", "", NULL, SCOPELIST_TEXT,
                SCOPELIST_LONGTEXT, VLC_TRUE );
    add_string( "slp-namingauthority", "*", NULL, NAMINGAUTHORITY_TEXT,
                NAMINGAUTHORITY_LONGTEXT, VLC_TRUE );
    add_string( "slp-filter", "", NULL, FILTER_TEXT, FILTER_LONGTEXT,
                VLC_TRUE );
    add_string( "slp-lang", "", NULL, LANG_TEXT, LANG_LONGTEXT, VLC_TRUE );

    set_capability( "access", 0 );
    set_callbacks( Open, Close );

    add_submodule();
        add_shortcut( "demux_slp" );
        set_capability( "demux", 0 );
        set_callbacks( Init, End );
vlc_module_end();

/*****************************************************************************
 * AttrCallback: updates the description of a playlist item
 *****************************************************************************/
static SLPBoolean AttrCallback( SLPHandle slph_slp,
                           const char * psz_attrlist,
                           SLPError slpe_errcode,
                           void * p_cookie )
{
    playlist_item_t * p_playlist_item = (playlist_item_t *)p_cookie;

    /* our callback was only called to tell us there's nothing more to read */
    if( slpe_errcode == SLP_LAST_CALL )
    {
        return SLP_TRUE;
    }

    /* or there was a problem with getting the data we requested */
    if( (slpe_errcode != SLP_OK) )
    {
#if 0
        msg_Err( (vlc_object_t*)NULL,
                 "AttrCallback got an error %i with attribute %s",
                 slpe_errcode,
                 psz_attrlist );
#endif
        return SLP_TRUE;
    }

    if( p_playlist_item->input.psz_name )
        free( p_playlist_item->input.psz_name );

    p_playlist_item->input.psz_name = strdup(psz_attrlist); /* NULL is checked */
    return SLP_TRUE;
}

/*****************************************************************************
 * SrvUrlCallback: adds an entry to the playlist
 *****************************************************************************/
static SLPBoolean SrvUrlCallback( SLPHandle slph_slp,
                           const char * psz_srvurl,
                           uint16_t i_lifetime,
                           SLPError slpe_errcode,
                           void * p_cookie )
{
    input_thread_t *p_input = (input_thread_t  *)p_cookie;
    playlist_t * p_playlist;
    char psz_item[42] = ""; //"udp:@";
    char * psz_s;                           /* to hold the uri of the stream */
    SLPHandle slph_slp3;
    SLPError slpe_result;
    playlist_item_t * p_playlist_item;

    /* our callback was only called to tell us there's nothing more to read */
    if( slpe_errcode == SLP_LAST_CALL )
    {
        return SLP_TRUE;
    }

    msg_Dbg( p_input,"URL: %s", psz_srvurl );

    /* or there was a problem with getting the data we requested */
    if( (slpe_errcode != SLP_OK) )
    {
        msg_Err( p_input, "SrvUrlCallback got an error %i with URL %s",
                 slpe_errcode, psz_srvurl );
        return SLP_TRUE;
    }

    /* search the returned address after a double-slash */
    psz_s = strstr( psz_srvurl, "//" );
    if( psz_s == NULL )
    {
        msg_Err( p_input,
                 "SrvUrlCallback got a strange string of your libslp" );
        return SLP_TRUE;
    }
    /* skip the slashes */
    psz_s = &psz_s[2];
    /* add udp:@ in front of the address */
    psz_s = strncat( psz_item,
                     psz_s,
                     sizeof(psz_item) - strlen(psz_item) - 1 );

    /* create a playlist  item */
    p_playlist_item = playlist_ItemNew( p_input, psz_s, NULL );
    if( p_playlist_item == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return SLP_TRUE;
    }

    p_playlist_item->i_group = i_group;
    p_playlist_item->b_enabled = VLC_TRUE;

    /* search the description of the stream */
    if( SLPOpen( config_GetPsz( p_input, "slp-lang" ),
                 SLP_FALSE,                              /* synchronous ops */
                 &slph_slp3 ) == SLP_OK )
    {
        /* search all attributes */
        slpe_result = SLPFindAttrs( slph_slp3,
                                    psz_srvurl,
                                    config_GetPsz( p_input, "slp-scopelist" ),
                                    config_GetPsz( p_input, "slp-attrids" ),
                                    AttrCallback,
                                    p_playlist_item
                                  );

        /* we're done, clean up */
        SLPClose( slph_slp3 );
    }

    /* search the main playlist object */
    p_playlist = vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
                                  FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        msg_Warn( p_input, "could not find playlist, not adding entries" );
        return SLP_TRUE;
    }

    playlist_AddItem( p_playlist, p_playlist_item,
                      PLAYLIST_APPEND, PLAYLIST_END );
    vlc_object_release( p_playlist );

    msg_Info( p_input, "added « %s » (lifetime %i) to playlist",
              psz_srvurl, i_lifetime );

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
    char *psz_eos;
    char *psz_service;

    msg_Dbg( p_input, "services: %s", psz_srvurl );
    /* our callback was only called to tell us there's nothing more to read */
    if( slpe_errcode == SLP_LAST_CALL )
    {
        return SLP_TRUE;
    }

    msg_Dbg( p_input, "services: %s", psz_srvurl );

    /* or there was a problem with getting the data we requested */
    if( slpe_errcode != SLP_OK )
    {
        msg_Err( p_input, "SrvTypeCallback got an error %i with URL %s",
                 slpe_errcode, psz_srvurl );
        return SLP_TRUE;
    }

    /* get a new handle to the library */
    if( SLPOpen( config_GetPsz( p_input, "slp-lang" ),
                 SLP_FALSE,                              /* synchronous ops */
                 &slph_slp2 ) == SLP_OK )
    {
        /* search for services */
        while(1)
        {
            if( *psz_srvurl == '\0')  break;

            if( !strncasecmp( psz_srvurl, "service:", 8 ) )
            {
                while(1)
                {
                    psz_eos = strchr( psz_srvurl, ',');
                    if(!psz_eos) break;
                    if(!strncasecmp(psz_eos+1,"service:",8)) break;
                }

                if(psz_eos)
                    *psz_eos = '\0';

                psz_service = strdup( psz_srvurl);

                msg_Dbg( p_input, "getting details for %s", psz_service );

                slpe_result = SLPFindSrvs( slph_slp2,
                                   psz_service,
                                   config_GetPsz( p_input, "slp-scopelist" ),
                                   config_GetPsz( p_input, "slp-filter" ),
                                   SrvUrlCallback,
                                   p_input );

                if(psz_eos)
                    psz_srvurl = psz_eos;

#if 0
                SLPClose( slph_slp2 );
#endif
                if( slpe_result != SLP_OK )
                {
                   msg_Err( p_input,
                           "SLPFindSrvs error %i finding servers of type %s",
                           slpe_result, psz_service );
                }
            }
            psz_srvurl++;
        }
    }
                SLPClose( slph_slp2 );

    return SLP_TRUE;
}

/*****************************************************************************
 * Open: initialize library for the access module
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    input_thread_t *   p_input = (input_thread_t *)p_this;
    SLPError           slpe_result;
    SLPHandle          slph_slp;
    playlist_t *       p_playlist;
    playlist_group_t * p_group;

    /* remove the "slp:" entry of the playlist */
    p_playlist = (playlist_t *) vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
                                                 FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Warn( p_input, "hey I can't find the main playlist, I need it" );
        return VLC_FALSE;
    }

    p_group = playlist_CreateGroup( p_playlist , "SLP" );
    i_group = p_group->i_id;
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
    p_input->pf_demux_control = demux_vaControlDefault;
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
