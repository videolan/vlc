/*****************************************************************************
 * Copyright (C) 2023 Videolabs
 *
 * Authors: Maxime Chapelet <umxprime # videolabs.io>

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

#include "libvlc/test.h"
#include <vlc_player.h>
#include <vlc_vout_display.h>
#include <vlc_interface.h>
#include <vlc_picture.h>
#include <vlc_es.h>
#include <vlc/libvlc.h>
#include "../lib/libvlc_internal.h"
#include "../src/video_output/vout_scheduler.h"

struct vlc_clock_t {};

typedef struct test_ctx_t test_ctx_t;
typedef struct test_step_t test_step_t;
typedef struct test_scenario_t test_scenario_t;
typedef enum test_ctx_state_t test_ctx_state_t;
typedef enum test_step_kind_t test_step_kind_t;

typedef struct {
    vlc_tick_t now;
    vlc_tick_t rate;
    pthread_mutex_t mtx;
} test_system_clock_t;

test_system_clock_t system_clock;

enum test_ctx_state_t {
    IDLE = 0,
    STARTED,
    FINISHED
};

struct test_scenario_t {
    test_step_t *steps;
    size_t steps_count;
    const char* name;
};

#define SCENARIO(a) \
{ \
    .steps = a, \
    .steps_count = ARRAY_SIZE(a), \
    .name = #a \
}

struct test_ctx_t {
    struct vlc_object_t obj;
    vlc_mutex_t step_mtx;
    vlc_cond_t step_scheduler_cond;
    vlc_cond_t step_test_cond;
    
    test_scenario_t *scenario;
    size_t step_index;

    struct vlc_vout_scheduler *scheduler;
    vout_thread_t *vout;
    vout_display_t *display;
    test_ctx_state_t state;
};

enum test_step_kind_t {
    SEND_VSYNC = 1,
    TICK_SYSTEM_CLOCK,
    PUT_PICTURE,
    FLUSH,
    PREPARE_PICTURE,
    PREPARE_PICTURE_NULL,
    RENDER_PICTURE,
    END
};

static char const * test_step_kind_String(test_step_kind_t kind) {
    switch (kind)
    {
    case SEND_VSYNC:
        return "SEND_VSYNC";
    case TICK_SYSTEM_CLOCK:
        return "TICK_SYSTEM_CLOCK";
    case PUT_PICTURE:
        return "PUT_PICTURE";
    case FLUSH:
        return "FLUSH";
    case PREPARE_PICTURE:
        return "PREPARE_PICTURE";
    case PREPARE_PICTURE_NULL:
        return "PREPARE_PICTURE_NULL";
    case RENDER_PICTURE:
        return "RENDER_PICTURE";
    case END:
        return "END";
    default:
        return NULL;
    }
}

struct test_step_t {
    test_step_kind_t kind;
    void (*execute)(test_ctx_t *ctx, test_step_t *step);
    union
    {
        struct {
            vlc_tick_t ts;
            struct vlc_vout_scheduler* scheduler;
        } send_vsync;
        struct {
            picture_t *picture;
        } put_picture;
        struct {
            vlc_tick_t date;
            picture_t *picture;
        } prepare;
        struct {
            picture_t *picture;
            vlc_tick_t expected_date;
        } render;
        struct {
            vlc_tick_t now;
        } tick_system_clock;
    } data;
    
};

static void test_step_SendVSync(test_ctx_t *ctx, test_step_t *step) {
    vlc_tick_t ts = step->data.send_vsync.ts;
    msg_Dbg(ctx, "execute SendVSync ts:%lld", ts);
    struct vlc_vout_scheduler *scheduler = ctx->scheduler;
    assert(scheduler);
    scheduler->ops->signal_vsync(scheduler, ts);
}

static void test_step_PutPicture(test_ctx_t *ctx, test_step_t *step) {
    msg_Dbg(ctx, "execute PutPicture");
    struct vlc_vout_scheduler *scheduler = ctx->scheduler;
    assert(scheduler);
    scheduler->ops->put_picture(scheduler, step->data.put_picture.picture);
}

static void test_step_Flush(test_ctx_t *ctx, test_step_t *step) {
    msg_Dbg(ctx, "execute Flush");
    struct vlc_vout_scheduler *scheduler = ctx->scheduler;
    assert(scheduler);
    scheduler->ops->flush(scheduler, vlc_tick_now());
}

static void test_step_TickSystemClock(test_ctx_t *ctx, test_step_t *step) {
    pthread_mutex_lock(&system_clock.mtx);
    system_clock.now = step->data.tick_system_clock.now;
    vlc_tick_t now = system_clock.now;
    pthread_mutex_unlock(&system_clock.mtx);
    msg_Dbg(ctx, "execute TickSystemClock now:%lld", now);
}

static void test_step_PreparePicture(test_ctx_t *ctx, test_step_t *step) {
    vlc_tick_t date = step->data.prepare.date;
    msg_Dbg(ctx, 
            "execute PreparePicture date:%lld",
            date);
    video_format_t fmt;
    video_format_Init(&fmt, VLC_CODEC_RGBA);
    picture_t *picture = picture_NewFromFormat(&fmt);
    picture->date = date;
    step->data.prepare.picture = picture;
    assert(picture);
}

static void test_step_PreparePictureNull(test_ctx_t *ctx, test_step_t *step) {
    msg_Dbg(ctx, 
            "execute PreparePictureNull");
    assert(step->data.prepare.picture == NULL);
}

static void test_step_RenderPicture(test_ctx_t *ctx, test_step_t *step) {
    msg_Dbg(ctx, 
            "execute RenderPicture date:%lld",
            step->data.render.picture->date);
    assert(step->data.render.picture);
    assert(step->data.render.expected_date == step->data.render.picture->date);
}

#define SEND_VSYNC_STEP(a) { \
    .kind = SEND_VSYNC, \
    .execute = test_step_SendVSync, \
    .data = { \
        .send_vsync = { \
            .ts = a, \
        }\
    } \
}

#define TICK_SYSTEM_CLOCK_STEP(a) {\
    .kind = TICK_SYSTEM_CLOCK, \
    .execute = test_step_TickSystemClock, \
    .data = { \
        .tick_system_clock = { \
            .now = a \
        }\
    } \
}

#define PUT_PICTURE_STEP { \
    .kind = PUT_PICTURE, \
    .execute = test_step_PutPicture, \
    .data = { \
        .put_picture = { \
            .picture = NULL, \
        }\
    } \
}

#define FLUSH_STEP { \
    .kind = FLUSH, \
    .execute = test_step_Flush \
}

#define PREPARE_PICTURE_STEP(a) { \
    .kind = PREPARE_PICTURE, \
    .execute = test_step_PreparePicture, \
    .data = { \
        .prepare = { \
            .date = a \
        }\
    } \
}

#define PREPARE_PICTURE_NULL_STEP { \
    .kind = PREPARE_PICTURE_NULL, \
    .execute = test_step_PreparePictureNull, \
    .data = { \
        .prepare = { \
            .picture = NULL \
        }\
    } \
}

#define RENDER_PICTURE_STEP(a) { \
    .kind = RENDER_PICTURE, \
    .execute = test_step_RenderPicture, \
    .data = { \
        .render = { \
            .expected_date = a \
        }\
    } \
}

#define END_STEP { \
    .kind = END, \
}


/*
CLOCK_NOW       (0)---(1)---(2)------
PUT_PICTURE     ---------------------
FLUSH           ---------------------
PREPARE_PICTURE ---------(1)---(N)---
                          |          
                           \_____    
                                  \  
                                   | 
RENDER_PICTURE  ------------------(1)
VSYNC_EVENT     ---(2)---------------
*/
test_step_t test_steps_RenderOnePicture[] =
{
    TICK_SYSTEM_CLOCK_STEP(0),
    SEND_VSYNC_STEP(1),
    TICK_SYSTEM_CLOCK_STEP(1),
    PREPARE_PICTURE_STEP(1),
    TICK_SYSTEM_CLOCK_STEP(2),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(1),
    END_STEP
};


