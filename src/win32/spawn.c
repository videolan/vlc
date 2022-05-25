/*****************************************************************************
 * spawn.c
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#include <windows.h>
#include <assert.h>
#include <fcntl.h>

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_spawn.h>
#include <vlc_memstream.h>

static LPPROC_THREAD_ATTRIBUTE_LIST allow_hstd_inherit(HANDLE *handles)
{
    size_t attribute_list_size;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_list_size);

    LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList = malloc(attribute_list_size);
    if (unlikely(!lpAttributeList))
        return NULL;

    BOOL result;
    result = InitializeProcThreadAttributeList(lpAttributeList, 1, 0,
                                               &attribute_list_size);
    if (unlikely(!result))
        goto error;

    /* duplicated handles in authorized list will cause CreateProcess() failure */
    int nb_handles = handles[2] == INVALID_HANDLE_VALUE ? 2 : 3;
    if (handles[0] == handles[nb_handles - 1])
        --nb_handles;
    if (nb_handles > 2 && (handles[1] == handles[2]))
        --nb_handles;
    if (nb_handles > 1 && (handles[0] == handles[1])) {
        ++handles;
        --nb_handles;
    }

    result = UpdateProcThreadAttribute(lpAttributeList, 0,
                                       PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles,
                                       sizeof(*handles) * nb_handles, NULL, NULL);
    if (unlikely(!result))
        goto error;

    return lpAttributeList;

error:
    if (lpAttributeList)
        free(lpAttributeList);
    return NULL;
}

static HANDLE dup_handle_from_fd(const int fd)
{
    HANDLE vlc_handle = (HANDLE)_get_osfhandle(fd);
    if (vlc_handle == INVALID_HANDLE_VALUE)
        return vlc_handle;

    if (vlc_handle == (HANDLE)-2)
        return INVALID_HANDLE_VALUE;

    HANDLE dup_inheritable_handle;
    BOOL result = DuplicateHandle(GetCurrentProcess(), vlc_handle, GetCurrentProcess(),
                                  &dup_inheritable_handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
    if (!result)
        return INVALID_HANDLE_VALUE;

    return dup_inheritable_handle;
}

static int vlc_spawn_inner(pid_t *restrict pid, const char *path,
                           const int fdv[4], const char *const *argv,
                           bool search)
{
    assert(pid != NULL);
    assert(path != NULL);
    assert(fdv != NULL);
    assert(argv != NULL);

    int ret = -1;
    int nulfd = -1;
    HANDLE nul_handle = INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION pi = {0};
    STARTUPINFOEXA siEx = {
        .StartupInfo = {
            .cb         = sizeof(siEx),
            .hStdInput  = INVALID_HANDLE_VALUE,
            .hStdOutput = INVALID_HANDLE_VALUE,
            .hStdError  = INVALID_HANDLE_VALUE,
            .dwFlags    = STARTF_USESTDHANDLES
        }
    };

    struct vlc_memstream application_name;
    struct vlc_memstream cmdline;
    {
        int error;
        error = vlc_memstream_open(&application_name);
        error |= vlc_memstream_open(&cmdline);
        if (unlikely(error))
            goto error;
    }

    if (fdv[0] == -1 || fdv[1] == -1) {
        nulfd = vlc_open("\\\\.\\NUL", O_RDWR);
        if (unlikely(nulfd == -1))
            goto error;

        nul_handle = dup_handle_from_fd(nulfd);
        if (unlikely(nul_handle == INVALID_HANDLE_VALUE))
            goto error;

        if (fdv[0] == -1)
            siEx.StartupInfo.hStdInput = nul_handle;
        if (fdv[1] == -1)
            siEx.StartupInfo.hStdOutput = nul_handle;
    }

    if (fdv[0] != -1) {
        siEx.StartupInfo.hStdInput = dup_handle_from_fd(fdv[0]);
        if (unlikely(siEx.StartupInfo.hStdInput == INVALID_HANDLE_VALUE))
            goto error;
    }
    if (fdv[1] != -1) {
        siEx.StartupInfo.hStdOutput = dup_handle_from_fd(fdv[1]);
        if (unlikely(siEx.StartupInfo.hStdOutput == INVALID_HANDLE_VALUE))
            goto error;
    }
    if (fdv[2] != -1) {
        siEx.StartupInfo.hStdError = dup_handle_from_fd(fdv[2]);
        if (unlikely(siEx.StartupInfo.hStdError == INVALID_HANDLE_VALUE))
            goto error;
    }

    HANDLE handles[3] = {
        siEx.StartupInfo.hStdInput,
        siEx.StartupInfo.hStdOutput,
        siEx.StartupInfo.hStdError
    };
    siEx.lpAttributeList = allow_hstd_inherit(handles);
    if (unlikely(!siEx.lpAttributeList))
        goto error;

    if (search) {
        if (!SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE))
            goto error;

        char *const application_path = malloc(MAX_PATH);
        if (unlikely(!application_path))
            goto error;

        if (!SearchPathA(NULL, path, NULL, MAX_PATH, application_path, NULL)) {
            free(application_path);
            goto error;
        }

        vlc_memstream_printf(&application_name, "%s", application_path);
        free(application_path);

    } else {
        vlc_memstream_printf(&application_name, "%s", path);
    }

    if (likely(argv[0])) {
        vlc_memstream_printf(&cmdline, "%s", argv[0]);
        for (int argc = 1; argv[argc]; ++argc)
            vlc_memstream_printf(&cmdline, " %s", argv[argc]);
    }

    BOOL bSuccess = CreateProcessA(application_name.ptr, cmdline.ptr, NULL,
                                   NULL, TRUE, EXTENDED_STARTUPINFO_PRESENT,
                                   NULL, NULL, &siEx.StartupInfo, &pi);
    if (!bSuccess)
        goto error;

    pid_t id = GetProcessId(pi.hProcess);
    if (unlikely(!id))
        goto error;
    *pid = id;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    ret = 0;

error:
    if (nulfd != -1)
        vlc_close(nulfd);
    if (nul_handle != INVALID_HANDLE_VALUE)
        CloseHandle(nul_handle);

    if (fdv[0] != -1 && siEx.StartupInfo.hStdInput != INVALID_HANDLE_VALUE)
        CloseHandle(siEx.StartupInfo.hStdInput);
    if (fdv[1] != -1 && siEx.StartupInfo.hStdOutput != INVALID_HANDLE_VALUE)
        CloseHandle(siEx.StartupInfo.hStdOutput);
    if (siEx.StartupInfo.hStdError != INVALID_HANDLE_VALUE)
        CloseHandle(siEx.StartupInfo.hStdError);

    if (siEx.lpAttributeList) {
        DeleteProcThreadAttributeList(siEx.lpAttributeList);
        free(siEx.lpAttributeList);
    }

    if (!vlc_memstream_close(&application_name))
        free(application_name.ptr);
    if (!vlc_memstream_close(&cmdline))
        free(cmdline.ptr);

    return ret;
}

int vlc_spawnp(pid_t *restrict pid, const char *path,
               const int *fdv, const char *const *argv)
{
    return vlc_spawn_inner(pid, path, fdv, argv, true);
}

int vlc_spawn(pid_t *restrict pid, const char *file,
              const int *fdv, const char *const *argv)
{
    return vlc_spawn_inner(pid, file, fdv, argv, false);
}

int vlc_waitpid(pid_t pid)
{
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!process)
        return -1;

    WaitForSingleObject(process, INFINITE);

    DWORD exit_code = -1;
    GetExitCodeProcess(process, &exit_code);

    CloseHandle(process);
    return exit_code;
}
