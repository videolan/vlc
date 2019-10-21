/*****************************************************************************
 * vlc_tls.h:
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
 * \ingroup net
 * \defgroup transport Transport layer sockets
 * Network stream abstraction
 *
 * Originally intended for the TLS protocol (Transport Layer Security),
 * the Transport Layer Sockets now provides a generic abstraction
 * for connection-oriented full-duplex I/O byte streams, such as TCP/IP sockets
 * and TLS protocol sessions.
 *
 * @{
 * \file
 * Transport layer functions
 */

# include <vlc_network.h>

/**
 * Transport layer socket.
 *
 * Transport layer sockets are full-duplex, meaning data can be sent and
 * received at the same time. As such, it is permitted for two threads to
 * use the same TLS simultaneously, if one thread is receiving data while the
 * other is sending data. However receiving or sending data from two threads
 * concurrently is undefined behaviour.
 *
 * The following functions are treated as sending data:
 * - vlc_tls_Write(),
 * - vlc_tls_Shutdown(),
 * - callback vlc_tls_operations.writev,
 * - callback vlc_tls_operations.shutdown.
 *
 * The following functions are treated as receiving data:
 * - vlc_tls_Read(),
 * - vlc_tls_GetLine(),
 * - callback vlc_tls_operations.readv,
 * - vlc_tls_Shutdown() if the duplex flag is true,
 * - callback vlc_tls_operations.shutdown if the duplex flag is true.
 */
typedef struct vlc_tls
{
    /** Callbacks to operate on the stream. */
    const struct vlc_tls_operations *ops;
    /** Reserved. Pointer to the underlying stream, or NULL if none. */
    struct vlc_tls *p;
} vlc_tls_t;

struct vlc_tls_operations
{
    /** Callback for events polling.
     *
     * See \ref vlc_tls_GetPollFD().
     */
    int (*get_fd)(struct vlc_tls *, short *events);

    /** Callback for receiving data.
     *
     * This callback receives/reads data into an I/O vector
     * in non-blocking mode.
     *
     * @param iov I/O vector to read data into
     * @param len number of entries of the I/O vector
     * @return the number of bytes received or -1 on error
     *
     * If no data is available without blocking, the function returns -1 and
     * sets @c errno to @c EAGAIN .
     */
    ssize_t (*readv)(struct vlc_tls *, struct iovec *iov, unsigned len);

    /** Callback for sending data.
     *
     * This callback sends/writes data from an I/O vector
     * in non-blocking mode.
     *
     * @param iov I/O vector to write data from
     * @param len number of entries of the I/O vector
     * @return the number of bytes sent or -1 on error
     *
     * If no data can be sent without blocking, the function returns -1 and
     * sets @c errno to @c EAGAIN .
     */
    ssize_t (*writev)(struct vlc_tls *, const struct iovec *iov, unsigned len);

    /** Callback for shutting down.
     *
     * This callback marks the end of the output (send/write) half of the
     * stream. If the duplex flag is set, it also marks the end of the input
     * (receive/read) half. See also \ref vlc_tls_Shutdown().
     */
    int (*shutdown)(struct vlc_tls *, bool duplex);

    /** Callback for closing.
     *
     * This callback terminates the stream and releases any associated
     * resources. However, it does <b>not</b> destroy the underlying stream
     * if there is one. See also \ref vlc_tls_SessionDelete().
     */
    void (*close)(struct vlc_tls *);
};

/**
 * \defgroup tls Transport Layer Security
 * @{
 * \defgroup tls_client TLS client
 * @{
 */

/**
 * TLS client-side credentials
 *
 * This structure contains the credentials for establishing TLS sessions
 * on client side, essentially the set of trusted root Certificate Authorities
 * with which to validate certificate chains presented by servers.
 */
typedef struct vlc_tls_client
{
    struct vlc_object_t obj;
    const struct vlc_tls_client_operations *ops;
    void *sys;
} vlc_tls_client_t;

struct vlc_tls_client_operations
{
    vlc_tls_t *(*open)(struct vlc_tls_client *, vlc_tls_t *sock,
                       const char *host, const char *const *alpn);
    int  (*handshake)(vlc_tls_t *session,
                      const char *hostname, const char *service,
                      char ** /*restrict*/ alp);
    void (*destroy)(struct vlc_tls_client *);
};

