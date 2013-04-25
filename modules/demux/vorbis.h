/*****************************************************************************
 * vorbis.h: Vorbis Comment parser
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <vlc_charset.h>
#include <vlc_strings.h>
#include <vlc_input.h>

static input_attachment_t* ParseFlacPicture( const uint8_t *p_data, int i_data,
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

    int i_len;
    int i_type;
    char *psz_mime = NULL;
    char psz_name[128];
    char *psz_description = NULL;
    input_attachment_t *p_attachment = NULL;

    if( i_data < 4 + 3*4 )
        return NULL;
#define RM(x) do { i_data -= (x); p_data += (x); } while(0)

    i_type = GetDWBE( p_data ); RM(4);
    i_len = GetDWBE( p_data ); RM(4);

    if( i_len < 0 || i_data < i_len + 4 )
        goto error;
    psz_mime = strndup( (const char*)p_data, i_len ); RM(i_len);
    i_len = GetDWBE( p_data ); RM(4);
    if( i_len < 0 || i_data < i_len + 4*4 + 4)
        goto error;
    psz_description = strndup( (const char*)p_data, i_len ); RM(i_len);
    EnsureUTF8( psz_description );
    RM(4*4);
    i_len = GetDWBE( p_data ); RM(4);
    if( i_len < 0 || i_len > i_data )
        goto error;

    /* printf( "Picture type=%d mime=%s description='%s' file length=%d\n",
             i_type, psz_mime, psz_description, i_len ); */

    snprintf( psz_name, sizeof(psz_name), "picture%d", i_attachments );
    if( !strcasecmp( psz_mime, "image/jpeg" ) )
        strcat( psz_name, ".jpg" );
    else if( !strcasecmp( psz_mime, "image/png" ) )
        strcat( psz_name, ".png" );

    p_attachment = vlc_input_attachment_New( psz_name, psz_mime,
            psz_description, p_data, i_data );

    if( i_type >= 0 && (unsigned int)i_type < sizeof(pi_cover_score)/sizeof(pi_cover_score[0]) &&
        *i_cover_score < pi_cover_score[i_type] )
    {
        *i_cover_idx = i_attachments;
        *i_cover_score = pi_cover_score[i_type];
    }

error:
    free( psz_mime );
    free( psz_description );
    return p_attachment;
}

static inline void vorbis_ParseComment( vlc_meta_t **pp_meta,
        const uint8_t *p_data, int i_data,
        int *i_attachments, input_attachment_t ***attachments,
        int *i_cover_score, int *i_cover_idx,
        int *i_seekpoint, seekpoint_t ***ppp_seekpoint )
{
    int n;
    int i_comment;
    seekpoint_t *sk = NULL;

    if( i_data < 8 )
        return;

    n = GetDWLE(p_data); RM(4);
    if( n < 0 || n > i_data )
        return;
#if 0
    if( n > 0 )
    {
        /* TODO report vendor string ? */
        char *psz_vendor = psz_vendor = strndup( p_data, n );
        free( psz_vendor );
    }
#endif
    RM(n);

    if( i_data < 4 )
        return;

    i_comment = GetDWLE(p_data); RM(4);
    if( i_comment <= 0 )
        return;

    /* */
    vlc_meta_t *p_meta = *pp_meta;
    if( !p_meta )
        *pp_meta = p_meta = vlc_meta_New();
    if( !p_meta )
        return;

    /* */
    bool hasTitle        = false;
    bool hasAlbum        = false;
    bool hasTrackNumber  = false;
    bool hasTrackTotal   = false;
    bool hasArtist       = false;
    bool hasCopyright    = false;
    bool hasDescription  = false;
    bool hasGenre        = false;
    bool hasDate         = false;
    bool hasPublisher    = false;

    for( ; i_comment > 0; i_comment-- )
    {
        char *psz_comment;
        if( i_data < 4 )
            break;
        n = GetDWLE(p_data); RM(4);
        if( n > i_data )
            break;
        if( n <= 0 )
            continue;

        psz_comment = strndup( (const char*)p_data, n );
        RM(n);

        EnsureUTF8( psz_comment );

#define IF_EXTRACT(txt,var) \
    if( !strncasecmp(psz_comment, txt, strlen(txt)) ) \
    { \
        const char *oldval = vlc_meta_Get( p_meta, vlc_meta_ ## var ); \
        if( oldval && has##var) \
        { \
            char * newval; \
            if( asprintf( &newval, "%s,%s", oldval, &psz_comment[strlen(txt)] ) == -1 ) \
                newval = NULL; \
            vlc_meta_Set( p_meta, vlc_meta_ ## var, newval ); \
            free( newval ); \
        } \
        else \
            vlc_meta_Set( p_meta, vlc_meta_ ## var, &psz_comment[strlen(txt)] ); \
        has##var = true; \
    }
        IF_EXTRACT("TITLE=", Title )
        else IF_EXTRACT("ALBUM=", Album )
        else IF_EXTRACT("TRACKNUMBER=", TrackNumber )
        else if( !strncasecmp(psz_comment, "TRACKTOTAL=", strlen("TRACKTOTAL=")))
            vlc_meta_Set( p_meta, vlc_meta_TrackTotal, &psz_comment[strlen("TRACKTOTAL=")] );
        else if( !strncasecmp(psz_comment, "TOTALTRACKS=", strlen("TOTALTRACKS=")))
            vlc_meta_Set( p_meta, vlc_meta_TrackTotal, &psz_comment[strlen("TOTALTRACKS=")] );
        else IF_EXTRACT("TOTALTRACKS=", TrackTotal )
        else IF_EXTRACT("ARTIST=", Artist )
        else IF_EXTRACT("COPYRIGHT=", Copyright )
        else IF_EXTRACT("ORGANIZATION=", Publisher )
        else IF_EXTRACT("DESCRIPTION=", Description )
        else IF_EXTRACT("COMMENTS=", Description )
        else IF_EXTRACT("GENRE=", Genre )
        else IF_EXTRACT("DATE=", Date )
        else if( !strncasecmp( psz_comment, "METADATA_BLOCK_PICTURE=", strlen("METADATA_BLOCK_PICTURE=")))
        {
            if( attachments == NULL )
                continue;

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
        else if( !strncasecmp(psz_comment, "chapter", strlen("chapter")) )
        {
            if( ppp_seekpoint == NULL )
                continue;

            int i_chapt;
            if( strstr( psz_comment, "name") && sscanf( psz_comment, "chapter%i=", &i_chapt ) == 1 )
            {
                char *p = strchr( psz_comment, '=' );
                *p++ = '\0';
                sk->psz_name = strdup( p );
            }
            else if( sscanf( psz_comment, "chapter %i=", &i_chapt ) == 1 )
            {
                int h, m, s, ms;
                char *p = strchr( psz_comment, '=' );
                *p++ = '\0';

                if( sscanf( p, "%d:%d:%d.%d", &h, &m, &s, &ms ) == 4 )
                {
                    sk = vlc_seekpoint_New();
                    sk->i_time_offset = ((h * 3600 + m * 60 + s) *1000 + ms) * 1000;
                    TAB_APPEND_CAST( (seekpoint_t**), *i_seekpoint, *ppp_seekpoint, sk );
                }
            }
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
        free( psz_comment );
    }
#undef RM
}

