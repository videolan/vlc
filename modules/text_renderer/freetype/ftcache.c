/*****************************************************************************
 * ftcache.c : Font Face and glyph cache freetype2
 *****************************************************************************
 * Copyright (C) 2020 - VideoLabs, VLC authors and VideoLAN
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_filter.h>
#include <vlc_text_style.h>

/* Freetype */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "ftcache.h"
#include "platform_fonts.h"
#include "lru.h"

struct vlc_ftcache_t
{
    vlc_object_t *obj;
    /** Font face cache */
    vlc_dictionary_t  face_ids;
    FTC_ScalerRec     scaler;
    FTC_Manager       cachemanager;
    FTC_ImageCache    image_cache;
    FTC_CMapCache     charmap_cache;
    /* Derived glyph cache */
    vlc_lru *         glyphs_lrucache;
    /* current face properties */
    FT_Long           style_flags;
};

struct vlc_ftcache_custom_glyph_ref_Rec_
{
    FT_Glyph glyph;
    unsigned refcount;
};

vlc_face_id_t * vlc_ftcache_GetFaceID( vlc_ftcache_t *ftcache,
                                       const char *psz_fontfile, int i_idx )
{
    char *psz_key;
    if( asprintf( &psz_key, "%s#%d",
                  psz_fontfile, i_idx ) < 0 )
        return NULL;

    vlc_face_id_t *faceid = vlc_dictionary_value_for_key( &ftcache->face_ids, psz_key );
    if( !faceid )
    {
        faceid = malloc(sizeof(vlc_face_id_t));
        if( faceid )
        {
            faceid->idx = i_idx;
            faceid->psz_filename = strdup(psz_fontfile);
            if( faceid->psz_filename )
            {
                faceid->charmap_index = -1;
                vlc_dictionary_insert( &ftcache->face_ids, psz_key, faceid );
            }
            else
            {
                free( faceid );
                faceid = NULL;
            }
        }
    }
    free( psz_key );
    return faceid;
}

vlc_face_id_t * vlc_ftcache_GetActiveFaceInfo( vlc_ftcache_t *ftcache,
                                               vlc_ftcache_metrics_t *metrics )
{
    if( metrics )
    {
        metrics->width_px = ftcache->scaler.width;
        metrics->height_px = ftcache->scaler.height;
    }
    return (vlc_face_id_t *) ftcache->scaler.face_id;
}

static int vlc_ftcache_SetCharmap( vlc_face_id_t *faceid, FT_Face p_face )
{
    if( FT_Select_Charmap( p_face, FT_ENCODING_UNICODE ) )
        /* We've loaded a font face which is unhelpful for actually rendering text */
        return VLC_EGENERIC;

    faceid->charmap_index =
            p_face->charmap ? FT_Get_Charmap_Index( p_face->charmap ) : 0;

    return VLC_SUCCESS;
}

FT_Face vlc_ftcache_LoadFaceByID( vlc_ftcache_t *ftcache, vlc_face_id_t *faceid,
                                  const vlc_ftcache_metrics_t *metrics )
{
    ftcache->scaler.face_id = faceid;
    ftcache->scaler.width = metrics->width_px;
    ftcache->scaler.height = metrics->height_px;

    FT_Size size;
    if(FTC_Manager_LookupSize( ftcache->cachemanager, &ftcache->scaler, &size ))
    {
        ftcache->scaler.face_id = NULL;
        ftcache->scaler.width = 0;
        ftcache->scaler.height = 0;
        return NULL;
    }

    return size->face;
}

int vlc_ftcache_LoadFaceByIDNoSize( vlc_ftcache_t *ftcache, vlc_face_id_t *faceid )
{
    FT_Face face;
    if( FTC_Manager_LookupFace( ftcache->cachemanager, faceid, &face ) )
        return VLC_EGENERIC;

    ftcache->style_flags = face->style_flags;
    ftcache->scaler.face_id = faceid;
    ftcache->scaler.width = 0;
    ftcache->scaler.height = 0;
    return VLC_SUCCESS;
}

FT_UInt vlc_ftcache_LookupCMapIndex( vlc_ftcache_t *ftcache, vlc_face_id_t *faceid, FT_UInt codepoint )
{
    return FTC_CMapCache_Lookup( ftcache->charmap_cache, faceid,
                                 faceid->charmap_index, codepoint );
}

int vlc_ftcache_GetGlyphForCurrentFace( vlc_ftcache_t *ftcache, FT_UInt index,
                                        vlc_ftcache_glyph_t *glyph, FT_Long *styleflags )
{
    int ret = FTC_ImageCache_LookupScaler( ftcache->image_cache,
                                           &ftcache->scaler,
                                           FT_LOAD_NO_BITMAP | FT_LOAD_DEFAULT,
                                           index, &glyph->p_glyph, &glyph->ref );
    if( !ret && styleflags )
        *styleflags = ftcache->style_flags;
    return ret;
}

static FT_Error RequestFace( FTC_FaceID face_id, FT_Library library,
                             FT_Pointer req_data, FT_Face* aface )
{
    vlc_ftcache_t *ftcache = (vlc_ftcache_t *) req_data;
    vlc_face_id_t *faceid = (vlc_face_id_t *) face_id;
    VLC_UNUSED(library);

    *aface = doLoadFace( ftcache->obj, faceid->psz_filename, faceid->idx);

    if( !*aface || vlc_ftcache_SetCharmap( faceid, *aface ) )
        return -1;

    return 0;
}