/**
 * Allocates TLS client-side credentials.
 *
 * Credentials can be cached and reused across multiple TLS sessions.
 *
 * @return TLS credentials object, or NULL on error.
 **/
VLC_API vlc_tls_client_t *vlc_tls_ClientCreate(vlc_object_t *);

/**
 * Releases TLS client-side credentials.
 *
 * Releases data allocated with vlc_tls_ClientCreate().
 */
VLC_API void vlc_tls_ClientDelete(vlc_tls_client_t *);

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
VLC_API vlc_tls_t *vlc_tls_ClientSessionCreate(vlc_tls_client_t *creds,
                                               vlc_tls_t *sock,
                                               const char *host,
                                               const char *service,
                                               const char *const *alpn,
                                               char **alp);

/**
 * @}
 * \defgroup tls_server TLS server
 * @{
 */

/**
 * TLS server-side credentials
 *
 * This structure contains the credentials for establishing TLS sessions.
 * This includes root Certificate Authorities (on client side),
 * trust and cryptographic parameters,
 * public certificates and private keys.
 */
typedef struct vlc_tls_server
{
    struct vlc_object_t obj;
    const struct vlc_tls_server_operations *ops;
    void *sys;

} vlc_tls_server_t;

struct vlc_tls_server_operations
{
    vlc_tls_t *(*open)(struct vlc_tls_server *, vlc_tls_t *sock,
                       const char *const *alpn);
    int  (*handshake)(vlc_tls_t *session, char ** /*restrict*/ alp);
    void (*destroy)(struct vlc_tls_server *);
};

/**
 * Allocates server TLS credentials.
 *
 * @param cert path to an x509 certificate (required)
 * @param key path to the PKCS private key for the certificate,
 *            or NULL to use cert path
 *
 * @return TLS credentials object, or NULL on error.
 */
VLC_API vlc_tls_server_t *vlc_tls_ServerCreate(vlc_object_t *,
                                               const char *cert,
                                               const char *key);

static inline int vlc_tls_SessionHandshake(vlc_tls_server_t *crd,
                                           vlc_tls_t *tls)
{
    return crd->ops->handshake(tls, NULL);
}

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
VLC_API vlc_tls_t *vlc_tls_ServerSessionCreate(vlc_tls_server_t *creds,
                                               vlc_tls_t *sock,
                                               const char *const *alpn);

/**
 * Releases server-side TLS credentials.
 *
 * Releases data allocated with vlc_tls_ServerCreate().
 */
VLC_API void vlc_tls_ServerDelete(vlc_tls_server_t *);

/** @} */

/** @} */

/**
 * Destroys a TLS session.
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

/**
 * Generates an event polling description.
 *
 * This function provides the necessary informations to make an event polling
 * description for use with poll() or similar event multiplexing functions.
 *
 * This function is necessary both for receiving and sending data, therefore
 * it is reentrant. It is not a cancellation point.
 *
 * @param events a pointer to a mask of poll events (e.g. POLLIN, POLLOUT)
 *               [IN/OUT]
 * @return the file descriptor to poll
 */
static inline int vlc_tls_GetPollFD(vlc_tls_t *tls, short *events)
{
    return tls->ops->get_fd(tls, events);
}

/**
 * Returns the underlying file descriptor.
 *
 * This function returns the file descriptor underlying the transport layer
 * stream object. This function is reentrant and is not a cancellation point.
 */
static inline int vlc_tls_GetFD(vlc_tls_t *tls)
{
    short events = 0;

    return vlc_tls_GetPollFD(tls, &events);
}

/**
 * Receives data through a socket.
 *
 * This dequeues incoming data from a transport layer socket.
 *
 * @param buf received buffer start address [OUT]
 * @param len buffer length (in bytes)
 * @param waitall whether to wait for the exact buffer length (true),
 *                or for any amount of data (false)
 *
 * @note At end of stream, the number of bytes returned may be shorter than
 * requested regardless of the "waitall" flag.
 *
 * @return the number of bytes actually dequeued, or -1 on error.
 */
VLC_API ssize_t vlc_tls_Read(vlc_tls_t *, void *buf, size_t len, bool waitall);

/**
 * Receives a text line through a socket.
 *
 * This dequeues one line of text from a transport layer socket.
 * @return a heap-allocated nul-terminated string, or NULL on error
 */
