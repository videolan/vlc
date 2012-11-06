/*
 * Secure RTP with libgcrypt
 * Copyright (C) 2007  Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifndef LIBVLC_SRTP_H
# define LIBVLC_SRTP_H 1

typedef struct srtp_session_t srtp_session_t;

enum
{
    SRTP_UNENCRYPTED=0x1,   //< do not encrypt SRTP packets
    SRTCP_UNENCRYPTED=0x2,  //< do not encrypt SRTCP packets
    SRTP_UNAUTHENTICATED=0x4, //< authenticate only SRTCP packets

    SRTP_RCC_MODE1=0x10,    //< use Roll-over-Counter Carry mode 1
    SRTP_RCC_MODE2=0x20,    //< use Roll-over-Counter Carry mode 2
    SRTP_RCC_MODE3=0x30,    //< use Roll-over-Counter Carry mode 3 (insecure)

    SRTP_FLAGS_MASK=0x37    //< mask for valid flags
};

/** SRTP encryption algorithms (ciphers); same values as MIKEY */
enum
{
    SRTP_ENCR_NULL=0,   //< no encryption
    SRTP_ENCR_AES_CM=1, //< AES counter mode
    SRTP_ENCR_AES_F8=2, //< AES F8 mode (not implemented)
};

/** SRTP authenticaton algorithms; same values as MIKEY */
enum
{
    SRTP_AUTH_NULL=0,      //< no authentication code
    SRTP_AUTH_HMAC_SHA1=1, //< HMAC-SHA1
};

/** SRTP pseudo random function; same values as MIKEY */
enum
{
    SRTP_PRF_AES_CM=0, //< AES counter mode
};

# ifdef __cplusplus
extern "C" {
# endif

srtp_session_t *srtp_create (int encr, int auth, unsigned tag_len, int prf,
                             unsigned flags);
void srtp_destroy (srtp_session_t *s);

int srtp_setkey (srtp_session_t *s, const void *key, size_t keylen,
                 const void *salt, size_t saltlen);
int srtp_setkeystring (srtp_session_t *s, const char *key, const char *salt);

void srtp_setrcc_rate (srtp_session_t *s, uint16_t rate);

int srtp_send (srtp_session_t *s, uint8_t *buf, size_t *lenp, size_t maxsize);
int srtp_recv (srtp_session_t *s, uint8_t *buf, size_t *lenp);
int srtcp_send (srtp_session_t *s, uint8_t *buf, size_t *lenp, size_t maxsiz);
int srtcp_recv (srtp_session_t *s, uint8_t *buf, size_t *lenp);

# ifdef __cplusplus
}
# endif
#endif

