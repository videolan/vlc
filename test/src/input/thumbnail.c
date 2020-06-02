/*****************************************************************************
 * thumbnail.c: test thumbnailing API
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
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

#include "../../libvlc/test.h"
#include "../lib/libvlc_internal.h"

#include <vlc_common.h>
#include <vlc_thumbnailer.h>
#include <vlc_input_item.h>
#include <vlc_picture.h>

#include <errno.h>

#define MOCK_DURATION VLC_TICK_FROM_SEC( 5 * 60 )

const struct
{
    uint32_t i_nb_video_tracks;
    uint32_t i_nb_audio_tracks;
    vlc_tick_t i_add_video_track_at;
    vlc_tick_t i_time;
    float f_pos;
    bool b_use_pos;
    bool b_fast_seek;
    vlc_tick_t i_timeout;
    bool b_expected_success;
} test_params[] = {
    /* Simple test with a thumbnail at 60s, with a video track */
    { 1, 0, VLC_TICK_INVALID, VLC_TICK_FROM_SEC( 60 ), .0f, false, true,
        VLC_TICK_FROM_SEC( 1 ), true },
    /* Test without fast-seek */
    { 1, 0, VLC_TICK_INVALID, VLC_TICK_FROM_SEC( 60 ), .0f, false, false,
        VLC_TICK_FROM_SEC( 1 ), true },
    /* Seek by position test */
    { 1, 0, VLC_TICK_INVALID, 0, .3f, true, true, VLC_TICK_FROM_SEC( 1 ), true },
    /* Seek at a negative position */
    { 1, 0, VLC_TICK_INVALID, -12345, .0f, false, true, VLC_TICK_FROM_SEC( 1 ), true },
    /* Take a thumbnail of a file without video, which should timeout. */
    { 0, 1, VLC_TICK_INVALID, VLC_TICK_FROM_SEC( 60 ), .0f, false, true, VLC_TICK_FROM_MS( 100 ), false },
    /* Take a thumbnail of a file with a video track starting later */
    { 1, 1, VLC_TICK_FROM_SEC( 120 ), VLC_TICK_FROM_SEC( 60 ), .0f, false, true,
        VLC_TICK_FROM_SEC( 2 ), true },
};

struct test_ctx
{
    vlc_cond_t cond;
    vlc_mutex_t lock;
    size_t test_idx;
    bool b_done;
};

static void thumbnailer_callback( void* data, picture_t* thumbnail )
{
    struct test_ctx* p_ctx = data;
    vlc_mutex_lock( &p_ctx->lock );

    if ( thumbnail != NULL )
    {
        assert( test_params[p_ctx->test_idx].b_expected_success &&
                "Expected failure but got a thumbnail" );
        assert( thumbnail->format.i_chroma == VLC_CODEC_ARGB );

        /* TODO: Enable this once the new clock is merged */
#if 0
        vlc_tick_t expected_date;
        /* Don't rely on the expected date if it was purposely invalid */
        if ( test_params[p_ctx->test_idx].b_use_pos == true )
            expected_date = MOCK_DURATION * test_params[p_ctx->test_idx].f_pos;
        else if ( test_params[p_ctx->test_idx].i_add_video_track_at != VLC_TICK_INVALID )
            expected_date = test_params[p_ctx->test_idx].i_add_video_track_at;
        else
        {
            if ( test_params[p_ctx->test_idx].i_time < 0 )
                expected_date = VLC_TICK_0;
            else
                expected_date = test_params[p_ctx->test_idx].i_time;
        }
        assert( thumbnail->date == expected_date && "Unexpected picture date");
#endif
    }
    else
        assert( !test_params[p_ctx->test_idx].b_expected_success &&
                "Expected a thumbnail but got a failure" );

    p_ctx->b_done = true;
    vlc_cond_signal( &p_ctx->cond );
    vlc_mutex_unlock( &p_ctx->lock );
}

