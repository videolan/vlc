/*****************************************************************************
 * ancillary.c: test for ancillary
 *****************************************************************************
 * Copyright (C) 2021 the VideoLAN team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_frame.h>
#include <vlc_picture.h>
#include <vlc_ancillary.h>

#include <assert.h>

static void
ancillary_free(void *data)
{
    assert(strncmp(data, "test", 4) == 0);
    free(data);
}

int main( void )
{
    vlc_frame_t *frame = vlc_frame_Alloc(1);
    assert(frame);

    int ret;
    struct vlc_ancillary *ancillary;

    /* Create and try to attach 3 ancillaries to the frame, only 2 will be
     * attached. */
    for (size_t i = 0; i < 3; ++i)
    {
        char *data;
        ret = asprintf(&data, "test%zu", i);
        assert(ret > 0);

        vlc_ancillary_id id;
        switch (i)
        {
            case 0:
                /* Check that only one ancillary of the same id is added (the
                 * last one take precedence). */
            case 1: id = VLC_ANCILLARY_ID('t','s','t','1'); break;
            case 2: id = VLC_ANCILLARY_ID('t','s','t','2'); break;
            default: vlc_assert_unreachable();
        }

        ancillary = vlc_ancillary_CreateWithFreeCb(data, id, ancillary_free);
        assert(ancillary);

        ret = vlc_frame_AttachAncillary(frame, ancillary);
        assert(ret == VLC_SUCCESS);
        vlc_ancillary_Release(ancillary);
    }

    /* Check that ancillaries are copied via a vlc_frame_CopyProperties() (done
     * by vlc_frame_Duplicate()). */
    vlc_frame_t *copy_frame = vlc_frame_Duplicate(frame);
    assert(copy_frame);
    vlc_frame_Release(frame);
    frame = copy_frame;

    picture_t *picture = picture_New(VLC_CODEC_NV12, 1, 1, 1, 1);
    assert(picture);

    /* Manually attach both ancillaries to a newly allocated picture. */
    ancillary = vlc_frame_GetAncillary(frame, VLC_ANCILLARY_ID('t','s','t','1'));
    assert(ancillary);
    picture_AttachAncillary(picture, ancillary);

    ancillary = vlc_frame_GetAncillary(frame, VLC_ANCILLARY_ID('t','s','t','2'));
    assert(ancillary);
    picture_AttachAncillary(picture, ancillary);

    vlc_frame_Release(frame);

    /* Check that ancillaries are copied via a picture_Clone(). */
    picture_t *clone = picture_Clone(picture);
    assert(clone);
    picture_Release(picture);

    /* Check that ancillaries are copied via a picture_Copy(). */
    picture_t *copy = picture_New(VLC_CODEC_I420, 1, 1, 1, 1);
    assert(copy);
    picture_Copy(copy, clone);
    picture_Release(clone);

    /* Check that ancillaries are still valid. */
    ancillary = picture_GetAncillary(copy, VLC_ANCILLARY_ID('t','s','t','1'));
    assert(ancillary);
    assert(strcmp("test1", vlc_ancillary_GetData(ancillary)) == 0);

    ancillary = picture_GetAncillary(copy, VLC_ANCILLARY_ID('t','s','t','2'));
    assert(ancillary);
    assert(strcmp("test2", vlc_ancillary_GetData(ancillary)) == 0);

    picture_Release(copy);

    return 0;
}
