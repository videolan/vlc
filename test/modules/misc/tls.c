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
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_tls.h>
#include <vlc_dialog.h>
#include "../../../lib/libvlc_internal.h"

#include <vlc/vlc.h>

static int tlspair(int fds[2])
{
    return vlc_socketpair(PF_LOCAL, SOCK_STREAM, 0, fds, true);
}

static void
dialog_display_question_cb(void *p_data, vlc_dialog_id *p_id, const char *psz_title,
                           const char *psz_text, vlc_dialog_question_type i_type,
                           const char *psz_cancel, const char *psz_action1,
                           const char *psz_action2)
{
    (void) psz_title;
    (void) psz_text;
    (void) i_type;
    (void) psz_cancel;
    (void) psz_action1;
    (void) psz_action2;
    int *value = p_data;
    vlc_dialog_id_post_action(p_id, *value);
}

static void dialog_cancel_cb(void *p_data, vlc_dialog_id *id)
{
    (void)p_data;
    vlc_dialog_id_dismiss(id);
}

static libvlc_instance_t *vlc;
static vlc_object_t *obj;
static vlc_tls_creds_t *server_creds;
static vlc_tls_creds_t *client_creds;

static void *tls_echo(void *data)
{
    vlc_tls_t *tls = data;
    struct pollfd ufd;
    ssize_t val;
    char buf[256];

    ufd.fd = vlc_tls_GetFD(tls);

    while ((val = vlc_tls_SessionHandshake(server_creds, tls)) > 0)
    {
        switch (val)
        {
            case 1:  ufd.events = POLLIN;  break;
            case 2:  ufd.events = POLLOUT; break;
            default: vlc_assert_unreachable();
        }
        poll(&ufd, 1, -1);
    }

    if (val < 0)
        goto error;

    while ((val = vlc_tls_Read(tls, buf, sizeof (buf), false)) > 0)
        if (vlc_tls_Write(tls, buf, val) < val)
            goto error;

    if (val < 0 || vlc_tls_Shutdown(tls, false))
        goto error;

    vlc_tls_Close(tls);
    return tls;
error:
    vlc_tls_Close(tls);
    return NULL;
}

static int securepair(vlc_thread_t *th, vlc_tls_t **restrict client,
                      const char *const *alpnv[2], char **restrict alp)
{
    vlc_tls_t *server;
    int val;
    int insecurev[2];

    val = tlspair(insecurev);
    assert(val == 0);

    server = vlc_tls_ServerSessionCreate(server_creds, insecurev[0], alpnv[0]);
    assert(server != NULL);

    val = vlc_clone(th, tls_echo, server, VLC_THREAD_PRIORITY_LOW);
    assert(val == 0);

    *client = vlc_tls_ClientSessionCreateFD(client_creds, insecurev[1],
                                          "localhost", "vlc-tls-test",
                                          alpnv[1], alp);
    if (*client == NULL)
    {
        val = vlc_close(insecurev[1]);
        assert(val == 0);
        vlc_join(*th, NULL);
        return -1;
    }
    return 0;
}

static const char certpath[] = SRCDIR"/modules/misc/certkey.pem";
static const char *const alpn[] = { "foo", "bar", NULL };

