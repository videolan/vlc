/*****************************************************************************
 * vlc_meta.h
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef _VLC_META_H
#define _VLC_META_H 1

/* VLC meta name */
#define VLC_META_TITLE              N_("Title")
#define VLC_META_AUTHOR             N_("Author")
#define VLC_META_ARTIST             N_("Artist")
#define VLC_META_GENRE              N_("Genre")
#define VLC_META_COPYRIGHT          N_("Copyright")
#define VLC_META_DESCRIPTION        N_("Description")
#define VLC_META_RATING             N_("Rating")
#define VLC_META_DATE               N_("Date")
#define VLC_META_SETTING            N_("Setting")
#define VLC_META_URL                N_("URL")
#define VLC_META_LANGUAGE           N_("Language")
#define VLC_META_NOW_PLAYING        N_("Now Playing")

#define VLC_META_CDDB_ARTIST        N_("CDDB Artist")
#define VLC_META_CDDB_CATEGORY      N_("CDDB Category")  
#define VLC_META_CDDB_DISCID        N_("CDDB Disc ID")
#define VLC_META_CDDB_EXT_DATA      N_("CDDB Extended Data")
#define VLC_META_CDDB_GENRE         N_("CDDB Genre")
#define VLC_META_CDDB_YEAR          N_("CDDB Year")      
#define VLC_META_CDDB_TITLE         N_("CDDB Title")

#define VLC_META_CDTEXT_ARRANGER    N_("CD-Text Arranger")
#define VLC_META_CDTEXT_COMPOSER    N_("CD-Text Composer")
#define VLC_META_CDTEXT_DISCID      N_("CD-Text Disc ID")
#define VLC_META_CDTEXT_GENRE       N_("CD-Text Genre")
#define VLC_META_CDTEXT_MESSAGE     N_("CD-Text Message")
#define VLC_META_CDTEXT_SONGWRITER  N_("CD-Text Songwriter")
#define VLC_META_CDTEXT_PERFORMER   N_("CD-Text Performer")
#define VLC_META_CDTEXT_TITLE       N_("CD-Text Title")

#define VLC_META_ISO_APPLICATION_ID N_("ISO-9660 Application ID") 
#define VLC_META_ISO_PREPARER       N_("ISO-9660 Preparer") 
#define VLC_META_ISO_PUBLISHER      N_("ISO-9660 Publisher") 
#define VLC_META_ISO_VOLUME         N_("ISO-9660 Volume") 
#define VLC_META_ISO_VOLUMESET      N_("ISO-9660 Volume Set") 

#define VLC_META_CODEC_NAME         N_("Codec Name")
#define VLC_META_CODEC_DESCRIPTION  N_("Codec Description")

struct vlc_meta_t
{
    /* meta name/value pairs */
    int     i_meta;
    char    **name;
    char    **value;

    /* track meta information */
    int         i_track;
    vlc_meta_t  **track;
};

static inline vlc_meta_t *vlc_meta_New( void )
{
    vlc_meta_t *m = (vlc_meta_t*)malloc( sizeof( vlc_meta_t ) );

    m->i_meta = 0;
    m->name   = NULL;
    m->value  = NULL;

    m->i_track= 0;
    m->track  = NULL;

    return m;
}

static inline void vlc_meta_Delete( vlc_meta_t *m )
{
    int i;
    for( i = 0; i < m->i_meta; i++ )
    {
        free( m->name[i] );
        free( m->value[i] );
    }
    if( m->name ) free( m->name );
    if( m->value ) free( m->value );

    for( i = 0; i < m->i_track; i++ )
    {
        vlc_meta_Delete( m->track[i] );
    }
    if( m->track ) free( m->track );
    free( m );
}

static inline void vlc_meta_Add( vlc_meta_t *m,
                                 const char *name, const char *value )
{
    m->name  = (char**)realloc( m->name, sizeof(char*) * ( m->i_meta + 1 ) );
    m->name[m->i_meta] = strdup( name );

    m->value = (char**)realloc( m->value, sizeof(char*) * ( m->i_meta + 1 ) );
    m->value[m->i_meta] = strdup( value );

    m->i_meta++;
}

static inline vlc_meta_t *vlc_meta_Duplicate( vlc_meta_t *src )
{
    vlc_meta_t *dst = vlc_meta_New();
    int i;
    for( i = 0; i < src->i_meta; i++ )
    {
        vlc_meta_Add( dst, src->name[i], src->value[i] );
    }
    for( i = 0; i < src->i_track; i++ )
    {
        vlc_meta_t *tk = vlc_meta_Duplicate( src->track[i] );

        dst->track = (vlc_meta_t**)realloc( dst->track, sizeof( vlc_meta_t* ) * (dst->i_track+1) );
        dst->track[dst->i_track++] = tk;
    }
    return dst;
}

static inline void vlc_meta_Merge( vlc_meta_t *dst, vlc_meta_t *src )
{
    int i, j;
    for( i = 0; i < src->i_meta; i++ )
    {
        /* Check if dst contains the entry */
        for( j = 0; j < dst->i_meta; j++ )
        {
            if( !strcmp( src->name[i], dst->name[j] ) ) break;
        }
        if( j < dst->i_meta )
        {
            if( dst->value[j] ) free( dst->value[j] );
            dst->value[j] = strdup( src->value[i] );
        }
        else vlc_meta_Add( dst, src->name[i], src->value[i] );
    }
}

static inline char *vlc_meta_GetValue( vlc_meta_t *m, const char *name )
{
    int i;

    for( i = 0; i < m->i_meta; i++ )
    {
        if( !strcmp( m->name[i], name ) )
        {
            char *value = NULL;
            if( m->value[i] ) value = strdup( m->value[i] );
            return value;
        }
    }
    return NULL;
}

#endif
