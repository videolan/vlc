#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_executor.h>

#include "vout_scheduler.h"
#ifdef VOUT_VSYNC_TEST
 #include "../../test/src/video_output/vout_vsync_test_clock.h"
#else
 #include "../clock/clock.h"
#endif

/* Maximum delay between 2 displayed pictures.
 * XXX it is needed for now but should be removed in the long term.
 */
#define VOUT_REDISPLAY_DELAY VLC_TICK_FROM_MS(80)

/**
 * Late pictures having a delay higher than this value are thrashed.
 */
#define VOUT_DISPLAY_LATE_THRESHOLD VLC_TICK_FROM_MS(20)

/* Better be in advance when awakening than late... */
#define VOUT_MWAIT_TOLERANCE VLC_TICK_FROM_MS(4)


enum vout_state_e {
    VOUT_STATE_IDLE,
    VOUT_STATE_CONTROL,
    VOUT_STATE_SAMPLE,
    VOUT_STATE_PREPARE,
    VOUT_STATE_DISPLAY,
};


struct vlc_vout_vsync_priv
{
    struct vlc_vout_scheduler impl;
    vout_thread_t *vout;
    vlc_clock_t *clock;
    vout_display_t *display;
    vlc_executor_t *executor;
    vlc_thread_t thread;

    /* MISC */
    float rate;
    bool clock_nowait;
    bool wait_interrupted;

    vlc_tick_t  date_refresh;
    struct {
        vlc_tick_t  date;
        vlc_tick_t  timestamp;
        bool        is_interlaced;
        picture_t   *decoded; // decoded picture before passed through chain_static
        picture_t   *current;
        picture_t   *next;
        bool current_rendered;
    } displayed;

    struct {
        bool        is_on;
        vlc_tick_t  date;
    } pause;

    struct {
        vlc_mutex_t lock;
        vlc_cond_t cond_update;
        vlc_tick_t current_date;
        vlc_tick_t next_date;
        vlc_tick_t last_date;
        vlc_tick_t initial_offset;
        bool missed;
    } vsync;

    atomic_bool vsync_halted;

    struct {
        vlc_mutex_t lock;
        vlc_cond_t cond_update;
        enum vout_state_e current;

        bool terminated;

        struct {
            vlc_tick_t deadline;
            bool wait;
        } display;
        struct {
            bool render_now;
            bool first;
        } prepare;
    } state;

    video_format_t last_format;
};

static void NextPicture(struct vlc_vout_scheduler *scheduler)
{
    struct vlc_vout_vsync_priv *priv =
        container_of(scheduler, struct vlc_vout_vsync_priv, impl);

    priv->displayed.current = priv->displayed.next;
    priv->displayed.next = NULL;
    priv->displayed.current_rendered = false;
    video_format_Copy(&priv->last_format, &priv->displayed.current->format);
}

