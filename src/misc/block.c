/*****************************************************************************
 * block.c: Data blocks management functions
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * Copyright (C) 2007-2009 Rémi Denis-Courmont
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <sys/stat.h>
#include <assert.h>
#include "vlc_block.h"

/**
 * @section Block handling functions.
 */

/**
 * Internal state for heap block.
  */
struct block_sys_t
{
    block_t     self;
    size_t      i_allocated_buffer;
    uint8_t     p_allocated_buffer[];
};

#ifndef NDEBUG
static void BlockNoRelease( block_t *b )
{
    fprintf( stderr, "block %p has no release callback! This is a bug!\n", b );
    abort();
}
#endif

void block_Init( block_t *restrict b, void *buf, size_t size )
{
    /* Fill all fields to their default */
    b->p_next = NULL;
    b->i_flags = 0;
    b->i_pts =
    b->i_dts = VLC_TS_INVALID;
    b->i_length = 0;
    b->i_rate = 0;
    b->p_buffer = buf;
    b->i_buffer = size;
#ifndef NDEBUG
    b->pf_release = BlockNoRelease;
#endif
}

static void BlockRelease( block_t *p_block )
{
    free( p_block );
}

static void BlockMetaCopy( block_t *restrict out, const block_t *in )
{
    out->i_dts     = in->i_dts;
    out->i_pts     = in->i_pts;
    out->i_flags   = in->i_flags;
    out->i_length  = in->i_length;
    out->i_rate    = in->i_rate;
    out->i_samples = in->i_samples;
}

/* Memory alignment */
#define BLOCK_ALIGN        16
/* Initial size of reserved header and footer */
#define BLOCK_PADDING_SIZE 32
/* Maximum size of reserved footer before we release with realloc() */
#define BLOCK_WASTE_SIZE   2048

block_t *block_Alloc( size_t i_size )
{
    /* We do only one malloc
     * TODO: bench if doing 2 malloc but keeping a pool of buffer is better
     * TODO: use memalign
     * 16 -> align on 16
     * 2 * BLOCK_PADDING_SIZE -> pre + post padding
     */
    const size_t i_alloc = i_size + 2 * BLOCK_PADDING_SIZE + BLOCK_ALIGN;
    block_sys_t *p_sys = malloc( sizeof( *p_sys ) + i_alloc );

    if( p_sys == NULL )
        return NULL;

    /* Fill opaque data */
    p_sys->i_allocated_buffer = i_alloc;

    block_Init( &p_sys->self, p_sys->p_allocated_buffer + BLOCK_PADDING_SIZE
                + BLOCK_ALIGN
                - ((uintptr_t)p_sys->p_allocated_buffer % BLOCK_ALIGN),
                i_size );
    p_sys->self.pf_release    = BlockRelease;

    return &p_sys->self;
}

