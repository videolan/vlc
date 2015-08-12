/*****************************************************************************
 * substx3gsub.c : MP4 tx3g subtitles decoder
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
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
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_sout.h>
#include <vlc_charset.h>

#include "substext.h"

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static subpicture_t *Decode( decoder_t *, block_t ** );

vlc_module_begin ()
    set_description( N_("tx3g subtitles decoder") )
    set_shortname( N_("tx3g subtitles") )
    set_capability( "decoder", 100 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    set_callbacks( Open, NULL )
vlc_module_end ()

/****************************************************************************
 * Local structs
 ****************************************************************************/

/*****************************************************************************
 * Open: probe the decoder and return score
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t *) p_this;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_TX3G )
        return VLC_EGENERIC;

    p_dec->pf_decode_sub = Decode;

    p_dec->fmt_out.i_cat = SPU_ES;
    p_dec->fmt_out.i_codec = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Local:
 *****************************************************************************/

#define FONT_FACE_BOLD      0x1
#define FONT_FACE_ITALIC    0x2
#define FONT_FACE_UNDERLINE 0x4

static int ConvertFlags( int i_atomflags )
{
    int i_vlcstyles_flags = 0;
    if ( i_atomflags & FONT_FACE_BOLD )
        i_vlcstyles_flags |= STYLE_BOLD;
    if ( i_atomflags & FONT_FACE_ITALIC )
        i_vlcstyles_flags |= STYLE_ITALIC;
    if ( i_atomflags & FONT_FACE_UNDERLINE )
        i_vlcstyles_flags |= STYLE_UNDERLINE;
    return i_vlcstyles_flags;
}

static size_t str8len( const char *psz_string )
{
    const char *psz_tmp = psz_string;
    size_t i=0;
    while ( *psz_tmp )
    {
        if ( (*psz_tmp & 0xC0) != 0x80 ) i++;
        psz_tmp++;
    }
    return i;
}

static char * str8indup( const char *psz_string, size_t i_skip, size_t n )
{
    while( i_skip && *psz_string )
    {
        if ( (*psz_string & 0xC0) != 0x80 ) i_skip--;
        psz_string++;
    }
    if ( ! *psz_string || i_skip ) return NULL;

    const char *psz_tmp = psz_string;
    while( n && *psz_tmp )
    {
        if ( (*psz_tmp & 0xC0) != 0x80 ) n--;
        psz_tmp++;
    }
    return strndup( psz_string, psz_tmp - psz_string );
}

typedef struct tx3g_segment_t tx3g_segment_t;

struct tx3g_segment_t
{
    text_segment_t *s;
    size_t i_size;
    tx3g_segment_t *p_next3g;
};

static tx3g_segment_t * tx3g_segment_New( const char *psz_string )
{
    tx3g_segment_t *p_seg = malloc( sizeof(tx3g_segment_t) );
    if( p_seg )
    {
        p_seg->i_size = 0;
        p_seg->p_next3g = NULL;
        p_seg->s = text_segment_New( psz_string );
        if( !p_seg->s )
        {
            free( p_seg );
            p_seg = NULL;
        }
    }
    return p_seg;
}

static void SegmentDoSplit( tx3g_segment_t *p_segment, uint16_t i_start, uint16_t i_end,
                            tx3g_segment_t **pp_segment_left,
                            tx3g_segment_t **pp_segment_middle,
                            tx3g_segment_t **pp_segment_right )
{
    tx3g_segment_t *p_segment_left = *pp_segment_left;
    tx3g_segment_t *p_segment_right = *pp_segment_right;
    tx3g_segment_t *p_segment_middle = *pp_segment_middle;
    p_segment_left = p_segment_middle = p_segment_right = NULL;

    if ( (p_segment->i_size - i_start < 1) || (p_segment->i_size - i_end < 1) )
        return;

    if ( i_start > 0 )
    {
        char* psz_text = str8indup( p_segment->s->psz_text, 0, i_start );
        p_segment_left = tx3g_segment_New( psz_text );
        free( psz_text );
        if ( !p_segment_left ) goto error;
        p_segment_left->s->style = text_style_Duplicate( p_segment->s->style );
        p_segment_left->i_size = str8len( p_segment_left->s->psz_text );
    }

    char* psz_text = str8indup( p_segment->s->psz_text, i_start, i_end - i_start + 1 );
    p_segment_middle = tx3g_segment_New( psz_text );
    free( psz_text );
    if ( !p_segment_middle ) goto error;
    p_segment_middle->s->style = text_style_Duplicate( p_segment->s->style );
    p_segment_middle->i_size = str8len( p_segment_middle->s->psz_text );

    if ( i_end < (p_segment->i_size - 1) )
    {
        char* psz_text = str8indup( p_segment->s->psz_text, i_end + 1, p_segment->i_size - i_end - 1 );
        p_segment_right = tx3g_segment_New( psz_text );
        free( psz_text );
        if ( !p_segment_right ) goto error;
        p_segment_right->s->style = text_style_Duplicate( p_segment->s->style );
        p_segment_right->i_size = str8len( p_segment_right->s->psz_text );
    }

    if ( p_segment_left ) p_segment_left->p_next3g = p_segment_middle;
    if ( p_segment_right ) p_segment_middle->p_next3g = p_segment_right;

    *pp_segment_left = p_segment_left;
    *pp_segment_middle = p_segment_middle;
    *pp_segment_right = p_segment_right;

    return;

error:
    text_segment_Delete( p_segment_left->s );
    free( p_segment_left );
    text_segment_Delete( p_segment_middle->s );
    free( p_segment_middle );
    text_segment_Delete( p_segment_right->s );
    free( p_segment_right );
}

