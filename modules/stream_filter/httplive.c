/*****************************************************************************
 * httplive.c: HTTP Live Streaming stream filter
 *****************************************************************************
 * Copyright (C) 2010-2012 M2X BV
 * $Id$
 *
 * Author: Jean-Paul Saman <jpsaman _AT_ videolan _DOT_ org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <assert.h>
#include <errno.h>
#include <gcrypt.h>

#include <vlc_threads.h>
#include <vlc_arrays.h>
#include <vlc_stream.h>
#include <vlc_memory.h>
#include <vlc_gcrypt.h>

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
#define AES_BLOCK_SIZE 16 /* Only support AES-128 */
typedef struct segment_s
{
    int         sequence;   /* unique sequence number */
    int         duration;   /* segment duration (seconds) */
    uint64_t    size;       /* segment size in bytes */
    uint64_t    bandwidth;  /* bandwidth usage of segments (bits per second)*/

    char        *url;
    char       *psz_key_path;         /* url key path */
    uint8_t     aes_key[16];      /* AES-128 */
    bool        b_key_loaded;

    vlc_mutex_t lock;
    block_t     *data;      /* data */
} segment_t;

typedef struct hls_stream_s
{
    int         id;         /* program id */
    int         version;    /* protocol version should be 1 */
    int         sequence;   /* media sequence number */
    int         duration;   /* maximum duration per segment (s) */
    uint64_t    bandwidth;  /* bandwidth usage of segments (bits per second)*/
    uint64_t    size;       /* stream length is calculated by taking the sum
                               foreach segment of (segment->duration * hls->bandwidth/8) */

    vlc_array_t *segments;  /* list of segments */
    char        *url;        /* uri to m3u8 */
    vlc_mutex_t lock;
    bool        b_cache;    /* allow caching */

    char        *psz_current_key_path;          /* URL path of the encrypted key */
    uint8_t      psz_AES_IV[AES_BLOCK_SIZE];    /* IV used when decypher the block */
    bool         b_iv_loaded;
} hls_stream_t;

struct stream_sys_t
{
    char         *m3u8;         /* M3U8 url */
    vlc_thread_t  reload;       /* HLS m3u8 reload thread */
    vlc_thread_t  thread;       /* HLS segment download thread */

    block_t      *peeked;

    /* */
    vlc_array_t  *hls_stream;   /* bandwidth adaptation */
    uint64_t      bandwidth;    /* measured bandwidth (bits per second) */

    /* Download */
    struct hls_download_s
    {
        int         stream;     /* current hls_stream  */
        int         segment;    /* current segment for downloading */
        int         seek;       /* segment requested by seek (default -1) */
        vlc_mutex_t lock_wait;  /* protect segment download counter */
        vlc_cond_t  wait;       /* some condition to wait on */
    } download;

    /* Playback */
    struct hls_playback_s
    {
        uint64_t    offset;     /* current offset in media */
        int         stream;     /* current hls_stream  */
        int         segment;    /* current segment for playback */
    } playback;

    /* Playlist */
    struct hls_playlist_s
    {
        mtime_t     last;       /* playlist last loaded */
        mtime_t     wakeup;     /* next reload time */
        int         tries;      /* times it was not changed */
    } playlist;

    struct hls_read_s
    {
        vlc_mutex_t lock_wait;  /* used by read condition variable */
        vlc_cond_t  wait;       /* some condition to wait on during read */
    } read;

    /* state */
    bool        b_cache;    /* can cache files */
    bool        b_meta;     /* meta playlist */
    bool        b_live;     /* live stream? or vod? */
    bool        b_error;    /* parsing error */
    bool        b_aesmsg;   /* only print one time that the media is encrypted */

    /* Shared data */
    vlc_cond_t   wait;
    vlc_mutex_t  lock;
    bool         paused;
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  Read   (stream_t *, void *p_read, unsigned int i_read);
static int  Peek   (stream_t *, const uint8_t **pp_peek, unsigned int i_peek);
static int  Control(stream_t *, int i_query, va_list);

static ssize_t read_M3U8_from_stream(stream_t *s, uint8_t **buffer);
static ssize_t read_M3U8_from_url(stream_t *s, const char *psz_url, uint8_t **buffer);
static char *ReadLine(uint8_t *buffer, uint8_t **pos, size_t len);

static int hls_Download(stream_t *s, segment_t *segment);

static void* hls_Thread(void *);
static void* hls_Reload(void *);

static segment_t *segment_GetSegment(hls_stream_t *hls, int wanted);
static void segment_Free(segment_t *segment);

/****************************************************************************
 *
 ****************************************************************************/
static bool isHTTPLiveStreaming(stream_t *s)
{
    const uint8_t *peek;

    int size = stream_Peek(s->p_source, &peek, 46);
    if (size < 7)
        return false;

    if (memcmp(peek, "#EXTM3U", 7) != 0)
        return false;

    peek += 7;
    size -= 7;

    /* Parse stream and search for
     * EXT-X-TARGETDURATION or EXT-X-STREAM-INF tag, see
     * http://tools.ietf.org/html/draft-pantos-http-live-streaming-04#page-8 */
    while (size--)
    {
        static const char *const ext[] = {
            "TARGETDURATION",
            "MEDIA-SEQUENCE",
            "KEY",
            "ALLOW-CACHE",
            "ENDLIST",
            "STREAM-INF",
            "DISCONTINUITY",
            "VERSION"
        };

        if (*peek++ != '#')
            continue;

        if (size < 6)
            continue;

        if (memcmp(peek, "EXT-X-", 6))
            continue;

        peek += 6;
        size -= 6;

        for (size_t i = 0; i < ARRAY_SIZE(ext); i++)
        {
            size_t len = strlen(ext[i]);
            if (size < 0 || (size_t)size < len)
                continue;
            if (!memcmp(peek, ext[i], len))
                return true;
        }
    }

    return false;
}

/* HTTP Live Streaming */
static hls_stream_t *hls_New(vlc_array_t *hls_stream, const int id, const uint64_t bw, const char *uri)
{
    hls_stream_t *hls = (hls_stream_t *)malloc(sizeof(hls_stream_t));
    if (hls == NULL) return NULL;

    hls->id = id;
    hls->bandwidth = bw;
    hls->duration = -1;/* unknown */
    hls->size = 0;
    hls->sequence = 0; /* default is 0 */
    hls->version = 1;  /* default protocol version */
    hls->b_cache = true;
    hls->url = strdup(uri);
    if (hls->url == NULL)
    {
        free(hls);
        return NULL;
    }
    hls->psz_current_key_path = NULL;
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
            segment_t *segment = segment_GetSegment(hls, n);
            if (segment) segment_Free(segment);
        }
        vlc_array_destroy(hls->segments);
    }
    free(hls->url);
    free(hls->psz_current_key_path);
    free(hls);
}

static hls_stream_t *hls_Copy(hls_stream_t *src, const bool b_cp_segments)
{
    assert(src);
    assert(!b_cp_segments); /* FIXME: copying segments is not implemented */

    hls_stream_t *dst = (hls_stream_t *)malloc(sizeof(hls_stream_t));
    if (dst == NULL) return NULL;

    dst->id = src->id;
    dst->bandwidth = src->bandwidth;
    dst->duration = src->duration;
    dst->size = src->size;
    dst->sequence = src->sequence;
    dst->version = src->version;
    dst->b_cache = src->b_cache;
    dst->psz_current_key_path = src->psz_current_key_path ?
                strdup( src->psz_current_key_path ) : NULL;
    dst->url = strdup(src->url);
    if (dst->url == NULL)
    {
        free(dst);
        return NULL;
    }
    if (!b_cp_segments)
        dst->segments = vlc_array_new();
    vlc_mutex_init(&dst->lock);
    return dst;
}

static hls_stream_t *hls_Get(vlc_array_t *hls_stream, const int wanted)
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
    return hls_Get(hls_stream, 0);
}

static hls_stream_t *hls_GetLast(vlc_array_t *hls_stream)
{
    int count = vlc_array_count(hls_stream);
    if (count <= 0)
        return NULL;
    count--;
    return hls_Get(hls_stream, count);
}

static hls_stream_t *hls_Find(vlc_array_t *hls_stream, hls_stream_t *hls_new)
{
    int count = vlc_array_count(hls_stream);
    for (int n = 0; n < count; n++)
    {
        hls_stream_t *hls = hls_Get(hls_stream, n);
        if (hls)
        {
            /* compare */
            if ((hls->id == hls_new->id) &&
                ((hls->bandwidth == hls_new->bandwidth)||(hls_new->bandwidth==0)))
                return hls;
        }
    }
    return NULL;
}

static uint64_t hls_GetStreamSize(hls_stream_t *hls)
{
    /* NOTE: Stream size is calculated based on segment duration and
     * HLS stream bandwidth from the .m3u8 file. If these are not correct
     * then the deviation from exact byte size will be big and the seek/
     * progressbar will not behave entirely as one expects. */
    uint64_t size = 0UL;

    /* If there is no valid bandwidth yet, then there is no point in
     * computing stream size. */
    if (hls->bandwidth == 0)
        return size;

    int count = vlc_array_count(hls->segments);
    for (int n = 0; n < count; n++)
    {
        segment_t *segment = segment_GetSegment(hls, n);
        if (segment)
        {
            size += (segment->duration * (hls->bandwidth / 8));
        }
    }
    return size;
}

/* Segment */
static segment_t *segment_New(hls_stream_t* hls, const int duration, const char *uri)
{
    segment_t *segment = (segment_t *)malloc(sizeof(segment_t));
    if (segment == NULL)
        return NULL;

    segment->duration = duration; /* seconds */
    segment->size = 0; /* bytes */
    segment->sequence = 0;
    segment->bandwidth = 0;
    segment->url = strdup(uri);
    if (segment->url == NULL)
    {
        free(segment);
        return NULL;
    }
    segment->data = NULL;
    vlc_array_append(hls->segments, segment);
    vlc_mutex_init(&segment->lock);
    segment->b_key_loaded = false;
    segment->psz_key_path = NULL;
    if (hls->psz_current_key_path)
        segment->psz_key_path = strdup(hls->psz_current_key_path);
    return segment;
}

