/*****************************************************************************
 * vorbis.h: Vorbis Comment parser
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <vlc_charset.h>
#include <vlc_strings.h>

static input_attachment_t* ParseFlacPicture( const uint8_t *p_data, int i_data, int i_attachments, int *i_type )
{
    int i_len;
    char *psz_mime = NULL;
    char psz_name[128];
    char *psz_description = NULL;
    input_attachment_t *p_attachment = NULL;

    if( i_data < 4 + 3*4 )
        return NULL;
#define RM(x) do { i_data -= (x); p_data += (x); } while(0)

    *i_type = GetDWBE( p_data ); RM(4);
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
             *i_type, psz_mime, psz_description, i_len ); */

    snprintf( psz_name, sizeof(psz_name), "picture%d", i_attachments );
    if( !strcasecmp( psz_mime, "image/jpeg" ) )
        strcat( psz_name, ".jpg" );
    else if( !strcasecmp( psz_mime, "image/png" ) )
        strcat( psz_name, ".png" );

    p_attachment = vlc_input_attachment_New( psz_name, psz_mime,
            psz_description, p_data, i_data );

error:
    free( psz_mime );
    free( psz_description );
    return p_attachment;
}

static inline void vorbis_ParseComment( vlc_meta_t **pp_meta, const uint8_t *p_data, int i_data,
        int *i_attachments, input_attachment_t ***attachments)
{
    int n;
    int i_comment;
    int i_attach = 0;
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

    bool hasTitle = false;
    bool hasAlbum = false;
    bool hasTrackNumber = false;
    bool hasArtist = false;
    bool hasCopyright = false;
    bool hasDescription = false;
    bool hasGenre = false;
    bool hasDate = false;
    bool hasPublisher = false;

    for( ; i_comment > 0; i_comment-- )
    {
        char *psz;
        if( i_data < 4 )
            break;
        n = GetDWLE(p_data); RM(4);
        if( n > i_data )
            break;
        if( n <= 0 )
            continue;

        psz = strndup( (const char*)p_data, n );
        RM(n);

        EnsureUTF8( psz );

#define IF_EXTRACT(txt,var) \
    if( !strncasecmp(psz, txt, strlen(txt)) ) \
    { \
        const char *oldval = vlc_meta_Get( p_meta, vlc_meta_ ## var ); \
        if( oldval && has##var) \
        { \
            char * newval; \
            if( asprintf( &newval, "%s,%s", oldval, &psz[strlen(txt)] ) == -1 ) \
                newval = NULL; \
            vlc_meta_Set( p_meta, vlc_meta_ ## var, newval ); \
            free( newval ); \
        } \
        else \
            vlc_meta_Set( p_meta, vlc_meta_ ## var, &psz[strlen(txt)] ); \
        has##var = true; \
    }
        IF_EXTRACT("TITLE=", Title )
        else IF_EXTRACT("ALBUM=", Album )
        else IF_EXTRACT("TRACKNUMBER=", TrackNumber )
        else IF_EXTRACT("ARTIST=", Artist )
        else IF_EXTRACT("COPYRIGHT=", Copyright )
        else IF_EXTRACT("ORGANIZATION=", Publisher )
        else IF_EXTRACT("DESCRIPTION=", Description )
        else if( !hasDescription )
        {
            IF_EXTRACT("COMMENTS=", Description )
        }
        else IF_EXTRACT("GENRE=", Genre )
        else IF_EXTRACT("DATE=", Date )
        else if( !strncasecmp( psz, "METADATA_BLOCK_PICTURE=", strlen("METADATA_BLOCK_PICTURE=")))
        {
            if( attachments == NULL )
                continue;

            int i;
            uint8_t *p_picture;
            size_t i_size = vlc_b64_decode_binary( &p_picture, &psz[strlen("METADATA_BLOCK_PICTURE=")]);
            input_attachment_t *p_attachment = ParseFlacPicture( p_picture, i_size, i_attach, &i );
            if( p_attachment )
            {
                char psz_url[128];
                snprintf( psz_url, sizeof(psz_url), "attachment://%s", p_attachment->psz_name );
                vlc_meta_Set( p_meta, vlc_meta_ArtworkURL, psz_url );
                i_attach++;
                TAB_APPEND_CAST( (input_attachment_t**),
                    *i_attachments, *attachments, p_attachment );
            }
        }
        else if( strchr( psz, '=' ) )
        {
            /* generic (PERFORMER/LICENSE/ORGANIZATION/LOCATION/CONTACT/ISRC,
             * undocumented tags and replay gain ) */
            char *p = strchr( psz, '=' );
            *p++ = '\0';
            vlc_meta_AddExtra( p_meta, psz, p );
        }
#undef IF_EXTRACT
        free( psz );
    }
#undef RM
}

