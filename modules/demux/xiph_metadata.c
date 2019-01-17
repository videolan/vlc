/*****************************************************************************
 * xiph_metadata.h: Vorbis Comment parser
 *****************************************************************************
 * Copyright © 2008-2013 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_charset.h>
#include <vlc_strings.h>
#include <vlc_arrays.h>
#include <vlc_input.h>
#include "xiph_metadata.h"

input_attachment_t* ParseFlacPicture( const uint8_t *p_data, size_t size,
    int i_attachments, int *i_cover_score, int *i_cover_idx )
{
    /* TODO: Merge with ID3v2 copy in modules/meta_engine/taglib.cpp. */
    static const char pi_cover_score[] = {
        0,  /* Other */
        5,  /* 32x32 PNG image that should be used as the file icon */
        4,  /* File icon of a different size or format. */
        20, /* Front cover image of the album. */
        19, /* Back cover image of the album. */
        13, /* Inside leaflet page of the album. */
        18, /* Image from the album itself. */
        17, /* Picture of the lead artist or soloist. */
        16, /* Picture of the artist or performer. */
        14, /* Picture of the conductor. */
        15, /* Picture of the band or orchestra. */
        9,  /* Picture of the composer. */
        8,  /* Picture of the lyricist or text writer. */
        7,  /* Picture of the recording location or studio. */
        10, /* Picture of the artists during recording. */
        11, /* Picture of the artists during performance. */
        6,  /* Picture from a movie or video related to the track. */
        1,  /* Picture of a large, coloured fish. */
        12, /* Illustration related to the track. */
        3,  /* Logo of the band or performer. */
        2   /* Logo of the publisher (record company). */
    };

    uint32_t type, len;

    if( size < 8 )
        return NULL;
#define RM(x) \
    do { \
        assert(size >= (x)); \
        size -= (x); \
        p_data += (x); \
    } while (0)

    type = GetDWBE( p_data );
    RM(4);
    len = GetDWBE( p_data );
    RM(4);

    if( size < len )
        return NULL;

    char *mime = strndup( (const char *)p_data, len );
    if( unlikely(mime == NULL) )
        return NULL;
    RM(len);

    if( size < 4 )
    {
        free( mime );
        return NULL;
    }

    len = GetDWBE( p_data );
    RM(4);

    if( size < len )
    {
        free( mime );
        return NULL;
    }

    input_attachment_t *p_attachment = NULL;
    char *description = strndup( (const char *)p_data, len );
    if( unlikely(description == NULL) )
        goto error;
    RM(len);

    EnsureUTF8( description );

    if( size < 20 )
        goto error;

    RM(4 * 4); /* skip */

    len = GetDWBE( p_data );
    RM(4);

    if( size < len )
        goto error;

    /* printf( "Picture type=%"PRIu32" mime=%s description='%s' "
               "file length=%zu\n", type, mime, description, len ); */

    char name[7 + (sizeof (i_attachments) * 3) + 4 + 1];

    snprintf( name, sizeof (name), "picture%u", i_attachments );

    if( !strcasecmp( mime, "image/jpeg" ) )
        strcat( name, ".jpg" );
    else if( !strcasecmp( mime, "image/png" ) )
        strcat( name, ".png" );

    p_attachment = vlc_input_attachment_New( name, mime, description, p_data,
                                             size /* XXX: len instead? */ );

    if( type < ARRAY_SIZE(pi_cover_score) &&
        *i_cover_score < pi_cover_score[type] )
    {
        *i_cover_idx = i_attachments;
        *i_cover_score = pi_cover_score[type];
    }

error:
    free( mime );
    free( description );
    return p_attachment;
}

#undef RM
#define RM(x) \
    do { \
        i_data -= (x); \
        p_data += (x); \
    } while (0)


typedef struct chapters_array_t
{
    unsigned int i_size;
    seekpoint_t ** pp_chapters;
} chapters_array_t;

