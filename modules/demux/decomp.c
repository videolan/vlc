/*****************************************************************************
 * decomp.c : Decompression module for vlc
 *****************************************************************************
 * Copyright © 2008 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_stream.h>
#include <vlc_network.h>
#include <unistd.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#ifdef __linux__
# include <sys/uio.h>
# include <sys/mman.h>
#endif

#include <assert.h>

static int  OpenGzip (vlc_object_t *);
static int  OpenBzip2 (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_description (N_("Decompression"))
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_DEMUX)
    set_capability ("demux", 20)
    set_callbacks (OpenBzip2, Close)
    /* TODO: shortnames */
    /* --demux support */

    add_submodule ()
    set_callbacks (OpenGzip, Close)
vlc_module_end ()

static int Demux   (demux_t *);
static int Control (demux_t *, int i_query, va_list args);

struct demux_sys_t
{
    stream_t    *out;
    vlc_thread_t thread;
    pid_t        pid;
    int          write_fd, read_fd;
};

static void cloexec (int fd)
{
    int flags = fcntl (fd, F_GETFD);
    fcntl (fd, F_SETFD, FD_CLOEXEC | ((flags != -1) ? flags : 0));
}

extern char **environ;

static const size_t bufsize = 65536;
static void cleanup_mmap (void *addr)
{
    munmap (addr, bufsize);
}


