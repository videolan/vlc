/*
 * Secure RTP with libgcrypt
 * Copyright (C) 2007  Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* TODO:
 * Useless stuff (because nothing depends on it):
 * - non-nul key derivation rate
 * - MKI payload
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

#ifdef _WIN32
# include <winsock2.h>
#else
# include <netinet/in.h>
#endif

#define debug( ... ) (void)0

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
    uint16_t rtp_rcc;
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


static inline unsigned rcc_mode (const srtp_session_t *s)
{
    return (s->flags >> 4) & 3;
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
 * @param encr encryption algorithm number
 * @param auth authentication algortihm number
 * @param tag_len authentication tag byte length (NOT including RCC)
 * @param flags OR'ed optional flags.
 *
 * @return NULL in case of error
 */
srtp_session_t *
srtp_create (int encr, int auth, unsigned tag_len, int prf, unsigned flags)
{
    if ((flags & ~SRTP_FLAGS_MASK))
        return NULL;

    int cipher, md;
    switch (encr)
    {
        case SRTP_ENCR_NULL:
            cipher = GCRY_CIPHER_NONE;
            break;

        case SRTP_ENCR_AES_CM:
            cipher = GCRY_CIPHER_AES;
            break;

        default:
            return NULL;
    }

    switch (auth)
    {
        case SRTP_AUTH_NULL:
            md = GCRY_MD_NONE;
            break;

        case SRTP_AUTH_HMAC_SHA1:
            md = GCRY_MD_SHA1;
            break;

        default:
            return NULL;
    }

    if (tag_len > gcry_md_get_algo_dlen (md))
        return NULL;

    if (prf != SRTP_PRF_AES_CM)
        return NULL;

    srtp_session_t *s = malloc (sizeof (*s));
    if (s == NULL)
        return NULL;

    memset (s, 0, sizeof (*s));
    s->flags = flags;
    s->tag_len = tag_len;
    s->rtp_rcc = 1; /* Default RCC rate */
    if (rcc_mode (s))
    {
        if (tag_len < 4)
            goto error;
    }

    if (proto_create (&s->rtp, cipher, md) == 0)
    {
        if (proto_create (&s->rtcp, cipher, md) == 0)
            return s;
        proto_destroy (&s->rtp);
    }

error:
    free (s);
    return NULL;
}


/**
 * Counter Mode encryption/decryption (ctr length = 16 bytes)
 * with non-padded (truncated) text
 */
static int
do_ctr_crypt (gcry_cipher_hd_t hd, const void *ctr, uint8_t *data, size_t len)
{
    const size_t ctrlen = 16;
    div_t d = div (len, ctrlen);

    if (gcry_cipher_setctr (hd, ctr, ctrlen)
     || gcry_cipher_encrypt (hd, data, d.quot * ctrlen, NULL, 0))
        return -1;

    if (d.rem)
    {
        /* Truncated last block */
        uint8_t dummy[ctrlen];
        data += d.quot * ctrlen;
        memcpy (dummy, data, d.rem);
        memset (dummy + d.rem, 0, ctrlen - d.rem);

        if (gcry_cipher_encrypt (hd, dummy, ctrlen, data, ctrlen))
            return -1;
        memcpy (data, dummy, d.rem);
    }

    return 0;
}


/**
 * AES-CM key derivation (saltlen = 14 bytes)
 */
static int
do_derive (gcry_cipher_hd_t prf, const void *salt,
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

    memset (out, 0, outlen);
    return do_ctr_crypt (prf, iv, out, outlen);
}


/**
 * Sets (or resets) the master key and master salt for a SRTP session.
 * This must be done at least once before using srtp_send(), srtp_recv(),
 * srtcp_send() or srtcp_recv(). Also, rekeying is required every
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
    /* SRTP/SRTCP cipher/salt/MAC keys derivation */
    gcry_cipher_hd_t prf;
    uint8_t r[6], keybuf[20];

    if (saltlen != 14)
        return EINVAL;

    if (gcry_cipher_open (&prf, GCRY_CIPHER_AES, GCRY_CIPHER_MODE_CTR, 0)
     || gcry_cipher_setkey (prf, key, keylen))
        return EINVAL;

    /* SRTP key derivation */