/*
CLOCK_NOW       (0)---(1)---(2)------------(3)------
PUT_PICTURE     ------------------------------------
FLUSH           ------------------------------------
PREPARE_PICTURE ---------(1)---(N)------(3)---(N)---
                          |              |          
                           \_____         \_____    
                                  \              \ 
                                   |              | 
RENDER_PICTURE  ------------------(1)------------(3)
VSYNC_EVENT     ---(2)---------------(4)------------
*/
test_step_t test_steps_RenderTwoPictures[] =
{
    TICK_SYSTEM_CLOCK_STEP(0),
    SEND_VSYNC_STEP(2),
    TICK_SYSTEM_CLOCK_STEP(1),
    PREPARE_PICTURE_STEP(1),
    TICK_SYSTEM_CLOCK_STEP(2),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(1),
    SEND_VSYNC_STEP(4),
    PREPARE_PICTURE_STEP(3),
    TICK_SYSTEM_CLOCK_STEP(3),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(3),
    END_STEP
};


/*
CLOCK_NOW       (0)---(1)---(2)------(3)------(5)---(6)---(7)------
PUT_PICTURE     ---------------------------------------------------
FLUSH           ---------------------------------------------------
PREPARE_PICTURE ---------(2)---(N)---------(4)---------(6)---(N)---
                          |                             |          
                           \_____                        \_____    
                                  \                             \  
                                   |                             | 
RENDER_PICTURE  ------------------(2)---------------------------(6)
VSYNC_EVENT     ---(1)------------------(3)------(7)---------------
*/
test_step_t test_steps_SkipOnePicture[] =
{
    TICK_SYSTEM_CLOCK_STEP(0),
    SEND_VSYNC_STEP(1),
    TICK_SYSTEM_CLOCK_STEP(1),
    PREPARE_PICTURE_STEP(2),
    TICK_SYSTEM_CLOCK_STEP(2),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(2),
    TICK_SYSTEM_CLOCK_STEP(3),
    SEND_VSYNC_STEP(3),
    PREPARE_PICTURE_STEP(4),
    TICK_SYSTEM_CLOCK_STEP(5),
    SEND_VSYNC_STEP(7),
    TICK_SYSTEM_CLOCK_STEP(6),
    PREPARE_PICTURE_STEP(6),
    TICK_SYSTEM_CLOCK_STEP(7),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(6),
    END_STEP
};

