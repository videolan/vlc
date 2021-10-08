/*****************************************************************************
 * frame.c: frames management functions
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 * Copyright (C) 2007-2009 Rémi Denis-Courmont
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
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

#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <vlc_common.h>
#include <vlc_frame.h>
#include <vlc_fs.h>

#ifndef NDEBUG
static void vlc_frame_Check (vlc_frame_t *frame)
{
    while (frame != NULL)
    {
        unsigned char *start = frame->p_start;
        unsigned char *end = frame->p_start + frame->i_size;
        unsigned char *bufstart = frame->p_buffer;
        unsigned char *bufend = frame->p_buffer + frame->i_buffer;

        assert (start <= end);
        assert (bufstart <= bufend);
        assert (bufstart >= start);
        assert (bufend <= end);

        frame = frame->p_next;
    }
}
#else
# define vlc_frame_Check(b) ((void)(b))
#endif

void vlc_frame_CopyProperties(vlc_frame_t *dst, const vlc_frame_t *src)
{
    dst->i_flags   = src->i_flags;
    dst->i_nb_samples = src->i_nb_samples;
    dst->i_dts     = src->i_dts;
    dst->i_pts     = src->i_pts;
    dst->i_length  = src->i_length;
}

vlc_frame_t *vlc_frame_Init(vlc_frame_t *restrict f, const struct vlc_frame_callbacks *cbs,
                            void *buf, size_t size)
{
    /* Fill all fields to their default */
    f->p_next = NULL;
    f->p_buffer = buf;
    f->i_buffer = size;
    f->p_start = buf;
    f->i_size = size;
    f->i_flags = 0;
    f->i_nb_samples = 0;
    f->i_pts =
    f->i_dts = VLC_TICK_INVALID;
    f->i_length = 0;
    f->cbs = cbs;
    return f;
}

static void vlc_frame_generic_Release (vlc_frame_t *frame)
{
    /* That is always true for frames allocated with vlc_frame_Alloc(). */
    assert (frame->p_start == (unsigned char *)(frame + 1));
    free (frame);
}

static const struct vlc_frame_callbacks vlc_frame_generic_cbs =
{
    vlc_frame_generic_Release,
};

/** Initial memory alignment of data frame.
 * @note This must be a multiple of sizeof(void*) and a power of two.
 * libavcodec AVX optimizations require at least 32-bytes. */
#define VLC_FRAME_ALIGN        32

/** Initial reserved header and footer size. */
#define VLC_FRAME_PADDING      32

vlc_frame_t *vlc_frame_Alloc (size_t size)
{
    if (unlikely(size >> 28))
    {
        errno = ENOBUFS;
        return NULL;
    }

    /* 2 * VLC_FRAME_PADDING: pre + post padding */
    const size_t alloc = sizeof (vlc_frame_t) + VLC_FRAME_ALIGN + (2 * VLC_FRAME_PADDING)
                       + size;
    if (unlikely(alloc <= size))
        return NULL;

    vlc_frame_t *f = malloc (alloc);
    if (unlikely(f == NULL))
        return NULL;

    vlc_frame_Init(f, &vlc_frame_generic_cbs, f + 1, alloc - sizeof (*f));
    static_assert ((VLC_FRAME_PADDING % VLC_FRAME_ALIGN) == 0,
                   "VLC_FRAME_PADDING must be a multiple of VLC_FRAME_ALIGN");
    f->p_buffer += VLC_FRAME_PADDING + VLC_FRAME_ALIGN - 1;
    f->p_buffer = (void *)(((uintptr_t)f->p_buffer) & ~(VLC_FRAME_ALIGN - 1));
    f->i_buffer = size;
    return f;
}

void vlc_frame_Release(vlc_frame_t *frame)
{
#ifndef NDEBUG
    frame->p_next = NULL;
    vlc_frame_Check (frame);
#endif
    frame->cbs->free(frame);
}

static vlc_frame_t *vlc_frame_ReallocDup( vlc_frame_t *frame, ssize_t i_prebody, size_t requested )
{
    vlc_frame_t *p_rea = vlc_frame_Alloc( requested );
    if( p_rea == NULL )
        return NULL;

    if( frame->i_buffer > 0 )
        memcpy( p_rea->p_buffer + i_prebody, frame->p_buffer, frame->i_buffer );

    p_rea->p_next = frame->p_next;
    vlc_frame_CopyProperties( p_rea, frame );

    vlc_frame_Release( frame );
    return p_rea;
}

