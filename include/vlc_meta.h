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
#define VLC_META_URL                N_("Url")
#define VLC_META_LANGUAGE           N_("Language")
#define VLC_META_CODEC_NAME         N_("Codec Name")
#define VLC_META_CODEC_DESCRIPTION  N_("Codec Description")

struct vlc_meta_t
{
    /* meta name/value pairs */
    int     i_meta;
    char    **name;
    char    **value;

    /* track meta informations */
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
static inline void vlc_meta_Add( vlc_meta_t *m, char *name, char *value )
{
    int i_meta = m->i_meta;

    name = strdup( name );
    value = strdup( value );

    TAB_APPEND( m->i_meta, m->name, name );
    TAB_APPEND( i_meta,    m->value,value );
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
        TAB_APPEND( dst->i_track, dst->track, tk );
    }
    return dst;
}

#endif