static void LRUGlyphRefRelease( void *priv, void *v )
{
    vlc_ftcache_custom_glyph_ref_t ref = (vlc_ftcache_custom_glyph_ref_t) v;
    VLC_UNUSED(priv);
    assert(ref->refcount);
    if( --ref->refcount == 0 )
    {
        FT_Done_Glyph( ref->glyph );
        free( ref );
    }
}

static void FreeFaceID( void *p_faceid, void *p_obj )
{
    VLC_UNUSED(p_obj);
    vlc_face_id_t *faceid = p_faceid;
    free(faceid->psz_filename);
    free(faceid);
}

void vlc_ftcache_Delete( vlc_ftcache_t *ftcache )
{
    if( ftcache->glyphs_lrucache )
        vlc_lru_Release( ftcache->glyphs_lrucache );

    if( ftcache->cachemanager )
        FTC_Manager_Done( ftcache->cachemanager );

    /* Fonts dicts */
    vlc_dictionary_clear( &ftcache->face_ids, FreeFaceID, NULL );

    free( ftcache );
}

vlc_ftcache_t * vlc_ftcache_New( vlc_object_t *obj, FT_Library p_library,
                                 unsigned maxkb )
{
    vlc_ftcache_t *ftcache = calloc(1, sizeof(*ftcache));
    if(!ftcache)
        return NULL;
    ftcache->obj = obj;

    /* Dictionnaries for fonts */
    vlc_dictionary_init( &ftcache->face_ids, 50 );

    ftcache->glyphs_lrucache = vlc_lru_New( 128, LRUGlyphRefRelease, ftcache );

    if(!ftcache->glyphs_lrucache ||
       FTC_Manager_New( p_library, 4, 8, maxkb << 10,
                        RequestFace, ftcache, &ftcache->cachemanager ) ||
       FTC_ImageCache_New( ftcache->cachemanager, &ftcache->image_cache ) ||
       FTC_CMapCache_New( ftcache->cachemanager, &ftcache->charmap_cache ))
    {
        vlc_ftcache_Delete( ftcache );
        return NULL;
    }

    ftcache->scaler.pixel = 1;
    ftcache->scaler.x_res = 0;
    ftcache->scaler.y_res = 0;

    return ftcache;
}

void vlc_ftcache_Glyph_Release( vlc_ftcache_t *ftcache, vlc_ftcache_glyph_t *g )
{
    if( g->ref )
    {
        FTC_Node_Unref( g->ref, ftcache->cachemanager );
        g->ref = NULL;
        g->p_glyph = NULL;
    }
    else if( g->p_glyph ) /* glyph is not cache reference */
    {
        FT_Done_Glyph( g->p_glyph );
        g->p_glyph = NULL;
    }
}

void vlc_ftcache_Glyph_Init( vlc_ftcache_glyph_t *g )
{
    g->p_glyph = NULL;
    g->ref = NULL;
}

static vlc_ftcache_custom_glyph_ref_t
vlc_ftcache_GetCustomGlyph( vlc_ftcache_t *ftcache, const char *psz_key )
{
    vlc_ftcache_custom_glyph_ref_t ref = vlc_lru_Get( ftcache->glyphs_lrucache, psz_key );
    if( ref )
        ref->refcount++;
    return ref;
}

static vlc_ftcache_custom_glyph_ref_t
vlc_ftcache_AddCustomGlyph( vlc_ftcache_t *ftcache, const char *psz_key, FT_Glyph glyph )
{
    assert(!vlc_lru_Get( ftcache->glyphs_lrucache, psz_key ));
    vlc_ftcache_custom_glyph_ref_t ref = malloc( sizeof(*ref) );
    if( ref )
    {
        ref->refcount = 2;
        ref->glyph = glyph;
        vlc_lru_Insert( ftcache->glyphs_lrucache, psz_key, ref );
    }
    return ref;
}

void vlc_ftcache_Custom_Glyph_Release( vlc_ftcache_custom_glyph_t *g )
{
    if( g->ref )
    {
        assert(g->ref->refcount);
        if( --g->ref->refcount == 0 )
        {
            FT_Done_Glyph( g->ref->glyph );
            free( g->ref );
        }
        vlc_ftcache_Custom_Glyph_Init( g );
    }
}

void vlc_ftcache_Custom_Glyph_Init( vlc_ftcache_custom_glyph_t *g )
{
    g->p_glyph = NULL;
    g->ref = NULL;
}

FT_Glyph vlc_ftcache_GetOutlinedGlyph( vlc_ftcache_t *ftcache, const vlc_face_id_t *faceid,
                                       FT_UInt index, const vlc_ftcache_metrics_t *metrics,
                                       FT_Long style, int radius, const FT_Glyph sourceglyph,
                                       int(*createOutline)(FT_Glyph, FT_Glyph *, void *),
                                       void *priv,
                                       vlc_ftcache_custom_glyph_ref_t *p_ref )
{
    char *psz_key;
    if( asprintf( &psz_key, "%s#%d#%d#%d,%d,%d,%lx,%d",
                  faceid->psz_filename, faceid->idx,
                  faceid->charmap_index, index,
                  metrics->width_px, metrics->height_px, style, radius ) < 0 )
    {
        *p_ref = NULL;
        return NULL;
    }

    FT_Glyph glyph = NULL;
    *p_ref = vlc_ftcache_GetCustomGlyph( ftcache, psz_key );
    if( *p_ref )
    {
        glyph = (*p_ref)->glyph;
    }
    else
    {
        if( !createOutline( sourceglyph, &glyph, priv ) )
            *p_ref = vlc_ftcache_AddCustomGlyph( ftcache, psz_key, glyph );
        if( !*p_ref )
        {
            FT_Done_Glyph( glyph );
            return NULL;
        }
    }
    free( psz_key );
    return glyph;
}
