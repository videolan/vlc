/*****************************************************************************
 * vlc_block.h: Data blocks management functions
 *****************************************************************************
 * Copyright (C) 2003 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef VLC_BLOCK_H
#define VLC_BLOCK_H 1

/**
 * \defgroup block Data blocks
 * \ingroup input
 *
 * Blocks of binary data.
 *
 * @ref block_t is a generic structure to represent a binary blob within VLC.
 * The primary goal of the structure is to avoid memory copying as data is
 * passed around. It is notably used between the \ref demux, the packetizer
 * (if present) and the \ref decoder, and for audio, between the \ref decoder,
 * the audio filters, and the \ref audio_output.
 *
 * @{
 * \file
 * Data block definition and functions
 */

#include <sys/types.h>  /* for ssize_t */

/****************************************************************************
 * block:
 ****************************************************************************
 * - i_flags may not always be set (ie could be 0, even for a key frame
 *      it depends where you receive the buffer (before/after a packetizer
 *      and the demux/packetizer implementations.
 * - i_dts/i_pts could be VLC_TICK_INVALID, it means no pts/dts
 * - i_length: length in microseond of the packet, can be null except in the
 *      sout where it is mandatory.
 *
 * - i_buffer number of valid data pointed by p_buffer
 *      you can freely decrease it but never increase it yourself
 *      (use block_Realloc)
 * - p_buffer: pointer over datas. You should never overwrite it, you can
 *   only incremment it to skip datas, in others cases use block_Realloc
 *   (don't duplicate yourself in a bigger buffer, block_Realloc is
 *   optimised for preheader/postdatas increase)
 ****************************************************************************/

/** The content doesn't follow the last block, possible some blocks in between
 *  have been lost */
#define BLOCK_FLAG_DISCONTINUITY 0x0001
/** Intra frame */
#define BLOCK_FLAG_TYPE_I        0x0002
/** Inter frame with backward reference only */
#define BLOCK_FLAG_TYPE_P        0x0004
/** Inter frame with backward and forward reference */
#define BLOCK_FLAG_TYPE_B        0x0008
/** For inter frame when you don't know the real type */
#define BLOCK_FLAG_TYPE_PB       0x0010
/** Warn that this block is a header one */
#define BLOCK_FLAG_HEADER        0x0020
/** This block contains the last part of a sequence  */
#define BLOCK_FLAG_END_OF_SEQUENCE 0x0040
/** This block contains a clock reference */
#define BLOCK_FLAG_CLOCK         0x0080
/** This block is scrambled */
#define BLOCK_FLAG_SCRAMBLED     0x0100
/** This block has to be decoded but not be displayed */
#define BLOCK_FLAG_PREROLL       0x0200
/** This block is corrupted and/or there is data loss  */
#define BLOCK_FLAG_CORRUPTED     0x0400
/** This block contains an interlaced picture with top field stored first */
#define BLOCK_FLAG_TOP_FIELD_FIRST 0x0800
/** This block contains an interlaced picture with bottom field stored first */
#define BLOCK_FLAG_BOTTOM_FIELD_FIRST 0x1000
/** This block contains a single field from interlaced picture. */
#define BLOCK_FLAG_SINGLE_FIELD  0x2000

/** This block contains an interlaced picture */
#define BLOCK_FLAG_INTERLACED_MASK \
    (BLOCK_FLAG_TOP_FIELD_FIRST|BLOCK_FLAG_BOTTOM_FIELD_FIRST|BLOCK_FLAG_SINGLE_FIELD)

#define BLOCK_FLAG_TYPE_MASK \
    (BLOCK_FLAG_TYPE_I|BLOCK_FLAG_TYPE_P|BLOCK_FLAG_TYPE_B|BLOCK_FLAG_TYPE_PB)

/* These are for input core private usage only */
#define BLOCK_FLAG_CORE_PRIVATE_MASK  0x00ff0000
#define BLOCK_FLAG_CORE_PRIVATE_SHIFT 16

/* These are for module private usage only */
#define BLOCK_FLAG_PRIVATE_MASK  0xff000000
#define BLOCK_FLAG_PRIVATE_SHIFT 24

typedef void (*block_free_t) (block_t *);

struct block_t
{
    block_t    *p_next;

