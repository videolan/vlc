/*****************************************************************************
 * vlc_hash.h: Hash functions
 *****************************************************************************
 * Copyright © 2004-2020 VLC authors and VideoLAN
 *
 * Authors: Rémi Denis-Courmont
 *          Rafaël Carré
 *          Marvin Scholz
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

#ifndef VLC_HASH_H
# define VLC_HASH_H

/**
 * \defgroup vlc_hash  Hash functions
 * APIs for simple and frequently used hash algorithms in VLC
 * 
 * Each hash algorithm has a context object which stores all data needed for the
 * hash calculation, this context is not supposed to be modified directly by the
 * called but only with the functions listed here.
 *
 * Supported hash algorithms:
 *   - \ref vlc_hash_md5 "MD5"
 *
 * @{
 */

/**
 * \defgroup vlc_hash_utils  Helper functions
 * Functions commonly used together with hashing functions
 * @{
 */

/**
 * Finish hash computation and return hex representation
 *
 * Finishes the hash computation and provides the hash for the
 * concatenation of all provided data in hex encoded format.
 * The result is written to the buffer pointed to by output, which
 * must be larger than twice the size of the hash output.
 *
 * \param[in,out] ctx    Hash context to finish
 * \param[out]    output Output buffer to write the string to
 */
#ifndef __cplusplus
#define vlc_hash_FinishHex(ctx, output)                             \
    do {                                                            \
        char out_tmp[_Generic((ctx),                                \
            vlc_hash_md5_t *: VLC_HASH_MD5_DIGEST_SIZE)];           \
        _Generic((ctx),                                             \
            vlc_hash_md5_t *: vlc_hash_md5_Finish)                  \
        (ctx, out_tmp, sizeof(out_tmp));                            \
        vlc_hex_encode_binary(out_tmp, sizeof(out_tmp), output);    \
    } while (0)
#endif

/**
 * @}
 */

/**
 * \defgroup vlc_hash_md5  MD5 hashing
 * APIs to hash data using the Message-Digest Algorithm 5 (MD5)
 * @{
 */

/**
 * MD5 hash context
 */
typedef struct vlc_hash_md5_ctx
{
    struct md5_s {
        uint32_t A, B, C, D; /* chaining variables */
        uint32_t nblocks;
        uint8_t buf[64];
        int count;
    } priv; /**< \internal Private */
} vlc_hash_md5_t;

/**
 * MD5 digest output size
 */
#define VLC_HASH_MD5_DIGEST_SIZE 16

/**
 * MD5 digest hex representation size
 */
#define VLC_HASH_MD5_DIGEST_HEX_SIZE 33 // 2 chars per byte + null

/**
 * Initialize MD5 context
 * 
 * Initializes the given MD5 hash function context, if the context is
 * already initialized, it is reset.
 * 
 * \param[out] ctx  MD5 hash context to init
 */
VLC_API void vlc_hash_md5_Init(vlc_hash_md5_t *ctx);

/**
 * Update MD5 hash computation with new data
 * 
 * Updates the context with provided data which is used for the hash
 * calculation. Can be called repeatedly with new data. The final
 * hash represents the hash for the concatenation of all data.
 * 
 * \param[in,out] ctx    MD5 hash context to update
 * \param         data   Data to add
 * \param         size   Size of the data to add
 */
VLC_API void vlc_hash_md5_Update(vlc_hash_md5_t *ctx, const void *data, size_t size);

/**
 * Finish MD5 hash computation
 * 
 * Finishes the MD5 hash computation and provides the hash for the
 * concatenation of all provided data by previous calls to \ref vlc_hash_md5_Update.
 * The result is written to the buffer pointed to by output, which must be at
 * least \ref VLC_HASH_MD5_DIGEST_SIZE big.
 * 
 * \param[in,out] ctx    MD5 hash context to finish
 * \param[out]    output Output buffer to write to
 * \param         size   Output buffer size
 */
VLC_API void vlc_hash_md5_Finish(vlc_hash_md5_t *ctx, void *output, size_t size);

/**
 * @}
 */

/**
 * @}
 */

#endif
