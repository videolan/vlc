/*****************************************************************************
 * announce.c : announce handler
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/sout.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define FREE( p ) if( p ) { free( p ); (p) = NULL; }


/****************************************************************************
 * Sout-side functions
 ****************************************************************************/

/**
 *  Register a new session with the announce handler
 *
 * \param p_sout a sout instance structure
 * \param p_session a session descriptor
 * \param p_method an announce method descriptor
 * \return VLC_SUCCESS or an error
 */
int sout_AnnounceRegister( sout_instance_t *p_sout,
                       session_descriptor_t *p_session,
                       announce_method_t *p_method )
{
    int i_ret;
    announce_handler_t *p_announce = (announce_handler_t*)
                              vlc_object_find( p_sout,
                                              VLC_OBJECT_ANNOUNCE,
                                              FIND_ANYWHERE );

    if( !p_announce )
    {
        msg_Dbg( p_sout, "No announce handler found, creating one" );
        p_announce = announce_HandlerCreate( p_sout );
        if( !p_announce )
        {
            msg_Err( p_sout, "Creation failed" );
            return VLC_ENOMEM;
        }
        vlc_object_yield( p_announce );
        msg_Dbg( p_sout,"Creation done" );
    }

    i_ret = announce_Register( p_announce, p_session, p_method );
    vlc_object_release( p_announce );

    return i_ret;
}

/**
 *  Register a new session with the announce handler, using a pregenerated SDP
 *
 * \param p_sout a sout instance structure
 * \param psz_sdp the SDP to register
 * \param p_method an announce method descriptor
 * \return the new session descriptor structure
 */
session_descriptor_t *sout_AnnounceRegisterSDP( sout_instance_t *p_sout,
                          char *psz_sdp, announce_method_t *p_method )
{
    session_descriptor_t *p_session;
    announce_handler_t *p_announce = (announce_handler_t*)
                                     vlc_object_find( p_sout,
                                              VLC_OBJECT_ANNOUNCE,
                                              FIND_ANYWHERE );
    if( !p_announce )
    {
        msg_Dbg( p_sout, "no announce handler found, creating one" );
        p_announce = announce_HandlerCreate( p_sout );
        if( !p_announce )
        {
            msg_Err( p_sout, "Creation failed" );
            return NULL;
        }
        vlc_object_yield( p_announce );
    }

    if( p_method->i_type != METHOD_TYPE_SAP )
    {
        msg_Warn( p_sout,"forcing SAP announcement");
    }

    p_session = sout_AnnounceSessionCreate();
    p_session->psz_sdp = strdup( psz_sdp );
    announce_Register( p_announce, p_session, p_method );

    vlc_object_release( p_announce );
    return p_session;
}

/**
 *  UnRegister an existing session
 *
 * \param p_sout a sout instance structure
 * \param p_session the session descriptor
 * \return VLC_SUCCESS or an error
 */
int sout_AnnounceUnRegister( sout_instance_t *p_sout,
                             session_descriptor_t *p_session )
{
    int i_ret;
    announce_handler_t *p_announce = (announce_handler_t*)
                              vlc_object_find( p_sout,
                                              VLC_OBJECT_ANNOUNCE,
                                              FIND_ANYWHERE );
    if( !p_announce )
    {
        msg_Dbg( p_sout, "Unable to remove announce: no announce handler" );
        return VLC_ENOOBJ;
    }
    i_ret  = announce_UnRegister( p_announce, p_session );

    vlc_object_release( p_announce );

    return i_ret;
}

/**
 * Create and initialize a session descriptor
 *
 * \return a new session descriptor
 */
session_descriptor_t * sout_AnnounceSessionCreate(void)
{
    session_descriptor_t *p_session;

    p_session = (session_descriptor_t *)malloc( sizeof(session_descriptor_t));

    if( p_session)
    {
        p_session->p_sap = NULL;
        p_session->psz_sdp = NULL;
        p_session->psz_name = NULL;
        p_session->psz_uri = NULL;
        p_session->i_port = 0;
        p_session->psz_group = NULL;
    }

    return p_session;
}

