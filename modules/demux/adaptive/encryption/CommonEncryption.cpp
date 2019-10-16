/*****************************************************************************
 * CommonEncryption.cpp
 *****************************************************************************
 * Copyright (C) 2015-2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "CommonEncryption.hpp"
#include "Keyring.hpp"
#include "../SharedResources.hpp"

#include <vlc_common.h>

#ifdef HAVE_GCRYPT
 #include <gcrypt.h>
 #include <vlc_gcrypt.h>
#endif

using namespace adaptive::encryption;


CommonEncryption::CommonEncryption()
{
    method = CommonEncryption::Method::NONE;
}

void CommonEncryption::mergeWith(const CommonEncryption &other)
{
    if(method == CommonEncryption::Method::NONE &&
       other.method != CommonEncryption::Method::NONE)
        method = other.method;
    if(uri.empty() && !other.uri.empty())
        uri = other.uri;
    if(iv.empty() && !other.iv.empty())
        iv = other.iv;
}

CommonEncryptionSession::CommonEncryptionSession()
{
    ctx = NULL;
}


CommonEncryptionSession::~CommonEncryptionSession()
{
    close();
}

bool CommonEncryptionSession::start(SharedResources *res, const CommonEncryption &enc)
{
    if(ctx)
        close();
    encryption = enc;
#ifdef HAVE_GCRYPT
    if(encryption.method == CommonEncryption::Method::AES_128)
    {
        if(key.empty())
        {
            if(!encryption.uri.empty())
                key = res->getKeyring()->getKey(res, encryption.uri);
            if(key.size() != 16)
                return false;
        }

        vlc_gcrypt_init();
        gcry_cipher_hd_t handle;
        if( gcry_cipher_open(&handle, GCRY_CIPHER_AES, GCRY_CIPHER_MODE_CBC, 0) ||
                gcry_cipher_setkey(handle, &key[0], 16) ||
                gcry_cipher_setiv(handle, &encryption.iv[0], 16) )
        {
            gcry_cipher_close(handle);
            ctx = NULL;
            return false;
        }
        ctx = handle;
    }
#endif
    return true;
}

void CommonEncryptionSession::close()
{
#ifdef HAVE_GCRYPT
    gcry_cipher_hd_t handle = reinterpret_cast<gcry_cipher_hd_t>(ctx);
    if(ctx)
        gcry_cipher_close(handle);
    ctx = NULL;
#endif
}

size_t CommonEncryptionSession::decrypt(void *inputdata, size_t inputbytes, bool last)
{
#ifndef HAVE_GCRYPT
    VLC_UNUSED(inputdata);
    VLC_UNUSED(last);
#else
    gcry_cipher_hd_t handle = reinterpret_cast<gcry_cipher_hd_t>(ctx);
    if(encryption.method == CommonEncryption::Method::AES_128 && ctx)
    {
        if ((inputbytes % 16) != 0 || inputbytes < 16 ||
            gcry_cipher_decrypt(handle, inputdata, inputbytes, NULL, 0))
        {
            inputbytes = 0;
        }
        else if(last)
        {
            /* last bytes */
            /* remove the PKCS#7 padding from the buffer */
            const uint8_t pad = reinterpret_cast<uint8_t *>(inputdata)[inputbytes - 1];
            for(uint8_t i=0; i<pad && i<16; i++)
            {
                if(reinterpret_cast<uint8_t *>(inputdata)[inputbytes - i - 1] != pad)
                    break;
                if(i+1==pad)
                    inputbytes -= pad;
            }
        }
    }
    else
#endif
    if(encryption.method != CommonEncryption::Method::NONE)
    {
        inputbytes = 0;
    }

    return inputbytes;
}