    uint8_t    *p_buffer; /**< Payload start */
    size_t      i_buffer; /**< Payload length */
    uint8_t    *p_start; /**< Buffer start */
    size_t      i_size; /**< Buffer total size */

    uint32_t    i_flags;
    unsigned    i_nb_samples; /* Used for audio */

    vlc_tick_t  i_pts;
    vlc_tick_t  i_dts;
    vlc_tick_t  i_length;

    /* Rudimentary support for overloading block (de)allocation. */
    block_free_t pf_release;
};

VLC_API void block_Init( block_t *, void *, size_t );

/**
 * Allocates a block.
 *
 * Creates a new block with the requested size.
 * The block must be released with block_Release().
 *
 * @param size size in bytes (possibly zero)
 * @return the created block, or NULL on memory error.
 */
VLC_API block_t *block_Alloc(size_t size) VLC_USED VLC_MALLOC;

VLC_API block_t *block_TryRealloc(block_t *, ssize_t pre, size_t body) VLC_USED;

/**
 * Reallocates a block.
 *
 * This function expands, shrinks or moves a data block.
 * In many cases, this function can return without any memory allocation by
 * reusing spare buffer space. Otherwise, a new block is created and data is
 * copied.
 *
 * @param pre count of bytes to prepend if positive,
 *            count of leading bytes to discard if negative
 * @param body new bytes size of the block
 *
 * @return the reallocated block on succes, NULL on error.
 *
 * @note Skipping leading bytes can be achieved directly by subtracting from
 * block_t.i_buffer and adding block_t.p_buffer.
 * @note Discard trailing bytes can be achieved directly by subtracting from
 * block_t.i_buffer.
 * @note On error, the block is discarded.
 * To avoid that, use block_TryRealloc() instead.
 */
VLC_API block_t *block_Realloc(block_t *, ssize_t pre, size_t body) VLC_USED;

/**
 * Releases a block.
 *
 * This function works for any @ref block_t block, regardless of the way it was
 * allocated.
 *
 * @note
 * If the block is in a chain, this function does <b>not</b> release any
 * subsequent block in the chain. Use block_ChainRelease() for that purpose.
 *
 * @param block block to release (cannot be NULL)
 */
static inline void block_Release(block_t *block)
{
    block->pf_release(block);
}

static inline void block_CopyProperties( block_t *dst, block_t *src )
{
    dst->i_flags   = src->i_flags;
    dst->i_nb_samples = src->i_nb_samples;
    dst->i_dts     = src->i_dts;
    dst->i_pts     = src->i_pts;
    dst->i_length  = src->i_length;
}

/**
 * Duplicates a block.
 *
 * Creates a writeable duplicate of a block.
 *
 * @return the duplicate on success, NULL on error.
 */
VLC_USED
static inline block_t *block_Duplicate( block_t *p_block )
{
    block_t *p_dup = block_Alloc( p_block->i_buffer );
    if( p_dup == NULL )
        return NULL;

    block_CopyProperties( p_dup, p_block );
    memcpy( p_dup->p_buffer, p_block->p_buffer, p_block->i_buffer );

    return p_dup;
}

/**
 * Wraps heap in a block.
 *
 * Creates a @ref block_t out of an existing heap allocation.
 * This is provided by LibVLC so that manually heap-allocated blocks can safely
 * be deallocated even after the origin plugin has been unloaded from memory.
 *
 * When block_Release() is called, VLC will free() the specified pointer.
 *
 * @param addr base address of the heap allocation (will be free()'d)
 * @param length bytes length of the heap allocation
 * @return NULL in case of error (ptr free()'d in that case), or a valid
 * block_t pointer.
 */
VLC_API block_t *block_heap_Alloc(void *, size_t) VLC_USED VLC_MALLOC;

/**
 * Wraps a memory mapping in a block
 *
 * Creates a @ref block_t from a virtual address memory mapping (mmap).
 * This is provided by LibVLC so that mmap blocks can safely be deallocated
 * even after the allocating plugin has been unloaded from memory.
 *
 * @param addr base address of the mapping (as returned by mmap)
 * @param length length (bytes) of the mapping (as passed to mmap)
 * @return NULL if addr is MAP_FAILED, or an error occurred (in the later
 * case, munmap(addr, length) is invoked before returning).
 */
