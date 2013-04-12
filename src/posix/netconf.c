/*****************************************************************************
 * netconf.c : Network configuration
 *****************************************************************************
 * Copyright (C) 2013 Rémi Denis-Courmont
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

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <spawn.h>
#include <unistd.h>

extern char **environ;

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_network.h>

/**
 * Determines the network proxy server to use (if any).
 * @param url absolute URL for which to get the proxy server
 * @return proxy URL, NULL if no proxy or error
 */
char *vlc_getProxyUrl(const char *url)
{
    /* libproxy helper */
    pid_t pid;
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attr;
    char *argv[3] = { (char *)"proxy", (char *)url, NULL };
    int fd[2];

    if (vlc_pipe(fd))
        return NULL;

    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null",
                                     O_RDONLY, 0644);
    posix_spawn_file_actions_adddup2(&actions, fd[1], STDOUT_FILENO);

    posix_spawnattr_init(&attr);
    {
        sigset_t set;

        sigemptyset(&set);
        posix_spawnattr_setsigmask(&attr, &set);
        sigaddset (&set, SIGPIPE);
        posix_spawnattr_setsigdefault(&attr, &set);
        posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGDEF
                                      | POSIX_SPAWN_SETSIGMASK);
    }

    if (posix_spawnp(&pid, "proxy", &actions, &attr, argv, environ))
        pid = -1;

    posix_spawnattr_destroy(&attr);
    posix_spawn_file_actions_destroy(&actions);
    close(fd[1]);

    if (pid != -1)
    {
        char buf[1024];
        size_t len = 0;

        do
        {
             ssize_t val = read(fd[0], buf + len, sizeof (buf) - len);
             if (val <= 0)
                 break;
        }
        while (len < sizeof (buf));

        close(fd[0]);
        while (waitpid(pid, &(int){ 0 }, 0) == -1);

        if (len >= 9 && !strncasecmp(buf, "direct://", 9))
            return NULL;

        char *end = memchr(buf, '\n', len);
        if (end != NULL)
        {
            *end = '\0';
            return strdup(buf);
        }
        /* Parse error: fallback (may be due to missing executable) */
    }
    else
        close(fd[0]);

    /* Fallback to environment variable */
    char *var = getenv("http_proxy");
    if (var != NULL)
        var = strdup(var);
    return var;
}
