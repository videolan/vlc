#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_threads.h>

#include "vout_scheduler.h"
#include "../clock/clock.h"

#include <stdatomic.h>

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

struct vlc_vout_scheduler_priv
{
    struct vlc_vout_scheduler impl;
    vout_thread_t *vout;
    vlc_clock_t *clock;
    vout_display_t *display;
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
    } displayed;

    struct {
        bool        is_on;
        vlc_tick_t  date;
    } pause;

    atomic_bool is_terminated;
};

#if 0
static int RenderPicture(struct vlc_vout_scheduler *scheduler, bool render_now)
{
    vout_thread_t *vout = scheduler->vout;
    vout_display_t *vd = scheduler->display;

    vlc_vout_scheduler_StartChrono(scheduler, VLC_VOUT_CHRONO_RENDER);

    picture_t *current; // TODO
    picture_t *filtered = vlc_vout_scheduler_FilterInteractive(scheduler, current);
    if (!filtered)
        return VLC_EGENERIC;

    vlc_clock_Lock(scheduler->clock);
    scheduler->clock_nowait = false;
    vlc_clock_Unlock(scheduler->clock);

    vlc_queuedmutex_lock(&sys->display_lock);

    picture_t *todisplay;
    subpicture_t *subpic;

    int ret = vlc_vout_scheduler_PrerenderPicture(scheduler, &render_now, filtered, &todisplay, &subpic);
    if (ret != VLC_SUCCESS)
    {
        vlc_queuedmutex_unlock(&sys->display_lock);
        return ret;
    }

    vlc_tick_t system_now = vlc_tick_now();
    const vlc_tick_t pts = todisplay->date;
    vlc_tick_t system_pts = render_now ? system_now :
        vlc_clock_ConvertToSystem(scheduler->clock, system_now, pts, scheduler->rate);
    if (unlikely(system_pts == VLC_TICK_MAX))
    {
        /* The clock is paused, it's too late to fallback to the previous
         * picture, display the current picture anyway and force the rendering
         * to now. */
        system_pts = system_now;
        render_now = true;
    }

    const unsigned frame_rate = todisplay->format.i_frame_rate;
    const unsigned frame_rate_base = todisplay->format.i_frame_rate_base;

    if (vd->ops->prepare != NULL)
        vd->ops->prepare(vd, todisplay, subpic, system_pts);

    vlc_vout_scheduler_StopChrono(scheduler, VLC_VOUT_CHRONO_RENDER);

    // struct vlc_tracer *tracer = GetTracer(sys);
    system_now = vlc_tick_now();
    if (!render_now)
    {
        const vlc_tick_t late = system_now - system_pts;
        if (unlikely(late > 0))
        {
            //if (tracer != NULL)
            //    vlc_tracer_TraceEvent(tracer, "RENDER", sys->str_id, "late");
            msg_Dbg(vd, "picture displayed late (missing %"PRId64" ms)", MS_FROM_VLC_TICK(late));
            vout_statistic_AddLate(&sys->statistic, 1);

            /* vd->prepare took too much time. Tell the clock that the pts was
             * rendered late. */
            system_pts = system_now;
        }
        else if (vd->ops->display != NULL)
        {
            vlc_tick_t max_deadline = system_now + VOUT_REDISPLAY_DELAY;

            /* Wait to reach system_pts if the plugin doesn't handle
             * asynchronous display */
            vlc_clock_Lock(scheduler->clock);

            bool timed_out = false;
            scheduler->wait_interrupted = false;
            while (!timed_out)
            {
                vlc_tick_t deadline;
                if (vlc_clock_IsPaused(scheduler->clock))
                    deadline = max_deadline;
                else
                {
                    deadline = vlc_clock_ConvertToSystemLocked(scheduler->clock,
                                                vlc_tick_now(), pts, scheduler->rate);
                    if (deadline > max_deadline)
                        deadline = max_deadline;
                }

                if (scheduler->clock_nowait)
                {
                    /* A caller (the UI thread) awaits for the rendering to
                     * complete urgently, do not wait. */
                    scheduler->wait_interrupted = true;
                    break;
                }

                system_pts = deadline;
                timed_out = vlc_clock_Wait(scheduler->clock, deadline);
            }
            vlc_clock_Unlock(scheduler->clock);
        }
        scheduler->displayed.date = system_pts;
    }
    else
    {
        scheduler->displayed.date = system_now;
        /* Tell the clock that the pts was forced */
        system_pts = VLC_TICK_MAX;
    }
    vlc_tick_t drift = vlc_clock_UpdateVideo(scheduler->clock, system_pts, pts, scheduler->rate,
                                             frame_rate, frame_rate_base);

    /* Display the direct buffer returned by vout_RenderPicture */
    vout_display_Display(vd, todisplay);
    vlc_queuedmutex_unlock(&sys->display_lock);

    picture_Release(todisplay);

    if (subpic)
        subpicture_Delete(subpic);

    vout_statistic_AddDisplayed(&sys->statistic, 1);

    if (tracer != NULL && system_pts != VLC_TICK_MAX)
        vlc_tracer_TraceWithTs(tracer, system_pts, VLC_TRACE("type", "RENDER"),
                               VLC_TRACE("id", sys->str_id),
                               VLC_TRACE("drift", drift), VLC_TRACE_END);

    return VLC_SUCCESS;
}
#endif