static void test_thumbnails( libvlc_instance_t* p_vlc )
{
    vlc_thumbnailer_t* p_thumbnailer = vlc_thumbnailer_Create(
                VLC_OBJECT( p_vlc->p_libvlc_int ) );
    assert( p_thumbnailer != NULL );

    struct test_ctx ctx;
    vlc_cond_init( &ctx.cond );
    vlc_mutex_init( &ctx.lock );

    for ( size_t i = 0; i < ARRAY_SIZE(test_params); ++i)
    {
        char* psz_mrl;

        ctx.test_idx = i;
        ctx.b_done = false;

        if ( asprintf( &psz_mrl, "mock://video_track_count=%u;audio_track_count=%u"
                       ";length=%" PRId64 ";video_chroma=ARGB;video_add_track_at=%" PRId64,
                       test_params[i].i_nb_video_tracks,
                       test_params[i].i_nb_audio_tracks, MOCK_DURATION,
                       test_params[i].i_add_video_track_at ) < 0 )
            assert( !"Failed to allocate mock mrl" );
        input_item_t* p_item = input_item_New( psz_mrl, "mock item" );
        assert( p_item != NULL );

        vlc_mutex_lock( &ctx.lock );
        int res = 0;

        if ( test_params[i].b_use_pos )
        {
            vlc_thumbnailer_RequestByPos( p_thumbnailer, test_params[i].f_pos,
                test_params[i].b_fast_seek ?
                    VLC_THUMBNAILER_SEEK_FAST : VLC_THUMBNAILER_SEEK_PRECISE,
                p_item, test_params[i].i_timeout, thumbnailer_callback, &ctx );
        }
        else
        {
            vlc_thumbnailer_RequestByTime( p_thumbnailer, test_params[i].i_time,
                test_params[i].b_fast_seek ?
                    VLC_THUMBNAILER_SEEK_FAST : VLC_THUMBNAILER_SEEK_PRECISE,
                p_item, test_params[i].i_timeout, thumbnailer_callback, &ctx );
        }

        while ( ctx.b_done == false )
        {
            vlc_tick_t timeout = vlc_tick_now() + VLC_TICK_FROM_SEC( 1 );
            res = vlc_cond_timedwait( &ctx.cond, &ctx.lock, timeout );
            assert( res != ETIMEDOUT );
        }
        vlc_mutex_unlock( &ctx.lock );

        input_item_Release( p_item );
        free( psz_mrl );
    }
    vlc_thumbnailer_Release( p_thumbnailer );
}

static void thumbnailer_callback_cancel( void* data, picture_t* p_thumbnail )
{
    struct test_ctx* p_ctx = data;
    assert( p_thumbnail == NULL );
    vlc_mutex_lock( &p_ctx->lock );
    p_ctx->b_done = true;
    vlc_mutex_unlock( &p_ctx->lock );
    vlc_cond_signal( &p_ctx->cond );
}


static void test_cancel_thumbnail( libvlc_instance_t* p_vlc )
{
    vlc_thumbnailer_t* p_thumbnailer = vlc_thumbnailer_Create(
                VLC_OBJECT( p_vlc->p_libvlc_int ) );
    assert( p_thumbnailer != NULL );

    struct test_ctx ctx;
    vlc_cond_init( &ctx.cond );
    vlc_mutex_init( &ctx.lock );

    const char* psz_mrl = "mock://video_track_count=1;audio_track_count=1";
    input_item_t* p_item = input_item_New( psz_mrl, "mock item" );
    assert( p_item != NULL );

    vlc_mutex_lock( &ctx.lock );
    int res = 0;
    vlc_thumbnailer_request_t* p_req = vlc_thumbnailer_RequestByTime( p_thumbnailer,
        VLC_TICK_FROM_SEC( 1 ), VLC_THUMBNAILER_SEEK_PRECISE, p_item,
        VLC_TICK_INVALID, thumbnailer_callback_cancel, &ctx );
    vlc_thumbnailer_Cancel( p_thumbnailer, p_req );
    while ( ctx.b_done == false )
    {
        vlc_tick_t timeout = vlc_tick_now() + VLC_TICK_FROM_SEC( 1 );
        res = vlc_cond_timedwait( &ctx.cond, &ctx.lock, timeout );
        assert( res != ETIMEDOUT );
    }
    vlc_mutex_unlock( &ctx.lock );

    input_item_Release( p_item );

    vlc_thumbnailer_Release( p_thumbnailer );
}

int main()
{
    test_init();

    static const char * argv[] = {
        "-v",
        "--ignore-config",
    };
    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(argv), argv);
    assert(vlc);

    test_thumbnails( vlc );
    test_cancel_thumbnail( vlc );

    libvlc_release( vlc );
}
