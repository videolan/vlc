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

#define debug( ... ) (void)0

/* TODO:
 * Useful stuff:
 * - ROC profile thingy (multicast really needs this)
 *
 * Useless stuff (because nothing depends on it):
 * - non-nul key derivation rate
 * - MKI payload
 */

typedef struct srtp_proto_t
{
    gcry_cipher_hd_t cipher;
    gcry_md_hd_t     mac;
    uint64_t         window;
    uint32_t         salt[4];
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
    uint8_t  tag_len;
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

#ifdef WIN32
# include <winsock2.h>
#else
# include <netinet/in.h>
# include <pthread.h>
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
 * Allocates a Secure RTP one-way session.
 * The same session cannot be used both ways because this would confuse
 * internal cryptographic counters; it is however of course feasible to open
 * multiple simultaneous sessions with the same master key.
 *
 * @param name cipher-suite name
 * @param kdr key derivation rate
 * @param flags OR'ed optional flags.
 *
 * @return NULL in case of error
 */
srtp_session_t *
srtp_create (const char *name, unsigned flags, unsigned kdr)
{
    assert (name != NULL);

    if (kdr != 0)
        return NULL; // FIXME: KDR not implemented yet

    uint8_t tag_len;
    int cipher = GCRY_CIPHER_AES, md = GCRY_MD_SHA1;

    if (strcmp (name, "AES_CM_128_HMAC_SHA1_80") == 0)
        tag_len = 10;
    else
    if (strcmp (name, "AES_CM_128_HMAC_SHA1_32") == 0)
        tag_len = 4;
    else
    // F8_128_HMAC_SHA1_80 is not implemented
        return NULL;

    if ((flags & ~SRTP_FLAGS_MASK) || init_libgcrypt ())
        return NULL;

    srtp_session_t *s = malloc (sizeof (*s));
    if (s == NULL)
        return NULL;

    memset (s, 0, sizeof (*s));
    s->flags = flags;
    s->kdr = kdr;
    s->tag_len = tag_len;

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

    uint8_t keybuf[20];
    uint8_t label = rtcp ? SRTCP_CRYPT : SRTP_CRYPT;

    if (derive (prf, salt, r, rlen, label++, keybuf, 16)
     || gcry_cipher_setkey (p->cipher, keybuf, 16)
     || derive (prf, salt, r, rlen, label++, keybuf, 20)
     || gcry_md_setkey (p->mac, keybuf, 20)
     || derive (prf, salt, r, rlen, label, p->salt, 14))
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
ctr_crypt (gcry_cipher_hd_t hd, uint32_t *ctr, uint8_t *data, size_t len)
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
static int
rtp_crypt (gcry_cipher_hd_t hd, uint32_t ssrc, uint32_t roc, uint16_t seq,
           const uint32_t *salt, uint8_t *data, size_t len)
{
    /* Determines cryptographic counter (IV) */
    uint32_t counter[4];
    counter[0] = salt[0];
    counter[1] = salt[1] ^ ssrc;
    counter[2] = salt[2] ^ htonl (roc);
    counter[3] = salt[3] ^ htonl (seq << 16);

    /* Encryption */
    return ctr_crypt (hd, counter, data, len);
}


/** Determines SRTP Roll-Over-Counter (in host-byte order) */
static uint32_t
srtp_compute_roc (const srtp_session_t *s, uint16_t seq)
{
    uint32_t roc = s->rtp_roc;

    if (((seq - s->rtp_seq) & 0xffff) < 0x8000)
    {
        /* Sequence is ahead, good */
        if (seq < s->rtp_seq)
            roc++; /* Sequence number wrap */
    }
    else
    {
        /* Sequence is late, bad */
        if (seq > s->rtp_seq)
            roc--; /* Wrap back */
    }
    return roc;
}


/** Returns RTP sequence (in host-byte order) */
static inline uint16_t rtp_seq (const uint8_t *buf)
{
    return (buf[2] << 8) | buf[3];
}


/** Message Authentication and Integrity for RTP */
static const uint8_t *
rtp_digest (srtp_session_t *s, const uint8_t *data, size_t len)
{
    const gcry_md_hd_t md = s->rtp.mac;
    uint32_t roc = htonl (srtp_compute_roc (s, rtp_seq (data)));

    gcry_md_reset (md);
    gcry_md_write (md, data, len);
    gcry_md_write (md, &roc, 4);
    return gcry_md_read (md, 0);
}


/**
 * Encrypts/decrypts a RTP packet and updates SRTP context
 * (CTR block cypher mode of operation has identical encryption and
 * decryption function).
 *
 * @param buf RTP packet to be en-/decrypted
 * @param len RTP packet length
 *
 * @return 0 on success, in case of error:
 *  EINVAL  malformatted RTP packet
 *  EACCES  replayed packet or out-of-window or sync lost
 */
static int srtp_crypt (srtp_session_t *s, uint8_t *buf, size_t len)
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
        offset += htons (extlen); // skips RTP extension header
    }

    if (len < offset)
        return EINVAL;

    /* Determines RTP 48-bits counter and SSRC */
    uint16_t seq = rtp_seq (buf);
    uint32_t roc = srtp_compute_roc (s, seq), ssrc;
    memcpy (&ssrc, buf + 8, 4);

    /* Updates ROC and sequence (it's safe now) */
    int16_t diff = seq - s->rtp_seq;
    if (diff > 0)
    {
        /* Sequence in the future, good */
        s->rtp.window = s->rtp.window << diff;
        s->rtp.window |= 1;
        s->rtp_seq = seq, s->rtp_roc = roc;
    }
    else
    {
        /* Sequence in the past/present, bad */
        diff = -diff;
        if ((diff >= 64) || ((s->rtp.window >> diff) & 1))
            return EACCES; /* Replay attack */
        s->rtp.window |= 1 << diff;
    }

    /* Encrypt/Decrypt */
    if (s->flags & SRTP_UNENCRYPTED)
        return 0;

    if (rtp_crypt (s->rtp.cipher, ssrc, roc, seq, s->rtp.salt,
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
 *  EINVAL  malformatted RTP packet or internal error
 *  ENOSPC  bufsize is too small (to add authentication tag)
 *  EACCES  packet would trigger a replay error on receiver
 */
int
srtp_send (srtp_session_t *s, uint8_t *buf, size_t *lenp, size_t bufsize)
{
    size_t len = *lenp;
    int val = srtp_crypt (s, buf, len);
    if (val)
        return val;

    if (!(s->flags & SRTP_UNAUTHENTICATED))
    {
        if (bufsize < (len + s->tag_len))
            return ENOSPC;

        const uint8_t *tag = rtp_digest (s, buf, len);
        memcpy (buf + len, tag, s->tag_len);
        *lenp = len + s->tag_len;
    }

    return 0;
}


/**
 * Turns a SRTP packet into a RTP packet: authenticates the packet,
 * then decrypts it.
 *
 * @param buf RTP packet to be digested/decrypted
 * @param lenp pointer to the SRTP packet length on entry,
 *             set to the RTP length on exit (undefined in case of error)
 *
 * @return 0 on success, in case of error:
 *  EINVAL  malformatted SRTP packet
 *  EACCES  authentication failed (spoofed packet or out-of-sync)
 */
int
srtp_recv (srtp_session_t *s, uint8_t *buf, size_t *lenp)
{
    size_t len = *lenp;

    if (!(s->flags & SRTP_UNAUTHENTICATED))
    {
        if (len < (12u + s->tag_len))
            return EINVAL;
        len -= s->tag_len;

        const uint8_t *tag = rtp_digest (s, buf, len);
        if (memcmp (buf + len, tag, s->tag_len))
            return EACCES;

        *lenp = len;
    }

    return srtp_crypt (s, buf, len);
}


/** AES-CM for RTCP (salt = 14 bytes + 2 nul bytes) */
static int
rtcp_crypt (gcry_cipher_hd_t hd, uint32_t ssrc, uint32_t index,
            const uint32_t *salt, uint8_t *data, size_t len)
{
    return rtp_crypt (hd, ssrc, index >> 16, index & 0xffff, salt, data, len);
}


/** Message Authentication and Integrity for RTCP */
static const uint8_t *
rtcp_digest (gcry_md_hd_t md, const void *data, size_t len)
{
    gcry_md_reset (md);
    gcry_md_write (md, data, len);
    return gcry_md_read (md, 0);
}


/**
 * Encrypts/decrypts a RTCP packet and updates SRTCP context
 * (CTR block cypher mode of operation has identical encryption and
 * decryption function).
 *
 * @param buf RTCP packet to be en-/decrypted
 * @param len RTCP packet length
 *
 * @return 0 on success, in case of error:
 *  EINVAL  malformatted RTCP packet
 */
static int srtcp_crypt (srtp_session_t *s, uint8_t *buf, size_t len)
{
    assert (s != NULL);

    /* 8-bytes unencrypted header, and 4-bytes unencrypted footer */
    if ((len < 12) || ((buf[0] >> 6) != 2))
        return EINVAL;

    uint32_t index;
    memcpy (&index, buf + len, 4);
    index = ntohl (index);
    if (((index >> 31) != 0) != ((s->flags & SRTCP_UNENCRYPTED) == 0))
        return EINVAL; // E-bit mismatch

    index &= ~(1 << 31); // clear E-bit for counter

    /* Updates SRTCP index (safe here) */
    int32_t diff = index - s->rtcp_index;
    if (diff > 0)
    {
        /* Packet in the future, good */
        s->rtcp.window = s->rtcp.window << diff;
        s->rtcp.window |= 1;
        s->rtcp_index = index;
    }
    else
    {
        /* Packet in the past/present, bad */
        diff = -diff;
        if ((diff >= 64) || ((s->rtcp.window >> diff) & 1))
            return EACCES; // replay attack!
        s->rtp.window |= 1 << diff;
    }

    /* Crypts SRTCP */
    if (s->flags & SRTCP_UNENCRYPTED)
        return 0;

    uint32_t ssrc;
    memcpy (&ssrc, buf + 4, 4);

    if (rtcp_crypt (s->rtcp.cipher, ssrc, index, s->rtp.salt,
                    buf + 8, len - 8))
        return EINVAL;
    return 0;
}


/**
 * Turns a RTCP packet into a SRTCP packet: encrypt it, then computes
 * the authentication tag and appends it.
 *
 * @param buf RTCP packet to be encrypted/digested
 * @param lenp pointer to the RTCP packet length on entry,
 *             set to the SRTCP length on exit (undefined in case of error)
 * @param bufsize size (bytes) of the packet buffer
 *
 * @return 0 on success, in case of error:
 *  EINVAL  malformatted RTCP packet or internal error
 *  ENOSPC  bufsize is too small (to add index and authentication tag)
 */
int
srtcp_send (srtp_session_t *s, uint8_t *buf, size_t *lenp, size_t bufsize)
{
    size_t len = *lenp;
    if (bufsize < (len + 4 + s->tag_len))
        return ENOSPC;

    uint32_t index = ++s->rtcp_index;
    if (index >> 31)
        s->rtcp_index = index = 0; /* 31-bit wrap */

    if ((s->flags & SRTCP_UNENCRYPTED) == 0)
        index |= 0x80000000; /* Set Encrypted bit */
    memcpy (buf + len, &(uint32_t){ htonl (index) }, 4);

    int val = srtcp_crypt (s, buf, len);
    if (val)
        return val;

    len += 4; /* Digests SRTCP index too */

    const uint8_t *tag = rtcp_digest (s->rtp.mac, buf, len);
    memcpy (buf + len, tag, s->tag_len);
    *lenp = len + s->tag_len;
    return 0;
}


/**
 * Turns a SRTCP packet into a RTCP packet: authenticates the packet,
 * then decrypts it.
 *
 * @param buf RTCP packet to be digested/decrypted
 * @param lenp pointer to the SRTCP packet length on entry,
 *             set to the RTCP length on exit (undefined in case of error)
 *
 * @return 0 on success, in case of error:
 *  EINVAL  malformatted SRTCP packet
 *  EACCES  authentication failed (spoofed packet or out-of-sync)
 */
int
srtcp_recv (srtp_session_t *s, uint8_t *buf, size_t *lenp)
{
    size_t len = *lenp;

    if (len < (4u + s->tag_len))
        return EINVAL;
    len -= s->tag_len;

    const uint8_t *tag = rtcp_digest (s->rtp.mac, buf, len);
    if (memcmp (buf + len, tag, s->tag_len))
         return EACCES;

    len -= 4; /* Remove SRTCP index before decryption */
    *lenp = len;
    return srtp_crypt (s, buf, len);
}

