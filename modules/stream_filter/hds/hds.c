/*****************************************************************************
 * hds.c: Http Dynamic Streaming (HDS) stream filter
 *****************************************************************************
 *
 * Author: Jonathan Thambidurai <jonathan _AT_ fastly _DOT_ com>
 * Heavily inspired by SMooth module of Frédéric Yhuel <fyhuel _AT_ viotech _DOT_ net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>
#include <vlc_strings.h>            /* b64_decode */
#include <vlc_xml.h>
#include <vlc_charset.h>            /* FromCharset */

typedef struct chunk_s
{
    int64_t     duration;   /* chunk duration in afrt timescale units */
    uint64_t    timestamp;
    uint32_t    frag_num;
    uint32_t    seg_num;
    uint32_t    frun_entry; /* Used to speed things up in vod situations */

    uint32_t    data_len;

    uint32_t    mdat_pos;   /* position in the mdat */
    uint32_t    mdat_len;

    void        *next;

    uint8_t     *mdat_data;
    uint8_t     *data;
    bool        failed;
    bool        eof;
} chunk_t;

typedef struct segment_run_s
{
    uint32_t first_segment;
    uint32_t fragments_per_segment;
} segment_run_t;

typedef struct fragment_run_s
{
    uint32_t fragment_number_start;
    uint32_t fragment_duration;
    uint64_t fragment_timestamp;
    uint8_t  discont;
} fragment_run_t;

typedef struct hds_stream_s
{
    /* linked-list of chunks */
    chunk_t        *chunks_head;
    chunk_t        *chunks_livereadpos;
    chunk_t        *chunks_downloadpos;

    char*          quality_segment_modifier;

    /* we can make this configurable */
    uint64_t       download_leadtime;

    /* in timescale units */
    uint32_t       afrt_timescale;

    /* these two values come from the abst */
    uint32_t       timescale;
    uint64_t       live_current_time;

    /* kilobits per second */
    uint32_t       bitrate;

    vlc_mutex_t    abst_lock;

    vlc_mutex_t    dl_lock;
    vlc_cond_t     dl_cond;

    /* can be left as null */
    char*          abst_url;

    /* these come from the manifest media section  */
    char*          url;
    uint8_t*       metadata;
    size_t         metadata_len;

    /* this comes from the bootstrap info */
    char*          movie_id;

#define MAX_HDS_SERVERS 10
    char*          server_entries[MAX_HDS_SERVERS];
    uint8_t        server_entry_count;

#define MAX_HDS_SEGMENT_RUNS 256
    segment_run_t  segment_runs[MAX_HDS_SEGMENT_RUNS];
    uint8_t        segment_run_count;

#define MAX_HDS_FRAGMENT_RUNS 10000
    fragment_run_t fragment_runs[MAX_HDS_FRAGMENT_RUNS];
    uint32_t       fragment_run_count;
} hds_stream_t;

/* this is effectively just a sanity check  mechanism */
#define MAX_REQUEST_SIZE (50*1024*1024)

#define BITRATE_AS_BYTES_PER_SECOND 1024/8

struct stream_sys_t
{
    char         *base_url;    /* URL common part for chunks */
    vlc_thread_t live_thread;
    vlc_thread_t dl_thread;

    /* we pend on peek until some number of segments arrives; otherwise
     * the downstream system dies in case of playback */
    uint64_t     chunk_count;

    vlc_array_t  hds_streams; /* available streams */

    /* Buffer that holds the very first bytes of the stream: the FLV
     * file header and a possible metadata packet.
     */
    uint8_t      *flv_header;
    size_t       flv_header_len;
    size_t       flv_header_bytes_sent;
    uint64_t     duration_seconds;

    bool         live;
    bool         closed;
};

typedef struct _bootstrap_info {
    uint8_t* data;
    char*    id;
    char*    url;
    char*    profile;
    int      data_len;
} bootstrap_info;

typedef struct _media_info {
    char*    stream_id;
    char*    media_url;
    char*    bootstrap_id;
    uint8_t* metadata;
    size_t   metadata_len;
    uint32_t bitrate;
} media_info;

#define MAX_BOOTSTRAP_INFO 10
#define MAX_MEDIA_ELEMENTS 10
#define MAX_XML_DEPTH 256

typedef struct _manifest {
    char* element_stack[MAX_XML_DEPTH];
    bootstrap_info bootstraps[MAX_BOOTSTRAP_INFO];
    media_info medias[MAX_MEDIA_ELEMENTS];
    xml_reader_t *vlc_reader;
} manifest_t;

static unsigned char flv_header_bytes[] = {
        'F',
        'L',
        'V',
        0x1, //version
        0x5, //indicates audio and video
        0x0, // length
        0x0, // length
        0x0, // length
        0x9, // length of header
        0x0,
        0x0,
        0x0,
        0x0, // initial "trailer"
};

static unsigned char amf_object_end[] = { 0x0, 0x0, 0x9 };

#define FLV_FILE_HEADER_LEN sizeof(flv_header_bytes)
#define FLV_TAG_HEADER_LEN 15
#define SCRIPT_TAG 0x12

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_STREAM_FILTER )
    set_description( N_("HTTP Dynamic Streaming") )
    set_shortname( "Dynamic Streaming")
    set_capability( "stream_filter", 30 )
    set_callbacks( Open, Close )
vlc_module_end()

static ssize_t Read( stream_t *, void *, size_t );
static int   Control( stream_t *, int , va_list );

static inline bool isFQUrl( const char* url )
{
    return ( NULL != strcasestr( url, "https://") ||
             NULL != strcasestr( url, "http://" ) );
}

static bool isHDS( stream_t *s )
{
    const uint8_t *peek;
    int i_size = vlc_stream_Peek( s->p_source, &peek, 200 );
    if( i_size < 200 )
        return false;

    char *str;

    if( !memcmp( peek, "\xFF\xFE", 2 ) )
    {
        str = FromCharset( "UTF-16LE", peek, i_size );
    }
    else if( !memcmp( peek, "\xFE\xFF", 2 ) )
    {
        str = FromCharset( "UTF-16BE", peek, i_size );
    }
    else
        str = strndup( (const char *)peek, i_size );

    if( str == NULL )
        return false;

    bool ret = strstr( str, "<manifest" ) != NULL;
    free( str );
    return ret;
}

static uint64_t get_stream_size( stream_t* s )
{
    stream_sys_t *p_sys = s->p_sys;

    if ( p_sys->live )
        return 0;

    if ( vlc_array_count( &p_sys->hds_streams ) == 0 )
        return 0;

    hds_stream_t* hds_stream = p_sys->hds_streams.pp_elems[0];

    if ( hds_stream->bitrate == 0 )
        return 0;

    return p_sys->flv_header_len + p_sys->duration_seconds *
        hds_stream->bitrate * BITRATE_AS_BYTES_PER_SECOND;
}

