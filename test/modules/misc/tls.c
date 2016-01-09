/*****************************************************************************
 * tls.c: Transport Layer Security module test
 *****************************************************************************
 * Copyright © 2016 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
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

#undef NDEBUG
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#ifndef SOCK_CLOEXEC
# define SOCK_CLOEXEC 0
#endif
#include <poll.h>
#include <fcntl.h>

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_tls.h>
#include <vlc_dialog.h>
#include "../../lib/libvlc_internal.h"

#include <vlc/vlc.h>

static int tlspair(int fds[2])
{
    if (socketpair(PF_LOCAL, SOCK_STREAM|SOCK_CLOEXEC, 0, fds))
        return -1;

    fcntl(fds[0], F_SETFL, O_NONBLOCK | fcntl(fds[0], F_GETFL));
    fcntl(fds[1], F_SETFL, O_NONBLOCK | fcntl(fds[1], F_GETFL));
    return 0;
}

static int question_callback(vlc_object_t *obj, const char *varname,
                             vlc_value_t old, vlc_value_t cur, void *data)
{
    dialog_question_t *q = cur.p_address;
    int *value = data;

    q->answer = *value;

    assert(obj == VLC_OBJECT(obj->p_libvlc));
    assert(!strcmp(varname, "dialog-question"));
    (void) old;
    return VLC_SUCCESS;
}

static libvlc_instance_t *vlc;
static vlc_object_t *obj;
static vlc_tls_creds_t *server;
static vlc_tls_creds_t *client;

static void *tls_handshake(void *data)
{
    vlc_tls_t *tls = data;
    struct pollfd ufd;
    int val;

    ufd.fd = tls->fd;

    while ((val = vlc_tls_SessionHandshake(server, tls)) > 0)
    {
        switch (val)
        {
            case 1:  ufd.events = POLLIN;  break;
            case 2:  ufd.events = POLLOUT; break;
            default: vlc_assert_unreachable();
        }
        poll(&ufd, 1, -1);
    }

    return val == 0 ? tls : NULL;
}

static const char certpath[] = SRCDIR"/modules/misc/certkey.pem";
static const char *const alpn[] = { "foo", "bar", NULL };

int main(void)
{
    vlc_thread_t th;
    int insecurev[2];
    vlc_tls_t *securev[2];
    char *alp;
    void *p;
    int val;
    int answer = 0;

    setenv("VLC_PLUGIN_PATH", "../modules", 1);

    vlc = libvlc_new(0, NULL);
    assert(vlc != NULL);
    obj = VLC_OBJECT(vlc->p_libvlc_int);

    var_Create(obj, "dialog-question", VLC_VAR_ADDRESS);
    var_AddCallback(obj, "dialog-question", question_callback, &answer);
    dialog_Register(obj);

    val = tlspair(insecurev);
    assert(val == 0);

    server = vlc_tls_ServerCreate(obj, SRCDIR"/does/not/exist", NULL);
    assert(server == NULL);
    server = vlc_tls_ServerCreate(obj, SRCDIR"/samples/empty.voc", NULL);
    assert(server == NULL);
    server = vlc_tls_ServerCreate(obj, certpath, SRCDIR"/does/not/exist");
    assert(server == NULL);
    server = vlc_tls_ServerCreate(obj, certpath, NULL);
    if (server == NULL)
    {
        val = 77;
        goto out;
    }

    assert(server != NULL);

    client = vlc_tls_ClientCreate(obj);
    assert(client != NULL);

    securev[0] = vlc_tls_SessionCreate(server, insecurev[0], NULL, alpn + 1);
    assert(securev[0] != NULL);

    val = vlc_clone(&th, tls_handshake, securev[0], VLC_THREAD_PRIORITY_LOW);
    assert(val == 0);

    answer = 2;

    securev[1] = vlc_tls_ClientSessionCreate(client, insecurev[1], "localhost",
                                             "vlc-test-XXX", alpn, &alp);
    assert(securev[1] != NULL);
    assert(alp != NULL);
    assert(!strcmp(alp, "bar"));
    free(alp);

    vlc_join(th, &p);
    assert(p == securev[0]);

    char buf[12];

    val = securev[1]->recv(securev[1], buf, sizeof (buf));
    assert(val == -1 && errno == EAGAIN);

    val = vlc_tls_Write(securev[0], "Hello ", 6);
    assert(val == 6);
    val = vlc_tls_Write(securev[0], "world!", 6);
    assert(val == 6);

    val = vlc_tls_Read(securev[1], buf, sizeof (buf), true);
    assert(val == 12);
    assert(!memcmp(buf, "Hello world!", 12));

    val = vlc_tls_Shutdown(securev[0], false);
    assert(val == 0);
    val = vlc_tls_Read(securev[1], buf, sizeof (buf), false);
    assert(val == 0);
    val = vlc_tls_Shutdown(securev[1], true);
    assert(val == 0);

    vlc_tls_SessionDelete(securev[1]);
    vlc_tls_SessionDelete(securev[0]);

    vlc_tls_Delete(client);
    vlc_tls_Delete(server);

    val = 0;
out:
    dialog_Unregister(obj);
    var_DelCallback(obj, "dialog-question", question_callback, &answer);
    libvlc_release(vlc);
    return val;
}