#if 0
static int DisplayNextFrame(struct vlc_vout_scheduler *scheduler)
{
    struct vlc_vout_scheduler_priv *priv =
        container_of(priv, struct vlc_vout_scheduler_priv, impl);

    //UpdateDeinterlaceFilter(sys);

    picture_t *next = PreparePicture(priv, !priv->displayed.current, true);

    if (next)
    {
        if (likely(priv->displayed.current != NULL))
            picture_Release(priv->displayed.current);
        priv->displayed.current = next;
    }

    if (!priv->displayed.current)
        return VLC_EGENERIC;

    return RenderPicture(priv, true);
}
#endif

static bool UpdateCurrentPicture(struct vlc_vout_scheduler *scheduler)
{
    struct vlc_vout_scheduler_priv *priv =
        container_of(scheduler, struct vlc_vout_scheduler_priv, impl);

    assert(priv->clock);

    if (priv->displayed.current == NULL)
    {
        priv->displayed.current = vlc_vout_scheduler_PreparePicture(scheduler, true, false);
        return priv->displayed.current != NULL;
    }

    if (priv->pause.is_on || priv->wait_interrupted)
        return false;

    const vlc_tick_t system_now = vlc_tick_now();
    const vlc_tick_t system_swap_current =
        vlc_clock_ConvertToSystem(priv->clock, system_now,
                                  priv->displayed.current->date,
                                  priv->rate);
    if (unlikely(system_swap_current == VLC_TICK_MAX))
        // the clock is paused but the vout thread is not ?
        return false;

    const vlc_tick_t render_delay = /* vout_chrono_GetHigh(&priv->chrono.render) */ + VOUT_MWAIT_TOLERANCE;
    vlc_tick_t system_prepare_current = system_swap_current - render_delay;
    if (unlikely(system_prepare_current > system_now))
        // the current frame is not late, we still have time to display it
        // no need to get a new picture
        return true;

    // the current frame will be late, look for the next not late one
    picture_t *next = vlc_vout_scheduler_PreparePicture(scheduler, false, false);
    if (next == NULL)
        return false;
    /* We might have reset the current picture when preparing the next one,
     * because filters had to be changed. In this case, avoid releasing the
     * picture since it will lead to null pointer dereference errors. */
    if (priv->displayed.current != NULL)
        picture_Release(priv->displayed.current);

    priv->displayed.current = next;

    return true;
}

