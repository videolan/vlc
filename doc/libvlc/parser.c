/*****************************************************************************
 * parser.c:  libvlc_parser API sample usage
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
#include <locale.h>
#include <semaphore.h>
#include <errno.h>
#include <inttypes.h>

#include <vlc/vlc.h>

#define VLC_PARSER_TIMEOUT   5000 /* 5 secs */

static const char *
status_to_string(libvlc_parser_status_t status)
{
    switch (status)
    {
        case libvlc_parser_status_failed:     return "failed";
        case libvlc_parser_status_timeout:    return "timeout";
        case libvlc_parser_status_cancelled:  return "cancelled";
        case libvlc_parser_status_done:       return "done";
        default: abort(); /* assert/unreachable */
    }
}

static const char *
type_to_string(libvlc_media_type_t type)
{
    switch (type)
    {
        case libvlc_media_type_unknown:     return "unknown";
        case libvlc_media_type_file:        return "file";
        case libvlc_media_type_directory:   return "directory";
        case libvlc_media_type_disc:        return "disc";
        case libvlc_media_type_stream:      return "stream";
        case libvlc_media_type_playlist:    return "playlist";
        default: abort(); /* assert/unreachable */
    }
}

static void
on_media_parsed(void *opaque, libvlc_parser_task *task,
                libvlc_parser_status_t status)
{
    /* Function called when is request is done (with success or error) */
    sem_t *sem = opaque;
    libvlc_media_t *media = libvlc_parser_task_get_media(task);

    const char *status_str = status_to_string(status);
    const char *type_str = type_to_string(libvlc_media_get_type(media));

    char *mrl = libvlc_media_get_mrl(media);
    printf("Parsed '%s'\n"
           "\tstatus: %s\n"
           "\ttype: %s\n"
           "\tduration: %"PRId64"\n",
           mrl != NULL ? mrl : "<nil>", status_str,
           type_str,
           libvlc_media_get_duration(media));
    free(mrl);

    /* Signal the main thread that one request was processed */
    sem_post(sem);
    libvlc_parser_task_release(task);
}

static const struct libvlc_parser_cbs cbs = {
    .version = 0,
    .on_parsed = on_media_parsed,
};

static int
parse_file_async(libvlc_parser_t *parser, const char *url, sem_t *sem)
{
    /* Create a media */
    libvlc_media_t *media = NULL;
    if (strstr(url, "://") != NULL)
        media = libvlc_media_new_location(url);
    else
        media = libvlc_media_new_path(url);

    if (media == NULL)
        return -ENOMEM;

    const libvlc_media_parse_flag_t parse_flags =
        libvlc_media_fetch_local | libvlc_media_fetch_network |
        libvlc_media_parse;

    /* A request */
    libvlc_parser_request_t request = {
        .version = 0,
        .media = media,
        .parse_flags = parse_flags,
    };

    /* And send it to the parser, the actual parsing will be done from an other
     * thread */
    libvlc_parser_task *task = libvlc_parser_queue(parser, &request, &cbs, sem);
    libvlc_media_release(media); /* media held by the request */
    if (task == NULL)
    {
        fprintf(stderr, "Failed to queue parsing request\n");
        return -1; /* generic error code */
    }

    return 0;
}

static libvlc_parser_t *
create_parser(const struct libvlc_parser_cfg *cfg)
{
    static const char* const args[] = {
        "--verbose=1",
    };
    libvlc_instance_t *libvlc = libvlc_new(sizeof args / sizeof *args, args);
    if (libvlc == NULL)
        return NULL;

    libvlc_parser_t *parser = libvlc_parser_new(libvlc, cfg);
    libvlc_release(libvlc); /* instance held by the parser */

    return parser;
}

static void
print_usage(const char *name)
{
    fprintf(stderr, "Usage: %s <mediatoparse1> [mediatoparse2]...\n", name);
}

int
main(int argc, const char **argv)
{
    /* mandatory to support UTF-8 filenames (provided the locale is well set)*/
    setlocale(LC_ALL, "");

    if (argc < 2)
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    /* Semaphore to wait for asynchronous parsing, do not use that mechanism
     * if your applications has a UI or an event loop, since it will block. */
    sem_t sem;
    sem_init(&sem, 0, 0);

    const struct libvlc_parser_cfg cfg = {
        .version = 0,
        .max_parser_threads = 1,
        .timeout = VLC_PARSER_TIMEOUT,
    };

    libvlc_parser_t *parser = create_parser(&cfg);
    if (parser == NULL)
    {
        sem_destroy(&sem);
        return EXIT_FAILURE;
    }

    size_t request_count = argc - 1;
    /* Parse all files given in arguments */
    for (int i = 1; i < argc; ++i)
    {
        const char *url = argv[i];

        int err = parse_file_async(parser, url, &sem);
        if (err != 0)
        {
            /* error: don't wait on this request */
            request_count--;
        }
    }

    /* Wait for all request to be processed */
    for (size_t i = 0; i < request_count; ++i)
        sem_wait(&sem);

    /* Must be called once all request are destroyed */
    libvlc_parser_destroy(parser);

    sem_destroy(&sem);

    return EXIT_SUCCESS;
}
