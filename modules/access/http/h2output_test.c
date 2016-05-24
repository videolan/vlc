/*****************************************************************************
 * h2output_test.c: HTTP/2 send queue test
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
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
# include <config.h>
#endif

#undef NDEBUG

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <vlc_common.h>
#include <vlc_tls.h>
#include "h2frame.h"
#include "h2output.h"

#undef msleep

static unsigned char counter = 0;
static bool send_failure = false;
static bool expect_hello = true;
static vlc_sem_t rx;

static int fd_callback(vlc_tls_t *tls)
{
    (void) tls;
    return fileno(stderr); /* should be writable (at least most of the time) */
}

static ssize_t send_callback(vlc_tls_t *tls, const struct iovec *iov,
                             unsigned count)
{
    assert(count == 1);
    assert(tls->writev == send_callback);

    const uint8_t *p = iov->iov_base;
    size_t len = iov->iov_len;

    if (expect_hello)
    {
        assert(len >= 24);
        assert(!memcmp(p, "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n", 24));

        expect_hello = false;
        vlc_sem_post(&rx);

        if (len == 24)
            return send_failure ? -1 : (ssize_t)len;

        p += 24;
        len -= 24;
    }

    assert(len == 9 + 1);
    assert(p[9] == counter);

    if (send_failure)
        errno = EIO;
    else
        counter++;
    vlc_sem_post(&rx);

    return send_failure ? -1 : (ssize_t)len;
}

static vlc_tls_t fake_tls =
{
    .get_fd = fd_callback,
    .writev = send_callback,
};

static struct vlc_h2_frame *frame(unsigned char c)
{
    struct vlc_h2_frame *f = vlc_h2_frame_data(1, &c, 1, false);
    assert(f != NULL);
    return f;
}

static struct vlc_h2_frame *frame_list(struct vlc_h2_frame *first, ...)
{
    struct vlc_h2_frame **pp = &first;
    va_list ap;

    va_start(ap, first);
    for (struct vlc_h2_frame *f = first;
         f != NULL;
         f = va_arg(ap, struct vlc_h2_frame *))
    {
        *pp = f;
        pp = &f->next;
    }
    va_end(ap);
    return first;
}

int main(void)
{
    struct vlc_h2_output *out;

    /* Dummy */
    out = vlc_h2_output_create(&fake_tls, false);
    assert(out != NULL);
    vlc_h2_output_destroy(out);

    vlc_sem_init(&rx, 0);
    out = vlc_h2_output_create(&fake_tls, expect_hello = true);
    assert(out != NULL);
    vlc_h2_output_destroy(out);
    vlc_sem_destroy(&rx);

    /* Success */
    vlc_sem_init(&rx, 0);
    out = vlc_h2_output_create(&fake_tls, false);
    assert(out != NULL);
    assert(vlc_h2_output_send_prio(out, NULL) == -1);
    assert(vlc_h2_output_send_prio(out, frame(0)) == 0);
    assert(vlc_h2_output_send_prio(out, frame(1)) == 0);
    assert(vlc_h2_output_send(out, NULL) == -1);
    assert(vlc_h2_output_send(out, frame(2)) == 0);
    assert(vlc_h2_output_send(out, frame(3)) == 0);
    assert(vlc_h2_output_send(out, frame_list(frame(4), frame(5),
                                              frame(6), NULL)) == 0);
    assert(vlc_h2_output_send(out, frame(7)) == 0);
    for (unsigned i = 0; i < 8; i++)
        vlc_sem_wait(&rx);

    assert(vlc_h2_output_send_prio(out, frame(8)) == 0);
    assert(vlc_h2_output_send(out, frame(9)) == 0);

    vlc_h2_output_destroy(out);
    vlc_sem_destroy(&rx);

    /* Failure */
    send_failure = true;

    vlc_sem_init(&rx, 0);
    counter = 10;
    out = vlc_h2_output_create(&fake_tls, false);
    assert(out != NULL);

    assert(vlc_h2_output_send(out, frame(10)) == 0);
    for (unsigned char i = 11; vlc_h2_output_send(out, frame(i)) == 0; i++)
        msleep(CLOCK_FREQ/10); /* eventually, it should start failing */
    assert(vlc_h2_output_send(out, frame(0)) == -1);
    assert(vlc_h2_output_send_prio(out, frame(0)) == -1);
    vlc_h2_output_destroy(out);
    vlc_sem_destroy(&rx);

    /* Failure during hello */
    vlc_sem_init(&rx, 0);
    counter = 0;
    out = vlc_h2_output_create(&fake_tls, expect_hello = true);
    assert(out != NULL);
    vlc_sem_wait(&rx);

    for (unsigned char i = 1; vlc_h2_output_send_prio(out, frame(i)) == 0; i++)
        msleep(CLOCK_FREQ/10);
    assert(vlc_h2_output_send(out, frame(0)) == -1);
    assert(vlc_h2_output_send_prio(out, frame(0)) == -1);
    vlc_h2_output_destroy(out);
    vlc_sem_destroy(&rx);

    return 0;
}