static void DisplayPicture(struct vlc_vout_scheduler *scheduler, bool current_changed)
{
    struct vlc_vout_scheduler_priv *priv =
        container_of(scheduler, struct vlc_vout_scheduler_priv, impl);

    assert(priv->clock);

    bool render_now = true;

    /* Next frame will still need some waiting before display, we don't need
     * to render now, display forced picture immediately */
    if (current_changed)
        render_now = priv->displayed.current->b_force;
    else if (priv->wait_interrupted)
        priv->wait_interrupted = false;

    vlc_vout_scheduler_RenderPicture(scheduler, priv->displayed.current, render_now);
}

/*****************************************************************************
 * Thread: video output thread
 *****************************************************************************
 * Video output thread. This function does only returns when the thread is
 * terminated. It handles the pictures arriving in the video heap and the
 * display device events.
 *****************************************************************************/
static void *Thread(void *opaque)
{
    vlc_thread_set_name("vlc-vout-sched");

    struct vlc_vout_scheduler *scheduler = opaque;
    struct vlc_vout_scheduler_priv *priv =
        container_of(scheduler, struct vlc_vout_scheduler_priv, impl);

    vlc_tick_t deadline = VLC_TICK_INVALID;

    for (;;) {
        if (atomic_load(&priv->is_terminated))
            break;

        bool terminated = vlc_vout_scheduler_WaitControl(scheduler, deadline);
        if (terminated)
            break;

        bool current_changed = UpdateCurrentPicture(scheduler);
        if (atomic_load(&priv->is_terminated))
            break;

        const vlc_tick_t system_now = vlc_tick_now();
        /* In case nothing change, plan next update accordingly. */
        if (!current_changed && !priv->wait_interrupted
            && likely(priv->displayed.date != VLC_TICK_INVALID)
            && system_now < priv->date_refresh)
        {

            vlc_tick_t max_deadline = system_now + VOUT_REDISPLAY_DELAY;
            deadline = __MIN(priv->date_refresh, max_deadline);
        }

        // TODO
        // UpdateDeinterlaceFilter(sys);
        if (priv->displayed.current != NULL &&
             (current_changed || priv->wait_interrupted))
            DisplayPicture(scheduler, current_changed);

        if (atomic_load(&priv->is_terminated))
            break;

        // TODO  compute date_refresh
        priv->date_refresh = priv->displayed.date + VOUT_REDISPLAY_DELAY;

        assert(deadline == VLC_TICK_INVALID ||
               deadline <= vlc_tick_now() + VOUT_REDISPLAY_DELAY);

        // TODO
        //const bool picture_interlaced = sys->displayed.is_interlaced;

        //vout_SetInterlacingState(&vout->obj, &sys->private, picture_interlaced);
    }
    return NULL;
}

static void DestroyScheduler(struct vlc_vout_scheduler *scheduler)
{
    struct vlc_vout_scheduler_priv *priv =
        container_of(scheduler, struct vlc_vout_scheduler_priv, impl);

    atomic_store(&priv->is_terminated, true);
    vlc_join(priv->thread, NULL);

    if (priv->displayed.current != NULL)
        picture_Release(priv->displayed.current);

    free(priv);
}


struct vlc_vout_scheduler *
vlc_vout_scheduler_New(vout_thread_t *vout, vlc_clock_t *clock,
                       vout_display_t *display,
                       const struct vlc_vout_scheduler_callbacks *cbs,
                       void *owner)
{
    struct vlc_vout_scheduler_priv *priv = malloc(sizeof *priv);
    struct vlc_vout_scheduler *scheduler = &priv->impl;

    static const struct vlc_vout_scheduler_operations ops =
    {
        .destroy = DestroyScheduler,
    };
    scheduler->ops = &ops;
    priv->vout = vout;
    priv->clock = clock;
    priv->display = display;
    priv->displayed.current = NULL;
    priv->rate = 1.f; // TODO
    priv->pause.is_on = false;
    atomic_store(&priv->is_terminated, false);

    scheduler->owner.sys = owner;
    scheduler->owner.cbs = cbs;


    if (vlc_clone(&priv->thread, Thread, scheduler) != VLC_SUCCESS)
    {
        free(priv);
        return NULL;
    }

    return scheduler;
}