VLC_API block_t *block_mmap_Alloc(void *addr, size_t length) VLC_USED VLC_MALLOC;

/**
 * Wraps a System V memory segment in a block
 *
 * Creates a @ref block_t from a System V shared memory segment (shmget()).
 * This is provided by LibVLC so that segments can safely be deallocated
 * even after the allocating plugin has been unloaded from memory.
 *
 * @param addr base address of the segment (as returned by shmat())
 * @param length length (bytes) of the segment (as passed to shmget())
 * @return NULL if an error occurred (in that case, shmdt(addr) is invoked
 * before returning NULL).
 */
VLC_API block_t * block_shm_Alloc(void *addr, size_t length) VLC_USED VLC_MALLOC;

/**
 * Maps a file handle in memory.
 *
 * Loads a file into a block of memory through a file descriptor.
 * If possible a private file mapping is created. Otherwise, the file is read
 * normally. This function is a cancellation point.
 *
 * @note On 32-bits platforms,
 * this function will not work for very large files,
 * due to memory space constraints.
 *
 * @param fd file descriptor to load from
 * @param write If true, request a read/write private mapping.
 *              If false, request a read-only potentially shared mapping.
 *
 * @return a new block with the file content at p_buffer, and file length at
 * i_buffer (release it with block_Release()), or NULL upon error (see errno).
 */
VLC_API block_t *block_File(int fd, bool write) VLC_USED VLC_MALLOC;

/**
 * Maps a file in memory.
 *
 * Loads a file into a block of memory from a path to the file.
 * See also block_File().
 *
 * @param write If true, request a read/write private mapping.
 *              If false, request a read-only potentially shared mapping.
 */
VLC_API block_t *block_FilePath(const char *, bool write) VLC_USED VLC_MALLOC;

static inline void block_Cleanup (void *block)
{
    block_Release ((block_t *)block);
}
#define block_cleanup_push( block ) vlc_cleanup_push (block_Cleanup, block)

/**
 * \defgroup block_fifo Block chain
 * @{
 */

/****************************************************************************
 * Chains of blocks functions helper
 ****************************************************************************
 * - block_ChainAppend : append a block to the last block of a chain. Try to
 *      avoid using with a lot of data as it's really slow, prefer
 *      block_ChainLastAppend, p_block can be NULL
 * - block_ChainLastAppend : use a pointer over a pointer to the next blocks,
 *      and update it.
 * - block_ChainRelease : release a chain of block
 * - block_ChainExtract : extract data from a chain, return real bytes counts
 * - block_ChainGather : gather a chain, free it and return one block.
 ****************************************************************************/
static inline void block_ChainAppend( block_t **pp_list, block_t *p_block )
{
    if( *pp_list == NULL )
    {
        *pp_list = p_block;
    }
    else
    {
        block_t *p = *pp_list;

        while( p->p_next ) p = p->p_next;
        p->p_next = p_block;
    }
}

static inline void block_ChainLastAppend( block_t ***ppp_last, block_t *p_block )
{
    block_t *p_last = p_block;

    **ppp_last = p_block;

    while( p_last->p_next ) p_last = p_last->p_next;
    *ppp_last = &p_last->p_next;
}

static inline void block_ChainRelease( block_t *p_block )
{
    while( p_block )
    {
        block_t *p_next = p_block->p_next;
        block_Release( p_block );
        p_block = p_next;
    }
}

static size_t block_ChainExtract( block_t *p_list, void *p_data, size_t i_max )
{
    size_t  i_total = 0;
    uint8_t *p = (uint8_t*)p_data;

    while( p_list && i_max )
    {
        size_t i_copy = __MIN( i_max, p_list->i_buffer );
        memcpy( p, p_list->p_buffer, i_copy );
        i_max   -= i_copy;
        i_total += i_copy;
        p       += i_copy;

        p_list = p_list->p_next;
    }
    return i_total;
}

