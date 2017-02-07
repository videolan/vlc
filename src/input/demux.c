/*****************************************************************************
 * demux.c
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Laurent Aimar <fenrir@via.ecp.fr>
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

#include <assert.h>
#include <limits.h>

#include "demux.h"
#include <libvlc.h>
#include <vlc_codec.h>
#include <vlc_meta.h>
#include <vlc_url.h>
#include <vlc_modules.h>
#include <vlc_strings.h>

static bool SkipID3Tag( demux_t * );
static bool SkipAPETag( demux_t *p_demux );

typedef const struct
{
    char const key[20];
    char const name[8];

} demux_mapping;

static int demux_mapping_cmp( const void *k, const void *v )
{
    demux_mapping* entry = v;
    return vlc_ascii_strcasecmp( k, entry->key );
}

static demux_mapping* demux_lookup( char const* key,
                                    demux_mapping* data, size_t size )
{
    return bsearch( key, data, size, sizeof( *data ), demux_mapping_cmp );
}

static const char *demux_NameFromContentType(const char *mime)
{
    static demux_mapping types[] =
    {   /* Must be sorted in ascending ASCII order */
        { "audio/aac",           "m4a"     },
        { "audio/aacp",          "m4a"     },
        { "audio/mpeg",          "mp3"     },
        { "application/rss+xml", "podcast" },
        //{ "video/MP1S",          "es,mpgv" }, !b_force
        { "video/dv",            "rawdv"   },
        { "video/MP2P",          "ps"      },
        { "video/MP2T",          "ts"      },
        { "video/nsa",           "nsv"     },
        { "video/nsv",           "nsv"     },
    };
    demux_mapping *type = demux_lookup( mime, types, ARRAY_SIZE( types ) );
    return (type != NULL) ? type->name : "any";
}

static const char* DemuxNameFromExtension( char const* ext,
                                           bool b_preparsing )
{
    /* NOTE: Add only file without any problems here and with strong detection:
     * - no .mp3, .a52, ...
     *  - wav can't be added 'cause of a52 and dts in them as raw audio
     */
    static demux_mapping strong[] =
    { /* NOTE: must be sorted in asc order */
        { "aiff", "aiff" },
        { "asf",  "asf" },
        { "au",   "au" },
        { "avi",  "avi" },
        { "drc",  "dirac" },
        { "dv",   "dv" },
        { "flac", "flac" },
        { "h264", "h264" },
        { "kar", "smf" },
        { "m3u",  "m3u" },
        { "m4a",  "mp4" },
        { "m4v",  "m4v" },
        { "mid",  "smf" },
        { "mka",  "mkv" },
        { "mks",  "mkv" },
        { "mkv",  "mkv" },
        { "moov", "mp4" },
        { "mov",  "mp4" },
        { "mp4",  "mp4" },
        { "nsv",  "nsv" },
        { "oga",  "ogg" },
        { "ogg",  "ogg" },
        { "ogm",  "ogg" },
        { "ogv",  "ogg" },
        { "ogx",  "ogg" }, /*RFC5334*/
        { "opus", "ogg" }, /*draft-terriberry-oggopus-01*/
        { "pva",  "pva" },
        { "rm",   "avformat" },
        { "rmi",  "smf" },
        { "spx",  "ogg" },
        { "voc",  "voc" },
        { "wma",  "asf" },
        { "wmv",  "asf" },
    };

    /* Here, we don't mind if it does not work, it must be quick */
    static demux_mapping quick[] =
    { /* NOTE: shall be sorted in asc order */
        { "mp3", "mpga" },
        { "ogg", "ogg" },
        { "wma", "asf" },
    };

    struct {
        demux_mapping* data;
        size_t size;

    } lookup = {
        .data = b_preparsing ? quick : strong,
        .size = b_preparsing ? ARRAY_SIZE( quick ) : ARRAY_SIZE( strong )
    };

    demux_mapping* result = demux_lookup( ext, lookup.data, lookup.size );
    return result ? result->name : NULL;
}

/*****************************************************************************
 * demux_New:
 *  if s is NULL then load a access_demux
 *****************************************************************************/
demux_t *demux_New( vlc_object_t *p_obj, const char *psz_name,
                    const char *psz_location, stream_t *s, es_out_t *out )
{
    return demux_NewAdvanced( p_obj, NULL,
                              (s == NULL) ? psz_name : "",
                              (s != NULL) ? psz_name : "",
                              psz_location, s, out, false );
}

