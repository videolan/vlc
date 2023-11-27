/*****************************************************************************
 * es_format.c : es_format_t helpers.
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 *
 * Author: Laurent Aimar <fenrir@videolan.org>
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_es.h>

void video_format_Setup( video_format_t *p_fmt, vlc_fourcc_t i_chroma,
                         int i_width, int i_height,
                         int i_visible_width, int i_visible_height,
                         int i_sar_num, int i_sar_den )
{
    p_fmt->i_chroma         = vlc_fourcc_GetCodec( VIDEO_ES, i_chroma );
    p_fmt->i_width          = i_width;
    p_fmt->i_visible_width  = i_visible_width;
    p_fmt->i_height         = i_height;
    p_fmt->i_visible_height = i_visible_height;
    p_fmt->i_x_offset       =
    p_fmt->i_y_offset       = 0;
    p_fmt->orientation      = ORIENT_NORMAL;
    vlc_ureduce( &p_fmt->i_sar_num, &p_fmt->i_sar_den,
                 i_sar_num, i_sar_den, 0 );
}

void video_format_CopyCrop( video_format_t *p_dst, const video_format_t *p_src )
{
    p_dst->i_x_offset       = p_src->i_x_offset;
    p_dst->i_y_offset       = p_src->i_y_offset;
    p_dst->i_visible_width  = p_src->i_visible_width;
    p_dst->i_visible_height = p_src->i_visible_height;
}

void video_format_ScaleCropAr( video_format_t *p_dst, const video_format_t *p_src )
{
    p_dst->i_x_offset       = (uint64_t)p_src->i_x_offset       * p_dst->i_width  / p_src->i_width;
    p_dst->i_y_offset       = (uint64_t)p_src->i_y_offset       * p_dst->i_height / p_src->i_height;
    p_dst->i_visible_width  = (uint64_t)p_src->i_visible_width  * p_dst->i_width  / p_src->i_width;
    p_dst->i_visible_height = (uint64_t)p_src->i_visible_height * p_dst->i_height / p_src->i_height;

    p_dst->i_sar_num *= p_src->i_width;
    p_dst->i_sar_den *= p_dst->i_width;
    vlc_ureduce(&p_dst->i_sar_num, &p_dst->i_sar_den,
                p_dst->i_sar_num, p_dst->i_sar_den, 65536);

    p_dst->i_sar_num *= p_dst->i_height;
    p_dst->i_sar_den *= p_src->i_height;
    vlc_ureduce(&p_dst->i_sar_num, &p_dst->i_sar_den,
                p_dst->i_sar_num, p_dst->i_sar_den, 65536);
}

//Simplify transforms to have something more manageable. Order: angle, hflip.
static void transform_GetBasicOps( video_transform_t transform,
                                   unsigned *restrict angle,
                                   bool *restrict hflip )
{
    *hflip = ORIENT_IS_MIRROR(transform);

    switch ( transform )
    {
        case TRANSFORM_R90:
        case TRANSFORM_TRANSPOSE:
            *angle = 90;
            break;
        case TRANSFORM_R180:
        case TRANSFORM_VFLIP:
            *angle = 180;
            break;
        case TRANSFORM_R270:
        case TRANSFORM_ANTI_TRANSPOSE:
            *angle = 270;
            break;
        case TRANSFORM_HFLIP:
        case TRANSFORM_IDENTITY:
            *angle = 0;
            break;
        default:
            vlc_assert_unreachable ();
    }
}

static video_transform_t transform_FromBasicOps( unsigned angle, bool hflip )
{
    switch ( angle )
    {
        case 90:
            return hflip ? TRANSFORM_TRANSPOSE : TRANSFORM_R90;
        case 180:
            return hflip ? TRANSFORM_VFLIP : TRANSFORM_R180;
        case 270:
            return hflip ? TRANSFORM_ANTI_TRANSPOSE : TRANSFORM_R270;
        default:
            return hflip ? TRANSFORM_HFLIP : TRANSFORM_IDENTITY;
    }
}

video_transform_t video_format_GetTransform( video_orientation_t src,
                                             video_orientation_t dst )
{
    unsigned angle1, angle2;
    bool hflip1, hflip2;

    transform_GetBasicOps(  (video_transform_t)src, &angle1, &hflip1 );
    transform_GetBasicOps( transform_Inverse( (video_transform_t)dst ),
                           &angle2, &hflip2 );

    int angle = (angle1 + angle2) % 360;
    bool hflip = hflip1 ^ hflip2;

    return transform_FromBasicOps(angle, hflip);
}

void video_format_TransformBy( video_format_t *fmt, video_transform_t transform )
{
    /* Get destination orientation */
    unsigned angle1, angle2;
    bool hflip1, hflip2;

    transform_GetBasicOps( transform, &angle1, &hflip1 );
    transform_GetBasicOps( (video_transform_t)fmt->orientation, &angle2, &hflip2 );

    unsigned angle = (angle2 - angle1 + 360) % 360;
    bool hflip = hflip2 ^ hflip1;

    video_orientation_t dst_orient = ORIENT_NORMAL;

    if( hflip ) {

        if( angle == 0 )
            dst_orient = ORIENT_HFLIPPED;
        else if( angle == 90 )
            dst_orient = ORIENT_ANTI_TRANSPOSED;
        else if( angle == 180 )
            dst_orient = ORIENT_VFLIPPED;
        else if( angle == 270 )
            dst_orient = ORIENT_TRANSPOSED;
    }
    else {

        if( angle == 90 )
            dst_orient = ORIENT_ROTATED_90;
        else if( angle == 180 )
            dst_orient = ORIENT_ROTATED_180;
        else if( angle == 270 )
            dst_orient = ORIENT_ROTATED_270;
    }

    /* Apply transform */
    if( ORIENT_IS_SWAP( fmt->orientation ) != ORIENT_IS_SWAP( dst_orient ) )
    {
        video_format_t scratch = *fmt;

        fmt->i_width = scratch.i_height;
        fmt->i_visible_width = scratch.i_visible_height;
        fmt->i_height = scratch.i_width;
        fmt->i_visible_height = scratch.i_visible_width;
        fmt->i_x_offset = scratch.i_y_offset;
        fmt->i_y_offset = scratch.i_x_offset;
        fmt->i_sar_num = scratch.i_sar_den;
        fmt->i_sar_den = scratch.i_sar_num;
    }

    fmt->orientation = dst_orient;
}

