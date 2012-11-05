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

#include "demux.h"
#include <libvlc.h>
#include <vlc_codec.h>
#include <vlc_meta.h>
#include <vlc_url.h>
#include <vlc_modules.h>

static bool SkipID3Tag( demux_t * );
static bool SkipAPETag( demux_t *p_demux );

/* Decode URL (which has had its scheme stripped earlier) to a file path. */
/* XXX: evil code duplication from access.c */
static char *get_path(const char *location)
{
    char *url, *path;

    /* Prepending "file://" is a bit hackish. But then again, we do not want
     * to hard-code the list of schemes that use file paths in make_path().
     */
    if (asprintf(&url, "file://%s", location) == -1)
        return NULL;

    path = make_path (url);
    free (url);
    return path;
}

#undef demux_New
/*****************************************************************************
 * demux_New:
 *  if s is NULL then load a access_demux
 *****************************************************************************/
demux_t *demux_New( vlc_object_t *p_obj, input_thread_t *p_parent_input,
                    const char *psz_access, const char *psz_demux,
                    const char *psz_location,
                    stream_t *s, es_out_t *out, bool b_quick )
{
    demux_t *p_demux = vlc_custom_create( p_obj, sizeof( *p_demux ), "demux" );
    const char *psz_module;

    if( p_demux == NULL ) return NULL;

    p_demux->p_input = p_parent_input;

    /* Parse URL */
    p_demux->psz_access = strdup( psz_access );
    p_demux->psz_demux  = strdup( psz_demux );
    p_demux->psz_location = strdup( psz_location );
    p_demux->psz_file = get_path( psz_location );

    /* Take into account "demux" to be able to do :demux=dump */
    if( *p_demux->psz_demux == '\0' )
    {
        free( p_demux->psz_demux );
        p_demux->psz_demux = var_GetNonEmptyString( p_obj, "demux" );
        if( p_demux->psz_demux == NULL )
            p_demux->psz_demux = strdup( "" );
    }

    if( !b_quick )
        msg_Dbg( p_obj, "creating demux: access='%s' demux='%s' "
                 "location='%s' file='%s'",
                 p_demux->psz_access, p_demux->psz_demux,
                 p_demux->psz_location, p_demux->psz_file );

    p_demux->s          = s;
    p_demux->out        = out;

    p_demux->pf_demux   = NULL;
    p_demux->pf_control = NULL;
    p_demux->p_sys      = NULL;
    p_demux->info.i_update = 0;
    p_demux->info.i_title  = 0;
    p_demux->info.i_seekpoint = 0;

    if( s ) psz_module = p_demux->psz_demux;
    else psz_module = p_demux->psz_access;

    const char *psz_ext;

    if( s && *psz_module == '\0'
     && p_demux->psz_file != NULL
     && (psz_ext = strrchr( p_demux->psz_file, '.' )) )
    {
       /* XXX: add only file without any problem here and with strong detection.
        *  - no .mp3, .a52, ...
        *  - wav can't be added 'cause of a52 and dts in them as raw audio
         */
         static const struct { char ext[5]; char demux[9]; } exttodemux[] =
         {
            { "aiff", "aiff" },
            { "asf",  "asf" }, { "wmv",  "asf" }, { "wma",  "asf" },
            { "avi",  "avi" },
            { "au",   "au" },
            { "flac", "flac" },
            { "dv",   "dv" },
            { "drc",  "dirac" },
            { "m3u",  "m3u" },
            { "m3u8", "m3u8" },
            { "mkv",  "mkv" }, { "mka",  "mkv" }, { "mks",  "mkv" },
            { "mp4",  "mp4" }, { "m4a",  "mp4" }, { "mov",  "mp4" }, { "moov", "mp4" },
            { "nsv",  "nsv" },
            { "ogg",  "ogg" }, { "ogm",  "ogg" }, /* legacy Ogg */
            { "oga",  "ogg" }, { "spx",  "ogg" }, { "ogv", "ogg" },
            { "ogx",  "ogg" }, /*RFC5334*/
            { "opus", "ogg" }, /*draft-terriberry-oggopus-01*/
            { "pva",  "pva" },
            { "rm",   "avformat" },
            { "m4v",  "m4v" },
            { "h264", "h264" },
            { "voc",  "voc" },
            { "mid",  "smf" }, { "rmi",  "smf" }, { "kar", "smf" },
            { "",  "" },
        };
        /* Here, we don't mind if it does not work, it must be quick */
        static const struct { char ext[4]; char demux[5]; } exttodemux_quick[] =
        {
            { "mp3", "mpga" },
            { "ogg", "ogg" },
            { "wma", "asf" },
            { "", "" }
        };

        psz_ext++; // skip '.'

        if( !b_quick )
        {
            for( unsigned i = 0; exttodemux[i].ext[0]; i++ )
            {
                if( !strcasecmp( psz_ext, exttodemux[i].ext ) )
                {
                    psz_module = exttodemux[i].demux;
                    break;
                }
            }
        }
        else
        {
            for( unsigned i = 0; exttodemux_quick[i].ext[0]; i++ )
            {
                if( !strcasecmp( psz_ext, exttodemux_quick[i].ext ) )
                {
                    psz_module = exttodemux_quick[i].demux;
                    break;
                }
            }

        }
    }

    if( s )
    {
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
            module_need( p_demux, "access_demux", psz_module,
                         !strcmp( psz_module, p_demux->psz_access ) );
    }

    if( p_demux->p_module == NULL )
    {
        free( p_demux->psz_file );
        free( p_demux->psz_location );
        free( p_demux->psz_demux );
        free( p_demux->psz_access );
        vlc_object_release( p_demux );
        return NULL;
    }

    return p_demux;
}

