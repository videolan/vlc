/*****************************************************************************
 * player.c:  libvlc media player API sample usage
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <locale.h>
#include <semaphore.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <time.h>

#ifdef __linux__
#include <termios.h>
#include <fcntl.h>
#endif

#include <vlc/vlc.h>

struct context
{
    libvlc_media_list_t *mlist;
    libvlc_media_player_t *mp;

    int playing_idx;
    char *playing_title;

    int pipefds[2];

    pthread_mutex_t mainloop_lock;
    libvlc_media_player_time_point_t time_point;
    libvlc_time_t paused_date;
    bool time_seeking;
    float volume;
};

static void
print_usage(const char *name)
{
    fprintf(stderr, "Usage: %s <media1> [media2]...\n", name);
}

static void
print_ui(struct context *ctx)
{
    printf("\n"
        "| seek/volume: arrows - previous/next: p/n - pause: space - next_frame: e - quit: q\n"
        "| playing: \"%s\"\n",
        ctx->playing_title);
}

static libvlc_media_t *
get_next_media_locked(struct context *ctx)
{
    int next_index = ctx->playing_idx + 1;
    return libvlc_media_list_count(ctx->mlist) > next_index ?
                libvlc_media_list_item_at_index(ctx->mlist, next_index) : NULL;
}

static void
player_on_media_changed(void *opaque, libvlc_media_t *media)
{
    struct context *ctx = opaque;

    if (media == NULL)
        return;

    free(ctx->playing_title);
    ctx->playing_title = libvlc_media_get_meta(media, libvlc_meta_Title);
    print_ui(ctx);

    /* Fetch the next media from the mlist */
    libvlc_media_list_lock(ctx->mlist);
    ctx->playing_idx = libvlc_media_list_index_of_item(ctx->mlist, media);
    assert(ctx->playing_idx >= 0);

    libvlc_media_t *next_media = get_next_media_locked(ctx);
    libvlc_media_list_unlock(ctx->mlist);

    /* Set the next media */
    if (next_media != NULL)
    {
        libvlc_media_player_set_next_media(ctx->mp, next_media);
        libvlc_media_release(next_media);
    }
}

static void
player_on_media_subitems_changed(void *opaque, libvlc_media_t *media)
{
    struct context *ctx = opaque;

    assert(ctx->playing_idx == libvlc_media_list_index_of_item(ctx->mlist, media));

    libvlc_media_list_t *subitems = libvlc_media_subitems(media);
    if (subitems == NULL)
        return;

    libvlc_media_type_t type = libvlc_media_get_type(media);

    libvlc_media_list_lock(ctx->mlist);

    /* Remove the current media if it contain only subitems as the list will be
     * flattened */
    if (type == libvlc_media_type_directory || type == libvlc_media_type_playlist)
    {
        libvlc_media_list_remove_index(ctx->mlist, ctx->playing_idx);
        ctx->playing_idx--;
    }

    libvlc_media_list_lock(subitems);
    /* Add subitems to the current flattened playlist */
    int end_id = libvlc_media_list_count(subitems) + ctx->playing_idx + 1;
    for (int i = ctx->playing_idx + 1, j = 0; i < end_id; ++i, ++j)
    {
        libvlc_media_t *subitem = libvlc_media_list_item_at_index(subitems, j);
        libvlc_media_list_insert_media(ctx->mlist, subitem, i);
        libvlc_media_release(subitem);
    }
    libvlc_media_t *next_media = get_next_media_locked(ctx);

    libvlc_media_list_unlock(subitems);
    libvlc_media_list_unlock(ctx->mlist);
    libvlc_media_list_release(subitems);

    /* Set the next media */
    if (next_media != NULL)
    {
        libvlc_media_player_set_next_media(ctx->mp, next_media);
        libvlc_media_release(next_media);
    }
}

static void
player_on_audio_volume_changed(void *opaque, float volume)
{
    struct context *ctx = opaque;

    pthread_mutex_lock(&ctx->mainloop_lock);
    ctx->volume = volume;
    pthread_mutex_unlock(&ctx->mainloop_lock);

    char buf = 'u';
    write(ctx->pipefds[1], &buf, 1);
}

