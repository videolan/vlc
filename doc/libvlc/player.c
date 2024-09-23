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

#ifdef __linux__
#include <termios.h>
#include <unistd.h>
#endif

#include <vlc/vlc.h>

struct context
{
    libvlc_media_list_t *mlist;
    libvlc_media_player_t *mp;
    int playing_idx;
    char *playing_title;
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
on_media_changed(void *opaque, libvlc_media_t *media)
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
    fprintf(stderr, "ctx->playing_idx: %d\n", ctx->playing_idx);
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
on_media_subitems_changed(void *opaque, libvlc_media_t *media)
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

static int
context_init(struct context *ctx, libvlc_instance_t *libvlc)
{
    ctx->mlist = libvlc_media_list_new();
    if (ctx->mlist == NULL)
        return -ENOMEM;

    static const struct libvlc_media_player_cbs cbs = {
        .version = 0,
        .on_media_changed = on_media_changed,
        .on_media_subitems_changed = on_media_subitems_changed,
    };

    ctx->mp = libvlc_media_player_new(libvlc, &cbs, ctx);
    if (ctx->mp == NULL)
    {
        libvlc_media_list_release(ctx->mlist);
        return -ENOMEM;
    }

    ctx->playing_idx = 0;
    ctx->playing_title = NULL;
    return 0;
}

static void
context_destroy(struct context *ctx)
{
    libvlc_media_list_release(ctx->mlist);
    libvlc_media_player_release(ctx->mp);
    free(ctx->playing_title);
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
    /* Avoid pressing enter to read commands */
#ifdef __linux__
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~ICANON;
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
increase_volume(struct context *ctx, int sign)
{
    int volume = libvlc_audio_get_volume(ctx->mp);
    if (volume == -1)
        return;
    volume += 10 * sign;
    if (volume < 0)
        volume = 0;
    else if (volume > 200)
        volume = 200;
    libvlc_audio_set_volume(ctx->mp, volume);
}

static void
mainloop(struct context *ctx)
{
    int c;
    while ((c = getchar()) != EOF)
    {
        switch (c)
        {
            case 'p':
                play_previous(ctx);
                break;
            case 'n':
                play_next(ctx);
                break;
            case ' ':
                libvlc_media_player_pause(ctx->mp);
                break;
            case 'q':
                return;
            case 'e':
                libvlc_media_player_next_frame(ctx->mp);
                break;
            case '\033':
                /* Escape char when pressing arrow keys */
                break;
            case 'D':
                libvlc_media_player_jump_time(ctx->mp, -1000);
                break;
            case 'A':
                increase_volume(ctx, 1);
                break;
            case 'C':
                libvlc_media_player_jump_time(ctx->mp, 1000);
                break;
            case 'B':
                increase_volume(ctx, -1);
                break;
            default:
                print_ui(ctx);
                break;
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
