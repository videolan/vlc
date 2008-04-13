/*****************************************************************************
 * stream_output.h : internal stream output
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x.nl>
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
 ***************************************************************************/

#if defined(__PLUGIN__) || defined(__BUILTIN__) || !defined(__LIBVLC__)
# error This header file can only be included from LibVLC.
#endif

#ifndef VLC_SRC_STREAMOUT_H
# define VLC_SRC_STREAMOUT_H 1

# include <vlc_sout.h>
# include <vlc_network.h>

/****************************************************************************
 * sout_packetizer_input_t: p_sout <-> p_packetizer
 ****************************************************************************/
struct sout_packetizer_input_t
{
    sout_instance_t     *p_sout;

    es_format_t         *p_fmt;

    sout_stream_id_t    *id;
};

#define sout_NewInstance(a,b) __sout_NewInstance(VLC_OBJECT(a),b)
VLC_EXPORT( sout_instance_t *,  __sout_NewInstance,  ( vlc_object_t *, char * ) );
VLC_EXPORT( void,               sout_DeleteInstance, ( sout_instance_t * ) );

VLC_EXPORT( sout_packetizer_input_t *, sout_InputNew,( sout_instance_t *, es_format_t * ) );
VLC_EXPORT( int,                sout_InputDelete,      ( sout_packetizer_input_t * ) );
VLC_EXPORT( int,                sout_InputSendBuffer,  ( sout_packetizer_input_t *, block_t* ) );

/* Announce system */

/* The SAP handler, running in a separate thread */
struct sap_handler_t
{
    VLC_COMMON_MEMBERS /* needed to create a thread */

    sap_session_t **pp_sessions;
    sap_address_t **pp_addresses;

    bool b_control;

    int i_sessions;
    int i_addresses;

    int i_current_session;

    int (*pf_add)  ( sap_handler_t*, session_descriptor_t *);
    int (*pf_del)  ( sap_handler_t*, session_descriptor_t *);

    /* private data, not in p_sys as there is one kind of sap_handler_t */
};

struct session_descriptor_t
{
    struct sockaddr_storage orig;
    socklen_t origlen;
    struct sockaddr_storage addr;
    socklen_t addrlen;

    char *psz_sdp;
    bool b_ssm;
};

/* The main announce handler object */
struct announce_handler_t
{
    VLC_COMMON_MEMBERS

    sap_handler_t *p_sap;
};

int announce_HandlerDestroy( announce_handler_t * );

/* Release it with vlc_object_release() */
sap_handler_t *announce_SAPHandlerCreate( announce_handler_t *p_announce );

#endif
