/*****************************************************************************
 * json.c: JSON tracer plugin
 *****************************************************************************
 * Copyright © 2021 Videolabs
 *
 * Authors : Nicolas Le Quec
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fs.h>
#include <vlc_charset.h>
#include <vlc_tracer.h>
#include <vlc_memstream.h>

#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>

#define JSON_FILENAME "vlc-log.json"

#define TIME_FROM_TICK(ts) NS_FROM_VLC_TICK(ts)

typedef struct
{
    FILE *stream;
} vlc_tracer_sys_t;

static void PrintUTF8Char(FILE *stream, uint32_t character)
{
    /* If the character is in the Basic Multilingual Plane (U+0000 through U+FFFF),
       then it may be represented as a six-character sequence: \uxxxx */
    if (character < 0x10000)
    {
        fprintf(stream, "\\u%04x", character);
    }
    /* To escape an extended character that is not in the Basic Multilingual
      Plane, the character is represented as a 12-character sequence, encoding
      the UTF-16 surrogate pair. */

    else if (0x10000 <= character && character <= 0x10FFFF) {
        unsigned int code;
        uint16_t units[2];

        code = (character - 0x10000);
        units[0] = 0xD800 | (code >> 10);
        units[1] = 0xDC00 | (code & 0x3FF);

        fprintf(stream, "\\u%04x\\u%04x", units[0], units[1]);
    }
}

static void JsonPrintString(FILE *stream, const char *str)
{
    if (!IsUTF8(str))
    {
        fputs("\"invalid string\"", stream);
        return;
    }

    fputc('\"', stream);

    unsigned char byte;
    while (*str != '\0')
    {
        switch (*str)
        {
        case '/':
            fputs("\\/", stream);
            break;
        case '\b':
            fputs("\\b", stream);
            break;
        case '\f':
            fputs("\\f", stream);
            break;
        case '\n':
            fputs("\\n", stream);
            break;
        case '\r':
            fputs("\\r", stream);
            break;
        case '\t':
            fputs("\\t", stream);
            break;
        case '\\':
        case '\"':
            fprintf(stream, "\\%c", *str);
            break;
        default:
            byte = *str;
            if (byte <= 0x1F || byte == 0x7F)
            {
                fprintf(stream, "\\u%04x", byte);
            }
            else if (byte < 0x80)
            {
                fputc(byte, stream);
            }
            else
            {
                uint32_t bytes;
                ssize_t len = vlc_towc(str, &bytes);
                assert(len > 0);
                PrintUTF8Char(stream, bytes);
                str += len - 1;
            }
        }
        str++;
    }
    fputc('\"', stream);
}

static void JsonPrintKeyValueNumber(FILE *stream, const char *key, int64_t value)
{
    JsonPrintString(stream, key);
    fprintf(stream, ": \"%"PRId64"\"", value);
}

static void JsonPrintKeyValueLabel(FILE *stream, const char *key, const char *value)
{
    JsonPrintString(stream, key);
    fputs(": ", stream);
    JsonPrintString(stream, value);
}

static void JsonStartObjectSection(FILE *stream, const char* name)
{
    if (name != NULL)
        fprintf(stream, "\"%s\": {", name);
    else
        fputc('{', stream);
}

static void JsonEndObjectSection(FILE *stream)
{
    fputc('}', stream);
}

static void TraceJson(void *opaque, vlc_tick_t ts, va_list entries)
{
    vlc_tracer_sys_t *sys = opaque;
    FILE* stream = sys->stream;

    flockfile(stream);
    JsonStartObjectSection(stream, NULL);
    JsonPrintKeyValueNumber(stream, "Timestamp", TIME_FROM_TICK(ts));
    fputc(',', stream);

    JsonStartObjectSection(stream, "Body");

    struct vlc_tracer_entry entry = va_arg(entries, struct vlc_tracer_entry);
    while (entry.key != NULL)
    {
        switch (entry.type)
        {
            case VLC_TRACER_INT:
                JsonPrintKeyValueNumber(stream, entry.key, entry.value.integer);
                break;
            case VLC_TRACER_TICK:
                JsonPrintKeyValueNumber(stream, entry.key,
                                        TIME_FROM_TICK(entry.value.tick));
                break;
            case VLC_TRACER_STRING:
                JsonPrintKeyValueLabel(stream, entry.key, entry.value.string);
                break;
            default:
                vlc_assert_unreachable();
                break;
        }
        entry = va_arg(entries, struct vlc_tracer_entry);
        if (entry.key != NULL)
        {
            fputc(',', stream);
        }
    }
    JsonEndObjectSection(stream);
    JsonEndObjectSection(stream);
    fputc('\n', stream);
    funlockfile(stream);
}

static void Close(void *opaque)
{
    vlc_tracer_sys_t *sys = opaque;

    free(sys);
}

static const struct vlc_tracer_operations json_ops =
{
    TraceJson,
    Close
};

static const struct vlc_tracer_operations *Open(vlc_object_t *obj,
                                               void **restrict sysp)
{
    vlc_tracer_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return NULL;

    const struct vlc_tracer_operations *ops = &json_ops;

    const char *filename = JSON_FILENAME;

    char *path = var_InheritString(obj, "json-tracer-file");
#ifdef __APPLE__
    if (path == NULL)
    {
        char *home = config_GetUserDir(VLC_HOME_DIR);
        if (home != NULL)
        {
            if (asprintf(&path, "%s/Library/Logs/"JSON_FILENAME, home) == -1)
                path = NULL;
            free(home);
        }
    }
#endif
    if (path != NULL)
        filename = path;

    /* Open the log file and remove any buffering for the stream */
    msg_Dbg(obj, "opening logfile `%s'", filename);
    sys->stream = vlc_fopen(filename, "at");
    if (sys->stream == NULL)
    {
        msg_Err(obj, "error opening log file `%s': %s", filename,
            vlc_strerror_c(errno) );
        free(path);
        free(sys);
        return NULL;
    }
    free(path);

    setvbuf(sys->stream, NULL, _IOLBF, 0);

    *sysp = sys;
    return ops;
}

#define FILE_LOG_TEXT N_("Log to file")
#define FILE_LOG_LONGTEXT N_("Log all VLC traces to a json file.")

#define LOGFILE_NAME_TEXT N_("Log filename")
#define LOGFILE_NAME_LONGTEXT N_("Specify the log filename.")

vlc_module_begin()
    set_shortname(N_("Tracer"))
    set_description(N_("JSON tracer"))
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_capability("tracer", 0)
    set_callback(Open)

    add_savefile("json-tracer-file", NULL, LOGFILE_NAME_TEXT, LOGFILE_NAME_LONGTEXT)
vlc_module_end()