void video_format_TransformTo( video_format_t *restrict fmt,
                               video_orientation_t dst_orientation )
{
    video_transform_t transform = video_format_GetTransform(fmt->orientation,
                                                            dst_orientation);
    video_format_TransformBy(fmt, transform);
}

void video_format_ApplyRotation( video_format_t *restrict out,
                                 const video_format_t *restrict in )
{
    *out = *in;

    video_format_TransformTo(out, ORIENT_NORMAL);
}

bool video_format_IsSameChroma( const video_format_t *f1,
                                const video_format_t *f2 )
{
    if( f1->i_chroma != f2->i_chroma )
        return false;

    return true;
}

bool video_format_IsSimilar( const video_format_t *f1,
                             const video_format_t *f2 )
{
    if( !video_format_IsSameChroma( f1, f2 ) )
        return false;

    if( f1->i_width != f2->i_width || f1->i_height != f2->i_height ||
        f1->i_visible_width != f2->i_visible_width ||
        f1->i_visible_height != f2->i_visible_height ||
        f1->i_x_offset != f2->i_x_offset || f1->i_y_offset != f2->i_y_offset )
        return false;
    if( (int64_t)f1->i_sar_num * f2->i_sar_den !=
        (int64_t)f2->i_sar_num * f1->i_sar_den )
        return false;

    if( f1->orientation != f2->orientation)
        return false;

    if( f1->multiview_mode!= f2->multiview_mode )
       return false;

    return true;
}

