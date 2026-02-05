// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * process.c: win32 implementation of process management
 *****************************************************************************
 * Copyright © 2025 Videolabs, VideoLAN and VLC authors
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <windows.h>
#include <io.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_process.h>
#include <vlc_interrupt.h>
#include <vlc_spawn.h>
#include <vlc_fs.h>

static void CALLBACK
vlc_process_WindowsPoll_i11e_wake_self(ULONG_PTR data)
{
    (void) data;
}

static void
vlc_process_WindowsPoll_i11e_wake(void *opaque)
{
    assert(opaque != NULL);

    HANDLE th = opaque;
    QueueUserAPC(vlc_process_WindowsPoll_i11e_wake_self, th, 0);
}

static int
vlc_process_WindowsPoll(HANDLE hFd, LPOVERLAPPED lpoverlapped, DWORD *bytes,
                        vlc_tick_t timeout_ms)
{
    HANDLE th;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                         GetCurrentProcess(), &th, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
        return ENOMEM;
    }
    vlc_interrupt_register(vlc_process_WindowsPoll_i11e_wake, th);
    DWORD waitResult = WaitForSingleObjectEx(lpoverlapped->hEvent,
                                             timeout_ms, TRUE);
    vlc_interrupt_unregister();
    CloseHandle(th);
    switch (waitResult) {
        case WAIT_OBJECT_0:
            if (GetOverlappedResult(hFd, lpoverlapped, bytes, FALSE)) {
                return VLC_SUCCESS;
            } else {
                return EINVAL;
            }
        case WAIT_TIMEOUT:
            /* Timeout occurred */
            CancelIo(hFd); /* Cancel the I/O operation */
            return ETIMEDOUT;
        case WAIT_IO_COMPLETION:
            /* Interrupt occurred */
            CancelIo(hFd); /* Cancel the I/O operation */
            return EINTR;
        default:
            return EINVAL;
    }
}

struct vlc_process {
    /* Pid of the linked process */
    pid_t pid;

    int fd_in;
    int fd_out;

    HANDLE hEvent;
};

struct vlc_process*
vlc_process_Spawn(const char *path, int argc, const char *const *argv)
{
    assert(path != NULL);
    if (argc > 0) {
        assert(argv != NULL);
        assert(argv[0] != NULL);
    }

    int ret = VLC_EGENERIC;
    int fds[2] = { -1, -1 };
    int extfd_in = -1;
    int extfd_out = -1;
    const char **args = NULL;

    struct vlc_process *process = malloc(sizeof(*process));
    if (process == NULL) {
        return NULL;
    }

    process->fd_in = -1;
    process->fd_out = -1;
    process->hEvent = INVALID_HANDLE_VALUE;

    ret = vlc_pipe(fds);
    if (ret != 0) {
        goto end;
    }
    extfd_out = fds[1];
    process->fd_in = fds[0];

    ret = vlc_pipe(fds);
    if (ret != 0) {
        goto end;
    }
    extfd_in = fds[0];
    process->fd_out = fds[1];

    process->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (process->hEvent == INVALID_HANDLE_VALUE) {
        errno = EINVAL;
        goto end;
    }

    int stderr_fd = -1;
    intptr_t h_err = _get_osfhandle(STDERR_FILENO);
    if (h_err != -1 && h_err != -2) {
        stderr_fd = STDERR_FILENO;
    }

    int process_fds[4] = {extfd_in, extfd_out, stderr_fd, -1};

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

    if (ret != 0) {
        if (process->fd_in != -1) {
            vlc_close(process->fd_in);
        }
        if (process->fd_out != -1) {
            vlc_close(process->fd_out);
        }
        if (process->hEvent != INVALID_HANDLE_VALUE) {
            CloseHandle(process->hEvent);
        }
        free(process);
        return NULL;
    }
    return process;
}

int
vlc_process_Terminate(struct vlc_process *process, bool kill_process)
{
    assert(process != NULL);

    if (kill_process) {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, process->pid);
        if (hProcess) {
            TerminateProcess(hProcess, 15);
            CloseHandle(hProcess);
        }
    }

    vlc_close(process->fd_in);
    vlc_close(process->fd_out);
    CloseHandle(process->hEvent);

    int status = vlc_waitpid(process->pid);
    process->pid = 0;
    free(process);
    return status;
}

ssize_t
vlc_process_fd_Read(struct vlc_process *process, uint8_t *buf, size_t size,
                    vlc_tick_t timeout_ms)
{
    assert(process != NULL);
    assert(buf != NULL);

    intptr_t h = _get_osfhandle(process->fd_in);
    if (h == -1) {
        errno = EINVAL;
        return -1;
    }
    HANDLE hFd = (HANDLE)h;

    DWORD bytes = 0;
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = process->hEvent;

    BOOL ret = FALSE;
    ret = ReadFile(hFd, buf, size, &bytes, &overlapped);

    int err = VLC_SUCCESS;

    if (ret) {
        return bytes;
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_IO_PENDING) {
            err = vlc_process_WindowsPoll(hFd, &overlapped, &bytes,
                                          timeout_ms);
        } else {
            err = EINVAL;
        }
    }
    if (err == VLC_SUCCESS) {
        return bytes;
    }
    errno = err;
    return -1;
}

ssize_t
vlc_process_fd_Write(struct vlc_process *process, const uint8_t *buf, size_t size,
                     vlc_tick_t timeout_ms)
{
    assert(process != NULL);
    assert(buf != NULL);

    intptr_t h = _get_osfhandle(process->fd_out);
    if (h == -1) {
        errno = EINVAL;
        return -1;
    }
    HANDLE hFd = (HANDLE)h;

    DWORD bytes = 0;
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = process->hEvent;

    BOOL ret = FALSE;
    ret = WriteFile(hFd, buf, size, &bytes, &overlapped);

    int err = VLC_SUCCESS;

    if (ret) {
        return bytes;
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_IO_PENDING) {
            err = vlc_process_WindowsPoll(hFd, &overlapped, &bytes,
                                          timeout_ms);
        } else {
            err = EINVAL;
        }
    }
    if (err == VLC_SUCCESS) {
        return bytes;
    }
    errno = err;
    return -1;
}
