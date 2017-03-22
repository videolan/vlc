/*****************************************************************************
 * sigwait.c: POSIX sigwait() replacement
 *****************************************************************************
 * Copyright © 2017 VLC authors and VideoLAN
 *
 * Author: Julian Scheel <julian@jusst.de>
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
# include <config.h>
#endif

#ifdef __native_client__
/* NaCl has no working sigwait, but SIGPIPE, for which vlc uses sigwait
 * currently, is never generated in NaCl. So for SIGPIPE it's safe to instantly
 * return, for all others run into an assertion. */

#include <assert.h>
#include <signal.h>

int sigwait(const sigset_t *set, int *sig)
{
    sigset_t s = *set;
    if (sigemptyset(&s))
        return 0;
    assert(sigismember(&s, SIGPIPE));
    sigdelset(&s, SIGPIPE);
    assert(sigemptyset(&s));

    *sig = SIGPIPE;
    return 0;
}
#else
# error sigwait not implemented on your platform!
#endif
