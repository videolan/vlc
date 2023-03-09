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

    demux_mapping *type = bsearch(mime, types, ARRAY_SIZE(types),
                                  sizeof (*types), demux_mapping_cmp);
    return (type != NULL) ? type->name : "any";
}

demux_t *demux_New( vlc_object_t *p_obj, const char *module, const char *url,
                    stream_t *s, es_out_t *out )
{
    assert(s != NULL );
    return demux_NewAdvanced( p_obj, NULL, module, url, s, out, false );
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
                            const char *module, const char *url,
                            stream_t *s, es_out_t *out, bool b_preparsing )
{
    const char *p = strchr(url, ':');
    if (p == NULL) {
        errno = EINVAL;
        return NULL;
    }

    struct vlc_demux_private *priv;
    demux_t *p_demux = vlc_stream_CustomNew(p_obj, demux_DestroyDemux,
                                            sizeof (*priv), "demux");

    if (unlikely(p_demux == NULL))
        return NULL;

    assert(s != NULL);
    priv = vlc_stream_Private(p_demux);

    p_demux->p_input_item = p_input ? input_GetItem(p_input) : NULL;
    p_demux->psz_name = strdup(module);
    if (unlikely(p_demux->psz_name == NULL))
        goto error;

    p_demux->psz_url = strdup(url);
    if (unlikely(p_demux->psz_url == NULL))
        goto error;

    p_demux->psz_location = p_demux->psz_url + 1 + (p - url);
    if (strncmp(p_demux->psz_location, "//", 2) == 0)
        p_demux->psz_location += 2;
    p_demux->psz_filepath = vlc_uri2path(url); /* parse URL */

    if( !b_preparsing )
        msg_Dbg( p_obj, "creating demux \"%s\", URL: %s, path: %s",
                 module, url, p_demux->psz_filepath );

    p_demux->s              = s;
    p_demux->out            = out;
    p_demux->b_preparsing   = b_preparsing;

    p_demux->pf_readdir = NULL;
    p_demux->pf_demux   = NULL;
    p_demux->pf_control = NULL;
    p_demux->p_sys      = NULL;
    p_demux->ops        = NULL;

    char *modbuf = NULL;
    bool strict = true;

    if (!strcasecmp(module, "any" ) || module[0] == '\0') {
        /* Look up demux by content type for hard to detect formats */
        char *type = stream_MimeType(s);

        if (type != NULL) {
            module = demux_NameFromMimeType(type);
            free(type);
        }
        strict = false;
    }

    if (strcasecmp(module, "any") == 0 && p_demux->psz_filepath != NULL)
    {
        const char *ext = strrchr(p_demux->psz_filepath, '.');

        if (ext != NULL) {
            if (b_preparsing && !vlc_ascii_strcasecmp(ext, ".mp3"))
                module = "mpga";
            else
            if (likely(asprintf(&modbuf, "ext-%s", ext + 1) >= 0))
                module = modbuf;
            else
                goto error;
        }
        strict = false;
    }

    priv->module = vlc_module_load(p_demux, "demux", module, strict,
                                   demux_Probe, p_demux);
    free(modbuf);

    if (priv->module == NULL)
        goto error;

    return p_demux;
error:
    free( p_demux->psz_filepath );
    free( p_demux->psz_name );
    stream_CommonDelete( p_demux );
    return NULL;
}

int demux_Demux(demux_t *demux)
{
    if (demux->pf_demux != NULL)
        return demux->pf_demux(demux);

    if (demux->pf_readdir != NULL && demux->p_input_item != NULL) {
        input_item_node_t *node = input_item_node_Create(demux->p_input_item);

        if (unlikely(node == NULL))
            return VLC_DEMUXER_EGENERIC;

        if (vlc_stream_ReadDir(demux, node)) {
             input_item_node_Delete(node);
             return VLC_DEMUXER_EGENERIC;
        }

        if (es_out_Control(demux->out, ES_OUT_POST_SUBNODE, node))
            input_item_node_Delete(node);
        return VLC_DEMUXER_EOF;
    }

    return VLC_DEMUXER_SUCCESS;
}

