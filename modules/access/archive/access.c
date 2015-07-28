/*****************************************************************************
 * access.c: libarchive based access
 *****************************************************************************
 * Copyright (C) 2014 Videolan Team
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

#include "archive.h"

#include <vlc_access.h>
#include <vlc_url.h>
#include <vlc_stream.h>

#include <archive.h>
#include <archive_entry.h>

typedef struct callback_data_t
{
    char *psz_uri;
    access_t *p_access;
} callback_data_t;

struct access_sys_t
{
    struct archive *p_archive;
    bool b_source_canseek;
    uint8_t buffer[ARCHIVE_READ_SIZE];

    callback_data_t *p_callback_data;
    unsigned int i_callback_data;

    struct archive_entry *p_entry;
    stream_t *p_stream;
    bool b_seekable; /* Is our archive type seekable ? */
};

static ssize_t Read(access_t *p_access, uint8_t *p_data, size_t i_size)
{
    access_sys_t *p_sys = p_access->p_sys;

    ssize_t i_read = 0;

    i_read = archive_read_data(p_sys->p_archive, p_data, i_size);

    if (i_read > 0)
        p_access->info.i_pos += i_read;
    else
        i_read = 0;

    if (i_size > 0 && i_read <= 0)
        p_access->info.b_eof = true;

    return i_read;
}

static int Seek(access_t *p_access, uint64_t i_pos)
{
    access_sys_t *p_sys = p_access->p_sys;

    if (!p_sys->b_seekable)
        return VLC_EGENERIC;

    int64_t i_ret = archive_seek_data(p_sys->p_archive, i_pos, SEEK_SET);
    if ( i_ret < ARCHIVE_OK )
        return VLC_EGENERIC;
    p_access->info.i_pos = i_ret;

    return VLC_SUCCESS;
}

static int FindVolumes(access_t *p_access, struct archive *p_archive, const char *psz_uri,
                       char *** const pppsz_files, unsigned int * const pi_files)
{
    VLC_UNUSED(p_archive);
    *pppsz_files = NULL;
    *pi_files = 0;

    static const struct
    {
        char const * const psz_match;
        char const * const psz_format;
        uint16_t i_min;
        uint16_t i_max;
    } patterns[] = {
        { ".part1.rar",   "%.*s.part%.1d.rar", 2,   9 },
        { ".part01.rar",  "%.*s.part%.2d.rar", 2,  99 },
        { ".part001.rar", "%.*s.part%.3d.rar", 2, 999 },
        { ".001",         "%.*s.%.3d",         2, 999 },
        { ".000",         "%.*s.%.3d",         1, 999 },
    };

    const int i_uri_size = strlen(psz_uri);
    const size_t i_patterns = ARRAY_SIZE(patterns);

    for (size_t i = 0; i < i_patterns; i++)
    {
        const int i_match_size = strlen(patterns[i].psz_match);
        if (i_uri_size < i_match_size)
            continue;

        if (!strcmp(&psz_uri[i_uri_size - i_match_size], patterns[i].psz_match))
        {
            char **ppsz_files = xmalloc(sizeof(char *) * patterns[i].i_max);

            for (unsigned j = patterns[i].i_min; j < patterns[i].i_max; j++)
            {
                char *psz_newuri;

                if (asprintf(&psz_newuri, patterns[i].psz_format,
                             i_uri_size - i_match_size, psz_uri, j) == -1)
                    break;

                /* Probe URI */
                int i_savedflags = p_access->i_flags;
                p_access->i_flags |= OBJECT_FLAGS_NOINTERACT;
                stream_t *p_stream = stream_UrlNew(p_access, psz_newuri);
                p_access->i_flags = i_savedflags;
                if (p_stream)
                {
                    ppsz_files[*pi_files] = psz_newuri;
                    (*pi_files)++;
                    stream_Delete(p_stream);
                }
                else
                {
                    free(psz_newuri);
                    break;
                }
            }

            if (*pi_files == 0)
                FREENULL(ppsz_files);
            *pppsz_files = ppsz_files;

            return VLC_SUCCESS;
        }
    }

    return VLC_SUCCESS;
}

static ssize_t ReadCallback(struct archive *p_archive, void *p_object, const void **pp_buffer)
{
    VLC_UNUSED(p_archive);
    callback_data_t *p_data = (callback_data_t *) p_object;
    access_sys_t *p_sys = p_data->p_access->p_sys;

    *pp_buffer = &p_sys->buffer;
    return stream_Read(p_sys->p_stream, &p_sys->buffer, ARCHIVE_READ_SIZE);
}

