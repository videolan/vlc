/*****************************************************************************
 * ytdl.c:
 *****************************************************************************
 * Copyright (C) 2019-2020 Rémi Denis-Courmont
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

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "json/json.h"
#include <vlc_common.h>
#include <vlc_stream.h>
#include <vlc_fs.h>
#include <vlc_input_item.h>
#include <vlc_plugin.h>
#include <vlc_spawn.h>

void json_parse_error(void *data, const char *msg)
{
    struct vlc_logger *log = data;

    vlc_error(log, "%s", msg);
}

static
FILE *vlc_popen(pid_t *restrict pid, const char *argv[])
{
    int fds[2];

    if (vlc_pipe(fds))
        return NULL;

    FILE *input = fdopen(fds[0], "rt");

    if (input == NULL) {
        vlc_close(fds[1]);
        vlc_close(fds[0]);
        return NULL;
    }

    int fdv[] = { -1, fds[1], 2, -1 };
    int val = vlc_spawn(pid, argv[0], fdv, argv);

    vlc_close(fds[1]);

    if (val) {
        fclose(input);
        input = NULL;
        errno = val;
    }

    return input;
}

struct ytdl_playlist {
    struct json_object json;
};

static const struct json_object *PickFormat(stream_t *s,
                                            const struct json_object *entry)
{
     const struct json_value *fmts = json_get(entry, "formats");

     if (fmts == NULL)
         return entry; /* only one format */
     if (fmts->type != JSON_ARRAY)
         return NULL;

     const struct json_object *best_fmt = NULL;
     double pref_height = var_InheritInteger(s, "preferred-resolution");
     double best_height = -1.;
     double best_abr = -1.;

     for (size_t i = 0; i < fmts->array.size; i++) {
         const struct json_value *v = &fmts->array.entries[i];

         if (v->type != JSON_OBJECT)
             continue;

         const struct json_object *fmt = &v->object;
         double height = json_get_num(fmt, "height");
         double abr = json_get_num(fmt, "abr");

         if (!isgreaterequal(height, best_height)
          || (best_height < pref_height && pref_height < height))
             continue;

         if (!isgreaterequal(abr, best_abr))
             continue;

         best_fmt = fmt;
         best_height = height;
         best_abr = abr;
     }

     return best_fmt;
}

static const char *PickArt(const struct json_object *entry)
{
    const struct json_value *v = json_get(entry, "thumbnails");

    if (v == NULL || v->type != JSON_ARRAY || v->array.size == 0)
        return NULL;

    v = &v->array.entries[0];

    if (v->type != JSON_OBJECT)
        return NULL;

    return json_get_str(&v->object, "url");
}

static int ReadItem(stream_t *s, input_item_node_t *node,
                    const struct json_object *json)
{
    const struct json_object *fmt = PickFormat(s, json);

    if (fmt == NULL)
        return VLC_EGENERIC;

    const char *url = json_get_str(fmt, "url");

    if (url == NULL)
        return VLC_EGENERIC;

    const char *title = json_get_str(json, "title");
    double duration = json_get_num(json, "duration");
    vlc_tick_t ticks = isnan(duration) ? INPUT_DURATION_UNSET
                                       : lround(duration * CLOCK_FREQ);

    if (title == NULL)
        title = url;

    input_item_t *item = input_item_NewStream(url, title, ticks);

    if (unlikely(item == NULL))
        return VLC_ENOMEM;

    const char *desc = json_get_str(json, "description");
    if (desc != NULL)
        input_item_SetDescription(item, desc);

    const char *author = json_get_str(json, "uploader");
    if (author != NULL)
        input_item_SetArtist(item, author);

    const char *arturl = PickArt(json);
    if (arturl != NULL)
        input_item_SetArtURL(item, arturl);

    input_item_AddOption(item, "no-ytdl", 0);
    input_item_node_AppendItem(node, item);
    input_item_Release(item);

    return VLC_SUCCESS;
}

static int ReadDir(stream_t *s, input_item_node_t *node)
{
    struct ytdl_playlist *sys = s->p_sys;
    const struct json_value *v = json_get(&sys->json, "entries");

    if (v == NULL) /* Single item */
        return ReadItem(s, node, &sys->json);

    /* Playlist: parse each entry */
    if (v->type != JSON_ARRAY)
        return VLC_EGENERIC;

    for (size_t i = 0; i < v->array.size; i++) {
         const struct json_value *e = &v->array.entries[i];

         if (e->type == JSON_OBJECT)
             ReadItem(s, node, &e->object);
    }

    return VLC_SUCCESS;
}

static int Control(stream_t *s, int query, va_list args)
{
    switch (query)
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg(args, bool *) = false;
            break;

        case STREAM_GET_TYPE:
            *va_arg(args, int *) = ITEM_TYPE_PLAYLIST;
            break;

        case STREAM_GET_PTS_DELAY:
            *va_arg(args, vlc_tick_t *) =
                 VLC_TICK_FROM_MS(var_InheritInteger(s, "network-caching"));
            break;

        default:
            return VLC_EGENERIC;

    }

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    stream_t *s = (stream_t *)obj;
    struct ytdl_playlist *sys = s->p_sys;

    json_free(&sys->json);
}

static int OpenCommon(vlc_object_t *obj)
{
    stream_t *s = (stream_t *)obj;

    struct ytdl_playlist *sys = vlc_obj_malloc(obj, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_EGENERIC;

    char *path = config_GetSysPath(VLC_PKG_DATA_DIR, "ytdl-extract.py");
    if (unlikely(path == NULL))
        return VLC_EGENERIC;

    pid_t pid;
    const char *argv[] = { path, s->psz_url, NULL };
    FILE *input = vlc_popen(&pid, argv);

    if (input == NULL) {
        msg_Dbg(obj, "cannot start %s: %s", path, vlc_strerror_c(errno));
        free(path);
        return VLC_EGENERIC;
    }

    free(path);

    int val = json_parse(obj->logger, input, &sys->json);

    kill(pid, SIGTERM);
    fclose(input);
    vlc_waitpid(pid);

    if (val) {
        /* Location not handled */
        msg_Dbg(s, "cannot extract infos");
        return VLC_EGENERIC;
    }

    s->p_sys = sys;
    s->pf_readdir = ReadDir;
    s->pf_control = Control;
    return VLC_SUCCESS;
}

static int OpenFilter(vlc_object_t *obj)
{
    stream_t *s = (stream_t *)obj;

    if (s->psz_url == NULL)
        return VLC_EGENERIC;
    if (strncasecmp(s->psz_url, "http:", 5)
     && strncasecmp(s->psz_url, "https:", 6))
        return VLC_EGENERIC;
    if (!var_InheritBool(obj, "ytdl"))
        return VLC_EGENERIC;

    return OpenCommon(obj);
}

vlc_module_begin()
    set_shortname("YT-DL")
    set_description("YoutubeDL")
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_STREAM_FILTER)
    set_capability("stream_filter", 305)
    set_callbacks(OpenFilter, Close)
    /* TODO: convert to demux and enable by default */
    add_bool("ytdl", false, N_("Enable YT-DL"), N_("Enable YT-DL"), true)
        change_safe()

    add_submodule()
    set_capability("access", 0)
    add_shortcut("ytdl")
    set_callbacks(OpenCommon, Close)
vlc_module_end()
