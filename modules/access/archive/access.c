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

struct access_sys_t
{
    struct archive *p_archive;
    bool b_source_canseek;
    uint8_t buffer[ARCHIVE_READ_SIZE];

    struct archive_entry *p_entry;
    stream_t *p_stream;
    char *psz_uri;
    bool b_seekable; /* Is our archive type seekable ? */
};

static ssize_t Read(access_t *p_access, uint8_t *p_data, size_t i_size)
{
    access_sys_t *p_sys = p_access->p_sys;

    size_t i_read = 0;

    i_read = archive_read_data(p_sys->p_archive, p_data, i_size);

    if (i_read > 0)
        p_access->info.i_pos += i_read;

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

static ssize_t ReadCallback(struct archive *p_archive, void *p_object, const void **pp_buffer)
{
    VLC_UNUSED(p_archive);
    access_t *p_access = (access_t*)p_object;
    access_sys_t *p_sys = p_access->p_sys;

    *pp_buffer = &p_sys->buffer;
    return stream_Read(p_sys->p_stream, &p_sys->buffer, ARCHIVE_READ_SIZE);
}

static ssize_t SkipCallback(struct archive *p_archive, void *p_object, ssize_t i_request)
{
    VLC_UNUSED(p_archive);
    access_t *p_access = (access_t*)p_object;
    access_sys_t *p_sys = p_access->p_sys;
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
    access_t *p_access = (access_t*)p_object;
    access_sys_t *p_sys = p_access->p_sys;

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

static int OpenCallback(struct archive *p_archive, void *p_object)
{
    VLC_UNUSED(p_archive);
    access_t *p_access = (access_t*)p_object;
    access_sys_t *p_sys = p_access->p_sys;

    p_sys->p_stream = stream_UrlNew( p_access, p_sys->psz_uri );
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
    access_t *p_access = (access_t*)p_object;

    if (p_access->p_sys->p_stream)
        stream_Delete(p_access->p_sys->p_stream);

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

    if (!strchr(p_access->psz_location, ARCHIVE_SEP_CHAR))
        return VLC_EGENERIC;

    char *psz_base = strdup(p_access->psz_location);
    if (!psz_base)
        return VLC_EGENERIC;
    char *psz_name = strchr(psz_base, ARCHIVE_SEP_CHAR);
    *psz_name++ = '\0';

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

    p_sys->psz_uri = psz_base;

    if (archive_read_open2(p_sys->p_archive, p_access, OpenCallback, ReadCallback, SkipCallback, CloseCallback) != ARCHIVE_OK)
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

    free(p_sys->psz_uri);
    free(p_sys);
}
