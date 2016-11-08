/*****************************************************************************
 * file_crypt.h: Crypt extension of the keystore memory module
 *****************************************************************************
 * Copyright © 2016 VLC authors, VideoLAN and VideoLabs
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#if defined(__ANDROID__) || defined(_WIN32)
# define CRYPTFILE

struct crypt
{
    void *  p_ctx;
    size_t  (*pf_encrypt)(vlc_keystore *, void *, const uint8_t *, size_t, uint8_t **);
    size_t  (*pf_decrypt)(vlc_keystore *, void *, const uint8_t *, size_t, uint8_t **);
    void    (*pf_clean)(vlc_keystore *, void *);
};

int CryptInit(vlc_keystore *, struct crypt *);

#endif