static void
player_time_on_update(void *opaque,
                      const libvlc_media_player_time_point_t *value)
{
    struct context *ctx = opaque;

    if (ctx->time_seeking)
        return;

    pthread_mutex_lock(&ctx->mainloop_lock);
    ctx->paused_date = 0;
    ctx->time_point = *value;
    pthread_mutex_unlock(&ctx->mainloop_lock);

    char buf = 'u';
    write(ctx->pipefds[1], &buf, 1);
}

static void
player_time_on_paused(void *opaque, libvlc_time_t system_date_us)
{
    struct context *ctx = opaque;

    fprintf(stderr, "player_time_on_paused\n");
    pthread_mutex_lock(&ctx->mainloop_lock);
    ctx->paused_date = system_date_us;
    pthread_mutex_unlock(&ctx->mainloop_lock);

    char buf = 'u';
    write(ctx->pipefds[1], &buf, 1);
}

static void
player_time_on_seek(void *opaque, const libvlc_media_player_time_point_t *value)
{
    struct context *ctx = opaque;

    pthread_mutex_lock(&ctx->mainloop_lock);
    if (value != NULL)
    {
        ctx->time_point = *value;
        ctx->time_seeking = true;
    }
    else
        ctx->time_seeking = false;
    pthread_mutex_unlock(&ctx->mainloop_lock);

    char buf = 'u';
    write(ctx->pipefds[1], &buf, 1);
}

static void
context_destroy(struct context *ctx)
{
    libvlc_media_list_release(ctx->mlist);
    libvlc_media_player_unwatch_time(ctx->mp);
    libvlc_media_player_release(ctx->mp);

    pthread_mutex_destroy(&ctx->mainloop_lock);
    close(ctx->pipefds[0]);
    close(ctx->pipefds[1]);

    free(ctx->playing_title);
}

static int
context_init(struct context *ctx, libvlc_instance_t *libvlc)
{
    ctx->playing_idx = 0;
    ctx->playing_title = NULL;
    ctx->paused_date = 0;
    ctx->time_seeking = false;
    ctx->volume = 1.f;

    int ret = pipe(ctx->pipefds);
    if (ret != 0)
        return -errno;

    pthread_mutex_init(&ctx->mainloop_lock, NULL);

    ctx->mlist = libvlc_media_list_new();
    if (ctx->mlist == NULL)
        goto error_pipe;

    static const struct libvlc_media_player_cbs cbs = {
        .version = 0,
        .on_media_changed = player_on_media_changed,
        .on_media_subitems_changed = player_on_media_subitems_changed,
        .on_audio_volume_changed = player_on_audio_volume_changed,
    };

    ctx->mp = libvlc_media_player_new(libvlc, &cbs, ctx);
    if (ctx->mp == NULL)
        goto error_mlist;

    static const struct libvlc_media_player_watch_time_cbs time_cbs = {
        .version = 0,
        .on_update = player_time_on_update,
        .on_paused = player_time_on_paused,
        .on_seek = player_time_on_seek,
    };

    ret = libvlc_media_player_watch_time(ctx->mp, 500000ULL,
                                         &time_cbs, ctx);
    if (ret != 0)
        goto error_mp;

    return 0;

error_mp:
    libvlc_media_player_release(ctx->mp);
error_mlist:
    libvlc_media_list_release(ctx->mlist);
error_pipe:
    pthread_mutex_destroy(&ctx->mainloop_lock);
    close(ctx->pipefds[0]);
    close(ctx->pipefds[1]);
    return -ENOMEM;
}

static int
context_parse_argv(struct context *ctx, int argc, const char **argv)
{
    libvlc_media_list_lock(ctx->mlist);

    for (int i = 1; i < argc; ++i)
    {
        const char *path = argv[i];

        libvlc_media_t *media;
        if (strstr(path, "://") != NULL)
            media = libvlc_media_new_location(path);
        else
            media = libvlc_media_new_path(path);

        if (media == NULL)
        {
            libvlc_media_list_unlock(ctx->mlist);
            return -ENOMEM;
        }

        libvlc_media_list_add_media(ctx->mlist, media);
        libvlc_media_release(media); /* Held by the media_list */
    }

    libvlc_media_t *media = libvlc_media_list_item_at_index(ctx->mlist, 0);
    libvlc_media_list_unlock(ctx->mlist);

    /* Set the first media */
    if (media != NULL)
    {
        libvlc_media_player_set_media(ctx->mp, media);
        libvlc_media_release(media);
    }

    return 0;
}

