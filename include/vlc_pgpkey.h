/*****************************************************************************
 * vlc_pgpkey.h: VideoLAN PGP Public Key used to sign releases
 *****************************************************************************
 * Copyright © 2008 the VideoLAN team
 * $Id$
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

/* We trust this public key, and by extension, also keys signed by it. */

/*
 * VideoLAN Release Signing Key (2009)
 * expirates on 2010-01-01
 */

static uint8_t videolan_public_key_longid[8] = {
  0x13, 0xE9, 0x5E, 0xDB, 0x37, 0x99, 0x97, 0xB5
};

/* gpg --export --armor "<id>"|sed -e s/^/\"/ -e s/\$/\\\\n\"/ */
static uint8_t videolan_public_key[] = {
"-----BEGIN PGP PUBLIC KEY BLOCK-----\n"
"Version: GnuPG v2.0.14 (GNU/Linux)\n"
"\n"
"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\n"
"8I1sl1Ayq95eDEgvEME=\n"
"=qyrr\n"
"-----END PGP PUBLIC KEY BLOCK-----\n"
};