static inline void block_ChainProperties( block_t *p_list, int *pi_count, size_t *pi_size, vlc_tick_t *pi_length )
{
    size_t i_size = 0;
    vlc_tick_t i_length = 0;
    int i_count = 0;

    while( p_list )
    {
        i_size += p_list->i_buffer;
        i_length += p_list->i_length;
        i_count++;

        p_list = p_list->p_next;
    }

    if( pi_size )
        *pi_size = i_size;
    if( pi_length )
        *pi_length = i_length;
    if( pi_count )
        *pi_count = i_count;
}

static inline block_t *block_ChainGather( block_t *p_list )
{
    size_t  i_total = 0;
    vlc_tick_t i_length = 0;
    block_t *g;

    if( p_list->p_next == NULL )
        return p_list;  /* Already gathered */

    block_ChainProperties( p_list, NULL, &i_total, &i_length );

    g = block_Alloc( i_total );
    if( !g )
        return NULL;
    block_ChainExtract( p_list, g->p_buffer, g->i_buffer );

    g->i_flags = p_list->i_flags;
    g->i_pts   = p_list->i_pts;
    g->i_dts   = p_list->i_dts;
    g->i_length = i_length;

    /* free p_list */
    block_ChainRelease( p_list );
    return g;
}

/**
 * @}
 * \defgroup fifo Block FIFO
 * Thread-safe block queue functions
 * @{
 */

/**
 * Creates a thread-safe FIFO queue of blocks.
 *
 * See also block_FifoPut() and block_FifoGet().
 * The created queue must be released with block_FifoRelease().
 *
 * @return the FIFO or NULL on memory error
 */
VLC_API block_fifo_t *block_FifoNew(void) VLC_USED VLC_MALLOC;

/**
 * Destroys a FIFO created by block_FifoNew().
 *
 * @note Any queued blocks are also destroyed.
 * @warning No other threads may be using the FIFO when this function is
 * called. Otherwise, undefined behaviour will occur.
 */
VLC_API void block_FifoRelease(block_fifo_t *);

/**
 * Clears all blocks in a FIFO.
 */
VLC_API void block_FifoEmpty(block_fifo_t *);

/**
 * Immediately queue one block at the end of a FIFO.
 *
 * @param fifo queue
 * @param block head of a block list to queue (may be NULL)
 */
VLC_API void block_FifoPut(block_fifo_t *fifo, block_t *block);

/**
 * Dequeue the first block from the FIFO. If necessary, wait until there is
 * one block in the queue. This function is (always) cancellation point.
 *
 * @return a valid block
 */
VLC_API block_t *block_FifoGet(block_fifo_t *) VLC_USED;

/**
 * Peeks the first block in the FIFO.
 *
 * @warning This function leaves the block in the FIFO.
 * You need to protect against concurrent threads who could dequeue the block.
 * Preferably, there should be only one thread reading from the FIFO.
 *
 * @warning This function is undefined if the FIFO is empty.
 *
 * @return a valid block.
 */
VLC_API block_t *block_FifoShow(block_fifo_t *);

size_t block_FifoSize(block_fifo_t *) VLC_USED VLC_DEPRECATED;
VLC_API size_t block_FifoCount(block_fifo_t *) VLC_USED VLC_DEPRECATED;

typedef struct block_fifo_t vlc_fifo_t;

/**
 * Locks a block FIFO.
 *
 * No more than one thread can lock the FIFO at any given
 * time, and no other thread can modify the FIFO while it is locked.
 * vlc_fifo_Unlock() releases the lock.
 *
 * @note If the FIFO is already locked by another thread, this function waits.
 * This function is not a cancellation point.
 *
 * @warning Recursively locking a single FIFO is undefined. Locking more than
 * one FIFO at a time may lead to lock inversion; mind the locking order.
 */
VLC_API void vlc_fifo_Lock(vlc_fifo_t *);

/**
 * Unlocks a block FIFO.
 *
 * The calling thread must have locked the FIFO previously with
 * vlc_fifo_Lock(). Otherwise, the behaviour is undefined.
 *
 * @note This function is not a cancellation point.
 */
VLC_API void vlc_fifo_Unlock(vlc_fifo_t *);

/**
 * Wakes up one thread waiting on the FIFO, if any.
 *
 * @note This function is not a cancellation point.
 *
 * @warning For race-free operations, the FIFO should be locked by the calling
 * thread. The function can be called on a unlocked FIFO however.
 */
