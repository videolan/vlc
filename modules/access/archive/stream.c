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

static ssize_t NoRead(stream_t *p_stream, void *buf, size_t len)
{
    (void) p_stream; (void) buf; (void) len;
    return -1;
}

static int Control(stream_t *p_stream, int i_query, va_list args)
{
    switch( i_query )
    {
        case STREAM_IS_DIRECTORY:
            *va_arg( args, bool * ) = false;
            break;

        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_GET_SIZE:
        case STREAM_SET_RECORD_STATE:
        case STREAM_GET_CONTENT_TYPE:
            return VLC_EGENERIC;

        default:
            return vlc_stream_vaControl( p_stream->p_source, i_query, args );
    }

    return VLC_SUCCESS;
}

static ssize_t ReadCallback(struct archive *p_archive, void *p_object, const void **pp_buffer)
{
    VLC_UNUSED(p_archive);
    stream_t *p_stream = (stream_t*)p_object;
    stream_sys_t *sys = p_stream->p_sys;

    *pp_buffer = &sys->buffer;
    return vlc_stream_Read(p_stream->p_source, &sys->buffer, ARCHIVE_READ_SIZE);
}

static ssize_t SkipCallback(struct archive *p_archive, void *p_object, ssize_t i_request)
{
    VLC_UNUSED(p_archive);
    stream_t *p_stream = (stream_t*)p_object;
    stream_sys_t *sys = p_stream->p_sys;
    ssize_t i_skipped = 0;

    /* be smart as small seeks converts to reads */
    if (sys->b_source_canseek)
    {
        int64_t i_pos = vlc_stream_Tell(p_stream->p_source);
        if (i_pos >=0)
            vlc_stream_Seek(p_stream->p_source, i_pos + i_request);
        i_skipped = vlc_stream_Tell(p_stream->p_source) - i_pos;
    }
    else while(i_request)
    {
        int i_skip = __MIN(INT32_MAX, i_request);
        int i_read = vlc_stream_Read(p_stream->p_source, NULL, i_skip);
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
        i_pos = vlc_stream_Tell(p_stream->p_source);
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

    vlc_stream_Seek(p_stream->p_source, i_pos + i_offset); /* We don't care about return val */
    return vlc_stream_Tell(p_stream->p_source);
}

static int Browse(stream_t *p_stream, input_item_node_t *p_node)
{
    stream_sys_t *p_sys = p_stream->p_sys;
    struct archive_entry *p_entry;

    while(archive_read_next_header(p_sys->p_archive, &p_entry) == ARCHIVE_OK)
    {
        char *psz_uri = NULL;
        char *psz_access_uri = NULL;
        int i_ret = asprintf(&psz_access_uri, "%s%c%s", p_stream->psz_url,
                             ARCHIVE_SEP_CHAR, archive_entry_pathname(p_entry));
        if (i_ret == -1)
            goto error;
        i_ret = asprintf(&psz_uri, "archive://%s", psz_access_uri);
        free(psz_access_uri);
        if(i_ret == -1)
            goto error;

        input_item_t *p_item = input_item_New(psz_uri, archive_entry_pathname(p_entry));
        free( psz_uri );
        if (p_item == NULL)
            goto error;

        input_item_CopyOptions(p_node->p_item, p_item);
        input_item_node_AppendItem(p_node, p_item);
        msg_Dbg(p_stream, "declaring playlist entry %s", archive_entry_pathname(p_entry));
        input_item_Release(p_item);
    }

    return VLC_SUCCESS;

error:
    return VLC_ENOMEM;
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
    vlc_stream_Control(p_stream->p_source, STREAM_CAN_SEEK,
                       &p_sys->b_source_canseek);
    if(p_sys->b_source_canseek)
        archive_read_set_seek_callback(p_sys->p_archive, SeekCallback);

    if (archive_read_open2(p_sys->p_archive, p_stream, NULL, ReadCallback, SkipCallback, NULL) != ARCHIVE_OK)
    {
        msg_Err(p_stream, "can't open archive: %s",
                archive_error_string(p_sys->p_archive));
        StreamClose(p_object);
        return VLC_EGENERIC;
    }

    p_stream->pf_read = NoRead;
    p_stream->pf_seek = NULL;
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
