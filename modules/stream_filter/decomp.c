/*****************************************************************************
 * decomp.c : Decompression module for vlc
 *****************************************************************************
 * Copyright © 2008-2009 Rémi Denis-Courmont
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>
#include <vlc_network.h>
#include <vlc_fs.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#ifndef _POSIX_SPAWN
# define _POSIX_SPAWN (-1)
#endif
#include <fcntl.h>
#if (_POSIX_SPAWN >= 0)
# include <spawn.h>
#endif
#include <sys/wait.h>
#include <sys/ioctl.h>
#if defined (__linux__) && defined (HAVE_VMSPLICE)
# include <sys/uio.h>
# include <sys/mman.h>
#else
# undef HAVE_VMSPLICE
#endif

static int  OpenGzip (vlc_object_t *);
static int  OpenBzip2 (vlc_object_t *);
static int  OpenXZ (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_STREAM_FILTER)
    set_capability ("stream_filter", 20)

    set_description (N_("LZMA decompression"))
    set_callbacks (OpenXZ, Close)

    add_submodule ()
    set_description (N_("Burrows-Wheeler decompression"))
    set_callbacks (OpenBzip2, Close)
    /* TODO: access shortnames for stream_UrlNew() */

    add_submodule ()
    set_description (N_("gzip decompression"))
    set_callbacks (OpenGzip, Close)
vlc_module_end ()

struct stream_sys_t
{
    /* Thread data */
    int          write_fd;

    /* Shared data */
    vlc_cond_t   wait;
    vlc_mutex_t  lock;
    bool         paused;

    /* Caller data */
    vlc_thread_t thread;
    pid_t        pid;

    uint64_t     offset;
    block_t      *peeked;

    int          read_fd;
    bool         can_pace;
    bool         can_pause;
};

extern char **environ;

static const size_t bufsize = 65536;
#ifdef HAVE_VMSPLICE
static void cleanup_mmap (void *addr)
{
    munmap (addr, bufsize);
}
#endif

