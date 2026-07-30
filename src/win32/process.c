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
#include <stdatomic.h>

#include <vlc_common.h>
#include <vlc_process.h>
#include <vlc_interrupt.h>
#include <vlc_spawn.h>
#include <vlc_fs.h>

static_assert(HAVE_VLC_PROCESS_SPAWN == 1, "mismatching HAVE_VLC_PROCESS_SPAWN declaration");

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

/**
 * Map the result of a failed overlapped operation to an errno-like value.
 */
static int
vlc_process_WindowsMapIoResult(DWORD error, DWORD *bytes, bool is_read,
                               int dflt)
{
    if (is_read) {
        if (error == ERROR_BROKEN_PIPE || error == ERROR_HANDLE_EOF) {
            *bytes = 0;
            return VLC_SUCCESS;
        }
    } else if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA ||
               error == ERROR_PIPE_NOT_CONNECTED) {
        return EPIPE;
    }
    return dflt;
}

/**
 * Cancel a pending overlapped operation and wait for its completion.
 */
static int vlc_process_WindowsCancelPoll(HANDLE hFd, LPOVERLAPPED lpoverlapped,
                                         DWORD *bytes, DWORD size,
                                         bool is_read, int cancel_err)
{
    CancelIoEx(hFd, lpoverlapped);
    if (GetOverlappedResult(hFd, lpoverlapped, bytes, TRUE)) {
        return VLC_SUCCESS;
    }

    DWORD error = GetLastError();
    if (error == ERROR_OPERATION_ABORTED) {
        if (*bytes > 0 && *bytes <= size) {
            return VLC_SUCCESS;
        }
        *bytes = 0;
        return cancel_err;
    }
    return vlc_process_WindowsMapIoResult(error, bytes, is_read, EINVAL);
}

static int
vlc_process_WindowsPoll(HANDLE hFd, LPOVERLAPPED lpoverlapped, DWORD *bytes,
                        DWORD size, bool is_read, vlc_tick_t timeout_ms)
{
    HANDLE th;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                         GetCurrentProcess(), &th, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
        return vlc_process_WindowsCancelPoll(hFd, lpoverlapped, bytes, size,
                                             is_read, ENOMEM);
    }
    vlc_interrupt_register(vlc_process_WindowsPoll_i11e_wake, th);
    DWORD waitResult = WaitForSingleObjectEx(lpoverlapped->hEvent,
                                             timeout_ms, TRUE);
    int interrupted = vlc_interrupt_unregister();
    CloseHandle(th);

    if (interrupted != 0) {
        return vlc_process_WindowsCancelPoll(hFd, lpoverlapped, bytes, size,
                                             is_read, EINTR);
    }

    switch (waitResult) {
        case WAIT_OBJECT_0:
            if (GetOverlappedResult(hFd, lpoverlapped, bytes, FALSE)) {
                return VLC_SUCCESS;
            }
            return vlc_process_WindowsMapIoResult(GetLastError(), bytes,
                                                  is_read, EINVAL);
        case WAIT_TIMEOUT:
            /* Timeout occurred */
            return vlc_process_WindowsCancelPoll(hFd, lpoverlapped, bytes,
                                                 size, is_read, ETIMEDOUT);
        case WAIT_IO_COMPLETION:
            /* Interrupt occurred */
            return vlc_process_WindowsCancelPoll(hFd, lpoverlapped, bytes,
                                                 size, is_read, EINTR);
        default:
            return vlc_process_WindowsCancelPoll(hFd, lpoverlapped, bytes,
                                                 size, is_read, EINVAL);
    }
}

struct vlc_process {
    /* Pid of the linked process */
    pid_t pid;

    HANDLE hProcess;

    int fd_in;
    int fd_out;

    HANDLE hEventRead;
    HANDLE hEventWrite;

    atomic_bool killed;
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
    process->hProcess = NULL;
    process->hEventRead = NULL;
    process->hEventWrite = NULL;
    atomic_init(&process->killed, false);

    process->hEventRead = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (process->hEventRead == NULL) {
        ret = VLC_ENOMEM;
        goto end;
    }