/*
CLOCK_NOW       (0)------(1)------------(2)------------(3)------------(4)------------(5)------------(6)------------(7)------
PUT_PICTURE     ------------------------------------------------------------------------------------------------------------
FLUSH           ------------------------------------------------------------------------------------------------------------
PREPARE_PICTURE ---(1)------(N)---(2)------(N)---(3)------(N)---(4)------(N)---(5)------(N)---(6)------(N)---(7)------(N)---
                    |              |              |              |              |              |              |             
                     \________      \________      \________      \________      \________      \________      \________    
                               \              \              \              \              \              \              \  
                                |              |              |              |              |              |              | 
RENDER_PICTURE  ---------------(1)------------(2)------------(3)------------(4)------------(5)------------(6)------------(7)
VSYNC_EVENT     ------(2)------------(3)------------(4)------------(5)------------(6)------------(7)------------(8)---------
*/
test_step_t test_steps_AllVSyncEventsAfterPrepare[] =
{
    TICK_SYSTEM_CLOCK_STEP(0),

    PREPARE_PICTURE_STEP(1),
    SEND_VSYNC_STEP(2),
    TICK_SYSTEM_CLOCK_STEP(1),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(1),

    PREPARE_PICTURE_STEP(2),
    SEND_VSYNC_STEP(3),
    TICK_SYSTEM_CLOCK_STEP(2),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(2),

    PREPARE_PICTURE_STEP(3),
    SEND_VSYNC_STEP(4),
    TICK_SYSTEM_CLOCK_STEP(3),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(3),

    PREPARE_PICTURE_STEP(4),
    SEND_VSYNC_STEP(5),
    TICK_SYSTEM_CLOCK_STEP(4),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(4),

    PREPARE_PICTURE_STEP(5),
    SEND_VSYNC_STEP(6),
    TICK_SYSTEM_CLOCK_STEP(5),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(5),

    PREPARE_PICTURE_STEP(6),
    SEND_VSYNC_STEP(7),
    TICK_SYSTEM_CLOCK_STEP(6),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(6),

    PREPARE_PICTURE_STEP(7),
    SEND_VSYNC_STEP(8),
    TICK_SYSTEM_CLOCK_STEP(7),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(7),

    END_STEP
};