static void *Thread (void *data)
{
    stream_t *stream = data;
    stream_sys_t *p_sys = stream->p_sys;
#ifdef HAVE_VMSPLICE
    const ssize_t page_mask = sysconf (_SC_PAGE_SIZE) - 1;
#endif
    int fd = p_sys->write_fd;
    bool error = false;

    do
    {
        ssize_t len;
        int canc = vlc_savecancel ();
#ifdef HAVE_VMSPLICE
        unsigned char *buf = mmap (NULL, bufsize, PROT_READ|PROT_WRITE,
                                   MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        if (unlikely(buf == MAP_FAILED))
            break;
        vlc_cleanup_push (cleanup_mmap, buf);
#else
        unsigned char *buf = malloc (bufsize);
        if (unlikely(buf == NULL))
            break;
        vlc_cleanup_push (free, buf);
#endif

        vlc_mutex_lock (&p_sys->lock);
        while (p_sys->paused) /* practically always false, but... */
            vlc_cond_wait (&p_sys->wait, &p_sys->lock);
        len = stream_Read (stream->p_source, buf, bufsize);
        vlc_mutex_unlock (&p_sys->lock);

        vlc_restorecancel (canc);
        error = len <= 0;

        for (ssize_t i = 0, j; i < len; i += j)
        {
#ifdef HAVE_VMSPLICE
            if ((len - i) <= page_mask) /* incomplete last page */
                j = write (fd, buf + i, len - i);
            else
            {
                struct iovec iov = { buf + i, (len - i) & ~page_mask, };
                j = vmsplice (fd, &iov, 1, SPLICE_F_GIFT);
            }
            if (j == -1 && errno == ENOSYS) /* vmsplice() not supported */
#endif
            j = write (fd, buf + i, len - i);
            if (j <= 0)
            {
                if (j == 0)
                    errno = EPIPE;
                msg_Err (stream, "cannot write data (%m)");
                error = true;
                break;
            }
        }
        vlc_cleanup_run (); /* free (buf) */
    }
    while (!error);

    msg_Dbg (stream, "compressed stream at EOF");
    /* Let child process know about EOF */
    p_sys->write_fd = -1;
    close (fd);
    return NULL;
}


static int Peek (stream_t *, const uint8_t **, unsigned int);

#define MIN_BLOCK (1 << 10)
#define MAX_BLOCK (1 << 20)
/**
 * Reads decompressed from the decompression program
 * @return -1 for EAGAIN, 0 for EOF, byte count otherwise.
 */
static int Read (stream_t *stream, void *buf, unsigned int buflen)
{
    stream_sys_t *p_sys = stream->p_sys;
    block_t *peeked;
    ssize_t length;

    if (buf == NULL) /* caller skips data, get big enough peek buffer */
        buflen = Peek (stream, &(const uint8_t *){ NULL }, buflen);

    if ((peeked = p_sys->peeked) != NULL)
    {   /* dequeue peeked data */
        length = (buflen > peeked->i_buffer) ? peeked->i_buffer : buflen;
        if (buf != NULL)
        {
            memcpy (buf, peeked->p_buffer, length);
            buf = ((char *)buf) + length;
        }
        buflen -= length;
        peeked->p_buffer += length;
        peeked->i_buffer -= length;
        if (peeked->i_buffer == 0)
        {
            block_Release (peeked);
            p_sys->peeked = NULL;
        }
        p_sys->offset += length;

        if (buflen > 0)
            length += Read (stream, ((char *)buf) + length, buflen - length);
        return length;
    }
    assert ((buf != NULL) || (buflen == 0));

    length = net_Read (stream, p_sys->read_fd, NULL, buf, buflen, false);
    if (length < 0)
        return 0;
    p_sys->offset += length;
    return length;
}

/**
 *
 */
static int Peek (stream_t *stream, const uint8_t **pbuf, unsigned int len)
{
    stream_sys_t *p_sys = stream->p_sys;
    block_t *peeked = p_sys->peeked;
    size_t curlen = 0;
    int fd = p_sys->read_fd;

    if (peeked == NULL)
        peeked = block_Alloc (len);
    else if ((curlen = peeked->i_buffer) < len)
        peeked = block_Realloc (peeked, 0, len);

    if ((p_sys->peeked = peeked) == NULL)
        return 0;

    if (curlen < len)
    {
        ssize_t val = net_Read (stream, fd, NULL, peeked->p_buffer + curlen,
                                len - curlen, true);
        if (val >= 0)
        {
            curlen += val;
            peeked->i_buffer = curlen;
        }
    }
    *pbuf = peeked->p_buffer;
    return curlen;
}

/**
 *
 */
static int Control (stream_t *stream, int query, va_list args)
{
    stream_sys_t *p_sys = stream->p_sys;

    switch (query)
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
            *(va_arg (args, bool *)) = false;
            break;
        case STREAM_CAN_PAUSE:
             *(va_arg (args, bool *)) = p_sys->can_pause;
            break;
        case STREAM_CAN_CONTROL_PACE:
            *(va_arg (args, bool *)) = p_sys->can_pace;
            break;
        case STREAM_GET_POSITION:
            *(va_arg (args, uint64_t *)) = p_sys->offset;
            break;
        case STREAM_GET_SIZE:
            *(va_arg (args, uint64_t *)) = 0;
            break;
        case STREAM_SET_PAUSE_STATE:
        {
            bool paused = va_arg (args, unsigned);

            vlc_mutex_lock (&p_sys->lock);
            stream_Control (stream->p_source, STREAM_SET_PAUSE_STATE, paused);
            p_sys->paused = paused;
            vlc_cond_signal (&p_sys->wait);
            vlc_mutex_unlock (&p_sys->lock);
            break;
        }
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/**
 * Pipe data through an external executable.
 * @param stream the stream filter object.
 * @param path path to the executable.
 */
static int Open (stream_t *stream, const char *path)
{
    stream_sys_t *p_sys = stream->p_sys = malloc (sizeof (*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    stream->pf_read = Read;
    stream->pf_peek = Peek;
    stream->pf_control = Control;

    vlc_cond_init (&p_sys->wait);
    vlc_mutex_init (&p_sys->lock);
    p_sys->paused = false;
    p_sys->pid = -1;
    p_sys->offset = 0;
    p_sys->peeked = NULL;
    stream_Control (stream->p_source, STREAM_CAN_PAUSE, &p_sys->can_pause);
    stream_Control (stream->p_source, STREAM_CAN_CONTROL_PACE,
                    &p_sys->can_pace);

    /* I am not a big fan of the pyramid style, but I cannot think of anything
     * better here. There are too many failure cases. */
    int ret = VLC_EGENERIC;
    int comp[2];

    /* We use two pipes rather than one stream socket pair, so that we can
     * use vmsplice() on Linux. */
    if (vlc_pipe (comp) == 0)
    {
        p_sys->write_fd = comp[1];

        int uncomp[2];
        if (vlc_pipe (uncomp) == 0)
        {
            p_sys->read_fd = uncomp[0];

#if (_POSIX_SPAWN >= 0)
            posix_spawn_file_actions_t actions;
            if (posix_spawn_file_actions_init (&actions) == 0)
            {
                char *const argv[] = { (char *)path, NULL };

                if (!posix_spawn_file_actions_adddup2 (&actions, comp[0], 0)
                 && !posix_spawn_file_actions_adddup2 (&actions, uncomp[1], 1)
                 && !posix_spawnp (&p_sys->pid, path, &actions, NULL, argv,
                                   environ))
                {
                    if (vlc_clone (&p_sys->thread, Thread, stream,
                                   VLC_THREAD_PRIORITY_INPUT) == 0)
                        ret = VLC_SUCCESS;
                }
                else
                {
                    msg_Err (stream, "Cannot execute %s", path);
                    p_sys->pid = -1;
                }
                posix_spawn_file_actions_destroy (&actions);
            }
#else /* _POSIX_SPAWN */
            switch (p_sys->pid = fork ())
            {
                case -1:
                    msg_Err (stream, "Cannot fork (%m)");
                    break;
                case 0:
                    dup2 (comp[0], 0);
                    dup2 (uncomp[1], 1);
                    execlp (path, path, (char *)NULL);
                    exit (1); /* if we get, execlp() failed! */
                default:
                    if (vlc_clone (&p_sys->thread, Thread, stream,
                                   VLC_THREAD_PRIORITY_INPUT) == 0)
                        ret = VLC_SUCCESS;
            }
#endif /* _POSIX_SPAWN < 0 */
            close (uncomp[1]);
            if (ret != VLC_SUCCESS)
                close (uncomp[0]);
        }
        close (comp[0]);
        if (ret != VLC_SUCCESS)
            close (comp[1]);
    }

    if (ret == VLC_SUCCESS)
        return VLC_SUCCESS;

    if (p_sys->pid != -1)
        while (waitpid (p_sys->pid, &(int){ 0 }, 0) == -1);
    vlc_mutex_destroy (&p_sys->lock);
    vlc_cond_destroy (&p_sys->wait);
    free (p_sys);
    return ret;
}


/**
 * Releases allocate resources.
 */
static void Close (vlc_object_t *obj)
{
    stream_t *stream = (stream_t *)obj;
    stream_sys_t *p_sys = stream->p_sys;
    int status;

    vlc_cancel (p_sys->thread);
    close (p_sys->read_fd);
    vlc_join (p_sys->thread, NULL);
    if (p_sys->write_fd != -1)
        /* Killed before EOF? */
        close (p_sys->write_fd);

    msg_Dbg (obj, "waiting for PID %u", (unsigned)p_sys->pid);
    while (waitpid (p_sys->pid, &status, 0) == -1);
    msg_Dbg (obj, "exit status %d", status);

    if (p_sys->peeked)
        block_Release (p_sys->peeked);
    vlc_mutex_destroy (&p_sys->lock);
    vlc_cond_destroy (&p_sys->wait);
    free (p_sys);
}


/**
 * Detects gzip file format
 */
static int OpenGzip (vlc_object_t *obj)
{
    stream_t      *stream = (stream_t *)obj;
    const uint8_t *peek;

    if (stream_Peek (stream->p_source, &peek, 3) < 3)
        return VLC_EGENERIC;

    if (memcmp (peek, "\x1f\x8b\x08", 3))
        return VLC_EGENERIC;

    msg_Dbg (obj, "detected gzip compressed stream");
    return Open (stream, "zcat");
}


/**
 * Detects bzip2 file format
 */
static int OpenBzip2 (vlc_object_t *obj)
{
    stream_t      *stream = (stream_t *)obj;
    const uint8_t *peek;

    /* (Try to) parse the bzip2 header */
    if (stream_Peek (stream->p_source, &peek, 10) < 10)
        return VLC_EGENERIC;

    if (memcmp (peek, "BZh", 3) || (peek[3] < '1') || (peek[3] > '9')
     || memcmp (peek + 4, "\x31\x41\x59\x26\x53\x59", 6))
        return VLC_EGENERIC;

    msg_Dbg (obj, "detected bzip2 compressed stream");
    return Open (stream, "bzcat");
}

/**
 * Detects xz file format
 */
static int OpenXZ (vlc_object_t *obj)
{
    stream_t      *stream = (stream_t *)obj;
    const uint8_t *peek;

    /* (Try to) parse the xz stream header */
    if (stream_Peek (stream->p_source, &peek, 8) < 8)
        return VLC_EGENERIC;

    if (memcmp (peek, "\xfd\x37\x7a\x58\x5a", 6))
        return VLC_EGENERIC;

    msg_Dbg (obj, "detected xz compressed stream");
    return Open (stream, "xzcat");
}