/*****************************************************************************
 * demux_Delete:
 *****************************************************************************/
void demux_Delete( demux_t *p_demux )
{
    module_unneed( p_demux, p_demux->p_module );

    free( p_demux->psz_file );
    free( p_demux->psz_location );
    free( p_demux->psz_demux );
    free( p_demux->psz_access );

    vlc_object_release( p_demux );
}

/*****************************************************************************
 * demux_GetParentInput:
 *****************************************************************************/
input_thread_t * demux_GetParentInput( demux_t *p_demux )
{
    return p_demux->p_input ? vlc_object_hold((vlc_object_t*)p_demux->p_input) : NULL;
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
    i_tell = stream_Tell( s );

    switch( i_query )
    {
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

                if( stream_Seek( s, i_start + i_block * i_align ) )
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
                if( stream_Seek( s, i_start + i_block * i_align ) )
                {
                    return VLC_EGENERIC;
                }
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_PTS_DELAY:
        case DEMUX_GET_FPS:
        case DEMUX_GET_META:
        case DEMUX_HAS_UNSUPPORTED_META:
        case DEMUX_SET_NEXT_DEMUX_TIME:
        case DEMUX_GET_TITLE_INFO:
        case DEMUX_SET_GROUP:
        case DEMUX_GET_ATTACHMENTS:
        case DEMUX_CAN_RECORD:
        case DEMUX_SET_RECORD_STATE:
            return VLC_EGENERIC;

        default:
            msg_Err( s, "unknown query in demux_vaControlDefault" );
            return VLC_EGENERIC;
    }
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
    if( stream_Peek( p_demux->s, &p_peek, 10 ) < 10 )
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
    stream_Read( p_demux->s, NULL, i_size );

    msg_Dbg( p_demux, "ID3v2.%d revision %d tag found, skipping %d bytes",
             version, revision, i_size );
    return true;
}
static bool SkipAPETag( demux_t *p_demux )
{
    const uint8_t *p_peek;
    int i_version;
    int i_size;
    uint32_t flags;

    if( !p_demux->s )
        return false;

    /* Get 32 byte ape header */
    if( stream_Peek( p_demux->s, &p_peek, 32 ) < 32 )
        return false;

    if( memcmp( p_peek, "APETAGEX", 8 ) )
        return false;

    i_version = GetDWLE( &p_peek[8] );
    flags = GetDWLE( &p_peek[8+4+4] );
    if( ( i_version != 1000 && i_version != 2000 ) || !( flags & (1<<29) ) )
        return false;

    i_size = GetDWLE( &p_peek[8+4] ) + ( (flags&(1<<30)) ? 32 : 0 );

    /* Skip the entire tag */
    stream_Read( p_demux->s, NULL, i_size );

    msg_Dbg( p_demux, "AP2 v%d tag found, skipping %d bytes",
             i_version/1000, i_size );
    return true;
}