static void SamplePicture(struct vlc_vout_scheduler *scheduler)
{
    struct vlc_vout_vsync_priv *priv =
        container_of(scheduler, struct vlc_vout_vsync_priv, impl);
    vout_thread_t *vout = priv->vout;
    vlc_object_t *obj = (vlc_object_t *)priv->vout;

    assert(priv->clock);

    if (priv->displayed.current == NULL)
    {
        NextPicture(scheduler);
        priv->state.current = VOUT_STATE_DISPLAY;
        return;
    }

    if (priv->displayed.next->b_force)
    {
        NextPicture(scheduler);
        priv->state.current = VOUT_STATE_DISPLAY;
        return;
    }

    /* If video format have changed, drop the intermediate pictures since the
     * vout has already changed the format at this point. */
    if (!video_format_IsSimilar(&priv->last_format, &priv->displayed.next->format))
    {
        picture_Release(priv->displayed.current);
        NextPicture(scheduler);
        priv->state.current = VOUT_STATE_CONTROL;
        return;
    }

    vlc_tick_t system_now = vlc_tick_now();
    const vlc_tick_t render_delay = /*vout_chrono_GetHigh(&sys->render) + */ VOUT_MWAIT_TOLERANCE;

    assert(priv->displayed.next);

    vlc_tick_t new_next_pts =
        vlc_clock_ConvertToSystem(priv->clock, system_now,
                                  priv->displayed.next->date, priv->rate);

    /* Well, we should use the most recent picture fitting in the vsync period */
    if (new_next_pts /*- priv->vsync.initial_offset */< priv->vsync.next_date)
    {
        picture_Release(priv->displayed.current);
        NextPicture(scheduler);
        priv->state.current = VOUT_STATE_CONTROL;
        return;
    }

    /* If the current VSYNC is not valid, return to the VOUT_STATE_IDLE
     * state and wait the next one. */
    if ( priv->vsync.next_date == VLC_TICK_INVALID
      || priv->vsync.last_date == priv->vsync.next_date
      || priv->vsync.missed == true)
    {
        const char *error =
            priv->vsync.missed ? "missed" :
            priv->vsync.next_date == VLC_TICK_INVALID ? "next date invalid" :
            "no new VSYNC";

        /* Wait for any input signal to unblock the state machine.
         * In particular, the state machine will always run into the
         * VOUT_STATE_IDLE state as long as no VSYNC has happened. */
        priv->state.current = VOUT_STATE_IDLE;
        return;
    }


    //bool no_vsync_halted = WaitForVSYNC(scheduler, new_next_pts, &vsync_date);

    //if (/*atomic_load(&sys->control_is_terminated) ||*/ !no_vsync_halted)
    //    return VLC_EGENERIC;
    vlc_tick_t vsync_date = priv->vsync.next_date;

    // TODO: why the condition on vsync_date and system_now here ?
    if (new_next_pts < vsync_date)// || system_now + render_delay > vsync_date)
    {
        //if (!priv->displayed.current_rendered)
        /*msg_Dbg((vlc_object_t*)vout, "Dropping 2/2 because of vsync, offset to vsync is now %dms / render_delay=%dms / next_pts=%dms vsync=%dms",
            (int)MS_FROM_VLC_TICK(vsync_date - new_next_pts),
            (int)MS_FROM_VLC_TICK(render_delay),
            (int)MS_FROM_VLC_TICK(new_next_pts),
            (int)MS_FROM_VLC_TICK(vsync_date));*/
        picture_Release(priv->displayed.current);
        NextPicture(scheduler);
        priv->state.current = VOUT_STATE_CONTROL;
        return;
    }

    /* Wait, this picture has already been rendered. */
    if (priv->displayed.current_rendered)
    {
        priv->state.current = VOUT_STATE_IDLE;
        return;
    }

    priv->state.current = VOUT_STATE_DISPLAY;
    return;
}

/*****************************************************************************
 * Thread: video output thread
 *****************************************************************************
 * Video output thread. This function does only returns when the thread is
 * terminated. It handles the pictures arriving in the video heap and the
 * display device events.
 *****************************************************************************/