static ssize_t SkipCallback(struct archive *p_archive, void *p_object, ssize_t i_request)
{
    VLC_UNUSED(p_archive);
    callback_data_t *p_data = (callback_data_t *) p_object;
    access_sys_t *p_sys = p_data->p_access->p_sys;
    ssize_t i_skipped = 0;

    /* be smart as small seeks converts to reads */
    if (p_sys->b_source_canseek)
    {
        int64_t i_pos = stream_Tell(p_sys->p_stream);
        if (i_pos >=0)
            stream_Seek(p_sys->p_stream, i_pos + i_request);
        i_skipped = stream_Tell(p_sys->p_stream) - i_pos;
    }
    else while(i_request)
    {
        int i_skip = __MIN(INT32_MAX, i_request);
        int i_read = stream_Read(p_sys->p_stream, NULL, i_skip);
        if (i_read > 0)
            i_skipped += i_read;
        else
            break;
        i_request -= i_read;
    }

    return i_skipped;
}

static ssize_t SeekCallback(struct archive *p_archive, void *p_object, ssize_t i_offset, int i_whence)
{
    VLC_UNUSED(p_archive);
    callback_data_t *p_data = (callback_data_t *) p_object;
    access_sys_t *p_sys = p_data->p_access->p_sys;

    ssize_t i_pos;

    switch(i_whence)
    {
    case SEEK_CUR:
        i_pos = stream_Tell(p_sys->p_stream);
        break;
    case SEEK_SET:
        i_pos = 0;
        break;
    case SEEK_END:
        i_pos = stream_Size(p_sys->p_stream) - 1;
        break;
    default:
        return -1;
    }

    if (i_pos < 0)
        return -1;

    stream_Seek(p_sys->p_stream, i_pos + i_offset); /* We don't care about return val */
    return stream_Tell(p_sys->p_stream);
}

static int SwitchCallback(struct archive *p_archive, void *p_object, void *p_object2)
{
    VLC_UNUSED(p_archive);
    callback_data_t *p_data = (callback_data_t *) p_object;
    callback_data_t *p_nextdata = (callback_data_t *) p_object2;
    access_sys_t *p_sys = p_data->p_access->p_sys;

    msg_Dbg(p_data->p_access, "opening next volume %s", p_nextdata->psz_uri);
    stream_Delete(p_sys->p_stream);
    p_sys->p_stream = stream_UrlNew(p_nextdata->p_access, p_nextdata->psz_uri);
    return p_sys->p_stream ? ARCHIVE_OK : ARCHIVE_FATAL;
}

static int OpenCallback(struct archive *p_archive, void *p_object)
{
    VLC_UNUSED(p_archive);
    callback_data_t *p_data = (callback_data_t *) p_object;
    access_sys_t *p_sys = p_data->p_access->p_sys;

    p_sys->p_stream = stream_UrlNew( p_data->p_access, p_data->psz_uri );
    if(!p_sys->p_stream)
        return ARCHIVE_FATAL;

    /* Seek callback must only be set if calls are guaranteed to succeed */
    stream_Control(p_sys->p_stream, STREAM_CAN_SEEK, &p_sys->b_source_canseek);
    if(p_sys->b_source_canseek)
        archive_read_set_seek_callback(p_sys->p_archive, SeekCallback);

    return ARCHIVE_OK;
}

static int CloseCallback(struct archive *p_archive, void *p_object)
{
    VLC_UNUSED(p_archive);
    callback_data_t *p_data = (callback_data_t *) p_object;
    access_sys_t *p_sys = p_data->p_access->p_sys;

    if (p_sys->p_stream)
    {
        stream_Delete(p_sys->p_stream);
        p_sys->p_stream = NULL;
    }

    return ARCHIVE_OK;
}

