/*****************************************************************************
 * vlc_update.h: VLC update and plugins download
 *****************************************************************************
 * Copyright © 2005-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
 *          Rafaël Carré <funman@videolanorg>
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

#if !defined( __LIBVLC__ )
  #error You are not libvlc or one of its plugins. You cannot include this file
#endif

#ifdef UPDATE_CHECK

#ifndef _VLC_UPDATE_H
#define _VLC_UPDATE_H

#include <vlc/vlc.h>

#include <vlc_stream.h>     /* key & signature downloading */
#include <vlc_strings.h>    /* b64 decoding */
#include <vlc_charset.h>    /* utf8_fopen() */
#include <gcrypt.h>         /* cryptography and digest algorithms */

/**
 * \defgroup update Update
 *
 * @{
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

/* used for public key signatures */
struct signature_packet_v4_t
{ /* hashed_data or unhashed_data can be empty, so the signature packet is
   * theorically at least 54 bytes long, but always more than that. */

    uint8_t version;
    uint8_t type;
    uint8_t public_key_algo;
    uint8_t digest_algo;
    uint8_t hashed_data_len[2];
    uint8_t *hashed_data;
    uint8_t unhashed_data_len[2];
    uint8_t *unhashed_data;
    uint8_t hash_verification[2];

    /* The part below is made of consecutive MPIs, their number and size being
     * public-key-algorithm dependant.
     * But since we use DSA signatures only, we fix it. */
    uint8_t r[2+20];
    uint8_t s[2+20];
};

/* Used for binary document signatures (to be compatible with older software)
 * DSA/SHA-1 is always 65 bytes */
struct signature_packet_v3_t
{
    uint8_t header[2];
    uint8_t version;            /* 3 */
    uint8_t hashed_data_len;    /* MUST be 5 */
    uint8_t type;
    uint8_t timestamp[4];       /* 4 bytes scalar number */
    uint8_t issuer_longid[8];  /* The key which signed the document */
    uint8_t public_key_algo;    /* we only know about DSA */
    uint8_t digest_algo;        /* and his little sister SHA-1 */
    uint8_t hash_verification[2];/* the 2 1st bytes of the SHA-1 hash */

    /* The part below is made of consecutive MPIs, their number and size being
     * public-key-algorithm dependant.
     * But since we use DSA signatures only, we fix it. */
    uint8_t r[2+20];
    uint8_t s[2+20];
};

typedef struct public_key_packet_t public_key_packet_t;
typedef struct signature_packet_v4_t signature_packet_v4_t;
typedef struct signature_packet_v3_t signature_packet_v3_t;

struct public_key_t
{
    uint8_t longid[8];       /* Long id */
    uint8_t *psz_username;    /* USER ID */

    public_key_packet_t key;       /* Public key packet */

    signature_packet_v4_t sig;     /* Signature packet, by the embedded key */
};

typedef struct public_key_t public_key_t;

enum
{
    UpdateReleaseStatusOlder,
    UpdateReleaseStatusEqual,
    UpdateReleaseStatusNewer
};

/**
 * Describes an update VLC release number
 */
struct update_release_t
{
    int i_major;        ///< Version major
    int i_minor;        ///< Version minor
    int i_revision;     ///< Version revision
    unsigned char extra;///< Version extra
    char* psz_url;      ///< Download URL
    char* psz_desc;     ///< Release description
};

/**
 * The update object. Stores (and caches) all information relative to updates
 */
struct update_t
{
    libvlc_int_t *p_libvlc;
    vlc_mutex_t lock;
    struct update_release_t release;    ///< Release (version)
    public_key_t *p_pkey;
};

#define update_New( a ) __update_New( VLC_OBJECT( a ) )

VLC_EXPORT( update_t *, __update_New, ( vlc_object_t * ) );
VLC_EXPORT( void, update_Delete, ( update_t * ) );
VLC_EXPORT( void, update_Check, ( update_t *, void (*callback)( void*, bool ), void * ) );
VLC_EXPORT( int, update_CompareReleaseToCurrent, ( update_t * ) );
VLC_EXPORT( void, update_Download, ( update_t *, char* ) );

/**
 * @}
 */

#endif

#endif
