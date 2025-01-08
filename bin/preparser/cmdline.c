// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * cmdline.c: preparser command line
 *****************************************************************************
 * Copyright © 2025 Videolabs, VideoLAN and VLC authors
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc/vlc.h>
#include <vlc_common.h>
#include <vlc_preparser.h>

#include "cmdline.h"

#include "../src/config/vlc_getopt.h"
#include "../src/config/vlc_jaro_winkler.h"

static bool
opt_set_Timeout(struct preparser_args *args, const char *arg)
{
    assert(args != NULL);
    assert(arg != NULL);

    char *endptr = NULL;
    args->timeout = VLC_TICK_FROM_MS(strtoull(arg, &endptr, 0));
    if (endptr != NULL && *endptr != '\0') {
        fprintf(stderr, "Error: Invalid timeout `%s'\n", arg);
        return false;
    }
    return true;
}

static bool
opt_set_TimeoutTick(struct preparser_args *args, const char *arg)
{
    assert(args != NULL);
    assert(arg != NULL);

    char *endptr = NULL;
    args->timeout = strtoull(arg, &endptr, 0);
    if (endptr != NULL && *endptr != '\0') {
        fprintf(stderr, "Error: Invalid timeout `%s'\n", arg);
        return false;
    }
    return true;
}

static bool
opt_set_Types(struct preparser_args *args, const char *arg)
{
    assert(args != NULL);
    assert(arg != NULL);


    char *endptr = NULL;
    args->types = strtoll(arg, &endptr, 0);
    if (endptr != NULL && *endptr != '\0') {
        fprintf(stderr, "Error: Invalid type `%s'\n", arg);
        return false;
    }

    return true;
}

static bool
opt_set_Type(struct preparser_args *args, const char *arg)
{
    assert(args != NULL);
    assert(arg != NULL);

    if (!strcmp(arg, "parse")) {
        args->types |= VLC_PREPARSER_TYPE_PARSE;
    } else if (!strcmp(arg, "thumbnail") || !strcmp(arg, "jpg")) {
        args->types |= VLC_PREPARSER_TYPE_THUMBNAIL;
    } else if (!strcmp(arg, "thumbnail_to_files")) {
        args->types |= VLC_PREPARSER_TYPE_THUMBNAIL_TO_FILES;
    } else {
        fprintf(stderr, "Error: Unknown preparser type `%s'\n", arg);
        return false;
    }
    return true;

    return true;
}

static bool
opt_set_Fetch(struct preparser_args *args, const char *arg)
{
    assert(args != NULL);
    assert(arg != NULL);

    if (!strcmp(arg, "local")) {
        args->types |= VLC_PREPARSER_TYPE_FETCHMETA_LOCAL;
    } else if (!strcmp(arg, "net") || !strcmp(arg, "jpg")) {
        args->types |= VLC_PREPARSER_TYPE_FETCHMETA_NET;
    } else if (!strcmp(arg, "all")) {
        args->types |= VLC_PREPARSER_TYPE_FETCHMETA_ALL;
    } else {
        fprintf(stderr, "Error: Unknown preparser fetching policy `%s'\n", arg);
        return false;
    }
    return true;

    return true;
}

static bool
opt_set_Daemon(struct preparser_args *args, const char *arg)
{
    assert(args != NULL);
    assert(arg == NULL);

    args->daemon = true;
    return true;
}

static bool
opt_set_SeekSpeed(struct preparser_args *args, const char *arg)
{
    assert(args != NULL);
    assert(arg != NULL);

    if (!strcmp(arg, "precise")) {
        args->seek.speed = VLC_THUMBNAILER_SEEK_PRECISE;
    } else if (!strcmp(arg, "fast")) {
        args->seek.speed = VLC_THUMBNAILER_SEEK_FAST;
    } else {
        fprintf(stderr, "Error: Unknown seek speed `%s'\n", arg);
        return false;
    }
    return true;
}