block_t *block_Realloc( block_t *p_block, ssize_t i_prebody, size_t i_body )
{
    block_sys_t *p_sys = (block_sys_t *)p_block;
    size_t requested = i_prebody + i_body;

    /* Corner case: empty block requested */
    if( i_prebody <= 0 && i_body <= (size_t)(-i_prebody) )
    {
        block_Release( p_block );
        return NULL;
    }

    if( p_block->pf_release != BlockRelease )
    {
        /* Special case when pf_release if overloaded
         * TODO if used one day, then implement it in a smarter way */
        block_t *p_dup = block_Duplicate( p_block );
        block_Release( p_block );
        if( !p_dup )
            return NULL;

        p_block = p_dup;
        p_sys = (block_sys_t *)p_block;
    }

    uint8_t *p_start = p_sys->p_allocated_buffer;
    uint8_t *p_end = p_sys->p_allocated_buffer + p_sys->i_allocated_buffer;

    assert( p_block->p_buffer + p_block->i_buffer <= p_end );
    assert( p_block->p_buffer >= p_start );

    /* Corner case: the current payload is discarded completely */
    if( i_prebody <= 0 && p_block->i_buffer <= (size_t)-i_prebody )
         p_block->i_buffer = 0; /* discard current payload */
    if( p_block->i_buffer == 0 )
    {
        size_t available = p_end - p_start;

        if( requested <= available )
        {   /* Enough room: recycle buffer */
            size_t extra = available - requested;

            p_block->p_buffer = p_start + (extra / 2);
            p_block->i_buffer = requested;
            return p_block;
        }
        /* Not enough room: allocate a new buffer */
        block_t *p_rea = block_Alloc( requested );
        if( p_rea )
            BlockMetaCopy( p_rea, p_block );
        block_Release( p_block );
        return p_rea;
    }

    /* First, shrink payload */

    /* Pull payload start */
    if( i_prebody < 0 )
    {
        assert( p_block->i_buffer >= (size_t)-i_prebody );
        p_block->p_buffer -= i_prebody;
        p_block->i_buffer += i_prebody;
        i_body += i_prebody;
        i_prebody = 0;
    }

    /* Trim payload end */
    if( p_block->i_buffer > i_body )
        p_block->i_buffer = i_body;

    /* Second, reallocate the buffer if we lack space. This is done now to
     * minimize the payload size for memory copy. */
    assert( i_prebody >= 0 );
    if( (size_t)(p_block->p_buffer - p_start) < (size_t)i_prebody
     || (size_t)(p_end - p_block->p_buffer) < p_block->i_buffer + i_body )
    {
        /* FIXME: this is really dumb, we should use realloc() */
        block_t *p_rea = block_Alloc( requested );
        if( p_rea )
        {
            BlockMetaCopy( p_rea, p_block );
            p_rea->p_buffer += i_prebody;
            p_rea->i_buffer -= i_prebody;
            memcpy( p_rea->p_buffer, p_block->p_buffer, p_block->i_buffer );
        }
        block_Release( p_block );
        p_block = p_rea;
    }
    else
    /* We have a very large reserved footer now? Release some of it.
     * XXX it might not preserve the alignment of p_buffer */
    if( p_end - (p_block->p_buffer + i_body) > BLOCK_WASTE_SIZE )
    {
        const ptrdiff_t i_prebody = p_block->p_buffer - p_start;
        const size_t i_new = requested + 1 * BLOCK_PADDING_SIZE;
        block_sys_t *p_new = realloc( p_sys, sizeof (*p_sys) + i_new );

        if( p_new != NULL )
        {
            p_sys = p_new;
            p_sys->i_allocated_buffer = i_new;
            p_block = &p_sys->self;
            p_block->p_buffer = &p_sys->p_allocated_buffer[i_prebody];
        }
    }

    /* NOTE: p_start and p_end are corrupted from this point */

    /* Third, expand payload */

    /* Push payload start */
    if( i_prebody > 0 )
    {
        p_block->p_buffer -= i_prebody;
        p_block->i_buffer += i_prebody;
        i_body += i_prebody;
        i_prebody = 0;
    }

    /* Expand payload to requested size */
    p_block->i_buffer = i_body;

    return p_block;
}


typedef struct
{
    block_t  self;
    void    *mem;
} block_heap_t;

static void block_heap_Release (block_t *self)
{
    block_heap_t *block = (block_heap_t *)self;

    free (block->mem);
    free (block);
}

/**
 * Creates a block from a heap allocation.
 * This is provided by LibVLC so that manually heap-allocated blocks can safely
 * be deallocated even after the origin plugin has been unloaded from memory.
 *
 * When block_Release() is called, VLC will free() the specified pointer.
 *
 * @param ptr base address of the heap allocation (will be free()'d)
 * @param addr base address of the useful buffer data
 * @param length bytes length of the useful buffer datan
 * @return NULL in case of error (ptr free()'d in that case), or a valid
 * block_t pointer.
 */
block_t *block_heap_Alloc (void *ptr, void *addr, size_t length)
{
    block_heap_t *block = malloc (sizeof (*block));
    if (block == NULL)
    {
        free (addr);
        return NULL;
    }

    block_Init (&block->self, (uint8_t *)addr, length);
    block->self.pf_release = block_heap_Release;
    block->mem = ptr;
    return &block->self;
}

#ifdef HAVE_MMAP
# include <sys/mman.h>

typedef struct block_mmap_t
{
    block_t     self;
    void       *base_addr;
    size_t      length;
} block_mmap_t;

static void block_mmap_Release (block_t *block)
{
    block_mmap_t *p_sys = (block_mmap_t *)block;

    munmap (p_sys->base_addr, p_sys->length);
    free (p_sys);
}

/**
 * Creates a block from a virtual address memory mapping (mmap).
 * This is provided by LibVLC so that mmap blocks can safely be deallocated
 * even after the allocating plugin has been unloaded from memory.
 *
 * @param addr base address of the mapping (as returned by mmap)
 * @param length length (bytes) of the mapping (as passed to mmap)
 * @return NULL if addr is MAP_FAILED, or an error occurred (in the later
 * case, munmap(addr, length) is invoked before returning).
 */
