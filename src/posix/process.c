// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * process.c: posix implementation of process management
 *****************************************************************************
 * Copyright © 2025 Videolabs, VideoLAN and VLC authors
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_POLL_H
# include <poll.h>
#endif

#include <assert.h>
#include <signal.h>
#include <sys/poll.h>

#include <vlc_common.h>
#include <vlc_process.h>
#include <vlc_interrupt.h>
#include <vlc_spawn.h>
#include <vlc_poll.h>
#include <vlc_fs.h>
#include <vlc_network.h>

struct vlc_process {

    /* Pid of the linked process */
    pid_t pid;

    /* File descriptor of the socketpair */
    int fd;
};

struct vlc_process*
vlc_process_Spawn(const char *path, int argc, const char *const *argv)
{
    assert(path != NULL);
    assert(argc > 0);
    assert(argv != NULL);
    assert(argv[0] != NULL);

    int ret = VLC_EGENERIC;
    int fds[2] = { -1, -1 };
    int extfd_in = -1;
    int extfd_out = -1;
    const char **args = NULL;

    struct vlc_process *process = malloc(sizeof(*process));
    if (process == NULL) {
        return NULL;
    }

    ret = vlc_socketpair(PF_LOCAL, SOCK_STREAM, 0, fds, false);
    if (ret != 0) {
        goto end;
    }

    extfd_in = vlc_dup(fds[1]);
    if (extfd_in == -1) {
        ret = -1;
        goto end;
    }
    extfd_out = vlc_dup(fds[1]);
    if (extfd_out == -1) {
        ret = -1;
        goto end;
    }

    process->fd = fds[0];

    int process_fds[4] = {extfd_in, extfd_out, STDERR_FILENO, -1};

    /* `argc + 2`, 1 for the process->path and the last to be NULL */
    args = malloc((argc + 2) * sizeof(*args));
    if (args == NULL) {
        ret = VLC_ENOMEM;
        goto end;
    }
    args[0] = path;
    for (int i = 0; i < argc; i++) {
        args[i + 1] = argv[i];
    }
    args[argc + 1] = NULL;

    ret = vlc_spawnp(&process->pid, path, process_fds, args);
    if (ret != 0) {
        goto end;
    }

end:

    free(args);

    if (extfd_in != -1) {
        vlc_close(extfd_in);
    }
    if (extfd_out != -1) {
        vlc_close(extfd_out);
    }
    if (fds[1] != -1) {
        net_Close(fds[1]);
    }

    if (ret != 0) {
        if (fds[0] != -1) {
            shutdown(fds[0], SHUT_RDWR);
            net_Close(fds[0]);
        }
        free(process);
        return NULL;
    }
    return process;
}

VLC_API int
vlc_process_Terminate(struct vlc_process *process, bool kill_process)
{
    assert(process != NULL);

    if (kill_process) {
        kill(process->pid, SIGTERM);
    }

    shutdown(process->fd, SHUT_RDWR);
    net_Close(process->fd);

    int status = vlc_waitpid(process->pid);
    process->pid = 0;
    process->fd = -1;
    free(process);
    return status;
}

ssize_t
vlc_process_fd_Read(struct vlc_process *process, uint8_t *buf, size_t size,
                    vlc_tick_t timeout_ms)
{
    assert(process != NULL);
    assert(process->fd != -1);
    assert(buf != NULL);

    struct pollfd fds = {
        .fd = process->fd, .events = POLLIN, .revents = 0
    };

    int ret = vlc_poll_i11e(&fds, 1, timeout_ms);
    if (ret < 0) {
        return -1;
    } else if (ret == 0) {
        errno = ETIMEDOUT;
        return -1;
    } else if (!(fds.revents & POLLIN)) {
        errno = EINVAL;
        return -1;
    }

    return recv(process->fd, buf, size, 0);
}

ssize_t
vlc_process_fd_Write(struct vlc_process *process, const uint8_t *buf,
                     size_t size, vlc_tick_t timeout_ms)
{
    assert(process != NULL);
    assert(process->fd != -1);
    assert(buf != NULL);

    struct pollfd fds = {
        .fd = process->fd, .events = POLLOUT, .revents = 0
    };

    int ret = vlc_poll_i11e(&fds, 1, timeout_ms);
    if (ret < 0) {
        return -1;
    } else if (ret == 0) {
        errno = ETIMEDOUT;
        return -1;
    } else if (!(fds.revents & POLLOUT)) {
        errno = EINVAL;
        return -1;
    }

    return vlc_send_i11e(process->fd, buf, size, 0);
}
