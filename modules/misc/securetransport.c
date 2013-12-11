/*****************************************************************************
 * securetransport.c
 *****************************************************************************
 * Copyright (C) 2013 David Fuhrmann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Öesser General Public License
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
#include <vlc_plugin.h>
#include <vlc_tls.h>
#include <vlc_dialog.h>

#include <Security/Security.h>
#include <Security/SecureTransport.h>
#include <TargetConditionals.h>

/* From MacErrors.h (cannot be included because it isn't present in iOS: */
#ifndef ioErr
# define ioErr -36
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenClient  (vlc_tls_creds_t *);
static void CloseClient (vlc_tls_creds_t *);

vlc_module_begin ()
    set_description(N_("TLS support for OS X and iOS"))
    set_capability("tls client", 2)
    set_callbacks(OpenClient, CloseClient)
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_NETWORK)
vlc_module_end ()


#define cfKeyHost CFSTR("host")
#define cfKeyCertificate CFSTR("certificate")

struct vlc_tls_creds_sys
{
    CFMutableArrayRef whitelist;
};

struct vlc_tls_sys {
    SSLContextRef p_context;
    vlc_tls_creds_sys_t *p_cred;
    size_t i_send_buffered_bytes;
    int i_fd;

    bool b_blocking_send;
    bool b_handshaked;
};

static int st_Error (vlc_tls_t *obj, int val)
{
    switch (val)
    {
        /* peer performed shutdown */
        case errSSLClosedNoNotify:
        case errSSLClosedGraceful:
            msg_Dbg(obj, "Got shutdown notification");
            return 0;

        case errSSLWouldBlock:
            errno = EAGAIN;
            break;

        default:
            msg_Err (obj, "Found error %d", val);
            errno = ECONNRESET;
    }
    return -1;
}

/*
 * Read function called by secure transport for socket read.
 *
 * Function is based on Apples SSLSample sample code.
 */
static OSStatus st_SocketReadFunc (SSLConnectionRef connection,
                                   void *data,
                                   size_t *dataLength) {

    vlc_tls_t *session = (vlc_tls_t *)connection;
    vlc_tls_sys_t *sys = session->sys;

    size_t bytesToGo = *dataLength;
    size_t initLen = bytesToGo;
    UInt8 *currData = (UInt8 *)data;
    OSStatus retValue = noErr;
    ssize_t val;

    for(;;) {
        val = read(sys->i_fd, currData, bytesToGo);
        if (val <= 0) {
            if(val == 0) {
                msg_Dbg(session, "found eof");
                retValue = errSSLClosedGraceful;
            } else { /* do the switch */
                switch(errno) {
                    case ENOENT:
                        /* connection closed */
                        retValue = errSSLClosedGraceful;
                        break;
                    case ECONNRESET:
                        retValue = errSSLClosedAbort;
                        break;
                    case EAGAIN:
                        retValue = errSSLWouldBlock;
                        sys->b_blocking_send = false;
                        break;
                    default:
                        msg_Err(session, "try to read %d bytes, got error %d",
                                (int)bytesToGo, errno);
                        retValue = ioErr;
                        break;
                }
            }
            break;
        } else {
            bytesToGo -= val;
            currData += val;
        }

        if(bytesToGo == 0) {
            /* filled buffer with incoming data, done */
            break;
        }
    }
    *dataLength = initLen - bytesToGo;

    return retValue;
}

/*
 * Write function called by secure transport for socket read.
 *
 * Function is based on Apples SSLSample sample code.
 */
static OSStatus st_SocketWriteFunc (SSLConnectionRef connection,
                                    const void *data,
                                    size_t *dataLength) {

    vlc_tls_t *session = (vlc_tls_t *)connection;
    vlc_tls_sys_t *sys = session->sys;

    size_t bytesSent = 0;
    size_t dataLen = *dataLength;
    OSStatus retValue = noErr;
    ssize_t val;

    do {
        val = write(sys->i_fd, (char *)data + bytesSent, dataLen - bytesSent);
    } while (val >= 0 && (bytesSent += val) < dataLen);

    if(val < 0) {
        if(errno == EAGAIN) {
            retValue = errSSLWouldBlock;
            sys->b_blocking_send = true;
        } else {
            msg_Err(session, "error while writing: %d", errno);
            retValue = ioErr;
        }
    }

    *dataLength = bytesSent;
    return retValue;
}