static seekpoint_t * getChapterEntry( unsigned int i_index, chapters_array_t *p_array )
{
    if ( i_index > 4096 ) return NULL;
    if ( i_index >= p_array->i_size )
    {
        unsigned int i_newsize = p_array->i_size;
        while( i_index >= i_newsize ) i_newsize += 50;

        if ( !p_array->pp_chapters )
        {
            p_array->pp_chapters = calloc( i_newsize, sizeof( seekpoint_t * ) );
            if ( !p_array->pp_chapters ) return NULL;
            p_array->i_size = i_newsize;
        } else {
            seekpoint_t **tmp = calloc( i_newsize, sizeof( seekpoint_t * ) );
            if ( !tmp ) return NULL;
            memcpy( tmp, p_array->pp_chapters, p_array->i_size * sizeof( seekpoint_t * ) );
            free( p_array->pp_chapters );
            p_array->pp_chapters = tmp;
            p_array->i_size = i_newsize;
        }
    }
    if ( !p_array->pp_chapters[i_index] )
        p_array->pp_chapters[i_index] = vlc_seekpoint_New();
    return p_array->pp_chapters[i_index];
}

#define XIPHMETA_Title        (1 << 0)
#define XIPHMETA_Artist       (1 << 1)
#define XIPHMETA_Genre        (1 << 2)
#define XIPHMETA_Copyright    (1 << 3)
#define XIPHMETA_Album        (1 << 4)
#define XIPHMETA_TrackNum     (1 << 5)
#define XIPHMETA_Description  (1 << 6)
#define XIPHMETA_Rating       (1 << 7)
#define XIPHMETA_Date         (1 << 8)
#define XIPHMETA_Language     (1 << 9)
#define XIPHMETA_Publisher    (1 << 10)
#define XIPHMETA_EncodedBy    (1 << 11)
#define XIPHMETA_TrackTotal   (1 << 12)

static char * xiph_ExtractCueSheetMeta( const char *psz_line,
                                        const char *psz_tag, int i_tag,
                                        bool b_quoted )
{
    if( !strncasecmp( psz_line, psz_tag, i_tag ) )
    {
        if( !b_quoted )
            return strdup( &psz_line[i_tag] );

        /* Unquote string value */
        char *psz_value = malloc( strlen( psz_line ) - i_tag + 1 );
        if( psz_value )
        {
            char *psz_out = psz_value;
            psz_line += i_tag;
            bool b_escaped = false;
            while( *psz_line )
            {
                switch( *psz_line )
                {
                    case '\\':
                        if( b_escaped )
                        {
                            b_escaped = false;
                            *(psz_out++) = *psz_line;
                        }
                        else
                        {
                            b_escaped = true;
                        }
                        break;
                    case '"':
                        if( b_escaped )
                        {
                            b_escaped = false;
                            *(psz_out++) = *psz_line;
                        }
                        break;
                    default:
                        *(psz_out++) = *psz_line;
                        break;
                }
                psz_line++;
            }
            *psz_out = 0;
            return psz_value;
        }
    }
    return NULL;
}

