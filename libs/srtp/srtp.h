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

#ifndef LIBVLC_SRTP_H
# define LIBVLC_SRTP_H 1

typedef struct srtp_session_t srtp_session_t;

enum
{
	SRTP_UNENCRYPTED=0x1, // do not encrypt SRTP packets
	SRTCP_UNENCRYPTED=0x2, // do not encrypt SRTCP packets
	SRTP_NULL_CIPHER=0x3, // use NULL cipher (encrypt nothing)
	SRTP_UNAUTHENTICATED=0x4, // do not authenticated SRTP packets
	SRTP_FLAGS_MASK=0x7
};


# ifdef __cplusplus
extern "C" {
# endif

srtp_session_t *srtp_create (const char *name, unsigned flags, unsigned kdr,
                             uint16_t winsize);
void srtp_destroy (srtp_session_t *s);
int srtp_setkey (srtp_session_t *s, const void *key, size_t keylen,
                 const void *salt, size_t saltlen);

int srtp_send (srtp_session_t *s, uint8_t *buf, size_t *lenp, size_t maxsize);
int srtp_recv (srtp_session_t *s, uint8_t *buf, size_t *lenp);
int srtcp_send (srtp_session_t *s, uint8_t *buf, size_t *lenp, size_t maxsiz);
int srtcp_recv (srtp_session_t *s, uint8_t *buf, size_t *lenp);

# ifdef __cplusplus
}
# endif
#endif