/*
CLOCK_NOW       (0)------(1)------------(2)------------(3)------------(4)------------(5)------------(6)------------(7)------
PUT_PICTURE     ------------------------------------------------------------------------------------------------------------
FLUSH           ------------------------------------------------------------------------------------------------------------
PREPARE_PICTURE ------(1)---(N)------(2)---(N)------(3)---(N)------(4)---(N)------(5)---(N)------(6)---(N)------(7)---(N)---
                       |              |              |              |              |              |              |          
                        \_____         \_____         \_____         \_____         \_____         \_____         \_____    
                               \              \              \              \              \              \              \  
                                |              |              |              |              |              |              | 
RENDER_PICTURE  ---------------(1)------------(2)------------(3)------------(4)------------(5)------------(6)------------(7)
VSYNC_EVENT     ---(2)------------(3)------------(4)------------(5)------------(6)------------(7)------------(8)------------
*/
test_step_t test_steps_AllVSyncEventsBeforePrepare[] =
{
    TICK_SYSTEM_CLOCK_STEP(0),

    SEND_VSYNC_STEP(2),
    PREPARE_PICTURE_STEP(1),
    TICK_SYSTEM_CLOCK_STEP(1),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(1),

    SEND_VSYNC_STEP(3),
    PREPARE_PICTURE_STEP(2),
    TICK_SYSTEM_CLOCK_STEP(2),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(2),

    SEND_VSYNC_STEP(4),
    PREPARE_PICTURE_STEP(3),
    TICK_SYSTEM_CLOCK_STEP(3),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(3),

    SEND_VSYNC_STEP(5),
    PREPARE_PICTURE_STEP(4),
    TICK_SYSTEM_CLOCK_STEP(4),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(4),

    SEND_VSYNC_STEP(6),
    PREPARE_PICTURE_STEP(5),
    TICK_SYSTEM_CLOCK_STEP(5),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(5),

    SEND_VSYNC_STEP(7),
    PREPARE_PICTURE_STEP(6),
    TICK_SYSTEM_CLOCK_STEP(6),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(6),

    SEND_VSYNC_STEP(8),
    PREPARE_PICTURE_STEP(7),
    TICK_SYSTEM_CLOCK_STEP(7),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(7),

    END_STEP
};

/*
CLOCK_NOW       (0)------(1)---------------(2)---------(3)------(4)------(5)------(6)------------(7)---------
PUT_PICTURE     ----|-----------------|-----------|--------|--------|--------|--------------|----------------
FLUSH           ---------------------------------------------------------------------------------------------
PREPARE_PICTURE ------(1)------(N)------(2)---------(3)------(4)------(5)------(6)---(N)------(7)------(N)---
                       |                                                        |              |             
                        \________                                                \_____         \________    
                                  \                                                     \                 \  
                                   |                                                     |                 | 
RENDER_PICTURE  ------------------(1)---------------------------------------------------(6)---------------(7)
VSYNC_EVENT     ------------(2)---------------(7)---------------------------------------------------(8)------
*/
test_step_t test_steps_Skip4Pictures[] =
{
    TICK_SYSTEM_CLOCK_STEP(0),

    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(1),
    TICK_SYSTEM_CLOCK_STEP(1),
    SEND_VSYNC_STEP(2),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(1),

    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(2),
    TICK_SYSTEM_CLOCK_STEP(2),
    SEND_VSYNC_STEP(7),
    
    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(3),
    TICK_SYSTEM_CLOCK_STEP(3),
    
    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(4),
    TICK_SYSTEM_CLOCK_STEP(4),

    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(5),
    TICK_SYSTEM_CLOCK_STEP(5),

    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(6),
    TICK_SYSTEM_CLOCK_STEP(6),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(6),

    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(7),
    TICK_SYSTEM_CLOCK_STEP(7),
    SEND_VSYNC_STEP(8),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(7),

    END_STEP
};