static bool
opt_set_SeekTime(struct preparser_args *args, const char *arg)
{
    assert(args != NULL);
    assert(arg != NULL);

    if (args->seek.type != VLC_THUMBNAILER_SEEK_NONE) {
        fprintf(stderr, "Error: --seek-time not compatible with --seek-pos\n");
        return false;
    }

    char *endptr = NULL;
    args->seek.time = VLC_TICK_FROM_MS(strtoull(arg, &endptr, 0));
    if (endptr != NULL && *endptr != '\0') {
        fprintf(stderr, "Error: Invalid time `%s'\n", arg);
        return false;
    }
    args->seek.type = VLC_THUMBNAILER_SEEK_TIME;
    return true;
}

static bool
opt_set_SeekPos(struct preparser_args *args, const char *arg)
{
    assert(args != NULL);
    assert(arg != NULL);

    if (args->seek.type != VLC_THUMBNAILER_SEEK_NONE) {
        fprintf(stderr, "Error: --seek-pos not compatible with --seek-time\n");
        return false;
    }

    char *endptr = NULL;
    args->seek.pos = strtod(arg, &endptr);
    if (endptr != NULL && *endptr != '\0') {
        fprintf(stderr, "Error: Invalid position `%s'\n", arg);
        return false;
    }
    if (!(args->seek.pos > 0)) {
        fprintf(stderr, "Error: Seek pos should be greater than 0\n");
        return false;
    }
    args->seek.pos = VLC_THUMBNAILER_SEEK_POS;
    return true;
}

static bool
opt_set_OutputPath(struct preparser_args *args, const char *arg)
{
    assert(args != NULL);
    assert(arg != NULL);

    if (args->output.file_path != NULL) {
        fprintf(stderr, "Error: --output-path already setted\n");
        return false;
    }

    args->output.file_path = arg;
    if (args->output.file_path == NULL) {
        fprintf(stderr, "Error: memory error\n");
        return false;
    }
    return true;
}

static bool
opt_set_OutputWidth(struct preparser_args *args, const char *arg)
{
    assert(args != NULL);
    assert(arg != NULL);

    if (args->output.width != 0) {
        fprintf(stderr, "Error: --output-width already setted\n");
        return false;
    }
    char *endptr = NULL;
    args->output.width = strtoull(arg, &endptr, 0);
    if (endptr != NULL && *endptr != '\0') {
        fprintf(stderr, "Error: Invalid time `%s'\n", arg);
        return false;
    }
    return true;
}

static bool
opt_set_OutputHeight(struct preparser_args *args, const char *arg)
{
    assert(args != NULL);
    assert(arg != NULL);

    if (args->output.height != 0) {
        fprintf(stderr, "Error: --output-height already setted\n");
        return false;
    }
    char *endptr = NULL;
    args->output.height = strtoull(arg, &endptr, 0);
    if (endptr != NULL && *endptr != '\0') {
        fprintf(stderr, "Error: Invalid time `%s'\n", arg);
        return false;
    }
    return true;
}

static bool
opt_set_OutputFormat(struct preparser_args *args, const char *arg)
{
    assert(args != NULL);
    assert(arg != NULL);

    if (!strcmp(arg, "png")) {
        args->output.format = VLC_THUMBNAILER_FORMAT_PNG;
    } else if (!strcmp(arg, "jpeg") || !strcmp(arg, "jpg")) {
        args->output.format = VLC_THUMBNAILER_FORMAT_JPEG;
    } else if (!strcmp(arg, "webp")) {
        args->output.format = VLC_THUMBNAILER_FORMAT_WEBP;
    } else {
        fprintf(stderr, "Error: Unknown output format `%s'\n", arg);
        return false;
    }
    return true;
}

static bool
opt_set_OutputCrop(struct preparser_args *args, const char *arg)
{
    assert(args != NULL);
    assert(arg == NULL);

    args->output.crop = true;
    return true;
}