VLC_API char *vlc_tls_GetLine(vlc_tls_t *);

/**
 * Sends data through a socket.
 */
VLC_API ssize_t vlc_tls_Write(vlc_tls_t *, const void *buf, size_t len);

/**
 * Shuts a connection down.
 *
 * This sends the connection close notification.
 *
 * If the TLS protocol is used, this provides a secure indication to the other
 * end that no further data will be sent. If using plain TCP/IP, this sets the
 * FIN flag.
 *
 * Data can still be received until a close notification is received from the
 * other end.
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
    return tls->ops->shutdown(tls, duplex);
}

/**
 * Closes a connection and its underlying resources.
 *
 * This function closes the transport layer socket, and terminates any
 * underlying connection. For instance, if the TLS protocol is used over a TCP
 * stream, this function terminates both the TLS session, and then underlying
 * TCP/IP connection.
 *
 * To close a connection but retain any underlying resources, use
 * vlc_tls_SessionDelete() instead.
 */
static inline void vlc_tls_Close(vlc_tls_t *session)
{
    do
    {
        vlc_tls_t *p = session->p;

        vlc_tls_SessionDelete(session);
        session = p;
    }
    while (session != NULL);
}

/**
 * Creates a transport-layer stream from a socket.
 *
 * Creates a transport-layer I/O stream from a socket file descriptor.
 * Data will be sent and received directly through the socket. This can be used
 * either to share common code between non-TLS and TLS cases, or for testing
 * purposes.
 *
 * This function is not a cancellation point.
 *
 * @deprecated This function is transitional. Do not use it directly.
 */
VLC_API vlc_tls_t *vlc_tls_SocketOpen(int fd);

/**
 * Creates a connected pair of transport-layer sockets.
 */
VLC_API int vlc_tls_SocketPair(int family, int protocol, vlc_tls_t *[2]);

struct addrinfo;

/**
 * Creates a transport-layer stream from a struct addrinfo.
 *
 * This function tries to allocate a socket using the specified addrinfo
 * structure. Normally, the vlc_tls_SocketOpenTCP() function takes care of
 * this. But in some cases, it cannot be used, notably:
 * - if the remote destination is not resolved (directly) from getaddrinfo(),
 * - if the socket type is not SOCK_STREAM,
 * - if the transport protocol is not TCP (IPPROTO_TCP), or
 * - if TCP Fast Open should be attempted.
 *
 * @note If the @c defer_connect flag is @c true , data must be sent with a
 * data sending function (other than vlc_tls_Shutdown()) before data can be
 * received.
 * Notwithstanding the thread-safety and reentrancy promises of \ref vlc_tls_t,
 * the owner of the stream object is responsible for ensuring that data will be
 * sent at least once before any attempt to receive data.
 * Otherwise @c defer_connect must be @c false .
 *
 * @param ai a filled addrinfo structure (the ai_next member is ignored)
 * @param defer_connect whether to attempt a TCP Fast Open connection or not
 */
VLC_API vlc_tls_t *vlc_tls_SocketOpenAddrInfo(const struct addrinfo *ai,
                                              bool defer_connect);

/**
 * Creates a transport-layer TCP stream from a name and port.
 *
 * This function resolves a hostname, and attempts to establish a TCP/IP
 * connection to the specified host and port number.
 *
 * @note The function currently iterates through the addrinfo linked list.
 * Future versions may implement different behaviour (e.g. RFC6555).
 *
 * @return a transport layer socket on success or NULL on error
 */
VLC_API vlc_tls_t *vlc_tls_SocketOpenTCP(vlc_object_t *obj,
                                         const char *hostname, unsigned port);

/**
 * Initiates a TLS session over TCP.
 *
 * This function resolves a hostname, attempts to establish a TCP/IP
 * connection to the specified host and port number, and finally attempts to
 * establish a TLS session over the TCP/IP stream.
 *
 * See also vlc_tls_SocketOpenTCP() and vlc_tls_ClientSessionCreate().
 */
VLC_API vlc_tls_t *vlc_tls_SocketOpenTLS(vlc_tls_client_t *crd,
                                         const char *hostname, unsigned port,
                                         const char *service,
                                         const char *const *alpn, char **alp);

/** @} */

#endif