static void
setup_terminal(void)
{
#ifdef __linux__
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

    /* Avoid pressing enter to read commands */
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
#endif
}

static void
play_next(struct context *ctx)
{
    libvlc_media_list_lock(ctx->mlist);
    if (libvlc_media_list_count(ctx->mlist) == ctx->playing_idx + 1)
        ctx->playing_idx = 0;
    else
        ctx->playing_idx++;
    libvlc_media_t *media =
        libvlc_media_list_item_at_index(ctx->mlist, ctx->playing_idx);
    libvlc_media_list_unlock(ctx->mlist);

    libvlc_media_player_set_media(ctx->mp, media);
    libvlc_media_player_play(ctx->mp);
    libvlc_media_release(media);

}

static void
play_previous(struct context *ctx)
{
    libvlc_media_list_lock(ctx->mlist);
    if (ctx->playing_idx == 0)
        ctx->playing_idx = libvlc_media_list_count(ctx->mlist) - 1;
    else
        ctx->playing_idx--;
    libvlc_media_t *media =
        libvlc_media_list_item_at_index(ctx->mlist, ctx->playing_idx);
    libvlc_media_list_unlock(ctx->mlist);

    libvlc_media_player_set_media(ctx->mp, media);
    libvlc_media_player_play(ctx->mp);
    libvlc_media_release(media);
}

static void
increase_volume(struct context *ctx, int value)
{
    int volume = libvlc_audio_get_volume(ctx->mp);
    if (volume == -1)
        return;
    volume += value;
    if (volume < 0)
        volume = 0;
    else if (volume > 200)
        volume = 200;
    libvlc_audio_set_volume(ctx->mp, volume);
}

static int
mainloop_handle_command(struct context *ctx, const char *command, size_t len)
{
    if (len == 1)
    {
        switch (command[0])
        {
            case EOF:
            case 'q':
                return -1;
            case 'p':
                play_previous(ctx);
                break;
            case 'n':
                play_next(ctx);
                break;
            case ' ':
                libvlc_media_player_pause(ctx->mp);
                break;
            case 'e':
                libvlc_media_player_next_frame(ctx->mp);
                break;
            default:
                print_ui(ctx);
                break;
        }
    }
    else if (len > 2 && command[0] == '\033' && command[1] == '[')
    {
        /* Basic arrow key handling on Linux:
         * '\033' + '[' + 'A', 'B', 'C' or 'D' for all 4 keys
         * '\033' + "[1;5" + 'A', 'B', 'C' or 'D' for ctrol + 4 keys
         */
        bool ctrol_pressed;
        char c;
        if (len == 6 && strncmp("1;5", &command[2], 3) == 0)
        {
            c = command[5];
            ctrol_pressed = true;
        }
        else if (len == 3)
        {
            c = command[2];
            ctrol_pressed = false;
        }
        else
            return 0;

        int seek_jump = 1000; /* 1 second jump */
        int vol_jump = 5; /* 5% jump */
        if (ctrol_pressed)
        {
            seek_jump *= 10; /* 10 seconds jump */
            vol_jump = 10; /* 10% jump */
        }

        switch (c)
        {
            case 'D':
                libvlc_media_player_jump_time(ctx->mp, -seek_jump);
                break;
            case 'A':
                increase_volume(ctx, vol_jump);
                break;
            case 'C':
                libvlc_media_player_jump_time(ctx->mp, seek_jump);
                break;
            case 'B':
                increase_volume(ctx, -vol_jump);
                break;
        }
    }

    return 0;
}


static size_t
format_time_us(libvlc_time_t us, char *buffer, size_t buffer_size)
{
    time_t seconds = us / 1000000;

    struct tm tm;
    gmtime_r(&seconds, &tm);
    return strftime(buffer, buffer_size, "%k:%M:%S", &tm);
}