block_t *block_mmap_Alloc (void *addr, size_t length)
{
    if (addr == MAP_FAILED)
        return NULL;

    block_mmap_t *block = malloc (sizeof (*block));
    if (block == NULL)
    {
        munmap (addr, length);
        return NULL;
    }

    block_Init (&block->self, (uint8_t *)addr, length);
    block->self.pf_release = block_mmap_Release;
    block->base_addr = addr;
    block->length = length;
    return &block->self;
}
#else
block_t *block_mmap_Alloc (void *addr, size_t length)
{
    (void)addr; (void)length; return NULL;
}
#endif


#ifdef WIN32
#ifdef UNDER_CE
#define _get_osfhandle(a) ((long) (a))
#endif

static
ssize_t pread (int fd, void *buf, size_t count, off_t offset)
{
    HANDLE handle = (HANDLE)(intptr_t)_get_osfhandle (fd);
    if (handle == INVALID_HANDLE_VALUE)
        return -1;

    OVERLAPPED olap; olap.Offset = offset; olap.OffsetHigh = (offset >> 32);
    DWORD written;
    /* This braindead API will override the file pointer even if we specify
     * an explicit read offset... So do not expect this to mix well with
     * regular read() calls. */
    if (ReadFile (handle, buf, count, &written, &olap))
        return written;
    return -1;
}
#endif

/**
 * Loads a file into a block of memory. If possible a private file mapping is
 * created. Otherwise, the file is read normally. On 32-bits platforms, this
 * function will not work for very large files, due to memory space
 * constraints. Cancellation point.
 *
 * @param fd file descriptor to load from
 * @return a new block with the file content at p_buffer, and file length at
 * i_buffer (release it with block_Release()), or NULL upon error (see errno).
 */
block_t *block_File (int fd)
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
    if (st.st_size >= SIZE_MAX)
    {
        errno = ENOMEM;
        return NULL;
    }
    length = (size_t)st.st_size;

#ifdef HAVE_MMAP
    if (length > 0)
    {
        void *addr;

        addr = mmap (NULL, length, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
        if (addr != MAP_FAILED)
            return block_mmap_Alloc (addr, length);
    }
#endif

    /* If mmap() is not implemented by the OS _or_ the filesystem... */
    block_t *block = block_Alloc (length);
    if (block == NULL)
        return NULL;
    block_cleanup_push (block);

    for (size_t i = 0; i < length;)
    {
        ssize_t len = pread (fd, block->p_buffer + i, length - i, i);
        if (len == -1)
        {
            block_Release (block);
            block = NULL;
            break;
        }
        i += len;
    }
    vlc_cleanup_pop ();
    return block;
}

/**
 * @section Thread-safe block queue functions
 */

/**
 * Internal state for block queues
 */
struct block_fifo_t
{
    vlc_mutex_t         lock;                         /* fifo data lock */
    vlc_cond_t          wait;      /**< Wait for data */
    vlc_cond_t          wait_room; /**< Wait for queue depth to shrink */

    block_t             *p_first;
    block_t             **pp_last;
    size_t              i_depth;
    size_t              i_size;
    bool          b_force_wake;
};

block_fifo_t *block_FifoNew( void )
{
    block_fifo_t *p_fifo = malloc( sizeof( block_fifo_t ) );
    if( !p_fifo )
        return NULL;

    vlc_mutex_init( &p_fifo->lock );
    vlc_cond_init( &p_fifo->wait );
    vlc_cond_init( &p_fifo->wait_room );
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
    p_fifo->i_depth = p_fifo->i_size = 0;
    p_fifo->b_force_wake = false;

    return p_fifo;
}

void block_FifoRelease( block_fifo_t *p_fifo )
{
    block_FifoEmpty( p_fifo );
    vlc_cond_destroy( &p_fifo->wait_room );
    vlc_cond_destroy( &p_fifo->wait );
    vlc_mutex_destroy( &p_fifo->lock );
    free( p_fifo );
}

void block_FifoEmpty( block_fifo_t *p_fifo )
{
    block_t *b;

    vlc_mutex_lock( &p_fifo->lock );
    for( b = p_fifo->p_first; b != NULL; )
    {
        block_t *p_next;

        p_next = b->p_next;
        block_Release( b );
        b = p_next;
    }

    p_fifo->i_depth = p_fifo->i_size = 0;
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
    vlc_cond_broadcast( &p_fifo->wait_room );
    vlc_mutex_unlock( &p_fifo->lock );
}