vlc_frame_t *vlc_frame_TryRealloc (vlc_frame_t *frame, ssize_t i_prebody, size_t i_body)
{
    vlc_frame_Check( frame );

    /* Corner case: empty frame requested */
    if( i_prebody <= 0 && i_body <= (size_t)(-i_prebody) )
        i_prebody = i_body = 0;

    assert( frame->p_start <= frame->p_buffer );
    assert( frame->p_start + frame->i_size
                                    >= frame->p_buffer + frame->i_buffer );

    /* First, shrink payload */

    /* Pull payload start */
    if( i_prebody < 0 )
    {
        if( frame->i_buffer >= (size_t)-i_prebody )
        {
            frame->p_buffer -= i_prebody;
            frame->i_buffer += i_prebody;
        }
        else /* Discard current payload entirely */
            frame->i_buffer = 0;
        i_body += i_prebody;
        i_prebody = 0;
    }

    /* Trim payload end */
    if( frame->i_buffer > i_body )
        frame->i_buffer = i_body;

    size_t requested = i_prebody + i_body;

    if( frame->i_buffer == 0 )
    {   /* Corner case: nothing to preserve */
        if( requested <= frame->i_size )
        {   /* Enough room: recycle buffer */
            size_t extra = frame->i_size - requested;

            frame->p_buffer = frame->p_start + (extra / 2);
            frame->i_buffer = requested;
            return frame;
        }

        /* Not enough room: allocate a new buffer */
        return vlc_frame_ReallocDup( frame, i_prebody, requested );
    }

    uint8_t *p_start = frame->p_start;
    uint8_t *p_end = p_start + frame->i_size;

    /* Second, reallocate the buffer if we lack space. */
    assert( i_prebody >= 0 );
    if( (size_t)(frame->p_buffer - p_start) < (size_t)i_prebody
     || (size_t)(p_end - frame->p_buffer) < i_body )
        return vlc_frame_ReallocDup( frame, i_prebody, requested );

    /* Third, expand payload */

    /* Push payload start */
    if( i_prebody > 0 )
    {
        frame->p_buffer -= i_prebody;
        frame->i_buffer += i_prebody;
        i_body += i_prebody;
        i_prebody = 0;
    }

    /* Expand payload to requested size */
    frame->i_buffer = i_body;

    return frame;
}

vlc_frame_t *vlc_frame_Realloc (vlc_frame_t *frame, ssize_t prebody, size_t body)
{
    vlc_frame_t *rea = vlc_frame_TryRealloc (frame, prebody, body);
    if (rea == NULL)
        vlc_frame_Release(frame);
    return rea;
}

static void vlc_frame_heap_Release (vlc_frame_t *frame)
{
    free (frame->p_start);
    free (frame);
}

static const struct vlc_frame_callbacks vlc_frame_heap_cbs =
{
    vlc_frame_heap_Release,
};

vlc_frame_t *vlc_frame_heap_Alloc (void *addr, size_t length)
{
    vlc_frame_t *frame = malloc (sizeof (*frame));
    if (frame == NULL)
    {
        free (addr);
        return NULL;
    }

    return vlc_frame_Init(frame, &vlc_frame_heap_cbs, addr, length);
}

#ifdef HAVE_MMAP
# include <sys/mman.h>

static void vlc_frame_mmap_Release (vlc_frame_t *frame)
{
    munmap (frame->p_start, frame->i_size);
    free (frame);
}

static const struct vlc_frame_callbacks vlc_frame_mmap_cbs =
{
    vlc_frame_mmap_Release,
};

vlc_frame_t *vlc_frame_mmap_Alloc (void *addr, size_t length)
{
    if (addr == MAP_FAILED)
        return NULL;

    long page_mask = sysconf(_SC_PAGESIZE) - 1;
    size_t left = ((uintptr_t)addr) & page_mask;
    size_t right = (-length) & page_mask;

    vlc_frame_t *frame = malloc (sizeof (*frame));
    if (frame == NULL)
    {
        munmap (addr, length);
        return NULL;
    }

    vlc_frame_Init(frame, &vlc_frame_mmap_cbs,
               ((char *)addr) - left, left + length + right);
    frame->p_buffer = addr;
    frame->i_buffer = length;
    return frame;
}
#else
vlc_frame_t *vlc_frame_mmap_Alloc (void *addr, size_t length)
{
    (void)addr; (void)length; return NULL;
}
#endif
#if defined(_WIN32)
struct vlc_frame_mv
{
    vlc_frame_t b;
    HANDLE  hMap;
};

static void vlc_frame_mapview_Release (vlc_frame_t *frame)
{
    struct vlc_frame_mv *mvframe = container_of(frame, struct vlc_frame_mv, b);

    UnmapViewOfFile(frame->p_start);
    CloseHandle(mvframe->hMap);
    free(mvframe);
}

static const struct vlc_frame_callbacks vlc_frame_mapview_cbs =
{
    vlc_frame_mapview_Release,
};

static vlc_frame_t *vlc_frame_mapview_Alloc(HANDLE hMap, void *addr, size_t length)
{
    struct vlc_frame_mv *mvframe = malloc (sizeof (*mvframe));
    if (unlikely(mvframe == NULL))
    {
        UnmapViewOfFile(addr);
        CloseHandle(hMap);
        return NULL;
    }
    mvframe->hMap = hMap;

    vlc_frame_t *frame = &mvframe->b;
    vlc_frame_Init(frame, &vlc_frame_mapview_cbs, addr, length);
    return frame;
}
#endif