static int st_validateServerCertificate (vlc_tls_t *session, const char *hostname) {

    int result = -1;
    vlc_tls_sys_t *sys = session->sys;
    SecCertificateRef leaf_cert = NULL;

    SecTrustRef trust = NULL;
    OSStatus ret = SSLCopyPeerTrust (sys->p_context, &trust);
    if (ret != noErr || trust == NULL) {
        msg_Err(session, "error getting certifictate chain");
        return -1;
    }

    CFStringRef cfHostname = CFStringCreateWithCString(kCFAllocatorDefault,
                                                       hostname,
                                                       kCFStringEncodingUTF8);


    /* enable default root / anchor certificates */
    ret = SecTrustSetAnchorCertificates (trust, NULL);
    if (ret != noErr) {
        msg_Err(session, "error setting anchor certificates");
        result = -1;
        goto out;
    }

    SecTrustResultType trust_eval_result = 0;

    ret = SecTrustEvaluate(trust, &trust_eval_result);
    if(ret != noErr) {
        msg_Err(session, "error calling SecTrustEvaluate");
        result = -1;
        goto out;
    }

    switch (trust_eval_result) {
        case kSecTrustResultUnspecified:
        case kSecTrustResultProceed:
            msg_Dbg(session, "cerfificate verification successful, result is %d", trust_eval_result);
            result = 0;
            goto out;

        case kSecTrustResultRecoverableTrustFailure:
        case kSecTrustResultDeny:
        default:
            msg_Warn(session, "cerfificate verification failed, result is %d", trust_eval_result);
    }

    /* get leaf certificate */
    /* SSLCopyPeerCertificates is only available on OSX 10.5 or later */
#if !TARGET_OS_IPHONE
    CFArrayRef cert_chain = NULL;
    ret = SSLCopyPeerCertificates (sys->p_context, &cert_chain);
    if (ret != noErr || !cert_chain) {
        result = -1;
        goto out;
    }

    if (CFArrayGetCount (cert_chain) == 0) {
        CFRelease (cert_chain);
        result = -1;
        goto out;
    }

    leaf_cert = (SecCertificateRef)CFArrayGetValueAtIndex (cert_chain, 0);
    CFRetain (leaf_cert);
    CFRelease (cert_chain);
#else
    /* SecTrustGetCertificateAtIndex is only available on 10.7 or iOS */
    if (SecTrustGetCertificateCount (trust) == 0) {
        result = -1;
        goto out;
    }

    leaf_cert = SecTrustGetCertificateAtIndex (trust, 0);
    CFRetain (leaf_cert);
#endif


    /* check if leaf already accepted */
    CFIndex max = CFArrayGetCount (sys->p_cred->whitelist);
    for (CFIndex i = 0; i < max; ++i) {
        CFDictionaryRef dict = CFArrayGetValueAtIndex (sys->p_cred->whitelist, i);
        CFStringRef knownHost = (CFStringRef)CFDictionaryGetValue (dict, cfKeyHost);
        SecCertificateRef knownCert = (SecCertificateRef)CFDictionaryGetValue (dict, cfKeyCertificate);

        if (!knownHost || !knownCert)
            continue;

        if (CFEqual (knownHost, cfHostname) && CFEqual (knownCert, leaf_cert)) {
            msg_Warn(session, "certificate already accepted, continuing");
            result = 0;
            goto out;
        }
    }

    /* We do not show more certificate details yet because there is no proper API to get
       a summary of the certificate. SecCertificateCopySubjectSummary is the only method
       available on iOS and 10.6. More promising API functions such as
       SecCertificateCopyLongDescription also print out the subject only, more or less.
       But only showing the certificate subject is of no real help for the user.
       We could use SecCertificateCopyValues, but then we need to parse all OID values for
       ourself. This is too mad for just printing information the user will never check
       anyway.
     */

    const char *msg = N_("You attempted to reach %s. "
             "However the security certificate presented by the server "
             "is unknown and could not be authenticated by any trusted "
             "Certification Authority. "
             "This problem may be caused by a configuration error "
             "or an attempt to breach your security or your privacy.\n\n"
             "If in doubt, abort now.\n");
    int answer = dialog_Question (session, _("Insecure site"), vlc_gettext (msg),
                                  _("Abort"), _("Accept certificate temporarily"), NULL, hostname);

    if(answer == 2) {
        msg_Warn(session, "Proceeding despite of failed certificate validation");

        /* save leaf certificate in whitelist */
        const void *keys[] = {cfKeyHost, cfKeyCertificate};
        const void *values[] = {cfHostname, leaf_cert};
        CFDictionaryRef dict = CFDictionaryCreate (kCFAllocatorDefault,
                                                   keys, values, 2,
                                                   &kCFTypeDictionaryKeyCallBacks,
                                                   &kCFTypeDictionaryValueCallBacks);
        if(!dict) {
            msg_Err (session, "error creating dict");
            result = -1;
            goto out;
        }

        CFArrayAppendValue (sys->p_cred->whitelist, dict);
        CFRelease (dict);

        result = 0;
        goto out;

    } else {
        result = -1;
        goto out;
    }

out:
    CFRelease (trust);

    if (cfHostname)
        CFRelease (cfHostname);
    if (leaf_cert)
        CFRelease (leaf_cert);

    return result;
}

