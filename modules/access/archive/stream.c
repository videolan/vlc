/*****************************************************************************
 * stream.c: libarchive based stream filter
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

#include <vlc_stream.h>
#include <vlc_url.h>
#include <vlc_input_item.h>

#include <archive.h>
#include <archive_entry.h>

struct stream_sys_t
{
    struct archive *p_archive;
    bool b_source_canseek;
    uint8_t buffer[ARCHIVE_READ_SIZE];
};

static int Control(stream_t *p_stream, int i_query, va_list args)
{
    switch( i_query )
    {
        case STREAM_IS_DIRECTORY:
        {
            bool *pb_canreaddir = va_arg( args, bool * );
            bool *pb_dirsorted = va_arg( args, bool * );
            bool *pb_dircanloop = va_arg( args, bool * );
            *pb_canreaddir = true;
            if (pb_dirsorted)
                *pb_dirsorted = false;
            if (pb_dircanloop)
                pb_dircanloop = false;
            break;
        }

        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_GET_SIZE:
        case STREAM_GET_POSITION:
        case STREAM_SET_POSITION:
        case STREAM_SET_RECORD_STATE:
        case STREAM_GET_CONTENT_TYPE:
            return VLC_EGENERIC;

        default:
            return stream_vaControl( p_stream->p_source, i_query, args );
    }

    return VLC_SUCCESS;
}

static ssize_t ReadCallback(struct archive *p_archive, void *p_object, const void **pp_buffer)
{
    VLC_UNUSED(p_archive);
    stream_t *p_stream = (stream_t*)p_object;

    *pp_buffer = &p_stream->p_sys->buffer;
    return stream_Read(p_stream->p_source, &p_stream->p_sys->buffer, ARCHIVE_READ_SIZE);
}

static ssize_t SkipCallback(struct archive *p_archive, void *p_object, ssize_t i_request)
{
    VLC_UNUSED(p_archive);
    stream_t *p_stream = (stream_t*)p_object;
    ssize_t i_skipped = 0;

    /* be smart as small seeks converts to reads */
    if (p_stream->p_sys->b_source_canseek)
    {
        int64_t i_pos = stream_Tell(p_stream->p_source);
        if (i_pos >=0)
            stream_Seek(p_stream->p_source, i_pos + i_request);
        i_skipped = stream_Tell(p_stream->p_source) - i_pos;
    }
    else while(i_request)
    {
        int i_skip = __MIN(INT32_MAX, i_request);
        int i_read = stream_Read(p_stream->p_source, NULL, i_skip);
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
    stream_t *p_stream = (stream_t*)p_object;
    ssize_t i_pos;

    switch(i_whence)
    {
    case SEEK_CUR:
        i_pos = stream_Tell(p_stream->p_source);
        break;
    case SEEK_SET:
        i_pos = 0;
        break;
    case SEEK_END:
        i_pos = stream_Size(p_stream->p_source) - 1;
        break;
    default:
        return -1;
    }

    if (i_pos < 0)
        return -1;

    stream_Seek(p_stream->p_source, i_pos + i_offset); /* We don't care about return val */
    return stream_Tell(p_stream->p_source);
}

static input_item_t *Browse(stream_t *p_stream)
{
    stream_sys_t *p_sys = p_stream->p_sys;
    struct archive_entry *p_entry;
    input_item_t *p_item = NULL;

    if (archive_read_next_header(p_sys->p_archive, &p_entry) == ARCHIVE_OK)
    {
        char *psz_uri = NULL;
        char *psz_access_uri = NULL;
        int i_ret = asprintf(&psz_access_uri, "%s%c%s", p_stream->psz_url,
                             ARCHIVE_SEP_CHAR, archive_entry_pathname(p_entry));
        if (i_ret == -1)
            return NULL;
        i_ret = asprintf(&psz_uri, "archive://%s", psz_access_uri);
        free(psz_access_uri);
        if( i_ret == -1 )
            return NULL;

        p_item = input_item_New(psz_uri, archive_entry_pathname(p_entry));
        free( psz_uri );
        if(p_item == NULL)
            return NULL;

        msg_Dbg(p_stream, "declaring playlist entry %s", archive_entry_pathname(p_entry));
    }

    return p_item;
}

int StreamOpen(vlc_object_t *p_object)
{
    stream_t *p_stream = (stream_t*) p_object;
    stream_sys_t *p_sys;

    if (!ProbeArchiveFormat(p_stream->p_source))
        return VLC_EGENERIC;
    if (p_stream->psz_url == NULL)
        return VLC_EGENERIC;

    p_stream->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->p_archive = archive_read_new();
    if (!p_sys->p_archive)
    {
        msg_Err(p_stream, "can't create libarchive instance: %s",
                archive_error_string(p_sys->p_archive));
        StreamClose(p_object);
        return VLC_EGENERIC;
    }

    EnableArchiveFormats(p_sys->p_archive);

    /* Seek callback must only be set if calls are guaranteed to succeed */
    stream_Control(p_stream->p_source, STREAM_CAN_SEEK, &p_sys->b_source_canseek);
    if(p_sys->b_source_canseek)
        archive_read_set_seek_callback(p_sys->p_archive, SeekCallback);

    if (archive_read_open2(p_sys->p_archive, p_stream, NULL, ReadCallback, SkipCallback, NULL) != ARCHIVE_OK)
    {
        msg_Err(p_stream, "can't open archive: %s",
                archive_error_string(p_sys->p_archive));
        StreamClose(p_object);
        return VLC_EGENERIC;
    }

    p_stream->pf_read = NULL;
    p_stream->pf_control = Control;
    p_stream->pf_readdir = Browse;

    return VLC_SUCCESS;
}

void StreamClose(vlc_object_t *object)
{
    stream_t *p_stream = (stream_t*)object;
    stream_sys_t *p_sys = p_stream->p_sys;

    if (p_sys->p_archive)
    {
        archive_read_close(p_sys->p_archive);
        archive_read_free(p_sys->p_archive);
    }

    free(p_sys);
}