static void *StateMachine(void *object)
{
    struct vlc_vout_scheduler *scheduler = object;
    struct vlc_vout_vsync_priv *priv =
        container_of(scheduler, struct vlc_vout_vsync_priv, impl);

    vlc_thread_set_name("vlc-vout-vsync");

    priv->state.current = VOUT_STATE_CONTROL;
    priv->state.display.deadline = VLC_TICK_INVALID;
    priv->state.display.wait = false;

    vlc_object_t *obj = (vlc_object_t*)priv->vout;

    bool terminated = false;
    //while (!atomic_load(&sys->control_is_terminated))

    vlc_mutex_lock(&priv->state.lock);
    while (!priv->state.terminated)
    {
        switch (priv->state.current)
        {
            /* For now, just suspend the runloop to set the state machine paused. */
            case VOUT_STATE_IDLE:
                while (priv->state.current == VOUT_STATE_IDLE &&
                       !priv->state.terminated)
                {
                    if (priv->displayed.next == NULL)
                    {
                        vlc_cond_wait(&priv->state.cond_update, &priv->state.lock);
                        continue;
                    }

                    vlc_tick_t system_now = vlc_tick_now();
                    vlc_tick_t deadline =
                        vlc_clock_ConvertToSystem(priv->clock, system_now,
                                                  priv->displayed.next->date, priv->rate);
                    if (deadline < system_now)
                    {
                        priv->state.current = VOUT_STATE_CONTROL;
                        break;
                    }
                    vlc_cond_timedwait(&priv->state.cond_update, &priv->state.lock, deadline);
                }
                break;

            case VOUT_STATE_CONTROL: {
                // TODO: submit a control task? remove and replace by cond wait?
                //vlc_executor_Submit(scheduler->executor, &task->runnable);

                // Probably a control function, maybe useless
                // vlc_mutex_lock(controls)
                // controls->wait = false;
                // vlc_cond_broadcast(&priv->wake_controls);
                // vlc_mutex_unlock(controls)
                //
                // while (priv->controls != 0)
                //    vlc_cond_wait(&priv->done_controls, lock);
                //
                // Probably a control function
                // vlc_mutex_lock(controls)
                // controls->wait = true;
                // vlc_mutex_unlock(controls)

                vlc_mutex_unlock(&priv->state.lock);
                terminated = vlc_vout_scheduler_WaitControl(scheduler, vlc_tick_now());
                vlc_mutex_lock(&priv->state.lock);

                if (terminated)
                    priv->state.terminated = true;


                priv->state.current = VOUT_STATE_PREPARE;
                break;
            }

            case VOUT_STATE_PREPARE: {
                // TODO: submit a prepare task?
                //vlc_executor_Submit(scheduler->executor, &task->runnable);

                const bool first = !priv->displayed.current;
                if (!priv->displayed.next)
                {
                    vlc_mutex_unlock(&priv->state.lock);
                    picture_t *next = vlc_vout_scheduler_PreparePicture(scheduler, first, false);
                    vlc_mutex_lock(&priv->state.lock);

                    priv->displayed.next = next;
                }

                if (priv->vsync.next_date == VLC_TICK_INVALID ||
                    (priv->displayed.next == NULL && priv->displayed.current_rendered))
                {
                    priv->state.current = VOUT_STATE_IDLE;
                    break;
                }

                // NOTE: potential deadlock putpicture ?

                /* If we don't have any next picture, wait */
                // TODO: to wait, we'd like to move to the control state and wait there for
                // any signal that would lead to any change, instead of waiting fixed steps.
                if (priv->displayed.next == NULL)
                {
                    /* We have a VSYNC, but no next picture, sample the last picture
                     * we got regardless of latency */
                    if (priv->vsync.next_date != VLC_TICK_INVALID
                      && priv->vsync.last_date != priv->vsync.next_date
                      && !priv->vsync.missed
                      && !priv->displayed.current_rendered
                      && priv->displayed.current != NULL)
                    {
                        priv->state.current = VOUT_STATE_DISPLAY;
                        break;
                    }
                    priv->state.current = VOUT_STATE_IDLE;
                    break;
                }

                priv->state.current = VOUT_STATE_SAMPLE;
                break;
            }

            case VOUT_STATE_SAMPLE: {
                // TODO: submit a task to call vlc_vout_scheduler_RenderPicture or DisplayPicture
                //vlc_executor_Submit(scheduler->executor, &task->runnable);

                SamplePicture(scheduler);

                /* TODO
                const bool picture_interlaced = priv->displayed.is_interlaced;
                vout_SetInterlacingState(&vout->obj, &sys->private, picture_interlaced);
                */

                /* 3 possibles cases:
                 *  - VSYNC has not been reached -> IDLE was set
                 *  - picture is not correct for VSYNC -> we want CONTROL
                 *  - everything is fine -> be ready for DISPLAY  */
                assert(priv->state.current != VOUT_STATE_SAMPLE);
                break;
            }


            case VOUT_STATE_DISPLAY: {
                // TODO: task, but the frames must be ready?
                //vlc_executor_Submit(scheduler->executor, &task->runnable);
                assert(priv->displayed.current);

                bool render_now = priv->state.prepare.render_now;
                vlc_mutex_unlock(&priv->state.lock);
                int ret = vlc_vout_scheduler_RenderPicture(scheduler, priv->displayed.current, false);
                vlc_mutex_lock(&priv->state.lock);

                priv->displayed.current_rendered = true;
                priv->vsync.last_date = priv->vsync.next_date;
                priv->state.current = VOUT_STATE_CONTROL;
                break;
            }
        }
    }
    vlc_mutex_unlock(&priv->state.lock);

    return NULL;
}

static void DestroyScheduler(struct vlc_vout_scheduler *scheduler)
{
    struct vlc_vout_vsync_priv *priv =
        container_of(scheduler, struct vlc_vout_vsync_priv, impl);

    vlc_mutex_lock(&priv->state.lock);
    priv->state.terminated = true;
    vlc_mutex_unlock(&priv->state.lock);
    vlc_cond_signal(&priv->state.cond_update);
    
    vlc_executor_Delete(priv->executor);
    vlc_join(priv->thread, NULL);

    if (priv->displayed.current != NULL)
        picture_Release(priv->displayed.current);

    if (priv->displayed.next != NULL)
        picture_Release(priv->displayed.next);

    free(priv);
}