static uint8_t* parse_asrt( vlc_object_t* p_this,
                        hds_stream_t* s,
                        uint8_t* data,
                        uint8_t* data_end )
{
    uint8_t* data_p = data;

    uint32_t asrt_len = 0;
    asrt_len = U32_AT( data_p );
    if( asrt_len > data_end - data ||
        data_end - data <  14 )
    {
        msg_Err( p_this, "Not enough asrt data (%"PRIu32", %tu)", asrt_len,
                 data_end - data );
        return NULL;
    }

    data_p += sizeof(asrt_len);

    if( 0 != memcmp( "asrt", data_p, 4 ) )
    {
        msg_Err( p_this, "Cant find asrt in bootstrap" );
        return NULL;
    }
    data_p += 4;

    /* ignore flags and versions (we don't handle multiple updates) */
    data_p += 4;

    uint8_t quality_entry_count = *data_p;
    bool quality_found = false;
    data_p++;

    if( ! s->quality_segment_modifier )
    {
        quality_found = true;
    }

    while( quality_entry_count-- > 0 )
    {
        char* str_start = (char*) data_p;
        data_p = memchr( data_p, '\0', data_end - data_p );
        if( ! data_p )
        {
            msg_Err( p_this, "Couldn't find quality entry string in asrt" );
            return NULL;
        }
        data_p++;

        if( ! quality_found )
        {
            if( ! strncmp( str_start, s->quality_segment_modifier,
                           strlen(s->quality_segment_modifier) ) )
            {
                quality_found = true;
            }
        }

        if( data_p >= data_end )
        {
            msg_Err( p_this, "Premature end of asrt in quality entries" );
            return NULL;
        }
    }

    if( data_end - data_p < 4 )
    {
        msg_Err( p_this, "Premature end of asrt after quality entries" );
        return NULL;
    }

    uint32_t segment_run_entry_count = U32_AT( data_p );
    data_p += sizeof(segment_run_entry_count);

    if( data_end - data_p < 8 * segment_run_entry_count )
    {
        msg_Err( p_this, "Not enough data in asrt for segment run entries" );
        return NULL;
    }

    if( segment_run_entry_count >= MAX_HDS_SEGMENT_RUNS )
    {
        msg_Err( p_this, "Too many segment runs" );
        return NULL;
    }

    while( segment_run_entry_count-- > 0 )
    {
        if( quality_found )
        {
            s->segment_runs[s->segment_run_count].first_segment = U32_AT(data_p);
        }
        data_p+=4;

        if( quality_found )
        {
            s->segment_runs[s->segment_run_count].fragments_per_segment = U32_AT(data_p);
        }
        data_p+=4;

        s->segment_run_count++;
    }

    return data_p;
}

static uint8_t* parse_afrt( vlc_object_t* p_this,
                        hds_stream_t* s,
                        uint8_t* data,
                        uint8_t* data_end )
{
    uint8_t* data_p = data;

    uint32_t afrt_len = U32_AT( data_p );
    if( afrt_len > data_end - data ||
        data_end - data <  9 )
    {
        msg_Err( p_this, "Not enough afrt data %u, %td", afrt_len,
                 data_end - data );
        return NULL;
    }
    data_p += sizeof(afrt_len);

    if( 0 != memcmp( data_p, "afrt", 4 ) )
    {
        msg_Err( p_this, "Cant find afrt in bootstrap" );
        return NULL;
    }
    data_p += 4;

    /* ignore flags and versions (we don't handle multiple updates) */
    data_p += 4;

    if( data_end - data_p < 9 )
    {
        msg_Err( p_this, "afrt is too short" );
        return NULL;
    }

    s->afrt_timescale = U32_AT( data_p );
    data_p += 4;

    bool quality_found = false;
    if( ! s->quality_segment_modifier )
    {
        quality_found = true;
    }

    uint32_t quality_entry_count = *data_p;
    data_p++;
    while( quality_entry_count-- > 0 )
    {
        char* str_start = (char*)data_p;
        data_p = memchr( data_p, '\0', data_end - data_p );
        if( ! data_p )
        {
            msg_Err( p_this, "Couldn't find quality entry string in afrt" );
            return NULL;
        }
        data_p++;

        if( ! quality_found )
        {
            if( ! strncmp( str_start, s->quality_segment_modifier,
                           strlen(s->quality_segment_modifier) ) )
            {
                quality_found = true;
            }
        }
    }

    if( data_end - data_p < 5 )
    {
        msg_Err( p_this, "No more space in afrt after quality entries" );
        return NULL;
    }

    uint32_t fragment_run_entry_count = U32_AT( data_p );
    data_p += sizeof(uint32_t);

    while(fragment_run_entry_count-- > 0)
    {
        if( data_end - data_p < 16 )
        {
            msg_Err( p_this, "Not enough data in afrt" );
            return NULL;
        }

        if( s->fragment_run_count >= MAX_HDS_FRAGMENT_RUNS )
        {
            msg_Err( p_this, "Too many fragment runs, exiting" );
            return NULL;
        }

        s->fragment_runs[s->fragment_run_count].fragment_number_start = U32_AT(data_p);
        data_p += 4;

        s->fragment_runs[s->fragment_run_count].fragment_timestamp = U64_AT( data_p );
        data_p += 8;

        s->fragment_runs[s->fragment_run_count].fragment_duration = U32_AT( data_p );
        data_p += 4;

        s->fragment_runs[s->fragment_run_count].discont = 0;
        if( s->fragment_runs[s->fragment_run_count].fragment_duration == 0 )
        {
            /* discontinuity flag */
            s->fragment_runs[s->fragment_run_count].discont = *(data_p++);
        }

        s->fragment_run_count++;
    }

    if ( s->fragment_runs[s->fragment_run_count-1].fragment_number_start == 0 &&
         s->fragment_runs[s->fragment_run_count-1].fragment_timestamp == 0 &&
         s->fragment_runs[s->fragment_run_count-1].fragment_duration == 0 &&
         s->fragment_runs[s->fragment_run_count-1].discont == 0 )
    {
        /* ignore sentinel value */
        s->fragment_run_count--;
    }

    return data_p;
}

static inline chunk_t* chunk_new()
{
    chunk_t* chunk = calloc(1, sizeof(chunk_t));
    return chunk;
}

static void chunk_free( chunk_t * chunk )
{
    FREENULL( chunk->data );
    free( chunk );
}

