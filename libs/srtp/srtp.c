/*
 * Secure RTP with libgcrypt
 * Copyright (C) 2007  Rémi Denis-Courmont <rdenis # simphalempin , com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdint.h>
#include <stddef.h>

#include "srtp.h"

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include <gcrypt.h>

#include <netinet/in.h>
#include <pthread.h>

/* TODO:
 * Useful stuff:
 * - ROC profil thingy (multicast really needs this)
 * - replay protection
 *
 * Requirements for conformance:
 * - suites with NULL cipher
 * - SRTCP
 *
 * Useless stuff (because nothing depends on it):
 * - non-nul key derivation rate
 * - MKI payload
 */

typedef struct srtp_proto_t
{
    gcry_cipher_hd_t cipher;
    gcry_md_hd_t     mac;
    uint32_t         salt[4];
    uint8_t          mac_len;
} srtp_proto_t;

struct srtp_session_t
{
    srtp_proto_t rtp;
    srtp_proto_t rtcp;
    unsigned flags;
    unsigned kdr;
    uint32_t rtcp_index;
    uint32_t rtp_roc;
    uint16_t rtp_seq;
};

enum
{
    SRTP_CRYPT,
    SRTP_AUTH,
    SRTP_SALT,
    SRTCP_CRYPT,
    SRTCP_AUTH,
    SRTCP_SALT
};

#ifndef WIN32
GCRY_THREAD_OPTION_PTHREAD_IMPL;
#endif

static bool libgcrypt_usable = false;

static void initonce_libgcrypt (void)
{
    if ((gcry_check_version ("1.1.94") == NULL)
     || gcry_control (GCRYCTL_DISABLE_SECMEM, 0)
     || gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0)
#ifndef WIN32
     || gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread)
#endif
       )
        return;

    libgcrypt_usable = true;
}

static int init_libgcrypt (void)
{
    int retval;
#ifndef WIN32
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    pthread_mutex_lock (&mutex);
    pthread_once (&once, initonce_libgcrypt);
    retval = -libgcrypt_usable;
    pthread_mutex_unlock (&mutex);
#else
# warning FIXME: This is not thread-safe.
    if (!libgcrypt_usable)
        initonce_libgcrypt ();
    retval = -libgcrypt_usable;
#endif

    return retval;

}


static void proto_destroy (srtp_proto_t *p)
{
    gcry_md_close (p->mac);
    gcry_cipher_close (p->cipher);
}


/**
 * Releases all resources associated with a Secure RTP session.
 */
void srtp_destroy (srtp_session_t *s)
{
    assert (s != NULL);

    proto_destroy (&s->rtcp);
    proto_destroy (&s->rtp);
    free (s);
}


static int proto_create (srtp_proto_t *p, int gcipher, int gmd)
{
    if (gcry_cipher_open (&p->cipher, gcipher, GCRY_CIPHER_MODE_CTR, 0) == 0)
    {
        if (gcry_md_open (&p->mac, gmd, GCRY_MD_FLAG_HMAC) == 0)
            return 0;
        gcry_cipher_close (p->cipher);
    }
    return -1;
}


/**
 * Allocates a Secure RTP session.
 *
 * @param name cipher-suite name
 * @param kdr key derivation rate
 * @param winsize anti-replay windows size (between 64 and 32767 inclusive)
 *                0 disable replay attack protection (OK for send only)
 * @param flags OR'ed optional flags.
 *
 * @return NULL in case of error
 */
srtp_session_t *
srtp_create (const char *name, unsigned flags, unsigned kdr, uint16_t winsize)
{
    assert (name != NULL);

    if (kdr != 0)
        return NULL; // FIXME: KDR not implemented yet
    if (winsize != 0)
        return NULL; // FIXME: replay protection not implemented yet

    uint8_t mac_len;
    int cipher = GCRY_CIPHER_AES, md = GCRY_MD_SHA1;

    if (strcmp (name, "AES_CM_128_HMAC_SHA1_80") == 0)
        mac_len = 80;
    else
    if (strcmp (name, "AES_CM_128_HMAC_SHA1_32") == 0)
        mac_len = 32;
    else
    // F8_128_HMAC_SHA1_80 is not implemented
        return NULL;

    if ((flags & ~SRTP_FLAGS_MASK) || (winsize > 32767) || init_libgcrypt ())
        return NULL;

    srtp_session_t *s = malloc (sizeof (*s));
    if (s == NULL)
        return NULL;

    memset (s, 0, sizeof (*s));
    s->flags = flags;
    s->kdr = kdr;

    if (proto_create (&s->rtp, cipher, md) == 0)
    {
        if (proto_create (&s->rtcp, cipher, md) == 0)
            return s;
        proto_destroy (&s->rtp);
    }

    free (s);
    return NULL;
}


