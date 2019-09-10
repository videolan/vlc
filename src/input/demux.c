/*****************************************************************************
 * demux.c
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
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
#include "input_internal.h"

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

static const char *demux_NameFromMimeType(const char *mime)
{
    static demux_mapping types[] =
    {   /* Must be sorted in ascending ASCII order */
        { "audio/aac",           "m4a"     },
        { "audio/aacp",          "m4a"     },
        { "audio/mpeg",          "mp3"     },
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

demux_t *demux_New( vlc_object_t *p_obj, const char *psz_name,
                    stream_t *s, es_out_t *out )
{
    assert(s != NULL );
    return demux_NewAdvanced( p_obj, NULL, psz_name, "", s, out, false );
}

struct vlc_demux_private
{
    module_t *module;
};

static void demux_DestroyDemux(demux_t *demux)
{
    struct vlc_demux_private *priv = vlc_stream_Private(demux);

    module_unneed(demux, priv->module);
    free(demux->psz_filepath);
    free(demux->psz_name);

    assert(demux->s != NULL);
    vlc_stream_Delete(demux->s);
}

static int demux_Probe(void *func, bool forced, va_list ap)
{
    int (*probe)(vlc_object_t *) = func;
    demux_t *demux = va_arg(ap, demux_t *);

    /* Restore input stream offset (in case previous probed demux failed to
     * to do so). */
    if (vlc_stream_Tell(demux->s) != 0 && vlc_stream_Seek(demux->s, 0))
    {
        msg_Err(demux, "seek failure before probing");
        return VLC_EGENERIC;
    }

    demux->obj.force = forced;

    int ret = probe(VLC_OBJECT(demux));
    if (ret)
        vlc_objres_clear(VLC_OBJECT(demux));
    return ret;
}

demux_t *demux_NewAdvanced( vlc_object_t *p_obj, input_thread_t *p_input,
                            const char *psz_demux, const char *url,
                            stream_t *s, es_out_t *out, bool b_preparsing )
{
    struct vlc_demux_private *priv;
    demux_t *p_demux = vlc_stream_CustomNew(p_obj, demux_DestroyDemux,
                                            sizeof (*priv), "demux");

    if (unlikely(p_demux == NULL))
        return NULL;

    assert(s != NULL);
    priv = vlc_stream_Private(p_demux);

    if (!strcasecmp( psz_demux, "any" ) || !psz_demux[0])
    {   /* Look up demux by mime-type for hard to detect formats */
        char *type = stream_MimeType( s );
        if( type != NULL )
        {
            psz_demux = demux_NameFromMimeType( type );
            free( type );
        }
    }

    p_demux->p_input_item = p_input ? input_GetItem(p_input) : NULL;
    p_demux->psz_name = strdup( psz_demux );
    if (unlikely(p_demux->psz_name == NULL))
        goto error;

    p_demux->psz_url = strdup(url);
    if (unlikely(p_demux->psz_url == NULL))
        goto error;

    const char *p = strstr(p_demux->psz_url, "://");
    p_demux->psz_location = (p != NULL) ? (p + 3) : "";
    p_demux->psz_filepath = get_path(p_demux->psz_location); /* parse URL */

    if( !b_preparsing )
        msg_Dbg( p_obj, "creating demux \"%s\", URL: %s, path: %s",
                 psz_demux, url, p_demux->psz_filepath );

    p_demux->s              = s;
    p_demux->out            = out;
    p_demux->b_preparsing   = b_preparsing;

    p_demux->pf_demux   = NULL;
    p_demux->pf_control = NULL;
    p_demux->p_sys      = NULL;

    const char *psz_module = NULL;

    if( !strcmp( p_demux->psz_name, "any" ) && p_demux->psz_filepath )
    {
        char const* psz_ext = strrchr( p_demux->psz_filepath, '.' );

        if( psz_ext )
            psz_module = DemuxNameFromExtension( psz_ext + 1, b_preparsing );
    }

    if( psz_module == NULL )
        psz_module = p_demux->psz_name;

    priv->module = vlc_module_load(p_demux, "demux", psz_module,
        !strcmp(psz_module, p_demux->psz_name), demux_Probe, p_demux);

    if (priv->module == NULL)
    {
        free( p_demux->psz_filepath );
        goto error;
    }

    return p_demux;
error:
    free( p_demux->psz_name );
    stream_CommonDelete( p_demux );
    return NULL;
}