static bool SegmentSplit( tx3g_segment_t *p_prev, tx3g_segment_t **pp_segment,
                          const uint16_t i_start, const uint16_t i_end,
                          const text_style_t *p_styles )
{
    tx3g_segment_t *p_segment_left = NULL, *p_segment_middle = NULL, *p_segment_right = NULL;

    if ( (*pp_segment)->i_size == 0 ) return false;
    if ( i_start > i_end ) return false;
    if ( (size_t)(i_end - i_start) > (*pp_segment)->i_size - 1 ) return false;
    if ( i_end > (*pp_segment)->i_size - 1 ) return false;

    SegmentDoSplit( *pp_segment, i_start, i_end, &p_segment_left, &p_segment_middle, &p_segment_right );
    if ( !p_segment_middle )
    {
        /* Failed */
        text_segment_Delete( p_segment_left->s );
        free( p_segment_left );
        text_segment_Delete( p_segment_right->s );
        free( p_segment_right );
        return false;
    }

    tx3g_segment_t *p_next3g = (*pp_segment)->p_next3g;
    text_segment_Delete( (*pp_segment)->s );
    free( *pp_segment );
    *pp_segment = ( p_segment_left ) ? p_segment_left : p_segment_middle ;
    if ( p_prev ) p_prev->p_next3g = *pp_segment;

    if ( p_segment_right )
        p_segment_right->p_next3g = p_next3g;
    else
        p_segment_middle->p_next3g = p_next3g;

    text_style_Delete( p_segment_middle->s->style );
    p_segment_middle->s->style = text_style_Duplicate( p_styles );

    return true;
}

/* Creates a new segment using the given style and split existing ones according
   to the start & end offsets */
static void ApplySegmentStyle( tx3g_segment_t **pp_segment, const uint16_t i_absstart,
                               const uint16_t i_absend, const text_style_t *p_styles )
{
    /* find the matching segment */
    uint16_t i_curstart = 0;
    tx3g_segment_t *p_prev = NULL;
    tx3g_segment_t *p_cur = *pp_segment;
    while ( p_cur )
    {
        uint16_t i_curend = i_curstart + p_cur->i_size - 1;
        if ( (i_absstart >= i_curstart) && (i_absend <= i_curend) )
        {
            /* segment found */
            if ( !SegmentSplit( p_prev, &p_cur, i_absstart - i_curstart,
                                i_absend - i_curstart, p_styles ) )
                return;
            if ( !p_prev ) *pp_segment = p_cur;
            break;
        }
        else
        {
            i_curstart += p_cur->i_size;
            p_prev = p_cur;
            p_cur = p_cur->p_next3g;
        }
    }
}

/*****************************************************************************
 * Decode:
 *****************************************************************************/