static void
PutPicture(struct vlc_vout_scheduler *scheduler, picture_t *picture)
{
    struct vlc_vout_vsync_priv *priv =
        container_of(scheduler, struct vlc_vout_vsync_priv, impl);
    vlc_object_t *obj = (vlc_object_t*)priv->vout;

    vlc_mutex_lock(&priv->state.lock);
    if (priv->state.current == VOUT_STATE_IDLE)
        priv->state.current = VOUT_STATE_CONTROL;
    vlc_cond_signal(&priv->state.cond_update);
    vlc_mutex_unlock(&priv->state.lock);
}

static void
SignalVSYNC(struct vlc_vout_scheduler *scheduler, vlc_tick_t next_ts)
{
    struct vlc_vout_vsync_priv *priv =
        container_of(scheduler, struct vlc_vout_vsync_priv, impl);
    vlc_object_t *obj = (vlc_object_t*)priv->vout;

    vlc_mutex_lock(&priv->state.lock);
    if (priv->state.current == VOUT_STATE_IDLE)
        priv->state.current = VOUT_STATE_CONTROL;

    vlc_tick_t now = vlc_tick_now();
    vlc_tick_t offset = now - priv->vsync.current_date;
    priv->vsync.current_date = now;
    priv->vsync.next_date = next_ts;
    priv->vsync.missed = false; // TODO: how much should we wait ?
    vlc_cond_signal(&priv->state.cond_update);
    vlc_mutex_unlock(&priv->state.lock);

#if 0
    msg_Info(obj, "New vsync date, period=%dms offset=%dms",
             (int)MS_FROM_VLC_TICK(next_ts - now),
             (int)MS_FROM_VLC_TICK(offset));
#endif

#if 1
    static vlc_tick_t last_offset = -1;

    if (  last_offset != -1
          && MS_FROM_VLC_TICK(offset) != 16
       )
    {
        msg_Warn(obj, "[2] Video_output/Vsync is irregular! offset = %lld ms ",MS_FROM_VLC_TICK(offset)  );
    }

    last_offset = offset;
#endif
}

static void
Flush(struct vlc_vout_scheduler *scheduler, vlc_tick_t before)
{
    struct vlc_vout_vsync_priv *priv =
        container_of(scheduler, struct vlc_vout_vsync_priv, impl);

    (void)before; // TODO: partial flush
    vlc_mutex_lock(&priv->state.lock);
    if (priv->displayed.current)
        picture_Release(priv->displayed.current);
    if (priv->displayed.next)
        picture_Release(priv->displayed.next);

    priv->displayed.current = NULL;
    priv->displayed.next = NULL;
    priv->state.current = VOUT_STATE_CONTROL;
    priv->displayed.current_rendered = false;

    vlc_mutex_unlock(&priv->state.lock);
}

struct vlc_vout_scheduler *
vlc_vout_scheduler_NewVSYNC(vout_thread_t *vout, vlc_clock_t *clock,
                       vout_display_t *display,
                       const struct vlc_vout_scheduler_callbacks *cbs,
                       void *owner)
{
    struct vlc_vout_vsync_priv *priv = malloc(sizeof *priv);
    struct vlc_vout_scheduler *scheduler = &priv->impl;

    static const struct vlc_vout_scheduler_operations ops =
    {
        .destroy = DestroyScheduler,
        .put_picture = PutPicture,
        .signal_vsync = SignalVSYNC,
        .flush = Flush,
    };
    scheduler->ops = &ops;
    priv->vout = vout;
    priv->clock = clock;
    priv->display = display;
    priv->displayed.current = NULL;
    priv->displayed.next = NULL;
    priv->state.current = VOUT_STATE_IDLE;
    priv->state.terminated = false;
    priv->rate = 1.f;

    priv->wait_interrupted = false;

    priv->vsync.current_date = VLC_TICK_INVALID;
    priv->vsync.next_date = VLC_TICK_INVALID;
    priv->vsync.last_date = VLC_TICK_INVALID;
    priv->vsync.current_date = VLC_TICK_INVALID;
    priv->vsync.missed = false;
    priv->displayed.current_rendered = true;
    video_format_Init(&priv->last_format, 0);
    vlc_mutex_init(&priv->state.lock);
    vlc_cond_init(&priv->state.cond_update);
    atomic_init(&priv->vsync_halted, false);

    scheduler->owner.sys = owner;
    scheduler->owner.cbs = cbs;

    // Unused for now, will replace the thread
    priv->executor = vlc_executor_New(1);
    if (priv->executor == NULL)
    {
        free(priv);
        return NULL;
    }

    if (vlc_clone(&priv->thread, StateMachine, scheduler) != VLC_SUCCESS)
    {
        vlc_executor_Delete(priv->executor);
        free(priv);
        return NULL;
    }

    return scheduler;
}