static void parse_BootstrapData( vlc_object_t* p_this,
                                 hds_stream_t * s,
                                 uint8_t* data,
                                 uint8_t* data_end )
{
    uint8_t* data_p = data;

    uint32_t abst_len = U32_AT( data_p );
    if( abst_len > data_end - data
        || data_end - data < 29 /* min size of data */ )
    {
        msg_Warn( p_this, "Not enough bootstrap data" );
        return;
    }
    data_p += sizeof(abst_len);

    if( 0 != memcmp( data_p, "abst", 4 ) )
    {
        msg_Warn( p_this, "Cant find abst in bootstrap" );
        return;
    }
    data_p += 4;

    /* version, flags*/
    data_p += 4;

    /* we ignore the version */
    data_p += 4;

    /* some flags we don't care about here because they are
     * in the manifest
     */
    data_p += 1;

    /* timescale */
    s->timescale = U32_AT( data_p );
    data_p += sizeof(s->timescale);

    s->live_current_time = U64_AT( data_p );
    data_p += sizeof(s->live_current_time);

    /* smtpe time code offset */
    data_p += 8;

    s->movie_id = strndup( (char*)data_p, data_end - data_p );
    data_p += ( strlen( s->movie_id ) + 1 );

    if( data_end - data_p < 4 ) {
        msg_Warn( p_this, "Not enough bootstrap after Movie Identifier" );
        return;
    }

    uint8_t server_entry_count = 0;
    server_entry_count = (uint8_t) *data_p;
    data_p++;

    s->server_entry_count = 0;
    while( server_entry_count-- > 0 )
    {
        if( s->server_entry_count < MAX_HDS_SERVERS )
        {
            s->server_entries[s->server_entry_count++] = strndup( (char*)data_p,
                                                                  data_end - data_p );
            data_p += strlen( s->server_entries[s->server_entry_count-1] ) + 1;
        }
        else
        {
            msg_Warn( p_this, "Too many servers" );
            data_p = memchr( data_p, '\0', data_end - data_p );
            if( ! data_p )
            {
                msg_Err( p_this, "Couldn't find server entry" );
                return;
            }
            data_p++;
        }

        if( data_p >= data_end )
        {
            msg_Warn( p_this, "Premature end of bootstrap info while reading servers" );
            return;
        }
    }

    if( data_end - data_p < 3 ) {
        msg_Warn( p_this, "Not enough bootstrap after Servers" );
        return;
    }

    s->quality_segment_modifier = NULL;

    uint8_t quality_entry_count = *data_p;
    data_p++;

    if( quality_entry_count > 1 )
    {
        msg_Err( p_this, "I don't know what to do with multiple quality levels in the bootstrap - shouldn't this be handled at the manifest level?" );
        return;
    }

    s->quality_segment_modifier = NULL;
    while( quality_entry_count-- > 0 )
    {
        if( s->quality_segment_modifier )
        {
            s->quality_segment_modifier = strndup( (char*)data_p, data_end - data_p );
        }
        data_p += strnlen( (char*)data_p, data_end - data_p ) + 1;
    }

    if( data_end - data_p < 2 ) {
        msg_Warn( p_this, "Not enough bootstrap after quality entries" );
        return;
    }

    /* ignoring "DrmData" */
    data_p = memchr( data_p, '\0', data_end - data_p );
    if( ! data_p )
    {
        msg_Err( p_this, "Couldn't find DRM Data" );
        return;
    }
    data_p++;

    if( data_end - data_p < 2 ) {
        msg_Warn( p_this, "Not enough bootstrap after drm data" );
        return;
    }

    /* ignoring "metadata" */
    data_p = memchr( data_p, '\0', data_end - data_p );
    if( ! data_p )
    {
        msg_Err( p_this, "Couldn't find metadata");
        return;
    }
    data_p++;

    if( data_end - data_p < 2 ) {
        msg_Warn( p_this, "Not enough bootstrap after drm data" );
        return;
    }

    uint8_t asrt_count = *data_p;
    data_p++;

    s->segment_run_count = 0;
    while( asrt_count-- > 0 &&
           data_end > data_p &&
           (data_p = parse_asrt( p_this, s, data_p, data_end )) );

    if( ! data_p )
    {
        msg_Warn( p_this, "Couldn't find afrt data" );
        return;
    }

    uint8_t afrt_count = *data_p;
    data_p++;

    s->fragment_run_count = 0;
    while( afrt_count-- > 0 &&
           data_end > data_p &&
           (data_p = parse_afrt( p_this, s, data_p, data_end )) );
}

/* this only works with ANSI characters - this is ok
   for the bootstrapinfo field which this function is
   exclusively used for since it is merely a base64 encoding
*/
static bool is_whitespace( char c )
{
    return ( ' '  == c ||
             '\t' == c ||
             '\n' == c ||
             '\v' == c ||
             '\f' == c ||
             '\r' == c );
}

/* see above note for is_whitespace */
static void whitespace_substr( char** start,
                               char** end )
{
    while( is_whitespace( **start ) && *start != *end ) {
        (*start)++;
    }

    if( *start == *end )
        return;

    while( is_whitespace(*(*end - 1) ) ) {
        (*end)--;
    }
}

/* returns length (could be zero, indicating all of remaining data) */
/* ptr is to start of data, right after 'mdat' string */
static uint32_t find_chunk_mdat( vlc_object_t* p_this,
                                 uint8_t* chunkdata, uint8_t* chunkdata_end,
                                 uint8_t** mdatptr )
{
    uint8_t* boxname = NULL;
    uint8_t* boxdata = NULL;
    uint64_t boxsize = 0;

    do {
        if( chunkdata_end < chunkdata ||
            chunkdata_end - chunkdata < 8 )
        {
            msg_Err( p_this, "Couldn't find mdat in box 1!" );
            *mdatptr = 0;
            return 0;
        }

        boxsize = (uint64_t)U32_AT( chunkdata );
        chunkdata += 4;

        boxname = chunkdata;
        chunkdata += 4;

        if( boxsize == 1 )
        {
            if( chunkdata_end - chunkdata >= 12 )
            {
                boxsize =  U64_AT(chunkdata);
                chunkdata += 8;
            }
            else
            {
                msg_Err( p_this, "Couldn't find mdat in box 2!");
                *mdatptr = 0;
                return 0;
            }
            boxdata = chunkdata;
            chunkdata += (boxsize - 16);
        }
        else
        {
            boxdata = chunkdata;
            chunkdata += (boxsize - 8);
        }
    } while ( 0 != memcmp( boxname, "mdat", 4 ) );

    *mdatptr = boxdata;

    return chunkdata_end - ((uint8_t*)boxdata);
}

/* returns data ptr if valid (updating the chunk itself
   tells the reader that the chunk is safe to read, which is not yet correct)*/
