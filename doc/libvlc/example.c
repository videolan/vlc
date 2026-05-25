/// This file is included in libVLC main page in the doxygen documentation.

//! [minimal example]
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <pthread.h>
#include <vlc/vlc.h>

struct cbs_context
{
    pthread_mutex_t lock; /* protect the following fields */
    pthread_cond_t cond; /* signal on state or time change */
    bool stopping; /* player is stopping */
    bool stopped; /* player is stopped */
    libvlc_time_t time_us; /* last reported time in microseconds */
    bool time_changed; /* true if time_us has been updated since last check */
};

static void on_state_changed(void *opaque, libvlc_state_t state)
{
    struct cbs_context *ctx = opaque;

    if (state != libvlc_Stopped && state != libvlc_Stopping)
        return;

    pthread_mutex_lock(&ctx->lock);
    if (state == libvlc_Stopping)
        ctx->stopping = true;
    else
        ctx->stopped = true;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);
}

static void on_position_changed(void *opaque, libvlc_time_t time, double pos)
{
    (void) pos;
    struct cbs_context *ctx = opaque;

    pthread_mutex_lock(&ctx->lock);
    ctx->time_us = time;
    ctx->time_changed = true;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);
}

int main(int argc, char* argv[])
{
    (void) argc; (void) argv;
    libvlc_instance_t * inst;
    libvlc_media_player_t *mp;
    libvlc_media_t *m;

    struct cbs_context ctx = {
        .lock       = PTHREAD_MUTEX_INITIALIZER,
        .cond       = PTHREAD_COND_INITIALIZER,
        .stopping   = false,
        .stopped    = false,
        .time_us    = 0,
        .time_changed = false,
    };

    /* Load the VLC engine */
    inst = libvlc_new (0, NULL);

    /* Create a new item */
    m = libvlc_media_new_location("http://mycool.movie.com/test.mov");
    //m = libvlc_media_new_path("/path/to/test.mov");

    struct libvlc_media_player_cbs cbs = {
        .version             = 0,
        .on_state_changed    = on_state_changed,
        .on_position_changed = on_position_changed,
    };

    /* Create a media player from the media */
    mp = libvlc_media_player_new_from_media (inst, m, &cbs, &ctx);

    /* No need to keep the media now */
    libvlc_media_release (m);

    /* play the media_player */
    libvlc_media_player_play (mp);

    /* wait until either the position advances or the player is stopping. */
    pthread_mutex_lock(&ctx.lock);
    while (!ctx.stopping)
    {
        while (!ctx.stopping && !ctx.time_changed)
            pthread_cond_wait(&ctx.cond, &ctx.lock);

        if (ctx.time_changed)
        {
            ctx.time_changed = false;
            libvlc_time_t microseconds = ctx.time_us;
            int64_t seconds = microseconds / 1000000;
            int64_t minutes = seconds / 60;
            microseconds -= seconds * 1000000;
            seconds -= minutes * 60;
            pthread_mutex_unlock(&ctx.lock);
            printf("Current time: %" PRId64 ":%" PRId64 ":%" PRId64 "\n",
                   minutes, seconds, microseconds);
            pthread_mutex_lock(&ctx.lock);
        }
    }

    /* Wait for the player to reach the Stopped state. */
    while (!ctx.stopped)
        pthread_cond_wait(&ctx.cond, &ctx.lock);
    pthread_mutex_unlock(&ctx.lock);

    /* Free the media_player */
    libvlc_media_player_release (mp);

    libvlc_release (inst);

    return 0;
}
//! [minimal example]