static void xiph_ParseCueSheetMeta( unsigned *pi_flags, vlc_meta_t *p_meta,
                                    const char *psz_line,
                                    int *pi_seekpoint, seekpoint_t ***ppp_seekpoint,
                                    seekpoint_t **pp_tmppoint, bool *pb_valid )
{
    VLC_UNUSED(pi_seekpoint);
    VLC_UNUSED(ppp_seekpoint);

    seekpoint_t *p_seekpoint = *pp_tmppoint;
    char *psz_string;

#define TRY_EXTRACT_CUEMETA(var, string, quoted) \
    if( !(*pi_flags & XIPHMETA_##var) &&\
         ( psz_string = xiph_ExtractCueSheetMeta( psz_line, string, sizeof(string) - 1, quoted ) ) )\
    {\
        vlc_meta_Set( p_meta, vlc_meta_##var, psz_string );\
        free( psz_string );\
        *pi_flags |= XIPHMETA_##var;\
    }

    TRY_EXTRACT_CUEMETA(Title, "TITLE \"", true)
    else TRY_EXTRACT_CUEMETA(Genre, "REM GENRE ", false)
    else TRY_EXTRACT_CUEMETA(Date, "REM DATE ", false)
    else TRY_EXTRACT_CUEMETA(Artist, "PERFORMER \"", true)
    else if( !strncasecmp( psz_line, "  TRACK ", 8 ) )
    {
        if( p_seekpoint )
        {
            if( *pb_valid )
                TAB_APPEND( *pi_seekpoint, *ppp_seekpoint, p_seekpoint );
            else
                vlc_seekpoint_Delete( p_seekpoint );
            *pb_valid = false;
        }
        *pp_tmppoint = p_seekpoint = vlc_seekpoint_New();
    }
    else if( p_seekpoint && !strncasecmp( psz_line, "    INDEX 01 ", 13 ) )
    {
        unsigned m, s, f;
        if( sscanf( &psz_line[13], "%u:%u:%u", &m, &s, &f ) == 3 )
        {
            p_seekpoint->i_time_offset = vlc_tick_from_sec(m * 60 + s) + vlc_tick_from_samples(f, 75);
            *pb_valid = true;
        }
    }
    else if( p_seekpoint && !p_seekpoint->psz_name )
    {
        p_seekpoint->psz_name = xiph_ExtractCueSheetMeta( psz_line, "    TITLE \"", 11, true );
    }
}

static void xiph_ParseCueSheet( unsigned *pi_flags, vlc_meta_t *p_meta,
                                const char *p_data, int i_data,
                                int *pi_seekpoint, seekpoint_t ***ppp_seekpoint )
{
    seekpoint_t *p_seekpoint = NULL;
    bool b_valid = false;

    const char *p_head = p_data;
    const char *p_tail = p_head;
    while( p_tail < p_data + i_data )
    {
        if( *p_tail == 0x0D )
        {
            char *psz = strndup( p_head, p_tail - p_head );
            if( psz )
            {
                xiph_ParseCueSheetMeta( pi_flags, p_meta, psz,
                                        pi_seekpoint, ppp_seekpoint,
                                        &p_seekpoint, &b_valid );
                free( psz );
            }
            if( *(++p_tail) == 0x0A )
                p_tail++;
            p_head = p_tail;
        }
        else
        {
            p_tail++;
        }
    }


    if( p_seekpoint )
    {
        if( b_valid )
            TAB_APPEND( *pi_seekpoint, *ppp_seekpoint, p_seekpoint );
        else
            vlc_seekpoint_Delete( p_seekpoint );
    }
}

void vorbis_ParseComment( es_format_t *p_fmt, vlc_meta_t **pp_meta,
        const uint8_t *p_data, size_t i_data,
        int *i_attachments, input_attachment_t ***attachments,
        int *i_cover_score, int *i_cover_idx,
        int *i_seekpoint, seekpoint_t ***ppp_seekpoint,
        float (* ppf_replay_gain)[AUDIO_REPLAY_GAIN_MAX],
        float (* ppf_replay_peak)[AUDIO_REPLAY_GAIN_MAX] )
{
    if( i_data < 8 )
        return;

    uint32_t vendor_length = GetDWLE(p_data); RM(4);

    if( vendor_length > i_data )
        return; /* invalid length */

    RM(vendor_length); /* TODO: handle vendor payload */

    if( i_data < 4 )
        return;

    uint32_t i_comment = GetDWLE(p_data); RM(4);

    if( i_comment > i_data || i_comment == 0 )
        return; /* invalid length */

    /* */
    vlc_meta_t *p_meta = *pp_meta;
    if( !p_meta )
        *pp_meta = p_meta = vlc_meta_New();

    if( unlikely( !p_meta ) )
        return;

    /* */
    unsigned hasMetaFlags = 0;

    chapters_array_t chapters_array = { 0, NULL };

    for( ; i_comment > 0 && i_data >= 4; i_comment-- )
    {
        uint32_t comment_size = GetDWLE(p_data); RM(4);

        if( comment_size > i_data )
            break;

        if( comment_size == 0 )
            continue;

        char* psz_comment = malloc( comment_size + 1 );

        if( unlikely( !psz_comment ) )
            goto next_comment;

        memcpy( psz_comment, p_data, comment_size );
        psz_comment[comment_size] = '\0';

        EnsureUTF8( psz_comment );

#define IF_EXTRACT(txt,var) \
    if( !strncasecmp(psz_comment, txt, strlen(txt)) ) \
    { \
        const char *oldval = vlc_meta_Get( p_meta, vlc_meta_ ## var ); \
        if( oldval && (hasMetaFlags & XIPHMETA_##var)) \
        { \
            char * newval; \
            if( asprintf( &newval, "%s,%s", oldval, &psz_comment[strlen(txt)] ) == -1 ) \
                newval = NULL; \
            vlc_meta_Set( p_meta, vlc_meta_ ## var, newval ); \
            free( newval ); \
        } \
        else \
            vlc_meta_Set( p_meta, vlc_meta_ ## var, &psz_comment[strlen(txt)] ); \
        hasMetaFlags |= XIPHMETA_##var; \
    }

#define IF_EXTRACT_ONCE(txt,var) \
    if( !strncasecmp(psz_comment, txt, strlen(txt)) && !(hasMetaFlags & XIPHMETA_##var) ) \
    { \
        vlc_meta_Set( p_meta, vlc_meta_ ## var, &psz_comment[strlen(txt)] ); \
        hasMetaFlags |= XIPHMETA_##var; \
    }

#define IF_EXTRACT_FMT(txt,var,fmt,target) \
    if( !strncasecmp(psz_comment, txt, strlen(txt)) ) \
    { \
        IF_EXTRACT(txt,var)\
        if( fmt )\
        {\
            free( fmt->target );\
            fmt->target = strdup(&psz_comment[strlen(txt)]);\
        }\
    }

        IF_EXTRACT("TITLE=", Title )
        else IF_EXTRACT("ARTIST=", Artist )
        else IF_EXTRACT("GENRE=", Genre )
        else IF_EXTRACT("COPYRIGHT=", Copyright )
        else IF_EXTRACT("ALBUM=", Album )
        else if( !(hasMetaFlags & XIPHMETA_TrackNum) && !strncasecmp(psz_comment, "TRACKNUMBER=", strlen("TRACKNUMBER=" ) ) )
        {
            /* Yeah yeah, such a clever idea, let's put xx/xx inside TRACKNUMBER
             * Oh, and let's not use TRACKTOTAL or TOTALTRACKS... */
            short unsigned u_track, u_total;
            int nb_values = sscanf( &psz_comment[strlen("TRACKNUMBER=")], "%hu/%hu", &u_track, &u_total );
            if( nb_values >= 1 )
            {
                char str[6];
                snprintf(str, 6, "%u", u_track);
                vlc_meta_Set( p_meta, vlc_meta_TrackNumber, str );
                hasMetaFlags |= XIPHMETA_TrackNum;
                if( nb_values >= 2 )
                {
                    snprintf(str, 6, "%u", u_total);
                    vlc_meta_Set( p_meta, vlc_meta_TrackTotal, str );
                    hasMetaFlags |= XIPHMETA_TrackTotal;
                }
            }
        }
        else IF_EXTRACT_ONCE("TRACKTOTAL=", TrackTotal )
        else IF_EXTRACT_ONCE("TOTALTRACKS=", TrackTotal )
        else IF_EXTRACT("DESCRIPTION=", Description )
        else IF_EXTRACT("COMMENT=", Description )
        else IF_EXTRACT("COMMENTS=", Description )
        else IF_EXTRACT("RATING=", Rating )
        else IF_EXTRACT("DATE=", Date )
        else IF_EXTRACT_FMT("LANGUAGE=", Language, p_fmt, psz_language )
        else IF_EXTRACT("ORGANIZATION=", Publisher )
        else IF_EXTRACT("ENCODER=", EncodedBy )
        else if( !strncasecmp( psz_comment, "METADATA_BLOCK_PICTURE=", strlen("METADATA_BLOCK_PICTURE=")))
        {
            if( attachments == NULL )
                goto next_comment;

            uint8_t *p_picture;
            size_t i_size = vlc_b64_decode_binary( &p_picture, &psz_comment[strlen("METADATA_BLOCK_PICTURE=")]);
            input_attachment_t *p_attachment = ParseFlacPicture( p_picture,
                i_size, *i_attachments, i_cover_score, i_cover_idx );
            free( p_picture );
            if( p_attachment )
            {
                TAB_APPEND_CAST( (input_attachment_t**),
                    *i_attachments, *attachments, p_attachment );
            }
        }
        else if ( ppf_replay_gain && ppf_replay_peak && !strncmp(psz_comment, "REPLAYGAIN_", 11) )
        {
            char *p = strchr( psz_comment, '=' );
            if (!p) goto next_comment;
            if ( !strncasecmp(psz_comment, "REPLAYGAIN_TRACK_GAIN=", 22) )
            {
                (*ppf_replay_gain)[AUDIO_REPLAY_GAIN_TRACK] = us_atof( ++p );
            }
            else if ( !strncasecmp(psz_comment, "REPLAYGAIN_ALBUM_GAIN=", 22) )
            {
                (*ppf_replay_gain)[AUDIO_REPLAY_GAIN_ALBUM] = us_atof( ++p );
            }
            else if ( !strncasecmp(psz_comment, "REPLAYGAIN_ALBUM_PEAK=", 22) )
            {
                (*ppf_replay_peak)[AUDIO_REPLAY_GAIN_ALBUM] = us_atof( ++p );
            }
            else if ( !strncasecmp(psz_comment, "REPLAYGAIN_TRACK_PEAK=", 22) )
            {
                (*ppf_replay_peak)[AUDIO_REPLAY_GAIN_TRACK] = us_atof( ++p );
            }
        }
        else if( !strncasecmp(psz_comment, "CHAPTER", 7) )
        {
            unsigned int i_chapt;
            seekpoint_t *p_seekpoint = NULL;

            for( int i = 0; psz_comment[i] && psz_comment[i] != '='; i++ )
                if( psz_comment[i] >= 'a' && psz_comment[i] <= 'z' )
                    psz_comment[i] -= 'a' - 'A';

            if( strstr( psz_comment, "NAME=" ) &&
                    sscanf( psz_comment, "CHAPTER%uNAME=", &i_chapt ) == 1 )
            {
                char *p = strchr( psz_comment, '=' );
                p_seekpoint = getChapterEntry( i_chapt, &chapters_array );
                if ( !p || ! p_seekpoint ) goto next_comment;
                if ( ! p_seekpoint->psz_name )
                    p_seekpoint->psz_name = strdup( ++p );
            }
            else if( sscanf( psz_comment, "CHAPTER%u=", &i_chapt ) == 1 )
            {
                unsigned int h, m, s, ms;
                char *p = strchr( psz_comment, '=' );
                if( p && sscanf( ++p, "%u:%u:%u.%u", &h, &m, &s, &ms ) == 4 )
                {
                    p_seekpoint = getChapterEntry( i_chapt, &chapters_array );
                    if ( ! p_seekpoint ) goto next_comment;
                    p_seekpoint->i_time_offset = vlc_tick_from_sec(h * 3600 + m * 60 + s) + VLC_TICK_FROM_MS(ms);
                }
            }
        }
        else if( !strncasecmp(psz_comment, "cuesheet=", 9) )
        {
            xiph_ParseCueSheet( &hasMetaFlags, p_meta, &psz_comment[9], comment_size - 9,
                                i_seekpoint, ppp_seekpoint );
        }
        else if( strchr( psz_comment, '=' ) )
        {
            /* generic (PERFORMER/LICENSE/ORGANIZATION/LOCATION/CONTACT/ISRC,
             * undocumented tags and replay gain ) */
            char *p = strchr( psz_comment, '=' );
            *p++ = '\0';

            for( int i = 0; psz_comment[i]; i++ )
                if( psz_comment[i] >= 'a' && psz_comment[i] <= 'z' )
                    psz_comment[i] -= 'a' - 'A';

            vlc_meta_AddExtra( p_meta, psz_comment, p );
        }
#undef IF_EXTRACT
next_comment:
        free( psz_comment );
        RM( comment_size );
    }
#undef RM

    for ( unsigned int i=0; i<chapters_array.i_size; i++ )
    {
        if ( !chapters_array.pp_chapters[i] ) continue;
        TAB_APPEND_CAST( (seekpoint_t**), *i_seekpoint, *ppp_seekpoint,
                         chapters_array.pp_chapters[i] );
    }
    free( chapters_array.pp_chapters );
}

const char *FindKateCategoryName( const char *psz_tag )
{
    for( size_t i = 0; i < sizeof(Katei18nCategories)/sizeof(Katei18nCategories[0]); i++ )
    {
        if( !strcmp( psz_tag, Katei18nCategories[i].psz_tag ) )
            return Katei18nCategories[i].psz_i18n;
    }
    return N_("Unknown category");
}