/*
CLOCK_NOW       (0)------(1)---------------(2)---------------(3)------------------(4)---------------(5)---------------(6)---------
PUT_PICTURE     ----|-----------------|-----------------|--------------------|-----------------|-----------------|----------------
FLUSH           ----------------------------------------------------------|-------------------------------------------------------
PREPARE_PICTURE ------(1)------(N)------(2)------(N)------(3)------(N)---------(4)------(N)------(5)------(N)------(6)------(N)---
                       |                 |                 |                    |                 |                 |             
                        \________         \________         \________            \________         \________         \________    
                                  \                 \                 \                    \                 \                 \  
                                   |                 |                 |                    |                 |                 | 
RENDER_PICTURE  ------------------(1)---------------(2)---------------(3)------------------(4)---------------(5)---------------(6)
VSYNC_EVENT     ------------(2)---------------(3)---------------(4)------------------(5)---------------(6)---------------(7)------
*/
test_step_t test_steps_3Renders1Flush3Renders[] =
{
    TICK_SYSTEM_CLOCK_STEP(0),

    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(1),
    TICK_SYSTEM_CLOCK_STEP(1),
    SEND_VSYNC_STEP(2),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(1),

    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(2),
    TICK_SYSTEM_CLOCK_STEP(2),
    SEND_VSYNC_STEP(3),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(2),
    
    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(3),
    TICK_SYSTEM_CLOCK_STEP(3),
    SEND_VSYNC_STEP(4),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(3),

    FLUSH_STEP,
    
    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(4),
    TICK_SYSTEM_CLOCK_STEP(4),
    SEND_VSYNC_STEP(5),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(4),

    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(5),
    TICK_SYSTEM_CLOCK_STEP(5),
    SEND_VSYNC_STEP(6),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(5),

    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(6),
    TICK_SYSTEM_CLOCK_STEP(6),
    SEND_VSYNC_STEP(7),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(6),

    END_STEP
};

/*
This test ensures :
given no new vsync event is received
pictures are still dequeued from fifo (prepare) without being displayed (render).

CLOCK_NOW       (0)------(1)---------------(2)------(3)------(4)------(5)------(6)
PUT_PICTURE     ----|-----------------|--------|--------|--------|--------|-------
FLUSH           ------------------------------------------------------------------
PREPARE_PICTURE ------(1)------(N)------(2)------(3)------(4)------(5)------(6)---
                       |                                                          
                        \________                                                 
                                  \                                               
                                   |                                              
RENDER_PICTURE  ------------------(1)---------------------------------------------
VSYNC_EVENT     ------------(2)---------------------------------------------------
*/

test_step_t test_steps_NoVSync[] =
{
    TICK_SYSTEM_CLOCK_STEP(0),

    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(1),
    TICK_SYSTEM_CLOCK_STEP(1),
    SEND_VSYNC_STEP(2),
    PREPARE_PICTURE_NULL_STEP,
    RENDER_PICTURE_STEP(1),

    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(2),
    TICK_SYSTEM_CLOCK_STEP(2),
    
    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(3),
    TICK_SYSTEM_CLOCK_STEP(3),
    
    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(4),
    TICK_SYSTEM_CLOCK_STEP(4),

    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(5),
    TICK_SYSTEM_CLOCK_STEP(5),

    PUT_PICTURE_STEP,
    PREPARE_PICTURE_STEP(6),
    TICK_SYSTEM_CLOCK_STEP(6),

    END_STEP
};

test_scenario_t test_scenarios[] = {
    SCENARIO(test_steps_RenderOnePicture),
    SCENARIO(test_steps_RenderTwoPictures),
    SCENARIO(test_steps_SkipOnePicture),
    SCENARIO(test_steps_AllVSyncEventsAfterPrepare),
    SCENARIO(test_steps_AllVSyncEventsBeforePrepare),
    SCENARIO(test_steps_Skip4Pictures),
    SCENARIO(test_steps_3Renders1Flush3Renders),
    SCENARIO(test_steps_NoVSync),
};

size_t test_scenarios_count = ARRAY_SIZE(test_scenarios);

static bool WaitControl(void *opaque, vlc_tick_t deadline)
{
    (void)opaque;
    (void)deadline;
    return false;
}

static picture_t *PreparePicture(void *opaque, bool reuse_decoded,
                                 bool frame_by_frame)
{
    (void)reuse_decoded;
    (void)frame_by_frame;
    
    test_ctx_t *sys = opaque;

    vlc_mutex_lock(&sys->step_mtx);
    test_step_t *step = &sys->scenario->steps[sys->step_index];

    if (sys->state != STARTED) {
        vlc_mutex_unlock(&sys->step_mtx);
        return NULL;
    }

    while (step->kind != PREPARE_PICTURE &&
            step->kind != PREPARE_PICTURE_NULL)
    {
        assert(step->kind != RENDER_PICTURE);
        if (step->kind == END) {
            vlc_mutex_unlock(&sys->step_mtx);
            return NULL;
        }
        vlc_cond_wait(&sys->step_scheduler_cond, &sys->step_mtx);
        step = &sys->scenario->steps[sys->step_index];
    }
    vlc_mutex_unlock(&sys->step_mtx);
    
    msg_Dbg(sys, "Enter PreparePicture");

    assert (step->execute);
    step->execute(sys, step);
    
    picture_t *picture = step->data.prepare.picture;
    
    assert(picture || step->kind == PREPARE_PICTURE_NULL);
    
    vlc_mutex_lock(&sys->step_mtx);
    sys->step_index++;
    vlc_cond_signal(&sys->step_test_cond);
    vlc_mutex_unlock(&sys->step_mtx);

    return picture;
}