/**
 * AES-CM key derivation (saltlen = 14 bytes)
 */
static int
derive (gcry_cipher_hd_t prf, const void *salt,
        const uint8_t *r, size_t rlen, uint8_t label,
        void *out, size_t outlen)
{
    uint8_t iv[16];

    memcpy (iv, salt, 14);
    iv[14] = iv[15] = 0;

    assert (rlen < 14);
    iv[13 - rlen] ^= label;
    for (size_t i = 0; i < rlen; i++)
        iv[sizeof (iv) - rlen + i] ^= r[i];

    /* TODO: retry with CTR mode */
    while (outlen >= sizeof (iv))
    {
        /* AES */
        if (gcry_cipher_encrypt (prf, out, sizeof (iv), iv, sizeof (iv)))
            return EINVAL;
        outlen -= sizeof (iv);
        out = ((uint8_t *)out) + sizeof (iv);

        /* Increment IV in network byte order */
        if (++iv[sizeof (iv) - 1] == 0)
            ++iv[sizeof (iv) -2];
    }

    if (outlen > 0)
    {
        /* Truncated last AES output block */
        if (gcry_cipher_encrypt (prf, iv, sizeof (iv), NULL, 0))
            return -1;
        memcpy (out, iv, outlen);
    }

    return 0;
}


static int
proto_derive (srtp_proto_t *p, gcry_cipher_hd_t prf,
              const void *salt, size_t saltlen,
              const uint8_t *r, size_t rlen, bool rtcp)
{
    if (saltlen != 14)
        return -1;

    uint8_t cipherkey[16];
    uint8_t label = rtcp ? SRTCP_CRYPT : SRTP_CRYPT;

    if (derive (prf, salt, r, rlen, label++, cipherkey, 16)
     || gcry_cipher_setkey (p->cipher, cipherkey, 16)
     || derive (prf, salt, r, rlen, label++, NULL, 0) /* FIXME HMAC */
     || derive (prf, salt, r, rlen, label++, p->salt, 14))
        return -1;

    return 0;
}


/**
 * SRTP/SRTCP cipher/salt/MAC keys derivation.
 */
static int
srtp_derive (srtp_session_t *s, const void *key, size_t keylen,
             const void *salt, size_t saltlen)
{
    gcry_cipher_hd_t prf;
    uint8_t r[6];

    /* TODO: retry with CTR mode */
    if (gcry_cipher_open (&prf, GCRY_CIPHER_AES, GCRY_CIPHER_MODE_ECB, 0)
     || gcry_cipher_setkey (prf, key, keylen))
        return -1;

    /* RTP key derivation */
    if (s->kdr != 0)
    {
        uint64_t index = (((uint64_t)s->rtp_roc) << 16) | s->rtp_seq;
        index /= s->kdr;

        for (int i = sizeof (r) - 1; i >= 0; i--)
        {
            r[i] = index & 0xff;
            index = index >> 8;
        }
    }
    else
        memset (r, 0, sizeof (r));

    if (proto_derive (&s->rtp, prf, salt, saltlen, r, 6, false))
        return -1;

    /* RTCP key derivation */
    memcpy (r, &(uint32_t){ htonl (s->rtcp_index) }, 4);
    if (proto_derive (&s->rtcp, prf, salt, saltlen, r, 4, true))
        return -1;

    (void)gcry_cipher_close (prf);
    return 0;
}



/**
 * Sets (or resets) the master key and master salt for a SRTP session.
 * This must be done at least once before using rtp_send(), rtp_recv(),
 * rtcp_send() or rtcp_recv(). Also, rekeying is required every
 * 2^48 RTP packets or 2^31 RTCP packets (whichever comes first),
 * otherwise the protocol security might be broken.
 *
 * @return 0 on success, in case of error:
 *  EINVAL  invalid or unsupported key/salt sizes combination
 */
int
srtp_setkey (srtp_session_t *s, const void *key, size_t keylen,
             const void *salt, size_t saltlen)
{
    return srtp_derive (s, key, keylen, salt, saltlen) ? EINVAL : 0;
}