static bool opt_set_Verbose(struct preparser_args *args, const char *arg)
{
    assert(args != NULL);
    assert(arg != NULL);

    args->verbosity = arg;
    return true;
}

struct preparser_opt {
    const char *name;
    const char *text;
    const int type;
    bool (*set)(struct preparser_args *args, const char *arg);
};

#define opt_type_bool 1
#define opt_add_bool(n, set_fn, description)\
    {.name = n, .type = opt_type_bool, .text = description, .set = set_fn}

#define opt_type_integer 2
#define opt_add_integer(n, set_fn, description)\
    {.name = n, .type = opt_type_integer, .text = description, .set = set_fn}

#define opt_type_string 3
#define opt_add_string(n, set_fn, description)\
    {.name = n, .type = opt_type_string, .text = description, .set = set_fn}

static const struct preparser_opt options[] = {
    opt_add_bool("help", NULL, "Print this help"),
    opt_add_integer("timeout", opt_set_Timeout, "Preparser timeout (ms)"),
    opt_add_integer("timeout-tick", opt_set_TimeoutTick, "Preparser timeout (vlc_tick_t)"),
    opt_add_integer("types", opt_set_Types, "Preparser types"),
    opt_add_string("type", opt_set_Type, "Preparser type (parse/thumbnail/thumbnail_to_files)"),
    opt_add_string("fetch", opt_set_Fetch, "Preparser fetching (local/net/all)"),
    opt_add_bool("daemon", opt_set_Daemon, "Start the preparser as a daemon reading request from the stdin"),
    opt_add_bool(NULL, NULL, "thumbnail and thumbnail_to_files"),
    opt_add_string("seek-speed", opt_set_SeekSpeed, "Set the seek speed (precise/fast)"),
    opt_add_integer("seek-time", opt_set_SeekTime, "Set from where to seek (ms)"),
    opt_add_integer("seek-pos", opt_set_SeekPos, "Set the seek position"),
    opt_add_bool(NULL, NULL, "thumbnail_to_files"),
    opt_add_string("output-path", opt_set_OutputPath, "Path of the thumbnail"),
    opt_add_integer("output-width", opt_set_OutputWidth, "Width of the thumbnail"),
    opt_add_integer("output-height", opt_set_OutputHeight, "Height of the thumbnail"),
    opt_add_string("output-format", opt_set_OutputFormat, "Format of the thumbnail (png/jp[e]g/webp)"),
    opt_add_bool("output-crop", opt_set_OutputCrop, "Crop the thumbnail"),
    opt_add_integer("verbose", opt_set_Verbose, "Verbosity (0,1,2)"),
};

static const char vlc_preparser_usage[] = N_(
    "Usage: %s [options] [url|path]\n"
    "\n"
    "URL syntax:\n"
    "  file:///path/file              Plain media file\n"
    "  http://host[:port]/file        HTTP URL\n"
    "  ftp://host[:port]/file         FTP URL\n"
    "  mms://host[:port]/file         MMS URL\n"
    "  screen://                      Screen capture\n"
    "  dvd://[device]                 DVD device\n"
    "  vcd://[device]                 VCD device\n"
    "  cdda://[device]                Audio CD device\n"
    "  udp://[[<source address>]@[<bind address>][:<bind port>]]\n"
    "                                 UDP stream sent by a streaming server\n"
    "  vlc://pause:<seconds>          Pause the playlist for a certain time\n"
    "  vlc://quit                     Special item to quit VLC\n"
    "\n"
);