/**
 * Wait until the FIFO gets below a certain size (if needed).
 *
 * Note that if more than one thread writes to the FIFO, you cannot assume that
 * the FIFO is actually below the requested size upon return (since another
 * thread could have refilled it already). This is typically not an issue, as
 * this function is meant for (relaxed) congestion control.
 *
 * This function may be a cancellation point and it is cancel-safe.
 *
 * @param fifo queue to wait on
 * @param max_depth wait until the queue has no more than this many blocks
 *                  (use SIZE_MAX to ignore this constraint)
 * @param max_size wait until the queue has no more than this many bytes
 *                  (use SIZE_MAX to ignore this constraint)
 * @return nothing.
 */
void block_FifoPace (block_fifo_t *fifo, size_t max_depth, size_t max_size)
{
    vlc_testcancel ();

    vlc_mutex_lock (&fifo->lock);
    while ((fifo->i_depth > max_depth) || (fifo->i_size > max_size))
    {
         mutex_cleanup_push (&fifo->lock);
         vlc_cond_wait (&fifo->wait_room, &fifo->lock);
         vlc_cleanup_pop ();
    }
    vlc_mutex_unlock (&fifo->lock);
}

/**
 * Immediately queue one block at the end of a FIFO.
 * @param fifo queue
 * @param block head of a block list to queue (may be NULL)
 */
size_t block_FifoPut( block_fifo_t *p_fifo, block_t *p_block )
{
    size_t i_size = 0;
    vlc_mutex_lock( &p_fifo->lock );

    while (p_block != NULL)
    {
        i_size += p_block->i_buffer;

        *p_fifo->pp_last = p_block;
        p_fifo->pp_last = &p_block->p_next;
        p_fifo->i_depth++;
        p_fifo->i_size += p_block->i_buffer;

        p_block = p_block->p_next;
    }

    /* We queued one block: wake up one read-waiting thread */
    vlc_cond_signal( &p_fifo->wait );
    vlc_mutex_unlock( &p_fifo->lock );

    return i_size;
}

void block_FifoWake( block_fifo_t *p_fifo )
{
    vlc_mutex_lock( &p_fifo->lock );
    if( p_fifo->p_first == NULL )
        p_fifo->b_force_wake = true;
    vlc_cond_broadcast( &p_fifo->wait );
    vlc_mutex_unlock( &p_fifo->lock );
}

block_t *block_FifoGet( block_fifo_t *p_fifo )
{
    block_t *b;

    vlc_testcancel( );

    vlc_mutex_lock( &p_fifo->lock );
    mutex_cleanup_push( &p_fifo->lock );

    /* Remember vlc_cond_wait() may cause spurious wakeups
     * (on both Win32 and POSIX) */
    while( ( p_fifo->p_first == NULL ) && !p_fifo->b_force_wake )
        vlc_cond_wait( &p_fifo->wait, &p_fifo->lock );

    vlc_cleanup_pop();
    b = p_fifo->p_first;

    p_fifo->b_force_wake = false;
    if( b == NULL )
    {
        /* Forced wakeup */
        vlc_mutex_unlock( &p_fifo->lock );
        return NULL;
    }

    p_fifo->p_first = b->p_next;
    p_fifo->i_depth--;
    p_fifo->i_size -= b->i_buffer;

    if( p_fifo->p_first == NULL )
    {
        p_fifo->pp_last = &p_fifo->p_first;
    }

    /* We don't know how many threads can queue new packets now. */
    vlc_cond_broadcast( &p_fifo->wait_room );
    vlc_mutex_unlock( &p_fifo->lock );

    b->p_next = NULL;
    return b;
}

block_t *block_FifoShow( block_fifo_t *p_fifo )
{
    block_t *b;

    vlc_testcancel( );

    vlc_mutex_lock( &p_fifo->lock );
    mutex_cleanup_push( &p_fifo->lock );

    while( p_fifo->p_first == NULL )
        vlc_cond_wait( &p_fifo->wait, &p_fifo->lock );

    b = p_fifo->p_first;

    vlc_cleanup_run ();
    return b;
}

/* FIXME: not thread-safe */
size_t block_FifoSize( const block_fifo_t *p_fifo )
{
    return p_fifo->i_size;
}

/* FIXME: not thread-safe */
size_t block_FifoCount( const block_fifo_t *p_fifo )
{
    return p_fifo->i_depth;
}
