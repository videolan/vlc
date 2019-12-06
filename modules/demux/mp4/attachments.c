/*****************************************************************************
 * attachments.c : MP4 attachments handling
 *****************************************************************************
 * Copyright (C) 2001-2015 VLC authors and VideoLAN
 *               2019 VideoLabs
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

#include <vlc_common.h>
#include <vlc_input.h>

#include "libmp4.h"
#include "attachments.h"
#include <limits.h>

static const char *psz_meta_roots[] = { "/moov/udta/meta/ilst",
                                        "/moov/meta/ilst",
                                        "/moov/udta/meta",
                                        "/moov/udta",
                                        "/meta/ilst",
                                        "/udta" };

const MP4_Box_t *MP4_GetMetaRoot( const MP4_Box_t *p_root, const char **ppsz_path )
{
    for( size_t i = 0; i < ARRAY_SIZE(psz_meta_roots); i++ )
    {
        MP4_Box_t *p_udta = MP4_BoxGet( p_root, psz_meta_roots[i] );
        if ( p_udta )
        {
            *ppsz_path = psz_meta_roots[i];
            return p_udta;
        }
    }
    return NULL;
}

static bool imageTypeCompatible( const MP4_Box_data_data_t *p_data )
{
    return p_data && (
    p_data->e_wellknowntype == DATA_WKT_PNG ||
    p_data->e_wellknowntype == DATA_WKT_JPEG ||
    p_data->e_wellknowntype == DATA_WKT_BMP );
}

static const MP4_Box_t * GetValidCovrMeta( const MP4_Box_t *p_data,
                                           unsigned *pi_index,
                                           const void **ctx )
{
    for( ; p_data; p_data = p_data->p_next )
    {
        if( p_data->i_type != ATOM_data || p_data == *ctx )
            continue;
        (*pi_index)++;
        if ( !imageTypeCompatible( BOXDATA(p_data) ) )
            continue;
        *ctx = p_data;
        return p_data;
    }
    return NULL;
}

static const MP4_Box_t * GetValidPnotMeta( const MP4_Box_t *p_pnot,
                                           unsigned *pi_index,
                                           const void **ctx )
{
    for( ; p_pnot; p_pnot = p_pnot->p_next )
    {
        if( p_pnot->i_type != ATOM_pnot || p_pnot == *ctx )
            continue;
        (*pi_index)++;
        if( BOXDATA(p_pnot)->i_type != ATOM_PICT &&
            BOXDATA(p_pnot)->i_type != ATOM_pict )
            continue;
        *ctx = p_pnot;
        return p_pnot;
    }
    return NULL;
}

static const MP4_Box_t * GetValidThumMeta( const MP4_Box_t *p_thum,
                                           unsigned *pi_index,
                                           const void **ctx )
{
    for( ; p_thum; p_thum = p_thum->p_next )
    {
        if( p_thum->i_type != ATOM_thum || p_thum == *ctx )
            continue;
        (*pi_index)++;
        if ( !p_thum->data.p_binary )
            continue;
        *ctx = p_thum;
        return p_thum;
    }
    return NULL;
}

int MP4_GetCoverMetaURI( const MP4_Box_t *p_root,
                         const MP4_Box_t *p_metaroot,
                         const char *psz_metapath,
                         vlc_meta_t *p_meta )
{
    bool b_attachment_set = false;

    if( !p_meta )
        return VLC_EGENERIC;

    if ( p_metaroot )
    {
        const MP4_Box_t *p_data = MP4_BoxGet( p_metaroot, "covr/data" );
        unsigned i_index = 0;
        const void *ctx = NULL;
        if( (p_data = GetValidCovrMeta( p_data, &i_index, &ctx )) )
        {
            char *psz_attachment;
            if ( -1 != asprintf( &psz_attachment,
                                 "attachment://%s/covr/data[%u]",
                                 psz_metapath, i_index - 1 ) )
            {
                vlc_meta_SetArtURL( p_meta, psz_attachment );
                b_attachment_set = true;
                free( psz_attachment );
            }
        }
    }

    const MP4_Box_t *p_pnot;
    if ( !b_attachment_set && (p_pnot = MP4_BoxGet( p_root, "pnot" )) )
    {
        unsigned i_index = 0;
        const void *ctx = NULL;
        if( (p_pnot = GetValidPnotMeta( p_pnot, &i_index, &ctx )) )
        {
            char *psz_attachment;
            if ( -1 != asprintf( &psz_attachment,
                                 "attachment://pnot[%u]", i_index - 1 ) )
            {
                vlc_meta_SetArtURL( p_meta, psz_attachment );
                b_attachment_set = true;
                free( psz_attachment );
            }
        }
    }


    const MP4_Box_t *p_thum;
    if( !b_attachment_set && (p_thum = MP4_BoxGet( p_root, "thum" )) )
    {
        unsigned i_index = 0;
        const void *ctx = NULL;
        if( (p_thum = GetValidThumMeta( p_thum, &i_index, &ctx )) )
        {
            char *psz_attachment;
            if ( -1 != asprintf( &psz_attachment,
                                 "attachment://thum[%u]",
                                 i_index - 1 ) )
            {
                vlc_meta_SetArtURL( p_meta, psz_attachment );
                b_attachment_set = true;
                free( psz_attachment );
            }
        }
    }

    if( !b_attachment_set )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

int MP4_GetAttachments( const MP4_Box_t *p_root, input_attachment_t ***ppp_attach )
{
    const MP4_Box_t *p_metaroot = NULL;
    const char *psz_metarootpath;
    unsigned i_count = 0;
    input_attachment_t **pp_attach = NULL;
    *ppp_attach = NULL;

    /* Count MAX number of total attachments */
    p_metaroot = MP4_GetMetaRoot( p_root, &psz_metarootpath );
    if( p_metaroot )
    {
        unsigned i_covercount = MP4_BoxCount( p_metaroot, "covr/data" );
        if( unlikely(i_covercount > INT_MAX - i_count) )
            return 0;
        i_count += i_covercount;
    }

    unsigned i_pictcount = MP4_BoxCount( p_root, "pnot" );
    if( unlikely(i_pictcount > INT_MAX - i_count) )
        return 0;
    i_count += i_pictcount;

    unsigned i_thmb_count = MP4_BoxCount( p_root, "thum" );
    if( unlikely(i_thmb_count > INT_MAX - i_count) )
        return 0;
    i_count += i_thmb_count;

    if ( i_count == 0 )
        return 0;

    pp_attach = vlc_alloc( i_count, sizeof(input_attachment_t*) );
    if( !(pp_attach) )
        return 0;

    /* Create and add valid attachments */
    i_count = 0;

    /* First add cover attachments */
    if ( p_metaroot )
    {
        const MP4_Box_t *p_data = MP4_BoxGet( p_metaroot, "covr/data" );
        unsigned i_index = 0;
        const void *ctx = NULL;
        while( (p_data = GetValidCovrMeta( p_data, &i_index, &ctx )) )
        {
            char *psz_mime;
            char *psz_filename;

            switch( BOXDATA(p_data)->e_wellknowntype )
            {
            case DATA_WKT_PNG:
                psz_mime = strdup( "image/png" );
                break;
            case DATA_WKT_JPEG:
                psz_mime = strdup( "image/jpeg" );
                break;
            case DATA_WKT_BMP:
                psz_mime = strdup( "image/bmp" );
                break;
            default:
                psz_mime = NULL;
                break;
            }

            if ( asprintf( &psz_filename, "%s/covr/data[%u]",
                           psz_metarootpath,
                           i_index - 1 ) > -1 )
            {
                input_attachment_t *p_attach =
                    vlc_input_attachment_New(
                            psz_filename,
                            psz_mime,
                            "Cover picture",
                            BOXDATA(p_data)->p_blob,
                            BOXDATA(p_data)->i_blob );
                free( psz_filename );
                if( p_attach )
                    pp_attach[i_count++] = p_attach;
            }

            free( psz_mime );
        }
    }

    /* Then quickdraw pict ones */
    const MP4_Box_t *p_pnot = MP4_BoxGet( p_root, "pnot" );
    if( p_pnot )
    {
        unsigned i_index = 0;
        const void *ctx = NULL;
        while( (p_pnot = GetValidPnotMeta( p_pnot, &i_index, &ctx )) )
        {
            char *psz_location;
            if ( asprintf( &psz_location, "pnot[%u]", i_index - 1 ) > -1 )
            {
                char rgz_path[14];
                snprintf( rgz_path, 14,
                         "/%4.4s[%"PRIu16"]",
                         (const char *) &p_pnot->data.p_pnot->i_type,
                         p_pnot->data.p_pnot->i_index - 1 );
                const MP4_Box_t *p_pict = MP4_BoxGet( p_root, rgz_path );
                if( p_pict )
                {
                    input_attachment_t *p_attach =
                            vlc_input_attachment_New(
                                psz_location,
                                "image/x-pict",
                                "Quickdraw image",
                                p_pict->data.p_binary->p_blob,
                                p_pict->data.p_binary->i_blob );
                    free( psz_location );
                    if( p_attach )
                        pp_attach[i_count++] = p_attach;
                }
            }
        }
    }


    /* Then other thumbnails */
    const MP4_Box_t *p_thum = MP4_BoxGet( p_root, "thum" );
    if( p_thum )
    {
        unsigned i_index = 0;
        const void *ctx = NULL;
        while( (p_thum = GetValidThumMeta( p_thum, &i_index, &ctx )) )
        {
            char *psz_location;
            if ( asprintf( &psz_location, "thum[%u]", i_index - 1 ) > -1 )
            {
                input_attachment_t *p_attach =
                        vlc_input_attachment_New(
                            psz_location,
                            NULL,
                            "Cover picture",
                            p_thum->data.p_binary->p_blob,
                            p_thum->data.p_binary->i_blob );
                free( psz_location );
                if( p_attach )
                    pp_attach[i_count++] = p_attach;
            }
        }
    }

    /* errors in adding attachments */
    if ( i_count == 0 )
    {
        free( pp_attach );
        return 0;
    }

    *ppp_attach = pp_attach;

    return i_count;
}