static int RenderPicture(void *opaque, picture_t *picture, bool render_now)
{
    (void)render_now;
    
    assert(picture);
    
    test_ctx_t *sys = opaque;

    vlc_mutex_lock(&sys->step_mtx);
    test_step_t *step = &sys->scenario->steps[sys->step_index];

    while (step->kind != RENDER_PICTURE)
    {
        assert(step->kind != PREPARE_PICTURE);
        assert(step->kind != PREPARE_PICTURE_NULL);
        if (step->kind == END) {
            vlc_mutex_unlock(&sys->step_mtx);
            return -1;
        }
        vlc_cond_wait(&sys->step_scheduler_cond, &sys->step_mtx);
        step = &sys->scenario->steps[sys->step_index];
    }
    vlc_mutex_unlock(&sys->step_mtx);
    
    msg_Dbg(sys, "Enter RenderPicture date:%lld", picture->date);

    step->data.render.picture = picture;

    assert (step->execute);
    step->execute(sys, step);

    vlc_mutex_lock(&sys->step_mtx);
    sys->step_index ++;
    vlc_cond_signal(&sys->step_test_cond);
    vlc_mutex_unlock(&sys->step_mtx);

    return VLC_SUCCESS;
}

static void test_ctx_Init(test_ctx_t* ctx, test_scenario_t *scenario) {
    assert(ctx);
    vlc_mutex_init(&ctx->step_mtx);
    vlc_cond_init(&ctx->step_test_cond);
    vlc_cond_init(&ctx->step_scheduler_cond);
    ctx->step_index = 0;
    ctx->scenario = scenario;
    ctx->state = IDLE;

    ctx->vout = vlc_object_create(ctx, 
                                  sizeof(* ctx->vout));
    vlc_clock_t clock;
    static const struct vlc_vout_scheduler_callbacks cbs =
    {
        .wait_control = WaitControl,
        .prepare_picture = PreparePicture,
        .render_picture = RenderPicture,
    };
    ctx->display = vlc_object_create(ctx, 
                                     sizeof(* ctx->display));
    ctx->scheduler = 
        vlc_vout_scheduler_NewVSYNC(ctx->vout, &clock, ctx->display, &cbs, ctx);
}

static void test_ctx_Delete(test_ctx_t* ctx) {
    vlc_vout_scheduler_Destroy(ctx->scheduler);
    vlc_object_delete(ctx);
    vlc_object_delete(ctx->vout);
    vlc_object_delete(ctx->display);
}

static int test_ctx_RunSteps(test_ctx_t *ctx) {
    assert(ctx);
    assert(ctx->scenario);
    assert(ctx->scenario->steps_count > 0);

    vlc_mutex_lock(&ctx->step_mtx);
    test_step_t *step = &ctx->scenario->steps[ctx->step_index];
    ctx->state = STARTED;
    vlc_mutex_unlock(&ctx->step_mtx);

    while (step->kind != END) {
        vlc_mutex_lock(&ctx->step_mtx);
        while (step->kind != SEND_VSYNC
            && step->kind != TICK_SYSTEM_CLOCK 
            && step->kind != PUT_PICTURE
            && step->kind != FLUSH
            && step->kind != END) {
            vlc_cond_wait(&ctx->step_test_cond, 
                          &ctx->step_mtx);
            step = &ctx->scenario->steps[ctx->step_index];
        }
        vlc_mutex_unlock(&ctx->step_mtx);
        msg_Dbg(ctx, "Starting step %s",
                test_step_kind_String(step->kind));
        if (step->kind == END) {
            break;
        }
        assert(step->execute);
        step->execute(ctx, step);
        vlc_mutex_lock(&ctx->step_mtx);
        ctx->step_index ++;
        step = &ctx->scenario->steps[ctx->step_index];
        vlc_cond_signal(&ctx->step_scheduler_cond);
        vlc_mutex_unlock(&ctx->step_mtx);
    }

    vlc_mutex_lock(&ctx->step_mtx);
    ctx->state = FINISHED;
    vlc_mutex_unlock(&ctx->step_mtx);

    return 1;
}

