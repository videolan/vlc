/**
 * @file rtpfmt_test.c
 */
/*****************************************************************************
 * Copyright © 2021 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#undef NDEBUG
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "../sdp.h"
#include <vlc_common.h>
#include <vlc_block.h>
#include "../rtp.h"

const char vlc_module_name[] = "rtpfmt_test";

static const struct vlc_rtp_pt_operations ops = {
    NULL, NULL, NULL, NULL,
};

int vlc_rtp_pt_instantiate(vlc_object_t *obj, struct vlc_rtp_pt *restrict pt,
                           const struct vlc_sdp_pt *restrict desc)
{
    assert(obj == NULL);
    assert(pt != NULL);
    assert(desc != NULL);
    pt->ops = &ops;
    return 0;
}

uint8_t ptmask[16];

int rtp_add_type(rtp_session_t *s, struct vlc_rtp_pt *pt)
{
    unsigned int num = pt->number;

    assert(s == NULL);
    assert(num < 128);
    assert((ptmask[num / 8] >> (num % 8)) & 1);
    ptmask[num / 8] &= ~(1 << (num % 8));
    vlc_rtp_pt_release(pt);
    return 0;
}

static const struct vlc_rtp_pt_owner owner = {
    NULL, NULL,
};

#define EXPECT_PTS(...) \
{ \
     static const unsigned char nums[] = { __VA_ARGS__ }; \
     for (size_t i = 0; i < sizeof (ptmask); i++) \
         assert(ptmask[i] == 0); \
     for (int i = 0; i < (int)ARRAY_SIZE(nums); i++) \
         ptmask[nums[i] / 8] |= 1 << (nums[i] % 8); \
}

int main(void)
{
    static const char sdptext[] =
        "v=0\r\n"
        "o=- 1 1 IN IP4 192.0.2.1\r\n"
        "s=Title here\r\n"
        "t=0 0\r\n"
        "m=audio 5004 RTP/AVP 0 3 8 12 14 33 96 97\r\n"
        "a=rtpmap:96 L16/44100/6\r\n"
        "a=rtpmap:97 L16/44100\r\n"
        "m=video 5006 RTP/AVP 32 33 98\r\n"
        "a=rtpmap:98 H264/90000\r\n"
        "a=fmtp:98 profile-level-id=42A01E; packetization-mode=1\r\n"
        "m=text 5008 RTP/AVP 127\r\n"
        "a=rtpmap:127 t140/1000\r\n"
        "m=naughty 5010 RTP/AVP 64 64 foobar 128 255\r\n"
        "a=rtpmap:64 invalid/0/0\r\n"
        "m=evil 5010 RTP/AVP 42 128 255\r\n"
        "a=rtpmap:128 overflow/90000\r\n";

    int val;

    struct vlc_sdp *sdp = vlc_sdp_parse(sdptext, strlen(sdptext));
    assert(sdp != NULL);

    /* audio */
    struct vlc_sdp_media *media = sdp->media;
    assert(media != NULL);
    EXPECT_PTS(0, 3, 8, 12, 14, 33, 96, 97);
    val = vlc_rtp_add_media_types(NULL, NULL, media, &owner);
    assert(val == 0);

    /* video */
    media = media->next;
    assert(media != NULL);
    EXPECT_PTS(32, 33, 98);
    val = vlc_rtp_add_media_types(NULL, NULL, media, &owner);
    assert(val == 0);
    /* text */
    media = media->next;
    assert(media != NULL);
    EXPECT_PTS(127);
    val = vlc_rtp_add_media_types(NULL, NULL, media, &owner);
    assert(val == 0);
    /* naughty */
    media = media->next;
    assert(media != NULL);
    EXPECT_PTS();
    vlc_rtp_add_media_types(NULL, NULL, media, &owner);
    /* evil */
    media = media->next;
    assert(media != NULL);
    EXPECT_PTS();
    vlc_rtp_add_media_types(NULL, NULL, media, &owner);

    media = media->next;
    EXPECT_PTS();
    assert(media == NULL);
    vlc_sdp_free(sdp);
    return 0;
}
