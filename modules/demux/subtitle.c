/*****************************************************************************
 * subtitle.c: Demux for subtitle text files.
 *****************************************************************************
 * Copyright (C) 1999-2007 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <ctype.h>
#include <math.h>
#include <assert.h>

#include <vlc_demux.h>
#include <vlc_charset.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t *p_this );
static void Close( vlc_object_t *p_this );

#define SUB_TYPE_LONGTEXT \
    N_("Force the subtiles format. Selecting \"auto\" means autodetection and should always work.")
#define SUB_DESCRIPTION_LONGTEXT \
    N_("Override the default track description.")

static const char *const ppsz_sub_type[] =
{
    "auto", "microdvd", "subrip", "subviewer", "ssa1",
    "ssa2-4", "ass", "vplayer", "sami", "dvdsubtitle", "mpl2",
    "aqt", "pjs", "mpsub", "jacosub", "psb", "realtext", "dks",
    "subviewer1", "sbv"
};

vlc_module_begin ()
    set_shortname( N_("Subtitles"))
    set_description( N_("Text subtitle parser") )
    set_capability( "demux", 0 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    add_string( "sub-type", "auto", N_("Subtitle format"),
                SUB_TYPE_LONGTEXT, true )
        change_string_list( ppsz_sub_type, ppsz_sub_type )
    add_string( "sub-description", NULL, N_("Subtitle description"),
                SUB_DESCRIPTION_LONGTEXT, true )
    set_callbacks( Open, Close )

    add_shortcut( "subtitle" )
vlc_module_end ()

/*****************************************************************************
 * Prototypes:
 *****************************************************************************/
enum subtitle_type_e
{
    SUB_TYPE_UNKNOWN = -1,
    SUB_TYPE_MICRODVD,
    SUB_TYPE_SUBRIP,
    SUB_TYPE_SSA1,
    SUB_TYPE_SSA2_4,
    SUB_TYPE_ASS,
    SUB_TYPE_VPLAYER,
    SUB_TYPE_SAMI,
    SUB_TYPE_SUBVIEWER, /* SUBVIEWER 2 */
    SUB_TYPE_DVDSUBTITLE, /* Mplayer calls it subviewer2 */
    SUB_TYPE_MPL2,
    SUB_TYPE_AQT,
    SUB_TYPE_PJS,
    SUB_TYPE_MPSUB,
    SUB_TYPE_JACOSUB,
    SUB_TYPE_PSB,
    SUB_TYPE_RT,
    SUB_TYPE_DKS,
    SUB_TYPE_SUBVIEW1, /* SUBVIEWER 1 - mplayer calls it subrip09,
                         and Gnome subtitles SubViewer 1.0 */
    SUB_TYPE_SBV,
    SUB_TYPE_SCC,      /* Scenarist Closed Caption */
};

typedef struct
{
    size_t  i_line_count;
    size_t  i_line;
    char    **line;
} text_t;

static int  TextLoad( text_t *, stream_t *s );
static void TextUnload( text_t * );

typedef struct
{
    vlc_tick_t i_start;
    vlc_tick_t i_stop;

    char    *psz_text;
} subtitle_t;

typedef struct
{
    enum subtitle_type_e i_type;
    vlc_tick_t  i_microsecperframe;

    char        *psz_header; /* SSA */

    struct
    {
        bool b_inited;

        int i_comment;
        int i_time_resolution;
        int i_time_shift;
    } jss;

    struct
    {
        bool  b_inited;

        float f_total;
        int i_factor;
    } mpsub;

    struct
    {
        const char *psz_start;
    } sami;

} subs_properties_t;

typedef struct
{
    es_out_id_t *es;
    bool        b_slave;
    bool        b_first_time;

    double      f_rate;
    vlc_tick_t  i_next_demux_date;

    struct
    {
        subtitle_t *p_array;
        size_t      i_count;
        size_t      i_current;
    } subtitles;

    vlc_tick_t  i_length;

    /* */
    subs_properties_t props;

    block_t * (*pf_convert)( const subtitle_t * );
} demux_sys_t;

static int  ParseMicroDvd   ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParseSubRip     ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParseSubViewer  ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParseSSA        ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParseVplayer    ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParseSami       ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParseDVDSubtitle( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParseMPL2       ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParseAQT        ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParsePJS        ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParseMPSub      ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParseJSS        ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParsePSB        ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParseRealText   ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParseDKS        ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParseSubViewer1 ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParseCommonSBV  ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );
static int  ParseSCC        ( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t *, size_t );

static const struct
{
    const char *psz_type_name;
    int  i_type;
    const char *psz_name;
    int  (*pf_read)( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t*, size_t );
} sub_read_subtitle_function [] =
{
    { "microdvd",   SUB_TYPE_MICRODVD,    "MicroDVD",    ParseMicroDvd },
    { "subrip",     SUB_TYPE_SUBRIP,      "SubRIP",      ParseSubRip },
    { "subviewer",  SUB_TYPE_SUBVIEWER,   "SubViewer",   ParseSubViewer },
    { "ssa1",       SUB_TYPE_SSA1,        "SSA-1",       ParseSSA },
    { "ssa2-4",     SUB_TYPE_SSA2_4,      "SSA-2/3/4",   ParseSSA },
    { "ass",        SUB_TYPE_ASS,         "SSA/ASS",     ParseSSA },
    { "vplayer",    SUB_TYPE_VPLAYER,     "VPlayer",     ParseVplayer },
    { "sami",       SUB_TYPE_SAMI,        "SAMI",        ParseSami },
    { "dvdsubtitle",SUB_TYPE_DVDSUBTITLE, "DVDSubtitle", ParseDVDSubtitle },
    { "mpl2",       SUB_TYPE_MPL2,        "MPL2",        ParseMPL2 },
    { "aqt",        SUB_TYPE_AQT,         "AQTitle",     ParseAQT },
    { "pjs",        SUB_TYPE_PJS,         "PhoenixSub",  ParsePJS },
    { "mpsub",      SUB_TYPE_MPSUB,       "MPSub",       ParseMPSub },
    { "jacosub",    SUB_TYPE_JACOSUB,     "JacoSub",     ParseJSS },
    { "psb",        SUB_TYPE_PSB,         "PowerDivx",   ParsePSB },
    { "realtext",   SUB_TYPE_RT,          "RealText",    ParseRealText },
    { "dks",        SUB_TYPE_DKS,         "DKS",         ParseDKS },
    { "subviewer1", SUB_TYPE_SUBVIEW1,    "Subviewer 1", ParseSubViewer1 },
    { "sbv",        SUB_TYPE_SBV,         "SBV",         ParseCommonSBV },
    { "scc",        SUB_TYPE_SCC,         "SCC",         ParseSCC },
    { NULL,         SUB_TYPE_UNKNOWN,     "Unknown",     NULL }
};
/* When adding support for more formats, be sure to add their file extension
 * to src/input/subtitles.c to enable auto-detection.
 */

static int Demux( demux_t * );
static int Control( demux_t *, int, va_list );

static void Fix( demux_t * );
static char * get_language_from_filename( const char * );

/*****************************************************************************
 * Decoder format output function
 *****************************************************************************/

static block_t *ToTextBlock( const subtitle_t *p_subtitle )
{
    block_t *p_block;
    size_t i_len = strlen( p_subtitle->psz_text ) + 1;

    if( i_len <= 1 || !(p_block = block_Alloc( i_len )) )
        return NULL;

    memcpy( p_block->p_buffer, p_subtitle->psz_text, i_len );

    return p_block;
}

static block_t *ToEIA608Block( const subtitle_t *p_subtitle )
{
    block_t *p_block;
    const size_t i_len = strlen( p_subtitle->psz_text );
    const size_t i_block = (1 + i_len / 5) * 3;

    if( i_len < 4 || !(p_block = block_Alloc( i_block )) )
        return NULL;

    p_block->i_buffer = 0;

    char *saveptr = NULL;
    char *psz_tok = strtok_r( p_subtitle->psz_text, " ", &saveptr );
    unsigned a, b;
    while( psz_tok &&
           sscanf( psz_tok, "%2x%2x", &a, &b ) == 2 &&
           i_block - p_block->i_buffer >= 3 )
    {
        uint8_t *p_data = &p_block->p_buffer[p_block->i_buffer];
        p_data[0] = 0xFC;
        p_data[1] = a;
        p_data[2] = b;
        p_block->i_buffer += 3;
        psz_tok = strtok_r( NULL, " ", &saveptr );
    }

    return p_block;
}

/*****************************************************************************
 * Module initializer
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    demux_t        *p_demux = (demux_t*)p_this;
    demux_sys_t    *p_sys;
    es_format_t    fmt;
    float          f_fps;
    char           *psz_type;
    int  (*pf_read)( vlc_object_t *, subs_properties_t *, text_t *, subtitle_t*, size_t );

    if( !p_demux->obj.force )
    {
        msg_Dbg( p_demux, "subtitle demux discarded" );
        return VLC_EGENERIC;
    }

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->b_slave = false;
    p_sys->b_first_time = true;
    p_sys->i_next_demux_date = 0;
    p_sys->f_rate = 1.0;

    p_sys->pf_convert = ToTextBlock;

    p_sys->subtitles.i_current= 0;
    p_sys->subtitles.i_count  = 0;
    p_sys->subtitles.p_array  = NULL;

    p_sys->props.psz_header         = NULL;
    p_sys->props.i_microsecperframe = VLC_TICK_FROM_MS(40);
    p_sys->props.jss.b_inited       = false;
    p_sys->props.mpsub.b_inited     = false;
    p_sys->props.sami.psz_start     = NULL;

    /* Get the FPS */
    f_fps = var_CreateGetFloat( p_demux, "sub-original-fps" );
    if( f_fps >= 1.f )
    {
        p_sys->props.i_microsecperframe = llroundf( (float)CLOCK_FREQ / f_fps );
        msg_Dbg( p_demux, "Override subtitle fps %f", (double) f_fps );
    }

    /* Get or probe the type */
    p_sys->props.i_type = SUB_TYPE_UNKNOWN;
    psz_type = var_CreateGetString( p_demux, "sub-type" );
    if( psz_type && *psz_type )
    {
        for( int i = 0; ; i++ )
        {
            if( sub_read_subtitle_function[i].psz_type_name == NULL )
                break;

            if( !strcmp( sub_read_subtitle_function[i].psz_type_name,
                         psz_type ) )
            {
                p_sys->props.i_type = sub_read_subtitle_function[i].i_type;
                break;
            }
        }
    }
    free( psz_type );