struct vlc_vout_vsync_priv
{
    struct vlc_vout_scheduler impl;
    vout_thread_t *vout;
};

#ifndef HAVE_STATIC_LIBVLC
const char vlc_module_name[] = "vout-vsync-test";
#endif

vlc_tick_t vlc_tick_now (void) {
    vlc_tick_t value;
    pthread_mutex_lock(&system_clock.mtx);
    value = system_clock.now;
    pthread_mutex_unlock(&system_clock.mtx);
    return value;
}


typedef struct {
    pthread_mutex_t mtx;
    pthread_t th;
    vlc_cond_t *cond;
    vlc_tick_t deadline;
} cond_timedwait_sys_t;

cond_timedwait_sys_t cond_timedwait_sys;

static void* TimedWaitThread(void *opaque) {
    (void)opaque;
    pthread_mutex_lock(&cond_timedwait_sys.mtx);
    if (   cond_timedwait_sys.cond 
        && cond_timedwait_sys.deadline >= vlc_tick_now()
        && cond_timedwait_sys.deadline != VLC_TICK_INVALID)
        vlc_cond_signal(cond_timedwait_sys.cond);
    pthread_mutex_unlock(&cond_timedwait_sys.mtx);
    return NULL;
}

static void start_timedwait(cond_timedwait_sys_t *sys) {
    pthread_mutex_init(&sys->mtx, NULL);

    sys->cond = NULL;
    sys->deadline = VLC_TICK_INVALID;

    // pthread_create(&sys->th, NULL, TimedWaitThread, sys);
    // pthread_detach(sys->th);
}

static void stop_timedwait(cond_timedwait_sys_t *sys) {
    (void)sys;
}

int vlc_cond_timedwait(vlc_cond_t *cond, vlc_mutex_t *mutex,
                               vlc_tick_t deadline) 
{
    // pthread_mutex_lock(&cond_timedwait_sys.mtx);
    // cond_timedwait_sys.cond = cond;
    // cond_timedwait_sys.deadline = deadline;
    // pthread_mutex_unlock(&cond_timedwait_sys.mtx);
    vlc_mutex_unlock(mutex);
    vlc_mutex_lock(mutex);
    // vlc_cond_wait(cond, mutex);

    // pthread_mutex_lock(&cond_timedwait_sys.mtx);
    // cond_timedwait_sys.cond = NULL;
    // cond_timedwait_sys.deadline = VLC_TICK_INVALID;
    // pthread_mutex_unlock(&cond_timedwait_sys.mtx);
    
    return 0;
}

static void test_run_scenario(test_scenario_t* scenario, vlc_object_t *instance)
{
    assert(instance);

    test_ctx_t *test_ctx = vlc_object_create(instance, 
                                             sizeof(*test_ctx));
        
    pthread_mutex_init(&system_clock.mtx, NULL);
    
    start_timedwait(&cond_timedwait_sys);
    
    test_ctx_Init(test_ctx, scenario);

    test_ctx_RunSteps(test_ctx);
    
    test_ctx_Delete(test_ctx);
    
    pthread_mutex_destroy(&cond_timedwait_sys.mtx);
    pthread_mutex_destroy(&system_clock.mtx);
}

int main(int argc, char * argv[])
{
    (void)argc;
    (void)argv;
    test_init();

    const char * const args[] = {
        "-vvv", "--vout=dummy", "--aout=dummy", "--text-renderer=dummy",
        "--no-auto-preparse", "--no-spu", "--no-osd",
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(args), args);
    
    vlc_object_t *root = (vlc_object_t *)vlc->p_libvlc_int;
    
    libvlc_playlist_play(vlc);

    for (size_t i = 0; i < test_scenarios_count; i++) {
        test_scenario_t *s = &test_scenarios[i];
        msg_Dbg(root, "Starting Scenario : %s", s->name);
        test_run_scenario(s, root);
        msg_Dbg(root, "Finished Scenario : %s", s->name);
    }
    libvlc_release(vlc);
}