int main(void)
{
    int val;
    int answer = 0;

    /* Create fake home for stored keys */
    char homedir[] = "/tmp/vlc-test-XXXXXX";
    if (mkdtemp(homedir) != homedir)
    {
        perror("Temporary directory");
        return 77;
    }

    assert(!strncmp(homedir, "/tmp/vlc-test-", 14));
    setenv("HOME", homedir, 1);
    setenv("VLC_PLUGIN_PATH", "../modules", 1);

    vlc = libvlc_new(0, NULL);
    assert(vlc != NULL);
    obj = VLC_OBJECT(vlc->p_libvlc_int);

    server_creds = vlc_tls_ServerCreate(obj, SRCDIR"/nonexistent", NULL);
    assert(server_creds == NULL);
    server_creds = vlc_tls_ServerCreate(obj, SRCDIR"/samples/empty.voc", NULL);
    assert(server_creds == NULL);
    server_creds = vlc_tls_ServerCreate(obj, certpath, SRCDIR"/nonexistent");
    assert(server_creds == NULL);
    server_creds = vlc_tls_ServerCreate(obj, certpath, NULL);
    if (server_creds == NULL)
    {
        libvlc_release(vlc);
        return 77;
    }

    client_creds = vlc_tls_ClientCreate(obj);
    assert(client_creds != NULL);

    vlc_dialog_cbs cbs = {
        .pf_display_question = dialog_display_question_cb,
        .pf_cancel = dialog_cancel_cb,
    };
    vlc_dialog_provider_set_callbacks(obj, &cbs, &answer);

    vlc_thread_t th;
    vlc_tls_t *tls;
    const char *const *alpnv[2] = { alpn + 1, alpn };
    char *alp;
    void *p;

    /* Test unknown certificate */
    answer = 0;
    val = securepair(&th, &tls, alpnv, &alp);
    assert(val == -1);

    /* Accept unknown certificate */
    answer = 1;
    val = securepair(&th, &tls, alpnv, &alp);
    assert(val == 0);
    assert(alp != NULL);
    assert(!strcmp(alp, "bar"));
    free(alp);

    /* Do some I/O */
    char buf[12];
    struct iovec iov;

    iov.iov_base = buf;
    iov.iov_len = sizeof (buf);
    val = tls->readv(tls, &iov, 1);
    assert(val == -1 && errno == EAGAIN);

    val = vlc_tls_Write(tls, "Hello ", 6);
    assert(val == 6);
    val = vlc_tls_Write(tls, "world!", 6);
    assert(val == 6);

    val = vlc_tls_Read(tls, buf, sizeof (buf), true);
    assert(val == 12);
    assert(!memcmp(buf, "Hello world!", 12));

    val = vlc_tls_Shutdown(tls, false);
    assert(val == 0);
    vlc_join(th, &p);
    assert(p != NULL);
    val = vlc_tls_Read(tls, buf, sizeof (buf), false);
    assert(val == 0);
    vlc_tls_Close(tls);

    /* Test known certificate, ignore ALPN result */
    answer = 0;
    val = securepair(&th, &tls, alpnv, NULL);
    assert(val == 0);

    /* Do a lot of I/O, test congestion handling */
    static unsigned char data[16184];
    size_t bytes = 0;
    unsigned seed = 0;

    iov.iov_base = data;
    iov.iov_len = sizeof (data);

    do
    {
        for (size_t i = 0; i < sizeof (data); i++)
            data[i] = rand_r(&seed);
        bytes += sizeof (data);
    }
    while ((val = tls->writev(tls, &iov, 1)) == sizeof (data));

    bytes -= sizeof (data);
    if (val > 0)
        bytes += val;

    fprintf(stderr, "Sent %zu bytes.\n", bytes);
    seed = 0;

    while (bytes > 0)
    {
        unsigned char c = rand_r(&seed);

        val = vlc_tls_Read(tls, buf, 1, false);
        assert(val == 1);
        assert(c == (unsigned char)buf[0]);
        bytes--;
    }

    vlc_tls_Close(tls);
    vlc_join(th, NULL);

    /* Test known certificate, no ALPN */
    alpnv[0] = alpnv[1] = NULL;
    val = securepair(&th, &tls, alpnv, NULL);
    assert(val == 0);
    vlc_tls_Close(tls);
    vlc_join(th, NULL);

    vlc_dialog_provider_set_callbacks(obj, NULL, NULL);
    vlc_tls_Delete(client_creds);
    vlc_tls_Delete(server_creds);
    libvlc_release(vlc);

    if (fork() == 0)
        execlp("rm", "rm", "-rf", homedir, (char *)NULL);
    return 0;
}
