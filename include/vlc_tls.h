/*****************************************************************************
 * vlc_tls.h: Transport Layer Security API
 *****************************************************************************
 * Copyright (C) 2004-2016 Rémi Denis-Courmont
 * Copyright (C) 2005-2006 VLC authors and VideoLAN
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

#ifndef VLC_TLS_H
# define VLC_TLS_H

/**
 * \ingroup sockets
 * \defgroup tls Transport Layer Security
 * @{
 * \file
 * Transport Layer Security (TLS) functions
 */

# include <vlc_network.h>

typedef struct vlc_tls vlc_tls_t;
typedef struct vlc_tls_creds vlc_tls_creds_t;

/** TLS session */
struct vlc_tls
{
    vlc_object_t *obj;
    void *sys;

    int (*get_fd)(struct vlc_tls *);
    ssize_t (*readv)(struct vlc_tls *, struct iovec *, unsigned);
    ssize_t (*writev)(struct vlc_tls *, const struct iovec *, unsigned);
    int (*shutdown)(struct vlc_tls *, bool duplex);
    void (*close)(struct vlc_tls *);

    void *p;
};

/**
 * Initiates a client TLS session.
 *
 * Initiates a Transport Layer Security (TLS) session as the client side, using
 * trusted root CAs previously loaded with vlc_tls_ClientCreate().
 *
 * This is a blocking network operation and may be a thread cancellation point.
 *
 * @param creds X.509 credentials, i.e. set of root certificates of trusted
 *              certificate authorities
 * @param sock socket through which to establish the secure channel
 * @param hostname expected server name, used both as Server Name Indication
 *                 and as expected Common Name of the peer certificate [IN]
 * @param service unique identifier for the service to connect to
 *                (only used locally for certificates database) [IN]
 * @param alpn NULL-terminated list of Application Layer Protocols
 *             to negotiate, or NULL to not negotiate protocols [IN]
 * @param alp storage space for the negotiated Application Layer
 *            Protocol or NULL if negotiation was not performed [OUT]
 *
 * @note The credentials must remain valid until the session is finished.
 *
 * @return TLS session, or NULL on error.
 **/
VLC_API vlc_tls_t *vlc_tls_ClientSessionCreate(vlc_tls_creds_t *creds,
                                               vlc_tls_t *sock,
                                               const char *host,
                                               const char *service,
                                               const char *const *alpn,
                                               char **alp);

/**
 * Creates a TLS server session.
 *
 * Allocates a Transport Layer Security (TLS) session as the server side, using
 * cryptographic keys pair and X.509 certificates chain already loaded with
 * vlc_tls_ServerCreate().
 *
 * Unlike vlc_tls_ClientSessionCreate(), this function does not perform any
 * actual network I/O. vlc_tls_SessionHandshake() must be used to perform the
 * TLS handshake before sending and receiving data through the TLS session.
 *
 * This function is non-blocking and is not a cancellation point.
 *
 * @param creds server credentials, i.e. keys pair and X.509 certificates chain
 * @param alpn NULL-terminated list of Application Layer Protocols
 *             to negotiate, or NULL to not negotiate protocols
 *
 * @return TLS session, or NULL on error.
 */
VLC_API vlc_tls_t *vlc_tls_ServerSessionCreate(vlc_tls_creds_t *creds, int fd,
                                               const char *const *alpn);

/**
 * Destroys a TLS session down.
 *
 * All resources associated with the TLS session are released.
 *
 * If the session was established successfully, then shutdown cleanly, the
 * underlying socket can be reused. Otherwise, it must be closed. Either way,
 * this function does not close the underlying socket: Use vlc_tls_Close()
 * instead to close it at the same.
 *
 * This function is non-blocking and is not a cancellation point.
 */
VLC_API void vlc_tls_SessionDelete (vlc_tls_t *);

static inline int vlc_tls_GetFD(vlc_tls_t *tls)
{
    return tls->get_fd(tls);
}

/**
 * Receives data through a TLS session.
 */
VLC_API ssize_t vlc_tls_Read(vlc_tls_t *, void *buf, size_t len, bool waitall);
VLC_API char *vlc_tls_GetLine(vlc_tls_t *);

