/* Copyright Rafaël Carré (licence WTFPL) */
/* A video thumbnailer compatible with nautilus */
/* Copyright © 2007-2011 Rafaël Carré <funman@videolanorg> */

/* Works with : libvlc 1.2.0
   gcc -pedantic -Wall -Werror -Wextra `pkg-config --cflags --libs libvlc`

  # to register the thumbnailer:
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

static void callback(const libvlc_event_t *ev, void *param)
{
    float new_position = ev->u.media_player_position_changed.new_position;

    switch (ev->type) {

    case libvlc_MediaPlayerPositionChanged:
        if (new_position >= VLC_THUMBNAIL_POSITION * .9) { /* 90% margin */
            *(int*)param = 1;
        }
        break;
    case libvlc_MediaPlayerSnapshotTaken:
        *(int*)param = 1;
        break;

    default:
        assert(0);
    }
}

/* FIXME: should use pthread notification */
static void event_wait(const char *error, int *f)
{
#define VLC_THUMBNAIL_TIMEOUT   5.0 /* 5 secs */
#define VLC_THUMBNAIL_LOOP_STEP 0.2 /* 200 ms */

    float max = VLC_THUMBNAIL_TIMEOUT;
    while ((max -= VLC_THUMBNAIL_LOOP_STEP) > 0.) {
        if (*f)
            return;

        usleep(VLC_THUMBNAIL_LOOP_STEP * 1000000);
    }

    fprintf(stderr,
            "%s (timeout after %.2f secs!\n", error, VLC_THUMBNAIL_TIMEOUT);
    exit(1);
}

static void set_position(libvlc_media_player_t *mp)
{
    int f = 0;
    libvlc_event_manager_t *em = libvlc_media_player_event_manager(mp);
    assert(em);

    libvlc_event_attach(em, libvlc_MediaPlayerPositionChanged, callback, &f);
    libvlc_media_player_set_position(mp, VLC_THUMBNAIL_POSITION);
    event_wait("Couldn't set position", &f);
    libvlc_event_detach(em, libvlc_MediaPlayerPositionChanged, callback, &f);
}

static void snapshot(libvlc_media_player_t *mp, int width, char *out_with_ext)
{
    int f = 0;
    libvlc_event_manager_t *em = libvlc_media_player_event_manager(mp);
    assert(em);

    libvlc_event_attach(em, libvlc_MediaPlayerSnapshotTaken, callback, &f);
    libvlc_video_take_snapshot(mp, 0, out_with_ext, width, 0);
    event_wait("Snapshot has not been written", &f);
    libvlc_event_detach(em, libvlc_MediaPlayerSnapshotTaken, callback, &f);
}

int main(int argc, const char **argv)
{
    const char *in;
    char *out, *out_with_ext;
    int width;
    libvlc_instance_t *libvlc;
    libvlc_media_player_t *mp;
    libvlc_media_t *m;

    /* mandatory to support UTF-8 filenames (provided the locale is well set)*/
    setlocale(LC_ALL, "");

    cmdline(argc, argv, &in, &out, &out_with_ext, &width);

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

    return 0;
}