static bool
mainloop_display_ui(struct context *ctx)
{
    bool run_timer;
    pthread_mutex_lock(&ctx->mainloop_lock);

    libvlc_time_t ts_us;
    double pos;
    if (!ctx->time_seeking)
    {
        libvlc_time_t date = ctx->paused_date > 0 ? ctx->paused_date
                                                  : libvlc_clock();
        int ret =
            libvlc_media_player_time_point_interpolate(&ctx->time_point, date,
                                                       &ts_us, &pos);
        if (ret != 0)
        {
            pthread_mutex_unlock(&ctx->mainloop_lock);
            return false;
        }

        run_timer = ctx->paused_date == 0 && ctx->time_point.system_date_us != INT64_MAX;
    }
    else
    {
        ts_us = ctx->time_point.ts_us;
        pos = ctx->time_point.position;
        run_timer = false;
    }
    float volume = ctx->volume * 100;
    pos *= 100;
    libvlc_time_t length_us = ctx->time_point.length_us;
    pthread_mutex_unlock(&ctx->mainloop_lock);

    unsigned time_100ms = ((unsigned)(ts_us / 1000.0) % 1000) / 100;

    char buffer_time[256];
    size_t len = format_time_us(ts_us, buffer_time, sizeof(buffer_time));
    if (len > 0)
    {
        char buffer_length[256];

        len = format_time_us(length_us, buffer_length, sizeof(buffer_length));
        fprintf(stdout, "| time: %s.%u / %s | pos: %5.1f%% | vol: %3.0f%%\n",
                buffer_time, time_100ms, len > 0 ? buffer_length : "",
                pos, volume);
        fflush(stdout);
    }
    return run_timer;
}

static void
mainloop(struct context *ctx)
{
    bool run_timer = false;

    for (;;)
    {
        fd_set rfds;
        int nfds = 0;

        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(ctx->pipefds[0], &rfds);

        assert(ctx->pipefds[0] > STDIN_FILENO);
        nfds = ctx->pipefds[0] + 1;

        struct timeval tv_buf = {
            .tv_sec = 0,
            .tv_usec = 100000, /* Update pos/time every 100ms */
        };
        struct timeval *tv = run_timer ? &tv_buf : NULL;

        int retval = select(nfds, &rfds, NULL, NULL, tv);
        if (retval == -1)
        {
             perror("select()");
             return;
        }
        else if (retval == 0)
            run_timer = mainloop_display_ui(ctx);
        else
        {
            if (FD_ISSET(STDIN_FILENO, &rfds))
            {
                char buf[128];
                ssize_t len = read(STDIN_FILENO, buf, sizeof(buf));
                if (len == -1 && errno != EAGAIN)
                {
                    perror("read()");
                    return;
                }
                else if (len == 0)
                    continue;

                retval = mainloop_handle_command(ctx, buf, len);
                if (retval == -1)
                    return;
            }

            if (FD_ISSET(ctx->pipefds[0], &rfds))
            {
                char buf;
                ssize_t len = read(ctx->pipefds[0], &buf, 1);
                if (len == -1)
                {
                    perror("read()");
                    return;
                }
                run_timer = mainloop_display_ui(ctx);
            }
        }
    }
}

int
main(int argc, const char **argv)
{
    int mainret = EXIT_FAILURE, ret;

    /* mandatory to support UTF-8 filenames (provided the locale is well set)*/
    setlocale(LC_ALL, "");

    if (argc < 2)
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    static const char* const vlcargs[] = {
        "--verbose=1",
    };
    libvlc_instance_t *libvlc = libvlc_new(sizeof vlcargs / sizeof *vlcargs,
                                           vlcargs);
    if (libvlc == NULL)
    {
        fprintf(stderr, "libvlc_new() failed\n");
        return EXIT_FAILURE;
    }

    struct context ctx;
    ret = context_init(&ctx, libvlc);
    if (ret != 0)
    {
        libvlc_release(libvlc);
        fprintf(stderr, "context_init() failed\n");
        return EXIT_FAILURE;
    }

    ret = context_parse_argv(&ctx, argc, argv);
    if (ret != 0)
    {
        fprintf(stderr, "context_parse_argv() failed\n");
        goto error;
    }

    ret = libvlc_media_player_play(ctx.mp);
    if (ret != 0)
    {
        fprintf(stderr, "libvlc_media_player_play() failed\n");
        goto error;
    }

    setup_terminal();

    mainloop(&ctx);

    mainret = EXIT_SUCCESS;
error:
    libvlc_release(libvlc);
    context_destroy(&ctx);
    return mainret;
}