typedef struct demux_priv_t
{
    demux_t demux;
    void (*destroy)(demux_t *);
} demux_priv_t;

static void demux_DestroyDemux(demux_t *demux)
{
    assert(demux->s != NULL);
    vlc_stream_Delete(demux->s);
}

static void demux_DestroyAccessDemux(demux_t *demux)
{
    assert(demux->s == NULL);
    (void) demux;
}

static void demux_DestroyDemuxFilter(demux_t *demux)
{
    assert(demux->p_next != NULL);
    (void) demux;
}

/*****************************************************************************
 * demux_NewAdvanced:
 *  if s is NULL then load a access_demux
 *****************************************************************************/
#undef demux_NewAdvanced
demux_t *demux_NewAdvanced( vlc_object_t *p_obj, input_thread_t *p_parent_input,
                            const char *psz_access, const char *psz_demux,
                            const char *psz_location,
                            stream_t *s, es_out_t *out, bool b_preparsing )
{
    demux_priv_t *priv = vlc_custom_create(p_obj, sizeof (*priv), "demux");
    if (unlikely(priv == NULL))
        return NULL;

    demux_t *p_demux = &priv->demux;

    if( s != NULL && (!strcasecmp( psz_demux, "any" ) || !psz_demux[0]) )
    {   /* Look up demux by Content-Type for hard to detect formats */
        char *type = stream_ContentType( s );
        if( type != NULL )
        {
            psz_demux = demux_NameFromContentType( type );
            free( type );
        }
    }

    p_demux->p_input = p_parent_input;
    p_demux->psz_access = strdup( psz_access );
    p_demux->psz_demux = strdup( psz_demux );
    p_demux->psz_location = strdup( psz_location );
    p_demux->psz_file = get_path( psz_location ); /* parse URL */

    if( unlikely(p_demux->psz_access == NULL
              || p_demux->psz_demux == NULL
              || p_demux->psz_location == NULL) )
        goto error;

    if( !b_preparsing )
        msg_Dbg( p_obj, "creating demux: access='%s' demux='%s' "
                 "location='%s' file='%s'",
                 p_demux->psz_access, p_demux->psz_demux,
                 p_demux->psz_location, p_demux->psz_file );

    p_demux->s              = s;
    p_demux->out            = out;
    p_demux->b_preparsing   = b_preparsing;

    p_demux->pf_demux   = NULL;
    p_demux->pf_control = NULL;
    p_demux->p_sys      = NULL;
    p_demux->info.i_update = 0;
    p_demux->info.i_title  = 0;
    p_demux->info.i_seekpoint = 0;
    priv->destroy = s ? demux_DestroyDemux : demux_DestroyAccessDemux;

    if( s != NULL )
    {
        const char *psz_module = NULL;

        if( !strcmp( p_demux->psz_demux, "any" ) && p_demux->psz_file )
        {
            char const* psz_ext = strrchr( p_demux->psz_file, '.' );

            if( psz_ext )
                psz_module = DemuxNameFromExtension( psz_ext + 1, b_preparsing );
        }

        if( psz_module == NULL )
            psz_module = p_demux->psz_demux;

        /* ID3/APE tags will mess-up demuxer probing so we skip it here.
         * ID3/APE parsers will called later on in the demuxer to access the
         * skipped info. */
        while (SkipID3Tag( p_demux ))
          ;
        SkipAPETag( p_demux );

        p_demux->p_module =
            module_need( p_demux, "demux", psz_module,
                         !strcmp( psz_module, p_demux->psz_demux ) );
    }
    else
    {
        p_demux->p_module =
            module_need( p_demux, "access_demux", p_demux->psz_access, true );
    }

    if( p_demux->p_module == NULL )
        goto error;

    return p_demux;
error:
    free( p_demux->psz_file );
    free( p_demux->psz_location );
    free( p_demux->psz_demux );
    free( p_demux->psz_access );
    vlc_object_release( p_demux );
    return NULL;
}

/*****************************************************************************
 * demux_Delete:
 *****************************************************************************/
void demux_Delete( demux_t *p_demux )
{
    demux_priv_t *priv = (demux_priv_t *)p_demux;

    module_unneed( p_demux, p_demux->p_module );

    priv->destroy(p_demux);
    free( p_demux->psz_file );
    free( p_demux->psz_location );
    free( p_demux->psz_demux );
    free( p_demux->psz_access );
    vlc_object_release( p_demux );
}