/**
 * Sends data through a TLS session.
 */
VLC_API ssize_t vlc_tls_Write(vlc_tls_t *, const void *buf, size_t len);

/**
 * Terminates a TLS session.
 *
 * This sends the TLS session close notification to the other end, securely
 * indicating that no further data will be sent. Data can still be received
 * until a close notification is received from the other end.
 *
 * @param duplex whether to stop receiving data as well
 * @retval 0 the session was terminated securely and cleanly
 *           (the underlying socket can be reused for other purposes)
 * @return -1 the session was terminated locally, but either a notification
 *            could not be sent or received (the underlying socket cannot be
 *            reused and must be closed)
 */
static inline int vlc_tls_Shutdown(vlc_tls_t *tls, bool duplex)
{
    return tls->shutdown(tls, duplex);
}

# define tls_Recv(a,b,c) vlc_tls_Read(a,b,c,false)
# define tls_Send(a,b,c) vlc_tls_Write(a,b,c)

/**
 * Closes a TLS session and underlying connection.
 *
 * This function is non-blocking and is a cancellation point.
 */
static inline void vlc_tls_Close(vlc_tls_t *session)
{
    int fd = vlc_tls_GetFD(session);

    vlc_tls_SessionDelete(session);
    shutdown(fd, SHUT_RDWR);
    net_Close(fd);
}

/** TLS credentials (certificate, private and trust settings) */
struct vlc_tls_creds
{
    VLC_COMMON_MEMBERS

    module_t  *module;
    void *sys;

    int (*open)(vlc_tls_creds_t *, vlc_tls_t *session, vlc_tls_t *sock,
                const char *host, const char *const *alpn);
    int  (*handshake)(vlc_tls_creds_t *, vlc_tls_t *session, const char *host,
                      const char *service, char ** /*restrict*/ alp);
};

/**
 * Allocates TLS credentials for a client.
 * Credentials can be cached and reused across multiple TLS sessions.
 *
 * @return TLS credentials object, or NULL on error.
 **/
VLC_API vlc_tls_creds_t *vlc_tls_ClientCreate (vlc_object_t *);

/**
 * Allocates server TLS credentials.
 *
 * @param cert path to an x509 certificate (required)
 * @param key path to the PKCS private key for the certificate,
 *            or NULL to use cert path
 *
 * @return TLS credentials object, or NULL on error.
 */
VLC_API vlc_tls_creds_t *vlc_tls_ServerCreate (vlc_object_t *,
                                               const char *cert,
                                               const char *key);

static inline int vlc_tls_SessionHandshake (vlc_tls_creds_t *crd,
                                            vlc_tls_t *tls)
{
    return crd->handshake(crd, tls, NULL, NULL, NULL);
}

/**
 * Releases TLS credentials.
 *
 * Releases data allocated with vlc_tls_ClientCreate() or
 * vlc_tls_ServerCreate().
 *
 * @param srv object to be destroyed (or NULL)
 */
VLC_API void vlc_tls_Delete (vlc_tls_creds_t *);

/**
 * Creates a transport-layer stream from a socket.
 *
 * Creates a transport-layer I/O stream from a socket file descriptor.
 * Data will be sent and received directly through the socket. This can be used
 * either to share common code between non-TLS and TLS cases, or for testing
 * purposes.
 *
 * This function is not a cancellation point.
 */
VLC_API vlc_tls_t *vlc_tls_SocketOpen(vlc_object_t *obj, int fd);

VLC_DEPRECATED
static inline vlc_tls_t *
vlc_tls_ClientSessionCreateFD(vlc_tls_creds_t *crd, int fd, const char *host,
                              const char *srv, const char *const *lp, char **p)
{
    vlc_tls_t *sock = vlc_tls_SocketOpen(VLC_OBJECT(crd), fd);
    if (unlikely(sock == NULL))
        return NULL;

    vlc_tls_t *tls = vlc_tls_ClientSessionCreate(crd, sock, host, srv, lp, p);
    if (unlikely(tls == NULL))
        vlc_tls_SessionDelete(sock);
    else
        tls->p = sock;
    return tls;
}

/** @} */

#endif