static int Control(access_t *p_access, int i_query, va_list args)
{
    access_sys_t *p_sys = p_access->p_sys;

    switch (i_query)
    {

    case ACCESS_CAN_SEEK:
        *va_arg(args, bool *)= p_sys->b_seekable;
        break;

    case ACCESS_CAN_FASTSEEK:
        if (!p_sys->b_seekable || !p_sys->p_stream)
        {
            *va_arg( args, bool* ) = false;
            break;
        }
        else
            return stream_vaControl( p_sys->p_stream, i_query, args );

    case ACCESS_SET_PAUSE_STATE:
        break;

    case ACCESS_CAN_PAUSE:
    case ACCESS_CAN_CONTROL_PACE:
        *va_arg(args, bool *) = true;
        break;

    case ACCESS_GET_SIZE:
        *va_arg(args, uint64_t *) = archive_entry_size(p_sys->p_entry);
        break;

    case ACCESS_GET_PTS_DELAY:
        *va_arg(args, int64_t *) = DEFAULT_PTS_DELAY;
        break;

    default:
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

int AccessOpen(vlc_object_t *p_object)
{
    access_t *p_access = (access_t*)p_object;
    const char *sep = strchr(p_access->psz_location, ARCHIVE_SEP_CHAR);
    if (sep == NULL)
        return VLC_EGENERIC;

    char *psz_base = strdup(p_access->psz_location);
    if (unlikely(psz_base == NULL))
        return VLC_ENOMEM;

    char *psz_name = psz_base + (sep - p_access->psz_location);
    *(psz_name++) = '\0';

    if (decode_URI(psz_base) == NULL)
    {
        free(psz_base);
        return VLC_EGENERIC;
    }

    access_sys_t *p_sys = p_access->p_sys = calloc(1, sizeof(access_sys_t));
    p_sys->p_archive = archive_read_new();
    if (!p_sys->p_archive)
    {
        msg_Err(p_access, "can't create libarchive instance: %s",
                archive_error_string(p_sys->p_archive));
        free(psz_base);
        goto error;
    }

    EnableArchiveFormats(p_sys->p_archive);

    /* Set up the switch callback for multiple volumes handling */
    archive_read_set_switch_callback(p_sys->p_archive, SwitchCallback);

    /* !Warn: sucks because libarchive can't guess format without reading 1st header
     *        and it can't tell either if volumes are missing neither set following
     *        volumes after the first Open().
     *        We need to know volumes uri in advance then :/
     */

    /* Try to list existing volumes */
    char **ppsz_files = NULL;
    unsigned int i_files = 0;
    FindVolumes(p_access, p_sys->p_archive, psz_base, &ppsz_files, &i_files);

    p_sys->i_callback_data = 1 + i_files;
    p_sys->p_callback_data = malloc(sizeof(callback_data_t) * p_sys->i_callback_data);
    if (!p_sys->p_callback_data)
    {
        for(unsigned int i=0; i<i_files; i++)
            free(ppsz_files[i]);
        free(ppsz_files);
        free(psz_base);
        AccessClose(p_object);
        return VLC_ENOMEM;
    }

    /* set up our callback struct for our main uri */
    p_sys->p_callback_data[0].psz_uri = psz_base;
    p_sys->p_callback_data[0].p_access = p_access;
    archive_read_append_callback_data(p_sys->p_archive, &p_sys->p_callback_data[0]);

    /* and register other volumes */
    for(unsigned int i=0; i<i_files; i++)
    {
        p_sys->p_callback_data[1+i].psz_uri = ppsz_files[i];
        p_sys->p_callback_data[1+i].p_access = p_access;
        archive_read_append_callback_data(p_sys->p_archive, &p_sys->p_callback_data[1+i]);
    }
    free(ppsz_files);

    if (archive_read_open2(p_sys->p_archive, &p_sys->p_callback_data[0],
                           OpenCallback, ReadCallback, SkipCallback, CloseCallback) != ARCHIVE_OK)
    {
        msg_Err(p_access, "can't open archive: %s",
                archive_error_string(p_sys->p_archive));
        AccessClose(p_object);
        return VLC_EGENERIC;
    }

    bool b_present = false;
    while(archive_read_next_header(p_sys->p_archive, &p_sys->p_entry) == ARCHIVE_OK)
    {
        if (!strcmp(archive_entry_pathname(p_sys->p_entry), psz_name))
        {
            b_present = true;
            break;
        }
        msg_Dbg(p_access, "skipping entry %s != %s", archive_entry_pathname(p_sys->p_entry), psz_name);
    }

    if (!b_present)
    {
        msg_Err(p_access, "entry '%s' not found in archive", psz_name);
        /* entry not found */
        goto error;
    }

    msg_Dbg(p_access, "reading entry %s %"PRId64, archive_entry_pathname(p_sys->p_entry),
                                                  archive_entry_size(p_sys->p_entry));

    /* try to guess if it is seekable or not (does not depend on backend) */
    p_sys->b_seekable = (archive_seek_data(p_sys->p_archive, 0, SEEK_SET) >= 0);

    p_access->pf_read    = Read;
    p_access->pf_block   = NULL; /* libarchive's zerocopy keeps owning block :/ */
    p_access->pf_control = Control;
    p_access->pf_seek    = Seek;

    access_InitFields(p_access);

    return VLC_SUCCESS;

error:
    AccessClose(p_object);
    return VLC_EGENERIC;
}

void AccessClose(vlc_object_t *p_object)
{
    access_t *p_access = (access_t*)p_object;
    access_sys_t *p_sys = p_access->p_sys;

    if (p_sys->p_archive)
    {
        archive_read_close(p_sys->p_archive);
        archive_read_free(p_sys->p_archive);
    }

    if (p_sys->p_callback_data)
    {
        for(unsigned int i=0; i<p_sys->i_callback_data; i++)
            free(p_sys->p_callback_data[i].psz_uri);
        free(p_sys->p_callback_data);
    }

    free(p_sys);
}