/**
 * Destroy a session descriptor and free all
 *
 * \param p_session the session to destroy
 * \return Nothing
 */
void sout_AnnounceSessionDestroy( session_descriptor_t *p_session )
{
    if( p_session )
    {
        FREE( p_session->psz_name );
        FREE( p_session->psz_group );
        FREE( p_session->psz_uri );
        FREE( p_session->psz_sdp );
        free( p_session );
    }
}

/**
 * Create and initialize an announcement method structure
 *
 * \param i_type METHOD_TYPE_SAP or METHOD_TYPE_SLP
 * \return a new announce_method structure
 */
announce_method_t * sout_AnnounceMethodCreate( int i_type )
{
    announce_method_t *p_method;

    p_method = (announce_method_t *)malloc( sizeof(announce_method_t) );

    if( p_method )
    {
        p_method->i_type = i_type;
        if( i_type == METHOD_TYPE_SAP )
            /* Default value */
            p_method->psz_address = NULL;
    }
    return p_method;
}

/************************************************************************
 * Announce handler functions (private)
 ************************************************************************/

/**
 * Create the announce handler object
 *
 * \param p_this a vlc_object structure
 * \return the new announce handler or NULL on error
 */
announce_handler_t *__announce_HandlerCreate( vlc_object_t *p_this )
{
    announce_handler_t *p_announce;

    p_announce = vlc_object_create( p_this, VLC_OBJECT_ANNOUNCE );

    if( !p_announce )
    {
        msg_Err( p_this, "out of memory" );
        return NULL;
    }

    p_announce->p_sap = NULL;

    vlc_object_attach( p_announce, p_this->p_vlc);


    return p_announce;
}

/**
 * Destroy a  announce handler object
 *
 * \param p_announce the announce handler to destroy
 * \return VLC_SUCCESS or an error
 */
int announce_HandlerDestroy( announce_handler_t *p_announce )
{

    if( p_announce->p_sap )
    {
        p_announce->p_sap->b_die = VLC_TRUE;
        /* Wait for the SAP thread to exit */
        vlc_thread_join( p_announce->p_sap );
        announce_SAPHandlerDestroy( p_announce->p_sap );
    }

    /* Free the structure */
    vlc_object_destroy( p_announce );

    return VLC_SUCCESS;
}

/* Register an announce */
int announce_Register( announce_handler_t *p_announce,
                       session_descriptor_t *p_session,
                       announce_method_t *p_method )
{

    msg_Dbg( p_announce, "registering announce");
    if( p_method->i_type == METHOD_TYPE_SAP )
    {
        /* Do we already have a SAP announce handler ? */
        if( !p_announce->p_sap )
        {
            sap_handler_t *p_sap = announce_SAPHandlerCreate( p_announce );
            msg_Dbg( p_announce, "creating SAP announce handler");
            if( !p_sap )
            {
                msg_Err( p_announce, "SAP handler creation failed" );
                return VLC_ENOOBJ;
            }
            p_announce->p_sap = p_sap;
        }
        /* this will set p_session->p_sap for later deletion */
        msg_Dbg( p_announce, "adding SAP session");
        p_announce->p_sap->pf_add( p_announce->p_sap, p_session, p_method );
    }
    else if( p_method->i_type == METHOD_TYPE_SLP )
    {
        msg_Dbg( p_announce, "SLP unsupported at the moment" );
        return VLC_EGENERIC;
    }
    else
    {
        msg_Dbg( p_announce, "Announce type unsupported" );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;;
}


/* Unregister an announce */
int announce_UnRegister( announce_handler_t *p_announce,
                  session_descriptor_t *p_session )
{
    msg_Dbg( p_announce, "unregistering announce" );
    if( p_session->p_sap != NULL ) /* SAP Announce */
    {
        if( !p_announce->p_sap )
        {
            msg_Err( p_announce, "can't remove announce, no SAP handler");
            return VLC_ENOOBJ;
        }
        p_announce->p_sap->pf_del( p_announce->p_sap, p_session );
    }
    return VLC_SUCCESS;
}
