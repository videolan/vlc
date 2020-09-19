/*****************************************************************************
 * spawn.c
 *****************************************************************************
 * Copyright (C) 2008, 2020 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include <vlc_common.h>
#include <vlc_spawn.h>

#if (_POSIX_SPAWN >= 0)
#include <spawn.h>

extern char **environ;

static int vlc_spawn_inner(pid_t *restrict pid, const char *path,
                           const int *fdv, const char *const *argv,
                           bool search)
{
    posix_spawnattr_t attr;
    posix_spawn_file_actions_t fas;
    char **vargv;
    size_t argc = 0;
    int err;

    assert(pid != NULL);
    assert(path != NULL);
    assert(fdv != NULL);
    assert(argv != NULL);

    err = posix_spawn_file_actions_init(&fas);
    if (unlikely(err != 0))
        return err;

    for (int newfd = 0; newfd < 3 || fdv[newfd] != -1; newfd++) {
        int oldfd = fdv[newfd];

        if (oldfd == -1)
            err = posix_spawn_file_actions_addopen(&fas, newfd, "/dev/null",
                                                   O_RDWR, 0644);
        else
            err = posix_spawn_file_actions_adddup2(&fas, oldfd, newfd);

        if (unlikely(err != 0)) {
            posix_spawn_file_actions_destroy(&fas);
            return err;
        }
    }

    posix_spawnattr_init(&attr);
    {
        sigset_t set;

        /* Unmask signals. */
        sigemptyset(&set);
        posix_spawnattr_setsigmask(&attr, &set);

        /* Reset SIGPIPE handler (which VLC overrode). */
        sigaddset(&set, SIGPIPE);
        posix_spawnattr_setsigdefault(&attr, &set);
    }
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGDEF
                                  | POSIX_SPAWN_SETSIGMASK);

    /* For hysterical raisins, POSIX uses non-const character pointers.
     * We need to copy manually due to aliasing rules.
     */
    while (argv[argc++] != NULL);

    vargv = malloc(sizeof (*argv) * argc);
    if (unlikely(vargv == NULL)) {
        err = errno;
        goto out;
    }

    for (size_t i = 0; i < argc; i++)
        vargv[i] = (char *)argv[i];

    if (search)
        err = posix_spawnp(pid, path, &fas, &attr, vargv, environ);
    else
        err = posix_spawn(pid, path, &fas, &attr, vargv, environ);

out:
    free(vargv);
    posix_spawnattr_destroy(&attr);
    posix_spawn_file_actions_destroy(&fas);
    return err;
}

#else /* _POSIX_SPAWN */

static int vlc_spawn_inner(pid_t *restrict pid, const char *path,
                           const int *fdv, const char *const *argv,
                           bool search)
{
    (void) pid; (void) path; (void) fdv; (void) argv; (void) search;
}

#endif /* _POSIX_SPAWN */

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
    int status;

    while (waitpid(pid, &status, 0) == -1)
        assert(errno != ECHILD && errno != EINVAL);

    return status;
}