    process->hEventWrite = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (process->hEventWrite == NULL) {
        ret = VLC_ENOMEM;
        goto end;
    }

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

    process->hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, process->pid);

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
        if (process->hEventRead != NULL) {
            CloseHandle(process->hEventRead);
        }
        if (process->hEventWrite != NULL) {
            CloseHandle(process->hEventWrite);
        }
        free(process);
        return NULL;
    }
    return process;
}

void
vlc_process_Kill(struct vlc_process *process)
{
    assert(process != NULL);

    if (process->hProcess != NULL) {
        TerminateProcess(process->hProcess, 15);
    }

    atomic_store(&process->killed, true);

    intptr_t h_in = _get_osfhandle(process->fd_in);
    if (h_in != -1 && h_in != -2) {
        CancelIoEx((HANDLE)h_in, NULL);
    }
    intptr_t h_out = _get_osfhandle(process->fd_out);
    if (h_out != -1 && h_out != -2) {
        CancelIoEx((HANDLE)h_out, NULL);
    }
}

int
vlc_process_Terminate(struct vlc_process *process, bool kill_process)
{
    assert(process != NULL);

    if (kill_process) {
        vlc_process_Kill(process);
    }

    vlc_close(process->fd_in);
    vlc_close(process->fd_out);
    CloseHandle(process->hEventRead);
    CloseHandle(process->hEventWrite);

    int status = vlc_waitpid(process->pid);
    if (process->hProcess != NULL) {
        CloseHandle(process->hProcess);
    }
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

    /* The channel is shut down, report end-of-stream like POSIX does. */
    if (atomic_load(&process->killed)) {
        return 0;
    }

    intptr_t h = _get_osfhandle(process->fd_in);
    if (h == -1) {
        errno = EINVAL;
        return -1;
    }
    HANDLE hFd = (HANDLE)h;

    DWORD bytes = 0;
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = process->hEventRead;

    /* The event is reused across calls, drop any leftover completion. */
    ResetEvent(process->hEventRead);

    int err;
    if (ReadFile(hFd, buf, size, NULL, &overlapped)) {
        if (GetOverlappedResult(hFd, &overlapped, &bytes, FALSE)) {
            err = VLC_SUCCESS;
        } else {
            err = vlc_process_WindowsMapIoResult(GetLastError(), &bytes, true,
                                                 EINVAL);
        }
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_IO_PENDING) {
            if (atomic_load(&process->killed)) {
                CancelIoEx(hFd, &overlapped);
            }
            err = vlc_process_WindowsPoll(hFd, &overlapped, &bytes,
                                          (DWORD)size, true, timeout_ms);
        } else {
            err = vlc_process_WindowsMapIoResult(error, &bytes, true, EINVAL);
        }
    }

    if (err == VLC_SUCCESS) {
        return bytes;
    }
    if (atomic_load(&process->killed)) {
        return 0;
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

    if (atomic_load(&process->killed)) {
        errno = EPIPE;
        return -1;
    }

    intptr_t h = _get_osfhandle(process->fd_out);
    if (h == -1) {
        errno = EINVAL;
        return -1;
    }
    HANDLE hFd = (HANDLE)h;

    DWORD bytes = 0;
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = process->hEventWrite;

    /* The event is reused across calls, drop any leftover completion. */
    ResetEvent(process->hEventWrite);

    int err;
    if (WriteFile(hFd, buf, size, NULL, &overlapped)) {
        if (GetOverlappedResult(hFd, &overlapped, &bytes, FALSE)) {
            err = VLC_SUCCESS;
        } else {
            err = vlc_process_WindowsMapIoResult(GetLastError(), &bytes, false,
                                                 EINVAL);
        }
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_IO_PENDING) {
            if (atomic_load(&process->killed)) {
                CancelIoEx(hFd, &overlapped);
            }
            err = vlc_process_WindowsPoll(hFd, &overlapped, &bytes,
                                          (DWORD)size, false, timeout_ms);
        } else {
            err = vlc_process_WindowsMapIoResult(error, &bytes, false, EINVAL);
        }
    }

    if (err == VLC_SUCCESS) {
        return bytes;
    }
    errno = atomic_load(&process->killed) ? EPIPE : err;
    return -1;
}
