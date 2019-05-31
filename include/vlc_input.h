/*****************************************************************************
 * vlc_input.h: Core input structures
 *****************************************************************************
 * Copyright (C) 1999-2015 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef VLC_INPUT_H
#define VLC_INPUT_H 1

/**
 * \defgroup input Input
 * \ingroup vlc
 * Input thread
 * @{
 * \file
 * Input thread interface
 */

#include <vlc_es.h>
#include <vlc_meta.h>
#include <vlc_epg.h>
#include <vlc_events.h>
#include <vlc_input_item.h>
#include <vlc_vout.h>
#include <vlc_vout_osd.h>

#include <string.h>

typedef struct input_resource_t input_resource_t;

/*****************************************************************************
 * Seek point: (generalisation of chapters)
 *****************************************************************************/
struct seekpoint_t
{
    vlc_tick_t i_time_offset;
    char    *psz_name;
};

static inline seekpoint_t *vlc_seekpoint_New( void )
{
    seekpoint_t *point = (seekpoint_t*)malloc( sizeof( seekpoint_t ) );
    if( !point )
        return NULL;
    point->i_time_offset = -1;
    point->psz_name = NULL;
    return point;
}

static inline void vlc_seekpoint_Delete( seekpoint_t *point )
{
    if( !point ) return;
    free( point->psz_name );
    free( point );
}

static inline seekpoint_t *vlc_seekpoint_Duplicate( const seekpoint_t *src )
{
    seekpoint_t *point = vlc_seekpoint_New();
    if( likely(point) )
    {
        if( src->psz_name ) point->psz_name = strdup( src->psz_name );
        point->i_time_offset = src->i_time_offset;
    }
    return point;
}

/*****************************************************************************
 * Title:
 *****************************************************************************/

/* input_title_t.i_flags field */
#define INPUT_TITLE_MENU         0x01   /* Menu title */
#define INPUT_TITLE_INTERACTIVE  0x02   /* Interactive title. Playback position has no meaning. */

typedef struct input_title_t
{
    char        *psz_name;

    vlc_tick_t  i_length;   /* Length(microsecond) if known, else 0 */

    unsigned    i_flags;    /* Is it a menu or a normal entry */

    /* Title seekpoint */
    int         i_seekpoint;
    seekpoint_t **seekpoint;
} input_title_t;

static inline input_title_t *vlc_input_title_New(void)
{
    input_title_t *t = (input_title_t*)malloc( sizeof( input_title_t ) );
    if( !t )
        return NULL;

    t->psz_name = NULL;
    t->i_flags = 0;
    t->i_length = 0;
    t->i_seekpoint = 0;
    t->seekpoint = NULL;

    return t;
}

static inline void vlc_input_title_Delete( input_title_t *t )
{
    int i;
    if( t == NULL )
        return;

    free( t->psz_name );
    for( i = 0; i < t->i_seekpoint; i++ )
        vlc_seekpoint_Delete( t->seekpoint[i] );
    free( t->seekpoint );
    free( t );
}

static inline input_title_t *vlc_input_title_Duplicate( const input_title_t *t )
{
    input_title_t *dup = vlc_input_title_New( );
    if( dup == NULL) return NULL;

    if( t->psz_name ) dup->psz_name = strdup( t->psz_name );
    dup->i_flags     = t->i_flags;
    dup->i_length    = t->i_length;
    if( t->i_seekpoint > 0 )
    {
        dup->seekpoint = (seekpoint_t**)vlc_alloc( t->i_seekpoint, sizeof(seekpoint_t*) );
        if( likely(dup->seekpoint) )
        {
            for( int i = 0; i < t->i_seekpoint; i++ )
                dup->seekpoint[i] = vlc_seekpoint_Duplicate( t->seekpoint[i] );
            dup->i_seekpoint = t->i_seekpoint;
        }
    }

    return dup;
}

/*****************************************************************************
 * Attachments
 *****************************************************************************/
struct input_attachment_t
{
    char *psz_name;
    char *psz_mime;
    char *psz_description;

    size_t i_data;
    void *p_data;
};

static inline void vlc_input_attachment_Delete( input_attachment_t *a )
{
    if( !a )
        return;

    free( a->p_data );
    free( a->psz_description );
    free( a->psz_mime );
    free( a->psz_name );
    free( a );
}

static inline input_attachment_t *vlc_input_attachment_New( const char *psz_name,
                                                            const char *psz_mime,
                                                            const char *psz_description,
                                                            const void *p_data,
                                                            size_t i_data )
{
    input_attachment_t *a = (input_attachment_t *)malloc( sizeof (*a) );
    if( unlikely(a == NULL) )
        return NULL;

    a->psz_name = strdup( psz_name ? psz_name : "" );
    a->psz_mime = strdup( psz_mime ? psz_mime : "" );
    a->psz_description = strdup( psz_description ? psz_description : "" );
    a->i_data = i_data;
    a->p_data = malloc( i_data );
    if( i_data > 0 && likely(a->p_data != NULL) )
        memcpy( a->p_data, p_data, i_data );

    if( unlikely(a->psz_name == NULL || a->psz_mime == NULL
              || a->psz_description == NULL || (i_data > 0 && a->p_data == NULL)) )
    {
        vlc_input_attachment_Delete( a );
        a = NULL;
    }
    return a;
}

static inline input_attachment_t *vlc_input_attachment_Duplicate( const input_attachment_t *a )
{
    return vlc_input_attachment_New( a->psz_name, a->psz_mime, a->psz_description,
                                     a->p_data, a->i_data );
}

/**
 * Input rate.
 *
 * It is an float used by the variable "rate" in the
 * range [INPUT_RATE_MIN, INPUT_RATE_MAX]
 * the default value being 1.f. It represents the ratio of playback speed to
 * nominal speed (bigger is faster).
 */

/**
 * Minimal rate value
 */
#define INPUT_RATE_MIN 0.03125f
/**
 * Maximal rate value
 */
#define INPUT_RATE_MAX 31.25f

/** @} */
#endif
