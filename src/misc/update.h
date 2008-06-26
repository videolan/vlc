/*****************************************************************************
 * update.h: VLC PGP update private API
 *****************************************************************************
 * Copyright © 2007-2008 the VideoLAN team
 *
 * Authors: Rafaël Carré <funman@videolanorg>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either release 2 of the License, or
 * (at your option) any later release.
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

/* Go reading the rfc 4880 ! NOW !! */

/*
 * XXX
 *  When PGP-signing a file, we only sign a SHA-1 hash of this file
 *  The DSA key size requires that we use an algorithm which produce
 *  a 160 bits long hash
 *  An alternative is RIPEMD160 , which you can use by giving the option
 *      --digest-algo RIPEMD160 to GnuPG
 *
 *  As soon as SHA-1 is broken, this method is not secure anymore, because an
 *  attacker could generate a file with the same SHA-1 hash.
 *
 *  Whenever this happens, we need to use another algorithm / type of key.
 * XXX
 */

enum    /* Public key algorithms */
{
    /* we will only use DSA public keys */
    PUBLIC_KEY_ALGO_DSA = 0x11
};

enum    /* Digest algorithms */
{
    /* and DSA use SHA-1 digest */
    DIGEST_ALGO_SHA1    = 0x02
};

enum    /* Packet types */
{
    SIGNATURE_PACKET    = 0x02,
    PUBLIC_KEY_PACKET   = 0x06,
    USER_ID_PACKET      = 0x0d
};

enum    /* Signature types */
{
    BINARY_SIGNATURE        = 0x00,
    TEXT_SIGNATURE          = 0x01,

    /* Public keys signatures */
    GENERIC_KEY_SIGNATURE   = 0x10, /* No assumption of verification */
    PERSONA_KEY_SIGNATURE   = 0x11, /* No verification has been made */
    CASUAL_KEY_SIGNATURE    = 0x12, /* Some casual verification */
    POSITIVE_KEY_SIGNATURE  = 0x13  /* Substantial verification */
};

enum    /* Signature subpacket types */
{
    ISSUER_SUBPACKET    = 0x10
};

struct public_key_packet_t
{ /* a public key packet (DSA/SHA-1) is 418 bytes */

    uint8_t version;      /* we use only version 4 */
    uint8_t timestamp[4]; /* creation time of the key */
    uint8_t algo;         /* we only use DSA */
    /* the multi precision integers, with their 2 bytes length header */
    uint8_t p[2+128];
    uint8_t q[2+20];
    uint8_t g[2+128];
    uint8_t y[2+128];
};

/* used for public key and file signatures */
struct signature_packet_t
{
    uint8_t version; /* 3 or 4 */

    uint8_t type;
    uint8_t public_key_algo;    /* DSA only */
    uint8_t digest_algo;        /* SHA-1 only */

    uint8_t hash_verification[2];
    uint8_t issuer_longid[8];

    union   /* version specific data */
    {
        struct
        {
            uint8_t hashed_data_len[2];     /* scalar number */
            uint8_t *hashed_data;           /* hashed_data_len bytes */
            uint8_t unhashed_data_len[2];   /* scalar number */
            uint8_t *unhashed_data;         /* unhashed_data_len bytes */
        } v4;
        struct
        {
            uint8_t hashed_data_len;    /* MUST be 5 */
            uint8_t timestamp[4];       /* 4 bytes scalar number */
        } v3;
    } specific;

/* The part below is made of consecutive MPIs, their number and size being
 * public-key-algorithm dependent.
 *
 * Since we use DSA signatures only, there is 2 integers, r & s, made of:
 *      2 bytes for the integer length (scalar number)
 *      160 bits (20 bytes) for the integer itself
 *
 * Note: the integers may be less than 160 significant bits
 */
    uint8_t r[2+20];
    uint8_t s[2+20];
};

typedef struct public_key_packet_t public_key_packet_t;
typedef struct signature_packet_t signature_packet_t;

struct public_key_t
{
    uint8_t longid[8];       /* Long id */
    uint8_t *psz_username;    /* USER ID */

    public_key_packet_t key;       /* Public key packet */

    signature_packet_t sig;     /* Signature packet, by the embedded key */
};

typedef struct public_key_t public_key_t;

/**
 * Non blocking binary download
 */
typedef struct
{
    VLC_COMMON_MEMBERS
    update_t *p_update;
    char *psz_destdir;
} update_download_thread_t;

/**
 * Non blocking update availability verification
 */
typedef struct
{
    VLC_COMMON_MEMBERS
    update_t *p_update;
    void (*pf_callback)( void *, bool );
    void *p_data;
} update_check_thread_t;
/**
 * The update object. Stores (and caches) all information relative to updates
 */
struct update_t
{
    libvlc_int_t *p_libvlc;
    vlc_mutex_t lock;
    struct update_release_t release;    ///< Release (version)
    public_key_t *p_pkey;
    update_download_thread_t *p_download;
    update_check_thread_t *p_check;
};