VLC_API void vlc_fifo_Signal(vlc_fifo_t *);

/**
 * Waits on the FIFO.
 *
 * Atomically unlocks the FIFO and waits until one thread signals the FIFO,
 * then locks the FIFO again. A signal can be sent by queueing a block to the
 * previously empty FIFO or by calling vlc_fifo_Signal() directly.
 * This function may also return spuriously at any moment.
 *
 * @note This function is a cancellation point. In case of cancellation, the
 * the FIFO will be locked before cancellation cleanup handlers are processed.
 */
VLC_API void vlc_fifo_Wait(vlc_fifo_t *);

VLC_API void vlc_fifo_WaitCond(vlc_fifo_t *, vlc_cond_t *);

/**
 * Timed variant of vlc_fifo_WaitCond().
 *
 * Atomically unlocks the FIFO and waits until one thread signals the FIFO up
 * to a certain date, then locks the FIFO again. See vlc_fifo_Wait().
 */
int vlc_fifo_TimedWaitCond(vlc_fifo_t *, vlc_cond_t *, vlc_tick_t);

/**
 * Queues a linked-list of blocks into a locked FIFO.
 *
 * @param block the head of the list of blocks
 *              (if NULL, this function has no effects)
 *
 * @note This function is not a cancellation point.
 *
 * @warning The FIFO must be locked by the calling thread using
 * vlc_fifo_Lock(). Otherwise behaviour is undefined.
 */
VLC_API void vlc_fifo_QueueUnlocked(vlc_fifo_t *, block_t *);

/**
 * Dequeues the first block from a locked FIFO, if any.
 *
 * @note This function is not a cancellation point.
 *
 * @warning The FIFO must be locked by the calling thread using
 * vlc_fifo_Lock(). Otherwise behaviour is undefined.
 *
 * @return the first block in the FIFO or NULL if the FIFO is empty
 */
VLC_API block_t *vlc_fifo_DequeueUnlocked(vlc_fifo_t *) VLC_USED;

/**
 * Dequeues the all blocks from a locked FIFO.
 *
 * This is equivalent to calling vlc_fifo_DequeueUnlocked() repeatedly until
 * the FIFO is emptied, but this function is much faster.
 *
 * @note This function is not a cancellation point.
 *
 * @warning The FIFO must be locked by the calling thread using
 * vlc_fifo_Lock(). Otherwise behaviour is undefined.
 *
 * @return a linked-list of all blocks in the FIFO (possibly NULL)
 */
VLC_API block_t *vlc_fifo_DequeueAllUnlocked(vlc_fifo_t *) VLC_USED;

/**
 * Counts blocks in a FIFO.
 *
 * Checks how many blocks are queued in a locked FIFO.
 *
 * @note This function is not cancellation point.
 *
 * @warning The FIFO must be locked by the calling thread using
 * vlc_fifo_Lock(). Otherwise behaviour is undefined.
 *
 * @return the number of blocks in the FIFO (zero if it is empty)
 */
VLC_API size_t vlc_fifo_GetCount(const vlc_fifo_t *) VLC_USED;

/**
 * Counts bytes in a FIFO.
 *
 * Checks how many bytes are queued in a locked FIFO.
 *
 * @note This function is not cancellation point.
 *
 * @warning The FIFO must be locked by the calling thread using
 * vlc_fifo_Lock(). Otherwise behaviour is undefined.
 *
 * @return the total number of bytes
 *
 * @note Zero bytes does not necessarily mean that the FIFO is empty since
 * a block could contain zero bytes. Use vlc_fifo_GetCount() to determine if
 * a FIFO is empty.
 */
VLC_API size_t vlc_fifo_GetBytes(const vlc_fifo_t *) VLC_USED;

VLC_USED static inline bool vlc_fifo_IsEmpty(const vlc_fifo_t *fifo)
{
    return vlc_fifo_GetCount(fifo) == 0;
}

static inline void vlc_fifo_Cleanup(void *fifo)
{
    vlc_fifo_Unlock((vlc_fifo_t *)fifo);
}
#define vlc_fifo_CleanupPush(fifo) vlc_cleanup_push(vlc_fifo_Cleanup, fifo)

/** @} */

/** @} */

#endif /* VLC_BLOCK_H */