void video_format_LogDifferences(struct vlc_logger *log,
                                 const char *name_a, const video_format_t *a,
                                 const char *name_b, const video_format_t *b)
{
    if (a->i_chroma != b->i_chroma)
        vlc_debug(log, "'%s/%s' i_chroma %4.4s / %4.4s", name_a, name_b, (char*)&a->i_chroma, (char*)&b->i_chroma);
    if( a->i_width != b->i_width )
        vlc_debug(log, "'%s/%s' i_width %u / %u", name_a, name_b, a->i_width, b->i_width);
    if( a->i_height != b->i_height )
        vlc_debug(log, "'%s/%s' i_height %u / %u", name_a, name_b, a->i_height, b->i_height);
    if( a->i_visible_width != b->i_visible_width )
        vlc_debug(log, "'%s/%s' i_visible_width %u / %u", name_a, name_b, a->i_visible_width, b->i_visible_width);
    if( a->i_visible_height != b->i_visible_height )
        vlc_debug(log, "'%s/%s' i_visible_height %u / %u", name_a, name_b, a->i_visible_height, b->i_visible_height);
    if( a->i_x_offset != b->i_x_offset )
        vlc_debug(log, "'%s/%s' i_x_offset %u / %u", name_a, name_b, a->i_x_offset, b->i_x_offset);
    if( a->i_y_offset != b->i_y_offset )
        vlc_debug(log, "'%s/%s' i_y_offset %u / %u", name_a, name_b, a->i_y_offset, b->i_y_offset);
    if( (int64_t)a->i_sar_num * b->i_sar_den !=
        (int64_t)b->i_sar_num * a->i_sar_den )
        vlc_debug(log, "'%s/%s' SAR %u/%u / %u/%u", name_a, name_b, a->i_sar_den, a->i_sar_num, b->i_sar_den, b->i_sar_num);

    if( a->orientation != b->orientation)
        vlc_debug(log, "'%s/%s' orientation %d / %d", name_a, name_b, a->orientation, b->orientation);

    if( a->multiview_mode!= b->multiview_mode )
        vlc_debug(log, "'%s/%s' multiview_mode %d / %d", name_a, name_b, a->multiview_mode, b->multiview_mode);
}

static const char *orient_to_string[] =
{
    [ORIENT_NORMAL]             = "normal",
    [ORIENT_TRANSPOSED]         = "transposed",
    [ORIENT_ANTI_TRANSPOSED]    = "antitransposed",
    [ORIENT_HFLIPPED]           = "hflip",
    [ORIENT_VFLIPPED]           = "vflip",
    [ORIENT_ROTATED_180]        = "80",
    [ORIENT_ROTATED_270]        = "270",
    [ORIENT_ROTATED_90]         = "90",
};

void video_format_Print( vlc_object_t *p_this,
                         const char *psz_text, const video_format_t *fmt )
{
    const char *orient;
    if ( fmt->orientation >= 0 && fmt->orientation < ARRAY_SIZE(orient_to_string) )
        orient = orient_to_string[fmt->orientation];
    else orient = "error";

    msg_Dbg( p_this,
             "%s sz %ux%u, of (%u,%u), vsz %ux%u, 4cc %4.4s, sar %u:%u, orient: %s",
             psz_text,
             fmt->i_width, fmt->i_height, fmt->i_x_offset, fmt->i_y_offset,
             fmt->i_visible_width, fmt->i_visible_height,
             (char*)&fmt->i_chroma,
             fmt->i_sar_num, fmt->i_sar_den,
             orient );
}

void es_format_Init( es_format_t *fmt,
                     int i_cat, vlc_fourcc_t i_codec )
{
    memset(fmt, 0, sizeof (*fmt));
    fmt->i_cat                  = i_cat;
    fmt->i_codec                = i_codec;
    fmt->i_profile              = -1;
    fmt->i_level                = -1;
    fmt->i_id                   = -1;
    fmt->i_priority             = ES_PRIORITY_SELECTABLE_MIN;
    fmt->psz_language           = NULL;
    fmt->psz_description        = NULL;
    fmt->p_extra_languages      = NULL;

    if (fmt->i_cat == VIDEO_ES)
        video_format_Init(&fmt->video, 0);

    fmt->b_packetized           = true;
    fmt->p_extra                = NULL;
}

