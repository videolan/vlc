/*****************************************************************************
 * update.h: VLC PGP update private API
 *****************************************************************************
 * Copyright © 2007-2008 VLC authors and VideoLAN
 *
 * Authors: Rafaël Carré <funman@videolanorg>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either release 2 of the License, or
 * (at your option) any later release.
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

#include <vlc_update.h>
#include <vlc_atomic.h>

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
{

    uint8_t version;      /* we use only version 4 */
    uint8_t timestamp[4]; /* creation time of the key */
    uint8_t algo;         /* DSA or RSA */

    /* the multi precision integers, with their 2 bytes length header */
    union {
        struct {
            uint8_t p[2+3072/8];
            uint8_t q[2+256/8];
            uint8_t g[2+3072/8];
            uint8_t y[2+3072/8];
        } dsa ;
        struct {
            uint8_t n[2+4096/8];
            uint8_t e[2+4096/8];
        } rsa;
    } sig;
};

/* used for public key and file signatures */
struct signature_packet_t
{
    uint8_t version; /* 3 or 4 */

    uint8_t type;
    uint8_t public_key_algo;    /* DSA or RSA */
    uint8_t digest_algo;

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
 */
    union {
        struct {
            uint8_t r[2+256/8];
            uint8_t s[2+256/8];
        } dsa;
        struct {
            uint8_t s[2+4096/8];
        } rsa;
    } algo_specific;
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

    vlc_thread_t thread;
    atomic_bool aborted;
    update_t *p_update;
    char *psz_destdir;
} update_download_thread_t;

/**
 * Non blocking update availability verification
 */
typedef struct
{
    vlc_thread_t thread;

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

/*
 * download a public key (the last one) from videolan server, and parse it
 */
public_key_t *
download_key(
        vlc_object_t *p_this, const uint8_t *p_longid,
        const uint8_t *p_signature_issuer );

/*
 * fill a public_key_t with public key data, including:
 *   * public key packet
 *   * signature packet issued by key which long id is p_sig_issuer
 *   * user id packet
 */
int
parse_public_key(
        const uint8_t *p_key_data, size_t i_key_len, public_key_t *p_key,
        const uint8_t *p_sig_issuer );

/*
 * Verify an OpenPGP signature made on some hash, with some public key
 */
int
verify_signature(signature_packet_t *sign, public_key_packet_t *p_key,
        uint8_t *p_hash );

/*
 * Download the signature associated to a document or a binary file.
 * We're given the file's url, we just append ".asc" to it and download
 */
int
download_signature(
        vlc_object_t *p_this, signature_packet_t *p_sig, const char *psz_url );

/*
 * return a hash of a text
 */
uint8_t *
hash_from_text(
        const char *psz_text, signature_packet_t *p_sig );

/*
 * return a hash of a file
 */
uint8_t *
hash_from_file(
        const char *psz_file, signature_packet_t *p_sig );

/*
 * return a hash of a public key
 */
uint8_t *
hash_from_public_key( public_key_t *p_pkey );