static void
preparser_help(const char *arg0)
{
    assert(arg0 != NULL);

    fprintf(stderr, vlc_preparser_usage, arg0);
    fprintf(stderr, "Options:\n");
    size_t nopts = ARRAY_SIZE(options);
    for (size_t i = 0; i < nopts; i++) {
        if (options[i].name == NULL) {
            fprintf(stderr, "\n  %s\n", options[i].text);
            continue;
        }
        const char *type = NULL;
        switch (options[i].type) {
            case opt_type_integer:
                type = "<integer>";
                break;
            case opt_type_string:
                type = "<string>";
                break;
        }
        int ret = fprintf(stderr, "  --%s %s", options[i].name,
                          type == NULL ? "" : type);
        if (ret >= 33) {
            ret = 0;
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "%*s%s\n", 33 - ret, "", options[i].text);
    }
    fprintf(stderr, "\n");
}


int
preparser_cmdline_Parse(int argc, char *const *argv,
                        struct preparser_args *args)
{
    assert(argc != 0);
    assert(argv != NULL);
    assert(args != NULL);

    size_t noptions = ARRAY_SIZE(options);
    struct vlc_option *opts = calloc(noptions + 1, sizeof(*opts));
    for (size_t i = 0; i < noptions; i++) {
        if (options[i].name == NULL) {
            opts[i].name = "";
        } else {
            opts[i].name = options[i].name;
        }
        opts[i].has_arg = options[i].type == opt_type_bool ? false : true;
        opts[i].flag = NULL;
        opts[i].is_obsolete = false;
        opts[i].val = 0;
    }

    bool error = false;
    vlc_getopt_t state = {0};
    int cmd = 0;
    int longid = 0;
    while (1) {
        cmd = vlc_getopt_long(argc, argv, ":", opts, &longid, &state);
        if (cmd == -1) {
            break;
        } else if (cmd == 0) {
            if (options[longid].set == NULL) {
                preparser_help(argv[0]);
                break;
            } else if (!options[longid].set(args, state.arg)) {
                error = true;
                fprintf(stderr, "For more information try --help\n");
                break;
            }
        } else {
            if (cmd == ':') {
                if (state.opt != 0) {
                    fprintf(stderr,
                            "Error: Missing mandatory value for option -%c\n",
                            cmd);
                } else {
                    fprintf(stderr,
                            "Error: Missing mandatory value for option %s\n",
                            argv[state.ind - 1]);
                }
            } else if (state.opt != 0) {
                fprintf(stderr, "Error: Unknown option `-%c'\n", state.opt);
            } else {
                fprintf(stderr, "Error: Unknown option `%s'\n",
                        argv[state.ind - 1]);

                /* suggestion matching */
                float jw_filter = 0.8f;
                float best_m = jw_filter;
                float m = 0;
                const char *best = NULL;
                const char *jw_a = argv[state.ind - 1] + 2;
                for (size_t i = 0; i < noptions; i++) {
                    if (opts[i].name == NULL) {
                        continue;
                    }
                    if (opts[i].is_obsolete)
                        continue;
                    const char *jw_b = opts[i].name;
                    if (vlc_jaro_winkler(jw_a, jw_b, &m) == 0) {
                        //ignore failed malloc calls
                        if (m > best_m || (!best && m >= jw_filter)) {
                            best = jw_b;
                            best_m = m;
                        }
                    }
                }
                if (best != NULL) {
                    fprintf( stderr, "       Did you mean --%s?\n", best);
                }
            }
            fprintf( stderr, "For more information try --help\n");
            return -1;
        }
    }
    free(opts);

    if (error) {
        return -1;
    } else if (cmd != -1) {
        return 0;
    }

    if (args->types == 0) {
        args->types = VLC_PREPARSER_TYPE_PARSE;
    }

    args->arg_idx = state.ind;
    if (!args->daemon) {
        if (args->types == VLC_PREPARSER_TYPE_THUMBNAIL_TO_FILES) {
            if (args->output.file_path == NULL) {
                fprintf(stderr, "Error: with `--type thummbnail_to_files' the "
                                "option `--output-path' is mandatory!\n");
                fprintf( stderr, "For more information try --help\n");
                return -1;
            }
        }
        if (args->arg_idx == argc) {
            fprintf(stderr, "No media to parse!\n");
            return -1;
        }
    }

    return 1;
}

