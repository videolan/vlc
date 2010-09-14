/*****************************************************************************
 * httplive.c: HTTP Live Streaming stream filter
 *****************************************************************************
 * Copyright (C) 2010 M2X BV
 * $Id$
 *
 * Author: Jean-Paul Saman <jpsaman _AT_ videolan _DOT_ org>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <assert.h>

#include <vlc_threads.h>
#include <vlc_arrays.h>
#include <vlc_stream.h>
#include <vlc_input.h>
#include <vlc_fs.h>
#include <vlc_network.h>
#include <vlc_url.h>

#include <vlc_modules.h>
#include <vlc_access.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_STREAM_FILTER)
    set_description(N_("Http Live Streaming stream filter"))
    set_capability("stream_filter", 20)
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 *
 *****************************************************************************/
typedef struct segment_s
{
    int         sequence;   /* unique sequence number */
    int         length;     /* segment duration (ms) */
    uint64_t    size;       /* segment size in bytes */

    vlc_url_t   url;
    vlc_mutex_t lock;
    block_t     *data;      /* data */
} segment_t;

typedef struct hls_stream_s
{
    int         id;         /* program id */
    uint64_t    bandwidth;  /* bandwidth usage of segments (kbps)*/
    int         version;    /* protocol version should be 1 */
    int         sequence;   /* media sequence number */
    int         duration;   /* maximum duration per segment (ms) */
    int         segment;    /* current segment downloading */
    vlc_array_t *segments;  /* list of segments */

    vlc_mutex_t lock;
    vlc_url_t   url;        /* uri to m3u8 */
    bool        b_cache;    /* allow caching */
} hls_stream_t;

typedef struct
{
    VLC_COMMON_MEMBERS

    /* */
    vlc_array_t *hls_stream;/* bandwidth adaptation */
    int         current;    /* current hls_stream  */

    stream_t    *s;
} hls_thread_t;

struct stream_sys_t
{
    access_t    *p_access;  /* HTTP access input */

    /* */
    hls_thread_t *thread;
    vlc_array_t *hls_stream;/* bandwidth adaptation */

    /* Playback */
    uint64_t    offset;     /* current offset in media */
    int         current;    /* current hls_stream  */
    int         segment;    /* current segment for playback */

    /* Playlist */
    mtime_t     last;       /* playlist last loaded */
    mtime_t     wakeup;     /* next reload time */
    int         tries;      /* times it was not changed */