#if 0
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
#endif
        memset (r, 0, sizeof (r));
    if (do_derive (prf, salt, r, 6, SRTP_CRYPT, keybuf, 16)
     || gcry_cipher_setkey (s->rtp.cipher, keybuf, 16)
     || do_derive (prf, salt, r, 6, SRTP_AUTH, keybuf, 20)
     || gcry_md_setkey (s->rtp.mac, keybuf, 20)
     || do_derive (prf, salt, r, 6, SRTP_SALT, s->rtp.salt, 14))
        return -1;

    /* SRTCP key derivation */
    memcpy (r, &(uint32_t){ htonl (s->rtcp_index) }, 4);
    if (do_derive (prf, salt, r, 4, SRTCP_CRYPT, keybuf, 16)
     || gcry_cipher_setkey (s->rtcp.cipher, keybuf, 16)
     || do_derive (prf, salt, r, 4, SRTCP_AUTH, keybuf, 20)
     || gcry_md_setkey (s->rtcp.mac, keybuf, 20)
     || do_derive (prf, salt, r, 4, SRTCP_SALT, s->rtcp.salt, 14))
        return -1;

    (void)gcry_cipher_close (prf);
    return 0;
}

static int hexdigit (char c)
{
    if ((c >= '0') && (c <= '9'))
        return c - '0';
    if ((c >= 'A') && (c <= 'F'))
        return c - 'A' + 0xA;
    if ((c >= 'a') && (c <= 'f'))
        return c - 'a' + 0xa;
    return -1;
}

static ssize_t hexstring (const char *in, uint8_t *out, size_t outlen)
{
    size_t inlen = strlen (in);

    if ((inlen > (2 * outlen)) || (inlen & 1))
        return -1;

    for (size_t i = 0; i < inlen; i += 2)
    {
        int a = hexdigit (in[i]), b = hexdigit (in[i + 1]);
        if ((a == -1) || (b == -1))
            return -1;
        out[i / 2] = (a << 4) | b;
    }
    return inlen / 2;
}

/**
 * Sets (or resets) the master key and master salt for a SRTP session
 * from hexadecimal strings. See also srtp_setkey().
 *
 * @return 0 on success, in case of error:
 *  EINVAL  invalid or unsupported key/salt sizes combination
 */
int
srtp_setkeystring (srtp_session_t *s, const char *key, const char *salt)
{
    uint8_t bkey[16]; /* TODO/NOTE: hard-coded for AES */
    uint8_t bsalt[14]; /* TODO/NOTE: hard-coded for the PRF-AES-CM */
    ssize_t bkeylen = hexstring (key, bkey, sizeof (bkey));
    ssize_t bsaltlen = hexstring (salt, bsalt, sizeof (bsalt));

    if ((bkeylen == -1) || (bsaltlen == -1))
        return EINVAL;
    return srtp_setkey (s, bkey, bkeylen, bsalt, bsaltlen) ? EINVAL : 0;
}

/**
 * Sets Roll-over-Counter Carry (RCC) rate for the SRTP session. If not
 * specified (through this function), the default rate of ONE is assumed
 * (i.e. every RTP packets will carry the RoC). RCC rate is ignored if none
 * of the RCC mode has been selected.
 *
 * The RCC mode is selected through one of these flags for srtp_create():
 *  SRTP_RCC_MODE1: integrity protection only for RoC carrying packets
 *  SRTP_RCC_MODE2: integrity protection for all packets
 *  SRTP_RCC_MODE3: no integrity protection
 *
 * RCC mode 3 is insecure. Compared to plain RTP, it provides confidentiality
 * (through encryption) but is much more prone to DoS. It can only be used if
 * anti-spoofing protection is provided by lower network layers (e.g. IPsec,
 * or trusted routers and proper source address filtering).
 *
 * If RCC rate is 1, RCC mode 1 and 2 are functionally identical.
 *
 * @param rate RoC Carry rate (MUST NOT be zero)
 */