static uint8_t* download_chunk( stream_t *s,
                                stream_sys_t* sys,
                                hds_stream_t* stream, chunk_t* chunk )
{
    const char* quality = "";
    char* server_base = sys->base_url;
    if( stream->server_entry_count > 0 &&
        strlen(stream->server_entries[0]) > 0 )
    {
        server_base = stream->server_entries[0];
    }

    if( stream->quality_segment_modifier )
    {
        quality = stream->quality_segment_modifier;
    }

    const char* movie_id = "";
    if( stream->url && strlen(stream->url) > 0 )
    {
        if( isFQUrl( stream->url ) )
        {
            server_base = stream->url;
        }
        else
        {
            movie_id = stream->url;
        }
    }

    char* fragment_url;
    if( 0 > asprintf( &fragment_url, "%s/%s%sSeg%u-Frag%u",
              server_base,
              movie_id,
              quality,
              chunk->seg_num,
                      chunk->frag_num ) ) {
        msg_Err(s, "Failed to allocate memory for fragment url" );
        return NULL;
    }

    msg_Info(s, "Downloading fragment %s",  fragment_url );

    stream_t* download_stream = vlc_stream_NewURL( s, fragment_url );
    if( ! download_stream )
    {
        msg_Err(s, "Failed to download fragment %s", fragment_url );
        free( fragment_url );
        chunk->failed = true;
        return NULL;
    }
    free( fragment_url );

    int64_t size = stream_Size( download_stream );
    chunk->data_len = (uint32_t) size;

    if( size > MAX_REQUEST_SIZE )
    {
        msg_Err(s, "Strangely-large chunk of %"PRIi64" Bytes", size );
        return NULL;
    }

    uint8_t* data = malloc( size );
    if( ! data )
    {
        msg_Err(s, "Couldn't allocate chunk" );
        return NULL;
    }

    int read = vlc_stream_Read( download_stream, data,
                            size );
    if( read < 0 )
        read = 0;
    chunk->data_len = read;

    if( read < size )
    {
        msg_Err( s, "Requested %"PRIi64" bytes, "\
                 "but only got %d", size, read );
        data = realloc( chunk->data, read );
        if( data != NULL )
            chunk->data = data;
        chunk->failed = true;
        return NULL;
    }
    else
    {
        chunk->failed = false;
    }

    vlc_stream_Delete( download_stream );
    return data;
}

static void* download_thread( void* p )
{
    vlc_object_t* p_this = (vlc_object_t*)p;
    stream_t* s = (stream_t*) p_this;
    stream_sys_t* sys = s->p_sys;

    if ( vlc_array_count( &sys->hds_streams ) == 0 )
        return NULL;

    // TODO: Change here for selectable stream
    hds_stream_t* hds_stream = sys->hds_streams.pp_elems[0];

    int canc = vlc_savecancel();

    vlc_mutex_lock( & hds_stream->dl_lock );

    while( ! sys->closed )
    {
        if( ! hds_stream->chunks_downloadpos )
        {
            chunk_t* chunk = hds_stream->chunks_head;
            while(chunk && chunk->data )
            {
                chunk = chunk->next;
            }

            if( chunk && ! chunk->data )
                hds_stream->chunks_downloadpos = chunk;
        }

        while( hds_stream->chunks_downloadpos )
        {
            chunk_t *chunk = hds_stream->chunks_downloadpos;

            uint8_t *data = download_chunk( (stream_t*)p_this,
                                            sys,
                                            hds_stream,
                                            chunk );

            if( ! chunk->failed )
            {
                chunk->mdat_len =
                    find_chunk_mdat( p_this,
                                     data,
                                     data + chunk->data_len,
                                     & chunk->mdat_data );
                if( chunk->mdat_len == 0 ) {
                    chunk->mdat_len = chunk->data_len - (chunk->mdat_data - data);
                }
                hds_stream->chunks_downloadpos = chunk->next;
                chunk->data = data;

                sys->chunk_count++;
            }
        }

        vlc_cond_wait( & hds_stream->dl_cond,
                       & hds_stream->dl_lock );
    }

    vlc_mutex_unlock( & hds_stream->dl_lock );

    vlc_restorecancel( canc );
    return NULL;
}

static chunk_t* generate_new_chunk(
    vlc_object_t* p_this,
    chunk_t* last_chunk,
    hds_stream_t* hds_stream )
{
    stream_t* s = (stream_t*) p_this;
    stream_sys_t *sys = s->p_sys;
    chunk_t *chunk = chunk_new();
    unsigned int frun_entry = 0;

    if( ! chunk ) {
        msg_Err( p_this, "Couldn't allocate new chunk!" );
        return NULL;
    }

    if( last_chunk )
    {
        chunk->timestamp = last_chunk->timestamp + last_chunk->duration;
        chunk->frag_num = last_chunk->frag_num + 1;

        if( ! sys->live )
        {
            frun_entry = last_chunk->frun_entry;
        }
    }
    else
    {
        fragment_run_t* first_frun  = hds_stream->fragment_runs;
        if( sys->live )
        {
            chunk->timestamp = (hds_stream->live_current_time * ((uint64_t)hds_stream->afrt_timescale)) / ((uint64_t)hds_stream->timescale);
        }
        else
        {
            chunk->timestamp = first_frun->fragment_timestamp;
            chunk->frag_num =  first_frun->fragment_number_start;
        }
    }

    for( ; frun_entry < hds_stream->fragment_run_count;
         frun_entry++ )
    {
        /* check for discontinuity first */
        if( hds_stream->fragment_runs[frun_entry].fragment_duration == 0 )
        {
            if( frun_entry == hds_stream->fragment_run_count - 1 )
            {
                msg_Err( p_this, "Discontinuity but can't find next timestamp!");
                chunk_free( chunk );
                return NULL;
            }

            chunk->frag_num = hds_stream->fragment_runs[frun_entry+1].fragment_number_start;
            chunk->duration = hds_stream->fragment_runs[frun_entry+1].fragment_duration;
            chunk->timestamp = hds_stream->fragment_runs[frun_entry+1].fragment_timestamp;

            frun_entry++;
            break;
        }

        if( chunk->frag_num == 0 )
        {
            if( frun_entry == hds_stream->fragment_run_count - 1 ||
                ( chunk->timestamp >= hds_stream->fragment_runs[frun_entry].fragment_timestamp &&
                  chunk->timestamp < hds_stream->fragment_runs[frun_entry+1].fragment_timestamp )
                )
            {
                fragment_run_t* frun = hds_stream->fragment_runs + frun_entry;
                chunk->frag_num = frun->fragment_number_start + ( (chunk->timestamp - frun->fragment_timestamp) /
                                                                  frun->fragment_duration );
                chunk->duration = frun->fragment_duration;
            }

        }

        if( hds_stream->fragment_runs[frun_entry].fragment_number_start <=
            chunk->frag_num &&
            (frun_entry == hds_stream->fragment_run_count - 1 ||
             hds_stream->fragment_runs[frun_entry+1].fragment_number_start > chunk->frag_num ) )
        {
            chunk->duration = hds_stream->fragment_runs[frun_entry].fragment_duration;
            chunk->timestamp = hds_stream->fragment_runs[frun_entry].fragment_timestamp +
                chunk->duration * (chunk->frag_num - hds_stream->fragment_runs[frun_entry].fragment_number_start);
            break;
        }
    }

    if( frun_entry == hds_stream->fragment_run_count )
    {
        msg_Err( p_this, "Couldn'd find the fragment run!" );
        chunk_free( chunk );
        return NULL;
    }

    int srun_entry = 0;
    unsigned int segment = 0;
    uint64_t fragments_accum = chunk->frag_num;
    for( srun_entry = 0; srun_entry < hds_stream->segment_run_count;
         srun_entry++ )
    {
        segment = hds_stream->segment_runs[srun_entry].first_segment +
            (chunk->frag_num - fragments_accum ) / hds_stream->segment_runs[srun_entry].fragments_per_segment;

        if( srun_entry + 1 == hds_stream->segment_run_count ||
            hds_stream->segment_runs[srun_entry+1].first_segment > segment )
        {
            break;
        }

        fragments_accum += (
            (hds_stream->segment_runs[srun_entry+1].first_segment -
             hds_stream->segment_runs[srun_entry].first_segment) *
            hds_stream->segment_runs[srun_entry].fragments_per_segment );
    }

    chunk->seg_num = segment;
    chunk->frun_entry = frun_entry;

    if( ! sys->live )
    {
        if( (chunk->timestamp + chunk->duration) / hds_stream->afrt_timescale  >= sys->duration_seconds )
        {
            chunk->eof = true;
        }
    }

    return chunk;
}

