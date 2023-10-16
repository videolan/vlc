/*****************************************************************************
 * storage.c
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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
#include "config.h"
#endif

#include <assert.h>
#include <fcntl.h>

#include <unistd.h>     /* close() */

#include <vlc_common.h>

#include <vlc_block.h>
#include <vlc_fs.h>

#include "hls.h"
#include "storage.h"

struct storage_priv
{
    hls_storage_t storage;
    void (*destroy)(struct storage_priv *storage);
    size_t size;

    union
    {
        struct
        {
            block_t *content;
        } mem;

        struct
        {
            char *path;
        } fs;
    };
};

static void mem_storage_Destroy(struct storage_priv *priv)
{
    block_ChainRelease(priv->mem.content);
    free(priv);
}

static ssize_t mem_storage_GetContent(const hls_storage_t *storage,
                                      uint8_t **dest)
{
    const struct storage_priv *priv =
        container_of(storage, struct storage_priv, storage);

    *dest = malloc(priv->size);
    if (unlikely(*dest == NULL))
        return -1;

    uint8_t *cursor = *dest;
    for (const block_t *it = priv->mem.content; it != NULL; it = it->p_next)
    {
        memcpy(cursor, it->p_buffer, it->i_buffer);
        cursor += it->i_buffer;
    }
    return priv->size;
}

static hls_storage_t *mem_storage_FromBlock(block_t *content)
{
    struct storage_priv *priv = malloc(sizeof(*priv));
    if (unlikely(priv == NULL))
        return NULL;

    priv->storage.get_content = mem_storage_GetContent;
    priv->destroy = mem_storage_Destroy;
    priv->mem.content = content;
    block_ChainProperties(content, NULL, &priv->size, NULL);
    return &priv->storage;
}

static hls_storage_t *mem_storage_FromBytes(void *bytes, size_t size)
{
    struct storage_priv *priv = malloc(sizeof(*priv));
    if (unlikely(priv == NULL))
        return NULL;

    block_t *content = block_heap_Alloc(bytes, size);
    if (unlikely(content == NULL))
    {
        free(priv);
        return NULL;
    }

    priv->storage.get_content = mem_storage_GetContent;
    priv->destroy = mem_storage_Destroy;
    priv->size = size;
    priv->mem.content = content;
    return &priv->storage;
}

static ssize_t fs_storage_Read(int fd, uint8_t buf[], size_t len)
{
    size_t total = 0;
    while (total < len)
    {
        const ssize_t n = read(fd, buf + total, len - total);
        if (n == -1)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return -1;
        }
        else if (n == 0)
            break;

        total += n;
    }
    return total;
}

static ssize_t fs_storage_GetContent(const hls_storage_t *storage,
                                     uint8_t **dest)
{
    const struct storage_priv *priv =
        container_of(storage, struct storage_priv, storage);

    const int fd = vlc_open(priv->fs.path, O_RDONLY);

    if (fd == -1)
        return -1;

    *dest = malloc(priv->size);
    if (unlikely(*dest == NULL))
        goto err;

    const ssize_t read = fs_storage_Read(fd, *dest, priv->size);
    if (read == -1)
        goto err;

    close(fd);
    return read;
err:
    free(dest);
    close(fd);
    return -1;
}

static int fs_storage_Write(int fd, const uint8_t *data, size_t len)
{
    size_t written = 0;
    while (written < len)
    {
        const ssize_t n = vlc_write(fd, data + written, len - written);
        if (n == -1)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return VLC_EGENERIC;
        }

        written += n;
    }
    return VLC_SUCCESS;
}

static void fs_storage_Destroy(struct storage_priv *priv)
{
    free(priv->fs.path);
    free(priv);
}

static inline char *fs_storage_CreatePath(const char *outdir,
                                          const char *storage_name)
{
    char *ret;

    if (asprintf(&ret, "%s/%s", outdir, storage_name) == -1)
        return NULL;
    return ret;
}

static hls_storage_t *
fs_storage_FromBlock(block_t *content,
                     const struct hls_storage_config *config,
                     const struct hls_config *hls_config)
{
    struct storage_priv *priv = malloc(sizeof(*priv));
    if (unlikely(priv == NULL))
        goto err;

    priv->fs.path = fs_storage_CreatePath(hls_config->outdir, config->name);
    if (unlikely(priv->fs.path == NULL))
        goto err;

    const int fd = vlc_open(priv->fs.path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1)
        goto err;

    size_t size = 0;
    for (const block_t *it = content; it != NULL; it = it->p_next)
    {
        const int status = fs_storage_Write(fd, it->p_buffer, it->i_buffer);
        if (status != VLC_SUCCESS)
        {
            close(fd);
            goto err;
        }
        size += it->i_buffer;
    }

    close(fd);
    block_ChainRelease(content);

    priv->storage.get_content = fs_storage_GetContent;
    priv->size = size;
    priv->destroy = fs_storage_Destroy;

    return &priv->storage;
err:
    block_ChainRelease(content);
    if (priv != NULL)
        free(priv->fs.path);
    free(priv);
    return NULL;
}

static hls_storage_t *
fs_storage_FromBytes(void *bytes,
                     size_t size,
                     const struct hls_storage_config *config,
                     const struct hls_config *hls_config)
{
    struct storage_priv *priv = malloc(sizeof(*priv));
    if (unlikely(priv == NULL))
        return NULL;

    priv->fs.path = fs_storage_CreatePath(hls_config->outdir, config->name);
    if (unlikely(priv->fs.path == NULL))
        goto err;

    const int fd = vlc_open(priv->fs.path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1)
        goto err;

    const int status = fs_storage_Write(fd, bytes, size);
    close(fd);

    if (unlikely(status != VLC_SUCCESS))
        goto err;

    priv->storage.get_content = fs_storage_GetContent;
    priv->size = size;
    priv->destroy = fs_storage_Destroy;

    free(bytes);
    return &priv->storage;
err:
    free(bytes);
    if (priv != NULL)
        free(priv->fs.path);
    free(priv);
    return NULL;
}

hls_storage_t *hls_storage_FromBlocks(block_t *content,
                                      const struct hls_storage_config *config,
                                      const struct hls_config *hls_config)
{
    hls_storage_t *storage;
    if (hls_config_IsMemStorageEnabled(hls_config))
        storage = mem_storage_FromBlock(content);
    else
        storage = fs_storage_FromBlock(content, config, hls_config);

    if (storage != NULL)
        storage->mime = config->mime;
    return storage;
}

hls_storage_t *hls_storage_FromBytes(void *data,
                                     size_t size,
                                     const struct hls_storage_config *config,
                                     const struct hls_config *hls_config)
{
    hls_storage_t *storage;
    if (hls_config_IsMemStorageEnabled(hls_config))
        storage = mem_storage_FromBytes(data, size);
    else
        storage = fs_storage_FromBytes(data, size, config, hls_config);

    if (storage != NULL)
        storage->mime = config->mime;
    return storage;
}

size_t hls_storage_GetSize(const hls_storage_t *storage)
{
    const struct storage_priv *priv =
        container_of(storage, struct storage_priv, storage);
    return priv->size;
}

void hls_storage_Destroy(hls_storage_t *storage)
{
    struct storage_priv *priv =
        container_of(storage, struct storage_priv, storage);
    priv->destroy(priv);
}