/*
 * @return -1 on fatal error, 0 on successful handshake completion,
 * 1 if more would-be blocking recv is needed,
 * 2 if more would-be blocking send is required.
 */
static int st_Handshake (vlc_tls_t *session, const char *host,
                                        const char *service) {
    VLC_UNUSED(service);

    vlc_tls_sys_t *sys = session->sys;

    OSStatus retValue = SSLHandshake(sys->p_context);

    if (retValue == errSSLWouldBlock) {
        msg_Dbg(session, "handshake is blocked, try again later");
        return 1 + (sys->b_blocking_send ? 1 : 0);
    }

    switch (retValue) {
        case noErr:
            if(st_validateServerCertificate(session, host) != 0) {
                return -1;
            }
            msg_Dbg(session, "handshake completed successfully");
            sys->b_handshaked = true;
            return 0;

        case errSSLServerAuthCompleted:
            return st_Handshake (session, host, service);

        case errSSLConnectionRefused:
            msg_Err(session, "connection was refused");
            return -1;
        case errSSLNegotiation:
            msg_Err(session, "cipher suite negotiation failed");
            return -1;
        case errSSLFatalAlert:
            msg_Err(session, "fatal error occured during handshake");
            return -1;

        default:
            msg_Err(session, "handshake returned error %d", (int)retValue);
            return -1;
    }
}

/**
 * Sends data through a TLS session.
 */
static int st_Send (void *opaque, const void *buf, size_t length)
{
    vlc_tls_t *session = opaque;
    vlc_tls_sys_t *sys = session->sys;
    OSStatus ret = noErr;

    /*
     * SSLWrite does not return the number of bytes actually written to
     * the socket, but the number of bytes written to the internal cache.
     *
     * If return value is errSSLWouldBlock, the underlying socket cannot
     * send all data, but the data is already cached. In this situation,
     * we need to call SSLWrite again. To ensure this call even for the
     * last bytes, we return EAGAIN. On the next call, we give no new data
     * to SSLWrite until the error is not errSSLWouldBlock anymore.
     *
     * This code is adapted the same way as done in curl.
     * (https://github.com/bagder/curl/blob/master/lib/curl_darwinssl.c#L2067)
     */

    size_t actualSize;
    if (sys->i_send_buffered_bytes > 0) {
        ret = SSLWrite(sys->p_context, NULL, 0, &actualSize);

        if (ret == noErr) {
            /* actualSize remains zero because no new data send */
            actualSize = sys->i_send_buffered_bytes;
            sys->i_send_buffered_bytes = 0;

        } else if (ret == errSSLWouldBlock) {
            /* EAGAIN is not expected by the core in this situation,
             so use EINTR here */
            errno = EINTR;
            return -1;
        }

    } else {
        ret = SSLWrite(sys->p_context, buf, length, &actualSize);

        if (ret == errSSLWouldBlock) {
            sys->i_send_buffered_bytes = length;
            /* EAGAIN is not expected by the core in this situation,
               so use EINTR here */
            errno = EINTR;
            return -1;
        }
    }

    return ret != noErr ? st_Error(session, ret) : actualSize;
}