static void maintain_live_chunks(
    vlc_object_t* p_this,
    hds_stream_t* hds_stream
    )
{
    if( ! hds_stream->chunks_head )
    {
        /* just start with the earliest in the abst
         * maybe it would be better to use the currentMediaTime?
         * but then we are right on the edge of buffering, esp for
         * small fragments */
        hds_stream->chunks_head = generate_new_chunk(
            p_this, 0, hds_stream );
        hds_stream->chunks_livereadpos = hds_stream->chunks_head;
    }

    chunk_t* chunk = hds_stream->chunks_head;
    bool dl = false;
    while( chunk && ( chunk->timestamp * ((uint64_t)hds_stream->timescale) )
           / ((uint64_t)hds_stream->afrt_timescale)
           <= hds_stream->live_current_time )
    {
        if( chunk->next )
        {
            chunk = chunk->next;
        }
        else
        {
            chunk->next = generate_new_chunk( p_this, chunk, hds_stream );
            chunk = chunk->next;
            dl = true;
        }
    }

    if( dl )
        vlc_cond_signal( & hds_stream->dl_cond );

    chunk = hds_stream->chunks_head;
    while( chunk && chunk->data && chunk->mdat_pos >= chunk->mdat_len && chunk->next )
    {
        chunk_t* next_chunk = chunk->next;
        chunk_free( chunk );
        chunk = next_chunk;
    }

    if( ! hds_stream->chunks_livereadpos )
        hds_stream->chunks_livereadpos = hds_stream->chunks_head;

    hds_stream->chunks_head = chunk;
}


static void* live_thread( void* p )
{
    vlc_object_t* p_this = (vlc_object_t*)p;
    stream_t* s = (stream_t*) p_this;
    stream_sys_t* sys = s->p_sys;

    if ( vlc_array_count( &sys->hds_streams ) == 0 )
        return NULL;

    // TODO: Change here for selectable stream
    hds_stream_t* hds_stream = sys->hds_streams.pp_elems[0];

    int canc = vlc_savecancel();

    char* abst_url;

    if( hds_stream->abst_url &&
        ( isFQUrl( hds_stream->abst_url ) ) )
    {
        if( !( abst_url = strdup( hds_stream->abst_url ) ) )
            return NULL;
    }
    else
    {
        char* server_base = sys->base_url;


        if( 0 > asprintf( &abst_url, "%s/%s",
                          server_base,
                          hds_stream->abst_url ) )
        {
            return NULL;
        }
    }

    mtime_t last_dl_start_time;

    while( ! sys->closed )
    {
        last_dl_start_time = mdate();
        stream_t* download_stream = vlc_stream_NewURL( p_this, abst_url );
        if( ! download_stream )
        {
            msg_Err( p_this, "Failed to download abst %s", abst_url );
        }
        else
        {
            int64_t size = stream_Size( download_stream );
            uint8_t* data = malloc( size );
            int read = vlc_stream_Read( download_stream, data,
                                    size );
            if( read < size )
            {
                msg_Err( p_this, "Requested %"PRIi64" bytes, "  \
                         "but only got %d", size, read );

            }
            else
            {
                vlc_mutex_lock( & hds_stream->abst_lock );
                parse_BootstrapData( p_this, hds_stream,
                                     data, data + read );
                vlc_mutex_unlock( & hds_stream->abst_lock );
                maintain_live_chunks( p_this, hds_stream );
            }

            free( data );

            vlc_stream_Delete( download_stream );
        }

        mwait( last_dl_start_time + ( ((int64_t)hds_stream->fragment_runs[hds_stream->fragment_run_count-1].fragment_duration) * 1000000LL) / ((int64_t)hds_stream->afrt_timescale) );


    }

    free( abst_url );

    vlc_restorecancel( canc );
    return NULL;
}

