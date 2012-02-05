/* Copyright Rafaël Carré (licence WTFPL) */
/* A video thumbnailer compatible with nautilus */
/* Copyright © 2007-2011 Rafaël Carré <funman@videolanorg> */

/* Works with : libvlc 2.0.0
   gcc -pedantic -Wall -Werror -Wextra `pkg-config --cflags --libs libvlc` -lpthread

  # to register the thumbnailer on gnome 3.x:
  cp vlc-thumb.thumbnailer /usr/share/thumbnailers

  # to register the thumbnailer on gnome 2.x:
  list=`grep ^Mime vlc.desktop|cut -d= -f2-|sed -e s/";"/\\\n/g -e s,/,@,g`
  vid=`echo $mimes|grep ^vid`
  for i in $vid
  do 
    key=/desktop/gnome/thumbnailers/$i/enable
    gconftool-2 -t boolean -s $key true
    gconftool-2 -t string  -s $key "vlc-thumb -s %s %u %o"
  done
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#if !defined (_POSIX_CLOCK_SELECTION)
#  define _POSIX_CLOCK_SELECTION (-1)
#endif

#if (_POSIX_CLOCK_SELECTION < 0)
#   error Clock selection is not available!
#endif

#include <vlc/vlc.h>

/* position at which the snapshot is taken */
#define VLC_THUMBNAIL_POSITION (30./100.)

static void usage(const char *name, int ret)
{
    fprintf(stderr, "Usage: %s [-s width] <video> <output.png>\n", name);
    exit(ret);
}

/* extracts options from command line */
static void cmdline(int argc, const char **argv, const char **in,
                    char **out, char **out_with_ext, int *w)
{
    int idx = 1;
    size_t len;

    if (argc != 3 && argc != 5)
        usage(argv[0], argc != 2 || strcmp(argv[1], "-h"));

    *w = 0;

    if (argc == 5) {
        if (strcmp(argv[1], "-s"))
            usage(argv[0], 1);

        idx += 2; /* skip "-s width" */
        *w = atoi(argv[2]);
    }

    *in  = argv[idx++];
    *out = strdup(argv[idx++]);
    if (!*out)
        abort();

    len = strlen(*out);
    if (len >= 4 && !strcmp(*out + len - 4, ".png")) {
        *out_with_ext = *out;
        return;
    }

    /* We need to add .png extension to filename,
     * VLC relies on it to detect output format,
     * and nautilus doesn't give filenames ending in .png */

    *out_with_ext = malloc(len + sizeof ".png");
    if (!*out_with_ext)
        abort();
    strcpy(*out_with_ext, *out);
    strcat(*out_with_ext, ".png");
}

static libvlc_instance_t *create_libvlc(void)
{
    static const char* const args[] = {
        "--intf", "dummy",                  /* no interface                   */
        "--vout", "dummy",                  /* we don't want video (output)   */
        "--no-audio",                       /* we don't want audio (decoding) */
        "--no-video-title-show",            /* nor the filename displayed     */
        "--no-stats",                       /* no stats                       */
        "--no-sub-autodetect-file",         /* we don't want subtitles        */
        "--no-inhibit",                     /* we don't want interfaces       */
        "--no-disable-screensaver",         /* we don't want interfaces       */
        "--no-snapshot-preview",            /* no blending in dummy vout      */
#ifndef NDEBUG
        "--verbose=2",                      /* full log                       */
#endif
    };

    return libvlc_new(sizeof args / sizeof *args, args);
}

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  wait;
static bool done;

static void callback(const libvlc_event_t *ev, void *param)
{
    float new_position;
    (void)param;

    pthread_mutex_lock(&lock);
    switch (ev->type) {
    case libvlc_MediaPlayerPositionChanged:
        new_position = ev->u.media_player_position_changed.new_position;
        if (new_position < VLC_THUMBNAIL_POSITION * .9 /* 90% margin */)
            break;
    case libvlc_MediaPlayerSnapshotTaken:
        done = true;
        pthread_cond_signal(&wait);
        break;

    default:
        assert(0);
    }
    pthread_mutex_unlock(&lock);
}

static void event_wait(const char *error)
{
    int ret;
    struct timespec ts;
#define VLC_THUMBNAIL_TIMEOUT   5.0 /* 5 secs */

    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec += VLC_THUMBNAIL_TIMEOUT;
    pthread_mutex_lock(&lock);
    ret = done ? 0 : pthread_cond_timedwait(&wait, &lock, &ts);
    pthread_mutex_unlock(&lock);

    assert(!ret || ret == ETIMEDOUT);

    if (ret) {
        fprintf(stderr,
                "%s (timeout after %.2f secs!\n", error, VLC_THUMBNAIL_TIMEOUT);
        exit(1);
    }
}

static void set_position(libvlc_media_player_t *mp)
{
    libvlc_event_manager_t *em = libvlc_media_player_event_manager(mp);
    assert(em);

    libvlc_event_attach(em, libvlc_MediaPlayerPositionChanged, callback, NULL);
    done = false;
    libvlc_media_player_set_position(mp, VLC_THUMBNAIL_POSITION);
    event_wait("Couldn't set position");
    libvlc_event_detach(em, libvlc_MediaPlayerPositionChanged, callback, NULL);
}

static void snapshot(libvlc_media_player_t *mp, int width, char *out_with_ext)
{
    libvlc_event_manager_t *em = libvlc_media_player_event_manager(mp);
    assert(em);

    libvlc_event_attach(em, libvlc_MediaPlayerSnapshotTaken, callback, NULL);
    done = false;
    libvlc_video_take_snapshot(mp, 0, out_with_ext, width, 0);
    event_wait("Snapshot has not been written");
    libvlc_event_detach(em, libvlc_MediaPlayerSnapshotTaken, callback, NULL);
}

int main(int argc, const char **argv)
{
    const char *in;
    char *out, *out_with_ext;
    int width;
    pthread_condattr_t attr;
    libvlc_instance_t *libvlc;
    libvlc_media_player_t *mp;
    libvlc_media_t *m;

    /* mandatory to support UTF-8 filenames (provided the locale is well set)*/
    setlocale(LC_ALL, "");

    cmdline(argc, argv, &in, &out, &out_with_ext, &width);

    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&wait, &attr);
    pthread_condattr_destroy(&attr);

    /* starts vlc */
    libvlc = create_libvlc();
    assert(libvlc);

    m = libvlc_media_new_path(libvlc, in);
    assert(m);

    mp = libvlc_media_player_new_from_media(m);
    assert(mp);

    libvlc_media_player_play(mp);

    /* takes snapshot */
    set_position(mp);
    snapshot(mp, width, out_with_ext);

    libvlc_media_player_stop(mp);

    /* clean up */
    if (out != out_with_ext) {
        rename(out_with_ext, out);
        free(out_with_ext);
    }
    free(out);

    libvlc_media_player_release(mp);
    libvlc_media_release(m);
    libvlc_release(libvlc);

    pthread_cond_destroy(&wait);

    return 0;
}
