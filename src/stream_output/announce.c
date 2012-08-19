/*****************************************************************************
 * announce.c : announce handler
 *****************************************************************************
 * Copyright (C) 2002-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include <vlc_sout.h>
#include "stream_output.h"
#include "libvlc.h"

#include <assert.h>

/****************************************************************************
 * Sout-side functions
 ****************************************************************************/

static void sap_destroy (vlc_object_t *p_this)
{
    libvlc_priv (p_this->p_libvlc)->p_sap = NULL;
}

#undef sout_AnnounceRegisterSDP

static vlc_mutex_t sap_mutex = VLC_STATIC_MUTEX;

/**
 *  Registers a new session with the announce handler, using a pregenerated SDP
 *
 * \param obj a VLC object
 * \param psz_sdp the SDP to register
 * \param psz_dst session address (needed for SAP address auto detection)
 * \return the new session descriptor structure
 */
session_descriptor_t *
sout_AnnounceRegisterSDP( vlc_object_t *obj, const char *psz_sdp,
                          const char *psz_dst )
{
    session_descriptor_t *p_session = calloc( 1, sizeof (*p_session) );
    if( !p_session )
        return NULL;

    p_session->psz_sdp = strdup( psz_sdp );

    /* GRUIK. We should not convert back-and-forth from string to numbers */
    struct addrinfo *res;
    if (vlc_getaddrinfo (psz_dst, 0, NULL, &res) == 0)
    {
        if (res->ai_addrlen <= sizeof (p_session->addr))
            memcpy (&p_session->addr, res->ai_addr,
                    p_session->addrlen = res->ai_addrlen);
        freeaddrinfo (res);
    }

    vlc_mutex_lock (&sap_mutex);
    sap_handler_t *p_sap = libvlc_priv (obj->p_libvlc)->p_sap;
    if (p_sap == NULL)
    {
        p_sap = SAP_Create (VLC_OBJECT (obj->p_libvlc));
        libvlc_priv (obj->p_libvlc)->p_sap = p_sap;
        vlc_object_set_destructor ((vlc_object_t *)p_sap, sap_destroy);
    }
    else
        vlc_object_hold ((vlc_object_t *)p_sap);
    vlc_mutex_unlock (&sap_mutex);

    if (p_sap == NULL)
        goto error;

    msg_Dbg (obj, "adding SAP session");
    if (SAP_Add (p_sap, p_session))
    {
        vlc_mutex_lock (&sap_mutex);
        vlc_object_release ((vlc_object_t *)p_sap);
        vlc_mutex_unlock (&sap_mutex);
        goto error;
    }

    return p_session;

error:
    free (p_session->psz_sdp);
    free (p_session);
    return NULL;
}

#undef sout_AnnounceUnRegister
/**
 *  Unregisters an existing session
 *
 * \param obj a VLC object
 * \param p_session the session descriptor
 * \return VLC_SUCCESS or an error
 */
int sout_AnnounceUnRegister( vlc_object_t *obj,
                             session_descriptor_t *p_session )
{
    sap_handler_t *p_sap = libvlc_priv (obj->p_libvlc)->p_sap;

    msg_Dbg (obj, "removing SAP session");
    SAP_Del (p_sap, p_session);

    vlc_mutex_lock (&sap_mutex);
    vlc_object_release ((vlc_object_t *)p_sap);
    vlc_mutex_unlock (&sap_mutex);

    free (p_session->psz_sdp);
    free (p_session);

    return 0;
}