/**
 * Receives data through a TLS session.
 */
static int st_Recv (void *opaque, void *buf, size_t length)
{
    vlc_tls_t *session = opaque;
    vlc_tls_sys_t *sys = session->sys;

    size_t actualSize;
    OSStatus ret = SSLRead(sys->p_context, buf, length, &actualSize);

    if(ret == errSSLWouldBlock && actualSize)
        return actualSize;

    return ret != noErr ? st_Error(session, ret) : actualSize;
}

/**
 * Closes a client-side TLS credentials.
 */
static void st_ClientSessionClose (vlc_tls_creds_t *crd, vlc_tls_t *session) {

    VLC_UNUSED(crd);

    vlc_tls_sys_t *sys = session->sys;
    msg_Dbg(session, "close TLS session");

    if(sys->b_handshaked) {
        OSStatus ret = SSLClose(sys->p_context);
        if(ret != noErr) {
            msg_Err(session, "error closing ssl context");
        }
    }

    if (sys->p_context) {
#if TARGET_OS_IPHONE
        CFRelease(sys->p_context);
#else
        if(SSLDisposeContext(sys->p_context) != noErr) {
            msg_Err(session, "error deleting context");
        }
#endif
    }
    free (sys);
}

/**
 * Initializes a client-side TLS session.
 */
static int st_ClientSessionOpen (vlc_tls_creds_t *crd, vlc_tls_t *session,
                                     int fd, const char *hostname) {
    msg_Dbg(session, "open TLS session for %s", hostname);

    vlc_tls_sys_t *sys = malloc (sizeof (*session->sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->p_cred = crd->sys;
    sys->i_fd = fd;
    sys->b_handshaked = false;
    sys->b_blocking_send = false;
    sys->i_send_buffered_bytes = 0;

    session->sys = sys;
    session->sock.p_sys = session;
    session->sock.pf_send = st_Send;
    session->sock.pf_recv = st_Recv;
    session->handshake = st_Handshake;

    SSLContextRef p_context = NULL;
#if TARGET_OS_IPHONE
    p_context = SSLCreateContext (NULL, kSSLClientSide, kSSLStreamType);
    if(p_context == NULL) {
        msg_Err(session, "cannot create ssl context");
        goto error;
    }
#else
    if (SSLNewContext (false, &p_context) != noErr) {
        msg_Err(session, "error calling SSLNewContext");
        goto error;
    }
#endif

    sys->p_context = p_context;

    OSStatus ret = SSLSetIOFuncs (p_context, st_SocketReadFunc, st_SocketWriteFunc);
    if(ret != noErr) {
        msg_Err(session, "cannot set io functions");
        goto error;
    }

    ret = SSLSetConnection (p_context, session);
    if(ret != noErr) {
        msg_Err(session, "cannot set connection");
        goto error;
    }

    ret = SSLSetPeerDomainName (p_context, hostname, strlen(hostname));
    if(ret != noErr) {
        msg_Err(session, "cannot set peer domain name");
        goto error;
    }

    /* disable automatic validation. We do so manually to also handle invalid
       certificates */

    /* this has effect only on iOS 5 and OSX 10.8 or later ... */
    SSLSetSessionOption (sys->p_context, kSSLSessionOptionBreakOnServerAuth, true);
#if !TARGET_OS_IPHONE
    /* ... thus calling this for earlier osx versions, which is not available on iOS in turn */
    SSLSetEnableCertVerify (sys->p_context, false);
#endif

    return VLC_SUCCESS;

error:
    st_ClientSessionClose(crd, session);
    return VLC_EGENERIC;
}

/**
 * Initializes a client-side TLS credentials.
 */
static int OpenClient (vlc_tls_creds_t *crd) {

    msg_Dbg(crd, "open st client");

    vlc_tls_creds_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->whitelist = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    crd->sys = sys;
    crd->open = st_ClientSessionOpen;
    crd->close = st_ClientSessionClose;

    return VLC_SUCCESS;

}

static void CloseClient (vlc_tls_creds_t *crd) {
    msg_Dbg(crd, "close secure transport client");

    vlc_tls_creds_sys_t *sys = crd->sys;

    if (sys->whitelist)
        CFRelease(sys->whitelist);

    free (sys);
}
