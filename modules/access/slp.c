/*****************************************************************************
 * slp.c: SLP access plugin
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: slp.c,v 1.1 2003/01/10 04:58:23 lool Exp $
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

#include <slp.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open       ( vlc_object_t * );
static void Close      ( vlc_object_t * );

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
                           "scopes"
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
    set_capability( "access", 0 );
    add_shortcut( "slp" );
    add_string( "slp-srvtype", "service:vls.services.videolan.org:udpm",
                NULL, SRVTYPE_TEXT, SRVTYPE_LONGTEXT );
    add_string( "slp-scopelist", "", NULL, SCOPELIST_TEXT,
                SCOPELIST_LONGTEXT );
    add_string( "slp-filter", "", NULL, FILTER_TEXT, FILTER_LONGTEXT );
    add_string( "slp-lang", "", NULL, LANG_TEXT, LANG_LONGTEXT );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * SrvUrlCallback: adds an entry to the playlist
 *****************************************************************************/
static SLPBoolean SrvUrlCallback( SLPHandle hslp,
                           const char * psz_srvurl,
                           uint16_t i_lifetime,
                           SLPError slpe_errcode,
                           void * p_input )
{
    playlist_t * p_playlist;
    char psz_item[42] = "udp:@";
    char * psz_s;

    if( slpe_errcode == SLP_OK )
    {
        p_playlist = vlc_object_find( (input_thread_t *)p_input,
                                      VLC_OBJECT_PLAYLIST,
                                      FIND_ANYWHERE );
        if( p_playlist == NULL )
        {
            msg_Dbg( (input_thread_t *)p_input, "could not find playlist" );
            return SLP_FALSE;
        }

        /* search the returned address after a double-slash */
        psz_s = strstr( psz_srvurl, "//" );
        /* skip the slashes */
        psz_s = &psz_s[2];
        if( psz_s == NULL )
        {
            msg_Dbg( (input_thread_t *)p_input,
                     "something went wrong with your libslp" );
            return SLP_FALSE;
        }
        /* add udp:@ in front of the address */
        psz_s = strncat( psz_item,
                         psz_s,
                         sizeof(psz_item) - strlen(psz_item) - 1 );
        playlist_Add( p_playlist, psz_s,
                      PLAYLIST_APPEND | PLAYLIST_GO,
                      PLAYLIST_END );
        vlc_object_release( (vlc_object_t *)p_playlist );

        msg_Dbg( (input_thread_t *)p_input,
                 "added « %s » (lifetime %i) to playlist",
                 psz_srvurl,
                 i_lifetime );
    }

    return SLP_TRUE;
}

/*****************************************************************************
 * Open: initialize library
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    input_thread_t * p_input = (input_thread_t *)p_this;
    char *           psz_name = strdup(p_input->psz_name);
    SLPError         slpe_result;
    SLPHandle        slph_slp;

    /* get a new handle to the library */
    if( SLPOpen( config_GetPsz( p_input, "slp-lang" ),
                 SLP_FALSE,                              /* synchronous ops */
                 &slph_slp ) == SLP_OK )
    {
        /* search for services */
        slpe_result = SLPFindSrvs( slph_slp,
                                   config_GetPsz( p_input, "slp-srvtype" ),
                                   config_GetPsz( p_input, "slp-scopelist" ),
                                   config_GetPsz( p_input, "slp-filter" ),
                                   SrvUrlCallback,
                                   p_input );
        if( slpe_result != SLP_OK )
        {
            msg_Dbg( p_input,
                     "slp error opening %s: %i",
                     psz_name,
                     slpe_result );
        }
        /* we're done, clean up */
        SLPClose( slph_slp );
    }

    return( -1 );
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{

}