void es_format_InitFromVideo( es_format_t *p_es, const video_format_t *p_fmt )
{
    es_format_Init( p_es, VIDEO_ES, p_fmt->i_chroma );
    video_format_Copy( &p_es->video, p_fmt );
}

int es_format_Copy(es_format_t *restrict dst, const es_format_t *src)
{
    int ret = VLC_SUCCESS;

    *dst = *src;

    if (src->psz_language != NULL)
    {
        dst->psz_language = strdup(src->psz_language);
        if (unlikely(dst->psz_language == NULL))
            ret = VLC_ENOMEM;
    }
    if (src->psz_description != NULL)
    {
        dst->psz_description = strdup(src->psz_description);
        if (unlikely(dst->psz_description == NULL))
            ret = VLC_ENOMEM;
    }

    if (src->i_extra > 0)
    {
        assert(src->p_extra != NULL);
        dst->p_extra = malloc( src->i_extra );

        if( likely(dst->p_extra != NULL) )
            memcpy(dst->p_extra, src->p_extra, src->i_extra);
        else
        {
            dst->i_extra = 0;
            ret = VLC_ENOMEM;
        }
    }
    else
        dst->p_extra = NULL;

    if (src->i_cat == VIDEO_ES)
        ret = video_format_Copy( &dst->video, &src->video );

    if (src->i_cat == SPU_ES)
    {
        if (src->subs.psz_encoding != NULL)
        {
            dst->subs.psz_encoding = strdup(src->subs.psz_encoding);
            if (unlikely(dst->subs.psz_encoding == NULL))
                ret = VLC_ENOMEM;
        }
    }

    if (src->i_extra_languages > 0)
    {
        assert(src->p_extra_languages != NULL);
        dst->p_extra_languages = calloc(dst->i_extra_languages,
                                        sizeof (*dst->p_extra_languages));
        if (likely(dst->p_extra_languages != NULL))
        {
            for (unsigned i = 0; i < dst->i_extra_languages; i++)
            {
                if (src->p_extra_languages[i].psz_language != NULL)
                    dst->p_extra_languages[i].psz_language = strdup(src->p_extra_languages[i].psz_language);
                if (src->p_extra_languages[i].psz_description != NULL)
                    dst->p_extra_languages[i].psz_description = strdup(src->p_extra_languages[i].psz_description);
            }
            dst->i_extra_languages = src->i_extra_languages;
        }
        else
        {
            dst->i_extra_languages = 0;
            ret = VLC_ENOMEM;
        }
    }
    return ret;
}

void es_format_Clean(es_format_t *fmt)
{
    free(fmt->psz_language);
    free(fmt->psz_description);
    assert(fmt->i_extra == 0 || fmt->p_extra != NULL);
    free(fmt->p_extra);

    if (fmt->i_cat == VIDEO_ES)
        video_format_Clean( &fmt->video );
    if (fmt->i_cat == SPU_ES)
        free(fmt->subs.psz_encoding);

    for (unsigned i = 0; i < fmt->i_extra_languages; i++)
    {
        free(fmt->p_extra_languages[i].psz_language);
        free(fmt->p_extra_languages[i].psz_description);
    }
    free(fmt->p_extra_languages);

    /* es_format_Clean can be called multiple times */
    es_format_Init(fmt, UNKNOWN_ES, 0);
}

