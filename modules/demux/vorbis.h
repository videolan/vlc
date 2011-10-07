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

static inline void vorbis_ParseComment( vlc_meta_t **pp_meta, const uint8_t *p_data, int i_data )
{
    int n;
    int i_comment;
    if( i_data < 8 )
        return;

#define RM(x) do { i_data -= (x); p_data += (x); } while(0)
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
        else IF_EXTRACT("DESCRIPTION=", Description )
        else IF_EXTRACT("GENRE=", Genre )
        else IF_EXTRACT("DATE=", Date )
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