#ifdef HAVE_SYS_SHM_H
# include <sys/shm.h>

static void vlc_frame_shm_Release (vlc_frame_t *frame)
{
    shmdt(frame->p_start);
    free(frame);
}

static const struct vlc_frame_callbacks vlc_frame_shm_cbs =
{
    vlc_frame_shm_Release,
};

vlc_frame_t *vlc_frame_shm_Alloc (void *addr, size_t length)
{
    vlc_frame_t *frame = malloc (sizeof (*frame));
    if (unlikely(frame == NULL))
    {
        shmdt (addr);
        return NULL;
    }

    return vlc_frame_Init(frame, &vlc_frame_shm_cbs, (uint8_t *)addr, length);
}
#else
vlc_frame_t *vlc_frame_shm_Alloc (void *addr, size_t length)
{
    (void) addr; (void) length;
    abort ();
}
#endif


#ifdef _WIN32
# include <io.h>

static
ssize_t pread (int fd, void *buf, size_t count, off_t offset)
{
    HANDLE handle = (HANDLE)(intptr_t)_get_osfhandle (fd);
    if (handle == INVALID_HANDLE_VALUE)
        return -1;

    OVERLAPPED olap = {.Offset = offset, .OffsetHigh = (offset >> 32)};
    DWORD written;
    /* This braindead API will override the file pointer even if we specify
     * an explicit read offset... So do not expect this to mix well with
     * regular read() calls. */
    if (ReadFile (handle, buf, count, &written, &olap))
        return written;
    return -1;
}
#endif

vlc_frame_t *vlc_frame_File(int fd, bool write)
{
    size_t length;
    struct stat st;

    /* First, get the file size */
    if (fstat (fd, &st))
        return NULL;

    /* st_size is meaningful for regular files, shared memory and typed memory.
     * It's also meaning for symlinks, but that's not possible with fstat().
     * In other cases, it's undefined, and we should really not go further. */
#ifndef S_TYPEISSHM
# define S_TYPEISSHM( buf ) (0)
#endif
    if (S_ISDIR (st.st_mode))
    {
        errno = EISDIR;
        return NULL;
    }
    if (!S_ISREG (st.st_mode) && !S_TYPEISSHM (&st))
    {
        errno = ESPIPE;
        return NULL;
    }

    /* Prevent an integer overflow in mmap() and malloc() */
    if ((uintmax_t)st.st_size >= SIZE_MAX)
    {
        errno = ENOMEM;
        return NULL;
    }
    length = (size_t)st.st_size;

#ifdef HAVE_MMAP
    if (length > 0)
    {
        int prot = PROT_READ | (write ? PROT_WRITE : 0);
        int flags = write ? MAP_PRIVATE : MAP_SHARED;
        void *addr = mmap(NULL, length, prot, flags, fd, 0);

        if (addr != MAP_FAILED)
            return vlc_frame_mmap_Alloc (addr, length);
    }
#elif defined(_WIN32)
    if (length > 0)
    {
        HANDLE handle = (HANDLE)(intptr_t)_get_osfhandle (fd);
        if (handle != INVALID_HANDLE_VALUE)
        {
            void *addr = NULL;
            HANDLE hMap;
            DWORD prot = write ? PAGE_READWRITE : PAGE_READONLY;
            DWORD access = FILE_MAP_READ | (write ? FILE_MAP_WRITE : 0);
#ifdef VLC_WINSTORE_APP
            hMap = CreateFileMappingFromApp(handle, NULL, prot, length, NULL);
            if (hMap != INVALID_HANDLE_VALUE)
                addr = MapViewOfFileFromApp(hMap, access, 0, length);
#else
            hMap = CreateFileMapping(handle, NULL, prot, 0, length, NULL);
            if (hMap != INVALID_HANDLE_VALUE)
                addr = MapViewOfFile(hMap, access, 0, 0, length);
#endif

            if (addr != NULL)
                return vlc_frame_mapview_Alloc(hMap, addr, length);

            CloseHandle(hMap);
        }
    }
#else
    (void) write;
#endif

    /* If mmap() is not implemented by the OS _or_ the filesystem... */
    vlc_frame_t *frame = vlc_frame_Alloc (length);
    if (frame == NULL)
        return NULL;
    vlc_frame_cleanup_push (frame);

    for (size_t i = 0; i < length;)
    {
        ssize_t len = pread (fd, frame->p_buffer + i, length - i, i);
        if (len == -1)
        {
            vlc_frame_Release (frame);
            frame = NULL;
            break;
        }
        i += len;
    }
    vlc_cleanup_pop ();
    return frame;
}

vlc_frame_t *vlc_frame_FilePath(const char *path, bool write)
{
    /* NOTE: Writeable shared mappings are not supported here. So there are no
     * needs to open the file for writing (even if the mapping is writable). */
    int fd = vlc_open (path, O_RDONLY);
    if (fd == -1)
        return NULL;

    vlc_frame_t *frame = vlc_frame_File(fd, write);
    vlc_close (fd);
    return frame;
}
