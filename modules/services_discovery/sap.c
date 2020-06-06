/*****************************************************************************
 * sap.c :  SAP interface module
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
 * Copyright © 2007 Rémi Denis-Courmont
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Rémi Denis-Courmont
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
 * Includes
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <assert.h>

#include <vlc_demux.h>
#include <vlc_services_discovery.h>

#include <vlc_network.h>
#include <vlc_charset.h>

#include <errno.h>
#include <unistd.h>
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_POLL_H
# include <poll.h>
#endif

#ifdef HAVE_ZLIB_H
#   include <zlib.h>
#endif

#ifdef HAVE_NET_IF_H
#   include <net/if.h>
#endif

#include "access/rtp/sdp.h"

/************************************************************************
 * Macros and definitions
 ************************************************************************/

#define MAX_LINE_LENGTH 256

/* SAP is always on that port */
#define SAP_PORT 9875
/* Global-scope SAP address */
#define SAP_V4_GLOBAL_ADDRESS   "224.2.127.254"
/* Organization-local SAP address */
#define SAP_V4_ORG_ADDRESS      "239.195.255.255"
/* Local (smallest non-link-local scope) SAP address */
#define SAP_V4_LOCAL_ADDRESS    "239.255.255.255"
/* Link-local SAP address */
#define SAP_V4_LINK_ADDRESS     "224.0.0.255"
#define ADD_SESSION 1

static int Decompress( const unsigned char *psz_src, unsigned char **_dst, int i_len )
{
#ifdef HAVE_ZLIB_H
    int i_result, i_dstsize, n = 0;
    unsigned char *psz_dst = NULL;
    z_stream d_stream;

    memset (&d_stream, 0, sizeof (d_stream));

    i_result = inflateInit(&d_stream);
    if( i_result != Z_OK )
        return( -1 );

    d_stream.next_in = (Bytef *)psz_src;
    d_stream.avail_in = i_len;

    do
    {
        n++;
        psz_dst = xrealloc( psz_dst, n * 1000 );
        d_stream.next_out = (Bytef *)&psz_dst[(n - 1) * 1000];
        d_stream.avail_out = 1000;

        i_result = inflate(&d_stream, Z_NO_FLUSH);
        if( ( i_result != Z_OK ) && ( i_result != Z_STREAM_END ) )
        {
            inflateEnd( &d_stream );
            free( psz_dst );
            return( -1 );
        }
    }
    while( ( d_stream.avail_out == 0 ) && ( d_stream.avail_in != 0 ) &&
           ( i_result != Z_STREAM_END ) );

    i_dstsize = d_stream.total_out;
    inflateEnd( &d_stream );

    *_dst = xrealloc( psz_dst, i_dstsize );

    return i_dstsize;
#else
    (void)psz_src;
    (void)_dst;
    (void)i_len;
    return -1;
#endif
}

typedef struct sap_announce_t
{
    vlc_tick_t i_last;
    vlc_tick_t i_period;
    uint8_t i_period_trust;

    uint16_t    i_hash;
    uint32_t    i_source[4];

    input_item_t * p_item;
} sap_announce_t;