void srtp_setrcc_rate (srtp_session_t *s, uint16_t rate)
{
    assert (rate != 0);
    s->rtp_rcc = rate;
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
    return do_ctr_crypt (hd, counter, data, len);
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
rtp_digest (gcry_md_hd_t md, const uint8_t *data, size_t len,
            uint32_t roc)
{
    gcry_md_reset (md);
    gcry_md_write (md, data, len);
    gcry_md_write (md, &(uint32_t){ htonl (roc) }, 4);
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
    assert (len >= 12u);

    if ((buf[0] >> 6) != 2)
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
        s->rtp.window |= UINT64_C(1);
        s->rtp_seq = seq, s->rtp_roc = roc;
    }
    else
    {
        /* Sequence in the past/present, bad */
        diff = -diff;
        if ((diff >= 64) || ((s->rtp.window >> diff) & 1))
            return EACCES; /* Replay attack */
        s->rtp.window |= UINT64_C(1) << diff;
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
 *             set to the SRTP length on exit (undefined on non-ENOSPC error)
 * @param bufsize size (bytes) of the packet buffer
 *
 * @return 0 on success, in case of error:
 *  EINVAL  malformatted RTP packet or internal error
 *  ENOSPC  bufsize is too small to add authentication tag
 *          (<lenp> will hold the required byte size)
 *  EACCES  packet would trigger a replay error on receiver
 */
int
srtp_send (srtp_session_t *s, uint8_t *buf, size_t *lenp, size_t bufsize)
{
    size_t len = *lenp;
    size_t tag_len;
    size_t roc_len = 0;

    /* Compute required buffer size */
    if (len < 12u)
        return EINVAL;

    if (!(s->flags & SRTP_UNAUTHENTICATED))
    {
        tag_len = s->tag_len;

        if (rcc_mode (s))
        {
            assert (tag_len >= 4);
            assert (s->rtp_rcc != 0);
            if ((rtp_seq (buf) % s->rtp_rcc) == 0)
            {
                roc_len = 4;
                if (rcc_mode (s) == 3)
                    tag_len = 0; /* RCC mode 3 -> no auth*/
                else
                    tag_len -= 4; /* RCC mode 1 or 2 -> auth*/
            }
            else
            {
                if (rcc_mode (s) & 1)
                    tag_len = 0; /* RCC mode 1 or 3 -> no auth */
            }
        }

        *lenp = len + roc_len + tag_len;
    }
    else
        tag_len = 0;

    if (bufsize < *lenp)
        return ENOSPC;

    /* Encrypt payload */
    int val = srtp_crypt (s, buf, len);
    if (val)
        return val;

    /* Authenticate payload */
    if (!(s->flags & SRTP_UNAUTHENTICATED))
    {
        uint32_t roc = srtp_compute_roc (s, rtp_seq (buf));
        const uint8_t *tag = rtp_digest (s->rtp.mac, buf, len, roc);

        if (roc_len)
        {
            memcpy (buf + len, &(uint32_t){ htonl (s->rtp_roc) }, 4);
            len += 4;
        }
        memcpy (buf + len, tag, tag_len);
#if 0
        printf ("Sent    : 0x");
        for (unsigned i = 0; i < tag_len; i++)
            printf ("%02x", tag[i]);
        puts ("");
#endif
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
    if (len < 12u)
        return EINVAL;

    if (!(s->flags & SRTP_UNAUTHENTICATED))
    {
        size_t tag_len = s->tag_len, roc_len = 0;
        if (rcc_mode (s))
        {
            if ((rtp_seq (buf) % s->rtp_rcc) == 0)
            {
                roc_len = 4;
                if (rcc_mode (s) == 3)
                    tag_len = 0;
                else
                    tag_len -= 4;
            }
            else
            {
                if (rcc_mode (s) & 1)
                    tag_len = 0; // RCC mode 1 or 3: no auth
            }
        }

        if (len < (12u + roc_len + tag_len))
            return EINVAL;
        len -= roc_len + tag_len;

        uint32_t roc = srtp_compute_roc (s, rtp_seq (buf)), rcc;
        if (roc_len)
        {
            assert (roc_len == 4);
            memcpy (&rcc, buf + len, 4);
            rcc = ntohl (rcc);
        }
        else
            rcc = roc;

        const uint8_t *tag = rtp_digest (s->rtp.mac, buf, len, rcc);
#if 0
        printf ("Computed: 0x");
        for (unsigned i = 0; i < tag_len; i++)
            printf ("%02x", tag[i]);
        printf ("\nReceived: 0x");
        for (unsigned i = 0; i < tag_len; i++)
            printf ("%02x", buf[len + roc_len + i]);
        puts ("");
#endif
        if (memcmp (buf + len + roc_len, tag, tag_len))
            return EACCES;

        if (roc_len)
        {
            /* Authenticated packet carried a Roll-Over-Counter */
            s->rtp_roc += rcc - roc;
            assert (srtp_compute_roc (s, rtp_seq (buf)) == rcc);
        }
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
        s->rtcp.window |= UINT64_C(1);
        s->rtcp_index = index;
    }
    else
    {
        /* Packet in the past/present, bad */
        diff = -diff;
        if ((diff >= 64) || ((s->rtcp.window >> diff) & 1))
            return EACCES; // replay attack!
        s->rtp.window |= UINT64_C(1) << diff;
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

    const uint8_t *tag = rtcp_digest (s->rtcp.mac, buf, len);
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

    const uint8_t *tag = rtcp_digest (s->rtcp.mac, buf, len);
    if (memcmp (buf + len, tag, s->tag_len))
         return EACCES;

    len -= 4; /* Remove SRTCP index before decryption */
    *lenp = len;
    return srtp_crypt (s, buf, len);
}