static void *Thread (void *data)
{
    demux_t *demux = data;
    demux_sys_t *p_sys = demux->p_sys;
    int fd = p_sys->write_fd;
    bool error = false;

    do
    {
        ssize_t len;
        int canc = vlc_savecancel ();
#ifdef __linux__
        unsigned char *buf = mmap (NULL, bufsize, PROT_READ|PROT_WRITE,
                                   MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        vlc_cleanup_push (cleanup_mmap, buf);
#else
        unsigned char buf[bufsize];
#endif

        len = stream_Read (demux->s, buf, bufsize);
        vlc_restorecancel (canc);

        if (len <= 0)
            break;

        for (ssize_t i = 0, j = 0; i < len; i += j)
        {
            struct iovec iov[1] = { { buf + i, len - i, } };

#ifdef __linux__
            j = vmsplice (fd, iov, 1, SPLICE_F_GIFT);
#else
            j = writev (fd, iov, 1);
#endif
            if (j <= 0)
            {
                if (j == 0)
                    errno = EPIPE;
                msg_Err (demux, "cannot write data (%m)");
                error = true;
                break;
            }
        }
#ifdef __linux__
        vlc_cleanup_run (); /* munmap (buf, bufsize) */
#endif
    }
    while (!error);

    msg_Dbg (demux, "compressed stream at EOF");
    return NULL;
}


#define MIN_BLOCK (1 << 10)
#define MAX_BLOCK (1 << 20)
/**
 * Read data, decompress it, and forward
 * @return -1 in case of error, 0 in case of EOF, 1 otherwise.
 */
static int Demux (demux_t *demux)
{
    demux_sys_t *p_sys = demux->p_sys;
    int length, fd = p_sys->read_fd;

#ifdef TIOCINQ
    if (ioctl (fd, TIOCINQ, &length) == 0)
    {
        if (length > MAX_BLOCK)
            length = MAX_BLOCK;
        else
        if (length < MIN_BLOCK)
            length = MIN_BLOCK;
    }
    else
#endif
        length = MIN_BLOCK;

    block_t *block = block_Alloc (length);
    if (block == NULL)
        return VLC_ENOMEM;

    length = net_Read (demux, fd, NULL, block->p_buffer, length, false);
    if (length <= 0)
        return 0;
    block->i_buffer = length;
    stream_DemuxSend (p_sys->out, block);
    return 1;
}


/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control (demux_t *demux, int query, va_list args)
{
    /*demux_sys_t *p_sys = demux->p_sys;*/
    (void)demux;
    (void)query; (void)args;
    return VLC_EGENERIC;
}

/**
 * Pipe data through an external executable.
 * @param demux the demux object.
 * @param path path to the executable.
 */
static int Open (demux_t *demux, const char *path)
{
    demux_sys_t *p_sys = demux->p_sys = malloc (sizeof (*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    demux->pf_demux = Demux;
    demux->pf_control = Control;

    /* I am not a big fan of the pyramid style, but I cannot think of anything
     * better here. There are too many failure cases. */
    int ret = VLC_EGENERIC;
    int comp[2];
    if (pipe (comp) == 0)
    {
        cloexec (comp[1]);
        p_sys->write_fd = comp[1];

        int uncomp[2];
        if (pipe (uncomp) == 0)
        {
            cloexec (uncomp[0]);
            p_sys->read_fd = uncomp[0];

            posix_spawn_file_actions_t actions;
            if (posix_spawn_file_actions_init (&actions) == 0)
            {
                char *const argv[] = { (char *)path, NULL };

                if (!posix_spawn_file_actions_adddup2 (&actions, comp[0], 0)
                 && !posix_spawn_file_actions_addclose (&actions, comp[0])
                 && !posix_spawn_file_actions_adddup2 (&actions, uncomp[1], 1)
                 && !posix_spawn_file_actions_addclose (&actions, uncomp[1])
                 && !posix_spawnp (&p_sys->pid, path, &actions, NULL, argv,
                                   environ))
                {
                    p_sys->out = stream_DemuxNew (demux, "", demux->out);
                    if (p_sys->out != NULL)
                    {
                        if (vlc_clone (&p_sys->thread, Thread, demux,
                                       VLC_THREAD_PRIORITY_INPUT) == 0)
                            ret = VLC_SUCCESS;
                        else
                            stream_Delete (p_sys->out);
                    }
                    else
                        msg_Err (demux, "Cannot create demux");
                }
                else
                    msg_Err (demux, "Cannot execute %s", path);
                posix_spawn_file_actions_destroy (&actions);
            }
            if (ret != VLC_SUCCESS)
            {
                close (comp[1]);
                close (comp[0]);
            }
        }
        if (ret != VLC_SUCCESS)
            close (uncomp[0]);
        close (uncomp[1]);
    }
    return ret;
}


/**
 * Releases allocate resources.
 */
static void Close (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *p_sys = demux->p_sys;
    int status;

    vlc_cancel (p_sys->thread);
    stream_Delete (p_sys->out);
    close (p_sys->read_fd);
    vlc_join (p_sys->thread, NULL);
    close (p_sys->write_fd);

    msg_Dbg (obj, "waiting for PID %u", (unsigned)p_sys->pid);
    while (waitpid (p_sys->pid, &status, 0) == -1);
    msg_Dbg (obj, "exit status %d", status);

    free (p_sys);
}


/**
 * Detects gzip file format
 */
static int OpenGzip (vlc_object_t *obj)
{
    demux_t       *demux = (demux_t *)obj;
    stream_t      *stream = demux->s;
    const uint8_t *peek;

    if (stream_Peek (stream, &peek, 3) < 3)
        return VLC_EGENERIC;

    if (memcmp (peek, "\x1f\x8b\x08", 3))
        return VLC_EGENERIC;

    msg_Dbg (obj, "detected gzip compressed stream");
    return Open (demux, "zcat");
}


/**
 * Detects bzip2 file format
 */
static int OpenBzip2 (vlc_object_t *obj)
{
    demux_t       *demux = (demux_t *)obj;
    stream_t      *stream = demux->s;
    const uint8_t *peek;

    /* (Try to) parse the bzip2 header */
    if (stream_Peek (stream, &peek, 10) < 10)
        return VLC_EGENERIC;

    if (memcmp (peek, "BZh", 3) || (peek[3] < '1') || (peek[3] > '9')
     || memcmp (peek, "\x31\x41\x59\x26\x53\x59", 6))
        return VLC_EGENERIC;

    msg_Dbg (obj, "detected bzip2 compressed stream");
    return Open (demux, "bzcat");
}