#define static_control_match(foo) \
    static_assert((unsigned) DEMUX_##foo == STREAM_##foo, "Mismatch")

int demux_vaControl( demux_t *demux, int query, va_list args )
{
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
    vlc_tick_t i64;

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
            if( i_bitrate > 0 && i_end > i_start )
            {
                *va_arg( args, vlc_tick_t * ) = INT64_C(8000000) * (i_end - i_start) / i_bitrate;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_TIME:
            if( i_bitrate > 0 && i_tell >= i_start )
            {
                *va_arg( args, vlc_tick_t * ) = INT64_C(8000000) * (i_tell - i_start) / i_bitrate;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_POSITION:
            pf = va_arg( args, double * );
            if( i_start < i_end )
            {
                *pf = (double)( i_tell - i_start ) /
                      (double)( i_end  - i_start );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        case DEMUX_GET_NORMAL_TIME:
            return VLC_EGENERIC;

        case DEMUX_SET_POSITION:
            f = va_arg( args, double );
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
            i64 = va_arg( args, vlc_tick_t );
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
        case DEMUX_SET_GROUP_DEFAULT:
        case DEMUX_SET_GROUP_ALL:
        case DEMUX_SET_GROUP_LIST:
        case DEMUX_SET_ES:
        case DEMUX_SET_ES_LIST:
        case DEMUX_GET_ATTACHMENTS:
        case DEMUX_CAN_RECORD:
        case DEMUX_TEST_AND_CLEAR_FLAGS:
        case DEMUX_GET_TITLE:
        case DEMUX_GET_SEEKPOINT:
        case DEMUX_NAV_ACTIVATE:
        case DEMUX_NAV_UP:
        case DEMUX_NAV_DOWN:
        case DEMUX_NAV_LEFT:
        case DEMUX_NAV_RIGHT:
        case DEMUX_NAV_POPUP:
        case DEMUX_NAV_MENU:
        case DEMUX_FILTER_ENABLE:
        case DEMUX_FILTER_DISABLE:
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

    p_packetizer->pf_decode = NULL;
    p_packetizer->pf_packetize = NULL;

    p_packetizer->fmt_in = *p_fmt;
    es_format_Init( &p_packetizer->fmt_out, p_fmt->i_cat, 0 );

    p_packetizer->p_module = module_need( p_packetizer, "packetizer", NULL, false );
    if( !p_packetizer->p_module )
    {
        es_format_Clean( p_fmt );
        vlc_object_delete(p_packetizer);
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
    vlc_object_delete(p_packetizer);
}

unsigned demux_TestAndClearFlags( demux_t *p_demux, unsigned flags )
{
    unsigned update = flags;

    if (demux_Control( p_demux, DEMUX_TEST_AND_CLEAR_FLAGS, &update))
        return 0;
    return update;
}

int demux_GetTitle( demux_t *p_demux )
{
    int title;

    if (demux_Control(p_demux, DEMUX_GET_TITLE, &title))
        title = 0;
    return title;
}

int demux_GetSeekpoint( demux_t *p_demux )
{
    int seekpoint;

    if (demux_Control(p_demux, DEMUX_GET_SEEKPOINT, &seekpoint))
        seekpoint = 0;
    return seekpoint;
}

static demux_t *demux_FilterNew( demux_t *p_next, const char *p_name )
{
    struct vlc_demux_private *priv;
    demux_t *p_demux = vlc_stream_CustomNew(VLC_OBJECT(p_next),
                                            demux_DestroyDemux, sizeof (*priv),
                                            "demux filter");
    if (unlikely(p_demux == NULL))
        return NULL;

    priv = vlc_stream_Private(p_demux);
    p_demux->p_next       = p_next;
    p_demux->p_input_item = NULL;
    p_demux->p_sys        = NULL;
    p_demux->psz_name     = NULL;
    p_demux->psz_url      = NULL;
    p_demux->psz_location = NULL;
    p_demux->psz_filepath = NULL;
    p_demux->out          = NULL;

    priv->module = module_need(p_demux, "demux_filter", p_name,
                               p_name != NULL);
    if (priv->module == NULL)
        goto error;

    return p_demux;
error:
    stream_CommonDelete( p_demux );
    return NULL;
}

demux_t *demux_FilterChainNew( demux_t *p_demux, const char *psz_chain )
{
    if( !psz_chain || !*psz_chain )
        return NULL;

    char *psz_parser = strdup(psz_chain);
    if(!psz_parser)
        return NULL;

    /* parse chain */
    while(psz_parser)
    {
        config_chain_t *p_cfg;
        char *psz_name;
        char *psz_rest_chain = config_ChainCreate( &psz_name, &p_cfg, psz_parser );
        free( psz_parser );
        psz_parser = psz_rest_chain;

        demux_t *filter = demux_FilterNew(p_demux, psz_name);
        if (filter != NULL)
            p_demux = filter;

        free(psz_name);
        config_ChainDestroy(p_cfg);
    }

    return p_demux;
}

static bool demux_filter_enable_disable(demux_t *p_demux,
                                        const char *psz_demux, bool b_enable)
{
    struct vlc_demux_private *priv = vlc_stream_Private(p_demux);

    if ( psz_demux &&
        (strcmp(module_get_name(priv->module, false), psz_demux) == 0
      || strcmp(module_get_name(priv->module, true), psz_demux) == 0) )
    {
        demux_Control( p_demux,
                       b_enable ? DEMUX_FILTER_ENABLE : DEMUX_FILTER_DISABLE );
        return true;
    }
    return false;
}

bool demux_FilterEnable( demux_t *p_demux_chain, const char* psz_demux )
{
    return demux_filter_enable_disable( p_demux_chain, psz_demux, true );
}

bool demux_FilterDisable( demux_t *p_demux_chain, const char* psz_demux )
{
    return demux_filter_enable_disable( p_demux_chain, psz_demux, false );
}