#define static_control_match(foo) \
    static_assert((unsigned) DEMUX_##foo == STREAM_##foo, "Mismatch")

int demux_vaControl( demux_t *demux, int query, va_list args )
{
    if (demux->ops == NULL)
        return demux->pf_control( demux, query, args );

    switch (query) {
        case DEMUX_CAN_SEEK:
        {
            bool *can_seek = va_arg(args, bool *);
            if (demux->ops->can_seek != NULL) {
                *can_seek = demux->ops->can_seek(demux);
            } else {
                *can_seek = false;
            }
            return VLC_SUCCESS;
        }
        case DEMUX_CAN_PAUSE:
        {
            bool *can_pause = va_arg(args, bool *);
            if (demux->ops->can_pause != NULL) {
                *can_pause = demux->ops->can_pause(demux);
            } else {
                *can_pause = false;
            }
            return VLC_SUCCESS;
        }
        case DEMUX_CAN_RECORD:
        {
            bool *can_record = va_arg(args, bool *);
            if (demux->ops->demux.can_record != NULL) {
                *can_record = demux->ops->demux.can_record(demux);
            } else {
                *can_record = false;
            }
            return VLC_SUCCESS;
        }
        case DEMUX_CAN_CONTROL_PACE:
        {
            bool *can_control_pace = va_arg(args, bool *);
            if (demux->ops->can_control_pace != NULL) {
                *can_control_pace = demux->ops->can_control_pace(demux);
            } else {
                *can_control_pace = false;
            }
            return VLC_SUCCESS;
        }
        case DEMUX_CAN_CONTROL_RATE:
        {
            bool *can_control_rate = va_arg(args, bool *);
            if (demux->ops->demux.can_control_rate != NULL) {
                *can_control_rate = demux->ops->demux.can_control_rate(demux);
            } else {
                *can_control_rate = false;
            }
            return VLC_SUCCESS;
        }
        case DEMUX_HAS_UNSUPPORTED_META:
        {
            bool *has_unsupported_meta = va_arg(args, bool *);
            if (demux->ops->demux.has_unsupported_meta != NULL) {
                *has_unsupported_meta = demux->ops->demux.has_unsupported_meta(demux);
            } else {
                *has_unsupported_meta = false;
            }
            return VLC_SUCCESS;
        }
        case DEMUX_GET_PTS_DELAY:
            if (demux->ops->get_pts_delay != NULL) {
                vlc_tick_t *pts_delay = va_arg(args, vlc_tick_t *);
                return demux->ops->get_pts_delay(demux, pts_delay);
            }
            return VLC_EGENERIC;
        case DEMUX_GET_TITLE_INFO:
            if (demux->ops->demux.get_title_info != NULL) {
                input_title_t ***title_info = va_arg(args, input_title_t ***);
                int *size = va_arg(args, int *);
                int *pi_title_offset = va_arg(args, int *);
                int *pi_seekpoint_offset = va_arg(args, int *);
                return demux->ops->demux.get_title_info(demux, title_info, size, pi_title_offset, pi_seekpoint_offset);
            }
            return VLC_EGENERIC;
        case DEMUX_GET_TITLE:
            if (demux->ops->demux.get_title != NULL) {
                int *title = va_arg(args, int *);
                return demux->ops->demux.get_title(demux, title);
            }
            return VLC_EGENERIC;
        case DEMUX_GET_SEEKPOINT:
            if (demux->ops->demux.get_seekpoint != NULL) {
                int *seekpoint = va_arg(args, int *);
                return demux->ops->demux.get_seekpoint(demux, seekpoint);
            }
            return VLC_EGENERIC;
        case DEMUX_GET_META:
            if (demux->ops->get_meta != NULL) {
                vlc_meta_t *meta = va_arg(args, vlc_meta_t *);
                return demux->ops->get_meta(demux, meta);
            }
            return VLC_EGENERIC;
        case DEMUX_GET_SIGNAL:
            if (demux->ops->get_signal != NULL) {
                double *quality = va_arg(args, double *);
                double *strength = va_arg(args, double *);
                return demux->ops->get_signal(demux, quality, strength);
            }
            return VLC_EGENERIC;
        case DEMUX_GET_TYPE:
            if (demux->ops->get_type != NULL) {
                int *type = va_arg(args, int *);
                return demux->ops->get_type(demux, type);
            }
            return VLC_EGENERIC;
        case DEMUX_GET_POSITION:
            if (demux->ops->demux.get_position != NULL) {
                *va_arg(args, double *) = demux->ops->demux.get_position(demux);
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        case DEMUX_GET_LENGTH:
            if (demux->ops->demux.get_length != NULL) {
                *va_arg(args, vlc_tick_t *) = demux->ops->demux.get_length(demux);
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        case DEMUX_GET_TIME:
            if (demux->ops->demux.get_time != NULL) {
                *va_arg(args, vlc_tick_t *) = demux->ops->demux.get_time(demux);
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        case DEMUX_GET_NORMAL_TIME:
            if (demux->ops->demux.get_normal_time != NULL) {
                vlc_tick_t *normal_time = va_arg(args, vlc_tick_t *);
                return demux->ops->demux.get_normal_time(demux, normal_time);
            }
            return VLC_EGENERIC;
        case DEMUX_GET_FPS:
            if (demux->ops->demux.get_fps != NULL) {
                double *fps = va_arg(args, double *);
                return demux->ops->demux.get_fps(demux, fps);
            }
            return VLC_EGENERIC;
        case DEMUX_GET_ATTACHMENTS:
            if (demux->ops->demux.get_attachments != NULL) {
                input_attachment_t ***attachments = va_arg(args, input_attachment_t ***);
                return demux->ops->demux.get_attachments(demux, attachments);
            }
            return VLC_EGENERIC;
        case DEMUX_SET_SEEKPOINT:
            if (demux->ops->set_seek_point != NULL) {
                int seekpoint = va_arg(args, int);
                return demux->ops->set_seek_point(demux, seekpoint);
            }
            return VLC_EGENERIC;
        case DEMUX_SET_TITLE:
            if (demux->ops->set_title != NULL) {
                int title = va_arg(args, int);
                return demux->ops->set_title(demux, title);
            }
            return VLC_EGENERIC;
        case DEMUX_SET_PAUSE_STATE:
            if (demux->ops->set_pause_state != NULL) {
                bool pause_state = (bool)va_arg(args, int);
                return demux->ops->set_pause_state(demux, pause_state);
            }
            return VLC_EGENERIC;
        case DEMUX_SET_RECORD_STATE:
            if (demux->ops->demux.set_record_state != NULL) {
                bool record_state = (bool)va_arg(args, int);
                const char *dir_path = NULL;
                if (record_state) {
                    dir_path = va_arg(args, const char *);
                }
                return demux->ops->demux.set_record_state(demux, record_state, dir_path);
            }
            return VLC_EGENERIC;
        case DEMUX_SET_POSITION:
        {
            if (demux->ops->demux.set_position != NULL) {
                double position = va_arg(args, double);
                bool precise = (bool)va_arg(args, int);
                return demux->ops->demux.set_position(demux, position, precise);
            }
            return VLC_EGENERIC;
        }
        case DEMUX_SET_TIME:
        {
            if (demux->ops->demux.set_time != NULL) {
                vlc_tick_t time = va_arg(args, vlc_tick_t);
                bool precise = (bool)va_arg(args, int);
                return demux->ops->demux.set_time(demux, time, precise);
            }
            return VLC_EGENERIC;
        }
        case DEMUX_SET_NEXT_DEMUX_TIME:
            if (demux->ops->demux.set_next_demux_time != NULL) {
                vlc_tick_t next_demux_time = va_arg(args, vlc_tick_t);
                return demux->ops->demux.set_next_demux_time(demux, next_demux_time);
            }
            return VLC_EGENERIC;
        case DEMUX_SET_RATE:
            if (demux->ops->demux.set_next_demux_time != NULL) {
                float *rate = va_arg(args, float *);
                return demux->ops->demux.set_rate(demux, rate);
            }
            return VLC_EGENERIC;
        case DEMUX_SET_GROUP_DEFAULT:
            if (demux->ops->demux.set_group_default != NULL) {
                return demux->ops->demux.set_group_default(demux);
            }
            return VLC_EGENERIC;
        case DEMUX_SET_GROUP_ALL:
            if (demux->ops->demux.set_group_all != NULL) {
                return demux->ops->demux.set_group_all(demux);
            }
            return VLC_EGENERIC;
        case DEMUX_SET_GROUP_LIST:
            if (demux->ops->demux.set_group_list != NULL) {
                size_t size = va_arg(args, size_t);
                const int *idx = va_arg(args, const int *);
                return demux->ops->demux.set_group_list(demux, size, idx);
            }
            return VLC_EGENERIC;
        case DEMUX_SET_ES:
            if (demux->ops->demux.set_es != NULL) {
                int es = va_arg(args, int);
                return demux->ops->demux.set_es(demux, es);
            }
            return VLC_EGENERIC;
        case DEMUX_SET_ES_LIST:
            if (demux->ops->demux.set_es_list != NULL) {
                size_t size = va_arg(args, size_t);
                const int *idx = va_arg(args, const int *);
                return demux->ops->demux.set_es_list(demux, size, idx);
            }
            return VLC_EGENERIC;
        case DEMUX_NAV_ACTIVATE:
            if (demux->ops->demux.nav_activate != NULL) {
                return demux->ops->demux.nav_activate(demux);
            }
            return VLC_EGENERIC;
        case DEMUX_NAV_UP:
            if (demux->ops->demux.nav_up != NULL) {
                return demux->ops->demux.nav_up(demux);
            }
            return VLC_EGENERIC;
        case DEMUX_NAV_DOWN:
            if (demux->ops->demux.nav_down != NULL) {
                return demux->ops->demux.nav_down(demux);
            }
            return VLC_EGENERIC;
        case DEMUX_NAV_LEFT:
            if (demux->ops->demux.nav_left != NULL) {
                return demux->ops->demux.nav_left(demux);
            }
            return VLC_EGENERIC;
        case DEMUX_NAV_RIGHT:
            if (demux->ops->demux.nav_right != NULL) {
                return demux->ops->demux.nav_right(demux);
            }
            return VLC_EGENERIC;
        case DEMUX_NAV_POPUP:
            if (demux->ops->demux.nav_popup != NULL) {
                return demux->ops->demux.nav_popup(demux);
            }
            return VLC_EGENERIC;
        case DEMUX_NAV_MENU:
            if (demux->ops->demux.nav_menu != NULL) {
                return demux->ops->demux.nav_menu(demux);
            }
            return VLC_EGENERIC;
        case DEMUX_FILTER_ENABLE:
            if (demux->ops->demux.filter_enable != NULL) {
                return demux->ops->demux.filter_enable(demux);
            }
            return VLC_EGENERIC;
        case DEMUX_FILTER_DISABLE:
            if (demux->ops->demux.filter_disable != NULL) {
                return demux->ops->demux.filter_disable(demux);
            }
            return VLC_EGENERIC;
        case DEMUX_TEST_AND_CLEAR_FLAGS:
            if (demux->ops->demux.test_and_clear_flags != NULL) {
                unsigned *flags = va_arg(args, unsigned *);
                return demux->ops->demux.test_and_clear_flags(demux, flags);
            }
            return VLC_EGENERIC;
        default:
            vlc_assert_unreachable();
    }
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
        case DEMUX_GET_TYPE:
        case DEMUX_SET_PAUSE_STATE:
            return vlc_stream_vaControl( s, i_query, args );

        case DEMUX_GET_LENGTH:
            if( i_bitrate > 0 && i_end > i_start )
            {
                *va_arg( args, vlc_tick_t * ) = vlc_tick_from_samples((i_end - i_start) * 8, i_bitrate);
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_TIME:
            if( i_bitrate > 0 && i_tell >= i_start )
            {
                *va_arg( args, vlc_tick_t * ) = vlc_tick_from_samples((i_tell - i_start) * 8, i_bitrate);
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
                int64_t i_block = samples_from_vlc_tick( i64, i_bitrate ) / (8 * i_align);
                if( vlc_stream_Seek( s, i_start + i_block * i_align ) )
                {
                    return VLC_EGENERIC;
                }
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

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

struct packetizer_owner
{
    decoder_t   packetizer;
    es_format_t fmt_in;
};

/****************************************************************************
 * Utility functions
 ****************************************************************************/
decoder_t *demux_PacketizerNew( vlc_object_t *p_demux, es_format_t *p_fmt, const char *psz_msg )
{
    struct packetizer_owner *owner = vlc_custom_create( p_demux, sizeof( *owner ),
                                      "demux packetizer" );
    if( !owner )
    {
        es_format_Clean( p_fmt );
        return NULL;
    }
    decoder_t *p_packetizer = &owner->packetizer;
    p_fmt->b_packetized = false;

    p_packetizer->pf_decode = NULL;
    p_packetizer->pf_packetize = NULL;

    owner->fmt_in = *p_fmt;
    p_packetizer->fmt_in = &owner->fmt_in;
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
    struct packetizer_owner *owner = container_of(p_packetizer, struct packetizer_owner, packetizer);
    if( p_packetizer->p_module )
        module_unneed( p_packetizer, p_packetizer->p_module );
    es_format_Clean( &owner->fmt_in );
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
    p_demux->s            = p_next;
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
        (strcmp(module_GetShortName(priv->module), psz_demux) == 0
      || strcmp(module_GetLongName(priv->module), psz_demux) == 0) )
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