#define static_control_match(foo) \
    static_assert((unsigned) DEMUX_##foo == STREAM_##foo, "Mismatch")

static int demux_ControlInternal( demux_t *demux, int query, ... )
{
    int ret;
    va_list ap;

    va_start( ap, query );
    ret = demux->pf_control( demux, query, ap );
    va_end( ap );
    return ret;
}

int demux_vaControl( demux_t *demux, int query, va_list args )
{
    if( demux->s != NULL )
        switch( query )
        {
            /* Legacy fallback for missing getters in synchronous demuxers */
            case DEMUX_CAN_PAUSE:
            case DEMUX_CAN_CONTROL_PACE:
            case DEMUX_GET_PTS_DELAY:
            {
                int ret;
                va_list ap;

                va_copy( ap, args );
                ret = demux->pf_control( demux, query, args );
                if( ret != VLC_SUCCESS )
                    ret = vlc_stream_vaControl( demux->s, query, ap );
                va_end( ap );
                return ret;
            }

            /* Some demuxers need to control pause directly (e.g. adaptive),
             * but many legacy demuxers do not understand pause at all.
             * If DEMUX_CAN_PAUSE is not implemented, bypass the demuxer and
             * byte stream. If DEMUX_CAN_PAUSE is implemented and pause is
             * supported, pause the demuxer normally. Else, something went very
             * wrong.
             *
             * Note that this requires asynchronous/threaded demuxers to
             * always return VLC_SUCCESS for DEMUX_CAN_PAUSE, so that they are
             * never bypassed. Otherwise, we would reenter demux->s callbacks
             * and break thread safety. At the time of writing, asynchronous or
             * threaded *non-access* demuxers do not exist and are not fully
             * supported by the input thread, so this is theoretical. */
            case DEMUX_SET_PAUSE_STATE:
            {
                bool can_pause;

                if( demux_ControlInternal( demux, DEMUX_CAN_PAUSE,
                                           &can_pause ) )
                    return vlc_stream_vaControl( demux->s, query, args );

                /* The caller shall not pause if pause is unsupported. */
                assert( can_pause );
                break;
            }
        }

    return demux->pf_control( demux, query, args );
}

/*****************************************************************************
 * demux_vaControlHelper:
 *****************************************************************************/
int demux_vaControlHelper( stream_t *s,
                            int64_t i_start, int64_t i_end,
                            int64_t i_bitrate, int i_align,
                            int i_query, va_list args )
{
    int64_t i_tell;
    double  f, *pf;
    int64_t i64, *pi64;

    if( i_end < 0 )    i_end   = stream_Size( s );
    if( i_start < 0 )  i_start = 0;
    if( i_align <= 0 ) i_align = 1;
    i_tell = vlc_stream_Tell( s );

    static_control_match(CAN_PAUSE);
    static_control_match(CAN_CONTROL_PACE);
    static_control_match(GET_PTS_DELAY);
    static_control_match(GET_META);
    static_control_match(GET_SIGNAL);
    static_control_match(SET_PAUSE_STATE);

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
        {
            bool *b = va_arg( args, bool * );

            if( (i_bitrate <= 0 && i_start >= i_end)
             || vlc_stream_Control( s, STREAM_CAN_SEEK, b ) )
                *b = false;
            break;
        }

        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_GET_PTS_DELAY:
        case DEMUX_GET_META:
        case DEMUX_GET_SIGNAL:
        case DEMUX_SET_PAUSE_STATE:
            return vlc_stream_vaControl( s, i_query, args );

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( i_bitrate > 0 && i_end > i_start )
            {
                *pi64 = INT64_C(8000000) * (i_end - i_start) / i_bitrate;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( i_bitrate > 0 && i_tell >= i_start )
            {
                *pi64 = INT64_C(8000000) * (i_tell - i_start) / i_bitrate;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            if( i_start < i_end )
            {
                *pf = (double)( i_tell - i_start ) /
                      (double)( i_end  - i_start );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;


        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            if( i_start < i_end && f >= 0.0 && f <= 1.0 )
            {
                int64_t i_block = (f * ( i_end - i_start )) / i_align;

                if( vlc_stream_Seek( s, i_start + i_block * i_align ) )
                {
                    return VLC_EGENERIC;
                }
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            if( i_bitrate > 0 && i64 >= 0 )
            {
                int64_t i_block = i64 * i_bitrate / INT64_C(8000000) / i_align;
                if( vlc_stream_Seek( s, i_start + i_block * i_align ) )
                {
                    return VLC_EGENERIC;
                }
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_IS_PLAYLIST:
            *va_arg( args, bool * ) = false;
            return VLC_SUCCESS;

        case DEMUX_GET_FPS:
        case DEMUX_HAS_UNSUPPORTED_META:
        case DEMUX_SET_NEXT_DEMUX_TIME:
        case DEMUX_GET_TITLE_INFO:
        case DEMUX_SET_GROUP:
        case DEMUX_SET_ES:
        case DEMUX_GET_ATTACHMENTS:
        case DEMUX_CAN_RECORD:
        case DEMUX_TEST_AND_CLEAR_FLAGS:
        case DEMUX_GET_TITLE:
        case DEMUX_GET_SEEKPOINT:
            return VLC_EGENERIC;

        case DEMUX_SET_TITLE:
        case DEMUX_SET_SEEKPOINT:
        case DEMUX_SET_RECORD_STATE:
            assert(0);
        default:
            msg_Err( s, "unknown query 0x%x in %s", i_query, __func__ );
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/****************************************************************************
 * Utility functions
 ****************************************************************************/
decoder_t *demux_PacketizerNew( demux_t *p_demux, es_format_t *p_fmt, const char *psz_msg )
{
    decoder_t *p_packetizer;
    p_packetizer = vlc_custom_create( p_demux, sizeof( *p_packetizer ),
                                      "demux packetizer" );
    if( !p_packetizer )
    {
        es_format_Clean( p_fmt );
        return NULL;
    }
    p_fmt->b_packetized = false;

    p_packetizer->pf_decode_audio = NULL;
    p_packetizer->pf_decode_video = NULL;
    p_packetizer->pf_decode_sub = NULL;
    p_packetizer->pf_packetize = NULL;

    p_packetizer->fmt_in = *p_fmt;
    es_format_Init( &p_packetizer->fmt_out, UNKNOWN_ES, 0 );

    p_packetizer->p_module = module_need( p_packetizer, "packetizer", NULL, false );
    if( !p_packetizer->p_module )
    {
        es_format_Clean( p_fmt );
        vlc_object_release( p_packetizer );
        msg_Err( p_demux, "cannot find packetizer for %s", psz_msg );
        return NULL;
    }

    return p_packetizer;
}

void demux_PacketizerDestroy( decoder_t *p_packetizer )
{
    if( p_packetizer->p_module )
        module_unneed( p_packetizer, p_packetizer->p_module );
    es_format_Clean( &p_packetizer->fmt_in );
    es_format_Clean( &p_packetizer->fmt_out );
    if( p_packetizer->p_description )
        vlc_meta_Delete( p_packetizer->p_description );
    vlc_object_release( p_packetizer );
}

static bool SkipID3Tag( demux_t *p_demux )
{
    const uint8_t *p_peek;
    uint8_t version, revision;
    int i_size;
    int b_footer;

    if( !p_demux->s )
        return false;

    /* Get 10 byte id3 header */
    if( vlc_stream_Peek( p_demux->s, &p_peek, 10 ) < 10 )
        return false;

    if( memcmp( p_peek, "ID3", 3 ) )
        return false;

    version = p_peek[3];
    revision = p_peek[4];
    b_footer = p_peek[5] & 0x10;
    i_size = (p_peek[6]<<21) + (p_peek[7]<<14) + (p_peek[8]<<7) + p_peek[9];

    if( b_footer ) i_size += 10;
    i_size += 10;

    /* Skip the entire tag */
    if( vlc_stream_Read( p_demux->s, NULL, i_size ) < i_size )
        return false;

    msg_Dbg( p_demux, "ID3v2.%d revision %d tag found, skipping %d bytes",
             version, revision, i_size );
    return true;
}
static bool SkipAPETag( demux_t *p_demux )
{
    const uint8_t *p_peek;

    if( !p_demux->s )
        return false;

    /* Get 32 byte ape header */
    if( vlc_stream_Peek( p_demux->s, &p_peek, 32 ) < 32 )
        return false;

    if( memcmp( p_peek, "APETAGEX", 8 ) )
        return false;

    uint_fast32_t version = GetDWLE( &p_peek[8] );
    uint_fast32_t size = GetDWLE( &p_peek[8+4] );
    uint_fast32_t flags = GetDWLE( &p_peek[8+4+4] );

    if( (version != 1000 && version != 2000) || !(flags & (1u << 29))
     || (size > SSIZE_MAX - 32u) )
        return false;

    if( flags & (1u << 30) )
        size += 32;

    /* Skip the entire tag */
    if( vlc_stream_Read( p_demux->s, NULL, size ) < (ssize_t)size )
        return false;

    msg_Dbg( p_demux, "AP2 v%"PRIuFAST32" tag found, "
             "skipping %"PRIuFAST32" bytes", version / 1000, size );
    return true;
}

unsigned demux_TestAndClearFlags( demux_t *p_demux, unsigned flags )
{
    unsigned i_update;
    if ( demux_Control( p_demux, DEMUX_TEST_AND_CLEAR_FLAGS, &i_update ) == VLC_SUCCESS )
        return i_update;
    unsigned ret = p_demux->info.i_update & flags;
    p_demux->info.i_update &= ~flags;
    return ret;
}

int demux_GetTitle( demux_t *p_demux )
{
    int i_title;
    if ( demux_Control( p_demux, DEMUX_GET_TITLE, &i_title ) == VLC_SUCCESS )
        return i_title;
    return p_demux->info.i_title;
}

int demux_GetSeekpoint( demux_t *p_demux )
{
    int i_seekpoint;
    if ( demux_Control( p_demux, DEMUX_GET_SEEKPOINT, &i_seekpoint ) == VLC_SUCCESS  )
        return i_seekpoint;
    return p_demux->info.i_seekpoint;
}

static demux_t *demux_FilterNew( demux_t *p_next, const char *p_name )
{
    demux_priv_t *priv = vlc_custom_create(p_next, sizeof (*priv), "demux_filter");
    if (unlikely(priv == NULL))
        return NULL;

    demux_t *p_demux = &priv->demux;

    p_demux->p_next       = p_next;
    p_demux->p_input      = NULL;
    p_demux->p_sys        = NULL;
    p_demux->psz_access   = NULL;
    p_demux->psz_demux    = NULL;
    p_demux->psz_location = NULL;
    p_demux->psz_file     = NULL;
    p_demux->out          = NULL;
    priv->destroy         = demux_DestroyDemuxFilter;
    p_demux->p_module =
        module_need( p_demux, "demux_filter", p_name, p_name != NULL );

    if( p_demux->p_module == NULL )
        goto error;

    return p_demux;
error:
    vlc_object_release( p_demux );
    return NULL;
}

demux_t *demux_FilterChainNew( demux_t *p_demux, const char *psz_chain )
{
    if( !psz_chain || !*psz_chain )
        return NULL;

    char *psz_parser = strdup(psz_chain);
    if(!psz_parser)
        return NULL;

    vlc_array_t name;
    vlc_array_init(&name);

    /* parse chain */
    while(psz_parser)
    {
        config_chain_t *p_cfg;
        char *psz_name;
        char *psz_rest_chain = config_ChainCreate( &psz_name, &p_cfg, psz_parser );
        free( psz_parser );
        psz_parser = psz_rest_chain;

        vlc_array_append(&name, psz_name);
        config_ChainDestroy(p_cfg);
    }

    int i = vlc_array_count(&name);
    vlc_array_t module;
    vlc_array_init(&module);
    while(i--)
    {
        const char *p_name = vlc_array_item_at_index(&name, i);
        demux_t *p_next = demux_FilterNew( p_demux, p_name );
        if(!p_next)
            goto error;

        vlc_array_append(&module, p_next);
        p_demux = p_next;
    }

    vlc_array_clear(&name);
    vlc_array_clear(&module);

    return p_demux;
 error:
    i++;    /* last module couldn't be created */

    /* destroy all modules created, starting with the last one */
    int modules = vlc_array_count(&module);
    while(modules--)
        demux_Delete(vlc_array_item_at_index(&module, modules));
    vlc_array_clear(&module);

    while(i--)
        free(vlc_array_item_at_index(&name, i));
    vlc_array_clear(&name);

    return NULL;
}