/** AES-CM encryption/decryption (ctr length = 16 bytes) */
static int
encrypt (gcry_cipher_hd_t hd, uint32_t *ctr, uint8_t *data, size_t len)
{
    const size_t ctrlen = 16;
    while (len >= ctrlen)
    {
        if (gcry_cipher_setctr (hd, ctr, ctrlen)
         || gcry_cipher_encrypt (hd, data, ctrlen, NULL, 0))
            return -1;

        data += ctrlen;
        len -= ctrlen;
        ctr[3] = htonl (ntohl (ctr[3]) + 1);
    }

    if (len > 0)
    {
        /* Truncated last block */
        uint8_t dummy[ctrlen];
        memcpy (dummy, data, len);
        memset (dummy + len, 0, ctrlen - len);

        if (gcry_cipher_setctr (hd, ctr, ctrlen)
         || gcry_cipher_encrypt (hd, dummy, ctrlen, data, ctrlen))
            return -1;
        memcpy (data, dummy, len);
    }

    return 0;
}


/** AES-CM for RTP (salt = 14 bytes + 2 nul bytes) */
static inline int
rtp_encrypt (gcry_cipher_hd_t hd, uint32_t ssrc, uint32_t roc, uint16_t seq,
             const uint32_t *salt, uint8_t *data, size_t len)
{
    /* Determines cryptographic counter (IV) */
    uint32_t counter[4];
    counter[0] = salt[0];
    counter[1] = salt[1] ^ ssrc;
    counter[2] = salt[2] ^ htonl (roc);
    counter[3] = salt[3] ^ htonl (seq << 16);

    /* Encryption */
    return encrypt (hd, counter, data, len);
}


/**
 * Encrypts/decrypts a RTP packet and updates SRTP context
 * (CTR block cypher mode of operation has identical encryption and
 * decryption function).
 *
 * @param buf RTP packet to be encrypted/digested
 * @param len RTP packet length
 *
 * @return 0 on success, in case of error:
 *  EINVAL  malformatted RTP packet
 */
static int srtp_encrypt (srtp_session_t *s, uint8_t *buf, size_t len)
{
    assert (s != NULL);

    if ((len < 12) || ((buf[0] >> 6) != 2))
        return EINVAL;

    /* Computes encryption offset */
    uint16_t offset = 12;
    offset += (buf[0] & 0xf) * 4; // skips CSRC

    if (buf[0] & 0x10)
    {
        uint16_t extlen;

        offset += 4;
        if (len < offset)
            return EINVAL;

        memcpy (&extlen, buf + offset - 2, 2);
        offset += htons (extlen);
    }

    if (len < offset)
        return EINVAL;

    /* Determines RTP 48-bits counter and SSRC */
    uint32_t ssrc;
    memcpy (&ssrc, buf + 8, 4);

    uint16_t seq = (buf[2] << 8) | buf[3];
    if (((seq - s->rtp_seq) & 0xffff) < 32768)
    {
        if (seq < s->rtp_seq)
            s->rtp_roc++; /* Sequence number wrap */
    }
    else
    {
        if (seq > s->rtp_seq)
            s->rtp_roc--;
    }

    s->rtp_seq = seq;

    if (s->flags & SRTP_UNENCRYPTED)
        return 0;

    if (rtp_encrypt (s->rtp.cipher, ssrc, s->rtp_roc, seq, s->rtp.salt,
                     buf + offset, len - offset))
        return EINVAL;

    return 0;
}


/**
 * Turns a RTP packet into a SRTP packet: encrypt it, then computes
 * the authentication tag and appends it.
 * Note that you can encrypt packet in disorder.
 *
 * @param buf RTP packet to be encrypted/digested
 * @param lenp pointer to the RTP packet length on entry,
 *             set to the SRTP length on exit (undefined in case of error)
 * @param bufsize size (bytes) of the packet buffer
 *
 * @return 0 on success, in case of error:
 *  EINVAL  malformatted RTP packet
 *  ENOBUFS bufsize is too small
 */
int
srtp_send (srtp_session_t *s, uint8_t *buf, size_t *lenp, size_t bufsize)
{
    size_t len = *lenp;
    int val = srtp_encrypt (s, buf, len);
    if (val)
        return val;

    if (bufsize < (len + s->rtp.mac_len))
        return ENOBUFS;

    /* FIXME: HMAC and anti-replay */
    return 0;
}


/**
 * Turns a RTP packet into a SRTP packet: encrypt it, then computes
 * the authentication tag and appends it.
 * Note that you can encrypt packet in disorder.
 *
 * @param buf RTP packet to be decrypted/digested
 * @param lenp pointer to the RTP packet length on entry,
 *             set to the SRTP length on exit (undefined in case of error)
 *
 * @return 0 on success, in case of error:
 *  EINVAL  malformatted RTP packet
 *  EACCES  authentication failed (spoofed packet or out-of-sync)
 */
int
srtp_recv (srtp_session_t *s, uint8_t *buf, size_t *lenp)
{
    size_t len = *lenp;
    int val = srtp_encrypt (s, buf, len);
    if (val)
        return val;

    /* FIXME: HMAC and anti-replay */
    return 0;
}