static sap_announce_t *CreateAnnounce(services_discovery_t *p_sd,
                                      const uint32_t *i_source,
                                      uint16_t i_hash, const char *psz_sdp)
{
    /* Parse SDP info */
    struct vlc_sdp *p_sdp = vlc_sdp_parse(psz_sdp, strlen(psz_sdp));
    if (p_sdp == NULL)
        return NULL;

    char *uri = NULL;

    if (asprintf(&uri, "sdp://%s", psz_sdp) == -1)
    {
        vlc_sdp_free(p_sdp);
        return NULL;
    }

    input_item_t *p_input;
    const char *psz_value;
    sap_announce_t *p_sap = malloc(sizeof (*p_sap));
    if( p_sap == NULL )
        return NULL;

    p_sap->i_last = vlc_tick_now();
    p_sap->i_period = 0;
    p_sap->i_period_trust = 0;
    p_sap->i_hash = i_hash;
    memcpy (p_sap->i_source, i_source, sizeof(p_sap->i_source));

    /* Released in RemoveAnnounce */
    p_input = input_item_NewStream(uri, p_sdp->name,
                                   INPUT_DURATION_INDEFINITE);
    if( unlikely(p_input == NULL) )
    {
        free( p_sap );
        return NULL;
    }
    p_sap->p_item = p_input;

    vlc_meta_t *p_meta = vlc_meta_New();
    if( likely(p_meta != NULL) )
    {
        vlc_meta_Set(p_meta, vlc_meta_Description, p_sdp->info);
        p_input->p_meta = p_meta;
    }

    psz_value = vlc_sdp_attr_value(p_sdp, "tool");
    if( psz_value != NULL )
    {
        input_item_AddInfo( p_input, _("Session"), _("Tool"), "%s", psz_value );
    }

    /* Handle category */
    psz_value = vlc_sdp_attr_value(p_sdp, "cat");
    if (psz_value != NULL)
    {
        /* a=cat provides a dot-separated hierarchy.
         * For the time being only replace dots with pipe. TODO: FIXME */
        char *str = strdup(psz_value);
        if (likely(str != NULL))
            for (char *p = strchr(str, '.'); p != NULL; p = strchr(p, '.'))
                *(p++) = '|';
        services_discovery_AddItemCat(p_sd, p_input, str ? str : psz_value);
        free(str);
    }
    else
    {
        /* backward compatibility with VLC 0.7.3-2.0.0 senders */
        psz_value = vlc_sdp_attr_value(p_sdp, "x-plgroup");
        services_discovery_AddItemCat(p_sd, p_input, psz_value);
    }

    vlc_sdp_free(p_sdp);
    return p_sap;
}

typedef struct
{
    vlc_thread_t thread;

    /* Socket descriptors */
    int i_fd;
    int *pi_fd;

    /* Table of announces */
    int i_announces;
    struct sap_announce_t **pp_announces;

    vlc_tick_t i_timeout;
} services_discovery_sys_t;

static int RemoveAnnounce( services_discovery_t *p_sd,
                           sap_announce_t *p_announce )
{
    if( p_announce->p_item )
    {
        services_discovery_RemoveItem( p_sd, p_announce->p_item );
        input_item_Release( p_announce->p_item );
        p_announce->p_item = NULL;
    }

    services_discovery_sys_t *p_sys = p_sd->p_sys;
    TAB_REMOVE(p_sys->i_announces, p_sys->pp_announces, p_announce);
    free( p_announce );

    return VLC_SUCCESS;
}

static int InitSocket( services_discovery_t *p_sd, const char *psz_address,
                       int i_port )
{
    int i_fd = net_ListenUDP1 ((vlc_object_t *)p_sd, psz_address, i_port);
    if (i_fd == -1)
        return VLC_EGENERIC;

    shutdown( i_fd, SHUT_WR );
    services_discovery_sys_t *p_sys = p_sd->p_sys;
    TAB_APPEND(p_sys->i_fd, p_sys->pi_fd, i_fd);
    return VLC_SUCCESS;
}

