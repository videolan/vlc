/*****************************************************************************
 * missing.c: missing libvlccore symbols
 *****************************************************************************
 * Copyright (C) 2008-2011 Rémi Denis-Courmont
 * Copyright (C) 2009-2014 VLC authors and VideoLAN
 *
 * Authors: Rémi Denis-Courmont
 *          Pierre Ynard <linkfanel # yahoo fr>
 *          Toralf Niebuhr <gmthor85 # aim com>
 *          Felix Paul Kühne <fkuehne # videolan org>
 *          Jean-Paul Saman <jpsaman # videolan org>
 *          Antoine Cellerier <dionoea # videolan org>
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

/** \file
 * This file contains dummy replacement API for disabled features
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <assert.h>

#ifndef ENABLE_VLM
# include <vlc_vlm.h>

_Noreturn int vlm_Control(vlm_t *vlm, int query, ...)
{
    VLC_UNUSED (query);
    VLC_UNUSED (vlm);
    vlc_assert_unreachable ();
}

_Noreturn void vlm_Delete(vlm_t *vlm)
{
    VLC_UNUSED (vlm);
    vlc_assert_unreachable ();
}

_Noreturn int vlm_ExecuteCommand(vlm_t *vlm, const char *cmd,
                                vlm_message_t **pm)
{
    VLC_UNUSED (vlm);
    VLC_UNUSED (cmd);
    VLC_UNUSED (pm);
    vlc_assert_unreachable ();
}

_Noreturn vlm_message_t *vlm_MessageAdd(vlm_message_t *a, vlm_message_t *b)
{
    VLC_UNUSED (a);
    VLC_UNUSED (b);
    vlc_assert_unreachable ();
}

_Noreturn void vlm_MessageDelete(vlm_message_t *m)
{
    VLC_UNUSED (m);
    vlc_assert_unreachable ();
}

vlm_message_t *vlm_MessageSimpleNew (const char *a)
{
    VLC_UNUSED (a);
    return NULL;
}

vlm_message_t *vlm_MessageNew (const char *a, const char *fmt, ...)
{
    VLC_UNUSED (a);
    VLC_UNUSED (fmt);
    return vlm_MessageSimpleNew (a);
}

#undef vlm_New
vlm_t *vlm_New (libvlc_int_t *obj, const char *file)
{
     msg_Err (obj, "VLM not compiled-in!");
     (void) file;
     return NULL;
}
#endif /* !ENABLE_VLM */

#ifndef UPDATE_CHECK
# include <vlc_update.h>

update_t *(update_New)(vlc_object_t *obj)
{
    (void) obj;
    return NULL;
}

_Noreturn void update_Delete(update_t *u)
{
    (void) u;
    vlc_assert_unreachable();
}

_Noreturn void update_Check(update_t *u, void (*cb)(void *, bool), void *opaque)
{
    (void) u; (void) cb; (void) opaque;
    vlc_assert_unreachable();
}

_Noreturn bool update_NeedUpgrade(update_t *u)
{
    (void) u;
    vlc_assert_unreachable();
}

_Noreturn void update_Download(update_t *u, const char *dir)
{
    (void) u; (void) dir;
    vlc_assert_unreachable();
}

_Noreturn update_release_t *update_GetRelease(update_t *u)
{
    (void) u;
    vlc_assert_unreachable();
}
#endif /* !UPDATE_CHECK */

#include <vlc_threads.h>
#if defined(LIBVLC_USE_PTHREAD_CLEANUP)
_Noreturn void vlc_control_cancel (vlc_cleanup_t *cleaner)
{
    (void) cleaner;
    vlc_assert_unreachable ();
}
#endif

#if !defined(_WIN32)
# define HAVE_WEAK_SPAWN
#else
# if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#  define HAVE_WEAK_SPAWN
# endif
#endif

#ifdef HAVE_WEAK_SPAWN
#include <errno.h>
#include <vlc_spawn.h>

VLC_WEAK
int vlc_spawn(pid_t *pid, const char *file, const int *fds,
              const char *const *args)
{
    (void) pid; (void) file; (void) fds; (void) args;
    return ENOSYS;
}

VLC_WEAK
int vlc_spawnp(pid_t *pid, const char *path, const int *fds,
               const char *const *args)
{
    (void) pid; (void) path; (void) fds; (void) args;
    return ENOSYS;
}

VLC_WEAK
int vlc_waitpid(pid_t pid)
{
    (void) pid;
    vlc_assert_unreachable();
}
#endif