#ifndef NDEBUG
    const uint64_t i_start_pos = vlc_stream_Tell( p_demux->s );
#endif

    size_t i_peek;
    const uint8_t *p_peek;
    if( vlc_stream_Peek( p_demux->s, &p_peek, 16 ) < 16 )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    enum
    {
        UTF8BOM,
        UTF16LE,
        UTF16BE,
        NOBOM,
    } e_bom = NOBOM;
    const char *psz_bom = NULL;

    i_peek = 4096;
    /* Detect Unicode while skipping the UTF-8 Byte Order Mark */
    if( !memcmp( p_peek, "\xEF\xBB\xBF", 3 ) )
    {
        e_bom = UTF8BOM;
        psz_bom = "UTF-8";
    }
    else if( !memcmp( p_peek, "\xFF\xFE", 2 ) )
    {
        e_bom = UTF16LE;
        psz_bom = "UTF-16LE";
        i_peek *= 2;
    }
    else if( !memcmp( p_peek, "\xFE\xFF", 2 ) )
    {
        e_bom = UTF16BE;
        psz_bom = "UTF-16BE";
        i_peek *= 2;
    }

    if( e_bom != NOBOM )
        msg_Dbg( p_demux, "detected %s Byte Order Mark", psz_bom );

    i_peek = vlc_stream_Peek( p_demux->s, &p_peek, i_peek );
    if( unlikely(i_peek < 16) )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    stream_t *p_probestream = NULL;
    if( e_bom != UTF8BOM && e_bom != NOBOM )
    {
        if( i_peek > 16 )
        {
            char *p_outbuf = FromCharset( psz_bom, p_peek, i_peek );
            if( p_outbuf != NULL )
                p_probestream = vlc_stream_MemoryNew( p_demux, (uint8_t *)p_outbuf,
                                                      strlen( p_outbuf ),
                                                      false ); /* free p_outbuf on release */
        }
    }
    else
    {
        const size_t i_skip = (e_bom == UTF8BOM) ? 3 : 0;
        p_probestream = vlc_stream_MemoryNew( p_demux, (uint8_t *) &p_peek[i_skip],
                                              i_peek - i_skip, true );
    }

    if( p_probestream == NULL )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Probe if unknown type */
    if( p_sys->props.i_type == SUB_TYPE_UNKNOWN )
    {
        int     i_try;
        char    *s = NULL;

        msg_Dbg( p_demux, "autodetecting subtitle format" );
        for( i_try = 0; i_try < 256; i_try++ )
        {
            int i_dummy;
            char p_dummy;

            if( (s = vlc_stream_ReadLine( p_probestream ) ) == NULL )
                break;

            if( strcasestr( s, "<SAMI>" ) )
            {
                p_sys->props.i_type = SUB_TYPE_SAMI;
                break;
            }
            else if( sscanf( s, "{%d}{%d}", &i_dummy, &i_dummy ) == 2 ||
                     sscanf( s, "{%d}{}", &i_dummy ) == 1)
            {
                p_sys->props.i_type = SUB_TYPE_MICRODVD;
                break;
            }
            else if( sscanf( s, "%d:%d:%d,%d --> %d:%d:%d,%d",
                             &i_dummy,&i_dummy,&i_dummy,&i_dummy,
                             &i_dummy,&i_dummy,&i_dummy,&i_dummy ) == 8 ||
                     sscanf( s, "%d:%d:%d --> %d:%d:%d,%d",
                             &i_dummy,&i_dummy,&i_dummy,&i_dummy,
                             &i_dummy,&i_dummy,&i_dummy ) == 7 ||
                     sscanf( s, "%d:%d:%d,%d --> %d:%d:%d",
                             &i_dummy,&i_dummy,&i_dummy,&i_dummy,
                             &i_dummy,&i_dummy,&i_dummy ) == 7 ||
                     sscanf( s, "%d:%d:%d.%d --> %d:%d:%d.%d",
                             &i_dummy,&i_dummy,&i_dummy,&i_dummy,
                             &i_dummy,&i_dummy,&i_dummy,&i_dummy ) == 8 ||
                     sscanf( s, "%d:%d:%d --> %d:%d:%d.%d",
                             &i_dummy,&i_dummy,&i_dummy,&i_dummy,
                             &i_dummy,&i_dummy,&i_dummy ) == 7 ||
                     sscanf( s, "%d:%d:%d.%d --> %d:%d:%d",
                             &i_dummy,&i_dummy,&i_dummy,&i_dummy,
                             &i_dummy,&i_dummy,&i_dummy ) == 7 ||
                     sscanf( s, "%d:%d:%d --> %d:%d:%d",
                             &i_dummy,&i_dummy,&i_dummy,
                             &i_dummy,&i_dummy,&i_dummy ) == 6 )
            {
                p_sys->props.i_type = SUB_TYPE_SUBRIP;
                break;
            }
            else if( !strncasecmp( s, "!: This is a Sub Station Alpha v1", 33 ) )
            {
                p_sys->props.i_type = SUB_TYPE_SSA1;
                break;
            }
            else if( !strncasecmp( s, "ScriptType: v4.00+", 18 ) )
            {
                p_sys->props.i_type = SUB_TYPE_ASS;
                break;
            }
            else if( !strncasecmp( s, "ScriptType: v4.00", 17 ) )
            {
                p_sys->props.i_type = SUB_TYPE_SSA2_4;
                break;
            }
            else if( !strncasecmp( s, "Dialogue: Marked", 16  ) )
            {
                p_sys->props.i_type = SUB_TYPE_SSA2_4;
                break;
            }
            else if( !strncasecmp( s, "Dialogue:", 9  ) )
            {
                p_sys->props.i_type = SUB_TYPE_ASS;
                break;
            }
            else if( strcasestr( s, "[INFORMATION]" ) )
            {
                p_sys->props.i_type = SUB_TYPE_SUBVIEWER; /* I hope this will work */
                break;
            }
            else if( sscanf( s, "%d:%d:%d.%d %d:%d:%d",
                                 &i_dummy, &i_dummy, &i_dummy, &i_dummy,
                                 &i_dummy, &i_dummy, &i_dummy ) == 7 ||
                     sscanf( s, "@%d @%d", &i_dummy, &i_dummy) == 2)
            {
                p_sys->props.i_type = SUB_TYPE_JACOSUB;
                break;
            }
            else if( sscanf( s, "%d:%d:%d.%d,%d:%d:%d.%d",
                                 &i_dummy, &i_dummy, &i_dummy, &i_dummy,
                                 &i_dummy, &i_dummy, &i_dummy, &i_dummy ) == 8 )
            {
                p_sys->props.i_type = SUB_TYPE_SBV;
                break;
            }
            else if( sscanf( s, "%d:%d:%d:", &i_dummy, &i_dummy, &i_dummy ) == 3 ||
                     sscanf( s, "%d:%d:%d ", &i_dummy, &i_dummy, &i_dummy ) == 3 )
            {
                p_sys->props.i_type = SUB_TYPE_VPLAYER;
                break;
            }
            else if( sscanf( s, "{T %d:%d:%d:%d", &i_dummy, &i_dummy,
                             &i_dummy, &i_dummy ) == 4 )
            {
                p_sys->props.i_type = SUB_TYPE_DVDSUBTITLE;
                break;
            }
            else if( sscanf( s, "[%d:%d:%d]%c",
                     &i_dummy, &i_dummy, &i_dummy, &p_dummy ) == 4 )
            {
                p_sys->props.i_type = SUB_TYPE_DKS;
                break;
            }
            else if( strstr( s, "*** START SCRIPT" ) )
            {
                p_sys->props.i_type = SUB_TYPE_SUBVIEW1;
                break;
            }
            else if( sscanf( s, "[%d][%d]", &i_dummy, &i_dummy ) == 2 ||
                     sscanf( s, "[%d][]", &i_dummy ) == 1)
            {
                p_sys->props.i_type = SUB_TYPE_MPL2;
                break;
            }
            else if( sscanf (s, "FORMAT=%d", &i_dummy) == 1 ||
                     ( sscanf (s, "FORMAT=TIM%c", &p_dummy) == 1
                       && p_dummy =='E' ) )
            {
                p_sys->props.i_type = SUB_TYPE_MPSUB;
                break;
            }
            else if( sscanf( s, "-->> %d", &i_dummy) == 1 )
            {
                p_sys->props.i_type = SUB_TYPE_AQT;
                break;
            }
            else if( sscanf( s, "%d,%d,", &i_dummy, &i_dummy ) == 2 )
            {
                p_sys->props.i_type = SUB_TYPE_PJS;
                break;
            }
            else if( sscanf( s, "{%d:%d:%d}",
                                &i_dummy, &i_dummy, &i_dummy ) == 3 )
            {
                p_sys->props.i_type = SUB_TYPE_PSB;
                break;
            }
            else if( strcasestr( s, "<time" ) )
            {
                p_sys->props.i_type = SUB_TYPE_RT;
                break;
            }
            else if( !strncasecmp( s, "WEBVTT",6 ) )
            {
                /* FAIL */
                break;
            }
            else if( !strncasecmp( s, "Scenarist_SCC V1.0", 18 ) )
            {
                p_sys->props.i_type = SUB_TYPE_SCC;
                p_sys->pf_convert = ToEIA608Block;
                break;
            }

            free( s );
            s = NULL;
        }

        free( s );
    }

    vlc_stream_Delete( p_probestream );

    /* Quit on unknown subtitles */
    if( p_sys->props.i_type == SUB_TYPE_UNKNOWN )
    {
#ifndef NDEBUG
        /* Ensure it will work with non seekable streams */
        assert( i_start_pos == vlc_stream_Tell( p_demux->s ) );
#endif
        msg_Warn( p_demux, "failed to recognize subtitle type" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    for( int i = 0; ; i++ )
    {
        if( sub_read_subtitle_function[i].i_type == p_sys->props.i_type )
        {
            msg_Dbg( p_demux, "detected %s format",
                     sub_read_subtitle_function[i].psz_name );
            pf_read = sub_read_subtitle_function[i].pf_read;
            break;
        }
    }

    msg_Dbg( p_demux, "loading all subtitles..." );

    if( e_bom == UTF8BOM && /* skip BOM */
        vlc_stream_Read( p_demux->s, NULL, 3 ) != 3 )
    {
        Close( p_this );
        return VLC_EGENERIC;
    }

    /* Load the whole file */
    text_t txtlines;
    TextLoad( &txtlines, p_demux->s );

    /* Parse it */
    for( size_t i_max = 0; i_max < SIZE_MAX - 500 * sizeof(subtitle_t); )
    {
        if( p_sys->subtitles.i_count >= i_max )
        {
            i_max += 500;
            subtitle_t *p_realloc = realloc( p_sys->subtitles.p_array, sizeof(subtitle_t) * i_max );
            if( p_realloc == NULL )
            {
                TextUnload( &txtlines );
                Close( p_this );
                return VLC_ENOMEM;
            }
            p_sys->subtitles.p_array = p_realloc;
        }

        if( pf_read( VLC_OBJECT(p_demux), &p_sys->props, &txtlines,
                     &p_sys->subtitles.p_array[p_sys->subtitles.i_count],
                     p_sys->subtitles.i_count ) )
            break;

        p_sys->subtitles.i_count++;
    }
    /* Unload */
    TextUnload( &txtlines );

    msg_Dbg(p_demux, "loaded %zu subtitles", p_sys->subtitles.i_count );

    /* *** add subtitle ES *** */
    if( p_sys->props.i_type == SUB_TYPE_SSA1 ||
             p_sys->props.i_type == SUB_TYPE_SSA2_4 ||
             p_sys->props.i_type == SUB_TYPE_ASS )
    {
        Fix( p_demux );
        es_format_Init( &fmt, SPU_ES, VLC_CODEC_SSA );
    }
    else if( p_sys->props.i_type == SUB_TYPE_SCC )
    {
        es_format_Init( &fmt, SPU_ES, VLC_CODEC_CEA608 );
        fmt.subs.cc.i_reorder_depth = -1;
    }
    else
        es_format_Init( &fmt, SPU_ES, VLC_CODEC_SUBT );

    p_sys->subtitles.i_current = 0;
    p_sys->i_length = 0;
    if( p_sys->subtitles.i_count > 0 )
        p_sys->i_length = p_sys->subtitles.p_array[p_sys->subtitles.i_count-1].i_stop;

    /* Stupid language detection in the filename */
    char * psz_language = get_language_from_filename( p_demux->psz_filepath );

    if( psz_language )
    {
        fmt.psz_language = psz_language;
        msg_Dbg( p_demux, "detected language %s of subtitle: %s", psz_language,
                 p_demux->psz_location );
    }

    char *psz_description = var_InheritString( p_demux, "sub-description" );
    if( psz_description && *psz_description )
        fmt.psz_description = psz_description;
    else
        free( psz_description );
    if( p_sys->props.psz_header != NULL &&
       (fmt.p_extra = strdup( p_sys->props.psz_header )) )
    {
        fmt.i_extra = strlen( p_sys->props.psz_header ) + 1;
    }

    fmt.i_id = 0;
    p_sys->es = es_out_Add( p_demux->out, &fmt );
    es_format_Clean( &fmt );
    if( p_sys->es == NULL )
    {
        Close( p_this );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: Close subtitle demux
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    for( size_t i = 0; i < p_sys->subtitles.i_count; i++ )
        free( p_sys->subtitles.p_array[i].psz_text );
    free( p_sys->subtitles.p_array );
    free( p_sys->props.psz_header );

    free( p_sys );
}

static void
ResetCurrentIndex( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    for( size_t i = 0; i < p_sys->subtitles.i_count; i++ )
    {
        if( p_sys->subtitles.p_array[i].i_start * p_sys->f_rate >
            p_sys->i_next_demux_date && i > 0 )
            break;
        p_sys->subtitles.i_current = i;
    }
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double *pf, f;

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            *va_arg( args, bool * ) = true;
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            *va_arg( args, vlc_tick_t * ) = p_sys->i_length;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            *va_arg( args, vlc_tick_t * ) = p_sys->i_next_demux_date;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
        {
            p_sys->b_first_time = true;
            p_sys->i_next_demux_date = va_arg( args, vlc_tick_t );
            ResetCurrentIndex( p_demux );
            return VLC_SUCCESS;
        }

        case DEMUX_GET_POSITION:
            pf = va_arg( args, double * );
            if( p_sys->subtitles.i_current >= p_sys->subtitles.i_count )
            {
                *pf = 1.0;
            }
            else if( p_sys->subtitles.i_count > 0 && p_sys->i_length )
            {
                *pf = p_sys->i_next_demux_date;
                *pf /= p_sys->i_length;
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = va_arg( args, double );
            if( p_sys->subtitles.i_count && p_sys->i_length )
            {
                vlc_tick_t i64 = VLC_TICK_0 + f * p_sys->i_length;
                return demux_Control( p_demux, DEMUX_SET_TIME, i64 );
            }
            break;

        case DEMUX_CAN_CONTROL_RATE:
            *va_arg( args, bool * ) = true;
            return VLC_SUCCESS;
        case DEMUX_SET_RATE:
            p_sys->f_rate = *va_arg( args, float * );
            ResetCurrentIndex( p_demux );
            return VLC_SUCCESS;
        case DEMUX_SET_NEXT_DEMUX_TIME:
            p_sys->b_slave = true;
            p_sys->i_next_demux_date = va_arg( args, vlc_tick_t ) - VLC_TICK_0;
            return VLC_SUCCESS;

        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
            return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );

        case DEMUX_GET_PTS_DELAY:
        case DEMUX_GET_FPS:
        case DEMUX_GET_META:
        case DEMUX_GET_ATTACHMENTS:
        case DEMUX_GET_TITLE_INFO:
        case DEMUX_HAS_UNSUPPORTED_META:
        case DEMUX_CAN_RECORD:
        default:
            break;

    }
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Demux: Send subtitle to decoder
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    vlc_tick_t i_barrier = p_sys->i_next_demux_date;

    while( p_sys->subtitles.i_current < p_sys->subtitles.i_count &&
           ( p_sys->subtitles.p_array[p_sys->subtitles.i_current].i_start *
             p_sys->f_rate ) <= i_barrier )
    {
        const subtitle_t *p_subtitle = &p_sys->subtitles.p_array[p_sys->subtitles.i_current];

        if ( !p_sys->b_slave && p_sys->b_first_time )
        {
            es_out_SetPCR( p_demux->out, VLC_TICK_0 + i_barrier );
            p_sys->b_first_time = false;
        }

        if( p_subtitle->i_start >= 0 )
        {
            block_t *p_block = p_sys->pf_convert( p_subtitle );
            if( p_block )
            {
                p_block->i_dts =
                p_block->i_pts = VLC_TICK_0 + p_subtitle->i_start * p_sys->f_rate;
                if( p_subtitle->i_stop >= 0 && p_subtitle->i_stop >= p_subtitle->i_start )
                    p_block->i_length = (p_subtitle->i_stop - p_subtitle->i_start) * p_sys->f_rate;

                es_out_Send( p_demux->out, p_sys->es, p_block );
            }
        }

        p_sys->subtitles.i_current++;
    }

    if ( !p_sys->b_slave )
    {
        es_out_SetPCR( p_demux->out, VLC_TICK_0 + i_barrier );
        p_sys->i_next_demux_date += VLC_TICK_FROM_MS(125);
    }

    if( p_sys->subtitles.i_current >= p_sys->subtitles.i_count )
        return VLC_DEMUXER_EOF;

    return VLC_DEMUXER_SUCCESS;
}


static int subtitle_cmp( const void *first, const void *second )
{
    vlc_tick_t result = ((subtitle_t *)(first))->i_start - ((subtitle_t *)(second))->i_start;
    /* Return -1, 0 ,1, and not directly subtraction
     * as result can be > INT_MAX */
    return result == 0 ? 0 : result > 0 ? 1 : -1;
}
/*****************************************************************************
 * Fix: fix time stamp and order of subtitle
 *****************************************************************************/
static void Fix( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* *** fix order (to be sure...) *** */
    qsort( p_sys->subtitles.p_array, p_sys->subtitles.i_count, sizeof( p_sys->subtitles.p_array[0] ), subtitle_cmp);
}

static int TextLoad( text_t *txt, stream_t *s )
{
    size_t i_line_max;

    /* init txt */
    i_line_max          = 500;
    txt->i_line_count   = 0;
    txt->i_line         = 0;
    txt->line           = calloc( i_line_max, sizeof( char * ) );
    if( !txt->line )
        return VLC_ENOMEM;

    /* load the complete file */
    for( ;; )
    {
        char *psz = vlc_stream_ReadLine( s );

        if( psz == NULL )
            break;

        txt->line[txt->i_line_count] = psz;
        if( txt->i_line_count + 1 >= i_line_max )
        {
            i_line_max += 100;
            char **p_realloc = realloc( txt->line, i_line_max * sizeof( char * ) );
            if( p_realloc == NULL )
                return VLC_ENOMEM;
            txt->line = p_realloc;
        }
        txt->i_line_count++;
    }

    if( txt->i_line_count == 0 )
    {
        free( txt->line );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}
static void TextUnload( text_t *txt )
{
    if( txt->i_line_count )
    {
        for( size_t i = 0; i < txt->i_line_count; i++ )
            free( txt->line[i] );
        free( txt->line );
    }
    txt->i_line       = 0;
    txt->i_line_count = 0;
}

static char *TextGetLine( text_t *txt )
{
    if( txt->i_line >= txt->i_line_count )
        return( NULL );

    return txt->line[txt->i_line++];
}
static void TextPreviousLine( text_t *txt )
{
    if( txt->i_line > 0 )
        txt->i_line--;
}

/*****************************************************************************
 * Specific Subtitle function
 *****************************************************************************/
/* ParseMicroDvd:
 *  Format:
 *      {n1}{n2}Line1|Line2|Line3....
 *  where n1 and n2 are the video frame number (n2 can be empty)
 */
static int ParseMicroDvd( vlc_object_t *p_obj, subs_properties_t *p_props,
                          text_t *txt, subtitle_t *p_subtitle,
                          size_t i_idx )
{
    VLC_UNUSED( i_idx );
    char *psz_text;
    int  i_start;
    int  i_stop;
    int  i;

    for( ;; )
    {
        const char *s = TextGetLine( txt );
        if( !s )
            return VLC_EGENERIC;

        psz_text = malloc( strlen(s) + 1 );
        if( !psz_text )
            return VLC_ENOMEM;

        i_start = 0;
        i_stop  = -1;
        if( sscanf( s, "{%d}{}%[^\r\n]", &i_start, psz_text ) == 2 ||
            sscanf( s, "{%d}{%d}%[^\r\n]", &i_start, &i_stop, psz_text ) == 3)
        {
            if( i_start != 1 || i_stop != 1 )
                break;

            /* We found a possible setting of the framerate "{1}{1}23.976" */
            /* Check if it's usable, and if the sub-original-fps is not set */
            float f_fps = us_strtof( psz_text, NULL );
            if( f_fps > 0.f && var_GetFloat( p_obj, "sub-original-fps" ) <= 0.f )
                p_props->i_microsecperframe = llroundf((float)CLOCK_FREQ / f_fps);
        }
        free( psz_text );
    }

    /* replace | by \n */
    for( i = 0; psz_text[i] != '\0'; i++ )
    {
        if( psz_text[i] == '|' )
            psz_text[i] = '\n';
    }

    /* */
    p_subtitle->i_start  = i_start * p_props->i_microsecperframe;
    p_subtitle->i_stop   = i_stop >= 0 ? (i_stop  * p_props->i_microsecperframe) : -1;
    p_subtitle->psz_text = psz_text;
    return VLC_SUCCESS;
}

/* ParseSubRipSubViewer
 *  Format SubRip
 *      n
 *      h1:m1:s1,d1 --> h2:m2:s2,d2
 *      Line1
 *      Line2
 *      ....
 *      [Empty line]
 *  Format SubViewer v1/v2
 *      h1:m1:s1.d1,h2:m2:s2.d2
 *      Line1[br]Line2
 *      Line3
 *      ...
 *      [empty line]
 *  We ignore line number for SubRip
 */
static int ParseSubRipSubViewer( vlc_object_t *p_obj, subs_properties_t *p_props,
                                 text_t *txt, subtitle_t *p_subtitle,
                                 int (* pf_parse_timing)(subtitle_t *, const char *),
                                 bool b_replace_br )
{
    VLC_UNUSED(p_obj);
    VLC_UNUSED(p_props);
    char    *psz_text;

    for( ;; )
    {
        const char *s = TextGetLine( txt );

        if( !s )
            return VLC_EGENERIC;

        if( pf_parse_timing( p_subtitle, s) == VLC_SUCCESS &&
            p_subtitle->i_start < p_subtitle->i_stop )
        {
            break;
        }
    }

    /* Now read text until an empty line */
    psz_text = strdup("");
    if( !psz_text )
        return VLC_ENOMEM;

    for( ;; )
    {
        const char *s = TextGetLine( txt );
        size_t i_len;
        size_t i_old;

        i_len = s ? strlen( s ) : 0;
        if( i_len <= 0 )
        {
            p_subtitle->psz_text = psz_text;
            return VLC_SUCCESS;
        }

        i_old = strlen( psz_text );
        psz_text = realloc_or_free( psz_text, i_old + i_len + 1 + 1 );
        if( !psz_text )
        {
            return VLC_ENOMEM;
        }
        strcat( psz_text, s );
        strcat( psz_text, "\n" );

        /* replace [br] by \n */
        if( b_replace_br )
        {
            char *p;

            while( ( p = strstr( psz_text, "[br]" ) ) )
            {
                *p++ = '\n';
                memmove( p, &p[3], strlen(&p[3])+1 );
            }
        }
    }
}

/* subtitle_ParseSubRipTimingValue
 * Parses SubRip timing value.
 */
static int subtitle_ParseSubRipTimingValue(vlc_tick_t *timing_value,
                                           const char *s)
{
    int h1, m1, s1, d1 = 0;

    if ( sscanf( s, "%d:%d:%d,%d",
                 &h1, &m1, &s1, &d1 ) == 4 ||
         sscanf( s, "%d:%d:%d.%d",
                 &h1, &m1, &s1, &d1 ) == 4 ||
         sscanf( s, "%d:%d:%d",
                 &h1, &m1, &s1) == 3 )
    {
        (*timing_value) = vlc_tick_from_sec( h1 * 3600 + m1 * 60 + s1) +
                          VLC_TICK_FROM_MS( d1 );

        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

/* subtitle_ParseSubRipTiming
 * Parses SubRip timing.
 */
static int subtitle_ParseSubRipTiming( subtitle_t *p_subtitle,
                                       const char *s )
{
    int i_result = VLC_EGENERIC;
    char *psz_start, *psz_stop;
    psz_start = malloc( strlen(s) + 1 );
    psz_stop = malloc( strlen(s) + 1 );

    if( sscanf( s, "%s --> %s", psz_start, psz_stop) == 2 &&
        subtitle_ParseSubRipTimingValue( &p_subtitle->i_start, psz_start ) == VLC_SUCCESS &&
        subtitle_ParseSubRipTimingValue( &p_subtitle->i_stop,  psz_stop ) == VLC_SUCCESS )
    {
        i_result = VLC_SUCCESS;
    }

    free(psz_start);
    free(psz_stop);

    return i_result;
}
/* ParseSubRip
 */
static int  ParseSubRip( vlc_object_t *p_obj, subs_properties_t *p_props,
                         text_t *txt, subtitle_t *p_subtitle,
                         size_t i_idx )
{
    VLC_UNUSED( i_idx );
    return ParseSubRipSubViewer( p_obj, p_props, txt, p_subtitle,
                                 &subtitle_ParseSubRipTiming,
                                 false );
}

/* subtitle_ParseSubViewerTiming
 * Parses SubViewer timing.
 */
static int subtitle_ParseSubViewerTiming( subtitle_t *p_subtitle,
                                   const char *s )
{
    int h1, m1, s1, d1, h2, m2, s2, d2;

    if( sscanf( s, "%d:%d:%d.%d,%d:%d:%d.%d",
                &h1, &m1, &s1, &d1, &h2, &m2, &s2, &d2) == 8 )
    {
        p_subtitle->i_start = vlc_tick_from_sec( h1 * 3600 + m1 * 60 + s1) +
                              VLC_TICK_FROM_MS( d1 );

        p_subtitle->i_stop  = vlc_tick_from_sec( h2 * 3600 + m2 * 60 + s2 ) +
                              VLC_TICK_FROM_MS( d2 );
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

/* ParseSubViewer
 */
static int  ParseSubViewer( vlc_object_t *p_obj, subs_properties_t *p_props,
                            text_t *txt, subtitle_t *p_subtitle,
                            size_t i_idx )
{
    VLC_UNUSED( i_idx );

    return ParseSubRipSubViewer( p_obj, p_props, txt, p_subtitle,
                                 &subtitle_ParseSubViewerTiming,
                                 true );
}

/* ParseSSA
 */
static int  ParseSSA( vlc_object_t *p_obj, subs_properties_t *p_props,
                      text_t *txt, subtitle_t *p_subtitle,
                      size_t i_idx )
{
    VLC_UNUSED(p_obj);
    size_t header_len = 0;

    for( ;; )
    {
        const char *s = TextGetLine( txt );
        int h1, m1, s1, c1, h2, m2, s2, c2;
        char *psz_text, *psz_temp;
        char temp[16];

        if( !s )
            return VLC_EGENERIC;

        /* We expect (SSA2-4):
         * Format: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
         * Dialogue: Marked=0,0:02:40.65,0:02:41.79,Wolf main,Cher,0000,0000,0000,,Et les enregistrements de ses ondes delta ?
         *
         * SSA-1 is similar but only has 8 commas up untill the subtitle text. Probably the Effect field is no present, but not 100 % sure.
         */

        /* For ASS:
         * Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
         * Dialogue: Layer#,0:02:40.65,0:02:41.79,Wolf main,Cher,0000,0000,0000,,Et les enregistrements de ses ondes delta ?
         */

        /* The output text is - at least, not removing numbers - 18 chars shorter than the input text. */
        psz_text = malloc( strlen(s) );
        if( !psz_text )
            return VLC_ENOMEM;

        if( sscanf( s,
                    "Dialogue: %15[^,],%d:%d:%d.%d,%d:%d:%d.%d,%[^\r\n]",
                    temp,
                    &h1, &m1, &s1, &c1,
                    &h2, &m2, &s2, &c2,
                    psz_text ) == 10 )
        {
            /* The dec expects: ReadOrder, Layer, Style, Name, MarginL, MarginR, MarginV, Effect, Text */
            /* (Layer comes from ASS specs ... it's empty for SSA.) */
            if( p_props->i_type == SUB_TYPE_SSA1 )
            {
                /* SSA1 has only 8 commas before the text starts, not 9 */
                memmove( &psz_text[1], psz_text, strlen(psz_text)+1 );
                psz_text[0] = ',';
            }
            else
            {
                int i_layer = ( p_props->i_type == SUB_TYPE_ASS ) ? atoi( temp ) : 0;

                /* ReadOrder, Layer, %s(rest of fields) */
                if( asprintf( &psz_temp, "%zu,%d,%s", i_idx, i_layer, psz_text ) == -1 )
                {
                    free( psz_text );
                    return VLC_ENOMEM;
                }

                free( psz_text );
                psz_text = psz_temp;
            }

            p_subtitle->i_start = vlc_tick_from_sec( h1 * 3600 + m1 * 60 + s1 ) +
                                  VLC_TICK_FROM_MS( c1 * 10 );
            p_subtitle->i_stop  = vlc_tick_from_sec( h2 * 3600 + m2 * 60 + s2 ) +
                                  VLC_TICK_FROM_MS( c2 * 10 );
            p_subtitle->psz_text = psz_text;
            return VLC_SUCCESS;
        }
        free( psz_text );

        /* All the other stuff we add to the header field */
        if( header_len == 0 && p_props->psz_header )
            header_len = strlen( p_props->psz_header );

        size_t s_len = strlen( s );
        p_props->psz_header = realloc_or_free( p_props->psz_header, header_len + s_len + 2 );
        if( !p_props->psz_header )
            return VLC_ENOMEM;
        snprintf( p_props->psz_header + header_len, s_len + 2, "%s\n", s );
        header_len += s_len + 1;
    }
}

/* ParseVplayer
 *  Format
 *      h:m:s:Line1|Line2|Line3....
 *  or
 *      h:m:s Line1|Line2|Line3....
 */
static int ParseVplayer( vlc_object_t *p_obj, subs_properties_t *p_props,
                         text_t *txt, subtitle_t *p_subtitle,
                         size_t i_idx )
{
    VLC_UNUSED(p_obj);
    VLC_UNUSED(p_props);
    VLC_UNUSED( i_idx );
    char *psz_text;

    for( ;; )
    {
        const char *s = TextGetLine( txt );
        int h1, m1, s1;

        if( !s )
            return VLC_EGENERIC;

        psz_text = malloc( strlen( s ) + 1 );
        if( !psz_text )
            return VLC_ENOMEM;

        if( sscanf( s, "%d:%d:%d%*c%[^\r\n]",
                    &h1, &m1, &s1, psz_text ) == 4 )
        {
            p_subtitle->i_start = vlc_tick_from_sec( h1 * 3600 + m1 * 60 + s1 );
            p_subtitle->i_stop  = -1;
            break;
        }
        free( psz_text );
    }

    /* replace | by \n */
    for( size_t i = 0; psz_text[i] != '\0'; i++ )
    {
        if( psz_text[i] == '|' )
            psz_text[i] = '\n';
    }
    p_subtitle->psz_text = psz_text;
    return VLC_SUCCESS;
}

/* ParseSami
 */
static const char *ParseSamiSearch( text_t *txt,
                                    const char *psz_start, const char *psz_str )
{
    if( psz_start && strcasestr( psz_start, psz_str ) )
    {
        const char *s = strcasestr( psz_start, psz_str );
        return &s[strlen( psz_str )];
    }

    for( ;; )
    {
        const char *p = TextGetLine( txt );
        if( !p )
            return NULL;

        const char *s = strcasestr( p, psz_str );
        if( s != NULL )
            return &s[strlen( psz_str )];
    }
}
static int ParseSami( vlc_object_t *p_obj, subs_properties_t *p_props,
                      text_t *txt, subtitle_t *p_subtitle, size_t i_idx )
{
    VLC_UNUSED(p_obj);
    VLC_UNUSED(p_props);
    VLC_UNUSED( i_idx );
    const char *s;
    int64_t i_start;

    unsigned int i_text;
    char text[8192]; /* Arbitrary but should be long enough */

    /* search "Start=" */
    s = ParseSamiSearch( txt, p_props->sami.psz_start, "Start=" );
    p_props->sami.psz_start = NULL;
    if( !s )
        return VLC_EGENERIC;

    /* get start value */
    char *psz_end;
    i_start = strtol( s, &psz_end, 0 );
    s = psz_end;

    /* search <P */
    if( !( s = ParseSamiSearch( txt, s, "<P" ) ) )
        return VLC_EGENERIC;

    /* search > */
    if( !( s = ParseSamiSearch( txt, s, ">" ) ) )
        return VLC_EGENERIC;

    i_text = 0;
    text[0] = '\0';
    /* now get all txt until  a "Start=" line */
    for( ;; )
    {
        char c = '\0';
        /* Search non empty line */
        while( s && *s == '\0' )
            s = TextGetLine( txt );
        if( !s )
            break;

        if( *s == '<' )
        {
            if( !strncasecmp( s, "<br", 3 ) )
            {
                c = '\n';
            }
            else if( strcasestr( s, "Start=" ) )
            {
                p_props->sami.psz_start = s;
                break;
            }
            s = ParseSamiSearch( txt, s, ">" );
        }
        else if( !strncmp( s, "&nbsp;", 6 ) )
        {
            c = ' ';
            s += 6;
        }
        else if( *s == '\t' )
        {
            c = ' ';
            s++;
        }
        else
        {
            c = *s;
            s++;
        }
        if( c != '\0' && i_text+1 < sizeof(text) )
        {
            text[i_text++] = c;
            text[i_text] = '\0';
        }
    }

    p_subtitle->i_start = VLC_TICK_FROM_MS(i_start);
    p_subtitle->i_stop  = -1;
    p_subtitle->psz_text = strdup( text );

    return VLC_SUCCESS;
}

/* ParseDVDSubtitle
 *  Format
 *      {T h1:m1:s1:c1
 *      Line1
 *      Line2
 *      ...
 *      }
 * TODO it can have a header
 *      { HEAD
 *          ...
 *          CODEPAGE=...
 *          FORMAT=...
 *          LANG=English
 *      }
 *      LANG support would be cool
 *      CODEPAGE is probably mandatory FIXME
 */
static int ParseDVDSubtitle(vlc_object_t *p_obj, subs_properties_t *p_props,
                            text_t *txt, subtitle_t *p_subtitle, size_t i_idx )
{
    VLC_UNUSED(p_obj);
    VLC_UNUSED(p_props);
    VLC_UNUSED( i_idx );
    char *psz_text;

    for( ;; )
    {
        const char *s = TextGetLine( txt );
        int h1, m1, s1, c1;

        if( !s )
            return VLC_EGENERIC;

        if( sscanf( s,
                    "{T %d:%d:%d:%d",
                    &h1, &m1, &s1, &c1 ) == 4 )
        {
            p_subtitle->i_start = vlc_tick_from_sec( h1 * 3600 + m1 * 60 + s1 ) +
                                  VLC_TICK_FROM_MS( c1 * 10 );
            p_subtitle->i_stop = -1;
            break;
        }
    }

    /* Now read text until a line containing "}" */
    psz_text = strdup("");
    if( !psz_text )
        return VLC_ENOMEM;
    for( ;; )
    {
        const char *s = TextGetLine( txt );
        int i_len;
        int i_old;

        if( !s )
        {
            free( psz_text );
            return VLC_EGENERIC;
        }

        i_len = strlen( s );
        if( i_len == 1 && s[0] == '}')
        {
            p_subtitle->psz_text = psz_text;
            return VLC_SUCCESS;
        }

        i_old = strlen( psz_text );
        psz_text = realloc_or_free( psz_text, i_old + i_len + 1 + 1 );
        if( !psz_text )
            return VLC_ENOMEM;
        strcat( psz_text, s );
        strcat( psz_text, "\n" );
    }
}

/* ParseMPL2
 *  Format
 *     [n1][n2]Line1|Line2|Line3...
 *  where n1 and n2 are the video frame number (n2 can be empty)
 */
static int ParseMPL2(vlc_object_t *p_obj, subs_properties_t *p_props,
                     text_t *txt, subtitle_t *p_subtitle, size_t i_idx )
{
    VLC_UNUSED(p_obj);
    VLC_UNUSED(p_props);
    VLC_UNUSED( i_idx );
    char *psz_text;
    int i;

    for( ;; )
    {
        const char *s = TextGetLine( txt );
        int i_start;
        int i_stop;

        if( !s )
            return VLC_EGENERIC;

        psz_text = malloc( strlen(s) + 1 );
        if( !psz_text )
            return VLC_ENOMEM;

        i_start = 0;
        i_stop  = -1;
        if( sscanf( s, "[%d][] %[^\r\n]", &i_start, psz_text ) == 2 ||
            sscanf( s, "[%d][%d] %[^\r\n]", &i_start, &i_stop, psz_text ) == 3)
        {
            p_subtitle->i_start = VLC_TICK_FROM_MS(i_start * 100);
            p_subtitle->i_stop  = i_stop >= 0 ? VLC_TICK_FROM_MS(i_stop  * 100) : -1;
            break;
        }
        free( psz_text );
    }

    for( i = 0; psz_text[i] != '\0'; )
    {
        /* replace | by \n */
        if( psz_text[i] == '|' )
            psz_text[i] = '\n';

        /* Remove italic */
        if( psz_text[i] == '/' && ( i == 0 || psz_text[i-1] == '\n' ) )
            memmove( &psz_text[i], &psz_text[i+1], strlen(&psz_text[i+1])+1 );
        else
            i++;
    }
    p_subtitle->psz_text = psz_text;
    return VLC_SUCCESS;
}

static int ParseAQT(vlc_object_t *p_obj, subs_properties_t *p_props, text_t *txt, subtitle_t *p_subtitle, size_t i_idx )
{
    VLC_UNUSED(p_obj);
    VLC_UNUSED(p_props);
    VLC_UNUSED( i_idx );

    char *psz_text = strdup( "" );
    int i_old = 0;
    int i_firstline = 1;

    for( ;; )
    {
        int t; /* Time */

        const char *s = TextGetLine( txt );

        if( !s )
        {
            free( psz_text );
            return VLC_EGENERIC;
        }

        /* Data Lines */
        if( sscanf (s, "-->> %d", &t) == 1)
        {
            p_subtitle->i_start = (int64_t)t; /* * FPS*/
            p_subtitle->i_stop  = -1;

            /* Starting of a subtitle */
            if( i_firstline )
            {
                i_firstline = 0;
            }
            /* We have been too far: end of the subtitle, begin of next */
            else
            {
                TextPreviousLine( txt );
                break;
            }
        }
        /* Text Lines */
        else
        {
            i_old = strlen( psz_text ) + 1;
            psz_text = realloc_or_free( psz_text, i_old + strlen( s ) + 1 );
            if( !psz_text )
                 return VLC_ENOMEM;
            strcat( psz_text, s );
            strcat( psz_text, "\n" );
            if( txt->i_line == txt->i_line_count )
                break;
        }
    }
    p_subtitle->psz_text = psz_text;
    return VLC_SUCCESS;
}

static int ParsePJS(vlc_object_t *p_obj, subs_properties_t *p_props,
                    text_t *txt, subtitle_t *p_subtitle, size_t i_idx )
{
    VLC_UNUSED(p_obj);
    VLC_UNUSED(p_props);
    VLC_UNUSED( i_idx );

    char *psz_text;
    int i;

    for( ;; )
    {
        const char *s = TextGetLine( txt );
        int t1, t2;

        if( !s )
            return VLC_EGENERIC;

        psz_text = malloc( strlen(s) + 1 );
        if( !psz_text )
            return VLC_ENOMEM;

        /* Data Lines */
        if( sscanf (s, "%d,%d,\"%[^\n\r]", &t1, &t2, psz_text ) == 3 )
        {
            /* 1/10th of second ? Frame based ? FIXME */
            p_subtitle->i_start = 10 * t1;
            p_subtitle->i_stop = 10 * t2;
            /* Remove latest " */
            psz_text[ strlen(psz_text) - 1 ] = '\0';

            break;
        }
        free( psz_text );
    }

    /* replace | by \n */
    for( i = 0; psz_text[i] != '\0'; i++ )
    {
        if( psz_text[i] == '|' )
            psz_text[i] = '\n';
    }

    p_subtitle->psz_text = psz_text;
    msg_Dbg( p_obj, "%s", psz_text );
    return VLC_SUCCESS;
}

static int ParseMPSub( vlc_object_t *p_obj, subs_properties_t *p_props,
                       text_t *txt, subtitle_t *p_subtitle, size_t i_idx )
{
    VLC_UNUSED( i_idx );

    char *psz_text = strdup( "" );

    if( !p_props->mpsub.b_inited )
    {
        p_props->mpsub.f_total = 0.0;
        p_props->mpsub.i_factor = 0;

        p_props->mpsub.b_inited = true;
    }

    for( ;; )
    {
        char p_dummy;
        char *psz_temp;

        const char *s = TextGetLine( txt );
        if( !s )
        {
            free( psz_text );
            return VLC_EGENERIC;
        }

        if( strstr( s, "FORMAT" ) )
        {
            if( sscanf (s, "FORMAT=TIM%c", &p_dummy ) == 1 && p_dummy == 'E')
            {
                p_props->mpsub.i_factor = 100;
                break;
            }

            psz_temp = malloc( strlen(s) );
            if( !psz_temp )
            {
                free( psz_text );
                return VLC_ENOMEM;
            }

            if( sscanf( s, "FORMAT=%[^\r\n]", psz_temp ) )
            {
                float f_fps = us_strtof( psz_temp, NULL );

                if( f_fps > 0.f && var_GetFloat( p_obj, "sub-original-fps" ) <= 0.f )
                    var_SetFloat( p_obj, "sub-original-fps", f_fps );

                p_props->mpsub.i_factor = 1;
                free( psz_temp );
                break;
            }
            free( psz_temp );
        }

        /* Data Lines */
        float f1 = us_strtof( s, &psz_temp );
        if( *psz_temp )
        {
            float f2 = us_strtof( psz_temp, NULL );
            p_props->mpsub.f_total += f1 * p_props->mpsub.i_factor;
            p_subtitle->i_start = llroundf(10000.f * p_props->mpsub.f_total);
            p_props->mpsub.f_total += f2 * p_props->mpsub.i_factor;
            p_subtitle->i_stop = llroundf(10000.f * p_props->mpsub.f_total);
            break;
        }
    }

    for( ;; )
    {
        const char *s = TextGetLine( txt );

        if( !s )
        {
            free( psz_text );
            return VLC_EGENERIC;
        }

        size_t i_len = strlen( s );
        if( i_len == 0 )
            break;

        size_t i_old = strlen( psz_text );

        psz_text = realloc_or_free( psz_text, i_old + i_len + 1 + 1 );
        if( !psz_text )
             return VLC_ENOMEM;

        strcat( psz_text, s );
        strcat( psz_text, "\n" );
    }

    p_subtitle->psz_text = psz_text;
    return VLC_SUCCESS;
}

static int ParseJSS( vlc_object_t *p_obj, subs_properties_t *p_props,
                     text_t *txt, subtitle_t *p_subtitle, size_t i_idx )
{
    VLC_UNUSED( i_idx );
    char         *psz_text, *psz_orig;
    char         *psz_text2, *psz_orig2;

    if( !p_props->jss.b_inited )
    {
        p_props->jss.i_comment = 0;
        p_props->jss.i_time_resolution = 30;
        p_props->jss.i_time_shift = 0;

        p_props->jss.b_inited = true;
    }

    /* Parse the main lines */
    for( ;; )
    {
        const char *s = TextGetLine( txt );
        if( !s )
            return VLC_EGENERIC;

        size_t line_length = strlen( s );
        psz_orig = malloc( line_length + 1 );
        if( !psz_orig )
            return VLC_ENOMEM;
        psz_text = psz_orig;

        /* Complete time lines */
        int h1, h2, m1, m2, s1, s2, f1, f2;
        if( sscanf( s, "%d:%d:%d.%d %d:%d:%d.%d %[^\n\r]",
                    &h1, &m1, &s1, &f1, &h2, &m2, &s2, &f2, psz_text ) == 9 )
        {
            p_subtitle->i_start = vlc_tick_from_sec( ( h1 *3600 + m1 * 60 + s1 ) +
                (int64_t)( ( f1 +  p_props->jss.i_time_shift ) / p_props->jss.i_time_resolution ) );
            p_subtitle->i_stop = vlc_tick_from_sec( ( h2 *3600 + m2 * 60 + s2 ) +
                (int64_t)( ( f2 +  p_props->jss.i_time_shift ) / p_props->jss.i_time_resolution ) );
            break;
        }
        /* Short time lines */
        else if( sscanf( s, "@%d @%d %[^\n\r]", &f1, &f2, psz_text ) == 3 )
        {
            p_subtitle->i_start =
                    vlc_tick_from_sec( (f1 + p_props->jss.i_time_shift ) / p_props->jss.i_time_resolution );
            p_subtitle->i_stop =
                    vlc_tick_from_sec( (f2 + p_props->jss.i_time_shift ) / p_props->jss.i_time_resolution );
            break;
        }
        /* General Directive lines */
        /* Only TIME and SHIFT are supported so far */
        else if( s[0] == '#' )
        {
            int h = 0, m =0, sec = 1, f = 1;
            unsigned shift = 1;
            int inv = 1;

            strcpy( psz_text, s );

            switch( toupper( (unsigned char)psz_text[1] ) )
            {
            case 'S':
                 shift = isalpha( (unsigned char)psz_text[2] ) ? 6 : 2 ;
                 if ( shift > line_length )
                     break;

                 if( sscanf( &psz_text[shift], "%d", &h ) )
                 {
                     /* Negative shifting */
                     if( h < 0 )
                     {
                         h *= -1;
                         inv = -1;
                     }

                     if( sscanf( &psz_text[shift], "%*d:%d", &m ) )
                     {
                         if( sscanf( &psz_text[shift], "%*d:%*d:%d", &sec ) )
                         {
                             sscanf( &psz_text[shift], "%*d:%*d:%*d.%d", &f );
                         }
                         else
                         {
                             h = 0;
                             sscanf( &psz_text[shift], "%d:%d.%d",
                                     &m, &sec, &f );
                             m *= inv;
                         }
                     }
                     else
                     {
                         h = m = 0;
                         sscanf( &psz_text[shift], "%d.%d", &sec, &f);
                         sec *= inv;
                     }
                     p_props->jss.i_time_shift = ( ( h * 3600 + m * 60 + sec )
                         * p_props->jss.i_time_resolution + f ) * inv;
                 }
                 break;

            case 'T':
                shift = isalpha( (unsigned char)psz_text[2] ) ? 8 : 2 ;
                if ( shift > line_length )
                    break;

                sscanf( &psz_text[shift], "%d", &p_props->jss.i_time_resolution );
                if( !p_props->jss.i_time_resolution )
                    p_props->jss.i_time_resolution = 30;
                break;
            }
            free( psz_orig );
            continue;
        }
        else
            /* Unkown type line, probably a comment */
        {
            free( psz_orig );
            continue;
        }
    }

    while( psz_text[ strlen( psz_text ) - 1 ] == '\\' )
    {
        const char *s2 = TextGetLine( txt );

        if( !s2 )
        {
            free( psz_orig );
            return VLC_EGENERIC;
        }

        size_t i_len = strlen( s2 );
        if( i_len == 0 )
            break;

        size_t i_old = strlen( psz_text );

        psz_text = realloc_or_free( psz_text, i_old + i_len + 1 );
        if( !psz_text )
             return VLC_ENOMEM;

        psz_orig = psz_text;
        strcat( psz_text, s2 );
    }

    /* Skip the blanks */
    while( *psz_text == ' ' || *psz_text == '\t' ) psz_text++;

    /* Parse the directives */
    if( isalpha( (unsigned char)*psz_text ) || *psz_text == '[' )
    {
        while( *psz_text && *psz_text != ' ' )
            ++psz_text;

        /* Directives are NOT parsed yet */
        /* This has probably a better place in a decoder ? */
        /* directive = malloc( strlen( psz_text ) + 1 );
           if( sscanf( psz_text, "%s %[^\n\r]", directive, psz_text2 ) == 2 )*/
    }

    /* Skip the blanks after directives */
    while( *psz_text == ' ' || *psz_text == '\t' ) psz_text++;

    /* Clean all the lines from inline comments and other stuffs */
    psz_orig2 = calloc( strlen( psz_text) + 1, 1 );
    psz_text2 = psz_orig2;

    for( ; *psz_text != '\0' && *psz_text != '\n' && *psz_text != '\r'; )
    {
        switch( *psz_text )
        {
        case '{':
            p_props->jss.i_comment++;
            break;
        case '}':
            if( p_props->jss.i_comment )
            {
                p_props->jss.i_comment = 0;
                if( (*(psz_text + 1 ) ) == ' ' ) psz_text++;
            }
            break;
        case '~':
            if( !p_props->jss.i_comment )
            {
                *psz_text2 = ' ';
                psz_text2++;
            }
            break;
        case ' ':
        case '\t':
            if( (*(psz_text + 1 ) ) == ' ' || (*(psz_text + 1 ) ) == '\t' )
                break;
            if( !p_props->jss.i_comment )
            {
                *psz_text2 = ' ';
                psz_text2++;
            }
            break;
        case '\\':
            if( (*(psz_text + 1 ) ) == 'n' )
            {
                *psz_text2 = '\n';
                psz_text++;
                psz_text2++;
                break;
            }
            if( ( toupper((unsigned char)*(psz_text + 1 ) ) == 'C' ) ||
                    ( toupper((unsigned char)*(psz_text + 1 ) ) == 'F' ) )
            {
                psz_text++;
                break;
            }
            if( (*(psz_text + 1 ) ) == 'B' || (*(psz_text + 1 ) ) == 'b' ||
                (*(psz_text + 1 ) ) == 'I' || (*(psz_text + 1 ) ) == 'i' ||
                (*(psz_text + 1 ) ) == 'U' || (*(psz_text + 1 ) ) == 'u' ||
                (*(psz_text + 1 ) ) == 'D' || (*(psz_text + 1 ) ) == 'N' )
            {
                psz_text++;
                break;
            }
            if( (*(psz_text + 1 ) ) == '~' || (*(psz_text + 1 ) ) == '{' ||
                (*(psz_text + 1 ) ) == '\\' )
                psz_text++;
            else if( ( *(psz_text + 1 ) == '\r' ||  *(psz_text + 1 ) == '\n' ) &&
                     *(psz_text + 1 ) != '\0' )
            {
                psz_text++;
            }
            break;
        default:
            if( !p_props->jss.i_comment )
            {
                *psz_text2 = *psz_text;
                psz_text2++;
            }
        }
        psz_text++;
    }

    p_subtitle->psz_text = psz_orig2;
    msg_Dbg( p_obj, "%s", p_subtitle->psz_text );
    free( psz_orig );
    return VLC_SUCCESS;
}

static int ParsePSB( vlc_object_t *p_obj, subs_properties_t *p_props,
                     text_t *txt, subtitle_t *p_subtitle, size_t i_idx )
{
    VLC_UNUSED(p_obj);
    VLC_UNUSED(p_props);
    VLC_UNUSED( i_idx );

    char *psz_text;
    int i;

    for( ;; )
    {
        int h1, m1, s1;
        int h2, m2, s2;
        const char *s = TextGetLine( txt );

        if( !s )
            return VLC_EGENERIC;

        psz_text = malloc( strlen( s ) + 1 );
        if( !psz_text )
            return VLC_ENOMEM;

        if( sscanf( s, "{%d:%d:%d}{%d:%d:%d}%[^\r\n]",
                    &h1, &m1, &s1, &h2, &m2, &s2, psz_text ) == 7 )
        {
            p_subtitle->i_start = vlc_tick_from_sec( h1 * 3600 + m1 * 60 + s1 );
            p_subtitle->i_stop  = vlc_tick_from_sec( h2 * 3600 + m2 * 60 + s2 );
            break;
        }
        free( psz_text );
    }

    /* replace | by \n */
    for( i = 0; psz_text[i] != '\0'; i++ )
    {
        if( psz_text[i] == '|' )
            psz_text[i] = '\n';
    }
    p_subtitle->psz_text = psz_text;
    return VLC_SUCCESS;
}

static int64_t ParseRealTime( char *psz, int *h, int *m, int *s, int *f )
{
    if( *psz == '\0' ) return 0;
    if( sscanf( psz, "%d:%d:%d.%d", h, m, s, f ) == 4 ||
            sscanf( psz, "%d:%d.%d", m, s, f ) == 3 ||
            sscanf( psz, "%d.%d", s, f ) == 2 ||
            sscanf( psz, "%d:%d", m, s ) == 2 ||
            sscanf( psz, "%d", s ) == 1 )
    {
        return vlc_tick_from_sec((( *h * 60 + *m ) * 60 ) + *s )
               + VLC_TICK_FROM_MS(*f * 10);
    }
    else return VLC_EGENERIC;
}

static int ParseRealText( vlc_object_t *p_obj, subs_properties_t *p_props,
                          text_t *txt, subtitle_t *p_subtitle, size_t i_idx )
{
    VLC_UNUSED(p_obj);
    VLC_UNUSED(p_props);
    VLC_UNUSED( i_idx );
    char *psz_text = NULL;

    for( ;; )
    {
        int h1 = 0, m1 = 0, s1 = 0, f1 = 0;
        int h2 = 0, m2 = 0, s2 = 0, f2 = 0;
        const char *s = TextGetLine( txt );
        free( psz_text );

        if( !s )
            return VLC_EGENERIC;

        psz_text = malloc( strlen( s ) + 1 );
        if( !psz_text )
            return VLC_ENOMEM;

        /* Find the good begining. This removes extra spaces at the beginning
           of the line.*/
        char *psz_temp = strcasestr( s, "<time");
        if( psz_temp != NULL )
        {
            char psz_end[12], psz_begin[12];
            /* Line has begin and end */
            if( ( sscanf( psz_temp,
                  "<%*[t|T]ime %*[b|B]egin=\"%11[^\"]\" %*[e|E]nd=\"%11[^\"]%*[^>]%[^\n\r]",
                            psz_begin, psz_end, psz_text) != 3 ) &&
                    /* Line has begin and no end */
                    ( sscanf( psz_temp,
                              "<%*[t|T]ime %*[b|B]egin=\"%11[^\"]\"%*[^>]%[^\n\r]",
                              psz_begin, psz_text ) != 2) )
                /* Line is not recognized */
            {
                continue;
            }

            /* Get the times */
            int64_t i_time = ParseRealTime( psz_begin, &h1, &m1, &s1, &f1 );
            p_subtitle->i_start = i_time >= 0 ? i_time : 0;

            i_time = ParseRealTime( psz_end, &h2, &m2, &s2, &f2 );
            p_subtitle->i_stop = i_time >= 0 ? i_time : -1;
            break;
        }
    }

    /* Get the following Lines */
    for( ;; )
    {
        const char *s = TextGetLine( txt );

        if( !s )
        {
            free( psz_text );
            return VLC_EGENERIC;
        }

        size_t i_len = strlen( s );
        if( i_len == 0 ) break;

        if( strcasestr( s, "<time" ) ||
            strcasestr( s, "<clear/") )
        {
            TextPreviousLine( txt );
            break;
        }

        size_t i_old = strlen( psz_text );

        psz_text = realloc_or_free( psz_text, i_old + i_len + 1 + 1 );
        if( !psz_text )
            return VLC_ENOMEM;

        strcat( psz_text, s );
        strcat( psz_text, "\n" );
    }

    /* Remove the starting ">" that remained after the sscanf */
    memmove( &psz_text[0], &psz_text[1], strlen( psz_text ) );

    p_subtitle->psz_text = psz_text;

    return VLC_SUCCESS;
}

static int ParseDKS( vlc_object_t *p_obj, subs_properties_t *p_props,
                     text_t *txt, subtitle_t *p_subtitle, size_t i_idx )
{
    VLC_UNUSED(p_obj);
    VLC_UNUSED(p_props);
    VLC_UNUSED( i_idx );

    char *psz_text;

    for( ;; )
    {
        int h1, m1, s1;
        int h2, m2, s2;
        char *s = TextGetLine( txt );

        if( !s )
            return VLC_EGENERIC;

        psz_text = malloc( strlen( s ) + 1 );
        if( !psz_text )
            return VLC_ENOMEM;

        if( sscanf( s, "[%d:%d:%d]%[^\r\n]",
                    &h1, &m1, &s1, psz_text ) == 4 )
        {
            p_subtitle->i_start = vlc_tick_from_sec( h1 * 3600 + m1 * 60 + s1 );

            s = TextGetLine( txt );
            if( !s )
            {
                free( psz_text );
                return VLC_EGENERIC;
            }

            if( sscanf( s, "[%d:%d:%d]", &h2, &m2, &s2 ) == 3 )
                p_subtitle->i_stop  = vlc_tick_from_sec(h2 * 3600 + m2 * 60 + s2 );
            else
                p_subtitle->i_stop  = -1;
            break;
        }
        free( psz_text );
    }

    /* replace [br] by \n */
    char *p;
    while( ( p = strstr( psz_text, "[br]" ) ) )
    {
        *p++ = '\n';
        memmove( p, &p[3], strlen(&p[3])+1 );
    }

    p_subtitle->psz_text = psz_text;
    return VLC_SUCCESS;
}

static int ParseSubViewer1( vlc_object_t *p_obj, subs_properties_t *p_props,
                            text_t *txt, subtitle_t *p_subtitle, size_t i_idx )
{
    VLC_UNUSED(p_obj);
    VLC_UNUSED(p_props);
    VLC_UNUSED( i_idx );
    char *psz_text;

    for( ;; )
    {
        int h1, m1, s1;
        int h2, m2, s2;
        char *s = TextGetLine( txt );

        if( !s )
            return VLC_EGENERIC;

        if( sscanf( s, "[%d:%d:%d]", &h1, &m1, &s1 ) == 3 )
        {
            p_subtitle->i_start = vlc_tick_from_sec( h1 * 3600 + m1 * 60 + s1 );

            s = TextGetLine( txt );
            if( !s )
                return VLC_EGENERIC;

            psz_text = strdup( s );
            if( !psz_text )
                return VLC_ENOMEM;

            s = TextGetLine( txt );
            if( !s )
            {
                free( psz_text );
                return VLC_EGENERIC;
            }

            if( sscanf( s, "[%d:%d:%d]", &h2, &m2, &s2 ) == 3 )
                p_subtitle->i_stop  = vlc_tick_from_sec( h2 * 3600 + m2 * 60 + s2 );
            else
                p_subtitle->i_stop  = -1;

            break;
        }
    }

    p_subtitle->psz_text = psz_text;

    return VLC_SUCCESS;
}

static int ParseCommonSBV( vlc_object_t *p_obj, subs_properties_t *p_props,
                           text_t *txt, subtitle_t *p_subtitle, size_t i_idx )
{
    VLC_UNUSED(p_obj);
    VLC_UNUSED( i_idx );
    VLC_UNUSED( p_props );
    char        *psz_text;

    for( ;; )
    {
        const char *s = TextGetLine( txt );
        int h1 = 0, m1 = 0, s1 = 0, d1 = 0;
        int h2 = 0, m2 = 0, s2 = 0, d2 = 0;

        if( !s )
            return VLC_EGENERIC;

        if( sscanf( s,"%d:%d:%d.%d,%d:%d:%d.%d",
                    &h1, &m1, &s1, &d1,
                    &h2, &m2, &s2, &d2 ) == 8 )
        {
            p_subtitle->i_start = vlc_tick_from_sec( h1 * 3600 + m1 * 60 + s1 ) +
                                  VLC_TICK_FROM_MS( d1 );

            p_subtitle->i_stop  = vlc_tick_from_sec( h2 * 3600 + m2 * 60 + s2 ) +
                                  VLC_TICK_FROM_MS( d2 );
            if( p_subtitle->i_start < p_subtitle->i_stop )
                break;
        }
    }

    /* Now read text until an empty line */
    psz_text = strdup("");
    if( !psz_text )
        return VLC_ENOMEM;

    for( ;; )
    {
        const char *s = TextGetLine( txt );
        size_t i_len;
        size_t i_old;

        i_len = s ? strlen( s ) : 0;
        if( i_len <= 0 )
        {
            p_subtitle->psz_text = psz_text;
            return VLC_SUCCESS;
        }

        i_old = strlen( psz_text );
        psz_text = realloc_or_free( psz_text, i_old + i_len + 1 + 1 );
        if( !psz_text )
            return VLC_ENOMEM;

        strcat( psz_text, s );
        strcat( psz_text, "\n" );
    }
}

static int ParseSCC( vlc_object_t *p_obj, subs_properties_t *p_props,
                     text_t *txt, subtitle_t *p_subtitle, size_t i_idx )
{
    VLC_UNUSED(p_obj);
    VLC_UNUSED( i_idx );
    VLC_UNUSED( p_props );

    static const struct rates
    {
        unsigned val;
        vlc_rational_t rate;
        bool b_drop_allowed;
    } framerates[] = {
        { 2398, { 24000, 1001 }, false },
        { 2400, { 24, 1 },       false },
        { 2500, { 25, 1 },       false },
        { 2997, { 30000, 1001 }, true }, /* encoding rate */
        { 3000, { 30, 1 },       false },
        { 5000, { 50, 1 },       false },
        { 5994, { 60000, 1001 }, true },
        { 6000, { 60, 1 },       false },
    };
    const struct rates *p_rate = &framerates[3];
    float f_fps = var_GetFloat( p_obj, "sub-original-fps" );
    if( f_fps > 1.0 )
    {
        for( size_t i=0; i<ARRAY_SIZE(framerates); i++ )
        {
            if( (unsigned)(f_fps * 100) == framerates[i].val )
            {
                p_rate = &framerates[i];
                break;
            }
        }
    }

    for( ;; )
    {
        const char *psz_line = TextGetLine( txt );
        if( !psz_line )
            return VLC_EGENERIC;

        unsigned h, m, s, f;
        char c;
        if( sscanf( psz_line, "%u:%u:%u%c%u ", &h, &m, &s, &c, &f ) != 5 ||
                ( c != ':' && c != ';' ) )
            continue;

        /* convert everything to seconds */
        uint64_t i_frames = h * 3600 + m * 60 + s;

        if( c == ';' && p_rate->b_drop_allowed ) /* dropframe */
        {
            /* convert to frame # to be accurate between inter drop drift
             * of 18 frames see http://andrewduncan.net/timecodes/ */
            const unsigned i_mins = h * 60 + m;
            i_frames = i_frames * p_rate[+1].rate.num + f
                    - (p_rate[+1].rate.den * 2 * (i_mins - i_mins % 10));
        }
        else
        {
            /* convert to frame # at 29.97 */
            i_frames = i_frames * framerates[3].rate.num / framerates[3].rate.den + f;
        }
        p_subtitle->i_start = VLC_TICK_0 + vlc_tick_from_sec(i_frames)*
                                         p_rate->rate.den / p_rate->rate.num;
        p_subtitle->i_stop = -1;

        const char *psz_text = strchr( psz_line, '\t' );
        if( !psz_text && !(psz_text = strchr( psz_line, ' ' )) )
            continue;

        if ( psz_text[1] == '\0' )
            continue;

        p_subtitle->psz_text = strdup( psz_text + 1 );
        if( !p_subtitle->psz_text )
            return VLC_ENOMEM;

        break;
    }

    return VLC_SUCCESS;
}

/* Matches filename.xx.srt */
static char * get_language_from_filename( const char * psz_sub_file )
{
    char *psz_ret = NULL;
    char *psz_tmp, *psz_language_begin;

    if( !psz_sub_file ) return NULL;
    char *psz_work = strdup( psz_sub_file );

    /* Removing extension, but leaving the dot */
    psz_tmp = strrchr( psz_work, '.' );
    if( psz_tmp )
    {
        psz_tmp[0] = '\0';
        psz_language_begin = strrchr( psz_work, '.' );
        if( psz_language_begin )
            psz_ret = strdup(++psz_language_begin);
        psz_tmp[0] = '.';
    }

    free( psz_work );
    return psz_ret;
}