bool es_format_IsSimilar( const es_format_t *p_fmt1, const es_format_t *p_fmt2 )
{
    if( p_fmt1->i_cat != p_fmt2->i_cat ||
        vlc_fourcc_GetCodec( p_fmt1->i_cat, p_fmt1->i_codec ) !=
        vlc_fourcc_GetCodec( p_fmt2->i_cat, p_fmt2->i_codec ) )
        return false;

    switch( p_fmt1->i_cat )
    {
    case AUDIO_ES:
    {
        audio_format_t a1 = p_fmt1->audio;
        audio_format_t a2 = p_fmt2->audio;

        if( a1.i_format && a2.i_format && a1.i_format != a2.i_format )
            return false;
        if( a1.channel_type != a2.channel_type ||
            a1.i_rate != a2.i_rate ||
            a1.i_channels != a2.i_channels ||
            a1.i_physical_channels != a2.i_physical_channels ||
            a1.i_chan_mode != a2.i_chan_mode )
            return false;
        if( p_fmt1->i_profile != p_fmt2->i_profile )
            return false;
        return true;
    }

    case VIDEO_ES:
    {
        video_format_t v1 = p_fmt1->video;
        video_format_t v2 = p_fmt2->video;
        if( p_fmt1->i_profile != p_fmt2->i_profile )
            return false;
        if( !v1.i_chroma )
            v1.i_chroma = vlc_fourcc_GetCodec( p_fmt1->i_cat, p_fmt1->i_codec );
        if( !v2.i_chroma )
            v2.i_chroma = vlc_fourcc_GetCodec( p_fmt2->i_cat, p_fmt2->i_codec );
        return video_format_IsSimilar( &v1, &v2 );
    }

    case SPU_ES:
    default:
        return true;
    }
}


void es_format_LogDifferences(struct vlc_logger *log,
                              const char *name_a, const es_format_t *a,
                              const char *name_b, const es_format_t *b)
{
    if (a->i_cat != b->i_cat)
        vlc_debug(log, "'%s/%s' category %d / %d", name_a, name_b, a->i_cat, b->i_cat);
    if (a->i_codec != b->i_codec)
    {
        vlc_fourcc_t a_codec = vlc_fourcc_GetCodec(a->i_cat, a->i_codec);
        vlc_fourcc_t b_codec = vlc_fourcc_GetCodec(a->i_cat, a->i_codec);
        vlc_debug(log, "'%s/%s' codec %4.4s(%4.4s) / %4.4s(%4.4s)",
                  name_a, name_b, (char*)&a_codec, (char*)&a->i_codec, (char*)&b_codec, (char*)&b->i_codec);
    }
    switch (a->i_cat)
    {
    case AUDIO_ES:
    {
        audio_format_t aa = a->audio;
        audio_format_t ab = b->audio;

        if( aa.i_format && ab.i_format && aa.i_format != ab.i_format )
            vlc_debug(log, "'%s/%s' format %d / %d", name_a, name_b, aa.i_format, ab.i_format);
        if( aa.channel_type != ab.channel_type)
            vlc_debug(log, "'%s/%s' channel_type %d / %d",
                      name_a, name_b, aa.channel_type, ab.channel_type);
        if( aa.i_rate != ab.i_rate)
            vlc_debug(log, "'%s/%s' i_rate %u / %u", name_a, name_b, aa.i_rate, ab.i_rate);
        if( aa.i_channels != ab.i_channels)
            vlc_debug(log, "'%s/%s' i_channels %" PRIu8 " / %" PRIu8 ,
                      name_a, name_b, aa.i_channels, ab.i_channels);
        if( aa.i_physical_channels != ab.i_physical_channels)
            vlc_debug(log, "'%s/%s' i_physical_channels %" PRIu16 " / %" PRIu16 ,
                      name_a, name_b, aa.i_physical_channels, ab.i_physical_channels);
        if( aa.i_chan_mode != ab.i_chan_mode)
            vlc_debug(log, "'%s/%s' i_chan_mode %" PRIu16 " / %" PRIu16 ,
                      name_a, name_b, aa.i_chan_mode, ab.i_chan_mode);
        if (a->i_profile != b->i_profile)
            vlc_debug(log, "'%s/%s' profile %d / %d", name_a, name_b, a->i_profile, b->i_profile);
        break;
    }
    case VIDEO_ES:
        if (a->i_profile != b->i_profile)
            vlc_debug(log, "'%s/%s' profile %d / %d", name_a, name_b, a->i_profile, b->i_profile);
        video_format_t va = a->video;
        video_format_t vb = b->video;
        if( !va.i_chroma )
            va.i_chroma = vlc_fourcc_GetCodec(a->i_cat, a->i_codec);
        if( !vb.i_chroma )
            vb.i_chroma = vlc_fourcc_GetCodec(b->i_cat, b->i_codec);
        video_format_LogDifferences(log, name_a, &va, name_b, &vb);
        break;
    default:
        break;
    }
}