static int init_Manifest( stream_t *s, manifest_t *m )
{
    memset(m, 0, sizeof(*m));
    stream_t *st = s->p_source;

    m->vlc_reader = xml_ReaderCreate( st, st );
    if( !m->vlc_reader )
    {
        msg_Err( s, "Failed to open source for parsing" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void cleanup_Manifest( manifest_t *m )
{
    for (unsigned i = 0; i < MAX_XML_DEPTH; i++)
        free( m->element_stack[i] );

    for( unsigned i = 0; i < MAX_MEDIA_ELEMENTS; i++ )
    {
        free( m->medias[i].stream_id );
        free( m->medias[i].media_url );
        free( m->medias[i].bootstrap_id );
        free( m->medias[i].metadata );
    }

    for( unsigned i = 0; i < MAX_BOOTSTRAP_INFO; i++ )
    {
        free( m->bootstraps[i].data );
        free( m->bootstraps[i].id );
        free( m->bootstraps[i].url );
        free( m->bootstraps[i].profile );
    }

    if( m->vlc_reader )
        xml_ReaderDelete( m->vlc_reader );
}

static void cleanup_threading( hds_stream_t *stream )
{
    vlc_mutex_destroy( &stream->dl_lock );
    vlc_cond_destroy( &stream->dl_cond );
    vlc_mutex_destroy( &stream->abst_lock );
}

static void write_int_24( uint8_t *p, uint32_t val )
{
    *p         = ( val & 0xFF0000 ) >> 16;
    *( p + 1 ) = ( val & 0xFF00 ) >> 8;
    *( p + 2 ) = val & 0xFF;
}

static void write_int_32( uint8_t *p, uint32_t val )
{
    *p         = ( val & 0xFF000000 ) >> 24;
    *( p + 1 ) = ( val & 0xFF0000 ) >> 16;
    *( p + 2 ) = ( val & 0xFF00 ) >> 8;
    *( p + 3 ) = val & 0xFF;
}

static size_t write_flv_header_and_metadata(
    uint8_t **pp_buffer,
    const uint8_t *p_metadata_payload,
    size_t metadata_payload_len )
{
    size_t metadata_packet_len;
    if ( metadata_payload_len > 0 && p_metadata_payload )
        metadata_packet_len = FLV_TAG_HEADER_LEN + metadata_payload_len;
    else
        metadata_packet_len = 0;
    size_t data_len = FLV_FILE_HEADER_LEN + metadata_packet_len;

    *pp_buffer = malloc( data_len );
    if ( ! *pp_buffer )
    {
        return 0;
    }

    // FLV file header
    memcpy( *pp_buffer, flv_header_bytes, FLV_FILE_HEADER_LEN );

    if ( metadata_packet_len > 0 )
    {
        uint8_t *p = *pp_buffer + FLV_FILE_HEADER_LEN;

        // tag type
        *p = SCRIPT_TAG;
        p++;

        // payload size
        write_int_24( p, metadata_payload_len );
        p += 3;

        // timestamp and stream id
        memset( p, 0, 7 );
        p += 7;

        // metadata payload
        memcpy( p, p_metadata_payload, metadata_payload_len );
        p += metadata_payload_len;

        // packet payload size
        write_int_32( p, metadata_packet_len );
    }

    return data_len;
}

static void initialize_header_and_metadata( stream_sys_t* p_sys, hds_stream_t *stream )
{
    p_sys->flv_header_len =
        write_flv_header_and_metadata( &p_sys->flv_header, stream->metadata,
                                       stream->metadata_len );
}

static int parse_Manifest( stream_t *s, manifest_t *m )
{
    int type = 0;

    msg_Dbg( s, "Manifest parsing\n" );

    char *node;

    stream_sys_t *sys = s->p_sys;

    sys->duration_seconds = 0;

    uint8_t bootstrap_idx = 0;
    uint8_t media_idx = 0;
    uint8_t current_element_idx = 0;
    char* current_element = NULL;

    const char* attr_name;
    const char* attr_value;

    char** element_stack = m->element_stack;
    bootstrap_info *bootstraps = m->bootstraps;
    media_info *medias = m->medias;
    xml_reader_t *vlc_reader = m->vlc_reader;
    char* media_id = NULL;

#define TIMESCALE 10000000
    while( (type = xml_ReaderNextNode( vlc_reader, (const char**) &node )) > 0 )
    {
        switch( type )
        {
        case XML_READER_STARTELEM:
            if( current_element_idx == 0 && element_stack[current_element_idx] == 0 ) {
                if( !( element_stack[current_element_idx] = strdup( node ) ) )
                    return VLC_ENOMEM;
            } else {
                if ( !( element_stack[++current_element_idx] = strdup( node ) ) )
                    return VLC_ENOMEM;
            }

            break;
        case XML_READER_ENDELEM:
            if( current_element && ! strcmp( current_element, "bootstrapInfo") ) {
                if( bootstrap_idx + 1 == MAX_BOOTSTRAP_INFO ) {
                    msg_Warn( (vlc_object_t*) s, "Too many bootstraps, ignoring" );
                } else {
                    bootstrap_idx++;
                }
            }

            free( current_element );
            current_element = NULL;
            element_stack[current_element_idx--] = 0;
            break;
        }

        if( ! element_stack[current_element_idx] ) {
            continue;
        }

        current_element = element_stack[current_element_idx];

        if( type == XML_READER_STARTELEM && ! strcmp( current_element, "media") )
        {
            if( media_idx == MAX_MEDIA_ELEMENTS )
            {
                msg_Err( (vlc_object_t*) s, "Too many media elements, quitting" );
                return VLC_EGENERIC;
            }

            while( ( attr_name = xml_ReaderNextAttr( vlc_reader, &attr_value )) )
            {
                if( !strcmp(attr_name, "streamId" ) )
                {
                    if( !( medias[media_idx].stream_id = strdup( attr_value ) ) )
                        return VLC_ENOMEM;
                }
                else if( !strcmp(attr_name, "url" ) )
                {
                    if( !( medias[media_idx].media_url = strdup( attr_value ) ) )
                        return VLC_ENOMEM;
                }
                else if( !strcmp(attr_name, "bootstrapInfoId" ) )
                {
                    if( !( medias[media_idx].bootstrap_id = strdup( attr_value ) ) )
                        return VLC_ENOMEM;
                }
                else if( !strcmp(attr_name, "bitrate" ) )
                {
                    medias[media_idx].bitrate = (uint32_t) atoi( attr_value );
                }
            }
            media_idx++;
        }

        else if( type == XML_READER_STARTELEM && ! strcmp( current_element, "bootstrapInfo") )
        {
            while( ( attr_name = xml_ReaderNextAttr( vlc_reader, &attr_value )) )
            {
                if( !strcmp(attr_name, "url" ) )
                {
                    if( !( bootstraps[bootstrap_idx].url = strdup( attr_value ) ) )
                        return VLC_ENOMEM;
                }
                else if( !strcmp(attr_name, "id" ) )
                {
                    if( !( bootstraps[bootstrap_idx].id = strdup( attr_value ) ) )
                       return VLC_ENOMEM;
                }
                else if( !strcmp(attr_name, "profile" ) )
                {
                    if( !( bootstraps[bootstrap_idx].profile = strdup( attr_value ) ) )
                        return VLC_ENOMEM;
                }
            }
        }

        else if( type == XML_READER_TEXT )
        {
            if( ! strcmp( current_element, "bootstrapInfo" ) )
            {
                char* start = node;
                char* end = start + strlen(start);
                whitespace_substr( &start, &end );
                *end = '\0';

                bootstraps[bootstrap_idx].data_len =
                    vlc_b64_decode_binary( (uint8_t**)&bootstraps[bootstrap_idx].data, start );
                if( ! bootstraps[bootstrap_idx].data )
                {
                    msg_Err( (vlc_object_t*) s, "Couldn't decode bootstrap info" );
                }
            }
            else if( ! strcmp( current_element, "duration" ) )
            {
                double shutup_gcc = atof( node );
                sys->duration_seconds = (uint64_t) shutup_gcc;
            }
            else if( ! strcmp( current_element, "id" ) )
            {
                if( ! strcmp( element_stack[current_element_idx-1], "manifest" ) )
                {
                    if( !( media_id = strdup( node ) ) )
                        return VLC_ENOMEM;
                }
            }
            else if( ! strcmp( current_element, "metadata" ) &&
                     ! strcmp( element_stack[current_element_idx-1], "media" ) &&
                     ( media_idx >= 1 ) )
            {
                uint8_t mi = media_idx - 1;
                if ( ! medias[mi].metadata )
                {
                    char* start = node;
                    char* end = start + strlen(start);
                    whitespace_substr( &start, &end );
                    *end = '\0';

                    medias[mi].metadata_len =
                        vlc_b64_decode_binary( (uint8_t**)&medias[mi].metadata, start );

                    if ( ! medias[mi].metadata )
                        return VLC_ENOMEM;

                    uint8_t *end_marker =
                        medias[mi].metadata + medias[mi].metadata_len - sizeof(amf_object_end);
                    if ( ( end_marker < medias[mi].metadata ) ||
                         memcmp(end_marker, amf_object_end, sizeof(amf_object_end)) != 0 )
                    {
                        msg_Dbg( (vlc_object_t*)s, "Ignoring invalid metadata packet on stream %d", mi );
                        FREENULL( medias[mi].metadata );
                        medias[mi].metadata_len = 0;
                    }
                }
            }
        }
    }

    for( int i = 0; i <= media_idx; i++ )
    {
        for( int j = 0; j < bootstrap_idx; j++ )
        {
            if( ( ! medias[i].bootstrap_id && ! bootstraps[j].id ) ||
                (medias[i].bootstrap_id && bootstraps[j].id &&
                 ! strcmp( medias[i].bootstrap_id, bootstraps[j].id ) ) )
            {
                hds_stream_t* new_stream = malloc(sizeof(hds_stream_t));
                if( !new_stream )
                {
                    free(media_id);
                    return VLC_ENOMEM;
                }
                memset( new_stream, 0, sizeof(hds_stream_t));

                vlc_mutex_init( & new_stream->abst_lock );
                vlc_mutex_init( & new_stream->dl_lock );
                vlc_cond_init( & new_stream->dl_cond );

                if( sys->duration_seconds )
                {
                    sys->live = false;
                }
                else
                {
                    sys->live = true;
                }

                if( medias[i].media_url )
                {
                    if( !(new_stream->url = strdup( medias[i].media_url ) ) )
                    {
                        free( media_id );
                        cleanup_threading( new_stream );
                        free( new_stream );
                        return VLC_ENOMEM;
                    }
                }

                if( medias[i].metadata )
                {
                    new_stream->metadata = malloc( medias[i].metadata_len );
                    if ( ! new_stream->metadata )
                    {
                        free( new_stream->url );
                        free( media_id );
                        cleanup_threading( new_stream );
                        free( new_stream );
                        return VLC_ENOMEM;
                    }

                    memcpy( new_stream->metadata, medias[i].metadata, medias[i].metadata_len );
                    new_stream->metadata_len = medias[i].metadata_len;
                }

                if( ! sys->live )
                {
                    parse_BootstrapData( (vlc_object_t*)s,
                                         new_stream,
                                         bootstraps[j].data,
                                         bootstraps[j].data + bootstraps[j].data_len );

                    new_stream->download_leadtime = 15;

                    new_stream->chunks_head = generate_new_chunk(
                        (vlc_object_t*) s, 0, new_stream );
                    chunk_t* chunk = new_stream->chunks_head;
                    uint64_t total_duration = chunk->duration;
                    while( chunk && total_duration/new_stream->afrt_timescale < new_stream->download_leadtime )
                    {
                        chunk->next = generate_new_chunk(
                            (vlc_object_t*) s, chunk, new_stream );
                        chunk = chunk->next;
                        if( chunk )
                            total_duration += chunk->duration;
                    }
                }
                else
                {
                    if( !(new_stream->abst_url = strdup( bootstraps[j].url ) ) )
                    {
                        free( new_stream->metadata );
                        free( new_stream->url );
                        free( media_id );
                        cleanup_threading( new_stream );
                        free( new_stream );
                        return VLC_ENOMEM;
                    }
                }

                new_stream->bitrate = medias[i].bitrate;

                vlc_array_append_or_abort( &sys->hds_streams, new_stream );

                msg_Info( (vlc_object_t*)s, "New track with quality_segment(%s), bitrate(%u), timescale(%u), movie_id(%s), segment_run_count(%d), fragment_run_count(%u)",
                          new_stream->quality_segment_modifier?new_stream->quality_segment_modifier:"", new_stream->bitrate, new_stream->timescale,
                          new_stream->movie_id, new_stream->segment_run_count, new_stream->fragment_run_count );

            }
        }
    }

    free( media_id );
    cleanup_Manifest( m );

    return VLC_SUCCESS;
}


static void hds_free( hds_stream_t *p_stream )
{
    FREENULL( p_stream->quality_segment_modifier );

    FREENULL( p_stream->abst_url );

    cleanup_threading( p_stream );

    FREENULL( p_stream->metadata );
    FREENULL( p_stream->url );
    FREENULL( p_stream->movie_id );
    for( int i = 0; i < p_stream->server_entry_count; i++ )
    {
        FREENULL( p_stream->server_entries[i] );
    }

    chunk_t* chunk = p_stream->chunks_head;
    while( chunk )
    {
        chunk_t* next = chunk->next;
        chunk_free( chunk );
        chunk = next;
    }

    free( p_stream );
}

static void SysCleanup( stream_sys_t *p_sys )
{
    for( size_t i = 0; i < p_sys->hds_streams.i_count ; i++ )
        hds_free( p_sys->hds_streams.pp_elems[i] );
    vlc_array_clear( &p_sys->hds_streams );
    free( p_sys->base_url );
}

static int Open( vlc_object_t *p_this )
{
    stream_t *s = (stream_t*)p_this;
    stream_sys_t *p_sys;

    if( !isHDS( s ) || s->psz_url == NULL )
        return VLC_EGENERIC;

    msg_Info( p_this, "HTTP Dynamic Streaming (%s)", s->psz_url );

    s->p_sys = p_sys = calloc( 1, sizeof(*p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    char *uri_without_query = strndup( s->psz_url,
                                       strcspn( s->psz_url, "?" ) );
    if( uri_without_query == NULL )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    /* remove the last part of the url */
    char *pos = strrchr( uri_without_query, '/');
    if ( pos == NULL )
    {
        free( uri_without_query );
        free( p_sys );
        return VLC_EGENERIC;
    }
    *pos = '\0';
    p_sys->base_url = uri_without_query;

    p_sys->flv_header_bytes_sent = 0;

    vlc_array_init( &p_sys->hds_streams );

    manifest_t m;
    if( init_Manifest( s, &m ) || parse_Manifest( s, &m ) )
    {
        cleanup_Manifest( &m );
        goto error;
    }

    s->pf_read = Read;
    s->pf_seek = NULL;
    s->pf_control = Control;

    if( vlc_clone( &p_sys->dl_thread, download_thread, s, VLC_THREAD_PRIORITY_INPUT ) )
    {
        goto error;
    }

    if( p_sys->live ) {
        msg_Info( p_this, "Live stream detected" );

        if( vlc_clone( &p_sys->live_thread, live_thread, s, VLC_THREAD_PRIORITY_INPUT ) )
        {
            goto error;
        }
    }

    return VLC_SUCCESS;

error:
    SysCleanup( p_sys );
    free( p_sys );
    return VLC_EGENERIC;
}

static void Close( vlc_object_t *p_this )
{
    stream_t *s = (stream_t*)p_this;
    stream_sys_t *p_sys = s->p_sys;

    // TODO: Change here for selectable stream
    hds_stream_t *stream = vlc_array_count(&p_sys->hds_streams) ?
        p_sys->hds_streams.pp_elems[0] : NULL;

    p_sys->closed = true;
    if (stream)
        vlc_cond_signal( & stream->dl_cond );

    vlc_join( p_sys->dl_thread, NULL );

    if( p_sys->live )
    {
        vlc_join( p_sys->live_thread, NULL );
    }

    SysCleanup( p_sys );
    free( p_sys );
}

static int send_flv_header( hds_stream_t *stream, stream_sys_t* p_sys,
                            void* buffer, unsigned i_read )
{
    if ( !p_sys->flv_header )
    {
        initialize_header_and_metadata( p_sys, stream );
    }

    uint32_t to_be_read = i_read;
    uint32_t header_remaining =
        p_sys->flv_header_len - p_sys->flv_header_bytes_sent;
    if( to_be_read > header_remaining ) {
        to_be_read = header_remaining;
    }

    memcpy( buffer, p_sys->flv_header + p_sys->flv_header_bytes_sent, to_be_read );

    p_sys->flv_header_bytes_sent += to_be_read;
    return to_be_read;
}

static unsigned read_chunk_data(
    vlc_object_t* p_this,
    uint8_t* buffer, unsigned read_len,
    hds_stream_t* stream )
{
    stream_t* s = (stream_t*) p_this;
    stream_sys_t* sys = s->p_sys;
    chunk_t* chunk = stream->chunks_head;
    uint8_t* buffer_start = buffer;
    bool dl = false;

    if( chunk && chunk->eof && chunk->mdat_pos >= chunk->mdat_len )
        return 0;

    while( chunk && chunk->data && read_len > 0 && ! (chunk->eof && chunk->mdat_pos >= chunk->mdat_len ) )
    {
        /* in the live case, it is necessary to store the next
         * pointer here, since as soon as we increment the mdat_pos, that
         * chunk may be deleted */
        chunk_t* next = chunk->next;

        if( chunk->mdat_pos < chunk->mdat_len )
        {
            unsigned cp_len = chunk->mdat_len - chunk->mdat_pos;
            if( cp_len > read_len )
                cp_len = read_len;
            memcpy( buffer, chunk->mdat_data + chunk->mdat_pos,
                    cp_len );

            read_len -= cp_len;
            buffer += cp_len;
            chunk->mdat_pos += cp_len;
        }

        if( ! sys->live && (chunk->mdat_pos >= chunk->mdat_len || chunk->failed) )
        {
            /* make sure there is at least one chunk in the queue */
            if( ! chunk->next && ! chunk->eof )
            {
                chunk->next = generate_new_chunk( p_this, chunk,  stream );
                dl = true;
            }

            if( ! chunk->eof )
            {
                chunk_free( chunk );
                chunk = next;
                stream->chunks_head = chunk;
            }
        }
        else if( sys->live && (chunk->mdat_pos >= chunk->mdat_len || chunk->failed) )
        {
            chunk = next;
        }
    }

    if( sys->live )
    {
        stream->chunks_livereadpos = chunk;
    }

    /* new chunk generation is handled by a different thread in live case */
    if( ! sys->live )
    {
        chunk = stream->chunks_head;
        if( chunk )
        {
            uint64_t total_duration = chunk->duration;
            while( chunk && total_duration/stream->afrt_timescale < stream->download_leadtime && ! chunk->eof )
            {
                if( ! chunk->next && ! chunk->eof )
                {
                    chunk->next = generate_new_chunk( p_this, chunk, stream );
                    dl = true;
                }

                if( ! chunk->eof )
                {
                    chunk = chunk->next;
                    if( chunk )
                    {
                        total_duration += chunk->duration;
                    }
                }
            }
        }

        if( dl )
            vlc_cond_signal( & stream->dl_cond );
    }

    return ( ((uint8_t*)buffer) - ((uint8_t*)buffer_start));
}

static inline bool header_unfinished( stream_sys_t *p_sys )
{
    return p_sys->flv_header_bytes_sent < p_sys->flv_header_len;
}

static ssize_t Read( stream_t *s, void *buffer, size_t i_read )
{
    stream_sys_t *p_sys = s->p_sys;

    if ( vlc_array_count( &p_sys->hds_streams ) == 0 )
        return 0;
    if( unlikely(i_read == 0) )
        return 0;

    // TODO: change here for selectable stream
    hds_stream_t *stream = p_sys->hds_streams.pp_elems[0];

    if ( header_unfinished( p_sys ) )
        return send_flv_header( stream, p_sys, buffer, i_read );

    return read_chunk_data( (vlc_object_t*)s, buffer, i_read, stream );
}

static int Control( stream_t *s, int i_query, va_list args )
{
    switch( i_query )
    {
        case STREAM_CAN_SEEK:
            *(va_arg( args, bool * )) = false;
            break;
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE: /* TODO */
            *(va_arg( args, bool * )) = false;
            break;
        case STREAM_CAN_CONTROL_PACE:
            *(va_arg( args, bool * )) = true;
            break;
        case STREAM_GET_PTS_DELAY:
            *va_arg (args, int64_t *) = INT64_C(1000) *
                var_InheritInteger(s, "network-caching");
             break;
        case STREAM_GET_SIZE:
            *(va_arg (args, uint64_t *)) = get_stream_size(s);
            break;
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