static subpicture_t *Decode( decoder_t *p_dec, block_t **pp_block )
{
    block_t       *p_block;
    subpicture_t  *p_spu = NULL;

    if( ( pp_block == NULL ) || ( *pp_block == NULL ) ) return NULL;
    p_block = *pp_block;
    *pp_block = NULL;

    if( ( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) ) ||
          p_block->i_buffer < sizeof(uint16_t) )
    {
        block_Release( p_block );
        return NULL;
    }

    uint8_t *p_buf = p_block->p_buffer;

    /* Read our raw string and create the styled segment for HTML */
    uint16_t i_psz_bytelength = GetWBE( p_buf );
    const uint8_t *p_pszstart = p_block->p_buffer + sizeof(uint16_t);
    char *psz_subtitle;
    if ( i_psz_bytelength > 2 &&
         ( !memcmp( p_pszstart, "\xFE\xFF", 2 ) || !memcmp( p_pszstart, "\xFF\xFE", 2 ) )
       )
    {
        psz_subtitle = FromCharset( "UTF-16", p_pszstart, i_psz_bytelength );
        if ( !psz_subtitle ) return NULL;
    }
    else
    {
        psz_subtitle = malloc( i_psz_bytelength + 1 );
        if ( !psz_subtitle ) return NULL;
        memcpy( psz_subtitle, p_pszstart, i_psz_bytelength );
        psz_subtitle[ i_psz_bytelength ] = '\0';
    }
    p_buf += i_psz_bytelength + sizeof(uint16_t);

    for( uint16_t i=0; i < i_psz_bytelength; i++ )
     if ( psz_subtitle[i] == '\r' ) psz_subtitle[i] = '\n';

    tx3g_segment_t *p_segment3g = tx3g_segment_New( psz_subtitle );
    p_segment3g->i_size = str8len( psz_subtitle );
    if ( p_dec->fmt_in.subs.p_style )
        p_segment3g->s->style = text_style_Duplicate( p_dec->fmt_in.subs.p_style );

    if ( !p_segment3g->s->psz_text )
    {
        text_segment_Delete( p_segment3g->s );
        free( p_segment3g );
        free( psz_subtitle );
        return NULL;
    }

    /* Create the subpicture unit */
    p_spu = decoder_NewSubpictureText( p_dec );
    if( !p_spu )
    {
        free( psz_subtitle );
        text_segment_Delete( p_segment3g->s );
        free( p_segment3g );
        return NULL;
    }
    subpicture_updater_sys_t *p_spu_sys = p_spu->updater.p_sys;

    /* Parse our styles */
    while( (size_t)(p_buf - p_block->p_buffer) + 8 < p_block->i_buffer )
    {
        uint32_t i_atomsize = GetDWBE( p_buf );
        vlc_fourcc_t i_atomtype = VLC_FOURCC(p_buf[4],p_buf[5],p_buf[6],p_buf[7]);
        p_buf += 8;
        switch( i_atomtype )
        {

        case VLC_FOURCC('s','t','y','l'):
        {
            if ( (size_t)(p_buf - p_block->p_buffer) < 14 ) break;
            uint16_t i_nbrecords = GetWBE(p_buf);
            uint16_t i_cur_record = 0;
            p_buf += 2;
            while( i_cur_record++ < i_nbrecords )
            {
                if ( (size_t)(p_buf - p_block->p_buffer) < 12 ) break;
                uint16_t i_start = __MIN( GetWBE(p_buf), i_psz_bytelength - 1 );
                uint16_t i_end =  __MIN( GetWBE(p_buf + 2), i_psz_bytelength - 1 );

                text_style_t style;
                memset( &style, 0, sizeof(text_style_t) );
                style.i_style_flags = ConvertFlags( p_buf[6] );
                style.f_font_relsize = p_buf[7] * 5 / 100; /* in % units of 0.05 height */
                style.i_font_color = GetDWBE(p_buf+8) >> 8;// RGBA -> RGB
                style.i_font_alpha = GetDWBE(p_buf+8) & 0xFF;
                style.i_features = STYLE_HAS_FONT_COLOR | STYLE_HAS_FONT_ALPHA;
                ApplySegmentStyle( &p_segment3g, i_start, i_end, &style );

                if ( i_nbrecords == 1 )
                {
                    if ( p_buf[6] )
                    {
                        if( (p_spu_sys->p_default_style->i_style_flags = ConvertFlags( p_buf[6] )) )
                            p_spu_sys->p_default_style->i_features |= STYLE_HAS_FLAGS;
                    }
                    p_spu_sys->p_default_style->f_font_relsize = p_buf[7] * 5 / 100;
                    p_spu_sys->p_default_style->i_font_color = GetDWBE(p_buf+8) >> 8;// RGBA -> ARGB
                    p_spu_sys->p_default_style->i_font_alpha = (GetDWBE(p_buf+8) & 0xFF) << 24;
                    p_spu_sys->p_default_style->i_features |= (STYLE_HAS_FONT_COLOR | STYLE_HAS_FONT_ALPHA);
                }

                p_buf += 12;
            }
        }   break;

        case VLC_FOURCC('d','r','p','o'):
            if ( (size_t)(p_buf - p_block->p_buffer) < 4 ) break;
            p_spu_sys->p_default_style->i_shadow_width = __MAX( GetWBE(p_buf), GetWBE(p_buf+2) );
            break;

        case VLC_FOURCC('d','r','p','t'):
            if ( (size_t)(p_buf - p_block->p_buffer) < 2 ) break;
            p_spu_sys->p_default_style->i_shadow_alpha = GetWBE(p_buf);
            p_spu_sys->p_default_style->i_features |= STYLE_HAS_SHADOW_ALPHA;
            break;

        default:
            break;

        }
        p_buf += i_atomsize;
    }

    p_spu->i_start    = p_block->i_pts;
    p_spu->i_stop     = p_block->i_pts + p_block->i_length;
    p_spu->b_ephemer  = (p_block->i_length == 0);
    p_spu->b_absolute = false;

    p_spu_sys->align = SUBPICTURE_ALIGN_BOTTOM;

    /* Unwrap */
    text_segment_t *p_text_segments = p_segment3g->s;
    text_segment_t *p_cur = p_text_segments;
    while( p_segment3g )
    {
        tx3g_segment_t * p_old = p_segment3g;
        p_segment3g = p_segment3g->p_next3g;
        free( p_old );
        if( p_segment3g )
            p_cur->p_next = p_segment3g->s;
        p_cur = p_cur->p_next;
    }

    p_spu_sys->p_segments = p_text_segments;

    block_Release( p_block );

    return p_spu;
}