static void segment_Free(segment_t *segment)
{
    vlc_mutex_destroy(&segment->lock);

    free(segment->url);
    free(segment->psz_key_path);
    if (segment->data)
        block_Release(segment->data);
    free(segment);
}

static segment_t *segment_GetSegment(hls_stream_t *hls, const int wanted)
{
    assert(hls);

    int count = vlc_array_count(hls->segments);
    if (count <= 0)
        return NULL;
    if ((wanted < 0) || (wanted >= count))
        return NULL;
    return (segment_t *) vlc_array_item_at_index(hls->segments, wanted);
}

static segment_t *segment_Find(hls_stream_t *hls, const int sequence)
{
    assert(hls);

    int count = vlc_array_count(hls->segments);
    if (count <= 0) return NULL;
    for (int n = 0; n < count; n++)
    {
        segment_t *segment = segment_GetSegment(hls, n);
        if (segment == NULL) break;
        if (segment->sequence == sequence)
            return segment;
    }
    return NULL;
}

static int ChooseSegment(stream_t *s, const int current)
{
    stream_sys_t *p_sys = (stream_sys_t *)s->p_sys;
    hls_stream_t *hls = hls_Get(p_sys->hls_stream, current);
    if (hls == NULL) return 0;

    /* Choose a segment to start which is no closer than
     * 3 times the target duration from the end of the playlist.
     */
    int wanted = 0;
    int duration = 0;
    int sequence = 0;
    int count = vlc_array_count(hls->segments);
    int i = p_sys->b_live ? count - 1 : -1;

    /* We do while loop only with live case, otherwise return 0*/
    while((i >= 0) && (i < count))
    {
        segment_t *segment = segment_GetSegment(hls, i);
        assert(segment);

        if (segment->duration > hls->duration)
        {
            msg_Err(s, "EXTINF:%d duration is larger than EXT-X-TARGETDURATION:%d",
                    segment->duration, hls->duration);
        }

        duration += segment->duration;
        if (duration >= 3 * hls->duration)
        {
            /* Start point found */
            wanted = i;
            sequence = segment->sequence;
            break;
        }

        i-- ;
    }

    msg_Dbg(s, "Choose segment %d/%d (sequence=%d)", wanted, count, sequence);
    return wanted;
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
        if (strncasecmp(begin, attr, strlen(attr)) == 0
          && begin[strlen(attr)] == '=')
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

static int string_to_IV(char *string_hexa, uint8_t iv[AES_BLOCK_SIZE])
{
    unsigned long long iv_hi, iv_lo;
    char *end = NULL;
    if (*string_hexa++ != '0')
        return VLC_EGENERIC;
    if (*string_hexa != 'x' && *string_hexa != 'X')
        return VLC_EGENERIC;

    string_hexa++;

    size_t len = strlen(string_hexa);
    if (len <= 16) {
        iv_hi = 0;
        iv_lo = strtoull(string_hexa, &end, 16);
        if (end)
            return VLC_EGENERIC;
    } else {
        iv_lo = strtoull(&string_hexa[len-16], NULL, 16);
        if (end)
            return VLC_EGENERIC;
        string_hexa[len-16] = '\0';
        iv_hi = strtoull(string_hexa, NULL, 16);
        if (end)
            return VLC_EGENERIC;
    }

    for (int i = 7; i >= 0 ; --i) {
        iv[  i] = iv_hi & 0xff;
        iv[8+i] = iv_lo & 0xff;
        iv_hi >>= 8;
        iv_lo >>= 8;
    }

    return VLC_SUCCESS;
}

static char *relative_URI(const char *psz_url, const char *psz_path)
{
    char *ret = NULL;
    assert(psz_url != NULL && psz_path != NULL);


    //If the path is actually an absolute URL, don't do anything.
    if (strncmp(psz_path, "http", 4) == 0)
        return NULL;

    size_t len = strlen(psz_path);

    char *new_url = strdup(psz_url);
    if (unlikely(!new_url))
        return NULL;

    if( psz_path[0] == '/' ) //Relative URL with absolute path
    {
        //Try to find separator for name and path, try to skip
        //access and first ://
        char *slash = strchr(&new_url[8], '/');
        if (unlikely(slash == NULL))
            goto end;
        *slash = '\0';
    } else {
        int levels = 0;
        while(len >= 3 && !strncmp(psz_path, "../", 3)) {
            psz_path += 3;
            len -= 3;
            levels++;
        }
        do {
            char *slash = strrchr(new_url, '/');
            if (unlikely(slash == NULL))
                goto end;
            *slash = '\0';
        } while (levels--);
    }

    if (asprintf(&ret, "%s/%s", new_url, psz_path) < 0)
        ret = NULL;

end:
    free(new_url);
    return ret;
}

static int parse_SegmentInformation(hls_stream_t *hls, char *p_read, int *duration)
{
    assert(hls);
    assert(p_read);

    /* strip of #EXTINF: */
    char *p_next = NULL;
    char *token = strtok_r(p_read, ":", &p_next);
    if (token == NULL)
        return VLC_EGENERIC;

    /* read duration */
    token = strtok_r(NULL, ",", &p_next);
    if (token == NULL)
        return VLC_EGENERIC;

    int value;
    char *endptr;
    if (hls->version < 3)
    {
        errno = 0;
        value = strtol(token, &endptr, 10);
        if (token == endptr || errno == ERANGE)
        {
            *duration = -1;
            return VLC_EGENERIC;
        }
        *duration = value;
    }
    else
    {
        errno = 0;
        double d = strtof(token, &endptr);
        if (token == endptr || errno == ERANGE)
        {
            *duration = -1;
            return VLC_EGENERIC;
        }
        if ((d) - ((int)d) >= 0.5)
            value = ((int)d) + 1;
        else
            value = ((int)d);
        *duration = value;
    }

    /* Ignore the rest of the line */
    return VLC_SUCCESS;
}

static int parse_AddSegment(hls_stream_t *hls, const int duration, const char *uri)
{
    assert(hls);
    assert(uri);

    /* Store segment information */
    vlc_mutex_lock(&hls->lock);

    char *psz_uri = relative_URI(hls->url, uri);

    segment_t *segment = segment_New(hls, duration, psz_uri ? psz_uri : uri);
    if (segment)
        segment->sequence = hls->sequence + vlc_array_count(hls->segments) - 1;
    free(psz_uri);

    vlc_mutex_unlock(&hls->lock);

    return segment ? VLC_SUCCESS : VLC_ENOMEM;
}

static int parse_TargetDuration(stream_t *s, hls_stream_t *hls, char *p_read)
{
    assert(hls);

    int duration = -1;
    int ret = sscanf(p_read, "#EXT-X-TARGETDURATION:%d", &duration);
    if (ret != 1)
    {
        msg_Err(s, "expected #EXT-X-TARGETDURATION:<s>");
        return VLC_EGENERIC;
    }

    hls->duration = duration; /* seconds */
    return VLC_SUCCESS;
}

static int parse_StreamInformation(stream_t *s, vlc_array_t **hls_stream,
                                   hls_stream_t **hls, char *p_read, const char *uri)
{
    int id;
    uint64_t bw;
    char *attr;

    assert(*hls == NULL);

    attr = parse_Attributes(p_read, "PROGRAM-ID");
    if (attr)
    {
        id = atol(attr);
        free(attr);
    }
    else
        id = 0;

    attr = parse_Attributes(p_read, "BANDWIDTH");
    if (attr == NULL)
    {
        msg_Err(s, "#EXT-X-STREAM-INF: expected BANDWIDTH=<value>");
        return VLC_EGENERIC;
    }
    bw = atoll(attr);
    free(attr);

    if (bw == 0)
    {
        msg_Err(s, "#EXT-X-STREAM-INF: bandwidth cannot be 0");
        return VLC_EGENERIC;
    }

    msg_Dbg(s, "bandwidth adaptation detected (program-id=%d, bandwidth=%"PRIu64").", id, bw);

    char *psz_uri = relative_URI(s->p_sys->m3u8, uri);

    *hls = hls_New(*hls_stream, id, bw, psz_uri ? psz_uri : uri);

    free(psz_uri);

    return (*hls == NULL) ? VLC_ENOMEM : VLC_SUCCESS;
}

static int parse_MediaSequence(stream_t *s, hls_stream_t *hls, char *p_read)
{
    assert(hls);

    int sequence;
    int ret = sscanf(p_read, "#EXT-X-MEDIA-SEQUENCE:%d", &sequence);
    if (ret != 1)
    {
        msg_Err(s, "expected #EXT-X-MEDIA-SEQUENCE:<s>");
        return VLC_EGENERIC;
    }

    if (hls->sequence > 0)
    {
        if (s->p_sys->b_live)
        {
            hls_stream_t *last = hls_GetLast(s->p_sys->hls_stream);
            segment_t *last_segment = segment_GetSegment( last, vlc_array_count( last->segments ) - 1 );
            if ( ( last_segment->sequence < sequence) &&
                 ( sequence - last_segment->sequence >= 1 ))
                msg_Err(s, "EXT-X-MEDIA-SEQUENCE gap in playlist (new=%d, old=%d)",
                            sequence, last_segment->sequence);
        }
        else
            msg_Err(s, "EXT-X-MEDIA-SEQUENCE already present in playlist (new=%d, old=%d)",
                        sequence, hls->sequence);
    }
    hls->sequence = sequence;
    return VLC_SUCCESS;
}

static int parse_Key(stream_t *s, hls_stream_t *hls, char *p_read)
{
    assert(hls);

    /* #EXT-X-KEY:METHOD=<method>[,URI="<URI>"][,IV=<IV>] */
    int err = VLC_SUCCESS;
    char *attr = parse_Attributes(p_read, "METHOD");
    if (attr == NULL)
    {
        msg_Err(s, "#EXT-X-KEY: expected METHOD=<value>");
        return err;
    }

    if (strncasecmp(attr, "NONE", 4) == 0)
    {
        char *uri = parse_Attributes(p_read, "URI");
        if (uri != NULL)
        {
            msg_Err(s, "#EXT-X-KEY: URI not expected");
            err = VLC_EGENERIC;
        }
        free(uri);
        /* IV is only supported in version 2 and above */
        if (hls->version >= 2)
        {
            char *iv = parse_Attributes(p_read, "IV");
            if (iv != NULL)
            {
                msg_Err(s, "#EXT-X-KEY: IV not expected");
                err = VLC_EGENERIC;
            }
            free(iv);
        }
    }
    else if (strncasecmp(attr, "AES-128", 7) == 0)
    {
        char *value, *uri, *iv;
        if (s->p_sys->b_aesmsg == false)
        {
            msg_Dbg(s, "playback of AES-128 encrypted HTTP Live media detected.");
            s->p_sys->b_aesmsg = true;
        }
        value = uri = parse_Attributes(p_read, "URI");
        if (value == NULL)
        {
            msg_Err(s, "#EXT-X-KEY: URI not found for encrypted HTTP Live media in AES-128");
            free(attr);
            return VLC_EGENERIC;
        }

        /* Url is put between quotes, remove them */
        if (*value == '"')
        {
            /* We need to strip the "" from the attribute value */
            uri = value + 1;
            char* end = strchr(uri, '"');
            if (end != NULL)
                *end = 0;
        }
        /* For absolute URI, just duplicate it
         * don't limit to HTTP, maybe some sanity checking
         * should be done more in here? */
        if( strstr( uri , "://" ) )
            hls->psz_current_key_path = strdup( uri );
        else
            hls->psz_current_key_path = relative_URI(hls->url, uri);
        free(value);

        value = iv = parse_Attributes(p_read, "IV");
        if (iv == NULL)
        {
            /*
            * If the EXT-X-KEY tag does not have the IV attribute, implementations
            * MUST use the sequence number of the media file as the IV when
            * encrypting or decrypting that media file.  The big-endian binary
            * representation of the sequence number SHALL be placed in a 16-octet
            * buffer and padded (on the left) with zeros.
            */
            hls->b_iv_loaded = false;
        }
        else
        {
            /*
            * If the EXT-X-KEY tag has the IV attribute, implementations MUST use
            * the attribute value as the IV when encrypting or decrypting with that
            * key.  The value MUST be interpreted as a 128-bit hexadecimal number
            * and MUST be prefixed with 0x or 0X.
            */

            if (string_to_IV(iv, hls->psz_AES_IV) == VLC_EGENERIC)
            {
                msg_Err(s, "IV invalid");
                err = VLC_EGENERIC;
            }
            else
                hls->b_iv_loaded = true;
            free(value);
        }
    }
    else
    {
        msg_Warn(s, "playback of encrypted HTTP Live media is not supported.");
        err = VLC_EGENERIC;
    }
    free(attr);
    return err;
}

static int parse_ProgramDateTime(stream_t *s, hls_stream_t *hls, char *p_read)
{
    VLC_UNUSED(hls);
    msg_Dbg(s, "tag not supported: #EXT-X-PROGRAM-DATE-TIME %s", p_read);
    return VLC_SUCCESS;
}

static int parse_AllowCache(stream_t *s, hls_stream_t *hls, char *p_read)
{
    assert(hls);

    char answer[4] = "\0";
    int ret = sscanf(p_read, "#EXT-X-ALLOW-CACHE:%3s", answer);
    if (ret != 1)
    {
        msg_Err(s, "#EXT-X-ALLOW-CACHE, ignoring ...");
        return VLC_EGENERIC;
    }

    hls->b_cache = (strncmp(answer, "NO", 2) != 0);
    return VLC_SUCCESS;
}

static int parse_Version(stream_t *s, hls_stream_t *hls, char *p_read)
{
    assert(hls);

    int version;
    int ret = sscanf(p_read, "#EXT-X-VERSION:%d", &version);
    if (ret != 1)
    {
        msg_Err(s, "#EXT-X-VERSION: no protocol version found, should be version 1.");
        return VLC_EGENERIC;
    }

    /* Check version */
    hls->version = version;
    if (hls->version <= 0 || hls->version > 3)
    {
        msg_Err(s, "#EXT-X-VERSION should be version 1, 2 or 3 iso %d", version);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int parse_EndList(stream_t *s, hls_stream_t *hls)
{
    assert(hls);

    s->p_sys->b_live = false;
    msg_Dbg(s, "video on demand (vod) mode");
    return VLC_SUCCESS;
}

static int parse_Discontinuity(stream_t *s, hls_stream_t *hls, char *p_read)
{
    assert(hls);

    /* FIXME: Do we need to act on discontinuity ?? */
    msg_Dbg(s, "#EXT-X-DISCONTINUITY %s", p_read);
    return VLC_SUCCESS;
}

static int hls_CompareStreams( const void* a, const void* b )
{
    hls_stream_t*   stream_a = *(hls_stream_t**)a;
    hls_stream_t*   stream_b = *(hls_stream_t**)b;

    return stream_a->bandwidth - stream_b->bandwidth;
}

/* The http://tools.ietf.org/html/draft-pantos-http-live-streaming-04#page-8
 * document defines the following new tags: EXT-X-TARGETDURATION,
 * EXT-X-MEDIA-SEQUENCE, EXT-X-KEY, EXT-X-PROGRAM-DATE-TIME, EXT-X-
 * ALLOW-CACHE, EXT-X-STREAM-INF, EXT-X-ENDLIST, EXT-X-DISCONTINUITY,
 * and EXT-X-VERSION.
 */
static int parse_M3U8(stream_t *s, vlc_array_t *streams, uint8_t *buffer, const ssize_t len)
{
    stream_sys_t *p_sys = s->p_sys;
    uint8_t *p_read, *p_begin, *p_end;

    assert(streams);
    assert(buffer);

    msg_Dbg(s, "parse_M3U8\n%s", buffer);
    p_begin = buffer;
    p_end = p_begin + len;

    char *line = ReadLine(p_begin, &p_read, p_end - p_begin);
    if (line == NULL)
        return VLC_ENOMEM;
    p_begin = p_read;

    if (strncmp(line, "#EXTM3U", 7) != 0)
    {
        msg_Err(s, "missing #EXTM3U tag .. aborting");
        free(line);
        return VLC_EGENERIC;
    }

    free(line);
    line = NULL;

    /* What is the version ? */
    int version = 1;
    uint8_t *p = (uint8_t *)strstr((const char *)buffer, "#EXT-X-VERSION:");
    if (p != NULL)
    {
        uint8_t *tmp = NULL;
        char *psz_version = ReadLine(p, &tmp, p_end - p);
        if (psz_version == NULL)
            return VLC_ENOMEM;
        int ret = sscanf((const char*)psz_version, "#EXT-X-VERSION:%d", &version);
        if (ret != 1)
        {
            msg_Warn(s, "#EXT-X-VERSION: no protocol version found, assuming version 1.");
            version = 1;
        }
        free(psz_version);
        p = NULL;
    }

    /* Is it a live stream ? */
    p_sys->b_live = (strstr((const char *)buffer, "#EXT-X-ENDLIST") == NULL) ? true : false;

    /* Is it a meta index file ? */
    bool b_meta = (strstr((const char *)buffer, "#EXT-X-STREAM-INF") == NULL) ? false : true;

    int err = VLC_SUCCESS;

    if (b_meta)
    {
        msg_Dbg(s, "Meta playlist");

        /* M3U8 Meta Index file */
        do {
            /* Next line */
            line = ReadLine(p_begin, &p_read, p_end - p_begin);
            if (line == NULL)
                break;
            p_begin = p_read;

            /* */
            if (strncmp(line, "#EXT-X-STREAM-INF", 17) == 0)
            {
                p_sys->b_meta = true;
                char *uri = ReadLine(p_begin, &p_read, p_end - p_begin);
                if (uri == NULL)
                    err = VLC_ENOMEM;
                else
                {
                    if (*uri == '#')
                    {
                        msg_Warn(s, "Skipping invalid stream-inf: %s", uri);
                        free(uri);
                    }
                    else
                    {
                        bool new_stream_added = false;
                        hls_stream_t *hls = NULL;
                        err = parse_StreamInformation(s, &streams, &hls, line, uri);
                        if (err == VLC_SUCCESS)
                            new_stream_added = true;

                        free(uri);

                        if (hls)
                        {
                            /* Download playlist file from server */
                            uint8_t *buf = NULL;
                            ssize_t len = read_M3U8_from_url(s, hls->url, &buf);
                            if (len < 0)
                            {
                                msg_Warn(s, "failed to read %s, continue for other streams", hls->url);

                                /* remove stream just added */
                                if (new_stream_added)
                                    vlc_array_remove(streams, vlc_array_count(streams) - 1);

                                /* ignore download error, so we have chance to try other streams */
                                err = VLC_SUCCESS;
                            }
                            else
                            {
                                /* Parse HLS m3u8 content. */
                                err = parse_M3U8(s, streams, buf, len);
                                free(buf);
                            }

                            hls->version = version;
                            if (!p_sys->b_live)
                                hls->size = hls_GetStreamSize(hls); /* Stream size (approximate) */
                        }
                    }
                }
                p_begin = p_read;
            }

            free(line);
            line = NULL;

            if (p_begin >= p_end)
                break;

        } while (err == VLC_SUCCESS);

        size_t stream_count = vlc_array_count(streams);
        msg_Dbg(s, "%d streams loaded in Meta playlist", (int)stream_count);
        if (stream_count == 0)
        {
            msg_Err(s, "No playable streams found in Meta playlist");
            err = VLC_EGENERIC;
        }
    }
    else
    {
        msg_Dbg(s, "%s Playlist HLS protocol version: %d", p_sys->b_live ? "Live": "VOD", version);

        hls_stream_t *hls = NULL;
        if (p_sys->b_meta)
            hls = hls_GetLast(streams);
        else
        {
            /* No Meta playlist used */
            hls = hls_New(streams, 0, 0, p_sys->m3u8);
            if (hls)
            {
                /* Get TARGET-DURATION first */
                p = (uint8_t *)strstr((const char *)buffer, "#EXT-X-TARGETDURATION:");
                if (p)
                {
                    uint8_t *p_rest = NULL;
                    char *psz_duration = ReadLine(p, &p_rest,  p_end - p);
                    if (psz_duration == NULL)
                        return VLC_EGENERIC;
                    err = parse_TargetDuration(s, hls, psz_duration);
                    free(psz_duration);
                    p = NULL;
                }

                /* Store version */
                hls->version = version;
            }
            else return VLC_ENOMEM;
        }
        assert(hls);

        /* */
        bool media_sequence_loaded = false;
        int segment_duration = -1;
        do
        {
            /* Next line */
            line = ReadLine(p_begin, &p_read, p_end - p_begin);
            if (line == NULL)
                break;
            p_begin = p_read;

            if (strncmp(line, "#EXTINF", 7) == 0)
                err = parse_SegmentInformation(hls, line, &segment_duration);
            else if (strncmp(line, "#EXT-X-TARGETDURATION", 21) == 0)
                err = parse_TargetDuration(s, hls, line);
            else if (strncmp(line, "#EXT-X-MEDIA-SEQUENCE", 21) == 0)
            {
                /* A Playlist file MUST NOT contain more than one EXT-X-MEDIA-SEQUENCE tag. */
                /* We only care about first one */
                if (!media_sequence_loaded)
                {
                    err = parse_MediaSequence(s, hls, line);
                    media_sequence_loaded = true;
                }
            }
            else if (strncmp(line, "#EXT-X-KEY", 10) == 0)
                err = parse_Key(s, hls, line);
            else if (strncmp(line, "#EXT-X-PROGRAM-DATE-TIME", 24) == 0)
                err = parse_ProgramDateTime(s, hls, line);
            else if (strncmp(line, "#EXT-X-ALLOW-CACHE", 18) == 0)
                err = parse_AllowCache(s, hls, line);
            else if (strncmp(line, "#EXT-X-DISCONTINUITY", 20) == 0)
                err = parse_Discontinuity(s, hls, line);
            else if (strncmp(line, "#EXT-X-VERSION", 14) == 0)
                err = parse_Version(s, hls, line);
            else if (strncmp(line, "#EXT-X-ENDLIST", 14) == 0)
                err = parse_EndList(s, hls);
            else if ((strncmp(line, "#", 1) != 0) && (*line != '\0') )
            {
                err = parse_AddSegment(hls, segment_duration, line);
                segment_duration = -1; /* reset duration */
            }

            free(line);
            line = NULL;

            if (p_begin >= p_end)
                break;

        } while (err == VLC_SUCCESS);

        free(line);
    }

    return err;
}


static int hls_DownloadSegmentKey(stream_t *s, segment_t *seg)
{
    stream_t *p_m3u8 = stream_UrlNew(s, seg->psz_key_path);
    if (p_m3u8 == NULL)
    {
        msg_Err(s, "Failed to load the AES key for segment sequence %d", seg->sequence);
        return VLC_EGENERIC;
    }

    int len = stream_Read(p_m3u8, seg->aes_key, sizeof(seg->aes_key));
    stream_Delete(p_m3u8);
    if (len != AES_BLOCK_SIZE)
    {
        msg_Err(s, "The AES key loaded doesn't have the right size (%d)", len);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int hls_ManageSegmentKeys(stream_t *s, hls_stream_t *hls)
{
    segment_t   *seg = NULL;
    segment_t   *prev_seg;
    int         count = vlc_array_count(hls->segments);

    for (int i = 0; i < count; i++)
    {
        prev_seg = seg;
        seg = segment_GetSegment(hls, i);
        if (seg == NULL )
            continue;
        if (seg->psz_key_path == NULL)
            continue;   /* No key to load ? continue */
        if (seg->b_key_loaded)
            continue;   /* The key is already loaded */

        /* if the key has not changed, and already available from previous segment,
         * try to copy it, and don't load the key */
        if (prev_seg && prev_seg->b_key_loaded && strcmp(seg->psz_key_path, prev_seg->psz_key_path) == 0)
        {
            memcpy(seg->aes_key, prev_seg->aes_key, AES_BLOCK_SIZE);
            seg->b_key_loaded = true;
            continue;
        }
        if (hls_DownloadSegmentKey(s, seg) != VLC_SUCCESS)
            return VLC_EGENERIC;
       seg->b_key_loaded = true;
    }
    return VLC_SUCCESS;
}

static int hls_DecodeSegmentData(stream_t *s, hls_stream_t *hls, segment_t *segment)
{
    /* Did the segment need to be decoded ? */
    if (segment->psz_key_path == NULL)
        return VLC_SUCCESS;

    /* Do we have loaded the key ? */
    if (!segment->b_key_loaded)
    {
        /* No ? try to download it now */
        if (hls_ManageSegmentKeys(s, hls) != VLC_SUCCESS)
            return VLC_EGENERIC;
    }

    /* For now, we only decode AES-128 data */
    gcry_error_t i_gcrypt_err;
    gcry_cipher_hd_t aes_ctx;
    /* Setup AES */
    i_gcrypt_err = gcry_cipher_open(&aes_ctx, GCRY_CIPHER_AES,
                                     GCRY_CIPHER_MODE_CBC, 0);
    if (i_gcrypt_err)
    {
        msg_Err(s, "gcry_cipher_open failed: %s", gpg_strerror(i_gcrypt_err));
        gcry_cipher_close(aes_ctx);
        return VLC_EGENERIC;
    }

    /* Set key */
    i_gcrypt_err = gcry_cipher_setkey(aes_ctx, segment->aes_key,
                                       sizeof(segment->aes_key));
    if (i_gcrypt_err)
    {
        msg_Err(s, "gcry_cipher_setkey failed: %s", gpg_strerror(i_gcrypt_err));
        gcry_cipher_close(aes_ctx);
        return VLC_EGENERIC;
    }

    if (hls->b_iv_loaded == false)
    {
        memset(hls->psz_AES_IV, 0, AES_BLOCK_SIZE);
        hls->psz_AES_IV[15] = segment->sequence & 0xff;
        hls->psz_AES_IV[14] = (segment->sequence >> 8)& 0xff;
        hls->psz_AES_IV[13] = (segment->sequence >> 16)& 0xff;
        hls->psz_AES_IV[12] = (segment->sequence >> 24)& 0xff;
    }

    i_gcrypt_err = gcry_cipher_setiv(aes_ctx, hls->psz_AES_IV,
                                      sizeof(hls->psz_AES_IV));

    if (i_gcrypt_err)
    {
        msg_Err(s, "gcry_cipher_setiv failed: %s", gpg_strerror(i_gcrypt_err));
        gcry_cipher_close(aes_ctx);
        return VLC_EGENERIC;
    }

    i_gcrypt_err = gcry_cipher_decrypt(aes_ctx,
                                       segment->data->p_buffer, /* out */
                                       segment->data->i_buffer,
                                       NULL, /* in */
                                       0);
    if (i_gcrypt_err)
    {
        msg_Err(s, "gcry_cipher_decrypt failed:  %s/%s\n", gcry_strsource(i_gcrypt_err), gcry_strerror(i_gcrypt_err));
        gcry_cipher_close(aes_ctx);
        return VLC_EGENERIC;
    }
    gcry_cipher_close(aes_ctx);
    /* remove the PKCS#7 padding from the buffer */
    int pad = segment->data->p_buffer[segment->data->i_buffer-1];
    if (pad <= 0 || pad > AES_BLOCK_SIZE)
    {
        msg_Err(s, "Bad padding character (0x%x), perhaps we failed to decrypt the segment with the correct key", pad);
        return VLC_EGENERIC;
    }
    int count = pad;
    while (count--)
    {
        if (segment->data->p_buffer[segment->data->i_buffer-1-count] != pad)
        {
                msg_Err(s, "Bad ending buffer, perhaps we failed to decrypt the segment with the correct key");
                return VLC_EGENERIC;
        }
    }

    /* not all the data is readable because of padding */
    segment->data->i_buffer -= pad;

    return VLC_SUCCESS;
}

static int get_HTTPLiveMetaPlaylist(stream_t *s, vlc_array_t **streams)
{
    stream_sys_t *p_sys = s->p_sys;
    assert(*streams);
    int err = VLC_EGENERIC;

    /* Duplicate HLS stream META information */
    for (int i = 0; i < vlc_array_count(p_sys->hls_stream); i++)
    {
        hls_stream_t *src, *dst;
        src = hls_Get(p_sys->hls_stream, i);
        if (src == NULL)
            return VLC_EGENERIC;

        dst = hls_Copy(src, false);
        if (dst == NULL)
            return VLC_ENOMEM;
        vlc_array_append(*streams, dst);

        /* Download playlist file from server */
        uint8_t *buf = NULL;
        ssize_t len = read_M3U8_from_url(s, dst->url, &buf);
        if (len < 0)
            err = VLC_EGENERIC;
        else
        {
            /* Parse HLS m3u8 content. */
            err = parse_M3U8(s, *streams, buf, len);
            free(buf);
        }
    }
    return err;
}

/* Update hls_old (an existing member of p_sys->hls_stream) to match hls_new
   (which represents a downloaded, perhaps newer version of the same playlist) */
static int hls_UpdatePlaylist(stream_t *s, hls_stream_t *hls_new, hls_stream_t *hls_old, bool *stream_appended)
{
    int count = vlc_array_count(hls_new->segments);

    msg_Dbg(s, "updating hls stream (program-id=%d, bandwidth=%"PRIu64") has %d segments",
             hls_new->id, hls_new->bandwidth, count);

    vlc_mutex_lock(&hls_old->lock);
    for (int n = 0; n < count; n++)
    {
        segment_t *p = segment_GetSegment(hls_new, n);
        if (p == NULL)
        {
            vlc_mutex_unlock(&hls_old->lock);
            return VLC_EGENERIC;
        }

        segment_t *segment = segment_Find(hls_old, p->sequence);
        if (segment)
        {
            vlc_mutex_lock(&segment->lock);

            assert(p->url);
            assert(segment->url);

            /* they should be the same */
            if ((p->sequence != segment->sequence) ||
                (p->duration != segment->duration) ||
                (strcmp(p->url, segment->url) != 0))
            {
                msg_Warn(s, "existing segment found with different content - resetting");
                msg_Warn(s, "- sequence: new=%d, old=%d", p->sequence, segment->sequence);
                msg_Warn(s, "- duration: new=%d, old=%d", p->duration, segment->duration);
                msg_Warn(s, "- file: new=%s", p->url);
                msg_Warn(s, "        old=%s", segment->url);

                /* Resetting content */
                segment->sequence = p->sequence;
                segment->duration = p->duration;
                free(segment->url);
                segment->url = strdup(p->url);
                if ( segment->url == NULL )
                {
                    msg_Err(s, "Failed updating segment %d - skipping it",  p->sequence);
                    segment_Free(p);
                    vlc_mutex_unlock(&segment->lock);
                    continue;
                }
                /* We must free the content, because if the key was not downloaded, content can't be decrypted */
                if ((p->psz_key_path || p->b_key_loaded) &&
                    segment->data)
                {
                    block_Release(segment->data);
                    segment->data = NULL;
                }
                free(segment->psz_key_path);
                segment->psz_key_path = p->psz_key_path ? strdup(p->psz_key_path) : NULL;
                segment_Free(p);
            }
            vlc_mutex_unlock(&segment->lock);
        }
        else
        {
            int last = vlc_array_count(hls_old->segments) - 1;
            segment_t *l = segment_GetSegment(hls_old, last);
            if (l == NULL) {
                vlc_mutex_unlock(&hls_old->lock);
                return VLC_EGENERIC;
            }

            if ((l->sequence + 1) != p->sequence)
            {
                msg_Err(s, "gap in sequence numbers found: new=%d expected %d",
                        p->sequence, l->sequence+1);
            }
            vlc_array_append(hls_old->segments, p);
            msg_Dbg(s, "- segment %d appended", p->sequence);

            // Signal download thread otherwise the segment will not get downloaded
            *stream_appended = true;
        }
    }

    /* update meta information */
    hls_old->sequence = hls_new->sequence;
    hls_old->duration = (hls_new->duration == -1) ? hls_old->duration : hls_new->duration;
    hls_old->b_cache = hls_new->b_cache;
    vlc_mutex_unlock(&hls_old->lock);
    return VLC_SUCCESS;

}

static int hls_ReloadPlaylist(stream_t *s)
{
    stream_sys_t *p_sys = s->p_sys;

    // Flag to indicate if we should signal download thread
    bool stream_appended = false;

    vlc_array_t *hls_streams = vlc_array_new();
    if (hls_streams == NULL)
        return VLC_ENOMEM;

    msg_Dbg(s, "Reloading HLS live meta playlist");

    if (get_HTTPLiveMetaPlaylist(s, &hls_streams) != VLC_SUCCESS)
    {
        /* Free hls streams */
        for (int i = 0; i < vlc_array_count(hls_streams); i++)
        {
            hls_stream_t *hls;
            hls = hls_Get(hls_streams, i);
            if (hls) hls_Free(hls);
        }
        vlc_array_destroy(hls_streams);

        msg_Err(s, "reloading playlist failed");
        return VLC_EGENERIC;
    }

    /* merge playlists */
    int count = vlc_array_count(hls_streams);
    for (int n = 0; n < count; n++)
    {
        hls_stream_t *hls_new = hls_Get(hls_streams, n);
        if (hls_new == NULL)
            continue;

        hls_stream_t *hls_old = hls_Find(p_sys->hls_stream, hls_new);
        if (hls_old == NULL)
        {   /* new hls stream - append */
            vlc_array_append(p_sys->hls_stream, hls_new);
            msg_Dbg(s, "new HLS stream appended (id=%d, bandwidth=%"PRIu64")",
                     hls_new->id, hls_new->bandwidth);

            // New segment available -  signal download thread
            stream_appended = true;
        }
        else if (hls_UpdatePlaylist(s, hls_new, hls_old, &stream_appended) != VLC_SUCCESS)
            msg_Warn(s, "failed updating HLS stream (id=%d, bandwidth=%"PRIu64")",
                     hls_new->id, hls_new->bandwidth);
    }
    vlc_array_destroy(hls_streams);

    // Must signal the download thread otherwise new segments will not be downloaded at all!
    if (stream_appended == true)
    {
        vlc_mutex_lock(&p_sys->download.lock_wait);
        vlc_cond_signal(&p_sys->download.wait);
        vlc_mutex_unlock(&p_sys->download.lock_wait);
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
    uint64_t bw_candidate = 0;

    int count = vlc_array_count(p_sys->hls_stream);
    for (int n = 0; n < count; n++)
    {
        /* Select best bandwidth match */
        hls_stream_t *hls = hls_Get(p_sys->hls_stream, n);
        if (hls == NULL) break;

        /* only consider streams with the same PROGRAM-ID */
        if (hls->id == progid)
        {
            if ((bw >= hls->bandwidth) && (bw_candidate < hls->bandwidth))
            {
                msg_Dbg(s, "candidate %d bandwidth (bits/s) %"PRIu64" >= %"PRIu64,
                         n, bw, hls->bandwidth); /* bits / s */
                bw_candidate = hls->bandwidth;
                candidate = n; /* possible candidate */
            }
        }
    }
    *bandwidth = bw_candidate;
    return candidate;
}

static int hls_DownloadSegmentData(stream_t *s, hls_stream_t *hls, segment_t *segment, int *cur_stream)
{
    stream_sys_t *p_sys = s->p_sys;

    assert(hls);
    assert(segment);

    vlc_mutex_lock(&segment->lock);
    if (segment->data != NULL)
    {
        /* Segment already downloaded */
        vlc_mutex_unlock(&segment->lock);
        return VLC_SUCCESS;
    }

    /* sanity check - can we download this segment on time? */
    if ((p_sys->bandwidth > 0) && (hls->bandwidth > 0))
    {
        uint64_t size = (segment->duration * hls->bandwidth); /* bits */
        int estimated = (int)(size / p_sys->bandwidth);
        if (estimated > segment->duration)
        {
            msg_Warn(s,"downloading segment %d predicted to take %ds, which exceeds its length (%ds)",
                        segment->sequence, estimated, segment->duration);
        }
    }

    mtime_t start = mdate();
    if (hls_Download(s, segment) != VLC_SUCCESS)
    {
        msg_Err(s, "downloading segment %d from stream %d failed",
                    segment->sequence, *cur_stream);
        vlc_mutex_unlock(&segment->lock);
        return VLC_EGENERIC;
    }
    mtime_t duration = mdate() - start;
    if (hls->bandwidth == 0 && segment->duration > 0)
    {
        /* Try to estimate the bandwidth for this stream */
        hls->bandwidth = (uint64_t)(((double)segment->size * 8) / ((double)segment->duration));
    }

    /* If the segment is encrypted, decode it */
    if (hls_DecodeSegmentData(s, hls, segment) != VLC_SUCCESS)
    {
        vlc_mutex_unlock(&segment->lock);
        return VLC_EGENERIC;
    }

    vlc_mutex_unlock(&segment->lock);

    msg_Dbg(s, "downloaded segment %d from stream %d",
                segment->sequence, *cur_stream);

    uint64_t bw = segment->size * 8 * 1000000 / __MAX(1, duration); /* bits / s */
    p_sys->bandwidth = bw;
    if (p_sys->b_meta && (hls->bandwidth != bw))
    {
        int newstream = BandwidthAdaptation(s, hls->id, &bw);

        /* FIXME: we need an average here */
        if ((newstream >= 0) && (newstream != *cur_stream))
        {
            msg_Dbg(s, "detected %s bandwidth (%"PRIu64") stream",
                     (bw >= hls->bandwidth) ? "faster" : "lower", bw);
            *cur_stream = newstream;
        }
    }
    return VLC_SUCCESS;
}

static void* hls_Thread(void *p_this)
{
    stream_t *s = (stream_t *)p_this;
    stream_sys_t *p_sys = s->p_sys;

    int canc = vlc_savecancel();

    while (vlc_object_alive(s))
    {
        hls_stream_t *hls = hls_Get(p_sys->hls_stream, p_sys->download.stream);
        assert(hls);

        /* Sliding window (~60 seconds worth of movie) */
        vlc_mutex_lock(&hls->lock);
        int count = vlc_array_count(hls->segments);
        vlc_mutex_unlock(&hls->lock);

        /* Is there a new segment to process? */
        if ((!p_sys->b_live && (p_sys->playback.segment < (count - 6))) ||
            (p_sys->download.segment >= count))
        {
            /* wait */
            vlc_mutex_lock(&p_sys->download.lock_wait);
            while (((p_sys->download.segment - p_sys->playback.segment > 6) ||
                    (p_sys->download.segment >= count)) &&
                   (p_sys->download.seek == -1))
            {
                vlc_cond_wait(&p_sys->download.wait, &p_sys->download.lock_wait);
                if (p_sys->b_live /*&& (mdate() >= p_sys->playlist.wakeup)*/)
                    break;
                if (!vlc_object_alive(s))
                    break;
            }
            /* */
            if (p_sys->download.seek >= 0)
            {
                p_sys->download.segment = p_sys->download.seek;
                p_sys->download.seek = -1;
            }
            vlc_mutex_unlock(&p_sys->download.lock_wait);
        }

        if (!vlc_object_alive(s)) break;

        vlc_mutex_lock(&hls->lock);
        segment_t *segment = segment_GetSegment(hls, p_sys->download.segment);
        vlc_mutex_unlock(&hls->lock);

        if ((segment != NULL) &&
            (hls_DownloadSegmentData(s, hls, segment, &p_sys->download.stream) != VLC_SUCCESS))
        {
            if (!vlc_object_alive(s)) break;

            if (!p_sys->b_live)
            {
                p_sys->b_error = true;
                break;
            }
        }

        /* download succeeded */
        /* determine next segment to download */
        vlc_mutex_lock(&p_sys->download.lock_wait);
        if (p_sys->download.seek >= 0)
        {
            p_sys->download.segment = p_sys->download.seek;
            p_sys->download.seek = -1;
        }
        else if (p_sys->download.segment < count)
            p_sys->download.segment++;
        vlc_cond_signal(&p_sys->download.wait);
        vlc_mutex_unlock(&p_sys->download.lock_wait);

        // In case of a successful download signal the read thread that data is available
        vlc_mutex_lock(&p_sys->read.lock_wait);
        vlc_cond_signal(&p_sys->read.wait);
        vlc_mutex_unlock(&p_sys->read.lock_wait);
    }

    vlc_restorecancel(canc);
    return NULL;
}

static void* hls_Reload(void *p_this)
{
    stream_t *s = (stream_t *)p_this;
    stream_sys_t *p_sys = s->p_sys;

    assert(p_sys->b_live);

    int canc = vlc_savecancel();

    double wait = 1.0;
    while (vlc_object_alive(s))
    {
        mtime_t now = mdate();
        if (now >= p_sys->playlist.wakeup)
        {
            /* reload the m3u8 if there are less than 2 segments what aren't downloaded */
            if ( ( p_sys->download.segment - p_sys->playback.segment < 2 ) &&
                 ( hls_ReloadPlaylist(s) != VLC_SUCCESS) )
            {
                /* No change in playlist, then backoff */
                p_sys->playlist.tries++;
                if (p_sys->playlist.tries == 1) wait = 0.5;
                else if (p_sys->playlist.tries == 2) wait = 1;
                else if (p_sys->playlist.tries >= 3) wait = 1.5;

                /* Can we afford to backoff? */
                if (p_sys->download.segment - p_sys->playback.segment < 3)
                {
                    p_sys->playlist.tries = 0;
                    wait = 0.5;
                }
            }
            else
            {
                p_sys->playlist.tries = 0;
                wait = 1.0;
            }

            hls_stream_t *hls = hls_Get(p_sys->hls_stream, p_sys->download.stream);
            assert(hls);

            /* determine next time to update playlist */
            p_sys->playlist.last = now;
            p_sys->playlist.wakeup = now + ((mtime_t)(hls->duration * wait)
                                                   * (mtime_t)1000000);
        }

        mwait(p_sys->playlist.wakeup);
    }

    vlc_restorecancel(canc);
    return NULL;
}

static int Prefetch(stream_t *s, int *current)
{
    stream_sys_t *p_sys = s->p_sys;
    int stream = *current;

    hls_stream_t *hls = hls_Get(p_sys->hls_stream, stream);
    if (hls == NULL)
        return VLC_EGENERIC;

    if (vlc_array_count(hls->segments) == 0)
        return VLC_EGENERIC;
    else if (vlc_array_count(hls->segments) == 1 && p_sys->b_live)
        msg_Warn(s, "Only 1 segment available to prefetch in live stream; may stall");

    /* Download ~10s worth of segments of this HLS stream if they exist */
    unsigned segment_amount = (0.5f + 10/hls->duration);
    for (int i = 0; i < __MIN(vlc_array_count(hls->segments), segment_amount); i++)
    {
        segment_t *segment = segment_GetSegment(hls, p_sys->download.segment);
        if (segment == NULL )
            return VLC_EGENERIC;

        /* It is useless to lock the segment here, as Prefetch is called before
           download and playlit thread are started. */
        if (segment->data)
        {
            p_sys->download.segment++;
            continue;
        }

        if (hls_DownloadSegmentData(s, hls, segment, current) != VLC_SUCCESS)
            return VLC_EGENERIC;

        p_sys->download.segment++;

        /* adapt bandwidth? */
        if (*current != stream)
        {
            hls_stream_t *hls = hls_Get(p_sys->hls_stream, *current);
            if (hls == NULL)
                return VLC_EGENERIC;

             stream = *current;
        }
    }

    return VLC_SUCCESS;
}

/****************************************************************************
 *
 ****************************************************************************/
static int hls_Download(stream_t *s, segment_t *segment)
{
    stream_sys_t *p_sys = s->p_sys;
    assert(segment);

    vlc_mutex_lock(&p_sys->lock);
    while (p_sys->paused)
        vlc_cond_wait(&p_sys->wait, &p_sys->lock);
    vlc_mutex_unlock(&p_sys->lock);

    stream_t *p_ts = stream_UrlNew(s, segment->url);
    if (p_ts == NULL)
        return VLC_EGENERIC;

    segment->size = stream_Size(p_ts);
    assert(segment->size > 0);

    segment->data = block_Alloc(segment->size);
    if (segment->data == NULL)
    {
        stream_Delete(p_ts);
        return VLC_ENOMEM;
    }

    assert(segment->data->i_buffer == segment->size);

    ssize_t length = 0, curlen = 0;
    uint64_t size;
    do
    {
        /* NOTE: Beware the size reported for a segment by the HLS server may not
         * be correct, when downloading the segment data. Therefore check the size
         * and enlarge the segment data block if necessary.
         */
        size = stream_Size(p_ts);
        if (size > segment->size)
        {
            msg_Dbg(s, "size changed %"PRIu64, segment->size);
            block_t *p_block = block_Realloc(segment->data, 0, size);
            if (p_block == NULL)
            {
                stream_Delete(p_ts);
                block_Release(segment->data);
                segment->data = NULL;
                return VLC_ENOMEM;
            }
            segment->data = p_block;
            segment->size = size;
            assert(segment->data->i_buffer == segment->size);
            p_block = NULL;
        }
        length = stream_Read(p_ts, segment->data->p_buffer + curlen, segment->size - curlen);
        if (length <= 0)
            break;
        curlen += length;
    } while (vlc_object_alive(s));

    stream_Delete(p_ts);
    return VLC_SUCCESS;
}

/* Read M3U8 file */
static ssize_t read_M3U8_from_stream(stream_t *s, uint8_t **buffer)
{
    int64_t total_bytes = 0;
    int64_t total_allocated = 0;
    uint8_t *p = NULL;

    while (1)
    {
        char buf[4096];
        int64_t bytes;

        bytes = stream_Read(s, buf, sizeof(buf));
        if (bytes == 0)
            break;      /* EOF ? */
        else if (bytes < 0)
        {
            free (p);
            return bytes;
        }

        if ( (total_bytes + bytes + 1) > total_allocated )
        {
            if (total_allocated)
                total_allocated *= 2;
            else
                total_allocated = __MIN((uint64_t)bytes+1, sizeof(buf));

            p = realloc_or_free(p, total_allocated);
            if (p == NULL)
                return VLC_ENOMEM;
        }

        memcpy(p+total_bytes, buf, bytes);
        total_bytes += bytes;
    }

    if (total_allocated == 0)
        return VLC_EGENERIC;

    p[total_bytes] = '\0';
    *buffer = p;

    return total_bytes;
}

static ssize_t read_M3U8_from_url(stream_t *s, const char* psz_url, uint8_t **buffer)
{
    assert(*buffer == NULL);

    /* Construct URL */
    stream_t *p_m3u8 = stream_UrlNew(s, psz_url);
    if (p_m3u8 == NULL)
        return VLC_EGENERIC;

    ssize_t size = read_M3U8_from_stream(p_m3u8, buffer);
    stream_Delete(p_m3u8);

    return size;
}

static char *ReadLine(uint8_t *buffer, uint8_t **pos, const size_t len)
{
    assert(buffer);

    char *line = NULL;
    uint8_t *begin = buffer;
    uint8_t *p = begin;
    uint8_t *end = p + len;

    while (p < end)
    {
        if ((*p == '\r') || (*p == '\n') || (*p == '\0'))
            break;
        p++;
    }

    /* copy line excluding \r \n or \0 */
    line = strndup((char *)begin, p - begin);

    while ((*p == '\r') || (*p == '\n') || (*p == '\0'))
    {
        if (*p == '\0')
        {
            *pos = end;
            break;
        }
        else
        {
            /* next pass start after \r and \n */
            p++;
            *pos = p;
        }
    }

    return line;
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

    /* Initialize crypto bit */
    vlc_gcrypt_init();

    /* */
    s->p_sys = p_sys = calloc(1, sizeof(*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    char *psz_uri = NULL;
    if (asprintf(&psz_uri,"%s://%s", s->psz_access, s->psz_path) < 0)
    {
        free(p_sys);
        return VLC_ENOMEM;
    }
    p_sys->m3u8 = psz_uri;

    char *new_path;
    if (asprintf(&new_path, "%s.ts", s->psz_path) < 0)
    {
        free(p_sys->m3u8);
        free(p_sys);
        return VLC_ENOMEM;
    }
    free(s->psz_path);
    s->psz_path = new_path;

    p_sys->bandwidth = 0;
    p_sys->b_live = true;
    p_sys->b_meta = false;
    p_sys->b_error = false;

    p_sys->hls_stream = vlc_array_new();
    if (p_sys->hls_stream == NULL)
    {
        free(p_sys->m3u8);
        free(p_sys);
        return VLC_ENOMEM;
    }

    /* */
    s->pf_read = Read;
    s->pf_peek = Peek;
    s->pf_control = Control;

    p_sys->paused = false;

    vlc_cond_init(&p_sys->wait);
    vlc_mutex_init(&p_sys->lock);

    /* Parse HLS m3u8 content. */
    uint8_t *buffer = NULL;
    ssize_t len = read_M3U8_from_stream(s->p_source, &buffer);
    if (len < 0)
        goto fail;
    if (parse_M3U8(s, p_sys->hls_stream, buffer, len) != VLC_SUCCESS)
    {
        free(buffer);
        goto fail;
    }
    free(buffer);
    /* HLS standard doesn't provide any guaranty about streams
       being sorted by bandwidth, so we sort them */
    qsort( p_sys->hls_stream->pp_elems, p_sys->hls_stream->i_count,
           sizeof( hls_stream_t* ), &hls_CompareStreams );

    /* Choose first HLS stream to start with */
    int current = p_sys->playback.stream = p_sys->hls_stream->i_count-1;
    p_sys->playback.segment = p_sys->download.segment = ChooseSegment(s, current);

    /* manage encryption key if needed */
    hls_ManageSegmentKeys(s, hls_Get(p_sys->hls_stream, current));

    if (Prefetch(s, &current) != VLC_SUCCESS)
    {
        msg_Err(s, "fetching first segment failed.");
        goto fail;
    }

    p_sys->download.stream = current;
    p_sys->playback.stream = current;
    p_sys->download.seek = -1;

    vlc_mutex_init(&p_sys->download.lock_wait);
    vlc_cond_init(&p_sys->download.wait);

    vlc_mutex_init(&p_sys->read.lock_wait);
    vlc_cond_init(&p_sys->read.wait);

    /* Initialize HLS live stream */
    if (p_sys->b_live)
    {
        hls_stream_t *hls = hls_Get(p_sys->hls_stream, current);
        p_sys->playlist.last = mdate();
        p_sys->playlist.wakeup = p_sys->playlist.last +
                ((mtime_t)hls->duration * UINT64_C(1000000));

        if (vlc_clone(&p_sys->reload, hls_Reload, s, VLC_THREAD_PRIORITY_LOW))
        {
            goto fail_thread;
        }
    }

    if (vlc_clone(&p_sys->thread, hls_Thread, s, VLC_THREAD_PRIORITY_INPUT))
    {
        if (p_sys->b_live)
            vlc_join(p_sys->reload, NULL);
        goto fail_thread;
    }

    return VLC_SUCCESS;

fail_thread:
    vlc_mutex_destroy(&p_sys->download.lock_wait);
    vlc_cond_destroy(&p_sys->download.wait);

    vlc_mutex_destroy(&p_sys->read.lock_wait);
    vlc_cond_destroy(&p_sys->read.wait);

fail:
    /* Free hls streams */
    for (int i = 0; i < vlc_array_count(p_sys->hls_stream); i++)
    {
        hls_stream_t *hls = hls_Get(p_sys->hls_stream, i);
        if (hls) hls_Free(hls);
    }
    vlc_array_destroy(p_sys->hls_stream);

    vlc_mutex_destroy(&p_sys->lock);
    vlc_cond_destroy(&p_sys->wait);

    /* */
    free(p_sys->m3u8);
    free(p_sys);
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

    vlc_mutex_lock(&p_sys->lock);
    p_sys->paused = false;
    vlc_cond_signal(&p_sys->wait);
    vlc_mutex_unlock(&p_sys->lock);

    /* */
    vlc_mutex_lock(&p_sys->download.lock_wait);
    /* negate the condition variable's predicate */
    p_sys->download.segment = p_sys->playback.segment = 0;
    p_sys->download.seek = 0; /* better safe than sorry */
    vlc_cond_signal(&p_sys->download.wait);
    vlc_mutex_unlock(&p_sys->download.lock_wait);

    /* */
    if (p_sys->b_live)
        vlc_join(p_sys->reload, NULL);
    vlc_join(p_sys->thread, NULL);
    vlc_mutex_destroy(&p_sys->download.lock_wait);
    vlc_cond_destroy(&p_sys->download.wait);

    vlc_mutex_destroy(&p_sys->read.lock_wait);
    vlc_cond_destroy(&p_sys->read.wait);

    /* Free hls streams */
    for (int i = 0; i < vlc_array_count(p_sys->hls_stream); i++)
    {
        hls_stream_t *hls = hls_Get(p_sys->hls_stream, i);
        if (hls) hls_Free(hls);
    }
    vlc_array_destroy(p_sys->hls_stream);

    /* */

    vlc_mutex_destroy(&p_sys->lock);
    vlc_cond_destroy(&p_sys->wait);

    free(p_sys->m3u8);
    if (p_sys->peeked)
        block_Release (p_sys->peeked);
    free(p_sys);
}

/****************************************************************************
 * Stream filters functions
 ****************************************************************************/
static segment_t *GetSegment(stream_t *s)
{
    stream_sys_t *p_sys = s->p_sys;
    segment_t *segment = NULL;

    /* Is this segment of the current HLS stream ready? */
    hls_stream_t *hls = hls_Get(p_sys->hls_stream, p_sys->playback.stream);
    if (hls != NULL)
    {
        vlc_mutex_lock(&hls->lock);
        segment = segment_GetSegment(hls, p_sys->playback.segment);
        if (segment != NULL)
        {
            vlc_mutex_lock(&segment->lock);
            /* This segment is ready? */
            if (segment->data != NULL)
            {
                vlc_mutex_unlock(&segment->lock);
                p_sys->b_cache = hls->b_cache;
                vlc_mutex_unlock(&hls->lock);
                goto check;
            }
            vlc_mutex_unlock(&segment->lock);
        }
        vlc_mutex_unlock(&hls->lock);
    }

    /* Was the HLS stream changed to another bitrate? */
    segment = NULL;
    for (int i_stream = 0; i_stream < vlc_array_count(p_sys->hls_stream); i_stream++)
    {
        /* Is the next segment ready */
        hls_stream_t *hls = hls_Get(p_sys->hls_stream, i_stream);
        if (hls == NULL)
            return NULL;

        vlc_mutex_lock(&hls->lock);
        segment = segment_GetSegment(hls, p_sys->playback.segment);
        if (segment == NULL)
        {
            vlc_mutex_unlock(&hls->lock);
            break;
        }

        vlc_mutex_lock(&p_sys->download.lock_wait);
        int i_segment = p_sys->download.segment;
        vlc_mutex_unlock(&p_sys->download.lock_wait);

        vlc_mutex_lock(&segment->lock);
        /* This segment is ready? */
        if ((segment->data != NULL) &&
            (p_sys->playback.segment < i_segment))
        {
            p_sys->playback.stream = i_stream;
            p_sys->b_cache = hls->b_cache;
            vlc_mutex_unlock(&segment->lock);
            vlc_mutex_unlock(&hls->lock);
            goto check;
        }
        vlc_mutex_unlock(&segment->lock);
        vlc_mutex_unlock(&hls->lock);

        if (!p_sys->b_meta)
            break;
    }
    /* */
    return NULL;

check:
    /* sanity check */
    assert(segment->data);
    if (segment->data->i_buffer == 0)
    {
        vlc_mutex_lock(&hls->lock);
        int count = vlc_array_count(hls->segments);
        vlc_mutex_unlock(&hls->lock);

        if ((p_sys->download.segment - p_sys->playback.segment == 0) &&
            ((count != p_sys->download.segment) || p_sys->b_live))
            msg_Err(s, "playback will stall");
        else if ((p_sys->download.segment - p_sys->playback.segment < 3) &&
                 ((count != p_sys->download.segment) || p_sys->b_live))
            msg_Warn(s, "playback in danger of stalling");
    }
    return segment;
}

static int segment_RestorePos(segment_t *segment)
{
    if (segment->data)
    {
        uint64_t size = segment->size - segment->data->i_buffer;
        if (size > 0)
        {
            segment->data->i_buffer += size;
            segment->data->p_buffer -= size;
        }
    }
    return VLC_SUCCESS;
}

/* p_read might be NULL if caller wants to skip data */
static ssize_t hls_Read(stream_t *s, uint8_t *p_read, unsigned int i_read)
{
    stream_sys_t *p_sys = s->p_sys;
    ssize_t used = 0;

    do
    {
        /* Determine next segment to read. If this is a meta playlist and
         * bandwidth conditions changed, then the stream might have switched
         * to another bandwidth. */
        segment_t *segment = GetSegment(s);
        if (segment == NULL)
            break;

        vlc_mutex_lock(&segment->lock);
        if (segment->data->i_buffer == 0)
        {
            if (!p_sys->b_cache || p_sys->b_live)
            {
                block_Release(segment->data);
                segment->data = NULL;
            }
            else
                segment_RestorePos(segment);

            vlc_mutex_unlock(&segment->lock);

            /* signal download thread */
            vlc_mutex_lock(&p_sys->download.lock_wait);
            p_sys->playback.segment++;
            vlc_cond_signal(&p_sys->download.wait);
            vlc_mutex_unlock(&p_sys->download.lock_wait);
            continue;
        }

        if (segment->size == segment->data->i_buffer)
            msg_Dbg(s, "playing segment %d from stream %d",
                     segment->sequence, p_sys->playback.stream);

        ssize_t len = -1;
        if (i_read <= segment->data->i_buffer)
            len = i_read;
        else if (i_read > segment->data->i_buffer)
            len = segment->data->i_buffer;

        if (len > 0)
        {
            if (p_read) /* if NULL, then caller skips data */
                memcpy(p_read + used, segment->data->p_buffer, len);
            segment->data->i_buffer -= len;
            segment->data->p_buffer += len;
            used += len;
            i_read -= len;
        }
        vlc_mutex_unlock(&segment->lock);

    } while (i_read > 0);

    return used;
}

static int Read(stream_t *s, void *buffer, unsigned int i_read)
{
    stream_sys_t *p_sys = s->p_sys;
    ssize_t length = 0;

    assert(p_sys->hls_stream);

    while (length == 0)
    {
        // In case an error occurred or the stream was closed return 0
        if (p_sys->b_error || !vlc_object_alive(s))
            return 0;

        // Lock the mutex before trying to read to avoid a race condition with the download thread
        vlc_mutex_lock(&p_sys->read.lock_wait);

        /* NOTE: buffer might be NULL if caller wants to skip data */
        length = hls_Read(s, (uint8_t*) buffer, i_read);

        // An error has occurred in hls_Read
        if (length < 0)
        {
            vlc_mutex_unlock(&p_sys->read.lock_wait);

            return 0;
        }

        // There is no data available yet for the demuxer so we need to wait until reload and
        // download operation are over.
        // Download thread will signal once download is finished.
        // A timed wait is used to avoid deadlock in case data never arrives since the thread
        // running this read operation is also responsible for closing the stream
        if (length == 0)
        {
            mtime_t start = mdate();

            // Wait for 10 seconds
            mtime_t timeout_limit = start + (10 * UINT64_C(1000000));

            int res = vlc_cond_timedwait(&p_sys->read.wait, &p_sys->read.lock_wait, timeout_limit);

            // Error - reached a timeout of 10 seconds without data arriving - kill the stream
            if (res == ETIMEDOUT)
            {
                msg_Warn(s, "timeout limit reached!");

                vlc_mutex_unlock(&p_sys->read.lock_wait);

                return 0;
            }
            else if (res == EINVAL)
                return 0; // Error - lock is not locked so we can just return
        }

        vlc_mutex_unlock(&p_sys->read.lock_wait);
    }

    p_sys->playback.offset += length;
    return length;
}

static int Peek(stream_t *s, const uint8_t **pp_peek, unsigned int i_peek)
{
    stream_sys_t *p_sys = s->p_sys;
    segment_t *segment;
    unsigned int len = i_peek;

    segment = GetSegment(s);
    if (segment == NULL)
    {
        msg_Err(s, "segment %d should have been available (stream %d)",
                p_sys->playback.segment, p_sys->playback.stream);
        return 0; /* eof? */
    }

    vlc_mutex_lock(&segment->lock);

    size_t i_buff = segment->data->i_buffer;
    uint8_t *p_buff = segment->data->p_buffer;

    if ( likely(i_peek < i_buff))
    {
        *pp_peek = p_buff;
        vlc_mutex_unlock(&segment->lock);
        return i_peek;
    }

    else /* This will seldom be run */
    {
        /* remember segment to read */
        int peek_segment = p_sys->playback.segment;
        size_t curlen = 0;
        segment_t *nsegment;
        p_sys->playback.segment++;
        block_t *peeked = p_sys->peeked;

        if (peeked == NULL)
            peeked = block_Alloc (i_peek);
        else if (peeked->i_buffer < i_peek)
            peeked = block_Realloc (peeked, 0, i_peek);
        if (peeked == NULL)
        {
            vlc_mutex_unlock(&segment->lock);
            return 0;
        }
        p_sys->peeked = peeked;

        memcpy(peeked->p_buffer, p_buff, i_buff);
        curlen = i_buff;
        len -= i_buff;
        vlc_mutex_unlock(&segment->lock);

        i_buff = peeked->i_buffer;
        p_buff = peeked->p_buffer;
        *pp_peek = p_buff;

        while (curlen < i_peek)
        {
            nsegment = GetSegment(s);
            if (nsegment == NULL)
            {
                msg_Err(s, "segment %d should have been available (stream %d)",
                        p_sys->playback.segment, p_sys->playback.stream);
                /* restore segment to read */
                p_sys->playback.segment = peek_segment;
                return curlen; /* eof? */
            }

            vlc_mutex_lock(&nsegment->lock);

            if (len < nsegment->data->i_buffer)
            {
                memcpy(p_buff + curlen, nsegment->data->p_buffer, len);
                curlen += len;
            }
            else
            {
                size_t i_nbuff = nsegment->data->i_buffer;
                memcpy(p_buff + curlen, nsegment->data->p_buffer, i_nbuff);
                curlen += i_nbuff;
                len -= i_nbuff;

                p_sys->playback.segment++;
            }

            vlc_mutex_unlock(&nsegment->lock);
        }

        /* restore segment to read */
        p_sys->playback.segment = peek_segment;
        return curlen;
    }
}

static bool hls_MaySeek(stream_t *s)
{
    stream_sys_t *p_sys = s->p_sys;

    if (p_sys->hls_stream == NULL)
        return false;

    hls_stream_t *hls = hls_Get(p_sys->hls_stream, p_sys->playback.stream);
    if (hls == NULL) return false;

    if (p_sys->b_live)
    {
        vlc_mutex_lock(&hls->lock);
        int count = vlc_array_count(hls->segments);
        vlc_mutex_unlock(&hls->lock);

        vlc_mutex_lock(&p_sys->download.lock_wait);
        bool may_seek = (p_sys->download.segment < (count - 2));
        vlc_mutex_unlock(&p_sys->download.lock_wait);
        return may_seek;
    }
    return true;
}

static uint64_t GetStreamSize(stream_t *s)
{
    stream_sys_t *p_sys = s->p_sys;

    if (p_sys->b_live)
        return 0;

    hls_stream_t *hls = hls_Get(p_sys->hls_stream, p_sys->playback.stream);
    if (hls == NULL) return 0;

    vlc_mutex_lock(&hls->lock);
    if (hls->size == 0)
        hls->size = hls_GetStreamSize(hls);
    uint64_t size = hls->size;
    vlc_mutex_unlock(&hls->lock);

    return size;
}

static int segment_Seek(stream_t *s, const uint64_t pos)
{
    stream_sys_t *p_sys = s->p_sys;

    hls_stream_t *hls = hls_Get(p_sys->hls_stream, p_sys->playback.stream);
    if (hls == NULL)
        return VLC_EGENERIC;

    vlc_mutex_lock(&hls->lock);

    bool b_found = false;
    uint64_t length = 0;
    uint64_t size = hls->size;
    int count = vlc_array_count(hls->segments);

    segment_t *currentSegment = segment_GetSegment(hls, p_sys->playback.segment);
    if (currentSegment == NULL)
    {
        vlc_mutex_unlock(&hls->lock);
        return VLC_EGENERIC;
    }

    for (int n = 0; n < count; n++)
    {
        segment_t *segment = segment_GetSegment(hls, n);
        if (segment == NULL)
        {
            vlc_mutex_unlock(&hls->lock);
            return VLC_EGENERIC;
        }

        vlc_mutex_lock(&segment->lock);
        length += segment->duration * (hls->bandwidth/8);
        vlc_mutex_unlock(&segment->lock);

        if (pos <= length)
        {
            if (count - n >= 3)
            {
                p_sys->playback.segment = n;
                b_found = true;
                break;
            }
            /* Do not search in last 3 segments */
            vlc_mutex_unlock(&hls->lock);
            return VLC_EGENERIC;
        }
    }

    /* */
    if (!b_found && (pos >= size))
    {
        p_sys->playback.segment = count - 1;
        b_found = true;
    }

    /* */
    if (b_found)
    {

        /* restore current segment to start position */
        vlc_mutex_lock(&currentSegment->lock);
        segment_RestorePos(currentSegment);
        vlc_mutex_unlock(&currentSegment->lock);

        /* restore seeked segment to start position */
        segment_t *segment = segment_GetSegment(hls, p_sys->playback.segment);
        if (segment == NULL)
        {
            vlc_mutex_unlock(&hls->lock);
            return VLC_EGENERIC;
        }

        vlc_mutex_lock(&segment->lock);
        segment_RestorePos(segment);
        vlc_mutex_unlock(&segment->lock);

        /* start download at current playback segment */
        vlc_mutex_unlock(&hls->lock);

        /* Wake up download thread */
        vlc_mutex_lock(&p_sys->download.lock_wait);
        p_sys->download.seek = p_sys->playback.segment;
        vlc_cond_signal(&p_sys->download.wait);

        /* Wait for download to be finished */
        msg_Dbg(s, "seek to segment %d", p_sys->playback.segment);
        while ((p_sys->download.seek != -1) ||
           ((p_sys->download.segment - p_sys->playback.segment < 3) &&
                (p_sys->download.segment < count)))
        {
            vlc_cond_wait(&p_sys->download.wait, &p_sys->download.lock_wait);
            if (!vlc_object_alive(s) || s->b_error) break;
        }
        vlc_mutex_unlock(&p_sys->download.lock_wait);

        return VLC_SUCCESS;
    }
    vlc_mutex_unlock(&hls->lock);

    return b_found ? VLC_SUCCESS : VLC_EGENERIC;
}

static int Control(stream_t *s, int i_query, va_list args)
{
    stream_sys_t *p_sys = s->p_sys;

    switch (i_query)
    {
        case STREAM_CAN_SEEK:
            *(va_arg (args, bool *)) = hls_MaySeek(s);
            break;
        case STREAM_CAN_CONTROL_PACE:
        case STREAM_CAN_PAUSE:
            *(va_arg (args, bool *)) = true;
            break;
        case STREAM_CAN_FASTSEEK:
            *(va_arg (args, bool *)) = false;
            break;
        case STREAM_GET_POSITION:
            *(va_arg (args, uint64_t *)) = p_sys->playback.offset;
            break;
        case STREAM_SET_PAUSE_STATE:
        {
            bool paused = va_arg (args, unsigned);

            vlc_mutex_lock(&p_sys->lock);
            p_sys->paused = paused;
            vlc_cond_signal(&p_sys->wait);
            vlc_mutex_unlock(&p_sys->lock);
            break;
        }
        case STREAM_SET_POSITION:
            if (hls_MaySeek(s))
            {
                uint64_t pos = (uint64_t)va_arg(args, uint64_t);
                if (segment_Seek(s, pos) == VLC_SUCCESS)
                {
                    p_sys->playback.offset = pos;
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