/* i_read is at least > 6 */
static int ParseSAP( services_discovery_t *p_sd, const uint8_t *buf,
                     size_t len )
{
    services_discovery_sys_t *p_sys = p_sd->p_sys;
    const char          *psz_sdp;
    const uint8_t *end = buf + len;
    uint32_t            i_source[4];

    assert (buf[len] == '\0');

    if (len < 4)
        return VLC_EGENERIC;

    uint8_t flags = buf[0];
    uint8_t auth_len = buf[1];

    /* First, check the sap announce is correct */
    if ((flags >> 5) != 1)
        return VLC_EGENERIC;

    bool b_ipv6 = (flags & 0x10) != 0;
    bool b_need_delete = (flags & 0x04) != 0;

    if (flags & 0x02)
    {
        msg_Dbg( p_sd, "encrypted packet, unsupported" );
        return VLC_EGENERIC;
    }

    bool b_compressed = (flags & 0x01) != 0;

    uint16_t i_hash = U16_AT (buf + 2);

    if( i_hash == 0 )
        return VLC_EGENERIC;

    buf += 4;
    if( b_ipv6 )
    {
        for( int i = 0; i < 4; i++,buf+=4)
            i_source[i] = U32_AT(buf);
    }
    else
    {
        memset(i_source, 0, sizeof(i_source));
        i_source[3] = U32_AT(buf);
        buf+=4;
    }
    // Skips auth data
    buf += auth_len;
    if (buf > end)
        return VLC_EGENERIC;

    uint8_t *decomp = NULL;
    if( b_compressed )
    {
        int newsize = Decompress (buf, &decomp, end - buf);
        if (newsize < 0)
        {
            msg_Dbg( p_sd, "decompression of SAP packet failed" );
            return VLC_EGENERIC;
        }

        decomp = xrealloc (decomp, newsize + 1);
        decomp[newsize] = '\0';
        psz_sdp = (const char *)decomp;
        len = newsize;
    }
    else
    {
        psz_sdp = (const char *)buf;
        len = end - buf;
    }

    /* len is a strlen here here. both buf and decomp are len+1 where the 1 should be a \0 */
    assert( psz_sdp[len] == '\0');

    /* Skip payload type */
    /* SAPv1 has implicit "application/sdp" payload type: first line is v=0 */
    if (strncmp (psz_sdp, "v=0", 3))
    {
        size_t clen = strlen (psz_sdp) + 1;

        if (strcmp (psz_sdp, "application/sdp"))
        {
            msg_Dbg (p_sd, "unsupported content type: %s", psz_sdp);
            goto error;
        }

        // skips content type
        if (len <= clen)
            goto error;

        len -= clen;
        psz_sdp += clen;
    }

    for( int i = 0 ; i < p_sys->i_announces ; i++ )
    {
        sap_announce_t * p_announce = p_sys->pp_announces[i];
        /* FIXME: slow */
        if (p_announce->i_hash == i_hash
         && memcmp(p_announce->i_source, i_source, sizeof (i_source)) == 0)
        {
            /* We don't support delete announcement as they can easily
             * Be used to highjack an announcement by a third party.
             * Instead we cleverly implement Implicit Announcement removal.
             *
             * if( b_need_delete )
             *    RemoveAnnounce( p_sd, p_sys->pp_announces[i]);
             * else
             */

            if( !b_need_delete )
            {
                /* No need to go after six, as we start to trust the
                 * average period at six */
                if( p_announce->i_period_trust <= 5 )
                    p_announce->i_period_trust++;

                /* Compute the average period */
                vlc_tick_t now = vlc_tick_now();
                p_announce->i_period = ( p_announce->i_period * (p_announce->i_period_trust-1) + (now - p_announce->i_last) ) / p_announce->i_period_trust;
                p_announce->i_last = now;
            }
            free (decomp);
            return VLC_SUCCESS;
        }
    }

    sap_announce_t *sap = CreateAnnounce(p_sd, i_source, i_hash, psz_sdp);
    if (sap != NULL)
        TAB_APPEND(p_sys->i_announces, p_sys->pp_announces, sap);

    free (decomp);
    return VLC_SUCCESS;
error:
    free (decomp);
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Run: main SAP thread
 *****************************************************************************
 * Listens to SAP packets, and sends them to packet_handle
 *****************************************************************************/
#define MAX_SAP_BUFFER 5000

static void *Run( void *data )
{
    services_discovery_t *p_sd = data;
    services_discovery_sys_t *p_sys = p_sd->p_sys;
    char *psz_addr;
    int timeout = -1;
    int canc = vlc_savecancel ();

    /* Braindead Winsock DNS resolver will get stuck over 2 seconds per failed
     * DNS queries, even if the DNS server returns an error with milliseconds.
     * You don't want to know why the bug (as of XP SP2) wasn't fixed since
     * Winsock 1.1 from Windows 95, if not Windows 3.1.
     * Anyway, to avoid a 30 seconds delay for failed IPv6 socket creation,
     * we have to open sockets in Run() rather than Open(). */
    InitSocket( p_sd, SAP_V4_GLOBAL_ADDRESS, SAP_PORT );
    InitSocket( p_sd, SAP_V4_ORG_ADDRESS, SAP_PORT );
    InitSocket( p_sd, SAP_V4_LOCAL_ADDRESS, SAP_PORT );
    InitSocket( p_sd, SAP_V4_LINK_ADDRESS, SAP_PORT );

    char psz_address[NI_MAXNUMERICHOST] = "ff02::2:7ffe%";
#ifndef _WIN32
    struct if_nameindex *l = if_nameindex ();
    if (l != NULL)
    {
        char *ptr = strchr (psz_address, '%') + 1;
        for (unsigned i = 0; l[i].if_index; i++)
        {
            strcpy (ptr, l[i].if_name);
            InitSocket (p_sd, psz_address, SAP_PORT);
        }
        if_freenameindex (l);
    }
#else
        /* this is the Winsock2 equivalant of SIOCGIFCONF on BSD stacks,
           which if_nameindex uses internally anyway */

        // first create a dummy socket to pin down the protocol family
        SOCKET s = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        if( s != INVALID_SOCKET )
        {
            INTERFACE_INFO ifaces[10]; // Assume there will be no more than 10 IP interfaces
            DWORD len = sizeof(ifaces);

            if( SOCKET_ERROR != WSAIoctl(s, SIO_GET_INTERFACE_LIST, NULL, 0, &ifaces, len, &len, NULL, NULL) )
            {
                unsigned ifcount = len/sizeof(INTERFACE_INFO);
                char *ptr = strchr (psz_address, '%') + 1;
                for(unsigned i = 1; i<=ifcount; ++i )
                {
                    // append link-local zone identifier
                    sprintf(ptr, "%d", i);
                }
            }
            closesocket(s);
        }
#endif
    *strchr (psz_address, '%') = '\0';

    static const char ipv6_scopes[] = "1456789ABCDE";
    for (const char *c_scope = ipv6_scopes; *c_scope; c_scope++)
    {
        psz_address[3] = *c_scope;
        InitSocket( p_sd, psz_address, SAP_PORT );
    }

    psz_addr = var_CreateGetString( p_sd, "sap-addr" );
    if( psz_addr && *psz_addr )
        InitSocket( p_sd, psz_addr, SAP_PORT );
    free( psz_addr );

    if( p_sys->i_fd == 0 )
    {
        msg_Err( p_sd, "unable to listen on any address" );
        return NULL;
    }

    /* read SAP packets */
    for (;;)
    {
        vlc_restorecancel (canc);
        unsigned n = p_sys->i_fd;
        struct pollfd ufd[n];

        for (unsigned i = 0; i < n; i++)
        {
            ufd[i].fd = p_sys->pi_fd[i];
            ufd[i].events = POLLIN;
            ufd[i].revents = 0;
        }

        int val = poll (ufd, n, timeout);
        canc = vlc_savecancel ();
        if (val > 0)
        {
            for (unsigned i = 0; i < n; i++)
            {
                if (ufd[i].revents)
                {
                    uint8_t p_buffer[MAX_SAP_BUFFER+1];
                    ssize_t i_read;

                    i_read = recv (ufd[i].fd, p_buffer, MAX_SAP_BUFFER, 0);
                    if (i_read < 0)
                        msg_Warn (p_sd, "receive error: %s",
                                  vlc_strerror_c(errno));
                    if (i_read > 6)
                    {
                        /* Parse the packet */
                        p_buffer[i_read] = '\0';
                        ParseSAP (p_sd, p_buffer, i_read);
                    }
                }
            }
        }

        vlc_tick_t now = vlc_tick_now();

        /* A 1 hour timeout correspond to the RFC Implicit timeout.
         * This timeout is tuned in the following loop. */
        timeout = 1000 * 60 * 60;

        /* Check for items that need deletion */
        for( int i = 0; i < p_sys->i_announces; i++ )
        {
            sap_announce_t * p_announce = p_sys->pp_announces[i];
            vlc_tick_t i_last_period = now - p_announce->i_last;

            /* Remove the announcement, if the last announcement was 1 hour ago
             * or if the last packet emitted was 10 times the average time
             * between two packets */
            if( ( p_announce->i_period_trust > 5 && i_last_period > 10 * p_announce->i_period ) ||
                i_last_period > p_sys->i_timeout )
            {
                RemoveAnnounce( p_sd, p_announce );
            }
            else
            {
                /* Compute next timeout */
                if( p_announce->i_period_trust > 5 )
                    timeout = __MIN(MS_FROM_VLC_TICK(10 * p_announce->i_period - i_last_period), timeout);
                timeout = __MIN(MS_FROM_VLC_TICK(p_sys->i_timeout - i_last_period), timeout);
            }
        }

        if( !p_sys->i_announces )
            timeout = -1; /* We can safely poll indefinitely. */
        else if( timeout < 200 )
            timeout = 200; /* Don't wakeup too fast. */
    }
    vlc_assert_unreachable ();
}

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t *p_sys  = (services_discovery_sys_t *)
                                malloc( sizeof( services_discovery_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->i_timeout = vlc_tick_from_sec(var_CreateGetInteger( p_sd, "sap-timeout" ));

    p_sd->p_sys  = p_sys;
    p_sd->description = _("Network streams (SAP)");

    p_sys->pi_fd = NULL;
    p_sys->i_fd = 0;

    p_sys->i_announces = 0;
    p_sys->pp_announces = NULL;
    /* TODO: create sockets here, and fix racy sockets table */
    if (vlc_clone (&p_sys->thread, Run, p_sd, VLC_THREAD_PRIORITY_LOW))
    {
        free (p_sys);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t* )p_this;
    services_discovery_sys_t    *p_sys  = p_sd->p_sys;
    int i;

    vlc_cancel (p_sys->thread);
    vlc_join (p_sys->thread, NULL);

    for( i = p_sys->i_fd-1 ; i >= 0 ; i-- )
    {
        net_Close( p_sys->pi_fd[i] );
    }
    FREENULL( p_sys->pi_fd );

    for( i = p_sys->i_announces  - 1;  i>= 0; i-- )
    {
        RemoveAnnounce( p_sd, p_sys->pp_announces[i] );
    }
    FREENULL( p_sys->pp_announces );

    free( p_sys );
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define SAP_ADDR_TEXT N_( "SAP multicast address" )
#define SAP_ADDR_LONGTEXT N_( "The SAP module normally chooses itself the " \
                              "right addresses to listen to. However, you " \
                              "can specify a specific address." )
#define SAP_TIMEOUT_TEXT N_( "SAP timeout (seconds)" )
#define SAP_TIMEOUT_LONGTEXT N_( \
       "Delay after which SAP items get deleted if no new announcement " \
       "is received." )
#define SAP_PARSE_TEXT N_( "Try to parse the announce" )
#define SAP_PARSE_LONGTEXT N_( \
       "This enables actual parsing of the announces by the SAP module. " \
       "Otherwise, all announcements are parsed by the \"live555\" " \
       "(RTP/RTSP) module." )

VLC_SD_PROBE_HELPER("sap", N_("Network streams (SAP)"), SD_CAT_LAN)

vlc_module_begin()
    set_shortname(N_("SAP"))
    set_description(N_("Network streams (SAP)") )
    set_category(CAT_PLAYLIST)
    set_subcategory(SUBCAT_PLAYLIST_SD)

    add_string("sap-addr", NULL, SAP_ADDR_TEXT, SAP_ADDR_LONGTEXT, true)
    add_obsolete_bool("sap-ipv4") /* since 2.0.0 */
    add_obsolete_bool("sap-ipv6") /* since 2.0.0 */
    add_integer("sap-timeout", 1800,
                SAP_TIMEOUT_TEXT, SAP_TIMEOUT_LONGTEXT, true)
    add_obsolete_bool("sap-parse") /* since 4.0.0 */
    add_obsolete_bool("sap-strict") /* since 4.0.0 */
    add_obsolete_bool("sap-timeshift") /* Redundant since 1.0.0 */

    set_capability("services_discovery", 0)
    set_callbacks(Open, Close)

    VLC_SD_PROBE_SUBMODULE
vlc_module_end()