    /* state */
    bool        b_cache;    /* can cache files */
    bool        b_meta;     /* meta playlist */
    bool        b_live;     /* live stream? or vod? */
    bool        b_error;    /* parsing error */
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  Read   (stream_t *, void *p_read, unsigned int i_read);
static int  Peek   (stream_t *, const uint8_t **pp_peek, unsigned int i_peek);
static int  Control(stream_t *, int i_query, va_list);

static int  AccessOpen(stream_t *s, vlc_url_t *url);
static void AccessClose(stream_t *s);
static char *AccessReadLine(access_t *p_access, uint8_t *psz_tmp, size_t i_len);
static int AccessDownload(stream_t *s, segment_t *segment);

static void* hls_Thread(vlc_object_t *);
static int get_HTTPLivePlaylist(stream_t *s, hls_stream_t *hls);

static void segment_Free(segment_t *segment);

/****************************************************************************
 *
 ****************************************************************************/
static bool isHTTPLiveStreaming(stream_t *s)
{
    const uint8_t *peek, *peek_end;

    int64_t i_size = stream_Peek(s->p_source, &peek, 46);
    if (i_size < 1)
        return false;

    if (strncasecmp((const char*)peek, "#EXTM3U", 7) != 0)
        return false;

    /* Parse stream and search for
     * EXT-X-TARGETDURATION or EXT-X-STREAM-INF tag, see
     * http://tools.ietf.org/html/draft-pantos-http-live-streaming-04#page-8 */
    peek_end = peek + i_size;
    while(peek <= peek_end)
    {
        if (*peek == '#')
        {
            if (strncasecmp((const char*)peek, "#EXT-X-TARGETDURATION", 21) == 0)
                return true;
            else if (strncasecmp((const char*)peek, "#EXT-X-STREAM-INF", 17) == 0)
                return true;
        }
        peek++;
    };

    return false;
}

/* HTTP Live Streaming */
static hls_stream_t *hls_New(vlc_array_t *hls_stream, int id, uint64_t bw, char *uri)
{
    hls_stream_t *hls = (hls_stream_t *)malloc(sizeof(hls_stream_t));
    if (hls == NULL) return NULL;

    hls->id = id;
    hls->bandwidth = bw;
    hls->sequence = 0; /* default is 0 */
    hls->version = 1;  /* default protocol version */
    hls->segment = 0;
    hls->b_cache = true;
    vlc_UrlParse(&hls->url, uri, 0);
    hls->segments = vlc_array_new();
    vlc_array_append(hls_stream, hls);
    vlc_mutex_init(&hls->lock);
    return hls;
}

static void hls_Free(hls_stream_t *hls)
{
    vlc_mutex_destroy(&hls->lock);

    if (hls->segments)
    {
        for (int n = 0; n < vlc_array_count(hls->segments); n++)
        {
            segment_t *segment = (segment_t *)vlc_array_item_at_index(hls->segments, n);
            if (segment) segment_Free(segment);
        }
        vlc_array_destroy(hls->segments);
    }

    vlc_UrlClean(&hls->url);
    free(hls);
    hls = NULL;
}

static hls_stream_t *hls_Get(vlc_array_t *hls_stream, int wanted)
{
    int count = vlc_array_count(hls_stream);
    if (count <= 0)
        return NULL;
    if ((wanted < 0) || (wanted >= count))
        return NULL;
    return (hls_stream_t *) vlc_array_item_at_index(hls_stream, wanted);
}

static inline hls_stream_t *hls_GetFirst(vlc_array_t *hls_stream)
{
    return (hls_stream_t*) hls_Get(hls_stream, 0);
}

static hls_stream_t *hls_GetLast(vlc_array_t *hls_stream)
{
    int count = vlc_array_count(hls_stream);
    if (count <= 0)
        return NULL;
    count--;
    return (hls_stream_t *) hls_Get(hls_stream, count);
}

/* Segment */
static segment_t *segment_New(hls_stream_t* hls, int duration, char *uri)
{
    segment_t *segment = (segment_t *)malloc(sizeof(segment_t));
    if (segment == NULL)
        return NULL;

    segment->length = duration; /* seconds */
    segment->size = 0; /* bytes */
    segment->sequence = 0;
    vlc_UrlParse(&segment->url, uri, 0);
    segment->data = NULL;
    vlc_array_append(hls->segments, segment);
    vlc_mutex_init(&segment->lock);
    return segment;
}

static void segment_Free(segment_t *segment)
{
    vlc_mutex_destroy(&segment->lock);

    vlc_UrlClean(&segment->url);
    if (segment->data)
        block_Release(segment->data);
    free(segment);
    segment = NULL;
}

static segment_t *segment_GetSegment(hls_stream_t *hls, int wanted)
{
    assert(hls);

    int count = vlc_array_count(hls->segments);
    if (count <= 0)
        return NULL;
    if ((wanted < 0) || (wanted >= count))
        return NULL;
    return (segment_t *) vlc_array_item_at_index(hls->segments, wanted);
}

/* Parsing */
static char *parse_Attributes(const char *line, const char *attr)
{
    char *p;
    char *begin = (char *) line;
    char *end = begin + strlen(line);

    /* Find start of attributes */
    if ((p = strchr(begin, ':' )) == NULL)
        return NULL;

    begin = p;
    do
    {
        if (strncasecmp(begin, attr, strlen(attr)) == 0)
        {
            /* <attr>=<value>[,]* */
            p = strchr(begin, ',');
            begin += strlen(attr) + 1;
            if (begin >= end)
                return NULL;
            if (p == NULL) /* last attribute */
                return strndup(begin, end - begin);
            /* copy till ',' */
            return strndup(begin, p - begin);
        }
        begin++;
    } while(begin < end);

    return NULL;
}

static void parse_SegmentInformation(stream_t *s, hls_stream_t *hls, char *p_read, char *uri)
{
    stream_sys_t *p_sys = s->p_sys;

    assert(hls);

    int duration;
    int ret = sscanf(p_read, "#EXTINF:%d,", &duration);
    if (ret != 1)
    {
        msg_Err(s, "expected #EXTINF:<s>,");
        p_sys->b_error = true;
        return;
    }

    vlc_mutex_lock(&hls->lock);
    segment_t *segment = segment_New(hls, duration, uri);
    if (segment)
        segment->sequence = hls->sequence + vlc_array_count(hls->segments);
    if (duration > hls->duration)
    {
        msg_Err(s, "EXTINF:%d duration is larger then EXT-X-TARGETDURATION:%d",
                duration, hls->duration);
    }
    vlc_mutex_unlock(&hls->lock);
}

static void parse_TargetDuration(stream_t *s, char *p_read)
{
    stream_sys_t *p_sys = s->p_sys;

    int duration;
    int ret = sscanf(p_read, "#EXT-X-TARGETDURATION:%d", &duration);
    if (ret != 1)
    {
        msg_Err(s, "expected #EXT-X-TARGETDURATION:<s>");
        p_sys->b_error = true;
        return;
    }

    hls_stream_t *hls = hls_GetLast(p_sys->hls_stream);
    assert(hls);
    hls->duration = duration; /* seconds */
}

static void parse_StreamInformation(stream_t *s, char *p_read, char *uri)
{
    stream_sys_t *p_sys = s->p_sys;

    int id, bw;
    char *attr;

    attr = parse_Attributes(p_read, "PROGRAM-ID");
    if (attr == NULL)
    {
        msg_Err(s, "#EXT-X-STREAM-INF: expected PROGRAM-ID=<value>");
        p_sys->b_error = true;
        return;
    }
    id = atol(attr);
    free(attr);

    attr = parse_Attributes(p_read, "BANDWIDTH");
    if (attr == NULL)
    {
        msg_Err(s, "#EXT-X-STREAM-INF: expected BANDWIDTH=<value>");
        p_sys->b_error = true;
        return;
    }
    bw = atoll(attr);
    free(attr);

    if (bw == 0)
    {
        msg_Err(s, "#EXT-X-STREAM-INF: bandwidth cannot be 0");
        p_sys->b_error = true;
        return;
    }

    msg_Info(s, "bandwidth adaption detected (program-id=%d, bandwidth=%d).", id, bw);

    hls_stream_t *hls = hls_New(p_sys->hls_stream, id, bw, uri);
    if (hls == NULL)
        p_sys->b_error = true;
}

static void parse_MediaSequence(stream_t *s, char *p_read)
{
    stream_sys_t *p_sys = s->p_sys;

    int sequence;
    int ret = sscanf(p_read, "#EXT-X-MEDIA-SEQUENCE:%d", &sequence);
    if (ret != 1)
    {
        msg_Err(s, "expected #EXT-X-MEDIA-SEQUENCE:<s>");
        p_sys->b_error = true;
        return;
    }

    hls_stream_t *hls = hls_GetLast(p_sys->hls_stream);
    assert(hls);
    if (hls->sequence > 0)
        msg_Err(s, "EXT-X-MEDIA-SEQUENCE already present in playlist");

    hls->sequence = sequence;
}

static void parse_Key(stream_t *s, char *p_read)
{
    stream_sys_t *p_sys = s->p_sys;

    /* #EXT-X-KEY:METHOD=<method>[,URI="<URI>"][,IV=<IV>] */
    char *attr;
    attr = parse_Attributes(p_read, "METHOD");
    if (attr == NULL)
    {
        msg_Err(s, "#EXT-X-KEY: expected METHOD=<value>");
        p_sys->b_error = true;
    }
    else if (strncasecmp(attr, "NONE", 4) == 0)
    {
        char *uri = parse_Attributes(p_read, "URI");
        char *iv = parse_Attributes(p_read, "IV");
        if ((iv != NULL) || (uri != NULL))
        {
            msg_Err(s, "#EXT-X-KEY: URI and IV not expected");
            p_sys->b_error = true;

            free(iv);
            free(uri);
        }
    }
    else
    {
        msg_Warn(s, "playback of encrypted HTTP Live media is not supported.");
        p_sys->b_error = true;
    }
    free(attr);
}

static void parse_ProgramDateTime(stream_t *s, char *p_read)
{
    msg_Dbg(s, "tag not supported: #EXT-X-PROGRAM-DATE-TIME %s", p_read);
}

static void parse_AllowCache(stream_t *s, char *p_read)
{
    stream_sys_t *p_sys = s->p_sys;

    char answer[4] = "\0";
    int ret = sscanf(p_read, "#EXT-X-ALLOW-CACHE:%3s", answer);
    if (ret != 1)
    {
        msg_Err(s, "#EXT-X-ALLOW-CACHE, ignoring ...");
        p_sys->b_error = true;
        return;
    }

    hls_stream_t *hls = hls_GetLast(p_sys->hls_stream);

    assert(hls);
    hls->b_cache = (strncmp(answer, "NO", 2) != 0);
}

static void parse_Version(stream_t *s, char *p_read)
{
    stream_sys_t *p_sys = s->p_sys;

    int version;
    int ret = sscanf(p_read, "#EXT-X-VERSION:%d", &version);
    if (ret != 1)
    {
        msg_Err(s, "#EXT-X-VERSION: no protocol version found, should be version 1.");
        p_sys->b_error = true;
        return;
    }

    hls_stream_t *hls = hls_GetLast(p_sys->hls_stream);
    assert(hls);

    /* Check version */
    hls->version = version;
    if (hls->version != 1)
    {
        msg_Err(s, "#EXT-X-VERSION should be version 1 iso %d", version);
        p_sys->b_error = true;
    }
}

static void parse_EndList(stream_t *s)
{
    stream_sys_t *p_sys = s->p_sys;
    p_sys->b_live = false;

    msg_Info(s, "video on demand (vod) mode");
}

static void parse_Discontinuity(stream_t *s, char *p_read)
{
    /* FIXME: Do we need to act on discontinuity ?? */
    msg_Dbg(s, "#EXT-X-DISCONTINUITY %s", p_read);
}

static void parse_M3U8ExtLine(stream_t *s, char *line)
{
    if (*line == '#')
    {
        if (strncmp(line, "#EXT-X-TARGETDURATION", 21) == 0)
            parse_TargetDuration(s, line);
        else if (strncmp(line, "#EXT-X-MEDIA-SEQUENCE", 22) == 0)
            parse_MediaSequence(s, line);
        else if (strncmp(line, "#EXT-X-KEY", 11) == 0)
            parse_Key(s, line);
        else if (strncmp(line, "#EXT-X-PROGRAM-DATE-TIME", 25) == 0)
            parse_ProgramDateTime(s, line);
        else if (strncmp(line, "#EXT-X-ALLOW-CACHE", 17) == 0)
            parse_AllowCache(s, line);
        else if (strncmp(line, "#EXT-X-DISCONTINUITY", 20) == 0)
            parse_Discontinuity(s, line);
        else if (strncmp(line, "#EXT-X-VERSION", 14) == 0)
            parse_Version(s, line);
        else if (strncmp(line, "#EXT-X-ENDLIST", 14) == 0)
            parse_EndList(s);
    }
}

#define HTTPLIVE_MAX_LINE 4096
static int get_HTTPLivePlaylist(stream_t *s, hls_stream_t *hls)
{
    stream_sys_t *p_sys = s->p_sys;

    /* Download new playlist file from server */
    if (AccessOpen(s, &hls->url) != VLC_SUCCESS)
        goto error;

    /* Parse the rest of the reply */
    uint8_t *tmp = calloc(1, HTTPLIVE_MAX_LINE);
    if (tmp == NULL)
    {
        AccessClose(s);
        return VLC_ENOMEM;
    }

    char *line = AccessReadLine(p_sys->p_access, tmp, HTTPLIVE_MAX_LINE);
    if (strncmp(line, "#EXTM3U", 7) != 0)
    {
        msg_Err(s, "missing #EXTM3U tag");
        goto error;
    }
    free(line);
    line = NULL;

    for( ; ; )
    {
        line = AccessReadLine(p_sys->p_access, tmp, HTTPLIVE_MAX_LINE);
        if (line == NULL)
        {
            msg_Dbg(s, "end of data");
            break;
        }

        if (!vlc_object_alive(s))
            goto error;

        /* some more checks for actual data */
        if (strncmp(line, "#EXTINF", 7) == 0)
        {
            char *uri = AccessReadLine(p_sys->p_access, tmp, HTTPLIVE_MAX_LINE);
            if (uri == NULL)
                p_sys->b_error = true;
            else
            {
                parse_SegmentInformation(s, hls, line, uri);
                free(uri);
            }
        }
        else
        {
            parse_M3U8ExtLine(s, line);
        }

        /* Error during m3u8 parsing abort */
        if (p_sys->b_error)
            goto error;

        free(line);
    }

    free(line);
    free(tmp);
    AccessClose(s);
    return VLC_SUCCESS;

error:
    free(line);
    free(tmp);
    AccessClose(s);
    return VLC_EGENERIC;
}
#undef HTTPLIVE_MAX_LINE

/* The http://tools.ietf.org/html/draft-pantos-http-live-streaming-04#page-8
 * document defines the following new tags: EXT-X-TARGETDURATION,
 * EXT-X-MEDIA-SEQUENCE, EXT-X-KEY, EXT-X-PROGRAM-DATE-TIME, EXT-X-
 * ALLOW-CACHE, EXT-X-STREAM-INF, EXT-X-ENDLIST, EXT-X-DISCONTINUITY,
 * and EXT-X-VERSION.
 */
static int parse_HTTPLiveStreaming(stream_t *s)
{
    stream_sys_t *p_sys = s->p_sys;
    char *p_read, *p_begin, *p_end;

    assert(p_sys->hls_stream);

    p_begin = p_read = stream_ReadLine(s->p_source);
    if (!p_begin)
        return VLC_ENOMEM;

    /* */
    int i_len = strlen(p_begin);
    p_end = p_read + i_len;

    if (strncmp(p_read, "#EXTM3U", 7) != 0)
    {
        msg_Err(s, "missing #EXTM3U tag .. aborting");
        free(p_begin);
        return VLC_EGENERIC;
    }

    do {
        free(p_begin);

        if (p_sys->b_error)
            return VLC_EGENERIC;

        /* Next line */
        p_begin = stream_ReadLine(s->p_source);
        if (p_begin == NULL)
            break;

        i_len = strlen(p_begin);
        p_read = p_begin;
        p_end = p_read + i_len;

        if (strncmp(p_read, "#EXT-X-STREAM-INF", 17) == 0)
        {
            p_sys->b_meta = true;
            char *uri = stream_ReadLine(s->p_source);
            if (uri == NULL)
                p_sys->b_error = true;
            else
            {
                parse_StreamInformation(s, p_read, uri);
                free(uri);
            }
        }
        else if (strncmp(p_read, "#EXTINF", 7) == 0)
        {
            char *uri = stream_ReadLine(s->p_source);
            if (uri == NULL)
                p_sys->b_error = true;
            else
            {
                hls_stream_t *hls = hls_GetLast(p_sys->hls_stream);
                if (hls)
                    parse_SegmentInformation(s, hls, p_read, uri);
                else
                    p_sys->b_error = true;
                free(uri);
            }
        }
        else
        {
            if (!p_sys->b_meta)
            {
                hls_stream_t *hls = hls_GetLast(p_sys->hls_stream);
                if (hls == NULL)
                {
                    hls = hls_New(p_sys->hls_stream, -1, -1, NULL);
                    if (hls == NULL)
                    {
                        p_sys->b_error = true;
                        return VLC_ENOMEM;
                    }
                }
            }
            /* Parse M3U8 Ext Line */
            parse_M3U8ExtLine(s, p_read);
        }
    } while(p_read < p_end);

    free(p_begin);

    /* */
    int count = vlc_array_count(p_sys->hls_stream);
    for (int n = 0; n < count; n++)
    {
        hls_stream_t *hls = hls_Get(p_sys->hls_stream, n);
        if (hls == NULL) break;

        /* Is it a meta playlist? */
        if (p_sys->b_meta)
        {
            if (get_HTTPLivePlaylist(s, hls) != VLC_SUCCESS)
            {
                msg_Err(s, "could not parse playlist file from meta index." );
                return VLC_EGENERIC;
            }
        }

        if (p_sys->b_live)
        {
            /* There should at least be 3 segments of hls->duration */
            int ok = 0;
            int num = vlc_array_count(hls->segments);
            for (int i = 0; i < num; i++)
            {
                segment_t *segment = segment_GetSegment(hls, i);
                if (segment && segment->length >= hls->duration)
                    ok++;
            }
            if (ok < 3)
            {
                msg_Err(s, "cannot start live playback at this time, try again later.");
                return VLC_EGENERIC;
            }
            /* Determine next time to reload playlist */
            p_sys->wakeup = p_sys->last + (hls->duration * 2 * (mtime_t)1000000);
        }
        /* Can we cache files after playback */
        p_sys->b_cache = hls->b_cache;
    }

    return VLC_SUCCESS;
}

/****************************************************************************
 * hls_Thread
 ****************************************************************************/
static int BandwidthAdaptation(stream_t *s, int progid, uint64_t *bandwidth)
{
    stream_sys_t *p_sys = s->p_sys;
    int candidate = -1;
    uint64_t bw = *bandwidth;

    msg_Dbg(s, "bandwidth (bits/s) %"PRIu64, bw * 1000); /* bits / s */

    int count = vlc_array_count(p_sys->hls_stream);
    for (int n = 0; n < count; n++)
    {
        /* Select best bandwidth match */
        hls_stream_t *hls = hls_Get(p_sys->hls_stream, n);
        if (hls == NULL) break;

        /* only consider streams with the same PROGRAM-ID */
        if (hls->id == progid)
        {
            if (bw <= hls->bandwidth)
            {
                *bandwidth = hls->bandwidth;
                candidate = n; /* possible candidate */
            }
        }
    }
    return candidate;
}

static int Download(stream_t *s, hls_stream_t *hls, segment_t *segment, int *cur_stream)
{
    assert(hls);
    assert(segment);

    vlc_mutex_lock(&segment->lock);
    if (segment->data != NULL)
    {
        /* Segment already downloaded */
        vlc_mutex_unlock(&segment->lock);
        return VLC_SUCCESS;
    }

    mtime_t start = mdate();
    if (AccessDownload(s, segment) != VLC_SUCCESS)
    {
        vlc_mutex_unlock(&segment->lock);
        return VLC_EGENERIC;
    }
    mtime_t duration = mdate() - start;

    vlc_mutex_unlock(&segment->lock);

    uint64_t bw = (segment->size * 8) / (duration/1000);      /* bits / ms */
    if (hls->bandwidth != bw)
    {
        int newstream = BandwidthAdaptation(s, hls->id, &bw);
        if ((newstream > 0) && (newstream != *cur_stream))
        {
            msg_Info(s, "switching to %s bandwidth (%"PRIu64") stream",
                     (hls->bandwidth <= bw) ? "faster" : "lower", bw);
            *cur_stream = newstream;
        }
    }
    return VLC_SUCCESS;
}

static void* hls_Thread(vlc_object_t *p_this)
{
    hls_thread_t *client = (hls_thread_t *) p_this;
    stream_t *s = client->s;
    stream_sys_t *p_sys = s->p_sys;

    int canc = vlc_savecancel();

    while (vlc_object_alive(p_this))
    {
        hls_stream_t *hls = hls_Get(client->hls_stream, client->current);
        assert(hls);

        vlc_mutex_lock(&hls->lock);
        segment_t *segment = segment_GetSegment(hls, hls->segment);
        if (segment) hls->segment++;
        vlc_mutex_unlock(&hls->lock);

        /* Is there a new segment to process? */
        if (segment == NULL)
        {
            if (!p_sys->b_live) break;
            mwait(p_sys->wakeup);
        }
        else if (Download(client->s, hls, segment, &client->current) != VLC_SUCCESS)
        {
            if (!p_sys->b_live) break;
        }

        /* FIXME: Reread the m3u8 index file */
        if (p_sys->b_live)
        {
            double wait = 1;
            mtime_t now = mdate();
            if (now >= p_sys->wakeup)
            {
#if 0
                /** FIXME: Implement m3u8 playlist reloading */
                if (!hls_ReloadPlaylist(client->s))
                {
                    /* No change in playlist, then backoff */
                    p_sys->tries++;
                    if (p_sys->tries == 1) wait = 0.5;
                    else if (p_sys->tries == 2) wait = 1;
                    else if (p_sys->tries >= 3) wait = 3;
                }
#endif
                /* determine next time to update playlist */
                p_sys->last = now;
                p_sys->wakeup = now + ((mtime_t)(hls->duration * wait) * (mtime_t)1000000);
            }
        }
    }

    vlc_restorecancel(canc);
    return NULL;
}

static int Prefetch(stream_t *s)
{
    stream_sys_t *p_sys = s->p_sys;
    int current;

again:
    current = p_sys->current;

    hls_stream_t *hls = hls_Get(p_sys->hls_stream, p_sys->current);
    if (hls == NULL)
        return VLC_EGENERIC;

    segment_t *segment = segment_GetSegment(hls, hls->segment);
    if (segment == NULL )
        return VLC_EGENERIC;

    if (Download(s, hls, segment, &p_sys->current) != VLC_SUCCESS)
        return VLC_EGENERIC;

    /* Bandwidth changed? */
    if (current != p_sys->current)
        goto again;

    return VLC_SUCCESS;
}

/****************************************************************************
 * Access
 ****************************************************************************/
static int AccessOpen(stream_t *s, vlc_url_t *url)
{
    stream_sys_t *p_sys = (stream_sys_t *) s->p_sys;

    if ((url->psz_protocol == NULL) ||
        (url->psz_path == NULL))
        return VLC_EGENERIC;

    p_sys->p_access = vlc_object_create(s, sizeof(access_t));
    if (p_sys->p_access)
    {
        p_sys->p_access->psz_access = strdup(url->psz_protocol);
        p_sys->p_access->psz_filepath = strdup(url->psz_path);
        if (url->psz_password || url->psz_username)
        {
            if (asprintf(&p_sys->p_access->psz_location, "%s:%s@%s%s",
                         url->psz_username, url->psz_password,
                         url->psz_host, url->psz_path) < 0)
            {
                msg_Err(s, "creating http access module");
                goto fail;
            }
        }
        else
        {
            if (asprintf(&p_sys->p_access->psz_location, "%s%s",
                         url->psz_host, url->psz_path) < 0)
            {
                msg_Err(s, "creating http access module");
                goto fail;
            }
        }
        vlc_object_attach(p_sys->p_access, s);
        p_sys->p_access->p_module =
            module_need(p_sys->p_access, "access", "http", true);
        if (p_sys->p_access->p_module == NULL)
        {
            msg_Err(s, "could not load http access module");
            goto fail;
        }
        return VLC_SUCCESS;
    }
    return VLC_ENOMEM;

fail:
    vlc_object_release(p_sys->p_access);
    p_sys->p_access = NULL;
    return VLC_EGENERIC;
}

static void AccessClose(stream_t *s)
{
    stream_sys_t *p_sys = (stream_sys_t *) s->p_sys;

    if (p_sys->p_access)
    {
        vlc_object_kill(p_sys->p_access);
        free(p_sys->p_access->psz_access);
        if (p_sys->p_access->p_module)
            module_unneed(p_sys->p_access,
                          p_sys->p_access->p_module);

        vlc_object_release(p_sys->p_access);
        p_sys->p_access = NULL;
    }
}

static char *AccessReadLine(access_t *p_access, uint8_t *psz_tmp, size_t i_len)
{
    char *line = NULL;
    char *begin = (char *)psz_tmp;

    assert(psz_tmp);

    int skip = strlen(begin);
    ssize_t len = p_access->pf_read(p_access, psz_tmp + skip, i_len - skip);
    if (len < 0) return NULL;
    if ((len == 0) && (skip == 0))
        return NULL;

    char *p = begin;
    char *end = p + len + skip;

    while (p < end)
    {
        if (*p == '\n')
            break;

        p++;
    }

    /* EOL */
    line = calloc(1, p - begin + 1);
    if (line == NULL)
        return NULL;
    /* copy line excluding \n */
    line = strndup(begin, p - begin);

    p++;
    if (p < end)
    {
        psz_tmp = memmove(begin, p, end - p);
        psz_tmp[end - p] = '\0';
    }
    else memset(psz_tmp, 0, i_len);

    return line;
}

static int AccessDownload(stream_t *s, segment_t *segment)
{
    stream_sys_t *p_sys = (stream_sys_t *) s->p_sys;

    assert(segment);

    /* Download new playlist file from server */
    if (AccessOpen(s, &segment->url) != VLC_SUCCESS)
        return VLC_EGENERIC;

    segment->size = p_sys->p_access->info.i_size;
    assert(segment->size > 0);

    segment->data = block_Alloc(segment->size);
    if (segment->data == NULL)
    {
        AccessClose(s);
        return VLC_ENOMEM;
    }

    assert(segment->data->i_buffer == segment->size);

    ssize_t length = 0, curlen = 0;
    do
    {
        if (p_sys->p_access->info.i_size > segment->size)
        {
            msg_Dbg(s, "size changed %"PRIu64, segment->size);
            segment->data = block_Realloc(segment->data, 0, p_sys->p_access->info.i_size);
            if (segment->data)
                segment->size = p_sys->p_access->info.i_size;
            assert(segment->data->i_buffer == segment->size);
        }
        length = p_sys->p_access->pf_read(p_sys->p_access,
                    segment->data->p_buffer + curlen, segment->size - curlen);
        if ((length <= 0) || ((uint64_t)length >= segment->size))
            break;
        curlen += length;
    } while (vlc_object_alive(s));

    assert(curlen == (ssize_t)segment->size);

    AccessClose(s);
    return VLC_SUCCESS;
}

/****************************************************************************
 * Open
 ****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    stream_t *s = (stream_t*)p_this;
    stream_sys_t *p_sys;

    if (!isHTTPLiveStreaming(s))
        return VLC_EGENERIC;

    msg_Info(p_this, "HTTP Live Streaming (%s)", s->psz_path);

    /* */
    s->p_sys = p_sys = calloc(1, sizeof(*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    p_sys->b_live = true;
    p_sys->b_meta = false;

    p_sys->hls_stream = vlc_array_new();
    if (p_sys->hls_stream == NULL)
    {
        free(p_sys);
        return VLC_ENOMEM;
    }

    /* */
    s->pf_read = Read;
    s->pf_peek = Peek;
    s->pf_control = Control;

    /* Select first segment to play */
    p_sys->last = mdate();
    if (parse_HTTPLiveStreaming(s) != VLC_SUCCESS)
    {
        goto fail;
    }

    /* */
    p_sys->segment = 0;
    p_sys->current = 0;

    if (Prefetch(s) != VLC_SUCCESS)
    {
        msg_Err(s, "fetching first segment.");
        goto fail;
    }

    p_sys->thread = vlc_object_create(s, sizeof(hls_thread_t));
    if( p_sys->thread == NULL )
    {
        msg_Err(s, "creating HTTP Live Streaming client thread");
        goto fail;
    }

    p_sys->thread->hls_stream = p_sys->hls_stream;
    p_sys->thread->current = p_sys->current;
    p_sys->thread->s = s;

    if (vlc_thread_create(p_sys->thread, "HTTP Live Streaming client",
                          hls_Thread, VLC_THREAD_PRIORITY_INPUT))
    {
        goto fail;
    }

    vlc_object_attach(p_sys->thread, s);

    return VLC_SUCCESS;

fail:
    Close(p_this);
    return VLC_EGENERIC;
}

/****************************************************************************
 * Close
 ****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    stream_t *s = (stream_t*)p_this;
    stream_sys_t *p_sys = s->p_sys;

    assert(p_sys->hls_stream);

    /* */
    if (p_sys->thread)
    {
        vlc_object_kill(p_sys->thread);
        vlc_thread_join(p_sys->thread);
        vlc_object_release(p_sys->thread);
    }

    /* Free hls streams */
    for (int i = 0; i < vlc_array_count(p_sys->hls_stream); i++)
    {
        hls_stream_t *hls;
        hls = (hls_stream_t *)vlc_array_item_at_index(p_sys->hls_stream, i);
        if (hls) hls_Free(hls);
    }
    vlc_array_destroy(p_sys->hls_stream);

    /* */
    free(p_sys);
}

/****************************************************************************
 * Stream filters functions
 ****************************************************************************/
static segment_t *NextSegment(stream_t *s)
{
    stream_sys_t *p_sys = s->p_sys;
    segment_t *segment;

    do
    {
        /* Is the next segment ready */
        hls_stream_t *hls = hls_Get(p_sys->hls_stream, p_sys->current);
        if (hls == NULL) return NULL;

        segment = segment_GetSegment(hls, p_sys->segment);
        if (segment == NULL) return NULL;

        /* This segment is ready? */
        if (segment->data != NULL)
            return segment;

        /* Was the stream changed to another bitrate? */
        if (!p_sys->b_meta) return NULL;

        if (p_sys->current != p_sys->thread->current)
        {
            /* YES it was */
            p_sys->current = p_sys->thread->current;
        }
        else
        {
#if 0
            /* Not downloaded yet, do it here */
            if (Download(s, hls, segment, &p_sys->current) != VLC_SUCCESS)
                return segment;
            else
#endif
                return NULL;
        }
    }
    while(vlc_object_alive(s));

    return segment;
}

static ssize_t hls_Read(stream_t *s, uint8_t *p_read, unsigned int i_read)
{
    stream_sys_t *p_sys = s->p_sys;
    ssize_t copied = 0;

    do
    {
        /* Determine next segment to read. If this is a meta playlist and
         * bandwidth conditions changed, then the stream might have switched
         * to another bandwidth. */
        segment_t *segment = NextSegment(s);
        if (segment == NULL)
            break;

        vlc_mutex_lock(&segment->lock);
        if (segment->data->i_buffer == 0)
        {
            if (!p_sys->b_cache)
            {
                block_Release(segment->data);
                segment->data = NULL;
            }
            msg_Dbg(s, "switching to segment %d", p_sys->segment);
            p_sys->segment++;
            vlc_mutex_unlock(&segment->lock);
            continue;
        }

        ssize_t len = -1;
        if (i_read <= segment->data->i_buffer)
            len = i_read;
        else if (i_read > segment->data->i_buffer)
            len = segment->data->i_buffer;

        if (len > 0)
        {
            memcpy(p_read + copied, segment->data->p_buffer, len);
            segment->data->i_buffer -= len;
            segment->data->p_buffer += len;
            copied += len;
            i_read -= len;
        }
        vlc_mutex_unlock(&segment->lock);

    } while ((i_read > 0) && vlc_object_alive(s));

    return copied;
}

static int Read(stream_t *s, void *buffer, unsigned int i_read)
{
    stream_sys_t *p_sys = s->p_sys;
    ssize_t length = 0;

    assert(p_sys->hls_stream);

    if (buffer == NULL)
    {
        /* caller skips data, get big enough buffer */
        msg_Warn(s, "buffer is NULL (allocate %d)", i_read);
        buffer = calloc(1, i_read);
        if (buffer == NULL)
            return 0; /* NO MEMORY left*/
    }

    length = hls_Read(s, (uint8_t*) buffer, i_read);
    if (length < 0)
        return 0;

    p_sys->offset += length;
    return length;
}

static int Peek(stream_t *s, const uint8_t **pp_peek, unsigned int i_peek)
{
    stream_sys_t *p_sys = s->p_sys;
    size_t curlen = 0;

    hls_stream_t *hls = hls_Get(p_sys->hls_stream, p_sys->current);
    if (hls == NULL)
        return 0;

    int peek_segment = p_sys->segment;
    do
    {
        segment_t *segment = segment_GetSegment(hls, peek_segment);
        if (segment == NULL) return 0;

        vlc_mutex_lock(&segment->lock);
        if (i_peek < segment->data->i_buffer)
        {
            *pp_peek = segment->data->p_buffer;
            curlen += i_peek;
        }
        else
        {
             peek_segment++;
        }
        vlc_mutex_unlock(&segment->lock);
    } while ((curlen < i_peek) && vlc_object_alive(s));

    return curlen;
}

static bool hls_MaySeek(stream_t *s)
{
    stream_sys_t *p_sys = s->p_sys;

    if (p_sys->hls_stream == NULL)
        return false;

    hls_stream_t *hls = hls_Get(p_sys->hls_stream, p_sys->current);
    if (hls == NULL) return false;

    if (p_sys->b_live)
    {
       int count = vlc_array_count(hls->segments);
       vlc_mutex_lock(&hls->lock);
       bool may_seek = (hls->segment < count - 2);
       vlc_mutex_unlock(&hls->lock);
       return may_seek;
    }
    return true;
}

static uint64_t GetStreamSize(stream_t *s)
{
    stream_sys_t *p_sys = s->p_sys;

    if (p_sys->b_live)
        return 0;

    hls_stream_t *hls = hls_Get(p_sys->hls_stream, p_sys->current);
    if (hls == NULL) return 0;

    /* FIXME: Stream size is not entirely exacts at this point */
    uint64_t length = 0UL;
    int count = vlc_array_count(hls->segments);
    for (int n = 0; n < count; n++)
    {
        segment_t *segment = segment_GetSegment(hls, n);
        if (segment)
        {
            length += (segment->size > 0) ? segment->size :
                        (segment->length * hls->bandwidth);
        }
    }
    return length;
}

static int segment_Seek(stream_t *s, uint64_t pos)
{
    stream_sys_t *p_sys = s->p_sys;

    /* Find the right offset */
    hls_stream_t *hls = hls_Get(p_sys->hls_stream, p_sys->current);
    if (hls == NULL)
        return VLC_EGENERIC;

    uint64_t length = 0;
    bool b_found = false;

    int count = vlc_array_count(hls->segments);
    for (int n = 0; n < count; n++)
    {
        /* FIXME: Seeking in segments not dowloaded is not supported. */
        if (n >= hls->segment)
        {
            msg_Err(s, "seeking in segment not downloaded yet.");
            return VLC_EGENERIC;
        }

        segment_t *segment = vlc_array_item_at_index(hls->segments, n);
        if (segment == NULL)
            return VLC_EGENERIC;

        vlc_mutex_lock(&segment->lock);
        length += segment->size;
        uint64_t size = segment->size -segment->data->i_buffer;
        if (size > 0)
        {
            segment->data->i_buffer += size;
            segment->data->p_buffer -= size;
        }

        if (!b_found && (pos <= length))
        {
            uint64_t used = length - pos;
            segment->data->i_buffer -= used;
            segment->data->p_buffer += used;

            count = p_sys->segment;
            p_sys->segment = n;
            b_found = true;
        }
        vlc_mutex_unlock(&segment->lock);
    }
    return VLC_SUCCESS;
}

static int Control(stream_t *s, int i_query, va_list args)
{
    stream_sys_t *p_sys = s->p_sys;

    switch (i_query)
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
            *(va_arg (args, bool *)) = hls_MaySeek(s);
            break;
        case STREAM_GET_POSITION:
            *(va_arg (args, uint64_t *)) = p_sys->offset;
            break;
        case STREAM_SET_POSITION:
            if (hls_MaySeek(s))
            {
                uint64_t pos = (uint64_t)va_arg(args, uint64_t);
                if (segment_Seek(s, pos) == VLC_SUCCESS)
                {
                    p_sys->offset = pos;
                    break;
                }
            }
            return VLC_EGENERIC;
        case STREAM_GET_SIZE:
            *(va_arg (args, uint64_t *)) = GetStreamSize(s);
            break;
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
